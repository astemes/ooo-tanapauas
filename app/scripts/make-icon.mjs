// Draws public/icon.png (192x192) from the real pet sprite via src/pet.ts. No dependencies:
// esbuild (bundled with Vite) compiles the renderer, zlib writes the PNG.
import { build } from "esbuild";
import { writeFileSync } from "node:fs";
import { deflateSync } from "node:zlib";

const { outputFiles } = await build({
  entryPoints: [new URL("../src/pet.ts", import.meta.url).pathname],
  bundle: true, write: false, format: "esm", platform: "node",
});
const { petPixels } = await import("data:text/javascript;base64," + Buffer.from(outputFiles[0].text).toString("base64"));

const SIZE = 192, SCALE = 10, OFF = (SIZE - 16 * SCALE) / 2;
const BG = [0x1e, 0x1b, 0x24];
const px = petPixels(2, 0, 0); // Child, Happy
const raw = Buffer.alloc((SIZE * 3 + 1) * SIZE);
for (let y = 0; y < SIZE; y++) {
  raw[y * (SIZE * 3 + 1)] = 0; // filter: none
  for (let x = 0; x < SIZE; x++) {
    const sx = Math.floor((x - OFF) / SCALE), sy = Math.floor((y - OFF) / SCALE);
    const c = sx >= 0 && sy >= 0 && sx < 16 && sy < 16 ? px[sy][sx] : null;
    const rgb = c ? [0, 2, 4].map((i) => parseInt(c.slice(i, i + 2), 16)) : BG;
    rgb.forEach((v, i) => (raw[y * (SIZE * 3 + 1) + 1 + x * 3 + i] = v));
  }
}

const crcTable = Array.from({ length: 256 }, (_, n) => {
  for (let k = 0; k < 8; k++) n = n & 1 ? 0xedb88320 ^ (n >>> 1) : n >>> 1;
  return n >>> 0;
});
const crc = (buf) => {
  let c = 0xffffffff;
  for (const b of buf) c = crcTable[(c ^ b) & 255] ^ (c >>> 8);
  return (c ^ 0xffffffff) >>> 0;
};
const chunk = (type, data) => {
  const len = Buffer.alloc(4), sum = Buffer.alloc(4), td = Buffer.concat([Buffer.from(type), data]);
  len.writeUInt32BE(data.length);
  sum.writeUInt32BE(crc(td));
  return Buffer.concat([len, td, sum]);
};
const ihdr = Buffer.alloc(13);
ihdr.writeUInt32BE(SIZE, 0);
ihdr.writeUInt32BE(SIZE, 4);
ihdr[8] = 8; // bit depth
ihdr[9] = 2; // truecolour RGB
const png = Buffer.concat([
  Buffer.from([0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a]),
  chunk("IHDR", ihdr),
  chunk("IDAT", deflateSync(raw, { level: 9 })),
  chunk("IEND", Buffer.alloc(0)),
]);
writeFileSync(new URL("../public/icon.png", import.meta.url), png);
console.log(`wrote public/icon.png (${png.length} bytes)`);
