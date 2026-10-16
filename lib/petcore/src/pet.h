// Pet stats, growth and mood. Pure C++, no Arduino dependencies.
#pragma once
#include <cstdint>

namespace petcore {

constexpr int kMaxSlots = 4;
constexpr uint8_t kNoSlot = 0xFF;  // culprit when nobody in particular (power loss)

enum class Stage : uint8_t { Egg = 0, Baby, Child, Teen, Adult, Elder, Count };
enum class Mood : uint8_t { Happy, Content, Sad, Crying, Dead };

// ---- Tuning -------------------------------------------------------------
constexpr uint8_t kMaxHearts = 3;
constexpr uint8_t kMaxHappiness = 100;
constexpr uint8_t kStartHappiness = 60;
constexpr uint8_t kBreakHappinessLoss = 35;
constexpr uint8_t kSuccessHappinessGain = 10;
constexpr uint8_t kMinutesPerHappinessPoint = 6;
constexpr uint8_t kAbsenceHappinessLossPerDay = 8;
constexpr uint8_t kCompletionBonusPercent = 20;

// XP needed to reach each stage (index = Stage).
constexpr uint32_t kStageXp[] = {0, 30, 150, 500, 1500, 4000};

struct Pet {
  uint32_t xp = 0;
  Stage stage = Stage::Egg;
  uint8_t hearts = kMaxHearts;
  uint8_t happiness = kStartHappiness;
  uint16_t generation = 1;
  uint32_t minutes = 0;  // minutes this pet has lived through in sessions
};

// Stats that survive a pet's death.
struct Lifetime {
  uint32_t total_minutes = 0;
  uint32_t longest_session_min = 0;
  uint16_t sessions_success = 0;
  uint16_t sessions_broken = 0;
  uint16_t streak = 0;
  uint16_t best_streak = 0;
  uint16_t graveyard = 0;
  uint16_t breaks[kMaxSlots] = {};  // shame leaderboard
  uint32_t last_seen_unix = 0;      // 0 = unknown wall-clock time
};

const char* stage_name(Stage s);
Stage stage_for_xp(uint32_t xp);
// XP needed for the next stage, or 0 at the final stage.
uint32_t next_stage_xp(Stage s);

// XP per session minute for a given number of docked players.
uint32_t xp_per_minute(int players);

// Returns true if the pet evolved to a new stage.
bool pet_add_xp(Pet& pet, uint32_t xp);
void pet_on_success(Pet& pet, uint32_t minutes);
// Returns true if the pet died.
bool pet_on_break(Pet& pet);
void pet_hatch_new(Pet& pet, uint16_t generation);
void pet_apply_absence(Pet& pet, uint32_t days);
Mood pet_mood(const Pet& pet);

}  // namespace petcore
