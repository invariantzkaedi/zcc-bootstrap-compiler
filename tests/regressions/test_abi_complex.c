#include <complex.h>
#include <stdio.h>

float _Complex fc(float a, float b) { return a + b*I; }
double _Complex dc(double a, double b) { return a + b*I; }

int main(void) {
  float _Complex x = fc(1.5f, 2.5f);
  double _Complex y = dc(3.0, 4.0);
  printf("%.1f %.1f %.1f %.1f\n", (double)crealf(x), (double)cimagf(x), creal(y), cimag(y));
  return 0;
}
