
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <signal.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "solutions.h"
#include "graph.h"

/*
    set up interrupt handler
*/
volatile sig_atomic_t terminate = 0;
static void interrupt(int signal){
    terminate = 1;
}

int main(int argc, char *argv[]){

    if (argc < 2) 
    {
        fprintf(stderr, "[%s] ERROR: No edges specified.\n  SYNOPSIS: %s vertice1-vertice2..\n", argv[0], argv[0]);
    }

    /* listen for sigint or sigterm */
    struct sigaction sa = {.sa_handler = interrupt};
    if (sigaction(SIGINT, &sa, NULL) + sigaction(SIGTERM, &sa, NULL) < 0)
    {
        fprintf(stderr, "[%s] ERROR: Could not listen for interrupts: %s\n", argv[0], strerror(errno));
        exit(EXIT_FAILURE);
    }

    /*
        set a random seed
    */
    srand((unsigned int)time(NULL));

    int edge_count, vertices_count, removed_edges;
    int best_solution = 8;
    int success = 0;

    /* parse edges */
    edge_t *edges;
    vertex_t *vertices;
    int res = edges_from_args(argc, argv, &edge_count, &vertices_count, &edges, &vertices);
    if (res == -1)
    {
        fprintf(stderr, "[%s] ERROR: Could not parse edge list.\n  SYNOPSIS: %s vertice1-vertice2..\n", argv[0], argv[0]);
        free(edges);
        free(vertices);
        return EXIT_FAILURE;
    }  

    /* open shared memory / buffer */
    struct solution_circular_buffer* solutions = open_solution_buffer(false);
    if (solutions == NULL)
    {
        fprintf(stderr, "[%s] ERROR: Could not open shared memory.\n", argv[0]);
        free(edges);
        free(vertices);
        return EXIT_FAILURE;
    }

    /*
        try random solutions until terminated
    */
    bool writing = false;
    while(terminate != 1 && solutions->memory->supervisor_available)
    {
        /*
            get a solution
        */
        char* solution = solve_3color(edges, vertices, edge_count, vertices_count, best_solution, &removed_edges);

        if(removed_edges < best_solution){
            printf("[%s] Found solution with %d removed edges %s\n", argv[0], removed_edges, solution);

            /*
                write solution to buffer
            */
            best_solution = removed_edges;
            success = put_solution(solutions, solution, &writing);
            if(success == -1)
            {
                terminate = 1;
                fprintf(stderr, "[%s] ERROR: Buffer could not be written.\n", argv[0]);
            }
        }

        free(solution);
    }

    /* clean ressources */
    close_solution_buffer(solutions, false, writing);
    free(edges);
    free(vertices);

    return success == -1 ? EXIT_FAILURE : EXIT_SUCCESS;
}
