// LVGL screens for the GIGA Display Shield (800x480).
#pragma once
#include <game.h>

namespace ui {

void begin(petcore::Game& game, bool storage_ok);
void on_event(const petcore::Event& e);
// Refresh widgets from game state; call every loop before lv_timer_handler().
void update(uint32_t now_ms);

}  // namespace ui
