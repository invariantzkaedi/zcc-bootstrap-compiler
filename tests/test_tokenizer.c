#include <stdio.h>
extern int sqlite3GetToken(const unsigned char *z, int *tokenType);

int main() {
    const unsigned char *sql = (const unsigned char *)"3.14";
    int tokenType = -1;
    int n = sqlite3GetToken(sql, &tokenType);
    printf("input='3.14' n=%d tokenType=%d\n", n, tokenType);
    printf("remaining='%s'\n", sql + n);

    // Now try just the dot
    sql = (const unsigned char *)".14";
    n = sqlite3GetToken(sql, &tokenType);
    printf("input='.14' n=%d tokenType=%d\n", n, tokenType);
    return 0;
}
