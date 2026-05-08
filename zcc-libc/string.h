#ifndef __size_t__
#define __size_t__
typedef __SIZE_TYPE__ size_t;
#endif
#define NULL 0
void* memcpy(void*, const void*, size_t);
void* memset(void*, int, size_t);
int strcmp(const char*, const char*);
int strcasecmp(const char*, const char*);
int strncasecmp(const char*, const char*, size_t);
int strncmp(const char*, const char*, size_t);
size_t strlen(const char*);
char* strcpy(char*, const char*);
char* strcat(char*, const char*);
char* strncpy(char*, const char*, size_t);
char* strdup(const char*);
char* strpbrk(const char*, const char*);
char* strchr(const char*, int);
char* strrchr(const char*, int);
char* strstr(const char*, const char*);
int memcmp(const void*, const void*, size_t);
void* memmove(void*, const void*, size_t);
void* memchr(const void*, int, size_t);
int strcoll(const char*, const char*);
