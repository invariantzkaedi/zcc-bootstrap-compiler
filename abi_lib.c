typedef union { void *p; long i; double n; } Value;
typedef struct { Value v; unsigned char tt; } TValue;
TValue make_num(double d) {
    TValue t;
    t.v.n = d;
    t.tt = 3;
    return t;
}
TValue make_int(long i) {
    TValue t;
    t.v.i = i;
    t.tt = 2;
    return t;
}
