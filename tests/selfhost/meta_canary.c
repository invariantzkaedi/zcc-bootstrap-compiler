#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Inner {
    int id;
    char code;
};

struct CanaryRecord {
    char tag[8];
    int count;
    long val;
    struct Inner inner;
};

static int sum_array(const int *p, int n) {
    int i;
    int s = 0;
    for (i = 0; i < n; i++) {
        s += p[i];
    }
    return s;
}

static unsigned long mix_bits(unsigned long x) {
    x ^= x << 7;
    x ^= x >> 9;
    x += 0x12345678UL;
    return x;
}

int main(void) {
    int a[5];
    int s;
    unsigned long x;
    struct CanaryRecord rec;

    a[0] = 3;
    a[1] = -7;
    a[2] = 11;
    a[3] = 19;
    a[4] = 5;

    s = sum_array(a, 5);
    x = mix_bits(0x10203040UL);

    memset(&rec, 0, sizeof(rec));
    strcpy(rec.tag, "ZCC_M1");
    rec.count = 42;
    rec.val = 9876543210L;
    rec.inner.id = 101;
    rec.inner.code = 'Z';

    if (s != 31) {
        printf("FAIL: sum=%d expected=31\n", s);
        return 21;
    }

    if (*(a + 2) != 11) {
        printf("FAIL: ptr=%d expected=11\n", *(a + 2));
        return 22;
    }

    if (rec.inner.id != 101 || rec.inner.code != 'Z') {
        printf("FAIL: struct inner mismatch\n");
        return 23;
    }

    printf("CANARY_SUM: %d\n", s);
    printf("CANARY_MIX: 0x%lx\n", x & 0xFFFFFFFFUL);
    printf("CANARY_TAG: %s count=%d val=%ld inner=%d:%c\n",
           rec.tag, rec.count, rec.val, rec.inner.id, rec.inner.code);
    printf("CANARY_STATUS: OK\n");

    return 0;
}
