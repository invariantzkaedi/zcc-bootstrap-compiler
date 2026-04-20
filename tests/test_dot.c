#include <stdio.h>
int main() {
    const char *s = "3.14";
    int i = 0;
    while (s[i] >= '0' && s[i] <= '9') i++;
    printf("stopped at i=%d char='%c' (0x%02x)\n", i, s[i], (unsigned char)s[i]);
    printf("dot test: %d\n", s[i] == '.');
    return 0;
}
