#include <stdio.h>
#include <string.h>

int main(void) {
    int fail = 0;

    /* double -> float narrowing in assignment */
    double d = 3.14159265358979;
    float f = d;
    float expected_f = 3.1415927f;
    unsigned int fb;
    unsigned int eb;
    memcpy(&fb, &f, 4);
    memcpy(&eb, &expected_f, 4);
    printf("double->float: got=0x%08X expected=0x%08X %s\n",
           fb, eb, fb == eb ? "PASS" : "FAIL");
    if (fb != eb) fail++;

    /* double -> int truncation */
    double dd = 3.7;
    int i = dd;
    printf("double->int trunc: got=%d expected=3 %s\n",
           i, i == 3 ? "PASS" : "FAIL");
    if (i != 3) fail++;

    /* negative double -> int truncation (toward zero) */
    double dn = -3.7;
    int in_val = dn;
    printf("neg double->int: got=%d expected=-3 %s\n",
           in_val, in_val == -3 ? "PASS" : "FAIL");
    if (in_val != -3) fail++;

    /* float -> int truncation */
    float ff = 2.9f;
    int fi = ff;
    printf("float->int trunc: got=%d expected=2 %s\n",
           fi, fi == 2 ? "PASS" : "FAIL");
    if (fi != 2) fail++;

    /* negative float -> int */
    float fneg = -2.9f;
    int fni = fneg;
    printf("neg float->int: got=%d expected=-2 %s\n",
           fni, fni == -2 ? "PASS" : "FAIL");
    if (fni != -2) fail++;

    /* int -> float (representable small int) */
    int smallint = 42;
    float from_int = smallint;
    float exp_fi = 42.0f;
    unsigned int fib;
    unsigned int efib;
    memcpy(&fib, &from_int, 4);
    memcpy(&efib, &exp_fi, 4);
    printf("int->float: got=0x%08X expected=0x%08X %s\n",
           fib, efib, fib == efib ? "PASS" : "FAIL");
    if (fib != efib) fail++;

    /* long -> int narrowing (high bits dropped) */
    long ll = 0x123456789ABCDEF0L;
    int truncated = (int)ll;
    int expected_trunc = (int)0x9ABCDEF0;
    printf("long->int: got=0x%08X expected=0x%08X %s\n",
           truncated, expected_trunc, truncated == expected_trunc ? "PASS" : "FAIL");
    if (truncated != expected_trunc) fail++;

    /* double -> short */
    double ds = 300.7;
    short s = ds;
    printf("double->short: got=%d expected=300 %s\n",
           s, s == 300 ? "PASS" : "FAIL");
    if (s != 300) fail++;

    /* int -> char with sign */
    int big = 0x12345678;
    char c = big;
    int c_as_int = c;
    int expected_c = 0x78;
    printf("int->char: got=0x%02X expected=0x%02X %s\n",
           c_as_int & 0xFF, expected_c, (c_as_int & 0xFF) == expected_c ? "PASS" : "FAIL");
    if ((c_as_int & 0xFF) != expected_c) fail++;

    printf("=== %d failures ===\n", fail);
    return fail;
}
