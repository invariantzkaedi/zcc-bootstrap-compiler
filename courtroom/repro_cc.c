struct S_0 {
        int f0;
        long long f1;
};
static int helper_struct_val_0(struct S_0 s) {
    return (int)s.f0 + (int)s.f1;
}

static int helper_many_args_1(char a0, int a1, char a2, long a3, char a4, int a5) {
    return (int)a0 + (int)a1 + (int)a2 + (int)a3 + (int)a4 + (int)a5;
}

struct S_2 {
        long f0;
        long f1;
};
static int helper_struct_val_2(struct S_2 s) {
    return (int)s.f0 + (int)s.f1;
}

static int helper_many_args_3(int a0, long a1, long long a2, char a3, int a4, char a5, short a6, long a7, char a8, long a9) {
    return (int)a0 + (int)a1 + (int)a2 + (int)a3 + (int)a4 + (int)a5 + (int)a6 + (int)a7 + (int)a8 + (int)a9;
}

int main() {
    int result = 0;
    /* struct val test 0 */
    {
        struct S_0 s = {41, 58};
        int r = helper_struct_val_0(s);
        if (r != 99) return 120 + 0;
        result += r & 0xFF;
    }
    /* many args test 1 */
    {
        int r = helper_many_args_1(40, 66, 10, 62, -52, -91);
        if (r != 35) return 110 + 1;
        result += r & 0xFF;
    }
    /* struct val test 2 */
    {
        struct S_2 s = {12, 42};
        int r = helper_struct_val_2(s);
        if (r != 54) return 120 + 2;
        result += r & 0xFF;
    }
    /* many args test 3 */
    {
        int r = helper_many_args_3(-30, -98, 41, -94, 69, -57, -70, -54, 63, -30);
        if (r != -260) return 110 + 3;
        result += r & 0xFF;
    }
    return result & 255;
}
