#include <stdio.h>
#include "abi_v13_types.h"

int main(void) {
    AbiU8 s8 = { 0x1122334455667788UL };
    AbiU16 s16 = { 0x1122334455667788UL, 0x2233445566778899UL };
    AbiU24 s24 = { 0x1111222233334444UL, 0x5555666677778888UL, 0x123456789abcdef0UL };

    /* 1. Pure INTEGER-class aggregate arguments */
    printf("ARG8:%lx\n", abi_arg_u8(s8));
    printf("ARG16:%lx\n", abi_arg_u16(s16));

    /* 2. MEMORY-class aggregate argument */
    printf("ARG24:%lx\n", abi_arg_u24(s24));

    /* 3. Pure INTEGER-class aggregate returns */
    AbiU8 r8 = abi_ret_u8(0x9988776655443322UL);
    printf("RET8:%lx\n", r8.x);

    AbiU16 r16 = abi_ret_u16(0x13579bdf2468ace0UL, 0x02468ace13579bdfUL);
    printf("RET16:%lx:%lx\n", r16.lo, r16.hi);

    /* 4. MEMORY-class aggregate return (sret) */
    AbiU24 r24 = abi_ret_u24(0x13579bdf2468ace0UL);
    printf("RET24:%lx:%lx:%lx\n", r24.a, r24.b, r24.c);

    return 0;
}
