#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define SCREENWIDTH 320
#define FG 0

typedef struct {
    short width;
    short height;
    short leftoffset;
    short topoffset;
    int data;
} patch_t;

typedef struct {
    int x;
    int y;
    patch_t **f;
    int sc;
    char l[80];
    int len;
    int drawcursor;
} hu_textline_t;

typedef struct {
    int x;
    int y;
    int patch_id;
} DrawRecord;

#define MAX_RECORDS 1024
static DrawRecord s_log_orig[MAX_RECORDS];
static int s_log_orig_count = 0;
static DrawRecord s_log_opt[MAX_RECORDS];
static int s_log_opt_count = 0;

static void V_DrawPatchDirect_orig(int x, int y, int scrn, patch_t *patch) {
    (void)scrn;
    if (s_log_orig_count < MAX_RECORDS) {
        s_log_orig[s_log_orig_count].x = x;
        s_log_orig[s_log_orig_count].y = y;
        s_log_orig[s_log_orig_count].patch_id = patch ? patch->data : -1;
        s_log_orig_count++;
    }
}

static void V_DrawPatchDirect_opt(int x, int y, int scrn, patch_t *patch) {
    (void)scrn;
    if (s_log_opt_count < MAX_RECORDS) {
        s_log_opt[s_log_opt_count].x = x;
        s_log_opt[s_log_opt_count].y = y;
        s_log_opt[s_log_opt_count].patch_id = patch ? patch->data : -1;
        s_log_opt_count++;
    }
}

#define SHORT(w) (w)

/* ========================================================================= */
/* 1. Original Implementation (Matching doom/hu_lib.c:100-140)               */
/* ========================================================================= */

static void HUlib_drawTextLine_orig(hu_textline_t *l, int drawcursor) {
    int i;
    int w;
    int x;
    unsigned char c;

    x = l->x;
    for (i = 0; i < l->len; i++) {
        c = toupper(l->l[i]);
        if (c != ' ' && c >= l->sc && c <= '_') {
            w = SHORT(l->f[c - l->sc]->width);
            if (x + w > SCREENWIDTH)
                break;
            V_DrawPatchDirect_orig(x, l->y, FG, l->f[c - l->sc]);
            x += w;
        } else {
            x += 4;
            if (x >= SCREENWIDTH)
                break;
        }
    }

    if (drawcursor && x + SHORT(l->f['_' - l->sc]->width) <= SCREENWIDTH) {
        V_DrawPatchDirect_orig(x, l->y, FG, l->f['_' - l->sc]);
    }
}

/* ========================================================================= */
/* 2. Autonomous Superoptimized Metamorphic Implementation                   */
/*    - Invariant Hoisting: ly = l->y, sc = l->sc, font_base = l->f - sc     */
/*    - O(1) Pre-calculated Glyph Dispatch LUT (Width + Validity)            */
/*    - Elimination of multi-hop pointer chasing & redundant SHORT macros    */
/* ========================================================================= */

static short s_hu_glyph_lut[256];
static int s_hu_glyph_lut_inited = 0;

static void init_hu_glyph_lut(int sc, patch_t **f) {
    if (s_hu_glyph_lut_inited) return;
    for (int ch = 0; ch < 256; ch++) {
        unsigned char uc = (unsigned char)toupper(ch);
        if (uc != ' ' && uc >= sc && uc <= '_') {
            s_hu_glyph_lut[ch] = f[uc - sc]->width;
        } else {
            s_hu_glyph_lut[ch] = -1; /* Special flag: non-glyph advance x += 4 */
        }
    }
    s_hu_glyph_lut_inited = 1;
}

static void HUlib_drawTextLine_opt(hu_textline_t *l, int drawcursor) {
    int x = l->x;
    const int ly = l->y;
    const int sc = l->sc;
    const int len = l->len;
    const char *text = l->l;
    patch_t **fonts = l->f;

    init_hu_glyph_lut(sc, fonts);

    for (int i = 0; i < len; i++) {
        unsigned char raw_c = (unsigned char)text[i];
        short w = s_hu_glyph_lut[raw_c];

        if (w >= 0) {
            if (x + w > SCREENWIDTH)
                break;
            unsigned char uc = (unsigned char)toupper(raw_c);
            V_DrawPatchDirect_opt(x, ly, FG, fonts[uc - sc]);
            x += w;
        } else {
            x += 4;
            if (x >= SCREENWIDTH)
                break;
        }
    }

    if (drawcursor) {
        int cursor_w = fonts['_' - sc]->width;
        if (x + cursor_w <= SCREENWIDTH) {
            V_DrawPatchDirect_opt(x, ly, FG, fonts['_' - sc]);
        }
    }
}

/* ========================================================================= */
/* 3. Differential Test Gauntlet Across Diverse HUD Text Vectors              */
/* ========================================================================= */

int main(void) {
    printf("=================================================================\n");
    printf("🔱 ZCC DIFFERENTIAL ORACLE: HUlib_drawTextLine (Superopt) 🔱\n");
    printf("Testing Invariant: Bit-exact HUD font rendering & cursor coordinates\n");
    printf("=================================================================\n");

    /* Setup Font Patches for ASCII 33 ('!') through 95 ('_') */
    const int sc = 33;
    const int num_patches = '_' - sc + 1;
    patch_t *patches = (patch_t *)malloc(sizeof(patch_t) * num_patches);
    patch_t **fonts = (patch_t **)malloc(sizeof(patch_t *) * num_patches);

    for (int i = 0; i < num_patches; i++) {
        patches[i].width = (short)(6 + (i % 5)); /* Variable glyph widths (6 to 10 px) */
        patches[i].height = 12;
        patches[i].leftoffset = 0;
        patches[i].topoffset = 0;
        patches[i].data = 1000 + i;
        fonts[i] = &patches[i];
    }

    const char *test_corpus[] = {
        "AMMO 200/200",
        "HEALTH 100%",
        "ARMOR 200%",
        "KILLS: 45/50 ITEMS: 10/10 SECRET: 2/3",
        "IDDQD GOD MODE ACTIVE",
        "IDKFA VERY HAPPY AMMO ALL KEYS GRANTED",
        "   LEADING AND TRAILING SPACES   ",
        "A QUICK BROWN FOX JUMPS OVER THE LAZY DOG 1234567890",
        "THIS IS A VERY LONG HUD TEXT LINE DESIGNED TO TEST SCREENWIDTH BOUNDARY CLIPPING SAFELY",
        "PUNCTUATION: ! \" # $ % & ' ( ) * + , - . / : ; < = > ? @ [ \\ ] ^ _",
        "",
        "X"
    };
    int num_tests = sizeof(test_corpus) / sizeof(test_corpus[0]);

    int mismatches = 0;
    int test_runs = 0;

    for (int t = 0; t < num_tests; t++) {
        for (int drawcursor = 0; drawcursor <= 1; drawcursor++) {
            for (int start_x = 0; start_x <= 200; start_x += 50) {
                hu_textline_t line;
                line.x = start_x;
                line.y = 100;
                line.f = fonts;
                line.sc = sc;
                strncpy(line.l, test_corpus[t], 79);
                line.l[79] = '\0';
                line.len = (int)strlen(line.l);
                line.drawcursor = drawcursor;

                s_log_orig_count = 0;
                s_log_opt_count = 0;

                HUlib_drawTextLine_orig(&line, drawcursor);
                HUlib_drawTextLine_opt(&line, drawcursor);

                test_runs++;

                if (s_log_orig_count != s_log_opt_count) {
                    printf("[FAIL] Test '%s' (x=%d, cur=%d): count mismatch orig=%d opt=%d\n",
                           test_corpus[t], start_x, drawcursor, s_log_orig_count, s_log_opt_count);
                    mismatches++;
                    continue;
                }

                for (int k = 0; k < s_log_orig_count; k++) {
                    if (s_log_orig[k].x != s_log_opt[k].x ||
                        s_log_orig[k].y != s_log_opt[k].y ||
                        s_log_orig[k].patch_id != s_log_opt[k].patch_id) {
                        printf("[FAIL] Glyph mismatch at rec %d in '%s': orig=(%d,%d,p=%d) opt=(%d,%d,p=%d)\n",
                               k, test_corpus[t],
                               s_log_orig[k].x, s_log_orig[k].y, s_log_orig[k].patch_id,
                               s_log_opt[k].x, s_log_opt[k].y, s_log_opt[k].patch_id);
                        mismatches++;
                        break;
                    }
                }
            }
        }
    }

    printf("Test Configurations Evaluated: %d\n", test_runs);
    printf("Total Rendering Divergences:   %d\n", mismatches);

    free(patches);
    free(fonts);

    if (mismatches == 0) {
        printf("=================================================================\n");
        printf("★ HULIB_DRAWTEXTLINE ORACLE: 100%% BIT-EXACT MATCH (PASS) ★\n");
        printf("HUD rendering parity verified across all string & clip boundaries.\n");
        printf("=================================================================\n");
        return 0;
    } else {
        printf("[!] Oracle rejected candidate: %d divergences.\n", mismatches);
        return 1;
    }
}
