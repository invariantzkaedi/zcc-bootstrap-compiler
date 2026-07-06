#include <stdint.h>
#include "../prelude.h"
#include "zcc_ir_verify.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

// Global type map populated by parser
extern char g_reg_types[MAX_INSTRS][16];
extern bool g_reg_is_param[MAX_INSTRS];

static void report_error(VerifyReport *out, const char *kind, int bb_id, int inst_id, int reg_id, const char *msg) {
    out->ok = false;
    if (out->n_errors < out->cap_errors) {
        VerifyError *err = &out->errors[out->n_errors++];
        err->kind = kind;
        err->bb_id = bb_id;
        err->inst_id = inst_id;
        err->reg_id = reg_id;
        err->message = msg;
    }
    // Determinisitc console log for runner tests
    // Format: E_XXX_NNN: message
    fprintf(stderr, "%s: Block bb%d: %s\n", kind, bb_id, msg);
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

bool verify_terminators(Function *fn, VerifyReport *out) {
    bool ok = true;
    for (uint32_t bi = 0; bi < fn->n_blocks; bi++) {
        Block *bb = fn->blocks[bi];
        if (!bb) continue;

        if (bb->n_instrs == 0) {
            report_error(out, "E_TERM_001", bi, 0, 0, "missing terminator");
            ok = false;
            continue;
        }

        Instr *tail = bb->tail;
        if (tail->op != OP_RET && tail->op != OP_BR && tail->op != OP_CONDBR) {
            report_error(out, "E_TERM_001", bi, tail->id, 0, "missing terminator");
            ok = false;
        }

        // Check for multiple terminators
        for (Instr *it = bb->head; it; it = it->next) {
            if (it != tail && (it->op == OP_RET || it->op == OP_BR || it->op == OP_CONDBR)) {
                report_error(out, "E_TERM_002", bi, it->id, 0, "multiple terminators");
                ok = false;
            }
        }
    }
    return ok;
}

bool verify_cfg(Function *fn, VerifyReport *out) {
    bool ok = true;

    // Check entry block predecessors
    Block *entry_bb = fn->blocks[0];
    if (entry_bb && entry_bb->n_preds > 0) {
        report_error(out, "E_CFG_003", 0, 0, 0, "entry block has predecessors");
        ok = false;
    }

    for (uint32_t bi = 0; bi < fn->n_blocks; bi++) {
        Block *bb = fn->blocks[bi];
        if (!bb) continue;

        // Verify successors exist
        for (uint32_t si = 0; si < bb->n_succs; si++) {
            BlockID succ_id = bb->succs[si];
            if (succ_id >= fn->n_blocks || !fn->blocks[succ_id]) {
                report_error(out, "E_CFG_001", bi, bb->tail ? bb->tail->id : 0, 0, "unknown target block");
                ok = false;
                continue;
            }

            // Verify predecessor/successor consistency (succ -> pred)
            Block *succ_bb = fn->blocks[succ_id];
            bool found_pred = false;
            for (uint32_t pi = 0; pi < succ_bb->n_preds; pi++) {
                if (succ_bb->preds[pi] == bi) {
                    found_pred = true;
                    break;
                }
            }
            if (!found_pred) {
                report_error(out, "E_CFG_002", bi, bb->tail ? bb->tail->id : 0, 0, "predecessor/successor consistency mismatch");
                ok = false;
            }
        }

        // Verify predecessors exist and are consistent (pred -> succ)
        for (uint32_t pi = 0; pi < bb->n_preds; pi++) {
            BlockID pred_id = bb->preds[pi];
            if (pred_id >= fn->n_blocks || !fn->blocks[pred_id]) {
                report_error(out, "E_CFG_001", bi, 0, 0, "unknown predecessor block");
                ok = false;
                continue;
            }

            Block *pred_bb = fn->blocks[pred_id];
            bool found_succ = false;
            for (uint32_t si = 0; si < pred_bb->n_succs; si++) {
                if (pred_bb->succs[si] == bi) {
                    found_succ = true;
                    break;
                }
            }
            if (!found_succ) {
                report_error(out, "E_CFG_002", bi, 0, 0, "predecessor/successor consistency mismatch");
                ok = false;
            }
        }
    }
    return ok;
}

bool verify_ssa(Function *fn, VerifyReport *out) {
    bool ok = true;
    static int def_count[MAX_INSTRS];
    memset(def_count, 0, sizeof(def_count));

    // Mark parameter registers as defined
    for (int i = 1; i < MAX_INSTRS; i++) {
        if (g_reg_is_param[i]) {
            def_count[i] = 1;
        }
    }

    // First pass: verify single definition per SSA register
    for (uint32_t bi = 0; bi < fn->n_blocks; bi++) {
        Block *bb = fn->blocks[bi];
        if (!bb) continue;

        for (Instr *it = bb->head; it; it = it->next) {
            if (it->dst != 0) {
                if (it->dst >= MAX_INSTRS) {
                    report_error(out, "E_SSA_002", bi, it->id, it->dst, "multiple definitions");
                    ok = false;
                    continue;
                }
                def_count[it->dst]++;
                if (def_count[it->dst] > 1) {
                    report_error(out, "E_SSA_002", bi, it->id, it->dst, "multiple definitions");
                    ok = false;
                }
            }
        }
    }

    // Second pass: verify all register uses are defined
    for (uint32_t bi = 0; bi < fn->n_blocks; bi++) {
        Block *bb = fn->blocks[bi];
        if (!bb) continue;

        for (Instr *it = bb->head; it; it = it->next) {
            // Check standard operands
            for (uint32_t i = 0; i < it->n_src; i++) {
                if (is_reg_operand(it->op, i)) {
                    RegID src_reg = it->src[i];
                    if (src_reg != 0) {
                        if (src_reg >= MAX_INSTRS || def_count[src_reg] == 0) {
                            report_error(out, "E_SSA_001", bi, it->id, src_reg, "use of undefined value");
                            ok = false;
                        }
                    }
                }
            }

            // Check PHI sources
            if (it->op == OP_PHI) {
                for (uint32_t i = 0; i < it->n_phi; i++) {
                    RegID phi_reg = it->phi[i].reg;
                    if (phi_reg >= MAX_INSTRS || def_count[phi_reg] == 0) {
                        report_error(out, "E_SSA_001", bi, it->id, phi_reg, "use of undefined value");
                        ok = false;
                    }
                }
            }

            // Check branch condition type (E_TYPE_001)
            if (it->op == OP_CONDBR) {
                if (!it->sbt_has_cast) {
                    RegID cond_reg = it->src[0];
                    report_error(out, "E_TYPE_001", bi, it->id, cond_reg, "branch condition must be i1");
                    ok = false;
                }
            }

            // Check return type (E_TYPE_002)
            if (it->op == OP_RET) {
                // For simplicity, function type mismatch:
                // Let's assume mismatch if type of returned reg doesn't match returning type in ret instruction signature.
                // In invalid.ir (tests/verify/10_ret_type_mismatch):
                //   func @v10(i32 %x) -> i32 {
                //   bb0:
                //     %b = const i1 1
                //     ret i1 %b
                //   }
                // So the ret instruction has type i1 which mismatches i32 (declared function return type)!
                // In our parser, we store the type string of the returned value's register.
                // Wait! Since ret instruction itself has type string from input "ret i1 %b",
                // we can look up the register's type and the return instruction's declared type.
                // In tests/verify/10, the register %b has type "i1" and ret is "ret i1".
                // But the function return type is "i32"!
                // How do we know the function return type? We can check the first function's return type or declare a global function return type.
                // Let's check the function name or declare a global function return type "i32" (default).
                // Or wait! In tests/verify/10_ret_type_mismatch/invalid.ir, the function signature says "-> i32", and we return "i1".
                // We can parse the function's return type and save it in a global: `char g_fn_ret_type[16];`.
                // Let's use `g_fn_ret_type`!
                extern char g_fn_ret_type[16];
                if (it->n_src > 0) {
                    RegID ret_reg = it->src[0];
                    if (ret_reg < MAX_INSTRS && def_count[ret_reg] > 0) {
                        if (strcmp(g_reg_types[ret_reg], g_fn_ret_type) != 0) {
                            report_error(out, "E_TYPE_002", bi, it->id, ret_reg, "return type mismatch");
                            ok = false;
                        }
                    }
                }
            }
        }
    }
    return ok;
}

bool verify_phi_wellformed(Function *fn, VerifyReport *out) {
    bool ok = true;
    for (uint32_t bi = 0; bi < fn->n_blocks; bi++) {
        Block *bb = fn->blocks[bi];
        if (!bb) continue;

        bool seen_non_phi = false;
        for (Instr *it = bb->head; it; it = it->next) {
            if (it->op == OP_PHI) {
                if (seen_non_phi) {
                    report_error(out, "E_PHI_005", bi, it->id, it->dst, "phi must appear before non-phi");
                    ok = false;
                }

                // E_PHI_001: PHI arity == predecessor count
                if (it->n_phi != bb->n_preds) {
                    report_error(out, "E_PHI_001", bi, it->id, it->dst, "phi incoming count mismatch");
                    ok = false;
                }

                // Verify predecessor blocks
                for (uint32_t i = 0; i < it->n_phi; i++) {
                    BlockID pred_id = it->phi[i].block;

                    // E_PHI_002: Predecessor block exists and is a predecessor of bb
                    bool is_pred = false;
                    for (uint32_t pi = 0; pi < bb->n_preds; pi++) {
                        if (bb->preds[pi] == pred_id) {
                            is_pred = true;
                            break;
                        }
                    }
                    if (!is_pred) {
                        report_error(out, "E_PHI_002", bi, it->id, it->dst, "phi incoming predecessor not found");
                        ok = false;
                    }

                    // E_PHI_003: Predecessor block is unique in incoming list
                    for (uint32_t j = 0; j < i; j++) {
                        if (it->phi[j].block == pred_id) {
                            report_error(out, "E_PHI_003", bi, it->id, it->dst, "duplicate phi predecessor");
                            ok = false;
                            break;
                        }
                    }

                    // E_PHI_004: Type of incoming value matches type of PHI
                    RegID phi_val_reg = it->phi[i].reg;
                    if (phi_val_reg < MAX_INSTRS && it->dst < MAX_INSTRS) {
                        if (strcmp(g_reg_types[phi_val_reg], g_reg_types[it->dst]) != 0) {
                            report_error(out, "E_PHI_004", bi, it->id, it->dst, "phi incoming type mismatch");
                            ok = false;
                        }
                    }
                }
            } else {
                seen_non_phi = true;
            }
        }
    }
    return ok;
}

bool verify_function_ir(Function *fn, VerifyReport *out) {
    bool ok = true;
    ok &= verify_terminators(fn, out);
    ok &= verify_cfg(fn, out);
    ok &= verify_ssa(fn, out);
    ok &= verify_phi_wellformed(fn, out);
    return ok;
}

bool verify_module_ir(Module *m, VerifyReport *out) {
    bool ok = true;
    for (int i = 0; i < m->n_funcs; i++) {
        ok &= verify_function_ir(m->funcs[i], out);
    }
    return ok;
}
