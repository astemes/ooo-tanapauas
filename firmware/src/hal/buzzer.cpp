#include "buzzer.h"

#include <Arduino.h>

#include "../config.h"

using namespace petcore;

namespace buzzer {

void begin() {
  if (kBuzzerEnabled) pinMode(kBuzzerPin, OUTPUT);
}

static void beep(unsigned int hz, unsigned long ms) {
  if (kBuzzerEnabled) tone(kBuzzerPin, hz, ms);
}

void click() { beep(2000, 15); }

void on_event(const Event& e) {
  switch (e.type) {
    case EventType::ArmingStarted: beep(880, 80); break;
    case EventType::SessionStarted: beep(1320, 200); break;
    case EventType::PlayerJoined: beep(1100, 80); break;
    case EventType::Evolved: beep(1760, 400); break;
    case EventType::TargetReached: beep(1568, 500); break;
    case EventType::Hatched: beep(1200, 250); break;
    case EventType::SessionEnded: {
      auto kind = static_cast<ResultKind>(e.value);
      if (kind == ResultKind::Broken || kind == ResultKind::Died) beep(180, 900);
      else if (kind == ResultKind::Success) beep(1760, 300);
      break;
    }
    default: break;
  }
}

}  // namespace buzzer
