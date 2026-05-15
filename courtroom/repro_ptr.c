static int helper_6(void) { return 153; }

int main() {
    int result = 0;
    /* struct offset test 0 */
    {
        struct S0 {
            short f0;
        long f1;
        int f2;
        };
        struct S0 s0 = {89, 8, 63};
        struct S0 *ps0 = &s0;
        if (ps0->f2 != 63) return 130 + 0;
        if (s0.f2 != ps0->f2) return 140 + 0;
        result += s0.f2 & 0xFF;
    }
    /* ptr sub test 1 */
    {
        long long arr[11] = {19, 33, 108, 102, 77, 88, 111, 15, 9, 90, 60};
        long long *lo = &arr[1];
        long long *hi = &arr[3];
        long diff = hi - lo;
        if (diff != 2) return 120 + 1;
        result += (int)diff & 0xFF;
    }
    /* void* cast test 2 */
    {
        long long x2 = 111;
        void *vp2 = &x2;
        long long *tp2 = (long long *)vp2;
        if (*tp2 != 111) return 150 + 2;
        result += *tp2 & 0xFF;
    }
    /* ptr add test 3 */
    {
        short arr[6] = {69, 119, 109, 66, 13, 42};
        short *base = arr;
        short *stepped = base + 2;
        short val_direct = arr[2];
        short val_ptr    = *stepped;
        if (val_direct != val_ptr) return 110 + 3;
        result += val_direct & 0xFF;
    }
    /* array decay test 4 */
    {
        long ad4[2] = {79, 40};
        long *dp4 = ad4;
        if (*dp4 != 79) return 160 + 4;
        if (*(dp4 + 1) != 40) return 170 + 4;
        result += *dp4 & 0xFF;
    }
    /* ptr add test 5 */
    {
        long long arr[5] = {34, 44, 38, 33, 16};
        long long *base = arr;
        long long *stepped = base + 4;
        long long val_direct = arr[4];
        long long val_ptr    = *stepped;
        if (val_direct != val_ptr) return 110 + 5;
        result += val_direct & 0xFF;
    }
    /* funcptr test 6 */
    {
        int (*fp6)(void) = helper_6;
        int fr6 = fp6();
        if (fr6 != 153) return 180 + 6;
        result += fr6 & 0xFF;
    }
    return result & 255;
}
