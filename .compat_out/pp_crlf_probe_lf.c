typedef struct { double x,y,z; } Vec3;
#define v_dot(a, b) ((a)->x*(b)->x + (a)->y*(b)->y + (a)->z*(b)->z)
#define v_norm(in, out) do { \\
    double _l = v_dot(in,in); \\
    if(_l>0.0){ (out)->x=(in)->x/_l; (out)->y=(in)->y/_l; (out)->z=(in)->z/_l; } \\
    else { (out)->x=0; (out)->y=0; (out)->z=1; } \\
} while(0)
int main(void){ Vec3 a={1,2,3}, b={0,0,0}; v_norm(&a,&b); return 0; }
