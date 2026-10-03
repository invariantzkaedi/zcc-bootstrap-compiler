int test_identities(int a, long b) {
    long x = b + 0;
    long y = x - 0;
    long z = y * 1;
    long w = z * 0;
    return (int)(x + y + z + w);
}

int main(void) {
    int res = test_identities(42, 100);
    if (res != 300) return 1;
    return 0;
}
