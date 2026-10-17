#include "wifi_link.h"

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <mbed.h>

#include <cstring>
#include <ctime>

#include "../hal/storage.h"

namespace wifi_link {

namespace {
const char* kFile = "wifi.bin";
constexpr uint32_t kMagic = 0x57494649;  // "WIFI"
constexpr uint32_t kRetryMs = 60000;
constexpr uint32_t kNtpEveryMs = 6UL * 3600 * 1000;
constexpr uint32_t kNtpRetryMs = 30000;
constexpr uint32_t kUnixValid = 1700000000;

struct Persisted {
  uint32_t magic;
  Credentials creds;
};

Persisted saved{};
char ip_text[16] = "";
uint32_t last_attempt = 0;
uint32_t last_ntp_ok = 0;
uint32_t last_ntp_try = 0;
bool ntp_ever = false;
WiFiUDP udp;
const char* error_text = "";

bool ssid_visible(const char* ssid) {
  int n = WiFi.scanNetworks();
  for (int i = 0; i < n; i++)
    if (std::strcmp(WiFi.SSID(i), ssid) == 0) return true;
  return false;
}

void connect_now() {
  last_attempt = millis();
  ip_text[0] = '\0';
  if (!has_credentials()) return;
  WiFi.setTimeout(10000);
  Serial.print("wifi: connecting to ");
  Serial.println(saved.creds.ssid);
  // Auto-detect the security from a scan first. If that fails, retry as plain
  // WPA2: it covers WPA2/WPA3 mixed-mode routers and hidden networks.
  bool ok = WiFi.begin(saved.creds.ssid, saved.creds.pass) == WL_CONNECTED;
  bool visible = ok || ssid_visible(saved.creds.ssid);
  if (!ok) {
    WiFi.disconnect();
    ok = WiFi.begin(saved.creds.ssid, saved.creds.pass, ENC_TYPE_CCMP) == WL_CONNECTED;
  }
  error_text = ok ? "" : visible ? "network found, but it refused the password or security type"
                                 : "network not found (it must be 2.4 GHz and the name must match exactly)";
  if (ok) {
    IPAddress addr = WiFi.localIP();
    snprintf(ip_text, sizeof ip_text, "%u.%u.%u.%u", addr[0], addr[1], addr[2], addr[3]);
    Serial.print("wifi: connected, ip ");
    Serial.println(ip_text);
  } else {
    Serial.println("wifi: connect failed");
  }
}

// One NTP round trip with a 1.5 s timeout.
bool ntp_sync() {
  last_ntp_try = millis();
  uint8_t packet[48] = {};
  packet[0] = 0b11100011;  // LI unsynced, NTP v4, client mode
  if (!udp.begin(2390)) return false;
  bool ok = false;
  if (udp.beginPacket("pool.ntp.org", 123) && udp.write(packet, sizeof packet) == sizeof packet && udp.endPacket()) {
    uint32_t start = millis();
    while (millis() - start < 1500) {
      if (udp.parsePacket() >= 48) {
        udp.read(packet, sizeof packet);
        uint32_t secs1900 = (uint32_t(packet[40]) << 24) | (uint32_t(packet[41]) << 16) |
                            (uint32_t(packet[42]) << 8) | packet[43];
        uint32_t unix = secs1900 - 2208988800UL;
        if (unix > kUnixValid) {
          set_time(unix);
          ok = true;
        }
        break;
      }
      delay(10);
    }
  }
  udp.stop();
  if (ok) {
    last_ntp_ok = millis();
    ntp_ever = true;
    Serial.println("wifi: clock set from NTP");
  }
  return ok;
}
}  // namespace

void begin() {
  if (!storage::read_blob(kFile, &saved, sizeof saved) || saved.magic != kMagic) {
    std::memset(&saved, 0, sizeof saved);
    saved.magic = kMagic;
  }
  saved.creds.ssid[sizeof saved.creds.ssid - 1] = '\0';
  saved.creds.pass[sizeof saved.creds.pass - 1] = '\0';
  connect_now();
  if (connected()) ntp_sync();
}

bool has_credentials() { return saved.creds.ssid[0] != '\0'; }
const Credentials& credentials() { return saved.creds; }

void set_credentials(const char* ssid, const char* pass) {
  std::strncpy(saved.creds.ssid, ssid, sizeof saved.creds.ssid - 1);
  std::strncpy(saved.creds.pass, pass, sizeof saved.creds.pass - 1);
  saved.creds.ssid[sizeof saved.creds.ssid - 1] = '\0';
  saved.creds.pass[sizeof saved.creds.pass - 1] = '\0';
  storage::write_blob(kFile, &saved, sizeof saved);
  WiFi.disconnect();
  connect_now();
  if (connected()) ntp_sync();
}

const char* last_error() { return error_text; }

void print_scan() {
  int n = WiFi.scanNetworks();
  for (int i = 0; i < n; i++) {
    static const char* kSec[] = {"?", "?", "WPA", "?", "WPA2", "WEP", "?", "OPEN", "AUTO"};
    uint8_t enc = WiFi.encryptionType(i);
    Serial.print("NET ");
    Serial.print(WiFi.RSSI(i));
    Serial.print(' ');
    Serial.print(WiFi.channel(i));
    Serial.print(' ');
    Serial.print(enc < 9 ? kSec[enc] : "?");
    Serial.print(' ');
    Serial.println(WiFi.SSID(i));
  }
  Serial.println("SCAN END");
}

bool connected() { return WiFi.status() == WL_CONNECTED && ip_text[0] != '\0'; }
const char* ip() { return connected() ? ip_text : ""; }

void poll(uint32_t now_ms, bool may_block) {
  if (!connected()) {
    if (may_block && has_credentials() && now_ms - last_attempt >= kRetryMs) connect_now();
    return;
  }
  uint32_t wait = ntp_ever ? kNtpEveryMs : kNtpRetryMs;
  if (may_block && now_ms - (ntp_ever ? last_ntp_ok : last_ntp_try) >= wait) ntp_sync();
}

uint32_t unix_now() {
  time_t t = time(nullptr);
  return t > static_cast<time_t>(kUnixValid) ? static_cast<uint32_t>(t) : 0;
}

void set_unix(uint32_t unix) {
  if (unix > kUnixValid) set_time(unix);
}

}  // namespace wifi_link
