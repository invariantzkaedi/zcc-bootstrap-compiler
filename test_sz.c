int printf(const char *, ...);
static const int sz = sizeof("local");
int main() { printf("%d\n", sz); return 0; }
