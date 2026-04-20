cat << 'PATCH' > patch.c
static void pp_parse_isolated(PPState *state, const char *src) {
    int old_depth = state->input_depth;
    PPInputCtx old_stack[32];
    memcpy(old_stack, state->input_stack, sizeof(old_stack));
    
    state->input_depth = 0;
    pp_push_input(state, src, NULL, NULL);
    pp_parse_target_depth(state, 1);
    
    state->input_depth = old_depth;
    memcpy(state->input_stack, old_stack, sizeof(old_stack));
}
PATCH
