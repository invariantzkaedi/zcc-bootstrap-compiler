
int main() {
    int result = 0;
    {
        int x = 10;
        int y = 20;
        int *p = &y;
        int *q = &x;
        *p = 50;
        *q = 60;
        result += (*p + *q) & 0xFF;
    }
    {
        int a = 0, b = 1, c = 2;
        if (a && (b = 3)) c = 4;
        if (b != 1) return 200 + 1; // b should not be evaluated
        if (a || (b = 4)) c = 5;
        if (b != 4) return 201 + 1; // b should be evaluated
        result += c & 0xFF;
    }
    {
        int a = 0, b = 1, c = 2;
        if (a && (b = 3)) c = 4;
        if (b != 1) return 200 + 2; // b should not be evaluated
        if (a || (b = 4)) c = 5;
        if (b != 4) return 201 + 2; // b should be evaluated
        result += c & 0xFF;
    }
    {
        struct S_3 { int a, b, c; } s1 = {2, 7, 7};
        struct S_3 s2;
        s2 = s1;
        s1.a = 100;
        result += (s2.a + s2.b + s2.c) & 0xFF;
    }
    {
        int max = 2147483647;
        int x = max + 86; // wrapped
        if (x > 0) return 202 + 4;
        result += (x >> 24) & 0xFF;
    }
    {
        int x = 1 ? 83 : 0;
        int y = 0 ? 0 : 82;
        volatile int z = x + y;
        result += z & 0xFF;
    }
    {
        int x = 1 ? 50 : 0;
        int y = 0 ? 0 : 27;
        volatile int z = x + y;
        result += z & 0xFF;
    }
    {
        int x = 27;
        volatile int *p = &x;
        x = 71;
        *p = 27; // barrier
        result += x & 0xFF;
    }
    return result & 255;
}
