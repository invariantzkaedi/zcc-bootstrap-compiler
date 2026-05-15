#include <stdarg.h>
#include <stdio.h>

int sum(int n, ...) {
  va_list ap;
  va_start(ap, n);
  int s = 0;
  for (int i=0;i<n;i++) s += va_arg(ap,int);
  va_end(ap);
  return s;
}

int main(){ 
    char a = 1;
    short b = 2;
    char c = 3;
    short d = 4;
    printf("sum: %d\n", sum(4, a, b, c, d));
    return 0;
}
