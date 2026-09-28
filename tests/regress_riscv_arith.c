int test_riscv_add(int a, int b) {
    return a + b;
}

int test_riscv_sub(int a, int b) {
    return a - b;
}

int test_riscv_mul(int a, int b) {
    return a * b;
}

int test_riscv_div(int a, int b) {
    return a / b;
}

unsigned int test_riscv_udiv(unsigned int a, unsigned int b) {
    return a / b;
}

int test_riscv_mod(int a, int b) {
    return a % b;
}

unsigned int test_riscv_umod(unsigned int a, unsigned int b) {
    return a % b;
}

int test_riscv_shr(int a, int s) {
    return a >> s;
}

unsigned int test_riscv_ushr(unsigned int a, int s) {
    return a >> s;
}

int test_riscv_shl(int a, int s) {
    return a << s;
}

int test_riscv_band(int a, int b) {
    return a & b;
}

int test_riscv_bor(int a, int b) {
    return a | b;
}

int test_riscv_bxor(int a, int b) {
    return a ^ b;
}
