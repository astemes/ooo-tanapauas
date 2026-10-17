// Versioned, CRC-protected save format. Encoded field by field so the bytes
// are identical on the GIGA and on a desktop host.
#pragma once
#include <cstddef>
#include <cstdint>

#include "pet.h"
#include "species.h"

namespace petcore {

constexpr int kNameLen = 12;

// Chosen on the setup screen; names are set from the companion app.
struct Settings {
  bool setup_done = false;
  uint8_t num_players = kMaxSlots;   // players use slots 0..num_players-1
  uint16_t daily_minimum_min = kDefaultDailyMinimum;  // 0 disables the daily challenge
  uint8_t day_reset_hour = kDefaultDayResetHour;      // local hour when a new day starts
  int16_t tz_offset_min = 0;         // local time = UTC + offset
  uint16_t pin = 0;                  // 4-digit PIN for changes made from the app
  char names[kMaxSlots][kNameLen + 1] = {};
};

// Progress towards today's daily minimum.
struct DailyProgress {
  uint32_t day = 0;  // day index (see day_index()); 0 = no clock yet
  uint16_t minutes[kMaxSlots] = {};
  uint32_t bonus_day = 0;  // day the last bonus life was earned
};

struct ActiveSession {
  bool active = false;  // still true after a power cut = someone pulled the plug
  uint8_t armed_mask = 0;
  uint16_t minutes = 0;
};

// Every species the box has hatched; the companion app shows these as cards.
struct Collection {
  uint8_t count[kSpeciesCount] = {};       // pets of this species hatched (saturates at 255)
  uint8_t best_stage[kSpeciesCount] = {};  // highest Stage one of them reached
};

struct SaveData {
  Pet pet;
  Lifetime life;
  ActiveSession session;
  Settings settings;
  DailyProgress daily;
  Collection collection;
};

// v1: pet, lifetime, session. v2 adds settings, daily progress and shame.
// v3 adds the pet's species and the collection. v4 adds the egg's time setting.
constexpr uint16_t kSaveVersion = 4;
constexpr size_t kSaveBufferSize = 256;

// Clamp settings to valid ranges and fill in default player names.
void settings_sanitize(Settings& s);
// Player's name, or "Someone" for kNoSlot.
const char* player_name(const Settings& s, uint8_t slot);

uint32_t crc32(const uint8_t* data, size_t len);
// Returns the number of bytes written, or 0 if the buffer is too small.
size_t save_encode(const SaveData& in, uint8_t* buf, size_t len);
// Accepts v1 to v4. Returns false on bad magic, version, length or CRC;
// `out` is untouched then. A v1 save loads with default settings and
// setup_done = false so the box asks for the player count once. Pets from
// before v3 were all Blobs, so they load as Blobs in the collection; pets
// from before v4 get the default 30-min egg.
bool save_decode(const uint8_t* buf, size_t len, SaveData& out);

}  // namespace petcore
