#include <stdio.h>
#include <string.h>
typedef struct { char *word; int token; } Keyword;
static Keyword keywords[] = {
    {" int\,