/* fractal.c — ZCC XMM floating-point stress kernel (ANSI C89)
 *
 * DELIBERATE PRESSURE POINTS:
 *   1. >8 simultaneously live doubles in mandel_iter() → forces spill past xmm7
 *   2. Scalar FP ops: movsd / addsd / subsd / mulsd / divsd / ucomisd
 *   3. Int→double conversion (cvtsi2sd) via (double)px * 3.5 / WIDTH
 *   4. SysV FP ABI: printf %f forces xmm0 arg + mov al,N varargs rule
 *   5. Struct-return FP: complex_t cmul() returns {double,double} in xmm0:xmm1
 *   6. Deterministic boundary tests: known math constants → any codegen bug surfaces
 *
 * REFERENCE BUILD (expected bit-identical output):
 *     gcc -O0 -o fractal_ref fractal.c
 *
 * ZCC BUILD:
 *     ./zcc -S fractal.c -o fractal.s
 *     ./zcc --ir --telemetry fractal.c -o fractal.o
 *     gcc -o fractal_zcc fractal.s
 *
 * NO C99. NO stdint.h. NO VLAs. NO designated initializers. Strict C89.
 */

#include <stdio.h>

#define WIDTH    60
#define HEIGHT   30
#define MAX_ITER 50

/* ---- Primary kernel: 10 simultaneously live doubles ----
 *
 * Live at inner-loop top:   zx, zy, zx2, zy2, dzx, dzy, temp, escape, cx, cy
 * That is 10 doubles. SysV gives us xmm0-xmm7 caller-saved; xmm8-xmm15 also
 * caller-saved. A correct register allocator MUST spill at least 2 to stack.
 *
 * The derivative (dzx, dzy) update is mathematically redundant for the escape
 * test but exists ONLY to inflate register pressure. Do not remove.
 */
static int mandel_iter(double cx, double cy)
{
    double zx, zy;
    double zx2, zy2;
    double dzx, dzy;
    double temp;
    double escape;
    int i;

    zx = 0.0;
    zy = 0.0;
    dzx = 1.0;
    dzy = 0.0;
    escape = 4.0;

    for (i = 0; i < MAX_ITER; i++) {
        zx2 = zx * zx;
        zy2 = zy * zy;
        if (zx2 + zy2 > escape) {
            return i;
        }
        /* derivative: dz' = 2*z*dz + 1 */
        temp = 2.0 * (zx * dzx - zy * dzy) + 1.0;
        dzy  = 2.0 * (zx * dzy + zy * dzx);
        dzx  = temp;
        /* state: z' = z^2 + c */
        temp = zx2 - zy2 + cx;
        zy   = 2.0 * zx * zy + cy;
        zx   = temp;
    }
    return MAX_ITER;
}

/* ---- Struct-return FP ABI probe ----
 *
 * SysV AMD64: a struct {double, double} is returned in xmm0:xmm1 (class SSE+SSE).
 * A compiler that gets this wrong will return via memory/rax → wrong values.
 */
typedef struct {
    double re;
    double im;
} complex_t;

static complex_t cmul(complex_t a, complex_t b)
{
    complex_t r;
    r.re = a.re * b.re - a.im * b.im;
    r.im = a.re * b.im + a.im * b.re;
    return r;
}

/* ---- Int→double conversion probe ---- */
static double ramp_pos(int idx, int total, double lo, double hi)
{
    return lo + (double)idx * (hi - lo) / (double)total;
}

int main(void)
{
    int px, py, iter;
    double cx, cy;
    char ch;
    const char *ramp = " .:-=+*#%@";
    complex_t a, b, c;

    /* Probe 1: struct-return FP ABI ----------------------------------- */
    a.re = 1.5;  a.im = -0.5;
    b.re = 2.0;  b.im = 0.5;
    c = cmul(a, b);
    /* (1.5*2.0 - (-0.5)*0.5, 1.5*0.5 + (-0.5)*2.0) = (3.25, -0.25) */
    printf("cmul: re=%.6f im=%.6f\n", c.re, c.im);

    /* Probe 2: ASCII fractal (integration test) ----------------------- */
    for (py = 0; py < HEIGHT; py++) {
        cy = ramp_pos(py, HEIGHT, -1.2, 1.2);
        for (px = 0; px < WIDTH; px++) {
            cx = ramp_pos(px, WIDTH, -2.5, 1.0);
            iter = mandel_iter(cx, cy);
            if (iter >= MAX_ITER) {
                ch = '@';
            } else {
                ch = ramp[iter * 9 / MAX_ITER];
            }
            putchar(ch);
        }
        putchar('\n');
    }

    /* Probe 3: deterministic boundary values -------------------------
     * Escape test |z|² > 4 runs BEFORE z update, so minimum return is 1.
     * Expected values derived analytically:
     *   (0,0)    : fixed at 0              → MAX_ITER
     *   (-1,0)   : 2-cycle {0, -1}         → MAX_ITER
     *   (0.25,0) : converges to ~0.366     → MAX_ITER (edge of cardioid)
     *   (2,0)    : 0 → 2 → 6, 36>4 at i=2  → 2
     *   (-2,0)   : 0 → -2 → 2 → 2 → ...    → MAX_ITER (trapped at z=2)
     *   (3,0)    : 0 → 3, 9>4 at i=1       → 1
     */
    printf("boundary:\n");
    printf("  mandel( 0.00, 0.00) expect=%d got=%d\n",
           MAX_ITER, mandel_iter(0.0, 0.0));
    printf("  mandel(-1.00, 0.00) expect=%d got=%d\n",
           MAX_ITER, mandel_iter(-1.0, 0.0));
    printf("  mandel( 0.25, 0.00) expect=%d got=%d\n",
           MAX_ITER, mandel_iter(0.25, 0.0));
    printf("  mandel( 2.00, 0.00) expect=2 got=%d\n",
           mandel_iter(2.0, 0.0));
    printf("  mandel(-2.00, 0.00) expect=%d got=%d\n",
           MAX_ITER, mandel_iter(-2.0, 0.0));
    printf("  mandel( 3.00, 0.00) expect=1 got=%d\n",
           mandel_iter(3.0, 0.0));

    /* Probe 4: FP comparison edge cases ------------------------------ */
    {
        double t;
        t = 0.1 + 0.2;       /* classic 0.30000000000000004 */
        printf("fp_cmp: 0.1+0.2 == 0.3 ? %d  (expect 0)\n",
               t == 0.3);
        printf("fp_cmp: 0.1+0.2 > 0.3 ? %d  (expect 1)\n",
               t > 0.3);
    }

    return 0;
}
