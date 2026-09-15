#ifndef _STDATOMIC_H
#define _STDATOMIC_H

enum memory_order {
    memory_order_relaxed = 0,
    memory_order_consume = 1,
    memory_order_acquire = 2,
    memory_order_release = 3,
    memory_order_acq_rel = 4,
    memory_order_seq_cst = 5
};
typedef enum memory_order memory_order;

typedef int atomic_int;
typedef unsigned int atomic_uint;
typedef long atomic_long;
typedef unsigned long atomic_ulong;
typedef struct { int __val; } atomic_flag;

#define ATOMIC_FLAG_INIT { 0 }
#define atomic_load(ptr) (*(ptr))
#define atomic_store(ptr, val) (*(ptr) = (val))
#define atomic_fetch_add(ptr, val) (*(ptr) += (val))
#define atomic_fetch_sub(ptr, val) (*(ptr) -= (val))

#define atomic_exchange_explicit(ptr, val, mo) (*(ptr) = (val), 0)
#define atomic_load_explicit(ptr, mo) (*(ptr))
#define atomic_store_explicit(ptr, val, mo) (*(ptr) = (val))

#define __builtin_ia32_pause() do {} while(0)

#endif /* _STDATOMIC_H */
