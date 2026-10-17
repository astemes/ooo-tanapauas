#!/usr/bin/env python3
"""Generate the Tanapaus logo from the real pet sprite in lib/petcore.

Outputs (docs/brand/):
  tanapaus-logo.svg       colour, for print and screens
  tanapaus-logo.png       colour, transparent background, 2400 px wide
  tanapaus-logo-mono.svg  one colour, merged outlines (holes for eyes/mouth)
  tanapaus-logo-mono.dxf  same outlines as closed polylines, in millimetres

Everything is built from square cells, so CAD gets clean straight-edged
profiles and no fonts are needed. Usage: python3 tools/make_logo.py [width_mm]
"""
import pathlib
import re
import struct
import sys
import zlib

ROOT = pathlib.Path(__file__).resolve().parents[1]
OUT = ROOT / "docs" / "brand"
WIDTH_MM = float(sys.argv[1]) if len(sys.argv) > 1 else 100.0

# ---- sprite data, read straight from the firmware source ----------------------
src = (ROOT / "lib/petcore/src/sprites.cpp").read_text()


def art(name):
    m = re.search(r"(?:Art|Face) " + name + r" = \{(.*?)\};", src, re.S)
    return re.findall(r'"([^"]*)"', m.group(1))


BODY = art("kChild")
FACE = art("kFaceHappy")
FACE_AT = (4, 5)  # kFaceAt[Child]
BODY_HEX = "7ED957"  # kBodyColour[Child]


def darken(h):
    n = int(h, 16)
    r, g, b = (n >> 16) & 255, (n >> 8) & 255, n & 255
    return f"{r * 7 // 10:02X}{g * 7 // 10:02X}{b * 7 // 10:02X}"


PALETTE = {"#": "2B2233", "o": BODY_HEX, "s": darken(BODY_HEX), "w": "F7F4EE", "p": "FF8FA3", "a": "FFE066"}
INK = "2B2233"

# ---- pixel wordmark (5x7) -----------------------------------------------------
GLYPHS = {
    "T": ["#####", "..#..", "..#..", "..#..", "..#..", "..#..", "..#.."],
    "A": [".###.", "#...#", "#...#", "#####", "#...#", "#...#", "#...#"],
    "N": ["#...#", "##..#", "#.#.#", "#..##", "#...#", "#...#", "#...#"],
    "P": ["####.", "#...#", "#...#", "####.", "#....", "#....", "#...."],
    "U": ["#...#", "#...#", "#...#", "#...#", "#...#", "#...#", ".###."],
    "S": [".####", "#....", "#....", ".###.", "....#", "....#", "####."],
}
WORD = "TANAPAUS"

# ---- layout in "units" (one pet pixel = 1 unit) ---------------------------------
LETTER_CELL = 1.25
LETTER_GAP = 1  # cells between letters
PET_ROWS = range(1, 15)  # rows of the Child sprite that have pixels
pet_h = len(PET_ROWS)
text_h = 7 * LETTER_CELL
text_y = round((pet_h - text_h) / 2 * 4) / 4  # centred on the pet, snapped to the outline grid
text_x = 16 + 2.5  # pet is 16 wide, then a gap

# Colour cells: (x, y, size, hex)
cells = []
pet = [list(r) for r in BODY]
for y, row in enumerate(FACE):
    for x, c in enumerate(row):
        if c != ".":
            pet[FACE_AT[1] + y][FACE_AT[0] + x] = c
for y in PET_ROWS:
    for x, c in enumerate(pet[y]):
        if c in PALETTE:
            cells.append((x, y - PET_ROWS.start, 1, PALETTE[c]))

letter_cells = []  # in letter-cell grid coordinates
cx = 0
for ch in WORD:
    for y, row in enumerate(GLYPHS[ch]):
        for x, c in enumerate(row):
            if c == "#":
                letter_cells.append((cx + x, y))
    cx += 5 + LETTER_GAP
text_w = (cx - LETTER_GAP) * LETTER_CELL
for x, y in letter_cells:
    cells.append((text_x + x * LETTER_CELL, text_y + y * LETTER_CELL, LETTER_CELL, INK))

total_w = text_x + text_w
total_h = pet_h
MM_PER_UNIT = WIDTH_MM / total_w

# ---- mono outlines: union of filled cells, traced into closed loops --------------
# Pet: everything opaque except the face features (eyes, mouth, cheeks), which
# become holes. Work on a fine integer grid so pet and letter cells line up.
FINE = 4  # fine cells per unit; letters are 1.25 units = 5 fine cells
filled = set()


def fill(x0, y0, size):
    fx, fy, n = round(x0 * FINE), round(y0 * FINE), round(size * FINE)
    for i in range(fx, fx + n):
        for j in range(fy, fy + n):
            filled.add((i, j))


# Cheeks stay filled in one colour; only the eyes and mouth are cut out.
face_px = {(FACE_AT[0] + x, FACE_AT[1] + y) for y, row in enumerate(FACE) for x, c in enumerate(row) if c == "#"}
for y in PET_ROWS:
    for x, c in enumerate(BODY[y]):
        if c != "." and (x, y) not in face_px:
            fill(x, y - PET_ROWS.start, 1)
for x, y in letter_cells:
    fill(text_x + x * LETTER_CELL, text_y + y * LETTER_CELL, LETTER_CELL)


def trace(cells_set):
    """Boundary loops of a set of unit squares, clockwise outer / ccw holes."""
    edges = {}
    for (x, y) in cells_set:
        for a, b in (((x, y), (x + 1, y)), ((x + 1, y), (x + 1, y + 1)),
                     ((x + 1, y + 1), (x, y + 1)), ((x, y + 1), (x, y))):
            if (b, a) in edges:
                del edges[(b, a)]
            else:
                edges[(a, b)] = True
    out = {}
    for a, b in edges:
        out.setdefault(a, []).append(b)
    loops = []
    while out:
        start = next(iter(out))
        loop, cur, prev_dir = [start], start, None
        while True:
            nexts = out[cur]
            if len(nexts) > 1 and prev_dir:
                # At a pinch point, turn the same way every time so loops never cross.
                def turn(n):
                    d = (n[0] - cur[0], n[1] - cur[1])
                    return prev_dir[0] * d[1] - prev_dir[1] * d[0]
                nexts.sort(key=turn, reverse=True)
            nxt = nexts.pop(0)
            if not nexts:
                del out[cur]
            prev_dir = (nxt[0] - cur[0], nxt[1] - cur[1])
            cur = nxt
            if cur == start:
                break
            loop.append(cur)
        # drop collinear points
        simple = []
        for i, p in enumerate(loop):
            a, c = loop[i - 1], loop[(i + 1) % len(loop)]
            if (p[0] - a[0]) * (c[1] - p[1]) != (p[1] - a[1]) * (c[0] - p[0]):
                simple.append(p)
        loops.append(simple)
    return loops


loops = trace(filled)


def fmt(v):
    return f"{v:.3f}".rstrip("0").rstrip(".")


# ---- writers ---------------------------------------------------------------------
OUT.mkdir(parents=True, exist_ok=True)
w_mm, h_mm = total_w * MM_PER_UNIT, total_h * MM_PER_UNIT
svg_head = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{fmt(w_mm)}mm" height="{fmt(h_mm)}mm" '
            f'viewBox="0 0 {fmt(total_w)} {fmt(total_h)}" shape-rendering="crispEdges">\n'
            f'  <title>Tanapaus</title>\n')

# Colour SVG: one rect per cell, merged by colour into paths for a small file.
by_colour = {}
for x, y, s, col in cells:
    by_colour.setdefault(col, []).append(f"M{fmt(x)} {fmt(y)}h{fmt(s)}v{fmt(s)}h-{fmt(s)}z")
colour_svg = svg_head + "".join(f'  <path fill="#{c}" d="{"".join(d)}"/>\n' for c, d in by_colour.items()) + "</svg>\n"
(OUT / "tanapaus-logo.svg").write_text(colour_svg)

mono_d = "".join("M" + "L".join(f"{fmt(px / FINE)} {fmt(py / FINE)}" for px, py in lp) + "Z" for lp in loops)
(OUT / "tanapaus-logo-mono.svg").write_text(
    svg_head + f'  <path fill="#{INK}" fill-rule="evenodd" d="{mono_d}"/>\n</svg>\n')

# DXF (R12-compatible POLYLINE entities, mm, y up so it isn't mirrored in CAD).
dxf = ["0", "SECTION", "2", "HEADER", "9", "$INSUNITS", "70", "4", "0", "ENDSEC", "0", "SECTION", "2", "ENTITIES"]
for lp in loops:
    dxf += ["0", "POLYLINE", "8", "LOGO", "66", "1", "70", "1"]
    for px, py in lp:
        dxf += ["0", "VERTEX", "8", "LOGO", "10", fmt(px / FINE * MM_PER_UNIT), "20",
                fmt((total_h - py / FINE) * MM_PER_UNIT)]
    dxf += ["0", "SEQEND", "8", "LOGO"]
dxf += ["0", "ENDSEC", "0", "EOF"]
(OUT / "tanapaus-logo-mono.dxf").write_text("\n".join(dxf) + "\n")

# PNG (colour, transparent), drawn on a pixel grid without any image library.
PX_PER_UNIT = 48
pw, ph = round(total_w * PX_PER_UNIT), round(total_h * PX_PER_UNIT)
img = bytearray(pw * ph * 4)
for x, y, s, col in cells:
    r, g, b = (int(col[i:i + 2], 16) for i in (0, 2, 4))
    x0, y0 = round(x * PX_PER_UNIT), round(y * PX_PER_UNIT)
    x1, y1 = round((x + s) * PX_PER_UNIT), round((y + s) * PX_PER_UNIT)
    for yy in range(y0, y1):
        row = yy * pw * 4
        img[row + x0 * 4: row + x1 * 4] = bytes((r, g, b, 255)) * (x1 - x0)
raw = b"".join(b"\x00" + bytes(img[yy * pw * 4:(yy + 1) * pw * 4]) for yy in range(ph))


def chunk(tag, data):
    return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)


png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", pw, ph, 8, 6, 0, 0, 0)) + \
      chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b"")
(OUT / "tanapaus-logo.png").write_bytes(png)

print(f"{w_mm:.1f} x {h_mm:.1f} mm, {len(loops)} outline loops, PNG {pw}x{ph}")
