# Phone-Free Pet Box

A Tamagotchi-style game for putting your phones away. Everyone docks their phone in a box with one microswitch per slot. While every phone stays put, a shared pixel pet grows and evolves on the GIGA Display Shield. If anyone picks their phone up, everyone loses: the pet loses a heart, and the screen shows who did it.

- **Hardware and wiring:** [docs/hardware.md](docs/hardware.md)
- **Rules, XP and evolution:** [docs/game-design.md](docs/game-design.md)

## Layout

```
lib/petcore/   Game logic, pet, save format, sprites, messages (pure C++17)
firmware/      PlatformIO project for the Arduino GIGA R1 WiFi + Display Shield
sim/           Terminal simulator using the same petcore code
test/          Host unit tests for petcore
docs/          Hardware and game design
```

## Try it on your computer

```bash
make test
```

```bash
make sim && ./build/petsim
```

Simulator keys: `1`–`4` dock/lift phones, `s` start, `c` cancel, `e` emergency pass, `f` finish, `Enter` OK, `t` cycle target, `w` time warp (×1/×60/×600), `r` reset the pet, `q` quit. The simulator saves to `petsim.sav` in the current folder. Quitting mid-session counts as pulling the plug.

## Build and flash the box

1. Wire it up following [docs/hardware.md](docs/hardware.md), then check the switches with `firmware/tools/switch_test/switch_test.ino` (Arduino IDE).
2. **One time only:** flash the core's *STM32H747_System → QSPIFormat* example from the Arduino IDE and answer the prompts. This creates the flash partitions the pet is saved to (partition 4). Without it the game still runs, but it doesn't save.
3. Install [PlatformIO](https://platformio.org/install/cli), then from `firmware/`:

```bash
pio run -t upload
```

```bash
pio device monitor
```

The display uses LVGL 8.3 via the core's `Arduino_H7_Video` and `Arduino_GigaDisplayTouch` libraries; its configuration is in `firmware/include/lv_conf.h`. Pins are set in `firmware/src/config.h`.
