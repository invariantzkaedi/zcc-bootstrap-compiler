/* Test for CG-IR-008: IR stack slot collision with caller params */
int deep_recursion(int a, int b, int c, int d, int e, int f, int g, int h) {
    if (a <= 0) return b + c + d + e + f + g + h;
    return deep_recursion(a-1, b, c, d, e, f, g, h) + a;
}
int main() { return deep_recursion(10, 1, 2, 3, 4, 5, 6, 7) == 325 ? 0 : 1; }
