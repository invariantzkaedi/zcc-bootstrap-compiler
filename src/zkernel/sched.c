#include "sched.h"

struct task {
    void *sp;
    int active;
};

static struct task tasks[2];
static int current_task_id = 0;

/* Assembly context switcher */
extern void switch_context(void **old_sp, void *new_sp);

void sched_init(void) {
    tasks[0].active = 1;
    tasks[0].sp = 0;
    tasks[1].active = 0;
    tasks[1].sp = 0;
    current_task_id = 0;
}

void sched_create_task(int task_id, void *stack_limit, int stack_size, task_func_t func) {
    unsigned int *stack_top;
    
    stack_top = (unsigned int *)((char *)stack_limit + stack_size);
    /* 8-byte align stack top */
    stack_top = (unsigned int *)((unsigned int)stack_top & ~7);
    
    /* Pre-populate stack for context restore */
    stack_top[-1] = (unsigned int)func; /* LR / PC */
    stack_top[-2] = 0; /* r7 */
    stack_top[-3] = 0; /* r6 */
    stack_top[-4] = 0; /* r5 */
    stack_top[-5] = 0; /* r4 */
    stack_top[-6] = 0; /* r11 */
    stack_top[-7] = 0; /* r10 */
    stack_top[-8] = 0; /* r9 */
    stack_top[-9] = 0; /* r8 */
    
    tasks[task_id].sp = (void *)&stack_top[-9];
    tasks[task_id].active = 1;
}

void sched_yield(void) {
    int prev_task_id;
    int next_task_id;
    
    prev_task_id = current_task_id;
    next_task_id = (current_task_id + 1) % 2;
    
    if (tasks[next_task_id].active) {
        current_task_id = next_task_id;
        switch_context(&tasks[prev_task_id].sp, tasks[next_task_id].sp);
    }
}
