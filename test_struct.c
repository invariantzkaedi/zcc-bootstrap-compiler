#include <stdio.h>
typedef struct ZIO ZIO;
struct SParser {
  ZIO *z;
  void *buff;
  struct { void *arr; int size; } actvar;
  struct { void *arr; int size; } gt;
  struct { void *arr; int size; } label;
  const char *name;
  const char *mode;
};
void f_parser(void *ud) {
    struct SParser *p = (struct SParser *)ud;
    printf("p->mode is %p\n", p->mode);
    const char *mode = p->mode ? p->mode : "bt";
    printf("mode is %p (%s)\n", mode, mode);
}
int main() {
    struct SParser p;
    p.mode = NULL;
    f_parser(&p);
    return 0;
}
