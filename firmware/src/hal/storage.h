// Persists the pet to the GIGA's QSPI flash (user-data partition 4, the
// layout created by the core's QSPIFormat example).
#pragma once
#include <save.h>

namespace storage {
// Returns false if the flash could not be mounted; the game then runs
// without persistence and says so on screen.
bool begin();
// Prints the external flash's MBR partition table over Serial (read-only).
void print_partitions();
bool available();
bool load(petcore::SaveData& out);
bool save(const petcore::SaveData& data);

// Small fixed-size files next to the pet save (Wi-Fi settings, history).
// Writes go through a temp file and rename, like the pet save.
bool read_blob(const char* name, void* buf, size_t len);
bool write_blob(const char* name, const void* buf, size_t len);
}  // namespace storage
