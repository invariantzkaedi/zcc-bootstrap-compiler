#ifndef SCHED_H
#define SCHED_H

typedef void (*task_func_t)(void);

void sched_init(void);
void sched_create_task(int task_id, void *stack_limit, int stack_size, task_func_t func);
void sched_yield(void);

#endif /* SCHED_H */
