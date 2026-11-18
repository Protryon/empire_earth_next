"""Compares the window captures of two lan_game.py runs (their evidence directories), capture by capture.

The WON lobby's moves are checked against a test-profile exe built at the round's start: run lan_game.py with
`--exe` that exe and `--out A`, then with the round's exe and `--out B`, and

    python -I tools/lan_capture_compare.py A B            # every host-*.bmp and joiner-*.bmp of both
    python -I tools/lan_capture_compare.py A B --crop OUT  # also a PNG of each differing region (A's, then B's)

For each capture: identical, or the differing pixel count and their bounding box. The game's own frames
(`game`, `play`) differ by the simulation's timing between runs and are reported apart. Expected differences: the
hosted game's name carries the harness slot's digit (lantest0 against lantest1) and the staging screen's caret
blinks. Exit 1 when a lobby capture differs (read the boxes before calling it a regression).
"""
import argparse
import pathlib
import struct
import sys
import zlib


def load(p):
    b = p.read_bytes()
    off = struct.unpack_from("<I", b, 10)[0]
    w, h = struct.unpack_from("<ii", b, 18)
    bpp = struct.unpack_from("<H", b, 28)[0]
    return w, h, bpp // 8, b[off:]


def diff(a, b):
    w, h, bp, pa = a
    pb = b[3]
    stride = (w * bp + 3) & ~3
    n, xs, ys = 0, [], []
    for y in range(abs(h)):
        ra, rb = pa[y * stride:y * stride + w * bp], pb[y * stride:y * stride + w * bp]
        if ra == rb:
            continue
        for x in range(w):
            if ra[x * bp:x * bp + bp] != rb[x * bp:x * bp + bp]:
                n += 1
                xs.append(x)
                ys.append(h - 1 - y if h > 0 else y)  # top-down rows
    return n, (min(xs), min(ys), max(xs) + 1, max(ys) + 1) if n else None


def crop_png(img, box, dst, scale=2):
    w, h, bp, px = img
    stride = (w * bp + 3) & ~3
    x0, y0, x1, y1 = box
    rows = []
    for y in range(y0, y1):
        sy = (h - 1 - y) if h > 0 else y
        r = bytearray()
        for x in range(x0, x1):
            o = sy * stride + x * bp
            r += bytes((px[o + 2], px[o + 1], px[o])) * scale
        for _ in range(scale):
            rows.append(b"\0" + bytes(r))
    W, H = (x1 - x0) * scale, (y1 - y0) * scale

    def chunk(t, d):
        return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xffffffff)

    dst.write_bytes(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", W, H, 8, 2, 0, 0, 0))
                    + chunk(b"IDAT", zlib.compress(b"".join(rows))) + chunk(b"IEND", b""))


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("a", type=pathlib.Path)
    ap.add_argument("b", type=pathlib.Path)
    ap.add_argument("--crop", type=pathlib.Path, help="write each differing region as PNGs here")
    a = ap.parse_args()
    bad = 0
    for side in ("host", "joiner"):
        for p in sorted((a.a / side).glob(f"{side}-*.bmp")):
            q = a.b / side / p.name
            game = p.stem.split("-", 1)[1] in ("game", "play")
            if not q.exists():
                print(f"{p.name}: missing in {a.b}")
                bad += not game
                continue
            x, y = load(p), load(q)
            if x[:3] != y[:3]:
                print(f"{p.name}: size {x[:3]} against {y[:3]}")
                bad += not game
                continue
            n, box = diff(x, y)
            tag = " (the game's frame)" if game else ""
            print(f"{p.name}: " + ("identical" if n == 0 else f"{n} pixels differ in x {box[0]}..{box[2]} "
                                    f"y {box[1]}..{box[3]}") + tag)
            if n and not game:
                bad += 1
            if n and a.crop:
                a.crop.mkdir(parents=True, exist_ok=True)
                pad = 8
                bx = (max(0, box[0] - pad), max(0, box[1] - pad), min(x[0], box[2] + pad), min(abs(x[1]), box[3] + pad))
                crop_png(x, bx, a.crop / f"{p.stem}-a.png")
                crop_png(y, bx, a.crop / f"{p.stem}-b.png")
    print("lan_capture_compare: " + ("every lobby capture identical" if bad == 0 else f"{bad} lobby captures differ"))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
