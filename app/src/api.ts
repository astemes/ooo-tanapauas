import type { BoxEvent, Collection, History, Settings, State } from "./types";

// Production: the box serves this page, so relative URLs hit it. For development,
// `?box=http://192.168.1.50` (remembered; `?box=` clears it) points at another box.
const BOX_KEY = "petbox.box";

function boxBase(): string {
  const q = new URLSearchParams(location.search).get("box");
  try {
    if (q !== null) {
      if (q) localStorage.setItem(BOX_KEY, q.replace(/\/+$/, ""));
      else localStorage.removeItem(BOX_KEY);
    }
    return localStorage.getItem(BOX_KEY) || "";
  } catch {
    return q ? q.replace(/\/+$/, "") : "";
  }
}

export const BASE = boxBase();

export class ApiError extends Error {
  constructor(public status: number, message: string) {
    super(message);
  }
}

// The box handles one request at a time, so all calls go through one queue.
let queue: Promise<unknown> = Promise.resolve();

function request<T>(path: string, body?: unknown): Promise<T> {
  const run = async (): Promise<T> => {
    const ctl = new AbortController();
    const timer = setTimeout(() => ctl.abort(), 8000);
    try {
      const res = await fetch(BASE + path, {
        method: body === undefined ? "GET" : "POST",
        headers: body === undefined ? undefined : { "Content-Type": "application/json" },
        body: body === undefined ? undefined : JSON.stringify(body),
        signal: ctl.signal,
        cache: "no-store",
      });
      const data = await res.json().catch(() => ({}));
      if (!res.ok) throw new ApiError(res.status, (data && data.error) || `HTTP ${res.status}`);
      return data as T;
    } catch (e) {
      if (e instanceof ApiError) throw e;
      throw new ApiError(0, "Can't reach the box");
    } finally {
      clearTimeout(timer);
    }
  };
  const p = queue.then(run, run);
  queue = p.catch(() => undefined);
  return p;
}

export const api = {
  state: () => request<State>("/api/state"),
  history: (days = 30) => request<History>(`/api/history?days=${days}`),
  collection: () => request<Collection>("/api/collection"),
  egg: (pin: string, minutes: number) =>
    request<{ ok: boolean; egg_minutes: number; rare_chance: number }>("/api/egg", { pin, minutes }),
  newPet: (pin: string) => request<{ ok: boolean; generation: number }>("/api/pet/new", { pin }),
  events: (since: number) => request<{ next: number; events: BoxEvent[] }>(`/api/events?since=${since}`),
  rename: (slot: number, name: string, pin: string) =>
    request<{ ok: boolean; name: string }>(`/api/players/${slot}/name`, { name, pin }),
  settings: (pin: string, s: Partial<Omit<Settings, "players">>) => request<Settings>("/api/settings", { pin, ...s }),
  time: () => request<{ ok: boolean; clock: boolean }>("/api/time", { unix: Math.floor(Date.now() / 1000) }),
};
