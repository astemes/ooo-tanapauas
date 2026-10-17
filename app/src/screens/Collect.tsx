import { useState } from "preact/hooks";
import type { Ctx } from "../app";
import { STAGES, SPECIES, hatchOdds, petPixels, type Rarity } from "../pet";
import { everHeld, myCards } from "../network";
import { Pet, PixelCanvas } from "../ui";
import { Badges } from "./Badges";

export const RARITY_CLASS: Record<Rarity, string> = { Common: "r-common", Uncommon: "r-uncommon", Rare: "r-rare", Legendary: "r-legendary" };

export function RarityTag({ rarity }: { rarity: Rarity }) {
  return <span class={"rarity " + RARITY_CLASS[rarity]}>{rarity}</span>;
}

// Undiscovered species: the shape only, in shadow.
function Silhouette({ species, scale }: { species: number; scale: number }) {
  const px = petPixels(species, 3, 1, 0).map((r) => r.map((c) => (c ? "3A3544" : null)));
  return <PixelCanvas pixels={px} scale={scale} />;
}

export const boxCounts = (c: Ctx) => SPECIES.map((s) => c.collection?.species[s.index]?.count ?? 0);
const bestStage = (c: Ctx, s: number) => {
  const name = c.collection?.species[s]?.best_stage;
  return name ? STAGES.indexOf(name) : 3; // traded-in cards show as a Teen
};

export function Collect(c: Ctx) {
  const [seg, setSeg] = useState<"pets" | "badges">("pets");
  const [open, setOpen] = useState<number | null>(null);
  const counts = boxCounts(c);
  const cards = myCards(counts);
  const seen = everHeld(counts);
  const found = seen.filter(Boolean).length;

  return (
    <div class="screen">
      <h1>Collection</h1>
      <div class="seg">
        <button class={seg === "pets" ? "on" : ""} onClick={() => setSeg("pets")}>
          Pets {found}/{SPECIES.length}
        </button>
        <button class={seg === "badges" ? "on" : ""} onClick={() => setSeg("badges")}>
          Badges
        </button>
      </div>

      {seg === "badges" ? (
        <Badges {...c} />
      ) : (
        <>
          <p class="muted">
            Every egg hatches into a random animal. Raise them on the box, trade spares with friends, and fill the dex.
          </p>
          {!c.collection && <p class="muted small">Loading the box's collection…</p>}
          <div class="dex">
            {SPECIES.map((s) => (
              <button
                key={s.index}
                class={"dex-card " + RARITY_CLASS[s.rarity] + (seen[s.index] ? "" : " unknown")}
                onClick={() => setOpen(s.index)}
              >
                {seen[s.index] ? (
                  <Pet species={s.index} stage={bestStage(c, s.index)} mood={0} scale={4} still />
                ) : (
                  <Silhouette species={s.index} scale={4} />
                )}
                <b>{seen[s.index] ? s.name : "???"}</b>
                <span class="small">{cards[s.index] > 0 ? `×${cards[s.index]}` : seen[s.index] ? "traded away" : s.rarity}</span>
              </button>
            ))}
          </div>
        </>
      )}

      {open !== null && <SpeciesSheet c={c} species={open} cards={cards[open]} seen={seen[open]} onClose={() => setOpen(null)} />}
    </div>
  );
}

function SpeciesSheet({ c, species, cards, seen, onClose }: { c: Ctx; species: number; cards: number; seen: boolean; onClose: () => void }) {
  const s = SPECIES[species];
  const raised = c.collection?.species[species];
  const current = c.st.pet.species_index === species;
  return (
    <div class="sheet-bg" onClick={onClose}>
      <div class={"sheet " + RARITY_CLASS[s.rarity]} onClick={(e) => e.stopPropagation()}>
        <div class="sheet-pet">
          {seen ? <Pet species={species} stage={bestStage(c, species)} mood={0} scale={10} /> : <Silhouette species={species} scale={10} />}
        </div>
        <h2>
          {seen ? s.name : "Unknown"} <RarityTag rarity={s.rarity} />
        </h2>
        <dl class="facts">
          <dt>Hatch chance</dt>
          <dd>{hatchOdds(species).toFixed(hatchOdds(species) < 2 ? 1 : 0)} %</dd>
          <dt>Cards you hold</dt>
          <dd>{cards}</dd>
          <dt>Raised on your box</dt>
          <dd>{raised?.count ?? 0}</dd>
          {raised?.best_stage && (
            <>
              <dt>Best stage reached</dt>
              <dd>{raised.best_stage}</dd>
            </>
          )}
        </dl>
        <p class="muted small">
          {current
            ? "Your box's pet right now. Keep those phones docked."
            : seen
            ? cards > 1
              ? "You have spares: offer one to a friend in Friends."
              : "Trade for more in Friends, or hope the next egg brings one."
            : "Not found yet. Hatch it on the box or trade for it in Friends."}
        </p>
        <button class="btn ghost" onClick={onClose}>
          Close
        </button>
      </div>
    </div>
  );
}
