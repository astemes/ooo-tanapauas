// Human-readable messages for game events, shared by firmware and simulator.
#pragma once
#include <cstddef>

#include "game.h"

namespace petcore {

// Writes a one-line message for `e` into `buf`; returns false if the event
// has nothing worth showing.
bool describe_event(const Event& e, const Game& game, char* buf, size_t len);
// "Player 3", or "Someone" for kNoSlot.
void slot_name(uint8_t slot, char* buf, size_t len);

}  // namespace petcore
