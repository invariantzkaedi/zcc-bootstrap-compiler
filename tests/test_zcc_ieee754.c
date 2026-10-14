#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <string.h>

static uint64_t d2u(double d) {
    uint64_t u;
    memcpy(&u, &d, sizeof(u));
    return u;
}

double test_min_subnormal(void) { return 5e-324; }
double test_max_subnormal(void) { return 2.2250738585072014e-308; }
double test_subnormal_div(double a) { return (a * 5e-324) / 5e-324; }
double test_subnormal_cancel(double a) { return (a + 5e-324) - 5e-324; }
float  test_fround(double a) { return (float)a; }
double test_hypot(double a) { return hypot(a, 5e-324); }
double test_expm1(double a) { return expm1(a); }
double test_log1p(double a) { return log1p(a); }
double test_sin(double a) { return sin(a); }
double test_tan(double a) { return tan(a); }
double test_atan2(double a, double b) { return atan2(a, b); }
int    test_minus_zero_mul(double a) { double res = a * -1.0; return ((d2u(res) >> 63) != 0) && (res == 0.0); }
int64_t test_mantissa_round(double a) { return (int64_t)((a + 4503599627370496.0) - 4503599627370496.0); }
uint32_t test_float_to_uint32(double a) { return ((uint32_t)(int64_t)a); }
double test_cbrt(double a) { return cbrt(a); }

int main(void) {
    double sn1 = 5e-324;
    double f32_sn = 1.401298464324817e-45;
    float fr = test_fround(f32_sn);
    uint32_t fr_bits;
    memcpy(&fr_bits, &fr, sizeof(fr_bits));

    printf("SN_MIN:       0x%016lx\n", d2u(test_min_subnormal()));
    printf("SN_MAX:       0x%016lx\n", d2u(test_max_subnormal()));
    printf("SN_DIV:       0x%016lx\n", d2u(test_subnormal_div(1.0)));
    printf("SN_CANCEL:    0x%016lx\n", d2u(test_subnormal_cancel(1.0)));
    printf("FROUND_SN:    0x%08x\n", fr_bits);
    printf("HYPOT_SN:     0x%016lx\n", d2u(test_hypot(sn1)));
    printf("EXPM1_SN:     0x%016lx\n", d2u(test_expm1(-0.0)));
    printf("LOG1P_SN:     0x%016lx\n", d2u(test_log1p(sn1)));
    printf("SIN_SN:       0x%016lx\n", d2u(test_sin(sn1)));
    printf("TAN_SN:       0x%016lx\n", d2u(test_tan(sn1)));
    printf("ATAN2_QUAD:   0x%016lx\n", d2u(test_atan2(-0.0, -0.0)));
    printf("MINUS_ZERO:   %d\n", test_minus_zero_mul(0.0));
    printf("MANTISSA_RND: %ld\n", test_mantissa_round(12345.678));
    printf("FLOAT_TO_U32: %u\n", test_float_to_uint32(4294967295.0));
    printf("CBRT_SN:      0x%016lx\n", d2u(test_cbrt(-sn1)));

    return 0;
}
