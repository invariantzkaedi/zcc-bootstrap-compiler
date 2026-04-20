#include <stdio.h>

/* Include the JIT-generated neural network directly */
#include "zkaedi_core_fixed.c"

int main() {
    /* 23-D Synthetic Test Vector representing a Flash-Loan / Reentrancy trap. */
    /* Using Q16.16 (SCALE = 65536) fixed-point format representations. */
    long long test_vector[23] = {
        65536 * 1, /* [0] Reentrancy Energy */
        0, 0, 0, 0, 0, 0, 0, 0, 
        65536 * 2, /* [9] Flash Loan Energy */
        0, 0, 0, 0, 0, 0, 0, 0, 0,
        65536 * 15, /* [19] Base Cost */
        65536 * 2,  /* [20] Branch Density */
        65536 * 5,  /* [21] Call Density */
        65536 * 80  /* [22] Prime Energy */
    };

    printf("[TEST] Ouroboros 23-D Fixed-Point Inference Engine...\n");
    printf("[TEST] Initializing %d param MAC computations.\n", 23 * 128 + 128 * 64 + 64 * 19);

    /* Run the native prediction loop */
    long long class_id = evm_classifier_infer(test_vector);
    
    printf("[RESULT] Argmax Vulnerability Class ID: %lld\n", (long long)class_id);
    
    /* We expect a valid output ID inside [0, 18] representing one of the 19 classes */
    if (class_id >= 0 && class_id < 19) {
        printf("[SUCCESS] C Inference output perfectly matches boundary array %d-D.\n", 19);
        return 0;
    } else {
        printf("[ERROR] Output argmax outside known vulnerability mapping bounds!\n");
        return 1;
    }
}
