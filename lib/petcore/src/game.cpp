#include "game.h"

namespace petcore {

namespace {
int popcount(uint8_t v) {
  int n = 0;
  for (; v; v &= v - 1) n++;
  return n;
}
constexpr uint32_t kMinuteMs = 60000;
constexpr uint32_t kDaySeconds = 86400;
}  // namespace

Game::Game(const Config& cfg) : cfg_(cfg) {
  if (cfg_.num_slots < 1 || cfg_.num_slots > kMaxSlots) cfg_.num_slots = kMaxSlots;
}

void Game::begin(const SaveData& loaded, uint32_t now_ms, uint32_t now_unix) {
  data_ = loaded;
  state_ = State::Idle;
  result_ = ResultKind::None;
  culprit_ = kNoSlot;
  now_unix_ = now_unix;

  if (now_unix != 0) {
    uint32_t& seen = data_.life.last_seen_unix;
    if (seen == 0 || now_unix < seen) {
      seen = now_unix;
      dirty_ = true;
    } else if (uint32_t days = (now_unix - seen) / kDaySeconds) {
      pet_apply_absence(data_.pet, days);
      seen += days * kDaySeconds;
      dirty_ = true;
    }
  }

  if (data_.session.active) {
    // Rebooted mid-session: someone pulled the plug.
    armed_ = data_.session.armed_mask;
    minutes_awarded_ = data_.session.minutes;
    session_xp_ = 0;
    bonus_xp_ = 0;
    session_start_ = now_ms - minutes_awarded_ * kMinuteMs;
    end_session(ResultKind::Broken, kNoSlot, now_ms);
  } else if (data_.pet.hearts == 0) {
    // Died but the result screen was never acknowledged.
    state_ = State::Result;
    result_ = ResultKind::Died;
    result_at_ = now_ms;
  }
}

void Game::update(uint32_t now_ms, uint8_t docked_mask, uint32_t now_unix) {
  if (now_unix != 0) now_unix_ = now_unix;
  docked_ = docked_mask & all_slots();

  switch (state_) {
    case State::Idle: {
      if (docked_ != all_slots()) {
        all_docked_ = false;
        autostart_ready_ = true;
      } else if (!all_docked_) {
        all_docked_ = true;
        all_docked_since_ = now_ms;
      } else if (autostart_ready_ && cfg_.autostart_ms > 0 &&
                 now_ms - all_docked_since_ >= cfg_.autostart_ms) {
        start(now_ms);
      }
      break;
    }

    case State::Arming:
      if (armed_ & ~docked_) {
        cancel();
        break;
      }
      armed_ |= docked_;  // late joiners are welcome during the countdown
      if (now_ms - arming_started_ >= cfg_.arming_ms) enter_running(now_ms);
      break;

    case State::Running: {
      award_minutes(now_ms);

      for (int s = 0; s < cfg_.num_slots; s++) {
        uint8_t bit = 1u << s;
        if ((docked_ & bit) && !(armed_ & bit)) {
          armed_ |= bit;
          data_.session.armed_mask = armed_;
          dirty_ = true;
          emit(EventType::PlayerJoined, s);
        }
      }

      if (emergency_window_ && now_ms - emergency_started_ >= cfg_.emergency_window_ms) {
        emergency_window_ = false;
        emit(EventType::EmergencyExpired);
      }

      uint8_t culprit = kNoSlot;
      uint32_t culprit_since = 0;
      for (int s = 0; s < cfg_.num_slots; s++) {
        uint8_t bit = 1u << s;
        if (!(armed_ & bit) || (docked_ & bit)) {
          open_mask_ &= ~bit;
          continue;
        }
        if (!(open_mask_ & bit)) {
          open_mask_ |= bit;
          open_since_[s] = now_ms;
        }
        if (emergency_window_) {
          // The emergency pass lets exactly one player leave without penalty.
          armed_ &= ~bit;
          open_mask_ &= ~bit;
          emergency_window_ = false;
          emergency_used_ = true;
          data_.session.armed_mask = armed_;
          dirty_ = true;
          emit(EventType::EmergencyUsed, s);
          continue;
        }
        uint32_t open_for = now_ms - open_since_[s];
        if (open_for >= cfg_.lift_tolerance_ms &&
            (culprit == kNoSlot || open_since_[s] < culprit_since)) {
          culprit = static_cast<uint8_t>(s);
          culprit_since = open_since_[s];
        }
      }

      if (culprit != kNoSlot) {
        // After the target is reached, picking up your phone just ends the session.
        end_session(target_reached_ ? ResultKind::Success : ResultKind::Broken, culprit, now_ms);
      } else if (armed_ == 0) {
        end_session(finish_counts_as_success() ? ResultKind::Success : ResultKind::Aborted,
                    kNoSlot, now_ms);
      }
      break;
    }

    case State::Result:
      if (cfg_.result_timeout_ms > 0 && now_ms - result_at_ >= cfg_.result_timeout_ms) acknowledge();
      break;
  }
}

bool Game::start(uint32_t now_ms) {
  if (state_ != State::Idle || docked_ == 0) return false;
  armed_ = docked_;
  arming_started_ = now_ms;
  autostart_ready_ = false;
  state_ = State::Arming;
  emit(EventType::ArmingStarted);
  return true;
}

bool Game::cancel() {
  if (state_ != State::Arming) return false;
  armed_ = 0;
  state_ = State::Idle;
  emit(EventType::ArmingCancelled);
  return true;
}

bool Game::request_emergency(uint32_t now_ms) {
  if (state_ != State::Running || emergency_used_ || emergency_window_) return false;
  emergency_window_ = true;
  emergency_started_ = now_ms;
  emit(EventType::EmergencyArmed, kNoSlot, cfg_.emergency_window_ms);
  return true;
}

bool Game::finish(uint32_t now_ms) {
  if (state_ != State::Running) return false;
  award_minutes(now_ms);
  end_session(finish_counts_as_success() ? ResultKind::Success : ResultKind::Aborted, kNoSlot,
              now_ms);
  return true;
}

bool Game::acknowledge() {
  if (state_ != State::Result) return false;
  if (result_ == ResultKind::Died || data_.pet.hearts == 0) {
    pet_hatch_new(data_.pet, static_cast<uint16_t>(data_.pet.generation + 1));
    emit(EventType::Hatched, kNoSlot, data_.pet.generation);
    dirty_ = true;
  }
  state_ = State::Idle;
  result_ = ResultKind::None;
  culprit_ = kNoSlot;
  autostart_ready_ = false;  // phones still docked shouldn't instantly re-arm
  return true;
}

bool Game::poll_event(Event& out) {
  if (q_count_ == 0) return false;
  out = queue_[q_head_];
  q_head_ = (q_head_ + 1) % kQueueSize;
  q_count_--;
  return true;
}

bool Game::take_dirty() {
  bool d = dirty_;
  dirty_ = false;
  return d;
}

uint8_t Game::lifting_mask() const { return state_ == State::Running ? (open_mask_ & armed_) : 0; }

uint32_t Game::emergency_remaining_ms(uint32_t now_ms) const {
  if (!emergency_window_) return 0;
  uint32_t used = now_ms - emergency_started_;
  return used >= cfg_.emergency_window_ms ? 0 : cfg_.emergency_window_ms - used;
}

uint32_t Game::arming_remaining_ms(uint32_t now_ms) const {
  if (state_ != State::Arming) return 0;
  uint32_t used = now_ms - arming_started_;
  return used >= cfg_.arming_ms ? 0 : cfg_.arming_ms - used;
}

uint32_t Game::session_elapsed_ms(uint32_t now_ms) const {
  if (state_ == State::Running) return now_ms - session_start_;
  if (state_ == State::Result) return session_end_ - session_start_;
  return 0;
}

bool Game::finish_counts_as_success() const {
  return target_reached_ || minutes_awarded_ >= cfg_.min_success_minutes;
}

Mood Game::mood() const {
  if (state_ == State::Result) {
    if (result_ == ResultKind::Died) return Mood::Dead;
    if (result_ == ResultKind::Broken) return Mood::Crying;
  }
  return pet_mood(data_.pet);
}

void Game::emit(EventType type, uint8_t slot, uint32_t value) {
  if (q_count_ == kQueueSize) {  // drop the oldest rather than the newest
    q_head_ = (q_head_ + 1) % kQueueSize;
    q_count_--;
  }
  queue_[(q_head_ + q_count_) % kQueueSize] = Event{type, slot, value};
  q_count_++;
}

void Game::award_minutes(uint32_t now_ms) {
  uint32_t minutes = (now_ms - session_start_) / kMinuteMs;
  while (minutes_awarded_ < minutes) {
    minutes_awarded_++;
    uint32_t xp = xp_per_minute(popcount(armed_));
    session_xp_ += xp;
    data_.pet.minutes++;
    data_.life.total_minutes++;
    data_.session.minutes = static_cast<uint16_t>(minutes_awarded_);
    if (pet_add_xp(data_.pet, xp))
      emit(EventType::Evolved, kNoSlot, static_cast<uint32_t>(data_.pet.stage));
    if (target_minutes_ > 0 && !target_reached_ && minutes_awarded_ >= target_minutes_) {
      target_reached_ = true;
      emit(EventType::TargetReached, kNoSlot, target_minutes_);
    }
    if (cfg_.save_every_minutes > 0 && minutes_awarded_ % cfg_.save_every_minutes == 0)
      dirty_ = true;
  }
}

void Game::enter_running(uint32_t now_ms) {
  state_ = State::Running;
  session_start_ = now_ms;
  minutes_awarded_ = 0;
  session_xp_ = 0;
  bonus_xp_ = 0;
  target_reached_ = false;
  emergency_used_ = false;
  emergency_window_ = false;
  open_mask_ = 0;
  data_.session.active = true;
  data_.session.armed_mask = armed_;
  data_.session.minutes = 0;
  dirty_ = true;
  emit(EventType::SessionStarted);
}

void Game::end_session(ResultKind kind, uint8_t culprit, uint32_t now_ms) {
  Lifetime& life = data_.life;
  switch (kind) {
    case ResultKind::Success:
      bonus_xp_ = session_xp_ * kCompletionBonusPercent / 100 + (target_reached_ ? target_minutes_ : 0);
      if (pet_add_xp(data_.pet, bonus_xp_))
        emit(EventType::Evolved, kNoSlot, static_cast<uint32_t>(data_.pet.stage));
      pet_on_success(data_.pet, minutes_awarded_);
      life.sessions_success++;
      life.streak++;
      if (life.streak > life.best_streak) life.best_streak = life.streak;
      if (minutes_awarded_ > life.longest_session_min) life.longest_session_min = minutes_awarded_;
      break;
    case ResultKind::Broken:
      life.sessions_broken++;
      life.streak = 0;
      if (culprit < kMaxSlots) life.breaks[culprit]++;
      if (pet_on_break(data_.pet)) {
        kind = ResultKind::Died;
        life.graveyard++;
      }
      break;
    default:
      break;
  }

  data_.session = ActiveSession{};
  if (now_unix_ != 0) life.last_seen_unix = now_unix_;
  state_ = State::Result;
  result_ = kind;
  culprit_ = culprit;
  result_at_ = now_ms;
  session_end_ = now_ms;
  armed_ = 0;
  open_mask_ = 0;
  emergency_window_ = false;
  dirty_ = true;
  emit(EventType::SessionEnded, culprit, static_cast<uint32_t>(kind));
}

}  // namespace petcore
