#include <stdio.h>
#include <stdarg.h>
#include <assert.h>

typedef struct {
    char data[128];
} BigStruct;

BigStruct get_big_struct(void) {
    BigStruct s;
    for (int i = 0; i < 128; i++) s.data[i] = (char)(0xAA);
    return s;
}

int variadic_with_sret(int count, ...) {
    char pad[32];
    for (int i = 0; i < 32; i++) pad[i] = (char)i;

    /* Calling an sret function writes into cc->abi_scratch_offset + 32.
       If save_base is at -368 (inside [-448, -192]), get_big_struct writes
       from -416 to -288, clobbering the saved variadic registers at -368! */
    BigStruct bs = get_big_struct();

    va_list ap;
    va_start(ap, count);
    int sum = 0;
    for (int i = 0; i < count; i++) {
        sum += va_arg(ap, int);
    }
    va_end(ap);

    return sum + (unsigned char)bs.data[0];
}

int main(void) {
    /* 4 variadic arguments passed in registers: %rdx=10, %rcx=20, %r8=30, %r9=40 */
    int result = variadic_with_sret(4, 10, 20, 30, 40);
    printf("Result: %d (Expected: 10 + 20 + 30 + 40 + 0xAA = 270)\n", result);
    if (result != 270) {
        printf("BUG CONFIRMED: Variadic register save area was CLOBBERED by sret call! Got: %d\n", result);
        return 1;
    }
    printf("SUCCESS: No clobbering!\n");
    return 0;
}
