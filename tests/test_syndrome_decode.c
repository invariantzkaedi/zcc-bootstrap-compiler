#include <stdio.h>
#include <assert.h>
#include <math.h>
#include "zqec.h"

// Mock definitions for missing quantum functions to allow standalone test linking
void quantum_init_state(QuantumState *q) {}
void quantum_evolve(QuantumState *q, ir_node_t *insn, double H_field[32][32]) {}
void quantum_measure_and_collapse(QuantumState *q, ir_node_t *insn, FILE *out) {}

void inject_fault(QECState *qec, int err_type) {
    // Clear syndrome
    for (int i = 0; i < 9; i++) qec->syndrome[i] = 0.0;
    
    if (err_type == 1) { // X error
        qec->syndrome[0] = 1.0;
        qec->syndrome[1] = 0.5;
        qec->syndrome[2] = 0.25;
    } else if (err_type == 2) { // Z error
        qec->syndrome[3] = 1.0;
        qec->syndrome[4] = 0.5;
        qec->syndrome[5] = 0.25;
    } else if (err_type == 3) { // Y error
        qec->syndrome[0] = 1.0;
        qec->syndrome[1] = 0.5;
        qec->syndrome[2] = 0.25;
        qec->syndrome[3] = 1.0;
        qec->syndrome[4] = 0.5;
        qec->syndrome[5] = 0.25;
    }
}

void test_syndrome_corrections(void) {
    QECState q;
    
    // 1. Check clean no-error scenario
    qec_init(&q);
    int err = qec_detect_error(&q);
    assert(err == 0);
    printf("  [PASS] test_no_error_detected\n");
    
    // 2. Check X fault correction
    qec_init(&q);
    inject_fault(&q, 1);
    err = qec_detect_error(&q);
    assert(err == 1); // X error code
    printf("  [PASS] test_x_error_detected\n");
    
    // 3. Check Z fault correction
    qec_init(&q);
    inject_fault(&q, 2);
    err = qec_detect_error(&q);
    assert(err == 2); // Z error code
    printf("  [PASS] test_z_error_detected\n");
    
    // 4. Check Y fault correction
    qec_init(&q);
    inject_fault(&q, 3);
    err = qec_detect_error(&q);
    assert(err == 3); // Y error code (Z and X combined)
    printf("  [PASS] test_y_error_detected\n");
}

int main(void) {
    printf("Running syndrome-to-correction accuracy tests...\n");
    test_syndrome_corrections();
    printf("ALL SYNDROME-TO-CORRECTION TESTS PASSED!\n");
    return 0;
}
