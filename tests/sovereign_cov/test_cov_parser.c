struct SimpleStruct {
    int a;
    double b;
};

struct NestedStruct {
    struct SimpleStruct inner;
    char tag[16];
    union {
        int u_int;
        double u_double;
        char *u_ptr;
    } u;
};

struct BitfieldStruct {
    unsigned int b1 : 1;
    unsigned int b2 : 3;
    unsigned int b3 : 7;
    signed int b4 : 5;
    unsigned int : 0; // force alignment
    unsigned int b5 : 16;
};

struct __attribute__((packed)) PackedStruct {
    char c;
    int i;
    short s;
    double d;
};

typedef int (*math_fn_t)(int, int);

int add_fn(int a, int b) { return a + b; }
int sub_fn(int a, int b) { return a - b; }

int test_ast_constructs(int selector, int n) {
    struct NestedStruct ns;
    ns.inner.a = 42;
    ns.inner.b = 3.14;
    ns.u.u_int = 100;
    
    struct BitfieldStruct bfs;
    bfs.b1 = 1;
    bfs.b2 = 5;
    bfs.b3 = 100;
    bfs.b4 = -10;
    bfs.b5 = 65000;
    
    struct PackedStruct ps;
    ps.c = 'Z';
    ps.i = 12345;
    ps.s = 99;
    ps.d = 0.5;
    
    math_fn_t fn = (selector > 0) ? add_fn : sub_fn;
    int fn_res = fn(10, 20);
    
    // Designated initializers
    struct SimpleStruct ss_init = { .b = 9.99, .a = 777 };
    int arr_init[10] = { [0] = 1, [4] = 5, [9] = 10 };
    
    // Control flow statements
    int sum = 0;
    for (int i = 0; i < n; i++) {
        if (i % 2 == 0) continue;
        if (i > 100) break;
        sum += i;
    }
    
    int w = 0;
    while (w < 5) { w++; }
    
    do { w--; } while (w > 0);
    
    switch (selector) {
        case 0: sum += 1; break;
        case 1: sum += 10; // fallthrough
        case 2: sum += 20; break;
        default: sum += 100; break;
    }
    
    return sum + fn_res + ss_init.a + arr_init[4] + ps.i + bfs.b2;
}