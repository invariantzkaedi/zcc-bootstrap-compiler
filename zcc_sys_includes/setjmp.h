#ifndef _SETJMP_H
#define _SETJMP_H
/* x86-64 glibc: sizeof(jmp_buf) = 200 bytes
 * = __jmp_buf(long[8]=64) + __mask_was_saved(int=4) + __sigset_t(128) + pad(4)
 * Use long[25] = 200 bytes to match. */
typedef long jmp_buf[25];
int  setjmp(jmp_buf env);
void longjmp(jmp_buf env, int val);
#endif
