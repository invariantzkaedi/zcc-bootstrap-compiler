#include <stdint.h>
#include <setjmp.h>
static jmp_buf env;
int main(void) {
    volatile int x = 46;
    if (setjmp(env) == 0) {
        x += 16;
        longjmp(env, 1);
    } else {
        x *= 2;
    }
    return (uint8_t)(x & 0xFF);
}
