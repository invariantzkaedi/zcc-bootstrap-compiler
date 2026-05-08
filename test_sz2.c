int printf(const char *, ...);
int main() {
    static const int szloc = sizeof("local") - 1;
    printf("szloc = %d\n", szloc);
    return 0;
}
