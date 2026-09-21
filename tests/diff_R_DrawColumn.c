#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* =======================================================================================================
 * 🔱 ZKAEDI METAMORPHIC SUPEROPTIMIZATION HARNESS: DOOM R_DrawColumn 🔱
 * Target: Doom's legendary texture-scaling column blitter (r_draw.c)
 * Grounded Agents: MixedRealityCollaborationAgent64 & ZccGvnPunningVerifier
 * Invariant:
 *   ∀ column c = (x, yl, yh, iscale, texturemid, source, colormap):
 *     framebuffer_pixel(scalar) ≡ framebuffer_pixel(superopt)
 *
 * Optimization Passes Synthesized:
 *   1. Invariant Base-Pointer Hoisting (hoisting dc_source & dc_colormap into callee-saved registers)
 *   2. Fixed-Point Step Pre-multiplication (step2, step3, step4 computed once outside inner loop)
 *   3. 4-way Loop Unrolling with Quad-Pixel Framebuffer Interleaving
 *   4. Zero-Drift Scalar Epilogue for (count % 4) Remainder
 * ======================================================================================================= */

#define SCREENWIDTH   320
#define SCREENHEIGHT  200
#define TEXTUREHEIGHT 128
#define TEXTUREMASK   (TEXTUREHEIGHT - 1)
#define FRACBITS      16
#define FRACUNIT      (1 << FRACBITS)

typedef int32_t fixed_t;

/* Global state mimicking classic Doom architecture */
int dc_x;
int dc_yl;
int dc_yh;
fixed_t dc_iscale;
fixed_t dc_texturemid;
const uint8_t *dc_source;
const uint8_t *dc_colormap;
uint8_t *ylookup[SCREENHEIGHT];
int columnofs[SCREENWIDTH];

/* Framebuffers for scalar reference vs superoptimized execution */
static uint8_t fb_scalar[SCREENWIDTH * SCREENHEIGHT];
static uint8_t fb_superopt[SCREENWIDTH * SCREENHEIGHT];

/* Synthetic texture data (128x128) and colormap tables (32 light levels * 256 colors) */
static uint8_t test_texture[128 * 128];
static uint8_t test_colormaps[32 * 256];

void R_InitTables(void) {
    for (int y = 0; y < SCREENHEIGHT; y++) {
        ylookup[y] = (uint8_t *)(intptr_t)(y * SCREENWIDTH);
    }
    for (int x = 0; x < SCREENWIDTH; x++) {
        columnofs[x] = x;
    }
    for (int i = 0; i < 128 * 128; i++) {
        test_texture[i] = (uint8_t)((i * 37 + 13) & 0xFF);
    }
    for (int light = 0; light < 32; light++) {
        for (int c = 0; c < 256; c++) {
            test_colormaps[light * 256 + c] = (uint8_t)((c * (32 - light)) / 32);
        }
    }
}

/* -------------------------------------------------------------------------------------------------------
 * BASELINE: Original Doom R_DrawColumn (linuxdoom-1.10 / r_draw.c)
 * ------------------------------------------------------------------------------------------------------- */
void R_DrawColumn_scalar(uint8_t *dest_base) {
    int count;
    uint8_t *dest;
    fixed_t frac;
    fixed_t fracstep;

    count = dc_yh - dc_yl;
    if (count < 0)
        return;

    dest = dest_base + (intptr_t)ylookup[dc_yl] + columnofs[dc_x];
    fracstep = dc_iscale;
    frac = dc_texturemid + (dc_yl - 100) * fracstep;

    do {
        *dest = dc_colormap[dc_source[(frac >> FRACBITS) & TEXTUREMASK]];
        dest += SCREENWIDTH;
        frac += fracstep;
    } while (count--);
}

/* -------------------------------------------------------------------------------------------------------
 * SUPEROPTIMIZED: ZKAEDI Metamorphic Pass (Pointer Hoisting + 4x Step Unroll + Scalar Epilogue)
 * ------------------------------------------------------------------------------------------------------- */
void R_DrawColumn_superopt(uint8_t *dest_base) {
    int count = dc_yh - dc_yl + 1;
    if (count <= 0)
        return;

    /* Invariant Base-Pointer Hoisting:
     * Hoist global tables into registers, breaking false compiler memory-aliasing dependencies. */
    const uint8_t *src = dc_source;
    const uint8_t *cmap = dc_colormap;
    uint8_t *dest = dest_base + (intptr_t)ylookup[dc_yl] + columnofs[dc_x];

    fixed_t fracstep = dc_iscale;
    fixed_t frac = dc_texturemid + (dc_yl - 100) * fracstep;

    /* Fixed-point step pre-multiplication */
    fixed_t step2 = fracstep + fracstep;
    fixed_t step3 = step2 + fracstep;
    fixed_t step4 = step2 + step2;

    int unrolled_iters = count >> 2; /* count / 4 */
    int remainder = count & 3;       /* count % 4 */

    /* 4-way unrolled core blitter */
    while (unrolled_iters--) {
        dest[0 * SCREENWIDTH] = cmap[src[(frac) >> FRACBITS & TEXTUREMASK]];
        dest[1 * SCREENWIDTH] = cmap[src[(frac + fracstep) >> FRACBITS & TEXTUREMASK]];
        dest[2 * SCREENWIDTH] = cmap[src[(frac + step2) >> FRACBITS & TEXTUREMASK]];
        dest[3 * SCREENWIDTH] = cmap[src[(frac + step3) >> FRACBITS & TEXTUREMASK]];
        dest += 4 * SCREENWIDTH;
        frac += step4;
    }

    /* Zero-drift scalar epilogue for 0..3 remaining pixels */
    switch (remainder) {
        case 3:
            dest[0 * SCREENWIDTH] = cmap[src[(frac) >> FRACBITS & TEXTUREMASK]];
            dest[1 * SCREENWIDTH] = cmap[src[(frac + fracstep) >> FRACBITS & TEXTUREMASK]];
            dest[2 * SCREENWIDTH] = cmap[src[(frac + step2) >> FRACBITS & TEXTUREMASK]];
            break;
        case 2:
            dest[0 * SCREENWIDTH] = cmap[src[(frac) >> FRACBITS & TEXTUREMASK]];
            dest[1 * SCREENWIDTH] = cmap[src[(frac + fracstep) >> FRACBITS & TEXTUREMASK]];
            break;
        case 1:
            dest[0 * SCREENWIDTH] = cmap[src[(frac) >> FRACBITS & TEXTUREMASK]];
            break;
        case 0:
        default:
            break;
    }
}

/* =======================================================================================================
 * ADVERSARIAL DIFFERENTIAL SUITE: 320 Columns, Extreme Zooms, Prime Steps & Full Framebuffer Equality
 * ======================================================================================================= */
int main(void) {
    printf("=================================================================\n");
    printf("🔱 ZKAEDI METAMORPHIC DIFFERENTIAL HARNESS: R_DrawColumn 🔱\n");
    printf("Validating Baseline vs Superoptimized Texture-Scaling Blitter\n");
    printf("=================================================================\n");

    R_InitTables();

    int total_tests = 0;
    int mismatches = 0;

    /* Adversarial scaling factors (Zoom-in, normal, Zoom-out, odd prime step, large overflow step) */
    const fixed_t test_scales[] = {
        3276,     /* 0.05x (extreme zoom-in, slow step) */
        16384,    /* 0.25x */
        65536,    /* 1.0x (normal 1:1 texel to pixel) */
        131072,   /* 2.0x */
        524288,   /* 8.0x (extreme minification) */
        983040,   /* 15.0x */
        43217,    /* Prime step (0.6594x) */
        1234567   /* Massive step (18.83x) */
    };
    int num_scales = sizeof(test_scales) / sizeof(test_scales[0]);

    /* Test Case 1: Exhaustive Full-Screen Column Sweep across All 320 Screen Columns */
    printf("[*] Running 320-Column Sweep across Variable Heights and Scales...\n");

    for (int scale_idx = 0; scale_idx < num_scales; scale_idx++) {
        dc_iscale = test_scales[scale_idx];
        dc_source = test_texture;
        dc_colormap = &test_colormaps[(scale_idx % 32) * 256];
        dc_texturemid = (scale_idx * 13579) - 100000;

        /* Poison framebuffers with 0xAA before each scale test */
        memset(fb_scalar, 0xAA, sizeof(fb_scalar));
        memset(fb_superopt, 0xAA, sizeof(fb_superopt));

        for (int x = 0; x < SCREENWIDTH; x++) {
            dc_x = x;
            /* Vary column height based on sinusoidal perspective profile */
            int yl = (x * 7) % 60;
            int yh = 199 - ((x * 11) % 60);
            if (yl > yh) { int tmp = yl; yl = yh; yh = tmp; }
            dc_yl = yl;
            dc_yh = yh;

            R_DrawColumn_scalar(fb_scalar);
            R_DrawColumn_superopt(fb_superopt);
            total_tests++;
        }

        /* Assert 100% full-framebuffer byte-exact equality */
        if (memcmp(fb_scalar, fb_superopt, sizeof(fb_scalar)) != 0) {
            printf("  [!] DIVERGENCE at scale_idx=%d (iscale=%d)!\n", scale_idx, dc_iscale);
            mismatches++;
        }
    }

    /* Test Case 2: Boundary & Corner Cases (Degenerate Heights & Epilogue Remainder Stress) */
    printf("[*] Stress-testing Corner Cases (yl > yh, single pixel, counts 1..7)...\n");
    const struct { int yl; int yh; fixed_t scale; const char *desc; } corners[] = {
        {50, 40, 65536, "Negative count (dc_yl > dc_yh)"},
        {100, 100, 65536, "Single pixel (count = 1)"},
        {10, 11, 65536, "Two pixels (count = 2)"},
        {20, 22, 65536, "Three pixels (count = 3)"},
        {30, 33, 65536, "Four pixels (exact unroll chunk, count = 4)"},
        {40, 44, 65536, "Five pixels (count = 5, unroll + 1)"},
        {50, 56, 65536, "Seven pixels (count = 7, unroll + 3)"},
        {0, 199, 131072, "Full-screen vertical span (200 pixels)"}
    };
    int num_corners = sizeof(corners) / sizeof(corners[0]);

    for (int i = 0; i < num_corners; i++) {
        memset(fb_scalar, 0x55, sizeof(fb_scalar));
        memset(fb_superopt, 0x55, sizeof(fb_superopt));

        dc_x = 160;
        dc_yl = corners[i].yl;
        dc_yh = corners[i].yh;
        dc_iscale = corners[i].scale;
        dc_texturemid = 32768;
        dc_source = test_texture;
        dc_colormap = test_colormaps;

        R_DrawColumn_scalar(fb_scalar);
        R_DrawColumn_superopt(fb_superopt);
        total_tests++;

        if (memcmp(fb_scalar, fb_superopt, sizeof(fb_scalar)) != 0) {
            printf("  [!] CORNER CASE DIVERGENCE on '%s'!\n", corners[i].desc);
            mismatches++;
        }
    }

    printf("=================================================================\n");
    printf("Total Columns Evaluated:  %d\n", total_tests);
    printf("Bit-Exact Matches:        %d\n", total_tests - mismatches);
    printf("Total Divergences:        %d\n", mismatches);
    printf("=================================================================\n");

    if (mismatches == 0) {
        printf("★ DOOM R_DrawColumn METAMORPHIC TEST: 100%% BIT-EXACT (PASS) ★\n");
        printf("Zero-drift invariant verified: Invariant base pointers & 4x unroll identical to baseline.\n");
        return 0;
    } else {
        printf("FAILED: Detected %d divergences in column blitter!\n", mismatches);
        return 1;
    }
}
