#include <stdio.h>
typedef unsigned short u16;
typedef struct { const char *z; unsigned int n; } Token;
typedef struct { void *pOn; void *pUsing; } OnOrUsing;
typedef struct { int iAddr; int regReturn; } FrameBound;
typedef struct { int value; int mask; } TrigEvent;
typedef union {
  int yyinit; Token yy0; void* yy33; void* yy41; void* yy47;
  void* yy131; TrigEvent yy180; struct {int value; int mask;} yy231;
  void* yy254; unsigned int yy285; void* yy322; void* yy385;
  int yy394; void* yy444; unsigned char yy516; void* yy521;
  const char* yy522; void* yy528; OnOrUsing yy561; FrameBound yy595;
} YYMINORTYPE;
typedef struct {
  u16 stateno;
  u16 major;
  YYMINORTYPE minor;
} yyStackEntry;
typedef struct yyParser {
  yyStackEntry *yytos;
  void *pParse;
  yyStackEntry yystack[100];
  yyStackEntry *yystackEnd;
} yyParser;
int main() {
  yyParser p;
  printf("sizeof(yyParser) = %zu\n", sizeof(yyParser));
  printf("offsetof(yytos) = %zu\n", (size_t)((char*)&p.yytos - (char*)&p));
  printf("offsetof(pParse) = %zu\n", (size_t)((char*)&p.pParse - (char*)&p));
  printf("offsetof(yystack) = %zu\n", (size_t)((char*)&p.yystack - (char*)&p));
  printf("offsetof(yystackEnd) = %zu\n", (size_t)((char*)&p.yystackEnd - (char*)&p));
  printf("sizeof(yyStackEntry) = %zu\n", sizeof(yyStackEntry));
  return 0;
}
