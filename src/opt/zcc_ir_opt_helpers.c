#include "zcc_ir_opt_helpers.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <stddef.h>
#include "zcc_opt_metrics.h"

__attribute__((weak)) int g_emit_smt_proofs = 0;
__attribute__((weak)) void smt_prove_ir_strength_reduction(const char *name, int op1, int op2, long long int val1, long long int val2, int width, size_t reg) {
    (void)name; (void)op1; (void)op2; (void)val1; (void)val2; (void)width; (void)reg;
}
__attribute__((weak)) void smt_prove_ir_peephole(const char *name, int op1, int op2, long long int val1, long long int val2, int check1, int check2, int width, size_t reg) {
    (void)name; (void)op1; (void)op2; (void)val1; (void)val2; (void)check1; (void)check2; (void)width; (void)reg;
}

int64_t zcc_now_us(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000000 + tv.tv_usec;
}

Instr* def_of(Function *fn, int reg) {
    if (reg <= 0 || reg >= MAX_INSTRS) return NULL;
    return fn->def_of[reg];
}

bool reg_is_const(Function *fn, int reg, int64_t *out) {
    Instr *d = def_of(fn, reg);
    if (d && d->op == OP_CONST) {
        *out = d->imm;
        return true;
    }
    return false;
}

int make_const(Function *fn, IRType ty, int64_t k, Instr *insert_before) {
    for (uint32_t bi = 0; bi < fn->n_blocks; bi++) {
        Block *temp = fn->blocks[bi];
        if (!temp) continue;
        for (Instr *it = temp->head; it; it = it->next) {
            if (it->op == OP_CONST && it->imm == k && it->ir_type == ty) {
                return it->dst;
            }
        }
    }

    Instr *inst = calloc(1, sizeof(Instr));
    inst->id = fn->n_regs + 1;
    inst->op = OP_CONST;
    inst->dst = ++fn->n_regs;
    inst->imm = k;
    inst->ir_type = ty;

    if (insert_before) {
        inst->prev = insert_before->prev;
        inst->next = insert_before;
        if (insert_before->prev) {
            insert_before->prev->next = inst;
        } else {
            for (uint32_t bi = 0; bi < fn->n_blocks; bi++) {
                Block *temp = fn->blocks[bi];
                if (temp && temp->head == insert_before) {
                    temp->head = inst;
                    break;
                }
            }
        }
        insert_before->prev = inst;

        for (uint32_t bi = 0; bi < fn->n_blocks; bi++) {
            Block *temp = fn->blocks[bi];
            if (temp) {
                for (Instr *curr = temp->head; curr; curr = curr->next) {
                    if (curr == inst) {
                        temp->n_instrs++;
                        break;
                    }
                }
            }
        }
    } else {
        Block *entry = fn->blocks[0];
        if (entry->head) {
            inst->next = entry->head;
            entry->head->prev = inst;
            entry->head = inst;
        } else {
            entry->head = inst;
            entry->tail = inst;
        }
        entry->n_instrs++;
    }
    fn->def_of[inst->dst] = inst;
    return inst->dst;
}

static bool is_reg_operand(Opcode op, int operand_index) {
    if (op == OP_BR) {
        return false;
    }
    if (op == OP_CONDBR) {
        return operand_index == 0;
    }
    return true;
}

bool replace_all_uses(Function *fn, int old_reg, int new_reg) {
    bool changed = false;
    for (uint32_t bi = 0; bi < fn->n_blocks; bi++) {
        Block *bb = fn->blocks[bi];
        if (!bb) continue;
        for (Instr *it = bb->head; it; it = it->next) {
            for (uint32_t i = 0; i < it->n_src; i++) {
                if (is_reg_operand(it->op, i) && it->src[i] == old_reg) {
                    it->src[i] = new_reg;
                    changed = true;
                }
            }
            if (it->op == OP_PHI) {
                for (uint32_t i = 0; i < it->n_phi; i++) {
                    if (it->phi[i].reg == old_reg) {
                        it->phi[i].reg = new_reg;
                        changed = true;
                    }
                }
            }
        }
    }
    return changed;
}

bool erase_instr(Function *fn, Instr *it) {
    Block *bb = NULL;
    for (uint32_t bi = 0; bi < fn->n_blocks; bi++) {
        Block *temp = fn->blocks[bi];
        if (!temp) continue;
        for (Instr *curr = temp->head; curr; curr = curr->next) {
            if (curr == it) {
                bb = temp;
                break;
            }
        }
        if (bb) break;
    }
    if (!bb) return false;

    if (it->prev) it->prev->next = it->next;
    else bb->head = it->next;

    if (it->next) it->next->prev = it->prev;
    else bb->tail = it->prev;

    bb->n_instrs--;
    if (it->dst != 0 && it->dst < MAX_INSTRS) {
        fn->def_of[it->dst] = NULL;
    }
    free(it);
    return true;
}

bool rewrite_to_copy(Function *fn, Instr *it, int src_reg) {
    (void)fn;
    it->op = OP_COPY;
    it->src[0] = src_reg;
    it->n_src = 1;
    return true;
}

bool rewrite_to_const(Function *fn, Instr *it, int64_t k) {
    (void)fn;
    it->op = OP_CONST;
    it->imm = k;
    it->n_src = 0;
    return true;
}

int count_uses(Function *fn, RegID reg) {
    if (reg == 0) return 0;
    int count = 0;
    for (uint32_t bi = 0; bi < fn->n_blocks; bi++) {
        Block *bb = fn->blocks[bi];
        if (!bb) continue;
        for (Instr *it = bb->head; it; it = it->next) {
            for (uint32_t i = 0; i < it->n_src; i++) {
                if (it->src[i] == reg) count++;
            }
            if (it->op == OP_PHI) {
                for (uint32_t i = 0; i < it->n_phi; i++) {
                    if (it->phi[i].reg == reg) count++;
                }
            }
        }
    }
    return count;
}

int resolve_copy(Function *fn, int reg) {
    int current = reg;
    for (int i = 0; i < 100; i++) {
        Instr *d = def_of(fn, current);
        if (d && d->op == OP_COPY) {
            current = d->src[0];
        } else {
            break;
        }
    }
    return current;
}

bool will_overflow_add(IRType ty, int64_t a, int64_t b) {
    if (ty == IR_TY_I32) {
        int64_t sum = a + b;
        return (sum < -2147483648LL || sum > 2147483647LL);
    } else {
        if (b > 0 && a > 9223372036854775807LL - b) return true;
        if (b < 0 && a < (-9223372036854775807LL - 1LL) - b) return true;
    }
    return false;
}

bool will_overflow_mul(IRType ty, int64_t a, int64_t b) {
    if (ty == IR_TY_I32) {
        int64_t prod = a * b;
        return (prod < -2147483648LL || prod > 2147483647LL);
    } else {
        if (a > 0 && b > 0 && a > 9223372036854775807LL / b) return true;
        if (a > 0 && b < 0 && b < (-9223372036854775807LL - 1LL) / a) return true;
        if (a < 0 && b > 0 && a < (-9223372036854775807LL - 1LL) / b) return true;
        if (a < 0 && b < 0 && b < 9223372036854775807LL / a) return true;
    }
    return false;
}

bool is_all_ones_for_type(IRType ty, int64_t k) {
    if (ty == IR_TY_I32 || ty == IR_TY_U32) {
        return k == -1 || (uint32_t)k == 0xFFFFFFFF;
    }
    return k == -1 || k == (int64_t)0xFFFFFFFFFFFFFFFFULL;
}

void rebuild_def_use(Function *fn) {
    memset(fn->def_of, 0, sizeof(fn->def_of));
    for (uint32_t bi = 0; bi < fn->n_blocks; bi++) {
        Block *bb = fn->blocks[bi];
        if (!bb) continue;
        for (Instr *it = bb->head; it; it = it->next) {
            if (it->dst != 0 && it->dst < MAX_INSTRS) {
                fn->def_of[it->dst] = it;
            }
        }
    }
}

int fn_count_instructions(const Function *fn) {
    int count = 0;
    for (uint32_t bi = 0; bi < fn->n_blocks; bi++) {
        Block *bb = fn->blocks[bi];
        if (!bb) continue;
        count += bb->n_instrs;
    }
    return count;
}

int fn_count_blocks(const Function *fn) {
    int count = 0;
    for (uint32_t bi = 0; bi < fn->n_blocks; bi++) {
        if (fn->blocks[bi]) count++;
    }
    return count;
}

void licm_build_def_block(Function *fn) {
    memset(fn->def_block, 0xFF, sizeof(fn->def_block));
    memset(fn->def_of, 0, sizeof(fn->def_of));
    for (uint32_t bi = 0; bi < fn->n_blocks; bi++) {
        Block *blk = fn->blocks[bi];
        if (!blk || !blk->reachable) continue;
        for (Instr *ins = blk->head; ins; ins = ins->next) {
            if (ins->dst && ins->dst < MAX_INSTRS) {
                fn->def_block[ins->dst] = bi;
                fn->def_of[ins->dst] = ins;
            }
        }
    }
}

void opt_metrics_init(OptMetricsSink *s) {
    s->rows = NULL;
    s->n_rows = 0;
    s->cap_rows = 0;
}

void opt_metrics_push(OptMetricsSink *s, OptPassMetricRow row) {
    if (s->n_rows >= s->cap_rows) {
        s->cap_rows = s->cap_rows == 0 ? 16 : s->cap_rows * 2;
        s->rows = realloc(s->rows, s->cap_rows * sizeof(OptPassMetricRow));
    }
    s->rows[s->n_rows++] = row;
}

void opt_metrics_dump_csv(const OptMetricsSink *s, const char *path) {
    FILE *f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "Failed to open metrics file %s\n", path);
        return;
    }
    fprintf(f, "pass,function,instructions_before,instructions_after,blocks_before,blocks_after,time_us,changed\n");
    for (int i = 0; i < s->n_rows; i++) {
        OptPassMetricRow r = s->rows[i];
        fprintf(f, "%s,%s,%d,%d,%d,%d,%lld,%d\n",
            r.pass_name ? r.pass_name : "",
            r.fn_name ? r.fn_name : "",
            r.instr_before,
            r.instr_after,
            r.blocks_before,
            r.blocks_after,
            (long long)r.pass_time_us,
            r.changed ? 1 : 0);
    }
    fclose(f);
}

int fn_max_register(const Function *fn) {
    int max_reg = 0;
    for (uint32_t bi = 0; bi < fn->n_blocks; bi++) {
        Block *bb = fn->blocks[bi];
        if (!bb) continue;
        for (Instr *ins = bb->head; ins; ins = ins->next) {
            if (ins->dst > max_reg && ins->dst < MAX_INSTRS) max_reg = ins->dst;
            for (int i = 0; i < 2; i++) {
                if (ins->src[i] > max_reg && ins->src[i] < MAX_INSTRS) max_reg = ins->src[i];
            }
        }
    }
    return max_reg;
}
