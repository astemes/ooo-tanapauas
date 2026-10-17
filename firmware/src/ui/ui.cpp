#include "ui.h"

#include <Arduino.h>
#include <lvgl.h>
#include <sprites.h>
#include <text.h>

#include <cstdio>
#include <cstring>

#include "../config.h"
#include "../hal/buzzer.h"
#include "../net/wifi_link.h"

using namespace petcore;

namespace ui {

namespace {

// Layout (800x480): header row; a large pet filling the left column; the
// state panel on the right; slots and the link button along the bottom. The
// link QR opens full screen. Progress bars and daily minutes are in the app.
constexpr int kScale = 18;                      // sprite pixel -> screen pixels
constexpr int kPetPx = kSpriteSize * kScale;    // 288: fills the left column
constexpr int kPetX = (340 - kPetPx) / 2, kPetY = 76;
constexpr int kHeartScale = 4;
constexpr int kHeartPx = kHeartSize * kHeartScale;
constexpr int kQrPx = 340;
constexpr uint32_t kBg = 0x1E1B24;
constexpr uint32_t kPanel = 0x2A2632;
constexpr uint32_t kMuted = 0xA7A1B2;
constexpr uint32_t kShameRed = 0xFF6B83;
constexpr uint32_t kBonusGold = 0xFFC94D;
constexpr uint32_t kGoalGreen = 0x7ED957;
constexpr uint32_t kSlotEmpty = 0x3A3542;
constexpr uint32_t kSlotDocked = 0x2F6FB0;
constexpr uint32_t kSlotLocked = 0x3E9B3A;
constexpr uint32_t kSlotLifting = 0xD6334F;
// Egg sizes, in the order of petcore's kEggs.
const char* kEggMap[] = {"15 min", "30 min", "60 min", "90 min", ""};
const char* kPlayersMap[] = {"1", "2", "3", "4", ""};
const uint16_t kDailyOptions[] = {15, 30, 45, 60, 0};
const char* kDailyMap[] = {"15", "30", "45", "60", "Off", ""};

Game* g = nullptr;
bool storage_ok = true;

lv_color_t pet_buf[kPetPx * kPetPx];
lv_color_t heart_buf[kMaxBonusHearts][kHeartPx * kHeartPx];

lv_obj_t* pet_canvas;
lv_obj_t* heart_canvas[kMaxBonusHearts];
lv_obj_t* title_label;
lv_obj_t* xp_label;
lv_obj_t* happy_label;
lv_obj_t* message_label;
lv_obj_t* qr_btn;
lv_obj_t* qr_overlay;
lv_obj_t* qr;
lv_obj_t* qr_url_label;
lv_obj_t* qr_btn_label;

lv_obj_t* idle_panel;
lv_obj_t* arming_panel;
lv_obj_t* running_panel;
lv_obj_t* result_panel;
lv_obj_t* stats_panel;
lv_obj_t* setup_panel;
lv_obj_t* wifi_panel;

lv_obj_t* shame_label;
lv_obj_t* idle_title;
lv_obj_t* idle_sub;
lv_obj_t* egg_matrix;
lv_obj_t* egg_odds;
int drawn_egg = -1;
lv_obj_t* new_pet_label;
bool new_pet_armed = false;  // "New pet" needs a second tap
uint32_t new_pet_armed_at = 0;
lv_obj_t* arming_count;
lv_obj_t* timer_label;
lv_obj_t* status_label;
lv_obj_t* challenge_label;
lv_obj_t* emergency_btn;
lv_obj_t* emergency_label;
lv_obj_t* finish_label;
lv_obj_t* result_title;
lv_obj_t* result_text;
lv_obj_t* stats_text;
lv_obj_t* slot_box[kMaxSlots];
lv_obj_t* slot_label[kMaxSlots];
uint32_t slot_colour[kMaxSlots];

lv_obj_t* players_matrix;
lv_obj_t* daily_matrix;
lv_obj_t* tz_label;
lv_obj_t* setup_wifi_label;
lv_obj_t* pin_label;
lv_obj_t* ssid_ta;
lv_obj_t* pass_ta;
lv_obj_t* keyboard;
lv_obj_t* wifi_status;

bool stats_open = false;
bool setup_open = false;  // re-running setup from Settings
bool wifi_open = false;
bool qr_open = false;
uint32_t message_until = 0;  // a short hint clears itself; 0 = stays (status)
Settings draft;
char qr_text[32] = "";
uint32_t last_frame_at = 0;
uint32_t frame = 0;
int drawn_stage = -1, drawn_mood = -1, drawn_hearts = -1;
uint32_t drawn_frame = UINT32_MAX;
int drawn_species = -1;

// ---- helpers --------------------------------------------------------------

void set_text(lv_obj_t* label, const char* text) {
  if (std::strcmp(lv_label_get_text(label), text) != 0) lv_label_set_text(label, text);
}

void set_hidden(lv_obj_t* obj, bool hidden) {
  if (hidden == lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN)) return;
  if (hidden) lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
}

void set_bg(lv_obj_t* obj, uint32_t rgb) { lv_obj_set_style_bg_color(obj, lv_color_hex(rgb), 0); }

lv_obj_t* make_panel(lv_obj_t* parent, int x = 340, int y = 84, int w = 440, int h = 300) {
  lv_obj_t* p = lv_obj_create(parent);
  lv_obj_set_size(p, w, h);
  lv_obj_set_pos(p, x, y);
  set_bg(p, kPanel);
  lv_obj_set_style_border_width(p, 0, 0);
  lv_obj_set_style_radius(p, 16, 0);
  lv_obj_clear_flag(p, LV_OBJ_FLAG_SCROLLABLE);
  return p;
}

lv_obj_t* make_label(lv_obj_t* parent, const lv_font_t* font, const char* text) {
  lv_obj_t* l = lv_label_create(parent);
  lv_obj_set_style_text_font(l, font, 0);
  lv_label_set_text(l, text);
  return l;
}

lv_obj_t* make_button(lv_obj_t* parent, const char* text, lv_event_cb_t cb, uint32_t rgb,
                      lv_obj_t** label_out = nullptr, int w = 190, int h = 64) {
  lv_obj_t* b = lv_btn_create(parent);
  lv_obj_set_size(b, w, h);
  set_bg(b, rgb);
  lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* l = make_label(b, &lv_font_montserrat_20, text);
  lv_obj_center(l);
  if (label_out) *label_out = l;
  return b;
}

lv_obj_t* make_matrix(lv_obj_t* parent, const char** map, int checked, lv_event_cb_t cb, int w) {
  lv_obj_t* m = lv_btnmatrix_create(parent);
  lv_btnmatrix_set_map(m, map);
  lv_btnmatrix_set_btn_ctrl_all(m, LV_BTNMATRIX_CTRL_CHECKABLE);
  lv_btnmatrix_set_one_checked(m, true);
  lv_btnmatrix_set_btn_ctrl(m, checked, LV_BTNMATRIX_CTRL_CHECKED);
  lv_obj_set_size(m, w, 58);
  lv_obj_add_event_cb(m, cb, LV_EVENT_VALUE_CHANGED, nullptr);
  return m;
}

void fill_scaled(const uint8_t* src, int size, int scale, uint8_t species, Stage stage, lv_color_t* dst,
                 uint32_t heart_rgb = 0) {
  int stride = size * scale;
  for (int y = 0; y < size; y++) {
    for (int x = 0; x < size; x++) {
      uint8_t idx = src[y * size + x];
      uint32_t rgb = idx == kTransparent ? kBg : (idx == kHeartFull && heart_rgb) ? heart_rgb : palette_rgb(species, stage, idx);
      lv_color_t c = lv_color_hex(rgb);
      lv_color_t* row = dst + (y * scale) * stride + x * scale;
      for (int dy = 0; dy < scale; dy++)
        for (int dx = 0; dx < scale; dx++) row[dy * stride + dx] = c;
    }
  }
}

void format_tz(int16_t minutes, char* buf, size_t len) {
  int a = minutes < 0 ? -minutes : minutes;
  if (a % 60) std::snprintf(buf, len, "UTC%c%d:%02d", minutes < 0 ? '-' : '+', a / 60, a % 60);
  else std::snprintf(buf, len, "UTC%c%d", minutes < 0 ? '-' : '+', a / 60);
}

int index_of(const uint16_t* values, int n, uint16_t v, int fallback) {
  for (int i = 0; i < n; i++)
    if (values[i] == v) return i;
  return fallback;
}

// ---- callbacks ----------------------------------------------------------------

void on_start(lv_event_t*) {
  buzzer::click();
  if (!g->start(millis())) {
    set_text(message_label, "Dock at least one phone first.");
    message_until = millis() + 4000;
  }
}
void on_cancel(lv_event_t*) { buzzer::click(); g->cancel(); }
void on_emergency(lv_event_t*) { buzzer::click(); g->request_emergency(millis()); }
void on_finish(lv_event_t*) { buzzer::click(); g->finish(millis()); }
void on_ok(lv_event_t*) { buzzer::click(); g->acknowledge(); }
void on_stats(lv_event_t*) {
  buzzer::click();
  stats_open = !stats_open;
  new_pet_armed = false;
}
void on_new_pet(lv_event_t*) {
  buzzer::click();
  if (!new_pet_armed) {
    new_pet_armed = true;
    new_pet_armed_at = millis();
    return;
  }
  new_pet_armed = false;
  if (g->new_pet()) stats_open = false;
}
void on_qr_open(lv_event_t*) { buzzer::click(); qr_open = true; }
void on_qr_close(lv_event_t*) { buzzer::click(); qr_open = false; }
void on_egg(lv_event_t* e) {
  uint16_t i = lv_btnmatrix_get_selected_btn(lv_event_get_target(e));
  if (i < kEggCount) g->choose_egg(kEggs[i].minutes);
}

void open_setup() {
  draft = g->data().settings;
  if (draft.pin == 0) draft.pin = 1000 + (micros() ^ (analogRead(A0) << 7)) % 9000;
  lv_btnmatrix_set_btn_ctrl(players_matrix, draft.num_players - 1, LV_BTNMATRIX_CTRL_CHECKED);
  lv_btnmatrix_set_btn_ctrl(daily_matrix, index_of(kDailyOptions, 5, draft.daily_minimum_min, 1),
                            LV_BTNMATRIX_CTRL_CHECKED);
  setup_open = true;
  stats_open = false;
}
void on_settings(lv_event_t*) {
  buzzer::click();
  if (g->state() == State::Idle) open_setup();
}
void on_title_long_press(lv_event_t*) {
  if (g->state() == State::Idle) open_setup();
}
void on_players(lv_event_t* e) {
  draft.num_players = lv_btnmatrix_get_selected_btn(lv_event_get_target(e)) + 1;
}
void on_daily(lv_event_t* e) {
  uint16_t i = lv_btnmatrix_get_selected_btn(lv_event_get_target(e));
  if (i < 5) draft.daily_minimum_min = kDailyOptions[i];
}
void on_tz_minus(lv_event_t*) { if (draft.tz_offset_min > -720) draft.tz_offset_min -= 60; }
void on_tz_plus(lv_event_t*) { if (draft.tz_offset_min < 840) draft.tz_offset_min += 60; }
void on_setup_done(lv_event_t*) {
  buzzer::click();
  if (g->complete_setup(draft)) setup_open = false;
}

void open_wifi(lv_event_t*) {
  buzzer::click();
  lv_textarea_set_text(ssid_ta, wifi_link::credentials().ssid);
  lv_textarea_set_text(pass_ta, "");
  lv_keyboard_set_textarea(keyboard, ssid_ta);
  set_text(wifi_status, "Type your network name and password.");
  wifi_open = true;
}
void on_ta_focus(lv_event_t* e) { lv_keyboard_set_textarea(keyboard, lv_event_get_target(e)); }
void on_wifi_cancel(lv_event_t*) { buzzer::click(); wifi_open = false; }
void on_wifi_connect(lv_event_t*) {
  buzzer::click();
  set_text(wifi_status, "Connecting... (up to 10 s)");
  lv_refr_now(nullptr);  // show it before the blocking connect
  wifi_link::set_credentials(lv_textarea_get_text(ssid_ta), lv_textarea_get_text(pass_ta));
  if (wifi_link::connected()) {
    wifi_open = false;
  } else {
    set_text(wifi_status, "Couldn't connect. Check the name and password (2.4 GHz networks only).");
  }
}

// ---- screens --------------------------------------------------------------

void build_header(lv_obj_t* scr) {
  title_label = make_label(scr, &lv_font_montserrat_28, "");
  lv_obj_set_pos(title_label, 24, 20);
  lv_obj_add_flag(title_label, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(title_label, on_title_long_press, LV_EVENT_LONG_PRESSED, nullptr);

  for (int i = 0; i < kMaxBonusHearts; i++) {
    heart_canvas[i] = lv_canvas_create(scr);
    lv_canvas_set_buffer(heart_canvas[i], heart_buf[i], kHeartPx, kHeartPx, LV_IMG_CF_TRUE_COLOR);
    lv_obj_set_pos(heart_canvas[i], 290 + i * 36, 20);
  }

  // Plain numbers here; the companion app has the progress bars.
  happy_label = make_label(scr, &lv_font_montserrat_20, "");
  lv_obj_set_pos(happy_label, 490, 24);
  xp_label = make_label(scr, &lv_font_montserrat_20, "");
  lv_obj_set_pos(xp_label, 630, 24);
}

void build_left(lv_obj_t* scr) {
  pet_canvas = lv_canvas_create(scr);
  lv_canvas_set_buffer(pet_canvas, pet_buf, kPetPx, kPetPx, LV_IMG_CF_TRUE_COLOR);
  lv_obj_set_pos(pet_canvas, kPetX, kPetY);

  // Bottom-right corner, next to the slot row.
  qr_btn = make_button(scr, "Link phone", on_qr_open, 0x2F6FB0, &qr_btn_label, 146, 64);
  lv_obj_set_pos(qr_btn, 630, 400);

  // One line under the pet, only for status (offline, flash problems) and hints.
  message_label = make_label(scr, &lv_font_montserrat_14, "");
  lv_obj_set_style_text_color(message_label, lv_color_hex(kMuted), 0);
  lv_label_set_long_mode(message_label, LV_LABEL_LONG_DOT);
  lv_obj_set_width(message_label, 312);
  lv_obj_set_pos(message_label, 20, 372);
}

void build_idle() {
  idle_panel = make_panel(lv_scr_act());
  // A fresh egg shows the egg picker; after its first session, the goal it set.
  idle_title = make_label(idle_panel, &lv_font_montserrat_28, "");
  lv_obj_align(idle_title, LV_ALIGN_TOP_LEFT, 0, 0);
  idle_sub = make_label(idle_panel, &lv_font_montserrat_14, "");
  lv_obj_set_style_text_color(idle_sub, lv_color_hex(kMuted), 0);
  lv_obj_align(idle_sub, LV_ALIGN_TOP_LEFT, 0, 42);
  egg_matrix = make_matrix(idle_panel, kEggMap, 1, on_egg, 400);
  lv_obj_align(egg_matrix, LV_ALIGN_TOP_LEFT, 0, 66);
  egg_odds = make_label(idle_panel, &lv_font_montserrat_14, "");
  lv_obj_set_style_text_color(egg_odds, lv_color_hex(kBonusGold), 0);
  lv_obj_align(egg_odds, LV_ALIGN_TOP_LEFT, 0, 132);

  shame_label = make_label(idle_panel, &lv_font_montserrat_20, "");
  lv_obj_set_style_text_color(shame_label, lv_color_hex(kShameRed), 0);
  lv_label_set_long_mode(shame_label, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(shame_label, 400);
  lv_obj_align(shame_label, LV_ALIGN_TOP_LEFT, 0, 76);

  lv_obj_t* start = make_button(idle_panel, "Start", on_start, 0x3E9B3A);
  lv_obj_align(start, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  lv_obj_t* stats = make_button(idle_panel, "Stats", on_stats, 0x5A5560);
  lv_obj_align(stats, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
}

void build_arming() {
  arming_panel = make_panel(lv_scr_act());
  lv_obj_t* t = make_label(arming_panel, &lv_font_montserrat_28, "Starting in");
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 10);
  arming_count = make_label(arming_panel, &lv_font_montserrat_48, "10");
  lv_obj_align(arming_count, LV_ALIGN_CENTER, 0, -20);
  lv_obj_t* hint = make_label(arming_panel, &lv_font_montserrat_14, "Late? Dock your phone now to join.");
  lv_obj_align(hint, LV_ALIGN_CENTER, 0, 30);
  lv_obj_t* cancel = make_button(arming_panel, "Cancel", on_cancel, 0x5A5560);
  lv_obj_align(cancel, LV_ALIGN_BOTTOM_MID, 0, 0);
}

void build_running() {
  running_panel = make_panel(lv_scr_act());
  status_label = make_label(running_panel, &lv_font_montserrat_20, "");
  lv_obj_align(status_label, LV_ALIGN_TOP_MID, 0, -6);
  // Largest built-in font; transform_zoom didn't render on the device.
  timer_label = make_label(running_panel, &lv_font_montserrat_48, "00:00:00");
  lv_obj_align(timer_label, LV_ALIGN_TOP_MID, 0, 44);
  challenge_label = make_label(running_panel, &lv_font_montserrat_20, "");
  lv_obj_set_style_text_color(challenge_label, lv_color_hex(kBonusGold), 0);
  lv_obj_set_style_text_align(challenge_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_long_mode(challenge_label, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(challenge_label, 400);
  lv_obj_align(challenge_label, LV_ALIGN_TOP_MID, 0, 126);

  emergency_btn = make_button(running_panel, "Emergency pass", on_emergency, 0xB5651D, &emergency_label);
  lv_obj_align(emergency_btn, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  lv_obj_t* finish = make_button(running_panel, "Stop early", on_finish, 0x5A5560, &finish_label);
  lv_obj_align(finish, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
}

void build_result() {
  result_panel = make_panel(lv_scr_act());
  result_title = make_label(result_panel, &lv_font_montserrat_48, "");
  lv_obj_align(result_title, LV_ALIGN_TOP_MID, 0, 0);
  result_text = make_label(result_panel, &lv_font_montserrat_20, "");
  lv_label_set_long_mode(result_text, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(result_text, 400);
  lv_obj_set_style_text_align(result_text, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(result_text, LV_ALIGN_TOP_MID, 0, 80);
  lv_obj_t* ok = make_button(result_panel, "OK", on_ok, 0x2F6FB0);
  lv_obj_align(ok, LV_ALIGN_BOTTOM_MID, 0, 0);
}

void build_stats() {
  stats_panel = make_panel(lv_scr_act());
  stats_text = make_label(stats_panel, &lv_font_montserrat_14, "");
  lv_obj_align(stats_text, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_t* settings = make_button(stats_panel, "Settings", on_settings, 0x2F6FB0, nullptr, 126);
  lv_obj_align(settings, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  lv_obj_t* fresh = make_button(stats_panel, "New pet", on_new_pet, 0x8A3B52, &new_pet_label, 126);
  lv_obj_align(fresh, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_t* back = make_button(stats_panel, "Back", on_stats, 0x5A5560, nullptr, 126);
  lv_obj_align(back, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
}

void build_setup() {
  setup_panel = make_panel(lv_scr_act(), 20, 68, 760, 324);
  lv_obj_t* t = make_label(setup_panel, &lv_font_montserrat_28, "Setup");
  lv_obj_align(t, LV_ALIGN_TOP_LEFT, 0, 0);

  lv_obj_t* l1 = make_label(setup_panel, &lv_font_montserrat_20, "Players");
  lv_obj_align(l1, LV_ALIGN_TOP_LEFT, 0, 60);
  players_matrix = make_matrix(setup_panel, kPlayersMap, 3, on_players, 300);
  lv_obj_align(players_matrix, LV_ALIGN_TOP_LEFT, 190, 44);

  lv_obj_t* l2 = make_label(setup_panel, &lv_font_montserrat_20, "Daily minimum");
  lv_obj_align(l2, LV_ALIGN_TOP_LEFT, 0, 126);
  daily_matrix = make_matrix(setup_panel, kDailyMap, 1, on_daily, 360);
  lv_obj_align(daily_matrix, LV_ALIGN_TOP_LEFT, 190, 110);

  lv_obj_t* l3 = make_label(setup_panel, &lv_font_montserrat_20, "Time zone");
  lv_obj_align(l3, LV_ALIGN_TOP_LEFT, 0, 192);
  lv_obj_t* minus = make_button(setup_panel, "-", on_tz_minus, 0x5A5560, nullptr, 56, 48);
  lv_obj_align(minus, LV_ALIGN_TOP_LEFT, 190, 180);
  tz_label = make_label(setup_panel, &lv_font_montserrat_20, "UTC+0");
  lv_obj_align(tz_label, LV_ALIGN_TOP_LEFT, 262, 192);
  lv_obj_t* plus = make_button(setup_panel, "+", on_tz_plus, 0x5A5560, nullptr, 56, 48);
  lv_obj_align(plus, LV_ALIGN_TOP_LEFT, 380, 180);

  lv_obj_t* l4 = make_label(setup_panel, &lv_font_montserrat_20, "Wi-Fi");
  lv_obj_align(l4, LV_ALIGN_TOP_LEFT, 0, 254);
  setup_wifi_label = make_label(setup_panel, &lv_font_montserrat_14, "");
  lv_obj_set_width(setup_wifi_label, 190);
  lv_label_set_long_mode(setup_wifi_label, LV_LABEL_LONG_DOT);
  lv_obj_align(setup_wifi_label, LV_ALIGN_TOP_LEFT, 80, 258);
  lv_obj_t* wifi = make_button(setup_panel, "Change", open_wifi, 0x2F6FB0, nullptr, 140, 48);
  lv_obj_align(wifi, LV_ALIGN_TOP_LEFT, 280, 242);

  pin_label = make_label(setup_panel, &lv_font_montserrat_20, "");
  lv_obj_align(pin_label, LV_ALIGN_TOP_RIGHT, 0, 4);
  lv_obj_t* done = make_button(setup_panel, "Done", on_setup_done, 0x3E9B3A);
  lv_obj_align(done, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
}

void build_wifi() {
  wifi_panel = make_panel(lv_scr_act(), 0, 0, 800, 480);
  lv_obj_set_style_radius(wifi_panel, 0, 0);
  lv_obj_t* t = make_label(wifi_panel, &lv_font_montserrat_28, "Wi-Fi");
  lv_obj_align(t, LV_ALIGN_TOP_LEFT, 0, 0);

  ssid_ta = lv_textarea_create(wifi_panel);
  lv_textarea_set_one_line(ssid_ta, true);
  lv_textarea_set_max_length(ssid_ta, 32);
  lv_textarea_set_placeholder_text(ssid_ta, "Network name");
  lv_obj_set_width(ssid_ta, 360);
  lv_obj_align(ssid_ta, LV_ALIGN_TOP_LEFT, 0, 44);
  lv_obj_add_event_cb(ssid_ta, on_ta_focus, LV_EVENT_FOCUSED, nullptr);

  pass_ta = lv_textarea_create(wifi_panel);
  lv_textarea_set_one_line(pass_ta, true);
  lv_textarea_set_password_mode(pass_ta, true);
  lv_textarea_set_max_length(pass_ta, 64);
  lv_textarea_set_placeholder_text(pass_ta, "Password");
  lv_obj_set_width(pass_ta, 360);
  lv_obj_align(pass_ta, LV_ALIGN_TOP_LEFT, 380, 44);
  lv_obj_add_event_cb(pass_ta, on_ta_focus, LV_EVENT_FOCUSED, nullptr);

  wifi_status = make_label(wifi_panel, &lv_font_montserrat_14, "");
  lv_obj_set_width(wifi_status, 380);
  lv_label_set_long_mode(wifi_status, LV_LABEL_LONG_WRAP);
  lv_obj_align(wifi_status, LV_ALIGN_TOP_LEFT, 0, 104);
  lv_obj_t* connect = make_button(wifi_panel, "Connect", on_wifi_connect, 0x3E9B3A, nullptr, 160, 52);
  lv_obj_align(connect, LV_ALIGN_TOP_LEFT, 400, 96);
  lv_obj_t* cancel = make_button(wifi_panel, "Cancel", on_wifi_cancel, 0x5A5560, nullptr, 160, 52);
  lv_obj_align(cancel, LV_ALIGN_TOP_LEFT, 580, 96);

  keyboard = lv_keyboard_create(wifi_panel);
  lv_obj_set_size(keyboard, 760, 270);
  lv_obj_align(keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
}

// Full-screen link QR; tap anywhere to close.
void build_qr_overlay() {
  qr_overlay = make_panel(lv_scr_act(), 0, 0, 800, 480);
  lv_obj_set_style_radius(qr_overlay, 0, 0);
  set_bg(qr_overlay, kBg);
  lv_obj_add_flag(qr_overlay, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(qr_overlay, on_qr_close, LV_EVENT_CLICKED, nullptr);
  qr = lv_qrcode_create(qr_overlay, kQrPx, lv_color_hex(0x1E1B24), lv_color_hex(0xF4F2F7));
  lv_obj_set_style_border_color(qr, lv_color_hex(0xF4F2F7), 0);
  lv_obj_set_style_border_width(qr, 8, 0);
  lv_obj_align(qr, LV_ALIGN_TOP_MID, 0, 8);
  qr_url_label = make_label(qr_overlay, &lv_font_montserrat_20, "");
  lv_obj_set_style_text_align(qr_url_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(qr_url_label, LV_ALIGN_BOTTOM_MID, 0, -24);
  lv_obj_t* hint = make_label(qr_overlay, &lv_font_montserrat_14, "Tap anywhere to close");
  lv_obj_set_style_text_color(hint, lv_color_hex(kMuted), 0);
  lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, 0);
}

void build_slots(lv_obj_t* scr) {
  for (int s = 0; s < kMaxSlots; s++) {
    slot_box[s] = lv_obj_create(scr);
    lv_obj_set_size(slot_box[s], 140, 64);  // leaves the corner for the link button
    lv_obj_set_pos(slot_box[s], 24 + s * 150, 400);
    lv_obj_set_style_border_width(slot_box[s], 0, 0);
    lv_obj_set_style_radius(slot_box[s], 12, 0);
    lv_obj_clear_flag(slot_box[s], LV_OBJ_FLAG_SCROLLABLE);
    slot_colour[s] = UINT32_MAX;
    slot_label[s] = make_label(slot_box[s], &lv_font_montserrat_20, "");
    lv_obj_set_width(slot_label[s], 128);
    lv_label_set_long_mode(slot_label[s], LV_LABEL_LONG_WRAP);  // "PUT IT BACK!" takes two lines
    lv_obj_set_style_text_align(slot_label[s], LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(slot_label[s]);
  }
}

// ---- refresh ----------------------------------------------------------------

void refresh_pet(uint32_t now_ms) {
  if (now_ms - last_frame_at >= kAnimFrameMs) {
    last_frame_at = now_ms;
    frame++;
  }
  const Pet& pet = g->data().pet;
  int stage = static_cast<int>(pet.stage), mood = static_cast<int>(g->mood());
  if (stage != drawn_stage || mood != drawn_mood || frame != drawn_frame || pet.species != drawn_species) {
    Sprite sp;
    render_pet(pet.species, pet.stage, g->mood(), frame, sp);
    fill_scaled(&sp[0][0], kSpriteSize, kScale, pet.species, pet.stage, pet_buf);
    lv_obj_invalidate(pet_canvas);
    drawn_stage = stage;
    drawn_mood = mood;
    drawn_frame = frame;
    drawn_species = pet.species;
  }
  if (pet.hearts != drawn_hearts) {
    for (int i = 0; i < kMaxBonusHearts; i++) {
      HeartSprite h;
      render_heart(i < pet.hearts, h);
      fill_scaled(&h[0][0], kHeartSize, kHeartScale, pet.species, pet.stage, heart_buf[i], i >= kMaxHearts ? kBonusGold : 0);
      lv_obj_invalidate(heart_canvas[i]);
      set_hidden(heart_canvas[i], i >= kMaxHearts && i >= pet.hearts);  // bonus slots only when earned
    }
    drawn_hearts = pet.hearts;
  }
}

void refresh_header() {
  const Pet& pet = g->data().pet;
  char buf[64];
  if (pet.species < kSpeciesCount)
    std::snprintf(buf, sizeof buf, "%s %s  -  gen %u", species_name(pet.species), stage_name(pet.stage),
                  pet.generation);
  else
    std::snprintf(buf, sizeof buf, "%s  -  gen %u", stage_name(pet.stage), pet.generation);
  set_text(title_label, buf);
  std::snprintf(buf, sizeof buf, "Happy %u%%", pet.happiness);
  set_text(happy_label, buf);
  uint32_t hi = next_stage_xp(pet.stage);
  if (hi) std::snprintf(buf, sizeof buf, "XP %lu/%lu", (unsigned long)pet.xp, (unsigned long)hi);
  else std::snprintf(buf, sizeof buf, "XP %lu", (unsigned long)pet.xp);
  set_text(xp_label, buf);
}

void refresh_left() {
  char buf[64];

  // During a session the left column shows only the pet, centred.
  bool session = g->state() == State::Arming || g->state() == State::Running;
  if (message_until && static_cast<int32_t>(millis() - message_until) > 0) {
    set_text(message_label, "");
    message_until = 0;
  }
  set_hidden(message_label, session);

  const char* ip = wifi_link::ip();
  char want[32] = "";
  if (ip[0]) std::snprintf(want, sizeof want, "http://%s/", ip);
  if (std::strcmp(want, qr_text) != 0) {
    std::strcpy(qr_text, want);
    if (ip[0]) lv_qrcode_update(qr, qr_text, std::strlen(qr_text));
    std::snprintf(buf, sizeof buf, "Scan with your phone camera\n%s", qr_text);
    set_text(qr_url_label, buf);
  }
  if (!ip[0] || session) qr_open = false;
  set_hidden(qr_btn, session);
  set_hidden(qr_overlay, !qr_open);
  set_text(qr_btn_label, ip[0] ? "Link phone" : "No Wi-Fi");
  if (ip[0]) lv_obj_clear_state(qr_btn, LV_STATE_DISABLED);
  else lv_obj_add_state(qr_btn, LV_STATE_DISABLED);
}

void refresh_slots(uint32_t now_ms) {
  bool blink = (now_ms / 250) % 2;
  const Settings& st = g->data().settings;
  for (int s = 0; s < kMaxSlots; s++) {
    set_hidden(slot_box[s], s >= g->num_players());
    uint8_t bit = 1u << s;
    uint32_t colour = kSlotEmpty;
    const char* state = "";
    if (g->lifting_mask() & bit) {
      colour = blink ? kSlotLifting : kSlotEmpty;
      state = "PUT IT BACK!";
    } else if (g->armed_mask() & bit) {
      colour = kSlotLocked;
    } else if (g->docked_mask() & bit) {
      colour = kSlotDocked;
    }
    if (g->state() == State::Result && g->culprit() == s) {
      colour = kSlotLifting;
      state = "CULPRIT";
    }
    if (slot_colour[s] != colour) {  // restyling every loop would force a redraw
      set_bg(slot_box[s], colour);
      slot_colour[s] = colour;
    }
    char buf[40];
    if (state[0]) std::snprintf(buf, sizeof buf, "%d  %s", s + 1, state);
    else std::snprintf(buf, sizeof buf, "%d  %s", s + 1, player_name(st, s));
    set_text(slot_label[s], buf);
  }
}

void refresh_panels(uint32_t now_ms) {
  State st = g->state();
  if (st != State::Idle) stats_open = false;
  if (st != State::Idle && st != State::Setup) setup_open = false;
  bool setup_visible = st == State::Setup || setup_open;
  if (st == State::Setup && !setup_open) open_setup();

  set_hidden(wifi_panel, !wifi_open);
  set_hidden(setup_panel, !setup_visible || wifi_open);
  set_hidden(idle_panel, st != State::Idle || stats_open || setup_visible);
  set_hidden(stats_panel, !(st == State::Idle && stats_open) || setup_visible);
  set_hidden(arming_panel, st != State::Arming);
  set_hidden(running_panel, st != State::Running);
  set_hidden(result_panel, st != State::Result);
  char buf[400];

  if (setup_visible) {
    format_tz(draft.tz_offset_min, buf, sizeof buf);
    set_text(tz_label, buf);
    const char* ssid = wifi_link::credentials().ssid;
    if (wifi_link::connected()) std::snprintf(buf, sizeof buf, "%s (connected)", ssid);
    else if (ssid[0]) std::snprintf(buf, sizeof buf, "%s (offline)", ssid);
    else std::snprintf(buf, sizeof buf, "Not set");
    set_text(setup_wifi_label, buf);
    std::snprintf(buf, sizeof buf, "App PIN: %04u", draft.pin);
    set_text(pin_label, buf);
  }

  switch (st) {
    case State::Setup:
      break;

    case State::Idle: {
      // Egg picker until the egg's first session, then the goal it set.
      bool picking = !g->egg_locked();
      uint16_t egg = g->target_minutes();
      set_hidden(egg_matrix, !picking);
      set_hidden(egg_odds, !picking);
      set_hidden(shame_label, picking);
      if (picking) {
        set_text(idle_title, "Pick an egg");
        set_text(idle_sub, "Longer sessions hatch rarer pets.");
        for (int i = 0; i < kEggCount; i++)
          if (kEggs[i].minutes == egg && drawn_egg != i) {
            lv_btnmatrix_set_btn_ctrl(egg_matrix, i, LV_BTNMATRIX_CTRL_CHECKED);
            drawn_egg = i;
          }
        std::snprintf(buf, sizeof buf, "%u-min sessions  -  %d%% chance of a rare pet", egg, egg_rare_percent(egg));
        set_text(egg_odds, buf);
      } else {
        std::snprintf(buf, sizeof buf, "Goal: %u min", egg);
        set_text(idle_title, buf);
        std::snprintf(buf, sizeof buf, "Set by this egg. Bonus life at %u min.", static_cast<unsigned>(g->bonus_goal_minutes()));
        set_text(idle_sub, egg ? buf : "Free play");
      }
      if (new_pet_armed && millis() - new_pet_armed_at > 4000) new_pet_armed = false;
      set_text(new_pet_label, new_pet_armed ? "Sure?" : "New pet");
      char shame[96];
      if (describe_shame(*g, shame, sizeof shame)) {
        std::snprintf(buf, sizeof buf, "Wall of shame: %s", shame);
        set_text(shame_label, buf);
      } else {
        set_text(shame_label, "");
      }
      if (stats_open) {
        const SaveData& d = g->data();
        const Lifetime& l = d.life;
        int n = std::snprintf(buf, sizeof buf,
                              "Session streak %u (best %u)   Won %u   Broken %u\n"
                              "Phone-free %lu h %lu min   Longest %lu min\n"
                              "Perfect days %u   Bonus lives %u   Graves %u\n",
                              l.streak, l.best_streak, l.sessions_success, l.sessions_broken,
                              (unsigned long)(l.total_minutes / 60), (unsigned long)(l.total_minutes % 60),
                              (unsigned long)l.longest_session_min, l.perfect_days, l.bonus_lives, l.graveyard);
        for (int p = 0; p < g->num_players() && n > 0 && n < static_cast<int>(sizeof buf); p++)
          n += std::snprintf(buf + n, sizeof buf - n, "\n%s: daily streak %u, lifted %u, missed %u",
                             player_name(d.settings, p), l.daily_streak[p], l.breaks[p], l.missed_days[p]);
        set_text(stats_text, buf);
      }
      break;
    }

    case State::Arming:
      std::snprintf(buf, sizeof buf, "%lu", (unsigned long)((g->arming_remaining_ms(now_ms) + 999) / 1000));
      set_text(arming_count, buf);
      break;

    case State::Running: {
      // Phase 1: count down to the goal. Phase 2 (goal complete): count down
      // to double the goal, which earns an extra life. Then show the time
      // past the goal. Free play counts up, with the extra life at an hour.
      uint32_t e = g->session_elapsed_ms(now_ms) / 1000;
      uint32_t goal_m = g->target_minutes(), bonus_m = g->bonus_goal_minutes();
      bool can_win = g->bonus_available_today() && !g->bonus_earned();
      bool done = goal_m && g->target_reached();
      uint32_t shown = e;
      const char* sign = "";
      if (goal_m && !done) shown = e < goal_m * 60 ? goal_m * 60 - e : 0;
      else if (done && can_win && e < bonus_m * 60) shown = bonus_m * 60 - e;
      else if (done) shown = e - goal_m * 60, sign = "+";
      std::snprintf(buf, sizeof buf, "%s%02lu:%02lu:%02lu", sign, (unsigned long)(shown / 3600),
                    (unsigned long)((shown / 60) % 60), (unsigned long)(shown % 60));
      set_text(timer_label, buf);

      if (done) std::snprintf(buf, sizeof buf, "GOAL COMPLETE!");
      else if (goal_m) std::snprintf(buf, sizeof buf, "Goal %lu min", (unsigned long)goal_m);
      else std::snprintf(buf, sizeof buf, "Free play");
      set_text(status_label, buf);
      lv_obj_set_style_text_color(status_label, lv_color_hex(done ? kGoalGreen : kMuted), 0);

      if (g->bonus_earned()) std::snprintf(buf, sizeof buf, "BONUS COMPLETE! Extra life earned.");
      else if (!g->bonus_available_today())
        std::snprintf(buf, sizeof buf, "%s", done ? "Bonus extra life already won today." : "");
      else if (done) std::snprintf(buf, sizeof buf, "BONUS ROUND: keep going to %lu min for an EXTRA LIFE!", (unsigned long)bonus_m);
      else if (goal_m) std::snprintf(buf, sizeof buf, "Bonus: keep going to %lu min for an extra life", (unsigned long)bonus_m);
      else std::snprintf(buf, sizeof buf, "Bonus: reach %lu min for an extra life", (unsigned long)bonus_m);
      set_text(challenge_label, buf);

      if (g->emergency_window_open()) {
        std::snprintf(buf, sizeof buf, "Lift now! %lus", (unsigned long)(g->emergency_remaining_ms(now_ms) / 1000));
        set_text(emergency_label, buf);
      } else {
        set_text(emergency_label, g->emergency_available() ? "Emergency pass" : "Pass used");
      }
      if (g->emergency_available() && !g->emergency_window_open())
        lv_obj_clear_state(emergency_btn, LV_STATE_DISABLED);
      else
        lv_obj_add_state(emergency_btn, LV_STATE_DISABLED);
      set_text(finish_label, g->finish_counts_as_success() ? "Finish" : "Stop early");
      break;
    }

    case State::Result: {
      const char* title = "";
      switch (g->result()) {
        case ResultKind::Success: title = "Well done!"; break;
        case ResultKind::Aborted: title = "Stopped"; break;
        case ResultKind::Broken: title = "BUSTED!"; break;
        case ResultKind::Died: title = "R.I.P."; break;
        default: break;
      }
      set_text(result_title, title);
      if (lv_label_get_text(result_text)[0] == '\0') {  // e.g. after a reboot
        Event e{EventType::SessionEnded, g->culprit(), static_cast<uint32_t>(g->result())};
        if (describe_event(e, *g, buf, sizeof buf)) set_text(result_text, buf);
      }
      break;
    }
  }
}

}  // namespace

void begin(Game& game, bool storage_available) {
  g = &game;
  storage_ok = storage_available;
  lv_disp_t* disp = lv_disp_get_default();
  lv_theme_t* theme = lv_theme_default_init(disp, lv_palette_main(LV_PALETTE_TEAL),
                                            lv_palette_main(LV_PALETTE_PINK), true, &lv_font_montserrat_20);
  lv_disp_set_theme(disp, theme);

  lv_obj_t* scr = lv_scr_act();
  set_bg(scr, kBg);
  lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

  build_header(scr);
  build_left(scr);
  build_idle();
  build_arming();
  build_running();
  build_result();
  build_stats();
  build_slots(scr);
  build_setup();
  build_wifi();
  build_qr_overlay();

  set_text(message_label, storage_ok ? ""
                                     : "Flash not ready: progress will NOT be saved. See docs/hardware.md.");
  update(millis());
}

void show_status(const char* text) {
  set_text(message_label, text);
  lv_refr_now(nullptr);
}

void on_event(const Event& e) {
  char buf[128];
  if (e.type == EventType::SessionStarted) return;  // the timer says it all
  // Event messages show in the app; the box only keeps the result text.
  if (!describe_event(e, *g, buf, sizeof buf)) return;
  if (e.type == EventType::SessionEnded) set_text(result_text, buf);
  if (e.type == EventType::Hatched || e.type == EventType::ArmingStarted) set_text(result_text, "");
}

void update(uint32_t now_ms) {
  refresh_header();
  refresh_pet(now_ms);
  refresh_left();
  refresh_slots(now_ms);
  refresh_panels(now_ms);
}

}  // namespace ui
