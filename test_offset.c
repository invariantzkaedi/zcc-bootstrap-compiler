struct SParser {
  void *z;
  char buff[24];
  char dyd[48];
  const char *mode;
  const char *name;
};
void test_zcc_offset(void *L, void *z, const char *name, const char *mode) {
  struct SParser p;
  int status;
  p.mode = mode;
}
