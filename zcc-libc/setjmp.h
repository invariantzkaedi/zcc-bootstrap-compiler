typedef int jmp_buf[64];
int setjmp(jmp_buf);
void longjmp(jmp_buf, int);
#define _setjmp setjmp
#define _longjmp longjmp
