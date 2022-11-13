/**
 * @file solutions.c
 * @author Tobias Scharsching e12123692@student.tuwien.ac.at
 * @date 11.11.2022
 *
 * @brief Implements a circular buffer for reading/writing solutions handled 
 * by multiple processes, based on shared mem
 **/

#ifndef SOLUTIONS_H
#define SOLUTIONS_H

#include <stdbool.h> /* for booleans */
#include <semaphore.h> /* for semaphores */

#include "solutions.h"

/* ----------       define constants        ---------- */

/**
 * @brief A symbol that indicates that a solution sequence has ended or is not present
 */
#define SOLUTION_TERMINATOR ']'

/**
 * @brief A symbol that has to be first at a solution sequence, to filter out damaged packets
 */
#define SOLUTION_STARTER '['

/**
 * @brief A symbol that represents no data, = blank
 */
#define  BLANK_SYMBOL '_'

/**
 * @brief The size of the usable shared memory for saving solutions, in bytes
 */
#define SOLUTION_DATA_SIZE 1024

/* ----------       defines of shared memory       ---------- */

/**
 * @brief struct with pointers to get access to shared memory
 */
struct solution_memory {
    size_t read_index; /** pointer to current buffer read position */
    size_t write_index; /** pointer to current buffer write position */ 
    bool supervisor_available; /** indicator that the supervisor is still waiting for results */
	char data[SOLUTION_DATA_SIZE]; /** data buffer */ 
};

/* ----------       defines of solution circular buffer       ---------- */

/**
 * @brief struct that holds data to access and handle the shared solution memory 
 */
struct solution_circular_buffer {
    sem_t* semaphore_free_space; // semaphore to lock free space pointer
    sem_t* semaphore_used_space; // semaphore to lock used space pointer
    sem_t* semaphore_block_write; // semaphore to lock writing to memory
    struct solution_memory* memory; // the shared memory struct 
    int file_descriptor; // file descriptor of the mapped shared memory
};

/**
 * @brief opens a solution buffer from a shared memory and inits semaphores to access it
 * @details
 * uses the constants for the write, free and used semaphore names and the solution buffer size
 * 
 * @param supervisor indicates if the caller process is the supervisor
 * @return struct solution_circular_buffer* holds semaphores and buffer struct; null if errored
 */
struct solution_circular_buffer *open_solution_buffer(bool supervisor);

/**
 * @brief Closes all semaphores of a solution struct and releases its memory
 * @details
 * uses the constants for the write, free and used semaphore names 
 * 
 * @param solution the solution struct that is to be closed
 * @param supervisor indicates if the caller process is the supervisor
 * @return int indicating success; -1 if cleanup of semaphores or memory was unsuccessful
 */
int close_solution_buffer(struct solution_circular_buffer* solutions, bool supervisor, bool writing);

/**
 * @brief writes a solution in the buffer index
 * @details
 * uses the constants for the starter, blank and termination symbols
 * Writes a solution in the buffer memory, and adds the starter symbol and terminal symbol around it to ensure complete transmission
 * 
 * @param solutions the solution_circular_buffer struct that holds semaphores and the buffer data
 * @param solution the new solution to write in the buffer, excluding starter and terminal symbol!
 * @return int success of the processing if 0, otherwise error occured
 */
int put_solution(struct solution_circular_buffer* solutions, char* solution, bool *writing);

/**
 * @brief Reads a solution from the memory solution buffer
 * @details
 * uses the constants for the starter, blank and termination symbols and the solution buffer size
 * Handles buffer reading; where a correctly written word 
 * has to be surrounded be starter-symbol and terminator-symbol.
 * If a starter symbol occurs without the previous solution being terminated, 
 * the previous word is discarded and the new one taken.
 * This can happen when a generator crashes during sending.
 * 
 * @param solutions struct that holds shared memory, indexes and semaphores to access
 * @return char* the fetched solution, including terminal symbol
 */
char* read_solution(struct solution_circular_buffer* solutions, int *solution_len);

#endif

