/* ================================================================ */
/* threads.h — C11 Standard Threads & Concurrency Header             */
/* Self-contained freestanding POSIX ABI implementation             */
/* ================================================================ */

#ifndef _ZCC_THREADS_H
#define _ZCC_THREADS_H

#ifndef NULL
#define NULL ((void*)0)
#endif

#ifndef thread_local
#define thread_local _Thread_local
#endif

#define ONCE_FLAG_INIT        0
#define TSS_DTOR_ITERATIONS   4

/* C11 Enumerations */
enum {
    thrd_success  = 0,
    thrd_error    = 1,
    thrd_busy     = 2,
    thrd_timedout = 3,
    thrd_nomem    = 4
};

enum {
    mtx_plain     = 0,
    mtx_recursive = 1,
    mtx_timed     = 2
};

/* SystemV x86-64 ABI Concurrency Types */
typedef unsigned long thrd_t;

typedef union {
    char __size[40];
    long __align;
} mtx_t;

typedef union {
    char __size[48];
    long long __align;
} cnd_t;

typedef unsigned int tss_t;
typedef int once_flag;

typedef int (*thrd_start_t)(void *);
typedef void (*tss_dtor_t)(void *);

struct timespec {
    long tv_sec;
    long tv_nsec;
};

/* Underlying POSIX system bindings */
extern int pthread_create(unsigned long *thread, const void *attr, void *(*start_routine)(void *), void *arg);
extern int pthread_join(unsigned long thread, void **retval);
extern unsigned long pthread_self(void);
extern int pthread_equal(unsigned long t1, unsigned long t2);
extern int pthread_detach(unsigned long thread);
extern void pthread_exit(void *retval);
extern int pthread_mutex_init(void *mutex, const void *attr);
extern int pthread_mutex_lock(void *mutex);
extern int pthread_mutex_trylock(void *mutex);
extern int pthread_mutex_timedlock(void *mutex, const void *abstime);
extern int pthread_mutex_unlock(void *mutex);
extern int pthread_mutex_destroy(void *mutex);
extern int pthread_cond_init(void *cond, const void *attr);
extern int pthread_cond_signal(void *cond);
extern int pthread_cond_broadcast(void *cond);
extern int pthread_cond_wait(void *cond, void *mutex);
extern int pthread_cond_timedwait(void *cond, void *mutex, const void *abstime);
extern int pthread_cond_destroy(void *cond);
extern int pthread_key_create(unsigned int *key, void (*destructor)(void *));
extern int pthread_key_delete(unsigned int key);
extern void *pthread_getspecific(unsigned int key);
extern int pthread_setspecific(unsigned int key, const void *value);
extern int pthread_once(int *once_control, void (*init_routine)(void));
extern int nanosleep(const struct timespec *req, struct timespec *rem);
extern int sched_yield(void);

extern void *malloc(unsigned long size);
extern void free(void *ptr);

/* Internal trampoline structure to adapt (int (*)(void *)) to (void *(*)(void *)) */
struct __zcc_thrd_ctx {
    thrd_start_t func;
    void *arg;
};

static void *__zcc_thrd_wrapper(void *arg) {
    struct __zcc_thrd_ctx *ctx = (struct __zcc_thrd_ctx *)arg;
    thrd_start_t fn = ctx->func;
    void *user_arg = ctx->arg;
    free(ctx);
    int res = fn(user_arg);
    return (void *)(long)res;
}

/* ---------------------------------------------------------------- */
/* Call Once                                                        */
/* ---------------------------------------------------------------- */
static void call_once(once_flag *flag, void (*func)(void)) {
    pthread_once(flag, func);
}

/* ---------------------------------------------------------------- */
/* Thread Management                                                */
/* ---------------------------------------------------------------- */
static int thrd_create(thrd_t *thr, thrd_start_t func, void *arg) {
    struct __zcc_thrd_ctx *ctx = (struct __zcc_thrd_ctx *)malloc(sizeof(struct __zcc_thrd_ctx));
    if (!ctx) return thrd_nomem;
    ctx->func = func;
    ctx->arg = arg;
    if (pthread_create(thr, (const void *)0, __zcc_thrd_wrapper, ctx) != 0) {
        free(ctx);
        return thrd_error;
    }
    return thrd_success;
}

static thrd_t thrd_current(void) {
    return pthread_self();
}

static int thrd_detach(thrd_t thr) {
    return (pthread_detach(thr) == 0) ? thrd_success : thrd_error;
}

static int thrd_equal(thrd_t thr0, thrd_t thr1) {
    return pthread_equal(thr0, thr1);
}

static void thrd_exit(int res) {
    pthread_exit((void *)(long)res);
}

static int thrd_join(thrd_t thr, int *res) {
    void *retval = (void *)0;
    if (pthread_join(thr, &retval) != 0) return thrd_error;
    if (res) *res = (int)(long)retval;
    return thrd_success;
}

static int thrd_sleep(const struct timespec *duration, struct timespec *remaining) {
    return nanosleep(duration, remaining);
}

static void thrd_yield(void) {
    sched_yield();
}

/* ---------------------------------------------------------------- */
/* Mutex Management                                                 */
/* ---------------------------------------------------------------- */
typedef struct {
    char __size[4];
    int __align;
} pthread_mutexattr_t;
extern int pthread_mutexattr_init(pthread_mutexattr_t *attr);
extern int pthread_mutexattr_settype(pthread_mutexattr_t *attr, int kind);
extern int pthread_mutexattr_destroy(pthread_mutexattr_t *attr);

static int mtx_init(mtx_t *mtx, int type) {
    if (type & mtx_recursive) {
        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_settype(&attr, 1); /* PTHREAD_MUTEX_RECURSIVE = 1 */
        int rc = pthread_mutex_init((void *)mtx, &attr);
        pthread_mutexattr_destroy(&attr);
        return (rc == 0) ? thrd_success : thrd_error;
    }
    return (pthread_mutex_init((void *)mtx, (const void *)0) == 0) ? thrd_success : thrd_error;
}

static int mtx_lock(mtx_t *mtx) {
    return (pthread_mutex_lock((void *)mtx) == 0) ? thrd_success : thrd_error;
}

static int mtx_trylock(mtx_t *mtx) {
    int rc = pthread_mutex_trylock((void *)mtx);
    if (rc == 0) return thrd_success;
    if (rc == 16) return thrd_busy; /* EBUSY on Linux x86_64 is 16 */
    return thrd_error;
}

static int mtx_timedlock(mtx_t *mtx, const struct timespec *ts) {
    int rc = pthread_mutex_timedlock((void *)mtx, (const void *)ts);
    if (rc == 0) return thrd_success;
    if (rc == 110) return thrd_timedout; /* ETIMEDOUT on Linux x86_64 is 110 */
    return thrd_error;
}

static int mtx_unlock(mtx_t *mtx) {
    return (pthread_mutex_unlock((void *)mtx) == 0) ? thrd_success : thrd_error;
}

static void mtx_destroy(mtx_t *mtx) {
    pthread_mutex_destroy((void *)mtx);
}

/* ---------------------------------------------------------------- */
/* Condition Variable Management                                    */
/* ---------------------------------------------------------------- */
static int cnd_init(cnd_t *cond) {
    return (pthread_cond_init((void *)cond, (const void *)0) == 0) ? thrd_success : thrd_error;
}

static int cnd_signal(cnd_t *cond) {
    return (pthread_cond_signal((void *)cond) == 0) ? thrd_success : thrd_error;
}

static int cnd_broadcast(cnd_t *cond) {
    return (pthread_cond_broadcast((void *)cond) == 0) ? thrd_success : thrd_error;
}

static int cnd_wait(cnd_t *cond, mtx_t *mtx) {
    return (pthread_cond_wait((void *)cond, (void *)mtx) == 0) ? thrd_success : thrd_error;
}

static int cnd_timedwait(cnd_t *cond, mtx_t *mtx, const struct timespec *ts) {
    int rc = pthread_cond_timedwait((void *)cond, (void *)mtx, (const void *)ts);
    if (rc == 0) return thrd_success;
    if (rc == 110) return thrd_timedout; /* ETIMEDOUT = 110 */
    return thrd_error;
}

static void cnd_destroy(cnd_t *cond) {
    pthread_cond_destroy((void *)cond);
}

/* ---------------------------------------------------------------- */
/* Thread-Specific Storage (TSS)                                    */
/* ---------------------------------------------------------------- */
static int tss_create(tss_t *key, tss_dtor_t dtor) {
    return (pthread_key_create((unsigned int *)key, dtor) == 0) ? thrd_success : thrd_error;
}

static void *tss_get(tss_t key) {
    return pthread_getspecific(key);
}

static int tss_set(tss_t key, void *val) {
    return (pthread_setspecific(key, (const void *)val) == 0) ? thrd_success : thrd_error;
}

static void tss_delete(tss_t key) {
    pthread_key_delete(key);
}

#endif /* _ZCC_THREADS_H */
