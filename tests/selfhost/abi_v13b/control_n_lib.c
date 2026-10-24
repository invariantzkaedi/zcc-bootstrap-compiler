#include <stdio.h>
#include "abi_v13b_types.h"

void zcc_callee_align_test(void) {
    long misalign = check_stack_align();
    printf("CALLEE_ALIGN_MISALIGN:%ld\n", misalign);
}
