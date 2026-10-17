#include "text.h"

#include <cstdio>

namespace petcore {

bool describe_shame(const Game& game, char* buf, size_t len) {
  const Shame& shame = game.data().life.shame;
  const char* who = player_name(game.data().settings, shame.slot);
  switch (shame.reason) {
    case ShameReason::Lift:
      std::snprintf(buf, len, "%s picked up their phone mid-session.", who);
      return true;
    case ShameReason::MissedDaily:
      if (shame.slot == kNoSlot)
        std::snprintf(buf, len, "The team missed the daily %u min.",
                      static_cast<unsigned>(game.data().settings.daily_minimum_min));
      else
        std::snprintf(buf, len, "%s missed the daily %u min.", who,
                      static_cast<unsigned>(game.data().settings.daily_minimum_min));
      return true;
    case ShameReason::PowerCut:
      std::snprintf(buf, len, "Someone pulled the plug.");
      return true;
    default:
      return false;
  }
}

bool describe_event(const Event& e, const Game& game, char* buf, size_t len) {
  const char* who = player_name(game.data().settings, e.slot);
  unsigned mins = game.session_minutes();
  unsigned daily_min = game.data().settings.daily_minimum_min;
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
    case EventType::Evolved: {
      Stage stage = static_cast<Stage>(e.value);
      uint8_t species = game.data().pet.species;
      Rarity rarity = species_info(species).rarity;
      if (stage == Stage::Baby && rarity >= Rarity::Rare)
        std::snprintf(buf, len, "It hatched! A %s %s!", rarity_name(rarity), species_name(species));
      else if (stage == Stage::Baby)
        std::snprintf(buf, len, "It hatched! Say hi to your new %s.", species_name(species));
      else
        std::snprintf(buf, len, "Your %s grew into a %s!", species_name(species), stage_name(stage));
      return true;
    }
    case EventType::NightStarted:
      std::snprintf(buf, len, "Bedtime! The pet sleeps until %02u:00. Keep the phones away.",
                    static_cast<unsigned>(e.value));
      return true;
    case EventType::Morning:
      std::snprintf(buf, len, "Good morning! The pet is awake.");
      return true;
    case EventType::NewSpecies:
      std::snprintf(buf, len, "New in your collection: %s (%s).", species_name(static_cast<uint8_t>(e.value)),
                    rarity_name(species_info(static_cast<uint8_t>(e.value)).rarity));
      return true;
    case EventType::TargetReached:
      if (game.bonus_available_today())
        std::snprintf(buf, len, "Goal of %u min complete! Keep going to %u min for an extra life.",
                      static_cast<unsigned>(e.value), static_cast<unsigned>(game.bonus_goal_minutes()));
      else
        std::snprintf(buf, len, "Goal of %u min complete! Finish whenever you like.", static_cast<unsigned>(e.value));
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
    case EventType::DailyMissed:
      std::snprintf(buf, len, "%s only managed %u of %u min yesterday. The pet lost a heart!",
                    e.slot == kNoSlot ? "The team" : who, static_cast<unsigned>(e.value), daily_min);
      return true;
    case EventType::DayClosed:
      if (e.value != 0) return false;  // DailyMissed already said who
      if (daily_min == 0) return false;
      std::snprintf(buf, len, "Perfect day! The team hit its %u min.", daily_min);
      return true;
    case EventType::BonusLife:
      std::snprintf(buf, len, "Challenge done: %u min! Extra life: %u hearts.",
                    static_cast<unsigned>(game.bonus_goal_minutes()), static_cast<unsigned>(e.value));
      return true;
    case EventType::SetupDone:
      std::snprintf(buf, len, "All set. Pick an egg, then everyone docks.");
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
          if (game.result_cause() == ShameReason::MissedDaily)
            std::snprintf(buf, len, "%s missed the daily %u min... and the pet died.",
                          e.slot == kNoSlot ? "The team" : who, daily_min);
          else if (e.slot == kNoSlot)
            std::snprintf(buf, len, "Someone pulled the plug... and the pet died.");
          else
            std::snprintf(buf, len, "%s picked up their phone... and the pet died.", who);
          return true;
        default:
          return false;
      }
  }
  return false;
}

}  // namespace petcore
