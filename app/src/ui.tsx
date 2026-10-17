import { useEffect, useRef, useState } from "preact/hooks";
import type { ComponentChildren } from "preact";
import { heartPixels, petPixels, type HeartKind } from "./pet";

type Pixels = (string | null)[][];

export function PixelCanvas({ pixels, scale, class: cls }: { pixels: Pixels; scale: number; class?: string }) {
  const ref = useRef<HTMLCanvasElement>(null);
  const h = pixels.length, w = pixels[0].length;
  useEffect(() => {
    const ctx = ref.current!.getContext("2d")!;
    ctx.clearRect(0, 0, w, h);
    for (let y = 0; y < h; y++)
      for (let x = 0; x < w; x++) {
        const c = pixels[y][x];
        if (c) {
          ctx.fillStyle = "#" + c;
          ctx.fillRect(x, y, 1, 1);
        }
      }
  });
  return <canvas ref={ref} width={w} height={h} class={"px " + (cls || "")} style={{ width: w * scale, height: h * scale }} />;
}

// The pet animates at ~2 frames/s like the box.
export function Pet({
  species = null,
  stage,
  mood,
  scale,
  still,
}: {
  species?: number | null;
  stage: number;
  mood: number;
  scale: number;
  still?: boolean; // grids of many pets: skip the animation
}) {
  const [frame, setFrame] = useState(0);
  useEffect(() => {
    if (still) return;
    const t = setInterval(() => setFrame((f) => f + 1), 500);
    return () => clearInterval(t);
  }, [still]);
  return <PixelCanvas pixels={petPixels(species, stage, mood, frame)} scale={scale} />;
}

const heartCache: Partial<Record<HeartKind, Pixels>> = {};
export function Heart({ kind, scale = 3 }: { kind: HeartKind; scale?: number }) {
  return <PixelCanvas pixels={(heartCache[kind] ||= heartPixels(kind))} scale={scale} class={kind} />;
}

// Regular hearts up to max_hearts, then banked bonus lives in gold.
export function Hearts({ hearts, max, scale = 3 }: { hearts: number; max: number; scale?: number }) {
  const list: HeartKind[] = [];
  for (let i = 0; i < max; i++) list.push(i < hearts ? "full" : "empty");
  for (let i = max; i < hearts; i++) list.push("bonus");
  return (
    <div class="hearts" aria-label={`${hearts} hearts`}>
      {list.map((k, i) => <Heart key={i} kind={k} scale={scale} />)}
    </div>
  );
}

export function Bar({ value, max, tone = "teal", mark }: { value: number; max: number; tone?: string; mark?: number }) {
  const pct = max > 0 ? Math.min(100, (value / max) * 100) : 0;
  return (
    <div class={"bar " + tone}>
      <i style={{ width: pct + "%" }} />
      {mark !== undefined && max > 0 && <b style={{ left: Math.min(100, (mark / max) * 100) + "%" }} />}
    </div>
  );
}

export function Card({ children, class: cls }: { children: ComponentChildren; class?: string }) {
  return <section class={"card " + (cls || "")}>{children}</section>;
}

export function useTick(ms = 1000) {
  const [, set] = useState(0);
  useEffect(() => {
    const t = setInterval(() => set((n) => n + 1), ms);
    return () => clearInterval(t);
  }, [ms]);
}

export const pad = (n: number) => String(n).padStart(2, "0");
export function clock(s: number) {
  s = Math.max(0, Math.floor(s));
  const h = Math.floor(s / 3600), m = Math.floor(s / 60) % 60;
  return h ? `${h}:${pad(m)}:${pad(s % 60)}` : `${m}:${pad(s % 60)}`;
}
export function hm(s: number) {
  s = Math.max(0, Math.floor(s));
  const h = Math.floor(s / 3600), m = Math.floor(s / 60) % 60;
  return h ? `${h} h ${m} min` : `${m} min`;
}
export function ago(at: number | null, now: number) {
  if (!at) return "";
  const d = Math.max(0, now - at);
  if (d < 90) return "just now";
  if (d < 3600) return `${Math.round(d / 60)} min ago`;
  if (d < 86400 * 2) return `${Math.round(d / 3600)} h ago`;
  return `${Math.round(d / 86400)} days ago`;
}
export const tzLabel = (min: number) =>
  `UTC${min < 0 ? "−" : "+"}${Math.floor(Math.abs(min) / 60)}${Math.abs(min) % 60 ? ":" + pad(Math.abs(min) % 60) : ""}`;
