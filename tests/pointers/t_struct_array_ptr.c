/* Regression: struct array pointer arithmetic.
 * (ptr + 1) must advance by sizeof(struct), not sizeof(int). */
int main() {
    struct P { int x; int y; };
    struct P arr[3] = {{10, 20}, {30, 40}, {50, 60}};
    struct P *p = arr;
    int sum = 0;
    sum += p->x;           /* 10 */
    sum += (p + 1)->y;     /* 40 */
    sum += p[2].x;         /* 50 */
    sum += (p + 2)->y;     /* 60 */
    /* total = 160 */
    return sum & 255;  /* 160 */
}
