#include <stdint.h>
#include <setjmp.h>
static jmp_buf env;
int main(void) {
    volatile int x = -92;
    if (setjmp(env) == 0) {
        x += 27;
        longjmp(env, 1);
    } else {
        x *= 2;
    }
    return (uint8_t)(x & 0xFF);
}
