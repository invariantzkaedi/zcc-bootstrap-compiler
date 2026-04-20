#include <stdio.h>
typedef unsigned short u16;
typedef union { void *yy528; int yy394; } YYMINORTYPE;
typedef struct {
  u16 stateno;
  u16 major;
  YYMINORTYPE minor;
} yyStackEntry;
int main() {
  yyStackEntry e;
  printf("sizeof(yyStackEntry) = %zu\n", sizeof(yyStackEntry));
  printf("offsetof minor = %zu\n", (size_t)((char*)&e.minor - (char*)&e));
  printf("offsetof major = %zu\n", (size_t)((char*)&e.major - (char*)&e));
  return 0;
}
