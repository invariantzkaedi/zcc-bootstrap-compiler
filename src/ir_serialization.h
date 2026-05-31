#ifndef ZCC_IR_SERIALIZATION_H
#define ZCC_IR_SERIALIZATION_H

#include "../ir.h"

/* Serialize SSA IR module to clean JSON format */
int ir_serialize_json(const ir_module_t *mod, const char *out_filename, const char *source_file);

/* Deserialize JSON schema directly back into native ir_module_t and instructions in memory */
int ir_deserialize_json(ir_module_t *mod, const char *in_filename);

/* Graph-matching CFG difference engine evaluating optimization topology drifts */
int ir_diff_json(const char *path_a, const char *path_b);

#endif /* ZCC_IR_SERIALIZATION_H */
