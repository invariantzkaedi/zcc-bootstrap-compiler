#include <stdio.h>
typedef struct { const char *z; unsigned int n; } Token;
typedef struct { void *pOn; void *pUsing; } OnOrUsing;
typedef union { Token yy0; OnOrUsing yy561; } YYMINORTYPE;
int main() { printf("%ld\n", sizeof(YYMINORTYPE)); return 0; }
