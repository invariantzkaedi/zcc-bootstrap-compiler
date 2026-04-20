int printf(char *fmt, ...);

#define print(x) _Generic((x), \
    int: printf("int: %d\n", x), \
    long: printf("long: %ld\n", x), \
    double: printf("double: %f\n", x), \
    char*: printf("string: %s\n", x), \
    default: printf("unknown type\n"))

#define max(a, b) _Generic((a), \
    int: ((a) > (b) ? (a) : (b)), \
    double: ((a) > (b) ? (a) : (b)))

int main() {
    print(42);
    print(3.14);
    print("hello");
    
    int x = max(10, 20);
    printf("max(10, 20) = %d\n", x);
    
    float f = 2.5f;
    print(f);
    
    printf("PASS\n");
    return 0;
}
