#include <stdio.h>
int main() {
    long arr[4] = {10, 20, 30, 40};
    printf("arr[0]=%ld arr[1]=%ld arr[2]=%ld arr[3]=%ld\n", arr[0], arr[1], arr[2], arr[3]);
    return (int)(arr[2]);
}
