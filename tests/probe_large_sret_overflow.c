#include <stdio.h>
#include <assert.h>

typedef struct {
    char data[512];
} HugeStruct;

HugeStruct get_huge(char fill) {
    HugeStruct h;
    for (int i = 0; i < 512; i++) h.data[i] = fill;
    return h;
}

int caller_fn(void) {
    HugeStruct h1;
    HugeStruct h2;
    HugeStruct h3 = get_huge((char)0x88);

    /* h3 is at [-1536, -1024].
       Now call get_huge(0x55) for h1.
       get_huge writes into -1824..-1312, directly clobbering h3! */
    h1 = get_huge((char)0x55);
    h2 = get_huge((char)0x77);

    int ok = 1;
    for (int i = 0; i < 512; i++) {
        if (h3.data[i] != (char)0x88) {
            printf("BUG CONFIRMED: h3[%d] clobbered! Expected 0x88, got 0x%02x\n",
                   i, (unsigned char)h3.data[i]);
            ok = 0;
            break;
        }
    }
    return ok;
}

int main(void) {
    int ok = caller_fn();
    if (ok) printf("SUCCESS: Large sret data intact!\n");
    return ok ? 0 : 1;
}
