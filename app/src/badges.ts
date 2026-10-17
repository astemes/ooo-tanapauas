// Badges are computed on the phone from /api/state and /api/history; the box doesn't know them.
import type { History, State } from "./types";

export interface Badge {
  icon: string;
  name: string;
  desc: string;
  have: boolean;
  progress?: [number, number];
}

function bestRun(flags: boolean[]): number {
  let best = 0, run = 0;
  for (const f of flags) best = Math.max(best, (run = f ? run + 1 : 0));
  return best;
}

export function badges(st: State, h: History | null, me: number): Badge[] {
  const p = st.players.find((x) => x.slot === me)!;
  const n = st.settings.players;
  // History is newest first; walk it oldest first and skip the unfinished day.
  const done = h ? h.days.filter((d) => !d.today).reverse() : [];
  const myDays = h ? h.days.map((d) => d.minutes[me] ?? 0) : [];
  const myBestDay = Math.max(0, ...myDays);
  const perfectRun = bestRun(done.map((d) => d.met.slice(0, n).every(Boolean)));
  const lifetimeH = st.life.total_min / 60;
  const cap = (v: number, max: number): [number, number] => [Math.min(v, max), max];

  return [
    { icon: "🥚", name: "Hatched", desc: "Your pet left the egg", have: st.pet.stage_index >= 1 || st.pet.generation > 1 },
    { icon: "✅", name: "First daily", desc: "Hit the daily minimum once", have: p.best_daily_streak >= 1 || p.met_today },
    { icon: "⏰", name: "First full hour", desc: "60 phone-free minutes in a day", have: myBestDay >= 60 || st.life.longest_min >= 60, progress: cap(myBestDay, 60) },
    { icon: "🔥", name: "7-day streak", desc: "Met the minimum 7 days in a row", have: p.best_daily_streak >= 7, progress: cap(p.best_daily_streak, 7) },
    { icon: "🌋", name: "30-day streak", desc: "A whole month of discipline", have: p.best_daily_streak >= 30, progress: cap(p.best_daily_streak, 30) },
    { icon: "🌈", name: "Perfect week", desc: "Everyone met the minimum 7 days running", have: perfectRun >= 7, progress: cap(perfectRun, 7) },
    { icon: "💛", name: "Bonus life", desc: "Keep a session going to double its goal", have: st.life.bonus_lives >= 1 },
    { icon: "💖", name: "5 hearts", desc: "Bank the pet up to five hearts", have: st.pet.hearts >= st.pet.bonus_cap, progress: cap(st.pet.hearts, st.pet.bonus_cap) },
    { icon: "🦉", name: "Elder reached", desc: "Raise a pet all the way", have: st.pet.stage_index >= 5, progress: cap(st.pet.stage_index, 5) },
    { icon: "🏃", name: "Marathon", desc: "A 2-hour session", have: st.life.longest_min >= 120, progress: cap(st.life.longest_min, 120) },
    { icon: "📵", name: "10 hours", desc: "10 phone-free hours together", have: lifetimeH >= 10, progress: cap(Math.floor(lifetimeH), 10) },
    { icon: "🏔️", name: "100 hours", desc: "100 phone-free hours together", have: lifetimeH >= 100, progress: cap(Math.floor(lifetimeH), 100) },
    { icon: "😇", name: "Clean hands", desc: "Never lifted or missed a day (after a week)", have: p.lifted === 0 && p.missed_days === 0 && done.length >= 7 },
  ];
}
