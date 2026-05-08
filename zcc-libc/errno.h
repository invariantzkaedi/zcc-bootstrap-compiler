int *__errno_location(void);
#define errno (*__errno_location())
#define ENOENT 2
#define EINTR 4
#define EWOULDBLOCK 11
#define ECONNRESET 104
