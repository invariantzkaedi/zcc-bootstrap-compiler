/* test_generic.c — Step 2 & 3 regression suite for ZCC _Generic selection */
#include <stdio.h>
#include <assert.h>
#include <string.h>

#define type_name(x) _Generic((x), \
    int: "int", \
    double: "double", \
    char*: "char*", \
    default: "other" \
)

int side_effect_counter = 0;
int dummy_fn(void) {
    side_effect_counter++;
    return 42;
}

int main(void) {
    int i = 10;
    double d = 3.14;
    char *s = "hello";
    float f = 1.0f;

    printf("Testing _Generic selection...\n");

    assert(strcmp(type_name(i), "int") == 0);
    assert(strcmp(type_name(d), "double") == 0);
    assert(strcmp(type_name(s), "char*") == 0);
    assert(strcmp(type_name(f), "other") == 0);

    /* Verify controlling expression side effects are NOT evaluated at runtime */
    const char *fn_type = _Generic(dummy_fn(), int: "int_ret", default: "other_ret");
    assert(strcmp(fn_type, "int_ret") == 0);
    assert(side_effect_counter == 0);

    printf("[OK] All _Generic verification tests passed cleanly!\n");
    return 0;
}
