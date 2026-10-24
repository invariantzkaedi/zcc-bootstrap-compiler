#include "abi_types.h"

int64_t abi_sum_10args(int64_t a1, int64_t a2, int64_t a3, int64_t a4,
                       int64_t a5, int64_t a6, int64_t a7, int64_t a8,
                       int64_t a9, int64_t a10) {
    return a1 + (a2 * 2) + (a3 * 3) + (a4 * 4) +
           (a5 * 5) + (a6 * 6) + (a7 * 7) + (a8 * 8) +
           (a9 * 9) + (a10 * 10);
}

int64_t abi_process_packet(ABIPacket *p, int64_t multiplier) {
    int64_t acc = 0;
    int32_t cb_res;
    
    if (!p) return -1;
    
    acc = (int64_t)p->tag + (int64_t)p->id + (int64_t)p->delta;
    acc ^= (int64_t)(p->mask & 0xFFFFFFFF);
    
    if (p->callback) {
        cb_res = p->callback((int32_t)p->tag, (int32_t)p->delta);
        acc += (int64_t)cb_res;
    }
    
    p->id += 100;
    p->delta -= 5;
    
    return acc * multiplier;
}

int32_t abi_callback_proxy(int32_t (*cb)(int32_t, int32_t), int32_t x, int32_t y) {
    if (!cb) return 0;
    return cb(x, y);
}

int64_t abi_sign_extend_test(int8_t i8, uint8_t u8, int16_t i16, uint16_t u16, int32_t i32, uint32_t u32) {
    int64_t r1 = (int64_t)i8;
    int64_t r2 = (int64_t)u8;
    int64_t r3 = (int64_t)i16;
    int64_t r4 = (int64_t)u16;
    int64_t r5 = (int64_t)i32;
    int64_t r6 = (int64_t)u32;
    
    return r1 + r2 + r3 + r4 + r5 + r6;
}
