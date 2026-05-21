#include <stdio.h>
extern int lookup_keyword(char*);
int my_test() {
    printf("int: %d\n", lookup_keyword("int"));
    return 0;
}
int main() {
    my_test();
    return 0;
}
