// Persists the pet to the GIGA's QSPI flash (user-data partition 4, the
// layout created by the core's QSPIFormat example).
#pragma once
#include <save.h>

namespace storage {
// Returns false if the flash could not be mounted; the game then runs
// without persistence and says so on screen.
bool begin();
bool available();
bool load(petcore::SaveData& out);
bool save(const petcore::SaveData& data);
}  // namespace storage
