/* test_alignas.c — Step 1 regression suite for ZCC _Alignas / alignas */
#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <stdalign.h>

_Alignas(0) int a = 1;
_Alignas(16) int b = 2;
_Alignas(double) char c = 'X';
_Alignas(8) _Alignas(64) int d = 4;
static _Alignas(32) unsigned char e[3] = {1, 2, 3};
_Alignas(32) int comma_a = 10, comma_b = 20;

struct AlignedStruct {
    char header;
    _Alignas(32) int aligned_member;
};

int main(void) {
    _Alignas(16) int local_b = 42;
    _Alignas(32) char local_buf[64];
    struct AlignedStruct s;
    s.aligned_member = 99;

    printf("Global alignment checks...\n");
    assert((uintptr_t)&b % 16 == 0);
    assert((uintptr_t)&c % sizeof(double) == 0);
    assert((uintptr_t)&d % 64 == 0);
    assert((uintptr_t)&e % 32 == 0);
    assert((uintptr_t)&comma_a % 32 == 0);
    assert((uintptr_t)&comma_b % 32 == 0);

    printf("Local alignment checks...\n");
    assert((uintptr_t)&local_b % 16 == 0);
    assert((uintptr_t)&local_buf % 32 == 0);

    printf("Struct member alignment checks...\n");
    assert((uintptr_t)&s.aligned_member % 32 == 0);

    printf("[OK] All _Alignas verification tests passed cleanly!\n");
    return 0;
}
