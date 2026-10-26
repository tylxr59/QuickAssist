#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef void Window;
typedef void TextLayer;
typedef void StatusBarLayer;
typedef void Layer;
typedef void ScrollLayer;
typedef void DictationSession;
typedef struct { bool active; uint32_t due; void (*callback)(void *); void *context; } AppTimer;
typedef void *ClickRecognizerRef;
typedef int AppMessageResult;
typedef int DictationSessionStatus;
enum { APP_MSG_OK, APP_MSG_BUSY, BUTTON_ID_SELECT, BUTTON_ID_UP, BUTTON_ID_DOWN };
enum { DictationSessionStatusSuccess, DictationSessionStatusFailureTranscriptionRejected,
  DictationSessionStatusFailureSystemAborted, DictationSessionStatusFailureNoSpeechDetected };
enum { MESSAGE_KEY_Type, MESSAGE_KEY_RequestId, MESSAGE_KEY_Text, MESSAGE_KEY_Ready,
  MESSAGE_KEY_Assistant, MESSAGE_KEY_ChunkIndex, MESSAGE_KEY_ChunkLast, MESSAGE_KEY_IdleTimeout };
typedef union { uint8_t uint8; uint16_t uint16; uint32_t uint32; char *cstring; } Value;
typedef struct { uint16_t length; Value *value; } Tuple;
typedef struct { Tuple tuples[8]; Value values[8]; bool present[8]; } DictionaryIterator;
static DictionaryIterator out;
static bool outbox_busy, session_available = true;
static int starts, stops, commands, hellos;
static uint32_t last_deadline, now;
static AppTimer timers[128];
static int timer_count, exits, pulses, error_buzzes;
static char command_text[1025], shown[8193];
static void show(const char *text) { snprintf(shown, sizeof(shown), "%s", text); }
static void footer(const char *text) {}
static void text_layer_set_text(TextLayer *layer, const char *text) {}
static void app_timer_cancel(AppTimer *timer) { timer->active = false; }
static AppTimer *app_timer_register(uint32_t ms, void (*callback)(void *), void *context) {
  assert(timer_count < 128);
  AppTimer *timer = &timers[timer_count++];
  *timer = (AppTimer){true, now + ms, callback, context};
  last_deadline = ms; return timer;
}
static void advance(uint32_t ms) {
  now += ms;
  for (int i = 0; i < timer_count; i++) {
    if (timers[i].active && timers[i].due <= now) {
      timers[i].active = false;
      timers[i].callback(timers[i].context);
    }
  }
}
static void window_stack_pop_all(bool animated) { exits++; }
typedef struct { const uint32_t *durations; uint32_t num_segments; } VibePattern;
static void vibes_enqueue_custom_pattern(VibePattern pattern) {
  if (pattern.num_segments == 1) pulses++; else error_buzzes++;
}
static DictationSession *dictation_session_create(uint32_t size, void (*cb)(DictationSession *, DictationSessionStatus, char *, void *), void *ctx) { return session_available ? (DictationSession *)1 : NULL; }
static void dictation_session_enable_confirmation(DictationSession *session, bool enabled) {}
static void dictation_session_enable_error_dialogs(DictationSession *session, bool enabled) {}
static DictationSessionStatus dictation_session_start(DictationSession *session) { starts++; return DictationSessionStatusSuccess; }
static void dictation_session_stop(DictationSession *session) { stops++; }
static AppMessageResult app_message_outbox_begin(DictionaryIterator **it) {
  if (outbox_busy) return APP_MSG_BUSY;
  memset(&out, 0, sizeof(out)); *it = &out; return APP_MSG_OK;
}
static void dict_write_uint32(DictionaryIterator *it, int key, uint32_t value) {
  it->present[key] = true; it->values[key].uint32 = value;
  it->tuples[key].value = &it->values[key];
}
static void dict_write_uint8(DictionaryIterator *it, int key, uint8_t value) { dict_write_uint32(it, key, value); }
static void dict_write_uint16(DictionaryIterator *it, int key, uint16_t value) { dict_write_uint32(it, key, value); }
static void dict_write_cstring(DictionaryIterator *it, int key, const char *text) { snprintf(command_text, sizeof(command_text), "%s", text); }
static AppMessageResult app_message_outbox_send(void) {
  if (out.values[MESSAGE_KEY_Type].uint8 == 0) hellos++;
  if (out.values[MESSAGE_KEY_Type].uint8 == 2) commands++;
  return APP_MSG_OK;
}
static Tuple *dict_find(DictionaryIterator *it, int key) { return it->present[key] ? &it->tuples[key] : NULL; }
static void window_single_click_subscribe(int button, void (*handler)(ClickRecognizerRef, void *)) {}

static void window_raw_click_subscribe(int button, void (*down)(ClickRecognizerRef, void *), void (*up)(ClickRecognizerRef, void *), void *context) {}

/* WATCH_CODE */

static void config(bool ready, uint32_t revision) {
  DictionaryIterator it = {0};
  dict_write_uint8(&it, MESSAGE_KEY_Type, CONFIG);
  dict_write_uint8(&it, MESSAGE_KEY_Ready, ready);
  dict_write_uint32(&it, MESSAGE_KEY_RequestId, revision);
  inbox(&it, NULL);
}
static void reset(void) {
  s_ready = s_busy = s_listening = s_auto_started = s_received_config = s_complete = false;
  s_timer = s_start_timer = s_idle_timer = NULL; s_idle_seconds = 30;
  now = timer_count = exits = pulses = error_buzzes = 0; s_dictation = NULL;
  s_pending[0] = '\0'; s_request = s_config_revision = 0;
  starts = stops = commands = hellos = 0; outbox_busy = false; session_available = true;
}
static void reply_chunk(uint16_t index, bool last) {
  DictionaryIterator it = {0};
  dict_write_uint8(&it, MESSAGE_KEY_Type, CHUNK);
  dict_write_uint32(&it, MESSAGE_KEY_RequestId, s_request);
  dict_write_uint16(&it, MESSAGE_KEY_ChunkIndex, index);
  dict_write_uint8(&it, MESSAGE_KEY_ChunkLast, last);
  it.present[MESSAGE_KEY_Text] = true;
  it.values[MESSAGE_KEY_Text].cstring = "Reply";
  it.tuples[MESSAGE_KEY_Text].value = &it.values[MESSAGE_KEY_Text];
  inbox(&it, NULL);
}
static void reply_error(void) {
  DictionaryIterator it = {0};
  dict_write_uint8(&it, MESSAGE_KEY_Type, ERROR);
  dict_write_uint32(&it, MESSAGE_KEY_RequestId, s_request);
  inbox(&it, NULL);
}
static void begin_command(void) {
  reset(); start_automatic(NULL); config(true, 1);
  dictation_result(NULL, 0, "hello", NULL);
}
static void configure_timeout(uint32_t seconds) {
  DictionaryIterator it = {0};
  dict_write_uint8(&it, MESSAGE_KEY_Type, CONFIG);
  dict_write_uint8(&it, MESSAGE_KEY_Ready, true);
  dict_write_uint32(&it, MESSAGE_KEY_RequestId, 2);
  dict_write_uint32(&it, MESSAGE_KEY_IdleTimeout, seconds);
  inbox(&it, NULL);
}
int main(void) {
  // Readiness while listening must preserve the active session, including duplicates.
  reset(); start_automatic(NULL); assert(s_listening && starts == 1 && !s_received_config);
  config(true, 1); config(true, 1); assert(s_listening && stops == 0 && starts == 1);
  dictation_result(NULL, 0, "hello", NULL);
  assert(commands == 1 && s_busy && last_deadline == 80000);
  config(true, 1); outbox_sent(NULL, NULL); assert(commands == 1 && s_busy);

  // Recognition can finish first; its borrowed string must be copied until ready.
  reset(); start_automatic(NULL);
  char text[] = "turn on lamp";
  dictation_result(NULL, 0, text, NULL); text[0] = 'X';
  assert(commands == 0 && s_pending[0] && last_deadline == 15000);
  select_click(NULL, NULL); assert(starts == 1 && hellos == 0);
  config(true, 1); assert(commands == 1 && strcmp(command_text, "turn on lamp") == 0);
  assert(!s_pending[0]);

  // A busy startup outbox defers submission until its completion callback.
  reset(); start_automatic(NULL); dictation_result(NULL, 0, "hello", NULL);
  outbox_busy = true; config(true, 1); assert(commands == 0 && s_pending[0]);
  outbox_busy = false; outbox_sent(NULL, NULL); outbox_sent(NULL, NULL);
  assert(commands == 1 && !s_pending[0]);

  // A failed startup control message also releases the outbox for queued speech.
  reset(); start_automatic(NULL); dictation_result(NULL, 0, "hello", NULL);
  outbox_busy = true; config(true, 1); outbox_busy = false;
  DictionaryIterator hello = {0}; dict_write_uint8(&hello, MESSAGE_KEY_Type, HELLO);
  outbox_failed(&hello, APP_MSG_BUSY, NULL); assert(commands == 1);
  // A failed command is never replayed by another completion callback.
  outbox_failed(&out, APP_MSG_BUSY, NULL); outbox_sent(NULL, NULL);
  assert(commands == 1 && !s_busy && !s_pending[0]);

  // Timing out drops queued speech so late readiness cannot execute it.
  reset(); start_automatic(NULL); dictation_result(NULL, 0, "hello", NULL);
  timeout(NULL); config(true, 1); assert(commands == 0 && !s_pending[0]);
  assert(starts == 1 && !s_start_timer);
  reset(); start_automatic(NULL); timeout(NULL);
  assert(!s_listening && stops == 1);

  // Cancellation must not auto-reopen dictation when readiness arrives.
  reset(); start_automatic(NULL);
  dictation_result(NULL, DictationSessionStatusFailureTranscriptionRejected, NULL, NULL);
  config(true, 1); assert(commands == 0 && !s_start_timer && starts == 1);

  // Invalid setup and changed settings invalidate both queued speech and dictation.
  reset(); start_automatic(NULL); dictation_result(NULL, 0, "hello", NULL);
  config(false, 1); config(true, 1); assert(commands == 0 && !s_pending[0]);
  reset(); start_automatic(NULL); config(true, 1); config(true, 2);
  assert(stops == 1 && !s_listening);
  reset(); start_automatic(NULL); config(true, 1); outbox_busy = true;
  dictation_result(NULL, 0, "hello", NULL); config(true, 2);
  outbox_busy = false; outbox_sent(NULL, NULL); assert(commands == 0);

  // Retry startup if the phone was initially unavailable for dictation creation.
  reset(); session_available = false; start_automatic(NULL); assert(starts == 0);
  session_available = true; config(true, 1); assert(s_start_timer);
  start_automatic(NULL); assert(starts == 1 && s_listening);
  // The full successful reply starts the default inactivity timer.
  begin_command(); assert(!s_idle_timer);
  reply_chunk(0, false); assert(!s_idle_timer && s_busy && pulses == 0);
  reply_chunk(1, true); assert(s_idle_timer && !s_busy && last_deadline == 30000 && pulses == 1);
  advance(29999); assert(exits == 0);
  advance(1); assert(exits == 1 && !s_idle_timer);

  // Scrolling/button activity extends the deadline; duplicate chunks do not.
  begin_command(); reply_chunk(0, true);
  advance(20000); user_activity(NULL, NULL);
  advance(20000); reply_chunk(0, true); assert(pulses == 1);
  advance(9999); assert(exits == 0);
  advance(1); assert(exits == 1);

  // Following up cancels auto-close through dictation, processing, and errors.
  begin_command(); reply_chunk(0, true); advance(20000);
  select_click(NULL, NULL); assert(s_listening && !s_idle_timer);
  advance(30000); assert(exits == 0);
  dictation_result(NULL, 0, "follow up", NULL); assert(s_busy && !s_idle_timer);
  timeout(NULL); assert(!s_idle_timer && exits == 0 && pulses == 1 && error_buzzes == 1);

  // Failed requests buzz twice; failures before any speech stay silent.
  begin_command(); reply_error(); assert(error_buzzes == 1 && pulses == 0 && !s_busy);
  reset(); start_automatic(NULL); dictation_result(NULL, 0, "hello", NULL);
  timeout(NULL); assert(error_buzzes == 1);
  reset(); timeout(NULL); assert(error_buzzes == 0);
  reset(); start_automatic(NULL); dictation_result(NULL, DictationSessionStatusFailureNoSpeechDetected, NULL, NULL);
  assert(error_buzzes == 0);
  begin_command(); outbox_failed(&out, APP_MSG_BUSY, NULL); assert(error_buzzes == 1 && !s_busy);

  // Settings changes cancel the old timer and apply a validated duration.
  begin_command(); reply_chunk(0, true); configure_timeout(60);
  assert(!s_idle_timer && s_idle_seconds == 60);
  dictate(); dictation_result(NULL, 0, "hello", NULL); reply_chunk(0, true);
  advance(59999); assert(exits == 0);
  advance(1); assert(exits == 1);
  reset(); configure_timeout(0); assert(s_idle_seconds == 30);
  reset(); configure_timeout(301); assert(s_idle_seconds == 30);
  reset(); configure_timeout(42); assert(s_idle_seconds == 30);
  reset(); configure_timeout(5); assert(s_idle_seconds == 5);
  reset(); configure_timeout(300); assert(s_idle_seconds == 300);
  return 0;
}
