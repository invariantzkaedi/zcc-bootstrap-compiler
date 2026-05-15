/* Regression: pointer subtraction must divide byte difference by sizeof(elem).
 * ptrdiff_t result = (hi - lo) == element count, not byte count. */
int main() {
    long arr[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    long *lo = &arr[1];
    long *hi = &arr[6];
    long diff = hi - lo;  /* 5 elements, NOT 40 bytes */
    return (int)(diff & 255);  /* 5 */
}
