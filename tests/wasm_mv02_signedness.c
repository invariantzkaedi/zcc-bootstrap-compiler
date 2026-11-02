/* tests/wasm_mv02_signedness.c */
#include <stdint.h>

int test_lt(int64_t a, int64_t b) { return a < b; }
int64_t test_widen(int32_t x) { return (int64_t)x + 1; }
int64_t test_load(void) {
    int64_t v = 0x1122334455667788LL;
    int64_t *p = &v;
    return *p >> 32;
}
int test_switch(int64_t x) {
    switch (x) {
        case 4294967296LL: return 11;
        case -4294967296LL: return 22;
        default: return 33;
    }
}
int test_loop_switch(int64_t x) {
    int acc = 0;
    for (int i = 0; i < 3; i++) {
        switch (x + i) {
            case 4294967296LL: acc += 10; break;
            default: acc += 1; break;
        }
        acc += 100;
    }
    return acc;
}
int main(void) {
    if (!test_lt((int64_t)0x8000000000000000ULL, 1)) return 1;
    if (test_widen(-5) > 0 || test_widen(-5) != -4LL) return 2;
    if (test_load() != 0x11223344LL) return 3;
    if (test_switch(4294967296LL) != 11) return 4;
    if (test_switch(-4294967296LL) != 22) return 5;
    if (test_loop_switch(4294967295LL) != 312) return 6;
    return 0;
}

