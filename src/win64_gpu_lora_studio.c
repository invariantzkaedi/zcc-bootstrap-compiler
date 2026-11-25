/*
 * ZKAEDI ZCC // Sovereign Fused OpenAI HTTP/SSE Server + Blackwell LoRA Studio (Track 20)
 * ======================================================================================
 * Unifies Track 18 (Pure C99 Winsock2 OpenAI HTTP/1.1 + Chunked SSE Server + Web UI)
 * and Track 19 (Zero-PyTorch, Zero-CUDA-SDK Blackwell sm_120 LoRA Autodiff Trainer)
 * into a single standalone Win64 PE32+ executable.
 *
 * ZERO Python, ZERO PyTorch, ZERO MSVC, ZERO MinGW, ZERO CUDA Toolkit, ZERO OpenSSL.
 * Direct Win64 PE32+ imports: System32\{WS2_32,msvcrt,nvcuda}.dll ONLY.
 */

#define ZCC_GPU_LORA_LIBRARY 1
#include "win64_gpu_lora.c"
#include "lora_studio_ui_html.h"
#include "runner_game_ui_html.h"
#include "svg_forge_ui_html.h"
#include "mario_game_ui_html.h"
#include "openapi_spec_json.h"
#include "api_docs_ui_html.h"

#ifndef SIZE_MAX
#define SIZE_MAX (~(size_t)0U)
#endif

#define ZCC_AF_INET        2
#define ZCC_SOCK_STREAM    1
#define ZCC_IPPROTO_TCP    6
#define ZCC_TCP_NODELAY    1
#define ZCC_SD_SEND        1
#define ZCC_SD_BOTH        2

#define ZCC_MAX_STUDIO_ADAPTERS 16

typedef struct {
    int16_t sin_family;
    uint16_t sin_port;
    uint32_t sin_addr;
    uint8_t sin_zero[8];
} ZccSockAddrIn;

#ifdef ZCC_LINUX_ELF
/* POSIX libc + socket shims for native ZCC Linux ELF64 + libcuda.so.1 execution */
typedef int ZccSocket;
typedef unsigned int ZccSockLen;
#define ZCC_INVALID_SOCKET (-1)
#define ZCC_SOL_SOCKET     1
#define ZCC_SO_REUSEADDR   2
#define ZCC_SO_RCVTIMEO    20
#define ZCC_SO_SNDTIMEO    21
#define ZCC_SEND_FLAGS     16384 /* MSG_NOSIGNAL */

extern int open(const char *path, int oflag, int mode);
extern int close(int fd);
extern long read(int fd, void *buf, size_t count);
extern long lseek(int fd, long offset, int origin);
extern long time(long *timer);
extern void *signal(int sig, void *handler);
extern void *opendir(const char *name);
extern void *readdir(void *dirp);
extern int closedir(void *dirp);

int _open(const char *path, int oflag) { (void)oflag; return open(path, 0, 0); }
int _close(int fd) { return close(fd); }
int _read(int fd, void *buf, unsigned int count) {
    long r = read(fd, buf, (size_t)count);
    return (r < 0L) ? -1 : (int)r;
}
long long _lseeki64(int fd, long long offset, int origin) {
    return (long long)lseek(fd, (long)offset, origin);
}
int _get_pgmptr(char **pValue) { if (pValue) *pValue = ""; return -1; }

extern int socket(int af, int type, int protocol);
extern int bind(int s, const void *addr, ZccSockLen namelen);
extern int listen(int s, int backlog);
extern int accept(int s, void *addr, ZccSockLen *addrlen);
extern long recv(int s, void *buf, size_t len, int flags);
extern long send(int s, const void *buf, size_t len, int flags);
extern int setsockopt(int s, int level, int optname, const void *optval, ZccSockLen optlen);
extern int shutdown(int s, int how);
extern int getsockname(int s, void *name, ZccSockLen *namelen);
extern uint16_t htons(uint16_t hostshort);
extern uint16_t ntohs(uint16_t netshort);
extern uint32_t inet_addr(const char *cp);

static long long studio_time64(void) { return (long long)time(NULL); }
static int studio_net_init(void) { signal(13, (void *)1); return 0; } /* ignore SIGPIPE */
static void studio_net_cleanup(void) {}
static void studio_sock_close(ZccSocket s) { close(s); }
static int studio_net_err(void) { return 0; }
static long studio_send(ZccSocket s, const char *buf, int len) { return send(s, buf, (size_t)len, ZCC_SEND_FLAGS); }
static long studio_recv(ZccSocket s, char *buf, int len) { return recv(s, buf, (size_t)len, 0); }
static void studio_set_listen_reuse(ZccSocket s) {
    int yes = 1;
    setsockopt(s, ZCC_SOL_SOCKET, ZCC_SO_REUSEADDR, &yes, (ZccSockLen)sizeof(yes));
}
static void studio_set_client_sockopts(ZccSocket s) {
    long tv_rcv[2] = { 15L, 0L };
    long tv_snd[2] = { 60L, 0L };
    int nodelay = 1;
    setsockopt(s, ZCC_SOL_SOCKET, ZCC_SO_RCVTIMEO, tv_rcv, (ZccSockLen)sizeof(tv_rcv));
    setsockopt(s, ZCC_SOL_SOCKET, ZCC_SO_SNDTIMEO, tv_snd, (ZccSockLen)sizeof(tv_snd));
    setsockopt(s, ZCC_IPPROTO_TCP, ZCC_TCP_NODELAY, &nodelay, (ZccSockLen)sizeof(nodelay));
}
#else
/* MSVC CRT 64-bit epoch time & process id */
extern long long _time64(long long *timer);
extern int _getpid(void);

/* Winsock2 TCP/IP networking (System32\WS2_32.dll) */
typedef uint64_t ZccSocket;
typedef int ZccSockLen;
#define ZCC_INVALID_SOCKET (~(ZccSocket)0ULL)
#define ZCC_SOL_SOCKET     0xFFFF
#define ZCC_SO_REUSEADDR   0x0004
#define ZCC_SO_SNDTIMEO    0x1005
#define ZCC_SO_RCVTIMEO    0x1006

extern int __stdcall WSAStartup(uint16_t wVersionRequested, void *lpWSAData);
extern int __stdcall WSACleanup(void);
extern int __stdcall WSAGetLastError(void);
extern ZccSocket __stdcall socket(int af, int type, int protocol);
extern int __stdcall bind(ZccSocket s, const void *addr, int namelen);
extern int __stdcall listen(ZccSocket s, int backlog);
extern ZccSocket __stdcall accept(ZccSocket s, void *addr, int *addrlen);
extern int __stdcall recv(ZccSocket s, char *buf, int len, int flags);
extern int __stdcall send(ZccSocket s, const char *buf, int len, int flags);
extern int __stdcall closesocket(ZccSocket s);
extern int __stdcall setsockopt(ZccSocket s, int level, int optname, const char *optval, int optlen);
extern int __stdcall shutdown(ZccSocket s, int how);
extern int __stdcall getsockname(ZccSocket s, void *name, int *namelen);
extern uint16_t __stdcall htons(uint16_t hostshort);
extern uint16_t __stdcall ntohs(uint16_t netshort);
extern uint32_t __stdcall inet_addr(const char *cp);

static long long studio_time64(void) { return _time64(NULL); }
static int studio_net_init(void) {
    uint8_t wsa_data[512];
    memset(wsa_data, 0, sizeof(wsa_data));
    return WSAStartup(0x0202U, wsa_data);
}
static void studio_net_cleanup(void) { WSACleanup(); }
static void studio_sock_close(ZccSocket s) { closesocket(s); }
static int studio_net_err(void) { return WSAGetLastError(); }
static long studio_send(ZccSocket s, const char *buf, int len) { return (long)send(s, buf, len, 0); }
static long studio_recv(ZccSocket s, char *buf, int len) { return (long)recv(s, buf, len, 0); }
static void studio_set_listen_reuse(ZccSocket s) { (void)s; }
static void studio_set_client_sockopts(ZccSocket s) {
    uint32_t rcv_ms = 15000U;
    uint32_t snd_ms = 60000U;
    int nodelay = 1;
    setsockopt(s, ZCC_SOL_SOCKET, ZCC_SO_RCVTIMEO, (const char *)&rcv_ms, 4);
    setsockopt(s, ZCC_SOL_SOCKET, ZCC_SO_SNDTIMEO, (const char *)&snd_ms, 4);
    setsockopt(s, ZCC_IPPROTO_TCP, ZCC_TCP_NODELAY, (const char *)&nodelay, 4);
}
#endif

#define ZCC_MAX_DISK_FILES 64

typedef struct {
    char names[ZCC_MAX_DISK_FILES][260];
    int count;
} StudioDirFiles;

static int is_lora_basename(const char *nm) {
    if (!nm || !nm[0] || nm[0] == '.') return 0;
    size_t n = strlen(nm);
    if (n <= 9U || n >= 256U) return 0;
    for (size_t i = 0U; i < n; i++) {
        if (nm[i] == '/' || nm[i] == '\\') return 0;
    }
    return (strcmp(nm + n - 9U, ".zcc_lora") == 0) ? 1 : 0;
}

static void sort_dir_files(StudioDirFiles *out) {
    for (int i = 0; i < out->count; i++) {
        for (int j = i + 1; j < out->count; j++) {
            if (strcmp(out->names[i], out->names[j]) > 0) {
                char tmp[260];
                memcpy(tmp, out->names[i], sizeof(tmp));
                memcpy(out->names[i], out->names[j], sizeof(tmp));
                memcpy(out->names[j], tmp, sizeof(tmp));
            }
        }
    }
}

#ifdef ZCC_LINUX_ELF
static void studio_list_lora_dir(const char *dir, StudioDirFiles *out) {
    memset(out, 0, sizeof(*out));
    if (!dir || !dir[0]) return;
    void *dp = opendir(dir);
    if (!dp) return;
    for (;;) {
        void *ent = readdir(dp);
        if (!ent) break;
        const char *nm = (const char *)ent + 19; /* glibc x86_64 struct dirent.d_name offset */
        if (is_lora_basename(nm) && out->count < ZCC_MAX_DISK_FILES) {
            snprintf(out->names[out->count++], sizeof(out->names[0]), "%s", nm);
        }
    }
    closedir(dp);
    sort_dir_files(out);
}
#else
static int is_safe_win_dir_path(const char *s) {
    if (!s || !s[0]) return 0;
    size_t n = strlen(s);
    if (n >= 400U) return 0;
    if (strstr(s, "..") != NULL) return 0;
    for (size_t i = 0U; i < n; i++) {
        char c = s[i];
        int ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                 (c >= '0' && c <= '9') || c == '_' || c == '.' ||
                 c == '/' || c == '\\' || c == ':' || c == ' ' || c == '-';
        if (!ok) return 0;
    }
    return 1;
}

static void studio_list_lora_dir(const char *dir, StudioDirFiles *out) {
    memset(out, 0, sizeof(*out));
    if (!is_safe_win_dir_path(dir)) return;

    char win_dir[416];
    snprintf(win_dir, sizeof(win_dir), "%s", dir);
    size_t wlen = strlen(win_dir);
    for (size_t i = 0U; i < wlen; i++) {
        if (win_dir[i] == '/') win_dir[i] = '\\';
    }
    while (wlen > 1U && win_dir[wlen - 1U] == '\\') {
        win_dir[--wlen] = '\0';
    }

    const char *tdir = getenv("TEMP");
    if (!tdir || !is_safe_win_dir_path(tdir)) tdir = ".";
    char win_tmp[416];
    snprintf(win_tmp, sizeof(win_tmp), "%s", tdir);
    size_t tlen = strlen(win_tmp);
    while (tlen > 1U && (win_tmp[tlen - 1U] == '\\' || win_tmp[tlen - 1U] == '/')) {
        win_tmp[--tlen] = '\0';
    }

    char tmpfile[512];
    snprintf(tmpfile, sizeof(tmpfile), "%s\\zcc_ls_%d.tmp", win_tmp, _getpid());
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "cmd /c dir /b \"%s\\*.zcc_lora\" > \"%s\" 2>nul", win_dir, tmpfile);
    (void)system(cmd);

    FILE *fp = fopen(tmpfile, "r");
    if (fp) {
        char line[320];
        while (fgets(line, (int)sizeof(line), fp) != NULL) {
            size_t l = strlen(line);
            while (l > 0U && (line[l - 1U] == '\r' || line[l - 1U] == '\n' || line[l - 1U] == ' ')) {
                line[--l] = '\0';
            }
            if (is_lora_basename(line) && out->count < ZCC_MAX_DISK_FILES) {
                snprintf(out->names[out->count++], sizeof(out->names[0]), "%s", line);
            }
        }
        fclose(fp);
    }
    remove(tmpfile);
    sort_dir_files(out);
}
#endif

/* Growable string buffer */
typedef struct {
    char *data;
    size_t len;
    size_t cap;
} StrBuf;

static int sb_ensure(StrBuf *sb, size_t add) {
    if (add > SIZE_MAX - sb->len - 1U) return -1;
    size_t need = sb->len + add + 1U;
    if (need <= sb->cap) return 0;
    size_t ncap = (sb->cap == 0U) ? 256U : sb->cap;
    while (ncap < need) {
        if (ncap > SIZE_MAX / 2U) { ncap = need; break; }
        ncap *= 2U;
    }
    char *nb = (char *)realloc(sb->data, ncap);
    if (!nb) return -1;
    sb->data = nb;
    sb->cap = ncap;
    return 0;
}

static int sb_append_n(StrBuf *sb, const char *s, size_t n) {
    if (sb_ensure(sb, n) != 0) return -1;
    if (n > 0U && s) memcpy(sb->data + sb->len, s, n);
    sb->len += n;
    sb->data[sb->len] = '\0';
    return 0;
}

static int sb_append(StrBuf *sb, const char *s) {
    return sb_append_n(sb, s ? s : "", s ? strlen(s) : 0U);
}

static int sb_append_char(StrBuf *sb, char c) {
    return sb_append_n(sb, &c, 1U);
}

static void sb_free(StrBuf *sb) {
    if (sb->data) free(sb->data);
    sb->data = NULL;
    sb->len = 0U;
    sb->cap = 0U;
}

static int sb_append_json_str(StrBuf *sb, const char *s) {
    if (sb_append_char(sb, '"') != 0) return -1;
    if (s) {
        for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
            unsigned char c = *p;
            if (c == '"') { if (sb_append(sb, "\\\"") != 0) return -1; }
            else if (c == '\\') { if (sb_append(sb, "\\\\") != 0) return -1; }
            else if (c == '\b') { if (sb_append(sb, "\\b") != 0) return -1; }
            else if (c == '\f') { if (sb_append(sb, "\\f") != 0) return -1; }
            else if (c == '\n') { if (sb_append(sb, "\\n") != 0) return -1; }
            else if (c == '\r') { if (sb_append(sb, "\\r") != 0) return -1; }
            else if (c == '\t') { if (sb_append(sb, "\\t") != 0) return -1; }
            else if (c < 0x20U) {
                char esc[8];
                snprintf(esc, sizeof(esc), "\\u%04x", (unsigned int)c);
                if (sb_append(sb, esc) != 0) return -1;
            } else {
                if (sb_append_char(sb, (char)c) != 0) return -1;
            }
        }
    }
    return sb_append_char(sb, '"');
}

/* Raw Winsock2 helpers */
static int sock_send_all(ZccSocket s, const char *buf, size_t len) {
    size_t sent = 0U;
    while (sent < len) {
        size_t rem = len - sent;
        int chunk = (rem > 65536U) ? 65536 : (int)rem;
        long rc = studio_send(s, buf + sent, chunk);
        if (rc <= 0L) return -1;
        sent += (size_t)rc;
    }
    return 0;
}

static const char *CORS_HDRS =
    "Access-Control-Allow-Origin: *\r\n"
    "Access-Control-Allow-Methods: GET, POST, DELETE, OPTIONS\r\n"
    "Access-Control-Allow-Headers: Content-Type, Authorization, x-api-key, anthropic-version\r\n"
    "Connection: close\r\n";

static unsigned long long g_studio_api_errors = 0ULL;

static int send_http_response(ZccSocket s, int code, const char *reason,
                              const char *ctype, const char *body, size_t body_len) {
    char hdr[1024];
    char len_s[32];
    fmt_u64_dec((uint64_t)body_len, len_s, sizeof(len_s));
    int hlen = snprintf(
        hdr, sizeof(hdr),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %s\r\n"
        "Cache-Control: no-cache\r\n"
        "%s\r\n",
        code, reason, ctype, len_s, CORS_HDRS);
    if (hlen <= 0 || (size_t)hlen >= sizeof(hdr)) return -1;
    if (sock_send_all(s, hdr, (size_t)hlen) != 0) return -1;
    if (body && body_len > 0U) {
        if (sock_send_all(s, body, body_len) != 0) return -1;
    }
    return 0;
}

static int send_http_json(ZccSocket s, int code, const char *reason, const char *json) {
    return send_http_response(s, code, reason, "application/json; charset=utf-8",
                              json, json ? strlen(json) : 0U);
}

static int send_http_error(ZccSocket s, int code, const char *reason, const char *err_type, const char *msg) {
    g_studio_api_errors++;
    StrBuf sb;
    memset(&sb, 0, sizeof(sb));
    sb_append(&sb, "{\"error\":{\"message\":");
    sb_append_json_str(&sb, msg ? msg : reason);
    sb_append(&sb, ",\"type\":");
    sb_append_json_str(&sb, err_type ? err_type : "invalid_request_error");
    sb_append(&sb, "}}");
    int rc = send_http_json(s, code, reason, sb.data ? sb.data : "{}");
    sb_free(&sb);
    return rc;
}

static int send_http_chunked_sse_hdr(ZccSocket s) {
    char hdr[512];
    int hlen = snprintf(
        hdr, sizeof(hdr),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/event-stream; charset=utf-8\r\n"
        "Cache-Control: no-cache\r\n"
        "Transfer-Encoding: chunked\r\n"
        "%s\r\n",
        CORS_HDRS);
    if (hlen <= 0 || (size_t)hlen >= sizeof(hdr)) return -1;
    return sock_send_all(s, hdr, (size_t)hlen);
}

static int send_http_chunk(ZccSocket s, const char *data, size_t len) {
    if (len == 0U) return 0;
    char pre[32];
    int plen = snprintf(pre, sizeof(pre), "%x\r\n", (unsigned int)len);
    if (plen <= 0) return -1;
    if (sock_send_all(s, pre, (size_t)plen) != 0) return -1;
    if (sock_send_all(s, data, len) != 0) return -1;
    return sock_send_all(s, "\r\n", 2U);
}

/* Pure C99 RFC 8259 JSON parser */
typedef struct {
    const char *p;
    const char *end;
} JsonCur;

static void j_ws(JsonCur *c) {
    while (c->p < c->end) {
        char ch = *c->p;
        if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n') c->p++;
        else break;
    }
}

static int j_hex_val(char ch) {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return 10 + (ch - 'a');
    if (ch >= 'A' && ch <= 'F') return 10 + (ch - 'A');
    return -1;
}

static int j_parse_hex4(JsonCur *c, uint32_t *out) {
    if (c->end - c->p < 4) return -1;
    uint32_t v = 0U;
    for (int i = 0; i < 4; i++) {
        int d = j_hex_val(c->p[i]);
        if (d < 0) return -1;
        v = (v << 4) | (uint32_t)d;
    }
    c->p += 4;
    *out = v;
    return 0;
}

static int sb_append_utf8(StrBuf *sb, uint32_t cp) {
    char tmp[4];
    if (cp <= 0x7FU) {
        tmp[0] = (char)cp;
        return sb_append_n(sb, tmp, 1U);
    } else if (cp <= 0x7FFU) {
        tmp[0] = (char)(0xC0U | (cp >> 6));
        tmp[1] = (char)(0x80U | (cp & 0x3FU));
        return sb_append_n(sb, tmp, 2U);
    } else if (cp <= 0xFFFFU) {
        tmp[0] = (char)(0xE0U | (cp >> 12));
        tmp[1] = (char)(0x80U | ((cp >> 6) & 0x3FU));
        tmp[2] = (char)(0x80U | (cp & 0x3FU));
        return sb_append_n(sb, tmp, 3U);
    } else if (cp <= 0x10FFFFU) {
        tmp[0] = (char)(0xF0U | (cp >> 18));
        tmp[1] = (char)(0x80U | ((cp >> 12) & 0x3FU));
        tmp[2] = (char)(0x80U | ((cp >> 6) & 0x3FU));
        tmp[3] = (char)(0x80U | (cp & 0x3FU));
        return sb_append_n(sb, tmp, 4U);
    }
    return -1;
}

static char *j_parse_str_alloc(JsonCur *c) {
    j_ws(c);
    if (c->p >= c->end || *c->p != '"') return NULL;
    c->p++;
    StrBuf sb;
    memset(&sb, 0, sizeof(sb));
    sb_append_n(&sb, "", 0U);
    while (c->p < c->end) {
        unsigned char ch = (unsigned char)*c->p++;
        if (ch == '"') return sb.data;
        if (ch < 0x20U) { sb_free(&sb); return NULL; }
        if (ch == '\\') {
            if (c->p >= c->end) { sb_free(&sb); return NULL; }
            char esc = *c->p++;
            if (esc == '"' || esc == '\\' || esc == '/') {
                if (sb_append_char(&sb, esc) != 0) { sb_free(&sb); return NULL; }
            } else if (esc == 'b') { if (sb_append_char(&sb, '\b') != 0) { sb_free(&sb); return NULL; } }
            else if (esc == 'f') { if (sb_append_char(&sb, '\f') != 0) { sb_free(&sb); return NULL; } }
            else if (esc == 'n') { if (sb_append_char(&sb, '\n') != 0) { sb_free(&sb); return NULL; } }
            else if (esc == 'r') { if (sb_append_char(&sb, '\r') != 0) { sb_free(&sb); return NULL; } }
            else if (esc == 't') { if (sb_append_char(&sb, '\t') != 0) { sb_free(&sb); return NULL; } }
            else if (esc == 'u') {
                uint32_t u1 = 0U;
                if (j_parse_hex4(c, &u1) != 0) { sb_free(&sb); return NULL; }
                if (u1 >= 0xD800U && u1 <= 0xDBFFU) {
                    if (c->end - c->p >= 6 && c->p[0] == '\\' && c->p[1] == 'u') {
                        c->p += 2;
                        uint32_t u2 = 0U;
                        if (j_parse_hex4(c, &u2) != 0) { sb_free(&sb); return NULL; }
                        if (u2 >= 0xDC00U && u2 <= 0xDFFFU) {
                            u1 = 0x10000U + (((u1 - 0xD800U) << 10) | (u2 - 0xDC00U));
                        }
                    }
                }
                if (sb_append_utf8(&sb, u1) != 0) { sb_free(&sb); return NULL; }
            } else {
                sb_free(&sb);
                return NULL;
            }
        } else {
            if (sb_append_char(&sb, (char)ch) != 0) { sb_free(&sb); return NULL; }
        }
    }
    sb_free(&sb);
    return NULL;
}

static int j_skip_value(JsonCur *c, int depth);

static int j_parse_num(JsonCur *c, double *out) {
    j_ws(c);
    if (c->p >= c->end) return -1;
    const char *start = c->p;
    if (*c->p == '-') c->p++;
    if (c->p >= c->end || *c->p < '0' || *c->p > '9') return -1;
    if (*c->p == '0') c->p++;
    else { while (c->p < c->end && *c->p >= '0' && *c->p <= '9') c->p++; }
    if (c->p < c->end && *c->p == '.') {
        c->p++;
        if (c->p >= c->end || *c->p < '0' || *c->p > '9') return -1;
        while (c->p < c->end && *c->p >= '0' && *c->p <= '9') c->p++;
    }
    if (c->p < c->end && (*c->p == 'e' || *c->p == 'E')) {
        c->p++;
        if (c->p < c->end && (*c->p == '+' || *c->p == '-')) c->p++;
        if (c->p >= c->end || *c->p < '0' || *c->p > '9') return -1;
        while (c->p < c->end && *c->p >= '0' && *c->p <= '9') c->p++;
    }
    size_t n = (size_t)(c->p - start);
    char buf[64];
    if (n >= sizeof(buf)) n = sizeof(buf) - 1U;
    memcpy(buf, start, n);
    buf[n] = '\0';
    char *ep = NULL;
    double v = strtod(buf, &ep);
    if (ep == buf) return -1;
    *out = v;
    return 0;
}

static int j_parse_bool(JsonCur *c, int *out) {
    j_ws(c);
    if (c->end - c->p >= 4 && memcmp(c->p, "true", 4U) == 0) {
        c->p += 4; *out = 1; return 0;
    }
    if (c->end - c->p >= 5 && memcmp(c->p, "false", 5U) == 0) {
        c->p += 5; *out = 0; return 0;
    }
    return -1;
}

static int j_skip_value(JsonCur *c, int depth) {
    if (depth > 32) return -1;
    j_ws(c);
    if (c->p >= c->end) return -1;
    char ch = *c->p;
    if (ch == '"') {
        char *s = j_parse_str_alloc(c);
        if (!s) return -1;
        free(s);
        return 0;
    } else if (ch == '{') {
        c->p++;
        j_ws(c);
        if (c->p < c->end && *c->p == '}') { c->p++; return 0; }
        for (;;) {
            char *k = j_parse_str_alloc(c);
            if (!k) return -1;
            free(k);
            j_ws(c);
            if (c->p >= c->end || *c->p != ':') return -1;
            c->p++;
            if (j_skip_value(c, depth + 1) != 0) return -1;
            j_ws(c);
            if (c->p >= c->end) return -1;
            if (*c->p == ',') { c->p++; continue; }
            if (*c->p == '}') { c->p++; return 0; }
            return -1;
        }
    } else if (ch == '[') {
        c->p++;
        j_ws(c);
        if (c->p < c->end && *c->p == ']') { c->p++; return 0; }
        for (;;) {
            if (j_skip_value(c, depth + 1) != 0) return -1;
            j_ws(c);
            if (c->p >= c->end) return -1;
            if (*c->p == ',') { c->p++; continue; }
            if (*c->p == ']') { c->p++; return 0; }
            return -1;
        }
    } else if (ch == 't') {
        if (c->end - c->p < 4 || memcmp(c->p, "true", 4U) != 0) return -1;
        c->p += 4;
        return 0;
    } else if (ch == 'f') {
        if (c->end - c->p < 5 || memcmp(c->p, "false", 5U) != 0) return -1;
        c->p += 5;
        return 0;
    } else if (ch == 'n') {
        if (c->end - c->p < 4 || memcmp(c->p, "null", 4U) != 0) return -1;
        c->p += 4;
        return 0;
    } else if (ch == '-' || (ch >= '0' && ch <= '9')) {
        double dv = 0.0;
        return j_parse_num(c, &dv);
    }
    return -1;
}

typedef struct {
    char *role;
    char *content;
} OpenAiMsg;

typedef struct {
    OpenAiMsg *msgs;
    size_t n_msgs;
    int has_messages;
    char *prompt;
    int has_prompt;
    char *system_text;
    char *model;
    char *adapter;
    char *stops[8];
    int n_stops;
    char *response_format_type;
    int stream;
    int include_usage;
    int logprobs;
    int top_logprobs;
    float temperature;
    float top_p;
    int top_k;
    float min_p;
    float repeat_penalty;
    float presence_penalty;
    float frequency_penalty;
    float lora_scale;
    int has_lora_scale;
    int max_tokens;
    uint32_t seed;
} OpenAiReq;

static void openai_req_free(OpenAiReq *r) {
    if (!r) return;
    for (size_t i = 0U; i < r->n_msgs; i++) {
        if (r->msgs[i].role) free(r->msgs[i].role);
        if (r->msgs[i].content) free(r->msgs[i].content);
    }
    if (r->msgs) free(r->msgs);
    for (int si = 0; si < r->n_stops; si++) {
        if (r->stops[si]) free(r->stops[si]);
    }
    if (r->prompt) free(r->prompt);
    if (r->system_text) free(r->system_text);
    if (r->model) free(r->model);
    if (r->adapter) free(r->adapter);
    if (r->response_format_type) free(r->response_format_type);
    memset(r, 0, sizeof(*r));
}

static char *j_parse_content_text_alloc(JsonCur *c) {
    j_ws(c);
    if (c->p >= c->end) return NULL;
    if (*c->p == '"') return j_parse_str_alloc(c);
    if (*c->p == '[') {
        c->p++;
        j_ws(c);
        StrBuf out;
        memset(&out, 0, sizeof(out));
        sb_append_n(&out, "", 0U);
        if (c->p < c->end && *c->p == ']') { c->p++; return out.data; }
        for (;;) {
            j_ws(c);
            if (c->p >= c->end || *c->p != '{') { sb_free(&out); return NULL; }
            c->p++;
            j_ws(c);
            if (c->p < c->end && *c->p == '}') {
                c->p++;
            } else {
                for (;;) {
                    char *k = j_parse_str_alloc(c);
                    if (!k) { sb_free(&out); return NULL; }
                    j_ws(c);
                    if (c->p >= c->end || *c->p != ':') { free(k); sb_free(&out); return NULL; }
                    c->p++;
                    if (strcmp(k, "text") == 0) {
                        char *txt = j_parse_str_alloc(c);
                        if (!txt) { free(k); sb_free(&out); return NULL; }
                        sb_append(&out, txt);
                        free(txt);
                    } else {
                        if (j_skip_value(c, 1) != 0) { free(k); sb_free(&out); return NULL; }
                    }
                    free(k);
                    j_ws(c);
                    if (c->p >= c->end) { sb_free(&out); return NULL; }
                    if (*c->p == ',') { c->p++; continue; }
                    if (*c->p == '}') { c->p++; break; }
                    sb_free(&out);
                    return NULL;
                }
            }
            j_ws(c);
            if (c->p >= c->end) { sb_free(&out); return NULL; }
            if (*c->p == ',') { c->p++; continue; }
            if (*c->p == ']') { c->p++; return out.data; }
            sb_free(&out);
            return NULL;
        }
    }
    return NULL;
}

static int j_parse_messages_array(JsonCur *c, OpenAiReq *req) {
    j_ws(c);
    if (c->p >= c->end || *c->p != '[') return -1;
    c->p++;
    j_ws(c);
    if (c->p < c->end && *c->p == ']') { c->p++; return 0; }
    for (;;) {
        j_ws(c);
        if (c->p >= c->end || *c->p != '{') return -1;
        c->p++;
        j_ws(c);
        char *role = NULL;
        char *content = NULL;
        if (c->p < c->end && *c->p == '}') {
            c->p++;
        } else {
            for (;;) {
                char *k = j_parse_str_alloc(c);
                if (!k) { if (role) free(role); if (content) free(content); return -1; }
                j_ws(c);
                if (c->p >= c->end || *c->p != ':') {
                    free(k); if (role) free(role); if (content) free(content); return -1;
                }
                c->p++;
                if (strcmp(k, "role") == 0) {
                    char *rv = j_parse_str_alloc(c);
                    if (!rv) { free(k); if (role) free(role); if (content) free(content); return -1; }
                    if (role) free(role);
                    role = rv;
                } else if (strcmp(k, "content") == 0) {
                    char *cv = j_parse_content_text_alloc(c);
                    if (!cv) { free(k); if (role) free(role); if (content) free(content); return -1; }
                    if (content) free(content);
                    content = cv;
                } else {
                    if (j_skip_value(c, 1) != 0) {
                        free(k); if (role) free(role); if (content) free(content); return -1;
                    }
                }
                free(k);
                j_ws(c);
                if (c->p >= c->end) { if (role) free(role); if (content) free(content); return -1; }
                if (*c->p == ',') { c->p++; continue; }
                if (*c->p == '}') { c->p++; break; }
                if (role) free(role); if (content) free(content);
                return -1;
            }
        }
        if (!role || !content) {
            if (role) free(role);
            if (content) free(content);
            return -1;
        }
        OpenAiMsg *nm = (OpenAiMsg *)realloc(req->msgs, (req->n_msgs + 1U) * sizeof(OpenAiMsg));
        if (!nm) { free(role); free(content); return -1; }
        req->msgs = nm;
        req->msgs[req->n_msgs].role = role;
        req->msgs[req->n_msgs].content = content;
        req->n_msgs++;
        j_ws(c);
        if (c->p >= c->end) return -1;
        if (*c->p == ',') { c->p++; continue; }
        if (*c->p == ']') { c->p++; return 0; }
        return -1;
    }
}

static int j_parse_stop_field(JsonCur *c, OpenAiReq *req) {
    j_ws(c);
    if (c->p >= c->end) return -1;
    if (c->end - c->p >= 4 && memcmp(c->p, "null", 4U) == 0) { c->p += 4; return 0; }
    if (*c->p == '"') {
        char *s = j_parse_str_alloc(c);
        if (!s) return -1;
        if (s[0] != '\0' && req->n_stops < 8) req->stops[req->n_stops++] = s;
        else free(s);
        return 0;
    }
    if (*c->p == '[') {
        c->p++;
        j_ws(c);
        if (c->p < c->end && *c->p == ']') { c->p++; return 0; }
        for (;;) {
            char *s = j_parse_str_alloc(c);
            if (!s) return -1;
            if (s[0] != '\0' && req->n_stops < 8) req->stops[req->n_stops++] = s;
            else free(s);
            j_ws(c);
            if (c->p >= c->end) return -1;
            if (*c->p == ',') { c->p++; continue; }
            if (*c->p == ']') { c->p++; return 0; }
            return -1;
        }
    }
    return -1;
}

static int j_parse_stream_options(JsonCur *c, OpenAiReq *req) {
    j_ws(c);
    if (c->p >= c->end) return -1;
    if (c->end - c->p >= 4 && memcmp(c->p, "null", 4U) == 0) { c->p += 4; return 0; }
    if (*c->p != '{') return -1;
    c->p++;
    j_ws(c);
    if (c->p < c->end && *c->p == '}') { c->p++; return 0; }
    for (;;) {
        char *k = j_parse_str_alloc(c);
        if (!k) return -1;
        j_ws(c);
        if (c->p >= c->end || *c->p != ':') { free(k); return -1; }
        c->p++;
        if (strcmp(k, "include_usage") == 0) {
            int bv = 0;
            if (j_parse_bool(c, &bv) != 0) { free(k); return -1; }
            req->include_usage = bv;
        } else {
            if (j_skip_value(c, 1) != 0) { free(k); return -1; }
        }
        free(k);
        j_ws(c);
        if (c->p >= c->end) return -1;
        if (*c->p == ',') { c->p++; continue; }
        if (*c->p == '}') { c->p++; return 0; }
        return -1;
    }
}

static int j_parse_response_format(JsonCur *c, OpenAiReq *req) {
    j_ws(c);
    if (c->p >= c->end) return -1;
    if (c->end - c->p >= 4 && memcmp(c->p, "null", 4U) == 0) { c->p += 4; return 0; }
    if (*c->p != '{') return -1;
    c->p++;
    j_ws(c);
    if (c->p < c->end && *c->p == '}') { c->p++; return 0; }
    for (;;) {
        char *k = j_parse_str_alloc(c);
        if (!k) return -1;
        j_ws(c);
        if (c->p >= c->end || *c->p != ':') { free(k); return -1; }
        c->p++;
        if (strcmp(k, "type") == 0) {
            char *tv = j_parse_str_alloc(c);
            if (!tv) { free(k); return -1; }
            if (req->response_format_type) free(req->response_format_type);
            req->response_format_type = tv;
        } else {
            if (j_skip_value(c, 1) != 0) { free(k); return -1; }
        }
        free(k);
        j_ws(c);
        if (c->p >= c->end) return -1;
        if (*c->p == ',') { c->p++; continue; }
        if (*c->p == '}') { c->p++; return 0; }
        return -1;
    }
}

static int parse_openai_request(const char *body, size_t body_len, OpenAiReq *req) {
    memset(req, 0, sizeof(*req));
    req->temperature = 0.7f;
    req->top_p = 0.9f;
    req->top_k = 40;
    req->min_p = 0.0f;
    req->repeat_penalty = 1.1f;
    req->presence_penalty = 0.0f;
    req->frequency_penalty = 0.0f;
    req->max_tokens = 128;
    req->seed = 42U;

    JsonCur cur;
    cur.p = body;
    cur.end = body + body_len;
    j_ws(&cur);
    if (cur.p >= cur.end || *cur.p != '{') return -1;
    cur.p++;
    j_ws(&cur);
    if (cur.p < cur.end && *cur.p == '}') {
        cur.p++;
        j_ws(&cur);
        return (cur.p == cur.end) ? 0 : -1;
    }
    for (;;) {
        char *k = j_parse_str_alloc(&cur);
        if (!k) goto fail;
        j_ws(&cur);
        if (cur.p >= cur.end || *cur.p != ':') { free(k); goto fail; }
        cur.p++;
        j_ws(&cur);
        if (strcmp(k, "messages") == 0) {
            req->has_messages = 1;
            if (j_parse_messages_array(&cur, req) != 0) { free(k); goto fail; }
        } else if (strcmp(k, "prompt") == 0) {
            char *pv = j_parse_str_alloc(&cur);
            if (!pv) { free(k); goto fail; }
            if (req->prompt) free(req->prompt);
            req->prompt = pv;
            req->has_prompt = 1;
        } else if (strcmp(k, "input") == 0) {
            j_ws(&cur);
            if (cur.p < cur.end && *cur.p == '[') {
                req->has_messages = 1;
                if (j_parse_messages_array(&cur, req) != 0) { free(k); goto fail; }
            } else {
                char *pv = j_parse_str_alloc(&cur);
                if (!pv) { free(k); goto fail; }
                if (req->prompt) free(req->prompt);
                req->prompt = pv;
                req->has_prompt = 1;
            }
        } else if (strcmp(k, "system") == 0 || strcmp(k, "instructions") == 0) {
            char *sv = j_parse_content_text_alloc(&cur);
            if (!sv) { free(k); goto fail; }
            if (req->system_text) free(req->system_text);
            req->system_text = sv;
        } else if (strcmp(k, "model") == 0) {
            char *mv = j_parse_str_alloc(&cur);
            if (!mv) { free(k); goto fail; }
            if (req->model) free(req->model);
            req->model = mv;
        } else if (strcmp(k, "adapter") == 0) {
            char *av = j_parse_str_alloc(&cur);
            if (!av) { free(k); goto fail; }
            if (req->adapter) free(req->adapter);
            req->adapter = av;
        } else if (strcmp(k, "stop") == 0 || strcmp(k, "stop_sequences") == 0) {
            if (j_parse_stop_field(&cur, req) != 0) { free(k); goto fail; }
        } else if (strcmp(k, "stream") == 0) {
            int bv = 0;
            if (j_parse_bool(&cur, &bv) != 0) { free(k); goto fail; }
            req->stream = bv;
        } else if (strcmp(k, "stream_options") == 0) {
            if (j_parse_stream_options(&cur, req) != 0) { free(k); goto fail; }
        } else if (strcmp(k, "response_format") == 0) {
            if (j_parse_response_format(&cur, req) != 0) { free(k); goto fail; }
        } else if (strcmp(k, "logprobs") == 0) {
            j_ws(&cur);
            if (cur.p < cur.end && (*cur.p == 't' || *cur.p == 'f')) {
                int bv = 0;
                if (j_parse_bool(&cur, &bv) != 0) { free(k); goto fail; }
                req->logprobs = bv;
            } else {
                double v = 0.0;
                if (j_parse_num(&cur, &v) != 0 || v < 0.0 || v > 5.0) { free(k); goto fail; }
                req->logprobs = (v > 0.0) ? 1 : 0;
                if (req->top_logprobs == 0 && v > 0.0) req->top_logprobs = (int)v;
            }
        } else if (strcmp(k, "top_logprobs") == 0) {
            double v = 0.0;
            if (j_parse_num(&cur, &v) != 0 || v < 0.0 || v > 5.0) { free(k); goto fail; }
            req->top_logprobs = (int)v;
            if (v > 0.0) req->logprobs = 1;
        } else if (strcmp(k, "temperature") == 0) {
            double v = 0.0;
            if (j_parse_num(&cur, &v) != 0 || !(v >= 0.0 && v <= 100.0)) { free(k); goto fail; }
            req->temperature = (float)v;
        } else if (strcmp(k, "top_p") == 0) {
            double v = 0.0;
            if (j_parse_num(&cur, &v) != 0 || !(v > 0.0 && v <= 1.0)) { free(k); goto fail; }
            req->top_p = (float)v;
        } else if (strcmp(k, "top_k") == 0) {
            double v = 0.0;
            if (j_parse_num(&cur, &v) != 0 || v < 1.0 || v > 1000000.0) { free(k); goto fail; }
            req->top_k = (int)v;
        } else if (strcmp(k, "min_p") == 0) {
            double v = 0.0;
            if (j_parse_num(&cur, &v) != 0 || !(v >= 0.0 && v <= 1.0)) { free(k); goto fail; }
            req->min_p = (float)v;
        } else if (strcmp(k, "repeat_penalty") == 0) {
            double v = 0.0;
            if (j_parse_num(&cur, &v) != 0 || !(v > 0.0 && v <= 100.0)) { free(k); goto fail; }
            req->repeat_penalty = (float)v;
        } else if (strcmp(k, "presence_penalty") == 0) {
            double v = 0.0;
            if (j_parse_num(&cur, &v) != 0 || !(v >= -2.0 && v <= 2.0)) { free(k); goto fail; }
            req->presence_penalty = (float)v;
        } else if (strcmp(k, "frequency_penalty") == 0) {
            double v = 0.0;
            if (j_parse_num(&cur, &v) != 0 || !(v >= -2.0 && v <= 2.0)) { free(k); goto fail; }
            req->frequency_penalty = (float)v;
        } else if (strcmp(k, "lora_scale") == 0) {
            double v = 0.0;
            if (j_parse_num(&cur, &v) != 0 || !(v >= 0.0 && v <= 4.0)) { free(k); goto fail; }
            req->lora_scale = (float)v;
            req->has_lora_scale = 1;
        } else if (strcmp(k, "max_tokens") == 0 || strcmp(k, "max_completion_tokens") == 0 || strcmp(k, "max_output_tokens") == 0) {
            double v = 0.0;
            if (j_parse_num(&cur, &v) != 0 || v < 1.0 || v > 65536.0) { free(k); goto fail; }
            req->max_tokens = (int)v;
        } else if (strcmp(k, "seed") == 0) {
            double v = 0.0;
            if (j_parse_num(&cur, &v) != 0 || v < 0.0) { free(k); goto fail; }
            req->seed = (uint32_t)v;
        } else {
            if (j_skip_value(&cur, 0) != 0) { free(k); goto fail; }
        }
        free(k);
        j_ws(&cur);
        if (cur.p >= cur.end) goto fail;
        if (*cur.p == ',') { cur.p++; continue; }
        if (*cur.p == '}') {
            cur.p++;
            j_ws(&cur);
            return (cur.p == cur.end) ? 0 : -1;
        }
        goto fail;
    }
fail:
    openai_req_free(req);
    return -1;
}

typedef struct {
    char *name;
    char *path;
    char *sft_text;
    char *save_path;
    int rank;
    float alpha;
    float lr;
    float scale;
    int has_scale;
    int steps;
    uint32_t seed;
    int chat;
    int fp32_matmul;
    int stream;
    int activate;
} LoraCtrlReq;

static void lora_ctrl_req_free(LoraCtrlReq *r) {
    if (!r) return;
    if (r->name) free(r->name);
    if (r->path) free(r->path);
    if (r->sft_text) free(r->sft_text);
    if (r->save_path) free(r->save_path);
    memset(r, 0, sizeof(*r));
}

static int parse_lora_ctrl_request(const char *body, size_t body_len, LoraCtrlReq *r) {
    memset(r, 0, sizeof(*r));
    r->rank = 8;
    r->alpha = 16.0f;
    r->lr = 5e-4f;
    r->steps = 80;
    r->seed = 42U;
    r->chat = 1;
    r->fp32_matmul = 0;
    r->stream = 1;
    r->activate = 1;

    JsonCur cur;
    cur.p = body;
    cur.end = body + body_len;
    j_ws(&cur);
    if (cur.p >= cur.end || *cur.p != '{') return -1;
    cur.p++;
    j_ws(&cur);
    if (cur.p < cur.end && *cur.p == '}') {
        cur.p++;
        j_ws(&cur);
        return (cur.p == cur.end) ? 0 : -1;
    }
    for (;;) {
        char *k = j_parse_str_alloc(&cur);
        if (!k) goto fail;
        j_ws(&cur);
        if (cur.p >= cur.end || *cur.p != ':') { free(k); goto fail; }
        cur.p++;
        j_ws(&cur);
        if (strcmp(k, "name") == 0) {
            char *v = j_parse_str_alloc(&cur);
            if (!v) { free(k); goto fail; }
            if (r->name) free(r->name);
            r->name = v;
        } else if (strcmp(k, "path") == 0) {
            char *v = j_parse_str_alloc(&cur);
            if (!v) { free(k); goto fail; }
            if (r->path) free(r->path);
            r->path = v;
        } else if (strcmp(k, "sft_text") == 0) {
            char *v = j_parse_str_alloc(&cur);
            if (!v) { free(k); goto fail; }
            if (r->sft_text) free(r->sft_text);
            r->sft_text = v;
        } else if (strcmp(k, "save_path") == 0) {
            j_ws(&cur);
            if (cur.end - cur.p >= 4 && memcmp(cur.p, "null", 4U) == 0) {
                cur.p += 4;
            } else {
                char *v = j_parse_str_alloc(&cur);
                if (!v) { free(k); goto fail; }
                if (r->save_path) free(r->save_path);
                r->save_path = (v[0] != '\0') ? v : (free(v), (char *)NULL);
            }
        } else if (strcmp(k, "rank") == 0) {
            double v = 0.0;
            if (j_parse_num(&cur, &v) != 0) { free(k); goto fail; }
            r->rank = (int)v;
        } else if (strcmp(k, "alpha") == 0) {
            double v = 0.0;
            if (j_parse_num(&cur, &v) != 0) { free(k); goto fail; }
            r->alpha = (float)v;
        } else if (strcmp(k, "lr") == 0) {
            double v = 0.0;
            if (j_parse_num(&cur, &v) != 0) { free(k); goto fail; }
            r->lr = (float)v;
        } else if (strcmp(k, "scale") == 0) {
            double v = 0.0;
            if (j_parse_num(&cur, &v) != 0) { free(k); goto fail; }
            r->scale = (float)v;
            r->has_scale = 1;
        } else if (strcmp(k, "steps") == 0) {
            double v = 0.0;
            if (j_parse_num(&cur, &v) != 0) { free(k); goto fail; }
            r->steps = (int)v;
        } else if (strcmp(k, "seed") == 0) {
            double v = 0.0;
            if (j_parse_num(&cur, &v) != 0 || v < 0.0) { free(k); goto fail; }
            r->seed = (uint32_t)v;
        } else if (strcmp(k, "chat") == 0) {
            int bv = 1;
            if (j_parse_bool(&cur, &bv) != 0) { free(k); goto fail; }
            r->chat = bv;
        } else if (strcmp(k, "fp32_matmul") == 0) {
            int bv = 0;
            if (j_parse_bool(&cur, &bv) != 0) { free(k); goto fail; }
            r->fp32_matmul = bv;
        } else if (strcmp(k, "stream") == 0) {
            int bv = 1;
            if (j_parse_bool(&cur, &bv) != 0) { free(k); goto fail; }
            r->stream = bv;
        } else if (strcmp(k, "activate") == 0) {
            int bv = 1;
            if (j_parse_bool(&cur, &bv) != 0) { free(k); goto fail; }
            r->activate = bv;
        } else {
            if (j_skip_value(&cur, 0) != 0) { free(k); goto fail; }
        }
        free(k);
        j_ws(&cur);
        if (cur.p >= cur.end) goto fail;
        if (*cur.p == ',') { cur.p++; continue; }
        if (*cur.p == '}') {
            cur.p++;
            j_ws(&cur);
            return (cur.p == cur.end) ? 0 : -1;
        }
        goto fail;
    }
fail:
    lora_ctrl_req_free(r);
    return -1;
}

static char *format_openai_messages_ex_alloc(const LlamaModel *m, const char *sys_text,
                                             const OpenAiMsg *msgs, size_t n_msgs) {
    int is_chatml = (strcmp(m->arch, "qwen2") == 0) ||
                    (tok_lookup(&m->tok, "<|im_start|>", 12U) >= 0);
    StrBuf sb;
    memset(&sb, 0, sizeof(sb));
    sb_append_n(&sb, "", 0U);
    if (is_chatml) {
        if (sys_text && sys_text[0] != '\0') {
            sb_append(&sb, "<|im_start|>system\n");
            sb_append(&sb, sys_text);
            sb_append(&sb, "<|im_end|>\n");
        }
        for (size_t i = 0U; i < n_msgs; i++) {
            sb_append(&sb, "<|im_start|>");
            sb_append(&sb, msgs[i].role);
            sb_append(&sb, "\n");
            sb_append(&sb, msgs[i].content);
            sb_append(&sb, "<|im_end|>\n");
        }
        sb_append(&sb, "<|im_start|>assistant\n");
    } else if (n_msgs == 1U && strcmp(msgs[0].role, "user") == 0 && (!sys_text || sys_text[0] == '\0')) {
        sb_append(&sb, "[INST] ");
        sb_append(&sb, msgs[0].content);
        sb_append(&sb, " [/INST]");
    } else {
        if (sys_text && sys_text[0] != '\0') {
            sb_append(&sb, "[INST] <<SYS>>\n");
            sb_append(&sb, sys_text);
            sb_append(&sb, "\n<</SYS>> [/INST] ");
        }
        for (size_t i = 0U; i < n_msgs; i++) {
            if (strcmp(msgs[i].role, "system") == 0) {
                sb_append(&sb, "[INST] <<SYS>>\n");
                sb_append(&sb, msgs[i].content);
                sb_append(&sb, "\n<</SYS>> [/INST] ");
            } else if (strcmp(msgs[i].role, "assistant") == 0) {
                sb_append(&sb, msgs[i].content);
                sb_append(&sb, " </s> ");
            } else {
                sb_append(&sb, "[INST] ");
                sb_append(&sb, msgs[i].content);
                sb_append(&sb, " [/INST]");
            }
        }
    }
    return sb.data;
}

static char *format_openai_messages_alloc(const LlamaModel *m, const OpenAiMsg *msgs, size_t n_msgs) {
    return format_openai_messages_ex_alloc(m, NULL, msgs, n_msgs);
}

/* Studio State & Adapter Registry */
typedef struct {
    char name[64];
    char path[512];
    LoraLayer *ls;
    unsigned int rank;
    float alpha;
    float scale_mult;
    uint64_t params;
    uint32_t fnv;
} StudioAdapter;

typedef struct {
    void *ctx;
    void *mod_llm;
    void *mod_lora;
    void *ev0;
    void *ev1;
    LlamaModel model;
    GpuActivations act;
    uint64_t d_inf_h;
    int use_tc;
    size_t kv_bytes;
    unsigned int eff_max_seq;
    int kv_type;
    int no_cuda_graph;
    LoraDecodePlan decode_plan;
    char model_id[128];
    char gpu_name[128];
    int gpu_cc_major;
    int gpu_cc_minor;
    int gpu_sm_count;
    int cuda_driver_ver;
    size_t vram_total_bytes;
    long long started_at;
    char adapter_dir[512];
    StudioAdapter adapters[ZCC_MAX_STUDIO_ADAPTERS];
    int n_adapters;
    int active_idx;
    float *h_logits;
    float *work_logits;
    float *h_probs;
    SampleCand *cands;
    uint32_t *hist;
    int *gen_ids;
    char *gen_text;
    size_t gen_text_cap;
    unsigned long long req_counter;
    unsigned long long total_embeddings;
    double last_tps;
    int last_gen_tokens;
    uint32_t last_prompt_tokens;
    double last_elapsed_ms;
    unsigned long long total_gen_tokens;
    char api_key[256];
    float *emb_buf;
} ZccStudioState;

static int is_valid_adapter_name(const char *s) {
    if (!s || !s[0]) return 0;
    size_t n = strlen(s);
    if (n < 1U || n > 48U) return 0;
    if (strcmp(s, "base") == 0 || strcmp(s, "none") == 0 ||
        strcmp(s, ".") == 0 || strcmp(s, "..") == 0) {
        return 0;
    }
    for (size_t i = 0U; i < n; i++) {
        char ch = s[i];
        int ok = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
                 (ch >= '0' && ch <= '9') || ch == '.' || ch == '_' || ch == '-';
        if (!ok) return 0;
    }
    return 1;
}

static int find_adapter_idx(const ZccStudioState *st, const char *name) {
    if (!st || !name || !name[0]) return -1;
    for (int i = 0; i < st->n_adapters; i++) {
        if (strcmp(st->adapters[i].name, name) == 0) return i;
    }
    return -1;
}

static const char *active_adapter_name_str(const ZccStudioState *st) {
    if (st->active_idx >= 0 && st->active_idx < st->n_adapters) {
        return st->adapters[st->active_idx].name;
    }
    return "base";
}

static int resolve_req_adapter_idx(const ZccStudioState *st, const OpenAiReq *req, int *out_not_found) {
    *out_not_found = 0;
    if (req->adapter && req->adapter[0]) {
        if (strcmp(req->adapter, "base") == 0 || strcmp(req->adapter, "none") == 0) return -1;
        int idx = find_adapter_idx(st, req->adapter);
        if (idx < 0) *out_not_found = 1;
        return idx;
    }
    if (req->model && req->model[0]) {
        const char *colon = strchr(req->model, ':');
        if (colon && colon[1]) {
            const char *suf = colon + 1;
            if (strcmp(suf, "base") == 0 || strcmp(suf, "none") == 0) return -1;
            int idx = find_adapter_idx(st, suf);
            if (idx < 0) *out_not_found = 1;
            return idx;
        }
        int exact = find_adapter_idx(st, req->model);
        if (exact >= 0) return exact;
    }
    return st->active_idx;
}

static int upsert_adapter(
    ZccStudioState *st,
    const char *name,
    const char *path,
    LoraLayer *ls,
    unsigned int rank,
    float alpha,
    uint32_t fnv,
    int activate
) {
    int slot = find_adapter_idx(st, name);
    if (slot < 0) {
        if (st->n_adapters >= ZCC_MAX_STUDIO_ADAPTERS) return -1;
        slot = st->n_adapters++;
    } else if (st->adapters[slot].ls) {
        lora_decode_plan_invalidate(&st->decode_plan);
        free_lora_layers(st->adapters[slot].ls, st->model.n_layers);
        st->adapters[slot].ls = NULL;
    }
    StudioAdapter *ad = &st->adapters[slot];
    memset(ad, 0, sizeof(*ad));
    snprintf(ad->name, sizeof(ad->name), "%s", name);
    if (path && path[0]) snprintf(ad->path, sizeof(ad->path), "%s", path);
    ad->ls = ls;
    ad->rank = rank;
    ad->alpha = alpha;
    ad->scale_mult = 1.0f;
    ad->params = lora_payload_byte_size(&st->model, rank) / 4ULL;
    ad->fnv = fnv;
    if (activate) st->active_idx = slot;
    return slot;
}

#define STUDIO_API_COMPLETION 0
#define STUDIO_API_CHAT       1
#define STUDIO_API_ANTHROPIC  2
#define STUDIO_API_RESPONSES  3

typedef struct {
    int tok;
    float lp;
} StudioLpAlt;

typedef struct {
    int chosen_tok;
    float chosen_lp;
    int n_alt;
    StudioLpAlt alts[5];
} StudioLpStep;

typedef struct {
    ZccSocket sock;
    int api_mode;
    int sent_role;
    int disconnected;
    long long created;
    const char *req_id;
    const char *model_id;
} SseStreamCtx;

static void sse_emit_raw_chunk(SseStreamCtx *sc, const char *data, size_t len) {
    if (!sc || sc->disconnected || len == 0U) return;
    if (send_http_chunk(sc->sock, data, len) != 0) {
        sc->disconnected = 1;
        g_zcc_llm_failed = 1;
    }
}

static void sse_emit_frame(SseStreamCtx *sc, const char *piece, const char *finish_reason) {
    if (!sc || sc->disconnected) return;
    StrBuf sb;
    memset(&sb, 0, sizeof(sb));
    if (sc->api_mode == STUDIO_API_ANTHROPIC) {
        if (piece && piece[0] != '\0') {
            sb_append(&sb, "event: content_block_delta\ndata: {\"type\":\"content_block_delta\",\"index\":0,\"delta\":{\"type\":\"text_delta\",\"text\":");
            sb_append_json_str(&sb, piece);
            sb_append(&sb, "}}\n\n");
            sse_emit_raw_chunk(sc, sb.data, sb.len);
        }
        sb_free(&sb);
        return;
    }
    if (sc->api_mode == STUDIO_API_RESPONSES) {
        if (piece && piece[0] != '\0') {
            sb_append(&sb, "event: response.output_text.delta\ndata: {\"type\":\"response.output_text.delta\",\"delta\":");
            sb_append_json_str(&sb, piece);
            sb_append(&sb, "}\n\n");
            sse_emit_raw_chunk(sc, sb.data, sb.len);
        }
        sb_free(&sb);
        return;
    }

    sb_append(&sb, "data: {\"id\":");
    sb_append_json_str(&sb, sc->req_id);
    if (sc->api_mode == STUDIO_API_CHAT) {
        sb_append(&sb, ",\"object\":\"chat.completion.chunk\"");
    } else {
        sb_append(&sb, ",\"object\":\"text_completion\"");
    }
    char cr_s[32];
    fmt_u64_dec((uint64_t)(sc->created > 0LL ? sc->created : 0LL), cr_s, sizeof(cr_s));
    sb_append(&sb, ",\"created\":");
    sb_append(&sb, cr_s);
    sb_append(&sb, ",\"model\":");
    sb_append_json_str(&sb, sc->model_id);
    if (sc->api_mode == STUDIO_API_CHAT) {
        sb_append(&sb, ",\"choices\":[{\"index\":0,\"delta\":{");
        int need_comma = 0;
        if (!sc->sent_role) {
            sb_append(&sb, "\"role\":\"assistant\"");
            sc->sent_role = 1;
            need_comma = 1;
        }
        if (piece && piece[0] != '\0') {
            if (need_comma) sb_append_char(&sb, ',');
            sb_append(&sb, "\"content\":");
            sb_append_json_str(&sb, piece);
        }
        sb_append(&sb, "},\"finish_reason\":");
    } else {
        sb_append(&sb, ",\"choices\":[{\"index\":0,\"text\":");
        sb_append_json_str(&sb, piece ? piece : "");
        sb_append(&sb, ",\"logprobs\":null,\"finish_reason\":");
    }
    if (finish_reason) sb_append_json_str(&sb, finish_reason);
    else sb_append(&sb, "null");
    sb_append(&sb, "}]}\n\n");
    sse_emit_raw_chunk(sc, sb.data, sb.len);
    sb_free(&sb);
}

static void sse_token_cb(const char *piece_utf8, int token_id, void *userdata) {
    (void)token_id;
    SseStreamCtx *sc = (SseStreamCtx *)userdata;
    if (!sc || sc->disconnected) return;
    if (!sc->sent_role && sc->api_mode == STUDIO_API_CHAT) {
        sse_emit_frame(sc, piece_utf8, NULL);
    } else if (piece_utf8 && piece_utf8[0] != '\0') {
        sse_emit_frame(sc, piece_utf8, NULL);
    }
}

static void apply_presence_freq_penalties(
    float *logits,
    uint32_t V,
    const int *gen_ids,
    int gen_count,
    float pres_pen,
    float freq_pen
) {
    if (gen_count <= 0 || (pres_pen == 0.0f && freq_pen == 0.0f)) return;
    for (int i = 0; i < gen_count; i++) {
        int tid = gen_ids[i];
        if (tid < 0 || (uint32_t)tid >= V) continue;
        int seen = 0;
        for (int p = 0; p < i; p++) {
            if (gen_ids[p] == tid) { seen = 1; break; }
        }
        if (seen) continue;
        int cnt = 1;
        for (int q = i + 1; q < gen_count; q++) {
            if (gen_ids[q] == tid) cnt++;
        }
        logits[tid] -= (pres_pen + freq_pen * (float)cnt);
    }
}

static double compute_logits_logz(const float *logits, uint32_t V) {
    double mx = (double)logits[0];
    for (uint32_t i = 1U; i < V; i++) {
        if ((double)logits[i] > mx) mx = (double)logits[i];
    }
    double sum = 0.0;
    for (uint32_t i = 0U; i < V; i++) {
        sum += exp((double)logits[i] - mx);
    }
    return mx + log(sum > 0.0 ? sum : 1.0);
}

static void collect_top_logprobs(
    const float *logits,
    uint32_t V,
    double log_z,
    int chosen_tok,
    int want_k,
    StudioLpStep *out_step
) {
    out_step->chosen_tok = chosen_tok;
    out_step->chosen_lp = (chosen_tok >= 0 && (uint32_t)chosen_tok < V)
                          ? (float)((double)logits[chosen_tok] - log_z) : 0.0f;
    if (want_k < 1) want_k = 1;
    if (want_k > 5) want_k = 5;
    out_step->n_alt = 0;
    for (int k = 0; k < want_k; k++) {
        int best_i = -1;
        float best_v = -1e30f;
        for (uint32_t i = 0U; i < V; i++) {
            int dup = 0;
            for (int p = 0; p < out_step->n_alt; p++) {
                if (out_step->alts[p].tok == (int)i) { dup = 1; break; }
            }
            if (!dup && (best_i < 0 || logits[i] > best_v)) {
                best_i = (int)i;
                best_v = logits[i];
            }
        }
        if (best_i >= 0) {
            out_step->alts[out_step->n_alt].tok = best_i;
            out_step->alts[out_step->n_alt].lp = (float)((double)best_v - log_z);
            out_step->n_alt++;
        }
    }
}

static void sb_append_tok_piece_json(StrBuf *sb, const LlamaModel *m, int tok) {
    char piece[256];
    size_t plen = 0U;
    if (tok >= 0 && (uint32_t)tok < m->vocab_size) {
        plen = detokenize_piece(&m->tok, (uint32_t)tok, piece, sizeof(piece));
        if (plen >= sizeof(piece)) plen = sizeof(piece) - 1U;
    }
    piece[plen] = '\0';
    sb_append_json_str(sb, piece);
}

static void sb_append_chat_logprobs(StrBuf *sb, const LlamaModel *m, const StudioLpStep *steps, int n_steps) {
    sb_append(sb, "{\"content\":[");
    for (int i = 0; i < n_steps; i++) {
        if (i > 0) sb_append_char(sb, ',');
        char fbuf[48];
        sb_append(sb, "{\"token\":");
        sb_append_tok_piece_json(sb, m, steps[i].chosen_tok);
        snprintf(fbuf, sizeof(fbuf), ",\"logprob\":%.6f,\"top_logprobs\":[", (double)steps[i].chosen_lp);
        sb_append(sb, fbuf);
        for (int j = 0; j < steps[i].n_alt; j++) {
            if (j > 0) sb_append_char(sb, ',');
            sb_append(sb, "{\"token\":");
            sb_append_tok_piece_json(sb, m, steps[i].alts[j].tok);
            snprintf(fbuf, sizeof(fbuf), ",\"logprob\":%.6f}", (double)steps[i].alts[j].lp);
            sb_append(sb, fbuf);
        }
        sb_append(sb, "]}");
    }
    sb_append(sb, "]}");
}

static char *studio_strdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *p = (char *)malloc(n + 1U);
    if (!p) return NULL;
    memcpy(p, s, n + 1U);
    return p;
}

static void studio_execute_generation(ZccStudioState *st, ZccSocket client,
                                      const OpenAiReq *req_in, int api_mode,
                                      char **out_batch_json) {
    const OpenAiReq *req = req_in;
    if (out_batch_json) *out_batch_json = NULL;

    int not_found = 0;
    int ad_idx = resolve_req_adapter_idx(st, req, &not_found);
    if (not_found) {
        if (!out_batch_json) send_http_error(client, 404, "Not Found", "invalid_request_error", "Requested LoRA adapter not loaded");
        return;
    }
    float scale_mult = (ad_idx >= 0) ? (req->has_lora_scale ? req->lora_scale : st->adapters[ad_idx].scale_mult) : 0.0f;
    float lora_scale = (ad_idx >= 0) ? ((st->adapters[ad_idx].alpha / (float)st->adapters[ad_idx].rank) * scale_mult) : 0.0f;
    const LoraLayer *ls = (ad_idx >= 0 && lora_scale > 0.0f) ? st->adapters[ad_idx].ls : NULL;
    const char *used_adapter = (ad_idx >= 0) ? st->adapters[ad_idx].name : "base";

    char *prompt_alloc = NULL;
    const char *raw_prompt = NULL;
    if (api_mode == STUDIO_API_CHAT || api_mode == STUDIO_API_ANTHROPIC ||
        (api_mode == STUDIO_API_RESPONSES && req->has_messages && req->n_msgs > 0U)) {
        if (!req->has_messages || req->n_msgs == 0U) {
            if (!out_batch_json) send_http_error(client, 400, "Bad Request", "invalid_request_error", "Missing or empty 'messages' array");
            return;
        }
        prompt_alloc = format_openai_messages_ex_alloc(&st->model, req->system_text, req->msgs, req->n_msgs);
        raw_prompt = prompt_alloc;
    } else if (api_mode == STUDIO_API_RESPONSES) {
        if (!req->has_prompt || !req->prompt || req->prompt[0] == '\0') {
            if (!out_batch_json) send_http_error(client, 400, "Bad Request", "invalid_request_error", "Missing 'input' for /v1/responses");
            return;
        }
        OpenAiMsg one;
        one.role = (char *)"user";
        one.content = req->prompt;
        prompt_alloc = format_openai_messages_ex_alloc(&st->model, req->system_text, &one, 1U);
        raw_prompt = prompt_alloc;
    } else {
        if (!req->has_prompt || !req->prompt || req->prompt[0] == '\0') {
            if (!out_batch_json) send_http_error(client, 400, "Bad Request", "invalid_request_error", "Missing or empty 'prompt' string");
            return;
        }
        raw_prompt = req->prompt;
    }

    uint32_t *t_ids = NULL;
    uint32_t t_len = 0U;
    if (!raw_prompt || tokenize_prompt(&st->model, raw_prompt, 0, &t_ids, &t_len) != 0 || t_len == 0U) {
        if (prompt_alloc) free(prompt_alloc);
        if (!out_batch_json) send_http_error(client, 400, "Bad Request", "invalid_request_error", "Prompt tokenized to empty sequence");
        return;
    }
    if (prompt_alloc) { free(prompt_alloc); prompt_alloc = NULL; }

    if ((uint64_t)t_len >= (uint64_t)st->eff_max_seq) {
        free(t_ids);
        if (!out_batch_json) send_http_error(client, 400, "Bad Request", "context_length_exceeded", "Prompt exceeds context window");
        return;
    }

    int max_steps = req->max_tokens;
    int room = (int)(st->eff_max_seq - t_len);
    if (max_steps > room) max_steps = room;
    if (max_steps < 1) max_steps = 1;

    g_zcc_llm_failed = 0;
    g_zcc_llm_err[0] = '\0';

    int do_logprobs = (req->logprobs > 0 && !req->stream && !out_batch_json) ? 1 : 0;
    int has_extra_pen = (req->presence_penalty != 0.0f || req->frequency_penalty != 0.0f) ? 1 : 0;
    int use_evolved = (!st->no_cuda_graph && st->model.head_dim <= 256U &&
                       (st->kv_type == ZCC_KV_F16 || st->kv_type == ZCC_KV_F32)) ? 1 : 0;
    int use_gpu_argmax = (use_evolved && req->temperature <= 0.0f && req->repeat_penalty == 1.0f &&
                          !do_logprobs && !has_extra_pen) ? 1 : 0;

    if (use_evolved) {
        lora_decode_plan_ensure(&st->decode_plan, &st->model, &st->act, ls, st->d_inf_h,
                                st->eff_max_seq, st->use_tc, st->kv_type);
        if (g_zcc_llm_failed) {
            free(t_ids);
            if (!out_batch_json) send_http_error(client, 500, "Internal Server Error", "server_error", g_zcc_llm_err);
            g_zcc_llm_failed = 0;
            return;
        }
    }

    check_cu(cuMemsetD8_v2(st->act.k_cache, 0, st->kv_bytes), "st_reset_k");
    check_cu(cuMemsetD8_v2(st->act.v_cache, 0, st->kv_bytes), "st_reset_v");
    if (g_zcc_llm_failed) {
        free(t_ids);
        if (!out_batch_json) send_http_error(client, 500, "Internal Server Error", "server_error", g_zcc_llm_err);
        g_zcc_llm_failed = 0;
        return;
    }

    unsigned int hist_len = 0U;
    int nxt_cached = 0;
    check_cu(cuEventRecord(st->ev0, NULL), "st_ev0");
    unsigned int pos = 0U;
    uint32_t prefill_n = 0U;
    for (uint32_t pi = 0U; pi < t_len && pi + 1U < st->eff_max_seq; pi++) prefill_n = pi + 1U;
    for (uint32_t pi = 0U; pi < prefill_n; pi++) {
        uint32_t tk = (t_ids[pi] < st->model.vocab_size) ? t_ids[pi] : 0U;
        st->hist[hist_len++] = tk;
        if (use_evolved) {
            if (pi + 1U < prefill_n) {
                lora_decode_step_body(&st->decode_plan, &st->act, tk, pos, st->eff_max_seq, lora_scale);
            } else if (use_gpu_argmax) {
                nxt_cached = lora_decode_step_argmax(&st->decode_plan, &st->model, &st->act,
                                                     tk, pos, st->eff_max_seq, lora_scale, -1);
            } else {
                lora_decode_step_logits(&st->decode_plan, &st->act, tk, pos, st->eff_max_seq, lora_scale);
            }
        } else {
            gpu_forward_token_lora(&st->model, &st->act, ls, lora_scale, st->d_inf_h,
                                   tk, pos, st->eff_max_seq, st->use_tc, st->kv_type);
        }
        pos++;
        if (g_zcc_llm_failed) break;
    }
    free(t_ids);

    if (!use_gpu_argmax) {
        if (!use_evolved && !g_zcc_llm_failed) check_cu(cuCtxSynchronize(), "st_pf_sync");
        if (!g_zcc_llm_failed) check_cu(cuMemcpyDtoH_v2(st->h_logits, st->act.logits, (size_t)st->model.vocab_size * 4U), "st_pf_logits");
        if (!g_zcc_llm_failed) check_finite_logits(st->h_logits, st->model.vocab_size, -1);
    }
    if (g_zcc_llm_failed) {
        if (!out_batch_json) send_http_error(client, 500, "Internal Server Error", "server_error", g_zcc_llm_err);
        g_zcc_llm_failed = 0;
        return;
    }

    StudioLpStep *lp_steps = NULL;
    if (do_logprobs) {
        lp_steps = (StudioLpStep *)calloc((size_t)max_steps, sizeof(StudioLpStep));
    }

    st->req_counter++;
    char req_id[64];
    const char *id_pre = "cmpl-zcc";
    if (api_mode == STUDIO_API_CHAT) id_pre = "chatcmpl-zcc";
    else if (api_mode == STUDIO_API_ANTHROPIC) id_pre = "msg_zcc";
    else if (api_mode == STUDIO_API_RESPONSES) id_pre = "resp_zcc";
    snprintf(req_id, sizeof(req_id), "%s-%llu", id_pre, st->req_counter);
    long long now_ts = studio_time64();

    int do_stream = (req->stream && !out_batch_json) ? 1 : 0;
    SseStreamCtx sctx;
    memset(&sctx, 0, sizeof(sctx));
    sctx.sock = client;
    sctx.api_mode = api_mode;
    sctx.created = now_ts;
    sctx.req_id = req_id;
    sctx.model_id = st->model_id;

    if (do_stream) {
        if (send_http_chunked_sse_hdr(client) != 0) {
            if (lp_steps) free(lp_steps);
            return;
        }
        if (api_mode == STUDIO_API_ANTHROPIC) {
            StrBuf sb;
            memset(&sb, 0, sizeof(sb));
            char tmp_a[64];
            fmt_u64_dec((uint64_t)t_len, tmp_a, sizeof(tmp_a));
            sb_append(&sb, "event: message_start\ndata: {\"type\":\"message_start\",\"message\":{\"id\":");
            sb_append_json_str(&sb, req_id);
            sb_append(&sb, ",\"type\":\"message\",\"role\":\"assistant\",\"content\":[],\"model\":");
            sb_append_json_str(&sb, st->model_id);
            sb_append(&sb, ",\"stop_reason\":null,\"stop_sequence\":null,\"usage\":{\"input_tokens\":");
            sb_append(&sb, tmp_a);
            sb_append(&sb, ",\"output_tokens\":0}}}\n\n"
                           "event: content_block_start\ndata: {\"type\":\"content_block_start\",\"index\":0,\"content_block\":{\"type\":\"text\",\"text\":\"\"}}\n\n");
            sse_emit_raw_chunk(&sctx, sb.data, sb.len);
            sb_free(&sb);
        }
    }

    uint32_t seed = req->seed ? req->seed : 42U;
    int gen_count = 0;
    size_t gen_text_len = 0U;
    st->gen_text[0] = '\0';
    int hit_stop_seq = 0;
    const char *matched_stop = NULL;

    for (int s = 0; s < max_steps && pos < st->eff_max_seq; s++) {
        int nxt = 0;
        if (use_gpu_argmax) {
            nxt = nxt_cached;
        } else {
            if (has_extra_pen) {
                apply_presence_freq_penalties(st->h_logits, st->model.vocab_size, st->gen_ids, gen_count,
                                              req->presence_penalty, req->frequency_penalty);
            }
            double log_z = 0.0;
            if (do_logprobs && lp_steps) {
                log_z = compute_logits_logz(st->h_logits, st->model.vocab_size);
            }
            nxt = sample_next_token(st->h_logits, st->work_logits, st->h_probs, st->cands,
                                    st->model.vocab_size, st->hist, hist_len,
                                    req->temperature, req->top_k, req->top_p, req->min_p, req->repeat_penalty, &seed);
            if (do_logprobs && lp_steps && gen_count < max_steps) {
                collect_top_logprobs(st->h_logits, st->model.vocab_size, log_z, nxt, req->top_logprobs, &lp_steps[gen_count]);
            }
        }
        if (is_stop_token(&st->model, 0, (uint32_t)nxt)) break;
        record_generated_token_ex(&st->model, (uint32_t)nxt, st->gen_ids, &gen_count,
                                  st->hist, &hist_len, &st->gen_text, &st->gen_text_cap,
                                  &gen_text_len, 0,
                                  do_stream ? sse_token_cb : NULL,
                                  do_stream ? (void *)&sctx : NULL);
        if (sctx.disconnected || g_zcc_llm_failed) break;

        if (req->n_stops > 0 && st->gen_text) {
            char *earliest = NULL;
            const char *which = NULL;
            for (int si = 0; si < req->n_stops; si++) {
                if (!req->stops[si] || !req->stops[si][0]) continue;
                char *f = strstr(st->gen_text, req->stops[si]);
                if (f && (!earliest || f < earliest)) {
                    earliest = f;
                    which = req->stops[si];
                }
            }
            if (earliest) {
                if (!do_stream) {
                    *earliest = '\0';
                    gen_text_len = (size_t)(earliest - st->gen_text);
                }
                hit_stop_seq = 1;
                matched_stop = which;
                break;
            }
        }

        if (s + 1 < max_steps && pos + 1U < st->eff_max_seq) {
            if (use_gpu_argmax) {
                nxt_cached = lora_decode_step_argmax(&st->decode_plan, &st->model, &st->act,
                                                     (uint32_t)nxt, pos, st->eff_max_seq, lora_scale, s);
            } else if (use_evolved) {
                lora_decode_step_logits(&st->decode_plan, &st->act,
                                        (uint32_t)nxt, pos, st->eff_max_seq, lora_scale);
                if (!g_zcc_llm_failed) {
                    check_cu(cuMemcpyDtoH_v2(st->h_logits, st->act.logits, (size_t)st->model.vocab_size * 4U), "st_step_logits");
                    if (!g_zcc_llm_failed) check_finite_logits(st->h_logits, st->model.vocab_size, s);
                }
            } else {
                gpu_forward_token_lora(&st->model, &st->act, ls, lora_scale, st->d_inf_h,
                                       (uint32_t)nxt, pos, st->eff_max_seq, st->use_tc, st->kv_type);
                if (!g_zcc_llm_failed) {
                    check_cu(cuCtxSynchronize(), "st_step_sync");
                    check_cu(cuMemcpyDtoH_v2(st->h_logits, st->act.logits, (size_t)st->model.vocab_size * 4U), "st_step_logits");
                    if (!g_zcc_llm_failed) check_finite_logits(st->h_logits, st->model.vocab_size, s);
                }
            }
            pos++;
            if (g_zcc_llm_failed) break;
        }
    }

    if (sctx.disconnected) {
        if (lp_steps) free(lp_steps);
        g_zcc_llm_failed = 0;
        return;
    }

    if (g_zcc_llm_failed) {
        if (lp_steps) free(lp_steps);
        g_zcc_llm_failed = 0;
        if (!do_stream && !out_batch_json) {
            send_http_error(client, 500, "Internal Server Error", "server_error", g_zcc_llm_err);
        } else if (do_stream) {
            sock_send_all(client, "0\r\n\r\n", 5U);
        }
        return;
    }

    check_cu(cuEventRecord(st->ev1, NULL), "st_ev1");
    check_cu(cuEventSynchronize(st->ev1), "st_ev1_sync");
    float ms = 0.0f;
    check_cu(cuEventElapsedTime(&ms, st->ev0, st->ev1), "st_elapsed");
    g_zcc_llm_failed = 0;

    st->last_gen_tokens = gen_count;
    st->last_prompt_tokens = t_len;
    st->last_elapsed_ms = (double)ms;
    st->total_gen_tokens += (unsigned long long)gen_count;
    st->last_tps = (ms > 0.0f) ? (((double)(t_len + (uint32_t)gen_count) * 1000.0) / (double)ms) : 0.0;
    const char *finish_reason = hit_stop_seq ? "stop" : ((gen_count >= max_steps) ? "length" : "stop");
    const char *ant_stop = hit_stop_seq ? "stop_sequence" : ((gen_count >= max_steps) ? "max_tokens" : "end_turn");

    if (do_stream) {
        char pt_s[32], ct_s[32], tt_s[32];
        fmt_u64_dec((uint64_t)t_len, pt_s, sizeof(pt_s));
        fmt_u64_dec((uint64_t)gen_count, ct_s, sizeof(ct_s));
        fmt_u64_dec((uint64_t)t_len + (uint64_t)gen_count, tt_s, sizeof(tt_s));
        if (api_mode == STUDIO_API_ANTHROPIC) {
            StrBuf sb;
            memset(&sb, 0, sizeof(sb));
            sb_append(&sb, "event: content_block_stop\ndata: {\"type\":\"content_block_stop\",\"index\":0}\n\n"
                           "event: message_delta\ndata: {\"type\":\"message_delta\",\"delta\":{\"stop_reason\":");
            sb_append_json_str(&sb, ant_stop);
            sb_append(&sb, ",\"stop_sequence\":");
            if (matched_stop) sb_append_json_str(&sb, matched_stop);
            else sb_append(&sb, "null");
            sb_append(&sb, "},\"usage\":{\"output_tokens\":");
            sb_append(&sb, ct_s);
            sb_append(&sb, "}}\n\nevent: message_stop\ndata: {\"type\":\"message_stop\"}\n\n");
            sse_emit_raw_chunk(&sctx, sb.data, sb.len);
            sb_free(&sb);
        } else if (api_mode == STUDIO_API_RESPONSES) {
            StrBuf sb;
            memset(&sb, 0, sizeof(sb));
            sb_append(&sb, "event: response.completed\ndata: {\"type\":\"response.completed\",\"response\":{\"id\":");
            sb_append_json_str(&sb, req_id);
            sb_append(&sb, ",\"object\":\"response\",\"status\":\"completed\",\"output_text\":");
            sb_append_json_str(&sb, st->gen_text ? st->gen_text : "");
            sb_append(&sb, "}}\n\n");
            sse_emit_raw_chunk(&sctx, sb.data, sb.len);
            sb_free(&sb);
        } else {
            if (!sctx.sent_role && api_mode == STUDIO_API_CHAT) {
                sse_emit_frame(&sctx, "", NULL);
            }
            sse_emit_frame(&sctx, "", finish_reason);
            if (req->include_usage && !sctx.disconnected) {
                StrBuf usb;
                memset(&usb, 0, sizeof(usb));
                usb.data = NULL;
                sb_append(&usb, "data: {\"id\":");
                sb_append_json_str(&usb, req_id);
                sb_append(&usb, ",\"object\":");
                sb_append_json_str(&usb, (api_mode == STUDIO_API_CHAT) ? "chat.completion.chunk" : "text_completion");
                sb_append(&usb, ",\"model\":");
                sb_append_json_str(&usb, st->model_id);
                sb_append(&usb, ",\"choices\":[],\"usage\":{\"prompt_tokens\":");
                sb_append(&usb, pt_s);
                sb_append(&usb, ",\"completion_tokens\":");
                sb_append(&usb, ct_s);
                sb_append(&usb, ",\"total_tokens\":");
                sb_append(&usb, tt_s);
                sb_append(&usb, "}}\n\n");
                sse_emit_raw_chunk(&sctx, usb.data, usb.len);
                sb_free(&usb);
            }
            if (!sctx.disconnected) {
                send_http_chunk(client, "data: [DONE]\n\n", 14U);
            }
        }
        if (!sctx.disconnected) sock_send_all(client, "0\r\n\r\n", 5U);
    } else {
        StrBuf out;
        memset(&out, 0, sizeof(out));
        char cr_s[32], pt_s[32], ct_s[32], tt_s[32], met_s[192];
        fmt_u64_dec((uint64_t)(now_ts > 0LL ? now_ts : 0LL), cr_s, sizeof(cr_s));
        fmt_u64_dec((uint64_t)t_len, pt_s, sizeof(pt_s));
        fmt_u64_dec((uint64_t)gen_count, ct_s, sizeof(ct_s));
        fmt_u64_dec((uint64_t)t_len + (uint64_t)gen_count, tt_s, sizeof(tt_s));

        if (api_mode == STUDIO_API_ANTHROPIC) {
            sb_append(&out, "{\"id\":");
            sb_append_json_str(&out, req_id);
            sb_append(&out, ",\"type\":\"message\",\"role\":\"assistant\",\"model\":");
            sb_append_json_str(&out, st->model_id);
            sb_append(&out, ",\"adapter\":");
            sb_append_json_str(&out, used_adapter);
            sb_append(&out, ",\"content\":[{\"type\":\"text\",\"text\":");
            sb_append_json_str(&out, st->gen_text ? st->gen_text : "");
            sb_append(&out, "}],\"stop_reason\":");
            sb_append_json_str(&out, ant_stop);
            sb_append(&out, ",\"stop_sequence\":");
            if (matched_stop) sb_append_json_str(&out, matched_stop);
            else sb_append(&out, "null");
            sb_append(&out, ",\"usage\":{\"input_tokens\":");
            sb_append(&out, pt_s);
            sb_append(&out, ",\"output_tokens\":");
            sb_append(&out, ct_s);
        } else if (api_mode == STUDIO_API_RESPONSES) {
            sb_append(&out, "{\"id\":");
            sb_append_json_str(&out, req_id);
            sb_append(&out, ",\"object\":\"response\",\"created_at\":");
            sb_append(&out, cr_s);
            sb_append(&out, ",\"status\":\"completed\",\"model\":");
            sb_append_json_str(&out, st->model_id);
            sb_append(&out, ",\"adapter\":");
            sb_append_json_str(&out, used_adapter);
            sb_append(&out, ",\"output\":[{\"type\":\"message\",\"id\":\"msg_0\",\"status\":\"completed\",\"role\":\"assistant\",\"content\":[{\"type\":\"output_text\",\"text\":");
            sb_append_json_str(&out, st->gen_text ? st->gen_text : "");
            sb_append(&out, "}]}],\"output_text\":");
            sb_append_json_str(&out, st->gen_text ? st->gen_text : "");
            sb_append(&out, ",\"usage\":{\"input_tokens\":");
            sb_append(&out, pt_s);
            sb_append(&out, ",\"output_tokens\":");
            sb_append(&out, ct_s);
            sb_append(&out, ",\"total_tokens\":");
            sb_append(&out, tt_s);
        } else {
            sb_append(&out, "{\"id\":");
            sb_append_json_str(&out, req_id);
            sb_append(&out, (api_mode == STUDIO_API_CHAT) ? ",\"object\":\"chat.completion\",\"created\":" : ",\"object\":\"text_completion\",\"created\":");
            sb_append(&out, cr_s);
            sb_append(&out, ",\"model\":");
            sb_append_json_str(&out, st->model_id);
            sb_append(&out, ",\"adapter\":");
            sb_append_json_str(&out, used_adapter);
            sb_append(&out, ",\"system_fingerprint\":\"zcc-sm120-cuda-graph\"");
            if (api_mode == STUDIO_API_CHAT) {
                sb_append(&out, ",\"choices\":[{\"index\":0,\"message\":{\"role\":\"assistant\",\"content\":");
                sb_append_json_str(&out, st->gen_text ? st->gen_text : "");
                sb_append(&out, "},\"logprobs\":");
                if (lp_steps) sb_append_chat_logprobs(&out, &st->model, lp_steps, gen_count);
                else sb_append(&out, "null");
                sb_append(&out, ",\"finish_reason\":");
            } else {
                sb_append(&out, ",\"choices\":[{\"index\":0,\"text\":");
                sb_append_json_str(&out, st->gen_text ? st->gen_text : "");
                sb_append(&out, ",\"logprobs\":");
                if (lp_steps) sb_append_chat_logprobs(&out, &st->model, lp_steps, gen_count);
                else sb_append(&out, "null");
                sb_append(&out, ",\"finish_reason\":");
            }
            sb_append_json_str(&out, finish_reason);
            sb_append(&out, "}],\"usage\":{\"prompt_tokens\":");
            sb_append(&out, pt_s);
            sb_append(&out, ",\"completion_tokens\":");
            sb_append(&out, ct_s);
            sb_append(&out, ",\"total_tokens\":");
            sb_append(&out, tt_s);
        }
        snprintf(met_s, sizeof(met_s),
                 "},\"zcc_metrics\":{\"tok_per_sec\":%.2f,\"elapsed_ms\":%.2f,\"lora_scale\":%.2f}}",
                 st->last_tps, (double)ms, (double)scale_mult);
        sb_append(&out, met_s);

        if (out_batch_json) {
            *out_batch_json = out.data ? out.data : studio_strdup("{}");
            out.data = NULL;
        } else {
            send_http_json(client, 200, "OK", out.data ? out.data : "{}");
            sb_free(&out);
        }
    }
    if (lp_steps) free(lp_steps);
}

static void handle_openai_inference(ZccStudioState *st, ZccSocket client,
                                    const char *body, size_t body_len, int api_mode) {
    OpenAiReq req;
    if (parse_openai_request(body, body_len, &req) != 0) {
        send_http_error(client, 400, "Bad Request", "invalid_request_error", "Malformed JSON request body");
        return;
    }
    studio_execute_generation(st, client, &req, api_mode, NULL);
    openai_req_free(&req);
}

static void handle_lora_adapters_get(ZccStudioState *st, ZccSocket client) {
    StrBuf sb;
    memset(&sb, 0, sizeof(sb));
    char num[128];
    sb_append(&sb, "{\"active\":");
    sb_append_json_str(&sb, active_adapter_name_str(st));
    snprintf(num, sizeof(num), ",\"count\":%d,\"adapters\":[", st->n_adapters);
    sb_append(&sb, num);
    for (int i = 0; i < st->n_adapters; i++) {
        const StudioAdapter *ad = &st->adapters[i];
        if (i > 0) sb_append_char(&sb, ',');
        sb_append(&sb, "{\"name\":");
        sb_append_json_str(&sb, ad->name);
        snprintf(num, sizeof(num),
                 ",\"rank\":%u,\"alpha\":%.2f,\"scale\":%.2f,\"params\":%llu,\"fnv\":\"%08x\",\"active\":%s,\"path\":",
                 ad->rank, (double)ad->alpha, (double)ad->scale_mult, (unsigned long long)ad->params,
                 ad->fnv, (st->active_idx == i) ? "true" : "false");
        sb_append(&sb, num);
        sb_append_json_str(&sb, ad->path);
        sb_append_char(&sb, '}');
    }
    sb_append(&sb, "]}");
    send_http_json(client, 200, "OK", sb.data ? sb.data : "{}");
    sb_free(&sb);
}

static void handle_lora_select(ZccStudioState *st, ZccSocket client, const char *body, size_t body_len) {
    LoraCtrlReq req;
    if (parse_lora_ctrl_request(body, body_len, &req) != 0 || !req.name || !req.name[0]) {
        lora_ctrl_req_free(&req);
        send_http_error(client, 400, "Bad Request", "invalid_request_error", "Expected JSON {\"name\":\"base\"|\"<adapter>\"}");
        return;
    }
    if (strcmp(req.name, "base") == 0 || strcmp(req.name, "none") == 0) {
        st->active_idx = -1;
        lora_ctrl_req_free(&req);
        send_http_json(client, 200, "OK", "{\"status\":\"ok\",\"active\":\"base\"}");
        return;
    }
    int idx = find_adapter_idx(st, req.name);
    if (idx < 0) {
        lora_ctrl_req_free(&req);
        send_http_error(client, 404, "Not Found", "invalid_request_error", "Unknown LoRA adapter");
        return;
    }
    st->active_idx = idx;
    StrBuf sb;
    memset(&sb, 0, sizeof(sb));
    sb_append(&sb, "{\"status\":\"ok\",\"active\":");
    sb_append_json_str(&sb, st->adapters[idx].name);
    sb_append(&sb, "}");
    send_http_json(client, 200, "OK", sb.data ? sb.data : "{}");
    sb_free(&sb);
    lora_ctrl_req_free(&req);
}

static void handle_lora_load(ZccStudioState *st, ZccSocket client, const char *body, size_t body_len) {
    LoraCtrlReq req;
    if (parse_lora_ctrl_request(body, body_len, &req) != 0 ||
        !req.name || !is_valid_adapter_name(req.name) || !req.path || !req.path[0]) {
        lora_ctrl_req_free(&req);
        send_http_error(client, 400, "Bad Request", "invalid_request_error", "Expected valid 'name' and 'path'");
        return;
    }
    if (find_adapter_idx(st, req.name) < 0 && st->n_adapters >= ZCC_MAX_STUDIO_ADAPTERS) {
        lora_ctrl_req_free(&req);
        send_http_error(client, 400, "Bad Request", "invalid_request_error", "Adapter registry full (max 16)");
        return;
    }

    g_zcc_llm_failed = 0;
    g_zcc_llm_err[0] = '\0';
    g_last_lora_fnv = 0U;
    unsigned int rk = 0U;
    float al = 0.0f;
    LoraLayer *ls = load_lora_adapter(req.path, &st->model, &rk, &al);
    if (!ls || g_zcc_llm_failed) {
        const char *err_msg = g_zcc_llm_err[0] ? g_zcc_llm_err : "LORA_IO_ERROR: failed to load adapter";
        g_zcc_llm_failed = 0;
        if (ls) free_lora_layers(ls, st->model.n_layers);
        send_http_error(client, 400, "Bad Request", "invalid_request_error", err_msg);
        lora_ctrl_req_free(&req);
        return;
    }

    uint32_t fnv = g_last_lora_fnv;
    int slot = upsert_adapter(st, req.name, req.path, ls, rk, al, fnv, req.activate);
    if (slot < 0) {
        free_lora_layers(ls, st->model.n_layers);
        send_http_error(client, 400, "Bad Request", "invalid_request_error", "Adapter registry full");
        lora_ctrl_req_free(&req);
        return;
    }

    StrBuf sb;
    memset(&sb, 0, sizeof(sb));
    char tmp[256];
    sb_append(&sb, "{\"status\":\"ok\",\"name\":");
    sb_append_json_str(&sb, st->adapters[slot].name);
    sb_append(&sb, ",\"active\":");
    sb_append_json_str(&sb, active_adapter_name_str(st));
    snprintf(tmp, sizeof(tmp), ",\"rank\":%u,\"alpha\":%.2f,\"params\":%llu,\"fnv\":\"%08x\"}",
             rk, (double)al, (unsigned long long)st->adapters[slot].params, fnv);
    sb_append(&sb, tmp);
    send_http_json(client, 200, "OK", sb.data ? sb.data : "{}");
    sb_free(&sb);
    lora_ctrl_req_free(&req);
}

static void handle_lora_unload(ZccStudioState *st, ZccSocket client, const char *body, size_t body_len) {
    LoraCtrlReq req;
    if (parse_lora_ctrl_request(body, body_len, &req) != 0 || !req.name || !is_valid_adapter_name(req.name)) {
        lora_ctrl_req_free(&req);
        send_http_error(client, 400, "Bad Request", "invalid_request_error", "Expected valid adapter 'name'");
        return;
    }
    int idx = find_adapter_idx(st, req.name);
    if (idx < 0) {
        lora_ctrl_req_free(&req);
        send_http_error(client, 404, "Not Found", "invalid_request_error", "Adapter not loaded");
        return;
    }

    char removed[64];
    snprintf(removed, sizeof(removed), "%s", st->adapters[idx].name);
    lora_decode_plan_invalidate(&st->decode_plan);
    if (st->adapters[idx].ls) free_lora_layers(st->adapters[idx].ls, st->model.n_layers);
    for (int i = idx; i + 1 < st->n_adapters; i++) {
        memcpy(&st->adapters[i], &st->adapters[i + 1], sizeof(StudioAdapter));
    }
    st->n_adapters--;
    memset(&st->adapters[st->n_adapters], 0, sizeof(StudioAdapter));

    if (st->active_idx == idx) st->active_idx = -1;
    else if (st->active_idx > idx) st->active_idx--;

    StrBuf sb;
    memset(&sb, 0, sizeof(sb));
    char tmp[64];
    sb_append(&sb, "{\"status\":\"ok\",\"unloaded\":");
    sb_append_json_str(&sb, removed);
    sb_append(&sb, ",\"active\":");
    sb_append_json_str(&sb, active_adapter_name_str(st));
    snprintf(tmp, sizeof(tmp), ",\"count\":%d}", st->n_adapters);
    sb_append(&sb, tmp);
    send_http_json(client, 200, "OK", sb.data ? sb.data : "{}");
    sb_free(&sb);
    lora_ctrl_req_free(&req);
}

static void handle_lora_scale(ZccStudioState *st, ZccSocket client, const char *body, size_t body_len) {
    LoraCtrlReq req;
    if (parse_lora_ctrl_request(body, body_len, &req) != 0 || !req.name || !is_valid_adapter_name(req.name) || !req.has_scale) {
        lora_ctrl_req_free(&req);
        send_http_error(client, 400, "Bad Request", "invalid_request_error", "Expected JSON {\"name\":\"<adapter>\",\"scale\":0.0..4.0}");
        return;
    }
    if (!(req.scale >= 0.0f && req.scale <= 4.0f)) {
        lora_ctrl_req_free(&req);
        send_http_error(client, 400, "Bad Request", "invalid_request_error", "LoRA scale must be in range 0.0..4.0");
        return;
    }
    int idx = find_adapter_idx(st, req.name);
    if (idx < 0) {
        lora_ctrl_req_free(&req);
        send_http_error(client, 404, "Not Found", "invalid_request_error", "Adapter not loaded");
        return;
    }
    st->adapters[idx].scale_mult = req.scale;
    float eff = (st->adapters[idx].alpha / (float)st->adapters[idx].rank) * req.scale;
    StrBuf sb;
    memset(&sb, 0, sizeof(sb));
    char tmp[128];
    sb_append(&sb, "{\"status\":\"ok\",\"name\":");
    sb_append_json_str(&sb, st->adapters[idx].name);
    snprintf(tmp, sizeof(tmp), ",\"scale\":%.2f,\"effective_scale\":%.4f}",
             (double)st->adapters[idx].scale_mult, (double)eff);
    sb_append(&sb, tmp);
    send_http_json(client, 200, "OK", sb.data ? sb.data : "{}");
    sb_free(&sb);
    lora_ctrl_req_free(&req);
}

static void handle_lora_save(ZccStudioState *st, ZccSocket client, const char *body, size_t body_len) {
    LoraCtrlReq req;
    if (parse_lora_ctrl_request(body, body_len, &req) != 0 || !req.name || !is_valid_adapter_name(req.name)) {
        lora_ctrl_req_free(&req);
        send_http_error(client, 400, "Bad Request", "invalid_request_error", "Expected valid adapter 'name' and target 'path'");
        return;
    }
    const char *dst_path = (req.path && req.path[0]) ? req.path : req.save_path;
    if (!dst_path || !dst_path[0]) {
        lora_ctrl_req_free(&req);
        send_http_error(client, 400, "Bad Request", "invalid_request_error", "Expected non-empty target 'path' for saving .zcc_lora");
        return;
    }
    int idx = find_adapter_idx(st, req.name);
    if (idx < 0) {
        lora_ctrl_req_free(&req);
        send_http_error(client, 404, "Not Found", "invalid_request_error", "Adapter not loaded");
        return;
    }
    StudioAdapter *ad = &st->adapters[idx];
    g_zcc_llm_failed = 0;
    g_zcc_llm_err[0] = '\0';
    g_last_lora_fnv = 0U;
    if (save_lora_adapter(dst_path, &st->model, ad->ls, ad->rank, ad->alpha) != 0 || g_zcc_llm_failed) {
        const char *em = g_zcc_llm_err[0] ? g_zcc_llm_err : "LORA_IO_ERROR: failed to save adapter";
        g_zcc_llm_failed = 0;
        send_http_error(client, 500, "Internal Server Error", "server_error", em);
        lora_ctrl_req_free(&req);
        return;
    }
    snprintf(ad->path, sizeof(ad->path), "%s", dst_path);
    ad->fnv = g_last_lora_fnv;
    uint64_t total_bytes = 64ULL + lora_payload_byte_size(&st->model, ad->rank);

    StrBuf sb;
    memset(&sb, 0, sizeof(sb));
    char tmp[160];
    sb_append(&sb, "{\"status\":\"ok\",\"name\":");
    sb_append_json_str(&sb, ad->name);
    sb_append(&sb, ",\"path\":");
    sb_append_json_str(&sb, ad->path);
    snprintf(tmp, sizeof(tmp), ",\"bytes\":%llu,\"fnv\":\"%08x\"}",
             (unsigned long long)total_bytes, ad->fnv);
    sb_append(&sb, tmp);
    send_http_json(client, 200, "OK", sb.data ? sb.data : "{}");
    sb_free(&sb);
    lora_ctrl_req_free(&req);
}

static void handle_lora_files_get(ZccStudioState *st, ZccSocket client) {
    StudioDirFiles *df = (StudioDirFiles *)calloc(1U, sizeof(StudioDirFiles));
    if (!df) {
        send_http_error(client, 500, "Internal Server Error", "server_error", "OOM scanning vault directory");
        return;
    }
    studio_list_lora_dir(st->adapter_dir, df);

    uint64_t model_fp = compute_model_struct_fp(&st->model);
    size_t dlen = strlen(st->adapter_dir);
    int has_sep = (dlen > 0U && (st->adapter_dir[dlen - 1U] == '/' || st->adapter_dir[dlen - 1U] == '\\'));

    StrBuf sb;
    memset(&sb, 0, sizeof(sb));
    char tmp[320];
    sb_append(&sb, "{\"dir\":");
    sb_append_json_str(&sb, st->adapter_dir);
    snprintf(tmp, sizeof(tmp), ",\"count\":%d,\"files\":[", df->count);
    sb_append(&sb, tmp);

    for (int i = 0; i < df->count; i++) {
        char full_path[800];
        if (has_sep) snprintf(full_path, sizeof(full_path), "%s%s", st->adapter_dir, df->names[i]);
        else snprintf(full_path, sizeof(full_path), "%s/%s", st->adapter_dir, df->names[i]);

        uint64_t fsz = 0ULL;
        unsigned int rk = 0U;
        float al = 0.0f;
        uint32_t p_fnv = 0U;
        int valid_hdr = 0;
        int compatible = 0;
        int is_loaded = 0;

        FILE *fp = fopen(full_path, "rb");
        if (fp) {
            fseek(fp, 0, SEEK_END);
            long lsz = ftell(fp);
            if (lsz > 0L) fsz = (uint64_t)lsz;
            fseek(fp, 0, SEEK_SET);
            if (lsz >= 64L) {
                uint8_t hdr[64];
                if (fread(hdr, 1U, 64U, fp) == 64U && memcmp(hdr, "ZCCLORA1", 8U) == 0) {
                    if (fnv1a_bytes32(hdr, 60U) == read_le32(hdr + 60)) {
                        uint32_t ver = read_le32(hdr + 8);
                        rk = (unsigned int)read_le32(hdr + 12);
                        uint32_t alpha_bits = read_le32(hdr + 16);
                        memcpy(&al, &alpha_bits, 4U);
                        uint32_t nl = read_le32(hdr + 20);
                        uint32_t dim = read_le32(hdr + 24);
                        uint32_t kvd = read_le32(hdr + 28);
                        p_fnv = read_le32(hdr + 32);
                        uint64_t mfp = read_le64(hdr + 36);
                        uint64_t pbytes = read_le64(hdr + 44);
                        if (ver == 1U && rk >= 1U && rk <= 64U && fsz == (64ULL + pbytes)) {
                            valid_hdr = 1;
                            if (nl == st->model.n_layers && dim == st->model.dim &&
                                kvd == st->model.kv_dim && mfp == model_fp &&
                                pbytes == lora_payload_byte_size(&st->model, rk)) {
                                compatible = 1;
                            }
                        }
                    }
                }
            }
            fclose(fp);
        }

        for (int j = 0; j < st->n_adapters; j++) {
            const StudioAdapter *ad = &st->adapters[j];
            if (strcmp(ad->path, full_path) == 0 || (compatible && ad->fnv == p_fnv)) {
                is_loaded = 1;
                break;
            }
        }

        if (i > 0) sb_append_char(&sb, ',');
        sb_append(&sb, "{\"filename\":");
        sb_append_json_str(&sb, df->names[i]);
        sb_append(&sb, ",\"path\":");
        sb_append_json_str(&sb, full_path);
        snprintf(tmp, sizeof(tmp),
                 ",\"bytes\":%llu,\"rank\":%u,\"alpha\":%.2f,\"fnv\":\"%08x\",\"valid\":%s,\"compatible\":%s,\"loaded\":%s}",
                 (unsigned long long)fsz, rk, (double)al, p_fnv,
                 valid_hdr ? "true" : "false",
                 compatible ? "true" : "false",
                 is_loaded ? "true" : "false");
        sb_append(&sb, tmp);
    }

    sb_append(&sb, "]}");
    send_http_json(client, 200, "OK", sb.data ? sb.data : "{}");
    sb_free(&sb);
    free(df);
}

static void handle_lora_train(ZccStudioState *st, ZccSocket client, const char *body, size_t body_len) {
    LoraCtrlReq req;
    if (parse_lora_ctrl_request(body, body_len, &req) != 0 ||
        !req.name || !is_valid_adapter_name(req.name) || !req.sft_text || !req.sft_text[0]) {
        lora_ctrl_req_free(&req);
        send_http_error(client, 400, "Bad Request", "invalid_request_error", "Expected valid 'name' and non-empty 'sft_text'");
        return;
    }
    if (req.rank < 1 || req.rank > 32 ||
        !(req.alpha >= 0.01f && req.alpha <= 1024.0f) ||
        !(req.lr >= 1e-7f && req.lr <= 0.5f) ||
        req.steps < 1 || req.steps > 500) {
        lora_ctrl_req_free(&req);
        send_http_error(client, 400, "Bad Request", "invalid_request_error", "Invalid training hyperparameters (rank 1..32, steps 1..500, lr 1e-7..0.5)");
        return;
    }
    if (find_adapter_idx(st, req.name) < 0 && st->n_adapters >= ZCC_MAX_STUDIO_ADAPTERS) {
        lora_ctrl_req_free(&req);
        send_http_error(client, 400, "Bad Request", "invalid_request_error", "Adapter registry full (max 16)");
        return;
    }

    LoraDataset ds;
    if (parse_lora_dataset_buf(req.sft_text, strlen(req.sft_text), &st->model, req.chat, &ds) != 0 || ds.count == 0U) {
        free_lora_dataset(&ds);
        lora_ctrl_req_free(&req);
        send_http_error(client, 400, "Bad Request", "invalid_request_error", "Zero valid Q:/A: examples parsed from sft_text");
        return;
    }
    if (ds.max_seq_len > 256U) {
        free_lora_dataset(&ds);
        lora_ctrl_req_free(&req);
        send_http_error(client, 400, "Bad Request", "invalid_request_error", "SFT example exceeds 256 token studio training limit");
        return;
    }

    unsigned int active_rank = (unsigned int)req.rank;
    unsigned int train_T_max = (ds.max_seq_len < 16U) ? 16U : ds.max_seq_len;
    g_zcc_llm_failed = 0;
    g_zcc_llm_err[0] = '\0';

    LoraLayer *new_ls = alloc_lora_layers(&st->model, active_rank, 1, 0, req.seed);
    LoraTrainMem *tm = alloc_lora_train_mem(&st->model, train_T_max, active_rank, req.fp32_matmul);
    if (g_zcc_llm_failed) {
        const char *em = g_zcc_llm_err[0] ? g_zcc_llm_err : "CUDA training initialization failed";
        free_lora_train_mem(tm, st->model.n_layers);
        free_lora_layers(new_ls, st->model.n_layers);
        free_lora_dataset(&ds);
        lora_ctrl_req_free(&req);
        g_zcc_llm_failed = 0;
        send_http_error(client, 500, "Internal Server Error", "server_error", em);
        return;
    }

    float scale = req.alpha / (float)active_rank;
    uint64_t params = lora_payload_byte_size(&st->model, active_rank) / 4ULL;

    int disconnected = 0;
    if (req.stream) {
        if (send_http_chunked_sse_hdr(client) != 0) {
            disconnected = 1;
        } else {
            StrBuf sb;
            memset(&sb, 0, sizeof(sb));
            char tmp[256];
            sb_append(&sb, "data: {\"event\":\"start\",\"name\":");
            sb_append_json_str(&sb, req.name);
            snprintf(tmp, sizeof(tmp), ",\"examples\":%zu,\"max_T\":%u,\"rank\":%u,\"alpha\":%.2f,\"params\":%llu,\"steps\":%d}\n\n",
                     ds.count, ds.max_seq_len, active_rank, (double)req.alpha, (unsigned long long)params, req.steps);
            sb_append(&sb, tmp);
            if (send_http_chunk(client, sb.data, sb.len) != 0) disconnected = 1;
            sb_free(&sb);
        }
    }

    void *tr_ev0 = NULL, *tr_ev1 = NULL;
    if (!disconnected) {
        check_cu(cuEventCreate(&tr_ev0, 0U), "studio_tr_ev0");
        check_cu(cuEventCreate(&tr_ev1, 0U), "studio_tr_ev1");
        check_cu(cuEventRecord(tr_ev0, NULL), "studio_tr_start");
    }

    double first_loss = 0.0, last_loss = 0.0, total_toks = 0.0;
    for (int st_i = 1; st_i <= req.steps && !disconnected && !g_zcc_llm_failed; st_i++) {
        const LoraExample *ex = &ds.items[(size_t)(st_i - 1) % ds.count];
        double step_loss = lora_forward_loss(&st->model, new_ls, tm, ex, active_rank, scale);
        if (g_zcc_llm_failed) break;
        lora_backward_pass(&st->model, new_ls, tm, ex->T, scale);
        if (g_zcc_llm_failed) break;
        float lr_t = req.lr * (0.1f + 0.9f * 0.5f * (1.0f + (float)cos(3.14159265358979323846 * (double)(st_i - 1) / (double)req.steps)));
        float gn = lora_clip_and_adamw(&st->model, new_ls, tm, lr_t, 0.0f, 1.0f, st_i);
        if (g_zcc_llm_failed) break;
        total_toks += (double)ex->T;
        if (st_i == 1) first_loss = step_loss;
        last_loss = step_loss;

        if (req.stream && (st_i == 1 || st_i == req.steps || req.steps <= 20 || (st_i % 5 == 0))) {
            check_cu(cuEventRecord(tr_ev1, NULL), "studio_tr_tick");
            check_cu(cuEventSynchronize(tr_ev1), "studio_tr_sync");
            float ms = 0.0f;
            check_cu(cuEventElapsedTime(&ms, tr_ev0, tr_ev1), "studio_tr_ms");
            double tps = (ms > 0.0f) ? (total_toks * 1000.0 / (double)ms) : 0.0;
            if (!g_zcc_llm_failed) {
                char line[320];
                int n = snprintf(line, sizeof(line),
                                 "data: {\"event\":\"step\",\"step\":%d,\"steps\":%d,\"loss\":%.6f,\"gnorm\":%.4f,\"lr\":%.6f,\"tok_per_sec\":%.2f}\n\n",
                                 st_i, req.steps, step_loss, (double)gn, (double)lr_t, tps);
                if (n > 0 && send_http_chunk(client, line, (size_t)n) != 0) disconnected = 1;
            }
        }
    }

    if (tr_ev0) cuEventDestroy_v2(tr_ev0);
    if (tr_ev1) cuEventDestroy_v2(tr_ev1);
    free_lora_train_mem(tm, st->model.n_layers);
    free_lora_dataset(&ds);

    if (disconnected) {
        free_lora_layers(new_ls, st->model.n_layers);
        g_zcc_llm_failed = 0;
        lora_ctrl_req_free(&req);
        return;
    }

    if (g_zcc_llm_failed) {
        const char *em = g_zcc_llm_err[0] ? g_zcc_llm_err : "CUDA training error";
        free_lora_layers(new_ls, st->model.n_layers);
        g_zcc_llm_failed = 0;
        if (req.stream) {
            StrBuf sb;
            memset(&sb, 0, sizeof(sb));
            sb_append(&sb, "data: {\"event\":\"error\",\"message\":");
            sb_append_json_str(&sb, em);
            sb_append(&sb, "}\n\n");
            send_http_chunk(client, sb.data, sb.len);
            sb_free(&sb);
            sock_send_all(client, "0\r\n\r\n", 5U);
        } else {
            send_http_error(client, 500, "Internal Server Error", "server_error", em);
        }
        lora_ctrl_req_free(&req);
        return;
    }

    /* Drop optimizer + gradient buffers, leaving A + B weights live in VRAM for zero-copy hot-swap */
    free_lora_layers_train_mem(new_ls, st->model.n_layers);

    uint32_t fnv = 0U;
    int save_failed = 0;
    if (req.save_path && req.save_path[0]) {
        g_last_lora_fnv = 0U;
        if (save_lora_adapter(req.save_path, &st->model, new_ls, active_rank, req.alpha) == 0) {
            fnv = g_last_lora_fnv;
        } else {
            save_failed = 1;
            fnv = compute_lora_layers_fnv(&st->model, new_ls, active_rank);
        }
    } else {
        fnv = compute_lora_layers_fnv(&st->model, new_ls, active_rank);
    }

    int slot = upsert_adapter(st, req.name, req.save_path ? req.save_path : "", new_ls, active_rank, req.alpha, fnv, req.activate);
    if (slot < 0) free_lora_layers(new_ls, st->model.n_layers);

    StrBuf out;
    memset(&out, 0, sizeof(out));
    char tmp[320];
    if (req.stream) sb_append(&out, "data: {\"event\":\"done\",");
    else sb_append(&out, "{");
    sb_append(&out, "\"status\":\"ok\",\"name\":");
    sb_append_json_str(&out, req.name);
    sb_append(&out, ",\"active\":");
    sb_append_json_str(&out, active_adapter_name_str(st));
    snprintf(tmp, sizeof(tmp),
             ",\"rank\":%u,\"alpha\":%.2f,\"params\":%llu,\"fnv\":\"%08x\",\"initial_loss\":%.6f,\"final_loss\":%.6f,\"save_failed\":%s,\"saved_path\":",
             active_rank, (double)req.alpha, (unsigned long long)params, fnv,
             first_loss, last_loss, save_failed ? "true" : "false");
    sb_append(&out, tmp);
    sb_append_json_str(&out, req.save_path ? req.save_path : "");
    sb_append(&out, req.stream ? "}\n\n" : "}");

    if (req.stream) {
        if (send_http_chunk(client, out.data, out.len) == 0) {
            send_http_chunk(client, "data: [DONE]\n\n", 14U);
        }
        sock_send_all(client, "0\r\n\r\n", 5U);
    } else {
        send_http_json(client, 200, "OK", out.data ? out.data : "{}");
    }
    sb_free(&out);
    lora_ctrl_req_free(&req);
}

static int ascii_strncaseeq(const char *a, const char *b, size_t n);

static int extract_http_header_val(const char *hdr, size_t hdr_len, const char *hname, char *out, size_t cap) {
    if (!hdr || !hname || !out || cap == 0U) return 0;
    out[0] = '\0';
    size_t hlen = strlen(hname);
    size_t pos = 0U;
    while (pos < hdr_len && hdr[pos] != '\n') pos++;
    if (pos < hdr_len) pos++;
    while (pos < hdr_len) {
        size_t ls = pos;
        while (pos < hdr_len && hdr[pos] != '\n') pos++;
        size_t le = pos;
        if (le > ls && hdr[le - 1U] == '\r') le--;
        if (pos < hdr_len) pos++;
        if (le == ls) break;
        size_t llen = le - ls;
        if (llen > hlen && hdr[ls + hlen] == ':' && ascii_strncaseeq(hdr + ls, hname, hlen)) {
            const char *vp = hdr + ls + hlen + 1U;
            const char *vend = hdr + le;
            while (vp < vend && (*vp == ' ' || *vp == '\t')) vp++;
            while (vend > vp && (vend[-1] == ' ' || vend[-1] == '\t')) vend--;
            size_t vlen = (size_t)(vend - vp);
            if (vlen >= cap) vlen = cap - 1U;
            memcpy(out, vp, vlen);
            out[vlen] = '\0';
            return 1;
        }
    }
    return 0;
}

static int check_api_auth(const ZccStudioState *st, const char *hdr, size_t hdr_len) {
    if (!st || st->api_key[0] == '\0') return 1;
    char val[320];
    if (extract_http_header_val(hdr, hdr_len, "x-api-key", val, sizeof(val))) {
        if (strcmp(val, st->api_key) == 0) return 1;
    }
    if (extract_http_header_val(hdr, hdr_len, "Authorization", val, sizeof(val))) {
        const char *tok = val;
        if (ascii_strncaseeq(val, "Bearer ", 7U)) tok = val + 7;
        while (*tok == ' ' || *tok == '\t') tok++;
        if (strcmp(tok, st->api_key) == 0) return 1;
    }
    return 0;
}

static void handle_metrics_get(ZccStudioState *st, ZccSocket client) {
    size_t v_free = 0U, v_tot = st->vram_total_bytes;
    if (cuMemGetInfo_v2(&v_free, &v_tot) != 0 || v_tot == 0U) {
        v_tot = st->vram_total_bytes;
        v_free = 0U;
    }
    size_t v_used = (v_tot >= v_free) ? (v_tot - v_free) : 0U;
    long long now_s = studio_time64();
    long long up_s = (now_s >= st->started_at) ? (now_s - st->started_at) : 0LL;
    int dec_evolved = (!st->no_cuda_graph && st->model.head_dim <= 256U &&
                       (st->kv_type == ZCC_KV_F16 || st->kv_type == ZCC_KV_F32)) ? 1 : 0;
    unsigned int dec_nodes = (dec_evolved && st->decode_plan.body_ready)
                             ? st->decode_plan.decode_argmax_count : 0U;

    StrBuf sb;
    memset(&sb, 0, sizeof(sb));
    char line[256];
    snprintf(line, sizeof(line), "# TYPE zcc_uptime_seconds gauge\nzcc_uptime_seconds %lld\n", up_s); sb_append(&sb, line);
    snprintf(line, sizeof(line), "# TYPE zcc_requests_total counter\nzcc_requests_total %llu\n", st->req_counter); sb_append(&sb, line);
    snprintf(line, sizeof(line), "# TYPE zcc_generated_tokens_total counter\nzcc_generated_tokens_total %llu\n", st->total_gen_tokens); sb_append(&sb, line);
    snprintf(line, sizeof(line), "# TYPE zcc_embeddings_total counter\nzcc_embeddings_total %llu\n", st->total_embeddings); sb_append(&sb, line);
    snprintf(line, sizeof(line), "# TYPE zcc_api_errors_total counter\nzcc_api_errors_total %llu\n", g_studio_api_errors); sb_append(&sb, line);
    snprintf(line, sizeof(line), "# TYPE zcc_last_tok_per_sec gauge\nzcc_last_tok_per_sec %.2f\n", st->last_tps); sb_append(&sb, line);
    snprintf(line, sizeof(line), "# TYPE zcc_last_elapsed_ms gauge\nzcc_last_elapsed_ms %.2f\n", st->last_elapsed_ms); sb_append(&sb, line);
    snprintf(line, sizeof(line), "# TYPE zcc_vram_used_bytes gauge\nzcc_vram_used_bytes %llu\n", (unsigned long long)v_used); sb_append(&sb, line);
    snprintf(line, sizeof(line), "# TYPE zcc_vram_total_bytes gauge\nzcc_vram_total_bytes %llu\n", (unsigned long long)v_tot); sb_append(&sb, line);
    snprintf(line, sizeof(line), "# TYPE zcc_adapters_loaded gauge\nzcc_adapters_loaded %d\n", st->n_adapters); sb_append(&sb, line);
    snprintf(line, sizeof(line), "# TYPE zcc_decode_graph_nodes gauge\nzcc_decode_graph_nodes %u\n", dec_nodes); sb_append(&sb, line);
    snprintf(line, sizeof(line), "# TYPE zcc_gpu_sm_count gauge\nzcc_gpu_sm_count %d\n", st->gpu_sm_count); sb_append(&sb, line);
    snprintf(line, sizeof(line), "# TYPE zcc_ctx_max_tokens gauge\nzcc_ctx_max_tokens %u\n", st->eff_max_seq); sb_append(&sb, line);
    send_http_response(client, 200, "OK", "text/plain; version=0.0.4; charset=utf-8",
                       sb.data ? sb.data : "", sb.len);
    sb_free(&sb);
}

static void handle_model_get_by_id(ZccStudioState *st, ZccSocket client, const char *req_id) {
    char cr_s[32];
    long long ts = studio_time64();
    fmt_u64_dec((uint64_t)(ts > 0LL ? ts : 0LL), cr_s, sizeof(cr_s));

    int matched = 0;
    int is_lora = 0;
    if (strcmp(req_id, st->model_id) == 0) {
        matched = 1;
    } else {
        for (int i = 0; i < st->n_adapters; i++) {
            char combined[256];
            snprintf(combined, sizeof(combined), "%s:%s", st->model_id, st->adapters[i].name);
            if (strcmp(req_id, combined) == 0 || strcmp(req_id, st->adapters[i].name) == 0) {
                matched = 1;
                is_lora = 1;
                break;
            }
        }
    }
    if (!matched) {
        send_http_error(client, 404, "Not Found", "invalid_request_error", "Model or model:adapter ID not found");
        return;
    }

    StrBuf sb;
    memset(&sb, 0, sizeof(sb));
    sb_append(&sb, "{\"id\":");
    sb_append_json_str(&sb, req_id);
    sb_append(&sb, ",\"object\":\"model\",\"created\":");
    sb_append(&sb, cr_s);
    sb_append(&sb, ",\"owned_by\":");
    sb_append_json_str(&sb, is_lora ? "zkaedi-zcc-lora" : "zkaedi-zcc");
    sb_append_char(&sb, '}');
    send_http_json(client, 200, "OK", sb.data ? sb.data : "{}");
    sb_free(&sb);
}

static void handle_tokenize(ZccStudioState *st, ZccSocket client, const char *body, size_t body_len) {
    char *text = NULL;
    int add_bos = 0;
    JsonCur cur;
    cur.p = body;
    cur.end = body + body_len;
    j_ws(&cur);
    if (cur.p >= cur.end || *cur.p != '{') {
        send_http_error(client, 400, "Bad Request", "invalid_request_error", "Malformed JSON request body");
        return;
    }
    cur.p++;
    j_ws(&cur);
    if (cur.p < cur.end && *cur.p != '}') {
        for (;;) {
            char *k = j_parse_str_alloc(&cur);
            if (!k) { if (text) free(text); send_http_error(client, 400, "Bad Request", "invalid_request_error", "Malformed JSON"); return; }
            j_ws(&cur);
            if (cur.p >= cur.end || *cur.p != ':') { free(k); if (text) free(text); send_http_error(client, 400, "Bad Request", "invalid_request_error", "Malformed JSON"); return; }
            cur.p++;
            j_ws(&cur);
            if (strcmp(k, "text") == 0 || strcmp(k, "prompt") == 0 || strcmp(k, "input") == 0) {
                char *v = j_parse_str_alloc(&cur);
                if (!v) { free(k); if (text) free(text); send_http_error(client, 400, "Bad Request", "invalid_request_error", "Invalid text field"); return; }
                if (text) free(text);
                text = v;
            } else if (strcmp(k, "add_bos") == 0) {
                if (j_parse_bool(&cur, &add_bos) != 0) { free(k); if (text) free(text); send_http_error(client, 400, "Bad Request", "invalid_request_error", "Invalid add_bos boolean"); return; }
            } else {
                if (j_skip_value(&cur, 0) != 0) { free(k); if (text) free(text); send_http_error(client, 400, "Bad Request", "invalid_request_error", "Malformed JSON value"); return; }
            }
            free(k);
            j_ws(&cur);
            if (cur.p >= cur.end) { if (text) free(text); send_http_error(client, 400, "Bad Request", "invalid_request_error", "Truncated JSON"); return; }
            if (*cur.p == ',') { cur.p++; continue; }
            if (*cur.p == '}') { cur.p++; break; }
            if (text) free(text); send_http_error(client, 400, "Bad Request", "invalid_request_error", "Malformed JSON"); return;
        }
    }
    if (!text) {
        send_http_error(client, 400, "Bad Request", "invalid_request_error", "Missing 'text' field");
        return;
    }

    uint32_t *ids = NULL, cnt = 0U;
    if (tokenize_prompt(&st->model, text, add_bos ? 1 : 0, &ids, &cnt) != 0) {
        free(text);
        send_http_error(client, 400, "Bad Request", "invalid_request_error", "Tokenization failed");
        return;
    }
    free(text);

    StrBuf sb;
    memset(&sb, 0, sizeof(sb));
    char num[32];
    fmt_u64_dec((uint64_t)cnt, num, sizeof(num));
    sb_append(&sb, "{\"count\":");
    sb_append(&sb, num);
    sb_append(&sb, ",\"tokens\":[");
    for (uint32_t i = 0U; i < cnt; i++) {
        if (i > 0U) sb_append_char(&sb, ',');
        fmt_u64_dec((uint64_t)ids[i], num, sizeof(num));
        sb_append(&sb, num);
    }
    sb_append(&sb, "],\"pieces\":[");
    for (uint32_t i = 0U; i < cnt; i++) {
        if (i > 0U) sb_append_char(&sb, ',');
        sb_append_tok_piece_json(&sb, &st->model, (int)ids[i]);
    }
    sb_append(&sb, "]}");
    if (ids) free(ids);
    send_http_json(client, 200, "OK", sb.data ? sb.data : "{}");
    sb_free(&sb);
}

static void handle_detokenize(ZccStudioState *st, ZccSocket client, const char *body, size_t body_len) {
    uint32_t *toks = NULL;
    uint32_t cnt = 0U, cap = 0U;
    JsonCur cur;
    cur.p = body;
    cur.end = body + body_len;
    j_ws(&cur);
    if (cur.p >= cur.end || *cur.p != '{') {
        send_http_error(client, 400, "Bad Request", "invalid_request_error", "Malformed JSON request body");
        return;
    }
    cur.p++;
    j_ws(&cur);
    int saw_tokens = 0;
    if (cur.p < cur.end && *cur.p != '}') {
        for (;;) {
            char *k = j_parse_str_alloc(&cur);
            if (!k) { if (toks) free(toks); send_http_error(client, 400, "Bad Request", "invalid_request_error", "Malformed JSON"); return; }
            j_ws(&cur);
            if (cur.p >= cur.end || *cur.p != ':') { free(k); if (toks) free(toks); send_http_error(client, 400, "Bad Request", "invalid_request_error", "Malformed JSON"); return; }
            cur.p++;
            j_ws(&cur);
            if (strcmp(k, "tokens") == 0) {
                saw_tokens = 1;
                if (cur.p >= cur.end || *cur.p != '[') { free(k); if (toks) free(toks); send_http_error(client, 400, "Bad Request", "invalid_request_error", "'tokens' must be array"); return; }
                cur.p++;
                j_ws(&cur);
                if (cur.p < cur.end && *cur.p != ']') {
                    for (;;) {
                        double dv = 0.0;
                        if (j_parse_num(&cur, &dv) != 0 || dv < 0.0 || dv >= (double)st->model.vocab_size) {
                            free(k); if (toks) free(toks);
                            send_http_error(client, 400, "Bad Request", "invalid_request_error", "Token ID out of range");
                            return;
                        }
                        if (cnt == cap) {
                            cap = (cap == 0U) ? 64U : (cap * 2U);
                            toks = (uint32_t *)realloc(toks, (size_t)cap * sizeof(uint32_t));
                        }
                        toks[cnt++] = (uint32_t)dv;
                        j_ws(&cur);
                        if (cur.p >= cur.end) { free(k); if (toks) free(toks); send_http_error(client, 400, "Bad Request", "invalid_request_error", "Truncated JSON"); return; }
                        if (*cur.p == ',') { cur.p++; continue; }
                        if (*cur.p == ']') { cur.p++; break; }
                        free(k); if (toks) free(toks); send_http_error(client, 400, "Bad Request", "invalid_request_error", "Malformed array"); return;
                    }
                } else if (cur.p < cur.end && *cur.p == ']') {
                    cur.p++;
                }
            } else {
                if (j_skip_value(&cur, 0) != 0) { free(k); if (toks) free(toks); send_http_error(client, 400, "Bad Request", "invalid_request_error", "Malformed value"); return; }
            }
            free(k);
            j_ws(&cur);
            if (cur.p >= cur.end) { if (toks) free(toks); send_http_error(client, 400, "Bad Request", "invalid_request_error", "Truncated JSON"); return; }
            if (*cur.p == ',') { cur.p++; continue; }
            if (*cur.p == '}') { cur.p++; break; }
            if (toks) free(toks); send_http_error(client, 400, "Bad Request", "invalid_request_error", "Malformed JSON"); return;
        }
    }
    if (!saw_tokens) {
        if (toks) free(toks);
        send_http_error(client, 400, "Bad Request", "invalid_request_error", "Missing 'tokens' array");
        return;
    }

    StrBuf txt_sb;
    memset(&txt_sb, 0, sizeof(txt_sb));
    sb_append_n(&txt_sb, "", 0U);
    for (uint32_t i = 0U; i < cnt; i++) {
        char piece[256];
        size_t plen = detokenize_piece(&st->model.tok, toks[i], piece, sizeof(piece));
        if (plen >= sizeof(piece)) plen = sizeof(piece) - 1U;
        if (plen > 0U) sb_append_n(&txt_sb, piece, plen);
    }
    if (toks) free(toks);

    StrBuf out;
    memset(&out, 0, sizeof(out));
    char num[32];
    fmt_u64_dec((uint64_t)cnt, num, sizeof(num));
    sb_append(&out, "{\"text\":");
    sb_append_json_str(&out, txt_sb.data ? txt_sb.data : "");
    sb_append(&out, ",\"count\":");
    sb_append(&out, num);
    sb_append_char(&out, '}');
    sb_free(&txt_sb);
    send_http_json(client, 200, "OK", out.data ? out.data : "{}");
    sb_free(&out);
}

static void handle_embeddings(ZccStudioState *st, ZccSocket client, const char *body, size_t body_len) {
    char **inputs = (char **)calloc(32U, sizeof(char *));
    int n_inputs = 0;
    OpenAiReq meta_req;
    memset(&meta_req, 0, sizeof(meta_req));
    int req_dims = 0;
    int parse_err = 0;

    if (!inputs) {
        send_http_error(client, 500, "Internal Server Error", "server_error", "OOM allocating embeddings array");
        return;
    }

    JsonCur cur;
    cur.p = body;
    cur.end = body + body_len;
    j_ws(&cur);
    if (cur.p >= cur.end || *cur.p != '{') { parse_err = 1; goto emb_done; }
    cur.p++;
    j_ws(&cur);
    if (cur.p < cur.end && *cur.p != '}') {
        for (;;) {
            char *k = j_parse_str_alloc(&cur);
            if (!k) { parse_err = 1; goto emb_done; }
            j_ws(&cur);
            if (cur.p >= cur.end || *cur.p != ':') { free(k); parse_err = 1; goto emb_done; }
            cur.p++;
            j_ws(&cur);
            if (strcmp(k, "input") == 0) {
                if (cur.p < cur.end && *cur.p == '"') {
                    char *s = j_parse_str_alloc(&cur);
                    if (!s) { free(k); parse_err = 1; goto emb_done; }
                    inputs[n_inputs++] = s;
                } else if (cur.p < cur.end && *cur.p == '[') {
                    cur.p++;
                    j_ws(&cur);
                    if (cur.p < cur.end && *cur.p != ']') {
                        for (;;) {
                            char *s = j_parse_str_alloc(&cur);
                            if (!s || n_inputs >= 32) { if (s) free(s); free(k); parse_err = 1; goto emb_done; }
                            inputs[n_inputs++] = s;
                            j_ws(&cur);
                            if (cur.p >= cur.end) { free(k); parse_err = 1; goto emb_done; }
                            if (*cur.p == ',') { cur.p++; continue; }
                            if (*cur.p == ']') { cur.p++; break; }
                            free(k); parse_err = 1; goto emb_done;
                        }
                    } else if (cur.p < cur.end && *cur.p == ']') {
                        cur.p++;
                    }
                } else {
                    free(k); parse_err = 1; goto emb_done;
                }
            } else if (strcmp(k, "model") == 0) {
                char *v = j_parse_str_alloc(&cur);
                if (!v) { free(k); parse_err = 1; goto emb_done; }
                if (meta_req.model) free(meta_req.model);
                meta_req.model = v;
            } else if (strcmp(k, "adapter") == 0) {
                char *v = j_parse_str_alloc(&cur);
                if (!v) { free(k); parse_err = 1; goto emb_done; }
                if (meta_req.adapter) free(meta_req.adapter);
                meta_req.adapter = v;
            } else if (strcmp(k, "lora_scale") == 0) {
                double dv = 0.0;
                if (j_parse_num(&cur, &dv) != 0 || !(dv >= 0.0 && dv <= 4.0)) { free(k); parse_err = 1; goto emb_done; }
                meta_req.lora_scale = (float)dv;
                meta_req.has_lora_scale = 1;
            } else if (strcmp(k, "dimensions") == 0) {
                double dv = 0.0;
                if (j_parse_num(&cur, &dv) != 0 || dv < 1.0) { free(k); parse_err = 1; goto emb_done; }
                req_dims = (int)dv;
            } else {
                if (j_skip_value(&cur, 0) != 0) { free(k); parse_err = 1; goto emb_done; }
            }
            free(k);
            j_ws(&cur);
            if (cur.p >= cur.end) { parse_err = 1; goto emb_done; }
            if (*cur.p == ',') { cur.p++; continue; }
            if (*cur.p == '}') { cur.p++; break; }
            parse_err = 1; goto emb_done;
        }
    }

emb_done:
    if (parse_err || n_inputs <= 0) {
        for (int i = 0; i < n_inputs; i++) free(inputs[i]);
        free(inputs);
        openai_req_free(&meta_req);
        send_http_error(client, 400, "Bad Request", "invalid_request_error", "Expected non-empty 'input' string or array (1..32)");
        return;
    }

    int not_found = 0;
    int ad_idx = resolve_req_adapter_idx(st, &meta_req, &not_found);
    if (not_found) {
        for (int i = 0; i < n_inputs; i++) free(inputs[i]);
        free(inputs);
        openai_req_free(&meta_req);
        send_http_error(client, 404, "Not Found", "invalid_request_error", "Requested LoRA adapter not loaded");
        return;
    }
    float scale_mult = (ad_idx >= 0) ? (meta_req.has_lora_scale ? meta_req.lora_scale : st->adapters[ad_idx].scale_mult) : 0.0f;
    float lora_scale = (ad_idx >= 0) ? ((st->adapters[ad_idx].alpha / (float)st->adapters[ad_idx].rank) * scale_mult) : 0.0f;
    const LoraLayer *ls = (ad_idx >= 0 && lora_scale > 0.0f) ? st->adapters[ad_idx].ls : NULL;
    const char *used_adapter = (ad_idx >= 0) ? st->adapters[ad_idx].name : "base";
    openai_req_free(&meta_req);

    int use_evolved = (!st->no_cuda_graph && st->model.head_dim <= 256U &&
                       (st->kv_type == ZCC_KV_F16 || st->kv_type == ZCC_KV_F32)) ? 1 : 0;
    if (use_evolved) {
        lora_decode_plan_ensure(&st->decode_plan, &st->model, &st->act, ls, st->d_inf_h,
                                st->eff_max_seq, st->use_tc, st->kv_type);
    }

    unsigned int dim = st->model.dim;
    unsigned int out_dim = (req_dims > 0 && (unsigned int)req_dims < dim) ? (unsigned int)req_dims : dim;
    double *pool = (double *)calloc((size_t)dim, sizeof(double));
    if (!pool || !st->emb_buf) {
        if (pool) free(pool);
        for (int i = 0; i < n_inputs; i++) free(inputs[i]);
        free(inputs);
        send_http_error(client, 500, "Internal Server Error", "server_error", "OOM allocating embedding buffer");
        return;
    }

    StrBuf out;
    memset(&out, 0, sizeof(out));
    sb_append(&out, "{\"object\":\"list\",\"data\":[");
    uint64_t total_toks = 0ULL;
    g_zcc_llm_failed = 0;
    check_cu(cuEventRecord(st->ev0, NULL), "emb_ev0");

    for (int i = 0; i < n_inputs && !g_zcc_llm_failed; i++) {
        uint32_t *t_ids = NULL, t_len = 0U;
        if (tokenize_prompt(&st->model, inputs[i], 0, &t_ids, &t_len) != 0 || t_len == 0U) {
            if (t_ids) free(t_ids);
            g_zcc_llm_failed = 1;
            snprintf(g_zcc_llm_err, sizeof(g_zcc_llm_err), "Embedding input tokenized to empty");
            break;
        }
        if (t_len > st->eff_max_seq) t_len = st->eff_max_seq;
        total_toks += (uint64_t)t_len;

        check_cu(cuMemsetD8_v2(st->act.k_cache, 0, st->kv_bytes), "emb_rst_k");
        check_cu(cuMemsetD8_v2(st->act.v_cache, 0, st->kv_bytes), "emb_rst_v");
        memset(pool, 0, (size_t)dim * sizeof(double));

        uint64_t xb_p = st->act.xb, x_p = st->act.x, w_p = st->model.d_out_norm, xb2_p = st->act.xb2;
        float eps = st->model.rms_eps;
        void *prn[6] = { &xb_p, &x_p, &w_p, &xb2_p, &dim, &eps };

        for (uint32_t pi = 0U; pi < t_len && !g_zcc_llm_failed; pi++) {
            uint32_t tk = (t_ids[pi] < st->model.vocab_size) ? t_ids[pi] : 0U;
            if (use_evolved) {
                lora_decode_step_body(&st->decode_plan, &st->act, tk, pi, st->eff_max_seq, lora_scale);
                check_cu(cuLaunchKernel(dfn_rmsnorm, 1U, 1U, 1U, 256U, 1U, 1U, 0U, NULL, prn, NULL), "emb_rn");
            } else {
                gpu_forward_token_lora(&st->model, &st->act, ls, lora_scale, st->d_inf_h,
                                       tk, pi, st->eff_max_seq, st->use_tc, st->kv_type);
            }
            check_cu(cuCtxSynchronize(), "emb_sync");
            check_cu(cuMemcpyDtoH_v2(st->emb_buf, st->act.xb, (size_t)dim * 4U), "emb_dtoh");
            for (unsigned int d = 0U; d < out_dim; d++) {
                pool[d] += (double)st->emb_buf[d];
            }
        }
        free(t_ids);
        if (g_zcc_llm_failed) break;

        double inv_t = 1.0 / (double)t_len;
        double nsq = 0.0;
        for (unsigned int d = 0U; d < out_dim; d++) {
            pool[d] *= inv_t;
            nsq += pool[d] * pool[d];
        }
        double inv_norm = (nsq > 1e-24) ? (1.0 / sqrt(nsq)) : 1.0;

        char idx_s[32];
        fmt_u64_dec((uint64_t)i, idx_s, sizeof(idx_s));
        if (i > 0) sb_append_char(&out, ',');
        sb_append(&out, "{\"object\":\"embedding\",\"index\":");
        sb_append(&out, idx_s);
        sb_append(&out, ",\"embedding\":[");
        for (unsigned int d = 0U; d < out_dim; d++) {
            char fbuf[32];
            snprintf(fbuf, sizeof(fbuf), "%.6f", pool[d] * inv_norm);
            if (d > 0U) sb_append_char(&out, ',');
            sb_append(&out, fbuf);
        }
        sb_append(&out, "]}");
    }

    for (int i = 0; i < n_inputs; i++) free(inputs[i]);
    free(inputs);
    free(pool);

    if (g_zcc_llm_failed) {
        g_zcc_llm_failed = 0;
        sb_free(&out);
        send_http_error(client, 500, "Internal Server Error", "server_error", g_zcc_llm_err);
        return;
    }

    check_cu(cuEventRecord(st->ev1, NULL), "emb_ev1");
    check_cu(cuEventSynchronize(st->ev1), "emb_ev1_s");
    float ms = 0.0f;
    check_cu(cuEventElapsedTime(&ms, st->ev0, st->ev1), "emb_ms");

    st->req_counter++;
    st->total_embeddings += (unsigned long long)n_inputs;

    char pt_s[32], met_s[160];
    fmt_u64_dec(total_toks, pt_s, sizeof(pt_s));
    sb_append(&out, "],\"model\":");
    sb_append_json_str(&out, st->model_id);
    sb_append(&out, ",\"adapter\":");
    sb_append_json_str(&out, used_adapter);
    sb_append(&out, ",\"usage\":{\"prompt_tokens\":");
    sb_append(&out, pt_s);
    sb_append(&out, ",\"total_tokens\":");
    sb_append(&out, pt_s);
    snprintf(met_s, sizeof(met_s), "},\"zcc_metrics\":{\"elapsed_ms\":%.2f,\"dimensions\":%u}}", (double)ms, out_dim);
    sb_append(&out, met_s);

    send_http_json(client, 200, "OK", out.data ? out.data : "{}");
    sb_free(&out);
}

static void handle_batch(ZccStudioState *st, ZccSocket client, const char *body, size_t body_len) {
    char **items = (char **)calloc(32U, sizeof(char *));
    int n_items = 0;
    if (!items) {
        send_http_error(client, 500, "Internal Server Error", "server_error", "OOM allocating batch results");
        return;
    }

    JsonCur cur;
    cur.p = body;
    cur.end = body + body_len;
    j_ws(&cur);
    if (cur.p >= cur.end || *cur.p != '{') {
        free(items);
        send_http_error(client, 400, "Bad Request", "invalid_request_error", "Malformed JSON request body");
        return;
    }
    cur.p++;
    j_ws(&cur);
    int saw_reqs = 0;
    int parse_err = 0;
    if (cur.p < cur.end && *cur.p != '}') {
        for (;;) {
            char *k = j_parse_str_alloc(&cur);
            if (!k) { parse_err = 1; break; }
            j_ws(&cur);
            if (cur.p >= cur.end || *cur.p != ':') { free(k); parse_err = 1; break; }
            cur.p++;
            j_ws(&cur);
            if (strcmp(k, "requests") == 0) {
                saw_reqs = 1;
                if (cur.p >= cur.end || *cur.p != '[') { free(k); parse_err = 1; break; }
                cur.p++;
                j_ws(&cur);
                if (cur.p < cur.end && *cur.p != ']') {
                    for (;;) {
                        j_ws(&cur);
                        const char *obj_s = cur.p;
                        if (j_skip_value(&cur, 0) != 0 || n_items >= 32) { free(k); parse_err = 1; break; }
                        size_t olen = (size_t)(cur.p - obj_s);
                        OpenAiReq sub_req;
                        if (parse_openai_request(obj_s, olen, &sub_req) != 0) { free(k); parse_err = 1; break; }
                        char *item_json = NULL;
                        int sub_mode = (sub_req.has_messages && sub_req.n_msgs > 0U) ? STUDIO_API_CHAT : STUDIO_API_COMPLETION;
                        studio_execute_generation(st, client, &sub_req, sub_mode, &item_json);
                        openai_req_free(&sub_req);
                        if (!item_json) item_json = studio_strdup("{\"error\":{\"message\":\"Batch item execution failed\",\"type\":\"invalid_request_error\"}}");
                        items[n_items++] = item_json;
                        j_ws(&cur);
                        if (cur.p >= cur.end) { free(k); parse_err = 1; break; }
                        if (*cur.p == ',') { cur.p++; continue; }
                        if (*cur.p == ']') { cur.p++; break; }
                        free(k); parse_err = 1; break;
                    }
                    if (parse_err) break;
                } else if (cur.p < cur.end && *cur.p == ']') {
                    cur.p++;
                }
            } else {
                if (j_skip_value(&cur, 0) != 0) { free(k); parse_err = 1; break; }
            }
            free(k);
            j_ws(&cur);
            if (cur.p >= cur.end) { parse_err = 1; break; }
            if (*cur.p == ',') { cur.p++; continue; }
            if (*cur.p == '}') { cur.p++; break; }
            parse_err = 1; break;
        }
    }

    if (parse_err || !saw_reqs || n_items <= 0) {
        for (int i = 0; i < n_items; i++) free(items[i]);
        free(items);
        send_http_error(client, 400, "Bad Request", "invalid_request_error", "Expected non-empty 'requests' array (1..32)");
        return;
    }

    StrBuf out;
    memset(&out, 0, sizeof(out));
    char hdr_buf_s[96];
    snprintf(hdr_buf_s, sizeof(hdr_buf_s), "{\"object\":\"batch.result\",\"count\":%d,\"results\":[", n_items);
    sb_append(&out, hdr_buf_s);
    for (int i = 0; i < n_items; i++) {
        if (i > 0) sb_append_char(&out, ',');
        sb_append(&out, items[i]);
        free(items[i]);
    }
    free(items);
    sb_append(&out, "]}");
    send_http_json(client, 200, "OK", out.data ? out.data : "{}");
    sb_free(&out);
}

static void handle_lora_adapter_by_name(ZccStudioState *st, ZccSocket client,
                                        const char *method, const char *name) {
    if (!is_valid_adapter_name(name)) {
        send_http_error(client, 400, "Bad Request", "invalid_request_error", "Invalid LoRA adapter name");
        return;
    }
    int idx = find_adapter_idx(st, name);
    if (idx < 0) {
        send_http_error(client, 404, "Not Found", "invalid_request_error", "Adapter not loaded");
        return;
    }
    if (strcmp(method, "GET") == 0) {
        const StudioAdapter *ad = &st->adapters[idx];
        StrBuf sb;
        memset(&sb, 0, sizeof(sb));
        char num[160];
        sb_append(&sb, "{\"name\":");
        sb_append_json_str(&sb, ad->name);
        snprintf(num, sizeof(num),
                 ",\"rank\":%u,\"alpha\":%.2f,\"scale\":%.2f,\"params\":%llu,\"fnv\":\"%08x\",\"active\":%s,\"path\":",
                 ad->rank, (double)ad->alpha, (double)ad->scale_mult, (unsigned long long)ad->params,
                 ad->fnv, (st->active_idx == idx) ? "true" : "false");
        sb_append(&sb, num);
        sb_append_json_str(&sb, ad->path);
        sb_append_char(&sb, '}');
        send_http_json(client, 200, "OK", sb.data ? sb.data : "{}");
        sb_free(&sb);
        return;
    }
    if (strcmp(method, "DELETE") == 0) {
        char removed[64];
        snprintf(removed, sizeof(removed), "%s", st->adapters[idx].name);
        lora_decode_plan_invalidate(&st->decode_plan);
        if (st->adapters[idx].ls) free_lora_layers(st->adapters[idx].ls, st->model.n_layers);
        for (int i = idx; i + 1 < st->n_adapters; i++) {
            memcpy(&st->adapters[i], &st->adapters[i + 1], sizeof(StudioAdapter));
        }
        st->n_adapters--;
        memset(&st->adapters[st->n_adapters], 0, sizeof(StudioAdapter));
        if (st->active_idx == idx) st->active_idx = -1;
        else if (st->active_idx > idx) st->active_idx--;

        StrBuf sb;
        memset(&sb, 0, sizeof(sb));
        char tmp[64];
        sb_append(&sb, "{\"status\":\"ok\",\"deleted\":");
        sb_append_json_str(&sb, removed);
        sb_append(&sb, ",\"active\":");
        sb_append_json_str(&sb, active_adapter_name_str(st));
        snprintf(tmp, sizeof(tmp), ",\"count\":%d}", st->n_adapters);
        sb_append(&sb, tmp);
        send_http_json(client, 200, "OK", sb.data ? sb.data : "{}");
        sb_free(&sb);
        return;
    }
    send_http_error(client, 405, "Method Not Allowed", "invalid_request_error", "Use GET or DELETE on /v1/lora/adapters/{name}");
}

static const char *find_header_end(const char *buf, size_t len, size_t *out_hdr_len) {
    for (size_t i = 0U; i + 3U < len; i++) {
        if (buf[i] == '\r' && buf[i + 1U] == '\n' && buf[i + 2U] == '\r' && buf[i + 3U] == '\n') {
            *out_hdr_len = i + 4U;
            return buf + i + 4U;
        }
    }
    for (size_t i = 0U; i + 1U < len; i++) {
        if (buf[i] == '\n' && buf[i + 1U] == '\n') {
            *out_hdr_len = i + 2U;
            return buf + i + 2U;
        }
    }
    return NULL;
}

static int ascii_strncaseeq(const char *a, const char *b, size_t n) {
    for (size_t i = 0U; i < n; i++) {
        unsigned char ca = (unsigned char)a[i];
        unsigned char cb = (unsigned char)b[i];
        if (ca >= 'A' && ca <= 'Z') ca = (unsigned char)(ca + ('a' - 'A'));
        if (cb >= 'A' && cb <= 'Z') cb = (unsigned char)(cb + ('a' - 'A'));
        if (ca != cb) return 0;
        if (ca == '\0') return 1;
    }
    return 1;
}

static int parse_content_length_hdr(const char *hdr, size_t hdr_len, int *out_has_cl, uint64_t *out_cl) {
    *out_has_cl = 0;
    *out_cl = 0ULL;
    size_t pos = 0U;
    while (pos < hdr_len && hdr[pos] != '\n') pos++;
    if (pos < hdr_len) pos++;
    while (pos < hdr_len) {
        size_t line_start = pos;
        while (pos < hdr_len && hdr[pos] != '\n') pos++;
        size_t line_end = pos;
        if (line_end > line_start && hdr[line_end - 1U] == '\r') line_end--;
        if (pos < hdr_len) pos++;
        if (line_end == line_start) break;
        size_t llen = line_end - line_start;
        if (llen >= 15U && ascii_strncaseeq(hdr + line_start, "Content-Length:", 15U)) {
            const char *vp = hdr + line_start + 15U;
            const char *vend = hdr + line_end;
            while (vp < vend && (*vp == ' ' || *vp == '\t')) vp++;
            while (vend > vp && (vend[-1] == ' ' || vend[-1] == '\t')) vend--;
            if (vp >= vend) return -1;
            uint64_t v = 0ULL;
            for (const char *q = vp; q < vend; q++) {
                if (*q < '0' || *q > '9') return -1;
                uint64_t d = (uint64_t)(*q - '0');
                if (v > (1000000000ULL - d) / 10ULL) {
                    *out_has_cl = 1;
                    *out_cl = 1000000000ULL;
                    return 0;
                }
                v = v * 10ULL + d;
            }
            *out_has_cl = 1;
            *out_cl = v;
        }
    }
    return 0;
}

static void derive_clean_name(const char *path, const char *ext, char *out, size_t cap, const char *fallback) {
    if (!path || !path[0]) {
        snprintf(out, cap, "%s", fallback);
        return;
    }
    const char *base = path;
    for (const char *p = path; *p; p++) {
        if (*p == '/' || *p == '\\') base = p + 1;
    }
    snprintf(out, cap, "%s", base[0] ? base : fallback);
    size_t len = strlen(out), elen = ext ? strlen(ext) : 0U;
    if (elen > 0U && len > elen && strcmp(out + len - elen, ext) == 0) out[len - elen] = '\0';
}

int main(int argc, char **argv) {
    const char *model_path = NULL;
    const char *load_lora_paths[ZCC_MAX_STUDIO_ADAPTERS];
    const char *load_lora_names[ZCC_MAX_STUDIO_ADAPTERS];
    int n_load_loras = 0;
    const char *pending_lora_name = NULL;
    for (int li = 0; li < ZCC_MAX_STUDIO_ADAPTERS; li++) {
        load_lora_paths[li] = NULL;
        load_lora_names[li] = NULL;
    }
    const char *lora_dir_arg = "build_win64";
    const char *host_ip = "127.0.0.1";
    const char *model_name_override = NULL;
    const char *api_key_arg = NULL;
    int port = 8080;
    int kv_type = ZCC_KV_F32;
    int ctx_override = 512;
    int max_requests = 0;
    int allow_shutdown = 0;
    int no_cuda_graph = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--model") == 0 && i + 1 < argc) { model_path = argv[++i]; }
        else if (strcmp(argv[i], "--load-lora") == 0 && i + 1 < argc) {
            if (n_load_loras >= ZCC_MAX_STUDIO_ADAPTERS) {
                fprintf(stderr, "INVALID_ARG: too many --load-lora arguments (max %d)\n", ZCC_MAX_STUDIO_ADAPTERS);
                return 1;
            }
            load_lora_paths[n_load_loras] = argv[++i];
            load_lora_names[n_load_loras] = pending_lora_name;
            pending_lora_name = NULL;
            n_load_loras++;
        } else if (strcmp(argv[i], "--lora-name") == 0 && i + 1 < argc) {
            const char *nm_arg = argv[++i];
            if (n_load_loras > 0 && load_lora_names[n_load_loras - 1] == NULL) {
                load_lora_names[n_load_loras - 1] = nm_arg;
            } else {
                pending_lora_name = nm_arg;
            }
        }
        else if (strcmp(argv[i], "--lora-dir") == 0 && i + 1 < argc) { lora_dir_arg = argv[++i]; }
        else if (strcmp(argv[i], "--host") == 0 && i + 1 < argc) { host_ip = argv[++i]; }
        else if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            if (parse_int_arg(argv[++i], "--port", 0, 65535, &port) != 0) return 1;
        } else if (strcmp(argv[i], "--kv-type") == 0 && i + 1 < argc) {
            const char *kv_arg = argv[++i];
            if (strcmp(kv_arg, "f32") == 0 || strcmp(kv_arg, "0") == 0) kv_type = ZCC_KV_F32;
            else if (strcmp(kv_arg, "f16") == 0 || strcmp(kv_arg, "1") == 0) kv_type = ZCC_KV_F16;
            else { fprintf(stderr, "INVALID_ARG: unsupported --kv-type '%s' (use f32 or f16)\n", kv_arg); return 1; }
        } else if (strcmp(argv[i], "--ctx") == 0 && i + 1 < argc) {
            if (parse_int_arg(argv[++i], "--ctx", 16, 32768, &ctx_override) != 0) return 1;
        } else if (strcmp(argv[i], "--model-name") == 0 && i + 1 < argc) {
            model_name_override = argv[++i];
        } else if (strcmp(argv[i], "--max-requests") == 0 && i + 1 < argc) {
            if (parse_int_arg(argv[++i], "--max-requests", 0, 1000000, &max_requests) != 0) return 1;
        } else if (strcmp(argv[i], "--allow-shutdown") == 0) {
            allow_shutdown = 1;
        } else if (strcmp(argv[i], "--no-cuda-graph") == 0) {
            no_cuda_graph = 1;
        } else if (strcmp(argv[i], "--api-key") == 0 && i + 1 < argc) {
            api_key_arg = argv[++i];
        } else {
            fprintf(stderr, "INVALID_ARG: unknown studio arg '%s'\n", argv[i]);
            return 1;
        }
    }

    if (!model_path) {
        fprintf(stderr, "Usage: %s --model <model.gguf> [--load-lora <a.zcc_lora>] [--host 127.0.0.1] [--port 8080] [--kv-type f32|f16] [--ctx N] [--allow-shutdown] [--no-cuda-graph]\n", argv[0]);
        return 1;
    }

    uint32_t bind_addr = 0U;
    if (strcmp(host_ip, "0.0.0.0") != 0) {
        bind_addr = inet_addr(host_ip);
        if (bind_addr == 0xFFFFFFFFU && strcmp(host_ip, "255.255.255.255") != 0) {
            fprintf(stderr, "INVALID_HOST: expected IPv4 address, got '%s'\n", host_ip);
            return 1;
        }
    }

    ZccStudioState *st = (ZccStudioState *)calloc(1U, sizeof(ZccStudioState));
    if (!st) { fprintf(stderr, "OOM allocating studio state\n"); return 1; }
    st->active_idx = -1;
    st->kv_type = kv_type;
    st->started_at = studio_time64();
    if (api_key_arg && api_key_arg[0]) {
        snprintf(st->api_key, sizeof(st->api_key), "%s", api_key_arg);
    }
    snprintf(st->adapter_dir, sizeof(st->adapter_dir), "%s",
             (lora_dir_arg && lora_dir_arg[0]) ? lora_dir_arg : "build_win64");
    if (model_name_override && model_name_override[0]) {
        snprintf(st->model_id, sizeof(st->model_id), "%s", model_name_override);
    } else {
        derive_clean_name(model_path, ".gguf", st->model_id, sizeof(st->model_id), "zkaedi-zcc-llm");
    }

    if (cuInit(0U) != 0) { fprintf(stderr, "cuInit failed\n"); free(st); return 1; }
    int dev_cnt = 0, dev = 0;
    if (cuDeviceGetCount(&dev_cnt) != 0 || dev_cnt <= 0 || cuDeviceGet(&dev, 0) != 0) {
        fprintf(stderr, "No CUDA device\n");
        free(st);
        return 1;
    }
    if (cuCtxCreate_v2(&st->ctx, 0U, dev) != 0) { fprintf(stderr, "cuCtxCreate_v2 failed\n"); free(st); return 1; }
    snprintf(st->gpu_name, sizeof(st->gpu_name), "NVIDIA CUDA GPU");
    cuDeviceGetName(st->gpu_name, (int)sizeof(st->gpu_name), dev);
    cuDeviceGetAttribute(&st->gpu_cc_major, 75, dev);
    cuDeviceGetAttribute(&st->gpu_cc_minor, 76, dev);
    cuDeviceGetAttribute(&st->gpu_sm_count, 16, dev);
    cuDriverGetVersion(&st->cuda_driver_ver);
    cuDeviceTotalMem_v2(&st->vram_total_bytes, dev);

    int aot_t = 0;
    uint64_t meta[16];
    unsigned int ptx_b = 0U;
    if (load_ptx_module(NULL, g_gpu_llm_kernels_ptx, (unsigned int)(sizeof(g_gpu_llm_kernels_ptx) - 1U), 0,
                        &aot_t, &st->mod_llm, meta, &ptx_b) != 0 || load_generic_kernels(st->mod_llm) != 0) {
        cuCtxDestroy_v2(st->ctx); free(st); return 1;
    }
    if (load_ptx_module(NULL, g_gpu_lora_kernels_ptx, (unsigned int)(sizeof(g_gpu_lora_kernels_ptx) - 1U), 0,
                        &aot_t, &st->mod_lora, meta, &ptx_b) != 0 || load_lora_kernels(st->mod_lora) != 0) {
        cuModuleUnload(st->mod_llm); cuCtxDestroy_v2(st->ctx); free(st); return 1;
    }

    if (parse_and_stream_gguf_ex(model_path, NULL, 0U, 0ULL, 0ULL, NULL, &st->model, 0, NULL, 0, 0ULL) != 0) {
        free_model_gpu_mem(&st->model); free_llama_model(&st->model);
        cuModuleUnload(st->mod_lora); cuModuleUnload(st->mod_llm); cuCtxDestroy_v2(st->ctx); free(st);
        return 1;
    }

    for (int li = 0; li < n_load_loras; li++) {
        const char *lpath = load_lora_paths[li];
        const char *lname = load_lora_names[li];
        char nm[64];
        if (lname && is_valid_adapter_name(lname)) {
            snprintf(nm, sizeof(nm), "%s", lname);
        } else {
            derive_clean_name(lpath, ".zcc_lora", nm, sizeof(nm), "default_lora");
            if (!is_valid_adapter_name(nm)) snprintf(nm, sizeof(nm), "default_lora_%d", li);
        }
        g_zcc_llm_failed = 0;
        g_last_lora_fnv = 0U;
        unsigned int rk = 0U;
        float al = 0.0f;
        LoraLayer *ls = load_lora_adapter(lpath, &st->model, &rk, &al);
        if (!ls || g_zcc_llm_failed) {
            if (ls) free_lora_layers(ls, st->model.n_layers);
            for (int lj = 0; lj < st->n_adapters; lj++) {
                if (st->adapters[lj].ls) free_lora_layers(st->adapters[lj].ls, st->model.n_layers);
            }
            free_model_gpu_mem(&st->model); free_llama_model(&st->model);
            cuModuleUnload(st->mod_lora); cuModuleUnload(st->mod_llm); cuCtxDestroy_v2(st->ctx); free(st);
            return 1;
        }
        upsert_adapter(st, nm, lpath, ls, rk, al, g_last_lora_fnv, (li == 0) ? 1 : 0);
    }

    st->eff_max_seq = (unsigned int)ctx_override;
    if (st->eff_max_seq > st->model.max_seq_len) st->eff_max_seq = st->model.max_seq_len;
    uint64_t row_kv = (kv_type == ZCC_KV_F16) ? ((uint64_t)st->model.kv_dim * 2ULL) : ((uint64_t)st->model.kv_dim * 4ULL);
    st->kv_bytes = (size_t)((uint64_t)st->model.n_layers * (uint64_t)st->eff_max_seq * row_kv);
    st->use_tc = (st->model.weight_type_primary == ZCC_GGML_F16 && (st->model.dim % 16U) == 0U) ? 1 : 0;

    check_cu(cuMemAlloc_v2(&st->act.k_cache, st->kv_bytes), "st.kc");
    check_cu(cuMemAlloc_v2(&st->act.v_cache, st->kv_bytes), "st.vc");
    check_cu(cuMemAlloc_v2(&st->act.x, (size_t)st->model.dim * 4U), "st.x");
    check_cu(cuMemAlloc_v2(&st->act.xb, (size_t)st->model.dim * 4U), "st.xb");
    check_cu(cuMemAlloc_v2(&st->act.xb2, (size_t)st->model.dim * 4U), "st.xb2");
    check_cu(cuMemAlloc_v2(&st->act.x_f16, (size_t)(st->model.hidden_dim > st->model.dim ? st->model.hidden_dim : st->model.dim) * 2U), "st.xf16");
    check_cu(cuMemAlloc_v2(&st->act.q, (size_t)st->model.dim * 4U), "st.q");
    check_cu(cuMemAlloc_v2(&st->act.k, (size_t)st->model.kv_dim * 4U), "st.k");
    check_cu(cuMemAlloc_v2(&st->act.v, (size_t)st->model.kv_dim * 4U), "st.v");
    if (st->model.head_dim > 256U) check_cu(cuMemAlloc_v2(&st->act.att_scratch, (size_t)st->model.n_heads * st->eff_max_seq * 4U), "st.att");
    check_cu(cuMemAlloc_v2(&st->act.hb, (size_t)st->model.hidden_dim * 4U), "st.hb");
    check_cu(cuMemAlloc_v2(&st->act.hb2, (size_t)st->model.hidden_dim * 4U), "st.hb2");
    check_cu(cuMemAlloc_v2(&st->act.logits, (size_t)st->model.vocab_size * 4U), "st.logits");
    check_cu(cuMemAlloc_v2(&st->act.step_args, 16U), "st.step_args");
    check_cu(cuMemAlloc_v2(&st->act.argmax_ids, (size_t)(2048U + 4U) * 4U), "st.argmax_ids");
    check_cu(cuMemAlloc_v2(&st->act.argmax_vals, (size_t)(2048U + 4U) * 4U), "st.argmax_vals");
    check_cu(cuMemAlloc_v2(&st->act.draft_ids, (size_t)2048U * 4U), "st.argmax_bad");
    check_cu(cuMemAlloc_v2(&st->d_inf_h, 4U * 64U * 4U), "st.inf_lora_h");
    check_cu(cuEventCreate(&st->ev0, 0U), "st_ev0");
    check_cu(cuEventCreate(&st->ev1, 0U), "st_ev1");
    st->no_cuda_graph = no_cuda_graph;
    lora_decode_plan_init(&st->decode_plan, !no_cuda_graph);
    if (!g_zcc_llm_failed && !st->no_cuda_graph && st->model.head_dim <= 256U &&
        (st->kv_type == ZCC_KV_F16 || st->kv_type == ZCC_KV_F32)) {
        const LoraLayer *init_ls = (st->active_idx >= 0) ? st->adapters[st->active_idx].ls : NULL;
        lora_decode_plan_ensure(&st->decode_plan, &st->model, &st->act, init_ls, st->d_inf_h,
                                st->eff_max_seq, st->use_tc, st->kv_type);
    }
    if (g_zcc_llm_failed) {
        fprintf(stderr, "FATAL CUDA init: %s\n", g_zcc_llm_err);
        return 1;
    }

    st->h_logits = (float *)malloc((size_t)st->model.vocab_size * 4U);
    st->work_logits = (float *)malloc((size_t)st->model.vocab_size * 4U);
    st->h_probs = (float *)malloc((size_t)st->model.vocab_size * 4U);
    st->cands = (SampleCand *)malloc((size_t)st->model.vocab_size * sizeof(SampleCand));
    st->hist = (uint32_t *)malloc(((size_t)st->eff_max_seq * 2U + 128U) * sizeof(uint32_t));
    st->gen_ids = (int *)malloc(((size_t)st->eff_max_seq + 32U) * sizeof(int));
    st->gen_text_cap = 16384U;
    st->gen_text = (char *)malloc(st->gen_text_cap);
    st->emb_buf = (float *)malloc((size_t)st->model.dim * 4U);

    if (studio_net_init() != 0) {
        fprintf(stderr, "Network init failed\n");
        return 1;
    }

    ZccSocket listen_sock = socket(ZCC_AF_INET, ZCC_SOCK_STREAM, ZCC_IPPROTO_TCP);
    if (listen_sock == ZCC_INVALID_SOCKET) {
        fprintf(stderr, "SOCKET_ERROR: err=%d\n", studio_net_err());
        studio_net_cleanup();
        return 1;
    }
    studio_set_listen_reuse(listen_sock);

    ZccSockAddrIn addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = ZCC_AF_INET;
    addr.sin_port = htons((uint16_t)port);
    addr.sin_addr = bind_addr;
    if (bind(listen_sock, &addr, (ZccSockLen)sizeof(addr)) != 0) {
        fprintf(stderr, "BIND_ERROR host=%s port=%d err=%d\n", host_ip, port, studio_net_err());
        studio_sock_close(listen_sock);
        studio_net_cleanup();
        return 1;
    }

    ZccSockAddrIn bound_addr;
    memset(&bound_addr, 0, sizeof(bound_addr));
    ZccSockLen bound_len = (ZccSockLen)sizeof(bound_addr);
    if (getsockname(listen_sock, &bound_addr, &bound_len) == 0) {
        port = (int)ntohs(bound_addr.sin_port);
    }

    if (listen(listen_sock, 16) != 0) {
        fprintf(stderr, "LISTEN_ERROR err=%d\n", studio_net_err());
        studio_sock_close(listen_sock);
        studio_net_cleanup();
        return 1;
    }

    printf("ZCC_LORA_STUDIO_READY url=http://%s:%d model=%s kv=%s ctx=%u adapters=%d active=%s\n",
           host_ip, port, st->model_id, kv_type_name_str(st->kv_type), st->eff_max_seq,
           st->n_adapters, active_adapter_name_str(st));
    fflush(stdout);

    int hdr_cap = 65536;
    char *hdr_buf = (char *)malloc((size_t)hdr_cap + 1U);
    int running = 1;
    int served_total = 0;

    while (running) {
        ZccSocket client = accept(listen_sock, NULL, NULL);
        if (client == ZCC_INVALID_SOCKET) break;

        studio_set_client_sockopts(client);

        size_t got = 0U, hdr_len = 0U;
        const char *hdr_end = NULL;
        while (got < (size_t)hdr_cap) {
            long n = studio_recv(client, hdr_buf + got, hdr_cap - (int)got);
            if (n <= 0L) break;
            got += (size_t)n;
            hdr_buf[got] = '\0';
            hdr_end = find_header_end(hdr_buf, got, &hdr_len);
            if (hdr_end) break;
        }

        if (!hdr_end) {
            if (got >= (size_t)hdr_cap) {
                send_http_error(client, 413, "Payload Too Large", "invalid_request_error", "HTTP headers exceed 64 KB");
            } else if (got > 0U) {
                send_http_error(client, 400, "Bad Request", "invalid_request_error", "Malformed HTTP request");
            }
            shutdown(client, ZCC_SD_BOTH);
            studio_sock_close(client);
            continue;
        }

        char method[16], raw_path[256];
        method[0] = '\0'; raw_path[0] = '\0';
        size_t rl = 0U;
        while (rl < hdr_len && hdr_buf[rl] != '\r' && hdr_buf[rl] != '\n') rl++;
        const char *sp1 = (const char *)memchr(hdr_buf, ' ', rl);
        const char *sp2 = sp1 ? (const char *)memchr(sp1 + 1, ' ', rl - (size_t)((sp1 + 1) - hdr_buf)) : NULL;
        if (!sp1 || !sp2 || sp1 == hdr_buf) {
            send_http_error(client, 400, "Bad Request", "invalid_request_error", "Malformed HTTP request line");
            shutdown(client, ZCC_SD_BOTH);
            studio_sock_close(client);
            continue;
        }
        size_t mlen = (size_t)(sp1 - hdr_buf);
        if (mlen >= sizeof(method)) mlen = sizeof(method) - 1U;
        memcpy(method, hdr_buf, mlen); method[mlen] = '\0';

        size_t plen = (size_t)(sp2 - (sp1 + 1));
        if (plen >= sizeof(raw_path)) plen = sizeof(raw_path) - 1U;
        memcpy(raw_path, sp1 + 1, plen); raw_path[plen] = '\0';
        char *qmark = strchr(raw_path, '?');
        if (qmark) *qmark = '\0';

        int has_cl = 0;
        uint64_t cl_val = 0ULL;
        if (parse_content_length_hdr(hdr_buf, hdr_len, &has_cl, &cl_val) != 0) {
            send_http_error(client, 400, "Bad Request", "invalid_request_error", "Invalid Content-Length header");
            shutdown(client, ZCC_SD_BOTH);
            studio_sock_close(client);
            continue;
        }
        if (has_cl && cl_val > 1048576ULL) {
            send_http_error(client, 413, "Payload Too Large", "invalid_request_error", "Request body exceeds 1 MiB limit");
            shutdown(client, ZCC_SD_BOTH);
            studio_sock_close(client);
            continue;
        }

        size_t body_len = has_cl ? (size_t)cl_val : 0U;
        char *body_buf = NULL;
        int body_err = 0;
        if (body_len > 0U) {
            body_buf = (char *)malloc(body_len + 1U);
            if (!body_buf) {
                send_http_error(client, 500, "Internal Server Error", "server_error", "OOM reading request body");
                shutdown(client, ZCC_SD_BOTH);
                studio_sock_close(client);
                continue;
            }
            size_t already = got - hdr_len;
            size_t copied = (already < body_len) ? already : body_len;
            if (copied > 0U) memcpy(body_buf, hdr_end, copied);
            while (copied < body_len) {
                long n = studio_recv(client, body_buf + copied, (int)(body_len - copied));
                if (n <= 0L) { body_err = 1; break; }
                copied += (size_t)n;
            }
            body_buf[body_len] = '\0';
        }

        if (body_err) {
            if (body_buf) free(body_buf);
            send_http_error(client, 400, "Bad Request", "invalid_request_error", "Truncated HTTP request body");
            shutdown(client, ZCC_SD_BOTH);
            studio_sock_close(client);
            continue;
        }

        if (strcmp(method, "OPTIONS") == 0) {
            send_http_response(client, 204, "No Content", "text/plain", "", 0U);
        } else if (st->api_key[0] != '\0' &&
                   (strncmp(raw_path, "/v1/", 4U) == 0 || strcmp(raw_path, "/shutdown") == 0) &&
                   !check_api_auth(st, hdr_buf, hdr_len)) {
            send_http_error(client, 401, "Unauthorized", "authentication_error", "Invalid or missing API key");
        } else if (strcmp(raw_path, "/") == 0) {
            if (strcmp(method, "GET") != 0) send_http_error(client, 405, "Method Not Allowed", "invalid_request_error", "Use GET for /");
            else send_http_response(client, 200, "OK", "text/html; charset=utf-8",
                                    (const char *)g_lora_studio_ui_html, (size_t)(sizeof(g_lora_studio_ui_html) - 1U));
        } else if (strcmp(raw_path, "/runner") == 0) {
            if (strcmp(method, "GET") != 0) send_http_error(client, 405, "Method Not Allowed", "invalid_request_error", "Use GET for /runner");
            else send_http_response(client, 200, "OK", "text/html; charset=utf-8",
                                    (const char *)g_runner_game_ui_html, (size_t)(sizeof(g_runner_game_ui_html) - 1U));
        } else if (strcmp(raw_path, "/svg") == 0) {
            if (strcmp(method, "GET") != 0) send_http_error(client, 405, "Method Not Allowed", "invalid_request_error", "Use GET for /svg");
            else send_http_response(client, 200, "OK", "text/html; charset=utf-8",
                                    (const char *)g_svg_forge_ui_html, (size_t)(sizeof(g_svg_forge_ui_html) - 1U));
        } else if (strcmp(raw_path, "/mario") == 0) {
            if (strcmp(method, "GET") != 0) send_http_error(client, 405, "Method Not Allowed", "invalid_request_error", "Use GET for /mario");
            else send_http_response(client, 200, "OK", "text/html; charset=utf-8",
                                    (const char *)g_mario_game_ui_html, (size_t)(sizeof(g_mario_game_ui_html) - 1U));
        } else if (strcmp(raw_path, "/docs") == 0) {
            if (strcmp(method, "GET") != 0) send_http_error(client, 405, "Method Not Allowed", "invalid_request_error", "Use GET for /docs");
            else send_http_response(client, 200, "OK", "text/html; charset=utf-8",
                                    (const char *)g_api_docs_ui_html, (size_t)(sizeof(g_api_docs_ui_html) - 1U));
        } else if (strcmp(raw_path, "/openapi.json") == 0) {
            if (strcmp(method, "GET") != 0) send_http_error(client, 405, "Method Not Allowed", "invalid_request_error", "Use GET for /openapi.json");
            else send_http_response(client, 200, "OK", "application/json; charset=utf-8",
                                    (const char *)g_openapi_spec_json, (size_t)(sizeof(g_openapi_spec_json) - 1U));
        } else if (strcmp(raw_path, "/metrics") == 0) {
            if (strcmp(method, "GET") != 0) send_http_error(client, 405, "Method Not Allowed", "invalid_request_error", "Use GET for /metrics");
            else handle_metrics_get(st, client);
        } else if (strcmp(raw_path, "/favicon.ico") == 0) {
            send_http_response(client, 204, "No Content", "image/x-icon", "", 0U);
        } else if (strcmp(raw_path, "/health") == 0) {
            if (strcmp(method, "GET") != 0) {
                send_http_error(client, 405, "Method Not Allowed", "invalid_request_error", "Use GET for /health");
            } else {
                size_t v_free = 0U, v_tot = st->vram_total_bytes;
                if (cuMemGetInfo_v2(&v_free, &v_tot) != 0 || v_tot == 0U) {
                    v_tot = st->vram_total_bytes;
                    v_free = 0U;
                }
                size_t v_used = (v_tot >= v_free) ? (v_tot - v_free) : 0U;
                double v_free_mib = (double)v_free / (1024.0 * 1024.0);
                double v_used_mib = (double)v_used / (1024.0 * 1024.0);
                double v_tot_mib = (double)v_tot / (1024.0 * 1024.0);
                double kv_mib = (double)(st->kv_bytes * 2U) / (1024.0 * 1024.0);
                double active_sc = (st->active_idx >= 0) ? (double)st->adapters[st->active_idx].scale_mult : 0.0;
                long long now_s = studio_time64();
                long long up_s = (now_s >= st->started_at) ? (now_s - st->started_at) : 0LL;
                int dec_evolved = (!st->no_cuda_graph && st->model.head_dim <= 256U &&
                                   (st->kv_type == ZCC_KV_F16 || st->kv_type == ZCC_KV_F32)) ? 1 : 0;
                unsigned int dec_nodes = (dec_evolved && st->decode_plan.body_ready)
                                         ? st->decode_plan.decode_argmax_count : 0U;

                StrBuf hb;
                memset(&hb, 0, sizeof(hb));
                char tmp[1152];
                sb_append(&hb, "{\"status\":\"ok\",\"model\":");
                sb_append_json_str(&hb, st->model_id);
                sb_append(&hb, ",\"arch\":");
                sb_append_json_str(&hb, st->model.arch);
                sb_append(&hb, ",\"active_adapter\":");
                sb_append_json_str(&hb, active_adapter_name_str(st));
                sb_append(&hb, ",\"gpu_name\":");
                sb_append_json_str(&hb, st->gpu_name);
                sb_append(&hb, ",\"adapter_dir\":");
                sb_append_json_str(&hb, st->adapter_dir);
                snprintf(tmp, sizeof(tmp),
                         ",\"n_layers\":%u,\"dim\":%u,\"hidden_dim\":%u,\"n_heads\":%u,\"n_kv_heads\":%u,\"vocab_size\":%u,\"kv_type\":\"%s\",\"eff_max_seq\":%u,\"kv_cache_mib\":%.2f,\"adapter_count\":%d,\"active_scale\":%.2f,\"requests_served\":%llu,\"last_tok_per_sec\":%.2f,\"last_gen_tokens\":%d,\"last_prompt_tokens\":%u,\"last_elapsed_ms\":%.2f,\"total_gen_tokens\":%llu,\"gpu_cc\":\"%d.%d\",\"gpu_sm_count\":%d,\"cuda_driver\":%d,\"vram_free_mib\":%.1f,\"vram_used_mib\":%.1f,\"vram_total_mib\":%.1f,\"uptime_sec\":%lld,\"decoder_mode\":\"%s\",\"decode_graph_nodes\":%u}",
                         st->model.n_layers, st->model.dim, st->model.hidden_dim, st->model.n_heads, st->model.n_kv_heads,
                         st->model.vocab_size, kv_type_name_str(st->kv_type), st->eff_max_seq, kv_mib,
                         st->n_adapters, active_sc, st->req_counter, st->last_tps, st->last_gen_tokens,
                         st->last_prompt_tokens, st->last_elapsed_ms, st->total_gen_tokens,
                         st->gpu_cc_major, st->gpu_cc_minor, st->gpu_sm_count, st->cuda_driver_ver,
                         v_free_mib, v_used_mib, v_tot_mib, up_s,
                         dec_evolved ? "cuda_graph_q4k_8r" : "legacy_direct", dec_nodes);
                sb_append(&hb, tmp);
                send_http_json(client, 200, "OK", hb.data ? hb.data : "{}");
                sb_free(&hb);
            }
        } else if (strcmp(raw_path, "/v1/models") == 0) {
            if (strcmp(method, "GET") != 0) {
                send_http_error(client, 405, "Method Not Allowed", "invalid_request_error", "Use GET for /v1/models");
            } else {
                StrBuf mb;
                memset(&mb, 0, sizeof(mb));
                char cr_s[32];
                long long ts = studio_time64();
                fmt_u64_dec((uint64_t)(ts > 0LL ? ts : 0LL), cr_s, sizeof(cr_s));
                sb_append(&mb, "{\"object\":\"list\",\"data\":[{\"id\":");
                sb_append_json_str(&mb, st->model_id);
                sb_append(&mb, ",\"object\":\"model\",\"created\":");
                sb_append(&mb, cr_s);
                sb_append(&mb, ",\"owned_by\":\"zkaedi-zcc\"}");
                for (int i = 0; i < st->n_adapters; i++) {
                    char combined[256];
                    snprintf(combined, sizeof(combined), "%s:%s", st->model_id, st->adapters[i].name);
                    sb_append(&mb, ",{\"id\":");
                    sb_append_json_str(&mb, combined);
                    sb_append(&mb, ",\"object\":\"model\",\"created\":");
                    sb_append(&mb, cr_s);
                    sb_append(&mb, ",\"owned_by\":\"zkaedi-zcc-lora\"}");
                }
                sb_append(&mb, "]}");
                send_http_json(client, 200, "OK", mb.data ? mb.data : "{}");
                sb_free(&mb);
            }
        } else if (strncmp(raw_path, "/v1/models/", 11U) == 0 && raw_path[11] != '\0') {
            if (strcmp(method, "GET") != 0) send_http_error(client, 405, "Method Not Allowed", "invalid_request_error", "Use GET for /v1/models/{id}");
            else handle_model_get_by_id(st, client, raw_path + 11);
        } else if (strcmp(raw_path, "/v1/chat/completions") == 0) {
            if (strcmp(method, "POST") != 0) send_http_error(client, 405, "Method Not Allowed", "invalid_request_error", "Use POST for /v1/chat/completions");
            else if (!body_buf || body_len == 0U) send_http_error(client, 400, "Bad Request", "invalid_request_error", "Empty JSON request body");
            else handle_openai_inference(st, client, body_buf, body_len, STUDIO_API_CHAT);
        } else if (strcmp(raw_path, "/v1/completions") == 0) {
            if (strcmp(method, "POST") != 0) send_http_error(client, 405, "Method Not Allowed", "invalid_request_error", "Use POST for /v1/completions");
            else if (!body_buf || body_len == 0U) send_http_error(client, 400, "Bad Request", "invalid_request_error", "Empty JSON request body");
            else handle_openai_inference(st, client, body_buf, body_len, STUDIO_API_COMPLETION);
        } else if (strcmp(raw_path, "/v1/messages") == 0) {
            if (strcmp(method, "POST") != 0) send_http_error(client, 405, "Method Not Allowed", "invalid_request_error", "Use POST for /v1/messages");
            else if (!body_buf || body_len == 0U) send_http_error(client, 400, "Bad Request", "invalid_request_error", "Empty JSON request body");
            else handle_openai_inference(st, client, body_buf, body_len, STUDIO_API_ANTHROPIC);
        } else if (strcmp(raw_path, "/v1/responses") == 0) {
            if (strcmp(method, "POST") != 0) send_http_error(client, 405, "Method Not Allowed", "invalid_request_error", "Use POST for /v1/responses");
            else if (!body_buf || body_len == 0U) send_http_error(client, 400, "Bad Request", "invalid_request_error", "Empty JSON request body");
            else handle_openai_inference(st, client, body_buf, body_len, STUDIO_API_RESPONSES);
        } else if (strcmp(raw_path, "/v1/embeddings") == 0) {
            if (strcmp(method, "POST") != 0) send_http_error(client, 405, "Method Not Allowed", "invalid_request_error", "Use POST for /v1/embeddings");
            else if (!body_buf || body_len == 0U) send_http_error(client, 400, "Bad Request", "invalid_request_error", "Empty JSON request body");
            else handle_embeddings(st, client, body_buf, body_len);
        } else if (strcmp(raw_path, "/v1/tokenize") == 0) {
            if (strcmp(method, "POST") != 0) send_http_error(client, 405, "Method Not Allowed", "invalid_request_error", "Use POST for /v1/tokenize");
            else if (!body_buf || body_len == 0U) send_http_error(client, 400, "Bad Request", "invalid_request_error", "Empty JSON request body");
            else handle_tokenize(st, client, body_buf, body_len);
        } else if (strcmp(raw_path, "/v1/detokenize") == 0) {
            if (strcmp(method, "POST") != 0) send_http_error(client, 405, "Method Not Allowed", "invalid_request_error", "Use POST for /v1/detokenize");
            else if (!body_buf || body_len == 0U) send_http_error(client, 400, "Bad Request", "invalid_request_error", "Empty JSON request body");
            else handle_detokenize(st, client, body_buf, body_len);
        } else if (strcmp(raw_path, "/v1/batch") == 0) {
            if (strcmp(method, "POST") != 0) send_http_error(client, 405, "Method Not Allowed", "invalid_request_error", "Use POST for /v1/batch");
            else if (!body_buf || body_len == 0U) send_http_error(client, 400, "Bad Request", "invalid_request_error", "Empty JSON request body");
            else handle_batch(st, client, body_buf, body_len);
        } else if (strcmp(raw_path, "/v1/lora/adapters") == 0) {
            if (strcmp(method, "GET") != 0) send_http_error(client, 405, "Method Not Allowed", "invalid_request_error", "Use GET for /v1/lora/adapters");
            else handle_lora_adapters_get(st, client);
        } else if (strncmp(raw_path, "/v1/lora/adapters/", 18U) == 0 && raw_path[18] != '\0') {
            handle_lora_adapter_by_name(st, client, method, raw_path + 18);
        } else if (strcmp(raw_path, "/v1/lora/files") == 0) {
            if (strcmp(method, "GET") != 0) send_http_error(client, 405, "Method Not Allowed", "invalid_request_error", "Use GET for /v1/lora/files");
            else handle_lora_files_get(st, client);
        } else if (strcmp(raw_path, "/v1/lora/select") == 0) {
            if (strcmp(method, "POST") != 0) send_http_error(client, 405, "Method Not Allowed", "invalid_request_error", "Use POST for /v1/lora/select");
            else if (!body_buf || body_len == 0U) send_http_error(client, 400, "Bad Request", "invalid_request_error", "Empty JSON request body");
            else handle_lora_select(st, client, body_buf, body_len);
        } else if (strcmp(raw_path, "/v1/lora/scale") == 0) {
            if (strcmp(method, "POST") != 0) send_http_error(client, 405, "Method Not Allowed", "invalid_request_error", "Use POST for /v1/lora/scale");
            else if (!body_buf || body_len == 0U) send_http_error(client, 400, "Bad Request", "invalid_request_error", "Empty JSON request body");
            else handle_lora_scale(st, client, body_buf, body_len);
        } else if (strcmp(raw_path, "/v1/lora/save") == 0) {
            if (strcmp(method, "POST") != 0) send_http_error(client, 405, "Method Not Allowed", "invalid_request_error", "Use POST for /v1/lora/save");
            else if (!body_buf || body_len == 0U) send_http_error(client, 400, "Bad Request", "invalid_request_error", "Empty JSON request body");
            else handle_lora_save(st, client, body_buf, body_len);
        } else if (strcmp(raw_path, "/v1/lora/load") == 0) {
            if (strcmp(method, "POST") != 0) send_http_error(client, 405, "Method Not Allowed", "invalid_request_error", "Use POST for /v1/lora/load");
            else if (!body_buf || body_len == 0U) send_http_error(client, 400, "Bad Request", "invalid_request_error", "Empty JSON request body");
            else handle_lora_load(st, client, body_buf, body_len);
        } else if (strcmp(raw_path, "/v1/lora/unload") == 0) {
            if (strcmp(method, "POST") != 0) send_http_error(client, 405, "Method Not Allowed", "invalid_request_error", "Use POST for /v1/lora/unload");
            else if (!body_buf || body_len == 0U) send_http_error(client, 400, "Bad Request", "invalid_request_error", "Empty JSON request body");
            else handle_lora_unload(st, client, body_buf, body_len);
        } else if (strcmp(raw_path, "/v1/lora/train") == 0) {
            if (strcmp(method, "POST") != 0) send_http_error(client, 405, "Method Not Allowed", "invalid_request_error", "Use POST for /v1/lora/train");
            else if (!body_buf || body_len == 0U) send_http_error(client, 400, "Bad Request", "invalid_request_error", "Empty JSON request body");
            else handle_lora_train(st, client, body_buf, body_len);
        } else if (strcmp(raw_path, "/shutdown") == 0 && allow_shutdown) {
            if (strcmp(method, "POST") != 0) send_http_error(client, 405, "Method Not Allowed", "invalid_request_error", "Use POST for /shutdown");
            else {
                send_http_json(client, 200, "OK", "{\"status\":\"shutting_down\"}");
                running = 0;
            }
        } else {
            send_http_error(client, 404, "Not Found", "invalid_request_error", "Unknown endpoint");
        }

        if (body_buf) free(body_buf);
        shutdown(client, ZCC_SD_SEND);
        studio_sock_close(client);
        served_total++;
        if (max_requests > 0 && served_total >= max_requests) break;
    }

    free(hdr_buf);
    studio_sock_close(listen_sock);
    studio_net_cleanup();

    lora_decode_plan_free(&st->decode_plan);
    for (int i = 0; i < st->n_adapters; i++) {
        if (st->adapters[i].ls) free_lora_layers(st->adapters[i].ls, st->model.n_layers);
    }
    if (st->d_inf_h) cuMemFree_v2(st->d_inf_h);
    if (st->act.step_args) cuMemFree_v2(st->act.step_args);
    if (st->act.argmax_ids) cuMemFree_v2(st->act.argmax_ids);
    if (st->act.argmax_vals) cuMemFree_v2(st->act.argmax_vals);
    if (st->act.draft_ids) cuMemFree_v2(st->act.draft_ids);
    cuMemFree_v2(st->act.k_cache); cuMemFree_v2(st->act.v_cache);
    cuMemFree_v2(st->act.x); cuMemFree_v2(st->act.xb); cuMemFree_v2(st->act.xb2); cuMemFree_v2(st->act.x_f16);
    cuMemFree_v2(st->act.q); cuMemFree_v2(st->act.k); cuMemFree_v2(st->act.v);
    if (st->act.att_scratch) cuMemFree_v2(st->act.att_scratch);
    cuMemFree_v2(st->act.hb); cuMemFree_v2(st->act.hb2); cuMemFree_v2(st->act.logits);
    if (st->ev0) cuEventDestroy_v2(st->ev0);
    if (st->ev1) cuEventDestroy_v2(st->ev1);
    free(st->h_logits); free(st->work_logits); free(st->h_probs);
    free(st->cands); free(st->hist); free(st->gen_ids); free(st->gen_text);
    if (st->emb_buf) free(st->emb_buf);
    free_model_gpu_mem(&st->model);
    free_llama_model(&st->model);
    cuModuleUnload(st->mod_lora);
    cuModuleUnload(st->mod_llm);
    cuCtxDestroy_v2(st->ctx);
    free(st);
    return 0;
}
