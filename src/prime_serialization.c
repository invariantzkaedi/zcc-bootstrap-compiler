#include "prime_serialization.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define ZCS_MAGIC "ZCS:"
#define ZCS_MAGIC_LEN 4

/* Binary serialization using fixed structural alignment */
int zcc_prime_pack_binary(const zcc_prime_consensus_t *consensus, uint8_t *buf, size_t max_len, size_t *out_len) {
    size_t required_len = ZCS_MAGIC_LEN + sizeof(zcc_prime_consensus_t);
    if (max_len < required_len) {
        return -1;
    }

    /* Copy magic header */
    memcpy(buf, ZCS_MAGIC, ZCS_MAGIC_LEN);
    
    /* Copy struct directly to buffer */
    memcpy(buf + ZCS_MAGIC_LEN, consensus, sizeof(zcc_prime_consensus_t));
    
    *out_len = required_len;
    return 0;
}

int zcc_prime_unpack_binary(const uint8_t *buf, size_t len, zcc_prime_consensus_t *consensus) {
    size_t required_len = ZCS_MAGIC_LEN + sizeof(zcc_prime_consensus_t);
    if (len < required_len) {
        return -1;
    }

    if (memcmp(buf, ZCS_MAGIC, ZCS_MAGIC_LEN) != 0) {
        return -2; /* magic mismatch */
    }

    memcpy(consensus, buf + ZCS_MAGIC_LEN, sizeof(zcc_prime_consensus_t));
    return 0;
}

/* Simple JSON serialization */
int zcc_prime_serialize_json(const zcc_prime_consensus_t *consensus, char *buf, size_t max_len) {
    char history_str[2048] = {0};
    size_t offset = 0;
    
    offset += snprintf(history_str + offset, sizeof(history_str) - offset, "[");
    for (size_t i = 0; i < consensus->state.history_count; i++) {
        offset += snprintf(history_str + offset, sizeof(history_str) - offset, "%.17g", consensus->state.history[i]);
        if (i < consensus->state.history_count - 1) {
            offset += snprintf(history_str + offset, sizeof(history_str) - offset, ", ");
        }
    }
    snprintf(history_str + offset, sizeof(history_str) - offset, "]");

    int written = snprintf(buf, max_len,
        "{\n"
        "  \"consensus_score\": %.17g,\n"
        "  \"drift\": %.17g,\n"
        "  \"jackpot\": %u,\n"
        "  \"alerts\": \"%s\",\n"
        "  \"state\": {\n"
        "    \"h\": %.17g,\n"
        "    \"h0\": %.17g,\n"
        "    \"eta\": %.17g,\n"
        "    \"gamma\": %.17g,\n"
        "    \"epsilon\": %.17g,\n"
        "    \"beta\": %.17g,\n"
        "    \"seed\": %lu,\n"
        "    \"timestamp\": %lu,\n"
        "    \"history\": %s,\n"
        "    \"agent_a_score\": %.17g,\n"
        "    \"agent_b_score\": %.17g,\n"
        "    \"agent_c_score\": %.17g,\n"
        "    \"context\": \"%s\"\n"
        "  }\n"
        "}",
        consensus->consensus_score,
        consensus->drift,
        consensus->jackpot,
        consensus->alerts,
        consensus->state.h,
        consensus->state.h0,
        consensus->state.eta,
        consensus->state.gamma,
        consensus->state.epsilon,
        consensus->state.beta,
        (unsigned long)consensus->state.seed,
        (unsigned long)consensus->state.timestamp,
        history_str,
        consensus->state.agent_a_score,
        consensus->state.agent_b_score,
        consensus->state.agent_c_score,
        consensus->state.context
    );

    if (written < 0 || (size_t)written >= max_len) {
        return -1;
    }
    return 0;
}

/* Helper to scan values from JSON without full parser dependency */
static const char* find_json_key(const char *json, const char *key) {
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char *p = strstr(json, pattern);
    if (!p) return NULL;
    p = strchr(p + strlen(pattern), ':');
    if (!p) return NULL;
    p++;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    return p;
}

int zcc_prime_deserialize_json(const char *json_str, zcc_prime_consensus_t *consensus) {
    memset(consensus, 0, sizeof(zcc_prime_consensus_t));
    
    const char *p;
    
    if ((p = find_json_key(json_str, "consensus_score"))) consensus->consensus_score = strtod(p, NULL);
    if ((p = find_json_key(json_str, "drift")))           consensus->drift = strtod(p, NULL);
    if ((p = find_json_key(json_str, "jackpot")))         consensus->jackpot = (uint32_t)strtoul(p, NULL, 10);
    
    if ((p = find_json_key(json_str, "alerts"))) {
        if (*p == '"') {
            p++;
            size_t i = 0;
            while (*p && *p != '"' && i < sizeof(consensus->alerts) - 1) {
                consensus->alerts[i++] = *p++;
            }
            consensus->alerts[i] = '\0';
        }
    }
    
    if ((p = find_json_key(json_str, "h")))         consensus->state.h = strtod(p, NULL);
    if ((p = find_json_key(json_str, "h0")))        consensus->state.h0 = strtod(p, NULL);
    if ((p = find_json_key(json_str, "eta")))       consensus->state.eta = strtod(p, NULL);
    if ((p = find_json_key(json_str, "gamma")))     consensus->state.gamma = strtod(p, NULL);
    if ((p = find_json_key(json_str, "epsilon")))   consensus->state.epsilon = strtod(p, NULL);
    if ((p = find_json_key(json_str, "beta")))      consensus->state.beta = strtod(p, NULL);
    if ((p = find_json_key(json_str, "seed")))      consensus->state.seed = (uint64_t)strtoull(p, NULL, 10);
    if ((p = find_json_key(json_str, "timestamp"))) consensus->state.timestamp = (uint64_t)strtoull(p, NULL, 10);
    
    if ((p = find_json_key(json_str, "agent_a_score"))) consensus->state.agent_a_score = strtod(p, NULL);
    if ((p = find_json_key(json_str, "agent_b_score"))) consensus->state.agent_b_score = strtod(p, NULL);
    if ((p = find_json_key(json_str, "agent_c_score"))) consensus->state.agent_c_score = strtod(p, NULL);
    
    if ((p = find_json_key(json_str, "context"))) {
        if (*p == '"') {
            p++;
            size_t i = 0;
            while (*p && *p != '"' && i < sizeof(consensus->state.context) - 1) {
                consensus->state.context[i++] = *p++;
            }
            consensus->state.context[i] = '\0';
        }
    }
    
    /* Parse history array */
    if ((p = find_json_key(json_str, "history"))) {
        if (*p == '[') {
            p++;
            consensus->state.history_count = 0;
            while (*p && *p != ']' && consensus->state.history_count < 128) {
                while (*p == ' ' || *p == ',' || *p == '\t' || *p == '\r' || *p == '\n') p++;
                if (*p == ']') break;
                char *end;
                double val = strtod(p, &end);
                if (p == end) break;
                consensus->state.history[consensus->state.history_count++] = val;
                p = end;
            }
        }
    }

    return 0;
}
