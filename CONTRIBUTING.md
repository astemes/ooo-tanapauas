# Contributing to Tanapaus

Thanks for helping people put their phones down. Pull requests are welcome: bug fixes, new pets, better sprites, translations, app features, and ports to other hardware.

## Licensing of contributions

By opening a pull request you agree that your contribution is licensed under the project's terms: code under GPL-3.0-or-later, documentation and artwork under CC BY-SA 4.0 (see [NOTICE.md](NOTICE.md)). You keep the copyright to your work.

These licenses apply to everyone in the same way, the project's maintainer included. So anyone, including the maintainer, may use your contribution commercially, for example in a Tanapaus product, on the license's terms: the source of what they distribute stays open. Nobody gets extra rights to your work, such as using it in closed software.

Please sign off your commits (`git commit -s`). The `Signed-off-by` line certifies the [Developer Certificate of Origin](https://developercertificate.org/): that you wrote the change, or otherwise have the right to submit it under these licenses.

## Before you start

- For anything bigger than a small fix, open an issue first so we can agree on the approach.
- Keep pull requests focused: one change per PR, with a short description of what it does and how you tested it.
- Run the tests and builds that your change touches:

```bash
make test
```

```bash
npm --prefix app run build
```

```bash
pio run -d firmware
```

## How the code is split

| Part | What lives there | Depends on hardware? |
|------|------------------|----------------------|
| `lib/petcore/` | Game rules, sessions, eggs and species, pet growth, save format, sprites, every player-facing message | No: plain C++17, no Arduino |
| `firmware/src/hal/` | Slot switches, flash storage, buzzer | Yes |
| `firmware/src/net/` | Wi-Fi and the JSON API / app server | Yes (WiFi library) |
| `firmware/src/ui/` | LVGL screens for the 800x480 touchscreen | Yes (display) |
| `app/` | Companion web app, served by the box | No |
| `sim/`, `test/` | Desktop simulator and unit tests for petcore | No |

**Game rules and presentation logic stay in `petcore`.** That includes timing, scoring, odds, what the pet looks like (`render_pet`, `palette_rgb`) and what the messages say (`describe_event`, `describe_shame`). Frontends only draw what petcore gives them and pass inputs back. This keeps the box, the simulator, the tests and the app in agreement: the app even draws its pets from sprite data exported out of `sprites.cpp` (`app/scripts/export-petcore.py`).

If you change a rule, change it in petcore, add or update a test in `test/test_petcore.cpp`, and update [docs/game-design.md](docs/game-design.md). If you change the API, update [docs/api.md](docs/api.md), `app/src/types.ts` and the mock box in `app/mock/server.mjs`.

## Porting to other hardware

Ports are very welcome. Keep the game common and abstract the hardware: a port should be a new frontend next to `firmware/`, reusing `lib/petcore` unchanged. If petcore needs a change to support your board, make it a general change that works for every frontend.

### Minimum hardware

- **A screen**, for the pet and the session timer
- **Wi-Fi**, to serve the companion app and keep the clock
- **One digital input per phone slot**, telling whether a phone is in place (a microswitch, reed switch, IR sensor, or anything else that gives a stable on/off)

A buzzer, a touchscreen and persistent storage are nice to have. Without storage the game runs but forgets the pet on power loss.

### What a port provides

1. **Slots:** read each slot's input every loop and pass the bit mask to `Game::update(now_ms, docked_mask, unix)`. Debouncing of short bumps is already done in petcore.
2. **Time:** milliseconds since boot, plus Unix time when known (NTP, an RTC, or `POST /api/time` from the app).
3. **Storage (optional):** `save_encode` / `save_decode` give you a small CRC-checked buffer; store it wherever your board can. Save when `Game::take_dirty()` returns true.
4. **Screen:** draw the pet from `render_pet` + `palette_rgb`, the hearts, the session timer and the messages from `describe_event`. See `firmware/src/ui/ui.cpp` for what the box shows in each `State`.
5. **The companion app and API:** serve `app/dist` and the endpoints in [docs/api.md](docs/api.md). `firmware/src/net/server.cpp` is a small reference implementation with no dependencies beyond a TCP server.

### Without a touchscreen

The game still works:

- **Starting:** sessions start on their own when every player's phone is docked (`Config::autostart_ms`, 5 s by default), followed by the usual countdown.
- **Linking phones:** show the app's QR code on screen permanently instead of the *Link phone* button.
- **Everything else** moves to the companion app: picking an egg (`POST /api/egg`), starting a new pet (`POST /api/pet/new`), player names and settings. The PIN that guards these is shown on the box's settings screen today, so a screen-only port should show it somewhere else (for example on the idle screen) or print it over serial.

One gap to close for touch-free ports: the player count and first-time setup are only on the touchscreen today. Moving them into the app (behind the PIN) would be a welcome contribution.

## Style

- C++: C++17, the style of the surrounding code (2-space indent, `snake_case` functions, `kConstants`). No dynamic allocation in petcore.
- TypeScript: as in `app/src`. Keep the app small; it ships inside the firmware (`app/scripts/size.mjs` checks the budget).
- Comments explain why, not what.
- Player-facing text is short and plain, and it lives in petcore (`text.cpp`) when the box shows it.
