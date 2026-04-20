void *malloc(long size);
void test() {
    long size = 10;
    void *p = malloc(size);
    {
        char *cp;
        long i;
        cp = (char *)p;
        for (i = 0; i < size; i++) {
            cp[i] = 0;
        }
    }
}
