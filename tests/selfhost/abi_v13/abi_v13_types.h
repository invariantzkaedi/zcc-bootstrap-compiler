#ifndef ABI_V13_TYPES_H
#define ABI_V13_TYPES_H

typedef struct {
    unsigned long x;
} AbiU8;

typedef struct {
    unsigned long lo;
    unsigned long hi;
} AbiU16;

typedef struct {
    unsigned long a;
    unsigned long b;
    unsigned long c;
} AbiU24;

/* Pure INTEGER-class aggregate arguments */
unsigned long abi_arg_u8(AbiU8 v);
unsigned long abi_arg_u16(AbiU16 v);

/* MEMORY-class aggregate argument (passed in stack argument area) */
unsigned long abi_arg_u24(AbiU24 v);

/* Pure INTEGER-class aggregate returns */
AbiU8  abi_ret_u8(unsigned long x);
AbiU16 abi_ret_u16(unsigned long lo, unsigned long hi);

/* MEMORY-class aggregate return (sret caller-allocated hidden %rdi pointer) */
AbiU24 abi_ret_u24(unsigned long seed);

#endif /* ABI_V13_TYPES_H */
