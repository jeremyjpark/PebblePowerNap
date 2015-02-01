#ifndef POWER_NAP_HEADER
#define POWER_NAP_HEADER

static void update_time();
static void increment_click_handler(ClickRecognizerRef recognizer, void *context);
static void decrement_click_handler(ClickRecognizerRef recognizer, void *context);
static void sleep_wake_click_handler(ClickRecognizerRef recognizer, void *context);
static void decrease_remaining_time_callback(void *data);
static void set_mode(int new_mode);
static void click_config_provider(void *context);

static void window_load(Window *me);
static void window_unload(Window *window);
static void init(void);
static void deinit(void);

#endif