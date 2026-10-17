// Mock pet box for app development: serves /api/* per docs/api.md with a simulated
// household. Plain Node, no dependencies.
//
//   npm run mock                      # http://localhost:8787, PIN 1234
//   MOCK_SPEED=60 npm run mock        # sim seconds per real second (default 10)
//   MOCK_CLOCK=1 npm run mock         # box already knows the time (skip POST /api/time)
//   MOCK_TZ=120 npm run mock          # box time zone in minutes (default 0, to test the tz banner)
import http from "node:http";

const PORT = Number(process.env.PORT || 8787);
const SPEED = Number(process.env.MOCK_SPEED || 10);
const PIN = "1234";
const STAGES = ["Egg", "Baby", "Child", "Teen", "Adult", "Elder"];
const STAGE_XP = [0, 30, 150, 500, 1500, 4000];
const MAX_HEARTS = 3, BONUS_CAP = 5;
// Mirrors lib/petcore/src/species.cpp (order = species_index).
const SPECIES = [
  ["Blob", "Common"], ["Chick", "Common"], ["Bunny", "Common"], ["Frog", "Common"],
  ["Cat", "Uncommon"], ["Bear", "Uncommon"], ["Pig", "Uncommon"],
  ["Fox", "Rare"], ["Panda", "Rare"], ["Axolotl", "Rare"], ["Deer", "Rare"],
  ["Dragon", "Legendary"], ["Unicorn", "Legendary"],
];
// Egg sizes (species.h kEggs): percent per tier Common/Uncommon/Rare/Legendary.
const EGGS = { 15: [75, 20, 5, 0], 30: [55, 28, 14, 3], 60: [40, 32, 22, 6], 90: [25, 33, 30, 12] };
const TIERS = ["Common", "Uncommon", "Rare", "Legendary"];
const rareChance = (m) => (EGGS[m] || EGGS[30])[2] + (EGGS[m] || EGGS[30])[3];

// Seeded RNG so restarts look alike.
let seed = 7;
const rnd = () => ((seed = (seed * 1103515245 + 12345) & 0x7fffffff) / 0x7fffffff);
const pick = (a) => a[Math.floor(rnd() * a.length)];

const settings = { players: 3, daily_minimum_min: 30, day_reset_hour: 4, tz_offset_min: Number(process.env.MOCK_TZ || 0) };
let clockKnown = process.env.MOCK_CLOCK === "1";
const realStart = Date.now();
let simBase = Math.floor(realStart / 1000); // unix at sim start
let simAt = realStart; // real ms when simBase applied
const now = () => simBase + Math.floor(((Date.now() - simAt) / 1000) * SPEED);

const players = ["Anna", "Ben", "Cleo", "Dan"].map((name, slot) => ({
  slot, name, docked: false, in_session: false, lifting: false, today_min: 0, met_today: false,
  daily_streak: [3, 0, 6, 0][slot], best_daily_streak: [5, 2, 8, 0][slot],
  lifted: [1, 3, 0, 0][slot], missed_days: [0, 2, 1, 0][slot],
}));
const pet = { xp: 96, hearts: 3, happiness: 72, generation: 3, species: 4, egg_minutes: 30 };
const eggLocked = () => pet.xp > 0 || state === "Arming" || state === "Running";
const life = { total_min: 610, longest_min: 95, wins: 9, broken: 2, streak: 1, best_streak: 4, perfect_days: 3, bonus_lives: 0, graveyard: 2 };
// Pets this box has hatched: [count, best stage] per species.
const collection = SPECIES.map(() => [0, 0]);
collection[0] = [1, 3]; // gen 1 Blob made it to Teen
collection[1] = [1, 2]; // gen 2 Chick
collection[4] = [1, 1]; // the Cat on screen now
function rollSpecies() {
  let r = rnd() * 100;
  for (const [i, w] of (EGGS[pet.egg_minutes] || EGGS[30]).entries()) {
    const tier = TIERS[i];
    const members = SPECIES.map((sp, i) => (sp[1] === tier ? i : -1)).filter((i) => i >= 0);
    if (r < w) return members[Math.floor(rnd() * members.length)];
    r -= w;
  }
  return 0;
}
function grew(before) {
  const s = stageIndex();
  if (before === 0) {
    pet.species = rollSpecies();
    const c = collection[pet.species];
    if (c[0]++ === 0) emit("NewSpecies", null, pet.species, `New in your collection: ${SPECIES[pet.species][0]} (${SPECIES[pet.species][1]}).`);
    emit("Evolved", null, s, `It hatched! Say hi to your new ${SPECIES[pet.species][0]}.`);
  } else emit("Evolved", null, s, `Your ${SPECIES[pet.species][0]} grew into a ${STAGES[s]}!`);
  collection[pet.species][1] = Math.max(collection[pet.species][1], s);
}
let shame = { slot: 1, name: "Ben", reason: "MissedDaily", at: now() - 50000, text: "Ben missed the daily 30 min." };
let session = blankSession();
let state = "Idle", stateUntil = now() + 90, result = null, bonusToday = false;

// ---- history: newest first --------------------------------------------------
const dateOf = (unix) => {
  const local = unix + settings.tz_offset_min * 60 - settings.day_reset_hour * 3600;
  return new Date(local * 1000).toISOString().slice(0, 10);
};
let today = dateOf(now());
const past = [];
for (let i = 1; i <= 45; i++) {
  if (rnd() < 0.06) continue; // box was off
  const d = new Date(Date.parse(today) - i * 86400000).toISOString().slice(0, 10);
  const minutes = [0, 1, 2, 3].map((s) => (s < settings.players ? Math.round(rnd() < 0.2 ? rnd() * 25 : 25 + rnd() * 70) : 0));
  past.push({ date: d, minutes, met: minutes.map((m, s) => s < settings.players && m >= settings.daily_minimum_min), today: false });
}

// ---- events ------------------------------------------------------------------
let nextId = 1;
const events = [];
function emit(type, slot, value, text) {
  events.push({ id: nextId++, at: clockKnown ? now() : null, type, slot, value, text });
  if (events.length > 64) events.shift();
}
emit("DailyMissed", 1, 9, "Ben only managed 9 of 30 min yesterday. The pet lost a heart!");

function blankSession() {
  return { elapsed_s: 0, minutes: 0, xp: 0, target_min: 0, target_reached: false, bonus_earned: false, bonus_available: false, emergency_available: false, length: 0, liftAt: null };
}
const bonusGoal = () => (session.target_min ? 2 * session.target_min : 60);
const stageIndex = () => { let s = 0; while (s < 5 && pet.xp >= STAGE_XP[s + 1]) s++; return s; };
const mood = () => (pet.hearts === 0 ? "Dead" : state === "Result" && result?.kind === "Broken" ? "Crying" : pet.happiness >= 70 ? "Happy" : pet.happiness >= 35 ? "Content" : "Sad");
const active = () => players.slice(0, settings.players);
const dayEndsIn = () => {
  const local = now() + settings.tz_offset_min * 60 - settings.day_reset_hour * 3600;
  return 86400 - (((local % 86400) + 86400) % 86400);
};

// ---- simulation, stepped once per sim second ------------------------------------
let simulated = now();
function step(t) {
  const day = dateOf(t);
  if (day > today && clockKnown) closeDay(day);
  if (state === "Idle" && t >= stateUntil) {
    state = "Arming"; stateUntil = t + 10;
    for (const p of active()) p.docked = true; // team game: everyone docks, which starts the countdown
    emit("ArmingStarted", null, 0, "Phones docked. Session starts in 10 s.");
  } else if (state === "Arming" && t >= stateUntil) {
    state = "Running";
    session = { ...blankSession(), target_min: pet.egg_minutes, bonus_available: !bonusToday, emergency_available: true, length: 35 + Math.floor(rnd() * 50) };
    session.liftAt = rnd() < 0.25 ? 8 + Math.floor(rnd() * 20) : null;
    for (const p of active()) p.in_session = p.docked;
    emit("SessionStarted", null, active().filter((p) => p.in_session).length, `Session started with ${active().filter((p) => p.in_session).map((p) => p.name).join(" and ")}.`);
  } else if (state === "Running") {
    session.elapsed_s++;
    const ins = active().filter((p) => p.in_session);
    const lifter = active()[1];
    if (session.liftAt !== null && session.elapsed_s === session.liftAt * 60) { lifter.lifting = true; lifter.docked = false; }
    if (session.liftAt !== null && session.elapsed_s === session.liftAt * 60 + 20) return endSession("Broken", lifter);
    // Extra life for keeping going to double the goal (free play: an hour).
    if (session.bonus_available && !session.bonus_earned && session.elapsed_s === bonusGoal() * 60 && pet.hearts < BONUS_CAP) {
      session.bonus_earned = true; bonusToday = true; life.bonus_lives++;
      pet.hearts++;
      emit("BonusLife", null, pet.hearts, `Challenge done: ${bonusGoal()} min! Extra life: ${pet.hearts} hearts.`);
    }
    if (session.elapsed_s % 60 === 0) {
      session.minutes++;
      const xp = ins.length + (ins.length >= 4 ? 2 : ins.length >= 3 ? 1 : 0);
      session.xp += xp;
      const before = stageIndex();
      pet.xp += xp;
      if (stageIndex() !== before) grew(before);
      for (const p of ins) {
        if (p.lifting) continue;
        p.today_min++;
        if (!p.met_today && p.today_min >= settings.daily_minimum_min) {
          p.met_today = true;
          emit("TargetReached", p.slot, p.today_min, `${p.name} hit the daily ${settings.daily_minimum_min} min.`);
        }
      }
      if (session.target_min && !session.target_reached && session.minutes >= session.target_min) {
        session.target_reached = true;
        emit("TargetReached", null, session.minutes, session.bonus_available
          ? `Goal of ${session.target_min} min complete! Keep going to ${bonusGoal()} min for an extra life.`
          : `Goal of ${session.target_min} min complete! Finish whenever you like.`);
      }
      if (session.minutes >= session.length) endSession("Success", null);
    }
  } else if (state === "Result" && t >= stateUntil) {
    state = "Idle"; result = null; stateUntil = t + 120 + Math.floor(rnd() * 180);
    session = blankSession();
    for (const p of active()) Object.assign(p, { docked: false, in_session: false, lifting: false });
  }
}

function endSession(kind, culprit) {
  const m = session.minutes;
  state = "Result"; stateUntil = simulated + 60;
  life.total_min += m; life.longest_min = Math.max(life.longest_min, m);
  if (kind === "Success") {
    life.wins++; life.streak++; life.best_streak = Math.max(life.best_streak, life.streak);
    pet.happiness = Math.min(100, pet.happiness + 10);
    if (pet.hearts < MAX_HEARTS) pet.hearts++;
    shame = null;
    result = { kind, culprit: null, cause: null, text: `${m} min phone-free! +${session.xp} XP.` };
  } else {
    life.broken++; life.streak = 0; culprit.lifted++;
    pet.happiness = Math.max(0, pet.happiness - 35);
    pet.hearts = Math.max(0, pet.hearts - 1);
    result = { kind, culprit: culprit.slot, cause: "Lift", text: `${culprit.name} picked up their phone after ${m} min! Everyone loses.` };
    shame = { slot: culprit.slot, name: culprit.name, reason: "Lift", at: clockKnown ? simulated : null, text: `${culprit.name} picked up their phone after ${m} min.` };
  }
  emit("SessionEnded", culprit ? culprit.slot : null, m, result.text);
}

function closeDay(day) {
  const minutes = [0, 1, 2, 3].map((s) => (s < settings.players ? players[s].today_min : 0));
  past.unshift({ date: today, minutes, met: minutes.map((m, s) => s < settings.players && m >= settings.daily_minimum_min), today: false });
  let all = true;
  for (const p of active()) {
    if (p.today_min >= settings.daily_minimum_min) { p.daily_streak++; p.best_daily_streak = Math.max(p.best_daily_streak, p.daily_streak); }
    else {
      all = false; p.daily_streak = 0; p.missed_days++;
      pet.hearts = Math.max(0, pet.hearts - 1);
      shame = { slot: p.slot, name: p.name, reason: "MissedDaily", at: simulated, text: `${p.name} missed the daily ${settings.daily_minimum_min} min.` };
      emit("DailyMissed", p.slot, p.today_min, `${p.name} only managed ${p.today_min} of ${settings.daily_minimum_min} min yesterday. The pet lost a heart!`);
    }
    p.today_min = 0; p.met_today = false;
  }
  if (all) life.perfect_days++;
  bonusToday = false; today = day;
  emit("DayClosed", null, all ? 1 : 0, all ? "Perfect day: everyone made it!" : "New day. Go dock those phones.");
}

setInterval(() => {
  const t = now();
  while (simulated < t) step(++simulated);
}, 200);

// ---- HTTP ---------------------------------------------------------------------
function snapshot() {
  const running = state === "Running";
  const s = stageIndex();
  return {
    api: 1, now: now(), clock: clockKnown, night: false, night_hours: [22, 7], day_ends_in_s: clockKnown ? dayEndsIn() : null, settings,
    pet: {
      stage: STAGES[s], stage_index: s,
      egg_minutes: pet.egg_minutes, egg_locked: eggLocked(), rare_chance: rareChance(pet.egg_minutes),
      species: s ? SPECIES[pet.species][0] : null, species_index: s ? pet.species : null, rarity: s ? SPECIES[pet.species][1] : null,
      generation: pet.generation, hearts: pet.hearts, max_hearts: MAX_HEARTS, bonus_cap: BONUS_CAP,
      happiness: pet.happiness, mood: mood(), xp: pet.xp, stage_xp: STAGE_XP[s], next_stage_xp: s < 5 ? STAGE_XP[s + 1] : 0,
    },
    session: {
      state, elapsed_s: running ? session.elapsed_s : 0, minutes: running ? session.minutes : 0, xp: running ? session.xp : 0,
      target_min: running ? session.target_min : 0, target_reached: running && session.target_reached,
      bonus_goal_min: bonusGoal(), bonus_earned: running && session.bonus_earned, bonus_available: running && session.bonus_available,
      emergency_available: running && session.emergency_available,
    },
    result: state === "Result" ? result : null,
    players: active().map((p) => ({ ...p })),
    shame, life,
  };
}

function history(days) {
  days = Math.max(1, Math.min(60, days || 30));
  const minutes = [0, 1, 2, 3].map((s) => (s < settings.players ? players[s].today_min : 0));
  const cur = { date: today, minutes, met: minutes.map((m, s) => s < settings.players && m >= settings.daily_minimum_min), today: true };
  const cutoff = Date.parse(today) - (days - 1) * 86400000;
  return { minimum: settings.daily_minimum_min, players: settings.players, days: [cur, ...past].filter((d) => Date.parse(d.date) >= cutoff) };
}

function send(res, status, body) {
  const data = JSON.stringify(body);
  res.writeHead(status, {
    "Content-Type": "application/json; charset=utf-8", "Access-Control-Allow-Origin": "*",
    "Access-Control-Allow-Headers": "Content-Type", "Access-Control-Allow-Methods": "GET, POST, OPTIONS", "Cache-Control": "no-store",
  });
  res.end(data);
}

const readBody = (req) => new Promise((ok) => {
  let b = "";
  req.on("data", (c) => (b += c));
  req.on("end", () => { try { ok(JSON.parse(b || "{}")); } catch { ok(null); } });
});

http.createServer(async (req, res) => {
  const url = new URL(req.url, "http://x");
  const path = url.pathname;
  console.log(req.method, req.url);
  if (req.method === "OPTIONS") return send(res, 204, "");
  if (req.method === "GET" && path === "/api/state") return send(res, 200, snapshot());
  if (req.method === "GET" && path === "/api/collection")
    return send(res, 200, {
      species: SPECIES.map(([name, rarity], index) => ({
        index, name, rarity, count: collection[index][0], best_stage: collection[index][0] ? STAGES[collection[index][1]] : null,
      })),
    });
  if (req.method === "GET" && path === "/api/history") return send(res, 200, history(Number(url.searchParams.get("days"))));
  if (req.method === "GET" && path === "/api/events") {
    const since = Number(url.searchParams.get("since") || 0);
    return send(res, 200, { next: nextId - 1, events: events.filter((e) => e.id > since) });
  }
  if (req.method !== "POST") return send(res, 404, { error: "Not found" });
  const body = await readBody(req);
  if (!body) return send(res, 400, { error: "Bad JSON" });
  if (path === "/api/egg" || path === "/api/pet/new") {
    if (body.pin !== PIN) return send(res, 403, { error: "Wrong PIN" });
    if (state !== "Idle") return send(res, 409, { error: "Only between sessions" });
    if (path === "/api/pet/new") {
      Object.assign(pet, { xp: 0, hearts: 3, happiness: 60, generation: pet.generation + 1, species: null });
      emit("Hatched", null, pet.generation, `A new egg appeared. Generation ${pet.generation}.`);
      stateUntil = now() + 600; // give the player time to pick an egg
      return send(res, 200, { ok: true, generation: pet.generation });
    }
    if (eggLocked()) return send(res, 409, { error: "This egg's time is set until the pet is gone" });
    if (!EGGS[body.minutes]) return send(res, 400, { error: "minutes must be 15, 30, 60 or 90" });
    pet.egg_minutes = body.minutes;
    return send(res, 200, { ok: true, egg_minutes: pet.egg_minutes, rare_chance: rareChance(pet.egg_minutes) });
  }
  if (path === "/api/time") {
    if (clockKnown) return send(res, 409, { error: "The box already knows the time" });
    if (!Number.isInteger(body.unix) || body.unix < 1600000000) return send(res, 400, { error: "Bad time" });
    simBase = body.unix; simAt = Date.now(); simulated = body.unix; clockKnown = true;
    today = dateOf(body.unix);
    return send(res, 200, { ok: true, clock: true });
  }
  const m = path.match(/^\/api\/players\/(\d+)\/name$/);
  if (m || path === "/api/settings") {
    if (body.pin !== PIN) return send(res, 403, { error: "Wrong PIN" });
    if (m) {
      const slot = Number(m[1]);
      const name = String(body.name ?? "").replace(/["\\]/g, "'").trim();
      const len = Buffer.byteLength(name);
      if (slot >= settings.players || len < 1 || len > 12) return send(res, 400, { error: "Name must be 1-12 bytes" });
      players[slot].name = name;
      return send(res, 200, { ok: true, name });
    }
    if (state !== "Idle" && state !== "Setup") return send(res, 409, { error: "Settings can only change between sessions" });
    if (body.tz_offset_min !== undefined) settings.tz_offset_min = Number(body.tz_offset_min);
    if (body.daily_minimum_min !== undefined) settings.daily_minimum_min = Number(body.daily_minimum_min);
    if (body.day_reset_hour !== undefined) settings.day_reset_hour = Number(body.day_reset_hour);
    return send(res, 200, settings);
  }
  send(res, 404, { error: "Not found" });
}).listen(PORT, () => console.log(`mock pet box on http://localhost:${PORT} (PIN ${PIN}, ${SPEED}x speed)`));
