#include "abi_v13_types.h"

unsigned long abi_arg_u8(AbiU8 v) { return v.x; }
unsigned long abi_arg_u16(AbiU16 v) { return (v.lo * 17UL) + (v.hi * 31UL); }
unsigned long abi_arg_u24(AbiU24 v) { return (v.a * 17UL) + (v.b * 31UL) + (v.c * 47UL); }
AbiU8 abi_ret_u8(unsigned long x) { AbiU8 r; r.x = x; return r; }
AbiU24 abi_ret_u24(unsigned long seed) {
    AbiU24 r;
    r.a = seed;
    r.b = seed ^ 0x1111111111111111UL;
    r.c = seed ^ 0x2222222222222222UL;
    return r;
}
