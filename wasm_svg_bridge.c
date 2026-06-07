// wasm_svg_bridge.c — WebAssembly SVG renderer for EVM exploits
#include <emscripten.h>
#include <stdio.h>
#include <string.h>

static float g_phases[6] = {0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f};
static const char* topology_names[6] = {
    "reentrancy", "flashloan", "governance",
    "frontrun", "overflow", "honeypot"
};

EMSCRIPTEN_KEEPALIVE
const char* generate_svg(float* phases, int count) {
    static char svg_buffer[65536];  // plenty for full volumetric sheet
    char* ptr = svg_buffer;
    
    ptr += sprintf(ptr, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    ptr += sprintf(ptr, "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"1200\" height=\"800\" viewBox=\"0 0 1200 800\">\n");
    
    for (int i = 0; i < 6 && i < count; i++) {
        float p = (phases && i < count) ? phases[i] : g_phases[i];
        ptr += sprintf(ptr, "<!-- %s phase=%.3f -->\n", topology_names[i], p);
        // Volumetric topology primitives (similar to zcc_svg.h / zcc_anim.h)
        ptr += sprintf(ptr, "<g transform=\"translate(%d,100)\">\n", i*180);
        ptr += sprintf(ptr, "  <circle cx=\"60\" cy=\"60\" r=\"%.0f\" fill=\"none\" stroke=\"#0ff\" stroke-width=\"8\" />\n", 30.0f + p*40.0f);
        ptr += sprintf(ptr, "  <animate attributeName=\"r\" values=\"30;%.0f;30\" dur=\"%.1fs\" repeatCount=\"indefinite\"/>\n", 30.0f + p*40.0f, 1.0f + p);
        ptr += sprintf(ptr, "</g>\n");
    }
    ptr += sprintf(ptr, "</svg>\n");
    return svg_buffer;
}

EMSCRIPTEN_KEEPALIVE
void set_phase(int idx, float phase) {
    if (idx >= 0 && idx < 6) g_phases[idx] = phase;
}

EMSCRIPTEN_KEEPALIVE
float* get_phases() { return g_phases; }
