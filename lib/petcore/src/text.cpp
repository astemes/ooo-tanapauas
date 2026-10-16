#include "text.h"

#include <cstdio>

namespace petcore {

void slot_name(uint8_t slot, char* buf, size_t len) {
  if (slot == kNoSlot) std::snprintf(buf, len, "Someone");
  else std::snprintf(buf, len, "Player %u", slot + 1u);
}

bool describe_event(const Event& e, const Game& game, char* buf, size_t len) {
  char who[16];
  slot_name(e.slot, who, sizeof who);
  unsigned mins = game.session_minutes();
  switch (e.type) {
    case EventType::ArmingStarted:
      std::snprintf(buf, len, "Phones down! Countdown started.");
      return true;
    case EventType::ArmingCancelled:
      std::snprintf(buf, len, "Countdown cancelled.");
      return true;
    case EventType::SessionStarted:
      std::snprintf(buf, len, "Session started. Don't touch those phones!");
      return true;
    case EventType::PlayerJoined:
      std::snprintf(buf, len, "%s joined the session.", who);
      return true;
    case EventType::Evolved:
      std::snprintf(buf, len, "Your pet evolved into a %s!", stage_name(static_cast<Stage>(e.value)));
      return true;
    case EventType::TargetReached:
      std::snprintf(buf, len, "Target of %u min reached! Picking up now ends it happily.",
                    static_cast<unsigned>(e.value));
      return true;
    case EventType::EmergencyArmed:
      std::snprintf(buf, len, "Emergency pass: one player may lift within %u s.",
                    static_cast<unsigned>(e.value / 1000));
      return true;
    case EventType::EmergencyUsed:
      std::snprintf(buf, len, "%s used the emergency pass.", who);
      return true;
    case EventType::EmergencyExpired:
      std::snprintf(buf, len, "Emergency pass not used; you still have it.");
      return true;
    case EventType::Hatched:
      std::snprintf(buf, len, "A new egg appeared. Generation %u.", static_cast<unsigned>(e.value));
      return true;
    case EventType::SessionEnded:
      switch (static_cast<ResultKind>(e.value)) {
        case ResultKind::Success:
          std::snprintf(buf, len, "Success! %u min phone-free, +%u bonus XP.", mins,
                        static_cast<unsigned>(game.session_bonus_xp()));
          return true;
        case ResultKind::Aborted:
          std::snprintf(buf, len, "Stopped early after %u min. No reward, no penalty.", mins);
          return true;
        case ResultKind::Broken:
          if (e.slot == kNoSlot)
            std::snprintf(buf, len, "Someone pulled the plug! That counts as picking up a phone.");
          else
            std::snprintf(buf, len, "%s picked up their phone after %u min! Everyone loses.", who, mins);
          return true;
        case ResultKind::Died:
          std::snprintf(buf, len, "%s picked up their phone... and the pet died.", who);
          return true;
        default:
          return false;
      }
  }
  return false;
}

}  // namespace petcore
