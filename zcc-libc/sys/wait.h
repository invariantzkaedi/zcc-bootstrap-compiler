int waitpid(int, int*, int);
#define WNOHANG 1
#define WTERMSIG(status) ((status) & 0x7f)
#define WIFEXITED(status) (WTERMSIG(status) == 0)
#define WEXITSTATUS(status) (((status) >> 8) & 0xff)
#define WIFSIGNALED(status) (((status) & 0x7f) != 0 && ((status) & 0x7f) != 0x7f)
