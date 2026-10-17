// Phone-Free Pet Box: GIGA R1 WiFi + GIGA Display Shield.
// Wires the slot switches, flash storage, buzzer, Wi-Fi, web server and
// touchscreen UI to the platform-independent game in lib/petcore.
#include <Arduino.h>
#include <Arduino_GigaDisplayTouch.h>
#include <Arduino_H7_Video.h>
#include <lvgl.h>

#include <game.h>

#include <cstring>

#include "config.h"
#include "hal/buzzer.h"
#include "hal/slots.h"
#include "hal/storage.h"
#include "history.h"
#include "net/server.h"
#include "net/wifi_link.h"
#include "ui/ui.h"

using namespace petcore;

Arduino_H7_Video Display(800, 480, GigaDisplayShield);
Arduino_GigaDisplayTouch TouchDetector;

Slots slots;
Game game;

// USB serial setup, used by firmware/tools/set_wifi.py so long passwords
// don't have to be typed on the touchscreen. Line format:
//   WIFI<TAB>ssid<TAB>password<LF>   ->   "WIFI OK <ip>" or "WIFI FAIL"
//   STATUS<LF>                       ->   "STATUS <ssid> <ip or offline> storage=<ok|none> up=<s>"
//   SCAN<LF>                         ->   "NET <rssi> <ch> <security> <ssid>" lines, then "SCAN END"
static void poll_serial_commands() {
  static char line[128];
  static size_t len = 0;
  while (Serial.available()) {
    char c = static_cast<char>(Serial.read());
    if (c == '\r') continue;
    if (c != '\n') {
      if (len < sizeof line - 1) line[len++] = c;
      continue;
    }
    line[len] = '\0';
    len = 0;
    if (strncmp(line, "WIFI\t", 5) == 0) {
      char* ssid = line + 5;
      char* pass = strchr(ssid, '\t');
      if (!pass) {
        Serial.println("WIFI FAIL bad format");
        continue;
      }
      *pass++ = '\0';
      ui::show_status("Connecting to Wi-Fi...");
      wifi_link::set_credentials(ssid, pass);
      std::memset(line, 0, sizeof line);  // don't keep the password around
      if (wifi_link::connected()) {
        Serial.print("WIFI OK ");
        Serial.println(wifi_link::ip());
        ui::show_status("Wi-Fi connected. Scan the code to link your phone.");
      } else {
        Serial.print("WIFI FAIL ");
        Serial.println(wifi_link::last_error());
        ui::show_status("Wi-Fi failed. Check the network name and password.");
      }
    } else if (strcmp(line, "PARTS") == 0) {
      storage::print_partitions();
    } else if (strcmp(line, "SCAN") == 0) {
      wifi_link::print_scan();
    } else if (strcmp(line, "STATUS") == 0) {
      Serial.print("STATUS ");
      Serial.print(wifi_link::has_credentials() ? wifi_link::credentials().ssid : "-");
      Serial.print(" ");
      Serial.print(wifi_link::connected() ? wifi_link::ip() : "offline");
      Serial.print(storage::available() ? " storage=ok" : " storage=none");
      Serial.print(" up=");
      Serial.println(millis() / 1000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  Display.begin();
  // The library mounts the portrait panel as landscape with ROT_270; ROT_90
  // flips that 180 degrees. LVGL applies the same rotation to touch input.
  lv_disp_set_rotation(lv_disp_get_default(), LV_DISP_ROT_90);
  TouchDetector.begin();
  slots.begin();
  buzzer::begin();

  bool storage_ok = storage::begin();
  SaveData saved;
  if (storage_ok && !storage::load(saved)) Serial.println("storage: no save yet, new egg");
  history::begin();
  game.begin(saved, millis(), wifi_link::unix_now());
  game.seed(micros() ^ (analogRead(A0) << 16) ^ analogRead(A1));  // floating pins: which animal hatches
  ui::begin(game, storage_ok);

  ui::show_status("Connecting to Wi-Fi...");
  wifi_link::begin();
  server::begin(game);
  ui::show_status(wifi_link::connected() ? "" : "Offline. Set up Wi-Fi in Stats > Settings.");
}

void loop() {
  uint32_t now = millis();
  uint32_t unix = wifi_link::unix_now();

  history::before_update(game);
  game.update(now, slots.read(now), unix);
  history::after_update(game);

  for (Event e; game.poll_event(e);) {
    history::log_event(e, game, unix);
    ui::on_event(e);
    buzzer::on_event(e);
  }
  if (game.take_dirty() && !storage::save(game.data()) && storage::available())
    Serial.println("storage: save failed");

  // Reconnecting or syncing time can block for seconds: never mid-session.
  State st = game.state();
  wifi_link::poll(now, st != State::Running && st != State::Arming);
  server::poll(now);

  poll_serial_commands();
  ui::update(now);
  lv_timer_handler();
  delay(5);
}
