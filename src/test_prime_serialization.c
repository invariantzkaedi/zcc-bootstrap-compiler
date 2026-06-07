#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "prime_serialization.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <input_prime_state_c.pb>\n", argv[0]);
        return 1;
    }

    /* 1. Read binary file from disk */
    FILE *f = fopen(argv[1], "rb");
    if (!f) {
        perror("Error opening input file");
        return 1;
    }

    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    uint8_t *buffer = (uint8_t *)malloc(file_size);
    if (!buffer) {
        fprintf(stderr, "Out of memory allocating %ld bytes\n", file_size);
        fclose(f);
        return 1;
    }

    if (fread(buffer, 1, file_size, f) != (size_t)file_size) {
        perror("Error reading file");
        free(buffer);
        fclose(f);
        return 1;
    }
    fclose(f);

    printf("[C-TEST] Read binary payload size: %ld bytes\n", file_size);

    /* 2. Unpack binary struct */
    zcc_prime_consensus_t consensus;
    int unpack_res = zcc_prime_unpack_binary(buffer, file_size, &consensus);
    free(buffer);

    if (unpack_res != 0) {
        fprintf(stderr, "🔴 [C-TEST] Error unpacking binary consensus: %d\n", unpack_res);
        return 1;
    }

    printf("🟢 [C-TEST] Unpack Successful!\n");
    printf("           Consensus Score: %.6f\n", consensus.consensus_score);
    printf("           Consensus Drift: %.6f\n", consensus.drift);
    printf("           Jackpot Reward : %u\n", consensus.jackpot);
    printf("           Alerts         : %s\n", consensus.alerts);
    printf("           State H        : %.6f\n", consensus.state.h);
    printf("           State H0       : %.6f\n", consensus.state.h0);
    printf("           Context        : %s\n", consensus.state.context);
    printf("           History Count  : %lu\n", (unsigned long)consensus.state.history_count);

    /* 3. Serialize back to JSON and print */
    char json_buffer[4096];
    int serialize_res = zcc_prime_serialize_json(&consensus, json_buffer, sizeof(json_buffer));
    if (serialize_res != 0) {
        fprintf(stderr, "🔴 [C-TEST] Error serializing back to JSON\n");
        return 1;
    }

    printf("\n=== Re-serialized JSON output ===\n%s\n=================================\n", json_buffer);

    /* 4. Parse back from JSON and verify matching values */
    zcc_prime_consensus_t parsed;
    int deserialize_res = zcc_prime_deserialize_json(json_buffer, &parsed);
    if (deserialize_res != 0) {
        fprintf(stderr, "🔴 [C-TEST] Error deserializing from JSON\n");
        return 1;
    }

    int verification_failed = 0;
    
    #define CHECK_DOUBLE(field) \
        if (fabs(parsed.field - consensus.field) >= 1e-12) { \
            fprintf(stderr, "🔴 [C-TEST] Verification failed for " #field " (parsed: %.17g, original: %.17g)\n", parsed.field, consensus.field); \
            verification_failed = 1; \
        }
        
    #define CHECK_INT(field) \
        if (parsed.field != consensus.field) { \
            fprintf(stderr, "🔴 [C-TEST] Verification failed for " #field " (parsed: %lu, original: %lu)\n", (unsigned long)parsed.field, (unsigned long)consensus.field); \
            verification_failed = 1; \
        }

    #define CHECK_STR(field) \
        if (strcmp(parsed.field, consensus.field) != 0) { \
            fprintf(stderr, "🔴 [C-TEST] Verification failed for " #field " (parsed: '%s', original: '%s')\n", parsed.field, consensus.field); \
            verification_failed = 1; \
        }

    CHECK_DOUBLE(consensus_score);
    CHECK_DOUBLE(drift);
    CHECK_INT(jackpot);
    CHECK_STR(alerts);
    
    CHECK_DOUBLE(state.h);
    CHECK_DOUBLE(state.h0);
    CHECK_DOUBLE(state.eta);
    CHECK_DOUBLE(state.gamma);
    CHECK_DOUBLE(state.epsilon);
    CHECK_DOUBLE(state.beta);
    CHECK_INT(state.seed);
    CHECK_INT(state.timestamp);
    CHECK_STR(state.context);
    
    CHECK_INT(state.history_count);
    if (!verification_failed) {
        for (size_t i = 0; i < consensus.state.history_count; i++) {
            if (fabs(parsed.state.history[i] - consensus.state.history[i]) >= 1e-12) {
                fprintf(stderr, "🔴 [C-TEST] Verification failed for state.history[%lu] (parsed: %.17g, original: %.17g)\n",
                        (unsigned long)i, parsed.state.history[i], consensus.state.history[i]);
                verification_failed = 1;
                break;
            }
        }
    }
    
    CHECK_DOUBLE(state.agent_a_score);
    CHECK_DOUBLE(state.agent_b_score);
    CHECK_DOUBLE(state.agent_c_score);

    if (!verification_failed) {
        printf("🟢 [C-TEST] High-precision JSON Roundtrip verification: PASSED!\n");
    } else {
        fprintf(stderr, "🔴 [C-TEST] JSON Roundtrip verification: FAILED!\n");
        return 1;
    }

    return 0;
}
