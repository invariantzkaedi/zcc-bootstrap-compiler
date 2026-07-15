#include <stdio.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// 1. Spacing (ULP) calculator using double bit representation
double ulp_double(double x) {
    union { double d; unsigned long long u; } u;
    u.d = x;
    if (x > 0) {
        u.u += 1;
    } else if (x < 0) {
        u.u -= 1;
    } else {
        return 4.9406564584124654e-324;
    }
    double next_d = u.d;
    double diff = next_d - x;
    return diff >= 0 ? diff : -diff;
}

// 2. Kahan Summation
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

// 3. Neumaier Summation
double neumaier_sum(double *numbers, int count) {
    double sum = 0.0;
    double c = 0.0;
    for (int i = 0; i < count; i++) {
        double x = numbers[i];
        double t = sum + x;
        double diff;
        if (sum >= x || sum >= -x) { // abs(sum) >= abs(x)
            diff = (sum - t) + x;
        } else {
            diff = (x - t) + sum;
        }
        c += diff;
        sum = t;
    }
    return sum + c;
}

// Helper to inspect double bits
void inspect_double(double x) {
    union { double d; unsigned long long u; } u;
    u.d = x;
    unsigned long long sign = (u.u >> 63) & 1;
    unsigned long long exponent = (u.u >> 52) & 0x7FF;
    unsigned long long mantissa = u.u & 0xFFFFFFFFFFFFFULL;
    printf("Inspect %f: sign=%llu, unbiased_exponent=%lld, mantissa_bits=0x%llx\n",
           x, sign, (long long)exponent - 1023, mantissa);
}

// 4. Simpson's integration rule for sin(x) from 0 to PI
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

int main() {
    printf("=== ZCC FLOATING POINT TEST GAUNTLET ===\n\n");

    // Test 1: Machine Epsilon
    printf("--- 1. Machine Epsilon checks ---\n");
    float eps32_absorbed = 1.0f + 1e-8f;
    float eps32_detected = 1.0f + 1e-6f;
    printf("eps32_absorbed == 1.0f: %s\n", (eps32_absorbed == 1.0f) ? "True" : "False");
    printf("eps32_detected == 1.0f: %s\n", (eps32_detected == 1.0f) ? "True" : "False");

    double eps64_absorbed = 1.0 + 1e-17;
    double eps64_detected = 1.0 + 1e-15;
    printf("eps64_absorbed == 1.0: %s\n", (eps64_absorbed == 1.0) ? "True" : "False");
    printf("eps64_detected == 1.0: %s\n", (eps64_detected == 1.0) ? "True" : "False");
    printf("\n");

    // Test 2: ULP values at different magnitudes
    printf("--- 2. ULP Spacing Checks ---\n");
    printf("ulp(1.0)   = %e\n", ulp_double(1.0));
    printf("ulp(10.0)  = %e\n", ulp_double(10.0));
    printf("ulp(100.0) = %e\n", ulp_double(100.0));
    printf("ulp(1e10)  = %e\n", ulp_double(1e10));
    printf("ulp(1e20)  = %e\n", ulp_double(1e20));
    printf("ulp(1e-10) = %e\n", ulp_double(1e-10));
    printf("\n");

    // Test 3: Kahan & Neumaier Summation
    printf("--- 3. Compensated Summation (1e16 + 10000 * 1.0 - 1e16) ---\n");
    int count = 10002;
    static double numbers[10002]; // Use static or heap to avoid massive stack sizes in ZCC
    numbers[0] = 1e16;
    for (int i = 1; i <= 10000; i++) {
        numbers[i] = 1.0;
    }
    numbers[10001] = -1e16;

    double naive = 0.0;
    for (int i = 0; i < count; i++) {
        naive += numbers[i];
    }
    double kahan = kahan_sum(numbers, count);
    double neumaier = neumaier_sum(numbers, count);
    printf("Naive sum:    %f\n", naive);
    printf("Kahan sum:    %f\n", kahan);
    printf("Neumaier sum: %f\n", neumaier);
    printf("\n");

    // Test 4: Simpson's Rule
    printf("--- 4. Simpson's integration of sin(x) 0 to PI (Analytical = 2.0) ---\n");
    for (int n = 11; n <= 100001; n *= 10) {
        double result = integrate_sin(n);
        printf("n=%6d: result=%.14f, error=%e\n", n, result, result - 2.0);
    }
    printf("\n");

    // Test 5: Hexfloat Inspection
    printf("--- 5. Hexfloat Inspection ---\n");
    inspect_double(0.1);
    inspect_double(0.5);

    return 0;
}
