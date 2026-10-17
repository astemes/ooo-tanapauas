#include "save.h"

#include <cstdio>
#include <cstring>

namespace petcore {

namespace {

constexpr uint8_t kMagic[4] = {'P', 'E', 'T', 'B'};
constexpr size_t kHeaderSize = 8;  // magic + version + payload length

struct Writer {
  uint8_t* buf;
  size_t cap;
  size_t pos = 0;
  bool ok = true;
  void u8(uint8_t v) {
    if (pos + 1 > cap) { ok = false; return; }
    buf[pos++] = v;
  }
  void u16(uint16_t v) { u8(v & 0xFF); u8(v >> 8); }
  void u32(uint32_t v) { u16(v & 0xFFFF); u16(v >> 16); }
};

struct Reader {
  const uint8_t* buf;
  size_t len;
  size_t pos = 0;
  bool ok = true;
  uint8_t u8() {
    if (pos + 1 > len) { ok = false; return 0; }
    return buf[pos++];
  }
  uint16_t u16() { uint16_t lo = u8(); return static_cast<uint16_t>(lo | (u8() << 8)); }
  uint32_t u32() { uint32_t lo = u16(); return lo | (static_cast<uint32_t>(u16()) << 16); }
};

void write_payload(Writer& w, const SaveData& d) {
  w.u32(d.pet.xp);
  w.u8(static_cast<uint8_t>(d.pet.stage));
  w.u8(d.pet.hearts);
  w.u8(d.pet.happiness);
  w.u16(d.pet.generation);
  w.u32(d.pet.minutes);

  w.u32(d.life.total_minutes);
  w.u32(d.life.longest_session_min);
  w.u16(d.life.sessions_success);
  w.u16(d.life.sessions_broken);
  w.u16(d.life.streak);
  w.u16(d.life.best_streak);
  w.u16(d.life.graveyard);
  for (uint16_t b : d.life.breaks) w.u16(b);
  w.u32(d.life.last_seen_unix);

  w.u8(d.session.active ? 1 : 0);
  w.u8(d.session.armed_mask);
  w.u16(d.session.minutes);

  // ---- v2 ----
  for (uint16_t v : d.life.missed_days) w.u16(v);
  for (uint16_t v : d.life.daily_streak) w.u16(v);
  for (uint16_t v : d.life.best_daily_streak) w.u16(v);
  w.u16(d.life.perfect_days);
  w.u16(d.life.bonus_lives);
  w.u8(d.life.shame.slot);
  w.u8(static_cast<uint8_t>(d.life.shame.reason));
  w.u32(d.life.shame.unix);

  const Settings& st = d.settings;
  w.u8(st.setup_done ? 1 : 0);
  w.u8(st.num_players);
  w.u16(st.daily_minimum_min);
  w.u8(st.day_reset_hour);
  w.u16(static_cast<uint16_t>(st.tz_offset_min));
  w.u16(st.pin);
  for (const auto& name : st.names)
    for (int i = 0; i < kNameLen; i++) w.u8(static_cast<uint8_t>(name[i]));

  w.u32(d.daily.day);
  for (uint16_t v : d.daily.minutes) w.u16(v);
  w.u32(d.daily.bonus_day);

  // ---- v3 ----
  w.u8(d.pet.species);
  w.u8(kSpeciesCount);
  for (int s = 0; s < kSpeciesCount; s++) {
    w.u8(d.collection.count[s]);
    w.u8(d.collection.best_stage[s]);
  }

  // ---- v4 ----
  w.u16(d.pet.egg_minutes);
}

void read_payload(Reader& r, SaveData& d) {
  d.pet.xp = r.u32();
  uint8_t stage = r.u8();
  d.pet.stage = stage < static_cast<uint8_t>(Stage::Count) ? static_cast<Stage>(stage) : Stage::Egg;
  d.pet.hearts = r.u8();
  if (d.pet.hearts > kMaxBonusHearts) d.pet.hearts = kMaxBonusHearts;
  d.pet.happiness = r.u8();
  if (d.pet.happiness > kMaxHappiness) d.pet.happiness = kMaxHappiness;
  d.pet.generation = r.u16();
  d.pet.minutes = r.u32();

  d.life.total_minutes = r.u32();
  d.life.longest_session_min = r.u32();
  d.life.sessions_success = r.u16();
  d.life.sessions_broken = r.u16();
  d.life.streak = r.u16();
  d.life.best_streak = r.u16();
  d.life.graveyard = r.u16();
  for (uint16_t& b : d.life.breaks) b = r.u16();
  d.life.last_seen_unix = r.u32();

  d.session.active = r.u8() != 0;
  d.session.armed_mask = r.u8();
  d.session.minutes = r.u16();
}

void read_payload_v2(Reader& r, SaveData& d) {
  for (uint16_t& v : d.life.missed_days) v = r.u16();
  for (uint16_t& v : d.life.daily_streak) v = r.u16();
  for (uint16_t& v : d.life.best_daily_streak) v = r.u16();
  d.life.perfect_days = r.u16();
  d.life.bonus_lives = r.u16();
  d.life.shame.slot = r.u8();
  uint8_t reason = r.u8();
  d.life.shame.reason = reason <= static_cast<uint8_t>(ShameReason::PowerCut) ? static_cast<ShameReason>(reason)
                                                                              : ShameReason::None;
  d.life.shame.unix = r.u32();

  Settings& st = d.settings;
  st.setup_done = r.u8() != 0;
  st.num_players = r.u8();
  st.daily_minimum_min = r.u16();
  st.day_reset_hour = r.u8();
  st.tz_offset_min = static_cast<int16_t>(r.u16());
  st.pin = r.u16();
  for (auto& name : st.names) {
    for (int i = 0; i < kNameLen; i++) name[i] = static_cast<char>(r.u8());
    name[kNameLen] = '\0';
  }

  d.daily.day = r.u32();
  for (uint16_t& v : d.daily.minutes) v = r.u16();
  d.daily.bonus_day = r.u32();
}

void read_payload_v3(Reader& r, SaveData& d) {
  d.pet.species = r.u8();
  if (d.pet.species >= kSpeciesCount) d.pet.species = kNoSpecies;
  int n = r.u8();  // written by a build that may know more species: skip those
  for (int s = 0; s < n; s++) {
    uint8_t count = r.u8(), best = r.u8();
    if (s >= kSpeciesCount) continue;
    d.collection.count[s] = count;
    d.collection.best_stage[s] = best < static_cast<uint8_t>(Stage::Count) ? best : 0;
  }
}

// Before species existed every pet was a Blob, including the ones that died.
void migrate_to_species(SaveData& d) {
  bool hatched = d.pet.stage != Stage::Egg;
  d.pet.species = hatched ? 0 : kNoSpecies;
  uint32_t blobs = d.life.graveyard + (hatched ? 1u : 0u);
  d.collection.count[0] = static_cast<uint8_t>(blobs > 255 ? 255 : blobs);
  if (blobs) d.collection.best_stage[0] = static_cast<uint8_t>(hatched ? d.pet.stage : Stage::Baby);
}

}  // namespace

void settings_sanitize(Settings& s) {
  if (s.num_players < 1 || s.num_players > kMaxSlots) s.num_players = kMaxSlots;
  if (s.daily_minimum_min > 600) s.daily_minimum_min = 600;
  if (s.day_reset_hour > 23) s.day_reset_hour = 4;
  if (s.tz_offset_min < -720 || s.tz_offset_min > 840) s.tz_offset_min = 0;
  if (s.pin > 9999) s.pin = 0;
  for (int i = 0; i < kMaxSlots; i++) {
    char* name = s.names[i];
    name[kNameLen] = '\0';
    for (int c = 0; name[c]; c++)
      if (static_cast<unsigned char>(name[c]) < 0x20 || name[c] == '"' || name[c] == '\\') name[c] = '_';
    if (name[0] == '\0') std::snprintf(name, kNameLen + 1, "Player %d", i + 1);
  }
}

const char* player_name(const Settings& s, uint8_t slot) {
  if (slot >= kMaxSlots) return "Someone";
  return s.names[slot][0] ? s.names[slot] : "Player";
}

uint32_t crc32(const uint8_t* data, size_t len) {
  uint32_t crc = 0xFFFFFFFFu;
  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (int k = 0; k < 8; k++) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
  }
  return ~crc;
}

size_t save_encode(const SaveData& in, uint8_t* buf, size_t len) {
  Writer w{buf, len};
  for (uint8_t m : kMagic) w.u8(m);
  w.u16(kSaveVersion);
  w.u16(0);  // payload length, patched below
  write_payload(w, in);
  if (!w.ok || w.pos + 4 > len) return 0;
  uint16_t payload = static_cast<uint16_t>(w.pos - kHeaderSize);
  buf[6] = payload & 0xFF;
  buf[7] = payload >> 8;
  w.u32(crc32(buf, w.pos));
  return w.ok ? w.pos : 0;
}

bool save_decode(const uint8_t* buf, size_t len, SaveData& out) {
  if (len < kHeaderSize + 4) return false;
  for (int i = 0; i < 4; i++)
    if (buf[i] != kMagic[i]) return false;
  Reader header{buf, len, 4};
  uint16_t version = header.u16();
  if (version < 1 || version > kSaveVersion) return false;
  size_t payload = header.u16();
  size_t total = kHeaderSize + payload + 4;
  if (total > len) return false;
  Reader crc_reader{buf, len, kHeaderSize + payload};
  if (crc_reader.u32() != crc32(buf, kHeaderSize + payload)) return false;

  SaveData d;
  Reader r{buf, kHeaderSize + payload, kHeaderSize};
  read_payload(r, d);
  if (version >= 2) read_payload_v2(r, d);
  if (version >= 3) read_payload_v3(r, d);
  if (version >= 4) {
    d.pet.egg_minutes = r.u16();
    if (d.pet.egg_minutes != 0 && !is_egg_minutes(d.pet.egg_minutes)) d.pet.egg_minutes = kDefaultEggMinutes;
  }
  else migrate_to_species(d);
  if (!r.ok) return false;
  settings_sanitize(d.settings);
  out = d;
  return true;
}

}  // namespace petcore
