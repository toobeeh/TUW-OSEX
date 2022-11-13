/**
 * @file solutions.c
 * @author Tobias Scharsching e12123692@student.tuwien.ac.at
 * @date 11.11.2022
 *
 * @brief Implements a circular buffer for reading/writing solutions handled 
 * by multiple processes, based on shared mem
 **/

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <stddef.h> /* for struct */
#include <fcntl.h> /* for O_* constants */
#include <sys/mman.h> /* for shm_open */
#include <unistd.h> /* for ftruncate */
#include <sys/types.h> /* for ftruncate */
#include <stdio.h> /* for memset */
#include <string.h> /* for memset */

#include "solutions.h"

/* ----------       define constants        ---------- */

/**
 * @brief The name of the shared memory which will be accessed 
 */
#define SHARED_MEMORY_NAME "12123692_osue_1b_shared_memory" 

/**
 * @brief The name of the semaphore that keeps track of free space
 */
#define SEMAPHORE_FREE_SPACE "12123692_osue_1b_semaphore_free_space" 

/**
 * @brief The name of the semaphore that keeps track of used space
 */
#define SEMAPHORE_USED_SPACE "12123692_osue_1b_semaphore_used_space"

/**
 * @brief The name of the semaphore that prevents from simulatously writing to the circbuffer
 */
#define SEMAPHORE_BLOCK_WRITE "12123692_osue_1b_semaphore_block_write"

/* ----------       implementation of shared memory       ---------- */

/**
 * @brief size of a solution_memory struct
 */
static const int SOLUTION_MEMORY_SIZE = sizeof(struct solution_memory); // size of a solution memory struct 

/**
 * @brief opens shared memory for solution read/write access
 * @details 
 * uses the constants sahred memory name and -size
 * 
 * @param supervisor indicates whether the caller is a supervisor
 * @param file_descriptor the file descriptor to the shared mem file
 * @return struct shared_memory* struct that holds pointers to the shared memory
 */
static struct solution_memory *open_solution_memory(bool supervisor, int *file_descriptor)
{
    /* 
        set flags for shm_open: 
        supervisor may open & create, but must be unique; generators can only read/write to existing  
        and get struct size
    */
    int shm_open_flags = supervisor ? ( O_RDWR | O_CREAT | O_EXCL ) : ( O_RDWR );

    /*
        create shared memory object by name; with given flags.
        create with r/w owner rights ( 0x0600 )
        if failure, return null 
    */

    *file_descriptor = shm_open(SHARED_MEMORY_NAME, shm_open_flags, 0600);

    if (*file_descriptor == -1) 
    {
        //fprintf(stderr, "Failed to open shared mem with name %s and flags 0x%x; error: %s\n", SHARED_MEMORY_NAME, shm_open_flags, strerror(errno));
        return NULL;
    }
    /*
        if supervisor (creating shared mem file):
        set size of shared memory file to hold solution memory struct
        return if on failure and close shared mem file descriptor
    */
    if (supervisor && ftruncate(*file_descriptor, SOLUTION_MEMORY_SIZE) == -1)
    {
        close(*file_descriptor);
        unlink(SHARED_MEMORY_NAME);
        //fprintf(stderr, "Failed to set size of shared memory with name %s and memory size %d; error: %s\n", SHARED_MEMORY_NAME, SOLUTION_MEMORY_SIZE, strerror(errno));
        return NULL;
    }

    /*
        map file to address; NULL -> any address, flags for protection: read, write, flags for sharing: share between processes
        if failed, close file descriptor and return null
    */
    int mmap_prot_flags = PROT_READ | PROT_WRITE;
    int mmap_share_flag = MAP_SHARED;
    struct solution_memory *sm = mmap(NULL, SOLUTION_MEMORY_SIZE, mmap_prot_flags, mmap_share_flag, *file_descriptor, 0);
    if (sm == MAP_FAILED)
    {
        close(*file_descriptor);
        unlink(SHARED_MEMORY_NAME);
        //fprintf(stderr, "Failed to map shared memory; error: %s", strerror(errno));
        return NULL;
    }

    /*
        set init values of pointers (if server aka created shared memory) to position 0
        and fill solution memory with _ indicating no-value
    */
    if(supervisor)
    {
        sm->write_index = 0;
        sm->read_index = 0;
        sm->supervisor_available = true;
        memset(sm->data, BLANK_SYMBOL, SOLUTION_DATA_SIZE);
    }

    return sm;
}

/**
 * @brief Closes a shared solution memory
 * @details
 * uses the constants sahred memory name and -size
 * 
 * @param sm the solution memory struct
 * @param supervisor indicates if the calling process is a supervisor
 * @param file_descriptor the file descriptor to the shared mem file
 * @return int success of cleanup
 */
static int close_solution_memory(struct solution_memory* sm, bool supervisor, int file_descriptor)
{
    int success = 0;

    /*
        unmap linked shared memory
    */
    success = munmap(sm, SOLUTION_MEMORY_SIZE) == -1 ? -1 : success;
    
    /*
        close file descriptor of shared memory file
    */
    success = close(file_descriptor) == -1 ? -1 : success;

    /*
        if supervisor: remove file that shared memory was mapped to
    */
    success = supervisor && shm_unlink(SHARED_MEMORY_NAME) == -1 ? -1 : success;

    return success;
}


/* ----------       implementation of solution circular buffer       ---------- */

static const int SOLUTION_BUFFER_SIZE = sizeof(struct solution_circular_buffer); // size of a solution buffer struct 

struct solution_circular_buffer *open_solution_buffer(bool supervisor)
{
    /*
        alloc memory for the solution bufer,
        initialize it if not failed; try to open shared memory with functions from above
    */
    struct solution_circular_buffer *solutions = malloc(SOLUTION_BUFFER_SIZE);
    if (solutions == NULL) return NULL;
    
    solutions->semaphore_block_write = NULL;
    solutions->semaphore_free_space = NULL;
    solutions->semaphore_used_space = NULL;
    solutions->memory = open_solution_memory(supervisor, &solutions->file_descriptor);
    if (solutions->memory == NULL)
    {
        free(solutions);
        return NULL;
    }

    /*
        initialize semaphores with name (sem_open), start val and flag depending on supervisor or not
    */
    solutions->semaphore_free_space = supervisor ? 
        sem_open(SEMAPHORE_FREE_SPACE,  O_CREAT | O_EXCL, 0600, SOLUTION_DATA_SIZE) : 
        sem_open(SEMAPHORE_FREE_SPACE,  0);
    solutions->semaphore_used_space = supervisor ? 
        sem_open(SEMAPHORE_USED_SPACE,  O_CREAT | O_EXCL, 0600, 0) : 
        sem_open(SEMAPHORE_USED_SPACE,  0);
    solutions->semaphore_block_write = supervisor ? 
        sem_open(SEMAPHORE_BLOCK_WRITE,  O_CREAT | O_EXCL, 0600, 1) : 
        sem_open(SEMAPHORE_BLOCK_WRITE,  0);

    /*
        check all semaphores and do cleanup if one failed
    */
    if (solutions->semaphore_free_space == SEM_FAILED || solutions->semaphore_used_space == SEM_FAILED || solutions->semaphore_block_write == SEM_FAILED)
    {
        if (solutions->semaphore_free_space != SEM_FAILED){
            sem_close(solutions->semaphore_free_space);
            if (supervisor) sem_unlink(SEMAPHORE_FREE_SPACE);
        }

        if (solutions->semaphore_used_space != SEM_FAILED){
            sem_close(solutions->semaphore_used_space);
            if (supervisor) sem_unlink(SEMAPHORE_USED_SPACE);
        }

        if (solutions->semaphore_block_write != SEM_FAILED){
            sem_close(solutions->semaphore_block_write);
            if (supervisor) sem_unlink(SEMAPHORE_BLOCK_WRITE);
        }

        close_solution_memory(solutions->memory, supervisor, solutions->file_descriptor);
        free(solutions);
        
        return NULL;
    }

    return solutions;
}

int close_solution_buffer(struct solution_circular_buffer* solutions, bool supervisor, bool writing)
{
    /*
        if supervisor is terminating, set flag in shared memory so clients can terminate too 
        and release sem-free so that waiting processes dont get deadlocked
        else release sem-used so that supervisor doesn't get deadlocked
    */
    if (supervisor) 
    {
        solutions->memory->supervisor_available = false;
        sem_post(solutions->semaphore_free_space);
    }
    else 
    {
        if (writing) sem_post(solutions->semaphore_block_write); // also indicates the superviser that the writing one terminated
        //sem_post(solutions->semaphore_used_space);
    }

    /*
        close semaphores and shared mem
    */
    int success = 0;

    success = close_solution_memory(solutions->memory, supervisor, solutions->file_descriptor) == -1 ? -1 : success; 
    success = sem_close(solutions->semaphore_block_write) == -1 ? -1 : success;
    success = sem_close(solutions->semaphore_free_space) == -1 ? -1 : success;
    success = sem_close(solutions->semaphore_used_space) == -1 ? -1 : success;

    /*
        unlink semaphores if supervisor is terminating
    */
    success = supervisor && sem_unlink(SEMAPHORE_BLOCK_WRITE) == -1 ? -1 : success;
    success = supervisor && sem_unlink(SEMAPHORE_FREE_SPACE) == -1 ? -1 : success;
    success = supervisor && sem_unlink(SEMAPHORE_USED_SPACE) == -1 ? -1 : success;

    /*
        free solution buffer
    */
    free(solutions);

    return success;
}

int put_solution(struct solution_circular_buffer* solutions, char* solution, bool *writing)
{
    /*
        wait until no other processes are writing
    */
    *writing = false;
    if (sem_wait(solutions->semaphore_block_write) == -1) return -1;
    *writing = true;
    /*
        write solution into buffer; check if supervisor is still alive and solution string has not ended
    */
    int solution_index = 0;
    while(solutions->memory->supervisor_available)
    {
        /* 
            wait until there is free space in the buffer
        */
        if (sem_wait(solutions->semaphore_free_space) == -1)
        {
            sem_post(solutions->semaphore_block_write);
            return -1;
        } 

        /*
            put char in the buffer and handle next indexes
        */
        char item;
        if (solution_index == 0) item = SOLUTION_STARTER;
        else if (solution_index == strlen(solution)+1) item = SOLUTION_TERMINATOR;
        else item = solution[solution_index-1];

        solutions->memory->data[solutions->memory->write_index] = item; // put current solution char
        solutions->memory->write_index++;  // increment buffer write index 
        solutions->memory->write_index %= SOLUTION_DATA_SIZE; // on index overflow, put to start (-> ringbuffer)

        /*
            break if all characters have been sent
        */
        sem_post(solutions->semaphore_used_space);
        if (solution_index == strlen(solution)+1) break; 

        /*
            signalize there is content to read and
            continue with incremented index
        */
        solution_index++;
    }

    /*
        release write lock so next process can write to buffer
    */
    sem_post(solutions->semaphore_block_write);
    *writing = false;
    return 0;
}

char* read_solution(struct solution_circular_buffer* solutions, int *solution_len)
{
    /*
        set initial allocation length to 64 and resize if necessairy
    */
    size_t current_length = 0, alloc_length = 64;
    char* solution = malloc(sizeof(char) * alloc_length + 1);
    if (solution == NULL) return NULL;

    /*
        loop over buffer until a terminal symbol occurs
    */
    while (true)
    {
        /* 
            wait until there is data in the solution buffer
        */
        if (sem_wait(solutions->semaphore_used_space) == -1) 
        {
            free(solution); // free read buffer
            return NULL;
        }

        /* 
            read from buffer, and put in output
            check if solution terminates, afterwards increment read index
        */
        solution[current_length] = solutions->memory->data[solutions->memory->read_index]; // put char from buffer to output
        solutions->memory->data[solutions->memory->read_index] = BLANK_SYMBOL; 
        solutions->memory->read_index++; // increment buffer read index
        solutions->memory->read_index %= SOLUTION_DATA_SIZE; // on index overflow, put to start (-> ringbuffer)
        
        /*
            break if solution ended here
        */
        if (solution[current_length] == SOLUTION_TERMINATOR) break;

        /*
            else if previous solution wasn't complete (new starting symbol), start over
        */
        if (current_length != 0 && solution[current_length] == SOLUTION_STARTER) 
        {
            current_length = 0;
            alloc_length = 64;
            free(solution);
            solution = malloc(sizeof(char) * alloc_length);
            solution[0] = SOLUTION_STARTER;
        }

        /*
            resize buffer if needed and 
            signalize next free space is ready
        */
        current_length++; 
        if (current_length == alloc_length)
        {
            alloc_length *= 2;
            solution = realloc(solution, sizeof(char) * alloc_length);
        }
        sem_post(solutions->semaphore_free_space); 
    }

    /* 
        check if solution is valid and contains both start and terminating character
    */
    if (solution[0] != SOLUTION_STARTER || solution[current_length] != SOLUTION_TERMINATOR){
        free(solution);
        return NULL;
    } 

    char *result = malloc((--current_length) * sizeof(char));
    strncpy(result, solution+1, current_length);
    free(solution);

    /* exclude starter and terminator */
    *solution_len = current_length;

    return result;
}
