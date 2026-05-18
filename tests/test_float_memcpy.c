/* Verify float memcpy round-trip works after CG-FLOAT-001 fix */
#include <stdio.h>
#include <string.h>

int main(void) {
    float f;
    unsigned int i;

    f = 2.5f;
    memcpy(&i, &f, 4);
    printf("float->bits: 0x%08X (expect 0x40200000)\n", i);

    i = 0x40200000;
    memcpy(&f, &i, 4);
    printf("bits->float: %f (expect 2.5)\n", (double)f);

    f = -3.14f;
    memcpy(&i, &f, 4);
    printf("neg float: 0x%08X (expect 0xC048F5C3)\n", i);

    return 0;
}
