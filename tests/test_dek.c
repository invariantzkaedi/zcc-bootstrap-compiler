#include <stdio.h>
#include <string.h>

typedef unsigned long long u64;

static void dekkerMul2( double *x, double y, double yy){
  double p, q, m;
  double tx, ty, hx, hy;
  double c, cc;

  memcpy(&m, (void*)&x[0], 8);
  u64 bits;
  memcpy(&bits, &m, 8);
  bits &= 0xfffffffffc000000LL;
  memcpy(&m, &bits, 8);
  memcpy(&hx, &m, 8);
  tx = x[0] - hx;
  
  memcpy(&m, &y, 8);
  memcpy(&bits, &m, 8);
  bits &= 0xfffffffffc000000LL;
  memcpy(&m, &bits, 8);
  memcpy(&hy, &m, 8);
  ty = y - hy;
  p = hx*hy;
  q = hx*ty + tx*hy;
  c = p+q;
  cc = p - c + q + tx*ty + x[1]*y + x[0]*yy;
  
  u64 hex_cc;
  memcpy(&hex_cc, &cc, 8);
  printf("cc hex=%llx\n", hex_cc);

  x[0] = c + cc;
  x[1] = c - x[0];
  
  u64 hex_x1_1;
  double tmp1 = x[1];
  memcpy(&hex_x1_1, &tmp1, 8);
  printf("x[1] before += cc: hex=%llx\n", hex_x1_1);

  x[1] += cc;
  
  u64 hex_x1_2;
  double tmp2 = x[1];
  memcpy(&hex_x1_2, &tmp2, 8);
  printf("x[1] after += cc: hex=%llx\n", hex_x1_2);
}
int main() {
    double rr[2];
    rr[0] = 314.0;
    rr[1] = 0.0;
    dekkerMul2(rr, 1.0e-1, -5.5511151231257827021e-18);
    return 0;
}