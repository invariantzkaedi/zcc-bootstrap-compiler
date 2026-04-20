#include <stdio.h>

typedef long long i64;
typedef unsigned short u16;
typedef unsigned char u8;
typedef unsigned int u32;

struct sqlite3_value {
  union MemValue {
    double r;
    i64 i;
    int nZero;
    const char *zPType;
    void *pDef;
  } u;
  char *z;
  int n;
  u16 flags;
  u8 enc;
  u8 eSubtype;
  void *db;
  int szMalloc;
  u32 uTemp;
  char *zMalloc;
  void (*xDel)(void*);
};

int main() {
    struct sqlite3_value v;
    printf("sizeof=%lu\n", sizeof(v));
    printf("u=%lu\n", (char*)&v.u - (char*)&v);
    printf("u.r=%lu\n", (char*)&v.u.r - (char*)&v);
    printf("z=%lu\n", (char*)&v.z - (char*)&v);
    printf("n=%lu\n", (char*)&v.n - (char*)&v);
    printf("flags=%lu\n", (char*)&v.flags - (char*)&v);
    printf("db=%lu\n", (char*)&v.db - (char*)&v);
    printf("szMalloc=%lu\n", (char*)&v.szMalloc - (char*)&v);
    printf("zMalloc=%lu\n", (char*)&v.zMalloc - (char*)&v);
    printf("xDel=%lu\n", (char*)&v.xDel - (char*)&v);

    v.u.r = 3.14;
    printf("u.r=%.6f\n", v.u.r);
    return 0;
}
