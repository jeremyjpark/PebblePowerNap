#include <pebble.h>
#include "PowerNap.h"

// Key for saving nap minute count
#define NAP_TIME_KEY 6789

// Default number of nap minutes
#define NAP_TIME_DEFAULT 20

// Minimum and maximum number of minutes for a nap
#define NAP_TIME_MIN 10
#define NAP_TIME_MAX 30

#define SLEEP_MODE 0
#define WAKE_MODE 1
#define ALARM_MODE 2

#define ONE_MINUTE 5000 // One minute in milliseconds

static Window *window;

static GBitmap *action_icon_plus;
static GBitmap *action_icon_sleep;
static GBitmap *action_icon_wake;
static GBitmap *action_icon_minus;

static ActionBarLayer *action_bar;

static TextLayer *header_text_layer;
static TextLayer *body_text_layer;
static TextLayer *label_text_layer;

static InverterLayer *inverter_layer;

static AppTimer *timer;

static uint16_t nap_time = NAP_TIME_DEFAULT;
static uint16_t remaining_nap_time = 0;
static uint16_t mode = WAKE_MODE;

static void update_time() {
    static char body_text[10];
    uint16_t time = (mode == WAKE_MODE) ? nap_time : remaining_nap_time;
    snprintf(body_text, sizeof(body_text), "%u min", time);
    text_layer_set_text(body_text_layer, body_text);
}

static void increment_click_handler(ClickRecognizerRef recognizer, void *context) {
    // Prevent time from going above max
    if (nap_time >= NAP_TIME_MAX) {
        return;
    }
    
    nap_time++;
    update_time();
}

static void decrement_click_handler(ClickRecognizerRef recognizer, void *context) {
    // Prevent time from going below min
    if (nap_time <= NAP_TIME_MIN) {
        return;
    }
    
    nap_time--;
    update_time();
}

static void decrease_remaining_time_callback(void *data) {
    remaining_nap_time--;
    
    if (remaining_nap_time <= 0) {
        // Time remaining reached 0, start the alarm
        set_mode(ALARM_MODE);
    } else {
        // Still time remaining, restart the minute timer
        app_timer_register(ONE_MINUTE, decrease_remaining_time_callback, NULL);
    }
    
    update_time();
}

static void set_mode(int new_mode) {
    mode = new_mode;
    
    if (mode == SLEEP_MODE) {
        
        // Show sun icon in action bar
        action_bar_layer_set_icon(action_bar, BUTTON_ID_SELECT, action_icon_wake);
        // Black background
        layer_set_hidden(inverter_layer_get_layer(inverter_layer), false);
        // Show action bar
        layer_set_hidden(action_bar_layer_get_layer(action_bar), false);
        // Show "remaining" and all other text layers
        layer_set_hidden(text_layer_get_layer(header_text_layer), false);
        layer_set_hidden(text_layer_get_layer(body_text_layer), false);
        layer_set_hidden(text_layer_get_layer(label_text_layer), false);
        // Sets the remaining time in nap and starts countdown
        remaining_nap_time = nap_time;
        timer = app_timer_register(ONE_MINUTE, decrease_remaining_time_callback, NULL);
        
        update_time();
        
    } else if (mode == WAKE_MODE) {
        
        // Show moon icon in action bar
        action_bar_layer_set_icon(action_bar, BUTTON_ID_SELECT, action_icon_sleep);
        // White background
        layer_set_hidden(inverter_layer_get_layer(inverter_layer), true);
        // Show action bar
        layer_set_hidden(action_bar_layer_get_layer(action_bar), false);
        // Hide "remaining", show all other text layers
        layer_set_hidden(text_layer_get_layer(header_text_layer), false);
        layer_set_hidden(text_layer_get_layer(body_text_layer), false);
        layer_set_hidden(text_layer_get_layer(label_text_layer), true);
        // Stops countdown
        app_timer_cancel(timer);
        
        update_time();
        
    } else if (mode == ALARM_MODE) {
        
        // White background
        layer_set_hidden(inverter_layer_get_layer(inverter_layer), true);
        // Hide action bar
        layer_set_hidden(action_bar_layer_get_layer(action_bar), true);
        // Hide all text layers
        layer_set_hidden(text_layer_get_layer(header_text_layer), true);
        layer_set_hidden(text_layer_get_layer(body_text_layer), true);
        layer_set_hidden(text_layer_get_layer(label_text_layer), true);
        // Vibrate
        vibes_short_pulse();
        
    }
}

static void sleep_wake_click_handler(ClickRecognizerRef recognizer, void *context) {
    // Toggle the center action bar button to switch between wake and sleep
    if (mode == WAKE_MODE) {
        set_mode(SLEEP_MODE);
    } else {
        set_mode(WAKE_MODE);
    }
}

static void click_config_provider(void *context) {
    // Increment/decrement time can be held down to quickly change
    const uint16_t repeat_interval_ms = 100;
    window_single_repeating_click_subscribe(BUTTON_ID_UP, repeat_interval_ms, (ClickHandler) increment_click_handler);
    window_single_repeating_click_subscribe(BUTTON_ID_DOWN, repeat_interval_ms, (ClickHandler) decrement_click_handler);
    
    window_single_click_subscribe(BUTTON_ID_SELECT, sleep_wake_click_handler);
}

static void window_load(Window *me) {
    action_bar = action_bar_layer_create();
    action_bar_layer_add_to_window(action_bar, me);
    action_bar_layer_set_click_config_provider(action_bar, click_config_provider);
    
    action_bar_layer_set_icon(action_bar, BUTTON_ID_UP, action_icon_plus);
    action_bar_layer_set_icon(action_bar, BUTTON_ID_DOWN, action_icon_minus);
    
    Layer *layer = window_get_root_layer(me);
    const int16_t window_width = layer_get_frame(layer).size.w;
    const int16_t window_height = layer_get_frame(layer).size.h;
    const int16_t width = window_width - ACTION_BAR_WIDTH - 3;
    
    header_text_layer = text_layer_create(GRect(4, 0, width, 60));
    text_layer_set_font(header_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24));
    text_layer_set_background_color(header_text_layer, GColorClear);
    text_layer_set_text(header_text_layer, "Power Nap");
    layer_add_child(layer, text_layer_get_layer(header_text_layer));
    
    body_text_layer = text_layer_create(GRect(4, 44, width, 60));
    text_layer_set_font(body_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
    text_layer_set_background_color(body_text_layer, GColorClear);
    layer_add_child(layer, text_layer_get_layer(body_text_layer));
    
    label_text_layer = text_layer_create(GRect(4, 44 + 28, width, 60));
    text_layer_set_font(label_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
    text_layer_set_background_color(label_text_layer, GColorClear);
    text_layer_set_text(label_text_layer, "remaining");
    layer_add_child(layer, text_layer_get_layer(label_text_layer));
    
    inverter_layer = inverter_layer_create(GRect(0, 0, window_width, window_height));
    layer_add_child(layer, inverter_layer_get_layer(inverter_layer));
    
    update_time();
    set_mode(mode);
}

static void window_unload(Window *window) {
    text_layer_destroy(header_text_layer);
    text_layer_destroy(body_text_layer);
    
    action_bar_layer_destroy(action_bar);
    
    inverter_layer_destroy(inverter_layer);
}

static void init(void) {
    action_icon_plus = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_ACTION_ICON_PLUS);
    action_icon_sleep = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_ACTION_ICON_SLEEP);
    action_icon_wake = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_ACTION_ICON_WAKE);
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
    gbitmap_destroy(action_icon_sleep);
    gbitmap_destroy(action_icon_wake);
    gbitmap_destroy(action_icon_minus);
}

int main(void) {
    init();
    app_event_loop();
    deinit();
}
