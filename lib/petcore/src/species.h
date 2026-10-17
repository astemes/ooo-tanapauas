// Animals an egg can hatch into. Each egg rolls its species when it hatches,
// weighted by rarity; the box counts every species it has raised (the
// collection), and the companion app turns those into collectible cards.
#pragma once
#include <cstdint>

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

// Egg time settings. Longer eggs mean longer sessions and rarer pets: hatch
// odds per rarity tier in percent, split evenly within a tier.
struct EggKind {
  uint16_t minutes;
  uint8_t weight[static_cast<int>(Rarity::Count)];
};
constexpr EggKind kEggs[] = {
    {15, {75, 20, 5, 0}},
    {30, {55, 28, 14, 3}},
    {60, {40, 32, 22, 6}},
    {90, {25, 33, 30, 12}},
};
constexpr int kEggCount = sizeof(kEggs) / sizeof(kEggs[0]);
constexpr uint16_t kDefaultEggMinutes = 30;

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
