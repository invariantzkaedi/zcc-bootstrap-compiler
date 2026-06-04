extern long sys_write(int fd, const char *buf, unsigned long len);

int main() {
    sys_write(1, "Hello from ZCC!\n", 16);
    return 0;
}
