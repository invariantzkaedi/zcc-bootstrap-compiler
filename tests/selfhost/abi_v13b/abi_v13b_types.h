#ifndef ABI_V13B_TYPES_H
#define ABI_V13B_TYPES_H

/* Observe ((rsp + 8) & 15) at function entry */
long check_stack_align(void);

/* Callee stack alignment tester compiled via target */
void zcc_callee_align_test(void);

/* Multi-register and loop workload */
long zcc_workload(long a, long b, long c, long d, long e, long f, long g, long h);

/* Independent assembly caller testing preservation of %rbx, %rbp, %r12, %r13, %r14, %r15 */
long run_callee_saved_probe(void);

#endif /* ABI_V13B_TYPES_H */
