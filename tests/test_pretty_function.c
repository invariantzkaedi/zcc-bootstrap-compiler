/*
 * Regression test for DEFECT-CORE05-02: __PRETTY_FUNCTION__ identifier
 */
#include <stdio.h>
#include <string.h>

static const char *get_worker_name(void) {
    return __PRETTY_FUNCTION__;
}

static const char *get_worker_func(void) {
    return __func__;
}

static const char *get_worker_function(void) {
    return __FUNCTION__;
}

int main(void) {
    const char *main_pretty = __PRETTY_FUNCTION__;
    const char *main_func = __func__;
    const char *main_function = __FUNCTION__;

    if (strcmp(main_pretty, "main") != 0) {
        printf("FAIL: main __PRETTY_FUNCTION__ got '%s'\n", main_pretty);
        return 1;
    }
    if (strcmp(main_func, "main") != 0) {
        printf("FAIL: main __func__ got '%s'\n", main_func);
        return 1;
    }
    if (strcmp(main_function, "main") != 0) {
        printf("FAIL: main __FUNCTION__ got '%s'\n", main_function);
        return 1;
    }

    if (strcmp(get_worker_name(), "get_worker_name") != 0) {
        printf("FAIL: worker __PRETTY_FUNCTION__ got '%s'\n", get_worker_name());
        return 1;
    }
    if (strcmp(get_worker_func(), "get_worker_func") != 0) {
        printf("FAIL: worker __func__ got '%s'\n", get_worker_func());
        return 1;
    }
    if (strcmp(get_worker_function(), "get_worker_function") != 0) {
        printf("FAIL: worker __FUNCTION__ got '%s'\n", get_worker_function());
        return 1;
    }

    printf("SUCCESS: __PRETTY_FUNCTION__, __func__, __FUNCTION__ verified byte-exact\n");
    return 0;
}
