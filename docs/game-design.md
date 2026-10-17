# Game design

Everyone docks their phone in the box. A shared pet lives on the screen. It grows while every phone stays put, and it suffers if anyone picks theirs up.

## Session flow

| State | What happens | Leaves when |
|-------|--------------|-------------|
| **Idle** | Lobby. Slots show *docked* as phones go in. A fresh egg shows the egg picker (see [Eggs](#eggs)). | **Start** pressed, or all 4 slots docked for 5 s |
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

### Extra-life challenge

Reaching the target completes the session: the box shows **GOAL COMPLETE!** and you can finish (or pick up a phone) with a success. Keep going instead, and the timer counts down to **double the target**: get there and the pet earns a bonus life (above 3 hearts, up to 5). One bonus life per day; after that the box says it's already won today.

### Evolution

| Stage | XP | ~Time with 4 players |
|-------|----|----------------------|
| Egg | 0 | — |
| Baby | 30 | 5 min |
| Child | 150 | 25 min |
| Teen | 500 | 1.4 h |
| Adult | 1500 | 4 h |
| Elder | 4000 | 11 h |

### Eggs

You decide when a new pet starts: after a pet dies, or any time between sessions with **New pet** (box: Stats; app: Me). A new pet starts as an egg, and you pick the egg's time setting: **15, 30, 60 or 90 min**. That time is the goal of every session for this pet, and it stays until the pet is gone. You can change it until the egg's first session starts; a new egg starts with the previous setting.

Longer eggs ask more of the group, so they hatch rarer animals. Chance per rarity tier:

| Egg | Common | Uncommon | Rare | Legendary |
|-----|--------|----------|------|-----------|
| 15 min | 75 % | 20 % | 5 % | 0 % |
| 30 min | 55 % | 28 % | 14 % | 3 % |
| 60 min | 40 % | 32 % | 22 % | 6 % |
| 90 min | 25 % | 33 % | 30 % | 12 % |

### Species and collection

Every egg looks the same. When it hatches it becomes a random animal from its tier:

| Rarity | Species |
|--------|---------|
| Common | Blob, Chick, Bunny, Frog |
| Uncommon | Cat, Bear, Pig |
| Rare | Fox, Panda, Axolotl, Deer |
| Legendary | Dragon, Unicorn |

The chance is split evenly within a tier (from a 90-min egg, a Unicorn is 6 %). Species only changes the look: XP, hearts and evolution are the same for all. The box counts every species it has hatched and the furthest stage one reached. In the companion app those become cards: the **Collect** tab is the dex, and in **Friends** you can see other players' pets and stats and trade cards with them. The friends network is simulated on the phone for now. Pets from before species existed load as Blobs.

### Death and rebirth

When hearts reach 0 the pet dies and a tombstone is shown. After **OK**, a new egg hatches (generation + 1). Lifetime stats survive the pet: streaks, total time, the graveyard count, and the per-slot shame leaderboard.

## Tuning

All numbers live in `lib/petcore/src/pet.h` (pet tuning), `lib/petcore/src/species.h` (egg sizes and hatch odds), `species.cpp` (species list and colours) and `Config` in `lib/petcore/src/game.h` (timings). Play-test changes in the simulator (`make sim && ./build/petsim`) with time warp before flashing.
