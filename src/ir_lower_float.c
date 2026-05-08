/*
 * ir_lower_float.c — ZCC Floating-Point Lowering Pass
 * ===================================================
 * Scans the IR function to track types of temporaries, and lowers generic
 * IR_CAST instructions into explicit IR_ITOF (Integer-to-Float) or IR_FTOI
 * (Float-to-Integer) when crossing floating-point boundaries.
 */

#include <string.h>
#include <stdlib.h>
#include "../ir.h"
#include "../ir_pass_manager.h"

typedef struct {
    char name[64];
    ir_type_t type;
} temp_type_t;

ir_pass_result_t ir_pass_lower_float(void *fn_ptr) {
    ir_func_t *fn = (ir_func_t *)fn_ptr;
    ir_pass_result_t r;
    ir_node_t *n;
    int modified = 0;
    
    temp_type_t types[2048];
    int num_types = 0;
    
    memset(&r, 0, sizeof(r));
    
    for (n = fn->head; n; n = n->next) {
        r.nodes_before++;
        
        /* track dst type */
        if (n->dst[0] == '%' && n->dst[1] == 't') {
            int found = 0;
            int i;
            for (i = 0; i < num_types; i++) {
                if (strcmp(types[i].name, n->dst) == 0) {
                    types[i].type = n->type;
                    found = 1;
                    break;
                }
            }
            if (!found && num_types < 2048) {
                strcpy(types[num_types].name, n->dst);
                types[num_types].type = n->type;
                num_types++;
            }
        }
        
        if (n->op == IR_CAST) {
            ir_type_t src_type = IR_TY_I64; // Default to int64
            int i;
            for (i = 0; i < num_types; i++) {
                if (strcmp(types[i].name, n->src1) == 0) {
                    src_type = types[i].type;
                    break;
                }
            }
            
            int src_is_float = (src_type == IR_TY_F32 || src_type == IR_TY_F64);
            int dst_is_float = (n->type == IR_TY_F32 || n->type == IR_TY_F64);
            
            if (!src_is_float && dst_is_float) {
                n->op = IR_ITOF;
                modified++;
            } else if (src_is_float && !dst_is_float) {
                n->op = IR_FTOI;
                modified++;
            }
        }
    }
    
    if (modified > 0) {
        fprintf(stderr, "\033[38;5;51m[WARDEN] Lowered \033[38;5;199m%d\033[38;5;51m float casting nodes in %s\033[0m\n", modified, fn->name);
    }
    
    r.nodes_after = r.nodes_before;
    r.nodes_modified = modified;
    r.changed = modified > 0;
    return r;
}
