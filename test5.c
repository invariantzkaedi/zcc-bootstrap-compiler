#define gco2ccl(o) check_exp((o)->tt == 1, &((o)->cl.c))
#define check_exp(c, e) (lua_assert(c), (e))
#define lua_assert(c) ((void)0)
#define val_(o) ((o)->value)
#define checktag(o, t) ((o)->tt_ == (t))

#define test_obj(o) check_exp(checktag(o, 2), gco2ccl(val_(o).gc))

test_obj(X)
