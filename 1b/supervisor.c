/**
 * @file supervisor.c
 * @author Tobias Scharsching e12123692@student.tuwien.ac.at
 * @date 11.11.2022
 *
 * @brief Implements a supervisor that reads from a shared memory the best results of a problem, 
 * until a solution is satisfying
 **/

#include <signal.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "solutions.h"

/*
    set up interrupt handler
*/
volatile sig_atomic_t terminate = 0;
static void interrupt(int signal){
    terminate = 1;
}

int main(int argc, char *argv[]){

    if (argc > 1)
    {
        fprintf(stderr, "[%s] ERROR: Too many arguments.\n  SYNOPSIS: %s\n", argv[0], argv[0]);
        return EXIT_FAILURE;
    }

    /* listen for sigint or sigterm */
    struct sigaction sa = {.sa_handler = interrupt};
    if (sigaction(SIGINT, &sa, NULL) + sigaction(SIGTERM, &sa, NULL) < 0)
    {
        fprintf(stderr, "[%s] ERROR: Could not listen for interrupts: %s\n", argv[0], strerror(errno));
        return EXIT_FAILURE;
    }

    /* open shared memory buffer */
    struct solution_circular_buffer *solutions;
    solutions = open_solution_buffer(true);
    if (solutions == NULL) 
    {
        fprintf(stderr, "[%s] ERROR: Buffer with shared memory couldn't be opened: %s\n", argv[0], strerror(errno));
		return EXIT_FAILURE;
    }

    /* listen for solutions */
    while(terminate == 0)
    {
        int solution_length;
        char *solution = read_solution(solutions, &solution_length);
        if (solution == NULL)
        {
            printf("[%s] WARN: Got invalid solution\n", argv[0]);
            continue;
        }

        /* count edges */
        int count = 0;
        int i;
        for(i=0; i < solution_length; i++)
        {
            if (solution[i] == '-') count++; 
        }
        
        /* print solution */
        if (count > 0)
        {
            printf("[%s] Solution with %d edges: %s\n", argv[0], count, solution);
        }
        else 
        {
            printf("[%s] The graph is 3-colorable!\n", argv[0]);
            terminate = 1;
        }

        free(solution);
    }

    /* close shared memory buffer */
    if (close_solution_buffer(solutions, true, false) == -1)
    {
        fprintf(stderr, "[%s] ERROR: Buffer with shared memory couldn't be closed: %s\n", argv[0], strerror(errno));
		return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;
}