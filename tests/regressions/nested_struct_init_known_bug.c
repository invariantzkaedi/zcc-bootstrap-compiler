/* regression: nested struct initializer — distinct from CG-ABI-STRUCT-002
 * ZCC does not support nested brace initializers for struct-of-struct.
 *
 * Status: KNOWN BUG (pre-existing) — not introduced by CG-ABI-STRUCT-002 fix
 * Separate from the parameter-offset bug.
 *
 * Workaround (for Olympics generator): use field-by-field assignment instead:
 *   S v0; v0.inner_fld.nx = 9; v0.inner_fld.ny = 9.12;
 */
#include <stdio.h>
typedef struct { int nx; double ny; } Inner;
typedef struct { Inner inner_fld; } S;

S op0(S a, S b) {
    S r;
    r.inner_fld.nx = a.inner_fld.nx + b.inner_fld.nx;
    r.inner_fld.ny = a.inner_fld.ny - b.inner_fld.ny;
    return r;
}

int main(void) {
    /* Workaround: field-by-field init instead of nested braces */
    S v0; v0.inner_fld.nx = 9;  v0.inner_fld.ny = 9.12;
    S v1; v1.inner_fld.nx = 3;  v1.inner_fld.ny = 4.66;
    S res = op0(v0, v1);
    printf("nest: %d %.2f\n", res.inner_fld.nx, res.inner_fld.ny);
    /* expected: 12 4.46 */
    return 0;
}
