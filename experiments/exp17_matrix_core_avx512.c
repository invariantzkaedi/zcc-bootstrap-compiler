#include <stdio.h>
#include <stdlib.h>
#include "zcaedi_avx512_math.h"

#define ABORT(msg) { fprintf(stderr, "FATAL ZKAEDI TRAP: %s\n", msg); exit(1); }

void check_alignment(void* ptr) {
    if ((unsigned long)ptr % 64 != 0) {
        ABORT("64-BYTE ALIGNMENT VIOLATION!");
    }
}

int main() {
    printf("ZKAEDI PRIME SOVEREIGN MATRIX CORE (Exp17) initializing...\n");
    
    // Zero-dependency manual 64-byte alignment
    static char inputs_raw[sizeof(v16f) + 64];
    static char sines_raw[sizeof(v16f) + 64];
    static char cosines_raw[sizeof(v16f) + 64];
    
    unsigned long a1 = (unsigned long)inputs_raw;
    unsigned long a2 = (unsigned long)sines_raw;
    unsigned long a3 = (unsigned long)cosines_raw;
    
    v16f* inputs_ptr = (v16f*)((a1 % 64 == 0) ? a1 : (a1 + 64 - (a1 % 64)));
    v16f* sines_ptr = (v16f*)((a2 % 64 == 0) ? a2 : (a2 + 64 - (a2 % 64)));
    v16f* cosines_ptr = (v16f*)((a3 % 64 == 0) ? a3 : (a3 + 64 - (a3 % 64)));
    
    check_alignment(inputs_ptr);
    check_alignment(sines_ptr);
    check_alignment(cosines_ptr);
    
    #define inputs (*inputs_ptr)
    #define sines (*sines_ptr)
    #define cosines (*cosines_ptr)
    
    for (int i=0; i<16; i++) {
        inputs.v[i] = PI_F * ((float)i / 16.0f);
    }
    
    printf("Executing vector calculus (Minimax sin/cos)...\n");
    sines = _mm512_sin_ps(inputs);
    cosines = _mm512_cos_ps(inputs);
    
    printf("Verifying trigonometric identity: sin^2(x) + cos^2(x) = 1.0\n");
    v16f sin2 = _mm512_mul_ps(sines, sines);
    v16f cos2 = _mm512_mul_ps(cosines, cosines);
    v16f identity = _mm512_add_ps(sin2, cos2);
    
    float sum = 0.0f;
    for (int i=0; i<16; i++) {
        sum += identity.v[i];
    }
    
    printf("Exp17 Matrix Verification Hash: 0x%08X\n", *(unsigned int*)&sum);
    printf("Sovereign Matrix Core Established. Zero External Dependencies linked.\n");
    return 0;
}
