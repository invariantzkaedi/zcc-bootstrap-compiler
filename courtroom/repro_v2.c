struct SR_0 {
        char f0;
        int f1;
        char f2;
        long f3;
};
static struct SR_0 helper_struct_ret_0() {
    struct SR_0 s;
    s.f0 = 35;
    s.f1 = 24;
    s.f2 = 40;
    s.f3 = 2;
    return s;
}
struct SR_1 {
        int f0;
        long f1;
};
static struct SR_1 helper_struct_ret_1() {
    struct SR_1 s;
    s.f0 = 24;
    s.f1 = 42;
    return s;
}
struct SR_2 {
        int f0;
        short f1;
};
static struct SR_2 helper_struct_ret_2() {
    struct SR_2 s;
    s.f0 = 11;
    s.f1 = 25;
    return s;
}
struct SR_3 {
        char f0;
        int f1;
        short f2;
        char f3;
        short f4;
};
static struct SR_3 helper_struct_ret_3() {
    struct SR_3 s;
    s.f0 = 24;
    s.f1 = 23;
    s.f2 = 4;
    s.f3 = 42;
    s.f4 = 25;
    return s;
}
#include <stdarg.h>
static int helper_variadic_4(int count, ...) {
    va_list ap;
    va_start(ap, count);
    int s = (int)(va_arg(ap, int)) + (int)(va_arg(ap, long long)) + (int)(va_arg(ap, int)) + (int)(va_arg(ap, int));
    va_end(ap);
    return s;
}
int main() {
    int result = 0;
    {
        struct SR_0 s = helper_struct_ret_0();
        if (s.f0 != 35) return 130 + 0;
        result += s.f0 & 0xFF;
    }
    {
        struct SR_1 s = helper_struct_ret_1();
        if (s.f0 != 24) return 130 + 1;
        result += s.f0 & 0xFF;
    }
    {
        struct SR_2 s = helper_struct_ret_2();
        if (s.f0 != 11) return 130 + 2;
        result += s.f0 & 0xFF;
    }
    {
        struct SR_3 s = helper_struct_ret_3();
        if (s.f0 != 24) return 130 + 3;
        result += s.f0 & 0xFF;
    }
    {
        int r = helper_variadic_4(4, (char)-43, (long long)-42, (char)30, (int)22);
        if (r != -33) return 140 + 4;
        result += r & 0xFF;
    }
    return result & 255;
}
