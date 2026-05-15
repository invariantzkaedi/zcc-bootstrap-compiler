/* Regression: cast sign-extension must use movsbq/movswq (64-bit),
 * not movsbl/movswl (32-bit), when result feeds 64-bit arithmetic.
 * Seed 57825902 from promotion_fuzzer.py.
 * Expected: GCC -O0 -fwrapv parity. */
int main() {
    long long v0 = (((short)-473) / (((signed char)82) == 0 ? 1 : ((signed char)82)));
    unsigned int v1 = (((signed char)-597) | ((unsigned long)434));
    int ret_val = (v0 >> (v1 & 31));
    return (int)(ret_val & 255);
}
