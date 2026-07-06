#include <stdio.h>
#include <assert.h>
#include "zqec.h"

// Mock definitions for missing quantum functions to allow standalone test linking
void quantum_init_state(QuantumState *q) {}
void quantum_evolve(QuantumState *q, ir_node_t *insn, double H_field[32][32]) {}
void quantum_measure_and_collapse(QuantumState *q, ir_node_t *insn, FILE *out) {}

// Helper to initialize a clean Pauli vector
pauli_vec_t pauli_init(int n) {
    pauli_vec_t p;
    p.n_qubits = n;
    for (int i = 0; i < 32; i++) {
        p.x[i] = 0;
        p.z[i] = 0;
    }
    return p;
}

void test_cnot_x_control_spreads_to_target(void) {
    pauli_vec_t p = pauli_init(2);
    p.x[0] = 1;               // Xc
    prop_cnot(&p, 0, 1);
    assert(p.x[0] == 1);
    assert(p.x[1] == 1);      // Xt added
    printf("  [PASS] test_cnot_x_control_spreads_to_target\n");
}

void test_cnot_z_target_spreads_to_control(void) {
    pauli_vec_t p = pauli_init(2);
    p.z[1] = 1;               // Zt
    prop_cnot(&p, 0, 1);
    assert(p.z[1] == 1);
    assert(p.z[0] == 1);      // Zc added
    printf("  [PASS] test_cnot_z_target_spreads_to_control\n");
}

void test_cz_x_gets_neighbor_z(void) {
    pauli_vec_t p = pauli_init(2);
    p.x[0] = 1;               // Xa
    prop_cz(&p, 0, 1);
    assert(p.x[0] == 1);
    assert(p.z[1] == 1);      // Zb added
    printf("  [PASS] test_cz_x_gets_neighbor_z\n");
}

int main(void) {
    printf("Running focused unit tests for ZQEC propagation rules...\n");
    test_cnot_x_control_spreads_to_target();
    test_cnot_z_target_spreads_to_control();
    test_cz_x_gets_neighbor_z();
    printf("ALL ZQEC PROPAGATION TESTS PASSED!\n");
    return 0;
}
