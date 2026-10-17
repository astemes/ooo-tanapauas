#include "history.h"

#include <text.h>

#include <cstring>

#include "hal/storage.h"

using namespace petcore;

namespace history {

namespace {
const char* kFile = "history.bin";
constexpr uint32_t kMagic = 0x48495354;  // "HIST"

struct Persisted {
  uint32_t magic;
  uint16_t head;  // next write index
  uint16_t count;
  DayRecord days[kDays];
};

Persisted store;
LoggedEvent events[kEvents];
int event_head = 0;
int event_count = 0;
uint32_t event_next_id = 1;

DailyProgress before;
}  // namespace

void begin() {
  if (!storage::read_blob(kFile, &store, sizeof store) || store.magic != kMagic || store.count > kDays ||
      store.head >= kDays) {
    std::memset(&store, 0, sizeof store);
    store.magic = kMagic;
  }
}

void before_update(const Game& game) { before = game.data().daily; }

void after_update(const Game& game) {
  const DailyProgress& now = game.data().daily;
  // The day closed (or tracking restarted) during this update: keep the old day.
  if (before.day == 0 || now.day == 0 || now.day == before.day) return;
  DayRecord& rec = store.days[store.head];
  rec.day = before.day;
  std::memcpy(rec.minutes, before.minutes, sizeof rec.minutes);
  store.head = (store.head + 1) % kDays;
  if (store.count < kDays) store.count++;
  storage::write_blob(kFile, &store, sizeof store);
}

void log_event(const Event& e, const Game& game, uint32_t now_unix) {
  LoggedEvent& slot = events[(event_head + event_count) % kEvents];
  if (event_count == kEvents) event_head = (event_head + 1) % kEvents;
  else event_count++;
  slot.id = event_next_id++;
  slot.at = now_unix;
  slot.event = e;
  if (!describe_event(e, game, slot.text, sizeof slot.text)) slot.text[0] = '\0';
}

int recent_days(DayRecord* out, int max) {
  int n = 0;
  for (int i = 0; i < store.count && n < max; i++) {
    int idx = (store.head - 1 - i + kDays) % kDays;
    out[n++] = store.days[idx];
  }
  return n;
}

int events_since(uint32_t since, const LoggedEvent** out, int max) {
  int n = 0;
  for (int i = 0; i < event_count && n < max; i++) {
    const LoggedEvent& e = events[(event_head + i) % kEvents];
    if (e.id > since) out[n++] = &e;
  }
  return n;
}

uint32_t next_event_id() { return event_next_id; }

}  // namespace history
