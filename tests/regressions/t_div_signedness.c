/* Regression: signed long / unsigned int must use usual_arith_conv result type,
 * not lhs operand type, for div signedness selection.
 * Seed 165053450 from promotion_fuzzer.py.
 * Expected: GCC -O0 -fwrapv parity. */
int main() {
    long v0 = ((short)-149);
    unsigned int v1 = ((((unsigned short)497) + ((char)-472)) - (((short)-681) >> (((unsigned long)41) & 31)));
    unsigned int ret_val = ((v0 == 0 ? 1 : v0) / (v1 == 0 ? 1 : v1));
    return (int)(ret_val & 255);
}
