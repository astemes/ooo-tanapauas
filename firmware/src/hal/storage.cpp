#include "storage.h"

#include <Arduino.h>
#include <BlockDevice.h>
#include <FATFileSystem.h>
#include <LittleFileSystem.h>
#include <MBRBlockDevice.h>
#include <SlicingBlockDevice.h>

#include <cstdio>

using namespace petcore;

namespace storage {

namespace {
constexpr int kUserPartition = 4;
// Same range QSPIFormat gives partition 4. Used directly when the flash has
// no partition 4, so the partition table (and the Wi-Fi files) stay untouched.
constexpr mbed::bd_addr_t kUserStart = 7 * 1024 * 1024;
constexpr mbed::bd_addr_t kUserEnd = 14 * 1024 * 1024;
const char* kSavePath = "/pet/pet.sav";
const char* kTempPath = "/pet/pet.tmp";

mbed::BlockDevice* user_part = nullptr;
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

// True if every sampled block of the device is erased (0xFF), i.e. unused.
bool looks_blank(mbed::BlockDevice* bd) {
  uint8_t buf[256];
  mbed::bd_size_t size = bd->size();
  for (mbed::bd_addr_t addr = 0; addr < size; addr += 64 * 1024) {
    if (bd->read(buf, addr, sizeof buf) != 0) return false;
    for (uint8_t b : buf)
      if (b != 0xFF) return false;
  }
  return true;
}

bool begin() {
  mbed::BlockDevice* root = mbed::BlockDevice::get_default_instance();
  mbed::MBRBlockDevice* part = new mbed::MBRBlockDevice(root, kUserPartition);
  if (part->init() == 0) {
    // QSPIFormat formats the user partition as FAT or LittleFS; accept either.
    user_part = part;
    mounted = try_mount<mbed::FATFileSystem>(false) || try_mount<mbed::LittleFileSystem>(false);
    if (!mounted && try_mount<mbed::FATFileSystem>(true)) {
      Serial.println("storage: formatted user partition as FAT");
      mounted = true;
    }
  } else {
    delete part;
    if (root->init() != 0) {
      Serial.println("storage: flash init failed");
      return false;
    }
    user_part = new mbed::SlicingBlockDevice(root, kUserStart, kUserEnd);
    if (user_part->init() != 0) {
      Serial.println("storage: flash slice init failed");
      return false;
    }
    mounted = try_mount<mbed::LittleFileSystem>(false);
    if (!mounted) {
      if (looks_blank(user_part) && try_mount<mbed::LittleFileSystem>(true)) {
        Serial.println("storage: formatted blank 7-14 MB area as LittleFS");
        mounted = true;
      } else {
        Serial.println("storage: 7-14 MB area is in use by something else; not touching it");
      }
    }
  }
  if (!mounted) Serial.println("storage: mount failed, running without saves");
  return mounted;
}

bool available() { return mounted; }

void print_partitions() {
  mbed::BlockDevice* root = mbed::BlockDevice::get_default_instance();
  if (root->init() != 0) {
    Serial.println("PART error: flash init failed");
    return;
  }
  Serial.print("PART flash size=");
  Serial.print((unsigned long)(root->size() / 1024));
  Serial.print("KB read=");
  Serial.print((unsigned long)root->get_read_size());
  Serial.print(" erase=");
  Serial.println((unsigned long)root->get_erase_size());
  uint8_t mbr[512];
  if (root->read(mbr, 0, sizeof mbr) != 0) {
    Serial.println("PART error: read failed");
    return;
  }
  Serial.print("PART signature=");
  Serial.println((mbr[510] == 0x55 && mbr[511] == 0xAA) ? "ok" : "none");
  for (int i = 0; i < 4; i++) {
    const uint8_t* e = mbr + 446 + i * 16;
    uint32_t lba = e[8] | (e[9] << 8) | (e[10] << 16) | (uint32_t(e[11]) << 24);
    uint32_t sectors = e[12] | (e[13] << 8) | (e[14] << 16) | (uint32_t(e[15]) << 24);
    Serial.print("PART ");
    Serial.print(i + 1);
    Serial.print(" type=0x");
    Serial.print(e[4], HEX);
    Serial.print(" start=");
    Serial.print((unsigned long)lba * 512 / 1024);
    Serial.print("KB size=");
    Serial.print((unsigned long)sectors * 512 / 1024);
    Serial.println("KB");
  }
  Serial.println("PART END");
}

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

bool read_blob(const char* name, void* buf, size_t len) {
  if (!mounted) return false;
  char path[48];
  snprintf(path, sizeof path, "/pet/%s", name);
  FILE* f = fopen(path, "rb");
  if (!f) return false;
  bool ok = fread(buf, 1, len, f) == len;
  fclose(f);
  return ok;
}

bool write_blob(const char* name, const void* buf, size_t len) {
  if (!mounted) return false;
  char path[48], tmp[52];
  snprintf(path, sizeof path, "/pet/%s", name);
  snprintf(tmp, sizeof tmp, "%s.tmp", path);
  FILE* f = fopen(tmp, "wb");
  if (!f) return false;
  bool ok = fwrite(buf, 1, len, f) == len;
  ok = (fflush(f) == 0) && ok;
  fclose(f);
  if (!ok) return false;
  remove(path);
  return rename(tmp, path) == 0;
}

}  // namespace storage
