#include <stddef.h>

extern void *malloc(size_t size);

// Positive Case: Must flag
// CWE-787: No bounded dominator visible locally
void unbounded_write(int i) {
    char buf[16];
    buf[i] = 'A';
}

// Negative Case: Must NOT flag
// Dominant check visible: jump >= size
void safe_write() {
    char buf[16];
    for (int i = 0; i < 16; i++) {
        buf[i] = 'A';
    }
}

// Negative Case: Must NOT flag
// Explicit bounds guard
void guarded_write(int i) {
    char buf[16];
    if (i >= 16) return;
    buf[i] = 'A';
}

// Negative Case: Must NOT flag
// Not a stack array -> type is TY_PTR
void pointer_access(int i) {
    char *p = malloc(16);
    p[i] = 'A';
}

// Negative Case: Must NOT flag
// Parameter decay -> type is TY_PTR
void decayed_write(char *p, int i) {
    p[i] = 'A';
}
