// Session state machine: docking, arming, running, breaking and results.
// Time is passed in (milliseconds since boot, plus optional unix time) so the
// same code runs on the GIGA, in unit tests and in the desktop simulator.
#pragma once
#include <cstdint>

#include "pet.h"
#include "save.h"

namespace petcore {

enum class State : uint8_t { Setup, Idle, Arming, Running, Result };
enum class ResultKind : uint8_t { None, Success, Aborted, Broken, Died };

enum class EventType : uint8_t {
  ArmingStarted,
  ArmingCancelled,
  SessionStarted,
  PlayerJoined,      // slot
  Evolved,           // value = new stage
  TargetReached,     // value = minutes
  EmergencyArmed,    // value = window ms
  EmergencyUsed,     // slot
  EmergencyExpired,
  SessionEnded,      // value = ResultKind, slot = culprit
  Hatched,           // value = generation
  DailyMissed,       // slot, value = minutes they managed
  DayClosed,         // value = mask of players who missed
  BonusLife,         // value = hearts now
  SetupDone,
  NewSpecies,        // value = species, first time the box hatched one
  NightStarted,      // value = hour the pet wakes up
  Morning,           // the pet woke up
};

struct Event {
  EventType type;
  uint8_t slot = kNoSlot;
  uint32_t value = 0;
};

// Defaults come from tuning.h; tests and the simulator override some.
struct Config {
  uint8_t num_slots = kMaxSlots;
  uint32_t arming_ms = kArmingMs;
  uint32_t autostart_ms = kAutostartMs;
  uint32_t lift_tolerance_ms = kLiftToleranceMs;
  uint32_t emergency_window_ms = kEmergencyWindowMs;
  uint32_t min_success_minutes = kMinSuccessMinutes;
  uint32_t result_timeout_ms = kResultTimeoutMs;
  uint32_t save_every_minutes = kSaveEveryMinutes;
  uint32_t free_play_bonus_minutes = kFreePlayBonusMinutes;
  uint32_t max_catchup_days = kMaxCatchupDays;
};

// Day number for daily rules: changes at settings.day_reset_hour local time.
uint32_t day_index(uint32_t unix, const Settings& settings);

class Game {
 public:
  explicit Game(const Config& cfg = Config{});

  // Load persisted state. A session that was still active means the power
  // was cut mid-session, which counts as a break.
  void begin(const SaveData& loaded, uint32_t now_ms, uint32_t now_unix = 0);
  // Mix hardware noise into the species roll (optional; tests leave it out).
  void seed(uint32_t entropy);
  void update(uint32_t now_ms, uint8_t docked_mask, uint32_t now_unix = 0);

  // Touchscreen actions. Each returns false if not allowed right now.
  bool start(uint32_t now_ms);
  bool cancel();
  bool request_emergency(uint32_t now_ms);
  bool finish(uint32_t now_ms);
  bool acknowledge();
  // Pick the egg's time setting (one of kEggs, or 0 for free play). Allowed
  // while Idle until the egg's first session; then it's every session's goal
  // until the pet is gone.
  bool choose_egg(uint16_t minutes);
  // Retire the current pet and start a fresh egg (Idle only). The egg keeps
  // the previous time setting until a new one is chosen.
  bool new_pet();
  // Finish (or redo) setup: player count, daily minimum, day reset, time zone.
  // Today's progress is kept when redoing setup.
  bool complete_setup(const Settings& settings);
  // Change settings without leaving the current Setup/Idle state (app, box menu).
  bool update_settings(const Settings& settings);
  bool set_player_name(uint8_t slot, const char* name);

  bool poll_event(Event& out);
  // True once after anything worth persisting changed.
  bool take_dirty();

  // ---- View -----------------------------------------------------------
  const SaveData& data() const { return data_; }
  const Config& config() const { return cfg_; }
  State state() const { return state_; }
  ResultKind result() const { return result_; }
  uint8_t culprit() const { return culprit_; }
  uint8_t docked_mask() const { return docked_; }
  uint8_t armed_mask() const { return armed_; }
  uint8_t lifting_mask() const;  // armed slots currently open (within tolerance)
  uint16_t target_minutes() const { return data_.pet.egg_minutes; }
  // The egg's time setting can no longer change (its first session started).
  bool egg_locked() const;
  bool target_reached() const { return target_reached_; }
  bool emergency_available() const { return !emergency_used_ && state_ == State::Running; }
  bool emergency_window_open() const { return emergency_window_; }
  uint32_t emergency_remaining_ms(uint32_t now_ms) const;
  uint32_t arming_remaining_ms(uint32_t now_ms) const;
  uint32_t session_elapsed_ms(uint32_t now_ms) const;
  uint32_t session_minutes() const { return minutes_awarded_; }
  uint32_t session_xp() const { return session_xp_; }
  uint32_t session_bonus_xp() const { return bonus_xp_; }
  bool finish_counts_as_success() const;
  Mood mood() const;
  uint8_t num_players() const { return data_.settings.num_players; }
  // Why the result screen is showing: Lift, PowerCut or MissedDaily.
  ShameReason result_cause() const { return cause_; }
  uint8_t last_missed_mask() const { return last_missed_mask_; }
  bool clock_known() const { return now_unix_ != 0; }
  // Night hours (tuning.h): the pet sleeps and no session can run.
  bool is_night() const;
  uint32_t seconds_until_day_end() const;
  // The extra-life challenge: keep the session going to double the target
  // (free play: free_play_bonus_minutes). One bonus life per day.
  uint32_t bonus_goal_minutes() const;
  bool bonus_earned() const { return bonus_earned_; }
  bool bonus_available_today() const;

 private:
  void emit(EventType type, uint8_t slot = kNoSlot, uint32_t value = 0);
  void add_xp(uint32_t xp);
  uint32_t next_random();
  void award_minutes(uint32_t now_ms);
  void end_session(ResultKind kind, uint8_t culprit, uint32_t now_ms);
  void enter_running(uint32_t now_ms);
  void track_day(uint32_t now_ms);
  void close_day(uint32_t now_ms);
  uint8_t all_slots() const;

  Config cfg_;
  SaveData data_;
  State state_ = State::Idle;
  ResultKind result_ = ResultKind::None;
  uint8_t culprit_ = kNoSlot;

  uint8_t docked_ = 0;
  uint8_t armed_ = 0;
  uint32_t open_since_[kMaxSlots] = {};
  uint8_t open_mask_ = 0;  // armed slots currently seen open

  uint32_t all_docked_since_ = 0;
  bool all_docked_ = false;
  bool autostart_ready_ = true;
  uint32_t arming_started_ = 0;
  uint32_t session_start_ = 0;
  uint32_t session_end_ = 0;
  uint32_t minutes_awarded_ = 0;
  uint32_t session_xp_ = 0;
  uint32_t bonus_xp_ = 0;
  bool target_reached_ = false;
  bool emergency_used_ = false;
  bool emergency_window_ = false;
  uint32_t emergency_started_ = 0;
  uint32_t result_at_ = 0;
  uint32_t now_unix_ = 0;
  bool dirty_ = false;
  ShameReason cause_ = ShameReason::None;
  uint8_t last_missed_mask_ = 0;
  bool bonus_earned_ = false;
  bool night_ = false;
  uint32_t rng_ = 0x9E3779B9u;

  static constexpr int kQueueSize = 16;
  Event queue_[kQueueSize];
  int q_head_ = 0;
  int q_count_ = 0;
};

}  // namespace petcore
