// Versioned, CRC-protected save format. Encoded field by field so the bytes
// are identical on the GIGA and on a desktop host.
#pragma once
#include <cstddef>
#include <cstdint>

#include "pet.h"

namespace petcore {

struct ActiveSession {
  bool active = false;  // still true after a power cut = someone pulled the plug
  uint8_t armed_mask = 0;
  uint16_t minutes = 0;
};

struct SaveData {
  Pet pet;
  Lifetime life;
  ActiveSession session;
};

constexpr uint16_t kSaveVersion = 1;
constexpr size_t kSaveBufferSize = 128;

uint32_t crc32(const uint8_t* data, size_t len);
// Returns the number of bytes written, or 0 if the buffer is too small.
size_t save_encode(const SaveData& in, uint8_t* buf, size_t len);
// Returns false on bad magic, version, length or CRC; `out` is untouched then.
bool save_decode(const uint8_t* buf, size_t len, SaveData& out);

}  // namespace petcore
