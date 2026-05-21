#include <stdio.h>
int my_test() {
    int arr[5];
    arr[0] = 10; arr[1] = 20; arr[2] = 30; arr[3] = 40; arr[4] = 50;
    int pos = 0;
    int val = arr[pos++];
    printf("val: %d, pos: %d\n", val, pos);
    return val;
}
int main() {
    my_test();
    return 0;
}
