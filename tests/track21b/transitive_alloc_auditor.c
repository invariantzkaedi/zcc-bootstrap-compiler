#define _GNU_SOURCE
#include <dlfcn.h>
#include <elf.h>
#include <errno.h>
#include <execinfo.h>
#include <link.h>
#include <pthread.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

/*
 * ZCC Track 21B Independent Transitive Allocator & Module-Lifetime Auditor
 * (ZIG-VK-TRANSITIVE-001..008 / ZCC-EF-0 Independent E3 Observer)
 *
 * Preloaded via LD_PRELOAD to independently verify without sanitizer suppressions:
 *   1. Exact tracking across malloc, calloc, realloc, reallocarray,
 *      posix_memalign, aligned_alloc, memalign, valloc, and free.
 *   2. Exact address reuse removal and failed-allocation preservation (OOM-safe).
 *   3. Pre-dlclose and post-dlclose live-allocation accounting per ELF Build ID,
 *      callsite offset, writable-data (.data/.bss) reachability classification,
 *      and genuine /proc/self/maps + dl_iterate_phdr unload verification.
 *   4. Structured JSON receipt emitted to ZCC_AUDIT_JSON_OUT (plus concise stderr line)
 *      and optional non-zero abort if ZCC_AUDIT_FAIL_ON_LEAK=1.
 */

#define BOOT_POOL_SIZE   (512u * 1024u)
#define ALLOC_SLOTS      (1u << 20)  /* 1,048,576 open-addressed slots with backward-shift delete */
#define MAX_FRAMES       6
#define MAX_MODS         256
#define MAX_SEGS         16
#define MAX_BUCKETS      4096

enum {
    SLOT_EMPTY = 0,
    SLOT_LIVE = 1
};

typedef struct {
    uintptr_t start;
    uintptr_t end;
    uint32_t  flags;
} seg_range_t;

typedef struct {
    char        name[256];
    char        base[96];
    char        build_id[80];
    uintptr_t   base_addr;
    uintptr_t   rx_start;
    uintptr_t   rx_end;
    int         n_segs;
    seg_range_t segs[MAX_SEGS];
    int         active;
} mod_info_t;

typedef struct {
    char      mod_base[96];
    char      build_id[80];
    uintptr_t rel_off;
    uintptr_t abs_pc;
} frame_attr_t;

typedef struct {
    uintptr_t    ptr;
    size_t       size;
    uint64_t     seq;
    uint32_t     state;
    uint32_t     orphaned_on_dlclose;
    uint32_t     reachable_before_dlclose;
    uint32_t     reachable_at_exit;
    uint32_t     needs_resolve;
    int          n_frames;
    frame_attr_t frames[MAX_FRAMES];
} alloc_slot_t;

typedef struct {
    uint64_t total_allocs;
    uint64_t total_frees;
    uint64_t total_reallocs;
    uint64_t failed_allocs;
    uint64_t failed_reallocs;
    uint64_t address_reuses;
    uint64_t live_count;
    uint64_t live_bytes;
    uint64_t peak_live_bytes;
    uint64_t dlopen_calls;
    uint64_t dlclose_calls;
    uint64_t explicit_rtld_nodelete_calls;
    uint64_t orphaned_on_dlclose_count;
    uint64_t orphaned_on_dlclose_bytes;
    uint64_t unmapped_lvp_events;
    uint64_t retained_lvp_events;
} audit_stats_t;

typedef struct {
    char      mod_base[96];
    char      build_id[80];
    uint64_t  stack_key;
    char      stack_desc[512];
    uint64_t  count;
    uint64_t  bytes;
    uint64_t  orphaned_count;
    uint64_t  reachable_pre_dlclose;
    size_t    min_size;
    size_t    max_size;
} leak_bucket_t;

typedef struct {
    char      mod_base[96];
    char      build_id[80];
    int       pinned_mapped;
    int       is_system_runtime;
    uint64_t  count;
    uint64_t  bytes;
    uint64_t  orphaned_count;
    uint64_t  global_reachable_count;
    uint64_t  global_reachable_bytes;
} mod_live_t;

/* Real glibc functions */
static void *(*real_malloc)(size_t) = NULL;
static void *(*real_calloc)(size_t, size_t) = NULL;
static void *(*real_realloc)(void *, size_t) = NULL;
static void *(*real_reallocarray)(void *, size_t, size_t) = NULL;
static void  (*real_free)(void *) = NULL;
static int   (*real_posix_memalign)(void **, size_t, size_t) = NULL;
static void *(*real_aligned_alloc)(size_t, size_t) = NULL;
static void *(*real_memalign)(size_t, size_t) = NULL;
static void *(*real_valloc)(size_t) = NULL;
static void *(*real_dlopen)(const char *, int) = NULL;
static int   (*real_dlclose)(void *) = NULL;

static unsigned char g_boot_pool[BOOT_POOL_SIZE];
static size_t g_boot_off = 0;

static volatile int g_spin = 0;
static volatile int g_refresh_spin = 0;
static volatile int g_initialized = 0;
static volatile int g_finalized = 0;
static __thread int t_in_hook = 0;

static alloc_slot_t *g_slots = NULL;
static mod_info_t    g_mods[MAX_MODS];
static int           g_num_mods = 0;
static mod_info_t    g_snap_mods[MAX_MODS];
static int           g_snap_num_mods = 0;
static mod_info_t    g_seen_mods[MAX_MODS];
static int           g_num_seen_mods = 0;
static uint64_t      g_seq = 1;
static audit_stats_t g_stats;

static char g_target_dso[128] = "libvulkan_lvp";
static int  g_bt_enabled = 1;

static inline void spin_lock(void) {
    while (__atomic_test_and_set(&g_spin, __ATOMIC_ACQUIRE)) {
        sched_yield();
    }
}

static inline void spin_unlock(void) {
    __atomic_clear(&g_spin, __ATOMIC_RELEASE);
}

static inline void refresh_lock(void) {
    while (__atomic_test_and_set(&g_refresh_spin, __ATOMIC_ACQUIRE)) {
        sched_yield();
    }
}

static inline void refresh_unlock(void) {
    __atomic_clear(&g_refresh_spin, __ATOMIC_RELEASE);
}

static int is_boot_ptr(const void *p) {
    uintptr_t u = (uintptr_t)p;
    uintptr_t b = (uintptr_t)g_boot_pool;
    return (u >= b && u < b + BOOT_POOL_SIZE);
}

static void *boot_alloc(size_t align, size_t sz) {
    if (align < 16) align = 16;
    for (;;) {
        size_t cur = __atomic_load_n(&g_boot_off, __ATOMIC_RELAXED);
        size_t aligned = (cur + align - 1) & ~(align - 1);
        if (aligned + sz > BOOT_POOL_SIZE) return NULL;
        if (__atomic_compare_exchange_n(&g_boot_off, &cur, aligned + sz, 0,
                                        __ATOMIC_ACQ_REL, __ATOMIC_RELAXED)) {
            void *p = (void *)(g_boot_pool + aligned);
            memset(p, 0, sz);
            return p;
        }
    }
}

static const char *path_basename(const char *s) {
    if (!s || !*s) return "(main)";
    const char *sl = strrchr(s, '/');
    return sl ? (sl + 1) : s;
}

static void format_build_id(const struct dl_phdr_info *info, char *out, size_t out_sz) {
    out[0] = '\0';
    for (int i = 0; i < (int)info->dlpi_phnum; i++) {
        if (info->dlpi_phdr[i].p_type != PT_NOTE) continue;
        uintptr_t p = (uintptr_t)info->dlpi_addr + (uintptr_t)info->dlpi_phdr[i].p_vaddr;
        uintptr_t end = p + (uintptr_t)info->dlpi_phdr[i].p_memsz;
        while (p + sizeof(ElfW(Nhdr)) <= end) {
            const ElfW(Nhdr) *nh = (const ElfW(Nhdr) *)p;
            uintptr_t name_off = sizeof(ElfW(Nhdr));
            uintptr_t desc_off = name_off + ((nh->n_namesz + 3u) & ~3u);
            uintptr_t next = desc_off + ((nh->n_descsz + 3u) & ~3u);
            if (p + next > end || next == 0) break;
            const char *nm = (const char *)(p + name_off);
            if (nh->n_type == NT_GNU_BUILD_ID && nh->n_namesz >= 4 && memcmp(nm, "GNU", 3) == 0) {
                const unsigned char *desc = (const unsigned char *)(p + desc_off);
                size_t k = 0;
                for (size_t j = 0; j < nh->n_descsz && k + 2 < out_sz; j++) {
                    static const char hex[] = "0123456789abcdef";
                    out[k++] = hex[(desc[j] >> 4) & 0xf];
                    out[k++] = hex[desc[j] & 0xf];
                }
                out[k] = '\0';
                return;
            }
            p += next;
        }
    }
    snprintf(out, out_sz, "none");
}

typedef struct {
    mod_info_t *mods;
    int        *num_mods;
} snap_ctx_t;

static int phdr_cb(struct dl_phdr_info *info, size_t size, void *data) {
    (void)size;
    snap_ctx_t *sc = (snap_ctx_t *)data;
    if (*sc->num_mods >= MAX_MODS) return 1;
    mod_info_t *m = &sc->mods[(*sc->num_mods)++];
    memset(m, 0, sizeof(*m));
    snprintf(m->name, sizeof(m->name), "%.255s", (info->dlpi_name && info->dlpi_name[0]) ? info->dlpi_name : "(main)");
    snprintf(m->base, sizeof(m->base), "%.95s", path_basename(m->name));
    format_build_id(info, m->build_id, sizeof(m->build_id));
    m->base_addr = (uintptr_t)info->dlpi_addr;
    m->active = 1;
    for (int i = 0; i < (int)info->dlpi_phnum && m->n_segs < MAX_SEGS; i++) {
        if (info->dlpi_phdr[i].p_type != PT_LOAD || info->dlpi_phdr[i].p_memsz == 0) continue;
        uintptr_t s = (uintptr_t)info->dlpi_addr + (uintptr_t)info->dlpi_phdr[i].p_vaddr;
        uintptr_t e = s + (uintptr_t)info->dlpi_phdr[i].p_memsz;
        m->segs[m->n_segs].start = s;
        m->segs[m->n_segs].end = e;
        m->segs[m->n_segs].flags = info->dlpi_phdr[i].p_flags;
        if ((info->dlpi_phdr[i].p_flags & PF_X) && m->rx_start == 0) {
            m->rx_start = s;
            m->rx_end = e;
        }
        m->n_segs++;
    }
    return 0;
}

static const mod_info_t *find_mod_for_pc_locked(uintptr_t pc) {
    for (int i = 0; i < g_num_mods; i++) {
        const mod_info_t *m = &g_mods[i];
        for (int s = 0; s < m->n_segs; s++) {
            if (pc >= m->segs[s].start && pc < m->segs[s].end) {
                return m;
            }
        }
    }
    return NULL;
}

static void resolve_slot_frames_locked(alloc_slot_t *s) {
    int still_missing = 0;
    for (int i = 0; i < s->n_frames; i++) {
        frame_attr_t *fa = &s->frames[i];
        if (strcmp(fa->mod_base, "unknown") != 0) continue;
        const mod_info_t *m = find_mod_for_pc_locked(fa->abs_pc);
        if (m) {
            snprintf(fa->mod_base, sizeof(fa->mod_base), "%s", m->base);
            snprintf(fa->build_id, sizeof(fa->build_id), "%s", m->build_id);
            fa->rel_off = (fa->abs_pc >= m->base_addr) ? (fa->abs_pc - m->base_addr) : fa->abs_pc;
        } else {
            still_missing = 1;
        }
    }
    s->needs_resolve = (uint32_t)still_missing;
}

/* Always invoked WITHOUT g_spin held, enforcing g_refresh_spin -> _dl_load_lock, then release _dl_load_lock -> g_spin */
static void refresh_mods_snapshot(void) {
    refresh_lock();
    g_snap_num_mods = 0;
    snap_ctx_t sc = { g_snap_mods, &g_snap_num_mods };
    dl_iterate_phdr(phdr_cb, &sc);

    spin_lock();
    if (g_snap_num_mods > 0) {
        memcpy(g_mods, g_snap_mods, (size_t)g_snap_num_mods * sizeof(mod_info_t));
    }
    g_num_mods = g_snap_num_mods;
    for (int i = 0; i < g_num_mods; i++) {
        int found = 0;
        for (int s = 0; s < g_num_seen_mods; s++) {
            if (strcmp(g_seen_mods[s].base, g_mods[i].base) == 0 &&
                strcmp(g_seen_mods[s].build_id, g_mods[i].build_id) == 0) {
                found = 1;
                break;
            }
        }
        if (!found && g_num_seen_mods < MAX_MODS) {
            g_seen_mods[g_num_seen_mods++] = g_mods[i];
        }
    }
    if (g_slots) {
        for (uint32_t i = 0; i < ALLOC_SLOTS; i++) {
            if (g_slots[i].state == SLOT_LIVE && g_slots[i].needs_resolve) {
                resolve_slot_frames_locked(&g_slots[i]);
            }
        }
    }
    spin_unlock();
    refresh_unlock();
}

static int ptr_reachable_in_mods_locked(uintptr_t target) {
    for (int i = 0; i < g_num_mods; i++) {
        const mod_info_t *m = &g_mods[i];
        if (!strstr(m->base, g_target_dso) && !strstr(m->base, "vulkan") && strcmp(m->base, "(main)") != 0) {
            continue;
        }
        for (int s = 0; s < m->n_segs; s++) {
            if (!(m->segs[s].flags & PF_W)) continue;
            uintptr_t cur = (m->segs[s].start + sizeof(uintptr_t) - 1u) & ~(uintptr_t)(sizeof(uintptr_t) - 1u);
            uintptr_t end = m->segs[s].end & ~(uintptr_t)(sizeof(uintptr_t) - 1u);
            for (; cur + sizeof(uintptr_t) <= end; cur += sizeof(uintptr_t)) {
                uintptr_t val = *(const uintptr_t *)cur;
                if (val == target) return 1;
            }
        }
    }
    return 0;
}

static int slot_matches_domain(const alloc_slot_t *s, int kind) {
    /* kind: 0 = target_dso, 1 = zcc_owned, 2 = vulkan_api */
    const char *m0 = (s->n_frames > 0) ? s->frames[0].mod_base : "unknown";
    int m0_wrap = (strstr(m0, "libc.so") != NULL || strstr(m0, "libstdc++.so") != NULL);
    int max_fi = m0_wrap ? s->n_frames : 1;
    for (int fi = 0; fi < max_fi && fi < s->n_frames; fi++) {
        const char *mf = s->frames[fi].mod_base;
        if (kind == 0) {
            if (strstr(mf, g_target_dso) != NULL) return 1;
        } else if (kind == 1) {
            if (strcmp(mf, "(main)") == 0 || strstr(mf, "zcc") != NULL || strstr(mf, "test_track21b") != NULL) {
                return 1;
            }
        } else {
            if (strstr(mf, "libvulkan.so") != NULL || strstr(mf, "libVkLayer_") != NULL) return 1;
        }
    }
    return 0;
}

static int is_system_runtime_mod(const char *m) {
    static const char *const sys_prefixes[] = {
        "ld-linux", "libc.so", "libm.so", "libpthread.so", "libdl.so", "librt.so",
        "libstdc++.so", "libgcc_s.so", "libLLVM", "libdrm", "libz.so", "libzstd.so",
        "libexpat.so", "libelf.so", "libedit.so", "libtinfo.so", "libffi.so",
        "libxml2.so", "libicu", "libbsd.so", "libmd.so", "libasan.so", "libubsan.so"
    };
    if (!m || !*m) return 0;
    for (size_t i = 0; i < sizeof(sys_prefixes) / sizeof(sys_prefixes[0]); i++) {
        if (strstr(m, sys_prefixes[i]) != NULL) return 1;
    }
    return 0;
}

static int count_mapped_rx_proc(const char *substr) {
    if (!substr || !*substr) return 0;
    FILE *f = fopen("/proc/self/maps", "r");
    if (!f) return -1;
    char line[512];
    int c = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, substr) && strstr(line, "r-xp")) c++;
    }
    fclose(f);
    return c;
}

static inline uint32_t hash_ptr(uintptr_t p) {
    uint64_t x = (uint64_t)p;
    x ^= x >> 33;
    x *= 0xff51afd7ed558ccdULL;
    x ^= x >> 33;
    x *= 0xc4ceb9fe1a85ec53ULL;
    x ^= x >> 33;
    return (uint32_t)(x & (ALLOC_SLOTS - 1u));
}

static int find_slot_locked(uintptr_t ptr, uint32_t *out_idx) {
    uint32_t mask = ALLOC_SLOTS - 1u;
    uint32_t idx = hash_ptr(ptr);
    for (uint32_t step = 0; step < ALLOC_SLOTS; step++) {
        uint32_t cur = (idx + step) & mask;
        if (g_slots[cur].state == SLOT_EMPTY) {
            *out_idx = cur;
            return 0;
        }
        if (g_slots[cur].ptr == ptr) {
            *out_idx = cur;
            return 1;
        }
    }
    *out_idx = UINT32_MAX;
    return 0;
}

static void delete_slot_locked(uint32_t hole) {
    uint32_t mask = ALLOC_SLOTS - 1u;
    uint32_t i = hole;
    uint32_t j = (i + 1u) & mask;
    while (g_slots[j].state != SLOT_EMPTY) {
        uint32_t home = hash_ptr(g_slots[j].ptr);
        uint32_t dist_i = (i - home) & mask;
        uint32_t dist_j = (j - home) & mask;
        if (dist_i < dist_j) {
            g_slots[i] = g_slots[j];
            i = j;
        }
        j = (j + 1u) & mask;
        if (j == hole) break;
    }
    g_slots[i].state = SLOT_EMPTY;
    g_slots[i].ptr = 0;
}

static void mark_global_reachable_slots_locked(uintptr_t min_ptr, uintptr_t max_ptr) {
    if (min_ptr > max_ptr) return;
    for (int i = 0; i < g_num_mods; i++) {
        const mod_info_t *m = &g_mods[i];
        if (strstr(m->base, "libauditor") != NULL) continue;
        for (int s = 0; s < m->n_segs; s++) {
            if (!(m->segs[s].flags & PF_W)) continue;
            uintptr_t cur = (m->segs[s].start + sizeof(uintptr_t) - 1u) & ~(uintptr_t)(sizeof(uintptr_t) - 1u);
            uintptr_t end = m->segs[s].end & ~(uintptr_t)(sizeof(uintptr_t) - 1u);
            for (; cur + sizeof(uintptr_t) <= end; cur += sizeof(uintptr_t)) {
                uintptr_t val = *(const uintptr_t *)cur;
                if (val < min_ptr || val > max_ptr) continue;
                uint32_t hit_idx = 0;
                if (find_slot_locked(val, &hit_idx)) {
                    g_slots[hit_idx].reachable_at_exit = 1;
                }
            }
        }
    }
}

static void capture_frames_into_slot_locked(alloc_slot_t *s, void * const *pcs, int npc, int skip) {
    s->n_frames = 0;
    s->needs_resolve = 0;
    for (int i = skip; i < npc && s->n_frames < MAX_FRAMES; i++) {
        uintptr_t pc = (uintptr_t)pcs[i];
        if (!pc) continue;
        const mod_info_t *m = find_mod_for_pc_locked(pc);
        frame_attr_t *fa = &s->frames[s->n_frames++];
        fa->abs_pc = pc;
        if (m) {
            snprintf(fa->mod_base, sizeof(fa->mod_base), "%s", m->base);
            snprintf(fa->build_id, sizeof(fa->build_id), "%s", m->build_id);
            fa->rel_off = (pc >= m->base_addr) ? (pc - m->base_addr) : pc;
        } else {
            snprintf(fa->mod_base, sizeof(fa->mod_base), "unknown");
            snprintf(fa->build_id, sizeof(fa->build_id), "none");
            fa->rel_off = pc;
            s->needs_resolve = 1;
        }
    }
}

static void record_alloc(void *ptr, size_t sz, void * const *pcs, int npc, int skip) {
    if (!ptr || !g_slots || !g_initialized || g_finalized) return;
    spin_lock();
    uint32_t idx = 0;
    int existed = find_slot_locked((uintptr_t)ptr, &idx);
    if (existed) {
        /* Address reuse where old pointer had already been recycled by allocator race or missed free */
        g_stats.address_reuses++;
        if (g_stats.live_bytes >= g_slots[idx].size) g_stats.live_bytes -= g_slots[idx].size;
        if (g_stats.live_count > 0) g_stats.live_count--;
    } else if (idx == UINT32_MAX) {
        spin_unlock();
        return;
    }
    alloc_slot_t *s = &g_slots[idx];
    s->ptr = (uintptr_t)ptr;
    s->size = sz;
    s->seq = g_seq++;
    s->state = SLOT_LIVE;
    s->orphaned_on_dlclose = 0;
    s->reachable_before_dlclose = 0;
    s->reachable_at_exit = 0;
    capture_frames_into_slot_locked(s, pcs, npc, skip);

    g_stats.total_allocs++;
    g_stats.live_count++;
    g_stats.live_bytes += sz;
    if (g_stats.live_bytes > g_stats.peak_live_bytes) {
        g_stats.peak_live_bytes = g_stats.live_bytes;
    }
    spin_unlock();
}

static void record_free(void *ptr) {
    if (!ptr || is_boot_ptr(ptr) || !g_slots || !g_initialized || g_finalized) return;
    spin_lock();
    uint32_t idx = 0;
    if (find_slot_locked((uintptr_t)ptr, &idx)) {
        g_stats.total_frees++;
        if (g_stats.live_count > 0) g_stats.live_count--;
        if (g_stats.live_bytes >= g_slots[idx].size) {
            g_stats.live_bytes -= g_slots[idx].size;
        } else {
            g_stats.live_bytes = 0;
        }
        delete_slot_locked(idx);
    }
    spin_unlock();
}

static void init_hooks(void) {
    if (__atomic_load_n(&g_initialized, __ATOMIC_ACQUIRE)) return;
    t_in_hook = 1;
    real_malloc = dlsym(RTLD_NEXT, "malloc");
    real_calloc = dlsym(RTLD_NEXT, "calloc");
    real_realloc = dlsym(RTLD_NEXT, "realloc");
    real_reallocarray = dlsym(RTLD_NEXT, "reallocarray");
    real_free = dlsym(RTLD_NEXT, "free");
    real_posix_memalign = dlsym(RTLD_NEXT, "posix_memalign");
    real_aligned_alloc = dlsym(RTLD_NEXT, "aligned_alloc");
    real_memalign = dlsym(RTLD_NEXT, "memalign");
    real_valloc = dlsym(RTLD_NEXT, "valloc");
    real_dlopen = dlsym(RTLD_NEXT, "dlopen");
    real_dlclose = dlsym(RTLD_NEXT, "dlclose");

    const char *tgt = getenv("ZCC_AUDIT_TARGET_DSO");
    if (tgt && *tgt) snprintf(g_target_dso, sizeof(g_target_dso), "%s", tgt);
    const char *bt_env = getenv("ZCC_AUDIT_BT");
    if (bt_env && strcmp(bt_env, "0") == 0) g_bt_enabled = 0;

    if (!g_slots) {
        void *m = mmap(NULL, (size_t)ALLOC_SLOTS * sizeof(alloc_slot_t),
                       PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (m != MAP_FAILED) g_slots = (alloc_slot_t *)m;
    }
    void *warm[4];
    (void)backtrace(warm, 4); /* pre-load libgcc_s before hooks go live */

    refresh_mods_snapshot();

    __atomic_store_n(&g_initialized, 1, __ATOMIC_RELEASE);
    t_in_hook = 0;
}

__attribute__((constructor))
static void auditor_ctor(void) {
    init_hooks();
}

static int gather_pcs(void **pcs, int cap, void *fallback_ret0) {
    if (g_bt_enabled) {
        int n = backtrace(pcs, cap);
        if (n > 0) return n;
    }
    pcs[0] = fallback_ret0;
    return 1;
}

void *malloc(size_t sz) {
    if (!g_initialized) {
        if (t_in_hook) return boot_alloc(16, sz);
        init_hooks();
    }
    if (t_in_hook) return real_malloc ? real_malloc(sz) : boot_alloc(16, sz);
    t_in_hook = 1;
    void *p = real_malloc(sz);
    if (!p) {
        __atomic_fetch_add(&g_stats.failed_allocs, 1, __ATOMIC_RELAXED);
    } else {
        void *pcs[MAX_FRAMES + 2];
        int skip = g_bt_enabled ? 1 : 0;
        int n = gather_pcs(pcs, MAX_FRAMES + 2, __builtin_return_address(0));
        record_alloc(p, sz, pcs, n, skip);
    }
    t_in_hook = 0;
    return p;
}

void *calloc(size_t n, size_t sz) {
    if (!g_initialized) {
        if (t_in_hook) {
            if (n != 0 && sz > SIZE_MAX / n) return NULL;
            return boot_alloc(16, n * sz);
        }
        init_hooks();
    }
    if (t_in_hook) return real_calloc ? real_calloc(n, sz) : boot_alloc(16, n * sz);
    t_in_hook = 1;
    void *p = real_calloc(n, sz);
    if (!p) {
        __atomic_fetch_add(&g_stats.failed_allocs, 1, __ATOMIC_RELAXED);
    } else {
        size_t total = (n != 0 && sz > SIZE_MAX / n) ? sz : (n * sz);
        void *pcs[MAX_FRAMES + 2];
        int skip = g_bt_enabled ? 1 : 0;
        int npc = gather_pcs(pcs, MAX_FRAMES + 2, __builtin_return_address(0));
        record_alloc(p, total, pcs, npc, skip);
    }
    t_in_hook = 0;
    return p;
}

void *realloc(void *ptr, size_t sz) {
    if (!g_initialized) {
        if (t_in_hook) return boot_alloc(16, sz);
        init_hooks();
    }
    if (t_in_hook) {
        if (is_boot_ptr(ptr)) return boot_alloc(16, sz);
        return real_realloc ? real_realloc(ptr, sz) : NULL;
    }
    t_in_hook = 1;
    if (is_boot_ptr(ptr)) {
        void *np = real_malloc(sz);
        if (np) {
            memcpy(np, ptr, sz); /* conservative upper copy capped by boot pool layout */
            void *pcs[MAX_FRAMES + 2];
            int skip = g_bt_enabled ? 1 : 0;
            int npc = gather_pcs(pcs, MAX_FRAMES + 2, __builtin_return_address(0));
            record_alloc(np, sz, pcs, npc, skip);
        } else {
            __atomic_fetch_add(&g_stats.failed_reallocs, 1, __ATOMIC_RELAXED);
        }
        t_in_hook = 0;
        return np;
    }
    void *np = real_realloc(ptr, sz);
    __atomic_fetch_add(&g_stats.total_reallocs, 1, __ATOMIC_RELAXED);
    if (!np) {
        if (sz == 0 && ptr != NULL) {
            record_free(ptr);
        } else {
            __atomic_fetch_add(&g_stats.failed_reallocs, 1, __ATOMIC_RELAXED);
        }
    } else {
        if (ptr) record_free(ptr);
        void *pcs[MAX_FRAMES + 2];
        int skip = g_bt_enabled ? 1 : 0;
        int npc = gather_pcs(pcs, MAX_FRAMES + 2, __builtin_return_address(0));
        record_alloc(np, sz, pcs, npc, skip);
    }
    t_in_hook = 0;
    return np;
}

void *reallocarray(void *ptr, size_t nmemb, size_t size) {
    if (nmemb != 0 && size > SIZE_MAX / nmemb) {
        __atomic_fetch_add(&g_stats.failed_reallocs, 1, __ATOMIC_RELAXED);
        errno = ENOMEM;
        return NULL;
    }
    if (!g_initialized) init_hooks();
    if (t_in_hook) return real_reallocarray ? real_reallocarray(ptr, nmemb, size) : NULL;
    t_in_hook = 1;
    void *np = real_reallocarray ? real_reallocarray(ptr, nmemb, size) : real_realloc(ptr, nmemb * size);
    __atomic_fetch_add(&g_stats.total_reallocs, 1, __ATOMIC_RELAXED);
    if (!np) {
        __atomic_fetch_add(&g_stats.failed_reallocs, 1, __ATOMIC_RELAXED);
    } else {
        if (ptr) record_free(ptr);
        void *pcs[MAX_FRAMES + 2];
        int skip = g_bt_enabled ? 1 : 0;
        int npc = gather_pcs(pcs, MAX_FRAMES + 2, __builtin_return_address(0));
        record_alloc(np, nmemb * size, pcs, npc, skip);
    }
    t_in_hook = 0;
    return np;
}

void free(void *ptr) {
    if (!ptr || is_boot_ptr(ptr)) return;
    if (!g_initialized) init_hooks();
    if (!t_in_hook) {
        t_in_hook = 1;
        record_free(ptr);
        t_in_hook = 0;
    }
    real_free(ptr);
}

int posix_memalign(void **memptr, size_t alignment, size_t size) {
    if (!g_initialized) init_hooks();
    if (t_in_hook) return real_posix_memalign(memptr, alignment, size);
    t_in_hook = 1;
    int rc = real_posix_memalign(memptr, alignment, size);
    if (rc != 0 || !*memptr) {
        __atomic_fetch_add(&g_stats.failed_allocs, 1, __ATOMIC_RELAXED);
    } else {
        void *pcs[MAX_FRAMES + 2];
        int skip = g_bt_enabled ? 1 : 0;
        int npc = gather_pcs(pcs, MAX_FRAMES + 2, __builtin_return_address(0));
        record_alloc(*memptr, size, pcs, npc, skip);
    }
    t_in_hook = 0;
    return rc;
}

void *aligned_alloc(size_t alignment, size_t size) {
    if (!g_initialized) init_hooks();
    if (t_in_hook) return real_aligned_alloc(alignment, size);
    t_in_hook = 1;
    void *p = real_aligned_alloc(alignment, size);
    if (!p) {
        __atomic_fetch_add(&g_stats.failed_allocs, 1, __ATOMIC_RELAXED);
    } else {
        void *pcs[MAX_FRAMES + 2];
        int skip = g_bt_enabled ? 1 : 0;
        int npc = gather_pcs(pcs, MAX_FRAMES + 2, __builtin_return_address(0));
        record_alloc(p, size, pcs, npc, skip);
    }
    t_in_hook = 0;
    return p;
}

void *memalign(size_t alignment, size_t size) {
    if (!g_initialized) init_hooks();
    if (t_in_hook) return real_memalign(alignment, size);
    t_in_hook = 1;
    void *p = real_memalign(alignment, size);
    if (!p) {
        __atomic_fetch_add(&g_stats.failed_allocs, 1, __ATOMIC_RELAXED);
    } else {
        void *pcs[MAX_FRAMES + 2];
        int skip = g_bt_enabled ? 1 : 0;
        int npc = gather_pcs(pcs, MAX_FRAMES + 2, __builtin_return_address(0));
        record_alloc(p, size, pcs, npc, skip);
    }
    t_in_hook = 0;
    return p;
}

void *valloc(size_t size) {
    if (!g_initialized) init_hooks();
    if (t_in_hook) return real_valloc(size);
    t_in_hook = 1;
    void *p = real_valloc(size);
    if (!p) {
        __atomic_fetch_add(&g_stats.failed_allocs, 1, __ATOMIC_RELAXED);
    } else {
        void *pcs[MAX_FRAMES + 2];
        int skip = g_bt_enabled ? 1 : 0;
        int npc = gather_pcs(pcs, MAX_FRAMES + 2, __builtin_return_address(0));
        record_alloc(p, size, pcs, npc, skip);
    }
    t_in_hook = 0;
    return p;
}

void *dlopen(const char *filename, int flags) {
    if (!g_initialized) init_hooks();
    void *h = real_dlopen(filename, flags);
    __atomic_fetch_add(&g_stats.dlopen_calls, 1, __ATOMIC_RELAXED);
    if (flags & RTLD_NODELETE) {
        __atomic_fetch_add(&g_stats.explicit_rtld_nodelete_calls, 1, __ATOMIC_RELAXED);
    }
    if (!t_in_hook) {
        t_in_hook = 1;
        refresh_mods_snapshot();
        t_in_hook = 0;
    }
    return h;
}

static int mod_still_present_locked(const char *mod_base) {
    for (int i = 0; i < g_num_mods; i++) {
        if (strcmp(g_mods[i].base, mod_base) == 0) return 1;
    }
    return 0;
}

int dlclose(void *handle) {
    if (!g_initialized) init_hooks();
    __atomic_fetch_add(&g_stats.dlclose_calls, 1, __ATOMIC_RELAXED);

    int prev_t = t_in_hook;
    int pre_tgt = 0;
    if (!prev_t && g_slots) {
        t_in_hook = 1;
        int proc_pre = (count_mapped_rx_proc(g_target_dso) > 0);
        refresh_mods_snapshot();
        spin_lock();
        pre_tgt = mod_still_present_locked(g_target_dso) || proc_pre;
        for (uint32_t i = 0; i < ALLOC_SLOTS; i++) {
            if (g_slots[i].state != SLOT_LIVE) continue;
            if (g_slots[i].n_frames > 0 &&
                (strstr(g_slots[i].frames[0].mod_base, g_target_dso) ||
                 strstr(g_slots[i].frames[0].mod_base, "libvulkan"))) {
                if (ptr_reachable_in_mods_locked(g_slots[i].ptr)) {
                    g_slots[i].reachable_before_dlclose = 1;
                }
            }
        }
        spin_unlock();
        t_in_hook = prev_t;
    }

    int rc = real_dlclose(handle);

    if (!prev_t && g_slots) {
        t_in_hook = 1;
        int post_tgt = count_mapped_rx_proc(g_target_dso);
        refresh_mods_snapshot();
        spin_lock();
        if (pre_tgt > 0 && post_tgt == 0) {
            g_stats.unmapped_lvp_events++;
        } else if (pre_tgt > 0 && post_tgt > 0) {
            g_stats.retained_lvp_events++;
        }
        /* Flag any still-live allocation whose primary allocating module just vanished */
        for (uint32_t i = 0; i < ALLOC_SLOTS; i++) {
            if (g_slots[i].state != SLOT_LIVE || g_slots[i].orphaned_on_dlclose) continue;
            if (g_slots[i].n_frames > 0) {
                const char *mb = g_slots[i].frames[0].mod_base;
                if (strcmp(mb, "(main)") != 0 && strcmp(mb, "unknown") != 0 &&
                    !mod_still_present_locked(mb)) {
                    g_slots[i].orphaned_on_dlclose = 1;
                    g_stats.orphaned_on_dlclose_count++;
                    g_stats.orphaned_on_dlclose_bytes += g_slots[i].size;
                }
            }
        }
        spin_unlock();
        t_in_hook = prev_t;
    }
    return rc;
}

static uint64_t hash_stack_desc(const alloc_slot_t *s) {
    uint64_t h = 14695981039346656037ULL;
    for (int i = 0; i < s->n_frames; i++) {
        const char *p = s->frames[i].mod_base;
        while (*p) { h ^= (uint8_t)*p++; h *= 1099511628211ULL; }
        uint64_t r = (uint64_t)s->frames[i].rel_off;
        for (int b = 0; b < 8; b++) { h ^= (r >> (b * 8)) & 0xff; h *= 1099511628211ULL; }
    }
    return h;
}

static void format_stack_desc(const alloc_slot_t *s, char *out, size_t sz) {
    size_t pos = 0;
    out[0] = '\0';
    for (int i = 0; i < s->n_frames; i++) {
        int n = snprintf(out + pos, (pos < sz) ? (sz - pos) : 0,
                         "%s%s+0x%lx",
                         (i == 0) ? "" : " <- ",
                         s->frames[i].mod_base,
                         (unsigned long)s->frames[i].rel_off);
        if (n < 0 || pos + (size_t)n >= sz) break;
        pos += (size_t)n;
    }
}

__attribute__((destructor(101)))
static void auditor_dtor(void) {
    if (!g_initialized || g_finalized || !g_slots) return;
    t_in_hook = 1;
    __atomic_store_n(&g_finalized, 1, __ATOMIC_RELEASE);
    refresh_mods_snapshot();

    leak_bucket_t *buckets = mmap(NULL, MAX_BUCKETS * sizeof(leak_bucket_t),
                                  PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    mod_live_t *mod_live = mmap(NULL, MAX_MODS * sizeof(mod_live_t),
                                PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    int n_buckets = 0;
    int n_mod_live = 0;
    int buckets_truncated = 0;
    uint64_t final_live_count = 0;
    uint64_t final_live_bytes = 0;
    uint64_t zcc_owned_live_count = 0;
    uint64_t zcc_owned_live_bytes = 0;
    uint64_t vk_api_live_count = 0;
    uint64_t vk_api_live_bytes = 0;
    uint64_t target_dso_live_count = 0;
    uint64_t target_dso_live_bytes = 0;
    uint64_t sys_live_count = 0;
    uint64_t sys_live_bytes = 0;
    uint64_t other_live_count = 0;
    uint64_t other_live_bytes = 0;
    uint64_t bucket_sum_count = 0;
    uint64_t bucket_sum_bytes = 0;
    uint64_t global_reach_count = 0;
    uint64_t global_reach_bytes = 0;
    uint64_t pinned_attr_count = 0;
    uint64_t pinned_attr_bytes = 0;
    uint64_t unpinned_non_orphan_count = 0;
    uint64_t unpinned_non_orphan_bytes = 0;

    spin_lock();
    uintptr_t min_live_ptr = UINTPTR_MAX;
    uintptr_t max_live_ptr = 0;
    for (uint32_t i = 0; i < ALLOC_SLOTS; i++) {
        if (g_slots[i].state != SLOT_LIVE) continue;
        if (g_slots[i].ptr < min_live_ptr) min_live_ptr = g_slots[i].ptr;
        if (g_slots[i].ptr > max_live_ptr) max_live_ptr = g_slots[i].ptr;
    }
    mark_global_reachable_slots_locked(min_live_ptr, max_live_ptr);

    for (uint32_t i = 0; i < ALLOC_SLOTS; i++) {
        if (g_slots[i].state != SLOT_LIVE) continue;
        final_live_count++;
        final_live_bytes += g_slots[i].size;
        if (g_slots[i].reachable_at_exit) {
            global_reach_count++;
            global_reach_bytes += g_slots[i].size;
        }
        const char *m0 = (g_slots[i].n_frames > 0) ? g_slots[i].frames[0].mod_base : "unknown";
        const char *b0 = (g_slots[i].n_frames > 0) ? g_slots[i].frames[0].build_id : "none";
        int pinned = mod_still_present_locked(m0);
        if (pinned) {
            pinned_attr_count++;
            pinned_attr_bytes += g_slots[i].size;
        } else if (!g_slots[i].orphaned_on_dlclose) {
            unpinned_non_orphan_count++;
            unpinned_non_orphan_bytes += g_slots[i].size;
        }

        int is_tgt = slot_matches_domain(&g_slots[i], 0);
        int is_zcc = (!is_tgt && slot_matches_domain(&g_slots[i], 1));
        int is_vk  = (!is_tgt && !is_zcc && slot_matches_domain(&g_slots[i], 2));
        if (is_tgt) {
            target_dso_live_count++;
            target_dso_live_bytes += g_slots[i].size;
        } else if (is_zcc) {
            zcc_owned_live_count++;
            zcc_owned_live_bytes += g_slots[i].size;
        } else if (is_vk) {
            vk_api_live_count++;
            vk_api_live_bytes += g_slots[i].size;
        } else if (is_system_runtime_mod(m0)) {
            sys_live_count++;
            sys_live_bytes += g_slots[i].size;
        } else {
            other_live_count++;
            other_live_bytes += g_slots[i].size;
        }

        if (mod_live != MAP_FAILED) {
            int mf = -1;
            for (int m = 0; m < n_mod_live; m++) {
                if (strcmp(mod_live[m].mod_base, m0) == 0 && strcmp(mod_live[m].build_id, b0) == 0) {
                    mf = m;
                    break;
                }
            }
            if (mf < 0 && n_mod_live < MAX_MODS) {
                mf = n_mod_live++;
                memset(&mod_live[mf], 0, sizeof(mod_live[mf]));
                snprintf(mod_live[mf].mod_base, sizeof(mod_live[mf].mod_base), "%s", m0);
                snprintf(mod_live[mf].build_id, sizeof(mod_live[mf].build_id), "%s", b0);
                mod_live[mf].pinned_mapped = pinned;
                mod_live[mf].is_system_runtime = is_system_runtime_mod(m0);
            }
            if (mf >= 0) {
                mod_live[mf].count++;
                mod_live[mf].bytes += g_slots[i].size;
                if (g_slots[i].orphaned_on_dlclose) mod_live[mf].orphaned_count++;
                if (g_slots[i].reachable_at_exit) {
                    mod_live[mf].global_reachable_count++;
                    mod_live[mf].global_reachable_bytes += g_slots[i].size;
                }
            }
        }

        if (buckets != MAP_FAILED) {
            uint64_t sk = hash_stack_desc(&g_slots[i]);
            int found = -1;
            for (int b = 0; b < n_buckets; b++) {
                if (buckets[b].stack_key == sk) { found = b; break; }
            }
            if (found < 0) {
                if (n_buckets < MAX_BUCKETS) {
                    found = n_buckets++;
                    memset(&buckets[found], 0, sizeof(buckets[found]));
                    buckets[found].stack_key = sk;
                    buckets[found].min_size = SIZE_MAX;
                    snprintf(buckets[found].mod_base, sizeof(buckets[found].mod_base), "%s", m0);
                    snprintf(buckets[found].build_id, sizeof(buckets[found].build_id), "%s", b0);
                    format_stack_desc(&g_slots[i], buckets[found].stack_desc, sizeof(buckets[found].stack_desc));
                } else {
                    buckets_truncated = 1;
                }
            }
            if (found >= 0) {
                buckets[found].count++;
                buckets[found].bytes += g_slots[i].size;
                bucket_sum_count++;
                bucket_sum_bytes += g_slots[i].size;
                if (g_slots[i].orphaned_on_dlclose) buckets[found].orphaned_count++;
                if (g_slots[i].reachable_before_dlclose) buckets[found].reachable_pre_dlclose++;
                if (g_slots[i].size < buckets[found].min_size) buckets[found].min_size = g_slots[i].size;
                if (g_slots[i].size > buckets[found].max_size) buckets[found].max_size = g_slots[i].size;
            }
        }
    }
    spin_unlock();
    int final_lvp_mapped = count_mapped_rx_proc(g_target_dso);

    uint64_t expected_live_count = (g_stats.total_allocs >= (g_stats.total_frees + g_stats.address_reuses))
        ? (g_stats.total_allocs - g_stats.total_frees - g_stats.address_reuses)
        : UINT64_MAX;
    int live_count_reconciled = (expected_live_count == final_live_count);
    uint64_t part_count = target_dso_live_count + zcc_owned_live_count + vk_api_live_count + sys_live_count + other_live_count;
    uint64_t part_bytes = target_dso_live_bytes + zcc_owned_live_bytes + vk_api_live_bytes + sys_live_bytes + other_live_bytes;
    int domain_partition_reconciled = (part_count == final_live_count && part_bytes == final_live_bytes);

    fprintf(stderr,
            "TRANSITIVE_ALLOC_AUDIT total_allocs=%llu total_frees=%llu reallocs=%llu failed_allocs=%llu failed_reallocs=%llu reuses=%llu live_count=%llu live_bytes=%llu orphaned_count=%llu orphaned_bytes=%llu zcc_live=%llu target_dso_live=%llu vk_api_live=%llu sys_live=%llu other_live=%llu global_reach_bytes=%llu reconciled=%d unmapped_ev=%llu retained_ev=%llu final_target_mapped=%d\n",
            (unsigned long long)g_stats.total_allocs,
            (unsigned long long)g_stats.total_frees,
            (unsigned long long)g_stats.total_reallocs,
            (unsigned long long)g_stats.failed_allocs,
            (unsigned long long)g_stats.failed_reallocs,
            (unsigned long long)g_stats.address_reuses,
            (unsigned long long)final_live_count,
            (unsigned long long)final_live_bytes,
            (unsigned long long)g_stats.orphaned_on_dlclose_count,
            (unsigned long long)g_stats.orphaned_on_dlclose_bytes,
            (unsigned long long)zcc_owned_live_bytes,
            (unsigned long long)target_dso_live_bytes,
            (unsigned long long)vk_api_live_bytes,
            (unsigned long long)sys_live_bytes,
            (unsigned long long)other_live_bytes,
            (unsigned long long)global_reach_bytes,
            (live_count_reconciled && domain_partition_reconciled) ? 1 : 0,
            (unsigned long long)g_stats.unmapped_lvp_events,
            (unsigned long long)g_stats.retained_lvp_events,
            final_lvp_mapped);

    const char *json_path = getenv("ZCC_AUDIT_JSON_OUT");
    if (json_path && *json_path) {
        FILE *jf = fopen(json_path, "w");
        if (jf) {
            fprintf(jf, "{\n");
            fprintf(jf, "  \"schema_version\": \"zcc.transitive_alloc_audit.v1\",\n");
            fprintf(jf, "  \"target_dso\": \"%s\",\n", g_target_dso);
            fprintf(jf, "  \"total_allocs\": %llu,\n", (unsigned long long)g_stats.total_allocs);
            fprintf(jf, "  \"total_frees\": %llu,\n", (unsigned long long)g_stats.total_frees);
            fprintf(jf, "  \"total_reallocs\": %llu,\n", (unsigned long long)g_stats.total_reallocs);
            fprintf(jf, "  \"failed_allocs\": %llu,\n", (unsigned long long)g_stats.failed_allocs);
            fprintf(jf, "  \"failed_reallocs\": %llu,\n", (unsigned long long)g_stats.failed_reallocs);
            fprintf(jf, "  \"address_reuses\": %llu,\n", (unsigned long long)g_stats.address_reuses);
            fprintf(jf, "  \"peak_live_bytes\": %llu,\n", (unsigned long long)g_stats.peak_live_bytes);
            fprintf(jf, "  \"dlopen_calls\": %llu,\n", (unsigned long long)g_stats.dlopen_calls);
            fprintf(jf, "  \"dlclose_calls\": %llu,\n", (unsigned long long)g_stats.dlclose_calls);
            fprintf(jf, "  \"explicit_rtld_nodelete_calls\": %llu,\n", (unsigned long long)g_stats.explicit_rtld_nodelete_calls);
            fprintf(jf, "  \"live_count\": %llu,\n", (unsigned long long)final_live_count);
            fprintf(jf, "  \"live_bytes\": %llu,\n", (unsigned long long)final_live_bytes);
            fprintf(jf, "  \"zcc_owned_live_count\": %llu,\n", (unsigned long long)zcc_owned_live_count);
            fprintf(jf, "  \"zcc_owned_live_bytes\": %llu,\n", (unsigned long long)zcc_owned_live_bytes);
            fprintf(jf, "  \"vulkan_api_live_count\": %llu,\n", (unsigned long long)vk_api_live_count);
            fprintf(jf, "  \"vulkan_api_live_bytes\": %llu,\n", (unsigned long long)vk_api_live_bytes);
            fprintf(jf, "  \"target_dso_live_count\": %llu,\n", (unsigned long long)target_dso_live_count);
            fprintf(jf, "  \"target_dso_live_bytes\": %llu,\n", (unsigned long long)target_dso_live_bytes);
            fprintf(jf, "  \"orphaned_on_dlclose_count\": %llu,\n", (unsigned long long)g_stats.orphaned_on_dlclose_count);
            fprintf(jf, "  \"orphaned_on_dlclose_bytes\": %llu,\n", (unsigned long long)g_stats.orphaned_on_dlclose_bytes);
            fprintf(jf, "  \"unmapped_target_events\": %llu,\n", (unsigned long long)g_stats.unmapped_lvp_events);
            fprintf(jf, "  \"retained_target_events\": %llu,\n", (unsigned long long)g_stats.retained_lvp_events);
            fprintf(jf, "  \"final_target_mapped\": %d,\n", final_lvp_mapped);
            fprintf(jf, "  \"reconciliation\": {\n");
            fprintf(jf, "    \"allocs_minus_frees_minus_reuses\": %llu,\n", (unsigned long long)expected_live_count);
            fprintf(jf, "    \"live_count\": %llu,\n", (unsigned long long)final_live_count);
            fprintf(jf, "    \"live_count_reconciled\": %s,\n", live_count_reconciled ? "true" : "false");
            fprintf(jf, "    \"bucket_sum_count\": %llu,\n", (unsigned long long)bucket_sum_count);
            fprintf(jf, "    \"bucket_sum_bytes\": %llu,\n", (unsigned long long)bucket_sum_bytes);
            fprintf(jf, "    \"buckets_truncated\": %s,\n", buckets_truncated ? "true" : "false");
            fprintf(jf, "    \"global_data_bss_reachable_count\": %llu,\n", (unsigned long long)global_reach_count);
            fprintf(jf, "    \"global_data_bss_reachable_bytes\": %llu,\n", (unsigned long long)global_reach_bytes);
            fprintf(jf, "    \"pinned_module_attributed_count\": %llu,\n", (unsigned long long)pinned_attr_count);
            fprintf(jf, "    \"pinned_module_attributed_bytes\": %llu,\n", (unsigned long long)pinned_attr_bytes);
            fprintf(jf, "    \"unpinned_non_orphan_count\": %llu,\n", (unsigned long long)unpinned_non_orphan_count);
            fprintf(jf, "    \"unpinned_non_orphan_bytes\": %llu,\n", (unsigned long long)unpinned_non_orphan_bytes);
            fprintf(jf, "    \"domain_partition_reconciled\": %s,\n", domain_partition_reconciled ? "true" : "false");
            fprintf(jf, "    \"domains\": {\n");
            fprintf(jf, "      \"zcc_owned\": {\"count\": %llu, \"bytes\": %llu},\n", (unsigned long long)zcc_owned_live_count, (unsigned long long)zcc_owned_live_bytes);
            fprintf(jf, "      \"vulkan_api\": {\"count\": %llu, \"bytes\": %llu},\n", (unsigned long long)vk_api_live_count, (unsigned long long)vk_api_live_bytes);
            fprintf(jf, "      \"target_dso\": {\"count\": %llu, \"bytes\": %llu},\n", (unsigned long long)target_dso_live_count, (unsigned long long)target_dso_live_bytes);
            fprintf(jf, "      \"system_runtime\": {\"count\": %llu, \"bytes\": %llu},\n", (unsigned long long)sys_live_count, (unsigned long long)sys_live_bytes);
            fprintf(jf, "      \"other\": {\"count\": %llu, \"bytes\": %llu}\n", (unsigned long long)other_live_count, (unsigned long long)other_live_bytes);
            fprintf(jf, "    },\n");
            fprintf(jf, "    \"live_by_module\": [\n");
            for (int m = 0; m < n_mod_live; m++) {
                fprintf(jf,
                        "      {\"module\": \"%s\", \"build_id\": \"%s\", \"pinned_mapped\": %s, \"is_system_runtime\": %s, \"count\": %llu, \"bytes\": %llu, \"orphaned_count\": %llu, \"global_reachable_count\": %llu, \"global_reachable_bytes\": %llu}%s\n",
                        mod_live[m].mod_base,
                        mod_live[m].build_id,
                        mod_live[m].pinned_mapped ? "true" : "false",
                        mod_live[m].is_system_runtime ? "true" : "false",
                        (unsigned long long)mod_live[m].count,
                        (unsigned long long)mod_live[m].bytes,
                        (unsigned long long)mod_live[m].orphaned_count,
                        (unsigned long long)mod_live[m].global_reachable_count,
                        (unsigned long long)mod_live[m].global_reachable_bytes,
                        (m + 1 < n_mod_live) ? "," : "");
            }
            fprintf(jf, "    ]\n");
            fprintf(jf, "  },\n");
            fprintf(jf, "  \"dso_build_ids\": [\n");
            for (int m = 0; m < g_num_seen_mods; m++) {
                fprintf(jf, "    {\"module\": \"%s\", \"build_id\": \"%s\"}%s\n",
                        g_seen_mods[m].base, g_seen_mods[m].build_id,
                        (m + 1 < g_num_seen_mods) ? "," : "");
            }
            fprintf(jf, "  ],\n");
            fprintf(jf, "  \"buckets\": [\n");
            for (int b = 0; b < n_buckets; b++) {
                fprintf(jf,
                        "    {\"module\": \"%s\", \"build_id\": \"%s\", \"count\": %llu, \"bytes\": %llu, \"min_size\": %zu, \"max_size\": %zu, \"orphaned_count\": %llu, \"reachable_pre_dlclose\": %llu, \"stack\": \"%s\"}%s\n",
                        buckets[b].mod_base,
                        buckets[b].build_id,
                        (unsigned long long)buckets[b].count,
                        (unsigned long long)buckets[b].bytes,
                        buckets[b].min_size,
                        buckets[b].max_size,
                        (unsigned long long)buckets[b].orphaned_count,
                        (unsigned long long)buckets[b].reachable_pre_dlclose,
                        buckets[b].stack_desc,
                        (b + 1 < n_buckets) ? "," : "");
            }
            fprintf(jf, "  ]\n");
            fprintf(jf, "}\n");
            fclose(jf);
        }
    }

    if (buckets != MAP_FAILED) {
        munmap(buckets, MAX_BUCKETS * sizeof(leak_bucket_t));
    }
    if (mod_live != MAP_FAILED) {
        munmap(mod_live, MAX_MODS * sizeof(mod_live_t));
    }

    const char *fail_strict = getenv("ZCC_AUDIT_FAIL_ON_ORPHAN_OR_ZCC");
    if (fail_strict && strcmp(fail_strict, "1") == 0 &&
        (g_stats.orphaned_on_dlclose_bytes > 0 || zcc_owned_live_bytes > 0 ||
         target_dso_live_bytes > 0 || vk_api_live_bytes > 0 ||
         other_live_bytes > 0 || !live_count_reconciled || !domain_partition_reconciled)) {
        _exit(42);
    }
    const char *fail_leak = getenv("ZCC_AUDIT_FAIL_ON_LEAK");
    if (fail_leak && strcmp(fail_leak, "1") == 0 && final_live_bytes > 0) {
        _exit(42);
    }
    t_in_hook = 0;
}
