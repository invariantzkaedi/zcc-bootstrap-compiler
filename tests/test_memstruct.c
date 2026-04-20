#include <stdio.h>
#include <stddef.h>

// Minimal reproduction of SQLite's Mem struct layout
// Find the real one:
extern int sqlite3_open(const char*, void**);

// Just test union offset behavior
struct TestMem {
    void *flags;      // pointer
    union {
        long long i;
        double r;
        char *z;
    } u;
    long long n;
};

int main() {
    struct TestMem m;
    printf("sizeof(TestMem)=%lu\n", sizeof(m));
    printf("offset flags=%lu\n", (char*)&m.flags - (char*)&m);
    printf("offset u.i=%lu\n", (char*)&m.u.i - (char*)&m);
    printf("offset u.r=%lu\n", (char*)&m.u.r - (char*)&m);
    printf("offset n=%lu\n", (char*)&m.n - (char*)&m);

    m.u.r = 3.14;
    printf("u.r=%.6f u.i=%lld\n", m.u.r, m.u.i);

    return 0;
}
