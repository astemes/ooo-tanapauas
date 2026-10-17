// Human-readable messages for game events, shared by firmware and simulator.
#pragma once
#include <cstddef>

#include "game.h"

namespace petcore {

// Writes a one-line message for `e` into `buf`; returns false if the event
// has nothing worth showing.
bool describe_event(const Event& e, const Game& game, char* buf, size_t len);
// One line for the wall of shame, e.g. "Anna missed yesterday's 30 min".
// Returns false if nobody is currently shamed.
bool describe_shame(const Game& game, char* buf, size_t len);

}  // namespace petcore
