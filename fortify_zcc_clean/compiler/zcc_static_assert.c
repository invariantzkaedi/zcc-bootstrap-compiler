#include "zcc_static_assert.h"
#include "zcc_diagnostics.h"
#include "zcc_consteval.h"

void zcc_handle_static_assert(Expr *condition, const char *message, SourceLoc loc) {
    ConstEvalResult result = zcc_const_eval(condition);

    if (!result.is_constant) {
        zcc_diag(
            DIAG_ERROR,
            E_STATIC_ASSERT_NOT_CONSTANT,
            LAYOUT_PHASE_INIT,
            loc,
            "_Static_assert expression is not an integer constant expression"
        );
        return;
    }

    if (result.value == 0) {
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