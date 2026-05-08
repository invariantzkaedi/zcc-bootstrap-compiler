void dumpBlock(void *b, unsigned long size);
void test() {
    unsigned char y = 0x55;
    dumpBlock(&y, sizeof(y));
}
