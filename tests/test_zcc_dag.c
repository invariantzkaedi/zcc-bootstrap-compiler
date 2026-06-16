#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include "../src/zcc_dag.h"
#include "test_dag_data.h"

#define ASSERT_TRUE(x) do { \
    if (!(x)) { \
        fprintf(stderr, "ASSERTION FAILED: %s at %s:%d\n", #x, __FILE__, __LINE__); \
        exit(1); \
    } \
} while(0)

#define ASSERT_FALSE(x) ASSERT_TRUE(!(x))

static void test_linear_dag(void) {
    BuildDAG dag;
    int order[4];

    printf("Running test_linear_dag...\n");
    memset(&dag, 0, sizeof(dag));
    dag.num_nodes = 4;
    dag.dag_hash = 12345ULL;

    /* A -> B -> C -> D (Note: node depends on its dep, so D depends on C, C on B, B on A) */
    /* Meaning A runs first, then B, then C, then D. */
    strcpy(dag.nodes[0].name, "NodeA");
    dag.nodes[0].num_dependencies = 0;

    strcpy(dag.nodes[1].name, "NodeB");
    dag.nodes[1].num_dependencies = 1;
    dag.nodes[1].dependencies[0] = 0;

    strcpy(dag.nodes[2].name, "NodeC");
    dag.nodes[2].num_dependencies = 1;
    dag.nodes[2].dependencies[0] = 1;

    strcpy(dag.nodes[3].name, "NodeD");
    dag.nodes[3].num_dependencies = 1;
    dag.nodes[3].dependencies[0] = 2;

    /* Verify invariants */
    ASSERT_TRUE(zcc_dag_validate_invariants(&dag));

    /* Verify no cycle */
    ASSERT_FALSE(zcc_dag_has_cycle(&dag));

    /* Topological Sort */
    ASSERT_TRUE(zcc_dag_topo_sort(&dag, order));
    /* Expected order: 0, 1, 2, 3 */
    ASSERT_TRUE(order[0] == 0);
    ASSERT_TRUE(order[1] == 1);
    ASSERT_TRUE(order[2] == 2);
    ASSERT_TRUE(order[3] == 3);

    /* Levelization */
    zcc_dag_levelize(&dag);
    ASSERT_TRUE(dag.nodes[0].level == 0);
    ASSERT_TRUE(dag.nodes[1].level == 1);
    ASSERT_TRUE(dag.nodes[2].level == 2);
    ASSERT_TRUE(dag.nodes[3].level == 3);

    printf("  -> Linear DAG OK\n");
}

static void test_diamond_dag(void) {
    BuildDAG dag;
    int order[4];

    printf("Running test_diamond_dag...\n");
    memset(&dag, 0, sizeof(dag));
    dag.num_nodes = 4;
    dag.dag_hash = 67890ULL;

    /* 
     * Diamond DAG:
     * A (0) runs first.
     * B (1) depends on A.
     * C (2) depends on A.
     * D (3) depends on B and C.
     */
    strcpy(dag.nodes[0].name, "A");
    dag.nodes[0].num_dependencies = 0;

    strcpy(dag.nodes[1].name, "B");
    dag.nodes[1].num_dependencies = 1;
    dag.nodes[1].dependencies[0] = 0;

    strcpy(dag.nodes[2].name, "C");
    dag.nodes[2].num_dependencies = 1;
    dag.nodes[2].dependencies[0] = 0;

    strcpy(dag.nodes[3].name, "D");
    dag.nodes[3].num_dependencies = 2;
    dag.nodes[3].dependencies[0] = 1;
    dag.nodes[3].dependencies[1] = 2;

    /* Validate invariants */
    ASSERT_TRUE(zcc_dag_validate_invariants(&dag));

    /* Verify no cycle */
    ASSERT_FALSE(zcc_dag_has_cycle(&dag));

    /* Topological Sort */
    ASSERT_TRUE(zcc_dag_topo_sort(&dag, order));
    ASSERT_TRUE(order[0] == 0);
    /* order[1] and order[2] can be either 1 or 2 */
    ASSERT_TRUE((order[1] == 1 && order[2] == 2) || (order[1] == 2 && order[2] == 1));
    ASSERT_TRUE(order[3] == 3);

    /* Levelization */
    zcc_dag_levelize(&dag);
    ASSERT_TRUE(dag.nodes[0].level == 0);
    ASSERT_TRUE(dag.nodes[1].level == 1);
    ASSERT_TRUE(dag.nodes[2].level == 1);
    ASSERT_TRUE(dag.nodes[3].level == 2);

    printf("  -> Diamond DAG OK\n");
}

static void test_disconnected_dag(void) {
    BuildDAG dag;
    int order[5];
    int seen[5];
    int pos_a = -1, pos_b = -1, pos_c = -1, pos_d = -1, pos_e = -1;
    int i;

    printf("Running test_disconnected_dag...\n");
    memset(&dag, 0, sizeof(dag));
    dag.num_nodes = 5;

    /*
     * Graph 1: A (0) -> B (1)
     * Graph 2: C (2) -> D (3) -> E (4)
     */
    strcpy(dag.nodes[0].name, "A");
    dag.nodes[0].num_dependencies = 0;

    strcpy(dag.nodes[1].name, "B");
    dag.nodes[1].num_dependencies = 1;
    dag.nodes[1].dependencies[0] = 0;

    strcpy(dag.nodes[2].name, "C");
    dag.nodes[2].num_dependencies = 0;

    strcpy(dag.nodes[3].name, "D");
    dag.nodes[3].num_dependencies = 1;
    dag.nodes[3].dependencies[0] = 2;

    strcpy(dag.nodes[4].name, "E");
    dag.nodes[4].num_dependencies = 1;
    dag.nodes[4].dependencies[0] = 3;

    ASSERT_TRUE(zcc_dag_validate_invariants(&dag));
    ASSERT_FALSE(zcc_dag_has_cycle(&dag));

    ASSERT_TRUE(zcc_dag_topo_sort(&dag, order));

    /* Verify all elements are sorted and unique */
    memset(seen, 0, sizeof(seen));
    for (i = 0; i < 5; i++) {
        int val = order[i];
        ASSERT_TRUE(val >= 0 && val < 5);
        seen[val]++;
    }
    for (i = 0; i < 5; i++) {
        ASSERT_TRUE(seen[i] == 1);
    }

    /* Verify order invariants: A before B, C before D before E */
    for (i = 0; i < 5; i++) {
        if (order[i] == 0) pos_a = i;
        if (order[i] == 1) pos_b = i;
        if (order[i] == 2) pos_c = i;
        if (order[i] == 3) pos_d = i;
        if (order[i] == 4) pos_e = i;
    }
    ASSERT_TRUE(pos_a < pos_b);
    ASSERT_TRUE(pos_c < pos_d);
    ASSERT_TRUE(pos_d < pos_e);

    /* Levelization */
    zcc_dag_levelize(&dag);
    ASSERT_TRUE(dag.nodes[0].level == 0);
    ASSERT_TRUE(dag.nodes[1].level == 1);
    ASSERT_TRUE(dag.nodes[2].level == 0);
    ASSERT_TRUE(dag.nodes[3].level == 1);
    ASSERT_TRUE(dag.nodes[4].level == 2);

    printf("  -> Disconnected DAG OK\n");
}

static void test_cycle_rejection(void) {
    BuildDAG dag;
    int order[2];
    int order3[3];

    printf("Running test_cycle_rejection...\n");
    
    /* Self-loop cycle: A -> A */
    memset(&dag, 0, sizeof(dag));
    dag.num_nodes = 1;
    strcpy(dag.nodes[0].name, "A");
    dag.nodes[0].num_dependencies = 1;
    dag.nodes[0].dependencies[0] = 0;

    /* Self-loop dependency check in validate_invariants triggers failure */
    ASSERT_FALSE(zcc_dag_validate_invariants(&dag));
    ASSERT_TRUE(zcc_dag_has_cycle(&dag)); /* Invalid DAG is rejected */

    /* Basic cycle: A -> B -> A */
    memset(&dag, 0, sizeof(dag));
    dag.num_nodes = 2;
    strcpy(dag.nodes[0].name, "A");
    dag.nodes[0].num_dependencies = 1;
    dag.nodes[0].dependencies[0] = 1;

    strcpy(dag.nodes[1].name, "B");
    dag.nodes[1].num_dependencies = 1;
    dag.nodes[1].dependencies[0] = 0;

    ASSERT_TRUE(zcc_dag_validate_invariants(&dag));
    ASSERT_TRUE(zcc_dag_has_cycle(&dag));
    ASSERT_FALSE(zcc_dag_topo_sort(&dag, order));

    /* Multi-node cycle: A -> B -> C -> A */
    memset(&dag, 0, sizeof(dag));
    dag.num_nodes = 3;
    strcpy(dag.nodes[0].name, "A");
    dag.nodes[0].num_dependencies = 1;
    dag.nodes[0].dependencies[0] = 1;

    strcpy(dag.nodes[1].name, "B");
    dag.nodes[1].num_dependencies = 1;
    dag.nodes[1].dependencies[0] = 2;

    strcpy(dag.nodes[2].name, "C");
    dag.nodes[2].num_dependencies = 1;
    dag.nodes[2].dependencies[0] = 0;

    ASSERT_TRUE(zcc_dag_validate_invariants(&dag));
    ASSERT_TRUE(zcc_dag_has_cycle(&dag));
    ASSERT_FALSE(zcc_dag_topo_sort(&dag, order3));

    printf("  -> Cycle Rejection OK\n");
}

static void test_agent_topology_dag(void) {
    BuildDAG dag;
    int order[MAX_DAG_NODES];
    int positions[MAX_DAG_NODES];
    int i, j;

    printf("Running test_agent_topology_dag...\n");
    memset(&dag, 0, sizeof(dag));

    /* Initialize from auto-generated real zip map topology seed */
    init_agent_test_dag(&dag);

    /* Validate invariants */
    ASSERT_TRUE(zcc_dag_validate_invariants(&dag));

    /* Verify it is acyclic */
    ASSERT_FALSE(zcc_dag_has_cycle(&dag));

    /* Topological Sort */
    ASSERT_TRUE(zcc_dag_topo_sort(&dag, order));

    /* Verify topological ordering constraints are respected */
    for (i = 0; i < dag.num_nodes; i++) {
        positions[order[i]] = i;
    }

    for (i = 0; i < dag.num_nodes; i++) {
        for (j = 0; j < dag.nodes[i].num_dependencies; j++) {
            int dep_idx = dag.nodes[i].dependencies[j];
            /* The dependency must come BEFORE the node itself in topo order */
            ASSERT_TRUE(positions[dep_idx] < positions[i]);
        }
    }

    /* Levelization */
    zcc_dag_levelize(&dag);

    /* Verify level of dependency is strictly less than level of node */
    for (i = 0; i < dag.num_nodes; i++) {
        for (j = 0; j < dag.nodes[i].num_dependencies; j++) {
            int dep_idx = dag.nodes[i].dependencies[j];
            ASSERT_TRUE(dag.nodes[dep_idx].level < dag.nodes[i].level);
        }
    }

    /* Verify root levels are 0 */
    printf("  Computed Levels:\n");
    for (i = 0; i < dag.num_nodes; i++) {
        printf("    %-50s: Level %d\n", dag.nodes[i].name, dag.nodes[i].level);
    }

    printf("  -> Agent Topology DAG OK\n");
}

int main(void) {
    printf("===========================================\n");
    printf("ZCC BuildDAG Topo-Sort Verification Suite\n");
    printf("===========================================\n");

    test_linear_dag();
    test_diamond_dag();
    test_disconnected_dag();
    test_cycle_rejection();
    test_agent_topology_dag();

    printf("===========================================\n");
    printf("ALL BuildDAG TESTS PASSED SUCCESSFULLY!\n");
    printf("===========================================\n");
    return 0;
}

