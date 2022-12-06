/**
 * @file forksort.c
 * @author Tobias Scharsching / 12123692
 * @brief A program which sorts lines
 * @date 2022-12-05
 * 
 * 
 * -> gets one or more liesn as input
 * -> if there is only one line, write the line to stdout
 * -> else the lines will be sorted:
 *      -> the lines are split in two equal parts (+-1)
 *      -> two child processes are created
 *      -> each child process gets a half of the lines and sorts them in the same manner as described
 *      -> the outputs of the child processes are merged:
 *          -> outputs are compared line-by-line
 *          -> the line with lower value is put first, the bigger last
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
 * flag to activate debug output on stderr
 */
#define DEBUG 0


/**
 * global variable of the program name
 */
char *program_name = "undefined";


/**
 * a struct that holds information about a child process and the pipes that lead to it
 */
typedef struct {

    /** the child process id */
    pid_t pid;

    /** pipe that leads from parent to child */
    int pipe_parent_child[2];

    /** pipe that leads from child to parent*/
    int pipe_child_parent[2];
} child_proc_t;

void synopsis(){
    fprintf(stderr, "SYNOPSIS:\n   %s\n", program_name);
}

int open_child_and_pipes(child_proc_t *child)
{
    
    /* open pipes */
    if(pipe(child->pipe_parent_child) == -1 || pipe(child->pipe_child_parent) == -1)
    {
        if(DEBUG > 0) fprintf(stderr, "Failed to open pipe");
        return -1;
    }

    /* fork process */
    pid_t pid = fork();
    if(pid < 0){
        if(DEBUG > 0) fprintf(stderr, "Failed to fork process");
        return -1;
    }

    /* if child process, execute forksort and close unused ends */
    if(pid == 0){

        /* redirect pipes ~ pipe[0]-> read, pipe[1]-> write */
        if(dup2(child->pipe_parent_child[0], STDIN_FILENO) == -1 || dup2(child->pipe_child_parent[1], STDOUT_FILENO) == -1)
        {
            if(DEBUG > 0) fprintf(stderr, "Failed to redirect pipes");
            return -1;
        }

        /* close child's duplicated source file descriptors */
        if(close(child->pipe_parent_child[0]) == -1 || close(child->pipe_child_parent[1]) == -1 )
        {
            if(DEBUG > 0) fprintf(stderr, "Failed to close child's source pipe file descriptors %s", strerror(errno));
            return -1;
        }

        /* close child's unused pipe ends: parent-to-child write, child-to-parent read */
        if(close(child->pipe_parent_child[1]) == -1 || close(child->pipe_child_parent[0]) == -1 )
        {
            if(DEBUG > 0) fprintf(stderr, "Failed to close child's unused pipes %s", strerror(errno));
            return -1;
        }

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

    /* set new process id */
    child->pid = pid;

    return 0;
}

int pass_to_child(child_proc_t *child, char *message)
{
    
    /* if child process has not been initialized, open it */
    if(child->pid == 0)
    {

        if(open_child_and_pipes(child) == -1){

            /* TODO free res */
            exit(EXIT_FAILURE);
        }
    }

    if(write(child->pipe_parent_child[1], message, strlen(message)) == -1)
    {
        if(DEBUG > 0) fprintf(stderr, "Failed to pass message to child process");
        return -1;
    }

    return 0;
}

int close_parent_pipe(child_proc_t *child, char mode){

    int pipe = mode == 'r' ? child->pipe_child_parent[0] : child->pipe_parent_child[1];

    /* if child was init  */
    if(child != NULL)
    {
        if(close(pipe) == -1)
        {
            if(DEBUG > 0) fprintf(stderr, "Failed to close parent pipe %s with mode %c\n", strerror(errno), mode);
            return -1;
        }
    }
    return 0;
}

int print_pipes_sorted(child_proc_t *child_left, child_proc_t *child_right)
{

    /* open pipes as streams for convenience, if the child process is running */
    FILE *left_read = child_left->pid > 0 ? fdopen(child_left->pipe_child_parent[0], "r") : NULL;
    FILE *right_read = child_right->pid > 0 ? fdopen(child_right->pipe_child_parent[0], "r") : NULL;

    /* init stuff */
    char *last_left = NULL, *last_right = NULL;
    size_t len_last_left = 0, len_last_right = 0;
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

    free(last_left);
    free(last_right);
    return 1;
}

int main(int argc, char *argv[])
{

    if(DEBUG > 0) printf(stderr, "+ pid %d\n", getpid());

    child_proc_t *sort_left = malloc(sizeof(child_proc_t));
    child_proc_t *sort_right = malloc(sizeof(child_proc_t));
    int line_count = 0;
    int total_characters = 0;

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


    line_count++;

    /* check for further lines, while not eof */
    char *next_line = NULL;
    size_t next_line_length = 0;

    while((next_line_length = getline(&next_line, &next_line_length, stdin)) != EOF){

        /* update counters and state */
        total_characters += next_line_length;
        line_count++;

        /* get child switching */
        child_proc_t *child = (line_count % 2 == 1) ? sort_left : sort_right;

        if(DEBUG > 0) fprintf(stderr, "%d <- %s\n", getpid(), next_line);

        /* pass msg to child */
        if(pass_to_child(child, next_line) == -1)
        {
            /* TODO free ressources */
            exit(EXIT_FAILURE);
        }
    }

    if(DEBUG > 0) fprintf(stderr, "%d <- EOF\n", getpid());
    free(next_line);

    /* pass first line too if there were more than one lines */
    if(line_count > 1)
    {

        /* get child with inverse modulo to skip last used */
        child_proc_t *child = (line_count % 2 == 0) ? sort_left : sort_right;

        if(pass_to_child(child, first_line) == -1)
        {
            /* TODO free ressources */
            exit(EXIT_FAILURE);
        }

        free(first_line);
    }

    /* else only one line was present, write that to stdout */
    else
    {
        printf("%s", first_line);
        free(first_line);
        exit(EXIT_SUCCESS);
    }

    /* close write pipe to signalize finished reading */
    if(close_parent_pipe(sort_left, 'w') == -1 || close_parent_pipe(sort_right, 'w') == -1){
        // exit?
    }

    /* read lines from the pipes and print the output sorted */
    print_pipes_sorted(sort_left, sort_right);

    /* finally close all read pipes */
    if(close_parent_pipe(sort_left, 'r') == -1 || close_parent_pipe(sort_right, 'r') == -1){
        // exit?
    }

    exit(EXIT_SUCCESS);
}