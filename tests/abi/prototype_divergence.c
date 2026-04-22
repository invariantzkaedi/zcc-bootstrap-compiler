/* tests/abi/prototype_divergence.c
 * HYGIENE-ABI-003 probe: prototype-less forward declaration vs full prototype.
 *
 * The caller (main) sees only a forward declaration of sink_struct with no
 * parameter types — just "void sink_struct()". The callee definition has the
 * full prototype with struct-by-value. SysV ABI classification depends on
 * the callee's actual signature, but the caller must also classify correctly
 * for the call to work.
 *
 * Leg A: registers available (struct is first arg)
 * Leg B: registers exhausted (struct after 5 int args)
 */
#include <stdio.h>

struct S16 { long a; long b; };

/* Forward declaration WITHOUT parameter types (K&R style) */
void sink_a();
void sink_b();

int main(void) {
    struct S16 s = { 0xAAAA, 0xBBBB };

    printf("=== Leg A: registers available ===\n");
    sink_a(s);

    printf("=== Leg B: registers exhausted ===\n");
    sink_b(1, 2, 3, 4, 5, s);

    return 0;
}

/* Definitions WITH full prototypes — in a real scenario these would be in a
 * separate translation unit, but ZCC is single-file so we define them after
 * main to ensure the caller only saw the prototype-less declaration. */
void sink_a(struct S16 s) {
    printf("s.a=0x%lx s.b=0x%lx\n", s.a, s.b);
}

void sink_b(int a1, int a2, int a3, int a4, int a5, struct S16 s) {
    printf("ints: %d %d %d %d %d\n", a1, a2, a3, a4, a5);
    printf("s.a=0x%lx s.b=0x%lx\n", s.a, s.b);
}
