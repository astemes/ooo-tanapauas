#include "ui.h"

#include <Arduino.h>
#include <lvgl.h>
#include <sprites.h>
#include <text.h>

#include <cstdio>
#include <cstring>

#include "../config.h"
#include "../hal/buzzer.h"

using namespace petcore;

namespace ui {

namespace {

constexpr int kScale = 16;                      // sprite pixel -> screen pixels
constexpr int kPetPx = kSpriteSize * kScale;    // 256
constexpr int kHeartScale = 4;
constexpr int kHeartPx = kHeartSize * kHeartScale;
constexpr uint32_t kBg = 0x1E1B24;
constexpr uint32_t kPanel = 0x2A2632;
constexpr uint32_t kSlotEmpty = 0x3A3542;
constexpr uint32_t kSlotDocked = 0x2F6FB0;
constexpr uint32_t kSlotLocked = 0x3E9B3A;
constexpr uint32_t kSlotLifting = 0xD6334F;
const uint16_t kTargets[] = {0, 15, 30, 60, 90};
const char* kTargetMap[] = {"Free", "15", "30", "60", "90", ""};

Game* g = nullptr;

lv_color_t pet_buf[kPetPx * kPetPx];
lv_color_t heart_buf[kMaxHearts][kHeartPx * kHeartPx];

lv_obj_t* pet_canvas;
lv_obj_t* heart_canvas[kMaxHearts];
lv_obj_t* title_label;
lv_obj_t* xp_bar;
lv_obj_t* xp_label;
lv_obj_t* happy_bar;
lv_obj_t* message_label;

lv_obj_t* idle_panel;
lv_obj_t* arming_panel;
lv_obj_t* running_panel;
lv_obj_t* result_panel;
lv_obj_t* stats_panel;

lv_obj_t* arming_count;
lv_obj_t* timer_label;
lv_obj_t* session_label;
lv_obj_t* target_bar;
lv_obj_t* emergency_btn;
lv_obj_t* emergency_label;
lv_obj_t* finish_label;
lv_obj_t* result_title;
lv_obj_t* result_text;
lv_obj_t* stats_text;
lv_obj_t* slot_box[kMaxSlots];
lv_obj_t* slot_label[kMaxSlots];
uint32_t slot_colour[kMaxSlots];

bool stats_open = false;
uint32_t last_frame_at = 0;
uint32_t frame = 0;
int drawn_stage = -1, drawn_mood = -1, drawn_hearts = -1;
uint32_t drawn_frame = UINT32_MAX;

// ---- helpers --------------------------------------------------------------

void set_text(lv_obj_t* label, const char* text) {
  if (std::strcmp(lv_label_get_text(label), text) != 0) lv_label_set_text(label, text);
}

void set_hidden(lv_obj_t* obj, bool hidden) {
  if (hidden) lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
}

void set_bg(lv_obj_t* obj, uint32_t rgb) { lv_obj_set_style_bg_color(obj, lv_color_hex(rgb), 0); }

lv_obj_t* make_panel(lv_obj_t* parent) {
  lv_obj_t* p = lv_obj_create(parent);
  lv_obj_set_size(p, 440, 300);
  lv_obj_set_pos(p, 340, 84);
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
                      lv_obj_t** label_out = nullptr) {
  lv_obj_t* b = lv_btn_create(parent);
  lv_obj_set_size(b, 190, 64);
  set_bg(b, rgb);
  lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* l = make_label(b, &lv_font_montserrat_20, text);
  lv_obj_center(l);
  if (label_out) *label_out = l;
  return b;
}

void fill_scaled(const uint8_t* src, int size, int scale, Stage stage, lv_color_t* dst) {
  int stride = size * scale;
  for (int y = 0; y < size; y++) {
    for (int x = 0; x < size; x++) {
      uint8_t idx = src[y * size + x];
      lv_color_t c = lv_color_hex(idx == kTransparent ? kBg : palette_rgb(stage, idx));
      lv_color_t* row = dst + (y * scale) * stride + x * scale;
      for (int dy = 0; dy < scale; dy++)
        for (int dx = 0; dx < scale; dx++) row[dy * stride + dx] = c;
    }
  }
}

// ---- button callbacks -------------------------------------------------------

void on_start(lv_event_t*) {
  buzzer::click();
  if (!g->start(millis())) set_text(message_label, "Dock at least one phone first.");
}
void on_cancel(lv_event_t*) { buzzer::click(); g->cancel(); }
void on_emergency(lv_event_t*) { buzzer::click(); g->request_emergency(millis()); }
void on_finish(lv_event_t*) { buzzer::click(); g->finish(millis()); }
void on_ok(lv_event_t*) { buzzer::click(); g->acknowledge(); }
void on_stats(lv_event_t*) { buzzer::click(); stats_open = !stats_open; }
void on_target(lv_event_t* e) {
  lv_obj_t* m = lv_event_get_target(e);
  uint16_t i = lv_btnmatrix_get_selected_btn(m);
  if (i < sizeof(kTargets) / sizeof(kTargets[0])) g->set_target_minutes(kTargets[i]);
}

// ---- screens --------------------------------------------------------------

void build_header(lv_obj_t* scr) {
  title_label = make_label(scr, &lv_font_montserrat_28, "");
  lv_obj_set_pos(title_label, 24, 20);

  for (int i = 0; i < kMaxHearts; i++) {
    heart_canvas[i] = lv_canvas_create(scr);
    lv_canvas_set_buffer(heart_canvas[i], heart_buf[i], kHeartPx, kHeartPx, LV_IMG_CF_TRUE_COLOR);
    lv_obj_set_pos(heart_canvas[i], 340 + i * (kHeartPx + 8), 20);
  }

  lv_obj_t* happy = make_label(scr, &lv_font_montserrat_14, "Happiness");
  lv_obj_set_pos(happy, 480, 14);
  happy_bar = lv_bar_create(scr);
  lv_obj_set_size(happy_bar, 140, 14);
  lv_obj_set_pos(happy_bar, 480, 36);
  lv_bar_set_range(happy_bar, 0, kMaxHappiness);

  xp_label = make_label(scr, &lv_font_montserrat_14, "");
  lv_obj_set_pos(xp_label, 640, 14);
  xp_bar = lv_bar_create(scr);
  lv_obj_set_size(xp_bar, 140, 14);
  lv_obj_set_pos(xp_bar, 640, 36);
}

void build_idle() {
  idle_panel = make_panel(lv_scr_act());
  lv_obj_t* t = make_label(idle_panel, &lv_font_montserrat_28, "Dock your phones");
  lv_obj_align(t, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_t* sub = make_label(idle_panel, &lv_font_montserrat_14, "Session target (minutes):");
  lv_obj_align(sub, LV_ALIGN_TOP_LEFT, 0, 50);

  lv_obj_t* m = lv_btnmatrix_create(idle_panel);
  lv_btnmatrix_set_map(m, kTargetMap);
  lv_btnmatrix_set_btn_ctrl_all(m, LV_BTNMATRIX_CTRL_CHECKABLE);
  lv_btnmatrix_set_one_checked(m, true);
  lv_btnmatrix_set_btn_ctrl(m, 0, LV_BTNMATRIX_CTRL_CHECKED);
  lv_obj_set_size(m, 400, 70);
  lv_obj_align(m, LV_ALIGN_TOP_LEFT, 0, 74);
  lv_obj_add_event_cb(m, on_target, LV_EVENT_VALUE_CHANGED, nullptr);

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
  timer_label = make_label(running_panel, &lv_font_montserrat_48, "00:00:00");
  lv_obj_align(timer_label, LV_ALIGN_TOP_MID, 0, 0);
  session_label = make_label(running_panel, &lv_font_montserrat_20, "");
  lv_obj_align(session_label, LV_ALIGN_TOP_MID, 0, 70);
  target_bar = lv_bar_create(running_panel);
  lv_obj_set_size(target_bar, 400, 16);
  lv_obj_align(target_bar, LV_ALIGN_TOP_MID, 0, 110);

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
  stats_text = make_label(stats_panel, &lv_font_montserrat_20, "");
  lv_obj_align(stats_text, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_t* back = make_button(stats_panel, "Back", on_stats, 0x5A5560);
  lv_obj_align(back, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
}

void build_slots(lv_obj_t* scr) {
  for (int s = 0; s < kMaxSlots; s++) {
    slot_box[s] = lv_obj_create(scr);
    lv_obj_set_size(slot_box[s], 178, 64);
    lv_obj_set_pos(slot_box[s], 24 + s * 190, 400);
    lv_obj_set_style_border_width(slot_box[s], 0, 0);
    lv_obj_set_style_radius(slot_box[s], 12, 0);
    lv_obj_clear_flag(slot_box[s], LV_OBJ_FLAG_SCROLLABLE);
    slot_colour[s] = UINT32_MAX;
    slot_label[s] = make_label(slot_box[s], &lv_font_montserrat_20, "");
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
  if (stage != drawn_stage || mood != drawn_mood || frame != drawn_frame) {
    Sprite sp;
    render_pet(pet.stage, g->mood(), frame, sp);
    fill_scaled(&sp[0][0], kSpriteSize, kScale, pet.stage, pet_buf);
    lv_obj_invalidate(pet_canvas);
    drawn_stage = stage;
    drawn_mood = mood;
    drawn_frame = frame;
  }
  if (pet.hearts != drawn_hearts) {
    for (int i = 0; i < kMaxHearts; i++) {
      HeartSprite h;
      render_heart(i < pet.hearts, h);
      fill_scaled(&h[0][0], kHeartSize, kHeartScale, pet.stage, heart_buf[i]);
      lv_obj_invalidate(heart_canvas[i]);
    }
    drawn_hearts = pet.hearts;
  }
}

void refresh_header() {
  const Pet& pet = g->data().pet;
  char buf[64];
  std::snprintf(buf, sizeof buf, "%s  -  gen %u", stage_name(pet.stage), pet.generation);
  set_text(title_label, buf);
  lv_bar_set_value(happy_bar, pet.happiness, LV_ANIM_OFF);

  uint32_t lo = kStageXp[static_cast<int>(pet.stage)];
  uint32_t hi = next_stage_xp(pet.stage);
  if (hi) {
    std::snprintf(buf, sizeof buf, "XP %lu / %lu", (unsigned long)pet.xp, (unsigned long)hi);
    lv_bar_set_range(xp_bar, 0, static_cast<int32_t>(hi - lo));
    lv_bar_set_value(xp_bar, static_cast<int32_t>(pet.xp - lo), LV_ANIM_OFF);
  } else {
    std::snprintf(buf, sizeof buf, "XP %lu (max)", (unsigned long)pet.xp);
    lv_bar_set_range(xp_bar, 0, 1);
    lv_bar_set_value(xp_bar, 1, LV_ANIM_OFF);
  }
  set_text(xp_label, buf);
}

void refresh_slots(uint32_t now_ms) {
  bool blink = (now_ms / 250) % 2;
  for (int s = 0; s < kMaxSlots; s++) {
    uint8_t bit = 1u << s;
    uint32_t colour = kSlotEmpty;
    const char* state = "empty";
    if (g->lifting_mask() & bit) {
      colour = blink ? kSlotLifting : kSlotEmpty;
      state = "PUT IT BACK!";
    } else if (g->armed_mask() & bit) {
      colour = kSlotLocked;
      state = "locked";
    } else if (g->docked_mask() & bit) {
      colour = kSlotDocked;
      state = "docked";
    }
    if (g->state() == State::Result && g->culprit() == s) {
      colour = kSlotLifting;
      state = "CULPRIT";
    }
    if (slot_colour[s] != colour) {  // restyling every loop would force a redraw
      set_bg(slot_box[s], colour);
      slot_colour[s] = colour;
    }
    char buf[32];
    std::snprintf(buf, sizeof buf, "%d  %s", s + 1, state);
    set_text(slot_label[s], buf);
  }
}

void refresh_panels(uint32_t now_ms) {
  State st = g->state();
  if (st != State::Idle) stats_open = false;
  set_hidden(idle_panel, st != State::Idle || stats_open);
  set_hidden(stats_panel, !(st == State::Idle && stats_open));
  set_hidden(arming_panel, st != State::Arming);
  set_hidden(running_panel, st != State::Running);
  set_hidden(result_panel, st != State::Result);
  char buf[256];

  switch (st) {
    case State::Idle:
      if (stats_open) {
        const Lifetime& l = g->data().life;
        std::snprintf(buf, sizeof buf,
                      "Streak: %u (best %u)\nSessions won: %u   broken: %u\n"
                      "Phone-free: %lu h %lu min\nLongest: %lu min   Graves: %u\n"
                      "Shame:  P1 %u   P2 %u   P3 %u   P4 %u",
                      l.streak, l.best_streak, l.sessions_success, l.sessions_broken,
                      (unsigned long)(l.total_minutes / 60), (unsigned long)(l.total_minutes % 60),
                      (unsigned long)l.longest_session_min, l.graveyard, l.breaks[0], l.breaks[1],
                      l.breaks[2], l.breaks[3]);
        set_text(stats_text, buf);
      }
      break;

    case State::Arming:
      std::snprintf(buf, sizeof buf, "%lu", (unsigned long)((g->arming_remaining_ms(now_ms) + 999) / 1000));
      set_text(arming_count, buf);
      break;

    case State::Running: {
      uint32_t s = g->session_elapsed_ms(now_ms) / 1000;
      std::snprintf(buf, sizeof buf, "%02lu:%02lu:%02lu", (unsigned long)(s / 3600),
                    (unsigned long)((s / 60) % 60), (unsigned long)(s % 60));
      set_text(timer_label, buf);

      int players = 0;
      for (uint8_t m = g->armed_mask(); m; m &= m - 1) players++;
      if (g->target_minutes())
        std::snprintf(buf, sizeof buf, "+%lu XP  -  %d players  -  goal %u min%s",
                      (unsigned long)g->session_xp(), players, g->target_minutes(),
                      g->target_reached() ? " done!" : "");
      else
        std::snprintf(buf, sizeof buf, "+%lu XP  -  %d players  -  free play",
                      (unsigned long)g->session_xp(), players);
      set_text(session_label, buf);

      set_hidden(target_bar, g->target_minutes() == 0);
      if (g->target_minutes()) {
        lv_bar_set_range(target_bar, 0, g->target_minutes());
        uint32_t m = g->session_minutes();
        lv_bar_set_value(target_bar, m > g->target_minutes() ? g->target_minutes() : m, LV_ANIM_OFF);
      }

      if (g->emergency_window_open()) {
        std::snprintf(buf, sizeof buf, "Lift now! %lus",
                      (unsigned long)(g->emergency_remaining_ms(now_ms) / 1000));
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
      break;
    }
  }
}

}  // namespace

void begin(Game& game, bool storage_ok) {
  g = &game;
  lv_disp_t* disp = lv_disp_get_default();
  lv_theme_t* theme = lv_theme_default_init(disp, lv_palette_main(LV_PALETTE_TEAL),
                                            lv_palette_main(LV_PALETTE_PINK), true,
                                            &lv_font_montserrat_20);
  lv_disp_set_theme(disp, theme);

  lv_obj_t* scr = lv_scr_act();
  set_bg(scr, kBg);
  lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

  build_header(scr);
  pet_canvas = lv_canvas_create(scr);
  lv_canvas_set_buffer(pet_canvas, pet_buf, kPetPx, kPetPx, LV_IMG_CF_TRUE_COLOR);
  lv_obj_set_pos(pet_canvas, 40, 76);

  message_label = make_label(scr, &lv_font_montserrat_14, "");
  lv_label_set_long_mode(message_label, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(message_label, 300);
  lv_obj_set_pos(message_label, 24, 344);

  build_idle();
  build_arming();
  build_running();
  build_result();
  build_stats();
  build_slots(scr);

  set_text(message_label, storage_ok ? "Dock your phones and press Start."
                                     : "Flash not ready: progress will NOT be saved. See docs/hardware.md.");
  // A power-loss break may already be waiting from Game::begin().
  update(millis());
}

void on_event(const Event& e) {
  char buf[128];
  if (!describe_event(e, *g, buf, sizeof buf)) return;
  set_text(message_label, buf);
  if (e.type == EventType::SessionEnded) set_text(result_text, buf);
}

void update(uint32_t now_ms) {
  refresh_header();
  refresh_pet(now_ms);
  refresh_slots(now_ms);
  refresh_panels(now_ms);
}

}  // namespace ui
