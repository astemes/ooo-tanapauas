import { useState } from "preact/hooks";
import type { Ctx } from "../app";
import { api } from "../api";
import { EGGS, rareChance } from "../pet";
import { Card, Pet } from "../ui";
import { PinInput, usePinAction } from "./Settings";

// A fresh egg: pick its time setting. It is every session's goal for this pet,
// and longer eggs hatch rarer pets. Locked after the egg's first session.
export function EggPicker(c: Ctx) {
  const a = usePinAction();
  const current = c.st.pet.egg_minutes;
  return (
    <Card class="egg-pick">
      <h3>Pick an egg</h3>
      <p class="small muted">
        Its time is the goal for every session with this pet. Longer sessions hatch rarer pets. You can change it until the
        first session starts.
      </p>
      <div class="eggs">
        {EGGS.map((e) => (
          <button
            key={e.minutes}
            class={"egg" + (e.minutes === current ? " picked" : "")}
            disabled={a.busy}
            onClick={() =>
              e.minutes !== current &&
              a.run(async (pin) => {
                await api.egg(pin, e.minutes);
                c.refresh();
              }, `${e.minutes}-min egg picked.`)
            }
          >
            <Pet stage={0} mood={e.minutes === current ? 0 : 2} scale={3} still={e.minutes !== current} />
            <b>{e.minutes} min</b>
            <span class="small">{rareChance(e.minutes)}% rare</span>
          </button>
        ))}
      </div>
      <div class="row">
        <PinInput value={a.pin} onInput={a.setPin} />
        <span class="small muted">The PIN is on the box: Stats &gt; Settings.</span>
      </div>
      {a.msg && <p class={"small " + a.msg[0]}>{a.msg[1]}</p>}
    </Card>
  );
}

// Retire the current pet and start over with a fresh egg.
export function NewPetCard(c: Ctx) {
  const a = usePinAction();
  const [sure, setSure] = useState(false);
  const p = c.st.pet;
  return (
    <Card>
      <h3>Start a new pet</h3>
      <p class="small muted">
        Says goodbye to your {p.species ?? "egg"} (gen {p.generation}) and starts a fresh egg. Its species stays in your
        collection. Only between sessions.
      </p>
      <div class="row">
        <PinInput value={a.pin} onInput={a.setPin} />
        <button
          class={"btn" + (sure ? " danger" : " ghost")}
          disabled={a.busy || c.st.session.state !== "Idle"}
          onClick={() => {
            if (!sure) return setSure(true);
            setSure(false);
            a.run(async (pin) => {
              await api.newPet(pin);
              c.refresh();
            }, "A fresh egg is waiting on the box. Pick its time on the Pet tab.");
          }}
        >
          {sure ? "Yes, start over" : "New pet"}
        </button>
      </div>
      {a.msg && <p class={"small " + a.msg[0]}>{a.msg[1]}</p>}
    </Card>
  );
}
