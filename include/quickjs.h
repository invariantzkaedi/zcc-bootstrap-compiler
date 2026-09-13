#ifndef QUICKJS_H
#define QUICKJS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct JSRuntime JSRuntime;
typedef struct JSContext JSContext;
typedef struct JSObject JSObject;
typedef struct JSClass JSClass;
typedef uint32_t JSClassID;
typedef uint32_t JSAtom;

#if defined(__x86_64__) || defined(_M_X64)
typedef uint64_t JSValue;
#else
typedef struct JSValue {
    union {
        int32_t int32;
        double float64;
        void *ptr;
    } u;
    int64_t tag;
} JSValue;
#endif

typedef JSValue JSValueConst;

#define JS_EVAL_TYPE_GLOBAL   (0 << 0)
#define JS_EVAL_TYPE_MODULE   (1 << 0)
#define JS_EVAL_TYPE_DIRECT   (2 << 0)
#define JS_EVAL_TYPE_INDIRECT (3 << 0)

#define JS_TAG_FIRST              -11
#define JS_TAG_BIG_INT            -10
#define JS_TAG_SYMBOL             -9
#define JS_TAG_STRING             -8
#define JS_TAG_MODULE             -3
#define JS_TAG_FUNCTION_BYTECODE  -2
#define JS_TAG_OBJECT             -1
#define JS_TAG_INT                 0
#define JS_TAG_BOOL                1
#define JS_TAG_NULL                2
#define JS_TAG_UNDEFINED           3
#define JS_TAG_UNINITIALIZED       4
#define JS_TAG_CATCH_OFFSET        5
#define JS_TAG_EXCEPTION           6
#define JS_TAG_FLOAT64             7

JSRuntime *JS_NewRuntime(void);
void JS_FreeRuntime(JSRuntime *rt);
JSContext *JS_NewContext(JSRuntime *rt);
void JS_FreeContext(JSContext *ctx);

JSValue JS_Eval(JSContext *ctx, const char *input, size_t input_len,
                const char *filename, int eval_flags);

int JS_IsException(JSValue v);
JSValue JS_GetException(JSContext *ctx);
const char *JS_ToCString(JSContext *ctx, JSValue val);
void JS_FreeCString(JSContext *ctx, const char *ptr);
void JS_FreeValue(JSContext *ctx, JSValue v);

#ifdef __cplusplus
}
#endif

#endif /* QUICKJS_H */
