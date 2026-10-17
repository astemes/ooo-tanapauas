#include "species.h"

namespace petcore {

namespace {

// Order is the save format and the API's species_index: only append.
// Head features (ears, horns, ...) are kSpeciesFeature in sprites.cpp.
const SpeciesInfo kSpecies[kSpeciesCount] = {
    {"Blob", Rarity::Common, 0x000000, 0xFFE066},
    {"Chick", Rarity::Common, 0xFFD84D, 0xFF9A3C},
    {"Bunny", Rarity::Common, 0xEDE6F2, 0xFF8FA3},
    {"Frog", Rarity::Common, 0x7ED957, 0xF7F4EE},
    {"Cat", Rarity::Uncommon, 0xA7A1B2, 0xFF8FA3},
    {"Bear", Rarity::Uncommon, 0xB07A45, 0xE8C090},
    {"Pig", Rarity::Uncommon, 0xFFA8C0, 0xE0708F},
    {"Fox", Rarity::Rare, 0xFF8A3C, 0xF7F4EE},
    {"Panda", Rarity::Rare, 0xF4F1E8, 0x5A5464},
    {"Axolotl", Rarity::Rare, 0xFF9FC8, 0xE0457B},
    {"Deer", Rarity::Rare, 0xC98F5A, 0xF0DDB8},
    {"Dragon", Rarity::Legendary, 0x3FBF8F, 0xFFC94D},
    {"Unicorn", Rarity::Legendary, 0xF1EAFF, 0xFFC94D},
};

}  // namespace

const SpeciesInfo& species_info(uint8_t species) { return kSpecies[species < kSpeciesCount ? species : 0]; }

const char* species_name(uint8_t species) { return species < kSpeciesCount ? kSpecies[species].name : "Egg"; }

const char* rarity_name(Rarity r) {
  switch (r) {
    case Rarity::Common: return "Common";
    case Rarity::Uncommon: return "Uncommon";
    case Rarity::Rare: return "Rare";
    case Rarity::Legendary: return "Legendary";
    default: return "?";
  }
}

const EggKind& egg_kind(uint16_t minutes) {
  for (const EggKind& e : kEggs)
    if (e.minutes == minutes) return e;
  for (const EggKind& e : kEggs)
    if (e.minutes == kDefaultEggMinutes) return e;
  return kEggs[0];
}

bool is_egg_minutes(uint16_t minutes) {
  for (const EggKind& e : kEggs)
    if (e.minutes == minutes) return true;
  return false;
}

int egg_rare_percent(uint16_t minutes) {
  const EggKind& e = egg_kind(minutes);
  return e.weight[static_cast<int>(Rarity::Rare)] + e.weight[static_cast<int>(Rarity::Legendary)];
}

uint8_t roll_species(uint32_t random, uint16_t egg_minutes) {
  const uint8_t* weight = egg_kind(egg_minutes).weight;
  int total = 0;
  for (int t = 0; t < static_cast<int>(Rarity::Count); t++) total += weight[t];
  int r = static_cast<int>(random % static_cast<uint32_t>(total));
  int tier = 0;
  while (tier < static_cast<int>(Rarity::Count) - 1 && r >= weight[tier]) r -= weight[tier++];

  uint8_t members[kSpeciesCount];
  int n = 0;
  for (uint8_t s = 0; s < kSpeciesCount; s++)
    if (static_cast<int>(kSpecies[s].rarity) == tier) members[n++] = s;
  // Use different bits than the tier pick so the two choices are independent.
  return n ? members[(random >> 16) % static_cast<uint32_t>(n)] : 0;
}

}  // namespace petcore
