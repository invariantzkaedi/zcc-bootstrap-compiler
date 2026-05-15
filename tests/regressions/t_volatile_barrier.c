int main() {
    int x = 10;
    volatile int *p = &x;
    x = 20;
    *p = 30; // barrier
    return x == 30 ? 0 : 1;
}
