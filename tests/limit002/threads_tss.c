/* C11 <threads.h> test: tss_create, tss_set, tss_get, tss_delete */
#include <stdio.h>
#include <threads.h>

static tss_t g_key;

static int worker(void *arg) {
    int val = (int)(long)arg;
    tss_set(g_key, (void *)(long)val);

    thrd_sleep(&(struct timespec){.tv_sec = 0, .tv_nsec = 10000000}, NULL);

    int retrieved = (int)(long)tss_get(g_key);
    return (retrieved == val) ? 0 : 1;
}

int main(void) {
    if (tss_create(&g_key, NULL) != thrd_success) return 1;

    thrd_t t1, t2;
    thrd_create(&t1, worker, (void *)101);
    thrd_create(&t2, worker, (void *)202);

    int r1 = 0, r2 = 0;
    thrd_join(t1, &r1);
    thrd_join(t2, &r2);

    tss_delete(g_key);

    if (r1 != 0 || r2 != 0) return 2;

    return 0;
}
