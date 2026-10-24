#ifndef ABI_TYPES_H
#define ABI_TYPES_H

typedef unsigned char  uint8_t;
typedef signed char    int8_t;
typedef unsigned short uint16_t;
typedef short          int16_t;
typedef unsigned int   uint32_t;
typedef int            int32_t;
typedef unsigned long long uint64_t;
typedef long long          int64_t;

typedef struct {
    uint8_t   tag;
    uint32_t  id;
    int16_t   delta;
    uint64_t  mask;
    int32_t   (*callback)(int32_t a, int32_t b);
} ABIPacket;

/* Multi-argument function testing 6 registers + stack parameters (args 7, 8, 9, 10) */
int64_t abi_sum_10args(int64_t a1, int64_t a2, int64_t a3, int64_t a4,
                       int64_t a5, int64_t a6, int64_t a7, int64_t a8,
                       int64_t a9, int64_t a10);

/* Struct layout, alignment, padding and pointer indirection */
int64_t abi_process_packet(ABIPacket *p, int64_t multiplier);

/* Cross-TU function pointer callback invocation */
int32_t abi_callback_proxy(int32_t (*cb)(int32_t, int32_t), int32_t x, int32_t y);

/* Sign and zero extension across integer types */
int64_t abi_sign_extend_test(int8_t i8, uint8_t u8, int16_t i16, uint16_t u16, int32_t i32, uint32_t u32);

#endif /* ABI_TYPES_H */
