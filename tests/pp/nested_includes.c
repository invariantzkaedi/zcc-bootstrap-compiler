/* Regression: PP-INCLUDE-022
 * Three-level nested include must propagate -I paths through every sub-state.
 * Each header defines a distinct symbol so we can verify all three expanded. */
#include "nested_a.h"
int probe_pp_include_022(void) {
    return NESTED_A_VALUE + NESTED_B_VALUE + NESTED_C_VALUE;
    /* Expected expansion: 1 + 2 + 4 = 7 */
}
