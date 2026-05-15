/* Regression: struct member access via pointer must use correct field offsets.
 * Tests both s.field and ps->field equivalence. */
int main() {
    struct S { char a; int b; short c; long d; };
    struct S s = {42, 1000, 77, 999};
    struct S *ps = &s;
    int sum = 0;
    sum += s.a;       /* 42 */
    sum += ps->b;     /* 1000 */
    sum += s.c;       /* 77 */
    sum += (int)ps->d; /* 999 */
    /* total = 2118, & 255 = 70 */
    return sum & 255;
}
