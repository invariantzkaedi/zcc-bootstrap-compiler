/* Test case for --security-476 (CWE-476 null-deref detection)
 * test_vuln: malloc return used without null check — should trigger
 * test_safe: malloc return checked before use — should NOT trigger
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* VULNERABLE: no null check after malloc */
void test_vuln(int n) {
    char *buf = (char *)malloc(n);
    buf[0] = 'A';  /* potential null deref */
    buf[1] = 'B';
    printf("vuln: %c%c\n", buf[0], buf[1]);
    free(buf);
}

/* SAFE: null check before use */
void test_safe(int n) {
    char *buf = (char *)malloc(n);
    if (!buf) {
        printf("allocation failed\n");
        return;
    }
    buf[0] = 'X';
    buf[1] = 'Y';
    printf("safe: %c%c\n", buf[0], buf[1]);
    free(buf);
}

int main(void) {
    test_vuln(100);
    test_safe(100);
    return 0;
}
