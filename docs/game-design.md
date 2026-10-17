# Game design

Everyone docks their phone in the box. A shared pet lives on the screen. It grows while every phone stays put, and it suffers if anyone picks theirs up. It's a team game: every session needs every player, and the team wins or loses together.

## Session flow

| State | What happens | Leaves when |
|-------|--------------|-------------|
| **Idle** | Lobby. Slots show *docked* as phones go in. A fresh egg shows the egg picker (see [Eggs](#eggs)). | Every player's phone docked for 1.5 s (there is no Start button). Not at night |
| **Arming** | 10 s countdown. | Countdown ends → Running. Any phone lifted, or **Cancel** → Idle (no penalty) |
| **Running** | Pet earns XP every minute. | A lift, **Finish**, bedtime, or everyone leaves via emergency pass |
| **Result** | Success / Stopped / BUSTED! / R.I.P. screen. The culprit's slot is shown in red. | **OK**, or 2 min timeout |

### Rules while running

- **Lifting a phone** for longer than 0.75 s ends the session as **BUSTED!**: the pet loses a heart, happiness drops by 35, and the streak resets. The slot that lifted first is blamed and its shame counter goes up. Shorter lifts (a bump or a wobble) are ignored.
- **Target reached:** once the goal time has passed, picking up a phone ends the session as a success.
- **Finish button:** after at least 10 min (or once the target is reached) it says *Finish* and counts as a success. Before that it says *Stop early*: no reward, no penalty.
- **Emergency pass:** once per session, tap *Emergency pass*. The next phone lifted within 30 s leaves without penalty and the others carry on. If nobody lifts, you keep the pass.
- **Bedtime:** at night the pet sleeps (see [Night](#night)). A session still running at bedtime ends as if *Finish* was pressed.
- **Pulling the plug** mid-session counts as a break. The active session is saved to flash, so it is detected on the next boot (the culprit shows as "Someone").

## Pet

| Stat | Range | Notes |
|------|-------|-------|
| Hearts | 0–3 | −1 per break, +1 per successful session. 0 = the pet dies |
| Happiness | 0–100 | +10 (plus 1 per 6 min) per success, −35 per break, −8 per day without a session (only if the RTC knows the time) |
| XP | 0– | Drives evolution |

**Mood** comes from happiness: Happy ≥ 70, Content ≥ 35, otherwise Sad. It shows *Crying* on a BUSTED! screen and a tombstone when the pet has died.

### XP

**2 XP per minute**, whatever the size of the team: the challenge is keeping everyone off their phones, not how many there are. A successful session adds a **20 % completion bonus**, plus XP equal to the target minutes if a target was reached.

### Extra-life challenge

Reaching the target completes the session: the box shows **GOAL COMPLETE!** and you can finish (or pick up a phone) with a success. Keep going instead, and the timer counts down to **double the target**: get there and the pet earns a bonus life (above 3 hearts, up to 5). One bonus life per day; after that the box says it's already won today.

### Evolution

| Stage | XP | ~Sessions with a 30-min egg |
|-------|----|-----------------------------|
| Egg | 0 | — |
| Baby | 30 | the first one |
| Child | 150 | 2 |
| Teen | 500 | 5 |
| Adult | 1500 | 15 |
| Elder | 4000 | 40 |

A successful 30-min session earns about 100 XP (60 for the minutes, 12 completion bonus, 30 for reaching the goal).

### Night

From **22:00 to 07:00** local time the pet sleeps: it closes its eyes, the box shows *Zz*, and no session can start. A session still running at 22:00 ends as if *Finish* was pressed. Night needs the box to know the time (Wi-Fi or the app); the hours are in `tuning.h`.

### Daily minimum

The team has a daily minimum of docked minutes (30 by default, set on the box). Since every session includes everyone, the team meets it or misses it together: a missed day costs the pet **one** heart and puts *the team* on the wall of shame. Only a player who left a session on the emergency pass can miss it alone; then that player is named.

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

Every number lives in [`lib/petcore/src/tuning.h`](../lib/petcore/src/tuning.h); the species list and colours are in `species.cpp`. Play-test changes in the simulator (`make sim && ./build/petsim`) with time warp before flashing.
