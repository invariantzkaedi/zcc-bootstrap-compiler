struct Point {
    long long x;
    long long y;
};

long long test_struct_tbaa(struct Point *p) {
    p->x = 42;
    long long val1 = p->y; /* This load should be cached */
    p->x = 100;            /* Store to x (offset 0) must NOT invalidate cached load of y (offset 8)! */
    long long val2 = p->y; /* This load should reuse val1 via GVN */
    return val1 + val2;
}

long long test_cast_fallback(struct Point *p) {
    p->x = 42;
    long long val1 = p->y;
    char *cp = (char *)p;
    *cp = 99;              /* Store to a char pointer MUST invalidate cached load of y! */
    long long val2 = p->y; /* This load should NOT be GVN-eliminated! */
    return val1 + val2;
}

int gvn_test(int x, int y) {
    int a;
    if (x > 0) {
        a = x + y;   /* computed in BB_then */
    } else {
        a = x + y;   /* same expr in BB_else — should NOT be eliminated cross-branch */
    }
    return a + (x + y);  /* this x+y at join SHOULD be eliminated (dominated by entry) */
}

int forward_test(struct Point *p) {
    p->x = 55;
    return p->x;
}

int main() {
    struct Point pt = {10, 20};
    long long r1 = test_struct_tbaa(&pt);
    long long r2 = test_cast_fallback(&pt);
    return gvn_test(5, 10) + r1 + r2 + forward_test(&pt);
}

int slf_sink(int x) { return x; }

int slf_call_barrier(struct Point *p) {
    p->x = 77;
    slf_sink(p->x);
    return p->x;
}
