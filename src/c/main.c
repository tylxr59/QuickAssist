#include <pebble.h>

#define MAX_TRANSCRIPT 1024
#define MAX_REPLY 8192
#define SCREEN_WIDTH 200
#define TEXT_INSET 16
#define TEXT_WIDTH 168
#define VIEW_TOP 28
#define VIEW_HEIGHT 140
enum { HELLO, CONFIG, COMMAND, CHUNK, ERROR, ACK };
static Window *s_window;
static TextLayer *s_title, *s_text, *s_footer;
static StatusBarLayer *s_status;
static Layer *s_controls;
static ScrollLayer *s_scroll;
static DictationSession *s_dictation;
static AppTimer *s_timer, *s_start_timer, *s_idle_timer;
static uint32_t s_idle_seconds = 30;
static char s_pending[MAX_TRANSCRIPT + 1];
static char s_reply[MAX_REPLY + 1];
static char s_assistant[96] = "QuickAssist";
static uint32_t s_request, s_config_revision;
static uint16_t s_next_chunk;
static bool s_ready, s_busy, s_listening, s_auto_started, s_received_config, s_complete;

static void user_activity(ClickRecognizerRef recognizer, void *context);

static void draw_controls(Layer *layer, GContext *ctx) {
  const int voice_y = 201 - VIEW_TOP;
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_context_set_stroke_width(ctx, 2);
  // Only show button-side scroll cues when there is more to read.
  GPoint offset = scroll_layer_get_content_offset(s_scroll);
  GSize size = scroll_layer_get_content_size(s_scroll);
  if (offset.y < 0) {
    graphics_draw_line(ctx, GPoint(190, 11), GPoint(193, 8));
    graphics_draw_line(ctx, GPoint(193, 8), GPoint(196, 11));
  }
  if (size.h + offset.y > VIEW_HEIGHT) {
    graphics_draw_line(ctx, GPoint(190, VIEW_HEIGHT - 11), GPoint(193, VIEW_HEIGHT - 8));
    graphics_draw_line(ctx, GPoint(193, VIEW_HEIGHT - 8), GPoint(196, VIEW_HEIGHT - 11));
  }
  // A single voice anchor groups the assistant and action below the reply.
  // Static shapes avoid a repeating animation timer on the watch.
  if (s_busy || !s_received_config) {
    for (int x = 23; x <= 43; x += 10) graphics_fill_circle(ctx, GPoint(x, voice_y), 2);
    return;
  }
  if (!s_ready) {
    graphics_draw_round_rect(ctx, GRect(24, voice_y - 14, 18, 28), 3);
    graphics_draw_line(ctx, GPoint(29, voice_y - 10), GPoint(37, voice_y - 10));
    graphics_fill_circle(ctx, GPoint(33, voice_y + 10), 1);
    return;
  }
  const int heights[] = {6, 16, 26, 12, 20, 8};
  for (unsigned int i = 0; i < ARRAY_LENGTH(heights); i++) {
    graphics_fill_rect(ctx, GRect(18 + i * 6, voice_y - heights[i] / 2, 4, heights[i]), 2, GCornersAll);
  }
}
static void redraw(void) {
  layer_mark_dirty(s_controls);
  layer_mark_dirty(status_bar_layer_get_layer(s_status));
}
static void scrolled(ScrollLayer *scroll, void *context) { user_activity(NULL, NULL); redraw(); }
static void show(const char *text) {
  if (text != s_reply) snprintf(s_reply, sizeof(s_reply), "%s", text);
  text_layer_set_text(s_text, s_reply);
  layer_set_frame(text_layer_get_layer(s_text), GRect(TEXT_INSET, 0, TEXT_WIDTH, 30000));
  GSize size = text_layer_get_content_size(s_text);
  // Short responses breathe in the center; long responses start at the top.
  int top = size.h < VIEW_HEIGHT ? (VIEW_HEIGHT - size.h) / 2 : 0;
  int height = size.h + 8;
  layer_set_frame(text_layer_get_layer(s_text), GRect(TEXT_INSET, top, TEXT_WIDTH, height));
  int content_height = top + height > VIEW_HEIGHT ? top + height : VIEW_HEIGHT;
  scroll_layer_set_content_size(s_scroll, GSize(SCREEN_WIDTH, content_height));
  scroll_layer_set_content_offset(s_scroll, GPointZero, false);
  redraw();
}
static void footer(const char *text) {
  text_layer_set_text(s_footer, text);
  redraw();
}
static void cancel_idle_timer(void) {
  if (s_idle_timer) { app_timer_cancel(s_idle_timer); s_idle_timer = NULL; }
}
// One short tap for a reply, two for a failed request, so the result is clear without looking.
static void buzz(bool ok) {
  static const uint32_t success[] = {60}, failure[] = {60, 120, 60};
  vibes_enqueue_custom_pattern(ok ? (VibePattern){.durations = success, .num_segments = 1}
                                  : (VibePattern){.durations = failure, .num_segments = 3});
}
static void idle_timeout(void *context) {
  s_idle_timer = NULL;
  window_stack_pop_all(true);
}
static void restart_idle_timer(void) {
  cancel_idle_timer();
  s_idle_timer = app_timer_register(s_idle_seconds * 1000, idle_timeout, NULL);
}
static void user_activity(ClickRecognizerRef recognizer, void *context) {
  if (s_idle_timer) restart_idle_timer();
}
static void cancel_timer(void) { if (s_timer) { app_timer_cancel(s_timer); s_timer = NULL; } }
static void timeout(void *context) {
  s_timer = NULL;
  bool was_processing = s_busy, had_command = s_busy || s_pending[0];
  s_pending[0] = '\0';
  if (s_listening && s_dictation) dictation_session_stop(s_dictation);
  s_listening = false; s_busy = false;
  s_request++; // Late results must not overwrite this error.
  show(was_processing ? "Request timed out. It may have run. Check Home Assistant before trying again." : "Phone companion unavailable. Check Bluetooth and press the middle button to retry.");
  footer(s_received_config ? "Speak" : "Reconnect");
  if (had_command) buzz(false);
}
static void deadline(uint32_t ms) { cancel_timer(); s_timer = app_timer_register(ms, timeout, NULL); }
static void send_hello(void) {
  DictionaryIterator *it;
  if (app_message_outbox_begin(&it) == APP_MSG_OK) { dict_write_uint8(it, MESSAGE_KEY_Type, HELLO); app_message_outbox_send(); }
}
static void dictate(void);
static void dictation_result(DictationSession *session, DictationSessionStatus status, char *text, void *context);
static void start_automatic(void *context) { s_start_timer = NULL; dictate(); }
static void dictate(void) {
  if (s_busy || s_listening || s_pending[0]) return;
  cancel_idle_timer();
  if (s_received_config && !s_ready) { show("Open QuickAssist settings on your phone to connect your voice assistant."); return; }
  if (!s_dictation) s_dictation = dictation_session_create(0, dictation_result, NULL);
  if (!s_dictation) { show("Dictation unavailable. Check the phone connection and speech service."); return; }
  dictation_session_enable_confirmation(s_dictation, false);
  dictation_session_enable_error_dialogs(s_dictation, false);
  s_listening = true;
  if (dictation_session_start(s_dictation) != DictationSessionStatusSuccess) { s_listening = false; show("Could not start dictation. Check your phone and try again."); }
  else s_auto_started = true;
}
// Retain recognition until CONFIG arrives and any startup outbox message clears.
// Once a command is submitted it is never automatically replayed.
static void send_pending(void) {
  if (!s_pending[0] || !s_ready || s_busy) return;
  DictionaryIterator *it;
  if (app_message_outbox_begin(&it) != APP_MSG_OK) return;
  s_request++; s_next_chunk = 0; s_complete = false;
  dict_write_uint8(it, MESSAGE_KEY_Type, COMMAND);
  dict_write_uint32(it, MESSAGE_KEY_RequestId, s_request);
  dict_write_cstring(it, MESSAGE_KEY_Text, s_pending);
  show(s_pending); s_pending[0] = '\0'; s_busy = true; footer("Thinking…");
  if (app_message_outbox_send() != APP_MSG_OK) { cancel_timer(); s_busy = false; show("Could not send. Check your phone connection."); footer("Speak"); buzz(false); return; }
  deadline(80000);
}
static void dictation_result(DictationSession *session, DictationSessionStatus status, char *text, void *context) {
  s_listening = false;
  if (status != DictationSessionStatusSuccess) {
    show(status == DictationSessionStatusFailureTranscriptionRejected || status == DictationSessionStatusFailureSystemAborted ? "Cancelled. Press the middle button to speak." : status == DictationSessionStatusFailureNoSpeechDetected ? "No speech detected. Press the middle button to try again." : "Speech recognition failed. Check your phone connection and speech service.");
    footer("Speak"); return;
  }
  if (!text || !text[0]) { show("No speech detected. Please try again."); return; }
  if (strlen(text) > MAX_TRANSCRIPT) { show("Command too long. Please try a shorter command."); return; }
  snprintf(s_pending, sizeof(s_pending), "%s", text);
  show(s_pending); footer(s_ready ? "Sending…" : "Connecting…");
  // Bound the wait even if the outbox remains unavailable after readiness.
  deadline(15000);
  send_pending();
}
static void select_click(ClickRecognizerRef recognizer, void *context) {
  if (s_busy || s_listening || s_pending[0]) return;
  if (!s_received_config) { send_hello(); deadline(15000); }
  dictate();
}
static void clicks(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click);
  window_raw_click_subscribe(BUTTON_ID_UP, user_activity, user_activity, NULL);
  window_raw_click_subscribe(BUTTON_ID_DOWN, user_activity, user_activity, NULL);
  window_raw_click_subscribe(BUTTON_ID_SELECT, user_activity, user_activity, NULL);
}
static void acknowledge(uint32_t id, uint16_t index) {
  DictionaryIterator *it;
  if (app_message_outbox_begin(&it) != APP_MSG_OK) return;
  dict_write_uint8(it, MESSAGE_KEY_Type, ACK);
  dict_write_uint32(it, MESSAGE_KEY_RequestId, id);
  dict_write_uint16(it, MESSAGE_KEY_ChunkIndex, index);
  app_message_outbox_send();
}
static void inbox(DictionaryIterator *it, void *context) {
  Tuple *type = dict_find(it, MESSAGE_KEY_Type);
  if (!type) return;
  Tuple *text = dict_find(it, MESSAGE_KEY_Text);
  if (type->value->uint8 == CONFIG) {
    Tuple *ready = dict_find(it, MESSAGE_KEY_Ready), *name = dict_find(it, MESSAGE_KEY_Assistant);
    Tuple *revision = dict_find(it, MESSAGE_KEY_RequestId);
    uint32_t rev = revision ? revision->value->uint32 : 0;
    bool now_ready = ready && ready->value->uint8;
    if (s_received_config && now_ready && s_ready && rev == s_config_revision) return;
    cancel_idle_timer();
    Tuple *idle = dict_find(it, MESSAGE_KEY_IdleTimeout);
    uint32_t seconds = idle ? idle->value->uint32 : 30;
    s_idle_seconds = seconds >= 5 && seconds <= 300 && seconds % 5 == 0 ? seconds : 30;
    bool first_ready = !s_received_config && now_ready;
    s_config_revision = rev;
    s_received_config = true;
    if (!first_ready) {
      s_pending[0] = '\0';
      if (s_listening && s_dictation) dictation_session_stop(s_dictation);
      s_listening = false; s_busy = false; s_request++;
    }
    cancel_timer();
    s_ready = ready && ready->value->uint8;
    snprintf(s_assistant, sizeof(s_assistant), "%s", name && name->length > 1 ? name->value->cstring : "QuickAssist");
    text_layer_set_text(s_title, s_assistant);
    footer("Speak");
    if (!s_ready) { show(text ? text->value->cstring : "Configure QuickAssist on your phone."); footer("Set up on phone"); }
    else if (s_pending[0]) { deadline(15000); send_pending(); }
    else if (s_listening) footer("Listening…");
    else if (!s_auto_started && !s_start_timer) s_start_timer = app_timer_register(100, start_automatic, NULL);
    else show("Press the middle button to speak.");
    return;
  }
  Tuple *id = dict_find(it, MESSAGE_KEY_RequestId);
  if (!id || id->value->uint32 != s_request) return;
  if (type->value->uint8 == ERROR && s_busy) {
    cancel_timer(); s_busy = false; s_request++;
    show(text ? text->value->cstring : "Home Assistant request failed."); footer("Speak"); buzz(false);
  } else if (type->value->uint8 == CHUNK) {
    Tuple *index = dict_find(it, MESSAGE_KEY_ChunkIndex), *last = dict_find(it, MESSAGE_KEY_ChunkLast);
    if (!index || !last || !text) return;
    uint16_t n = index->value->uint16;
    if (n < s_next_chunk) { acknowledge(s_request, n); return; }
    if (!s_busy || s_complete || n != s_next_chunk) return;
    if (n == 0) s_reply[0] = '\0';
    size_t used = strlen(s_reply), added = strlen(text->value->cstring);
    if (used + added > MAX_REPLY) { cancel_timer(); s_busy = false; s_request++; show("Reply exceeded the display limit."); footer("Speak"); buzz(false); return; }
    memcpy(s_reply + used, text->value->cstring, added + 1);
    s_next_chunk++;
    acknowledge(s_request, n);
    if (last->value->uint8) {
      s_complete = true; s_busy = false; cancel_timer(); show(s_reply); footer("Ask again"); restart_idle_timer();
      buzz(true);
    }
  }
}
static void outbox_sent(DictionaryIterator *it, void *context) { send_pending(); }
static void outbox_failed(DictionaryIterator *it, AppMessageResult reason, void *context) {
  Tuple *type = dict_find(it, MESSAGE_KEY_Type);
  if (type && type->value->uint8 == COMMAND && s_busy) {
    cancel_timer(); s_busy = false; s_request++;
    show("Phone delivery failed. The command may have run; check Home Assistant."); footer("Speak"); buzz(false);
  } else send_pending();
}
static void init(void) {
  // Match the system dictation window's palette and clock bar exactly.
  s_window = window_create(); window_set_background_color(s_window, GColorLightGray);
  Layer *root = window_get_root_layer(s_window);
  s_status = status_bar_layer_create();
  status_bar_layer_set_colors(s_status, GColorLightGray, GColorBlack);
  status_bar_layer_set_separator_mode(s_status, StatusBarLayerSeparatorModeNone);
  layer_add_child(root, status_bar_layer_get_layer(s_status));
  s_title = text_layer_create(GRect(60, 179, 124, 24));
  text_layer_set_background_color(s_title, GColorClear); text_layer_set_text_color(s_title, GColorBlack);
  text_layer_set_overflow_mode(s_title, GTextOverflowModeTrailingEllipsis);
  text_layer_set_font(s_title, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD)); text_layer_set_text(s_title, s_assistant);
  layer_add_child(root, text_layer_get_layer(s_title));
  s_scroll = scroll_layer_create(GRect(0, VIEW_TOP, SCREEN_WIDTH, VIEW_HEIGHT));
  scroll_layer_set_shadow_hidden(s_scroll, true);
  s_text = text_layer_create(GRect(TEXT_INSET, 0, TEXT_WIDTH, VIEW_HEIGHT));
  text_layer_set_background_color(s_text, GColorClear); text_layer_set_text_color(s_text, GColorBlack);
  text_layer_set_font(s_text, fonts_get_system_font(FONT_KEY_GOTHIC_24));
  scroll_layer_add_child(s_scroll, text_layer_get_layer(s_text)); layer_add_child(root, scroll_layer_get_layer(s_scroll));
  s_footer = text_layer_create(GRect(60, 202, 124, 20));
  text_layer_set_background_color(s_footer, GColorClear); text_layer_set_text_color(s_footer, GColorBlack);
  text_layer_set_font(s_footer, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD));
  layer_add_child(root, text_layer_get_layer(s_footer));
  s_controls = layer_create(GRect(0, VIEW_TOP, SCREEN_WIDTH, 228 - VIEW_TOP));
  layer_set_update_proc(s_controls, draw_controls);
  layer_add_child(root, s_controls);
  scroll_layer_set_click_config_onto_window(s_scroll, s_window);
  scroll_layer_set_callbacks(s_scroll, (ScrollLayerCallbacks){.click_config_provider = clicks, .content_offset_changed_handler = scrolled});
  window_stack_push(s_window, true);
  show("Connecting to phone…"); footer("Connecting…");
  app_message_register_inbox_received(inbox); app_message_register_outbox_failed(outbox_failed);
  app_message_register_outbox_sent(outbox_sent);
  app_message_open(2048, 2048);
  s_dictation = dictation_session_create(0, dictation_result, NULL);
  send_hello(); deadline(15000);
  s_start_timer = app_timer_register(100, start_automatic, NULL);
}
static void deinit(void) {
  cancel_idle_timer(); cancel_timer(); if (s_start_timer) app_timer_cancel(s_start_timer);
  if (s_dictation) dictation_session_destroy(s_dictation);
  text_layer_destroy(s_text); scroll_layer_destroy(s_scroll); text_layer_destroy(s_title); text_layer_destroy(s_footer);
  layer_destroy(s_controls); status_bar_layer_destroy(s_status); window_destroy(s_window);
}
int main(void) { init(); app_event_loop(); deinit(); return 0; }
