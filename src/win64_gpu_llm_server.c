/*
 * ZKAEDI ZCC // Sovereign OpenAI-Compatible Win64 GPU HTTP/SSE Server (Track 18)
 * =============================================================================
 * Pure C99 Winsock2 HTTP/1.1 + Chunked SSE streaming inference server driving
 * src/win64_gpu_llm.c directly on Blackwell SM 12.0 GPUs.
 *
 * ZERO Python, ZERO MSVC, ZERO MinGW, ZERO CUDA Toolkit, ZERO OpenSSL.
 * Direct Win64 PE32+ imports: System32\{WS2_32,msvcrt,nvcuda}.dll ONLY.
 */

#define ZCC_GPU_LLM_LIBRARY 1
#include "win64_gpu_llm.c"
#include "server_ui_html.h"

/* MSVC CRT 64-bit epoch time */
extern long long _time64(long long *timer);

/* Winsock2 TCP/IP networking (System32\WS2_32.dll) */
typedef uint64_t ZccSocket;
#define ZCC_INVALID_SOCKET (~(ZccSocket)0ULL)
#define ZCC_AF_INET        2
#define ZCC_SOCK_STREAM    1
#define ZCC_IPPROTO_TCP    6
#define ZCC_SOL_SOCKET     0xFFFF
#define ZCC_SO_SNDTIMEO    0x1005
#define ZCC_SO_RCVTIMEO    0x1006
#define ZCC_TCP_NODELAY    1
#define ZCC_SD_SEND        1
#define ZCC_SD_BOTH        2

typedef struct {
    int16_t sin_family;
    uint16_t sin_port;
    uint32_t sin_addr;
    uint8_t sin_zero[8];
} ZccSockAddrIn;

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
        int rc = send(s, buf + sent, chunk, 0);
        if (rc <= 0) return -1;
        sent += (size_t)rc;
    }
    return 0;
}

static const char *CORS_HDRS =
    "Access-Control-Allow-Origin: *\r\n"
    "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
    "Access-Control-Allow-Headers: Content-Type, Authorization\r\n"
    "Connection: close\r\n";

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

/* Pure C99 RFC 8259 JSON parser for OpenAI /v1/{chat/completions,completions} */
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
    int stream;
    float temperature;
    int has_temp;
    float top_p;
    int top_k;
    float min_p;
    float repeat_penalty;
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
    if (r->prompt) free(r->prompt);
    memset(r, 0, sizeof(*r));
}

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

static char *j_parse_content_text_alloc(JsonCur *c) {
    j_ws(c);
    if (c->p >= c->end) return NULL;
    if (*c->p == '"') return j_parse_str_alloc(c);
    if (*c->p == '[') {
        /* Handle OpenAI content part array: [{"type":"text","text":"..."}] */
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

static int parse_openai_request(const char *body, size_t body_len, OpenAiReq *req) {
    memset(req, 0, sizeof(*req));
    req->temperature = 0.7f;
    req->top_p = 0.9f;
    req->top_k = 40;
    req->min_p = 0.0f;
    req->repeat_penalty = 1.1f;
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
        } else if (strcmp(k, "stream") == 0) {
            if (cur.end - cur.p >= 4 && memcmp(cur.p, "true", 4U) == 0) {
                req->stream = 1; cur.p += 4;
            } else if (cur.end - cur.p >= 5 && memcmp(cur.p, "false", 5U) == 0) {
                req->stream = 0; cur.p += 5;
            } else { free(k); goto fail; }
        } else if (strcmp(k, "temperature") == 0) {
            double v = 0.0;
            if (j_parse_num(&cur, &v) != 0 || !(v >= 0.0 && v <= 100.0)) { free(k); goto fail; }
            req->temperature = (float)v;
            req->has_temp = 1;
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
        } else if (strcmp(k, "max_tokens") == 0 || strcmp(k, "max_completion_tokens") == 0) {
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

static char *format_openai_messages_alloc(const LlamaModel *m, const OpenAiMsg *msgs, size_t n_msgs) {
    int is_chatml = (strcmp(m->arch, "qwen2") == 0) ||
                    (tok_lookup(&m->tok, "<|im_start|>", 12U) >= 0);
    StrBuf sb;
    memset(&sb, 0, sizeof(sb));
    sb_append_n(&sb, "", 0U);
    if (is_chatml) {
        for (size_t i = 0U; i < n_msgs; i++) {
            sb_append(&sb, "<|im_start|>");
            sb_append(&sb, msgs[i].role);
            sb_append(&sb, "\n");
            sb_append(&sb, msgs[i].content);
            sb_append(&sb, "<|im_end|>\n");
        }
        sb_append(&sb, "<|im_start|>assistant\n");
    } else {
        for (size_t i = 0U; i < n_msgs; i++) {
            if (strcmp(msgs[i].role, "system") == 0) {
                sb_append(&sb, "System: ");
                sb_append(&sb, msgs[i].content);
                sb_append(&sb, "\n");
            } else if (strcmp(msgs[i].role, "assistant") == 0) {
                sb_append(&sb, "Assistant: ");
                sb_append(&sb, msgs[i].content);
                sb_append(&sb, "\n");
            } else {
                sb_append(&sb, "User: ");
                sb_append(&sb, msgs[i].content);
                sb_append(&sb, "\n");
            }
        }
        sb_append(&sb, "Assistant:");
    }
    return sb.data;
}

/* Server inference state */
typedef struct {
    void *ctx;
    void *mod;
    void *fn_aot_fwd;
    void *ev0;
    void *ev1;
    LlamaModel model;
    GpuActivations act;
    McPlan mc_plan;
    McPrefillFns mc_pf;
    uint64_t aot_meta[16];
    uint64_t params;
    size_t kv_bytes;
    unsigned int aot_block_dim;
    unsigned int eff_max_seq;
    unsigned int prefill_batch;
    int aot_tier;
    int kv_type;
    int seal_verified;
    char seal_sha256[65];
    char model_id[128];
    float *h_logits;
    float *work_logits;
    float *h_probs;
    SampleCand *cands;
    uint32_t *hist;
    int *gen_ids;
    char *gen_text;
    size_t gen_text_cap;
    unsigned long long req_counter;
    double last_tps;
    int last_gen_tokens;
} ZccServerState;

typedef struct {
    ZccSocket sock;
    int is_chat;
    int sent_role;
    int disconnected;
    long long created;
    const char *req_id;
    const char *model_id;
} SseStreamCtx;

static void sse_emit_frame(SseStreamCtx *sc, const char *piece, const char *finish_reason) {
    if (!sc || sc->disconnected) return;
    StrBuf sb;
    memset(&sb, 0, sizeof(sb));
    sb_append(&sb, "data: {\"id\":");
    sb_append_json_str(&sb, sc->req_id);
    if (sc->is_chat) {
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
    if (sc->is_chat) {
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
    if (send_http_chunk(sc->sock, sb.data, sb.len) != 0) {
        sc->disconnected = 1;
        g_zcc_llm_failed = 1;
    }
    sb_free(&sb);
}

static void sse_token_cb(const char *piece_utf8, int token_id, void *userdata) {
    (void)token_id;
    SseStreamCtx *sc = (SseStreamCtx *)userdata;
    if (!sc || sc->disconnected) return;
    if (!sc->sent_role && sc->is_chat) {
        sse_emit_frame(sc, piece_utf8, NULL);
    } else if (piece_utf8 && piece_utf8[0] != '\0') {
        sse_emit_frame(sc, piece_utf8, NULL);
    }
}

static void handle_openai_inference(ZccServerState *st, ZccSocket client,
                                    const char *body, size_t body_len, int is_chat) {
    OpenAiReq req;
    if (parse_openai_request(body, body_len, &req) != 0) {
        send_http_error(client, 400, "Bad Request", "invalid_request_error", "Malformed JSON request body");
        return;
    }

    char *prompt_alloc = NULL;
    const char *raw_prompt = NULL;
    if (is_chat) {
        if (!req.has_messages || req.n_msgs == 0U) {
            openai_req_free(&req);
            send_http_error(client, 400, "Bad Request", "invalid_request_error", "Missing or empty 'messages' array");
            return;
        }
        prompt_alloc = format_openai_messages_alloc(&st->model, req.msgs, req.n_msgs);
        raw_prompt = prompt_alloc;
    } else {
        if (!req.has_prompt || !req.prompt || req.prompt[0] == '\0') {
            openai_req_free(&req);
            send_http_error(client, 400, "Bad Request", "invalid_request_error", "Missing or empty 'prompt' string");
            return;
        }
        raw_prompt = req.prompt;
    }

    uint32_t *t_ids = NULL;
    uint32_t t_len = 0U;
    if (!raw_prompt || tokenize_prompt(&st->model, raw_prompt, 0, &t_ids, &t_len) != 0 || t_len == 0U) {
        if (prompt_alloc) free(prompt_alloc);
        openai_req_free(&req);
        send_http_error(client, 400, "Bad Request", "invalid_request_error", "Prompt tokenized to empty sequence");
        return;
    }
    if (prompt_alloc) { free(prompt_alloc); prompt_alloc = NULL; }

    if ((uint64_t)t_len >= (uint64_t)st->eff_max_seq) {
        free(t_ids);
        openai_req_free(&req);
        send_http_error(client, 400, "Bad Request", "context_length_exceeded", "Prompt exceeds context window");
        return;
    }

    int max_steps = req.max_tokens;
    int room = (int)(st->eff_max_seq - t_len);
    if (max_steps > room) max_steps = room;
    if (max_steps < 1) max_steps = 1;

    g_zcc_llm_failed = 0;
    g_zcc_llm_err[0] = '\0';

    check_cu(cuMemsetD8_v2(st->act.k_cache, 0, st->kv_bytes), "srv_reset_k");
    check_cu(cuMemsetD8_v2(st->act.v_cache, 0, st->kv_bytes), "srv_reset_v");
    if (g_zcc_llm_failed) {
        free(t_ids);
        openai_req_free(&req);
        send_http_error(client, 500, "Internal Server Error", "server_error", g_zcc_llm_err);
        g_zcc_llm_failed = 0;
        return;
    }

    unsigned int hist_len = 0U;
    for (uint32_t i = 0U; i < t_len; i++) {
        uint32_t tok = t_ids[i];
        if (tok >= st->model.vocab_size) tok = 0U;
        st->hist[hist_len++] = tok;
    }

    st->req_counter++;
    char req_id[64];
    snprintf(req_id, sizeof(req_id), "%s-%llu", is_chat ? "chatcmpl-zcc" : "cmpl-zcc", st->req_counter);
    long long now_ts = _time64(NULL);

    check_cu(cuEventRecord(st->ev0, NULL), "srv_ev0");
    unsigned int pos = 0U;
    if (!g_zcc_llm_failed) {
        pos = run_prefill_active(st->aot_tier, st->fn_aot_fwd, st->aot_block_dim, st->mod,
                                 &st->mc_plan, &st->mc_pf, &st->model, &st->act,
                                 0, NULL, 0U, NULL, NULL, NULL, NULL,
                                 t_ids, t_len, 0U, st->eff_max_seq,
                                 1, st->kv_type, (int)st->prefill_batch, NULL, NULL);
    }
    free(t_ids);
    if (!g_zcc_llm_failed) check_cu(cuCtxSynchronize(), "srv_prefill_sync");
    if (!g_zcc_llm_failed) check_cu(cuMemcpyDtoH_v2(st->h_logits, st->act.logits, (size_t)st->model.vocab_size * 4U), "srv_dtoh_logits");
    if (!g_zcc_llm_failed) check_finite_logits(st->h_logits, st->model.vocab_size, -1);
    if (g_zcc_llm_failed) {
        openai_req_free(&req);
        send_http_error(client, 500, "Internal Server Error", "server_error", g_zcc_llm_err);
        g_zcc_llm_failed = 0;
        return;
    }

    SseStreamCtx sctx;
    memset(&sctx, 0, sizeof(sctx));
    sctx.sock = client;
    sctx.is_chat = is_chat;
    sctx.created = now_ts;
    sctx.req_id = req_id;
    sctx.model_id = st->model_id;

    if (req.stream) {
        if (send_http_chunked_sse_hdr(client) != 0) {
            openai_req_free(&req);
            return;
        }
    }

    uint32_t seed = req.seed ? req.seed : 42U;
    int gen_count = 0;
    pos = run_decode_sequence_ex(st->aot_tier, st->fn_aot_fwd, st->aot_block_dim,
                                 &st->mc_plan, &st->model, &st->act,
                                 1, st->kv_type, max_steps, pos, st->eff_max_seq,
                                 req.temperature, req.top_k, req.top_p, req.min_p, req.repeat_penalty, 0,
                                 0, NULL,
                                 req.stream ? sse_token_cb : NULL,
                                 req.stream ? (void *)&sctx : NULL,
                                 &seed, st->h_logits, st->work_logits, st->h_probs, st->cands,
                                 st->hist, &hist_len, st->gen_ids,
                                 &st->gen_text, &st->gen_text_cap, &gen_count);
    (void)pos;

    if (sctx.disconnected) {
        g_zcc_llm_failed = 0;
        openai_req_free(&req);
        return;
    }

    if (g_zcc_llm_failed) {
        g_zcc_llm_failed = 0;
        if (!req.stream) {
            send_http_error(client, 500, "Internal Server Error", "server_error", g_zcc_llm_err);
        } else {
            sock_send_all(client, "0\r\n\r\n", 5U);
        }
        openai_req_free(&req);
        return;
    }

    check_cu(cuEventRecord(st->ev1, NULL), "srv_ev1");
    check_cu(cuEventSynchronize(st->ev1), "srv_ev1_sync");
    float ms = 0.0f;
    check_cu(cuEventElapsedTime(&ms, st->ev0, st->ev1), "srv_elapsed");
    g_zcc_llm_failed = 0;

    st->last_gen_tokens = gen_count;
    st->last_tps = (ms > 0.0f) ? (((double)(t_len + (uint32_t)gen_count) * 1000.0) / (double)ms) : 0.0;
    const char *finish_reason = (gen_count >= max_steps) ? "length" : "stop";

    if (req.stream) {
        if (!sctx.sent_role && is_chat) {
            sse_emit_frame(&sctx, "", NULL);
        }
        sse_emit_frame(&sctx, "", finish_reason);
        if (!sctx.disconnected) {
            send_http_chunk(client, "data: [DONE]\n\n", 14U);
            sock_send_all(client, "0\r\n\r\n", 5U);
        }
    } else {
        StrBuf out;
        memset(&out, 0, sizeof(out));
        char cr_s[32], pt_s[32], ct_s[32], tt_s[32];
        fmt_u64_dec((uint64_t)(now_ts > 0LL ? now_ts : 0LL), cr_s, sizeof(cr_s));
        fmt_u64_dec((uint64_t)t_len, pt_s, sizeof(pt_s));
        fmt_u64_dec((uint64_t)gen_count, ct_s, sizeof(ct_s));
        fmt_u64_dec((uint64_t)t_len + (uint64_t)gen_count, tt_s, sizeof(tt_s));

        sb_append(&out, "{\"id\":");
        sb_append_json_str(&out, req_id);
        sb_append(&out, is_chat ? ",\"object\":\"chat.completion\",\"created\":" : ",\"object\":\"text_completion\",\"created\":");
        sb_append(&out, cr_s);
        sb_append(&out, ",\"model\":");
        sb_append_json_str(&out, st->model_id);
        if (is_chat) {
            sb_append(&out, ",\"choices\":[{\"index\":0,\"message\":{\"role\":\"assistant\",\"content\":");
            sb_append_json_str(&out, st->gen_text ? st->gen_text : "");
            sb_append(&out, "},\"finish_reason\":");
        } else {
            sb_append(&out, ",\"choices\":[{\"index\":0,\"text\":");
            sb_append_json_str(&out, st->gen_text ? st->gen_text : "");
            sb_append(&out, ",\"logprobs\":null,\"finish_reason\":");
        }
        sb_append_json_str(&out, finish_reason);
        sb_append(&out, "}],\"usage\":{\"prompt_tokens\":");
        sb_append(&out, pt_s);
        sb_append(&out, ",\"completion_tokens\":");
        sb_append(&out, ct_s);
        sb_append(&out, ",\"total_tokens\":");
        sb_append(&out, tt_s);
        sb_append(&out, "}}");

        send_http_json(client, 200, "OK", out.data ? out.data : "{}");
        sb_free(&out);
    }

    openai_req_free(&req);
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
    /* Skip request line */
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
                    /* Oversized number -> treat as > max body */
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

static void derive_model_id(const char *path, char *out, size_t cap) {
    if (!path || !path[0]) {
        snprintf(out, cap, "zkaedi-zcc-llm");
        return;
    }
    const char *base = path;
    for (const char *p = path; *p; p++) {
        if (*p == '/' || *p == '\\') base = p + 1;
    }
    snprintf(out, cap, "%s", base[0] ? base : "zkaedi-zcc-llm");
    size_t len = strlen(out);
    if (len > 5U && strcmp(out + len - 5U, ".gguf") == 0) out[len - 5U] = '\0';
}

int main(int argc, char **argv) {
    const char *model_path = NULL;
    const char *aot_ptx_path = NULL;
    const char *host_ip = "127.0.0.1";
    const char *model_name_override = NULL;
    int port = 8080;
    int kv_type = ZCC_KV_F32;
    int ctx_override = 0;
    int max_requests = 0;
    int allow_shutdown = 0;
    int user_gave_model = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--model") == 0 && i + 1 < argc) { model_path = argv[++i]; user_gave_model = 1; }
        else if (strcmp(argv[i], "--aot-ptx") == 0 && i + 1 < argc) { aot_ptx_path = argv[++i]; }
        else if (strcmp(argv[i], "--host") == 0 && i + 1 < argc) { host_ip = argv[++i]; }
        else if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            if (parse_int_arg(argv[++i], "--port", 0, 65535, &port) != 0) return 1;
        } else if (strcmp(argv[i], "--kv-type") == 0 && i + 1 < argc) {
            const char *kv_arg = argv[++i];
            if (strcmp(kv_arg, "f32") == 0 || strcmp(kv_arg, "0") == 0) kv_type = ZCC_KV_F32;
            else if (strcmp(kv_arg, "f16") == 0 || strcmp(kv_arg, "1") == 0) kv_type = ZCC_KV_F16;
            else if (strcmp(kv_arg, "q8_0") == 0 || strcmp(kv_arg, "2") == 0) kv_type = ZCC_KV_Q8_0;
            else { fprintf(stderr, "INVALID_ARG: unsupported --kv-type '%s'\n", kv_arg); return 1; }
        } else if (strcmp(argv[i], "--ctx") == 0 && i + 1 < argc) {
            if (parse_int_arg(argv[++i], "--ctx", 1, 131072, &ctx_override) != 0) return 1;
        } else if (strcmp(argv[i], "--model-name") == 0 && i + 1 < argc) {
            model_name_override = argv[++i];
        } else if (strcmp(argv[i], "--max-requests") == 0 && i + 1 < argc) {
            if (parse_int_arg(argv[++i], "--max-requests", 0, 1000000, &max_requests) != 0) return 1;
        } else if (strcmp(argv[i], "--allow-shutdown") == 0) {
            allow_shutdown = 1;
        } else {
            fprintf(stderr, "Unknown server arg: %s\n", argv[i]);
            return 1;
        }
    }

    int use_embedded_aot = 0;
#ifdef ZCC_EMBEDDED_AOT_PTX
    if (!aot_ptx_path) use_embedded_aot = 1;
#endif
    int use_embedded_gguf = 0;
#ifdef ZCC_EMBEDDED_GGUF
    if (!user_gave_model) use_embedded_gguf = 1;
#endif
    int use_sealed_overlay = 0;
#ifdef ZCC_SEALED_OVERLAY_GGUF
    if (!user_gave_model && !use_embedded_gguf) use_sealed_overlay = 1;
#endif

    if (!user_gave_model && !use_embedded_gguf && !use_sealed_overlay) {
        fprintf(stderr, "Usage: %s --model <model.gguf> [--aot-ptx <ptx>] [--host 127.0.0.1] [--port 8080] [--kv-type f32|f16|q8_0] [--ctx N] [--allow-shutdown]\n", argv[0]);
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

    uint64_t ov_off = 0ULL, ov_len = 0ULL;
    const uint8_t *ov_sha = NULL;
    char *pgm = NULL;
    const char *self_path = argv[0];
#ifdef ZCC_SEALED_OVERLAY_GGUF
    if (use_sealed_overlay) {
        if (_get_pgmptr(&pgm) == 0 && pgm && pgm[0]) self_path = pgm;
        if (probe_sealed_pe_overlay(self_path, g_sealed_gguf_len, g_sealed_gguf_sha256,
                                    &ov_off, &ov_len) != 0) {
            return 1;
        }
        model_path = self_path;
        ov_sha = g_sealed_gguf_sha256;
    }
#endif
    (void)pgm;
    (void)self_path;

    ZccServerState st;
    memset(&st, 0, sizeof(st));
    st.kv_type = kv_type;
    if (model_name_override && model_name_override[0]) {
        snprintf(st.model_id, sizeof(st.model_id), "%s", model_name_override);
    } else if (use_sealed_overlay || use_embedded_gguf) {
        derive_model_id(argv[0], st.model_id, sizeof(st.model_id));
    } else {
        derive_model_id(model_path, st.model_id, sizeof(st.model_id));
    }

    if (cuInit(0U) != 0) { fprintf(stderr, "cuInit failed\n"); return 1; }
    int dev_cnt = 0;
    if (cuDeviceGetCount(&dev_cnt) != 0 || dev_cnt <= 0) { fprintf(stderr, "No CUDA device\n"); return 1; }
    int dev = 0;
    if (cuDeviceGet(&dev, 0) != 0) { fprintf(stderr, "cuDeviceGet failed\n"); return 1; }
    if (cuCtxCreate_v2(&st.ctx, 0U, dev) != 0) { fprintf(stderr, "cuCtxCreate_v2 failed\n"); return 1; }

    int use_aot = (aot_ptx_path != NULL) || use_embedded_aot;
    const void *fb_ptx = NULL;
    unsigned int fb_bytes = 0U;
#ifdef ZCC_EMBEDDED_AOT_PTX
    fb_ptx = (const void *)g_embedded_aot_ptx;
    fb_bytes = (unsigned int)(sizeof(g_embedded_aot_ptx) - 1U);
#else
    fb_ptx = (const void *)g_gpu_llm_kernels_ptx;
    fb_bytes = (unsigned int)(sizeof(g_gpu_llm_kernels_ptx) - 1U);
#endif

    unsigned int ptx_bytes = 0U;
    if (load_ptx_module(aot_ptx_path, fb_ptx, fb_bytes, use_aot,
                        &st.aot_tier, &st.mod, st.aot_meta, &ptx_bytes) != 0) {
        cuCtxDestroy_v2(st.ctx);
        return 1;
    }
    (void)ptx_bytes;

    if (use_aot) {
        st.aot_block_dim = (unsigned int)st.aot_meta[14];
        if (st.aot_tier == 1) {
            check_cu(cuModuleGetFunction(&st.fn_aot_fwd, st.mod, "zcc_aot_forward_token"),
                     "get(zcc_aot_forward_token)");
        }
    } else {
        load_generic_kernels(st.mod);
    }
    if (g_zcc_llm_failed) {
        fprintf(stderr, "%s\n", g_zcc_llm_err);
        cuModuleUnload(st.mod); cuCtxDestroy_v2(st.ctx);
        return 1;
    }

    const uint8_t *emb_ptr = NULL;
    size_t emb_sz = 0U;
#ifdef ZCC_EMBEDDED_GGUF
    if (use_embedded_gguf) {
        emb_ptr = (const uint8_t *)g_embedded_gguf_u32;
        emb_sz = g_embedded_gguf_len;
    }
#endif

    if (parse_and_stream_gguf_ex(model_path, emb_ptr, emb_sz, ov_off, ov_len, ov_sha,
                                   &st.model, use_aot, st.aot_meta, 0, 0ULL) != 0) {
        free_model_gpu_mem(&st.model);
        free_llama_model(&st.model);
        cuModuleUnload(st.mod);
        cuCtxDestroy_v2(st.ctx);
        return 1;
    }
    if (ov_sha != NULL) {
        st.seal_verified = 1;
        sha256_to_hex(ov_sha, st.seal_sha256);
    }
    for (unsigned int ti = 0U; ti < st.model.num_tensors; ti++) {
        const GgufTensor *gt = &st.model.tensors[ti];
        uint64_t ne = (gt->n_dims > 0U) ? 1ULL : 0ULL;
        for (uint32_t d = 0U; d < gt->n_dims && d < 4U; d++) ne *= gt->dims[d];
        st.params += ne;
    }

    if (kv_type != ZCC_KV_F32 && st.aot_tier == 1) {
        fprintf(stderr, "INVALID_ARG: --kv-type %s not supported in Tier A aot_fused mode\n", kv_type_name_str(kv_type));
        free_model_gpu_mem(&st.model); free_llama_model(&st.model);
        cuModuleUnload(st.mod); cuCtxDestroy_v2(st.ctx);
        return 1;
    }
    if (kv_type == ZCC_KV_Q8_0 && ((st.model.head_dim & 31U) != 0U || (st.model.kv_dim & 31U) != 0U)) {
        fprintf(stderr, "INVALID_ARG: q8_0 requires head_dim/kv_dim multiple of 32\n");
        free_model_gpu_mem(&st.model); free_llama_model(&st.model);
        cuModuleUnload(st.mod); cuCtxDestroy_v2(st.ctx);
        return 1;
    }

    st.eff_max_seq = st.model.max_seq_len;
    if (ctx_override > 0) st.eff_max_seq = (unsigned int)ctx_override;

    uint64_t row_bytes_kv = (uint64_t)st.model.kv_dim * 4ULL;
    if (kv_type == ZCC_KV_F16) row_bytes_kv = (uint64_t)st.model.kv_dim * 2ULL;
    else if (kv_type == ZCC_KV_Q8_0) row_bytes_kv = (uint64_t)(st.model.kv_dim >> 5) * 34ULL;

    st.prefill_batch = (st.aot_tier == 1) ? 1U : 32U;
    size_t free_vram = 0U, total_vram = 0U;
    check_cu(cuMemGetInfo_v2(&free_vram, &total_vram), "cuMemGetInfo_v2");
    uint64_t ws_est = (uint64_t)st.prefill_batch * ((uint64_t)st.model.dim * 24ULL + (uint64_t)st.model.hidden_dim * 12ULL + (uint64_t)st.model.kv_dim * 8ULL) +
                      (uint64_t)st.model.vocab_size * 4ULL + 65536ULL;
    if (st.aot_tier != 1) {
        uint64_t mm = (st.model.hidden_dim > st.model.dim) ? (uint64_t)st.model.hidden_dim * st.model.dim : (uint64_t)st.model.dim * st.model.dim;
        ws_est += mm * 2ULL;
    }
    uint64_t kv_each64 = 0ULL, kv_total64 = 0ULL;
    for (;;) {
        int ov = (mul_u64_checked((uint64_t)st.model.n_layers, (uint64_t)st.eff_max_seq, &kv_each64) != 0 ||
                  mul_u64_checked(kv_each64, row_bytes_kv, &kv_each64) != 0);
        if (!ov) kv_total64 = kv_each64 * 2ULL;
        if (ctx_override == 0 && st.aot_tier != 1 && st.eff_max_seq > 256U && free_vram > 0U &&
            (ov || kv_total64 > (uint64_t)free_vram || kv_total64 + ws_est > ((uint64_t)free_vram * 85ULL) / 100ULL)) {
            st.eff_max_seq >>= 1;
            continue;
        }
        if (ov || kv_total64 + ws_est > (uint64_t)free_vram) {
            fprintf(stderr, "VRAM_BUDGET_EXCEEDED: insufficient VRAM\n");
            free_model_gpu_mem(&st.model); free_llama_model(&st.model);
            cuModuleUnload(st.mod); cuCtxDestroy_v2(st.ctx);
            return 1;
        }
        break;
    }
    st.kv_bytes = (size_t)kv_each64;

    check_cu(cuMemAlloc_v2(&st.act.k_cache, st.kv_bytes), "srv.k_cache");
    check_cu(cuMemAlloc_v2(&st.act.v_cache, st.kv_bytes), "srv.v_cache");
    check_cu(cuMemAlloc_v2(&st.act.logits, (size_t)st.model.vocab_size * 4U), "srv.logits");
    if (st.aot_tier != 1) {
        unsigned int max_k = (st.model.hidden_dim > st.model.dim) ? st.model.hidden_dim : st.model.dim;
        size_t pb = (size_t)st.prefill_batch;
        check_cu(cuMemAlloc_v2(&st.act.x, pb * (size_t)st.model.dim * 4U), "srv.x");
        check_cu(cuMemAlloc_v2(&st.act.xb, pb * (size_t)st.model.dim * 4U), "srv.xb");
        check_cu(cuMemAlloc_v2(&st.act.xb2, pb * (size_t)st.model.dim * 4U), "srv.xb2");
        check_cu(cuMemAlloc_v2(&st.act.x_f16, pb * (size_t)max_k * 2U), "srv.x_f16");
        check_cu(cuMemAlloc_v2(&st.act.q, pb * (size_t)st.model.dim * 4U), "srv.q");
        check_cu(cuMemAlloc_v2(&st.act.k, pb * (size_t)st.model.kv_dim * 4U), "srv.k");
        check_cu(cuMemAlloc_v2(&st.act.v, pb * (size_t)st.model.kv_dim * 4U), "srv.v");
        if (!use_aot && st.model.head_dim > 256U) {
            check_cu(cuMemAlloc_v2(&st.act.att_scratch, (size_t)st.model.n_heads * st.eff_max_seq * 4U), "srv.att");
        }
        check_cu(cuMemAlloc_v2(&st.act.hb, pb * (size_t)st.model.hidden_dim * 4U), "srv.hb");
        check_cu(cuMemAlloc_v2(&st.act.hb2, pb * (size_t)st.model.hidden_dim * 4U), "srv.hb2");
        if (!use_aot) check_cu(cuMemAlloc_v2(&st.act.probs, (size_t)st.model.vocab_size * 4U), "srv.probs");
        if (st.aot_tier == 2) {
            check_cu(cuMemAlloc_v2(&st.act.step_args, 16U), "srv.step_args");
            check_cu(cuMemAlloc_v2(&st.act.argmax_ids, 4U), "srv.argmax_ids");
            check_cu(cuMemAlloc_v2(&st.act.argmax_vals, 4U), "srv.argmax_vals");
            check_cu(cuMemAlloc_v2(&st.act.batch_toks, pb * 4U), "srv.batch_toks");
        }
        size_t max_mat = ((size_t)st.model.hidden_dim > (size_t)st.model.dim)
                         ? (size_t)st.model.hidden_dim * (size_t)st.model.dim
                         : (size_t)st.model.dim * (size_t)st.model.dim;
        check_cu(cuMemAlloc_v2(&st.act.w_f16_tmp, max_mat * 2U), "srv.w_f16_tmp");
        if (st.aot_tier == 0) check_cu(cuMemAlloc_v2(&st.act.prefill_tmp, pb * (size_t)st.model.dim * 4U), "srv.prefill_tmp");
    }
    if (st.aot_tier == 2) mc_plan_init(&st.mc_plan, st.mod, &st.model, &st.act, 0, kv_type, 1);
    check_cu(cuEventCreate(&st.ev0, 0U), "srv_ev0");
    check_cu(cuEventCreate(&st.ev1, 0U), "srv_ev1");

    st.h_logits = (float *)malloc((size_t)st.model.vocab_size * 4U);
    st.work_logits = (float *)malloc((size_t)st.model.vocab_size * 4U);
    st.h_probs = (float *)malloc((size_t)st.model.vocab_size * 4U);
    st.cands = (SampleCand *)malloc((size_t)st.model.vocab_size * sizeof(SampleCand));
    st.hist = (uint32_t *)malloc(((size_t)st.eff_max_seq + 128U) * sizeof(uint32_t));
    st.gen_ids = (int *)malloc(((size_t)st.eff_max_seq + 16U) * sizeof(int));
    st.gen_text_cap = 16384U;
    st.gen_text = (char *)malloc(st.gen_text_cap);

    uint8_t wsa_data[512];
    memset(wsa_data, 0, sizeof(wsa_data));
    if (WSAStartup(0x0202U, wsa_data) != 0) {
        fprintf(stderr, "WSAStartup failed\n");
        return 1;
    }

    ZccSocket listen_sock = socket(ZCC_AF_INET, ZCC_SOCK_STREAM, ZCC_IPPROTO_TCP);
    if (listen_sock == ZCC_INVALID_SOCKET) {
        fprintf(stderr, "SOCKET_ERROR: wsa=%d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    ZccSockAddrIn addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = ZCC_AF_INET;
    addr.sin_port = htons((uint16_t)port);
    addr.sin_addr = bind_addr;
    if (bind(listen_sock, &addr, (int)sizeof(addr)) != 0) {
        fprintf(stderr, "BIND_ERROR host=%s port=%d wsa=%d\n", host_ip, port, WSAGetLastError());
        closesocket(listen_sock);
        WSACleanup();
        return 1;
    }

    ZccSockAddrIn bound_addr;
    memset(&bound_addr, 0, sizeof(bound_addr));
    int bound_len = (int)sizeof(bound_addr);
    if (getsockname(listen_sock, &bound_addr, &bound_len) == 0) {
        port = (int)ntohs(bound_addr.sin_port);
    }

    if (listen(listen_sock, 16) != 0) {
        fprintf(stderr, "LISTEN_ERROR wsa=%d\n", WSAGetLastError());
        closesocket(listen_sock);
        WSACleanup();
        return 1;
    }

    const char *mode_str = use_sealed_overlay ? "aot_overlay"
                         : (st.aot_tier == 2 ? "aot_graph"
                         : (st.aot_tier == 1 ? "aot_fused" : "generic"));
    if (st.seal_verified) {
        printf("GPU_LLM_SEAL verified=1 mode=pe_overlay sha256=%s bytes=%llu\n",
               st.seal_sha256, (unsigned long long)ov_len);
    }
    printf("ZCC_SERVER_READY url=http://%s:%d model=%s mode=%s seal=%d aot_tier=%d kv=%s ctx=%u\n",
           host_ip, port, st.model_id, mode_str, st.seal_verified, st.aot_tier,
           kv_type_name_str(st.kv_type), st.eff_max_seq);
    fflush(stdout);

    int hdr_cap = 65536;
    char *hdr_buf = (char *)malloc((size_t)hdr_cap + 1U);
    int running = 1;
    int served_total = 0;

    while (running) {
        ZccSocket client = accept(listen_sock, NULL, NULL);
        if (client == ZCC_INVALID_SOCKET) break;

        uint32_t rcv_ms = 10000U;
        uint32_t snd_ms = 30000U;
        int nodelay = 1;
        setsockopt(client, ZCC_SOL_SOCKET, ZCC_SO_RCVTIMEO, (const char *)&rcv_ms, 4);
        setsockopt(client, ZCC_SOL_SOCKET, ZCC_SO_SNDTIMEO, (const char *)&snd_ms, 4);
        setsockopt(client, ZCC_IPPROTO_TCP, ZCC_TCP_NODELAY, (const char *)&nodelay, 4);

        size_t got = 0U;
        size_t hdr_len = 0U;
        const char *hdr_end = NULL;
        while (got < (size_t)hdr_cap) {
            int n = recv(client, hdr_buf + got, hdr_cap - (int)got, 0);
            if (n <= 0) break;
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
            closesocket(client);
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
            closesocket(client);
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
            closesocket(client);
            continue;
        }
        if (has_cl && cl_val > 1048576ULL) {
            send_http_error(client, 413, "Payload Too Large", "invalid_request_error", "Request body exceeds 1 MiB limit");
            shutdown(client, ZCC_SD_BOTH);
            closesocket(client);
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
                closesocket(client);
                continue;
            }
            size_t already = got - hdr_len;
            size_t copied = (already < body_len) ? already : body_len;
            if (copied > 0U) memcpy(body_buf, hdr_end, copied);
            while (copied < body_len) {
                int n = recv(client, body_buf + copied, (int)(body_len - copied), 0);
                if (n <= 0) { body_err = 1; break; }
                copied += (size_t)n;
            }
            body_buf[body_len] = '\0';
        }

        if (body_err) {
            if (body_buf) free(body_buf);
            send_http_error(client, 400, "Bad Request", "invalid_request_error", "Truncated HTTP request body");
            shutdown(client, ZCC_SD_BOTH);
            closesocket(client);
            continue;
        }

        if (strcmp(method, "OPTIONS") == 0) {
            send_http_response(client, 204, "No Content", "text/plain", "", 0U);
        } else if (strcmp(raw_path, "/") == 0) {
            if (strcmp(method, "GET") != 0) {
                send_http_error(client, 405, "Method Not Allowed", "invalid_request_error", "Use GET for /");
            } else {
                send_http_response(client, 200, "OK", "text/html; charset=utf-8",
                                   (const char *)g_server_ui_html, (size_t)(sizeof(g_server_ui_html) - 1U));
            }
        } else if (strcmp(raw_path, "/favicon.ico") == 0) {
            send_http_response(client, 204, "No Content", "image/x-icon", "", 0U);
        } else if (strcmp(raw_path, "/health") == 0) {
            if (strcmp(method, "GET") != 0) {
                send_http_error(client, 405, "Method Not Allowed", "invalid_request_error", "Use GET for /health");
            } else {
                StrBuf hb;
                memset(&hb, 0, sizeof(hb));
                char tmp[1024];
                sb_append(&hb, "{\"status\":\"ok\",\"model\":");
                sb_append_json_str(&hb, st.model_id);
                sb_append(&hb, ",\"arch\":");
                sb_append_json_str(&hb, st.model.arch);
                snprintf(tmp, sizeof(tmp),
                         ",\"n_layers\":%u,\"dim\":%u,\"vocab_size\":%u,\"aot_tier\":%d,\"kv_type\":\"%s\",\"eff_max_seq\":%u,\"seal_verified\":%s,\"seal_sha256\":\"%s\",\"requests_served\":%llu,\"last_tok_per_sec\":%.2f,\"last_gen_tokens\":%d}",
                         st.model.n_layers, st.model.dim, st.model.vocab_size, st.aot_tier,
                         kv_type_name_str(st.kv_type), st.eff_max_seq,
                         st.seal_verified ? "true" : "false", st.seal_sha256,
                         st.req_counter, st.last_tps, st.last_gen_tokens);
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
                long long ts = _time64(NULL);
                fmt_u64_dec((uint64_t)(ts > 0LL ? ts : 0LL), cr_s, sizeof(cr_s));
                sb_append(&mb, "{\"object\":\"list\",\"data\":[{\"id\":");
                sb_append_json_str(&mb, st.model_id);
                sb_append(&mb, ",\"object\":\"model\",\"created\":");
                sb_append(&mb, cr_s);
                sb_append(&mb, ",\"owned_by\":\"zkaedi-zcc\"}]}");
                send_http_json(client, 200, "OK", mb.data ? mb.data : "{}");
                sb_free(&mb);
            }
        } else if (strcmp(raw_path, "/v1/chat/completions") == 0) {
            if (strcmp(method, "POST") != 0) {
                send_http_error(client, 405, "Method Not Allowed", "invalid_request_error", "Use POST for /v1/chat/completions");
            } else if (!body_buf || body_len == 0U) {
                send_http_error(client, 400, "Bad Request", "invalid_request_error", "Empty JSON request body");
            } else {
                handle_openai_inference(&st, client, body_buf, body_len, 1);
            }
        } else if (strcmp(raw_path, "/v1/completions") == 0) {
            if (strcmp(method, "POST") != 0) {
                send_http_error(client, 405, "Method Not Allowed", "invalid_request_error", "Use POST for /v1/completions");
            } else if (!body_buf || body_len == 0U) {
                send_http_error(client, 400, "Bad Request", "invalid_request_error", "Empty JSON request body");
            } else {
                handle_openai_inference(&st, client, body_buf, body_len, 0);
            }
        } else if (strcmp(raw_path, "/shutdown") == 0 && allow_shutdown) {
            if (strcmp(method, "POST") != 0) {
                send_http_error(client, 405, "Method Not Allowed", "invalid_request_error", "Use POST for /shutdown");
            } else {
                send_http_json(client, 200, "OK", "{\"status\":\"shutting_down\"}");
                running = 0;
            }
        } else {
            send_http_error(client, 404, "Not Found", "invalid_request_error", "Unknown endpoint");
        }

        if (body_buf) free(body_buf);
        shutdown(client, ZCC_SD_SEND);
        closesocket(client);
        served_total++;
        if (max_requests > 0 && served_total >= max_requests) break;
    }

    free(hdr_buf);
    closesocket(listen_sock);
    WSACleanup();

    if (st.aot_tier == 2) mc_plan_free(&st.mc_plan);
    free_model_gpu_mem(&st.model);
    free_llama_model(&st.model);
    if (st.act.x) cuMemFree_v2(st.act.x);
    if (st.act.xb) cuMemFree_v2(st.act.xb);
    if (st.act.xb2) cuMemFree_v2(st.act.xb2);
    if (st.act.x_f16) cuMemFree_v2(st.act.x_f16);
    if (st.act.q) cuMemFree_v2(st.act.q);
    if (st.act.k) cuMemFree_v2(st.act.k);
    if (st.act.v) cuMemFree_v2(st.act.v);
    if (st.act.att_scratch) cuMemFree_v2(st.act.att_scratch);
    if (st.act.hb) cuMemFree_v2(st.act.hb);
    if (st.act.hb2) cuMemFree_v2(st.act.hb2);
    if (st.act.probs) cuMemFree_v2(st.act.probs);
    if (st.act.step_args) cuMemFree_v2(st.act.step_args);
    if (st.act.argmax_ids) cuMemFree_v2(st.act.argmax_ids);
    if (st.act.argmax_vals) cuMemFree_v2(st.act.argmax_vals);
    if (st.act.batch_toks) cuMemFree_v2(st.act.batch_toks);
    if (st.act.w_f16_tmp) cuMemFree_v2(st.act.w_f16_tmp);
    if (st.act.prefill_tmp) cuMemFree_v2(st.act.prefill_tmp);
    if (st.act.k_cache) cuMemFree_v2(st.act.k_cache);
    if (st.act.v_cache) cuMemFree_v2(st.act.v_cache);
    if (st.act.logits) cuMemFree_v2(st.act.logits);
    if (st.ev0) cuEventDestroy_v2(st.ev0);
    if (st.ev1) cuEventDestroy_v2(st.ev1);
    free(st.h_logits); free(st.work_logits); free(st.h_probs);
    free(st.cands); free(st.hist); free(st.gen_ids); free(st.gen_text);
    cuModuleUnload(st.mod);
    cuCtxDestroy_v2(st.ctx);
    return 0;
}
