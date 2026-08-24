const char *luaL_optstring(void *L, int arg, const char *def);
const char *test() { return luaL_optstring(0, 0, 0); }
int main() { return 0; }
