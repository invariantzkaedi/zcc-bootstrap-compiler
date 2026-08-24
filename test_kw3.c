#include <stdio.h>
#include <string.h>
typedef struct { char *word; int token; } Keyword;
static Keyword keywords[] = {
    {"int", 1},
    {"void", 2}
};
int main() {
    return keywords[0].token == 1 ? 0 : 1;
}