// ==============================================================================
// ZCC POLYGLOT LTO CRUCIBLE
// AESTHETIC: DEEP NAVY / CYAN & MAGENTA NEON // SPACE MONO
// ==============================================================================

// FFI Boundary Definition
extern int compute_hash(int limit);

int main() {
    int limit = 10;
    
    // Cross-domain execution. Rust guest takes control.
    int hash = compute_hash(limit);
    
    // limit = 10 (0 to 9)
    // Sum of (i * 17) for i in 0..9 = 17 * 45 = 765
    // 765 & 0xFF = 253
    
    return hash & 0xFF;
}
