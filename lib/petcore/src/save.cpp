#include "save.h"

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
}

void read_payload(Reader& r, SaveData& d) {
  d.pet.xp = r.u32();
  uint8_t stage = r.u8();
  d.pet.stage = stage < static_cast<uint8_t>(Stage::Count) ? static_cast<Stage>(stage) : Stage::Egg;
  d.pet.hearts = r.u8();
  if (d.pet.hearts > kMaxHearts) d.pet.hearts = kMaxHearts;
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

}  // namespace

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
  if (header.u16() != kSaveVersion) return false;
  size_t payload = header.u16();
  size_t total = kHeaderSize + payload + 4;
  if (total > len) return false;
  Reader crc_reader{buf, len, kHeaderSize + payload};
  if (crc_reader.u32() != crc32(buf, kHeaderSize + payload)) return false;

  SaveData d;
  Reader r{buf, kHeaderSize + payload, kHeaderSize};
  read_payload(r, d);
  if (!r.ok) return false;
  out = d;
  return true;
}

}  // namespace petcore
