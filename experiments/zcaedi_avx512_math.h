#ifndef ZCAEDI_AVX512_MATH_H
#define ZCAEDI_AVX512_MATH_H

// Sovereign Matrix Math Header
// Zero-dependency pure vector structures for ZCC compilation
// Replaces <immintrin.h> and bypassed vector attributes

typedef struct { float v[16]; } __attribute__((aligned(64))) v16f;
typedef struct { int v[16]; } __attribute__((aligned(64))) v16i;

#define PI_F 3.14159265f
#define HALF_PI_F 1.57079632f
#define TWO_PI_F 6.28318530f

static inline v16f _mm512_set_ps(float f15, float f14, float f13, float f12, float f11, float f10, float f9, float f8, float f7, float f6, float f5, float f4, float f3, float f2, float f1, float f0) {
    v16f res;
    res.v[0] = f0; res.v[1] = f1; res.v[2] = f2; res.v[3] = f3;
    res.v[4] = f4; res.v[5] = f5; res.v[6] = f6; res.v[7] = f7;
    res.v[8] = f8; res.v[9] = f9; res.v[10] = f10; res.v[11] = f11;
    res.v[12] = f12; res.v[13] = f13; res.v[14] = f14; res.v[15] = f15;
    return res;
}

static inline v16f _mm512_set1_ps(float x) {
    v16f res;
    for(int i=0; i<16; i++) res.v[i] = x;
    return res;
}

static inline v16f _mm512_setzero_ps() {
    return _mm512_set1_ps(0.0f);
}

static inline v16i _mm512_set1_epi32(int x) {
    v16i res;
    for(int i=0; i<16; i++) res.v[i] = x;
    return res;
}

static inline v16f _mm512_add_ps(v16f a, v16f b) {
    v16f res;
    for(int i=0; i<16; i++) res.v[i] = a.v[i] + b.v[i];
    return res;
}

static inline v16f _mm512_sub_ps(v16f a, v16f b) {
    v16f res;
    for(int i=0; i<16; i++) res.v[i] = a.v[i] - b.v[i];
    return res;
}

static inline v16f _mm512_mul_ps(v16f a, v16f b) {
    v16f res;
    for(int i=0; i<16; i++) res.v[i] = a.v[i] * b.v[i];
    return res;
}

static inline v16f _mm512_div_ps(v16f a, v16f b) {
    v16f res;
    for(int i=0; i<16; i++) res.v[i] = a.v[i] / b.v[i];
    return res;
}

static inline v16f _mm512_fmadd_ps(v16f a, v16f b, v16f c) {
    v16f res;
    for(int i=0; i<16; i++) res.v[i] = (a.v[i] * b.v[i]) + c.v[i];
    return res;
}

static inline v16f _mm512_fnmadd_ps(v16f a, v16f b, v16f c) {
    v16f res;
    for(int i=0; i<16; i++) res.v[i] = c.v[i] - (a.v[i] * b.v[i]);
    return res;
}

static inline v16f _mm512_and_ps(v16f a, v16f b) {
    v16f res;
    int* dst = (int*)&res;
    int* srcA = (int*)&a;
    int* srcB = (int*)&b;
    for(int i=0; i<16; i++) dst[i] = srcA[i] & srcB[i];
    return res;
}

static inline v16f _mm512_roundscale_ps(v16f x, int imm) {
    v16f res;
    for(int i=0; i<16; i++) {
        res.v[i] = (float)((int)(x.v[i] + (x.v[i] > 0.0f ? 0.5f : -0.5f)));
    }
    return res;
}

static inline v16i _mm512_cmp_ps_mask_v16i(v16f a, v16f b, int imm) {
    v16i res;
    for(int i=0; i<16; i++) {
        res.v[i] = (a.v[i] < b.v[i]) ? 1 : 0;
    }
    return res;
}

static inline v16i _mm512_cmp_ps_mask_ge(v16f a, v16f b) {
    v16i res;
    for(int i=0; i<16; i++) res.v[i] = (a.v[i] >= b.v[i]) ? 1 : 0;
    return res;
}

static inline v16i _mm512_mask_cmp_ps_mask(v16i mask, v16f a, v16f b, int imm) {
    v16i res;
    for(int i=0; i<16; i++) {
        res.v[i] = (mask.v[i] && (a.v[i] < b.v[i])) ? 1 : 0;
    }
    return res;
}

static inline v16i _mm512_and_epi32(v16i a, v16i b) {
    v16i res;
    for(int i=0; i<16; i++) res.v[i] = a.v[i] & b.v[i];
    return res;
}

static inline unsigned int _mm512_movepi32_mask(v16i mask) {
    unsigned int m = 0;
    for(int i=0; i<16; i++) {
        if(mask.v[i]) m |= (1U << i);
    }
    return m;
}

static inline v16f _mm512_maskz_loadu_ps(v16i mask, const float* mem) {
    v16f res;
    for(int i=0; i<16; i++) res.v[i] = mask.v[i] ? mem[i] : 0.0f;
    return res;
}

static inline void _mm512_mask_storeu_ps(float* mem, v16i mask, v16f a) {
    for(int i=0; i<16; i++) {
        if(mask.v[i]) mem[i] = a.v[i];
    }
}

static inline v16f _mm512_mask_blend_ps(v16i mask, v16f a, v16f b) {
    v16f res;
    for(int i=0; i<16; i++) {
        res.v[i] = mask.v[i] ? b.v[i] : a.v[i];
    }
    return res;
}

static inline v16f _mm512_mask_sub_ps_v16i(v16f src, v16i mask, v16f a, v16f b) {
    v16f res;
    for(int i=0; i<16; i++) {
        float sub_res = a.v[i] - b.v[i];
        res.v[i] = mask.v[i] ? sub_res : src.v[i];
    }
    return res;
}

static inline v16f _mm512_rcp14_ps(v16f x) {
    v16f res;
    for(int i=0; i<16; i++) res.v[i] = 1.0f / x.v[i];
    return res;
}

static inline v16f _mm512_rsqrt14_ps(v16f x) {
    v16f res;
    for(int i=0; i<16; i++) {
        float number = x.v[i];
        long i_val;
        float x2, y;
        const float threehalfs = 1.5F;
        x2 = number * 0.5F;
        y  = number;
        i_val  = * ( long * ) &y;
        i_val  = 0x5f3759df - ( i_val >> 1 );
        y  = * ( float * ) &i_val;
        y  = y * ( threehalfs - ( x2 * y * y ) );
        res.v[i] = y;
    }
    return res;
}

// Math approximations
static inline v16f _mm512_sin_ps(v16f x) {
    v16f inv_two_pi = _mm512_set1_ps(1.0f / TWO_PI_F);
    v16f two_pi = _mm512_set1_ps(TWO_PI_F);
    v16f q = _mm512_roundscale_ps(_mm512_mul_ps(x, inv_two_pi), 0);
    x = _mm512_fnmadd_ps(q, two_pi, x);
    
    v16f x2 = _mm512_mul_ps(x, x);
    
    v16f c1 = _mm512_set1_ps(-0.166666666f); 
    v16f c2 = _mm512_set1_ps(0.00833333333f);  
    v16f c3 = _mm512_set1_ps(-0.000198412698f); 
    v16f c4 = _mm512_set1_ps(0.00000275573192f);  
    
    v16f y = _mm512_fmadd_ps(x2, c4, c3);
    y = _mm512_fmadd_ps(y, x2, c2);
    y = _mm512_fmadd_ps(y, x2, c1);
    y = _mm512_fmadd_ps(y, x2, _mm512_setzero_ps());
    
    y = _mm512_fmadd_ps(y, x, x);
    return y;
}

static inline v16f _mm512_cos_ps(v16f x) {
    v16f half_pi = _mm512_set1_ps(HALF_PI_F);
    return _mm512_sin_ps(_mm512_add_ps(x, half_pi));
}

static inline v16f _mm512_atan2_ps(v16f y, v16f x) {
    v16f zero = _mm512_setzero_ps();
    v16f pi = _mm512_set1_ps(PI_F);
    v16f half_pi = _mm512_set1_ps(HALF_PI_F);
    v16f eps = _mm512_set1_ps(0.0000001f);
    
    v16f abs_y, abs_x;
    for(int i=0; i<16; i++) {
        int yi = *(int*)&y.v[i];
        yi &= 0x7FFFFFFF;
        abs_y.v[i] = *(float*)&yi;
        
        int xi = *(int*)&x.v[i];
        xi &= 0x7FFFFFFF;
        abs_x.v[i] = *(float*)&xi + 0.0000001f;
    }
    
    v16i swap = _mm512_cmp_ps_mask_v16i(abs_x, abs_y, 1);
    
    v16f num = _mm512_mask_blend_ps(swap, abs_y, abs_x);
    v16f den = _mm512_mask_blend_ps(swap, abs_x, abs_y);
    
    v16f z = _mm512_div_ps(num, den);
    v16f z2 = _mm512_mul_ps(z, z);
    
    v16f c1 = _mm512_set1_ps(-0.084679228f);
    v16f c2 = _mm512_set1_ps(0.332889505f);
    v16f c3 = _mm512_set1_ps(1.0f);
    
    v16f res = _mm512_fmadd_ps(z2, c1, c2);
    res = _mm512_fnmadd_ps(res, z2, c3);
    res = _mm512_mul_ps(res, z);
    
    res = _mm512_mask_sub_ps_v16i(res, swap, half_pi, res);
    
    v16i x_lt_zero = _mm512_cmp_ps_mask_v16i(x, zero, 1);
    v16i y_lt_zero = _mm512_cmp_ps_mask_v16i(y, zero, 1);
    
    res = _mm512_mask_sub_ps_v16i(res, x_lt_zero, pi, res);
    res = _mm512_mask_sub_ps_v16i(res, y_lt_zero, zero, res);
    
    return res;
}



static inline float sovereign_cosf(float f) {
    v16f x;
    for(int i=0; i<16; i++) x.v[i] = f;
    v16f res = _mm512_cos_ps(x);
    return res.v[0];
}

static inline float sovereign_sinf(float f) {
    v16f x;
    for(int i=0; i<16; i++) x.v[i] = f;
    v16f res = _mm512_sin_ps(x);
    return res.v[0];
}



static inline v16f _mm512_min_ps(v16f a, v16f b) {
    v16f res;
    for(int i=0; i<16; i++) res.v[i] = a.v[i] < b.v[i] ? a.v[i] : b.v[i];
    return res;
}

static inline v16f _mm512_max_ps(v16f a, v16f b) {
    v16f res;
    for(int i=0; i<16; i++) res.v[i] = a.v[i] > b.v[i] ? a.v[i] : b.v[i];
    return res;
}

static inline v16f _mm512_mask_mul_ps_v16i(v16f src, v16i mask, v16f a, v16f b) {
    v16f res;
    for(int i=0; i<16; i++) {
        res.v[i] = mask.v[i] ? (a.v[i] * b.v[i]) : src.v[i];
    }
    return res;
}

static inline v16f _mm512_loadu_ps(const float* mem) {
    v16f res;
    for(int i=0; i<16; i++) res.v[i] = mem[i];
    return res;
}

static inline void _mm512_storeu_ps(float* mem, v16f x) {
    for(int i=0; i<16; i++) mem[i] = x.v[i];
}

static inline float _mm512_reduce_add_ps(v16f a) {
    float sum = 0.0f;
    for(int i=0; i<16; i++) sum += a.v[i];
    return sum;
}

static inline v16f _mm512_mask_add_ps_v16i(v16f src, v16i mask, v16f a, v16f b) {
    v16f res;
    for(int i=0; i<16; i++) {
        res.v[i] = mask.v[i] ? (a.v[i] + b.v[i]) : src.v[i];
    }
    return res;
}

static inline v16f _mm512_mask_mov_ps(v16f src, v16i mask, v16f a) {
    v16f res;
    for(int i=0; i<16; i++) {
        res.v[i] = mask.v[i] ? a.v[i] : src.v[i];
    }
    return res;
}

static inline v16f sovereign_mask_fmadd_ps(v16f src, v16i mask, v16f multiplier, v16f multiplicand) {
    // Accumulates: src += multiplier * multiplicand if mask is set
    v16f res;
    for(int i=0; i<16; i++) {
        res.v[i] = mask.v[i] ? (src.v[i] + multiplier.v[i] * multiplicand.v[i]) : src.v[i];
    }
    return res;
}

/* ── Aligned load (sovereign = same as loadu) ── */
static inline v16f _mm512_load_ps(const float *src) {
    v16f r; for(int i=0;i<16;i++) r.v[i]=src[i]; return r;
}

/* ── Aligned store (sovereign = same as storeu) ── */
static inline void _mm512_store_ps(float *dst, v16f src) {
    for(int i=0;i<16;i++) dst[i]=src.v[i];
}

/* ── Non-temporal streaming store (semantically identical to storeu in sovereign mode) ── */
static inline void _mm512_stream_ps(float *dst, v16f src) {
    for(int i=0; i<16; i++) dst[i] = src.v[i];
}

/* ── Prefetch stub (no-op in sovereign scalar-emulation mode) ── */
#ifndef _MM_HINT_T0
#define _MM_HINT_T0 1
#endif
static inline void _mm_prefetch(const char *p, int hint) { (void)p; (void)hint; }

/* ── si512 zero ── */
static inline v16i _mm512_setzero_si512() {
    v16i r; for(int i=0;i<16;i++) r.v[i]=0; return r;
}

/* ── Horizontal reduce ops ── */

static inline int _mm512_reduce_add_epi32(v16i a) {
    int s = 0; for(int i=0;i<16;i++) s+=a.v[i]; return s;
}

/* ── Masked integer add ── */
static inline v16i _mm512_mask_add_epi32(v16i src, unsigned short mask, v16i a, v16i b) {
    v16i r;
    for(int i=0;i<16;i++) r.v[i] = (mask >> i & 1) ? (a.v[i]+b.v[i]) : src.v[i];
    return r;
}

/* ── sqrt ── */
extern float sqrtf(float);
static inline v16f _mm512_sqrt_ps(v16f a) {
    v16f r; for(int i=0;i<16;i++) r.v[i]=sqrtf(a.v[i]); return r;
}

/* ── rsqrt (approx) ── */
static inline v16f _mm512_rsqrt_ps(v16f a) {
    v16f r; for(int i=0;i<16;i++) r.v[i] = (a.v[i]>0.0f) ? (1.0f/sqrtf(a.v[i])) : 0.0f; return r;
}

/* ── rcp (approx) ── */
static inline v16f _mm512_rcp_ps(v16f a) {
    v16f r; for(int i=0;i<16;i++) r.v[i] = (a.v[i]!=0.0f) ? (1.0f/a.v[i]) : 0.0f; return r;
}

/* ── CMP predicates ── */
#define _CMP_GT_OQ 14
#define _CMP_LT_OQ 17
#define _CMP_GE_OQ 13

static inline unsigned short _mm512_cmp_ps_mask(const v16f *a, const v16f *b, int pred) {
    unsigned short m = 0;
    for(int i=0;i<16;i++) {
        int hit = 0;
        if(pred==_CMP_GT_OQ) hit = (a->v[i]>b->v[i]);
        else if(pred==_CMP_LT_OQ) hit = (a->v[i]<b->v[i]);
        else if(pred==_CMP_GE_OQ) hit = (a->v[i]>=b->v[i]);
        m |= (hit ? 1 : 0) << i;
    }
    return m;
}

/* ── Masked ps ops ── */
static inline v16f _mm512_mask_add_ps(v16f src, unsigned short mask, v16f a, v16f b) {
    v16f r; for(int i=0;i<16;i++) r.v[i]=(mask>>i&1)?(a.v[i]+b.v[i]):src.v[i]; return r;
}
static inline v16f _mm512_mask_sub_ps(v16f src, unsigned short mask, v16f a, v16f b) {
    v16f r; for(int i=0;i<16;i++) r.v[i]=(mask>>i&1)?(a.v[i]-b.v[i]):src.v[i]; return r;
}
static inline v16f _mm512_mask_mul_ps(v16f src, unsigned short mask, v16f a, v16f b) {
    v16f r; for(int i=0;i<16;i++) r.v[i]=(mask>>i&1)?(a.v[i]*b.v[i]):src.v[i]; return r;
}

#endif
