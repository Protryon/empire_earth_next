"""Summarise a game log (ee/EEGameLog.h: <data root>/Logs/game-*.log, written by every game unless config.ini [Game]
"Game Log" is 0), for the human and the computer players alike.

  python -I tools/game_log.py [LOG ...] [--every MIN] [--data ROOT] [--json OUT]

With no LOG, the newest log under --data's Logs (default: data/, then build/boot/windowed_f) is read. Per player: who
it is (human or computer, the AI's personality and cheats), the minute it reached every epoch, and a timeline every
--every minutes (default 2): epoch, citizens (idle, gatherers per resource f/w/s/g/i, builders), army, buildings and
towers, stock, income per minute and losses (citizens/army/buildings, and by a human's or a computer's units), and
every chat line with its game minute (notes typed while playing). The
computer players' decision trace (the "EE-AI" lines) is in the same file for tools/harness/ai_trace.py and grep.
"""
import argparse
import collections
import json
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent


# the first player colours (Data/db/dbcolortable.csv labels; read from the data when it is at hand)
COLORS = ["Blue", "Red", "Yellow", "Bright Purple", "Burnt Orange", "Lime Green", "Cyan", "Dark Red", "Lt. Brown",
          "Dark Purple", "Gray", "Light Gray"]


def color_names(roots):
    for r in roots:
        f = pathlib.Path(r) / "Data" / "db" / "dbcolortable.csv"
        if f.exists():
            names = {}
            for line in open(f, encoding="utf-8", errors="replace"):
                parts = line.rstrip("\n").split(",")
                if parts and parts[0].isdigit() and len(parts) > 2:
                    label = parts[2]
                    names[int(parts[0])] = label[label.find("(") + 1:label.rfind(")")] if "(" in label else label
            if names:
                return names
    return dict(enumerate(COLORS))


def kv(text):
    return dict(x.split("=", 1) for x in text.split() if "=" in x)


def five(text):
    return [int(v) for v in text.split("/")]


def read(path):
    players, samples, epochs = {}, collections.defaultdict(list), collections.defaultdict(dict)
    head, end, ai_lines, chats = {}, None, 0, []
    for line in open(path, encoding="utf-8", errors="replace"):
        if line.startswith("EE-AI "):
            ai_lines += 1
            continue
        m = re.match(r"EE-GAME (\d+) p(\d+) (\w+) ?(.*)", line)
        if not m:
            continue
        t, p, ev, rest = int(m[1]), int(m[2]), m[3], m[4]
        if ev == "chat":
            chats.append((t, p, rest[5:] if rest.startswith("text=") else rest))
        elif ev == "start":
            head = kv(rest)
        elif ev == "player":
            players[p] = kv(rest)
        elif ev == "sample":
            samples[p].append((t, kv(rest)))
        elif ev == "civ":
            players.setdefault(p, {})["civ"] = kv(rest).get("name", "-")
        elif ev == "epoch":
            epochs[p][int(kv(rest)["to"])] = t
        elif ev == "end":
            end = t
    return head, players, samples, epochs, end, ai_lines, chats


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0], epilog=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("log", nargs="*", type=pathlib.Path)
    ap.add_argument("--every", type=float, default=2.0)
    ap.add_argument("--data", type=pathlib.Path)
    ap.add_argument("--json", type=pathlib.Path)
    a = ap.parse_args()
    logs = a.log
    if not logs:
        roots = [a.data] if a.data else [ROOT / "data", ROOT / "build" / "boot" / "windowed_f"]
        found = sorted((p for r in roots for p in (r / "Logs").glob("game-*.log")), key=lambda p: p.stat().st_mtime)
        if not found:
            sys.exit("no game log found: pass LOG or --data ROOT")
        logs = [found[-1]]
    out = []
    colors = color_names([a.data or ROOT / "data", ROOT / "build" / "open_data" / "tree",
                          ROOT / "build" / "open_data" / "tree-binary"])
    for path in logs:
        head, players, samples, epochs, end, ai_lines, chats = read(path)
        last = max((s[-1][0] for s in samples.values() if s), default=0)
        print(f"== {path.name}: map {head.get('map', '?')} size {head.get('size', '?')} difficulty {head.get('difficulty', '?')}, "
              f"{last / 60000:.1f} game minutes{'' if end is not None else ' (no end line: the game was still running or crashed)'}"
              f", {ai_lines} AI trace lines")
        record = dict(log=str(path), head=head, players={}, chat=[dict(min=round(t / 60000, 1), player=p, text=x) for t, p, x in chats])
        if chats:
            print("\nchat (notes):")
            for t, p, x in chats:
                print(f"  {t / 60000:6.1f}m p{p}: {x}")
        for p in sorted(samples):
            info = players.get(p, {})
            who = info.get("kind", "?")
            if who == "computer":
                who += f" {info.get('personality', '-')} cheats={info.get('cheats', '-')}"
            reached = ", ".join(f"e{e} {t / 60000:.1f}m" for e, t in sorted(epochs[p].items())) or "none"
            color = colors.get(int(info["color"]), f"color {info['color']}") if info.get("color", "").isdigit() else "?"
            print(f"\np{p} {color} {who} civ={info.get('civ', '-')}: epochs {reached}")
            print("   min ep  cit idle  gather f/w/s/g/i  bld  army  bldg tow  stock f/w/s/g/i           income/min f/w/s/g/i  lost c/a/b by h/c")
            step = a.every * 60000
            next_t, window = 0.0, collections.Counter()
            lost, by = [0, 0, 0], [0, 0, 0]
            rows = []
            for t, s in samples[p]:
                for r, v in enumerate(five(s["got"])):
                    window[r] += v
                lost = [x + y for x, y in zip(lost, five(s["lost"]))]
                by = [x + y for x, y in zip(by, five(s["lost_by"]))]
                if t + 1 >= next_t:
                    span = max(step, 30000) / 60000 if next_t > 0 else 0.5
                    inc = [round(window[r] / span) for r in range(5)]
                    g = s["gather"]
                    print(f"  {t / 60000:4.0f} {s['epoch']:>2} {s['citizens']:>4} {s['idle']:>4}  {g:<17} {s['build']:>4} {s['army']:>5} "
                          f"{s['buildings']:>5} {s['towers']:>3}  {s['stock']:<26} {'/'.join(map(str, inc)):<21} "
                          f"{'/'.join(map(str, lost))} {by[0]}/{by[1]}")
                    rows.append({**s, "min": round(t / 60000, 1), "income_per_min": inc, "lost": list(lost), "lost_by": list(by)})
                    window.clear()
                    lost, by = [0, 0, 0], [0, 0, 0]
                    next_t = (t // step + 1) * step
            record["players"][p] = dict(info=info, epochs={e: t for e, t in epochs[p].items()}, timeline=rows)
        out.append(record)
    if a.json:
        a.json.write_text(json.dumps(out, indent=1) + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
