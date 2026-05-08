const char *f1();
const char *f2();
const char *test(int c) {
  if (c) return f1();
  else return f2();
}
