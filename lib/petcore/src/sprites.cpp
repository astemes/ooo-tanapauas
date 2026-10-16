#include "sprites.h"

#include <cstring>

namespace petcore {

namespace {

// '.' transparent  '#' outline  'o' body  's' shade  'w' white  'p' cheek
// 't' tear  'a' accent
using Art = const char* const[kSpriteSize];

Art kEgg = {
    "................",
    "......####......",
    ".....#wwww#.....",
    "....#wwwwww#....",
    "...#wwaawwww#...",
    "...#wwwwwwww#...",
    "..#wwwwwwaaww#..",
    "..#wwwwwwaaww#..",
    "..#waawwwwwww#..",
    "..#waawwwwwww#..",
    "..#wwwwwwwwww#..",
    "...#wwwwwwww#...",
    "...#swwwwwws#...",
    "....#ssssss#....",
    ".....######.....",
    "................",
};

Art kBaby = {
    "................",
    "................",
    "................",
    "................",
    "................",
    "......####......",
    "....##oooo##....",
    "...#oooooooo#...",
    "..#oooooooooo#..",
    "..#oooooooooo#..",
    "..#oooooooooo#..",
    "..#oooooooooo#..",
    "..#soooooooos#..",
    "...#ssssssss#...",
    "....########....",
    "................",
};

Art kChild = {
    "................",
    "...##......##...",
    "..#oo#....#oo#..",
    "..#oo######oo#..",
    "..#oooooooooo#..",
    ".#oooooooooooo#.",
    ".#oooooooooooo#.",
    ".#oooooooooooo#.",
    ".#oooooooooooo#.",
    ".#oooooooooooo#.",
    ".#soooooooooos#.",
    "..#ssssssssss#..",
    "...#oo####oo#...",
    "...#ss#..#ss#...",
    "....##....##....",
    "................",
};

Art kTeen = {
    ".......##.......",
    "......#aa#......",
    "....###aa###....",
    "...#oooooooo#...",
    "..#oooooooooo#..",
    "..#oooooooooo#..",
    "..#oooooooooo#..",
    "..#oooooooooo#..",
    ".##oooooooooo##.",
    "#oo#oooooooo#oo#",
    ".##soooooooos##.",
    "...#ssssssss#...",
    "...#oo#..#oo#...",
    "...#ss#..#ss#...",
    "....##....##....",
    "................",
};

Art kAdult = {
    "................",
    ".....######.....",
    "...##oooooo##...",
    "..#oooooooooo#..",
    ".#oooooooooooo#.",
    ".#oooooooooooo#.",
    ".#oooooooooooo#.",
    ".#oooooooooooo#.",
    "#oooowwwwwwoooo#",
    "#ooowwwwwwwwooo#",
    "#ooowwwwwwwwooo#",
    ".#soowwwwwwoos#.",
    "..#ssssssssss#..",
    "..#oo#....#oo#..",
    "..####....####..",
    "................",
};

Art kElder = {
    ".....aaaaaa.....",
    "....a######a....",
    "...##oooooo##...",
    "..#oooooooooo#..",
    ".#oooooooooooo#.",
    ".#oooooooooooo#.",
    ".#oooooooooooo#.",
    ".#owwwwwwwwwwo#.",
    "#oowwwwwwwwwwoo#",
    "#ooowwwwwwwwooo#",
    "#oooowwwwwwoooo#",
    ".#sooowwwwooos#.",
    "..#ssssssssss#..",
    "..#oo#....#oo#..",
    "..####....####..",
    "................",
};

Art kTombstone = {
    "................",
    "................",
    ".....######.....",
    "....#ssssss#....",
    "...#sss##sss#...",
    "...#s######s#...",
    "...#sss##sss#...",
    "...#sss##sss#...",
    "...#ssssssss#...",
    "...#ssssssss#...",
    "...#ssssssss#...",
    "...#ssssssss#...",
    ".aa##########aa.",
    "aaaaaaaaaaaaaaaa",
    "................",
    "................",
};

constexpr int kFaceW = 8;
constexpr int kFaceH = 4;
using Face = const char* const[kFaceH];

Face kFaceHappy = {".#....#.", "p......p", "..#..#..", "...##..."};
Face kFaceContent = {".#....#.", "p......p", "...##...", "........"};
Face kFaceSad = {".#....#.", "........", "...##...", "..#..#.."};
Face kFaceCrying = {"##....##", ".t....t.", ".t.##.t.", "..#..#.."};
constexpr const char* kClosedEyes = "##....##";

struct FaceAnchor {
  int x, y;
};
// Top-left of the 8x4 face per stage (Egg has no face).
constexpr FaceAnchor kFaceAt[] = {{0, 0}, {4, 8}, {4, 5}, {4, 4}, {4, 4}, {4, 4}};

const char* const* body_for(Stage s) {
  switch (s) {
    case Stage::Egg: return kEgg;
    case Stage::Baby: return kBaby;
    case Stage::Child: return kChild;
    case Stage::Teen: return kTeen;
    case Stage::Adult: return kAdult;
    default: return kElder;
  }
}

uint8_t index_for(char c) {
  switch (c) {
    case '#': return kOutline;
    case 'o': return kBody;
    case 's': return kShade;
    case 'w': return kWhite;
    case 'p': return kCheek;
    case 't': return kTear;
    case 'a': return kAccent;
    default: return kTransparent;
  }
}

// Copy art into `out`, shifted by (dx, dy); pixels pushed off the edge are dropped.
void blit(const char* const* art, int w, int h, int dx, int dy, bool skip_transparent, Sprite out) {
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      int tx = x + dx, ty = y + dy;
      if (tx < 0 || ty < 0 || tx >= kSpriteSize || ty >= kSpriteSize) continue;
      uint8_t idx = index_for(art[y][x]);
      if (skip_transparent && idx == kTransparent) continue;
      out[ty][tx] = idx;
    }
  }
}

uint32_t darken(uint32_t rgb) {
  uint32_t r = (rgb >> 16) & 0xFF, g = (rgb >> 8) & 0xFF, b = rgb & 0xFF;
  return ((r * 7 / 10) << 16) | ((g * 7 / 10) << 8) | (b * 7 / 10);
}

}  // namespace

void render_pet(Stage stage, Mood mood, uint32_t frame, Sprite out) {
  std::memset(out, kTransparent, sizeof(Sprite));

  if (mood == Mood::Dead) {
    blit(kTombstone, kSpriteSize, kSpriteSize, 0, 0, false, out);
    return;
  }

  if (stage == Stage::Egg) {
    // Wobble side to side; sad eggs sit still.
    int dx = mood == Mood::Sad ? 0 : (frame % 4 == 1 ? -1 : frame % 4 == 3 ? 1 : 0);
    blit(kEgg, kSpriteSize, kSpriteSize, dx, 0, false, out);
    return;
  }

  // Happy pets bounce; sad and crying ones droop slowly.
  int bob = (mood == Mood::Happy || mood == Mood::Content) ? static_cast<int>(frame % 2)
                                                           : (frame % 4 == 0 ? 1 : 0);
  blit(body_for(stage), kSpriteSize, kSpriteSize, 0, bob, false, out);

  const char* const* face = mood == Mood::Happy     ? kFaceHappy
                            : mood == Mood::Content ? kFaceContent
                            : mood == Mood::Sad     ? kFaceSad
                                                    : kFaceCrying;
  const char* rows[kFaceH] = {face[0], face[1], face[2], face[3]};
  bool blink = mood != Mood::Crying && frame % 8 == 7;
  if (blink) rows[0] = kClosedEyes;
  FaceAnchor at = kFaceAt[static_cast<int>(stage)];
  blit(rows, kFaceW, kFaceH, at.x, at.y + bob, true, out);
}

void render_heart(bool full, HeartSprite out) {
  static const char* const kHeart[kHeartSize] = {
      ".##..##.", "#xx##xx#", "#xxxxxx#", "#xxxxxx#",
      ".#xxxx#.", "..#xx#..", "...##...", "........",
  };
  for (int y = 0; y < kHeartSize; y++)
    for (int x = 0; x < kHeartSize; x++) {
      char c = kHeart[y][x];
      out[y][x] = c == '#' ? kOutline : c == 'x' ? (full ? kHeartFull : kHeartEmpty) : kTransparent;
    }
}

uint32_t palette_rgb(Stage stage, uint8_t index) {
  static const uint32_t kBodyColour[] = {
      0xF4F1E8,  // Egg (body unused)
      0xFFD84D,  // Baby: yellow
      0x7ED957,  // Child: green
      0x4DA6FF,  // Teen: blue
      0xFF9A3C,  // Adult: orange
      0xB98CFF,  // Elder: purple
  };
  uint32_t body = kBodyColour[static_cast<int>(stage) % 6];
  switch (index) {
    case kOutline: return 0x2B2233;
    case kBody: return body;
    case kShade: return stage == Stage::Egg ? 0xCFC8B8 : darken(body);
    case kWhite: return 0xF7F4EE;
    case kCheek: return 0xFF8FA3;
    case kTear: return 0x5BC0FF;
    case kAccent: return stage == Stage::Egg ? 0x8FD3C8 : 0xFFE066;
    case kHeartFull: return 0xFF4D6D;
    case kHeartEmpty: return 0x5A5560;
    default: return 0x000000;
  }
}

}  // namespace petcore
