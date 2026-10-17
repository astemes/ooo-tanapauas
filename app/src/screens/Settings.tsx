import { useState } from "preact/hooks";
import type { Ctx } from "../app";
import { api, ApiError, BASE } from "../api";
import { load, save } from "../store";
import { Card, tzLabel } from "../ui";
import { NewPetCard } from "./Egg";

const phoneTz = () => -new Date().getTimezoneOffset();

export function PinInput({ value, onInput }: { value: string; onInput: (v: string) => void }) {
  const saved = !value && !!load("pin");
  return (
    <input
      class="pin"
      type="password"
      inputMode="numeric"
      autoComplete="off"
      pattern="[0-9]*"
      maxLength={4}
      placeholder={saved ? "saved" : "PIN"}
      value={value}
      onInput={(e) => onInput((e.target as HTMLInputElement).value.replace(/\D/g, "").slice(0, 4))}
    />
  );
}

// Runs a PIN-protected write; remembers the PIN when the box accepts it.
export function usePinAction() {
  const [pinState, setPin] = useState(() => load("pin") || "");
  const [busy, setBusy] = useState(false);
  const [msg, setMsg] = useState<[string, string] | null>(null);
  const run = async (fn: (pin: string) => Promise<unknown>, ok: string) => {
    // Another form on the page may have saved the PIN since this one mounted.
    const pin = pinState || load("pin") || "";
    if (pin.length !== 4) return setMsg(["warn", "Enter the 4-digit PIN from the box's settings screen."]);
    setBusy(true);
    setMsg(null);
    try {
      await fn(pin);
      save("pin", pin);
      setMsg(["ok", ok]);
    } catch (e) {
      const err = e as ApiError;
      if (err.status === 403) {
        save("pin", null);
        setPin("");
        setMsg(["warn", "Wrong PIN. Check the box's settings screen."]);
      } else if (err.status === 409) setMsg(["warn", "Not right now: try again when no session is running."]);
      else setMsg(["warn", err.message]);
    }
    setBusy(false);
  };
  return { pin: pinState, setPin, busy, msg, run };
}

export function TzBanner(c: Ctx) {
  const tz = phoneTz();
  const a = usePinAction();
  if (c.st.settings.tz_offset_min === tz && !a.msg) return null;
  return (
    <Card class="tz">
      <h3>Time zone</h3>
      {c.st.settings.tz_offset_min !== tz && (
        <p class="small">
          The box runs on {tzLabel(c.st.settings.tz_offset_min)}, this phone on {tzLabel(tz)}. Days might end at the wrong
          hour.
        </p>
      )}
      {c.st.settings.tz_offset_min !== tz && (
        <div class="row">
          <PinInput value={a.pin} onInput={a.setPin} />
          <button
            class="btn"
            disabled={a.busy}
            onClick={() =>
              a.run(async (pin) => {
                await api.settings(pin, { tz_offset_min: tz });
                c.refresh();
              }, "Time zone updated.")
            }
          >
            Set time zone from this phone
          </button>
        </div>
      )}
      {a.msg && <p class={"small " + a.msg[0]}>{a.msg[1]}</p>}
    </Card>
  );
}

export function SettingsScreen(c: Ctx) {
  const { st, me } = c;
  const p = st.players.find((x) => x.slot === me)!;
  const [name, setName] = useState(p.name);
  const a = usePinAction();
  const bytes = new TextEncoder().encode(name.trim()).length;
  const valid = bytes >= 1 && bytes <= 12;

  return (
    <div class="screen">
      <h1>Me</h1>
      <Card>
        <h3>You are {p.name}, slot {p.slot + 1}</h3>
        <form
          class="stack"
          onSubmit={(e) => {
            e.preventDefault();
            if (valid)
              a.run(async (pin) => {
                const r = await api.rename(me, name.trim(), pin);
                setName(r.name);
                c.refresh();
              }, "Name saved on the box.");
          }}
        >
          <label>
            Name <span class="muted small">({bytes}/12)</span>
            <input value={name} maxLength={12} onInput={(e) => setName((e.target as HTMLInputElement).value)} />
          </label>
          <div class="row">
            <PinInput value={a.pin} onInput={a.setPin} />
            <button class="btn" disabled={!valid || a.busy || name.trim() === p.name}>
              Rename
            </button>
          </div>
          {a.msg && <p class={"small " + a.msg[0]}>{a.msg[1]}</p>}
        </form>
      </Card>

      <Card>
        <h3>Not you?</h3>
        <button class="btn ghost" onClick={() => c.setMe(null)}>
          Switch player
        </button>
      </Card>

      <TzBanner {...c} />

      <NewPetCard {...c} />

      <Card>
        <h3>The box</h3>
        <dl class="facts">
          <dt>Players</dt>
          <dd>{st.settings.players}</dd>
          <dt>Daily minimum</dt>
          <dd>{st.settings.daily_minimum_min} min</dd>
          <dt>New day at</dt>
          <dd>{String(st.settings.day_reset_hour).padStart(2, "0")}:00</dd>
          <dt>Time zone</dt>
          <dd>{tzLabel(st.settings.tz_offset_min)}</dd>
          <dt>Clock</dt>
          <dd>{st.clock ? "set" : "unknown"}</dd>
          <dt>Address</dt>
          <dd>{BASE || location.host}</dd>
        </dl>
        <p class="muted small">Player count and daily minimum are changed on the box itself.</p>
        {load("pin") && (
          <button class="btn ghost small" onClick={() => (save("pin", null), a.setPin(""))}>
            Forget saved PIN
          </button>
        )}
      </Card>
      <p class="foot">Tip: add this page to your home screen, then put the phone in the box.</p>
    </div>
  );
}
