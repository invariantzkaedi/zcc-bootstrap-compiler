#include <stdio.h>

typedef long long i64;
typedef unsigned short u16;
typedef unsigned char u8;
typedef unsigned int u32;

struct Mem {
  union {
    double r;
    i64 i;
  } u;
  char *z;
  int n;
  u16 flags;
  u8 enc;
  u8 eSubtype;
  void *db;
};

// Mimic sqlite3VdbeMemSetDouble
void setDouble(struct Mem *pMem, double val) {
    pMem->u.r = val;
    pMem->flags = 8; // MEM_Real
}

int main() {
    struct Mem m;
    m.u.r = 0.0;
    m.flags = 0;

    setDouble(&m, 3.14);
    printf("direct: u.r=%.6f flags=%d\n", m.u.r, m.flags);
    return 0;
}
