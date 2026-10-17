// Friends network: other players, their pets and stats, and pet-card trades.
//
// MOCKED. There is no server yet: friends are generated on this phone from a
// fixed seed, their numbers drift with the clock so the leaderboard feels
// alive, and trades are answered by a timer. Everything is kept in
// localStorage. A real backend only has to replace the functions exported
// below (same shapes), so the screens stay as they are.
import { SPECIES, hatchOdds, type Rarity } from "./pet";
import { load, save } from "./store";

export interface FriendPet {
  species: number;
  stage: number;
  mood: number; // index into MOODS
  generation: number;
  hearts: number;
}

export interface Friend {
  id: string;
  name: string;
  household: string;
  pet: FriendPet;
  today_min: number;
  week_min: number;
  daily_streak: number;
  wins: number;
  // Cards per species (index = species) after any trades with you.
  collection: number[];
  last_seen_min: number; // minutes ago
}

export type TradeStatus = "pending" | "accepted" | "declined" | "cancelled";

export interface Trade {
  id: string;
  friend: string; // Friend.id
  dir: "out" | "in"; // out: you proposed it
  give: number; // species you hand over
  get: number; // species you receive
  status: TradeStatus;
  at: number; // ms
  settle_at?: number; // ms; when the friend answers an outgoing offer
}

// How much a card is worth when a friend weighs up an offer.
export const RARITY_VALUE: Record<Rarity, number> = { Common: 1, Uncommon: 2, Rare: 4, Legendary: 10 };
export const cardValue = (species: number) => RARITY_VALUE[SPECIES[species].rarity];

// ---- deterministic fake friends ------------------------------------------------

function rng(seed: number) {
  let s = seed >>> 0 || 1;
  return () => {
    s ^= s << 13;
    s ^= s >>> 17;
    s ^= s << 5;
    return (s >>> 0) / 4294967296;
  };
}

function rollSpecies(r: () => number): number {
  let x = r() * 100;
  for (const sp of SPECIES) {
    x -= hatchOdds(sp.index);
    if (x < 0) return sp.index;
  }
  return 0;
}

const PEOPLE: [string, string, number][] = [
  // name, household, how dedicated they are (0..1)
  ["Maja", "The Lindqvists", 0.9],
  ["Oskar", "Flat 4B", 0.55],
  ["Lina", "Lina & Sam", 0.75],
  ["Elias", "Team Elias", 0.35],
  ["Noor", "Noor's family", 0.85],
  ["Viktor", "The Bergs", 0.6],
];

function baseFriends(): Friend[] {
  const day = Math.floor(Date.now() / 86400000);
  const hour = new Date().getHours();
  return PEOPLE.map(([name, household, keen], i) => {
    const r = rng(1009 * (i + 1));
    const collection = SPECIES.map(() => 0);
    const pets = 3 + Math.floor(r() * 9 * keen);
    for (let k = 0; k < pets; k++) collection[rollSpecies(r)]++;
    const species = rollSpecies(r);
    collection[species] = Math.max(1, collection[species]);
    const today = rng(day * 31 + i * 7);
    const dayShare = Math.min(1, Math.max(0, (hour - 7) / 14)); // the day fills up from 07:00
    return {
      id: "f" + i,
      name,
      household,
      pet: {
        species,
        stage: 1 + Math.floor(r() * 5 * keen + r()),
        mood: keen > 0.6 ? 0 : keen > 0.4 ? 1 : 2,
        generation: 1 + Math.floor(r() * 3 * (1 - keen)),
        hearts: 1 + Math.round(2 * keen),
      },
      today_min: Math.round((20 + today() * 90 * keen) * dayShare),
      week_min: Math.round(120 + keen * 500 + today() * 120),
      daily_streak: Math.floor(keen * 14 * r()),
      wins: Math.floor(10 + keen * 60 * r()),
      collection,
      last_seen_min: Math.floor(today() * 180),
    };
  });
}

// ---- persisted trades ------------------------------------------------------------

const KEY = "trades";
let trades: Trade[] = (() => {
  try {
    return JSON.parse(load(KEY) || "[]") as Trade[];
  } catch {
    return [];
  }
})();
const persist = () => save(KEY, JSON.stringify(trades.slice(-100)));

type Listener = (message: string, tone: string) => void;
const listeners = new Set<Listener>();
const notify = (m: string, tone = "") => listeners.forEach((l) => l(m, tone));

function apply(list: Friend[]): Friend[] {
  const byId = new Map(list.map((f) => [f.id, f]));
  for (const t of trades) {
    if (t.status !== "accepted") continue;
    const f = byId.get(t.friend);
    if (!f) continue;
    f.collection[t.get] = Math.max(0, f.collection[t.get] - 1);
    f.collection[t.give]++;
  }
  return list;
}

// Card counts you own: what your box hatched, plus/minus accepted trades.
export function myCards(boxCounts: number[]): number[] {
  const mine = SPECIES.map((s) => boxCounts[s.index] || 0);
  for (const t of trades) {
    if (t.status !== "accepted") continue;
    mine[t.give]--;
    mine[t.get]++;
  }
  return mine.map((n) => Math.max(0, n));
}

// Species you have ever held, even if traded away again: they stay in the dex.
export function everHeld(boxCounts: number[]): boolean[] {
  const seen = SPECIES.map((s) => (boxCounts[s.index] || 0) > 0);
  for (const t of trades) if (t.status === "accepted") seen[t.get] = true;
  return seen;
}

// ---- the API the screens use -----------------------------------------------------

export const MOCKED = true;

export function friends(): Friend[] {
  settle();
  return apply(baseFriends());
}

export function friend(id: string): Friend | undefined {
  return friends().find((f) => f.id === id);
}

export function listTrades(): Trade[] {
  settle();
  return [...trades].sort((a, b) => b.at - a.at);
}

export function propose(friendId: string, give: number, get: number): Trade {
  const t: Trade = {
    id: Math.random().toString(36).slice(2, 10),
    friend: friendId,
    dir: "out",
    give,
    get,
    status: "pending",
    at: Date.now(),
    settle_at: Date.now() + 4000 + Math.random() * 6000,
  };
  trades.push(t);
  persist();
  schedule();
  return t;
}

export function respond(id: string, accept: boolean, boxCounts: number[]): string | null {
  const t = trades.find((x) => x.id === id);
  if (!t || t.status !== "pending") return "That offer is gone.";
  if (accept && t.dir === "in" && myCards(boxCounts)[t.give] < 1)
    return `You no longer have a ${SPECIES[t.give].name} to give.`;
  if (t.dir === "out") t.status = "cancelled";
  else t.status = accept ? "accepted" : "declined";
  persist();
  return null;
}

export function subscribe(fn: Listener): () => void {
  listeners.add(fn);
  schedule();
  return () => listeners.delete(fn);
}

// Friends answer outgoing offers: fair-ish offers get accepted.
function settle() {
  const list = apply(baseFriends());
  let changed = false;
  for (const t of trades) {
    if (t.dir !== "out" || t.status !== "pending" || (t.settle_at ?? 0) > Date.now()) continue;
    const f = list.find((x) => x.id === t.friend);
    const fair = cardValue(t.give) >= cardValue(t.get) * 0.75;
    const has = !!f && f.collection[t.get] > 0;
    t.status = fair && has ? "accepted" : "declined";
    changed = true;
    const name = f?.name ?? "Your friend";
    if (t.status === "accepted")
      notify(`${name} accepted! Your ${SPECIES[t.give].name} for their ${SPECIES[t.get].name}.`, "gold");
    else notify(`${name} declined your ${SPECIES[t.give].name} for their ${SPECIES[t.get].name}.`, "bad");
  }
  if (changed) persist();
}

let timer: number | undefined;
function schedule() {
  clearTimeout(timer);
  const next = trades
    .filter((t) => t.dir === "out" && t.status === "pending")
    .map((t) => t.settle_at ?? 0)
    .sort((a, b) => a - b)[0];
  if (next === undefined) return;
  timer = window.setTimeout(() => {
    settle();
    schedule();
  }, Math.max(0, next - Date.now()) + 50);
}

// Now and then a friend offers you something for a card you have.
export function maybeIncomingOffer(boxCounts: number[]) {
  const mine = myCards(boxCounts);
  if (trades.some((t) => t.dir === "in" && t.status === "pending")) return;
  const last = trades.filter((t) => t.dir === "in").reduce((m, t) => Math.max(m, t.at), 0);
  if (Date.now() - last < 10 * 60000) return;
  const owned = mine.map((n, i) => (n > 0 ? i : -1)).filter((i) => i >= 0);
  if (!owned.length) return;
  const list = friends();
  const r = rng(Date.now() & 0xffffffff);
  const f = list[Math.floor(r() * list.length)];
  // Ask for a card you have (spares first), offer one you are missing if possible.
  const spares = owned.filter((i) => mine[i] > 1);
  const want = (spares.length ? spares : owned)[Math.floor(r() * (spares.length || owned.length))];
  const theirs = f.collection.map((n, i) => (n > 0 && i !== want ? i : -1)).filter((i) => i >= 0);
  if (!theirs.length) return;
  const missing = theirs.filter((i) => mine[i] === 0);
  const pool = missing.length ? missing : theirs;
  const give = pool[Math.floor(r() * pool.length)];
  trades.push({ id: Math.random().toString(36).slice(2, 10), friend: f.id, dir: "in", give: want, get: give, status: "pending", at: Date.now() });
  persist();
  notify(`${f.name} wants to trade their ${SPECIES[give].name} for your ${SPECIES[want].name}.`, "good");
}

// Clears every trade (Settings > reset demo network).
export function resetDemo() {
  trades = [];
  persist();
}
