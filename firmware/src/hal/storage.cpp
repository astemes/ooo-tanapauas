#include "storage.h"

#include <Arduino.h>
#include <BlockDevice.h>
#include <FATFileSystem.h>
#include <LittleFileSystem.h>
#include <MBRBlockDevice.h>

#include <cstdio>

using namespace petcore;

namespace storage {

namespace {
constexpr int kUserPartition = 4;
const char* kSavePath = "/pet/pet.sav";
const char* kTempPath = "/pet/pet.tmp";

mbed::MBRBlockDevice* user_part = nullptr;
mbed::FileSystem* fs = nullptr;  // mounted at "/pet"
bool mounted = false;

template <typename FS>
bool try_mount(bool format) {
  FS* candidate = new FS("pet");
  int err = format ? candidate->reformat(user_part) : candidate->mount(user_part);
  if (err == 0) {
    fs = candidate;
    return true;
  }
  delete candidate;  // frees the "pet" mount name for the next attempt
  return false;
}

bool read_file(const char* path, SaveData& out) {
  FILE* f = fopen(path, "rb");
  if (!f) return false;
  uint8_t buf[kSaveBufferSize];
  size_t n = fread(buf, 1, sizeof buf, f);
  fclose(f);
  return save_decode(buf, n, out);
}
}  // namespace

bool begin() {
  mbed::BlockDevice* root = mbed::BlockDevice::get_default_instance();
  user_part = new mbed::MBRBlockDevice(root, kUserPartition);
  if (user_part->init() != 0) {
    Serial.println("storage: no partition 4, run the QSPIFormat example once");
    return false;
  }
  // QSPIFormat formats the user partition as FAT or LittleFS; accept either.
  mounted = try_mount<mbed::FATFileSystem>(false) || try_mount<mbed::LittleFileSystem>(false);
  if (!mounted && try_mount<mbed::FATFileSystem>(true)) {
    Serial.println("storage: formatted user partition as FAT");
    mounted = true;
  }
  if (!mounted) Serial.println("storage: mount failed, running without saves");
  return mounted;
}

bool available() { return mounted; }

bool load(SaveData& out) {
  if (!mounted) return false;
  // A crash between writing the temp file and renaming leaves a valid temp.
  return read_file(kSavePath, out) || read_file(kTempPath, out);
}

bool save(const SaveData& data) {
  if (!mounted) return false;
  uint8_t buf[kSaveBufferSize];
  size_t n = save_encode(data, buf, sizeof buf);
  if (n == 0) return false;

  FILE* f = fopen(kTempPath, "wb");
  if (!f) return false;
  bool ok = fwrite(buf, 1, n, f) == n;
  ok = (fflush(f) == 0) && ok;
  fclose(f);
  if (!ok) return false;
  remove(kSavePath);
  return rename(kTempPath, kSavePath) == 0;
}

}  // namespace storage
