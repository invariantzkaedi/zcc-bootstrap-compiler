/*
 * Test TinyCC Tokenization, Character Escapes & Hex Literal Resolution under ZCC
 */

int printf(const char *fmt, ...);
int strcmp(const char *s1, const char *s2);

static int parse_tcc_char_escape(const char *p) {
    if (p[0] != '\\') return (unsigned char)p[0];
    switch (p[1]) {
        case 'n': return '\n';
        case 't': return '\t';
        case 'r': return '\r';
        case '0': return '\0';
        case '\\': return '\\';
        case '\'': return '\'';
        case '\"': return '\"';
        case 'x': {
            int val = 0;
            for (int k = 2; k < 4; k++) {
                char c = p[k];
                if (c >= '0' && c <= '9') val = (val << 4) | (c - '0');
                else if (c >= 'a' && c <= 'f') val = (val << 4) | (c - 'a' + 10);
                else if (c >= 'A' && c <= 'F') val = (val << 4) | (c - 'A' + 10);
                else break;
            }
            return val;
        }
        default: return p[1];
    }
}

int main(void) {
    const char *escapes[] = { "\\n", "\\t", "\\r", "\\\\", "\\x7f", "\\'" };
    int expected[] = { 10, 9, 13, 92, 127, 39 };

    int total = 0;
    for (int i = 0; i < 6; i++) {
        int c = parse_tcc_char_escape(escapes[i]);
        if (c != expected[i]) {
            printf("FAILED at index %d: got %d, expected %d\n", i, c, expected[i]);
            return 1;
        }
        total += c;
    }

    /* Expected: 10 + 9 + 13 + 92 + 127 + 39 = 290 */
    if (total != 290) {
        printf("FAILED: total=%d, expected 290\n", total);
        return 1;
    }

    printf("PASS: test_tcc_tok (total=%d)\n", total);
    return 0;
}
