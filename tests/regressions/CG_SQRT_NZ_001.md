# CG-SQRT-NZ-001

## sqrt(-0.0) Constant-Folder Sign Loss

| Field | Value |
|-------|-------|
| **Bug ID** | CG-SQRT-NZ-001 |
| **Category** | IEEE-754 constant folding |
| **Root Cause** | `det_sqrt_d()` in `part4.c` returned literal `0.0` for zero input |
| **Fix** | `return x` instead of `return 0.0` — preserves sign bit |
| **IEEE-754 Spec** | §5.4.1: `sqrt(-0.0) = -0.0`, `sqrt(+0.0) = +0.0` |
| **Discovery** | `ieee754_torture.c` test 14b |

## Divergence

```
          runtime     constfold
ZCC:   8000000000  0000000000   ← const-fold loses -0.0 sign
GCC:   8000000000  8000000000   ← both correct
```

## Fix

```diff
 static double det_sqrt_d(double x) {
     if (x < 0.0) return 0.0/0.0;
-    if (x == 0.0) return 0.0;
+    if (x == 0.0) return x;   /* IEEE-754: sqrt(-0.0) = -0.0, sqrt(+0.0) = +0.0 */
```

## Reproduction

```bash
# Constant-fold path:
echo '#include <stdio.h>
#include <math.h>
#include <string.h>
int main(void) {
    double r = sqrt(-0.0);
    unsigned long long u;
    memcpy(&u, &r, sizeof(u));
    printf("bits=%016llx\n", u);
    return 0;
}' > /tmp/t.c
./zcc -I./zcc_sys_includes /tmp/t.c -o /tmp/t.s
gcc /tmp/t.s -o /tmp/t -lm && /tmp/t
# Expected: bits=8000000000000000
```

## Status

- [x] Root cause identified
- [x] Fix applied in `part4.c:det_sqrt_d()`
- [x] Selfhost verified (Stage 2 ↔ Stage 3 assembly identical)
- [x] Olympics gate passing (🏆 GOLD, all 12 ABI buckets covered)
