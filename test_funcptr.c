static char *(*l_getenv)(const char *name);
int main() { return l_getenv != 0; }
