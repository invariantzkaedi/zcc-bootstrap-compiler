/* Regression: function pointer call. */
static int double_it(int x) { return x * 2; }
int main() {
    int (*fp)(int) = double_it;
    int r = fp(21);
    return r;  /* 42 */
}
