/* Regression: array indexing via pointer arithmetic must scale by sizeof(elem).
 * Tests: arr[i] == *(p + i) == p[i] for int (4 bytes). */
int main() {
    int arr[5] = {10, 20, 30, 40, 50};
    int *p = arr;
    int sum = 0;
    sum += arr[0] + arr[2] + arr[4];  /* 10 + 30 + 50 = 90 */
    sum += *(p + 1) + *(p + 3);       /* 20 + 40 = 60 */
    sum += p[2];                       /* 30 */
    /* total = 180 */
    return sum & 255;  /* 180 */
}
