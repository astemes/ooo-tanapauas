#include "game.h"

#include <cstring>

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

uint32_t day_index(uint32_t unix, const Settings& settings) {
  int64_t local = static_cast<int64_t>(unix) + settings.tz_offset_min * 60 -
                  static_cast<int64_t>(settings.day_reset_hour) * 3600;
  return local > 0 ? static_cast<uint32_t>(local / kDaySeconds) : 0;
}

Game::Game(const Config& cfg) : cfg_(cfg) {
  if (cfg_.num_slots < 1 || cfg_.num_slots > kMaxSlots) cfg_.num_slots = kMaxSlots;
}

void Game::seed(uint32_t entropy) {
  rng_ ^= entropy * 0x85EBCA6Bu;
  if (rng_ == 0) rng_ = 0x9E3779B9u;
}

uint32_t Game::next_random() {  // xorshift32
  rng_ ^= rng_ << 13;
  rng_ ^= rng_ >> 17;
  rng_ ^= rng_ << 5;
  return rng_;
}

// All XP goes through here: hatching rolls the species and fills the collection.
void Game::add_xp(uint32_t xp) {
  Pet& pet = data_.pet;
  Stage before = pet.stage;
  if (!pet_add_xp(pet, xp)) return;
  Collection& col = data_.collection;
  if (before == Stage::Egg) {
    pet.species = roll_species(next_random(), pet.egg_minutes);
    bool first = col.count[pet.species] == 0;
    if (col.count[pet.species] < 255) col.count[pet.species]++;
    if (first) emit(EventType::NewSpecies, kNoSlot, pet.species);
  }
  if (pet.species < kSpeciesCount && static_cast<uint8_t>(pet.stage) > col.best_stage[pet.species])
    col.best_stage[pet.species] = static_cast<uint8_t>(pet.stage);
  emit(EventType::Evolved, kNoSlot, static_cast<uint32_t>(pet.stage));
  dirty_ = true;
}

void Game::begin(const SaveData& loaded, uint32_t now_ms, uint32_t now_unix) {
  data_ = loaded;
  seed(now_ms ^ now_unix ^ (static_cast<uint32_t>(data_.pet.generation) << 20) ^ data_.life.total_minutes);
  settings_sanitize(data_.settings);
  state_ = State::Idle;
  cause_ = ShameReason::None;
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
    cause_ = data_.life.shame.reason;
    culprit_ = data_.life.shame.slot;
  }
  if (!data_.settings.setup_done && state_ == State::Idle) state_ = State::Setup;
}

void Game::update(uint32_t now_ms, uint8_t docked_mask, uint32_t now_unix) {
  if (now_unix != 0) now_unix_ = now_unix;
  docked_ = docked_mask & all_slots();
  if (state_ == State::Setup) return;
  track_day(now_ms);

  switch (state_) {
    case State::Setup:
      break;

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

bool Game::egg_locked() const {
  const Pet& pet = data_.pet;
  bool in_session = state_ == State::Arming || state_ == State::Running;
  return in_session || pet.stage != Stage::Egg || pet.xp > 0 || pet.minutes > 0;
}

bool Game::choose_egg(uint16_t minutes) {
  if (state_ != State::Idle || egg_locked()) return false;
  if (minutes != 0 && !is_egg_minutes(minutes)) return false;
  if (data_.pet.egg_minutes != minutes) {
    data_.pet.egg_minutes = minutes;
    dirty_ = true;
  }
  return true;
}

bool Game::new_pet() {
  if (state_ != State::Idle) return false;
  pet_hatch_new(data_.pet, static_cast<uint16_t>(data_.pet.generation + 1));
  emit(EventType::Hatched, kNoSlot, data_.pet.generation);
  dirty_ = true;
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
  cause_ = ShameReason::None;
  last_missed_mask_ = 0;
  autostart_ready_ = false;  // phones still docked shouldn't instantly re-arm
  return true;
}

bool Game::complete_setup(const Settings& settings) {
  if (state_ != State::Setup && state_ != State::Idle) return false;
  bool first_time = !data_.settings.setup_done;
  Settings next = settings;
  next.setup_done = true;
  settings_sanitize(next);
  data_.settings = next;
  // Start daily tracking fresh so time before the first setup is never penalised.
  if (first_time) data_.daily = DailyProgress{};
  state_ = State::Idle;
  autostart_ready_ = false;
  dirty_ = true;
  emit(EventType::SetupDone);
  return true;
}

bool Game::update_settings(const Settings& settings) {
  if (state_ != State::Setup && state_ != State::Idle) return false;
  Settings next = settings;
  next.setup_done = data_.settings.setup_done;
  settings_sanitize(next);
  // A new reset hour or time zone shifts day numbering: restart counting from today.
  if (next.day_reset_hour != data_.settings.day_reset_hour || next.tz_offset_min != data_.settings.tz_offset_min)
    data_.daily.day = 0;
  data_.settings = next;
  dirty_ = true;
  return true;
}

bool Game::set_player_name(uint8_t slot, const char* name) {
  if (slot >= kMaxSlots || name == nullptr) return false;
  char* dst = data_.settings.names[slot];
  std::strncpy(dst, name, kNameLen);
  dst[kNameLen] = '\0';
  settings_sanitize(data_.settings);
  dirty_ = true;
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

uint8_t Game::all_slots() const {
  int n = data_.settings.num_players < cfg_.num_slots ? data_.settings.num_players : cfg_.num_slots;
  return static_cast<uint8_t>((1u << n) - 1);
}

uint32_t Game::seconds_until_day_end() const {
  if (now_unix_ == 0) return 0;
  const Settings& st = data_.settings;
  int64_t local = static_cast<int64_t>(now_unix_) + st.tz_offset_min * 60 -
                  static_cast<int64_t>(st.day_reset_hour) * 3600;
  return kDaySeconds - static_cast<uint32_t>(local % kDaySeconds);
}

uint32_t Game::bonus_goal_minutes() const {
  uint16_t goal = target_minutes();
  return goal ? 2u * goal : cfg_.free_play_bonus_minutes;
}

bool Game::bonus_available_today() const {
  return data_.daily.day == 0 || data_.daily.bonus_day != data_.daily.day;
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
    for (int p = 0; p < kMaxSlots; p++)
      if ((armed_ & all_slots() & (1u << p)) && data_.daily.minutes[p] < 0xFFFF) data_.daily.minutes[p]++;
    add_xp(xp);
    if (target_minutes() > 0 && !target_reached_ && minutes_awarded_ >= target_minutes()) {
      target_reached_ = true;
      emit(EventType::TargetReached, kNoSlot, target_minutes());
    }
    if (!bonus_earned_ && minutes_awarded_ >= bonus_goal_minutes() && bonus_available_today() &&
        pet_add_bonus_heart(data_.pet)) {
      bonus_earned_ = true;
      if (data_.daily.day != 0) data_.daily.bonus_day = data_.daily.day;
      data_.life.bonus_lives++;
      dirty_ = true;
      emit(EventType::BonusLife, kNoSlot, data_.pet.hearts);
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
  bonus_earned_ = false;
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
      bonus_xp_ = session_xp_ * kCompletionBonusPercent / 100 + (target_reached_ ? target_minutes() : 0);
      add_xp(bonus_xp_);
      pet_on_success(data_.pet, minutes_awarded_);
      life.sessions_success++;
      life.streak++;
      if (life.streak > life.best_streak) life.best_streak = life.streak;
      if (minutes_awarded_ > life.longest_session_min) life.longest_session_min = minutes_awarded_;
      life.shame = Shame{};  // a good session wipes the wall of shame
      break;
    case ResultKind::Broken:
      life.sessions_broken++;
      life.streak = 0;
      if (culprit < kMaxSlots) life.breaks[culprit]++;
      cause_ = culprit < kMaxSlots ? ShameReason::Lift : ShameReason::PowerCut;
      life.shame = Shame{culprit, cause_, now_unix_};
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

namespace petcore {

void Game::track_day(uint32_t now_ms) {
  if (now_unix_ == 0 || !data_.settings.setup_done) return;
  uint32_t today = day_index(now_unix_, data_.settings);
  DailyProgress& daily = data_.daily;
  if (daily.day == 0) {
    daily.day = today;
    dirty_ = true;
    return;
  }
  // Days only close between sessions; a session over the reset hour still
  // counts towards the day it started in.
  if (today <= daily.day || state_ != State::Idle) return;

  uint32_t days = today - daily.day;
  if (days > cfg_.max_catchup_days) days = cfg_.max_catchup_days;
  for (uint32_t i = 0; i < days && state_ == State::Idle; i++) {
    close_day(now_ms);
    daily.day++;
  }
  daily.day = today;
  dirty_ = true;
}

void Game::close_day(uint32_t now_ms) {
  DailyProgress& daily = data_.daily;
  Lifetime& life = data_.life;
  uint16_t minimum = data_.settings.daily_minimum_min;
  uint8_t missed = 0;

  for (int p = 0; p < data_.settings.num_players; p++) {
    if (minimum == 0) break;
    if (daily.minutes[p] >= minimum) {
      life.daily_streak[p]++;
      if (life.daily_streak[p] > life.best_daily_streak[p]) life.best_daily_streak[p] = life.daily_streak[p];
    } else {
      life.daily_streak[p] = 0;
      life.missed_days[p]++;
      missed |= 1u << p;
      emit(EventType::DailyMissed, static_cast<uint8_t>(p), daily.minutes[p]);
    }
  }
  if (minimum > 0 && missed == 0) life.perfect_days++;
  for (uint16_t& m : daily.minutes) m = 0;
  emit(EventType::DayClosed, kNoSlot, missed);
  if (missed == 0) return;

  // Each player who missed costs the pet a heart; the first one is named.
  uint8_t first = kNoSlot;
  bool died = false;
  for (int p = 0; p < kMaxSlots && !died; p++) {
    if (!(missed & (1u << p))) continue;
    if (first == kNoSlot) first = static_cast<uint8_t>(p);
    died = pet_on_break(data_.pet);
  }
  life.shame = Shame{first, ShameReason::MissedDaily, now_unix_};
  last_missed_mask_ = missed;
  dirty_ = true;

  if (died) {
    life.graveyard++;
    state_ = State::Result;
    result_ = ResultKind::Died;
    cause_ = ShameReason::MissedDaily;
    culprit_ = first;
    result_at_ = now_ms;
    session_start_ = session_end_ = now_ms;
    minutes_awarded_ = 0;
    emit(EventType::SessionEnded, first, static_cast<uint32_t>(ResultKind::Died));
  }
}

}  // namespace petcore
