#include <stdio.h>
#include "part1.c"

int main() {
    printf("TK_EOF: %d\n", TK_EOF);
    printf("TK_IDENT: %d\n", TK_IDENT);
    printf("TK_CHAR_LIT: %d\n", TK_CHAR_LIT);
    printf("TK_LT: %d\n", TK_LT);
    printf("TK_LOR: %d\n", TK_LOR);
    printf("TK_LPAREN: %d\n", TK_LPAREN);
    printf("TK_RPAREN: %d\n", TK_RPAREN);
    printf("TK_SEMI: %d\n", TK_SEMI);
    return 0;
}
