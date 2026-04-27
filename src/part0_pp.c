/* part0_pp.c -- ZCC Preprocessor */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#define PP_MAX_MACROS 8192         /* raised: SQLite defines ~4k macros */
#define PP_MAX_PARAMS 128           /* raised: SQLite has 64-arg macros  */
#define PP_MAX_BODY   65536         /* raised: SQLite complex macro bodies */
#define PP_MAX_INCLUDE_DEPTH 64     /* raised: SQLite deep include chains */
#define PP_ARG_INIT_CAP  4096       /* initial per-arg heap buffer        */

typedef struct {
    char name[128];
    int  is_function_like;
    int  num_params;
    char params[PP_MAX_PARAMS][128]; /* wider: long SQLITE param names */
    char body[PP_MAX_BODY];
    int  active;
} PPMacro;

typedef struct {
    const char *src;
    int pos;
    int len;
    char *alloc_buf;
    PPMacro *expanding_macro;
} PPInputCtx;

typedef struct {
    PPMacro macros[PP_MAX_MACROS];
    int     num_macros;

    int     cond_stack[64];
    int     cond_satisfied[64];
    int     cond_else_seen[64];
    int     cond_depth;

    char    included_files[256][256];
    int     num_included;

    char   *out;
    int     out_len;
    int     out_cap;

    const char *src;
    int         pos;
    int         len;
    int         line;
    const char *filename;
    const char *include_paths;

    PPInputCtx input_stack[PP_MAX_INCLUDE_DEPTH];
    int input_depth;
    char *alloc_buf;
} PPState;

static const char *zcc_stddef_text = 
"#ifndef _ZCC_STDDEF_H_\n"
"#define _ZCC_STDDEF_H_\n"
"#define NULL ((void*)0)\n"
"#define EOF (-1)\n"
"#define EXIT_SUCCESS 0\n"
"#define EXIT_FAILURE 1\n"
"#define CHAR_BIT 8\n"
"#define INT_MAX 2147483647\n"
"#define INT_MIN (-2147483647-1)\n"
"#define LONG_MAX 9223372036854775807L\n"
"#define LONG_MIN (-9223372036854775807L-1L)\n"
"#define UINT_MAX 4294967295U\n"
"#define ULONG_MAX 18446744073709551615UL\n"
"#define SIZE_MAX ULONG_MAX\n"
"#define RAND_MAX 2147483647\n"
"#define BUFSIZ 8192\n"
"#define FILENAME_MAX 4096\n"
"#define true 1\n"
"#define false 0\n"
"#define __extension__\n"
"#define __restrict\n"
"#define __restrict__\n"
"#define restrict\n"
"#define __inline\n"
"#define __inline__\n"
"#define inline\n"
"#define __volatile\n"
"#define __volatile__\n"
"#define __const__ const\n"
"#define __const const\n"
"#define _Noreturn\n"
"#define _Static_assert(x, y)\n"
"#define __declspec(x)\n"
"#define __cdecl\n"
"#define __stdcall\n"
"#define __asm__(a,b,c,d,e)\n"
"#define asm(a,b,c,d,e)\n"
"typedef struct { unsigned int gp_offset; unsigned int fp_offset; void *overflow_arg_area; void *reg_save_area; } __va_list_struct[1];\n"
"#define __builtin_va_list __va_list_struct\n"
"#define __builtin_va_end(v)\n"
"#define __builtin_expect(exp, c) (exp)\n"
"#define __builtin_constant_p(x) 0\n"
"#define __builtin_types_compatible_p(x, y) 0\n"
"#define __builtin_unreachable()\n"
"#define __x86_64__ 1\n"
"#define __GNUC__ 1\n"
"#define assert(x)\n"
"#define offsetof(t, m) ((unsigned long)&(((t*)0)->m))\n"
"typedef int int32_t;\n"
"typedef unsigned int uint32_t;\n"
"typedef long int64_t;\n"
"typedef unsigned long uint64_t;\n"
"typedef char int8_t;\n"
"typedef unsigned char uint8_t;\n"
"typedef short int16_t;\n"
"typedef unsigned short uint16_t;\n"
"typedef unsigned long size_t;\n"
"typedef long ssize_t;\n"
"typedef struct _IO_FILE FILE;\n"
"extern FILE *stdin, *stdout, *stderr;\n"
"#endif\n";

static void pp_emit(PPState *state, char c) {
    if (state->out_len + 1 >= state->out_cap) {
        state->out_cap = (state->out_cap == 0) ? 4096 : state->out_cap * 2;
        state->out = (char *)realloc(state->out, state->out_cap);
    }
    state->out[state->out_len++] = c;
}

static void pp_emit_str(PPState *state, const char *str, int len) {
    int i;
    for (i = 0; i < len; i++) pp_emit(state, str[i]);
}

static char pp_peek(PPState *state) {
    static int pp_peek_cnt = 0;
    if (++pp_peek_cnt % 500000 == 0) {
        printf("DEBUG zcc2 pp_peek_cnt=%d pos=%d line=%d\n", pp_peek_cnt, state->pos, state->line);
        fflush(stdout);
    }
    while (state->pos >= state->len && state->input_depth > 0) {
        if (state->alloc_buf) free(state->alloc_buf);
        state->input_depth--;
        if (state->input_stack[state->input_depth].expanding_macro) {
            state->input_stack[state->input_depth].expanding_macro->active = 1;
        }
        state->src = state->input_stack[state->input_depth].src;
        state->pos = state->input_stack[state->input_depth].pos;
        state->len = state->input_stack[state->input_depth].len;
        state->alloc_buf = state->input_stack[state->input_depth].alloc_buf;
    }
    if (state->pos >= state->len) return 0;
    return state->src[state->pos];
}

static char pp_next(PPState *state) {
    char c = pp_peek(state);
    if (c) {
        if (c == '\n' && state->input_depth == 0) state->line++;
        state->pos++;
    }
    return c;
}

static void pp_push_input(PPState *state, const char *new_src, char *alloc_buf, PPMacro *macro) {
    if (state->input_depth >= PP_MAX_INCLUDE_DEPTH) {
        fprintf(stderr, "zcc preprocessor error: macro expansion too deep (depth=%d)\n", state->input_depth);
        exit(1);
    }
    state->input_stack[state->input_depth].src = state->src;
    state->input_stack[state->input_depth].pos = state->pos;
    state->input_stack[state->input_depth].len = state->len;
    state->input_stack[state->input_depth].alloc_buf = state->alloc_buf;
    state->input_stack[state->input_depth].expanding_macro = macro;
    state->input_depth++;
    
    state->src = new_src;
    state->pos = 0;
    state->len = strlen(new_src);
    state->alloc_buf = alloc_buf;
}

static void pp_skip_whitespace(PPState *state) {
    while (pp_peek(state) == ' ' || pp_peek(state) == '\t') pp_next(state);
}

static int is_ident_start(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static int is_ident_char(char c) {
    return is_ident_start(c) || (c >= '0' && c <= '9');
}

static void pp_parse_ident(PPState *state, char *out, int max) {
    int i = 0;
    while (is_ident_char(pp_peek(state)) && i < max - 1) out[i++] = pp_next(state);
    out[i] = 0;
}

static PPMacro *pp_find_macro(PPState *state, const char *name) {
    int i;
    for (i = state->num_macros - 1; i >= 0; i--) {
        if (state->macros[i].active && strcmp(state->macros[i].name, name) == 0) return &state->macros[i];
    }
    return 0;
}

static void pp_undef_macro(PPState *state, const char *name) {
    PPMacro *m = pp_find_macro(state, name);
    if (m) m->active = 0;
}

static PPMacro *pp_add_macro(PPState *state, const char *name) {
    PPMacro *m_macro;
    if (state->num_macros >= PP_MAX_MACROS) return 0;
    m_macro = &state->macros[state->num_macros++];
    memset(m_macro, 0, sizeof(PPMacro));
    strncpy(m_macro->name, name, 127);
    m_macro->active = 1;
    return m_macro;
}

static void pp_read_line(PPState *state, char *buf, int max) {
    int i = 0;
    int in_comment = 0;
    while (pp_peek(state) != 0 && i < max - 1) {
        char c = pp_peek(state);
        if (c == '\n' && !in_comment) {
            break;
        }
        if (c == '\\' && state->src[state->pos + 1] == '\n') {
            pp_next(state); pp_next(state);
            continue;
        }
        if (!in_comment && c == '/' && state->src[state->pos + 1] == '/') {
            while (pp_peek(state) != '\n' && pp_peek(state) != 0) pp_next(state);
            break;
        }
        if (!in_comment && c == '/' && state->src[state->pos + 1] == '*') {
            in_comment = 1; pp_next(state); pp_next(state);
            continue;
        }
        if (in_comment && c == '*' && state->src[state->pos + 1] == '/') {
            in_comment = 0; pp_next(state); pp_next(state);
            continue;
        }
        pp_next(state);
        if (!in_comment && c != '\n' && c != '\r') buf[i++] = c;
    }
    buf[i] = 0;
}

static int pp_is_active(PPState *state) {
    int i;
    for (i = 0; i <= state->cond_depth; i++) {
        if (!state->cond_stack[i]) return 0;
    }
    return 1;
}

static char *pp_read_file(const char *path, int *out_len) {
    FILE *fp = fopen(path, "rb");
    long len;
    char *buf;
    long nr;
    
    if (!fp) return 0;
    fseek(fp, 0, 2);
    len = ftell(fp);
    fseek(fp, 0, 0);
    
    buf = (char *)malloc(len + 1);
    if (!buf) { fclose(fp); return 0; }
    
    nr = fread(buf, 1, len, fp);
    buf[nr] = 0;
    fclose(fp);
    
    if (out_len) *out_len = nr;
    return buf;
}

static void pp_parse_target_depth(PPState *state, int target_depth);

static void pp_parse(PPState *state) {
    pp_parse_target_depth(state, state->input_depth);
}

static void pp_process_include(PPState *state, const char *path, int is_system) {
    char *file_src = 0;
    int file_len = 0;
    int i;
    
    /* Avoid system includes entirely and just inject stddef */
    if (is_system || strcmp(path, "stdio.h") == 0 || strcmp(path, "stdlib.h") == 0 || 
        strcmp(path, "string.h") == 0 || strcmp(path, "stddef.h") == 0) {
        file_src = (char *)zcc_stddef_text;
        file_len = strlen(file_src);
    } else {
        /* check circle include logic (very simple, limit depth) */
        void *p = pp_read_file(path, &file_len);
        if (p) file_src = (char *)p;
    }
    
    if (file_src) {
        PPState *sub_state = (PPState *)malloc(sizeof(PPState));
        if (!sub_state) {
            fprintf(stderr, "zcc preprocessor error: out of memory for include state\\n");
            exit(1);
        }
        memset(sub_state, 0, sizeof(PPState));
        sub_state->src = file_src;
        sub_state->len = file_len;
        sub_state->cond_stack[0] = 1;
        
        /* copy macro table into sub_state */
        sub_state->num_macros = state->num_macros;
        for (i = 0; i < state->num_macros; i++) memcpy(&sub_state->macros[i], &state->macros[i], sizeof(PPMacro));
        
        pp_parse(sub_state);
        
        /* we need to copy back the macros added by the header */
        for (i = state->num_macros; i < sub_state->num_macros; i++) {
            if (state->num_macros >= PP_MAX_MACROS) {
                fprintf(stderr, "MAX MACROS REACHED!\n");
                break;
            }
            memcpy(&state->macros[state->num_macros++], &sub_state->macros[i], sizeof(PPMacro));
        }
        
        fprintf(stderr, "pp_process_include: %s added %d macros, total is %d\n", path, sub_state->num_macros - i, state->num_macros);
        
        /* update all previous active flags because undefs may have occurred */
        for (i = 0; i < state->num_macros; i++) {
            state->macros[i].active = sub_state->macros[i].active;
        }
        
        {
            /* PP-LINEMARKER: emit standard # N "file" markers at include boundaries.
             * The "entering" marker tells the lexer to reset its line counter to 1
             * and switch filename to the header. The "returning" marker restores the
             * parent filename at the line AFTER the #include directive.
             *
             * state->line is still on the #include line here (the caller increments
             * past the trailing '\n' after pp_process_include returns), so
             * state->line + 1 is the correct post-include resume line.
             *
             * Filename buffer lifetime: single static per process is safe for
             * single-level includes. Known limitation: nested includes overwrite
             * this buffer. Fix B (arena_strdup in the lexer) deferred — see
             * commit message PP-LINEMARKER-001.
             */
            char linemarker[512];
            const char *hdr_name = (file_src == zcc_stddef_text)
                                 ? "<built-in>" : path;
            const char *par_name = state->filename ? state->filename : "<unknown>";
            /* entering header: reset to line 1 */
            snprintf(linemarker, sizeof(linemarker),
                     "# 1 \"%s\"\n", hdr_name);
            pp_emit_str(state, linemarker, (int)strlen(linemarker));
            /* header body */
            pp_emit_str(state, sub_state->out, sub_state->out_len);
            /* returning to parent: resume at line after #include */
            snprintf(linemarker, sizeof(linemarker),
                     "# %d \"%s\"\n", state->line + 1, par_name);
            pp_emit_str(state, linemarker, (int)strlen(linemarker));
        }
        
        if (sub_state->out) free(sub_state->out);
        free(sub_state);
        if (file_src != zcc_stddef_text) free(file_src);
    } else {
        fprintf(stderr, "zcc compiler error: include file not found: %s\n", path);
    }
}

/* parse parameter list for function-like macros */
static void pp_parse_params(PPState *state, PPMacro *m) {
    pp_skip_whitespace(state);
    while (pp_peek(state) != ')' && pp_peek(state) != '\n' && pp_peek(state) != 0) {
        if (is_ident_start(pp_peek(state))) {
            if (m->num_params < PP_MAX_PARAMS) {
                pp_parse_ident(state, m->params[m->num_params++], 128);
            } else {
                char dummy[128];
                pp_parse_ident(state, dummy, 128);
            }
        } else {
            pp_next(state); /* comma or whitespace */
        }
        pp_skip_whitespace(state);
    }
    if (pp_peek(state) == ')') pp_next(state);
}

static int pp_eval_expr_str(PPState *state, const char *s, int depth) {
    char buf[1024];
    int i=0, j=0;
    char *or_ptr, *and_ptr;
    
    fprintf(stderr, "eval depth=%d s='%s'\n", depth, s);
    if (depth > 16) {
        fprintf(stderr, "RECURSION DEPTH MAXED OUT!\n");
        return 0;
    }
    
    while(s[i] && j<1023) { if (s[i]!=' ' && s[i]!='\t' && s[i]!='\n' && s[i]!='\r') buf[j++]=s[i]; i++; }
    buf[j]=0;
    if (buf[0] == 0) return 0;
    
    or_ptr = strstr(buf, "||");
    if (or_ptr) {
        *or_ptr = 0;
        return pp_eval_expr_str(state, buf, depth+1) || pp_eval_expr_str(state, or_ptr + 2, depth+1);
    }
    and_ptr = strstr(buf, "&&");
    if (and_ptr) {
        *and_ptr = 0;
        return pp_eval_expr_str(state, buf, depth+1) && pp_eval_expr_str(state, and_ptr + 2, depth+1);
    }
    
    char *op;
    if ((op = strstr(buf, "=="))) {
        *op = 0; return pp_eval_expr_str(state, buf, depth+1) == pp_eval_expr_str(state, op+2, depth+1);
    }
    if ((op = strstr(buf, "!="))) {
        *op = 0; return pp_eval_expr_str(state, buf, depth+1) != pp_eval_expr_str(state, op+2, depth+1);
    }
    if ((op = strstr(buf, "<="))) {
        *op = 0; return pp_eval_expr_str(state, buf, depth+1) <= pp_eval_expr_str(state, op+2, depth+1);
    }
    if ((op = strstr(buf, ">="))) {
        *op = 0; return pp_eval_expr_str(state, buf, depth+1) >= pp_eval_expr_str(state, op+2, depth+1);
    }
    if ((op = strchr(buf, '<'))) {
        *op = 0; return pp_eval_expr_str(state, buf, depth+1) < pp_eval_expr_str(state, op+1, depth+1);
    }
    if ((op = strchr(buf, '>'))) {
        *op = 0; return pp_eval_expr_str(state, buf, depth+1) > pp_eval_expr_str(state, op+1, depth+1);
    }
    
    if (buf[0] == '!') return !pp_eval_expr_str(state, buf+1, depth+1);
    
    /* parenthesis stripping */
    if (buf[0] == '(' && buf[j-1] == ')') {
        buf[j-1] = 0;
        return pp_eval_expr_str(state, buf+1, depth+1);
    }
    
    if (buf[0] >= '0' && buf[0] <= '9') return strtol(buf, NULL, 0);
    
    if (strncmp(buf, "defined", 7) == 0) {
        char *p = buf + 7;
        char name[128];
        int n_idx = 0;
        while (*p == '(') p++;
        while (*p && *p != ')' && n_idx < 127) name[n_idx++] = *p++;
        name[n_idx] = 0;
        return pp_find_macro(state, name) != 0;
    }
    
    PPMacro *m = pp_find_macro(state, buf);
    if (m) return pp_eval_expr_str(state, m->body, depth+1);
    return 0;
}

static void pp_parse_directive(PPState *state) {
    char dir[1024];
    int active = pp_is_active(state);
    
    pp_skip_whitespace(state);
    pp_parse_ident(state, dir, 1024);
    pp_skip_whitespace(state);

    if (strcmp(dir, "ifdef") == 0 || strcmp(dir, "ifndef") == 0) {
        char name[128];
        int is_defined;
        pp_parse_ident(state, name, 128);
        is_defined = (pp_find_macro(state, name) != 0);
        
        state->cond_depth++;
        state->cond_else_seen[state->cond_depth] = 0;
        if (!active) {
            state->cond_stack[state->cond_depth] = 0;
        } else {
            if (strcmp(dir, "ifdef") == 0) state->cond_stack[state->cond_depth] = is_defined;
            else state->cond_stack[state->cond_depth] = !is_defined;
        }
        state->cond_satisfied[state->cond_depth] = state->cond_stack[state->cond_depth];
        pp_read_line(state, dir, 1024);
    } else if (strcmp(dir, "if") == 0) {
        char expr[256];
        pp_skip_whitespace(state);
        pp_read_line(state, expr, 256);
        state->cond_depth++;
        state->cond_else_seen[state->cond_depth] = 0;
        if (!active) {
            state->cond_stack[state->cond_depth] = 0;
        } else {
            state->cond_stack[state->cond_depth] = pp_eval_expr_str(state, expr, 0); 
        }
        state->cond_satisfied[state->cond_depth] = state->cond_stack[state->cond_depth];
    } else if (strcmp(dir, "elif") == 0) {
        if (state->cond_depth > 0 && !state->cond_else_seen[state->cond_depth]) {
            char expr[256];
            pp_read_line(state, expr, 256);
            if (state->cond_satisfied[state->cond_depth]) {
                state->cond_stack[state->cond_depth] = 0;
            } else {
                int active_parent = 1;
                int i;
                for (i = 0; i < state->cond_depth; i++) if (!state->cond_stack[i]) active_parent = 0;
                if (active_parent) {
                    int result = pp_eval_expr_str(state, expr, 0);
                    state->cond_stack[state->cond_depth] = result;
                    if (result) state->cond_satisfied[state->cond_depth] = 1;
                } else {
                    state->cond_stack[state->cond_depth] = 0;
                }
            }
        } else {
            pp_read_line(state, dir, 1024);
        }
    } else if (strcmp(dir, "else") == 0) {
        if (state->cond_depth > 0) {
            state->cond_else_seen[state->cond_depth] = 1;
            int parent_active = 1;
            int i;
            for (i = 0; i < state->cond_depth; i++) if (!state->cond_stack[i]) parent_active = 0;
            if (parent_active) {
                if (state->cond_satisfied[state->cond_depth]) {
                    state->cond_stack[state->cond_depth] = 0;
                } else {
                    state->cond_stack[state->cond_depth] = 1;
                    state->cond_satisfied[state->cond_depth] = 1;
                }
            } else {
                state->cond_stack[state->cond_depth] = 0;
            }
        }
        pp_read_line(state, dir, 1024);
    } else if (strcmp(dir, "endif") == 0) {
        if (state->cond_depth > 0) {
            state->cond_satisfied[state->cond_depth] = 0;
            state->cond_else_seen[state->cond_depth] = 0;
            state->cond_depth--;
        }

        pp_read_line(state, dir, 1024);
    } else if (active) {
        if (strcmp(dir, "define") == 0) {
            char name[128];
            PPMacro *m;
            pp_parse_ident(state, name, 128);
            m = pp_add_macro(state, name);
            if (pp_peek(state) == '(') {
                pp_next(state);
                m->is_function_like = 1;
                pp_parse_params(state, m);
                pp_skip_whitespace(state);
                pp_read_line(state, m->body, PP_MAX_BODY);
            } else {
                pp_skip_whitespace(state);
                pp_read_line(state, m->body, PP_MAX_BODY);
            }
        } else if (strcmp(dir, "undef") == 0) {
            char name[128];
            pp_parse_ident(state, name, 128);
            pp_undef_macro(state, name);
            pp_read_line(state, dir, 1024);
        } else if (strcmp(dir, "include") == 0) {
            char inc_path[256];
            int is_system = 0;
            if (pp_peek(state) == '<') {
                is_system = 1;
                pp_next(state);
            } else if (pp_peek(state) == '"') {
                pp_next(state);
            }
            pp_read_line(state, inc_path, 256);
            if (strlen(inc_path) > 0 && (inc_path[strlen(inc_path) - 1] == '"' || inc_path[strlen(inc_path) - 1] == '>')) {
                inc_path[strlen(inc_path) - 1] = 0;
            }
            pp_process_include(state, inc_path, is_system);
        } else if (strcmp(dir, "error") == 0) {
            char msg[256];
            pp_read_line(state, msg, 256);
            fprintf(stderr, "zcc preprocessor error: #error %s\n", msg);
        } else {
            pp_read_line(state, dir, 1024);
        }
    } else {
        pp_read_line(state, dir, 1024);
    }
    
    if (pp_peek(state) == '\n') pp_next(state);
    pp_emit(state, '\n');
}


static void pp_expand_ident(PPState *state, const char *ident) {
    PPMacro *m;
    int i, p_count;
    char c;
    
    if (strcmp(ident, "defined") == 0) {
        pp_emit_str(state, "defined", 7);
        return;
    }
    
    m = pp_find_macro(state, ident);
    if (!m) {
        pp_emit_str(state, ident, strlen(ident));
        return;
    }
    
    if (!m->is_function_like) {
        m->active = 0;
        pp_push_input(state, m->body, NULL, m);
        return;
    }
    /* Function-like macro expansion */
    
    int space_cap = 1024;
    char *space_buf = (char *)malloc(space_cap);
    int space_len = 0;
    
    while (pp_peek(state) == ' ' || pp_peek(state) == '\t' || pp_peek(state) == '\n' || pp_peek(state) == '\r') {
        if (space_len + 1 >= space_cap) {
            space_cap *= 2;
            space_buf = (char *)realloc(space_buf, space_cap);
        }
        space_buf[space_len++] = pp_next(state);
    }
    space_buf[space_len] = 0;

    if (pp_peek(state) != '(') {
        pp_emit_str(state, ident, strlen(ident));
        pp_emit_str(state, space_buf, space_len);
        free(space_buf);
        return;
    }
    free(space_buf);
    pp_next(state); /* consume '(' */
    
    /* ─── DYNAMIC ARGUMENT BUFFERS ───────────────────────────────────────────
     * UPGRADE: args[] and expanded_args[] are now heap-allocated per-slot
     * with dynamic realloc growth. This eliminates the critical silent
     * truncation bug: when SQLite passes 65KB+ arguments, the old 8192-byte
     * cap would silently drop the closing ')' token, triggering infinite
     * EOF recursion and a stack overflow.
     * ────────────────────────────────────────────────────────────────────── */
    char **args = (char **)calloc(PP_MAX_PARAMS, sizeof(char *));
    char **expanded_args = (char **)calloc(PP_MAX_PARAMS, sizeof(char *));
    int  *args_cap = (int *)calloc(PP_MAX_PARAMS, sizeof(int));
    int  *expanded_cap = (int *)calloc(PP_MAX_PARAMS, sizeof(int));

    for (i = 0; i < PP_MAX_PARAMS; i++) {
        args[i] = (char *)malloc(PP_ARG_INIT_CAP);
        args[i][0] = 0;
        args_cap[i] = PP_ARG_INIT_CAP;
        expanded_args[i] = (char *)malloc(PP_ARG_INIT_CAP);
        expanded_args[i][0] = 0;
        expanded_cap[i] = PP_ARG_INIT_CAP;
    }

    p_count = 0;
    int paren_level = 0;
    int arg_idx = 0;
    int in_string = 0;
    int in_char = 0;
    while (pp_peek(state) != 0) {
        c = pp_peek(state);

        if (c == '\\') {
            char esc;
            pp_next(state);
            if (pp_peek(state) == '\n') { pp_next(state); continue; }
            esc = pp_next(state);
            if (p_count < PP_MAX_PARAMS) {
                /* grow buffer if we need room for 2 chars + null */
                if (arg_idx + 3 >= args_cap[p_count]) {
                    args_cap[p_count] *= 2;
                    args[p_count] = (char *)realloc(args[p_count], args_cap[p_count]);
                }
                args[p_count][arg_idx++] = c;
                args[p_count][arg_idx++] = esc;
                args[p_count][arg_idx]   = 0;
            }
            continue;
        }

        if (c == '"' && !in_char) in_string = !in_string;
        if (c == '\'' && !in_string) in_char = !in_char;

        if (!in_string && !in_char) {
            if (c == '(') paren_level++;
            else if (c == ')') {
                if (paren_level == 0) {
                    pp_next(state); /* consume ')' */
                    if (arg_idx > 0 || m->num_params > 0) p_count++;
                    break;
                }
                paren_level--;
            } else if (c == ',' && paren_level == 0) {
                /* terminate current arg, advance slot */
                if (p_count < PP_MAX_PARAMS) args[p_count][arg_idx] = 0;
                p_count++;
                arg_idx = 0;
                pp_next(state);
                continue;
            }
        }

        if (p_count < PP_MAX_PARAMS) {
            /* dynamically grow the arg buffer on demand */
            if (arg_idx + 2 >= args_cap[p_count]) {
                args_cap[p_count] *= 2;
                args[p_count] = (char *)realloc(args[p_count], args_cap[p_count]);
            }
            args[p_count][arg_idx++] = c;
            args[p_count][arg_idx]   = 0;
        }
        pp_next(state);
    }

    /* ─── PRE-EXPAND ARGUMENTS ───────────────────────────────────────────── */
    /* Apply hide-set BEFORE arg expansion (C99 6.10.3.4 — the macro being
     * expanded is "painted blue" for its entire expansion, including argument
     * pre-expansion). This prevents patterns like:
     *   vfsList  →  GLOBAL(sqlite3_vfs *, vfsList)
     *   arg[1]   =  "vfsList"  →  re-expands vfsList  →  infinite loop */
    m->active = 0;
    for (i = 0; i < p_count; i++) {
        char *old_out     = state->out;
        int   old_out_len = state->out_len;
        int   old_out_cap = state->out_cap;

        state->out     = expanded_args[i];
        state->out_len = 0;
        state->out_cap = expanded_cap[i] - 1;

        pp_push_input(state, args[i], NULL, NULL);
        pp_parse_target_depth(state, state->input_depth);

        expanded_args[i]  = state->out;
        expanded_cap[i]   = state->out_cap + 1;
        expanded_args[i][state->out_len] = 0;

        state->out     = old_out;
        state->out_len = old_out_len;
        state->out_cap = old_out_cap;
    }

    /* ─── BODY SUBSTITUTION ─────────────────────────────────────────────── */
    int len = strlen(m->body);
    int subst_cap = len + (p_count * 256) + 4096;
    char *subst = (char *)malloc(subst_cap);
    int subst_idx = 0;

    for (i = 0; i < len; i++) {
        /* grow substitution buffer before we write */
        if (subst_idx + 2048 > subst_cap) {
            subst_cap *= 2;
            subst = (char *)realloc(subst, subst_cap);
        }

        if (m->body[i] == '#' && i + 1 < len && m->body[i+1] == '#') {
            i++; /* skip token-paste ## */
            while (subst_idx > 0 && (subst[subst_idx-1] == ' ' || subst[subst_idx-1] == '\t'))
                subst_idx--;
            while (i + 1 < len && (m->body[i+1] == ' ' || m->body[i+1] == '\t'))
                i++;
            continue;
        }
        if (is_ident_start(m->body[i])) {
            char param_name[128];
            int p_idx = 0;
            while (i < len && is_ident_char(m->body[i]) && p_idx < 127)
                param_name[p_idx++] = m->body[i++];
            param_name[p_idx] = 0;
            i--; /* backup one — outer loop will i++ */

            int found = -1;
            int j;
            for (j = 0; j < m->num_params; j++) {
                if (strcmp(m->params[j], param_name) == 0) { found = j; break; }
            }

            if (found >= 0 && found < p_count) {
                int e_len = strlen(expanded_args[found]);
                if (subst_idx + e_len + 2048 > subst_cap) {
                    subst_cap = subst_idx + e_len + 8192;
                    subst = (char *)realloc(subst, subst_cap);
                }
                memcpy(subst + subst_idx, expanded_args[found], e_len);
                subst_idx += e_len;
            } else {
                int n_len = strlen(param_name);
                if (subst_idx + n_len + 1024 > subst_cap) {
                    subst_cap = subst_idx + n_len + 4096;
                    subst = (char *)realloc(subst, subst_cap);
                }
                memcpy(subst + subst_idx, param_name, n_len);
                subst_idx += n_len;
            }
        } else {
            subst[subst_idx++] = m->body[i];
        }
    }
    subst[subst_idx] = 0;

    for (i = 0; i < PP_MAX_PARAMS; i++) {
        free(args[i]);
        free(expanded_args[i]);
    }
    free(args);
    free(expanded_args);
    free(args_cap);
    free(expanded_cap);
    
    /* m->active is already 0 (set before arg expansion above). */
    pp_push_input(state, subst, subst, m);
}

static void pp_parse_target_depth(PPState *state, int target_depth) {
    int in_string = 0;
    while (1) {
        char c = pp_peek(state);
        if (state->input_depth < target_depth || c == 0) break;
        
        if (c == '"' || c == '\'') {
            in_string = c;
            if (pp_is_active(state)) pp_emit(state, pp_next(state)); else pp_next(state);
            while (pp_peek(state) != 0 && pp_peek(state) != in_string) {
                if (pp_peek(state) == '\\') {
                    if (pp_is_active(state)) pp_emit(state, pp_next(state)); else pp_next(state);
                    if (pp_peek(state) != 0) {
                        if (pp_is_active(state)) pp_emit(state, pp_next(state)); else pp_next(state);
                    }
                } else {
                    if (pp_is_active(state)) pp_emit(state, pp_next(state)); else pp_next(state);
                }
            }
            if (pp_peek(state) != 0) {
                if (pp_is_active(state)) pp_emit(state, pp_next(state)); else pp_next(state);
            }
            in_string = 0;
            continue;
        }
        if (c == '/' && state->pos + 1 < state->len) {
            char next_c = state->src[state->pos + 1];
            if (next_c == '/') {
                if (pp_is_active(state)) { pp_emit(state, pp_next(state)); pp_emit(state, pp_next(state)); }
                else { pp_next(state); pp_next(state); }
                while (pp_peek(state) != 0 && pp_peek(state) != '\n') {
                    if (pp_is_active(state)) pp_emit(state, pp_next(state)); else pp_next(state);
                }
                continue;
            } else if (next_c == '*') {
                if (pp_is_active(state)) { pp_emit(state, pp_next(state)); pp_emit(state, pp_next(state)); }
                else { pp_next(state); pp_next(state); }
                while (pp_peek(state) != 0) {
                    if (pp_peek(state) == '*' && state->pos + 1 < state->len && state->src[state->pos + 1] == '/') {
                        if (pp_is_active(state)) { pp_emit(state, pp_next(state)); pp_emit(state, pp_next(state)); }
                        else { pp_next(state); pp_next(state); }
                        break;
                    }
                    if (pp_is_active(state)) pp_emit(state, pp_next(state)); else pp_next(state);
                }
                continue;
            }
        }
        
        if (is_ident_start(c)) {
            char ident[128];
            pp_parse_ident(state, ident, 128);
            if (pp_is_active(state)) pp_expand_ident(state, ident);
            continue;
        }

        if (c == '#') {
            pp_next(state);
            pp_parse_directive(state);
            continue;
        }

        if (pp_is_active(state)) pp_emit(state, pp_next(state));
        else {
            if (c == '\n') pp_emit(state, '\n');
            pp_next(state);
        }
    }
}

char *zcc_preprocess(const char *source, int source_len,
                     const char *filename,
                     const char *include_paths,
                     int *out_len) {
    PPState *state = (PPState *)malloc(sizeof(PPState));
    char *result;
    memset(state, 0, sizeof(PPState));
    
    state->src = source;
    state->len = source_len;
    state->filename = filename;
    state->include_paths = include_paths;
    state->line = 1;
    
    state->cond_stack[0] = 1;
    
    {
        PPMacro *m = pp_add_macro(state, "__x86_64__");
        strcpy(m->body, "1");
        m = pp_add_macro(state, "__GNUC__");
        strcpy(m->body, "1");
        m = pp_add_macro(state, "__thread");
        strcpy(m->body, "");
    }

    pp_parse_target_depth(state, 0);
    
    pp_emit(state, 0);
    if (out_len) *out_len = state->out_len - 1;
    result = state->out;
    free(state);
    return result;
}

/* ================================================================ */
