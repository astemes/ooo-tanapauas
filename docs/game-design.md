# Game design

Everyone docks their phone in the box. A shared pet lives on the screen. It grows while every phone stays put, and it suffers if anyone picks theirs up.

## Session flow

| State | What happens | Leaves when |
|-------|--------------|-------------|
| **Idle** | Lobby. Slots show *docked* as phones go in. Pick a target (free / 15 / 30 / 60 / 90 min). | **Start** pressed, or all 4 slots docked for 5 s |
| **Arming** | 10 s countdown. Late players can still dock and join. | Countdown ends → Running. Any armed phone lifted, or **Cancel** → Idle (no penalty) |
| **Running** | Pet earns XP every minute. Newly docked phones join mid-session. | A lift, **Finish**, or everyone leaves via emergency pass |
| **Result** | Success / Stopped / BUSTED! / R.I.P. screen. The culprit's slot is shown in red. | **OK**, or 2 min timeout |

### Rules while running

- **Lifting a phone** for longer than 0.75 s ends the session as **BUSTED!**: the pet loses a heart, happiness drops by 35, and the streak resets. The slot that lifted first is blamed and its shame counter goes up. Shorter lifts (a bump or a wobble) are ignored.
- **Target reached:** once the goal time has passed, picking up a phone ends the session as a success.
- **Finish button:** after at least 10 min (or once the target is reached) it says *Finish* and counts as a success. Before that it says *Stop early*: no reward, no penalty.
- **Emergency pass:** once per session, tap *Emergency pass*. The next phone lifted within 30 s leaves without penalty and the others carry on. If nobody lifts, you keep the pass.
- **Pulling the plug** mid-session counts as a break. The active session is saved to flash, so it is detected on the next boot (the culprit shows as "Someone").

## Pet

| Stat | Range | Notes |
|------|-------|-------|
| Hearts | 0–3 | −1 per break, +1 per successful session. 0 = the pet dies |
| Happiness | 0–100 | +10 (plus 1 per 6 min) per success, −35 per break, −8 per day without a session (only if the RTC knows the time) |
| XP | 0– | Drives evolution |

**Mood** comes from happiness: Happy ≥ 70, Content ≥ 35, otherwise Sad. It shows *Crying* on a BUSTED! screen and a tombstone when the pet has died.

### XP

Per minute, by the number of phones locked in:

| Players | 1 | 2 | 3 | 4 |
|---------|---|---|---|---|
| XP/min | 1 | 2 | 4 | 6 |

A successful session adds a **20 % completion bonus**, plus XP equal to the target minutes if a target was reached.

### Evolution

| Stage | XP | ~Time with 4 players |
|-------|----|----------------------|
| Egg | 0 | — |
| Baby | 30 | 5 min |
| Child | 150 | 25 min |
| Teen | 500 | 1.4 h |
| Adult | 1500 | 4 h |
| Elder | 4000 | 11 h |

### Death and rebirth

When hearts reach 0 the pet dies and a tombstone is shown. After **OK**, a new egg hatches (generation + 1). Lifetime stats survive the pet: streaks, total time, the graveyard count, and the per-slot shame leaderboard.

## Tuning

All numbers live in `lib/petcore/src/pet.h` (pet tuning) and `Config` in `lib/petcore/src/game.h` (timings). Play-test changes in the simulator (`make sim && ./build/petsim`) with time warp before flashing.
