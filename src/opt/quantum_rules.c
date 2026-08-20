#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_INSTRUCTIONS 10000

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

Instruction buffer[MAX_INSTRUCTIONS];
int buffer_count = 0;

void push_instruction(Instruction inst) {
    if (buffer_count > 0) {
        Instruction *last = &buffer[buffer_count - 1];
        
        // RULE 1: Hadamard Cancellation (H H -> I)
        if (inst.type == INST_H && last->type == INST_H && inst.q0 == last->q0) {
#ifdef FAULT_INJECT
            // Fault: do not cancel, just push to simulate a broken optimizer
            if (buffer_count >= MAX_INSTRUCTIONS) {
                fprintf(stderr, "FATAL: Instruction buffer overflow during FAULT_INJECT\n");
                exit(1);
            }
            buffer[buffer_count++] = inst;
            return;
#endif
            buffer_count--; // Cancel both instructions
            return;
        }
        
        // RULE 2: Phase Rotation Merge (RZ(a) RZ(b) -> RZ(a+b))
        if (inst.type == INST_RZ && last->type == INST_RZ && inst.q0 == last->q0) {
#ifdef FAULT_INJECT
            last->angle -= inst.angle; // Fault: subtract instead of add
#else
            last->angle += inst.angle;
#endif
            // Optional: If angle is exactly 0 (modulo 2PI), we could cancel it.
            // For now, we leave it as RZ 0.0, which acts as Identity.
            return;
        }
        
        // RULE 3: CNOT Cancellation (CNOT CNOT -> I)
        if (inst.type == INST_CNOT && last->type == INST_CNOT && inst.q0 == last->q0 && inst.q1 == last->q1) {
            buffer_count--; // Cancel both instructions
            return;
        }
        
        // RULE 4: SWAP Cancellation (SWAP SWAP -> I)
        if (inst.type == INST_SWAP && last->type == INST_SWAP && inst.q0 == last->q0 && inst.q1 == last->q1) {
            buffer_count--; // Cancel both instructions
            return;
        }
    }
    
    // Push new instruction to buffer if no peephole rules apply
    if (buffer_count >= MAX_INSTRUCTIONS) {
        fprintf(stderr, "FATAL: Quantum instruction buffer overflow (MAX_INSTRUCTIONS=%d)\n", MAX_INSTRUCTIONS);
        exit(1);
    }
    buffer[buffer_count++] = inst;
}

int main() {
    char line[256];
    while (fgets(line, sizeof(line), stdin)) {
        char type_str[16];
        if (sscanf(line, "%15s", type_str) != 1) continue;
        
        Instruction inst = {0};
        
        if (strcmp(type_str, "H") == 0) {
            inst.type = INST_H;
            if (sscanf(line, "%*s %d", &inst.q0) == 1) {
                push_instruction(inst);
            }
        } else if (strcmp(type_str, "RZ") == 0) {
            inst.type = INST_RZ;
            if (sscanf(line, "%*s %d %lf", &inst.q0, &inst.angle) == 2) {
                push_instruction(inst);
            }
        } else if (strcmp(type_str, "CNOT") == 0) {
            inst.type = INST_CNOT;
            if (sscanf(line, "%*s %d %d", &inst.q0, &inst.q1) == 2) {
                push_instruction(inst);
            }
        } else if (strcmp(type_str, "SWAP") == 0) {
            inst.type = INST_SWAP;
            if (sscanf(line, "%*s %d %d", &inst.q0, &inst.q1) == 2) {
                push_instruction(inst);
            }
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
