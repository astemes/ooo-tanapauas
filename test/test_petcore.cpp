// Host-side unit tests for petcore. Build and run with `make test`.
#include <cstdio>
#include <cstring>
#include <functional>
#include <vector>

#include "game.h"
#include "pet.h"
#include "save.h"
#include "sprites.h"

using namespace petcore;

static int g_failures = 0;
static int g_checks = 0;

#define CHECK(cond)                                                    \
  do {                                                                 \
    g_checks++;                                                        \
    if (!(cond)) {                                                     \
      g_failures++;                                                    \
      std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);    \
    }                                                                  \
  } while (0)

struct TestCase {
  const char* name;
  std::function<void()> fn;
};
static std::vector<TestCase>& tests() {
  static std::vector<TestCase> t;
  return t;
}
struct Register {
  Register(const char* n, std::function<void()> f) { tests().push_back({n, f}); }
};
#define TEST(name)                          \
  static void name();                       \
  static Register reg_##name(#name, name);  \
  static void name()

constexpr uint32_t kMin = 60000;
constexpr uint8_t kAll = 0x0F;

// Test harness that drives a Game with a fake clock.
struct Sim {
  Game game;
  uint32_t now = 1000;
  uint8_t docked = 0;
  std::vector<Event> events;

  explicit Sim(Config cfg = Config{}) : game(cfg) { game.begin(SaveData{}, now); }

  void step(uint32_t ms) {
    // Advance in 100 ms ticks like a real main loop would.
    uint32_t end = now + ms;
    while (now < end) {
      now += 100;
      game.update(now, docked);
      Event e;
      while (game.poll_event(e)) events.push_back(e);
    }
  }
  bool saw(EventType t) const {
    for (auto& e : events)
      if (e.type == t) return true;
    return false;
  }
  void start_running(uint8_t mask = kAll) {
    docked = mask;
    step(100);
    game.start(now);
    step(game.config().arming_ms + 100);
  }
};

TEST(stage_thresholds) {
  CHECK(stage_for_xp(0) == Stage::Egg);
  CHECK(stage_for_xp(29) == Stage::Egg);
  CHECK(stage_for_xp(30) == Stage::Baby);
  CHECK(stage_for_xp(4000) == Stage::Elder);
  CHECK(stage_for_xp(999999) == Stage::Elder);
  CHECK(next_stage_xp(Stage::Elder) == 0);
}

TEST(xp_scales_with_players) {
  CHECK(xp_per_minute(0) == 0);
  CHECK(xp_per_minute(1) == 1);
  CHECK(xp_per_minute(2) == 2);
  CHECK(xp_per_minute(3) == 4);
  CHECK(xp_per_minute(4) == 6);
}

TEST(arming_then_running) {
  Sim s;
  s.docked = 0b0011;
  s.step(100);
  CHECK(s.game.start(s.now));
  CHECK(s.game.state() == State::Arming);
  s.step(5000);
  CHECK(s.game.state() == State::Arming);
  s.step(5100);
  CHECK(s.game.state() == State::Running);
  CHECK(s.game.armed_mask() == 0b0011);
  CHECK(s.game.data().session.active);
  CHECK(s.saw(EventType::SessionStarted));
}

TEST(start_needs_a_phone) {
  Sim s;
  s.step(100);
  CHECK(!s.game.start(s.now));
  CHECK(s.game.state() == State::Idle);
}

TEST(lift_during_arming_cancels_without_penalty) {
  Sim s;
  s.docked = kAll;
  s.step(100);
  s.game.start(s.now);
  s.docked = 0b0111;
  s.step(200);
  CHECK(s.game.state() == State::Idle);
  CHECK(s.saw(EventType::ArmingCancelled));
  CHECK(s.game.data().pet.hearts == kMaxHearts);
}

TEST(late_joiner_during_arming) {
  Sim s;
  s.docked = 0b0001;
  s.step(100);
  s.game.start(s.now);
  s.docked = 0b0101;
  s.step(200);
  CHECK(s.game.armed_mask() == 0b0101);
}

TEST(autostart_when_all_docked) {
  Sim s;
  s.docked = kAll;
  s.step(4000);
  CHECK(s.game.state() == State::Idle);
  s.step(1500);
  CHECK(s.game.state() == State::Arming);
}

TEST(xp_awarded_per_minute) {
  Sim s;
  s.start_running(kAll);
  s.step(3 * kMin);
  CHECK(s.game.session_minutes() == 3);
  CHECK(s.game.data().pet.xp == 3 * 6);
  CHECK(s.game.data().life.total_minutes == 3);
}

TEST(egg_hatches_into_baby) {
  Sim s;
  s.start_running(kAll);
  s.step(5 * kMin);  // 30 xp
  CHECK(s.game.data().pet.stage == Stage::Baby);
  CHECK(s.saw(EventType::Evolved));
}

TEST(short_bump_is_ignored) {
  Sim s;
  s.start_running(kAll);
  s.docked = 0b1011;
  s.step(400);
  CHECK(s.game.lifting_mask() == 0b0100);
  s.docked = kAll;
  s.step(1000);
  CHECK(s.game.state() == State::Running);
  CHECK(s.game.lifting_mask() == 0);
}

TEST(lift_breaks_session_and_names_culprit) {
  Sim s;
  s.start_running(kAll);
  s.step(2 * kMin);
  s.docked = 0b1011;  // slot index 2 lifted
  s.step(1000);
  CHECK(s.game.state() == State::Result);
  CHECK(s.game.result() == ResultKind::Broken);
  CHECK(s.game.culprit() == 2);
  CHECK(s.game.data().pet.hearts == kMaxHearts - 1);
  CHECK(s.game.data().life.breaks[2] == 1);
  CHECK(s.game.data().life.streak == 0);
  CHECK(!s.game.data().session.active);
  CHECK(s.game.mood() == Mood::Crying);
}

TEST(earliest_lifter_is_blamed) {
  Sim s;
  s.start_running(kAll);
  s.docked = 0b1110;  // slot 0 lifts first
  s.step(300);
  s.docked = 0b0110;  // then slot 3
  s.step(1000);
  CHECK(s.game.culprit() == 0);
}

TEST(three_breaks_kill_the_pet_and_a_new_egg_hatches) {
  Sim s;
  for (int i = 0; i < 3; i++) {
    s.start_running(kAll);
    s.docked = 0b0111;
    s.step(1000);
    CHECK(s.game.state() == State::Result);
    if (i < 2) s.game.acknowledge();
  }
  CHECK(s.game.result() == ResultKind::Died);
  CHECK(s.game.mood() == Mood::Dead);
  CHECK(s.game.data().life.graveyard == 1);
  CHECK(s.game.acknowledge());
  s.step(100);
  CHECK(s.saw(EventType::Hatched));
  CHECK(s.game.data().pet.hearts == kMaxHearts);
  CHECK(s.game.data().pet.stage == Stage::Egg);
  CHECK(s.game.data().pet.generation == 2);
  CHECK(s.game.data().life.breaks[3] == 3);  // lifetime stats survive
}

TEST(finish_after_minimum_is_success) {
  Sim s;
  s.start_running(kAll);
  s.step(12 * kMin);
  uint32_t xp_before = s.game.data().pet.xp;
  CHECK(s.game.finish(s.now));
  CHECK(s.game.result() == ResultKind::Success);
  CHECK(s.game.data().life.streak == 1);
  CHECK(s.game.data().life.sessions_success == 1);
  CHECK(s.game.data().pet.xp == xp_before + s.game.session_xp() * kCompletionBonusPercent / 100);
}

TEST(finish_too_early_is_aborted) {
  Sim s;
  s.start_running(kAll);
  s.step(3 * kMin);
  s.game.finish(s.now);
  CHECK(s.game.result() == ResultKind::Aborted);
  CHECK(s.game.data().life.streak == 0);
  CHECK(s.game.data().pet.hearts == kMaxHearts);
}

TEST(success_restores_a_heart) {
  Sim s;
  s.start_running(kAll);
  s.docked = 0b0111;
  s.step(1000);
  s.game.acknowledge();
  CHECK(s.game.data().pet.hearts == 2);
  s.start_running(kAll);
  s.step(10 * kMin);
  s.game.finish(s.now);
  CHECK(s.game.data().pet.hearts == 3);
}

TEST(target_reached_then_lift_is_success) {
  Sim s;
  s.game.set_target_minutes(15);
  s.start_running(kAll);
  s.step(15 * kMin);
  CHECK(s.game.target_reached());
  CHECK(s.saw(EventType::TargetReached));
  s.docked = 0b0111;
  s.step(1000);
  CHECK(s.game.result() == ResultKind::Success);
  CHECK(s.game.session_bonus_xp() == s.game.session_xp() * kCompletionBonusPercent / 100 + 15);
}

TEST(emergency_pass_lets_one_player_leave) {
  Sim s;
  s.start_running(kAll);
  CHECK(s.game.request_emergency(s.now));
  CHECK(!s.game.request_emergency(s.now));
  s.docked = 0b1101;
  s.step(2000);
  CHECK(s.game.state() == State::Running);
  CHECK(s.game.armed_mask() == 0b1101);
  CHECK(s.saw(EventType::EmergencyUsed));
  CHECK(!s.game.emergency_available());
  // A second lift is a normal break.
  s.docked = 0b1100;
  s.step(1000);
  CHECK(s.game.result() == ResultKind::Broken);
  CHECK(s.game.culprit() == 0);
}

TEST(unused_emergency_window_expires_and_pass_is_kept) {
  Sim s;
  s.start_running(kAll);
  s.game.request_emergency(s.now);
  s.step(31000);
  CHECK(s.saw(EventType::EmergencyExpired));
  CHECK(s.game.emergency_available());
}

TEST(rejoin_mid_session) {
  Sim s;
  s.start_running(0b0011);
  s.docked = 0b0111;
  s.step(200);
  CHECK(s.game.armed_mask() == 0b0111);
  CHECK(s.saw(EventType::PlayerJoined));
}

TEST(power_loss_mid_session_counts_as_break) {
  Sim s;
  s.start_running(kAll);
  s.step(7 * kMin);
  SaveData saved = s.game.data();
  CHECK(saved.session.active);

  Game rebooted;
  rebooted.begin(saved, 500);
  CHECK(rebooted.state() == State::Result);
  CHECK(rebooted.result() == ResultKind::Broken);
  CHECK(rebooted.culprit() == kNoSlot);
  CHECK(rebooted.data().pet.hearts == kMaxHearts - 1);
  CHECK(!rebooted.data().session.active);
  CHECK(rebooted.take_dirty());
}

TEST(absence_decays_happiness_once) {
  SaveData d;
  d.pet.happiness = 60;
  d.life.last_seen_unix = 1700000000;
  Game g;
  g.begin(d, 0, 1700000000 + 3 * 86400 + 100);
  CHECK(g.data().pet.happiness == 60 - 3 * kAbsenceHappinessLossPerDay);
  Game again;
  again.begin(g.data(), 0, 1700000000 + 3 * 86400 + 200);
  CHECK(again.data().pet.happiness == g.data().pet.happiness);
}

TEST(save_roundtrip) {
  SaveData d;
  d.pet.xp = 1234;
  d.pet.stage = Stage::Teen;
  d.pet.hearts = 2;
  d.pet.happiness = 77;
  d.pet.generation = 5;
  d.life.breaks[1] = 9;
  d.life.best_streak = 4;
  d.life.last_seen_unix = 1760000000;
  d.session.active = true;
  d.session.armed_mask = 0b1010;
  d.session.minutes = 42;

  uint8_t buf[kSaveBufferSize];
  size_t n = save_encode(d, buf, sizeof buf);
  CHECK(n > 0);
  SaveData out;
  CHECK(save_decode(buf, n, out));
  CHECK(out.pet.xp == 1234);
  CHECK(out.pet.stage == Stage::Teen);
  CHECK(out.pet.generation == 5);
  CHECK(out.life.breaks[1] == 9);
  CHECK(out.life.last_seen_unix == 1760000000);
  CHECK(out.session.active);
  CHECK(out.session.minutes == 42);

  buf[10] ^= 0x40;
  SaveData corrupt;
  CHECK(!save_decode(buf, n, corrupt));
  CHECK(!save_decode(buf, 5, corrupt));
}

TEST(sprites_render) {
  Sprite sp;
  for (int s = 0; s < static_cast<int>(Stage::Count); s++) {
    for (int m = 0; m <= static_cast<int>(Mood::Dead); m++) {
      for (uint32_t f = 0; f < 8; f++) {
        render_pet(static_cast<Stage>(s), static_cast<Mood>(m), f, sp);
        int opaque = 0;
        for (auto& row : sp)
          for (uint8_t px : row) {
            CHECK(px < kPaletteCount);
            opaque += px != kTransparent;
          }
        CHECK(opaque > 40);
      }
    }
  }
}

int main() {
  for (auto& t : tests()) {
    int before = g_failures;
    t.fn();
    std::printf("%s %s\n", g_failures == before ? "ok  " : "FAIL", t.name);
  }
  std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
