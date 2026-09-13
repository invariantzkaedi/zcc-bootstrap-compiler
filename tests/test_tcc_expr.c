/*
 * Test TinyCC Expression Precedence, Bitwise Masks & Complex Indirection under ZCC
 */

int printf(const char *fmt, ...);
void exit(int status);

typedef struct {
    int op;
    long val;
    short flags;
} TCCExprToken;

static long eval_tcc_binop(int op, long a, long b) {
    switch (op) {
        case 1: return a + b;
        case 2: return a - b;
        case 3: return a * b;
        case 4: return b != 0 ? a / b : 0;
        case 5: return (a << 3) ^ (b >> 2);
        case 6: return (a & 0xFF00) | (b & 0x00FF);
        default: return 0;
    }
}

int main(void) {
    TCCExprToken tok_arr[4];
    tok_arr[0].op = 1; tok_arr[0].val = 100; tok_arr[0].flags = 0;
    tok_arr[1].op = 3; tok_arr[1].val = 25;  tok_arr[1].flags = 1;
    tok_arr[2].op = 5; tok_arr[2].val = 0x1234; tok_arr[2].flags = 2;
    tok_arr[3].op = 6; tok_arr[3].val = 0xABCD; tok_arr[3].flags = 3;

    long sum = 0;
    for (int i = 0; i < 4; i++) {
        long res = eval_tcc_binop(tok_arr[i].op, tok_arr[i].val, 16);
        sum += res + tok_arr[i].flags;
    }

    /* Expected:
     * op 1: 100 + 16 = 116 + 0 = 116
     * op 3: 25 * 16 = 400 + 1 = 401
     * op 5: (0x1234 << 3) ^ (16 >> 2) = (0x91A0) ^ 4 = 37280 ^ 4 = 37284 + 2 = 37286
     * op 6: (0xABCD & 0xFF00) | (16 & 0x00FF) = 0xAB00 | 16 = 0xAB10 = 43792 + 3 = 43795
     * Total sum = 116 + 401 + 37286 + 43795 = 81598
     */
    if (sum != 81598) {
        printf("FAILED: sum=%ld, expected 81598\n", sum);
        return 1;
    }

    printf("PASS: test_tcc_expr (sum=%ld)\n", sum);
    return 0;
}
