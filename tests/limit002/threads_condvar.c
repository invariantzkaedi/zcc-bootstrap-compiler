/* C11 <threads.h> test: cnd_init, cnd_signal, cnd_wait, cnd_destroy */
#include <stdio.h>
#include <threads.h>

static mtx_t g_cv_mtx;
static cnd_t g_cv;
static int g_ready = 0;
static int g_data = 0;

static int producer(void *arg) {
    (void)arg;
    mtx_lock(&g_cv_mtx);
    g_data = 12345;
    g_ready = 1;
    cnd_signal(&g_cv);
    mtx_unlock(&g_cv_mtx);
    return 0;
}

int main(void) {
    if (mtx_init(&g_cv_mtx, mtx_plain) != thrd_success) return 1;
    if (cnd_init(&g_cv) != thrd_success) return 2;

    thrd_t prod;
    thrd_create(&prod, producer, NULL);

    mtx_lock(&g_cv_mtx);
    while (!g_ready) {
        cnd_wait(&g_cv, &g_cv_mtx);
    }
    int received = g_data;
    mtx_unlock(&g_cv_mtx);

    thrd_join(prod, NULL);
    cnd_destroy(&g_cv);
    mtx_destroy(&g_cv_mtx);

    if (received != 12345) return 3;

    return 0;
}
