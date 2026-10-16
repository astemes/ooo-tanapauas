// Debounced reader for the phone-slot microswitches.
#pragma once
#include <Arduino.h>

#include <pet.h>

class Slots {
 public:
  void begin();
  // Bit n set = phone docked in slot n.
  uint8_t read(uint32_t now_ms);

 private:
  uint8_t stable_ = 0;
  uint8_t raw_last_ = 0;
  uint32_t changed_at_[petcore::kMaxSlots] = {};
};
