/**
 * @file graph.c
 * @author Tobias Scharsching e12123692@student.tuwien.ac.at
 * @date 11.11.2022
 *
 * @brief Implements functions & structures to solve the 3color problem by
 * randomly assigning colors and removing edges.
 **/

#ifndef GRAPH_H
#define GRAPH_H

/**
 * @brief Structure that connects two vertices, independent of drection
 */
typedef struct edge {
    int id1;
    int id2;
} edge_t;

/**
 * @brief Structure that describes the vertex color
 */
typedef struct vertex {
    int id;
    int color;
} vertex_t;

/**
 * @brief Parses the argv and argc of a program to a vertice and edge array
 * 
 * @param argc arc program arg count
 * @param argv argv program arg vals
 * @return graph with edges and count, if parsing error NULL
 */
int edges_from_args(int argc, char *argv[], int *edge_count, int *vertices_count, edge_t** _edges, vertex_t** _vertices);

/**
 * @brief Solves the 3color problem in a graph by assigning random colors and removing edges
 * 
 * @param edges pointer to the edges array
 * @param vertices pointer to the vertices array
 * @param edges_count count of edges in the array
 * @param vertices_count count of vertices in the array
 * @param max_removed_edges the maximal allowed count of removed edges to get a solution
 * @param removed_edges pointer to the int which will hold the amount of removed edges
 * @return char* pointer to a array that holds the solution if format v1-v2 v3-v4 ..
 */
char* solve_3color(edge_t *edges, vertex_t *vertices, int edges_count, int vertices_count, int max_removed_edges, int *removed_edges);

#endif