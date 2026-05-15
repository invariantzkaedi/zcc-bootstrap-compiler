struct Big { long a,b,c; };
struct Big make(int hidden_arg) { return (struct Big){hidden_arg, 2, 39}; }
int main() { struct Big x = make(1); printf("ret: %d\n", (int)x.c); return 0; }
