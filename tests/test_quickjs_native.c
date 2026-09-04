#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "quickjs.h"

static int test_eval(JSContext *ctx, const char *expr, const char *expected) {
    JSValue val = JS_Eval(ctx, expr, strlen(expr), "<test>", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(val)) {
        JSValue exc = JS_GetException(ctx);
        const char *err = JS_ToCString(ctx, exc);
        printf("  [FAIL] %s -> EXCEPTION: %s\n", expr, err ? err : "unknown");
        JS_FreeCString(ctx, err);
        JS_FreeValue(ctx, exc);
        JS_FreeValue(ctx, val);
        return 1;
    }
    const char *str = JS_ToCString(ctx, val);
    if (!str) {
        printf("  [FAIL] %s -> NULL string result\n", expr);
        JS_FreeValue(ctx, val);
        return 1;
    }
    int pass = (strcmp(str, expected) == 0);
    if (pass) {
        printf("  [PASS] %s == %s\n", expr, str);
    } else {
        printf("  [FAIL] %s -> got '%s', expected '%s'\n", expr, str, expected);
    }
    JS_FreeCString(ctx, str);
    JS_FreeValue(ctx, val);
    return pass ? 0 : 1;
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("===============================================================\n");
    printf("  ZCC NATIVE QUICKJS ES2020 TEST HARNESS\n");
    printf("===============================================================\n");

    JSRuntime *rt = JS_NewRuntime();
    if (!rt) {
        fprintf(stderr, "Failed to create JSRuntime\n");
        return 1;
    }

    JSContext *ctx = JS_NewContext(rt);
    if (!ctx) {
        fprintf(stderr, "Failed to create JSContext\n");
        JS_FreeRuntime(rt);
        return 1;
    }

    int failures = 0;

    printf("\n--- Test Suite 1: Arithmetic & IEEE-754 Floats ---\n");
    failures += test_eval(ctx, "1 + 2 * 3", "7");
    failures += test_eval(ctx, "Math.hypot(3, 4)", "5");
    failures += test_eval(ctx, "Math.sin(0)", "0");
    failures += test_eval(ctx, "Math.PI > 3.14 && Math.PI < 3.15", "true");
    failures += test_eval(ctx, "Number.MAX_SAFE_INTEGER.toString()", "9007199254740991");

    printf("\n--- Test Suite 2: Objects, Arrays & JSON ---\n");
    failures += test_eval(ctx, "JSON.stringify({ a: 42, b: 'hello' })", "{\"a\":42,\"b\":\"hello\"}");
    failures += test_eval(ctx, "[1, 2, 3, 4, 5].map(x => x * x).reduce((a, b) => a + b, 0)", "55");
    failures += test_eval(ctx, "['apple', 'banana', 'cherry'].join('-')", "apple-banana-cherry");

    printf("\n--- Test Suite 3: Closures, Loops & Functions ---\n");
    failures += test_eval(ctx, "(() => { let s = 0; for (let i = 1; i <= 10; i++) s += i; return s; })()", "55");
    failures += test_eval(ctx, "function fib(n) { return n <= 1 ? n : fib(n-1) + fib(n-2); }; fib(10)", "55");

    printf("\n--- Test Suite 4: RegExp & Strings ---\n");
    failures += test_eval(ctx, "'quickjs-2024'.replace(/([a-z]+)-([0-9]+)/, '$2-$1')", "2024-quickjs");
    failures += test_eval(ctx, "/^[a-z0-9_]+$/i.test('zcc_2026')", "true");

    printf("\n--- Test Suite 5: ES6 Classes & Prototypes ---\n");
    failures += test_eval(ctx, "class Point { constructor(x, y) { this.x = x; this.y = y; } norm2() { return this.x*this.x + this.y*this.y; } }; new Point(3, 4).norm2()", "25");

    printf("\n--- Test Suite 6: Date & BigInt ---\n");
    failures += test_eval(ctx, "new Date(0).toISOString()", "1970-01-01T00:00:00.000Z");
    failures += test_eval(ctx, "(2n ** 64n).toString()", "18446744073709551616");

    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);

    printf("\n===============================================================\n");
    if (failures == 0) {
        printf("  ALL QUICKJS TESTS PASSED CLEANLY! (Failures: 0)\n");
        printf("===============================================================\n");
        return 0;
    } else {
        printf("  QUICKJS TESTS FAILED (Failures: %d)\n", failures);
        printf("===============================================================\n");
        return 1;
    }
}
