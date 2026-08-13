/* test_string_ir.c — Enhanced string literal regression test */
#include <stdio.h>
#include <assert.h>
#include <string.h>

int main(void) {
    const char *s1 = "Hello ZCC";
    const char *s2 = "Hello ZCC";
    const char *s3 = "Different";
    const char *empty = "";

    assert(strcmp(s1, s2) == 0);
    assert(strcmp(s1, s3) != 0);
    assert(strlen(empty) == 0);
    assert(s1[0] == 'H');
    assert(s3[0] == 'D');

    puts("[OK] String IR regression tests passed!");
    return 0;
}
