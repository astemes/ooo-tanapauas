// Terminal simulator for the phone box. Keys 1-4 dock/lift phones; time can
// be warped to play-test whole sessions in seconds. Saves to petsim.sav.
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>

#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>

#include "game.h"
#include "save.h"
#include "sprites.h"
#include "text.h"

using namespace petcore;

namespace {

const char* kSavePath = "petsim.sav";
const uint16_t kTargets[] = {0, 15, 30, 60, 90};
const uint32_t kWarps[] = {1, 60, 600};
constexpr uint32_t kBackground = 0x1E1B24;

termios g_orig_term;
bool g_raw = false;

void restore_terminal() {
  if (g_raw) tcsetattr(STDIN_FILENO, TCSANOW, &g_orig_term);
  std::printf("\x1b[0m\x1b[?25h\n");
}

void enable_raw() {
  if (!isatty(STDIN_FILENO)) return;
  tcgetattr(STDIN_FILENO, &g_orig_term);
  termios raw = g_orig_term;
  raw.c_lflag &= ~(ICANON | ECHO);
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 0;
  tcsetattr(STDIN_FILENO, TCSANOW, &raw);
  g_raw = true;
}

// Returns -1 if no key is waiting, -2 on EOF.
int read_key() {
  fd_set fds;
  FD_ZERO(&fds);
  FD_SET(STDIN_FILENO, &fds);
  timeval tv{0, 0};
  if (select(STDIN_FILENO + 1, &fds, nullptr, nullptr, &tv) <= 0) return -1;
  char c;
  ssize_t n = read(STDIN_FILENO, &c, 1);
  if (n == 0) return -2;
  return n < 0 ? -1 : static_cast<unsigned char>(c);
}

bool load(SaveData& out) {
  FILE* f = std::fopen(kSavePath, "rb");
  if (!f) return false;
  uint8_t buf[kSaveBufferSize];
  size_t n = std::fread(buf, 1, sizeof buf, f);
  std::fclose(f);
  return save_decode(buf, n, out);
}

void store(const SaveData& d) {
  uint8_t buf[kSaveBufferSize];
  size_t n = save_encode(d, buf, sizeof buf);
  FILE* f = std::fopen(kSavePath, "wb");
  if (!f) return;
  std::fwrite(buf, 1, n, f);
  std::fclose(f);
}

void set_colour(bool fg, uint32_t rgb) {
  std::printf("\x1b[%d;2;%u;%u;%um", fg ? 38 : 48, (rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
}

uint32_t px_colour(Stage stage, uint8_t idx) {
  return idx == kTransparent ? kBackground : palette_rgb(stage, idx);
}

std::string clock_text(uint32_t ms) {
  uint32_t s = ms / 1000;
  char buf[16];
  std::snprintf(buf, sizeof buf, "%02u:%02u:%02u", s / 3600, (s / 60) % 60, s % 60);
  return buf;
}

void draw(const Game& g, uint32_t now, uint32_t frame, uint32_t warp, const std::string& msg) {
  const SaveData& d = g.data();
  std::printf("\x1b[H\x1b[0m");

  std::printf(" PHONE JAIL PET   Gen %u %-6s  ", d.pet.generation, stage_name(d.pet.stage));
  for (int i = 0; i < kMaxHearts; i++) {
    set_colour(true, i < d.pet.hearts ? 0xFF4D6D : 0x5A5560);
    std::printf("♥ ");
  }
  std::printf("\x1b[0m  Happy %3u%%  XP %u", d.pet.happiness, d.pet.xp);
  uint32_t next = next_stage_xp(d.pet.stage);
  if (next) std::printf("/%u", next);
  std::printf("\x1b[K\n\n");

  Sprite sp;
  render_pet(d.pet.stage, g.mood(), frame, sp);
  for (int y = 0; y < kSpriteSize; y += 2) {
    std::printf("      ");
    for (int x = 0; x < kSpriteSize; x++) {
      set_colour(true, px_colour(d.pet.stage, sp[y][x]));
      set_colour(false, px_colour(d.pet.stage, sp[y + 1][x]));
      std::printf("▀▀");
    }
    std::printf("\x1b[0m\x1b[K\n");
  }
  std::printf("\n");

  switch (g.state()) {
    case State::Idle:
      std::printf(" IDLE  Dock phones and press [s]. Target: %s\x1b[K\n",
                  g.target_minutes() ? (std::to_string(g.target_minutes()) + " min").c_str() : "free play");
      break;
    case State::Arming:
      std::printf(" ARMING  starting in %u s...\x1b[K\n", (g.arming_remaining_ms(now) + 999) / 1000);
      break;
    case State::Running: {
      std::printf(" RUNNING  %s  session XP %u", clock_text(g.session_elapsed_ms(now)).c_str(),
                  g.session_xp());
      if (g.target_minutes())
        std::printf("  target %u min%s", g.target_minutes(), g.target_reached() ? " (reached!)" : "");
      if (g.emergency_window_open())
        std::printf("  EMERGENCY %us", g.emergency_remaining_ms(now) / 1000);
      else if (!g.emergency_available())
        std::printf("  (pass used)");
      std::printf("\x1b[K\n");
      break;
    }
    case State::Result:
      std::printf(" RESULT  press [enter] to continue\x1b[K\n");
      break;
  }

  std::printf(" Slots: ");
  for (int s = 0; s < kMaxSlots; s++) {
    uint8_t bit = 1u << s;
    const char* label = "empty ";
    uint32_t colour = 0x8A8494;
    if (g.lifting_mask() & bit) {
      label = "LIFT! ";
      colour = 0xFF4D6D;
    } else if (g.armed_mask() & bit) {
      label = "locked";
      colour = 0x7ED957;
    } else if (g.docked_mask() & bit) {
      label = "docked";
      colour = 0x4DA6FF;
    }
    set_colour(true, colour);
    std::printf("[%d %s] ", s + 1, label);
  }
  std::printf("\x1b[0m\x1b[K\n\n");
  std::printf(" %s\x1b[K\n\n", msg.c_str());

  const Lifetime& l = d.life;
  std::printf(" Streak %u (best %u)  Wins %u  Breaks %u  Total %u min  Graves %u\x1b[K\n", l.streak,
              l.best_streak, l.sessions_success, l.sessions_broken, l.total_minutes, l.graveyard);
  std::printf(" Shame: P1 %u  P2 %u  P3 %u  P4 %u\x1b[K\n\n", l.breaks[0], l.breaks[1], l.breaks[2],
              l.breaks[3]);
  std::printf(" [1-4] dock/lift  [s]tart  [c]ancel  [e]mergency  [f]inish  [enter] ok\x1b[K\n");
  std::printf(" [t]arget  [w]arp x%u  [r]eset pet  [q]uit\x1b[K\n", warp);
  std::fflush(stdout);
}

}  // namespace

int main() {
  using clock = std::chrono::steady_clock;
  auto t0 = clock::now();
  auto last = t0;
  double game_ms = 1000;
  double unix_offset = 0;  // warped seconds added to the real wall clock
  int warp_idx = 0;
  int target_idx = 0;
  uint8_t docked = 0;
  std::string msg = "Welcome! Press 1-4 to dock phones.";

  SaveData saved;
  if (!load(saved)) msg = "No save found, a fresh egg appears.";
  Game game;
  game.begin(saved, static_cast<uint32_t>(game_ms), static_cast<uint32_t>(std::time(nullptr)));

  enable_raw();
  std::printf("\x1b[2J\x1b[?25l");
  bool running = true;
  while (running) {
    auto now = clock::now();
    double real_dt = std::chrono::duration<double, std::milli>(now - last).count();
    last = now;
    game_ms += real_dt * kWarps[warp_idx];
    unix_offset += real_dt * (kWarps[warp_idx] - 1) / 1000.0;
    uint32_t ms = static_cast<uint32_t>(game_ms);
    uint32_t unix_now = static_cast<uint32_t>(std::time(nullptr) + unix_offset);

    for (int key; (key = read_key()) != -1;) {
      if (key == -2 || key == 'q') {
        running = false;
        break;
      }
      if (key >= '1' && key <= '4') docked ^= 1u << (key - '1');
      else if (key == 's' && !game.start(ms)) msg = "Dock at least one phone first.";
      else if (key == 'c') game.cancel();
      else if (key == 'e') game.request_emergency(ms);
      else if (key == 'f') game.finish(ms);
      else if (key == '\n' || key == '\r' || key == ' ') game.acknowledge();
      else if (key == 'w') warp_idx = (warp_idx + 1) % 3;
      else if (key == 't' && game.state() == State::Idle) {
        target_idx = (target_idx + 1) % 5;
        game.set_target_minutes(kTargets[target_idx]);
      } else if (key == 'r' && game.state() == State::Idle) {
        game.begin(SaveData{}, ms, unix_now);
        store(game.data());
        msg = "Pet reset.";
      }
    }

    game.update(ms, docked, unix_now);
    char text[96];
    for (Event e; game.poll_event(e);)
      if (describe_event(e, game, text, sizeof text)) msg = text;
    if (game.take_dirty()) store(game.data());

    uint32_t frame = static_cast<uint32_t>(std::chrono::duration<double>(now - t0).count() * 2);
    draw(game, ms, frame, kWarps[warp_idx], msg);
    usleep(50000);
  }
  store(game.data());
  restore_terminal();
  return 0;
}
