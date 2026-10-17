import type { Ctx } from "../app";
import { moodIndex } from "../pet";
import { Bar, Card, Hearts, Pet, clock, hm, useTick } from "../ui";
import { TzBanner } from "./Settings";
import { RarityTag } from "./Collect";
import { EggPicker } from "./Egg";

export function Home(c: Ctx) {
  useTick(1000);
  const { st, me } = c;
  const p = st.players.find((x) => x.slot === me)!;
  const s = st.session;
  const min = st.settings.daily_minimum_min;
  const age = c.age();
  const running = s.state === "Running";
  const elapsed = running ? s.elapsed_s + age : 0;
  // Our minutes tick on between polls while we're in the session.
  const myMin = p.today_min + (running && p.in_session && !p.lifting ? Math.floor((s.elapsed_s % 60 + age) / 60) : 0);
  const left = st.day_ends_in_s === null ? null : Math.max(0, st.day_ends_in_s - age);
  const need = Math.max(0, min - myMin);
  // Like the box: count down to the goal, then to double the goal (extra
  // life), then show the time past the goal. Free play counts up.
  const goal = s.target_min * 60, bonus = s.bonus_goal_min * 60;
  const shown = !s.target_min
    ? elapsed
    : !s.target_reached
    ? Math.max(0, goal - elapsed)
    : s.bonus_available && !s.bonus_earned && elapsed < bonus
    ? bonus - elapsed
    : elapsed - goal;
  const over = !!s.target_min && s.target_reached && shown === elapsed - goal;

  return (
    <div class="screen home">
      <header class="pet-head">
        <div class="pet-stage">
          <Pet species={st.pet.species_index} stage={st.pet.stage_index} mood={moodIndex(st.pet.mood)} scale={11} />
        </div>
        <div class="pet-meta">
          <Hearts hearts={st.pet.hearts} max={st.pet.max_hearts} scale={3} />
          <div class="stage">
            {st.pet.species ? `${st.pet.species} ${st.pet.stage.toLowerCase()}` : st.pet.stage}{" "}
            {st.pet.rarity && st.pet.rarity !== "Common" && <RarityTag rarity={st.pet.rarity} />}
            <span class="muted"> · gen {st.pet.generation} · {st.pet.mood.toLowerCase()}</span>
          </div>
          <label class="mini">
            Happiness
            <Bar value={st.pet.happiness} max={100} tone="red" />
          </label>
          <label class="mini">
            {st.pet.next_stage_xp ? `XP to ${nextStage(st.pet.stage)}` : "XP (fully grown)"}
            <Bar
              value={st.pet.xp - st.pet.stage_xp}
              max={st.pet.next_stage_xp ? st.pet.next_stage_xp - st.pet.stage_xp : 1}
              tone="gold"
            />
          </label>
        </div>
      </header>

      {s.state === "Idle" && !st.pet.egg_locked && <EggPicker {...c} />}

      {p.lifting ? (
        <Card class="hero alarm">
          <h2>Your phone is out!</h2>
          <p>Put it back in slot {p.slot + 1} before the box calls it. The whole group is counting on you.</p>
        </Card>
      ) : running && p.in_session ? (
        <Card class="hero calm">
          {s.target_reached && <div class="goal-done">Goal complete!</div>}
          <div class="big-clock">{over ? "+" : ""}{clock(shown)}</div>
          <h2>{s.target_reached ? "Finish now, or take on the challenge." : "Session running. Put this phone down."}</h2>
          <p class="muted">
            {s.minutes} min · +{s.xp} XP{s.target_min ? ` · goal ${s.target_min} min` : " · free play"}
          </p>
          {/* Green bar to the goal stays full; the bonus round is a glowing gold fill on top of it. */}
          <div class="goal-bars">
            <Bar value={s.target_min ? Math.min(elapsed, goal) : 0} max={s.target_min ? goal : 1} tone="teal" />
            {(s.bonus_earned || (s.bonus_available && (s.target_reached || !s.target_min))) && (
              <Bar value={elapsed - (s.target_min ? goal : 0)} max={bonus - (s.target_min ? goal : 0)} tone="glow" />
            )}
          </div>
          <div class="together">
            {s.bonus_earned ? (
              <b class="gold">BONUS COMPLETE! Extra life earned.</b>
            ) : !s.bonus_available ? (
              s.target_reached && <span class="muted">Bonus extra life already won today.</span>
            ) : s.target_reached ? (
              <span>
                <b class="gold">BONUS ROUND:</b> keep going to {s.bonus_goal_min} min for an <b class="gold">extra life</b>!
              </span>
            ) : (
              <span class="muted">
                Bonus: {s.target_min ? "keep going" : "reach"} {s.bonus_goal_min} min for an extra life.
              </span>
            )}
          </div>
        </Card>
      ) : running ? (
        <Card class="hero nudge">
          <h2>You left on the emergency pass</h2>
          <p>The team carries on. Dock your phone in slot {p.slot + 1} again to rejoin.</p>
        </Card>
      ) : s.state === "Arming" ? (
        <Card class="hero nudge">
          <h2>Session starting…</h2>
          <p>Every phone is docked. Keep it that way: the team wins or loses together.</p>
        </Card>
      ) : st.night && s.state === "Idle" ? (
        <Card class="hero calm">
          <h2>The pet is asleep</h2>
          <p class="muted">
            Sessions start again at {String(st.night_hours[1]).padStart(2, "0")}:00. Phones away for the night too.
          </p>
        </Card>
      ) : s.state === "Result" && st.result ? (
        <Card class={"hero " + (st.result.kind === "Success" ? "calm" : "alarm")}>
          <h2>{{ Success: "Well done!", Aborted: "Stopped", Broken: "Busted!", Died: "R.I.P." }[st.result.kind]}</h2>
          <p>{st.result.text}</p>
        </Card>
      ) : !st.clock ? (
        <Card class="hero">
          <h2>The box doesn't know the time</h2>
          <p class="muted">Daily rules are paused until it does.</p>
        </Card>
      ) : !p.met_today ? (
        <Card class="hero nudge go">
          <h2>Go dock your phone</h2>
          <p>
            The team needs <b>{need} more min</b> today{left !== null && <> · day ends in {hm(left)}</>}. The session starts
            by itself when every phone is docked. Miss the minimum and the pet loses a heart.
          </p>
        </Card>
      ) : (
        <Card class="hero calm">
          <h2>Daily minimum done ✓</h2>
          <p class="muted">Every extra docked minute still grows the pet. Close this app and go live a little.</p>
        </Card>
      )}

      <Card>
        <div class="row between">
          <h3>My minutes today</h3>
          <span class={p.met_today ? "ok" : "warn"}>
            {myMin} / {min}
          </span>
        </div>
        <Bar value={myMin} max={Math.max(min, myMin)} tone={p.met_today ? "teal" : "red"} mark={min} />
        <div class="row between muted small">
          <span>{left !== null ? `Day ends in ${clock(left)}` : "Clock unknown"}</span>
          <span class="streak">
            🔥 {p.daily_streak} day{p.daily_streak === 1 ? "" : "s"} <span class="muted">(best {p.best_daily_streak})</span>
          </span>
        </div>
      </Card>

      <TzBanner {...c} />
      <p class="foot">The pet only grows while phones are in the box. Nothing more to see here 👋</p>
    </div>
  );
}

const nextStage = (s: string) => ({ Egg: "Baby", Baby: "Child", Child: "Teen", Teen: "Adult", Adult: "Elder" } as Record<string, string>)[s] || "";
