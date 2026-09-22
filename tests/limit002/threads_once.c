/* C11 <threads.h> test: call_once & ONCE_FLAG_INIT */
#include <stdio.h>
#include <threads.h>

static once_flag g_flag = ONCE_FLAG_INIT;
static int g_run_count = 0;

static void do_init(void) {
    g_run_count++;
}

static int worker(void *arg) {
    (void)arg;
    for (int i = 0; i < 50; i++) {
        call_once(&g_flag, do_init);
    }
    return 0;
}

int main(void) {
    thrd_t threads[4];
    for (int i = 0; i < 4; i++) {
        thrd_create(&threads[i], worker, NULL);
    }
    for (int i = 0; i < 4; i++) {
        thrd_join(threads[i], NULL);
    }

    if (g_run_count != 1) {
        printf("FAIL: do_init ran %d times, expected 1\n", g_run_count);
        return 1;
    }

    return 0;
}
