/*
 * ZCC WASI Preview1 VFS & CLI Driver for SQLite 3.45.0
 * =====================================================
 * Implements `sqlite3_os_init` / `sqlite3_os_end` and default VFS `"zcc_wasi"`
 * on top of `src/wasm_wasi_rt.c` (supporting both `:memory:` and persistent
 * WASI Preopen / BrowserWasiShim `.db` files with zero extra `env.*` imports).
 */

#ifndef SQLITE_OK
#include "sqlite3.h"
#endif

typedef struct {
    const sqlite3_io_methods *pMethods;
    char path[256];
    int is_temp;
    int dirty;
    uint8_t *buf;
    size_t len;
    size_t cap;
} WasiSqliteFile;

static int _wasi_sq_sync(sqlite3_file *sf, int flags) {
    (void)flags;
    WasiSqliteFile *f = (WasiSqliteFile *)sf;
    if (!f) return SQLITE_ERROR;
    if (f->is_temp || !f->dirty || !f->path[0]) return SQLITE_OK;
    FILE *fp = fopen(f->path, "wb");
    if (!fp) return SQLITE_IOERR;
    if (f->len > 0 && fwrite(f->buf, 1U, f->len, fp) != f->len) {
        fclose(fp);
        return SQLITE_IOERR;
    }
    if (fclose(fp) != 0) return SQLITE_IOERR;
    f->dirty = 0;
    return SQLITE_OK;
}

static int _wasi_sq_close(sqlite3_file *sf) {
    WasiSqliteFile *f = (WasiSqliteFile *)sf;
    if (!f) return SQLITE_OK;
    int rc = _wasi_sq_sync(sf, 0);
    if (f->buf) {
        free(f->buf);
        f->buf = NULL;
    }
    f->len = 0;
    f->cap = 0;
    return rc;
}

static int _wasi_sq_read(sqlite3_file *sf, void *zBuf, int iAmt, sqlite3_int64 iOfst) {
    WasiSqliteFile *f = (WasiSqliteFile *)sf;
    if (!f || iAmt < 0 || iOfst < 0) return SQLITE_ERROR;
    size_t off = (size_t)iOfst;
    size_t amt = (size_t)iAmt;
    if (off >= f->len) {
        if (amt > 0) memset(zBuf, 0, amt);
        return SQLITE_IOERR_SHORT_READ;
    }
    size_t avail = f->len - off;
    if (avail >= amt) {
        memcpy(zBuf, f->buf + off, amt);
        return SQLITE_OK;
    }
    memcpy(zBuf, f->buf + off, avail);
    memset((uint8_t *)zBuf + avail, 0, amt - avail);
    return SQLITE_IOERR_SHORT_READ;
}

static int _wasi_sq_write(sqlite3_file *sf, const void *zBuf, int iAmt, sqlite3_int64 iOfst) {
    WasiSqliteFile *f = (WasiSqliteFile *)sf;
    if (!f || iAmt < 0 || iOfst < 0) return SQLITE_ERROR;
    size_t off = (size_t)iOfst;
    size_t amt = (size_t)iAmt;
    size_t need = off + amt;
    if (need > f->cap) {
        size_t ncap = (f->cap > 4096U) ? f->cap : 4096U;
        while (ncap < need) ncap *= 2U;
        uint8_t *nb = (uint8_t *)realloc(f->buf, ncap);
        if (!nb) return SQLITE_NOMEM;
        if (ncap > f->cap) memset(nb + f->cap, 0, ncap - f->cap);
        f->buf = nb;
        f->cap = ncap;
    }
    if (off > f->len) {
        memset(f->buf + f->len, 0, off - f->len);
    }
    if (amt > 0) memcpy(f->buf + off, zBuf, amt);
    if (need > f->len) f->len = need;
    f->dirty = 1;
    return SQLITE_OK;
}

static int _wasi_sq_truncate(sqlite3_file *sf, sqlite3_int64 size) {
    WasiSqliteFile *f = (WasiSqliteFile *)sf;
    if (!f || size < 0) return SQLITE_ERROR;
    if ((size_t)size < f->len) {
        f->len = (size_t)size;
        f->dirty = 1;
    }
    return SQLITE_OK;
}

static int _wasi_sq_filesize(sqlite3_file *sf, sqlite3_int64 *pSize) {
    WasiSqliteFile *f = (WasiSqliteFile *)sf;
    if (!f || !pSize) return SQLITE_ERROR;
    *pSize = (sqlite3_int64)f->len;
    return SQLITE_OK;
}

static int _wasi_sq_lock(sqlite3_file *sf, int level) { (void)sf; (void)level; return SQLITE_OK; }
static int _wasi_sq_unlock(sqlite3_file *sf, int level) { (void)sf; (void)level; return SQLITE_OK; }
static int _wasi_sq_check_reserved(sqlite3_file *sf, int *pResOut) {
    (void)sf;
    if (pResOut) *pResOut = 0;
    return SQLITE_OK;
}
static int _wasi_sq_fcntl(sqlite3_file *sf, int op, void *pArg) {
    (void)sf; (void)op; (void)pArg;
    return SQLITE_NOTFOUND;
}
static int _wasi_sq_sector_size(sqlite3_file *sf) { (void)sf; return 4096; }
static int _wasi_sq_dev_chars(sqlite3_file *sf) { (void)sf; return 0; }

static sqlite3_io_methods _wasi_sq_io_methods;

static int _wasi_sq_vfs_open(sqlite3_vfs *vfs, sqlite3_filename zName, sqlite3_file *sf,
                             int flags, int *pOutFlags) {
    (void)vfs;
    WasiSqliteFile *f = (WasiSqliteFile *)sf;
    memset(f, 0, sizeof(WasiSqliteFile));
    f->pMethods = &_wasi_sq_io_methods;
    if (pOutFlags) *pOutFlags = flags;

    if (!zName || !zName[0]) {
        f->is_temp = 1;
        return SQLITE_OK;
    }

    size_t nlen = strlen(zName);
    if (nlen >= sizeof(f->path)) return SQLITE_CANTOPEN;
    memcpy(f->path, zName, nlen + 1U);
    f->is_temp = 0;

    FILE *fp = fopen(f->path, "rb");
    if (fp) {
        size_t cap = 4096U;
        size_t len = 0U;
        uint8_t *buf = (uint8_t *)malloc(cap);
        if (!buf) { fclose(fp); return SQLITE_NOMEM; }
        for (;;) {
            if (len + 4096U > cap) {
                size_t ncap = cap * 2U;
                uint8_t *nb = (uint8_t *)realloc(buf, ncap);
                if (!nb) { free(buf); fclose(fp); return SQLITE_NOMEM; }
                buf = nb;
                cap = ncap;
            }
            size_t nr = fread(buf + len, 1U, 4096U, fp);
            len += nr;
            if (nr < 4096U) break;
        }
        fclose(fp);
        f->buf = buf;
        f->len = len;
        f->cap = cap;
        return SQLITE_OK;
    }

    if (flags & SQLITE_OPEN_CREATE) {
        f->dirty = 1;
        return SQLITE_OK;
    }
    return SQLITE_CANTOPEN;
}

static int _wasi_sq_vfs_delete(sqlite3_vfs *vfs, const char *zName, int syncDir) {
    (void)vfs; (void)syncDir;
    if (!zName) return SQLITE_OK;
    if (access(zName, 0) != 0) return SQLITE_IOERR_DELETE_NOENT;
    return (remove(zName) == 0) ? SQLITE_OK : SQLITE_IOERR;
}

static int _wasi_sq_vfs_access(sqlite3_vfs *vfs, const char *zName, int flags, int *pResOut) {
    (void)vfs; (void)flags;
    if (pResOut) *pResOut = (zName && access(zName, 0) == 0) ? 1 : 0;
    return SQLITE_OK;
}

static int _wasi_sq_vfs_fullpath(sqlite3_vfs *vfs, const char *zName, int nOut, char *zOut) {
    (void)vfs;
    if (!zName || !zOut || nOut <= 0) return SQLITE_ERROR;
    size_t n = strlen(zName);
    if (n >= (size_t)nOut) return SQLITE_CANTOPEN_FULLPATH;
    memcpy(zOut, zName, n + 1U);
    return SQLITE_OK;
}

static void *_wasi_sq_dlopen(sqlite3_vfs *v, const char *n) { (void)v; (void)n; return NULL; }
static void _wasi_sq_dlerror(sqlite3_vfs *v, int n, char *m) { (void)v; if (m && n > 0) m[0] = '\0'; }
static sqlite3_syscall_ptr _wasi_sq_dlsym(sqlite3_vfs *v, void *p, const char *s) { (void)v; (void)p; (void)s; return NULL; }
static void _wasi_sq_dlclose(sqlite3_vfs *v, void *p) { (void)v; (void)p; }

static uint64_t _wasi_sq_rng_state = 0x9E3779B97F4A7C15ULL;
static int _wasi_sq_vfs_randomness(sqlite3_vfs *vfs, int nByte, char *zOut) {
    (void)vfs;
    uint64_t ns = 0;
    if (__wasi_clock_time_get(0U, 1000ULL, &ns) == 0) {
        _wasi_sq_rng_state ^= ns;
    }
    int i;
    for (i = 0; i < nByte; i++) {
        _wasi_sq_rng_state ^= _wasi_sq_rng_state >> 12;
        _wasi_sq_rng_state ^= _wasi_sq_rng_state << 25;
        _wasi_sq_rng_state ^= _wasi_sq_rng_state >> 27;
        zOut[i] = (char)((_wasi_sq_rng_state * 0x2545F4914F6CDD1DULL) >> 56);
    }
    return nByte;
}

static int _wasi_sq_vfs_sleep(sqlite3_vfs *vfs, int microseconds) {
    (void)vfs;
    return microseconds;
}

static int _wasi_sq_vfs_time64(sqlite3_vfs *vfs, sqlite3_int64 *piNow) {
    (void)vfs;
    uint64_t ns = 0;
    sqlite3_int64 ms = 1795000000000LL;
    if (__wasi_clock_time_get(0U, 1000000ULL, &ns) == 0 && ns > 0ULL) {
        ms = (sqlite3_int64)(ns / 1000000ULL);
    }
    if (piNow) *piNow = 210866760000000LL + ms;
    return SQLITE_OK;
}

static int _wasi_sq_vfs_time(sqlite3_vfs *vfs, double *prNow) {
    sqlite3_int64 now64 = 0;
    _wasi_sq_vfs_time64(vfs, &now64);
    if (prNow) *prNow = (double)now64 / 86400000.0;
    return SQLITE_OK;
}

static int _wasi_sq_vfs_lasterr(sqlite3_vfs *vfs, int n, char *m) {
    (void)vfs;
    if (m && n > 0) m[0] = '\0';
    return 0;
}

static sqlite3_vfs _wasi_sq_vfs;

int sqlite3_os_init(void) {
    memset(&_wasi_sq_io_methods, 0, sizeof(_wasi_sq_io_methods));
    _wasi_sq_io_methods.iVersion = 1;
    _wasi_sq_io_methods.xClose = _wasi_sq_close;
    _wasi_sq_io_methods.xRead = _wasi_sq_read;
    _wasi_sq_io_methods.xWrite = _wasi_sq_write;
    _wasi_sq_io_methods.xTruncate = _wasi_sq_truncate;
    _wasi_sq_io_methods.xSync = _wasi_sq_sync;
    _wasi_sq_io_methods.xFileSize = _wasi_sq_filesize;
    _wasi_sq_io_methods.xLock = _wasi_sq_lock;
    _wasi_sq_io_methods.xUnlock = _wasi_sq_unlock;
    _wasi_sq_io_methods.xCheckReservedLock = _wasi_sq_check_reserved;
    _wasi_sq_io_methods.xFileControl = _wasi_sq_fcntl;
    _wasi_sq_io_methods.xSectorSize = _wasi_sq_sector_size;
    _wasi_sq_io_methods.xDeviceCharacteristics = _wasi_sq_dev_chars;

    memset(&_wasi_sq_vfs, 0, sizeof(_wasi_sq_vfs));
    _wasi_sq_vfs.iVersion = 2;
    _wasi_sq_vfs.szOsFile = (int)sizeof(WasiSqliteFile);
    _wasi_sq_vfs.mxPathname = 255;
    _wasi_sq_vfs.zName = "zcc_wasi";
    _wasi_sq_vfs.xOpen = _wasi_sq_vfs_open;
    _wasi_sq_vfs.xDelete = _wasi_sq_vfs_delete;
    _wasi_sq_vfs.xAccess = _wasi_sq_vfs_access;
    _wasi_sq_vfs.xFullPathname = _wasi_sq_vfs_fullpath;
    _wasi_sq_vfs.xDlOpen = _wasi_sq_dlopen;
    _wasi_sq_vfs.xDlError = _wasi_sq_dlerror;
    _wasi_sq_vfs.xDlSym = (void (*(*)(sqlite3_vfs *, void *, const char *))(void))_wasi_sq_dlsym;
    _wasi_sq_vfs.xDlClose = _wasi_sq_dlclose;
    _wasi_sq_vfs.xRandomness = _wasi_sq_vfs_randomness;
    _wasi_sq_vfs.xSleep = _wasi_sq_vfs_sleep;
    _wasi_sq_vfs.xCurrentTime = _wasi_sq_vfs_time;
    _wasi_sq_vfs.xGetLastError = _wasi_sq_vfs_lasterr;
    _wasi_sq_vfs.xCurrentTimeInt64 = _wasi_sq_vfs_time64;

    return sqlite3_vfs_register(&_wasi_sq_vfs, 1);
}

int sqlite3_os_end(void) {
    return SQLITE_OK;
}

static int _wasi_sq_row_cb(void *unused, int argc, char **argv, char **colNames) {
    (void)unused; (void)colNames;
    int i;
    for (i = 0; i < argc; i++) {
        if (i > 0) putchar('|');
        fputs(argv[i] ? argv[i] : "NULL", stdout);
    }
    putchar('\n');
    return 0;
}

static char *_wasi_sq_read_all(FILE *fp) {
    size_t cap = 4096U, len = 0U;
    char *buf = (char *)malloc(cap);
    if (!buf) return NULL;
    for (;;) {
        if (len + 1024U >= cap) {
            size_t ncap = cap * 2U;
            char *nb = (char *)realloc(buf, ncap);
            if (!nb) { free(buf); return NULL; }
            buf = nb;
            cap = ncap;
        }
        size_t nr = fread(buf + len, 1U, 1024U, fp);
        len += nr;
        if (nr < 1024U) break;
    }
    buf[len] = '\0';
    return buf;
}

int main(int argc, char **argv) {
    const char *db_path = (argc >= 2 && argv[1][0]) ? argv[1] : ":memory:";
    sqlite3 *db = NULL;
    int rc = sqlite3_open(db_path, &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQLITE_ERROR: open(%s): %s\n", db_path, db ? sqlite3_errmsg(db) : "OOM");
        if (db) sqlite3_close(db);
        return 1;
    }

    char *errmsg = NULL;
    if (argc >= 4 && strcmp(argv[2], "-f") == 0) {
        FILE *fp = fopen(argv[3], "rb");
        if (!fp) {
            fprintf(stderr, "SQLITE_ERROR: cannot open SQL file '%s'\n", argv[3]);
            sqlite3_close(db);
            return 1;
        }
        char *sql = _wasi_sq_read_all(fp);
        fclose(fp);
        if (!sql) { sqlite3_close(db); return 1; }
        rc = sqlite3_exec(db, sql, _wasi_sq_row_cb, NULL, &errmsg);
        free(sql);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "SQLITE_ERROR: %s\n", errmsg ? errmsg : sqlite3_errstr(rc));
            if (errmsg) sqlite3_free(errmsg);
            sqlite3_close(db);
            return 1;
        }
    } else if (argc >= 3) {
        int i;
        for (i = 2; i < argc; i++) {
            rc = sqlite3_exec(db, argv[i], _wasi_sq_row_cb, NULL, &errmsg);
            if (rc != SQLITE_OK) {
                fprintf(stderr, "SQLITE_ERROR: %s\n", errmsg ? errmsg : sqlite3_errstr(rc));
                if (errmsg) sqlite3_free(errmsg);
                sqlite3_close(db);
                return 1;
            }
        }
    } else {
        char *sql = _wasi_sq_read_all(stdin);
        if (!sql) { sqlite3_close(db); return 1; }
        rc = sqlite3_exec(db, sql, _wasi_sq_row_cb, NULL, &errmsg);
        free(sql);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "SQLITE_ERROR: %s\n", errmsg ? errmsg : sqlite3_errstr(rc));
            if (errmsg) sqlite3_free(errmsg);
            sqlite3_close(db);
            return 1;
        }
    }

    fflush(stdout);
    sqlite3_close(db);
    return 0;
}
