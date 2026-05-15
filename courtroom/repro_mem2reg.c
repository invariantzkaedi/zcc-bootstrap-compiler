// ==============================================================================
// ZCC MEM2REG & ESCAPE ANALYSIS CRUCIBLE
// AESTHETIC: DEEP NAVY / CYAN & MAGENTA NEON // SPACE MONO
// ==============================================================================

extern void opaque_sink(int *ptr);
extern int opaque_source(void);

// ------------------------------------------------------------------------------
// TOPOLOGY 1: THE CONDITIONAL ESCAPE
// Tests if the Escape Analysis (E1-E4) correctly propagates escape state 
// backward through PHI nodes and conditional branches. 
// If promoted to a register, 'x' will corrupt because opaque_sink mutates it.
// ------------------------------------------------------------------------------
int test_conditional_escape(int flag) {
    int x = 42; 
    int *p = &x;
    
    if (flag) {
        // E3 Escape: Pointer passed as argument to external call
        opaque_sink(p); 
    } else {
        x = 100;
    }
    
    // If Mem2Reg illegally promoted 'x', it will return 100 or 42 here, 
    // missing the mutation from opaque_sink.
    return x; 
}

// ------------------------------------------------------------------------------
// TOPOLOGY 2: THE DOMINANCE FRONTIER MATRIX
// Tests Iterated Dominance Frontier (IDF) calculation. 
// 'accumulator' is mutated across multiple intersecting loops and branches.
// Mem2Reg MUST insert OP_PHI nodes at the exact loop headers and merge blocks.
// ------------------------------------------------------------------------------
int test_idf_matrix(int limit) {
    int accumulator = 0;
    int i = 0;
    
    while (i < limit) {
        if (i % 2 == 0) {
            accumulator = accumulator + i;
        } else {
            int temp = opaque_source();
            if (temp < 0) {
                break; // Premature exit creates complex reachability
            }
            accumulator = accumulator + temp;
        }
        i = i + 1;
    }
    
    return accumulator;
}

// ------------------------------------------------------------------------------
// TOPOLOGY 3: THE PROVENANCE DECOY
// Tests flow-insensitive points-to tracking.
// 'safe_var' never escapes and MUST be promoted to a register.
// 'leak_var' escapes via return and MUST remain on the stack.
// ------------------------------------------------------------------------------
int* test_provenance_decoy(void) {
    int safe_var = 77;
    static int leak_var = 88; // Static so returning it is valid C
    
    int *p1 = &safe_var;
    int *p2 = &leak_var;
    
    *p1 = *p1 + 10;
    
    // safe_var is mathematically confined to this activation record.
    // If the compiler fails to promote it to a virtual register, the pass is weak.
    if (*p1 == 87) {
        return p2; // E1 Escape: Pointer returned
    }
    
    return 0;
}

int main() {
    int res1 = test_conditional_escape(0);
    int res2 = test_idf_matrix(10);
    int* res3 = test_provenance_decoy();
    return (res1 + res2 + (res3 != 0)) & 0xFF;
}
