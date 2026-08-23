#include "include/zcc_qasm.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>

static int g_failed = 0;

#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL: %s (line %d)\n", msg, __LINE__); \
        g_failed++; \
    } else { \
        printf("PASS: %s\n", msg); \
    } \
} while (0)

int main(void) {
    printf("=== Running ZCC QASM Simulator C API Tests ===\n");

    /* 1. Ground state & norm test */
    ZCCQasmSimulator *sim = zcc_qasm_sim_create(2, 2, 42);
    TEST_ASSERT(sim != NULL, "simulator creation (2 qubits, 2 clbits)");
    TEST_ASSERT(fabs(zcc_qasm_sim_norm(sim) - 1.0) < 1e-12, "initial ground state norm == 1.0");

    /* Separable state entropy test */
    double s_sep0 = zcc_qasm_sim_entropy_1q(sim, 0);
    double s_sep1 = zcc_qasm_sim_entropy_1q(sim, 1);
    TEST_ASSERT(fabs(s_sep0 - 0.0) < 1e-10, "separable ground state entropy S(q0) == 0.0");
    TEST_ASSERT(fabs(s_sep1 - 0.0) < 1e-10, "separable ground state entropy S(q1) == 0.0");

    /* 2. Bell State Creation & Verification */
    const char *bell_qasm = 
        "OPENQASM 2.0;\n"
        "include \"qelib1.inc\";\n"
        "qreg q[2];\n"
        "h q[0];\n"
        "cx q[0], q[1];\n";
    char err_buf[256] = {0};
    ZCCQasmCircuit *bell_circ = zcc_qasm_parse_string(bell_qasm, "bell.qasm", err_buf, sizeof(err_buf));
    TEST_ASSERT(bell_circ != NULL, "parse bell state QASM");
    TEST_ASSERT(zcc_qasm_sim_apply_circuit(sim, bell_circ), "apply bell circuit to simulator");

    double norm_bell = zcc_qasm_sim_norm(sim);
    TEST_ASSERT(fabs(norm_bell - 1.0) < 1e-12, "Bell state norm conservation == 1.0");

    /* Entanglement Entropy of Bell State */
    double s_bell0 = zcc_qasm_sim_entropy_1q(sim, 0);
    double s_bell1 = zcc_qasm_sim_entropy_1q(sim, 1);
    printf("Bell state S(q0) = %.6f bits, S(q1) = %.6f bits\n", s_bell0, s_bell1);
    TEST_ASSERT(fabs(s_bell0 - 1.0) < 1e-6, "Bell state entanglement entropy S(q0) == 1.0 bit");
    TEST_ASSERT(fabs(s_bell1 - 1.0) < 1e-6, "Bell state entanglement entropy S(q1) == 1.0 bit");

    /* Amplitude verification for Bell State */
    TEST_ASSERT(fabs(sim->amplitudes[0].real - 1.0 / sqrt(2.0)) < 1e-10, "Bell amp[00].real == 1/sqrt(2)");
    TEST_ASSERT(fabs(sim->amplitudes[3].real - 1.0 / sqrt(2.0)) < 1e-10, "Bell amp[11].real == 1/sqrt(2)");
    TEST_ASSERT(fabs(sim->amplitudes[1].real) < 1e-10, "Bell amp[01].real == 0.0");
    TEST_ASSERT(fabs(sim->amplitudes[2].real) < 1e-10, "Bell amp[10].real == 0.0");

    zcc_qasm_circuit_free(bell_circ);
    zcc_qasm_sim_free(sim);

    /* 3. 3-Qubit GHZ State Entanglement Entropy */
    const char *ghz_qasm = 
        "OPENQASM 2.0;\n"
        "include \"qelib1.inc\";\n"
        "qreg q[3];\n"
        "h q[0];\n"
        "cx q[0], q[1];\n"
        "cx q[1], q[2];\n";
    ZCCQasmCircuit *ghz_circ = zcc_qasm_parse_string(ghz_qasm, "ghz.qasm", err_buf, sizeof(err_buf));
    TEST_ASSERT(ghz_circ != NULL, "parse GHZ state QASM");

    ZCCQasmSimulator *ghz_sim = zcc_qasm_sim_create(3, 0, 100);
    TEST_ASSERT(zcc_qasm_sim_apply_circuit(ghz_sim, ghz_circ), "apply GHZ circuit");

    double norm_ghz = zcc_qasm_sim_norm(ghz_sim);
    TEST_ASSERT(fabs(norm_ghz - 1.0) < 1e-12, "GHZ state norm == 1.0");

    double s_ghz0 = zcc_qasm_sim_entropy_1q(ghz_sim, 0);
    double s_ghz1 = zcc_qasm_sim_entropy_1q(ghz_sim, 1);
    double s_ghz2 = zcc_qasm_sim_entropy_1q(ghz_sim, 2);
    printf("GHZ state S(q0) = %.6f bits, S(q1) = %.6f bits, S(q2) = %.6f bits\n", s_ghz0, s_ghz1, s_ghz2);
    TEST_ASSERT(fabs(s_ghz0 - 1.0) < 1e-6, "GHZ state single-qubit entropy S(q0) == 1.0 bit");
    TEST_ASSERT(fabs(s_ghz1 - 1.0) < 1e-6, "GHZ state single-qubit entropy S(q1) == 1.0 bit");
    TEST_ASSERT(fabs(s_ghz2 - 1.0) < 1e-6, "GHZ state single-qubit entropy S(q2) == 1.0 bit");

    zcc_qasm_circuit_free(ghz_circ);
    zcc_qasm_sim_free(ghz_sim);

    /* 4. Safety limits verification */
    ZCCQasmSimulator *oversized_sim = zcc_qasm_sim_create(32, 0, 1);
    TEST_ASSERT(oversized_sim == NULL, "reject oversized simulation (> 28 qubits)");

    printf("=== Summary: %s (failures: %d) ===\n", (g_failed == 0) ? "ALL PASS" : "FAILED", g_failed);
    return g_failed ? 1 : 0;
}
