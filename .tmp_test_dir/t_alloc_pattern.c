char *my_alloc(int size) {
    char *cp;
    int i;
    int aligned;
    aligned = (size + 7) & -8;
    cp = (char *)calloc(1, aligned);
    if (!cp) return 0;
    for (i = 0; i < aligned; i = i + 1)
        cp[i] = 0;
    return cp;
}
int main() {
    char *p;
    p = my_alloc(100);
    if (p) { free(p); return 0; }
    return 1;
}
