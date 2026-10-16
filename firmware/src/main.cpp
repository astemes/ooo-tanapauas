// Phone-Free Pet Box: GIGA R1 WiFi + GIGA Display Shield.
// Wires the slot switches, flash storage, buzzer and touchscreen UI to the
// platform-independent game in lib/petcore.
#include <Arduino.h>
#include <Arduino_GigaDisplayTouch.h>
#include <Arduino_H7_Video.h>
#include <lvgl.h>

#include <ctime>

#include <game.h>

#include "config.h"
#include "hal/buzzer.h"
#include "hal/slots.h"
#include "hal/storage.h"
#include "ui/ui.h"

using namespace petcore;

Arduino_H7_Video Display(800, 480, GigaDisplayShield);
Arduino_GigaDisplayTouch TouchDetector;

Slots slots;
Game game;

// Unix time if the RTC has been set (e.g. kept by a VRTC coin cell), else 0.
static uint32_t wall_clock() {
  time_t t = time(nullptr);
  return t > 1700000000 ? static_cast<uint32_t>(t) : 0;
}

void setup() {
  Serial.begin(115200);
  Display.begin();
  TouchDetector.begin();
  slots.begin();
  buzzer::begin();

  bool storage_ok = storage::begin();
  SaveData saved;
  if (storage_ok && !storage::load(saved)) Serial.println("storage: no save yet, new egg");
  game.begin(saved, millis(), wall_clock());
  ui::begin(game, storage_ok);
}

void loop() {
  uint32_t now = millis();
  game.update(now, slots.read(now), wall_clock());

  for (Event e; game.poll_event(e);) {
    ui::on_event(e);
    buzzer::on_event(e);
  }
  if (game.take_dirty() && !storage::save(game.data()) && storage::available())
    Serial.println("storage: save failed");

  ui::update(now);
  lv_timer_handler();
  delay(5);
}
