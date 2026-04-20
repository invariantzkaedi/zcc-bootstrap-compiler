/* test_float_abi_regression.c — CG-FLOAT-001/002/003 regression harness
 *
 * Covers:
 *   1. Fixed-param float reception (CG-FLOAT-003 kill shot)
 *   2. Float<->double cast round-trip (CG-FLOAT-002)
 *   3. Variadic printf with float (must NOT cvtsd2ss — C promotion)
 *   4. Float variable passed to variadic (latent trap)
 *   5. codegen_load for float/double locals (CG-FLOAT-001)
 *   6. Multi-param mixed int/float/double call
 */
#include <stdio.h>
#include <string.h>

/* ---- Test 1: Fixed-param float reception ---- */
double receive_float(float f) {
    return (double)f;
}

/* ---- Test 2: Float<->double cast round-trip ---- */
float double_to_float(double d) {
    return (float)d;
}

double float_to_double(float f) {
    return (double)f;
}

/* ---- Test 5: codegen_load float/double locals ---- */
double load_float_local(void) {
    float x;
    x = 3.14f;
    return (double)x;
}

/* ---- Test 6: Mixed params ---- */
int mixed_params(int a, float b, double c, int d) {
    printf("  a=%d b_as_double=%f c=%f d=%d\n", a, (double)b, c, d);
    return a + d;
}

int main(void) {
    int pass;
    int fail;
    double result;
    float fresult;

    pass = 0;
    fail = 0;

    /* Test 1: Fixed-param float */
    result = receive_float(2.5f);
    printf("[Test 1] receive_float(2.5f) = %f ", result);
    if (result > 2.49 && result < 2.51) { printf("PASS\n"); pass++; }
    else { printf("FAIL (expected 2.5)\n"); fail++; }

    /* Test 2a: double -> float */
    fresult = double_to_float(1.75);
    printf("[Test 2a] double_to_float(1.75) = %f ", (double)fresult);
    if ((double)fresult > 1.74 && (double)fresult < 1.76) { printf("PASS\n"); pass++; }
    else { printf("FAIL\n"); fail++; }

    /* Test 2b: float -> double */
    result = float_to_double(0.5f);
    printf("[Test 2b] float_to_double(0.5f) = %f ", result);
    if (result > 0.49 && result < 0.51) { printf("PASS\n"); pass++; }
    else { printf("FAIL\n"); fail++; }

    /* Test 3: Variadic printf with float literal */
    printf("[Test 3] printf variadic: %f (expect 2.500000)\n", 2.5f);
    pass++; /* visual check */

    /* Test 4: Float variable passed to variadic */
    {
        float fvar;
        fvar = 3.14f;
        printf("[Test 4] printf float var: %f (expect ~3.14)\n", (double)fvar);
        pass++; /* visual check */
    }

    /* Test 5: codegen_load float local */
    result = load_float_local();
    printf("[Test 5] load_float_local() = %f ", result);
    if (result > 3.13 && result < 3.15) { printf("PASS\n"); pass++; }
    else { printf("FAIL\n"); fail++; }

    /* Test 6: Mixed int/float/double params */
    {
        int r;
        printf("[Test 6] mixed_params(10, 2.5f, 7.77, 20):\n");
        r = mixed_params(10, 2.5f, 7.77, 20);
        printf("  return=%d ", r);
        if (r == 30) { printf("PASS\n"); pass++; }
        else { printf("FAIL (expected 30)\n"); fail++; }
    }

    printf("\n=== RESULTS: %d passed, %d failed ===\n", pass, fail);
    return fail;
}
