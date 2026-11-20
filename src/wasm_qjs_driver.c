/*
 * ZCC WASI CLI & VFS Driver for Upstream QuickJS 2024-01-13 (`src/wasm_qjs_driver.c`)
 * ====================================================================================
 * Links with unmodified `tests/embedded_quickjs/*.c` and `src/wasm_wasi_rt.c` to form
 * a standalone zero-`env.*` ES2023 JavaScript WASI module (`build_wasm/qjs.wasm`).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tests/embedded_quickjs/quickjs.h"

extern void _wasi_rt_ensure_init(void);

static JSValue js_wasi_print_common(JSContext *ctx, int argc, JSValueConst *argv, FILE *out) {
    int i;
    for (i = 0; i < argc; i++) {
        size_t len = 0;
        const char *str = JS_ToCStringLen(ctx, &len, argv[i]);
        if (!str) return JS_EXCEPTION;
        if (i > 0) fputc(' ', out);
        fwrite(str, 1, len, out);
        JS_FreeCString(ctx, str);
    }
    fputc('\n', out);
    fflush(out);
    return JS_UNDEFINED;
}

static JSValue js_wasi_print(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)this_val;
    return js_wasi_print_common(ctx, argc, argv, stdout);
}

static JSValue js_wasi_print_err(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)this_val;
    return js_wasi_print_common(ctx, argc, argv, stderr);
}

static JSValue js_wasi_read_file(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 1) return JS_ThrowTypeError(ctx, "wasiReadFile expects a path");
    const char *path = JS_ToCString(ctx, argv[0]);
    if (!path) return JS_EXCEPTION;
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        JSValue err = JS_ThrowReferenceError(ctx, "cannot open file '%s'", path);
        JS_FreeCString(ctx, path);
        return err;
    }
    JS_FreeCString(ctx, path);
    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (sz < 0) sz = 0;
    char *buf = (char *)malloc((size_t)sz + 1U);
    if (!buf) {
        fclose(fp);
        return JS_ThrowOutOfMemory(ctx);
    }
    size_t nread = (sz > 0) ? fread(buf, 1, (size_t)sz, fp) : 0;
    fclose(fp);
    buf[nread] = '\0';
    JSValue res = JS_NewStringLen(ctx, buf, nread);
    free(buf);
    return res;
}

static JSValue js_wasi_write_file(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 2) return JS_ThrowTypeError(ctx, "wasiWriteFile expects (path, data)");
    const char *path = JS_ToCString(ctx, argv[0]);
    if (!path) return JS_EXCEPTION;
    size_t len = 0;
    const char *data = JS_ToCStringLen(ctx, &len, argv[1]);
    if (!data) {
        JS_FreeCString(ctx, path);
        return JS_EXCEPTION;
    }
    FILE *fp = fopen(path, "wb");
    if (!fp) {
        JSValue err = JS_ThrowReferenceError(ctx, "cannot write file '%s'", path);
        JS_FreeCString(ctx, path);
        JS_FreeCString(ctx, data);
        return err;
    }
    size_t nwritten = (len > 0) ? fwrite(data, 1, len, fp) : 0;
    fclose(fp);
    JS_FreeCString(ctx, path);
    JS_FreeCString(ctx, data);
    return JS_NewInt32(ctx, (int32_t)nwritten);
}

static void qjs_wasi_register_helpers(JSContext *ctx) {
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue console = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, console, "log", JS_NewCFunction(ctx, js_wasi_print, "log", 1));
    JS_SetPropertyStr(ctx, console, "error", JS_NewCFunction(ctx, js_wasi_print_err, "error", 1));
    JS_SetPropertyStr(ctx, global, "console", console);
    JS_SetPropertyStr(ctx, global, "print", JS_NewCFunction(ctx, js_wasi_print, "print", 1));
    JS_SetPropertyStr(ctx, global, "printErr", JS_NewCFunction(ctx, js_wasi_print_err, "printErr", 1));
    JS_SetPropertyStr(ctx, global, "wasiReadFile", JS_NewCFunction(ctx, js_wasi_read_file, "wasiReadFile", 1));
    JS_SetPropertyStr(ctx, global, "wasiWriteFile", JS_NewCFunction(ctx, js_wasi_write_file, "wasiWriteFile", 2));
    JS_FreeValue(ctx, global);
}

static void qjs_wasi_dump_error(JSContext *ctx) {
    JSValue exc = JS_GetException(ctx);
    const char *msg = JS_ToCString(ctx, exc);
    fprintf(stderr, "%s\n", msg ? msg : "<exception>");
    if (msg) JS_FreeCString(ctx, msg);
    if (JS_IsObject(exc)) {
        JSValue stack = JS_GetPropertyStr(ctx, exc, "stack");
        if (!JS_IsUndefined(stack)) {
            const char *sstr = JS_ToCString(ctx, stack);
            if (sstr && sstr[0]) fprintf(stderr, "%s", sstr);
            if (sstr) JS_FreeCString(ctx, sstr);
        }
        JS_FreeValue(ctx, stack);
    }
    JS_FreeValue(ctx, exc);
    fflush(stderr);
}

static int qjs_wasi_drain_jobs(JSRuntime *rt) {
    JSContext *ctx1 = NULL;
    int err;
    while ((err = JS_ExecutePendingJob(rt, &ctx1)) != 0) {
        if (err < 0) {
            if (ctx1) qjs_wasi_dump_error(ctx1);
            return -1;
        }
    }
    return 0;
}

static int qjs_wasi_eval_buf(JSRuntime *rt, JSContext *ctx, const char *buf, size_t len,
                             const char *fname, int print_res) {
    JSValue val = JS_Eval(ctx, buf, len, fname, JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(val)) {
        qjs_wasi_dump_error(ctx);
        JS_FreeValue(ctx, val);
        return 1;
    }
    if (qjs_wasi_drain_jobs(rt) < 0) {
        JS_FreeValue(ctx, val);
        return 1;
    }
    if (print_res) {
        const char *str = JS_ToCString(ctx, val);
        if (!str) {
            qjs_wasi_dump_error(ctx);
            JS_FreeValue(ctx, val);
            return 1;
        }
        printf("%s\n", str);
        fflush(stdout);
        JS_FreeCString(ctx, str);
    }
    JS_FreeValue(ctx, val);
    return 0;
}

static char *qjs_read_all_file(FILE *fp, size_t *out_len) {
    size_t cap = 4096;
    size_t len = 0;
    char *buf = (char *)malloc(cap);
    if (!buf) return NULL;
    for (;;) {
        if (len + 2048 > cap) {
            cap *= 2;
            char *nb = (char *)realloc(buf, cap);
            if (!nb) { free(buf); return NULL; }
            buf = nb;
        }
        size_t n = fread(buf + len, 1, 2048, fp);
        len += n;
        if (n == 0) break;
    }
    buf[len] = '\0';
    if (out_len) *out_len = len;
    return buf;
}

int main(int argc, char **argv) {
    _wasi_rt_ensure_init();

    const char *eval_expr = NULL;
    int print_result = 0;
    const char *script_path = NULL;
    int i;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            printf("Usage: qjs.wasm [-e <expr> | --eval <expr> | -p <expr> | --] [script.js | -]\n");
            fflush(stdout);
            return 0;
        } else if (strcmp(argv[i], "-e") == 0 || strcmp(argv[i], "--eval") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "qjs.wasm: option '%s' requires an argument\n", argv[i]);
                return 2;
            }
            eval_expr = argv[++i];
            print_result = 0;
        } else if (strcmp(argv[i], "-p") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "qjs.wasm: option '%s' requires an argument\n", argv[i]);
                return 2;
            }
            eval_expr = argv[++i];
            print_result = 1;
        } else if (strcmp(argv[i], "--") == 0) {
            if (i + 1 < argc) script_path = argv[i + 1];
            break;
        } else if (strcmp(argv[i], "-") == 0) {
            break;
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "qjs.wasm: unknown option '%s'\n", argv[i]);
            return 2;
        } else {
            script_path = argv[i];
            break;
        }
    }

    JSRuntime *rt = JS_NewRuntime();
    if (!rt) {
        fprintf(stderr, "qjs.wasm: JS_NewRuntime failed\n");
        return 1;
    }
    JSContext *ctx = JS_NewContext(rt);
    if (!ctx) {
        fprintf(stderr, "qjs.wasm: JS_NewContext failed\n");
        JS_FreeRuntime(rt);
        return 1;
    }
    qjs_wasi_register_helpers(ctx);

    int rc = 0;
    if (eval_expr) {
        rc = qjs_wasi_eval_buf(rt, ctx, eval_expr, strlen(eval_expr), "<cmdline>", print_result);
    } else if (script_path) {
        FILE *fp = fopen(script_path, "rb");
        if (!fp) {
            fprintf(stderr, "qjs.wasm: cannot open script '%s'\n", script_path);
            rc = 1;
        } else {
            size_t len = 0;
            char *src = qjs_read_all_file(fp, &len);
            fclose(fp);
            if (!src) {
                fprintf(stderr, "qjs.wasm: out of memory reading '%s'\n", script_path);
                rc = 1;
            } else {
                rc = qjs_wasi_eval_buf(rt, ctx, src, len, script_path, 0);
                free(src);
            }
        }
    } else {
        size_t len = 0;
        char *src = qjs_read_all_file(stdin, &len);
        if (src && len > 0) {
            rc = qjs_wasi_eval_buf(rt, ctx, src, len, "<stdin>", 0);
        }
        free(src);
    }

    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    fflush(stdout);
    fflush(stderr);
    return rc;
}
