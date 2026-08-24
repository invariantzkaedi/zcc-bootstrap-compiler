#include <stddef.h>
int test(const char *s) { return __builtin_expect(!s, 0); }
