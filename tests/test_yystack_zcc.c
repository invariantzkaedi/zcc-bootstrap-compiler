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
  printf("sizeof(yyStackEntry) = %d\n", (int)sizeof(yyStackEntry));
  printf("offsetof major = %d\n", (int)((char*)&e.major - (char*)&e));
  printf("offsetof minor = %d\n", (int)((char*)&e.minor - (char*)&e));
  return 0;
}
