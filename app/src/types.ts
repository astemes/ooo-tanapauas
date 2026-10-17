// Shapes from docs/api.md (API v1).

export type StageName = "Egg" | "Baby" | "Child" | "Teen" | "Adult" | "Elder";
export type MoodName = "Happy" | "Content" | "Sad" | "Crying" | "Dead" | "Sleeping";
export type ShameReason = "Lift" | "MissedDaily" | "PowerCut";
export type Rarity = "Common" | "Uncommon" | "Rare" | "Legendary";

export interface Settings {
  players: number;
  daily_minimum_min: number;
  day_reset_hour: number;
  tz_offset_min: number;
}

export interface Player {
  slot: number;
  name: string;
  docked: boolean;
  in_session: boolean;
  lifting: boolean;
  today_min: number;
  met_today: boolean;
  daily_streak: number;
  best_daily_streak: number;
  lifted: number;
  missed_days: number;
}

export interface State {
  api: number;
  now: number;
  clock: boolean;
  // Night hours (local): the pet sleeps and no session runs.
  night: boolean;
  night_hours: [number, number];
  day_ends_in_s: number | null;
  settings: Settings;
  pet: {
    stage: StageName;
    stage_index: number;
    // null while the pet is still an egg.
    species: string | null;
    species_index: number | null;
    rarity: Rarity | null;
    // The egg's time setting: every session's goal for this pet; longer = rarer hatch.
    egg_minutes: number;
    egg_locked: boolean; // set until the pet is gone (its first session started)
    rare_chance: number;
    generation: number;
    hearts: number;
    max_hearts: number;
    bonus_cap: number;
    happiness: number;
    mood: MoodName;
    xp: number;
    stage_xp: number;
    next_stage_xp: number;
  };
  session: {
    state: "Setup" | "Idle" | "Arming" | "Running" | "Result";
    elapsed_s: number;
    minutes: number;
    xp: number;
    target_min: number;
    target_reached: boolean;
    // Extra-life challenge: reach bonus_goal_min (double the target, or 60 in free play).
    bonus_goal_min: number;
    bonus_earned: boolean;
    bonus_available: boolean;
    emergency_available: boolean;
  };
  result: null | {
    kind: "Success" | "Aborted" | "Broken" | "Died";
    culprit: number | null;
    cause: ShameReason | null;
    text: string;
  };
  players: Player[];
  shame: null | { slot: number | null; name: string | null; reason: ShameReason; at: number | null; text: string };
  life: {
    total_min: number;
    longest_min: number;
    wins: number;
    broken: number;
    streak: number;
    best_streak: number;
    perfect_days: number;
    bonus_lives: number;
    graveyard: number;
  };
}

// GET /api/collection: every species and how many this box has hatched.
export interface CollectionEntry {
  index: number;
  name: string;
  rarity: Rarity;
  count: number;
  best_stage: StageName | null;
}
export interface Collection {
  species: CollectionEntry[];
}

export interface HistoryDay {
  date: string;
  minutes: number[];
  met: boolean[];
  today: boolean;
}

export interface History {
  minimum: number;
  players: number;
  days: HistoryDay[];
}

export interface BoxEvent {
  id: number;
  at: number | null;
  type: string;
  slot: number | null;
  // A number, except SessionEnded where the firmware sends the result kind ("Success", ...).
  value: number | string;
  text: string;
}
