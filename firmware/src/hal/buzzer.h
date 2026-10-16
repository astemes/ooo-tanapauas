// Short sound effects for game events (non-blocking tone()).
#pragma once
#include <game.h>

namespace buzzer {
void begin();
void on_event(const petcore::Event& e);
void click();
}  // namespace buzzer
