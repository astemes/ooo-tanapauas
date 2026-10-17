# Box API (v1)

The GIGA serves the companion app and this JSON API on the home Wi-Fi at `http://<box-ip>/`. The box shows its address and a QR code on the idle screen.

- Plain HTTP on port 80, one request at a time. Keep requests small and poll no faster than every 5 s.
- Responses are `application/json; charset=utf-8` with `Access-Control-Allow-Origin: *`, so the app can be developed on a laptop against a real box.
- Times are Unix seconds (UTC). `null` means "unknown" or "nothing".
- Slots are 0-based (`0` is the slot labelled 1 on the box). Only slots `< settings.players` are players.
- Writes need the 4-digit PIN shown on the box's settings screen. A wrong PIN returns `403`.

Errors look like `{"error": "Wrong PIN"}`, with status 400 (bad input), 403 (PIN), 404 (unknown path) or 409 (not allowed right now).

## Static files

| Path | Content |
|------|---------|
| `GET /` | The app: a single `index.html` with JS and CSS inlined, sent with `Content-Encoding: gzip` |
| `GET /manifest.webmanifest` | App name, colours and icon for "Add to home screen" |
| `GET /icon.png` | 192×192 app icon |

## `GET /api/state`

Everything the home and group screens need, in one call.

```json
{
  "api": 1,
  "now": 1767300000,
  "clock": true,
  "night": false,
  "night_hours": [22, 7],
  "day_ends_in_s": 33120,
  "settings": {
    "players": 2,
    "daily_minimum_min": 30,
    "day_reset_hour": 4,
    "tz_offset_min": 60
  },
  "pet": {
    "stage": "Baby",
    "stage_index": 1,
    "species": "Fox",
    "species_index": 7,
    "rarity": "Rare",
    "egg_minutes": 60,
    "egg_locked": true,
    "rare_chance": 28,
    "generation": 1,
    "hearts": 4,
    "max_hearts": 3,
    "bonus_cap": 5,
    "happiness": 72,
    "mood": "Happy",
    "xp": 96,
    "stage_xp": 30,
    "next_stage_xp": 150
  },
  "session": {
    "state": "Running",
    "elapsed_s": 1920,
    "minutes": 32,
    "xp": 192,
    "target_min": 60,
    "target_reached": false,
    "bonus_goal_min": 120,
    "bonus_earned": false,
    "bonus_available": true,
    "emergency_available": true
  },
  "result": null,
  "players": [
    {
      "slot": 0,
      "name": "Anna",
      "docked": true,
      "in_session": true,
      "lifting": false,
      "today_min": 32,
      "met_today": true,
      "daily_streak": 3,
      "best_daily_streak": 5,
      "lifted": 1,
      "missed_days": 0
    },
    {
      "slot": 1,
      "name": "Ben",
      "docked": true,
      "in_session": true,
      "lifting": false,
      "today_min": 9,
      "met_today": false,
      "daily_streak": 0,
      "best_daily_streak": 2,
      "lifted": 3,
      "missed_days": 1
    }
  ],
  "shame": {
    "slot": 1,
    "name": "Ben",
    "reason": "MissedDaily",
    "at": 1767240000,
    "text": "Ben missed the daily 30 min."
  },
  "life": {
    "total_min": 610,
    "longest_min": 95,
    "wins": 9,
    "broken": 2,
    "streak": 1,
    "best_streak": 4,
    "perfect_days": 3,
    "bonus_lives": 1,
    "graveyard": 0
  }
}
```

| Field | Values |
|-------|--------|
| `clock` | `false` until the box knows the time (NTP or `POST /api/time`). While false, daily rules are paused and `day_ends_in_s` is `null`. |
| `pet.stage` | `Egg`, `Baby`, `Child`, `Teen`, `Adult`, `Elder` |
| `pet.mood` | `Happy`, `Content`, `Sad`, `Crying`, `Dead`, `Sleeping` (at night) |
| `night` | `true` during the night hours `night_hours` (local `[start, end]`): the pet sleeps and no session can start. Always `false` while `clock` is `false`. |
| `pet.egg_minutes` | The egg's time setting (15, 30, 60 or 90): the goal of every session for this pet, also `session.target_min`. `egg_locked` is `false` while it can still be changed (a fresh egg before its first session). `rare_chance` is the percent chance this egg hatches Rare or Legendary. |
| `pet.species` | The animal the egg hatched into, or `null` while it's still an egg. `species_index` is its index in `GET /api/collection`; `rarity` is `Common`, `Uncommon`, `Rare` or `Legendary`. |
| `pet.hearts` | 0–5. Above `max_hearts` (3) are bonus lives. |
| `session.state` | `Setup`, `Idle`, `Arming`, `Running`, `Result`. The other `session` fields are `0`/`false` outside `Running`. |
| `session.bonus_goal_min` | The extra-life challenge: keep the session going this long (double `target_min`) for a bonus life. Only if `bonus_available` (one per day); `bonus_earned` turns true when it's won. |
| `result` | `null` unless `state` is `Result`, otherwise `{"kind": "Success" \| "Aborted" \| "Broken" \| "Died", "culprit": 1 \| null, "cause": "Lift" \| "MissedDaily" \| "PowerCut" \| null, "text": "Ben picked up their phone after 42 min! Everyone loses."}` |
| `players[].lifted` | Times this player picked up their phone mid-session (lifetime). |
| `players[].missed_days` | Days this player missed the daily minimum (lifetime). |
| `shame` | The current wall-of-shame entry, or `null`. `slot` and `name` are `null` for a power cut, which has no culprit. It's cleared by the next successful session. `reason` is `Lift`, `MissedDaily` or `PowerCut`. |

## `GET /api/history?days=30`

Daily minutes per player, newest first, at most 60 days. The current day is included and still in progress.

```json
{
  "minimum": 30,
  "players": 2,
  "days": [
    { "date": "2026-01-02", "minutes": [32, 9], "met": [true, false], "today": true },
    { "date": "2026-01-01", "minutes": [41, 30], "met": [true, true], "today": false }
  ]
}
```

`date` is the local calendar date the day started on (the day runs from `day_reset_hour` to `day_reset_hour` the next morning). Days the box was off are absent.

## `GET /api/events?since=<id>`

Recent events for the app's feed and toasts. The box keeps the last 64 in memory. `next` is the newest id the box has handed out; use it as `since` on the following poll (`since` excludes that id); `since=0` returns everything still kept. Event ids restart at 1 when the box reboots: if `next` is lower than the `since` you sent, start again from `since=0`.

```json
{
  "next": 41,
  "events": [
    { "id": 40, "at": 1767240000, "type": "DailyMissed", "slot": 1, "value": 9,
      "text": "Ben only managed 9 of 30 min yesterday. The pet lost a heart!" },
    { "id": 41, "at": 1767298080, "type": "BonusLife", "slot": null, "value": 4,
      "text": "Challenge done: 120 min! Extra life: 4 hearts." }
  ]
}
```

`value` is a number, except for `SessionEnded`, where it's the result kind as text (`"Success"`, `"Aborted"`, `"Broken"` or `"Died"`). `type` is one of: `ArmingStarted`, `ArmingCancelled`, `SessionStarted`, `PlayerJoined`, `Evolved`, `TargetReached`, `EmergencyArmed`, `EmergencyUsed`, `EmergencyExpired`, `SessionEnded`, `Hatched`, `DailyMissed`, `DayClosed`, `BonusLife`, `SetupDone`, `NightStarted` (`value` = hour the pet wakes), `Morning`, `NewSpecies` (`value` = species index, the first time the box hatches that species). `at` is `null` if the clock was unknown then.

## `GET /api/collection`

Every species, in `species_index` order, and how many of each this box has hatched. The app shows these as collectible cards.

```json
{
  "species": [
    { "index": 0, "name": "Blob", "rarity": "Common", "count": 2, "best_stage": "Teen" },
    { "index": 7, "name": "Fox", "rarity": "Rare", "count": 1, "best_stage": "Baby" },
    { "index": 12, "name": "Unicorn", "rarity": "Legendary", "count": 0, "best_stage": null }
  ]
}
```

(Shortened: the real list always has all 13 species.) `best_stage` is the furthest any pet of that species grew, or `null` if `count` is 0. Trades between friends are not on the box: the app keeps them (see `app/src/network.ts`, currently a local mock).

## `POST /api/players/<slot>/name`

```json
{ "name": "Anna", "pin": "4821" }
```

Sets a player's name: 1–12 bytes of UTF-8; `"` and `\` are replaced. Returns `{"ok": true, "name": "Anna"}`.

## `POST /api/egg`

```json
{ "pin": "4821", "minutes": 90 }
```

Picks the egg's time setting: 15, 30, 60 or 90. Returns `{"ok": true, "egg_minutes": 90, "rare_chance": 42}`, `400` for another number, or `409` if the egg is locked or a session is on.

## `POST /api/pet/new`

```json
{ "pin": "4821" }
```

Retires the current pet and starts a fresh egg (generation + 1) with the previous time setting, which can then be changed with `POST /api/egg`. The old pet's species stays in the collection. Returns `{"ok": true, "generation": 4}`, or `409` unless the box is `Idle`.

## `POST /api/settings`

```json
{ "pin": "4821", "tz_offset_min": 60, "daily_minimum_min": 45, "day_reset_hour": 4 }
```

Any subset of the three settings. The app uses it to set the time zone from the phone. Returns the new `settings` object. Returns `409` unless the box is in `Idle` or `Setup`. The player count can only be changed on the box itself.

## `POST /api/time`

```json
{ "unix": 1767300000 }
```

Clock fallback when the box has no internet for NTP: the app sends the phone's time on load. It's only accepted while `clock` is `false`, and needs no PIN. Returns `{"ok": true, "clock": true}`, or `409` if the box already knows the time.
