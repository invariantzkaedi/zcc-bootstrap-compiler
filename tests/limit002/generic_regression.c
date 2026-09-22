/* Regression test: C11 _Generic selection across types and dispatch macros */
#include <stdio.h>

#define type_name(x) _Generic((x), \
    int: 1, \
    double: 2, \
    float: 3, \
    char*: 4, \
    default: 0 \
)

static int fn_int(int a) { return a * 2; }
static int fn_double(double d) { return (int)(d * 3.0); }

#define dispatch_calc(x) _Generic((x), \
    int: fn_int, \
    double: fn_double \
)(x)

int main(void) {
    int i = 42;
    double d = 3.14;
    float f = 2.5f;
    char *s = "hello";
    long long l = 100LL;

    if (type_name(i) != 1) return 1;
    if (type_name(d) != 2) return 2;
    if (type_name(f) != 3) return 3;
    if (type_name(s) != 4) return 4;
    if (type_name(l) != 0) return 5;

    if (dispatch_calc(10) != 20) return 6;
    if (dispatch_calc(10.0) != 30) return 7;

    return 0;
}
