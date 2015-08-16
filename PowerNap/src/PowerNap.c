#include <pebble.h>
#include "PowerNap.h"

enum {
    // Key for saving nap minute count
    NAP_TIME_KEY = 6789,
    // Keys for wakeup
    WAKEUP_ID_KEY = 6790,
    WAKEUP_TIME_KEY = 6791,
    WAKEUP_REASON = 1000
};

// Default number of nap minutes
#define NAP_TIME_DEFAULT 20

// Minimum and maximum number of minutes for a nap
#define NAP_TIME_MIN 10
#define NAP_TIME_MAX 90

#define SLEEP_MODE 0
#define WAKE_MODE 1
#define ALARM_MODE 2

// times in milliseconds
#define ONE_MINUTE 60000
#define ONE_HOUR 3600000 // 3 600 000 milliseconds (60 * ONE_MINUTE)
#define VIBRATE_DELAY 2000

static Window *window;

static GBitmap *action_icon_plus, *action_icon_sleep, *action_icon_wake, *action_icon_minus, *alarm_image;

static ActionBarLayer *action_bar;
static TextLayer *header_text_layer, *time_text_layer, *min_text_layer, *remaining_text_layer;
static BitmapLayer *alarm_layer;
static InverterLayer *inverter_layer;

static AppTimer *timer, *alarm;

static uint16_t nap_time = NAP_TIME_DEFAULT;
static uint16_t nap_hour = nap_time / ONE_HOUR;
static uint16_t nap_minute = nap_time % ONE_HOUR; // is this part even necessary??
static uint16_t remaining_nap_time = 0;
static uint16_t mode = WAKE_MODE;
static uint16_t vibrate_count = 0;

static WakeupId s_wakeup_id = -1;

static void update_time() {
    static char body_text[10];
    uint16_t time = (mode == WAKE_MODE) ? nap_time : remaining_nap_time;
    snprintf(body_text, sizeof(body_text), "%u", time);
    text_layer_set_text(time_text_layer, body_text);
    GRect min_frame = layer_get_frame(text_layer_get_layer(min_text_layer));
    GSize text_size = text_layer_get_content_size(time_text_layer);
    layer_set_frame(text_layer_get_layer(min_text_layer),
                    GRect(10 + text_size.w,
                          min_frame.origin.y,
                          min_frame.size.w,
                          min_frame.size.h)
                    );
}

static void increment_click_handler(ClickRecognizerRef recognizer, void *context) {
    if (mode == WAKE_MODE) {
        // Prevent time from going above max
        if (nap_time < NAP_TIME_MAX) {
            nap_time++;
            update_time();
        }
    } else if (mode == ALARM_MODE) {
        set_mode(WAKE_MODE);
    }
}

static void decrement_click_handler(ClickRecognizerRef recognizer, void *context) {
    if (mode == WAKE_MODE) {
        // Prevent time from going below min
        if (nap_time > NAP_TIME_MIN) {
            nap_time--;
            update_time();
        }
    } else if (mode == ALARM_MODE) {
        set_mode(WAKE_MODE);
    }
}

static void sleep_wake_click_handler(ClickRecognizerRef recognizer, void *context) {
    // Toggle the center action bar button to switch between wake and sleep
    if (mode == WAKE_MODE) {
        // Sets the remaining time in nap and starts countdown
        remaining_nap_time = nap_time;
        timer = app_timer_register(ONE_MINUTE, decrease_remaining_time_callback, NULL);

        // Set the wakeup time to be remaining_nap_time minutes in the future
        time_t wakeup_time = time(NULL) + remaining_nap_time * 60;
        s_wakeup_id = wakeup_schedule(wakeup_time, WAKEUP_REASON, true);
        persist_write_int(WAKEUP_ID_KEY, s_wakeup_id);

        set_mode(SLEEP_MODE);
    } else {
        set_mode(WAKE_MODE);
    }
}

static void wakeup_handler(WakeupId id, int32_t reason) {
    persist_delete(WAKEUP_ID_KEY);
    s_wakeup_id = -1;
    if (mode != ALARM_MODE) {
        set_mode(ALARM_MODE);
    }
}

static void decrease_remaining_time_callback(void *data) {
    remaining_nap_time--;

    if (remaining_nap_time > 0) {
        // Still time remaining, restart the minute timer
        timer = app_timer_register(ONE_MINUTE, decrease_remaining_time_callback, NULL);
    } else {
        // Timer ran out, start the alarm
        if (mode != ALARM_MODE) {
            set_mode(ALARM_MODE);
        }
    }

    update_time();
}

static void vibrate() {
    vibes_long_pulse();
}

static void vibrate_callback(void *data) {
    vibrate();
    vibrate_count++;

    if (vibrate_count < ONE_MINUTE / VIBRATE_DELAY) {
        alarm = app_timer_register(VIBRATE_DELAY, vibrate_callback, NULL);
    } else {
        set_mode(WAKE_MODE);
    }
}

static void set_mode(uint16_t new_mode) {
    mode = new_mode;

    if (mode == SLEEP_MODE) {

        // Show sun icon in action bar
        action_bar_layer_set_icon(action_bar, BUTTON_ID_SELECT, action_icon_wake);
        // Black background
        layer_set_hidden(inverter_layer_get_layer(inverter_layer), false);
        // Remove time increment/decrement buttons
        action_bar_layer_clear_icon(action_bar, BUTTON_ID_UP);
        action_bar_layer_clear_icon(action_bar, BUTTON_ID_DOWN);
        // Show action bar
        layer_set_hidden(action_bar_layer_get_layer(action_bar), false);
        // Show "remaining" and all other text layers
        layer_set_hidden(text_layer_get_layer(header_text_layer), false);
        layer_set_hidden(text_layer_get_layer(time_text_layer), false);
        layer_set_hidden(text_layer_get_layer(min_text_layer), false);
        layer_set_hidden(text_layer_get_layer(remaining_text_layer), false);
        // Hide alarm image
        layer_set_hidden(bitmap_layer_get_layer(alarm_layer), true);

        update_time();

    } else if (mode == WAKE_MODE) {

        // Show moon icon in action bar
        action_bar_layer_set_icon(action_bar, BUTTON_ID_SELECT, action_icon_sleep);
        // White background
        layer_set_hidden(inverter_layer_get_layer(inverter_layer), true);
        // Show time increment/decrement buttons
        action_bar_layer_set_icon(action_bar, BUTTON_ID_UP, action_icon_plus);
        action_bar_layer_set_icon(action_bar, BUTTON_ID_DOWN, action_icon_minus);
        // Show action bar
        layer_set_hidden(action_bar_layer_get_layer(action_bar), false);
        // Hide "remaining", show all other text layers
        layer_set_hidden(text_layer_get_layer(header_text_layer), false);
        layer_set_hidden(text_layer_get_layer(time_text_layer), false);
        layer_set_hidden(text_layer_get_layer(min_text_layer), false);
        layer_set_hidden(text_layer_get_layer(remaining_text_layer), true);
        // Hide alarm image
        layer_set_hidden(bitmap_layer_get_layer(alarm_layer), true);
        // Stops any timers
        app_timer_cancel(timer);
        app_timer_cancel(alarm);
        // Cancel the wakeup
        wakeup_cancel_all();
        s_wakeup_id = -1;
        persist_delete(WAKEUP_ID_KEY);

        update_time();

    } else if (mode == ALARM_MODE) {

        // White background
        layer_set_hidden(inverter_layer_get_layer(inverter_layer), true);
        // Hide action bar
        layer_set_hidden(action_bar_layer_get_layer(action_bar), true);
        // Hide all text layers
        layer_set_hidden(text_layer_get_layer(header_text_layer), true);
        layer_set_hidden(text_layer_get_layer(time_text_layer), true);
        layer_set_hidden(text_layer_get_layer(min_text_layer), true);
        layer_set_hidden(text_layer_get_layer(remaining_text_layer), true);
        // Show alarm image
        layer_set_hidden(bitmap_layer_get_layer(alarm_layer), false);
        // Start repeating vibration
        vibrate_count = 0;
        vibrate();
        alarm = app_timer_register(VIBRATE_DELAY, vibrate_callback, NULL);

    }
}

static void click_config_provider(void *context) {
    // Increment/decrement time can be held down to quickly change
    const uint16_t repeat_interval_ms = 35;
    window_single_repeating_click_subscribe(BUTTON_ID_UP, repeat_interval_ms, (ClickHandler) increment_click_handler);
    window_single_repeating_click_subscribe(BUTTON_ID_DOWN, repeat_interval_ms, (ClickHandler) decrement_click_handler);

    window_single_click_subscribe(BUTTON_ID_SELECT, (ClickHandler) sleep_wake_click_handler);
}

static void window_load(Window *me) {
    action_bar = action_bar_layer_create();
    action_bar_layer_add_to_window(action_bar, me);
    action_bar_layer_set_click_config_provider(action_bar, click_config_provider);

    Layer *layer = window_get_root_layer(me);
    const int16_t window_width = layer_get_frame(layer).size.w;
    const int16_t window_height = layer_get_frame(layer).size.h;
    const int16_t width = window_width - ACTION_BAR_WIDTH - 3;

    header_text_layer = text_layer_create(GRect(4, 0, width, 60));
    text_layer_set_font(header_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28));
    text_layer_set_background_color(header_text_layer, GColorClear);
    text_layer_set_text(header_text_layer, "Power Nap");
    layer_add_child(layer, text_layer_get_layer(header_text_layer));

    time_text_layer = text_layer_create(GRect(4, 40, width, 60));
    text_layer_set_font(time_text_layer, fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD));
    text_layer_set_background_color(time_text_layer, GColorClear);
    layer_add_child(layer, text_layer_get_layer(time_text_layer));

    min_text_layer = text_layer_create(GRect(56, 54, width, 60));
    text_layer_set_font(min_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
    text_layer_set_background_color(min_text_layer, GColorClear);
    text_layer_set_text(min_text_layer, "min");
    layer_add_child(layer, text_layer_get_layer(min_text_layer));

    remaining_text_layer = text_layer_create(GRect(4, 40 + 42, width, 60));
    text_layer_set_font(remaining_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24));
    text_layer_set_background_color(remaining_text_layer, GColorClear);
    text_layer_set_text(remaining_text_layer, "remaining");
    layer_add_child(layer, text_layer_get_layer(remaining_text_layer));

    alarm_layer = bitmap_layer_create(GRect(0, 0, window_width, window_height));
    bitmap_layer_set_bitmap(alarm_layer, alarm_image);
    layer_add_child(layer, bitmap_layer_get_layer(alarm_layer));

    inverter_layer = inverter_layer_create(GRect(0, 0, window_width, window_height));
    layer_add_child(layer, inverter_layer_get_layer(inverter_layer));

    update_time();
    set_mode(mode);
}

static void window_unload(Window *window) {
    text_layer_destroy(header_text_layer);
    text_layer_destroy(time_text_layer);
    text_layer_destroy(min_text_layer);
    text_layer_destroy(remaining_text_layer);

    action_bar_layer_destroy(action_bar);

    inverter_layer_destroy(inverter_layer);

    bitmap_layer_destroy(alarm_layer);
}

static void init(void) {
    action_icon_plus = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_ACTION_ICON_PLUS);
    action_icon_sleep = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_ACTION_ICON_SLEEP);
    action_icon_wake = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_ACTION_ICON_WAKE);
    action_icon_minus = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_ACTION_ICON_MINUS);
    alarm_image = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_ALARM);

    window = window_create();
    window_set_window_handlers(window, (WindowHandlers) {
        .load = window_load,
        .unload = window_unload,
    });

    // Get the count from persistent storage for use if it exists, otherwise use the default
    nap_time = persist_exists(NAP_TIME_KEY) ? persist_read_int(NAP_TIME_KEY) : NAP_TIME_DEFAULT;
    if (nap_time < NAP_TIME_MIN) {
        nap_time = NAP_TIME_MIN;
    } else if (nap_time > NAP_TIME_MAX) {
        nap_time = NAP_TIME_MAX;
    }

    // Check if we have already scheduled a wakeup event
    if (persist_exists(WAKEUP_ID_KEY)) {
        s_wakeup_id = persist_read_int(WAKEUP_ID_KEY);
        // query if event is still valid, otherwise delete
        time_t wakeup_time;
        if (wakeup_query(s_wakeup_id, &wakeup_time)) {
            // Restart the countdown
            uint16_t remaining_sec = wakeup_time - time(NULL);
            uint16_t sec_past_min = remaining_sec % 60;
            uint16_t whole_min = remaining_sec / 60;
            if (sec_past_min > 0) {
                remaining_nap_time = whole_min + 1;
                timer = app_timer_register(sec_past_min * 1000, decrease_remaining_time_callback, NULL);
            }  else {
                remaining_nap_time = whole_min;
                timer = app_timer_register(ONE_MINUTE, decrease_remaining_time_callback, NULL);
            }
            mode = SLEEP_MODE;
        } else {
            persist_delete(WAKEUP_ID_KEY);
            s_wakeup_id = -1;
        }
    }

    window_stack_push(window, true /* Animated */);

    // If the app is launched from a wakeup, call the wakeup handler
    if (launch_reason() == APP_LAUNCH_WAKEUP) {
        WakeupId id = 0;
        int32_t reason = 0;
        if (wakeup_get_launch_event(&id, &reason)) {
            wakeup_handler(id, reason);
        }
    }

    // Subscribe to Wakeup API
    wakeup_service_subscribe(wakeup_handler);
}

static void deinit(void) {
    // Save the count into persistent storage on app exit
    persist_write_int(NAP_TIME_KEY, nap_time);

    window_destroy(window);

    gbitmap_destroy(action_icon_plus);
    gbitmap_destroy(action_icon_sleep);
    gbitmap_destroy(action_icon_wake);
    gbitmap_destroy(action_icon_minus);
    gbitmap_destroy(alarm_image);
}

int main(void) {
    init();
    app_event_loop();
    deinit();
}
