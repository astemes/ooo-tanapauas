import { useState } from "preact/hooks";
import type { Ctx } from "../app";
import type { HistoryDay } from "../types";
import { Card } from "../ui";

const DAYS = 30;

// Last 30 calendar days, oldest first; days the box was off are null.
export function lastDays(days: HistoryDay[]): { date: string; day: HistoryDay | null }[] {
  const byDate = new Map(days.map((d) => [d.date, d]));
  const end = days.find((d) => d.today)?.date || days[0]?.date;
  if (!end) return [];
  const t = Date.parse(end + "T00:00:00Z");
  const out = [];
  for (let i = DAYS - 1; i >= 0; i--) {
    const date = new Date(t - i * 86400000).toISOString().slice(0, 10);
    out.push({ date, day: byDate.get(date) || null });
  }
  return out;
}

function level(min: number, minimum: number): number {
  if (min <= 0) return 0;
  if (min < minimum / 2) return 1;
  if (min < minimum) return 2;
  if (min < minimum * 2) return 3;
  return 4;
}

export function HistoryScreen(c: Ctx) {
  const { st, history } = c;
  const [pick, setPick] = useState<string | null>(null);
  if (!history) return <div class="screen"><h1>History</h1><p class="muted">Loading…</p></div>;
  const players = st.players.filter((p) => p.slot < history.players);
  const cols = lastDays(history.days);
  const sel = cols.find((x) => x.date === pick) || null;
  const perfect = cols.filter((x) => x.day && !x.day.today && x.day.met.slice(0, history.players).every(Boolean)).length;

  return (
    <div class="screen">
      <h1>Last 30 days</h1>
      <Card>
        <div class="cal-head muted small">
          {["Mo", "Tu", "We", "Th", "Fr", "Sa", "Su"].map((d) => <span key={d}>{d}</span>)}
        </div>
        <div class="cal">
          {cols.map(({ date, day }, i) => (
            <button
              key={date}
              class={"day" + (!day ? " off" : "") + (day?.today ? " now" : "") + (pick === date ? " sel" : "")}
              style={i === 0 ? { gridColumnStart: ((new Date(date + "T00:00:00Z").getUTCDay() + 6) % 7) + 1 } : undefined}
              aria-label={date}
              onClick={() => setPick(pick === date ? null : date)}
            >
              <span class="dnum">{Number(date.slice(8))}</span>
              {day &&
                players.map((p) => {
                  const m = day.minutes[p.slot] ?? 0;
                  const met = day.met[p.slot];
                  return <i key={p.slot} class={`l${level(m, history.minimum)}${met ? "" : day.today ? "" : " miss"}`} />;
                })}
            </button>
          ))}
        </div>
        {sel && (
          <div class="heat-detail">
            <b>{sel.date}</b>
            {sel.day ? (
              players.map((p) => (
                <span key={p.slot}>
                  {p.name}: {sel.day!.minutes[p.slot] ?? 0} min {sel.day!.met[p.slot] ? "✓" : sel.day!.today ? "…" : "✗"}
                </span>
              ))
            ) : (
              <span class="muted">The box was off.</span>
            )}
          </div>
        )}
        <div class="legend muted small">
          <span>less</span>
          {[0, 1, 2, 3, 4].map((l) => <i key={l} class={"l" + l} />)}
          <span>more · red = missed · stripes: {players.map((p) => p.name).join(", ")}</span>
        </div>
      </Card>

      <Card>
        <h3>Met / missed</h3>
        <table class="board">
          <thead>
            <tr><th /><th>Met</th><th>Missed</th><th>Avg</th></tr>
          </thead>
          <tbody>
            {players.map((p) => {
              const done = cols.filter((x) => x.day && !x.day.today);
              const met = done.filter((x) => x.day!.met[p.slot]).length;
              const avg = done.length ? Math.round(done.reduce((a, x) => a + (x.day!.minutes[p.slot] ?? 0), 0) / done.length) : 0;
              return (
                <tr key={p.slot}>
                  <td>{p.name}</td>
                  <td class="ok">{met}</td>
                  <td class={done.length - met ? "warn" : ""}>{done.length - met}</td>
                  <td>{avg} min</td>
                </tr>
              );
            })}
          </tbody>
        </table>
        <p class="muted small">{perfect} perfect day{perfect === 1 ? "" : "s"} (everyone met the {history.minimum} min).</p>
      </Card>
    </div>
  );
}
