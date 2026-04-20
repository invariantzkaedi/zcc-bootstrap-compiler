#define clCvalue(o) check_exp(ttisCclosure(o), gco2ccl(val_(o).gc))
#define ttisCclosure(o) zzzzzzzzz((o), 1)
#define check_exp(c, e) (lua_assert(c), (e))
#define lua_assert(c) ((void)0)
#define val_(o) ((o)->value_)
#define zzzzzzzzz(o, t) ((o)->tt_ == (t))
#define gco2ccl(o) check_exp((o)->tt == 1, &((o)->cl.c))

clCvalue(obj)
