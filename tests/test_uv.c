#include <stdio.h>
int main() {
    int w = 1920, h = 1080;
    int i = 0, j = 0;
    double fov_scale = 1.0; /* Assume 1.0 for test */
    double u = (2.0 * (i + 0.5) / w - 1.0) * ((double)w / h) * fov_scale;
    double v = (1.0 - 2.0 * (j + 0.5) / h) * fov_scale;
    printf("%.15f %.15f\n", u, v);
    return 0;
}
