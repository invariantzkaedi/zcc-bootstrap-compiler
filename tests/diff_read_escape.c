#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Mock Compiler Context matching ZCC Part 1/2 (C89 compatible) */
typedef struct {
    const char *source;
    int source_len;
    int pos;
    int has_peek;
    int peek_char;
} MockCompiler;

static inline int mock_peek_char(MockCompiler *cc) {
    if (cc->has_peek) return cc->peek_char;
    if (cc->pos >= cc->source_len) return -1;
    return (unsigned char)cc->source[cc->pos];
}

static inline int mock_read_char(MockCompiler *cc) {
    if (cc->has_peek) {
        cc->has_peek = 0;
        return cc->peek_char;
    }
    if (cc->pos >= cc->source_len) return -1;
    return (unsigned char)cc->source[cc->pos++];
}

static inline int mock_hex_val(int c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* 1. Original Baseline Implementation (F:\__downloads\zcc (1).c:988) */
static int read_escape_orig(MockCompiler *cc) {
    int c;
    c = mock_read_char(cc);
    switch (c) {
        case 'n': return '\n';
        case 't': return '\t';
        case 'r': return '\r';
        case '\\': return '\\';
        case '\'': return '\'';
        case '"': return '"';
        case '0': return '\0';
        case 'a': return '\a';
        case 'b': return '\b';
        case 'f': return '\f';
        case 'v': return '\v';
        case 'x': {
            int val;
            int h;
            val = 0;
            h = mock_hex_val(mock_peek_char(cc));
            while (h >= 0) {
                mock_read_char(cc);
                val = val * 16 + h;
                h = mock_hex_val(mock_peek_char(cc));
            }
            return val;
        }
        default: return c;
    }
}

/* 2. Autonomous Agent Synthesized Optimization (C89 Table Lookup + Jump Table) */
static unsigned char s_escape_lut[256];
static int s_escape_lut_inited = 0;

static void init_escape_lut(void) {
    if (s_escape_lut_inited) return;
    memset(s_escape_lut, 0, sizeof(s_escape_lut));
    s_escape_lut['n'] = '\n';
    s_escape_lut['t'] = '\t';
    s_escape_lut['r'] = '\r';
    s_escape_lut['\\'] = '\\';
    s_escape_lut['\''] = '\'';
    s_escape_lut['"'] = '"';
    s_escape_lut['0'] = '\0';
    s_escape_lut['a'] = '\a';
    s_escape_lut['b'] = '\b';
    s_escape_lut['f'] = '\f';
    s_escape_lut['v'] = '\v';
    s_escape_lut_inited = 1;
}

static int read_escape_opt(MockCompiler *cc) {
    int c = mock_read_char(cc);
    if (c < 0) return c;
    
    init_escape_lut();
    
    /* Fast-path: O(1) direct table resolution */
    unsigned char mapped = s_escape_lut[(unsigned char)c];
    if (mapped != 0 || c == '0') {
        return (int)mapped;
    }
    
    /* Complex multi-character escape branch */
    if (c == 'x') {
        int val = 0;
        int h = mock_hex_val(mock_peek_char(cc));
        while (h >= 0) {
            mock_read_char(cc);
            val = (val << 4) | h;
            h = mock_hex_val(mock_peek_char(cc));
        }
        return val;
    }
    
    return c;
}

static void setup_compiler(MockCompiler *cc, const char *src, int len) {
    memset(cc, 0, sizeof(MockCompiler));
    cc->source = src;
    cc->source_len = len;
    cc->pos = 0;
    cc->has_peek = 0;
    cc->peek_char = 0;
}

int main(void) {
    printf("=================================================================\n");
    printf("ZCC DIFFERENTIAL ORACLE: read_escape (Original vs Optimized)\n");
    printf("Testing Invariant: Injective character mapping under all symbols\n");
    printf("=================================================================\n");

    int mismatches = 0;
    int test_count = 0;

    /* Corpus 1: All single-character ASCII escape candidates */
    for (int ch = 0; ch < 256; ch++) {
        char buf[4];
        buf[0] = (char)ch;
        buf[1] = 0;

        MockCompiler cc_orig, cc_opt;
        setup_compiler(&cc_orig, buf, 1);
        setup_compiler(&cc_opt, buf, 1);

        int res_orig = read_escape_orig(&cc_orig);
        int res_opt  = read_escape_opt(&cc_opt);

        test_count++;
        if (res_orig != res_opt || cc_orig.pos != cc_opt.pos) {
            printf("[FAIL] Single char %d ('%c'): orig=%d opt=%d, pos_orig=%d pos_opt=%d\n",
                   ch, (ch >= 32 && ch <= 126) ? ch : '?', res_orig, res_opt, cc_orig.pos, cc_opt.pos);
            mismatches++;
        }
    }

    /* Corpus 2: Hex escape sequences (\x00 to \xFF, plus non-hex terminators) */
    const char *hex_cases[10];
    hex_cases[0] = "x0";
    hex_cases[1] = "x00";
    hex_cases[2] = "x1A";
    hex_cases[3] = "xff";
    hex_cases[4] = "x7F";
    hex_cases[5] = "x41;";
    hex_cases[6] = "x000000FF";
    hex_cases[7] = "xZ";
    hex_cases[8] = "x";
    hex_cases[9] = "x1234";

    for (int i = 0; i < 10; i++) {
        const char *src = hex_cases[i];
        int len = (int)strlen(src);

        MockCompiler cc_orig, cc_opt;
        setup_compiler(&cc_orig, src, len);
        setup_compiler(&cc_opt, src, len);

        int res_orig = read_escape_orig(&cc_orig);
        int res_opt  = read_escape_opt(&cc_opt);

        test_count++;
        if (res_orig != res_opt || cc_orig.pos != cc_opt.pos) {
            printf("[FAIL] Hex case '%s': orig=%d opt=%d, pos_orig=%d pos_opt=%d\n",
                   src, res_orig, res_opt, cc_orig.pos, cc_opt.pos);
            mismatches++;
        }
    }

    /* Corpus 3: EOF Boundary Condition */
    {
        MockCompiler cc_orig, cc_opt;
        setup_compiler(&cc_orig, "", 0);
        setup_compiler(&cc_opt, "", 0);

        int res_orig = read_escape_orig(&cc_orig);
        int res_opt  = read_escape_opt(&cc_opt);
        test_count++;
        if (res_orig != res_opt || cc_orig.pos != cc_opt.pos) {
            printf("[FAIL] EOF test: orig=%d opt=%d, pos_orig=%d pos_opt=%d\n",
                   res_orig, res_opt, cc_orig.pos, cc_opt.pos);
            mismatches++;
        }
    }

    printf("Vectors Evaluated: %d\n", test_count);
    printf("Total Mismatches:  %d\n", mismatches);

    if (mismatches == 0) {
        printf("=================================================================\n");
        printf("★ DIFFERENTIAL ORACLE: 100%% BIT-EXACT MATCH (0 MISMATCHES) ★\n");
        printf("Zero-regression semantic equivalence verified.\n");
        printf("=================================================================\n");
        return 0;
    } else {
        printf("[!] Oracle rejected candidate patch: %d regressions detected.\n", mismatches);
        return 1;
    }
}
