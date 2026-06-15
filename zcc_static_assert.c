#include "zcc_static_assert.h"
#include "zcc_diagnostics.h"

long long eval_const_expr(Node *n);

static bool zcc_is_const_expr(Node *n) {
    if (!n) return true;
    switch (n->kind) {
        case ND_NUM:
        case ND_FLIT:
        case ND_STR:
        case ND_NOP:
            return true;
        case ND_VAR:
            if (n->sym && n->sym->is_enum_const && !n->sym->is_local) {
                return true;
            }
            return false;
        case ND_ADD:
        case ND_SUB:
        case ND_MUL:
        case ND_DIV:
        case ND_MOD:
        case ND_EQ:
        case ND_NE:
        case ND_LT:
        case ND_LE:
        case ND_GT:
        case ND_GE:
        case ND_LAND:
        case ND_LOR:
        case ND_BAND:
        case ND_BOR:
        case ND_BXOR:
        case ND_SHL:
        case ND_SHR:
        case ND_TERNARY:
            if (!zcc_is_const_expr(n->lhs)) return false;
            if (!zcc_is_const_expr(n->rhs)) return false;
            if (n->kind == ND_TERNARY && !zcc_is_const_expr(n->cond)) return false;
            return true;
        case ND_NEG:
        case ND_ADDR:
        case ND_DEREF:
        case ND_BNOT:
        case ND_LNOT:
        case ND_CAST:
            return zcc_is_const_expr(n->lhs);
        default:
            return false;
    }
}

void zcc_handle_static_assert(Node *condition, const char *message, SourceLoc loc) {
    if (!zcc_is_const_expr(condition)) {
        zcc_diag(
            DIAG_ERROR,
            E_STATIC_ASSERT_NOT_CONSTANT,
            LAYOUT_PHASE_INIT,
            loc,
            "_Static_assert expression is not an integer constant expression"
        );
        return;
    }

    long long value = eval_const_expr(condition);

    if (value == 0) {
        zcc_diag(
            DIAG_ERROR,
            E_STATIC_ASSERT_FAILED,
            LAYOUT_PHASE_INIT,
            loc,
            "static assertion failed: \"%s\"",
            message ? message : "static assertion failed"
        );
    }
}
