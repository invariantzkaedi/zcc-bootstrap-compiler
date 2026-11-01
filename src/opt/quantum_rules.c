#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MAX_INSTRUCTIONS 10000
#define MAX_QUBITS 64
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

typedef enum {
    INST_H,
    INST_RZ,
    INST_CNOT,
    INST_SWAP
} InstType;

typedef struct {
    InstType type;
    int q0;
    int q1;
    double angle;
} Instruction;

static Instruction buffer[MAX_INSTRUCTIONS];
static int buffer_count = 0;

static int inst_uses_qubit(const Instruction *inst, int q) {
    if (inst->type == INST_H || inst->type == INST_RZ) {
        return inst->q0 == q;
    }
    return inst->q0 == q || inst->q1 == q;
}

static int inst_disjoint(const Instruction *a, const Instruction *b) {
    if (inst_uses_qubit(b, a->q0)) return 0;
    if ((a->type == INST_CNOT || a->type == INST_SWAP) && inst_uses_qubit(b, a->q1)) return 0;
    return 1;
}

static void remove_at(int idx) {
    for (int i = idx; i < buffer_count - 1; i++) {
        buffer[i] = buffer[i + 1];
    }
    buffer_count--;
}

static int try_reduce_last(void) {
    if (buffer_count < 2) return 0;
    int top_idx = buffer_count - 1;
    Instruction inst = buffer[top_idx];

    /* Scan backward across disjoint-support gates */
    int match_idx = -1;
    for (int k = top_idx - 1; k >= 0; k--) {
        if (!inst_disjoint(&inst, &buffer[k])) {
            match_idx = k;
            break;
        }
    }
    if (match_idx < 0) return 0;

    Instruction *prev = &buffer[match_idx];

    /* RULE 1: Hadamard Cancellation (H H -> I) */
    if (inst.type == INST_H && prev->type == INST_H && inst.q0 == prev->q0) {
#ifdef FAULT_INJECT
        return 0;
#else
        remove_at(top_idx);
        remove_at(match_idx);
        return 1;
#endif
    }

    /* RULE 2: Phase Rotation Merge (RZ(a) RZ(b) -> RZ(a+b)) */
    if (inst.type == INST_RZ && prev->type == INST_RZ && inst.q0 == prev->q0) {
#ifdef FAULT_INJECT
        prev->angle -= inst.angle;
#else
        prev->angle += inst.angle;
#endif
        remove_at(top_idx);
        double rem = fmod(fabs(prev->angle), 2.0 * M_PI);
        if (rem < 1e-9 || fabs(rem - 2.0 * M_PI) < 1e-9) {
            remove_at(match_idx);
        }
        return 1;
    }

    /* RULE 3: CNOT Cancellation (CNOT CNOT -> I) */
    if (inst.type == INST_CNOT && prev->type == INST_CNOT &&
        inst.q0 == prev->q0 && inst.q1 == prev->q1) {
        remove_at(top_idx);
        remove_at(match_idx);
        return 1;
    }

    /* RULE 4: SWAP Cancellation (SWAP SWAP -> I) */
    if (inst.type == INST_SWAP && prev->type == INST_SWAP &&
        ((inst.q0 == prev->q0 && inst.q1 == prev->q1) ||
         (inst.q0 == prev->q1 && inst.q1 == prev->q0))) {
        remove_at(top_idx);
        remove_at(match_idx);
        return 1;
    }

    return 0;
}

void push_instruction(Instruction inst) {
    if (buffer_count >= MAX_INSTRUCTIONS) {
        fprintf(stderr, "FATAL: Quantum instruction buffer overflow (MAX_INSTRUCTIONS=%d)\n", MAX_INSTRUCTIONS);
        exit(2);
    }
    buffer[buffer_count++] = inst;
    while (try_reduce_last()) {
        /* Fixpoint cascade across exposed adjacencies */
    }
}

int main(void) {
    char line[256];
    while (fgets(line, sizeof(line), stdin)) {
        char type_str[32];
        if (sscanf(line, "%31s", type_str) != 1) continue;
        if (type_str[0] == '#') continue;

        Instruction inst = {0};
        char extra[16];

        if (strcmp(type_str, "H") == 0) {
            inst.type = INST_H;
            if (sscanf(line, "%*s %d %15s", &inst.q0, extra) != 1 ||
                inst.q0 < 0 || inst.q0 >= MAX_QUBITS) {
                fprintf(stderr, "ERROR: Invalid H instruction: %s", line);
                return 2;
            }
            push_instruction(inst);
        } else if (strcmp(type_str, "RZ") == 0) {
            inst.type = INST_RZ;
            if (sscanf(line, "%*s %d %lf %15s", &inst.q0, &inst.angle, extra) != 2 ||
                inst.q0 < 0 || inst.q0 >= MAX_QUBITS || isnan(inst.angle) || isinf(inst.angle)) {
                fprintf(stderr, "ERROR: Invalid RZ instruction: %s", line);
                return 2;
            }
            push_instruction(inst);
        } else if (strcmp(type_str, "CNOT") == 0) {
            inst.type = INST_CNOT;
            if (sscanf(line, "%*s %d %d %15s", &inst.q0, &inst.q1, extra) != 2 ||
                inst.q0 < 0 || inst.q0 >= MAX_QUBITS ||
                inst.q1 < 0 || inst.q1 >= MAX_QUBITS ||
                inst.q0 == inst.q1) {
                fprintf(stderr, "ERROR: Invalid CNOT instruction: %s", line);
                return 2;
            }
            push_instruction(inst);
        } else if (strcmp(type_str, "SWAP") == 0) {
            inst.type = INST_SWAP;
            if (sscanf(line, "%*s %d %d %15s", &inst.q0, &inst.q1, extra) != 2 ||
                inst.q0 < 0 || inst.q0 >= MAX_QUBITS ||
                inst.q1 < 0 || inst.q1 >= MAX_QUBITS ||
                inst.q0 == inst.q1) {
                fprintf(stderr, "ERROR: Invalid SWAP instruction: %s", line);
                return 2;
            }
            push_instruction(inst);
        } else {
            fprintf(stderr, "ERROR: Unsupported gate '%s' (refused fail-closed)\n", type_str);
            return 3;
        }
    }

    for (int i = 0; i < buffer_count; i++) {
        Instruction inst = buffer[i];
        if (inst.type == INST_H) {
            printf("H %d\n", inst.q0);
        } else if (inst.type == INST_RZ) {
            printf("RZ %d %f\n", inst.q0, inst.angle);
        } else if (inst.type == INST_CNOT) {
            printf("CNOT %d %d\n", inst.q0, inst.q1);
        } else if (inst.type == INST_SWAP) {
            printf("SWAP %d %d\n", inst.q0, inst.q1);
        }
    }

    return 0;
}
