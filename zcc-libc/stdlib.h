typedef unsigned long size_t;
#define NULL 0
void* malloc(size_t);
void free(void*);
void* calloc(size_t, size_t);
void* realloc(void*, size_t);
int atoi(const char*);
void exit(int);
#define RAND_MAX 32767
int rand(void);
int abs(int);
void abort(void);
long strtol(const char*, char**, int);
unsigned long strtoul(const char*, char**, int);
unsigned long long strtoull(const char*, char**, int);
char* getenv(const char*);
#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1
