#include <stdio.h>
#include <string.h>
#include <stdint.h>

typedef struct {
    unsigned char bytes[32];
} evm_u256_t;

static const uint64_t keccakf_rndc[24] = {
    0x0000000000000001ULL, 0x0000000000008082ULL, 0x800000000000808aULL,
    0x8000000080008000ULL, 0x000000000000808bULL, 0x0000000080000001ULL,
    0x8000000080008081ULL, 0x8000000000008009ULL, 0x000000000000008aULL,
    0x0000000000000088ULL, 0x0000000080008009ULL, 0x000000008000000aULL,
    0x000000008000808bULL, 0x800000000000008bULL, 0x8000000000008089ULL,
    0x8000000000008003ULL, 0x8000000000008002ULL, 0x8000000000000080ULL,
    0x000000000000800aULL, 0x800000008000000aULL, 0x8000000080008081ULL,
    0x8000000000008080ULL, 0x0000000080000001ULL, 0x8000000080008008ULL
};

static const unsigned int keccakf_rotc[24] = {
    1,  3,  6,  10, 15, 21, 28, 36, 45, 55, 2,  14,
    27, 41, 56, 8,  25, 43, 62, 18, 39, 61, 20, 44
};

static const unsigned int keccakf_piln[24] = {
    10, 7,  11, 17, 18, 3, 5,  16, 8,  21, 24, 4,
    15, 23, 19, 13, 12, 2, 20, 14, 22, 9,  6,  1
};

#define ROTL64(x, y) (((x) << (y)) | ((x) >> (64 - (y))))

static void keccakf(uint64_t st[25]) {
    int i, j, round;
    uint64_t t, bc[5];
    for (round = 0; round < 24; round++) {
        for (i = 0; i < 5; i++)
            bc[i] = st[i] ^ st[i + 5] ^ st[i + 10] ^ st[i + 15] ^ st[i + 20];
        for (i = 0; i < 5; i++) {
            t = bc[(i + 4) % 5] ^ ROTL64(bc[(i + 1) % 5], 1);
            for (j = 0; j < 25; j += 5)
                st[j + i] ^= t;
        }
        t = st[1];
        for (i = 0; i < 24; i++) {
            j = keccakf_piln[i];
            bc[0] = st[j];
            st[j] = ROTL64(t, keccakf_rotc[i]);
            t = bc[0];
        }
        for (j = 0; j < 25; j += 5) {
            for (i = 0; i < 5; i++)
                bc[i] = st[j + i];
            for (i = 0; i < 5; i++)
                st[j + i] ^= (~bc[(i + 1) % 5]) & bc[(i + 2) % 5];
        }
        st[0] ^= keccakf_rndc[round];
    }
}

static void evm_keccak256(const unsigned char *data, size_t len, evm_u256_t *out) {
    uint64_t st[25];
    memset(st, 0, sizeof(st));
    size_t i, r = 136;
    while (len >= r) {
        for (i = 0; i < r; i++) {
            st[i/8] ^= (uint64_t)(data[i]) << (8 * (i % 8));
        }
        keccakf(st);
        data += r;
        len -= r;
    }
    for (i = 0; i < len; i++) {
        st[i/8] ^= (uint64_t)(data[i]) << (8 * (i % 8));
    }
    st[len/8] ^= (uint64_t)1 << (8 * (len % 8));
    st[(r-1)/8] ^= (uint64_t)0x80 << (8 * ((r-1) % 8));
    keccakf(st);
    
    for (i = 0; i < 32; i++) {
        out->bytes[i] = (unsigned char)((st[i/8] >> (8 * (i % 8))) & 0xFF);
    }
}

int main() {
    evm_u256_t out;
    evm_keccak256((const unsigned char*)"", 0, &out);
    printf("keccak256('') = ");
    for (int i=0; i<32; i++) printf("%02x", out.bytes[i]);
    printf("\n(want c5d2460186f7233c927e7db2dcc703c0e500b653ca82273b7bfad8045d85a470)\n");

    evm_keccak256((const unsigned char*)"hello", 5, &out);
    printf("keccak256('hello') = ");
    for (int i=0; i<32; i++) printf("%02x", out.bytes[i]);
    printf("\n(want 1c8aff950685c2ed4bc3174f3472287b56d9517b9c948127319a09a7a36deac8)\n");
    return 0;
}
