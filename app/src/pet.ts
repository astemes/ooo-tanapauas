// Port of petcore::render_pet / render_heart / palette_rgb (lib/petcore/src/sprites.cpp).
// Sprite data comes from generated/petcore.json (scripts/export-petcore.py).
import P from "./generated/petcore.json";

export const STAGES = P.stages;
export const MOODS = P.moods;
export const TUNING = P.tuning;

export type Rarity = "Common" | "Uncommon" | "Rare" | "Legendary";
export interface Species {
  index: number;
  name: string;
  rarity: Rarity;
}
export const SPECIES: Species[] = P.species.map((s, index) => ({ index, name: s.name, rarity: s.rarity as Rarity }));
export const RARITIES = P.rarities as Rarity[];
// Egg sizes (lib/petcore species.h kEggs): longer eggs hatch rarer pets.
export const EGGS = P.eggs;
const egg = (minutes: number) => EGGS.find((e) => e.minutes === minutes) ?? EGGS.find((e) => e.minutes === 30)!;
// Percent chance an egg hatches Rare or Legendary.
export const rareChance = (minutes: number) => egg(minutes).weight[2] + egg(minutes).weight[3];
// Hatch odds per species for an egg, in percent (tier weight split across the tier).
export const hatchOdds = (s: number, minutes = 30) => {
  const r = SPECIES[s].rarity;
  return egg(minutes).weight[RARITIES.indexOf(r)] / SPECIES.filter((x) => x.rarity === r).length;
};
const MOOD = { Happy: 0, Content: 1, Sad: 2, Crying: 3, Dead: 4, Sleeping: 5 } as const;

// 16 rows of palette characters: '.' transparent, '#' outline, 'o' body, 's' shade,
// 'w' white, 'p' cheek, 't' tear, 'a' accent.
// species: index into SPECIES, or null while still an egg.
export function renderPet(species: number | null, stage: number, mood: number, frame: number): string[][] {
  const out = Array.from({ length: 16 }, () => Array<string>(16).fill("."));
  const blit = (art: string[], w: number, h: number, dx: number, dy: number, skip: boolean) => {
    for (let y = 0; y < h; y++)
      for (let x = 0; x < w; x++) {
        const tx = x + dx, ty = y + dy;
        if (tx < 0 || ty < 0 || tx >= 16 || ty >= 16) continue;
        const c = art[y][x];
        if (skip && c === ".") continue;
        out[ty][tx] = c;
      }
  };
  if (mood === MOOD.Dead) {
    blit(P.tombstone, 16, 16, 0, 0, false);
    return out;
  }
  if (stage === 0) {
    // Wobble side to side; sad eggs sit still.
    const still = mood === MOOD.Sad || mood === MOOD.Sleeping;
    const dx = still ? 0 : frame % 4 === 1 ? -1 : frame % 4 === 3 ? 1 : 0;
    blit(P.bodies[0], 16, 16, dx, 0, false);
    return out;
  }
  // Happy pets bounce; sad and crying ones droop slowly.
  const bob = mood === MOOD.Happy || mood === MOOD.Content ? frame % 2 : frame % 4 === 0 ? 1 : 0;
  const feature = species !== null ? P.species[species]?.feature : null;
  if (feature) blit(feature, 16, feature.length, 0, P.featureY[stage] + bob, true);
  blit(P.bodies[stage], 16, 16, 0, bob, true);
  const rows = P.faces[mood === MOOD.Sleeping ? 4 : Math.min(mood, 3)].slice();
  if (mood !== MOOD.Crying && mood !== MOOD.Sleeping && frame % 8 === 7) rows[0] = P.closedEyes;
  const [ax, ay] = P.faceAt[stage];
  blit(rows, 8, 4, ax, ay + bob, true);
  return out;
}

function darken(hex: string): string {
  const v = parseInt(hex, 16);
  const ch = (s: number) => Math.floor((((v >> s) & 255) * 7) / 10);
  return ((ch(16) << 16) | (ch(8) << 8) | ch(0)).toString(16).padStart(6, "0");
}

export function colourFor(species: number | null, stage: number, c: string): string | null {
  const sp = P.species[species ?? 0] ?? P.species[0];
  const pal = P.palette, body = sp.body ?? P.bodyColour[stage % 6];
  switch (c) {
    case "#": return pal.outline;
    case "o": return body;
    case "s": return stage === 0 ? pal.eggShade : darken(body);
    case "w": return pal.white;
    case "p": return pal.cheek;
    case "t": return pal.tear;
    case "a": return stage === 0 ? pal.eggAccent : sp.accent;
    default: return null;
  }
}

export type HeartKind = "full" | "empty" | "bonus";
const HEART_FILL: Record<HeartKind, string> = { full: P.palette.heartFull, empty: P.palette.heartEmpty, bonus: "FFC94D" };

export function heartPixels(kind: HeartKind): (string | null)[][] {
  return P.heart.map((r) => [...r].map((c) => (c === "#" ? P.palette.outline : c === "x" ? HEART_FILL[kind] : null)));
}

export function petPixels(species: number | null, stage: number, mood: number, frame: number): (string | null)[][] {
  return renderPet(species, stage, mood, frame).map((r) => r.map((c) => colourFor(species, stage, c)));
}

export const moodIndex = (m: string) => Math.max(0, MOODS.indexOf(m));
