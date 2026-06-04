int heavy(int a, int b, int c, int d, int e) {
    int x; int y; int z; int w; int v;
    x = a + b;
    y = c + d;
    z = x * y;
    w = z - e;
    v = w + a + b + c + d + e;
    return v;
}
int main() { return heavy(1, 2, 3, 4, 5); }
