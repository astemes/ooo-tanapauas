// Reports what the firmware will embed (it gzips index.html and the manifest).
import { readFileSync, statSync } from "node:fs";
import { gzipSync } from "node:zlib";

const LIMIT = 60 * 1024;
const dist = new URL("../dist/", import.meta.url);
const html = readFileSync(new URL("index.html", dist));
const gz = gzipSync(html, { level: 9 }).length;
console.log(`dist/index.html: ${(html.length / 1024).toFixed(1)} KB, ${(gz / 1024).toFixed(1)} KB gzipped (budget 60 KB)`);
console.log(`dist/icon.png: ${statSync(new URL("icon.png", dist)).size} bytes`);
if (/<script[^>]+src=|<link[^>]+stylesheet/.test(html.toString())) {
  console.error("index.html still references external JS/CSS");
  process.exit(1);
}
if (gz > LIMIT) {
  console.error("over the 60 KB gzip budget");
  process.exit(1);
}
