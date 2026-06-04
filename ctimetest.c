#define CTIMEOPT_VAL_(opt) #opt
#define CTIMEOPT_VAL(opt) CTIMEOPT_VAL_(opt)
const char *x = "prefix=" CTIMEOPT_VAL(-2000);
const char *y = CTIMEOPT_VAL(HELLO);
