// Animals an egg can hatch into. Each egg rolls its species when it hatches,
// weighted by rarity; the box counts every species it has raised (the
// collection), and the companion app turns those into collectible cards.
#pragma once
#include <cstdint>

#include "tuning.h"

namespace petcore {

enum class Rarity : uint8_t { Common, Uncommon, Rare, Legendary, Count };

constexpr uint8_t kSpeciesCount = 13;
constexpr uint8_t kNoSpecies = 0xFF;  // still an egg (or a save from before species)

struct SpeciesInfo {
  const char* name;
  Rarity rarity;
  uint32_t body;    // 0xRRGGBB; 0 = the classic per-stage colours (Blob)
  uint32_t accent;  // ear insides, horns, tufts
};

// Egg sizes and their odds are kEggs in tuning.h.
constexpr int kEggCount = sizeof(kEggs) / sizeof(kEggs[0]);
static_assert(sizeof(kEggs[0].weight) == static_cast<int>(Rarity::Count), "one weight per rarity tier");

// The kind for a time setting (unknown or 0 = the default egg's odds).
const EggKind& egg_kind(uint16_t minutes);
bool is_egg_minutes(uint16_t minutes);
// Percent chance that this egg hatches Rare or Legendary.
int egg_rare_percent(uint16_t minutes);

const SpeciesInfo& species_info(uint8_t species);  // Blob for unknown ids
const char* species_name(uint8_t species);          // "Egg" for kNoSpecies
const char* rarity_name(Rarity r);
// Pick a species from a random 32-bit number, with the egg's odds.
uint8_t roll_species(uint32_t random, uint16_t egg_minutes = kDefaultEggMinutes);

}  // namespace petcore
