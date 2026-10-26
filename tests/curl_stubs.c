#include <stdlib.h>
#include <string.h>

typedef void *(*curl_malloc_callback)(size_t size);
typedef void (*curl_free_callback)(void *ptr);
typedef void *(*curl_realloc_callback)(void *ptr, size_t size);
typedef void *(*curl_calloc_callback)(size_t nmemb, size_t size);
typedef char *(*curl_strdup_callback)(const char *str);

/* Function pointer variables matching libcurl easy.c */
curl_malloc_callback Curl_cmalloc = malloc;
curl_free_callback Curl_cfree = free;
curl_realloc_callback Curl_crealloc = realloc;
curl_calloc_callback Curl_ccalloc = calloc;
curl_strdup_callback Curl_cstrdup = strdup;

const void *Curl_get_scheme_handler(const char *scheme) { (void)scheme; return NULL; }
int Curl_is_ASCII_name(const char *hostname) { (void)hostname; return 1; }
unsigned ZSTD_versionNumber(void) { return 0; }

int Curl_parse_login_details(const char *src, const char **userp, const char **passwdp, const char **optionsp) {
    (void)src; (void)userp; (void)passwdp; (void)optionsp;
    return 0;
}
