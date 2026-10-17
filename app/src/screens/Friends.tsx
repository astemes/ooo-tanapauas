import { useEffect, useState } from "preact/hooks";
import type { Ctx } from "../app";
import { SPECIES, moodIndex } from "../pet";
import {
  MOCKED,
  cardValue,
  friend as getFriend,
  friends as listFriends,
  listTrades,
  myCards,
  propose,
  respond,
  subscribe,
  type Friend,
  type Trade,
} from "../network";
import { Card, Pet } from "../ui";
import { RARITY_CLASS, RarityTag, boxCounts } from "./Collect";

type View = { kind: "list" } | { kind: "friend"; id: string } | { kind: "trade"; id: string; get: number | null };

export function Friends(c: Ctx) {
  const [seg, setSeg] = useState<"board" | "trades">("board");
  const [view, setView] = useState<View>({ kind: "list" });
  const [, setVersion] = useState(0);
  // Re-render when a mocked friend answers an offer.
  useEffect(() => subscribe(() => setVersion((v) => v + 1)), []);

  if (view.kind === "friend") return <FriendView c={c} id={view.id} onBack={() => setView({ kind: "list" })} onTrade={(get) => setView({ kind: "trade", id: view.id, get })} />;
  if (view.kind === "trade")
    return (
      <TradeComposer
        c={c}
        id={view.id}
        initialGet={view.get}
        onDone={() => {
          setSeg("trades");
          setView({ kind: "list" });
        }}
        onBack={() => setView({ kind: "friend", id: view.id })}
      />
    );

  const pendingIn = listTrades().filter((t) => t.dir === "in" && t.status === "pending").length;
  return (
    <div class="screen">
      <h1>Friends</h1>
      {MOCKED && <p class="demo">Demo network: these friends and their trades are simulated on this phone.</p>}
      <div class="seg">
        <button class={seg === "board" ? "on" : ""} onClick={() => setSeg("board")}>
          Leaderboard
        </button>
        <button class={seg === "trades" ? "on" : ""} onClick={() => setSeg("trades")}>
          Trades {pendingIn > 0 && <i class="badge-dot">{pendingIn}</i>}
        </button>
      </div>
      {seg === "board" ? <Board c={c} onOpen={(id) => setView({ kind: "friend", id })} /> : <Trades c={c} onOpen={(id) => setView({ kind: "friend", id })} />}
    </div>
  );
}

// Your phone-free minutes over the last 7 days (today included).
function myWeek(c: Ctx): number {
  return (c.history?.days ?? []).slice(0, 7).reduce((sum, d) => sum + (d.minutes[c.me] ?? 0), 0);
}

function Board({ c, onOpen }: { c: Ctx; onOpen: (id: string) => void }) {
  const me = c.st.players.find((p) => p.slot === c.me)!;
  const mine = myCards(boxCounts(c)).filter((n) => n > 0).length;
  type Row = { id: string | null; name: string; sub: string; week: number; today: number; streak: number; species: number; pet: { species: number | null; stage: number; mood: number } };
  const rows: Row[] = [
    {
      id: null,
      name: me.name,
      sub: "You",
      week: myWeek(c),
      today: me.today_min,
      streak: me.daily_streak,
      species: mine,
      pet: { species: c.st.pet.species_index, stage: c.st.pet.stage_index, mood: moodIndex(c.st.pet.mood) },
    },
    ...listFriends().map((f) => ({
      id: f.id,
      name: f.name,
      sub: f.household,
      week: f.week_min,
      today: f.today_min,
      streak: f.daily_streak,
      species: f.collection.filter((n) => n > 0).length,
      pet: f.pet,
    })),
  ].sort((a, b) => b.week - a.week);

  return (
    <>
      <p class="muted small">Phone-free minutes in the last 7 days. Tap a friend to see their pets.</p>
      <div class="board-list">
        {rows.map((r, i) => (
          <button key={r.id ?? "me"} class={"friend-row" + (r.id === null ? " me" : "")} onClick={() => r.id && onOpen(r.id)} disabled={r.id === null}>
            <span class="rank">{i === 0 ? "👑" : i + 1}</span>
            <span class="mini-pet">
              <Pet species={r.pet.species} stage={r.pet.stage} mood={r.pet.mood} scale={3} still />
            </span>
            <span class="who">
              <b>{r.name}</b>
              <span class="muted small">
                {r.sub} · {r.species}/{SPECIES.length} species
              </span>
            </span>
            <span class="score">
              <b>{r.week}</b>
              <span class="muted small">
                min · 🔥{r.streak}
              </span>
            </span>
          </button>
        ))}
      </div>
    </>
  );
}

function FriendView({ c, id, onBack, onTrade }: { c: Ctx; id: string; onBack: () => void; onTrade: (get: number | null) => void }) {
  const f = getFriend(id);
  if (!f) return null;
  const mine = myCards(boxCounts(c));
  const owned = f.collection.map((n, i) => ({ n, i })).filter((x) => x.n > 0);
  return (
    <div class="screen">
      <button class="back" onClick={onBack}>
        ‹ Friends
      </button>
      <header class="pet-head">
        <div class="pet-stage">
          <Pet species={f.pet.species} stage={f.pet.stage} mood={f.pet.mood} scale={8} />
        </div>
        <div class="pet-meta">
          <div class="stage">{f.name}</div>
          <span class="muted">{f.household}</span>
          <span>
            {SPECIES[f.pet.species].name} · gen {f.pet.generation} <RarityTag rarity={SPECIES[f.pet.species].rarity} />
          </span>
          <span class="muted small">{f.last_seen_min < 5 ? "Active now" : `Seen ${f.last_seen_min} min ago`}</span>
        </div>
      </header>
      <Card>
        <div class="stats">
          <Stat v={`${f.today_min} min`} k="phone-free today" />
          <Stat v={`${f.week_min} min`} k="last 7 days" />
          <Stat v={`🔥 ${f.daily_streak}`} k="daily streak" />
          <Stat v={f.wins} k="sessions won" />
        </div>
      </Card>
      <Card>
        <div class="row between">
          <h3>Their pets</h3>
          <span class="muted small">
            {owned.length}/{SPECIES.length} species
          </span>
        </div>
        <div class="dex small-dex">
          {owned.map(({ n, i }) => (
            <button key={i} class={"dex-card " + RARITY_CLASS[SPECIES[i].rarity]} onClick={() => onTrade(i)}>
              <Pet species={i} stage={3} mood={0} scale={3} still />
              <b>{SPECIES[i].name}</b>
              <span class="small">
                ×{n}
                {mine[i] === 0 && <em class="new"> new for you</em>}
              </span>
            </button>
          ))}
        </div>
      </Card>
      <button class="btn" onClick={() => onTrade(null)}>
        Propose a trade with {f.name}
      </button>
    </div>
  );
}

function TradeComposer({ c, id, initialGet, onDone, onBack }: { c: Ctx; id: string; initialGet: number | null; onDone: () => void; onBack: () => void }) {
  const f = getFriend(id)!;
  const mine = myCards(boxCounts(c));
  const [get, setGet] = useState<number | null>(initialGet);
  const [give, setGive] = useState<number | null>(null);
  const theirs = f.collection.map((n, i) => (n > 0 ? i : -1)).filter((i) => i >= 0);
  const yours = mine.map((n, i) => (n > 0 ? i : -1)).filter((i) => i >= 0);
  const fair = give !== null && get !== null ? cardValue(give) >= cardValue(get) * 0.75 : null;

  return (
    <div class="screen">
      <button class="back" onClick={onBack}>
        ‹ {f.name}
      </button>
      <h1>Trade with {f.name}</h1>
      <Card>
        <h3>You get</h3>
        <Picker list={theirs} counts={f.collection} picked={get} onPick={setGet} mark={(i) => mine[i] === 0} />
      </Card>
      <Card>
        <h3>You give</h3>
        {yours.length ? (
          <Picker list={yours} counts={mine} picked={give} onPick={setGive} />
        ) : (
          <p class="muted">You have no pet cards yet. Hatch an egg on the box first.</p>
        )}
      </Card>
      {fair !== null && (
        <p class={"small " + (fair ? "ok" : "warn")}>
          {fair ? `Looks fair. ${f.name} will probably say yes.` : `${f.name} values ${SPECIES[get!].rarity.toLowerCase()} pets more than that. Expect a no.`}
        </p>
      )}
      <button
        class="btn"
        disabled={give === null || get === null}
        onClick={() => {
          propose(f.id, give!, get!);
          onDone();
        }}
      >
        Send offer
      </button>
    </div>
  );
}

function Picker({ list, counts, picked, onPick, mark }: { list: number[]; counts: number[]; picked: number | null; onPick: (i: number) => void; mark?: (i: number) => boolean }) {
  return (
    <div class="dex small-dex">
      {list.map((i) => (
        <button key={i} class={"dex-card " + RARITY_CLASS[SPECIES[i].rarity] + (picked === i ? " picked" : "")} onClick={() => onPick(i)}>
          <Pet species={i} stage={3} mood={0} scale={3} still />
          <b>{SPECIES[i].name}</b>
          <span class="small">
            ×{counts[i]}
            {mark?.(i) && <em class="new"> new</em>}
          </span>
        </button>
      ))}
    </div>
  );
}

function Trades({ c, onOpen }: { c: Ctx; onOpen: (id: string) => void }) {
  const [msg, setMsg] = useState<string | null>(null);
  const [, bump] = useState(0);
  const all = listTrades();
  const names = new Map(listFriends().map((f: Friend) => [f.id, f.name]));
  const incoming = all.filter((t) => t.dir === "in" && t.status === "pending");
  const outgoing = all.filter((t) => t.dir === "out" && t.status === "pending");
  const done = all.filter((t) => t.status !== "pending").slice(0, 12);
  const act = (t: Trade, accept: boolean) => {
    setMsg(respond(t.id, accept, boxCounts(c)));
    bump((n) => n + 1);
  };

  return (
    <>
      {msg && <p class="warn small">{msg}</p>}
      <Card>
        <h3>Offers for you</h3>
        {incoming.length === 0 && <p class="muted">No offers right now.</p>}
        {incoming.map((t) => (
          <div class="trade" key={t.id}>
            <TradeLine t={t} who={names.get(t.friend) ?? "?"} />
            <div class="row">
              <button class="btn" onClick={() => act(t, true)}>
                Accept
              </button>
              <button class="btn ghost" onClick={() => act(t, false)}>
                Decline
              </button>
            </div>
          </div>
        ))}
      </Card>
      <Card>
        <h3>Waiting for an answer</h3>
        {outgoing.length === 0 && <p class="muted">Nothing pending. Open a friend to propose a trade.</p>}
        {outgoing.map((t) => (
          <div class="trade" key={t.id}>
            <TradeLine t={t} who={names.get(t.friend) ?? "?"} />
            <div class="row between">
              <span class="muted small">Waiting for {names.get(t.friend)}…</span>
              <button class="link" onClick={() => act(t, false)}>
                Withdraw
              </button>
            </div>
          </div>
        ))}
      </Card>
      {done.length > 0 && (
        <Card>
          <h3>History</h3>
          {done.map((t) => (
            <div class="trade done" key={t.id} onClick={() => onOpen(t.friend)}>
              <TradeLine t={t} who={names.get(t.friend) ?? "?"} />
              <span class={"small " + (t.status === "accepted" ? "ok" : "muted")}>{t.status}</span>
            </div>
          ))}
        </Card>
      )}
    </>
  );
}

function TradeLine({ t, who }: { t: Trade; who: string }) {
  return (
    <div class="trade-line">
      <span class="swap">
        <Pet species={t.give} stage={3} mood={0} scale={2} still />
        <span>
          your <b>{SPECIES[t.give].name}</b>
        </span>
      </span>
      <span class="arrow">⇄</span>
      <span class="swap">
        <Pet species={t.get} stage={3} mood={0} scale={2} still />
        <span>
          {who}'s <b>{SPECIES[t.get].name}</b>
        </span>
      </span>
    </div>
  );
}

const Stat = ({ v, k }: { v: string | number; k: string }) => (
  <div class="stat">
    <b>{v}</b>
    <span>{k}</span>
  </div>
);
