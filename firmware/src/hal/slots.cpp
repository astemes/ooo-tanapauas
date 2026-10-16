#include "slots.h"

#include "../config.h"

void Slots::begin() {
  for (uint8_t pin : kSlotPins) pinMode(pin, INPUT_PULLUP);
  raw_last_ = stable_ = read(0);
}

uint8_t Slots::read(uint32_t now_ms) {
  for (int s = 0; s < petcore::kMaxSlots; s++) {
    uint8_t bit = 1u << s;
    uint8_t raw = digitalRead(kSlotPins[s]) == LOW ? bit : 0;
    if (raw != (raw_last_ & bit)) {
      raw_last_ = (raw_last_ & ~bit) | raw;
      changed_at_[s] = now_ms;
    } else if (now_ms - changed_at_[s] >= kSwitchDebounceMs || now_ms == 0) {
      stable_ = (stable_ & ~bit) | raw;
    }
  }
  return stable_;
}
