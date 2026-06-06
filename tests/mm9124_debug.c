/* mm9124_debug.c — debug seed9124 intermediate values */
int printf(const char *fmt, ...);

static int f1(int a, int b, int c) {
    unsigned long v1 = (unsigned long)((((((-815) << (((((((400) >> (((928) & 15)))) - (((-710569364) - (-71))))) & 15)))) >= (-283)) ? ((5331u) / (((246266380u) | 1))) : (~(7356u))));
    unsigned long v2 = (unsigned long)(((unsigned long)((-(((((476700336) - (0))) & ((((8505u) <= (8693u)) ? -102 : 976))))))));
    long v3 = (long)(-830);
    int _acc = 0;
    v2 = (unsigned long)((~(((((unsigned long)(6563u))) >> ((((((204) >= (-264301484)) ? 1361u : 9508u)) & 15))))));
    v1 = (unsigned long)(7787u);
    v3 = (long)(-437);
    { int v9; for (v9 = 0; v9 < 10; v9++) { v3 += (long)(((long)(8923u))); } }
    printf("f1: v1=%lu v2=%ld v3=%ld\n", v1, (long)v2, v3);
    printf("f1: (int)v1=%d (int)v2=%d (int)v3=%d\n", (int)v1, (int)v2, (int)v3);
    return ((int)v1 + (int)v2 + (int)v3) + a + b + c + _acc;
}

static int f2(int a, int b, int c) {
    unsigned long v10 = (unsigned long)(((((unsigned long)(199))) | ((~(((1976u) ^ (((unsigned long)(0xFFFFFFFFu)))))))));
    unsigned long v11 = (unsigned long)((((-(((((unsigned long)(0x7FFFFFFFu))) - (((8986u) >> (((6692u) & 15)))))))) / (((((((((5524u) & (1335720935u))) | (((unsigned long)(-491))))) >> ((((((((-888) >= (958)) ? 2575u : 3331u)) / (((((8498u) & (4497u))) | 1)))) & 15)))) | 1))));
    int v12 = (int)(((int)(((unsigned int)((((((-846685102) != (-708982819)) ? -641 : 927)) - (((302) / (((((-715) | 1)) == 0 ? 1 : (((-715) | 1)))))))))))));
    int _acc = 0;
    a = (int)(((int)(112)));
    a = (int)((a >> 15) | (a << 1));
    a = (int)((~((~(((573405672) + (127)))))));
    a = (int)((((((((67) < (-128)) ? 991 : 127)) * (-128))) - (((((-93) ^ (-12))) % (((((((int)(4509u))) | 1)) == 0 ? 1 : (((((int)(4509u))) | 1))))))));
    a = (int)((a >> 10) | (a << 6));
    printf("f2: v10=%lu v11=%lu v12=%d a=%d b=%d c=%d\n", v10, v11, v12, a, b, c);
    printf("f2: (int)v10=%d (int)v11=%d\n", (int)v10, (int)v11);
    return ((int)v10 + (int)v11 + (int)v12) + a + b + c + _acc;
}

int main(void) {
    int r1 = f1(6, 2, 3);
    int r2 = f2(6, 2, 3);
    printf("f1=%d f2=%d\n", r1, r2);
    printf("CHECKSUM=%ld\n", (long)r1 + (long)r2);
    return 0;
}
