int printf(const char *fmt, ...);
int main() {
    printf("size=%d val=%lu\n", sizeof(0xFFFFFFFFu), (unsigned long)(~(0xFFFFFFFFu)));
    return 0;
}
