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
  if (players <= 0) return 0;
  // Bigger groups are harder to keep off their phones, so reward them more.
  int bonus = players >= 4 ? 2 : players >= 3 ? 1 : 0;
  return static_cast<uint32_t>(players + bonus);
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
  if (pet.hearts < kMaxHearts) pet.hearts++;
  pet.happiness = clamp_happiness(pet.happiness + kSuccessHappinessGain +
                                  static_cast<int>(minutes / kMinutesPerHappinessPoint));
}

bool pet_on_break(Pet& pet) {
  if (pet.hearts > 0) pet.hearts--;
  pet.happiness = clamp_happiness(pet.happiness - kBreakHappinessLoss);
  return pet.hearts == 0;
}

void pet_hatch_new(Pet& pet, uint16_t generation) {
  pet = Pet{};
  pet.generation = generation;
}

void pet_apply_absence(Pet& pet, uint32_t days) {
  int loss = days > 100 ? kMaxHappiness : static_cast<int>(days * kAbsenceHappinessLossPerDay);
  pet.happiness = clamp_happiness(pet.happiness - loss);
}

Mood pet_mood(const Pet& pet) {
  if (pet.hearts == 0) return Mood::Dead;
  if (pet.happiness >= 70) return Mood::Happy;
  if (pet.happiness >= 35) return Mood::Content;
  return Mood::Sad;
}

}  // namespace petcore
