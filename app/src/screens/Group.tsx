import type { Ctx } from "../app";
import { Bar, Card, ago, hm } from "../ui";

const REASON: Record<string, string> = { Lift: "picked up their phone", MissedDaily: "missed the daily minimum", PowerCut: "pulled the plug" };

export function Group(c: Ctx) {
  const { st } = c;
  const min = st.settings.daily_minimum_min;
  const players = st.players.filter((p) => p.slot < st.settings.players);
  const board = [...players].sort((a, b) => b.lifted + b.missed_days - (a.lifted + a.missed_days));
  const cleanest = Math.min(...players.map((p) => p.lifted + p.missed_days));
  const L = st.life;
  const recent = c.events.slice(-6).reverse();

  return (
    <div class="screen">
      <h1>The group</h1>
      <Card>
        <h3>Today</h3>
        {players.map((p) => (
          <div class="player" key={p.slot}>
            <div class="row between">
              <span>
                <i class={"dot " + (p.lifting ? "lift" : p.docked ? "in" : "")} title={p.docked ? "docked" : "out"} />
                {p.name} {p.slot === c.me && <span class="muted">(you)</span>}
              </span>
              <span class={p.met_today ? "ok" : "muted"}>
                {p.met_today ? "✓ " : ""}
                {p.today_min} / {min} min
              </span>
            </div>
            <Bar value={p.today_min} max={Math.max(min, p.today_min)} tone={p.met_today ? "teal" : "dim"} mark={min} />
          </div>
        ))}
      </Card>

      <Card class="shame">
        <h3>Wall of shame</h3>
        {st.shame ? (
          <div class="shame-entry">
            <div class="shame-name">{st.shame.name || "Everyone"}</div>
            <p>{st.shame.text || `${st.shame.name} ${REASON[st.shame.reason]}.`}</p>
            <p class="muted small">{ago(st.shame.at, st.now)} · cleared by the next successful session</p>
          </div>
        ) : (
          <p class="muted">Empty. Keep it that way.</p>
        )}
      </Card>

      <Card>
        <h3>Shame board (lifetime)</h3>
        <table class="board">
          <thead>
            <tr>
              <th />
              <th>Lifted</th>
              <th>Missed</th>
              <th>Streak</th>
            </tr>
          </thead>
          <tbody>
            {board.map((p) => (
              <tr key={p.slot}>
                <td>
                  {p.lifted + p.missed_days === cleanest && "👑 "}
                  {p.name}
                </td>
                <td class={p.lifted ? "warn" : ""}>{p.lifted}</td>
                <td class={p.missed_days ? "warn" : ""}>{p.missed_days}</td>
                <td>{p.best_daily_streak}</td>
              </tr>
            ))}
          </tbody>
        </table>
      </Card>

      <Card>
        <h3>Together so far</h3>
        <div class="stats">
          <Stat v={hm(L.total_min * 60)} k="phone-free" />
          <Stat v={L.wins} k="sessions won" />
          <Stat v={L.broken} k="broken" />
          <Stat v={`${L.longest_min} min`} k="longest" />
          <Stat v={L.perfect_days} k="perfect days" />
          <Stat v={L.bonus_lives} k="bonus lives" />
          <Stat v={`${L.streak} / ${L.best_streak}`} k="win streak / best" />
          <Stat v={L.graveyard} k="pets lost" />
        </div>
      </Card>

      {recent.length > 0 && (
        <Card>
          <h3>Lately</h3>
          <ul class="events">
            {recent.map((e) => (
              <li key={e.id}>
                <span>{e.text}</span>
                <span class="muted small">{ago(e.at, st.now)}</span>
              </li>
            ))}
          </ul>
        </Card>
      )}
    </div>
  );
}

const Stat = ({ v, k }: { v: string | number; k: string }) => (
  <div class="stat">
    <b>{v}</b>
    <span>{k}</span>
  </div>
);
