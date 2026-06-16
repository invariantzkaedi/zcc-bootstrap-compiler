#include "zcc_dag.h"
#include <string.h>
#include <stdio.h>

/* Validate BuildDAG edge/node invariants */
int zcc_dag_validate_invariants(const BuildDAG *dag) {
    int i, j, k;

    if (!dag) return 0;
    if (dag->num_nodes < 0 || dag->num_nodes > MAX_DAG_NODES) return 0;

    for (i = 0; i < dag->num_nodes; i++) {
        const BuildDAGNode *node = &dag->nodes[i];

        /* Check node name is non-empty and unique */
        if (node->name[0] == '\0') return 0;
        for (j = i + 1; j < dag->num_nodes; j++) {
            if (strcmp(node->name, dag->nodes[j].name) == 0) {
                return 0; /* Duplicate node name */
            }
        }

        /* Check dependency boundaries and self-reference */
        if (node->num_dependencies < 0 || node->num_dependencies > MAX_DAG_DEPS) {
            return 0;
        }
        for (j = 0; j < node->num_dependencies; j++) {
            int dep_idx = node->dependencies[j];
            if (dep_idx < 0 || dep_idx >= dag->num_nodes) {
                return 0; /* Out-of-bounds dependency index */
            }
            if (dep_idx == i) {
                return 0; /* Self-dependency check */
            }
            /* Check for duplicate dependency entries within the same node */
            for (k = j + 1; k < node->num_dependencies; k++) {
                if (node->dependencies[j] == node->dependencies[k]) {
                    return 0; /* Duplicate dependency reference */
                }
            }
        }
    }
    return 1;
}

/* DFS cycle detection helper with 3-color states (0=unvisited/white, 1=visiting/gray, 2=visited/black) */
static int dfs_cycle_detect(const BuildDAG *dag, int node_idx, int *colors) {
    int i;
    colors[node_idx] = 1; /* Mark gray (visiting) */

    for (i = 0; i < dag->nodes[node_idx].num_dependencies; i++) {
        int dep_idx = dag->nodes[node_idx].dependencies[i];
        if (colors[dep_idx] == 1) {
            return 1; /* Found a back-edge to a visiting/gray node -> cycle detected */
        }
        if (colors[dep_idx] == 0) {
            if (dfs_cycle_detect(dag, dep_idx, colors)) {
                return 1;
            }
        }
    }

    colors[node_idx] = 2; /* Mark black (visited) */
    return 0;
}

/* Cycle detection with DFS color states */
int zcc_dag_has_cycle(const BuildDAG *dag) {
    int colors[MAX_DAG_NODES];
    int i;

    if (!dag || !zcc_dag_validate_invariants(dag)) return 1;

    memset(colors, 0, sizeof(colors));

    for (i = 0; i < dag->num_nodes; i++) {
        if (colors[i] == 0) {
            if (dfs_cycle_detect(dag, i, colors)) {
                return 1;
            }
        }
    }
    return 0;
}

/* Kahn's Topological Sort (In-Degree Reduction) */
int zcc_dag_topo_sort(const BuildDAG *dag, int *order_out) {
    int in_degree[MAX_DAG_NODES];
    int queue[MAX_DAG_NODES];
    int head = 0;
    int tail = 0;
    int count = 0;
    int i, j;

    if (!dag || !order_out || zcc_dag_has_cycle(dag)) return 0;

    memset(in_degree, 0, sizeof(in_degree));

    /* Calculate in-degrees: dep -> node means node depends on dep */
    for (i = 0; i < dag->num_nodes; i++) {
        for (j = 0; j < dag->nodes[i].num_dependencies; j++) {
            in_degree[i]++;
        }
    }

    for (i = 0; i < dag->num_nodes; i++) {
        if (in_degree[i] == 0) {
            if (tail < MAX_DAG_NODES) {
                queue[tail++] = i;
            } else {
                return 0; /* Queue overflow protection */
            }
        }
    }

    while (head < tail) {
        int u = queue[head++];
        order_out[count++] = u;

        /* Find all nodes v that depend on u and decrement their in-degree */
        for (i = 0; i < dag->num_nodes; i++) {
            for (j = 0; j < dag->nodes[i].num_dependencies; j++) {
                int dep_idx = dag->nodes[i].dependencies[j];
                if (dep_idx == u) {
                    in_degree[i]--;
                    if (in_degree[i] == 0) {
                        if (tail < MAX_DAG_NODES) {
                            queue[tail++] = i;
                        } else {
                            return 0; /* Queue overflow protection */
                        }
                    }
                }
            }
        }
    }

    /* If sorted count matches total count, topo sort is successful */
    return (count == dag->num_nodes);
}

/* Levelization based on topo order dependency depth */
void zcc_dag_levelize(BuildDAG *dag) {
    int i, j;
    int order[MAX_DAG_NODES];

    if (!dag || zcc_dag_has_cycle(dag)) return;

    /* Initialize all levels to 0 */
    for (i = 0; i < dag->num_nodes; i++) {
        dag->nodes[i].level = 0;
    }

    /* We use the topo sort to compute level in correct order */
    if (!zcc_dag_topo_sort(dag, order)) return;

    /* Process nodes in topological order */
    for (i = 0; i < dag->num_nodes; i++) {
        int u = order[i];
        int max_dep_level = -1;
        for (j = 0; j < dag->nodes[u].num_dependencies; j++) {
            int dep_idx = dag->nodes[u].dependencies[j];
            if (dag->nodes[dep_idx].level > max_dep_level) {
                max_dep_level = dag->nodes[dep_idx].level;
            }
        }
        if (max_dep_level != -1) {
            dag->nodes[u].level = max_dep_level + 1;
        }
    }
}

