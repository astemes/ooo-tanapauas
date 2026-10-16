// Pin assignments and hardware options. See docs/hardware.md.
#pragma once
#include <Arduino.h>

#include <pet.h>

// Microswitch per slot: NO -> pin, COM -> GND. LOW = phone docked.
constexpr uint8_t kSlotPins[petcore::kMaxSlots] = {2, 3, 4, 5};
constexpr uint32_t kSwitchDebounceMs = 25;

// Passive piezo via 100 ohm. Set kBuzzerEnabled = false if not fitted.
constexpr uint8_t kBuzzerPin = 7;
constexpr bool kBuzzerEnabled = true;

constexpr uint32_t kAnimFrameMs = 500;
