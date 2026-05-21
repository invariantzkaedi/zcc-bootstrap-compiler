#include "zcc_svg.h"
#include <stdio.h>
#include <math.h>

int main() {
    ZccSvgNode* svg = svg_svg();
    svg_set_width(svg, "800");
    svg_set_height(svg, "800");
    svg_set_viewBox(svg, "0 0 800 800");
    svg_set_style(svg, "background-color: #050510;");

    // We will generate a chaotic Strange Attractor (Clifford or Lorenz-ish projection)
    // using raw C math dynamics, then bake the trajectory into an SVG path.
    
    ZccSvgNode* p = svg_path();
    svg_set_fill(p, "none");
    svg_set_stroke(p, "cyan");
    svg_set_stroke_width(p, "0.5");
    svg_set_opacity(p, "0.8");

    SvgPathBuilder* pb = svg_path_begin();
    
    // Clifford Attractor parameters
    double a = -1.4, b = 1.6, c = 1.0, d = 0.7;
    double x = 0.0, y = 0.0;
    
    svg_path_move_to(pb, 400.0, 400.0);
    
    // Calculate 50,000 recursive dynamic steps instantly in C
    for (int i = 0; i < 50000; i++) {
        double nx = sin(a * y) + c * cos(a * x);
        double ny = sin(b * x) + d * cos(b * y);
        x = nx;
        y = ny;
        
        // Map to SVG coordinates
        double plot_x = 400 + (x * 120);
        double plot_y = 400 + (y * 120);
        
        svg_path_line_to(pb, plot_x, plot_y);
    }
    
    svg_apply_path(p, pb);
    svg_add_child(svg, p);

    char* output = svg_to_string(svg);
    printf("%s\n", output);
    return 0;
}
