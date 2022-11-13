/**
 * @file graph.c
 * @author Tobias Scharsching e12123692@student.tuwien.ac.at
 * @date 11.11.2022
 *
 * @brief Implements functions & structures to solve the 3color problem by
 * randomly assigning colors and removing edges.
 **/

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "graph.h"

/**
 * @brief Find an edge out of a edge array
 * 
 * @param edges the edge array
 * @param id1 the id1 to search for
 * @param id2 the id2 to search for
 * @return index of the edge or -1
 */
static int get_edge(edge_t *edges, int edge_count, int id1, int id2)
{
    int i;
    for (i = 0; i < edge_count; i++)
    {
        if (edges[i].id1 == id1 && edges[i].id2 == id2) return i;
        else if (edges[i].id1 == id2 && edges[i].id2 == id1) return i;
    }
    return -1;
}

/**
 * @brief Find an vertex out of a vertices array
 * 
 * @param vertex the vertices array
 * @param id the id1 to search for
 * @return index of the vertex or -1
 */
static int get_vertex(vertex_t *vertices, int vertices_count, int id)
{   
    int i;
    for (i=0; i < vertices_count; i++)
    {
        if (vertices[i].id == id) return i;
    }
    return -1;
}

int edges_from_args(int argc, char *argv[], int *edge_count, int *vertices_count, edge_t** _edges, vertex_t** _vertices)
{
    /* init arrays with max possible size, filled with 0 */
    int size = (argc-1);

    edge_t *edges = calloc(size, sizeof(edge_t));
    vertex_t *vertices = calloc(2*size, sizeof(vertex_t));
    int edge_pos = 0;
    int vertex_pos = 0;

    if ( edges == NULL || vertices == NULL) return -1;

    /* loop through arguments and set add vertices */
    int i;
    for (i = 1; i < argc; i++)
    {
        char *param = argv[i];

        /* split input param */
        char *left = strtok(param, "-");
        char *right = strtok(NULL, "-");

        /* check if edge could be parsed */
        if (left == NULL || right == NULL) {
            return -1;
        }

        /* convert to int - precondition that its convertable! */
        long int num_left, num_right;
        num_left = strtol(left, NULL, 10);
        num_right = strtol(right, NULL, 10);

        /* increment so that 0-bindexe dont conflict with empty-value */
        num_left++;
        num_right++;

        /* check if already contains edges with these vertices */
        if (get_edge(edges, size, num_left, num_right) == -1) 
        {
            edge_t edge;
            edge.id1 = num_left;
            edge.id2 = num_right;
            edges[edge_pos++] = edge;
        }

        /* check if left vertex is added */
        int vleft = get_vertex(vertices, 2*size, num_left);
        if (vleft == -1) 
        {
            vertex_t vertex;
            vertex.id = num_left;
            vertices[vertex_pos++] = vertex;
        }

        /* check if right vertex is added */
        int vright = get_vertex(vertices, 2*size, num_right);
        if (vright == -1) 
        {
            vertex_t vertex;
            vertex.id = num_right;
            vertices[vertex_pos++] = vertex;
        }
    }

    *edge_count = edge_pos;
    *vertices_count = vertex_pos;
    *_vertices = vertices;
    *_edges = edges;
    return 0;
}

char* solve_3color(edge_t *edges, vertex_t *vertices, int edges_count, int vertices_count, int max_removed_edges, int *removed_edges){

    /*
        set a new random color to each vertex
    */
    int i;
    for (i = 0; i < vertices_count; i++)
    {
        vertices[i].color = 1 + random() % 3;
    }

    /* 
        remove edges
    */
    int removed_length = 0;
    int removed[edges_count];
    for (i = 0; i < edges_count && removed_length < max_removed_edges; i++)
    {
        int v1 = get_vertex(vertices, vertices_count, edges[i].id1);
        int v2 = get_vertex(vertices, vertices_count, edges[i].id2);

        if (vertices[v1].color == vertices[v2].color)
        {
            removed[removed_length++] = i;
        }
    }

    /*
        calculate needed string length
    */
    int solution_length = 0;
    for(i = 0; i < removed_length; i++){
        solution_length += snprintf(NULL,0, "%d", edges[removed[i]].id1);    // vertex 1
        solution_length++;                                          // connection sign
        solution_length += snprintf(NULL,0, "%d", edges[removed[i]].id2);    // vertex 2
        if(i != removed_length - 1) solution_length++;              // separator sign
    }

    /*
        build solution string
    */
    char *solution = malloc(solution_length + 1);
    solution[solution_length] = '\0';

    if(solution == NULL) return NULL;

    char *ptr = solution;

    for (i = 0; i < removed_length; i++)
    {
        ptr += sprintf(ptr, "%d", edges[i].id1-1); // decrement because of earlier 0-val-protection
        ptr += sprintf(ptr, "-");
        ptr += sprintf(ptr, "%d", edges[i].id2-1);
        if (i != removed_length - 1) ptr += sprintf(ptr, " ");
    }

    *removed_edges = removed_length;
    return solution;
}
