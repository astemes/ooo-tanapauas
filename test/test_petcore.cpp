// Host-side unit tests for petcore. Build and run with `make test`.
#include <cstdio>
#include <cstring>
#include <functional>
#include <vector>

#include "game.h"
#include "pet.h"
#include "save.h"
#include "species.h"
#include "sprites.h"
#include "text.h"

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
constexpr uint32_t kHour = 60 * kMin;
constexpr uint8_t kAll = 0x0F;
constexpr uint32_t kDay = 86400;
constexpr uint32_t kUnix0 = 1767240000;  // 2026-01-01 04:00 UTC: a day boundary with reset 04:00

SaveData ready_save(uint8_t players = 4, uint16_t daily_min = 30) {
  SaveData d;
  d.settings.setup_done = true;
  d.settings.num_players = players;
  d.settings.daily_minimum_min = daily_min;
  settings_sanitize(d.settings);
  d.pet.egg_minutes = 0;  // free play unless a test picks an egg
  return d;
}

// Test harness that drives a Game with a fake clock.
struct Sim {
  Game game;
  uint32_t now = 1000;
  uint8_t docked = 0;
  std::vector<Event> events;

  uint32_t unix = 0;  // 0 = no wall clock; advances with `now` once set

  explicit Sim(Config cfg = Config{}, SaveData save = ready_save()) : game(cfg) { game.begin(save, now); }

  void step(uint32_t ms) {
    // Advance in 100 ms ticks like a real main loop would.
    uint32_t end = now + ms;
    while (now < end) {
      now += 100;
      if (unix && now % 1000 == 0) unix++;
      game.update(now, docked, unix);
      drain();
    }
  }
  void drain() {
    Event e;
    while (game.poll_event(e)) events.push_back(e);
  }
  // Jump the wall clock without simulating every tick (as if the box sat idle).
  void jump_days(uint32_t days) {
    unix += days * kDay;
    step(100);
  }
  int count(EventType t) const {
    int n = 0;
    for (auto& e : events) n += e.type == t;
    return n;
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
  CHECK(s.game.data().pet.species == kNoSpecies);
  s.start_running(kAll);
  s.step(5 * kMin);  // 30 xp
  const SaveData& d = s.game.data();
  CHECK(d.pet.stage == Stage::Baby);
  CHECK(s.saw(EventType::Evolved));
  CHECK(d.pet.species < kSpeciesCount);
  CHECK(d.collection.count[d.pet.species] == 1);
  CHECK(d.collection.best_stage[d.pet.species] == static_cast<uint8_t>(Stage::Baby));
  CHECK(s.count(EventType::NewSpecies) == 1);
  s.step(20 * kMin);  // 150 xp: Child
  CHECK(d.collection.best_stage[d.pet.species] == static_cast<uint8_t>(Stage::Child));
  CHECK(s.count(EventType::NewSpecies) == 1);
}

TEST(species_roll_follows_rarity) {
  int per_tier[static_cast<int>(Rarity::Count)] = {};
  int per_species[kSpeciesCount] = {};
  uint32_t x = 12345;
  const int n = 100000;
  for (int i = 0; i < n; i++) {
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    uint8_t s = roll_species(x);
    CHECK(s < kSpeciesCount);
    per_species[s]++;
    per_tier[static_cast<int>(species_info(s).rarity)]++;
  }
  for (int t = 0; t < static_cast<int>(Rarity::Count); t++) {
    int expected = n * egg_kind(kDefaultEggMinutes).weight[t] / 100;
    CHECK(per_tier[t] > expected * 9 / 10 && per_tier[t] < expected * 11 / 10);
  }
  for (int c : per_species) CHECK(c > 0);
  CHECK(std::strcmp(species_name(kNoSpecies), "Egg") == 0);
}

TEST(hatch_text_names_the_species) {
  Sim s;
  s.start_running(kAll);
  s.step(5 * kMin);
  char buf[128];
  Event e{EventType::Evolved, kNoSlot, static_cast<uint32_t>(Stage::Baby)};
  CHECK(describe_event(e, s.game, buf, sizeof buf));
  CHECK(std::strstr(buf, species_name(s.game.data().pet.species)) != nullptr);
}

TEST(v3_species_roundtrip) {
  SaveData d = ready_save();
  d.pet.stage = Stage::Teen;
  d.pet.species = 7;
  d.collection.count[7] = 2;
  d.collection.best_stage[7] = static_cast<uint8_t>(Stage::Adult);
  d.collection.count[12] = 1;
  uint8_t buf[kSaveBufferSize];
  size_t n = save_encode(d, buf, sizeof buf);
  CHECK(n > 0);
  SaveData out;
  CHECK(save_decode(buf, n, out));
  CHECK(out.pet.species == 7);
  CHECK(out.collection.count[7] == 2);
  CHECK(out.collection.best_stage[7] == static_cast<uint8_t>(Stage::Adult));
  CHECK(out.collection.count[12] == 1);
  CHECK(out.collection.count[0] == 0);
}

TEST(v2_save_becomes_blob) {
  SaveData d = ready_save();
  d.pet.xp = 600;
  d.pet.stage = Stage::Teen;
  d.life.graveyard = 2;
  uint8_t buf[kSaveBufferSize];
  CHECK(save_encode(d, buf, sizeof buf) > 0);
  // Drop the v3 and v4 tails, mark as v2 and fix the CRC.
  size_t payload = (buf[6] | (buf[7] << 8)) - (2 + 2 * kSpeciesCount) - 2;
  buf[4] = 2; buf[5] = 0;
  buf[6] = payload & 0xFF; buf[7] = payload >> 8;
  uint32_t crc = crc32(buf, 8 + payload);
  for (int i = 0; i < 4; i++) buf[8 + payload + i] = (crc >> (8 * i)) & 0xFF;
  SaveData out;
  CHECK(save_decode(buf, 8 + payload + 4, out));
  CHECK(out.pet.species == 0);
  CHECK(out.collection.count[0] == 3);  // two in the graveyard plus this one
  CHECK(out.collection.best_stage[0] == static_cast<uint8_t>(Stage::Teen));
  CHECK(out.pet.egg_minutes == kDefaultEggMinutes);
}

TEST(egg_time_is_the_goal_and_locks_at_first_session) {
  Sim s;
  CHECK(s.game.choose_egg(60));
  CHECK(!s.game.choose_egg(45));  // not an egg size
  CHECK(s.game.target_minutes() == 60);
  CHECK(!s.game.egg_locked());
  s.start_running(kAll);
  CHECK(s.game.egg_locked());
  s.step(2 * kMin);
  s.game.finish(s.now);
  s.game.acknowledge();
  CHECK(!s.game.choose_egg(15));  // retained for this pet
  CHECK(s.game.target_minutes() == 60);
  CHECK(s.game.new_pet());
  CHECK(s.game.data().pet.generation == 2);
  CHECK(s.game.data().pet.egg_minutes == 60);  // previous setting is the default
  CHECK(s.game.choose_egg(15));
  CHECK(s.game.target_minutes() == 15);
}

TEST(egg_time_survives_save) {
  SaveData d = ready_save();
  d.pet.egg_minutes = 90;
  uint8_t buf[kSaveBufferSize];
  size_t n = save_encode(d, buf, sizeof buf);
  SaveData out;
  CHECK(save_decode(buf, n, out));
  CHECK(out.pet.egg_minutes == 90);
}

TEST(longer_eggs_hatch_rarer_pets) {
  auto rare_share = [](uint16_t minutes) {
    uint32_t x = 99;
    int rare = 0;
    for (int i = 0; i < 20000; i++) {
      x ^= x << 13; x ^= x >> 17; x ^= x << 5;
      rare += species_info(roll_species(x, minutes)).rarity >= Rarity::Rare;
    }
    return rare;
  };
  CHECK(rare_share(15) < rare_share(30));
  CHECK(rare_share(30) < rare_share(60));
  CHECK(rare_share(60) < rare_share(90));
  CHECK(egg_rare_percent(90) == 42);
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
  s.game.choose_egg(15);
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
  // kSpeciesCount stands in for an unknown id, which draws like a Blob.
  for (int id = 0; id <= kSpeciesCount; id++)
    for (int s = 0; s < static_cast<int>(Stage::Count); s++)
      for (int m = 0; m <= static_cast<int>(Mood::Dead); m++)
        for (uint32_t f = 0; f < 8; f++) {
          render_pet(static_cast<uint8_t>(id), static_cast<Stage>(s), static_cast<Mood>(m), f, sp);
          int opaque = 0;
          for (auto& row : sp)
            for (uint8_t px : row) {
              CHECK(px < kPaletteCount);
              opaque += px != kTransparent;
            }
          CHECK(opaque > 40);
        }
}

// ---- Rules v2: setup, daily minimum, bonus life, shame --------------------

TEST(fresh_box_starts_in_setup) {
  Sim s(Config{}, SaveData{});
  CHECK(s.game.state() == State::Setup);
  s.docked = kAll;
  s.step(6000);
  CHECK(s.game.state() == State::Setup);  // no autostart before setup
  CHECK(!s.game.start(s.now));
  Settings st = s.game.data().settings;
  st.num_players = 2;
  st.daily_minimum_min = 45;
  CHECK(s.game.complete_setup(st));
  CHECK(s.game.state() == State::Idle);
  CHECK(s.game.data().settings.setup_done);
  CHECK(s.game.num_players() == 2);
  CHECK(std::strcmp(s.game.data().settings.names[1], "Player 2") == 0);
}

TEST(only_configured_players_count) {
  Sim s(Config{}, ready_save(2));
  s.docked = 0b0011;  // both players docked -> autostart
  s.step(5500);
  CHECK(s.game.state() == State::Arming);
  s.docked = 0b1111;  // phones in unused slots are ignored
  s.step(10100);
  CHECK(s.game.armed_mask() == 0b0011);
  s.step(kMin);
  CHECK(s.game.data().pet.xp == xp_per_minute(2));
}

TEST(daily_minutes_are_credited_per_player) {
  Sim s(Config{}, ready_save(4));
  s.unix = kUnix0 + 3600;
  s.start_running(0b0101);
  s.step(20 * kMin);
  CHECK(s.game.data().daily.minutes[0] == 20);
  CHECK(s.game.data().daily.minutes[1] == 0);
  CHECK(s.game.data().daily.minutes[2] == 20);
}

TEST(missing_daily_minimum_costs_a_heart_and_shames) {
  Sim s(Config{}, ready_save(2, 30));
  s.unix = kUnix0 + 3600;
  s.start_running(0b0011);
  s.step(31 * kMin);
  s.docked = 0b0001;  // player 2 leaves via emergency pass... no, finish instead
  s.game.finish(s.now);
  s.game.acknowledge();
  s.docked = 0;
  // Player 1 and 2 both got 31 min today: tomorrow is a perfect day.
  s.jump_days(1);
  CHECK(s.count(EventType::DailyMissed) == 0);
  CHECK(s.game.data().life.perfect_days == 1);
  CHECK(s.game.data().life.daily_streak[0] == 1);

  // Next day only player 1 plays 30 min.
  s.start_running(0b0001);
  s.step(30 * kMin + 500);
  s.game.finish(s.now);
  s.game.acknowledge();
  s.docked = 0;
  uint8_t hearts = s.game.data().pet.hearts;
  s.jump_days(1);
  CHECK(s.count(EventType::DailyMissed) == 1);
  CHECK(s.game.data().pet.hearts == hearts - 1);
  CHECK(s.game.data().life.missed_days[1] == 1);
  CHECK(s.game.data().life.daily_streak[1] == 0);
  CHECK(s.game.data().life.daily_streak[0] == 2);
  CHECK(s.game.data().life.shame.slot == 1);
  CHECK(s.game.data().life.shame.reason == ShameReason::MissedDaily);
  char buf[96];
  CHECK(describe_shame(s.game, buf, sizeof buf));
  CHECK(std::strstr(buf, "Player 2") != nullptr);
}

TEST(each_missing_player_costs_a_heart_and_can_kill) {
  Sim s(Config{}, ready_save(3, 30));
  s.unix = kUnix0 + 3600;
  s.step(100);
  s.jump_days(1);  // nobody played: 3 players miss -> 3 hearts -> dead
  CHECK(s.game.state() == State::Result);
  CHECK(s.game.result() == ResultKind::Died);
  CHECK(s.game.result_cause() == ShameReason::MissedDaily);
  CHECK(s.game.culprit() == 0);
  CHECK(s.game.data().life.graveyard == 1);
  CHECK(s.game.acknowledge());
  CHECK(s.game.data().pet.generation == 2);
}

TEST(days_do_not_close_mid_session) {
  Sim s(Config{}, ready_save(1, 30));
  s.unix = kUnix0 + kDay - 20 * 60;  // 20 min before the reset
  s.start_running(0b0001);
  s.step(40 * kMin);  // runs past 04:00
  CHECK(s.count(EventType::DayClosed) == 0);
  CHECK(s.game.data().daily.minutes[0] >= 30);
  s.game.finish(s.now);
  s.game.acknowledge();
  s.step(100);
  CHECK(s.count(EventType::DayClosed) == 1);
  CHECK(s.count(EventType::DailyMissed) == 0);  // the minutes counted for the old day
}

TEST(no_clock_means_no_daily_rules) {
  Sim s(Config{}, ready_save(4, 30));
  s.step(3 * 24 * kHour / 1000);  // unix stays 0
  CHECK(s.count(EventType::DayClosed) == 0);
  CHECK(s.game.data().pet.hearts == kMaxHearts);
}

TEST(long_downtime_is_capped) {
  Sim s(Config{}, ready_save(1, 30));
  s.unix = kUnix0 + 3600;
  s.step(100);
  s.game.begin(s.game.data(), s.now, s.unix);
  // 30 days off with one player: only max_catchup_days (7) count, the pet
  // dies on day 3 and closing stops there.
  s.jump_days(30);
  CHECK(s.count(EventType::DayClosed) == 3);
  CHECK(s.game.result() == ResultKind::Died);
}

TEST(double_the_goal_gives_bonus_life_above_max) {
  Sim s(Config{}, ready_save(2, 0));
  s.unix = kUnix0 + 3600;
  s.game.choose_egg(15);
  s.start_running(0b0011);
  CHECK(s.game.bonus_goal_minutes() == 30);
  s.step(15 * kMin + 1000);
  CHECK(s.game.target_reached());
  CHECK(s.count(EventType::BonusLife) == 0);
  s.step(15 * kMin);
  CHECK(s.count(EventType::BonusLife) == 1);
  CHECK(s.game.bonus_earned());
  CHECK(s.game.data().pet.hearts == kMaxHearts + 1);
  CHECK(s.game.data().life.bonus_lives == 1);
  s.step(kHour);
  CHECK(s.count(EventType::BonusLife) == 1);  // once per session
  s.game.finish(s.now);
  s.game.acknowledge();
  // Same day again: no second bonus.
  s.start_running(0b0011);
  s.step(31 * kMin);
  CHECK(s.count(EventType::BonusLife) == 1);
  CHECK(!s.game.bonus_earned());
  s.game.finish(s.now);
  s.game.acknowledge();
  // Next day it works again.
  s.docked = 0;
  s.jump_days(1);
  s.start_running(0b0011);
  s.step(31 * kMin);
  CHECK(s.count(EventType::BonusLife) == 2);
  CHECK(s.game.data().pet.hearts == kMaxHearts + 2);
}

TEST(free_play_bonus_after_an_hour) {
  Sim s(Config{}, ready_save(3, 0));
  s.start_running(0b0011);  // late or missing players don't matter any more
  CHECK(s.game.bonus_goal_minutes() == 60);
  s.step(59 * kMin);
  CHECK(s.count(EventType::BonusLife) == 0);
  s.step(1 * kMin + 1000);
  CHECK(s.count(EventType::BonusLife) == 1);
}

TEST(stopping_between_goal_and_double_is_a_plain_success) {
  Sim s(Config{}, ready_save(2, 0));
  s.game.choose_egg(30);
  s.start_running(0b0011);
  s.step(45 * kMin);
  s.docked = 0b0001;  // after the goal, lifting just ends it
  s.step(1000);
  CHECK(s.game.result() == ResultKind::Success);
  CHECK(s.count(EventType::BonusLife) == 0);
  char buf[128];
  Event e{EventType::TargetReached, kNoSlot, 30};
  CHECK(describe_event(e, s.game, buf, sizeof buf));
  CHECK(std::strstr(buf, "60 min for an extra life") != nullptr);
}

TEST(bonus_hearts_cap_at_five) {
  SaveData d = ready_save(1, 0);
  d.pet.hearts = kMaxBonusHearts;
  Sim s(Config{}, d);
  s.start_running(0b0001);
  s.step(kHour + 1000);
  CHECK(s.count(EventType::BonusLife) == 0);
  CHECK(s.game.data().pet.hearts == kMaxBonusHearts);
}

TEST(success_clears_the_wall_of_shame) {
  Sim s(Config{}, ready_save(2, 0));
  s.start_running(0b0011);
  s.docked = 0b0001;
  s.step(1000);
  CHECK(s.game.data().life.shame.reason == ShameReason::Lift);
  CHECK(s.game.data().life.shame.slot == 1);
  s.game.acknowledge();
  s.start_running(0b0011);
  s.step(10 * kMin);
  s.game.finish(s.now);
  CHECK(s.game.data().life.shame.reason == ShameReason::None);
}

TEST(player_names_are_used_in_messages) {
  Sim s(Config{}, ready_save(2, 0));
  CHECK(s.game.set_player_name(1, "Anna\"Bad"));
  CHECK(std::strcmp(s.game.data().settings.names[1], "Anna_Bad") == 0);
  s.start_running(0b0011);
  s.docked = 0b0001;
  s.step(1000);
  Event e{EventType::SessionEnded, 1, static_cast<uint32_t>(ResultKind::Broken)};
  char buf[128];
  CHECK(describe_event(e, s.game, buf, sizeof buf));
  CHECK(std::strstr(buf, "Anna_Bad picked up") != nullptr);
}

TEST(v1_save_migrates_to_setup) {
  // Hand-built v1 save: header + v1 payload + CRC.
  SaveData d = ready_save();
  d.pet.xp = 777;
  d.life.breaks[2] = 4;
  uint8_t full[kSaveBufferSize];
  size_t n = save_encode(d, full, sizeof full);
  CHECK(n > 0);
  // The v1 payload is a prefix of v2: truncate it and fix up the header + CRC.
  const size_t v1_payload = 4 + 1 + 1 + 1 + 2 + 4 + 4 + 4 + 2 * 5 + 2 * kMaxSlots + 4 + 1 + 1 + 2;
  uint8_t v1[kSaveBufferSize];
  std::memcpy(v1, full, 8 + v1_payload);
  v1[4] = 1; v1[5] = 0;
  v1[6] = v1_payload & 0xFF; v1[7] = v1_payload >> 8;
  uint32_t crc = crc32(v1, 8 + v1_payload);
  for (int i = 0; i < 4; i++) v1[8 + v1_payload + i] = (crc >> (8 * i)) & 0xFF;
  SaveData out;
  CHECK(save_decode(v1, 8 + v1_payload + 4, out));
  CHECK(out.pet.xp == 777);
  CHECK(out.life.breaks[2] == 4);
  CHECK(!out.settings.setup_done);
  CHECK(out.settings.num_players == kMaxSlots);
  Game g;
  g.begin(out, 0);
  CHECK(g.state() == State::Setup);
}

TEST(v2_settings_roundtrip) {
  SaveData d = ready_save(3, 45);
  d.settings.tz_offset_min = -300;
  d.settings.pin = 4321;
  std::strcpy(d.settings.names[0], "Åsa");
  d.daily.day = 20000;
  d.daily.minutes[2] = 17;
  d.life.missed_days[1] = 2;
  d.life.shame = Shame{1, ShameReason::MissedDaily, 1767300000};
  uint8_t buf[kSaveBufferSize];
  size_t n = save_encode(d, buf, sizeof buf);
  SaveData out;
  CHECK(save_decode(buf, n, out));
  CHECK(out.settings.num_players == 3);
  CHECK(out.settings.daily_minimum_min == 45);
  CHECK(out.settings.tz_offset_min == -300);
  CHECK(out.settings.pin == 4321);
  CHECK(std::strcmp(out.settings.names[0], "Åsa") == 0);
  CHECK(out.daily.day == 20000);
  CHECK(out.daily.minutes[2] == 17);
  CHECK(out.life.missed_days[1] == 2);
  CHECK(out.life.shame.reason == ShameReason::MissedDaily);
}

TEST(day_index_uses_reset_hour_and_timezone) {
  Settings st;
  st.day_reset_hour = 4;
  CHECK(day_index(kUnix0, st) == day_index(kUnix0 + kDay - 1, st));
  CHECK(day_index(kUnix0 - 1, st) + 1 == day_index(kUnix0, st));
  st.tz_offset_min = 120;  // UTC+2: the day starts at 02:00 UTC
  CHECK(day_index(kUnix0 - 2 * 3600, st) == day_index(kUnix0 - 2 * 3600 + kDay - 1, st));
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
