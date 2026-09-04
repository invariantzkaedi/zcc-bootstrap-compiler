#include <stdio.h>
#include <string.h>
#include "quickjs.h"

static void check_eval(JSContext *ctx, const char *expr) {
    JSValue val = JS_Eval(ctx, expr, strlen(expr), "<test>", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(val)) {
        JSValue exc = JS_GetException(ctx);
        const char *err = JS_ToCString(ctx, exc);
        printf("%s -> EXCEPTION: %s\n", expr, err ? err : "unknown");
        JS_FreeCString(ctx, err);
        JS_FreeValue(ctx, exc);
    } else {
        const char *s = JS_ToCString(ctx, val);
        printf("%s -> %s (tag=%d)\n", expr, s ? s : "(null)", (int)JS_VALUE_GET_TAG(val));
        JS_FreeCString(ctx, s);
    }
    JS_FreeValue(ctx, val);
}

int main(void) {
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx = JS_NewContext(rt);
    check_eval(ctx, "Math.PI");
    check_eval(ctx, "3.14");
    check_eval(ctx, "3.141592653589793 > 3.14");
    check_eval(ctx, "Math.PI > 3.14");
    check_eval(ctx, "Math.PI < 3.15");
    check_eval(ctx, "Math.PI > 3.14 && Math.PI < 3.15");
    check_eval(ctx, "2n ** 64n");
    return 0;
}
