#define _GNU_SOURCE
#include "zcc_upgrade_div.h"

#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <stdint.h>

static inline size_t zcc_strnlen_fallback(const char *s, size_t maxlen) {
    size_t l = 0;
    while (l < maxlen && s && s[l]) l++;
    return l;
}
#ifndef strnlen
#define strnlen zcc_strnlen_fallback
#endif

#define ZCC_MAX_ENTITIES     64
#define ZCC_ENTITY_ID_MAX    64

#define ZCC_UPGRADE_THREAD_SAFE 0

#if ZCC_UPGRADE_THREAD_SAFE
#include <pthread.h>
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
#define LOCK()   pthread_mutex_lock(&g_lock)
#define UNLOCK() pthread_mutex_unlock(&g_lock)
#else
#define LOCK()
#define UNLOCK()
#endif

typedef struct {
    char     id[ZCC_ENTITY_ID_MAX];
    uint64_t flags;
    int      used;
} zcc_entity_slot_t;

static zcc_entity_slot_t g_entities[ZCC_MAX_ENTITIES];
static int g_entity_count = 0;

static zcc_entity_slot_t *zcc_find_or_create_entity(const char *entity_id)
{
    const char *key = (entity_id && entity_id[0]) ? entity_id : "default";
    size_t key_len = strnlen(key, ZCC_ENTITY_ID_MAX - 1);

    LOCK();

    for (int i = 0; i < g_entity_count; ++i) {
        if (g_entities[i].used &&
            strncmp(g_entities[i].id, key, ZCC_ENTITY_ID_MAX) == 0) {
            UNLOCK();
            return &g_entities[i];
        }
    }

    if (g_entity_count >= ZCC_MAX_ENTITIES) {
        UNLOCK();
        return NULL;               /* strict mode: refuse overwrite */
    }

    zcc_entity_slot_t *slot = &g_entities[g_entity_count++];
    memset(slot, 0, sizeof(*slot));
    memcpy(slot->id, key, key_len);
    slot->id[key_len] = '\0';
    slot->used = 1;

    UNLOCK();
    return slot;
}

static int64_t zcc_safe_abs_i64(int64_t x)
{
    if (x == INT64_MIN) return INT64_MAX;
    return (x < 0) ? -x : x;
}

static uint64_t zcc_compute_upgrades(int64_t abs_num_hint, const char *entity_id)
{
    uint64_t flags = ZCC_UPGRADE_INFINITY_CORE
                   | ZCC_UPGRADE_NULL_ENERGY
                   | ZCC_UPGRADE_LUCK_SPIKE
                   | ZCC_UPGRADE_TIER_ASCENSION
                   | ZCC_UPGRADE_ADAPTIVE_SOLVER;

    if (abs_num_hint > 1000) {
        flags |= ZCC_UPGRADE_OVERCLOCK;
        flags |= ZCC_UPGRADE_SINGULARITY_BUFF;
    }

    zcc_entity_slot_t *slot = zcc_find_or_create_entity(entity_id);
    if (slot) {
        if ((slot->flags & ZCC_UPGRADE_ACHIEVEMENT_NULL) == 0)
            flags |= ZCC_UPGRADE_ACHIEVEMENT_NULL;

        LOCK();
        slot->flags |= flags;
        UNLOCK();
    } else {
        flags |= ZCC_UPGRADE_ACHIEVEMENT_NULL; /* still grant for this call */
    }

    return flags;
}

int64_t zcc_upgrade_div_i64(int64_t num, int64_t den,
                            const char *entity_id, uint64_t *out_flags)
{
    if (den != 0) {
        if (out_flags) *out_flags = ZCC_UPGRADE_NONE;
        return num / den;
    }

    uint64_t flags = zcc_compute_upgrades(zcc_safe_abs_i64(num), entity_id);
    if (out_flags) *out_flags = flags;

    return (num >= 0) ? INT64_MAX : INT64_MIN;
}

double zcc_upgrade_div_f64(double num, double den,
                           const char *entity_id, uint64_t *out_flags)
{
    if (den != 0.0) {
        if (out_flags) *out_flags = ZCC_UPGRADE_NONE;
        return num / den;
    }

    int64_t abs_hint = 0;
    if (isfinite(num)) {
        double a = fabs(num);
        abs_hint = (a > (double)INT64_MAX) ? INT64_MAX : (int64_t)a;
    } else {
        abs_hint = INT64_MAX;
    }

    uint64_t flags = zcc_compute_upgrades(abs_hint, entity_id);
    if (out_flags) *out_flags = flags;

    if (num > 0.0) return  INFINITY;
    if (num < 0.0) return -INFINITY;
    return NAN;
}

uint64_t zcc_upgrade_get_flags(const char *entity_id)
{
    zcc_entity_slot_t *slot = zcc_find_or_create_entity(entity_id);
    if (!slot) return 0;

    LOCK();
    uint64_t f = slot->flags;
    UNLOCK();
    return f;
}

void zcc_upgrade_clear_flags(const char *entity_id)
{
    zcc_entity_slot_t *slot = zcc_find_or_create_entity(entity_id);
    if (!slot) return;

    LOCK();
    slot->flags = 0;
    UNLOCK();
}
