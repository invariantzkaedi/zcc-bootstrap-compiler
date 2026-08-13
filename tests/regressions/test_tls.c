/* test_tls.c — Step 4 & 5 regression suite for ZCC Thread-Local Storage */
#include <stdio.h>
#include <assert.h>

_Thread_local int init_tls_var = 123;
__thread int uninit_tls_var;
static _Thread_local double static_tls_dbl = 2.71828;

int main(void) {
    printf("Testing Thread-Local Storage (_Thread_local / __thread)...\n");

    uninit_tls_var = 456;

    assert(init_tls_var == 123);
    assert(uninit_tls_var == 456);
    assert(static_tls_dbl > 2.71 && static_tls_dbl < 2.72);

    /* Test address resolution */
    int *p_init = &init_tls_var;
    int *p_uninit = &uninit_tls_var;

    *p_init = 789;
    *p_uninit = 101112;

    assert(init_tls_var == 789);
    assert(uninit_tls_var == 101112);

    printf("[OK] All Thread-Local Storage verification tests passed cleanly!\n");
    return 0;
}
