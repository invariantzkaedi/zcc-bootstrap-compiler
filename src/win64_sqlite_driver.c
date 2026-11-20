/*
 * ZCC Native Win64 MSVCRT VFS & CLI Driver for SQLite 3.45.0 (`src/win64_sqlite_driver.c`)
 * ========================================================================================
 * Implements `sqlite3_os_init` / `sqlite3_os_end` and default VFS `"zcc_win64"` over
 * `msvcrt.dll` stdio/time/filesystem primitives with zero MSVC/MinGW/binutils.
 *
 * Usage:
 *   sqlite3.exe <db_path|:memory:> <sql> [sql...]
 *   sqlite3.exe <db_path|:memory:> -f <sql_file>
 *   sqlite3.exe <db_path|:memory:>        (reads SQL from stdin)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

extern int _access(const char *path, int mode);

#include "sqlite3.h"

typedef struct {
    const sqlite3_io_methods *pMethods;
    char path[512];
    int is_temp;
    int dirty;
    uint8_t *buf;
    size_t len;
    size_t cap;
} Win64SqliteFile;

static int _win64_sq_sync(sqlite3_file *sf, int flags) {
    (void)flags;
    Win64SqliteFile *f = (Win64SqliteFile *)sf;
    if (!f) return SQLITE_ERROR;
    if (f->is_temp || !f->dirty || !f->path[0]) return SQLITE_OK;
    FILE *fp = fopen(f->path, "wb");
    if (!fp) return SQLITE_IOERR;
    if (f->len > 0U && fwrite(f->buf, 1U, f->len, fp) != f->len) {
        fclose(fp);
        return SQLITE_IOERR;
    }
    if (fclose(fp) != 0) return SQLITE_IOERR;
    f->dirty = 0;
    return SQLITE_OK;
}

static int _win64_sq_close(sqlite3_file *sf) {
    Win64SqliteFile *f = (Win64SqliteFile *)sf;
    if (!f) return SQLITE_OK;
    int rc = _win64_sq_sync(sf, 0);
    if (f->buf) {
        free(f->buf);
        f->buf = NULL;
    }
    f->len = 0U;
    f->cap = 0U;
    return rc;
}

static int _win64_sq_read(sqlite3_file *sf, void *zBuf, int iAmt, sqlite3_int64 iOfst) {
    Win64SqliteFile *f = (Win64SqliteFile *)sf;
    if (!f || iAmt < 0 || iOfst < 0) return SQLITE_ERROR;
    size_t off = (size_t)iOfst;
    size_t amt = (size_t)iAmt;
    if (off >= f->len) {
        if (amt > 0U) memset(zBuf, 0, amt);
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

static int _win64_sq_write(sqlite3_file *sf, const void *zBuf, int iAmt, sqlite3_int64 iOfst) {
    Win64SqliteFile *f = (Win64SqliteFile *)sf;
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
    if (amt > 0U) memcpy(f->buf + off, zBuf, amt);
    if (need > f->len) f->len = need;
    f->dirty = 1;
    return SQLITE_OK;
}

static int _win64_sq_truncate(sqlite3_file *sf, sqlite3_int64 size) {
    Win64SqliteFile *f = (Win64SqliteFile *)sf;
    if (!f || size < 0) return SQLITE_ERROR;
    if ((size_t)size < f->len) {
        f->len = (size_t)size;
        f->dirty = 1;
    }
    return SQLITE_OK;
}

static int _win64_sq_filesize(sqlite3_file *sf, sqlite3_int64 *pSize) {
    Win64SqliteFile *f = (Win64SqliteFile *)sf;
    if (!f || !pSize) return SQLITE_ERROR;
    *pSize = (sqlite3_int64)f->len;
    return SQLITE_OK;
}

static int _win64_sq_lock(sqlite3_file *sf, int level) { (void)sf; (void)level; return SQLITE_OK; }
static int _win64_sq_unlock(sqlite3_file *sf, int level) { (void)sf; (void)level; return SQLITE_OK; }
static int _win64_sq_check_reserved(sqlite3_file *sf, int *pResOut) {
    (void)sf;
    if (pResOut) *pResOut = 0;
    return SQLITE_OK;
}
static int _win64_sq_fcntl(sqlite3_file *sf, int op, void *pArg) {
    (void)sf; (void)op; (void)pArg;
    return SQLITE_NOTFOUND;
}
static int _win64_sq_sector_size(sqlite3_file *sf) { (void)sf; return 4096; }
static int _win64_sq_dev_chars(sqlite3_file *sf) { (void)sf; return 0; }

static sqlite3_io_methods _win64_sq_io_methods;

static int _win64_sq_vfs_open(sqlite3_vfs *vfs, sqlite3_filename zName, sqlite3_file *sf,
                               int flags, int *pOutFlags) {
    (void)vfs;
    Win64SqliteFile *f = (Win64SqliteFile *)sf;
    memset(f, 0, sizeof(Win64SqliteFile));
    f->pMethods = &_win64_sq_io_methods;
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
            if (nr == 0U) break;
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

static int _win64_sq_vfs_delete(sqlite3_vfs *vfs, const char *zName, int syncDir) {
    (void)vfs; (void)syncDir;
    if (!zName) return SQLITE_OK;
    if (_access(zName, 0) != 0) return SQLITE_IOERR_DELETE_NOENT;
    return (remove(zName) == 0) ? SQLITE_OK : SQLITE_IOERR;
}

static int _win64_sq_vfs_access(sqlite3_vfs *vfs, const char *zName, int flags, int *pResOut) {
    (void)vfs; (void)flags;
    if (pResOut) *pResOut = (zName && _access(zName, 0) == 0) ? 1 : 0;
    return SQLITE_OK;
}

static int _win64_sq_vfs_fullpath(sqlite3_vfs *vfs, const char *zName, int nOut, char *zOut) {
    (void)vfs;
    if (!zName || !zOut || nOut <= 0) return SQLITE_ERROR;
    size_t n = strlen(zName);
    if (n >= (size_t)nOut) return SQLITE_CANTOPEN_FULLPATH;
    memcpy(zOut, zName, n + 1U);
    return SQLITE_OK;
}

static void *_win64_sq_dlopen(sqlite3_vfs *v, const char *n) { (void)v; (void)n; return NULL; }
static void _win64_sq_dlerror(sqlite3_vfs *v, int n, char *m) { (void)v; if (m && n > 0) m[0] = '\0'; }
static sqlite3_syscall_ptr _win64_sq_dlsym(sqlite3_vfs *v, void *p, const char *s) { (void)v; (void)p; (void)s; return NULL; }
static void _win64_sq_dlclose(sqlite3_vfs *v, void *p) { (void)v; (void)p; }

static uint64_t _win64_sq_rng_state = 0x9E3779B97F4A7C15ULL;
static int _win64_sq_vfs_randomness(sqlite3_vfs *vfs, int nByte, char *zOut) {
    (void)vfs;
    _win64_sq_rng_state ^= (uint64_t)time(NULL) ^ ((uint64_t)clock() << 20);
    int i;
    for (i = 0; i < nByte; i++) {
        _win64_sq_rng_state ^= _win64_sq_rng_state >> 12;
        _win64_sq_rng_state ^= _win64_sq_rng_state << 25;
        _win64_sq_rng_state ^= _win64_sq_rng_state >> 27;
        zOut[i] = (char)((_win64_sq_rng_state * 0x2545F4914F6CDD1DULL) >> 56);
    }
    return nByte;
}

static int _win64_sq_vfs_sleep(sqlite3_vfs *vfs, int microseconds) {
    (void)vfs;
    return microseconds;
}

static int _win64_sq_vfs_time64(sqlite3_vfs *vfs, sqlite3_int64 *piNow) {
    (void)vfs;
    sqlite3_int64 ms = (sqlite3_int64)time(NULL) * 1000LL;
    if (piNow) *piNow = 210866760000000LL + ms;
    return SQLITE_OK;
}

static int _win64_sq_vfs_time(sqlite3_vfs *vfs, double *prNow) {
    sqlite3_int64 now64 = 0;
    _win64_sq_vfs_time64(vfs, &now64);
    if (prNow) *prNow = (double)now64 / 86400000.0;
    return SQLITE_OK;
}

static int _win64_sq_vfs_lasterr(sqlite3_vfs *vfs, int n, char *m) {
    (void)vfs;
    if (m && n > 0) m[0] = '\0';
    return 0;
}

static sqlite3_vfs _win64_sq_vfs;

int sqlite3_os_init(void) {
    memset(&_win64_sq_io_methods, 0, sizeof(_win64_sq_io_methods));
    _win64_sq_io_methods.iVersion = 1;
    _win64_sq_io_methods.xClose = _win64_sq_close;
    _win64_sq_io_methods.xRead = _win64_sq_read;
    _win64_sq_io_methods.xWrite = _win64_sq_write;
    _win64_sq_io_methods.xTruncate = _win64_sq_truncate;
    _win64_sq_io_methods.xSync = _win64_sq_sync;
    _win64_sq_io_methods.xFileSize = _win64_sq_filesize;
    _win64_sq_io_methods.xLock = _win64_sq_lock;
    _win64_sq_io_methods.xUnlock = _win64_sq_unlock;
    _win64_sq_io_methods.xCheckReservedLock = _win64_sq_check_reserved;
    _win64_sq_io_methods.xFileControl = _win64_sq_fcntl;
    _win64_sq_io_methods.xSectorSize = _win64_sq_sector_size;
    _win64_sq_io_methods.xDeviceCharacteristics = _win64_sq_dev_chars;

    memset(&_win64_sq_vfs, 0, sizeof(_win64_sq_vfs));
    _win64_sq_vfs.iVersion = 2;
    _win64_sq_vfs.szOsFile = (int)sizeof(Win64SqliteFile);
    _win64_sq_vfs.mxPathname = 511;
    _win64_sq_vfs.zName = "zcc_win64";
    _win64_sq_vfs.xOpen = _win64_sq_vfs_open;
    _win64_sq_vfs.xDelete = _win64_sq_vfs_delete;
    _win64_sq_vfs.xAccess = _win64_sq_vfs_access;
    _win64_sq_vfs.xFullPathname = _win64_sq_vfs_fullpath;
    _win64_sq_vfs.xDlOpen = _win64_sq_dlopen;
    _win64_sq_vfs.xDlError = _win64_sq_dlerror;
    _win64_sq_vfs.xDlSym = (void (*(*)(sqlite3_vfs *, void *, const char *))(void))_win64_sq_dlsym;
    _win64_sq_vfs.xDlClose = _win64_sq_dlclose;
    _win64_sq_vfs.xRandomness = _win64_sq_vfs_randomness;
    _win64_sq_vfs.xSleep = _win64_sq_vfs_sleep;
    _win64_sq_vfs.xCurrentTime = _win64_sq_vfs_time;
    _win64_sq_vfs.xGetLastError = _win64_sq_vfs_lasterr;
    _win64_sq_vfs.xCurrentTimeInt64 = _win64_sq_vfs_time64;

    return sqlite3_vfs_register(&_win64_sq_vfs, 1);
}

int sqlite3_os_end(void) {
    return SQLITE_OK;
}

static int _win64_sq_row_cb(void *unused, int argc, char **argv, char **colNames) {
    (void)unused; (void)colNames;
    int i;
    for (i = 0; i < argc; i++) {
        if (i > 0) putchar('|');
        fputs(argv[i] ? argv[i] : "NULL", stdout);
    }
    putchar('\n');
    return 0;
}

static char *_win64_sq_read_all(FILE *fp) {
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
        if (nr == 0U) break;
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
        char *sql = _win64_sq_read_all(fp);
        fclose(fp);
        if (!sql) { sqlite3_close(db); return 1; }
        rc = sqlite3_exec(db, sql, _win64_sq_row_cb, NULL, &errmsg);
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
            rc = sqlite3_exec(db, argv[i], _win64_sq_row_cb, NULL, &errmsg);
            if (rc != SQLITE_OK) {
                fprintf(stderr, "SQLITE_ERROR: %s\n", errmsg ? errmsg : sqlite3_errstr(rc));
                if (errmsg) sqlite3_free(errmsg);
                sqlite3_close(db);
                return 1;
            }
        }
    } else {
        char *sql = _win64_sq_read_all(stdin);
        if (!sql) { sqlite3_close(db); return 1; }
        rc = sqlite3_exec(db, sql, _win64_sq_row_cb, NULL, &errmsg);
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
