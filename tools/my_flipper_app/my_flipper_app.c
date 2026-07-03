#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>

// Draw callback - executed on every screen refresh
static void draw_callback(Canvas* canvas, void* ctx) {
    UNUSED(ctx);
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 10, 20, "ZCC Dev Monitor");
    
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 10, 40, "Status: Online");
    canvas_draw_str(canvas, 10, 52, "Press Back to Exit");
}

// Input callback - triggered on button presses
static void input_callback(InputEvent* input_event, void* ctx) {
    FuriMessageQueue* event_queue = ctx;
    furi_message_queue_put(event_queue, input_event, FuriWaitForever);
}

int32_t my_flipper_app_main(void* p) {
    UNUSED(p);
    
    // Create an event queue to hold input events
    FuriMessageQueue* event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    
    // Configure Viewport
    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, draw_callback, NULL);
    view_port_input_callback_set(view_port, input_callback, event_queue);
    
    // Register Viewport in GUI
    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);
    
    InputEvent event;
    bool running = true;
    
    // Main event loop
    while(running) {
        if(furi_message_queue_get(event_queue, &event, FuriWaitForever) == FuriStatusOk) {
            // If the user presses the BACK button, exit the loop
            if(event.type == InputTypeShort && event.key == InputKeyBack) {
                running = false;
            }
        }
    }
    
    // Cleanup memory
    view_port_enabled_set(view_port, false);
    gui_remove_view_port(gui, view_port);
    view_port_free(view_port);
    furi_record_close(RECORD_GUI);
    furi_message_queue_free(event_queue);
    
    return 0;
}
