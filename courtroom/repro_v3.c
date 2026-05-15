struct SR_0 {
        char f0;
        short f1;
};
static struct SR_0 helper_struct_ret_0(int hidden_arg) {
    struct SR_0 s;
    s.f0 = -35;
    s.f1 = -14;
    return s;
}
struct SR_1 {
        char f0;
        char f1;
        float f2;
        long f3;
        char f4;
        long long f5;
        short f6;
        long f7;
        float f8;
        float f9;
};
static struct SR_1 helper_struct_ret_1(int hidden_arg) {
    struct SR_1 s;
    s.f0 = 2;
    s.f1 = -22;
    s.f2 = -6.0;
    s.f3 = 20;
    s.f4 = -48;
    s.f5 = -49;
    s.f6 = 23;
    s.f7 = -50;
    s.f8 = -1.0;
    s.f9 = -10.0;
    return s;
}
struct SR_2 {
        double f0;
        int f1;
        double f2;
        short f3;
        long long f4;
        float f5;
        long f6;
        char f7;
        double f8;
};
static struct SR_2 helper_struct_ret_2(int hidden_arg) {
    struct SR_2 s;
    s.f0 = -1.0;
    s.f1 = 17;
    s.f2 = -7.0;
    s.f3 = 11;
    s.f4 = 6;
    s.f5 = -3.0;
    s.f6 = 6;
    s.f7 = -39;
    s.f8 = 2.0;
    return s;
}
struct SR_3 {
        double f0;
        short f1;
        float f2;
        long long f3;
        float f4;
        long long f5;
        float f6;
};
static struct SR_3 helper_struct_ret_3(int hidden_arg) {
    struct SR_3 s;
    s.f0 = 7.0;
    s.f1 = 22;
    s.f2 = 5.0;
    s.f3 = -23;
    s.f4 = -8.0;
    s.f5 = -19;
    s.f6 = 8.0;
    return s;
}
struct SR_4 {
        short f0;
        double f1;
        int f2;
        long long f3;
        int f4;
        int f5;
        short f6;
        long long f7;
        int f8;
        short f9;
};
static struct SR_4 helper_struct_ret_4(int hidden_arg) {
    struct SR_4 s;
    s.f0 = -38;
    s.f1 = 7.0;
    s.f2 = -26;
    s.f3 = -47;
    s.f4 = -42;
    s.f5 = 35;
    s.f6 = 35;
    s.f7 = 38;
    s.f8 = -18;
    s.f9 = -18;
    return s;
}
struct SR_5 {
        long f0;
        char f1;
        long long f2;
        int f3;
        char f4;
        long f5;
};
static struct SR_5 helper_struct_ret_5(int hidden_arg) {
    struct SR_5 s;
    s.f0 = 14;
    s.f1 = 49;
    s.f2 = -27;
    s.f3 = 25;
    s.f4 = 29;
    s.f5 = 44;
    return s;
}
int main() {
    int result = 0;
    {
        struct SR_0 s = helper_struct_ret_0(1);
        int r = (int)(s.f0) + (int)(s.f1);
        if (r != -49) return 130 + 0;
        result += r & 0xFF;
    }
    {
        struct SR_1 s = helper_struct_ret_1(1);
        int r = (int)(s.f0) + (int)(s.f1) + (int)(s.f2) + (int)(s.f3) + (int)(s.f4) + (int)(s.f5) + (int)(s.f6) + (int)(s.f7) + (int)(s.f8) + (int)(s.f9);
        if (r != -141) return 130 + 1;
        result += r & 0xFF;
    }
    {
        struct SR_2 s = helper_struct_ret_2(1);
        int r = (int)(s.f0) + (int)(s.f1) + (int)(s.f2) + (int)(s.f3) + (int)(s.f4) + (int)(s.f5) + (int)(s.f6) + (int)(s.f7) + (int)(s.f8);
        if (r != -8) return 130 + 2;
        result += r & 0xFF;
    }
    {
        struct SR_3 s = helper_struct_ret_3(1);
        int r = (int)(s.f0) + (int)(s.f1) + (int)(s.f2) + (int)(s.f3) + (int)(s.f4) + (int)(s.f5) + (int)(s.f6);
        if (r != -8) return 130 + 3;
        result += r & 0xFF;
    }
    {
        struct SR_4 s = helper_struct_ret_4(1);
        int r = (int)(s.f0) + (int)(s.f1) + (int)(s.f2) + (int)(s.f3) + (int)(s.f4) + (int)(s.f5) + (int)(s.f6) + (int)(s.f7) + (int)(s.f8) + (int)(s.f9);
        if (r != -74) return 130 + 4;
        result += r & 0xFF;
    }
    {
        struct SR_5 s = helper_struct_ret_5(1);
        int r = (int)(s.f0) + (int)(s.f1) + (int)(s.f2) + (int)(s.f3) + (int)(s.f4) + (int)(s.f5);
        if (r != 134) return 130 + 5;
        result += r & 0xFF;
    }
    return result & 255;
}
