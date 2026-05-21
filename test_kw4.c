#include <stdio.h>
#include <string.h>

char* kws_word[2];
int kws_token[2];
static int kw_count = 2;

static int lookup_keyword(char *name) {
    int i;
    if (!name) return 0;
    for (i = 0; i < kw_count; i++) {
        if (!kws_word[i]) break;
        if (strcmp(kws_word[i], name) == 0) {
            return kws_token[i];
        }
    }
    return 0;
}
int main() {
    kws_word[0] = " int\;
