#ifndef ZCC_STATIC_ASSERT_H
#define ZCC_STATIC_ASSERT_H

#include "zcc_types.h"
#include "zcc_diagnostics.h"

void zcc_handle_static_assert(Node *condition, const char *message, SourceLoc loc);

#endif
