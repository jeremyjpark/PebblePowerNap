#include <pebble.h>

// Key for saving nap minute count
#define NAP_TIME_KEY 6789

// Default number of nap minutes
#define NAP_TIME_DEFAULT 20

// Minimum and maximum number of minutes for a nap
#define NAP_TIME_MIN 10
#define NAP_TIME_MAX 30

static Window *window;

static GBitmap *action_icon_plus;
static GBitmap *action_icon_minus;

static ActionBarLayer *action_bar;

static TextLayer *header_text_layer;
static TextLayer *body_text_layer;

static int nap_time = NAP_TIME_DEFAULT;

static void update_text() {
    static char body_text[10];
    snprintf(body_text, sizeof(body_text), "%u min", nap_time);
    text_layer_set_text(body_text_layer, body_text);
}

static void increment_click_handler(ClickRecognizerRef recognizer, void *context) {
    // Prevent time from going above max
    if (nap_time >= NAP_TIME_MAX) {
        return;
    }
    
    nap_time++;
    update_text();
}

static void decrement_click_handler(ClickRecognizerRef recognizer, void *context) {
    // Prevent time from going below min
    if (nap_time <= NAP_TIME_MIN) {
        return;
    }
    
    nap_time--;
    update_text();
}

static void click_config_provider(void *context) {
    const uint16_t repeat_interval_ms = 100;
    window_single_repeating_click_subscribe(BUTTON_ID_UP, repeat_interval_ms, (ClickHandler) increment_click_handler);
    window_single_repeating_click_subscribe(BUTTON_ID_DOWN, repeat_interval_ms, (ClickHandler) decrement_click_handler);
}

static void window_load(Window *me) {
    action_bar = action_bar_layer_create();
    action_bar_layer_add_to_window(action_bar, me);
    action_bar_layer_set_click_config_provider(action_bar, click_config_provider);
    
    action_bar_layer_set_icon(action_bar, BUTTON_ID_UP, action_icon_plus);
    action_bar_layer_set_icon(action_bar, BUTTON_ID_DOWN, action_icon_minus);
    
    Layer *layer = window_get_root_layer(me);
    const int16_t width = layer_get_frame(layer).size.w - ACTION_BAR_WIDTH - 3;
    
    header_text_layer = text_layer_create(GRect(4, 0, width, 60));
    text_layer_set_font(header_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24));
    text_layer_set_background_color(header_text_layer, GColorClear);
    text_layer_set_text(header_text_layer, "Power Nap");
    layer_add_child(layer, text_layer_get_layer(header_text_layer));
    
    body_text_layer = text_layer_create(GRect(4, 44, width, 60));
    text_layer_set_font(body_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
    text_layer_set_background_color(body_text_layer, GColorClear);
    layer_add_child(layer, text_layer_get_layer(body_text_layer));
    
    update_text();
}

static void window_unload(Window *window) {
    text_layer_destroy(header_text_layer);
    text_layer_destroy(body_text_layer);
    
    action_bar_layer_destroy(action_bar);
}

static void init(void) {
    action_icon_plus = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_ACTION_ICON_PLUS);
    action_icon_minus = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_ACTION_ICON_MINUS);
    
    window = window_create();
    window_set_window_handlers(window, (WindowHandlers) {
        .load = window_load,
        .unload = window_unload,
    });
    
    // Get the count from persistent storage for use if it exists, otherwise use the default
    nap_time = persist_exists(NAP_TIME_KEY) ? persist_read_int(NAP_TIME_KEY) : NAP_TIME_DEFAULT;
    
    window_stack_push(window, true /* Animated */);
}

static void deinit(void) {
    // Save the count into persistent storage on app exit
    persist_write_int(NAP_TIME_KEY, nap_time);
    
    window_destroy(window);
    
    gbitmap_destroy(action_icon_plus);
    gbitmap_destroy(action_icon_minus);
}

int main(void) {
    init();
    app_event_loop();
    deinit();
}
