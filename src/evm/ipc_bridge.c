/*
 * ipc_bridge.c — Neural Warden IPC Streaming Interface
 * ====================================================
 * Sub-millisecond latency named-pipe (FIFO) bridge for streaming
 * EVM decompiled IR to the PyTorch orchestrator.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "../../evm_lifter.h"
#include "../../ir.h"

#define WARDEN_TX_PIPE "/tmp/warden_tx.pipe"
#define WARDEN_RX_PIPE "/tmp/warden_rx.pipe"

int ipc_bridge_init(void) {
    /* Ensure pipes exist */
    mkfifo(WARDEN_TX_PIPE, 0666);
    mkfifo(WARDEN_RX_PIPE, 0666);
    return 0;
}

/* Streams a chunk of text (AST/IR/C pseudo-code) over the TX pipe */
int ipc_bridge_stream(const char *payload) {
    int fd = open(WARDEN_TX_PIPE, O_WRONLY);
    if (fd < 0) return -1;
    size_t len = strlen(payload);
    size_t written = 0;
    while (written < len) {
        ssize_t res = write(fd, payload + written, len - written);
        if (res < 0) { close(fd); return -1; }
        written += res;
    }
    write(fd, "\0", 1);
    close(fd);
    return 0;
}

FILE *ipc_bridge_open_tx(void) {
    return fopen(WARDEN_TX_PIPE, "w");
}

void ipc_bridge_close_tx(FILE *f) {
    if (f) {
        fputc('\0', f);
        fclose(f);
    }
}

/* Blocks and reads the synthesized .ir exploit payload from the RX pipe */
char *ipc_bridge_receive(void) {
    int fd = open(WARDEN_RX_PIPE, O_RDONLY);
    if (fd < 0) {
        return NULL;
    }
    
    size_t cap = 4096;
    size_t len = 0;
    char *buf = malloc(cap);
    if (!buf) {
        close(fd);
        return NULL;
    }
    
    while (1) {
        if (len + 1024 > cap) {
            cap *= 2;
            buf = realloc(buf, cap);
        }
        ssize_t res = read(fd, buf + len, cap - len - 1);
        if (res < 0) {
            free(buf);
            close(fd);
            return NULL;
        }
        if (res == 0) break;
        len += res;
    }
    buf[len] = '\0';
    close(fd);
    return buf;
}

static ir_op_t parse_op_str(const char *op) {
    if (strcmp(op, "IR_ADD") == 0) return IR_ADD;
    if (strcmp(op, "IR_SUB") == 0) return IR_SUB;
    if (strcmp(op, "IR_MUL") == 0) return IR_MUL;
    if (strcmp(op, "IR_CONST") == 0) return IR_CONST;
    if (strcmp(op, "IR_STORE") == 0) return IR_STORE;
    if (strcmp(op, "IR_LOAD") == 0) return IR_LOAD;
    if (strcmp(op, "IR_CALL") == 0) return IR_CALL;
    if (strcmp(op, "IR_RET") == 0) return IR_RET;
    return IR_NOP;
}

ir_func_t *ipc_bridge_parse_ir(char *text) {
    ir_func_t *fn = calloc(1, sizeof(ir_func_t));
    if (!fn) return NULL;
    strcpy(fn->name, "warden_exploit");
    
    char *saveptr;
    char *line = strtok_r(text, "\n", &saveptr);
    ir_node_t *tail = NULL;
    while (line) {
        char op_str[64] = {0}, dst[64] = {0}, src1[64] = {0}, src2[64] = {0};
        int tokens = sscanf(line, "%63s %63s %63s %63s", op_str, dst, src1, src2);
        if (tokens >= 1) {
            ir_op_t op = parse_op_str(op_str);
            if (op != IR_NOP || strcmp(op_str, "IR_FUNC") != 0) {
                ir_node_t *n = calloc(1, sizeof(ir_node_t));
                n->op = op;
                if (op == IR_CONST) {
                    strncpy(n->dst, dst, 63);
                    n->imm = strtoull(src1, NULL, 0);
                } else if (op == IR_RET || op == IR_LOAD) {
                    strncpy(n->dst, dst, 63);
                    strncpy(n->src1, src1, 63);
                } else {
                    strncpy(n->dst, dst, 63);
                    strncpy(n->src1, src1, 63);
                    strncpy(n->src2, src2, 63);
                }
                if (tail) tail->next = n;
                else fn->head = n;
                tail = n;
            }
        }
        line = strtok_r(NULL, "\n", &saveptr);
    }
    return fn;
}

void ipc_bridge_compile_and_strike(char *payload) {
    ir_func_t *fn = ipc_bridge_parse_ir(payload);
    if (!fn) return;
    
    FILE *out = fopen("warden_payload.yul", "w");
    if (out) {
        extern void evm_yul_weaver(ir_func_t *fn, FILE *out);
        evm_yul_weaver(fn, out);
        fclose(out);
        printf("[WARDEN] Payload compiled. Executing strike detonator...\n");
        system("python3 ../../zcc_swarm/hamiltonian-swarm/scripts/execute_strike.py --sign warden_payload.yul");
    }
}
