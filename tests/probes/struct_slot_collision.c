// Deterministic probe for struct parameter stack-slot collision.
// Adjust shapes to match the bug class seen in your backend.

#include <stdio.h>
#include <stdint.h>

typedef struct {
  long a;
  long b;
} Pair;

static long consume(Pair x, Pair y, long z) {
  // If x/y alias same stack slot due to allocator bug, this tends to diverge.
  volatile long s1 = x.a + x.b;
  volatile long s2 = y.a + y.b;
  return s1 * 31 + s2 * 17 + z;
}

int main(void) {
  Pair p = {3, 5};      // sum 8
  Pair q = {11, 13};    // sum 24
  long r = consume(p, q, 7);
  // expected: 8*31 + 24*17 + 7 = 248 + 408 + 7 = 663
  printf("%ld\n", r);
  return 0;
}
