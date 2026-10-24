#include <stdio.h>
#include "abi_types.h"

static int32_t my_callback(int32_t a, int32_t b) {
    return (a * 10) + b;
}

int main(void) {
    int64_t sum;
    int64_t proc_res;
    int32_t cb_res;
    int64_t sext_res;
    ABIPacket pkt;

    printf("ABI_CANARY_START\n");

    /* 1. Multi-argument test (10 arguments spanning registers + stack) */
    sum = abi_sum_10args(10, 20, 30, 40, 50, 60, 70, 80, 90, 100);
    printf("ABI_SUM_10: %lld\n", (long long)sum);

    /* 2. Struct layout and pointer passing */
    pkt.tag = 42;
    pkt.id = 1000;
    pkt.delta = -15;
    pkt.mask = 0x1234567890ABCDEFULL;
    pkt.callback = my_callback;

    proc_res = abi_process_packet(&pkt, 3);
    printf("ABI_PACKET_RES: %lld (new_id=%u, new_delta=%d)\n",
           (long long)proc_res, pkt.id, (int)pkt.delta);

    /* 3. Cross-TU callback proxy */
    cb_res = abi_callback_proxy(my_callback, 7, 3);
    printf("ABI_CALLBACK_RES: %d\n", cb_res);

    /* 4. Sign and zero extension */
    sext_res = abi_sign_extend_test((int8_t)-10, (uint8_t)250, (int16_t)-1000, (uint16_t)65000, (int32_t)-50000, (uint32_t)4000000000U);
    printf("ABI_SEXT_RES: %lld\n", (long long)sext_res);

    printf("ABI_CANARY_END\n");
    return 0;
}
