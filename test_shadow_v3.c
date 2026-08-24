void *malloc(unsigned long);

void test_shadow() {
    void *p1 = malloc(10);
    *(int*)p1 = 1;

    void *p2 = malloc(20);
    if (!p2) return;
    *(int*)p2 = 2;
}

int main() {
    test_shadow();
    return 0;
}
