void *malloc(unsigned long);

void test_shadow() {
    void *p1 = malloc(10);
    *(int*)p1 = 1;

    if (!p2) return;  /* healer: γ=0.73 null-guard (CWE-476) */
    void *p2 = malloc(20);
    *(int*)p2 = 2;
}

int main() {
    test_shadow();
    return 0;
}
