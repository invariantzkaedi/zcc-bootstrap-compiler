#include <stdio.h>
typedef unsigned short u16;
typedef struct { const char *z; unsigned int n; } Token;
typedef struct { void *pOn; void *pUsing; } OnOrUsing;
typedef struct { int iAddr; int regReturn; } FrameBound;
typedef struct { int value; int mask; } TrigEvent;
typedef union {
  int yyinit;
  Token yy0;
  void* yy33;
  void* yy41;
  void* yy47;
  void* yy131;
  TrigEvent yy180;
  struct {int value; int mask;} yy231;
  void* yy254;
  unsigned int yy285;
  void* yy322;
  void* yy385;
  int yy394;
  void* yy444;
  unsigned char yy516;
  void* yy521;
  const char* yy522;
  void* yy528;
  OnOrUsing yy561;
  FrameBound yy595;
} YYMINORTYPE;
typedef struct {
  u16 stateno;
  u16 major;
  YYMINORTYPE minor;
} yyStackEntry;
int main() {
  printf("sizeof(YYMINORTYPE) = %zu\n", sizeof(YYMINORTYPE));
  printf("sizeof(yyStackEntry) = %zu\n", sizeof(yyStackEntry));
  yyStackEntry e;
  printf("offsetof minor = %zu\n", (size_t)((char*)&e.minor - (char*)&e));
  return 0;
}
