/* This file MUST fail to compile. CI asserts a non-zero exit.
 * If it ever compiles, the ir_bridge.h single-TU guard has regressed. */
#include "ir_emit_dispatch.h"
#include "ir_bridge.h"   /* no ZCC_IR_BRIDGE_ALLOWED defined → #error expected */
int main(void) { return 0; }
