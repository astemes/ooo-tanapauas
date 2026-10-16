# Hardware: Phone-Free Pet Box

## Parts list

| Qty | Part | Notes |
|----:|------|-------|
| 1 | Arduino GIGA R1 WiFi | Main board (3.3 V logic) |
| 1 | Arduino GIGA Display Shield | 800x480 touchscreen, shows the pet |
| 4 | Lever microswitch, e.g. SS-5GL / KW11-3Z with roller or long lever | One per phone slot. Pick a **low operating force** (≤ 0.5 N) so a light phone still presses it |
| 1 | Passive piezo buzzer (optional) | Sound effects |
| 1 | 100 Ω resistor (optional) | In series with the buzzer |
| 4 | 100 nF ceramic capacitor (optional) | Across each switch if the wires are longer than ~30 cm |
| — | Hook-up wire, Dupont jumpers or screw-terminal shield | |
| 1 | USB-C power supply, 5 V ≥ 2 A | Display backlight needs the headroom |

## Pin table

| Function | GIGA pin | Wire to |
|----------|----------|---------|
| Slot 1 switch | **D2** | Switch 1 **NO** terminal |
| Slot 2 switch | **D3** | Switch 2 **NO** terminal |
| Slot 3 switch | **D4** | Switch 3 **NO** terminal |
| Slot 4 switch | **D5** | Switch 4 **NO** terminal |
| Switch common | **GND** | All four switch **COM** terminals |
| Buzzer + (optional) | **D7** | via 100 Ω to buzzer + |
| Buzzer − (optional) | **GND** | buzzer − |
| Display | — | Plugs straight onto the GIGA; no wiring |

The firmware enables the internal pull-ups (`INPUT_PULLUP`), so **no external resistors are needed** for the switches.

- Phone in slot → lever pressed → NO contact closes → pin reads **LOW** = docked
- Phone lifted → lever released → contact opens → pin pulled **HIGH** = lifted

Leave the switches' **NC** terminals unconnected.

## Wiring diagram

```
  GIGA R1 WiFi                    Slot switches (lever microswitches)

  ┌───────────┐
  │        D2 ├───────────────── NO ─[ SW1 ]─ COM ──┐
  │        D3 ├───────────────── NO ─[ SW2 ]─ COM ──┤
  │        D4 ├───────────────── NO ─[ SW3 ]─ COM ──┤
  │        D5 ├───────────────── NO ─[ SW4 ]─ COM ──┤
  │           │                                     │
  │        D7 ├──[100Ω]── (+) piezo (−) ────────────┤   (optional)
  │           │                                     │
  │       GND ├─────────────────────────────────────┘   shared GND bus
  └───────────┘

  Each microswitch has 3 terminals:   COM   NO   NC
                                       │     │    └─ leave unconnected
                                       │     └────── to its D-pin
                                       └──────────── to GND
```

Simplified view of a single slot (repeat ×4):

```
   GIGA Dx (D2..D5) ───────────── NO  ┐
                                       ├─ microswitch (lever up = open)
   GIGA GND ──────────────────────── COM ┘
                          (optional 100 nF cap between Dx and GND)
```

## Box build notes

- **Slot layout:** four slots side by side, each ~90 mm wide × 20 mm deep, angled back ~10° so phones lean and rest on the lever. Label the slots 1–4 to match D2–D5. The shame screen names the slot number.
- **Switch placement:** mount each switch on the slot floor near the back wall so the lever sits under the phone's centre of mass. Put a thin foam or felt pad over the lever: it protects the phone and spreads the load.
- **Test the force:** use the lightest phone in your group. If it doesn't click the switch reliably, bend the lever up slightly or use a longer lever.
- **Display:** cut an angled window in the front or top for the GIGA Display Shield (active area ~ 108 × 65 mm; check the shield's datasheet for exact dimensions before cutting).
- **Access:** keep the GIGA's USB-C port reachable for power and flashing.
- **Pins:** D2–D5 and D7 are on the standard digital header. Fit the display shield first and check those pins are still reachable. If your box layout needs others, any free digital pin works; change `kSlotPins` / `kBuzzerPin` in `firmware/src/config.h` to match.
- **Unplugging counts as cheating:** the firmware remembers an active session in flash, so pulling the power mid-session counts as a broken session.

## Checking the wiring before assembly

Flash `firmware/tools/switch_test/switch_test.ino` with the Arduino IDE (board: *Arduino GIGA R1 WiFi*, M7 core), open the Serial Monitor at 115200 baud and press each switch. You should see:

```
slot1=DOCKED slot2=empty  slot3=empty  slot4=empty
```

Each slot should change only when its own switch is pressed. If a slot always reads DOCKED, the switch is wired to NC instead of NO. If it never changes, check the GND connection.
