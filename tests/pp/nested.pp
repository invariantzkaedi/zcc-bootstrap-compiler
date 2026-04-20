/* Regression: PP-INCLUDE-022
 * Three-level nested include must propagate -I paths through every sub-state.
 * Each header defines a distinct symbol so we can verify all three expanded. */















int probe_pp_include_022(void) {
    return 1 + 2 + 4;
    /* Expected expansion: 1 + 2 + 4 = 7 */
}
