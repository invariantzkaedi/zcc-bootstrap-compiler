/*
 * ZCC Win64 CLI & File Driver for Upstream MicroPython v1.24.1 (`src/win64_mpy_driver.c`)
 * =========================================================================================
 * Hosts unmodified upstream MicroPython v1.24.1 (`py/` + `extmod/`) atop ZCC's
 * freestanding Win64 MSVCRT CRT (`src/win64_app_crt.c`), IEEE-754 C99 runtime
 * (`src/win64_c99_rt.c`), and Microsoft x64 ABI `setjmp`/`longjmp` (`src/win64_setjmp.s`)
 * powering `MICROPY_NLR_SETJMP` and callee-saved register spilling in `gc_collect()`.
 *
 * Supports:
 *   micropython.exe -c "<python code>" [args...]
 *   micropython.exe <script.py> [args...]
 *   micropython.exe - [args...]       (reads Python source from stdin)
 *   micropython.exe                   (reads Python source from stdin)
 *   micropython.exe -v / --version
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <setjmp.h>

#include "py/compile.h"
#include "py/runtime.h"
#include "py/builtin.h"
#include "py/gc.h"
#include "py/stackctrl.h"
#include "py/mphal.h"
#include "py/mperrno.h"
#include "py/objexcept.h"
#include "py/objstr.h"
#include "py/stream.h"

#define ZCC_MPY_HEAP_BYTES (8U * 1024U * 1024U)
#define ZCC_MPY_PYSTACK_LEN 8192U

static char *mpy_heap = NULL;
static mp_obj_t mpy_pystack[ZCC_MPY_PYSTACK_LEN];
static char *mpy_c_stack_top = NULL;

void mp_hal_stdout_tx_strn(const char *str, size_t len) {
    if (str && len > 0U) {
        fwrite(str, 1U, len, stdout);
    }
}

static void mpy_stderr_print_strn(void *env, const char *str, size_t len) {
    (void)env;
    if (str && len > 0U) {
        fwrite(str, 1U, len, stderr);
    }
}

static const mp_print_t mpy_stderr_print = { NULL, mpy_stderr_print_strn };

int readline(vstr_t *line, const char *prompt) {
    if (prompt && prompt[0]) {
        fwrite(prompt, 1U, strlen(prompt), stdout);
        fflush(stdout);
    }
    int c;
    while ((c = fgetc(stdin)) != EOF) {
        if (c == '\n') break;
        if (c == '\r') continue;
        vstr_add_byte(line, (byte)c);
    }
    return (c == EOF && line->len == 0U) ? 4 : '\r';
}

void nlr_jump_fail(void *val) {
    (void)val;
    fprintf(stderr, "FATAL: uncaught NLR\n");
    fflush(stderr);
    exit(1);
}

void gc_collect(void) {
    /* Spill Microsoft x64 ABI callee-saved GPRs onto stack before scanning roots */
    jmp_buf regs;
    setjmp(regs);
    uintptr_t low = (uintptr_t)&regs;
    uintptr_t high = (uintptr_t)mpy_c_stack_top;
    gc_collect_start();
    if (mpy_c_stack_top && high > low) {
        gc_collect_root((void **)&regs, (high - low) / sizeof(mp_uint_t));
    }
    gc_collect_end();
}

/* -------------------------------------------------------------------------
 * Win64 File Object & Import Backing (`mp_import_stat`, `open`, imports)
 * ------------------------------------------------------------------------- */
typedef struct _mp_obj_win64_file_t {
    mp_obj_base_t base;
    FILE *fp;
    bool closed;
} mp_obj_win64_file_t;

static void win64_file_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) {
    (void)kind;
    mp_obj_win64_file_t *self = MP_OBJ_TO_PTR(self_in);
    mp_printf(print, "<Win64File closed=%s>", self->closed ? "True" : "False");
}

static mp_uint_t win64_file_read(mp_obj_t self_in, void *buf, mp_uint_t size, int *errcode) {
    mp_obj_win64_file_t *self = MP_OBJ_TO_PTR(self_in);
    if (self->closed || !self->fp) {
        *errcode = MP_EBADF;
        return MP_STREAM_ERROR;
    }
    if (size == 0U) return 0U;
    size_t n = fread(buf, 1U, (size_t)size, self->fp);
    return (mp_uint_t)n;
}

static mp_uint_t win64_file_write(mp_obj_t self_in, const void *buf, mp_uint_t size, int *errcode) {
    mp_obj_win64_file_t *self = MP_OBJ_TO_PTR(self_in);
    if (self->closed || !self->fp) {
        *errcode = MP_EBADF;
        return MP_STREAM_ERROR;
    }
    if (size == 0U) return 0U;
    size_t n = fwrite(buf, 1U, (size_t)size, self->fp);
    if (n == 0U && ferror(self->fp)) {
        *errcode = MP_EIO;
        return MP_STREAM_ERROR;
    }
    return (mp_uint_t)n;
}

static mp_uint_t win64_file_ioctl(mp_obj_t self_in, mp_uint_t request, uintptr_t arg, int *errcode) {
    mp_obj_win64_file_t *self = MP_OBJ_TO_PTR(self_in);
    if (request == MP_STREAM_CLOSE) {
        if (!self->closed && self->fp) {
            fclose(self->fp);
            self->fp = NULL;
            self->closed = true;
        }
        return 0U;
    }
    if (self->closed || !self->fp) {
        *errcode = MP_EBADF;
        return MP_STREAM_ERROR;
    }
    if (request == MP_STREAM_FLUSH) {
        fflush(self->fp);
        return 0U;
    }
    if (request == MP_STREAM_SEEK) {
        struct mp_stream_seek_t *s = (struct mp_stream_seek_t *)arg;
        int whence = SEEK_SET;
        if (s->whence == MP_SEEK_CUR) whence = SEEK_CUR;
        else if (s->whence == MP_SEEK_END) whence = SEEK_END;
        if (fseek(self->fp, (long)s->offset, whence) != 0) {
            *errcode = MP_EINVAL;
            return MP_STREAM_ERROR;
        }
        long pos = ftell(self->fp);
        if (pos < 0L) pos = 0L;
        s->offset = (mp_off_t)pos;
        return 0U;
    }
    *errcode = MP_EINVAL;
    return MP_STREAM_ERROR;
}

static const mp_rom_map_elem_t win64_file_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_read), MP_ROM_PTR(&mp_stream_read_obj) },
    { MP_ROM_QSTR(MP_QSTR_readinto), MP_ROM_PTR(&mp_stream_readinto_obj) },
    { MP_ROM_QSTR(MP_QSTR_readline), MP_ROM_PTR(&mp_stream_unbuffered_readline_obj) },
    { MP_ROM_QSTR(MP_QSTR_readlines), MP_ROM_PTR(&mp_stream_unbuffered_readlines_obj) },
    { MP_ROM_QSTR(MP_QSTR_write), MP_ROM_PTR(&mp_stream_write_obj) },
    { MP_ROM_QSTR(MP_QSTR_flush), MP_ROM_PTR(&mp_stream_flush_obj) },
    { MP_ROM_QSTR(MP_QSTR_close), MP_ROM_PTR(&mp_stream_close_obj) },
    { MP_ROM_QSTR(MP_QSTR_seek), MP_ROM_PTR(&mp_stream_seek_obj) },
    { MP_ROM_QSTR(MP_QSTR_tell), MP_ROM_PTR(&mp_stream_tell_obj) },
    { MP_ROM_QSTR(MP_QSTR___enter__), MP_ROM_PTR(&mp_identity_obj) },
    { MP_ROM_QSTR(MP_QSTR___exit__), MP_ROM_PTR(&mp_stream___exit___obj) },
};
static MP_DEFINE_CONST_DICT(win64_file_locals_dict, win64_file_locals_dict_table);

static const mp_stream_p_t win64_file_text_stream_p = {
    .read = win64_file_read,
    .write = win64_file_write,
    .ioctl = win64_file_ioctl,
    .is_text = 1,
};

static const mp_stream_p_t win64_file_bin_stream_p = {
    .read = win64_file_read,
    .write = win64_file_write,
    .ioctl = win64_file_ioctl,
    .is_text = 0,
};

static MP_DEFINE_CONST_OBJ_TYPE(
    mp_type_win64_file_text,
    MP_QSTR_TextIOWrapper,
    MP_TYPE_FLAG_ITER_IS_STREAM,
    print, win64_file_print,
    protocol, &win64_file_text_stream_p,
    locals_dict, &win64_file_locals_dict
);

static MP_DEFINE_CONST_OBJ_TYPE(
    mp_type_win64_file_bin,
    MP_QSTR_FileIO,
    MP_TYPE_FLAG_ITER_IS_STREAM,
    print, win64_file_print,
    protocol, &win64_file_bin_stream_p,
    locals_dict, &win64_file_locals_dict
);

mp_obj_t mp_builtin_open(size_t n_args, const mp_obj_t *args, mp_map_t *kwargs) {
    (void)kwargs;
    if (n_args < 1) {
        mp_raise_TypeError(MP_ERROR_TEXT("open() missing path"));
    }
    const char *path = mp_obj_str_get_str(args[0]);
    const char *mode = (n_args >= 2 && args[1] != mp_const_none) ? mp_obj_str_get_str(args[1]) : "r";
    bool is_binary = false;
    char cmode[8];
    size_t mi = 0, ci = 0;
    for (; mode[mi] && ci + 2 < sizeof(cmode); mi++) {
        if (mode[mi] == 'b') {
            is_binary = true;
            cmode[ci++] = 'b';
        } else if (mode[mi] != 't') {
            cmode[ci++] = mode[mi];
        }
    }
    if (!is_binary && ci + 1 < sizeof(cmode)) {
        cmode[ci++] = 'b';
    }
    cmode[ci] = '\0';

    FILE *fp = fopen(path, cmode);
    if (!fp) {
        mp_raise_OSError(MP_ENOENT);
    }
    mp_obj_win64_file_t *o = mp_obj_malloc(mp_obj_win64_file_t, is_binary ? &mp_type_win64_file_bin : &mp_type_win64_file_text);
    o->fp = fp;
    o->closed = false;
    return MP_OBJ_FROM_PTR(o);
}
MP_DEFINE_CONST_FUN_OBJ_KW(mp_builtin_open_obj, 1, mp_builtin_open);

static char *mpy_read_fp_to_gc(FILE *fp, size_t *out_len, size_t *out_alloc) {
    size_t cap = 4096U;
    size_t len = 0U;
    char *buf = m_new(char, cap);
    for (;;) {
        if (len + 1024U >= cap) {
            size_t ncap = cap * 2U;
            buf = m_renew(char, buf, cap, ncap);
            cap = ncap;
        }
        size_t nr = fread(buf + len, 1U, 1024U, fp);
        len += nr;
        if (nr < 1024U) break;
    }
    buf[len] = '\0';
    *out_len = len;
    *out_alloc = cap;
    return buf;
}

mp_lexer_t *mp_lexer_new_from_file(qstr filename) {
    const char *path = qstr_str(filename);
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        mp_raise_OSError(MP_ENOENT);
    }
    size_t len = 0U, alloc_sz = 0U;
    char *buf = mpy_read_fp_to_gc(fp, &len, &alloc_sz);
    fclose(fp);
    return mp_lexer_new_from_str_len(filename, buf, len, alloc_sz);
}

mp_import_stat_t mp_import_stat(const char *path) {
    if (!path || !path[0]) return MP_IMPORT_STAT_NO_EXIST;
    FILE *fp = fopen(path, "rb");
    if (fp) {
        fclose(fp);
        return MP_IMPORT_STAT_FILE;
    }
    return MP_IMPORT_STAT_NO_EXIST;
}

static char *mpy_read_fp_malloc(FILE *fp, size_t *out_len) {
    size_t cap = 4096U;
    size_t len = 0U;
    char *buf = (char *)malloc(cap);
    if (!buf) return NULL;
    for (;;) {
        if (len + 1024U >= cap) {
            size_t ncap = cap * 2U;
            char *nb = (char *)realloc(buf, ncap);
            if (!nb) { free(buf); return NULL; }
            buf = nb;
            cap = ncap;
        }
        size_t nr = fread(buf + len, 1U, 1024U, fp);
        len += nr;
        if (nr < 1024U) break;
    }
    buf[len] = '\0';
    if (out_len) *out_len = len;
    return buf;
}

static int mpy_run_source(qstr source_name, const char *src, size_t src_len) {
    nlr_buf_t nlr;
    if (nlr_push(&nlr) == 0) {
        mp_lexer_t *lex = mp_lexer_new_from_str_len(source_name, src, src_len, 0U);
        mp_parse_tree_t parse_tree = mp_parse(lex, MP_PARSE_FILE_INPUT);
        mp_obj_t module_fun = mp_compile(&parse_tree, lex->source_name, false);
        mp_call_function_0(module_fun);
        nlr_pop();
        return 0;
    } else {
        mp_obj_t exc = MP_OBJ_FROM_PTR(nlr.ret_val);
        if (mp_obj_exception_match(exc, MP_OBJ_FROM_PTR(&mp_type_SystemExit))) {
            mp_obj_exception_t *ex = (mp_obj_exception_t *)MP_OBJ_TO_PTR(exc);
            if (!ex->args || ex->args->len == 0U || ex->args->items[0] == mp_const_none) {
                return 0;
            }
            if (mp_obj_is_int(ex->args->items[0])) {
                return (int)(mp_obj_get_int(ex->args->items[0]) & 0xff);
            }
            mp_obj_print_helper(&mpy_stderr_print, ex->args->items[0], PRINT_STR);
            mpy_stderr_print_strn(NULL, "\n", 1U);
            return 1;
        }
        mp_obj_print_exception(&mpy_stderr_print, exc);
        return 1;
    }
}

int main(int argc, char **argv) {
    volatile int stack_anchor = 0;
    mpy_c_stack_top = (char *)&stack_anchor;

    bool mode_c = false;
    bool mode_stdin = false;
    const char *code_str = NULL;
    const char *script_path = NULL;
    int argv_start = 1;

    if (argc <= 1) {
        mode_stdin = true;
    } else if (strcmp(argv[1], "-v") == 0 || strcmp(argv[1], "--version") == 0) {
        printf("MicroPython v1.24.1 on ZCC-WIN64-PE32+\n");
        fflush(stdout);
        return 0;
    } else if (strcmp(argv[1], "-c") == 0) {
        if (argc < 3) {
            fprintf(stderr, "micropython.exe: -c requires an argument\n");
            return 2;
        }
        mode_c = true;
        code_str = argv[2];
        argv_start = 2;
    } else if (strcmp(argv[1], "-") == 0) {
        mode_stdin = true;
        argv_start = 1;
    } else if (argv[1][0] == '-') {
        fprintf(stderr, "micropython.exe: unknown option '%s'\n", argv[1]);
        return 2;
    } else {
        script_path = argv[1];
        argv_start = 1;
    }

    mpy_heap = (char *)malloc(ZCC_MPY_HEAP_BYTES);
    if (!mpy_heap) {
        fprintf(stderr, "micropython.exe: failed to allocate heap\n");
        return 1;
    }
    memset(mpy_heap, 0, ZCC_MPY_HEAP_BYTES);

    mp_pystack_init(mpy_pystack, &mpy_pystack[ZCC_MPY_PYSTACK_LEN]);
    gc_init(mpy_heap, mpy_heap + ZCC_MPY_HEAP_BYTES);
    mp_init();

    #if MICROPY_PY_SYS_PATH
    mp_obj_list_init(MP_OBJ_TO_PTR(mp_sys_path), 0);
    mp_obj_list_append(mp_sys_path, mp_obj_new_str(".", 1U));
    if (script_path) {
        const char *slash = strrchr(script_path, '/');
        const char *bslash = strrchr(script_path, '\\');
        const char *sep = (!slash || (bslash && bslash > slash)) ? bslash : slash;
        if (sep && sep > script_path) {
            mp_obj_list_append(mp_sys_path, mp_obj_new_str(script_path, (size_t)(sep - script_path)));
        }
    }
    #endif

    #if MICROPY_PY_SYS_ARGV
    mp_obj_list_init(MP_OBJ_TO_PTR(mp_sys_argv), 0);
    if (mode_c) {
        mp_obj_list_append(mp_sys_argv, mp_obj_new_str("-c", 2U));
        for (int i = 3; i < argc; i++) {
            mp_obj_list_append(mp_sys_argv, mp_obj_new_str(argv[i], strlen(argv[i])));
        }
    } else if (mode_stdin) {
        mp_obj_list_append(mp_sys_argv, mp_obj_new_str("-", 1U));
        for (int i = 2; i < argc; i++) {
            mp_obj_list_append(mp_sys_argv, mp_obj_new_str(argv[i], strlen(argv[i])));
        }
    } else {
        for (int i = argv_start; i < argc; i++) {
            mp_obj_list_append(mp_sys_argv, mp_obj_new_str(argv[i], strlen(argv[i])));
        }
    }
    #endif

    int rc = 0;
    if (mode_c) {
        rc = mpy_run_source(MP_QSTR__lt_string_gt_, code_str, strlen(code_str));
    } else if (mode_stdin) {
        size_t slen = 0U;
        char *buf = mpy_read_fp_malloc(stdin, &slen);
        if (!buf) {
            fprintf(stderr, "micropython.exe: out of memory reading stdin\n");
            rc = 1;
        } else {
            rc = mpy_run_source(MP_QSTR__lt_stdin_gt_, buf, slen);
            free(buf);
        }
    } else {
        FILE *fp = fopen(script_path, "rb");
        if (!fp) {
            fprintf(stderr, "micropython.exe: can't open file '%s'\n", script_path);
            rc = 1;
        } else {
            size_t slen = 0U;
            char *buf = mpy_read_fp_malloc(fp, &slen);
            fclose(fp);
            if (!buf) {
                fprintf(stderr, "micropython.exe: out of memory reading '%s'\n", script_path);
                rc = 1;
            } else {
                qstr sname = qstr_from_str(script_path);
                rc = mpy_run_source(sname, buf, slen);
                free(buf);
            }
        }
    }

    fflush(stdout);
    fflush(stderr);
    mp_deinit();
    free(mpy_heap);
    mpy_heap = NULL;
    return rc;
}
