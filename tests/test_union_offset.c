#include <stdio.h>
typedef void *Expr;
typedef struct { const char *z; unsigned int n; } Token;
typedef union {
    int yyinit;
    Token yy0;
    void *yy33;
    void *yy41;
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
    /* Set yy0 first to simulate token being there */
    yymsp[0].minor.yy0.z = "SELECT";
    yymsp[0].minor.yy0.n = 6;
    Expr dummy;
    /* Now assign yy528 — should overwrite yy0's z pointer at offset 0 */
    yymsp[0].minor.yy528 = &dummy;
    printf("yy528 written correctly: %d\n",
           yymsp[0].minor.yy528 == &dummy);
    printf("offset of yy528 = %zu\n",
           (char*)&yymsp[0].minor.yy528 - (char*)&yymsp[0]);
    printf("offset of yy0.z = %zu\n",
           (char*)&yymsp[0].minor.yy0.z - (char*)&yymsp[0]);
    return 0;
}
