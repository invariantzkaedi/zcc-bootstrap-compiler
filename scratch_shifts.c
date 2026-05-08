#include <stdio.h>
#include <string.h>

typedef struct {
    unsigned char bytes[32];
} evm_u256_t;

static void evm_u256_shl_op(evm_u256_t *dst, const evm_u256_t *shift, const evm_u256_t *val) {
    memset(dst->bytes, 0, 32);
    for (int i = 0; i < 31; i++) {
        if (shift->bytes[i] != 0) return;
    }
    unsigned int s = shift->bytes[31];
    unsigned int byte_shift = s / 8;
    unsigned int bit_shift = s % 8;
    for (int i = 0; i < 32; i++) {
        if (i + byte_shift < 32) {
            unsigned int v = val->bytes[i + byte_shift];
            if (bit_shift == 0) {
                dst->bytes[i] = v;
            } else {
                dst->bytes[i] = (v << bit_shift) & 0xFF;
                if (i + byte_shift + 1 < 32) {
                    dst->bytes[i] |= (val->bytes[i + byte_shift + 1] >> (8 - bit_shift)) & 0xFF;
                }
            }
        }
    }
}

static void evm_u256_shr_op(evm_u256_t *dst, const evm_u256_t *shift, const evm_u256_t *val) {
    memset(dst->bytes, 0, 32);
    for (int i = 0; i < 31; i++) {
        if (shift->bytes[i] != 0) return;
    }
    unsigned int s = shift->bytes[31];
    unsigned int byte_shift = s / 8;
    unsigned int bit_shift = s % 8;
    for (int i = 31; i >= 0; i--) {
        if (i - (int)byte_shift >= 0) {
            unsigned int v = val->bytes[i - byte_shift];
            if (bit_shift == 0) {
                dst->bytes[i] = v;
            } else {
                dst->bytes[i] = (v >> bit_shift) & 0xFF;
                if (i - (int)byte_shift - 1 >= 0) {
                    dst->bytes[i] |= (val->bytes[i - byte_shift - 1] << (8 - bit_shift)) & 0xFF;
                }
            }
        }
    }
}

static void evm_u256_sar_op(evm_u256_t *dst, const evm_u256_t *shift, const evm_u256_t *val) {
    int sign_bit = (val->bytes[0] >> 7) & 1;
    unsigned char fill = sign_bit ? 0xFF : 0x00;
    for (int i = 0; i < 31; i++) {
        if (shift->bytes[i] != 0) {
            memset(dst->bytes, fill, 32);
            return;
        }
    }
    unsigned int s = shift->bytes[31];
    unsigned int byte_shift = s / 8;
    unsigned int bit_shift = s % 8;
    for (int i = 31; i >= 0; i--) {
        if (i - (int)byte_shift >= 0) {
            unsigned int v = val->bytes[i - byte_shift];
            if (bit_shift == 0) {
                dst->bytes[i] = v;
            } else {
                dst->bytes[i] = (v >> bit_shift) & 0xFF;
                if (i - (int)byte_shift - 1 >= 0) {
                    dst->bytes[i] |= (val->bytes[i - byte_shift - 1] << (8 - bit_shift)) & 0xFF;
                } else {
                    dst->bytes[i] |= (fill << (8 - bit_shift)) & 0xFF;
                }
            }
        } else {
            dst->bytes[i] = fill;
        }
    }
}

static void print_u256(const char* name, const evm_u256_t *val) {
    printf("%s: ", name);
    for (int i = 0; i < 32; i++) {
        printf("%02x", val->bytes[i]);
    }
    printf("\n");
}

int main() {
    evm_u256_t val, shift, dst;
    memset(val.bytes, 0, 32);
    memset(shift.bytes, 0, 32);
    
    val.bytes[31] = 0xAA; // 1010 1010
    shift.bytes[31] = 4;
    evm_u256_shl_op(&dst, &shift, &val);
    print_u256("SHL 0xAA << 4", &dst);

    val.bytes[0] = 0x80; // MSB is 1
    evm_u256_shr_op(&dst, &shift, &val);
    print_u256("SHR 0x80..00 >> 4", &dst);

    evm_u256_sar_op(&dst, &shift, &val);
    print_u256("SAR 0x80..00 >> 4", &dst);

    shift.bytes[31] = 255;
    evm_u256_sar_op(&dst, &shift, &val);
    print_u256("SAR 0x80..00 >> 255", &dst);
    
    shift.bytes[30] = 1; shift.bytes[31] = 0;
    evm_u256_sar_op(&dst, &shift, &val);
    print_u256("SAR 0x80..00 >> 256", &dst);
    
    return 0;
}
