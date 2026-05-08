typedef unsigned long size_t;
typedef int ssize_t;
int close(int);
ssize_t read(int, void*, size_t);
ssize_t write(int, const void*, size_t);
int access(const char*, int);
int isatty(int fd);
