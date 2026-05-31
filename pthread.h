#ifndef PTHREAD_H
#define PTHREAD_H

typedef unsigned long pthread_t;
typedef int pthread_mutex_t;
#define PTHREAD_MUTEX_INITIALIZER 0

static int pthread_create(pthread_t *thread, const void *attr, void *(*start_routine) (void *), void *arg) {
    start_routine(arg);
    return 0;
}

static int pthread_join(pthread_t thread, void **retval) {
    return 0;
}

static int pthread_mutex_init(pthread_mutex_t *mutex, const void *attr) {
    return 0;
}

static int pthread_mutex_destroy(pthread_mutex_t *mutex) {
    return 0;
}

static int pthread_mutex_lock(pthread_mutex_t *mutex) {
    return 0;
}

static int pthread_mutex_unlock(pthread_mutex_t *mutex) {
    return 0;
}

#endif
