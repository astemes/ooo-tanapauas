// Home Wi-Fi connection and NTP clock for the box.
#pragma once
#include <cstdint>

namespace wifi_link {

struct Credentials {
  char ssid[33];
  char pass[65];
};

void begin();  // loads saved credentials and connects (blocks up to ~10 s)
bool has_credentials();
const Credentials& credentials();
void set_credentials(const char* ssid, const char* pass);  // saves and reconnects
bool connected();
// Why the last connect failed, e.g. "network not found"; "" after success.
const char* last_error();
// Prints visible networks over Serial as "NET <rssi> <channel> <security> <ssid>".
void print_scan();
const char* ip();  // "" when offline
// Keeps the link and the clock fresh; call every loop. Only blocks when it
// has to reconnect, and only when `may_block` (i.e. no session running).
void poll(uint32_t now_ms, bool may_block);

// Unix time if known (NTP, or set by a phone), else 0.
uint32_t unix_now();
void set_unix(uint32_t unix);

}  // namespace wifi_link
