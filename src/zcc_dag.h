#ifndef ZCC_DAG_H
#define ZCC_DAG_H

#define MAX_DAG_NODES 100
#define MAX_DAG_DEPS 16

typedef struct {
    char name[256];
    int state; /* 0 = pending, 1 = ready, 2 = running, 3 = done */
    unsigned long long input_hash;
    unsigned long long output_hash;
    int dependencies[MAX_DAG_DEPS];
    int num_dependencies;
    int level;
} BuildDAGNode;

typedef struct {
    BuildDAGNode nodes[MAX_DAG_NODES];
    int num_nodes;
    unsigned long long dag_hash;
} BuildDAG;

/* 
 * Invariant Validation:
 * - Checks that node indices in dependencies are valid (< num_nodes and >= 0)
 * - Checks that no node lists itself as a dependency
 * - Checks that node names are unique and non-empty
 * Returns 1 if valid, 0 if invalid.
 */
int zcc_dag_validate_invariants(const BuildDAG *dag);

/*
 * Cycle Detection:
 * - Performs DFS cycle detection using 3-color states (0=unvisited, 1=visiting, 2=visited)
 * Returns 1 if cycle detected, 0 if cycle-free.
 */
int zcc_dag_has_cycle(const BuildDAG *dag);

/*
 * Kahn's Topological Sort:
 * - Computes a valid topological order using Kahn's algorithm (in-degree reduction)
 * - order_out must be an array of size at least dag->num_nodes
 * Returns 1 on success, 0 on failure (e.g. cycle present).
 */
int zcc_dag_topo_sort(const BuildDAG *dag, int *order_out);

/*
 * Levelization:
 * - Computes execution levels based on dependency depth (longest path from roots)
 * - Updates node->level for all nodes
 */
void zcc_dag_levelize(BuildDAG *dag);

#endif /* ZCC_DAG_H */
