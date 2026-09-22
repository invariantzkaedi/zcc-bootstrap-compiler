/* LIMIT-002 Atomic Semantics Boundary Check
 * Milestone Scope: Syntax & Type-System Compatibility.
 * ATOMIC_SEMANTICS: NOT_CLAIMED.
 * Verifies standard variable store/load and volatile behavior without claiming lock-free hardware atomics.
 */
#include <stdio.h>

_Atomic(int) g_shared = 0;

int main(void) {
    g_shared = 42;
    int readback = g_shared;
    if (readback != 42) return 1;

    _Atomic(int) *p = &g_shared;
    *p += 10;
    if (g_shared != 52) return 2;

    return 0;
}
