#include <stdio.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    JS_CLOSURE_LOCAL,
    JS_CLOSURE_ARG,
    JS_CLOSURE_REF,
    JS_CLOSURE_GLOBAL_REF,
    JS_CLOSURE_GLOBAL_DECL,
    JS_CLOSURE_GLOBAL,
    JS_CLOSURE_MODULE_DECL,
    JS_CLOSURE_MODULE_IMPORT,
} JSClosureTypeEnum;

typedef struct JSClosureVar {
    JSClosureTypeEnum closure_type : 3;
    uint8_t is_lexical : 1;
    uint8_t is_const : 1;
    uint8_t var_kind : 4;
    uint16_t var_idx;
    uint32_t var_name;
} JSClosureVar;

int main(void) {
    printf("sizeof(JSClosureVar) = %zu\n", sizeof(JSClosureVar));
    printf("offsetof(var_idx) = %zu\n", offsetof(JSClosureVar, var_idx));
    printf("offsetof(var_name) = %zu\n", offsetof(JSClosureVar, var_name));
    
    JSClosureVar cv;
    unsigned char *bytes = (unsigned char *)&cv;
    for (int i = 0; i < sizeof(cv); i++) bytes[i] = 0;
    
    cv.closure_type = JS_CLOSURE_GLOBAL_DECL;
    cv.is_lexical = 1;
    cv.is_const = 0;
    cv.var_kind = 2;
    cv.var_idx = 0x1234;
    cv.var_name = 0x56789abc;
    
    printf("bytes: ");
    for (int i = 0; i < sizeof(cv); i++) printf("%02x ", bytes[i]);
    printf("\n");
    printf("closure_type = %d\n", (int)cv.closure_type);
    printf("is_lexical = %d\n", (int)cv.is_lexical);
    printf("var_kind = %d\n", (int)cv.var_kind);
    return 0;
}
