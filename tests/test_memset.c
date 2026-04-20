#include <stdio.h>
#include <string.h>

typedef long long i64;
typedef unsigned short u16;

struct Mem {
  union { double r; i64 i; } u;
  char *z;
  int n;
  u16 flags;
};

void memSetDouble(struct Mem *p, double val) {
    p->u.r = val;
    p->flags = 8;
}

int main() {
    struct Mem m;
    memSetDouble(&m, 3.14);
    printf("set: r=%.6f flags=%d\n", m.u.r, m.flags);

    // Manual byte swap
    unsigned char *b = (unsigned char *)&m.u.r;
    printf("le bytes: ");
    for (int i = 0; i < 8; i++) printf("%02x ", b[i]);
    printf("\n");
    printf("be bytes: ");
    for (int i = 7; i >= 0; i--) printf("%02x ", b[i]);
    printf("\n");
    return 0;
}
