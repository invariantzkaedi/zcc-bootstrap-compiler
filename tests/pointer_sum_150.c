#include <stdio.h>
#include <assert.h>

int compute_pointer_sum(const int *arr, int count) {
    const int *ptr = arr;
    int sum = 0;
    for (int i = 0; i < count; i++) {
        sum += *(ptr + i);
    }
    return sum;
}

int main(void) {
    int numbers[5] = {10, 20, 30, 40, 50};
    int sum = compute_pointer_sum(numbers, 5);

    printf("Numbers: [%d, %d, %d, %d, %d]\n",
           numbers[0], numbers[1], numbers[2], numbers[3], numbers[4]);
    printf("Computed pointer sum: %d (Expected: 150)\n", sum);

    assert(sum == 150);

    if (sum == 150) {
        printf("SOVEREIGN_SYMPHONY_PASS: sum=%d\n", sum);
        return 0;
    }
    return 1;
}
