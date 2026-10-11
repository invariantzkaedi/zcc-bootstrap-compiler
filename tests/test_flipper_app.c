typedef unsigned int uint32_t;
typedef int int32_t;
typedef void Canvas;
typedef void ViewPort;
typedef void Gui;

extern void canvas_clear(Canvas* canvas);
extern void canvas_set_font(Canvas* canvas, int font);
extern void canvas_draw_str(Canvas* canvas, int x, int y, const char* str);
extern ViewPort* view_port_alloc(void);
extern void view_port_draw_callback_set(ViewPort* vp, void (*cb)(Canvas*, void*), void* ctx);
extern Gui* furi_record_open(const char* name);
extern void gui_add_view_port(Gui* gui, ViewPort* vp, int layer);

void draw_callback(Canvas* canvas, void* ctx) {
    canvas_clear(canvas);
    canvas_set_font(canvas, 0);
    canvas_draw_str(canvas, 2, 11, "ZKAEDI SOVEREIGN CORTEX-M4");
    canvas_draw_str(canvas, 2, 23, "FLIPPER ZERO STM32WB55");
    canvas_draw_str(canvas, 2, 35, "ZCC BARE-METAL THUMB-2");
}

int32_t flipper_entry(void* p) {
    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, draw_callback, (void*)0);
    Gui* gui = furi_record_open("gui");
    gui_add_view_port(gui, view_port, 0);
    return 0;
}
