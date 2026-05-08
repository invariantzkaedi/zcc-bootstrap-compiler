#include <string.h>
#include <stdio.h>
int main() {
  const char *line = "  		local x";
  static const size_t szloc = sizeof("local") - 1;
  static const char space[] = " \t";
  line += strspn(line, space);
  printf("line: '%s', szloc=%zu\n", line, szloc);
  int cmp = strncmp(line, "local", szloc);
  printf("cmp=%d\n", cmp);
  printf("char at line+szloc = %d ('%c')\n", *(line + szloc), *(line+szloc));
  void *p = strchr(space, *(line + szloc));
  printf("strchr = %p\n", p);
  if (cmp == 0 && p != NULL) {
    printf("warning: triggered\n");
  } else {
    printf("warning: NOT triggered\n");
  }
  return 0;
}
