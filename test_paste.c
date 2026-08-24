#define LUA_GCPPAUSE 3
#define setgcparam(g,p,v) (g->gcparams[LUA_GCP##p] = v)
struct G { int gcparams[10]; } *g;
void test() { setgcparam(g,PAUSE,10); }
int main() { return 0; }
