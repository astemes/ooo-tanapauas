// HTTP server on the home Wi-Fi: serves the companion app and the JSON API
// described in docs/api.md. Handles one request per poll() so the screen and
// the slot switches never stall for long.
#pragma once
#include <game.h>

namespace server {

void begin(petcore::Game& game);
void poll(uint32_t now_ms);

}  // namespace server
