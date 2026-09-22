/* C11 <threads.h> test: mtx_plain, mtx_recursive, lock, trylock, unlock */
#include <stdio.h>
#include <threads.h>

static mtx_t g_plain_mtx;
static mtx_t g_rec_mtx;
static int g_shared_count = 0;

static int worker(void *arg) {
    (void)arg;
    for (int i = 0; i < 1000; i++) {
        mtx_lock(&g_plain_mtx);
        g_shared_count++;
        mtx_unlock(&g_plain_mtx);
    }
    return 0;
}

int main(void) {
    /* 1. Plain mutex concurrency */
    if (mtx_init(&g_plain_mtx, mtx_plain) != thrd_success) return 1;

    thrd_t t1, t2;
    thrd_create(&t1, worker, NULL);
    thrd_create(&t2, worker, NULL);
    thrd_join(t1, NULL);
    thrd_join(t2, NULL);

    if (g_shared_count != 2000) return 2;
    mtx_destroy(&g_plain_mtx);

    /* 2. Trylock verification */
    mtx_t try_mtx;
    mtx_init(&try_mtx, mtx_plain);
    if (mtx_trylock(&try_mtx) != thrd_success) return 3;
    if (mtx_trylock(&try_mtx) == thrd_success) return 4; /* Should fail (already locked) */
    mtx_unlock(&try_mtx);
    mtx_destroy(&try_mtx);

    /* 3. Recursive mutex */
    if (mtx_init(&g_rec_mtx, mtx_recursive) != thrd_success) return 5;
    if (mtx_lock(&g_rec_mtx) != thrd_success) return 6;
    if (mtx_lock(&g_rec_mtx) != thrd_success) return 7; /* Recursive lock must succeed */
    mtx_unlock(&g_rec_mtx);
    mtx_unlock(&g_rec_mtx);
    mtx_destroy(&g_rec_mtx);

    return 0;
}
