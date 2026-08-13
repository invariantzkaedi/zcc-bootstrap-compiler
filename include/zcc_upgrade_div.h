#ifndef ZCC_UPGRADE_DIV_H
#define ZCC_UPGRADE_DIV_H

#include <stdint.h>

#ifndef INT64_MAX
#define INT64_MAX (9223372036854775807LL)
#endif
#ifndef INT64_MIN
#define INT64_MIN (-9223372036854775807LL - 1LL)
#endif
#ifndef INFINITY
#define INFINITY (1.0f/0.0f)
#endif
#ifndef NAN
#define NAN (0.0f/0.0f)
#endif
#ifndef isfinite
#define isfinite(x) ((x) == (x) && (x) != INFINITY && (x) != -INFINITY)
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define ZCC_UPGRADE_NONE              (0ULL)
#define ZCC_UPGRADE_INFINITY_CORE     (1ULL << 0)
#define ZCC_UPGRADE_NULL_ENERGY       (1ULL << 1)
#define ZCC_UPGRADE_TIER_ASCENSION    (1ULL << 2)
#define ZCC_UPGRADE_LUCK_SPIKE        (1ULL << 3)
#define ZCC_UPGRADE_SINGULARITY_BUFF  (1ULL << 4)
#define ZCC_UPGRADE_ADAPTIVE_SOLVER   (1ULL << 5)
#define ZCC_UPGRADE_ACHIEVEMENT_NULL  (1ULL << 6)
#define ZCC_UPGRADE_OVERCLOCK         (1ULL << 7)

int64_t zcc_upgrade_div_i64(int64_t num, int64_t den,
                            const char *entity_id, uint64_t *out_flags);

double  zcc_upgrade_div_f64(double num, double den,
                            const char *entity_id, uint64_t *out_flags);

uint64_t zcc_upgrade_get_flags(const char *entity_id);
void     zcc_upgrade_clear_flags(const char *entity_id);

// Auto-func macro wrappers for automatic entity tracking
#define ZCC_SAFE_DIV_I64(num, den, out_flags) \
    zcc_upgrade_div_i64((num), (den), __func__, (out_flags))

#define ZCC_SAFE_DIV_F64(num, den, out_flags) \
    zcc_upgrade_div_f64((num), (den), __func__, (out_flags))

#ifdef __cplusplus
}
#endif

#endif /* ZCC_UPGRADE_DIV_H */
