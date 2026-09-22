/* C11 <threads.h> test: thrd_create, thrd_join, thrd_current, thrd_equal */
#include <stdio.h>
#include <threads.h>

static int worker(void *arg) {
    int val = (int)(long)arg;
    thrd_t self = thrd_current();
    if (!thrd_equal(self, thrd_current())) return -1;
    return val * 3;
}

int main(void) {
    thrd_t t;
    if (thrd_create(&t, worker, (void *)15) != thrd_success) {
        printf("FAIL: thrd_create\n");
        return 1;
    }

    int res = 0;
    if (thrd_join(t, &res) != thrd_success) {
        printf("FAIL: thrd_join\n");
        return 2;
    }

    if (res != 45) {
        printf("FAIL: unexpected res=%d (expected 45)\n", res);
        return 3;
    }

    return 0;
}
