#include <stdio.h>
typedef void *Expr;
typedef union {
    int yy0;
    Expr *yy528;
} YYMINORTYPE;
typedef struct {
    unsigned short stateno;
    unsigned short major;
    YYMINORTYPE minor;
} yyStackEntry;

int main() {
    yyStackEntry stack[5];
    yyStackEntry *yymsp = stack;
    Expr dummy = (Expr)0xdeadbeef;
    yymsp[0].minor.yy528 = &dummy;
    printf("yymsp[0].minor.yy528 = %p\n", yymsp[0].minor.yy528);
    printf("&dummy = %p\n", (void*)&dummy);
    return 0;
}
