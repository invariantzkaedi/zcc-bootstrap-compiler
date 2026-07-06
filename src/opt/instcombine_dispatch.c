typedef bool (*IcRuleFn)(ICtx*);

static IcRuleFn kRules[] = {
    ic_rule_add_zero,
    ic_rule_sub_zero,
    ic_rule_mul_one,
    ic_rule_mul_zero,
    ic_rule_div_one,
    ic_rule_and_zero,
    ic_rule_and_allones,
    ic_rule_or_zero,
    ic_rule_xor_zero,
    ic_rule_xor_self,
    ic_rule_sub_self,
    ic_rule_shift_by_zero,
    ic_rule_reassoc_add_consts,
    ic_rule_reassoc_mul_consts,
    ic_rule_icmp_self
};

bool ic_try_rules(Function *fn, Instr *it) {
    ICtx c = {.fn = fn, .it = it};
    for (int i = 0; i < (int)(sizeof(kRules)/sizeof(kRules[0])); i++) {
        if (kRules[i](&c)) return true;
    }
    return false;
}
