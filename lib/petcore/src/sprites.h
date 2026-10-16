// 16x16 pixel-art pet, composed from a body per stage plus a face per mood.
// Output is palette indices; the firmware and the simulator map them to RGB.
#pragma once
#include <cstdint>

#include "pet.h"

namespace petcore {

constexpr int kSpriteSize = 16;
constexpr int kHeartSize = 8;

enum PaletteIndex : uint8_t {
  kTransparent = 0,
  kOutline,
  kBody,
  kShade,
  kWhite,
  kCheek,
  kTear,
  kAccent,
  kHeartFull,
  kHeartEmpty,
  kPaletteCount,
};

using Sprite = uint8_t[kSpriteSize][kSpriteSize];
using HeartSprite = uint8_t[kHeartSize][kHeartSize];

// `frame` drives idle animation (bobbing, blinking, egg wobble); advance it ~2x/s.
void render_pet(Stage stage, Mood mood, uint32_t frame, Sprite out);
void render_heart(bool full, HeartSprite out);
// 0xRRGGBB for a palette index; body colours change with the stage.
uint32_t palette_rgb(Stage stage, uint8_t index);

}  // namespace petcore
