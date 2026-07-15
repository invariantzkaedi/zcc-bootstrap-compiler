#include <stdio.h>
#include <math.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifdef isnan
#undef isnan
#endif
#define isnan(x) ((x) != (x))

union DoubleBits {
    double d;
    unsigned long long u;
};

int inspect_sign(double x) {
    union DoubleBits u;
    u.d = x;
    return (u.u >> 63) & 1;
}

int inspect_exponent(double x) {
    union DoubleBits u;
    u.d = x;
    unsigned long long exp = (u.u >> 52) & 0x7FF;
    if (exp == 0) return -1022;
    return (int)exp - 1023;
}

unsigned long long inspect_mantissa(double x) {
    union DoubleBits u;
    u.d = x;
    return u.u & 0xFFFFFFFFFFFFFULL;
}

double ulp(double x) {
    union DoubleBits u;
    u.d = x;
    if (x > 0.0) {
        u.u += 1;
    } else if (x < 0.0) {
        u.u -= 1;
    } else {
        return 4.9406564584124654e-324;
    }
    double diff = u.d - x;
    return diff >= 0 ? diff : -diff;
}

double naive_sum(double *numbers, int count) {
    double sum = 0.0;
    for (int i = 0; i < count; i++) {
        sum += numbers[i];
    }
    return sum;
}

double kahan_sum(double *numbers, int count) {
    double sum = 0.0;
    double c = 0.0;
    for (int i = 0; i < count; i++) {
        double y = numbers[i] - c;
        double t = sum + y;
        c = (t - sum) - y;
        sum = t;
    }
    return sum;
}

double neumaier_sum(double *numbers, int count) {
    double sum = 0.0;
    double c = 0.0;
    for (int i = 0; i < count; i++) {
        double x = numbers[i];
        double t = sum + x;
        double diff;
        if (sum >= x || sum >= -x) {
            diff = (sum - t) + x;
        } else {
            diff = (x - t) + sum;
        }
        c += diff;
        sum = t;
    }
    return sum + c;
}

double integrate_sin(int n) {
    double h = M_PI / (n - 1);
    double sum_odd = 0.0;
    double sum_even = 0.0;
    for (int i = 1; i < n - 1; i++) {
        double x = i * h;
        double val = sin(x);
        if (i % 2 == 1) {
            sum_odd += val;
        } else {
            sum_even += val;
        }
    }
    double y_start = sin(0.0);
    double y_end = sin(M_PI);
    return (h / 3.0) * (y_start + 4.0 * sum_odd + 2.0 * sum_even + y_end);
}

int check_fail(int check_id, int condition, const char *name) {
    if (!condition) {
        printf("  FAIL [test_%s:%d]\n", name, check_id);
        return 1;
    }
    return 0;
}

int check_fail_d(int check_id, int condition, const char *name, double val) {
    if (!condition) {
        printf("  FAIL [test_%s:%d] val=%e\n", name, check_id, val);
        return 1;
    }
    return 0;
}

int main() {
    printf("ZCC FLOAT GAUNTLET — IEEE 754 binary64 conformance\n");
    int failures = 0;

    // 1. Epsilon properties (1-4)
    float eps32_absorbed = 1.0f + 1e-8f;
    float eps32_detected = 1.0f + 1e-6f;
    double eps64_absorbed = 1.0 + 1e-17;
    double eps64_detected = 1.0 + 1e-15;

    failures += check_fail(1, eps32_absorbed == 1.0f, "epsilon_absorbed_32");
    failures += check_fail(2, eps32_detected != 1.0f, "epsilon_detected_32");
    failures += check_fail(3, eps64_absorbed == 1.0, "epsilon_absorbed_64");
    failures += check_fail(4, eps64_detected != 1.0, "epsilon_detected_64");

    // 2. ULP Spacing Checks (5-8)
    double u1 = ulp(1.0);
    double u10 = ulp(10.0);
    double u1e10 = ulp(1e10);
    double u1e20 = ulp(1e20);

    failures += check_fail_d(5, u1 > 2.22e-16 && u1 < 2.23e-16, "ulp_1_0", u1);
    failures += check_fail_d(6, u10 > 1.77e-15 && u10 < 1.78e-15, "ulp_10_0", u10);
    failures += check_fail_d(7, u1e10 > 1.90e-6 && u1e10 < 1.91e-6, "ulp_1e10", u1e10);
    failures += check_fail_d(8, u1e20 > 1.63e4 && u1e20 < 1.64e4, "ulp_1e20", u1e20);

    // 3. Anatomy Checks (9-12)
    failures += check_fail(9, inspect_sign(0.1) == 0, "sign_0_1");
    failures += check_fail(10, inspect_exponent(0.1) == -4, "exponent_0_1");
    failures += check_fail(11, inspect_mantissa(0.1) == 0x999999999999aULL, "mantissa_0_1");
    failures += check_fail(12, inspect_mantissa(0.5) == 0ULL, "mantissa_0_5");

    // 4. Cancellation gauntlet (13-15)
    static double numbers[10002];
    numbers[0] = 1e16;
    for (int i = 1; i <= 10000; i++) {
        numbers[i] = 1.0;
    }
    numbers[10001] = -1e16;

    double naive = naive_sum(numbers, 10002);
    double kahan = kahan_sum(numbers, 10002);
    double neumaier = neumaier_sum(numbers, 10002);

    failures += check_fail(13, naive == 0.0, "naive_cancellation");
    failures += check_fail(14, kahan == 10000.0, "kahan_cancellation");
    failures += check_fail(15, neumaier == 10000.0, "neumaier_cancellation");

    // 5. Specials checks (16-23)
    double nan_val = 0.0 / 0.0;
    double inf_val = 1.0 / 0.0;
    double minus_inf = 1.0 / -0.0;

    failures += check_fail(16, nan_val != nan_val, "nan_not_eq");
    failures += check_fail(17, !(nan_val < 0.0), "nan_lt_zero");
    failures += check_fail(18, !(nan_val >= 0.0), "nan_ge_zero");
    failures += check_fail(19, inf_val == inf_val, "inf_eq_inf");
    failures += check_fail(20, minus_inf == -inf_val, "minus_inf");
    failures += check_fail(21, isnan(inf_val - inf_val), "inf_minus_inf");
    failures += check_fail(22, 1.0 / 0.0 != 1.0 / -0.0, "zero_sign");
    failures += check_fail(23, isnan(nan_val + 5.0), "nan_propagation");

    // 6. Subnormals checks (24-27)
    double subnormal = 5e-324;
    failures += check_fail(24, subnormal != 0.0, "subnormal_non_zero");
    failures += check_fail(25, subnormal / 2.0 == 0.0, "subnormal_underflow");
    failures += check_fail(26, inspect_sign(subnormal) == 0, "subnormal_sign");
    failures += check_fail(27, inspect_exponent(subnormal) == -1022, "subnormal_exponent");

    // 7. Simpson checks (28-31)
    double s11 = integrate_sin(11);
    double s21 = integrate_sin(21);
    double err11 = fabs(s11 - 2.0);
    double err21 = fabs(s21 - 2.0);

    failures += check_fail(28, err11 < 1.1e-4, "simpson_11_error");
    failures += check_fail(29, err21 < 7.0e-6, "simpson_21_error");
    failures += check_fail(30, err11 / err21 > 10.0, "simpson_ratio");
    failures += check_fail(31, fabs(kahan - neumaier) == 0.0, "kahan_vs_neumaier");

    if (failures == 0) {
        printf("GAUNTLET: PASS (31/31 checks)\n");
    } else {
        printf("GAUNTLET: FAIL (%d of 31 checks failed)\n", failures);
    }

    return failures;
}
