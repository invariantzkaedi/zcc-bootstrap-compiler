#include <stdio.h>
extern int lookup_keyword_fallback(const char*);
int my_test() {
    printf("int: %d\n", lookup_keyword_fallback("int"));
    return 0;
}
int main() {
    my_test();
    return 0;
}