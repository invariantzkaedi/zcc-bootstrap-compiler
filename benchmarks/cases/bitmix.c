int printf(const char *fmt, ...);
int main() {
    unsigned int i, val = 0x12345678;
    for (i = 0; i < 200000; i++) {
        val = (val ^ i) + 1;
        val = (val << 3) | (val >> 29);
        val = val & 0x0FFFFFFF;
    }
    printf("val = %u\n", val);
    return 0;
}
