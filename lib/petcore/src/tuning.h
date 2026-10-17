// Every number that shapes how the game feels, in one place.
//
// Change values here, run `make test`, and play-test in the simulator with
// time warp (`make sim && ./build/petsim`, key `w`) before flashing. The app
// reads its copy through app/scripts/export-petcore.py. docs/game-design.md
// explains the rules these numbers drive.
#pragma once
#include <cstdint>

namespace petcore {

// ---- Hearts -----------------------------------------------------------------
constexpr uint8_t kMaxHearts = 3;               // a fresh pet, and the most a success heals to
constexpr uint8_t kMaxBonusHearts = 5;          // bonus lives bank above kMaxHearts up to this
constexpr uint8_t kHeartsLostPerBreak = 1;      // a lifted phone, a power cut, or the team missing the daily minimum
constexpr uint8_t kHeartsHealedPerSuccess = 1;  // a successful session (never above kMaxHearts)

// ---- Happiness (0-100) ------------------------------------------------------
// Sets the pet's mood: it shows on its face and on the app.
constexpr uint8_t kMaxHappiness = 100;
constexpr uint8_t kStartHappiness = 60;
constexpr uint8_t kSuccessHappinessGain = 10;       // per successful session...
constexpr uint8_t kMinutesPerHappinessPoint = 6;    // ...plus 1 per this many minutes
constexpr uint8_t kBreakHappinessLoss = 35;         // per break (see kHeartsLostPerBreak)
constexpr uint8_t kAbsenceHappinessLossPerDay = 8;  // per day without any session
constexpr uint8_t kHappyMoodAt = 70;                // Happy at or above, Content at or above kContentMoodAt,
constexpr uint8_t kContentMoodAt = 35;              // Sad below

// ---- XP and growth ----------------------------------------------------------
// The game is played as a team: every player docks for every session, and
// the team earns the same XP per minute whatever its size.
constexpr uint8_t kXpPerMinute = 2;
constexpr uint8_t kCompletionBonusPercent = 20;  // extra XP on a successful session
constexpr uint8_t kGoalXpPerMinute = 1;          // reaching the goal adds goal minutes x this
// XP needed to reach each stage: Egg, Baby, Child, Teen, Adult, Elder.
constexpr uint32_t kStageXp[] = {0, 30, 150, 500, 1500, 4000};

// ---- Eggs and rarity --------------------------------------------------------
// The egg's time is the goal of every session for that pet; double it for the
// bonus life. Longer eggs hatch rarer pets. Chance per rarity tier in percent
// (Common, Uncommon, Rare, Legendary), split evenly between the species in a
// tier (species.cpp).
struct EggKind {
  uint16_t minutes;
  uint8_t weight[4];
};
constexpr EggKind kEggs[] = {
    {15, {75, 20, 5, 0}},
    {30, {55, 28, 14, 3}},
    {60, {40, 32, 22, 6}},
    {90, {25, 33, 30, 12}},
};
constexpr uint16_t kDefaultEggMinutes = 30;

// ---- Bonus life -------------------------------------------------------------
constexpr uint8_t kBonusGoalMultiplier = 2;     // keep going to this x the goal for a bonus life
constexpr uint32_t kFreePlayBonusMinutes = 60;  // the same without a goal (simulator, tests)
// At most one bonus life per day (DailyProgress::bonus_day).

// ---- Night --------------------------------------------------------------------
// Local hours when the pet sleeps (needs the clock). No sessions start, and a
// running session ends at bedtime, counted like pressing Finish.
constexpr uint8_t kNightStartHour = 22;
constexpr uint8_t kNightEndHour = 7;

// ---- Daily minimum ----------------------------------------------------------
constexpr uint16_t kDefaultDailyMinimum = 30;  // team minutes per day; 0 = off. Changeable in setup
// Missing it costs the pet kHeartsLostPerBreak once, however many players missed.
constexpr uint8_t kDefaultDayResetHour = 4;    // local hour a new day starts
constexpr uint32_t kMaxCatchupDays = 7;        // missed days penalised after the box was off

// ---- Session timing ---------------------------------------------------------
constexpr uint32_t kArmingMs = 10000;            // countdown after Start; late players can still dock
// Docking every player's phone starts the countdown (there is no Start
// button); this short wait rides out a phone being wiggled into its slot.
constexpr uint32_t kAutostartMs = 1500;
constexpr uint32_t kLiftToleranceMs = 750;       // ignore bumps shorter than this
constexpr uint32_t kEmergencyWindowMs = 30000;   // time to lift after using the pass (one per session)
constexpr uint32_t kMinSuccessMinutes = 10;      // "Finish" before this (and before the goal) is "Stop early"
constexpr uint32_t kResultTimeoutMs = 120000;    // the result screen dismisses itself
constexpr uint32_t kSaveEveryMinutes = 5;        // flash writes during a session

}  // namespace petcore
