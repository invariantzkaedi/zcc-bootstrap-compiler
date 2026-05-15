#ifndef IR_TO_SMT_H
#define IR_TO_SMT_H

#include "../../ir.h"

void export_smt2(ir_func_t* fn, const char* path);

#endif
