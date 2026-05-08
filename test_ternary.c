#include <stdio.h>
int main() {
    const char *p_mode = NULL;
    const char *mode = p_mode ? p_mode : "bt";
    if (mode == NULL) {
        printf("BUG: mode is NULL!\n");
    } else {
        printf("mode is: %s\n", mode);
    }
    return 0;
}
