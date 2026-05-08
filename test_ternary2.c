#include <stdio.h>
struct SParser { const char *mode; };
void f_parser(void *ud) {
    struct SParser *p = (struct SParser *)ud;
    const char *mode = p->mode ? p->mode : "bt";
    if (mode == NULL) {
        printf("BUG: mode is NULL!\n");
    } else {
        printf("mode is: %s\n", mode);
    }
}
int main() {
    struct SParser p;
    p.mode = NULL;
    f_parser(&p);
    return 0;
}
