#ifndef ZCC_PRIME_SERIALIZATION_H
#define ZCC_PRIME_SERIALIZATION_H

#include <stdint.h>
#include <stddef.h>

#pragma pack(push, 1)

/* Native C equivalents of the Protobuf PrimeVector message structure */
typedef struct {
    double h;
    double h0;
    double eta;
    double gamma;
    double epsilon;
    double beta;
    uint64_t seed;
    uint64_t timestamp;
    double history[128];
    size_t history_count;
    double agent_a_score;
    double agent_b_score;
    double agent_c_score;
    char context[128];
} zcc_prime_vector_t;

/* Native C equivalents of the Protobuf PrimeConsensus message structure */
typedef struct {
    zcc_prime_vector_t state;
    double consensus_score;
    double drift;
    uint32_t jackpot;
    char alerts[512]; /* comma-separated warning alerts */
} zcc_prime_consensus_t;

#pragma pack(pop)

/* Binary packing/unpacking signatures (Nanopb-like fixed format) */
int zcc_prime_pack_binary(const zcc_prime_consensus_t *consensus, uint8_t *buf, size_t max_len, size_t *out_len);
int zcc_prime_unpack_binary(const uint8_t *buf, size_t len, zcc_prime_consensus_t *consensus);

/* JSON representation functions for ledgers and ZXR telemetry integration */
int zcc_prime_serialize_json(const zcc_prime_consensus_t *consensus, char *buf, size_t max_len);
int zcc_prime_deserialize_json(const char *json_str, zcc_prime_consensus_t *consensus);

#endif /* ZCC_PRIME_SERIALIZATION_H */
