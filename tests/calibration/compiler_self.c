/*
 * compiler_self.c — D-30: Compiler Self Workload
 *
 * Simulates a mini text lexer / preprocessor state machine.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int run_compiler_self(const char *text) {
    int in_comment = 0;
    int token_count = 0;
    int paren_depth = 0;
    const char *p = text;
    
    while (*p) {
        if (in_comment) {
            if (p[0] == '*' && p[1] == '/') {
                in_comment = 0;
                p += 2;
                continue;
            }
            p++;
        } else {
            if (p[0] == '/' && p[1] == '*') {
                in_comment = 1;
                p += 2;
                continue;
            }
            if (*p == '(' || *p == '{' || *p == '[') {
                paren_depth++;
            } else if (*p == ')' || *p == '}' || *p == ']') {
                if (paren_depth > 0) paren_depth--;
            } else if (*p >= 'a' && *p <= 'z') {
                if (p == text || *(p - 1) == ' ' || *(p - 1) == '\n' || *(p - 1) == '\t') {
                    token_count++;
                }
            }
            p++;
        }
    }
    return token_count + paren_depth;
}

int main(int argc, char **argv) {
    const char *text = "/* comment block */ void func(int x) { int y = x * 2; /* trailing comment */ }";
    if (argc > 1) {
        text = argv[1];
    }
    int result = 0;
    /* Repeat scan to generate some call count and activity */
    for (int i = 0; i < 200; i++) {
        result += run_compiler_self(text);
    }
    printf("result=%d\n", result);
    return 0;
}
