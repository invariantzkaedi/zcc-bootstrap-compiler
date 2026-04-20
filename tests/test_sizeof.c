enum { IR_MAX_FUNCS = 8192 };
typedef void ir_func_t;
typedef struct {
    ir_func_t *funcs[IR_MAX_FUNCS];
    int func_count;
} ir_module_t;
int print_size() {
    return sizeof(ir_module_t);
}
