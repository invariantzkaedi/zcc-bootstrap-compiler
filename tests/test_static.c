int printf(char *s, ...);
static int x = 3;
int main() {
    while (x-- > 0) {
        printf("x=%d\n", x);
    }
    return 0;
}
