import { useCallback, useEffect, useRef, useState } from "preact/hooks";
import { api, ApiError } from "./api";
import { load, save } from "./store";
import type { BoxEvent, Collection, History, State } from "./types";
import { Pet } from "./ui";
import { moodIndex } from "./pet";
import { Home } from "./screens/Home";
import { Group } from "./screens/Group";
import { HistoryScreen } from "./screens/History";
import { Collect } from "./screens/Collect";
import { Friends } from "./screens/Friends";
import { maybeIncomingOffer, subscribe } from "./network";
import { SettingsScreen } from "./screens/Settings";

const POLL_MS = 5000;
type Tab = "home" | "group" | "collect" | "friends" | "history" | "settings";
const TABS: [Tab, string, string][] = [
  ["home", "Pet", "♥"],
  ["group", "Group", "☺"],
  ["collect", "Collect", "★"],
  ["friends", "Friends", "⇄"],
  ["history", "History", "▦"],
  ["settings", "Me", "⚙"],
];

export interface Ctx {
  st: State;
  me: number;
  // Seconds since `st` was fetched, for live counters between polls.
  age: () => number;
  history: History | null;
  collection: Collection | null;
  events: BoxEvent[];
  refresh: () => void;
  setMe: (slot: number | null) => void;
}

interface Toast {
  id: number;
  text: string;
  tone: string;
}

const TOAST_TONE: Record<string, string> = {
  DailyMissed: "bad", SessionEnded: "", BonusLife: "gold", Evolved: "gold", Hatched: "gold",
  TargetReached: "good", EmergencyUsed: "bad", PlayerJoined: "good", SessionStarted: "good", NewSpecies: "gold",
};

export function App() {
  const [st, setSt] = useState<State | null>(null);
  const [err, setErr] = useState<string | null>(null);
  const [history, setHistory] = useState<History | null>(null);
  const [collection, setCollection] = useState<Collection | null>(null);
  const [events, setEvents] = useState<BoxEvent[]>([]);
  const [toasts, setToasts] = useState<Toast[]>([]);
  const [tab, setTab] = useState<Tab>("home");
  const [me, setMeState] = useState<number | null>(() => {
    const v = load("slot");
    return v === null ? null : Number(v);
  });
  const fetchedAt = useRef(0);
  const cursor = useRef<number | null>(null);
  const historyAt = useRef(0);
  const timeSent = useRef(false);
  const timer = useRef<number>();

  const setMe = useCallback((slot: number | null) => {
    save("slot", slot === null ? null : String(slot));
    setMeState(slot);
  }, []);

  const toast = (text: string, tone = "") => {
    const id = Math.random();
    setToasts((t) => [...t.slice(-2), { id, text, tone }]);
    setTimeout(() => setToasts((t) => t.filter((x) => x.id !== id)), 6000);
  };

  const pollEvents = async (): Promise<void> => {
    const since = cursor.current ?? 0;
    const r = await api.events(since);
    if (cursor.current !== null && r.next < since) {
      // Box rebooted and ids restarted: take what it has as the new baseline.
      cursor.current = null;
      return pollEvents();
    }
    const first = cursor.current === null;
    // A hatch or growth changes the collection: fetch it with the next poll.
    if (r.events.some((e) => e.type === "Evolved" || e.type === "NewSpecies")) historyAt.current = 0;
    if (!first) for (const e of r.events) toast(e.text, TOAST_TONE[e.type] || "");
    cursor.current = r.next;
    if (first) setEvents(r.events.slice(-64));
    else if (r.events.length) setEvents((old) => [...old, ...r.events].slice(-64));
  };

  const poll = useCallback(async () => {
    clearTimeout(timer.current);
    try {
      let s = await api.state();
      if (!s.clock && !timeSent.current) {
        // The box has no NTP: lend it this phone's clock.
        timeSent.current = true;
        try {
          await api.time();
          s = await api.state();
        } catch {
          /* someone else may have set it */
        }
      }
      fetchedAt.current = Date.now();
      setSt(s);
      setErr(null);
      await pollEvents();
      if (Date.now() - historyAt.current > 60000) {
        historyAt.current = Date.now();
        setHistory(await api.history(30));
        const col = await api.collection();
        setCollection(col);
        maybeIncomingOffer(col.species.map((s) => s.count));
      }
    } catch (e) {
      setErr(e instanceof ApiError ? e.message : String(e));
    }
    if (document.visibilityState === "visible") timer.current = window.setTimeout(poll, POLL_MS);
  }, []);

  useEffect(() => {
    poll();
    const onVis = () => {
      if (document.visibilityState === "visible") poll();
      else clearTimeout(timer.current);
    };
    document.addEventListener("visibilitychange", onVis);
    return () => document.removeEventListener("visibilitychange", onVis);
  }, [poll]);

  // Simulated friends answering trades.
  useEffect(() => subscribe((text, tone) => toast(text, tone)), []);

  const refresh = useCallback(() => {
    historyAt.current = 0;
    poll();
  }, [poll]);

  if (!st)
    return (
      <div class="splash">
        <Pet stage={0} mood={err ? 2 : 0} scale={10} />
        <p>{err ? `${err}. Retrying…` : "Finding the box…"}</p>
      </div>
    );

  const players = st.players.filter((p) => p.slot < st.settings.players);
  if (me === null || !players.some((p) => p.slot === me)) return <Picker st={st} onPick={setMe} />;

  const ctx: Ctx = { st, me, age: () => (Date.now() - fetchedAt.current) / 1000, history, collection, events, refresh, setMe };
  return (
    <div class="shell">
      {err && <div class="offline">{err} · showing last known state</div>}
      <main>
        {tab === "home" && <Home {...ctx} />}
        {tab === "group" && <Group {...ctx} />}
        {tab === "history" && <HistoryScreen {...ctx} />}
        {tab === "collect" && <Collect {...ctx} />}
        {tab === "friends" && <Friends {...ctx} />}
        {tab === "settings" && <SettingsScreen {...ctx} />}
      </main>
      <div class="toasts">
        {toasts.map((t) => (
          <div key={t.id} class={"toast " + t.tone} onClick={() => setToasts((x) => x.filter((y) => y.id !== t.id))}>
            {t.text}
          </div>
        ))}
      </div>
      <nav class="tabs">
        {TABS.map(([id, label, icon]) => (
          <button key={id} class={tab === id ? "on" : ""} onClick={() => setTab(id)}>
            <span>{icon}</span>
            {label}
          </button>
        ))}
      </nav>
    </div>
  );
}

function Picker({ st, onPick }: { st: State; onPick: (slot: number) => void }) {
  const players = st.players.filter((p) => p.slot < st.settings.players);
  return (
    <div class="picker">
      <Pet species={st.pet.species_index} stage={st.pet.stage_index} mood={moodIndex(st.pet.mood)} scale={10} />
      <h1>Which player are you?</h1>
      <p class="muted">Pick the slot your phone sleeps in. This phone will remember.</p>
      <div class="pick-list">
        {players.map((p) => (
          <button key={p.slot} class="pick" onClick={() => onPick(p.slot)}>
            <span class="slot">{p.slot + 1}</span>
            {p.name}
          </button>
        ))}
      </div>
    </div>
  );
}
