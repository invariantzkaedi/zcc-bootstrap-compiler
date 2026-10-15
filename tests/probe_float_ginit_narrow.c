#include <stdio.h>
#include <string.h>

struct TestStruct {
    unsigned char id;
    char name[7];
    float rLimit;
    float rXform;
};

static struct TestStruct g_item = { 6, "second", 4.6427e+14, 1.0 };
static float g_arr[2] = { 4.6427e+14, 1.0 };
static float g_scalar = 4.6427e+14;

int main(void) {
    float expected_limit = (float)4.6427e+14;
    float expected_xform = (float)1.0;

    unsigned int bits_limit_actual, bits_limit_expected;
    unsigned int bits_xform_actual, bits_xform_expected;

    memcpy(&bits_limit_actual, &g_item.rLimit, 4);
    memcpy(&bits_limit_expected, &expected_limit, 4);

    memcpy(&bits_xform_actual, &g_item.rXform, 4);
    memcpy(&bits_xform_expected, &expected_xform, 4);

    printf("PROBE STRUCT rLimit bits: actual=0x%08x expected=0x%08x\n", bits_limit_actual, bits_limit_expected);
    printf("PROBE STRUCT rXform bits: actual=0x%08x expected=0x%08x\n", bits_xform_actual, bits_xform_expected);

    unsigned int bits_arr0, bits_arr1;
    memcpy(&bits_arr0, &g_arr[0], 4);
    memcpy(&bits_arr1, &g_arr[1], 4);
    printf("PROBE ARRAY[0] bits:      actual=0x%08x expected=0x%08x\n", bits_arr0, bits_limit_expected);
    printf("PROBE ARRAY[1] bits:      actual=0x%08x expected=0x%08x\n", bits_arr1, bits_xform_expected);

    unsigned int bits_scalar;
    memcpy(&bits_scalar, &g_scalar, 4);
    printf("PROBE SCALAR bits:        actual=0x%08x expected=0x%08x\n", bits_scalar, bits_limit_expected);

    if (bits_limit_actual != bits_limit_expected || bits_xform_actual != bits_xform_expected ||
        bits_arr0 != bits_limit_expected || bits_arr1 != bits_xform_expected ||
        bits_scalar != bits_limit_expected) {
        printf("PROBE RESULT: FAILED (bit pattern divergence)\n");
        return 1;
    }

    printf("PROBE RESULT: PASS\n");
    return 0;
}
