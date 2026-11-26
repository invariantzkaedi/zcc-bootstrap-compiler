/* SPDX-License-Identifier: MIT */
#include "zcc_compute.h"
#include <string.h>

#define ZCC_ROTR32(x, n) (((x) >> (n)) | ((x) << (32U - (n))))

static void sha256_transform(zcc_sha256_ctx_t *ctx, const uint8_t block[64]) {
    static const uint32_t k[64] = {
        0x428A2F98U, 0x71374491U, 0xB5C0FBCFU, 0xE9B5DBA5U,
        0x3956C25BU, 0x59F111F1U, 0x923F82A4U, 0xAB1C5ED5U,
        0xD807AA98U, 0x12835B01U, 0x243185BEU, 0x550C7DC3U,
        0x72BE5D74U, 0x80DEB1FEU, 0x9BDC06A7U, 0xC19BF174U,
        0xE49B69C1U, 0xEFBE4786U, 0x0FC19DC6U, 0x240CA1CCU,
        0x2DE92C6FU, 0x4A7484AAU, 0x5CB0A9DCU, 0x76F988DAU,
        0x983E5152U, 0xA831C66DU, 0xB00327C8U, 0xBF597FC7U,
        0xC6E00BF3U, 0xD5A79147U, 0x06CA6351U, 0x14292967U,
        0x27B70A85U, 0x2E1B2138U, 0x4D2C6DFCU, 0x53380D13U,
        0x650A7354U, 0x766A0ABBU, 0x81C2C92EU, 0x92722C85U,
        0xA2BFE8A1U, 0xA81A664BU, 0xC24B8B70U, 0xC76C51A3U,
        0xD192E819U, 0xD6990624U, 0xF40E3585U, 0x106AA070U,
        0x19A4C116U, 0x1E376C08U, 0x2748774CU, 0x34B0BCB5U,
        0x391C0CB3U, 0x4ED8AA4AU, 0x5B9CCA4FU, 0x682E6FF3U,
        0x748F82EEU, 0x78A5636FU, 0x84C87814U, 0x8CC70208U,
        0x90BEFFFAU, 0xA4506CEBU, 0xBEF9A3F7U, 0xC67178F2U
    };
    uint32_t w[64];
    for (unsigned int i = 0U; i < 16U; i++) {
        size_t j = (size_t)i * 4U;
        w[i] = ((uint32_t)block[j] << 24) | ((uint32_t)block[j + 1U] << 16) |
               ((uint32_t)block[j + 2U] << 8) | (uint32_t)block[j + 3U];
    }
    for (unsigned int i = 16U; i < 64U; i++) {
        uint32_t s0 = ZCC_ROTR32(w[i - 15U], 7U) ^ ZCC_ROTR32(w[i - 15U], 18U) ^ (w[i - 15U] >> 3U);
        uint32_t s1 = ZCC_ROTR32(w[i - 2U], 17U) ^ ZCC_ROTR32(w[i - 2U], 19U) ^ (w[i - 2U] >> 10U);
        w[i] = w[i - 16U] + s0 + w[i - 7U] + s1;
    }
    uint32_t a = ctx->state[0], b = ctx->state[1], c = ctx->state[2], d = ctx->state[3];
    uint32_t e = ctx->state[4], f = ctx->state[5], g = ctx->state[6], h = ctx->state[7];
    for (unsigned int i = 0U; i < 64U; i++) {
        uint32_t S1 = ZCC_ROTR32(e, 6U) ^ ZCC_ROTR32(e, 11U) ^ ZCC_ROTR32(e, 25U);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t temp1 = h + S1 + ch + k[i] + w[i];
        uint32_t S0 = ZCC_ROTR32(a, 2U) ^ ZCC_ROTR32(a, 13U) ^ ZCC_ROTR32(a, 22U);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t temp2 = S0 + maj;
        h = g; g = f; f = e; e = d + temp1;
        d = c; c = b; b = a; a = temp1 + temp2;
    }
    ctx->state[0] += a; ctx->state[1] += b; ctx->state[2] += c; ctx->state[3] += d;
    ctx->state[4] += e; ctx->state[5] += f; ctx->state[6] += g; ctx->state[7] += h;
}

void zcc_sha256_init(zcc_sha256_ctx_t *ctx) {
    if (!ctx) return;
    ctx->state[0] = 0x6A09E667U; ctx->state[1] = 0xBB67AE85U;
    ctx->state[2] = 0x3C6EF372U; ctx->state[3] = 0xA54FF53AU;
    ctx->state[4] = 0x510E527FU; ctx->state[5] = 0x9B05688CU;
    ctx->state[6] = 0x1F83D9ABU; ctx->state[7] = 0x5BE0CD19U;
    ctx->bitlen = 0ULL;
    ctx->buf_len = 0U;
}

void zcc_sha256_update(zcc_sha256_ctx_t *ctx, const void *data, size_t len) {
    if (!ctx || (!data && len > 0U)) return;
    const uint8_t *p = (const uint8_t *)data;
    size_t i = 0U;
    if (ctx->buf_len > 0U) {
        while (i < len && ctx->buf_len < 64U) {
            ctx->buf[ctx->buf_len++] = p[i++];
        }
        if (ctx->buf_len == 64U) {
            sha256_transform(ctx, ctx->buf);
            ctx->bitlen += 512ULL;
            ctx->buf_len = 0U;
        }
    }
    while (i + 64U <= len) {
        sha256_transform(ctx, p + i);
        ctx->bitlen += 512ULL;
        i += 64U;
    }
    while (i < len) {
        ctx->buf[ctx->buf_len++] = p[i++];
    }
}

void zcc_sha256_final(zcc_sha256_ctx_t *ctx, uint8_t out_digest[32]) {
    if (!ctx || !out_digest) return;
    size_t i = ctx->buf_len;
    if (i < 56U) {
        ctx->buf[i++] = 0x80U;
        while (i < 56U) ctx->buf[i++] = 0U;
    } else {
        ctx->buf[i++] = 0x80U;
        while (i < 64U) ctx->buf[i++] = 0U;
        sha256_transform(ctx, ctx->buf);
        memset(ctx->buf, 0, 56U);
    }
    ctx->bitlen += (uint64_t)ctx->buf_len * 8ULL;
    for (int j = 0; j < 8; j++) {
        ctx->buf[56 + j] = (uint8_t)((ctx->bitlen >> (56 - j * 8)) & 0xFFULL);
    }
    sha256_transform(ctx, ctx->buf);
    for (int j = 0; j < 8; j++) {
        out_digest[j * 4 + 0] = (uint8_t)((ctx->state[j] >> 24) & 0xFFU);
        out_digest[j * 4 + 1] = (uint8_t)((ctx->state[j] >> 16) & 0xFFU);
        out_digest[j * 4 + 2] = (uint8_t)((ctx->state[j] >> 8) & 0xFFU);
        out_digest[j * 4 + 3] = (uint8_t)(ctx->state[j] & 0xFFU);
    }
}

void zcc_sha256_digest_to_hex(const uint8_t digest[32], char out_hex[65]) {
    static const char hx[] = "0123456789abcdef";
    for (int i = 0; i < 32; i++) {
        out_hex[i * 2]     = hx[(digest[i] >> 4) & 0xFU];
        out_hex[i * 2 + 1] = hx[digest[i] & 0xFU];
    }
    out_hex[64] = '\0';
}

void zcc_sha256_hex(const void *data, size_t len, char out_hex[65]) {
    zcc_sha256_ctx_t ctx;
    uint8_t digest[32];
    zcc_sha256_init(&ctx);
    zcc_sha256_update(&ctx, data, len);
    zcc_sha256_final(&ctx, digest);
    zcc_sha256_digest_to_hex(digest, out_hex);
}

void zcc_hmac_sha256_hex(const uint8_t *key, size_t key_len, const void *data, size_t data_len, char out_hex[65]) {
    uint8_t k_pad[64];
    uint8_t o_key_pad[64];
    uint8_t i_key_pad[64];
    uint8_t inner_hash[32];
    uint8_t outer_hash[32];
    zcc_sha256_ctx_t ctx;

    if (!key || key_len == 0U) {
        out_hex[0] = '\0';
        return;
    }

    memset(k_pad, 0, sizeof(k_pad));
    if (key_len > 64U) {
        zcc_sha256_init(&ctx);
        zcc_sha256_update(&ctx, key, key_len);
        zcc_sha256_final(&ctx, k_pad);
    } else {
        memcpy(k_pad, key, key_len);
    }

    for (size_t i = 0U; i < 64U; i++) {
        o_key_pad[i] = (uint8_t)(k_pad[i] ^ 0x5CU);
        i_key_pad[i] = (uint8_t)(k_pad[i] ^ 0x36U);
    }

    zcc_sha256_init(&ctx);
    zcc_sha256_update(&ctx, i_key_pad, 64U);
    if (data && data_len > 0U) {
        zcc_sha256_update(&ctx, data, data_len);
    }
    zcc_sha256_final(&ctx, inner_hash);

    zcc_sha256_init(&ctx);
    zcc_sha256_update(&ctx, o_key_pad, 64U);
    zcc_sha256_update(&ctx, inner_hash, 32U);
    zcc_sha256_final(&ctx, outer_hash);

    zcc_sha256_digest_to_hex(outer_hash, out_hex);
    memset(k_pad, 0, sizeof(k_pad));
    memset(o_key_pad, 0, sizeof(o_key_pad));
    memset(i_key_pad, 0, sizeof(i_key_pad));
}
