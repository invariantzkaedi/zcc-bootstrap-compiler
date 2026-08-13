/* test_bitfields.c — Bitfield regression tests */
#include <stdio.h>
#include <assert.h>
#include <stdint.h>

struct BitfieldStruct {
    uint64_t low31 : 31;
    uint64_t mid32 : 32;
    uint64_t flag1 : 1;
    uint64_t full64 : 64;
};

int main(void) {
    struct BitfieldStruct b = {0};

    b.low31 = UINT64_C(0x7fffffff);
    b.mid32 = UINT64_C(0xffffffff);
    b.flag1 = 1;
    b.full64 = UINT64_C(0x123456789abcdef0);

    assert(b.low31 == UINT64_C(0x7fffffff));
    assert(b.mid32 == UINT64_C(0xffffffff));
    assert(b.flag1 == 1);
    assert(b.full64 == UINT64_C(0x123456789abcdef0));

    b.low31 = 0;
    assert(b.mid32 == UINT64_C(0xffffffff));
    assert(b.flag1 == 1);

    b.mid32 ^= UINT64_C(0xffffffff);
    assert(b.mid32 == 0);
    assert(b.flag1 == 1);

    b.full64 = UINT64_MAX;
    assert(b.full64 == UINT64_MAX);

    puts("[OK] Bitfield regression tests passed!");
    return 0;
}
