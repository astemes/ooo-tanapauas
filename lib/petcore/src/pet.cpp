#include "pet.h"

namespace petcore {

static uint8_t clamp_happiness(int v) {
  if (v < 0) return 0;
  if (v > kMaxHappiness) return kMaxHappiness;
  return static_cast<uint8_t>(v);
}

const char* stage_name(Stage s) {
  switch (s) {
    case Stage::Egg: return "Egg";
    case Stage::Baby: return "Baby";
    case Stage::Child: return "Child";
    case Stage::Teen: return "Teen";
    case Stage::Adult: return "Adult";
    case Stage::Elder: return "Elder";
    default: return "?";
  }
}

Stage stage_for_xp(uint32_t xp) {
  int s = 0;
  while (s + 1 < static_cast<int>(Stage::Count) && xp >= kStageXp[s + 1]) s++;
  return static_cast<Stage>(s);
}

uint32_t next_stage_xp(Stage s) {
  int next = static_cast<int>(s) + 1;
  return next < static_cast<int>(Stage::Count) ? kStageXp[next] : 0;
}

uint32_t xp_per_minute(int players) {
  return players > 0 ? kXpPerMinute : 0;
}

bool pet_add_xp(Pet& pet, uint32_t xp) {
  pet.xp += xp;
  Stage s = stage_for_xp(pet.xp);
  if (s != pet.stage) {
    pet.stage = s;
    return true;
  }
  return false;
}

void pet_on_success(Pet& pet, uint32_t minutes) {
  int healed = pet.hearts + kHeartsHealedPerSuccess;
  if (pet.hearts < kMaxHearts) pet.hearts = static_cast<uint8_t>(healed < kMaxHearts ? healed : kMaxHearts);
  pet.happiness = clamp_happiness(pet.happiness + kSuccessHappinessGain +
                                  static_cast<int>(minutes / kMinutesPerHappinessPoint));
}

bool pet_on_break(Pet& pet) {
  pet.hearts = pet.hearts > kHeartsLostPerBreak ? static_cast<uint8_t>(pet.hearts - kHeartsLostPerBreak) : 0;
  pet.happiness = clamp_happiness(pet.happiness - kBreakHappinessLoss);
  return pet.hearts == 0;
}

bool pet_add_bonus_heart(Pet& pet) {
  if (pet.hearts == 0 || pet.hearts >= kMaxBonusHearts) return false;
  pet.hearts++;
  return true;
}

void pet_hatch_new(Pet& pet, uint16_t generation) {
  uint16_t egg_minutes = pet.egg_minutes;
  pet = Pet{};
  pet.generation = generation;
  pet.egg_minutes = egg_minutes;
}

void pet_apply_absence(Pet& pet, uint32_t days) {
  int loss = days > 100 ? kMaxHappiness : static_cast<int>(days * kAbsenceHappinessLossPerDay);
  pet.happiness = clamp_happiness(pet.happiness - loss);
}

Mood pet_mood(const Pet& pet) {
  if (pet.hearts == 0) return Mood::Dead;
  if (pet.happiness >= kHappyMoodAt) return Mood::Happy;
  if (pet.happiness >= kContentMoodAt) return Mood::Content;
  return Mood::Sad;
}

}  // namespace petcore
