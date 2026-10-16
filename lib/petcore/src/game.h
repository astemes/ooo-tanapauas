// Session state machine: docking, arming, running, breaking and results.
// Time is passed in (milliseconds since boot, plus optional unix time) so the
// same code runs on the GIGA, in unit tests and in the desktop simulator.
#pragma once
#include <cstdint>

#include "pet.h"
#include "save.h"

namespace petcore {

enum class State : uint8_t { Idle, Arming, Running, Result };
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
};

struct Event {
  EventType type;
  uint8_t slot = kNoSlot;
  uint32_t value = 0;
};

struct Config {
  uint8_t num_slots = kMaxSlots;
  uint32_t arming_ms = 10000;
  uint32_t autostart_ms = 5000;          // all slots docked this long -> start
  uint32_t lift_tolerance_ms = 750;      // ignore bumps shorter than this
  uint32_t emergency_window_ms = 30000;  // time to lift after using the pass
  uint32_t min_success_minutes = 10;     // shorter "Finish" counts as aborted
  uint32_t result_timeout_ms = 120000;   // result screen auto-dismisses
  uint32_t save_every_minutes = 5;
};

class Game {
 public:
  explicit Game(const Config& cfg = Config{});

  // Load persisted state. A session that was still active means the power
  // was cut mid-session, which counts as a break.
  void begin(const SaveData& loaded, uint32_t now_ms, uint32_t now_unix = 0);
  void update(uint32_t now_ms, uint8_t docked_mask, uint32_t now_unix = 0);

  // Touchscreen actions. Each returns false if not allowed right now.
  bool start(uint32_t now_ms);
  bool cancel();
  bool request_emergency(uint32_t now_ms);
  bool finish(uint32_t now_ms);
  bool acknowledge();
  void set_target_minutes(uint16_t minutes) { target_minutes_ = minutes; }

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
  uint16_t target_minutes() const { return target_minutes_; }
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

 private:
  void emit(EventType type, uint8_t slot = kNoSlot, uint32_t value = 0);
  void award_minutes(uint32_t now_ms);
  void end_session(ResultKind kind, uint8_t culprit, uint32_t now_ms);
  void enter_running(uint32_t now_ms);
  uint8_t all_slots() const { return static_cast<uint8_t>((1u << cfg_.num_slots) - 1); }

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
  uint16_t target_minutes_ = 0;  // 0 = free play
  bool target_reached_ = false;
  bool emergency_used_ = false;
  bool emergency_window_ = false;
  uint32_t emergency_started_ = 0;
  uint32_t result_at_ = 0;
  uint32_t now_unix_ = 0;
  bool dirty_ = false;

  static constexpr int kQueueSize = 16;
  Event queue_[kQueueSize];
  int q_head_ = 0;
  int q_count_ = 0;
};

}  // namespace petcore
