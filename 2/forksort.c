/**
 * @file forksort.c
 * @author Tobias Scharsching / 12123692
 * @brief A program which sorts lines according to OSEX excercise 2, winter semester 2022
 * @date 2022-12-05
 * 
 *  Kill all processes: 
 *      pgrep -u "$(whoami)" forksort | xargs kill
 * 
 *  Test: 
 *      valgrind --leak-check=full --track-origins=yes --show-leak-kinds=all ./forksort < test.txt
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>


/**
 * @brief  flag to activate debug output on stderr
 */
#define DEBUG 0


/**
 * @brief  global variable of the program name
 */
char *program_name = "undefined";


/**
 * @brief  a struct that holds information about a child process and the pipes that lead to it
 */
typedef struct {

    /** @brief  child process id */
    pid_t pid;

    /** @brief  pipe that leads from parent to child */
    int pipe_parent_child[2];

    /** @brief  pipe that leads from child to parent */
    int pipe_child_parent[2];
} child_proc_t;


/**
 * @brief print the synopsis
 */
void synopsis(){
    fprintf(stderr, "SYNOPSIS:\n   %s\n", program_name);
}

/**
 * @brief inits a new child proc details struct
 * 
 * @details
 * process id 0 -> no child proces running
 * pipe file descriptor NULL -> no fd open
 * 
 * @return child_proc_t* 
 */
child_proc_t* init_child_proc_details()
{
    /* init child process struct */
    child_proc_t *proc = malloc(sizeof(child_proc_t));
    proc->pid = 0;
    proc->pipe_child_parent[0] = -1;
    proc->pipe_child_parent[1] = -1;
    proc->pipe_parent_child[0] = -1;
    proc->pipe_parent_child[1] = -1;

    return proc;
}

/**
 * @brief creates a new cild process and pipes from & to it
 * 
 * @details
 * forks the current process and creates two pipes; 
 * one from the parent to the child, and vice versa.
 * 
 * the parent does not read from the parent->child pipe and 
 * does not write to the child->parent pipe, so these ends are closed.
 * 
 * the child closes the opposite pipe ends and duplicates the open ends to stdin and stdout. 
 * the unused old file descriptors are closed.
 * finally, the child executes the original program and therefore creates a new recursion step of forksort
 * 
 * @param child struct pointer with allocated memory that will hold the new child process's details
 * @return int that is < 0 on error and 0 if the child process setup was successful
 */
int open_child_and_pipes(child_proc_t *child)
{
    
    /* open both pipes and check for an error */
    if(pipe(child->pipe_parent_child) == -1 || pipe(child->pipe_child_parent) == -1)
    {
        if(DEBUG > 0) fprintf(stderr, "Failed to open pipe: %s", strerror(errno));
        return -1;
    }

    /* fork process and check for error*/
    pid_t pid = fork();
    if(pid < 0){
        if(DEBUG > 0) fprintf(stderr, "Failed to fork process: %s", strerror(errno));
        return -1;
    }

    /* if current proc is child process, execute new forksort, redirect pipes and close unused ends - on error exit process */
    if(pid == 0){

        /* redirect pipes to stdin and stdout ~ pipe[0]-> read, pipe[1]-> write */
        if(dup2(child->pipe_parent_child[0], STDIN_FILENO) == -1 || dup2(child->pipe_child_parent[1], STDOUT_FILENO) == -1)
        {
            if(DEBUG > 0) fprintf(stderr, "Failed to redirect pipes %s", strerror(errno));
            exit(EXIT_FAILURE);
        }

        /* close child's duplicated source file descriptors */
        if(close(child->pipe_parent_child[0]) == -1 || close(child->pipe_child_parent[1]) == -1 )
        {
            if(DEBUG > 0) fprintf(stderr, "Failed to close child's source pipe file descriptors %s", strerror(errno));
            exit(EXIT_FAILURE);
        }

        /* close child's unused pipe ends: parent-to-child write, child-to-parent read */
        if(close(child->pipe_parent_child[1]) == -1 || close(child->pipe_child_parent[0]) == -1 )
        {
            if(DEBUG > 0) fprintf(stderr, "Failed to close child's unused pipes %s", strerror(errno));
            exit(EXIT_FAILURE);
        }

        /* execute forksort without any params */
        execlp(program_name, program_name, NULL);

        /* if reached, exec has failed */
        if(DEBUG > 0) fprintf(stderr, "Failed to exec program in forked process");
        exit(EXIT_FAILURE);
    }

    /* close unused pipe ends for parent: parent-to-child read, child-to-parent write */
    if(close(child->pipe_parent_child[0]) == -1 || close(child->pipe_child_parent[1]) == -1 )
    {
        if(DEBUG > 0) fprintf(stderr, "Failed to close parent's unused pipes %s", strerror(errno));
        return -1;
    }

    /* set the child's process id */
    child->pid = pid;

    return 0;
}

/**
 * @brief writes a message to a pipe that leads to a child process
 * 
 * @details
 * checks if the child process in the provided struct is initialized (pid is 0 if not)
 * if not, the child process and its pipes are inited.
 * the provided message is then written to the parent->child pipe
 * 
 * @param child the struct that holds information of the target child process
 * @param message the message to send
 * @return <0 if an error occured, 0 if the message was sent successfully
 */
int pass_to_child(child_proc_t *child, char *message)
{
    
    /* check if child process has been initialized yet */
    if(child->pid == 0)
    {
        /* open child and pipes to it */
        if(open_child_and_pipes(child) == -1){

            if(DEBUG > 0) fprintf(stderr, "Failed to init chld process when trying to write to it %s", strerror(errno));
            return -1;
        }
    }

    /* write message to the pipe */
    if(write(child->pipe_parent_child[1], message, strlen(message)) == -1)
    {
        if(DEBUG > 0) fprintf(stderr, "Failed to pass message to child process %s", strerror(errno));
        return -1;
    }

    return 0;
}

/**
 * @brief closes a pipe end of a child process
 * 
 * @details
 * closes either the read end (mode='r') or write end (mode='w) or both (mode=-1) of a pipe.
 * the pipe is fetched from a given child process, depending on the specified
 * direction mode ('p', 'c', -1 for both).
 * the file descriptor of the target pipe is then set to -1 to indicate as closed.
 * 
 * @param child the target child process details
 * @param mode_end the target pipe end; 'r' to close read end and 'w' for write end. -1 to close both.
 * @param mode_dir the target pipe direction; 'p' to close parent->child and 'c' for child->parent. -1 to close both.
 * @return -1 if the pipe end could not be closed. 0 if the pipe end was closed successfully.
 */
int close_pipe_ends(child_proc_t *child, char mode_end, char mode_dir){

    /* if no access mode is specified, close both pipe ends */
    if(mode_end == -1)
    {
        return close_pipe_ends(child, 'r', mode_dir) + close_pipe_ends(child, 'w', mode_dir);
    }

    /* if no direction mode is specified, close both directions */
    if(mode_dir == -1)
    {
        return close_pipe_ends(child, mode_dir, 'p') + close_pipe_ends(child, mode_dir, 'c');
    }

    /* get pipe & file desc of scpecified mode */
    int *file_desc = ((mode_dir == 'c') ? 
        &(child->pipe_child_parent[((mode_end == 'w') ? 1 : 0)]) : 
        &(child->pipe_parent_child[((mode_end == 'w') ? 1 : 0)]));

    /* if child was init and fd still open (>0), close the end  */
    if(child != NULL && *file_desc != -1)
    {
        if(close(*file_desc) == -1)
        {
            if(DEBUG > 0) fprintf(stderr, "Failed to close parent pipe %s with mode %c\n", strerror(errno), mode_end);
            return -1;
        }

        /* set descriptor to closed */
        *file_desc = -1;
    }
    return 0;
}

/**
 * @brief reads from two pipes and printes the lines sorted ascending
 * 
 * @details
 * takes two child processes and opens their read ends of the child->parent pipe as streams.
 * from both streams, there are read lines until there occurs an EOF.
 * each iteration, the smaller of the both currently fetched and printed. 
 * when the lines was printed, the next one from the respective stream is fetched.
 * 
 * @param child_left one of two child process details
 * @param child_right other child process details
 * @return <0 if there had occured an error, or 0 if all lines were read and printed successfully>
 */
int print_pipes_sorted(child_proc_t *child_left, child_proc_t *child_right)
{

    /* open pipes as streams for convenience, if the child process is running */
    FILE *left_read = child_left->pid > 0 ? fdopen(dup(child_left->pipe_child_parent[0]), "r") : NULL;
    FILE *right_read = child_right->pid > 0 ? fdopen(dup(child_right->pipe_child_parent[0]), "r") : NULL;

    /* char buffers that hold the last read line content */
    char *last_left = NULL, *last_right = NULL;

    /* the last read line length */
    size_t len_last_left = 0, len_last_right = 0;

    /* indicates whether there is a value ready from one of both streams */
    bool left_empty = true, right_empty = true;

    /* keep merging until both pipes have no more output */
    while(true)
    {
        /* get new content for left: read end from left child->parent pipe */
        if(left_empty && left_read != NULL) 
        {
            len_last_left = getline(&last_left, &len_last_left, left_read);
            if(len_last_left != EOF) left_empty = false;
        }

        /* get new content for left: read end from left child->parent pipe */
        if(right_empty && left_read != NULL) 
        {
            len_last_right = getline(&last_right, &len_last_right, right_read);
            if(len_last_right != EOF)right_empty = false;
        }

        /* end if both empty */
        if(left_empty && right_empty) break;

        /* if left is empty, put right, or vice versa */
        if(left_empty) 
        {
            fprintf(stdout, "%s", last_right);
            right_empty = true;
        }
        else if(right_empty)
        {
            fprintf(stdout,"%s", last_left);
            left_empty = true;
        }

        /* if both not empty, compare */
        else if(strcmp(last_left, last_right) > 0) 
        {
            fprintf(stdout,"%s", last_right);
            right_empty = true;
        }
        else 
        {
            fprintf(stdout,"%s", last_left);
            left_empty = true;
        }
    }

    /* free strings */
    free(last_left);
    free(last_right);

    if(left_read != NULL) fclose(left_read);
    if(right_read != NULL) fclose(right_read);

    return 0;
}

void cleanup(child_proc_t *child_left, child_proc_t *child_right)
{
    /* close all pipes */
    close_pipe_ends(child_left, -1, -1);
    close_pipe_ends(child_left, -1, -1);

    /* free structs */
    free(child_left);
    free(child_right);
}


int main(int argc, char *argv[])
{

    if(DEBUG > 0) printf(stderr, "+ pid %d\n", getpid());

    child_proc_t *sort_left = init_child_proc_details();
    child_proc_t *sort_right = init_child_proc_details();

    /* get prog name */
    program_name = argv[0];

    /* make sure there are no arguments */
    if(argc > 1) {
        synopsis();
        exit(EXIT_FAILURE);
    }

    /* get first line */
    char *first_line = NULL;
    size_t first_line_length = 0;
    first_line_length = getline(&first_line, &first_line_length, stdin);

    /* exit early if no lines were read - can only happen in first process */
    if(first_line_length == EOF)
    {
        printf("No lines to sort provided.\n");
        exit(EXIT_SUCCESS);
    }
    if(DEBUG > 0) fprintf(stderr, "%d <- %s\n", getpid(), first_line);

    /* check for further lines, while not eof */
    char *next_line = NULL;
    size_t next_line_length = 0;
    int line_count = 1;

    while((next_line_length = getline(&next_line, &next_line_length, stdin)) != EOF){

        /* update counter */
        line_count++;

        /* get child switching */
        child_proc_t *child = (line_count % 2 == 1) ? sort_left : sort_right;

        if(DEBUG > 0) fprintf(stderr, "%d <- %s\n", getpid(), next_line);

        /* pass msg to child */
        if(pass_to_child(child, next_line) == -1)
        {
            /* exit if message passing failed */
            cleanup(sort_left, sort_right);
            exit(EXIT_FAILURE);
        }
    }

    /* clean up line buf */
    if(DEBUG > 0) fprintf(stderr, "%d <- EOF\n", getpid());
    free(next_line);

    /* pass first line too if there were more than one lines */
    if(line_count > 1)
    {

        /* get child with inverse modulo to skip last used */
        child_proc_t *child = (line_count % 2 == 0) ? sort_left : sort_right;

        if(pass_to_child(child, first_line) == -1)
        {
            /* exit if message passing failed */
            cleanup(sort_left, sort_right);
            exit(EXIT_FAILURE);
        }

        free(first_line);
    }

    /* else only one line was present, write that to stdout */
    else
    {
        printf("%s", first_line);
        free(first_line);
        cleanup(sort_left, sort_right);
        exit(EXIT_SUCCESS);
    }

    /* close write pipe to signalize finished reading */
    if(close_pipe_ends(sort_left, 'w', 'p') == -1 || close_pipe_ends(sort_right, 'w', 'p') == -1){
        cleanup(sort_left, sort_right);
        exit(EXIT_SUCCESS);
    }

    /* read lines from the pipes and print the output sorted */
    print_pipes_sorted(sort_left, sort_right);

    /* finally close all read pipes and free structs */
    cleanup(sort_left, sort_right);

    exit(EXIT_SUCCESS);
}