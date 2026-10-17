// Day-by-day minutes per player (persisted) and a short event log (RAM),
// served to the companion app.
#pragma once
#include <game.h>

namespace history {

constexpr int kDays = 60;
constexpr int kEvents = 64;
constexpr int kEventText = 112;

struct DayRecord {
  uint32_t day;  // petcore::day_index; 0 = empty
  uint16_t minutes[petcore::kMaxSlots];
};

struct LoggedEvent {
  uint32_t id;
  uint32_t at;  // unix, 0 = unknown
  petcore::Event event;
  char text[kEventText];
};

void begin();
// Call before each Game::update(); records the previous day when it closes.
void before_update(const petcore::Game& game);
void after_update(const petcore::Game& game);
void log_event(const petcore::Event& e, const petcore::Game& game, uint32_t now_unix);

// Closed days, newest first. Returns how many were written to `out`.
int recent_days(DayRecord* out, int max);
// Events with id > since, oldest first.
int events_since(uint32_t since, const LoggedEvent** out, int max);
uint32_t next_event_id();

}  // namespace history
