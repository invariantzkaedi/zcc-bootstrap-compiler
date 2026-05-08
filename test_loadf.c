#include <stdio.h>
typedef struct LoadF {
  int n;
  FILE *f;
  char buff[BUFSIZ];
} LoadF;
int get_size() {
  LoadF lf;
  return sizeof(lf);
}
