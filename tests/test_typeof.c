int printf(char *fmt, ...);

int main() {
    int x = 10;
    typeof(x) y = 20;
    typeof(x + 1.5) z = 3.14;   /* z should be a double due to parser node scaling */
    
    __auto_type a = 42;
    __auto_type b = z;

    if (sizeof(y) == 4) printf("typeof(x) size OK (4)\n");
    if (sizeof(z) == 8) printf("typeof(x + 1.5) size OK (8)\n");
    if (sizeof(a) == 4) printf("__auto_type a size OK (4)\n");
    if (sizeof(b) == 8) printf("__auto_type b size OK (8)\n");
    
    printf("y = %d, a = %d\n", y, a);

    if (x == 10 && y == 20 && a == 42) {
        printf("PASS\n");
        return 0;
    }
    printf("FAIL\n");
    return 1;
}
