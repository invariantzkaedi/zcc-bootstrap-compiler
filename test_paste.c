#define LUA_GCPPAUSE 3
#define setgcparam(g,p,v) (g->gcparams[LUA_GCP##p] = v)
void test() { setgcparam(g,PAUSE,10); }
