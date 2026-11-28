"""The retail .eeai personalities as a Lua table: data/ai/ageofai/personas_data.lua.

  python -I tools/personas.py            # write data/ai/ageofai/personas_data.lua (the default)
  python -I tools/personas.py --check    # regenerate and compare: exit 1 when the file is behind
  python -I tools/personas.py -v         # and print what was read, per personality

The source is data/ai/ageofai/personalities: the personality files of Grille's Empire Earth: Age of AI, vendored
there unchanged from upstream config/personalities (commit 7a17182; GPL-3.0, Copyright (C) 2025-2026 Grille and the
Age of AI contributors, see personalities/NOTICE). tools/eedata's rule - the data stays data - is why this is a
generated Lua table and not a hand-written one: rerun the tool when the files change, never edit the output.

Per file (the basename is the persona id, e.g. baumeister) it reads:

  * `personality.name` - the in-game name (Hedda, Temur, ...); a list takes its first name;
  * the six traits of `personality`: aggression, loyalty, pride, greed, forgiveness, caution, written `NN%`;
  * the six leanings of `personality`: air_force, navy, early_anti_air, pressure, defences, spread, `NN%`, under
    the port's names (air, navy, anti_air, pressure, defences, spread - personas.lua's M.LEAN_KEYS);
  * `personality.character` - the few words the AI voice plays;
  * `personality.civilization` - the name the mod shows for the AI's civilization ("Walled Cities"), the same
    unconditional rule as the lines below (the port shows it through ai.civ; the upgrades stay the game's);
  * `lines` - one entry per situation the port has (see SITUATIONS): greeting, war, peace, alliance, betrayal,
    capitulation, push, capture, loss.

WHAT IS DROPPED, AND WHY
The .eeai value of a line is an expression of the game - conditions, chances, comparisons, the file's own names
(`pawn => them.strength * 2 < me.strength`), `chance(60%)`, `me.kills(them, 20, buildings)`. This tool does NOT
evaluate any of it: it has no game. It keeps the line a personality says when nothing special holds, which is the
one an AI without the retail game's view can honestly say:

  * a line written `[cond ? ..., cond ? ...] ?? ["...", "..."]` takes the `??` list - upstream: "`a`, unless it is
    nothing: then `b`", so it is the text left when every condition above it is false. A list item that is itself
    a condition without an else (`enemy && chance(50%) ? "..."`) is dropped: it says nothing on its own;
  * a line written as a conditional chain `c ? a : d ? b : e` (no `??`) takes its last branch, `e` - the else of
    the whole chain, again the text when nothing holds;
  * a plain string or list is taken as it stands; `pick([...])` is the list, and strings side by side in `( )` are
    the one text they build;
  * `(...)` around any of those is unwrapped first.

The texts keep their placeholders' intent but not their contents: `{them}` (the player a line is said to), `{me}`,
`{enemy}`, `{lord}`, `{tribute}`, `{epoch}`, `{slights[them]}` and the like are removed - this engine's chat has no
per-situation filler - and `{X}'s` becomes "their" ("my" for `{me}'s`). Spacing, punctuation around the hole and a
sentence's first letter are tidied ("Peace with {enemy}, {them}." -> "Peace."). A line can lose the name it was
addressing, and a sentence whose subject was a placeholder reads bare ("Come on, lads! {them} is waiting!" ->
"Come on, lads! Is waiting!"); the words themselves are the personality's.

Not read at all: `title`, `color`, the `talk` trait, `style`, `priorities`, `decide` and `prompt`
records, the file's own names (vars), its event handlers (`on_territory_taken() { ... }`), and `include(...)` - the
mod's `mod/*.eeai` base layers are not vendored (this port has no line system to build on them). `talk` only sets
how often a line is said upstream; the port says a line on an event, so it has nothing to spend it on.

The output is deterministic: files are read sorted by persona id (only files that call `register_personality()`,
so `_template.eeai` is skipped), traits and leanings in the order above, situations in SITUATIONS' order, UTF-8
with LF newlines, no timestamps.
"""
import argparse
import difflib
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
SRC = ROOT / "data" / "ai" / "ageofai" / "personalities"
OUT = ROOT / "data" / "ai" / "ageofai" / "personas_data.lua"

# the traits, as the .eeai files and personas.lua spell them
TRAITS = ("aggression", "loyalty", "pride", "greed", "forgiveness", "caution")

# the leanings: (.eeai name, the port's name in personas.lua's M.LEAN_KEYS)
LEANINGS = (
    ("air_force", "air"),
    ("navy", "navy"),
    ("early_anti_air", "anti_air"),
    ("pressure", "pressure"),
    ("defences", "defences"),
    ("spread", "spread"),
)

# the retail mod's preferred colours for a personality (the game's player colour is fixed by the lobby; this is
# the chat/UI colour the port applies at runtime through ai.color)
COLORS = {
    "orange": (230, 120, 40), "grey": (130, 130, 130), "gray": (130, 130, 130),
    "blue": (60, 100, 200), "red": (200, 60, 60), "green": (60, 180, 60),
    "yellow": (200, 200, 60), "purple": (140, 60, 180), "brown": (140, 90, 50),
    "black": (40, 40, 40), "white": (220, 220, 220), "cyan": (60, 180, 180),
    "pink": (200, 100, 150), "gold": (200, 170, 60), "silver": (180, 180, 190),
    "darkred": (160, 40, 40), "magenta": (200, 80, 160), "lightyellow": (220, 220, 120),
    "lime": (120, 220, 60), "lightblue": (120, 160, 220),
}

# the port's situations (voice.lua's M.say callers) and the .eeai situations that answer them, best first. The
# port says a line when an alliance is offered, when peace is made, when it capitulates (offers) and when its army
# sets out, so `alliance_offer`, `peace_accept`, `capitulate` and `attack` come first; the other names are the
# situation next door (an accepted offer for an offer), kept so a personality with only that one still speaks.
SITUATIONS = (
    ("greeting", ("greeting",)),
    ("war", ("war_declared",)),
    ("peace", ("peace_accept", "peace_offer")),
    ("alliance", ("alliance_offer", "alliance_accept", "offer_accepted")),
    ("betrayal", ("betrayed",)),
    ("capitulation", ("capitulate", "capitulation_offer", "capitulate_accepted", "capitulation_accept")),
    ("push", ("attack", "attack_threat")),
    ("capture", ("territory_taken", "capture")),
    ("loss", ("territory_lost", "loss")),
)

OPEN = {"(": ")", "[": "]", "{": "}"}
CLOSE = {")", "]", "}"}
ESCAPES = {"n": "\n", "t": "\t", "r": "\r", '"': '"', "'": "'", "\\": "\\", "{": "{", "}": "}"}


# ---------------------------------------------------------------------------------------------------------------
# reading the .eeai text: strings, comments, brackets, records

def scan_string(text, i):
    """i is a quote; return the index just past the closing quote of the string that starts there."""
    quote = text[i]
    j = i + 1
    while j < len(text):
        if text[j] == "\\":
            j += 2
            continue
        if text[j] == quote:
            return j + 1
        j += 1
    raise ValueError("a string without its closing quote")


def strip_comments(text):
    """Drop `// ...` to the end of the line, outside strings (the .eeai comment, as in the game's own scripts)."""
    out = []
    i = 0
    while i < len(text):
        c = text[i]
        if c in "\"'":
            j = scan_string(text, i)
            out.append(text[i:j])
            i = j
        elif text.startswith("//", i):
            j = text.find("\n", i)
            i = len(text) if j < 0 else j
        else:
            out.append(c)
            i += 1
    return "".join(out)


def match_bracket(text, i):
    """i is one of ( [ {; return the index of the bracket that closes it."""
    stack = [OPEN[text[i]]]
    j = i + 1
    while j < len(text):
        c = text[j]
        if c in "\"'":
            j = scan_string(text, j)
            continue
        if c in OPEN:
            stack.append(OPEN[c])
        elif c in CLOSE:
            if not stack or stack[-1] != c:
                raise ValueError("a mismatched bracket at offset %d" % j)
            stack.pop()
            if not stack:
                return j
        j += 1
    raise ValueError("a bracket without its partner")


def split_top(text, separator=","):
    """Split on a separator that is outside every string and ( [ { ... } ] )."""
    parts, start, depth, i = [], 0, 0, 0
    while i < len(text):
        c = text[i]
        if c in "\"'":
            i = scan_string(text, i)
            continue
        if c in OPEN:
            depth += 1
        elif c in CLOSE:
            depth -= 1
        elif c == separator and depth == 0:
            parts.append(text[start:i])
            start = i + 1
        i += 1
    parts.append(text[start:])
    return parts


def find_top(text, operators):
    """The first of `operators` at depth 0 (longest match wins; `?` never matches the `?` of `??`)."""
    depth, i = 0, 0
    while i < len(text):
        c = text[i]
        if c in "\"'":
            i = scan_string(text, i)
            continue
        if c in OPEN:
            depth += 1
            i += 1
            continue
        if c in CLOSE:
            depth -= 1
            i += 1
            continue
        if depth == 0:
            for operator in operators:
                if operator == "?" and text.startswith("??", i):
                    continue
                if text.startswith(operator, i):
                    return i, operator
        i += 1
    return -1, None


def ternary_colon(text, question):
    """The `:` that belongs to the `?` at `question`, at the same depth (nested `? :` counted)."""
    depth, nested, i = 0, 0, question + 1
    while i < len(text):
        c = text[i]
        if c in "\"'":
            i = scan_string(text, i)
            continue
        if c in OPEN:
            depth += 1
        elif c in CLOSE:
            depth -= 1
        elif depth == 0:
            if text.startswith("??", i):
                i += 2
                continue
            if c == "?":
                nested += 1
            elif c == ":":
                if nested == 0:
                    return i
                nested -= 1
        i += 1
    return -1


def unwrap(text):
    """`(...)`, `((...))`: the expression inside, without brackets that hold the whole of it."""
    text = text.strip()
    while text.startswith("(") and match_bracket(text, 0) == len(text) - 1:
        text = text[1:-1].strip()
    return text


def unconditional(text):
    """The text a value says when none of its conditions holds (see the module's docstring), or None."""
    text = unwrap(text)
    if not text:
        return None
    i, _ = find_top(text, ("??",))
    if i >= 0:
        return unconditional(text[i + 2:])
    i, _ = find_top(text, ("?",))
    if i < 0:
        return text
    colon = ternary_colon(text, i)
    if colon < 0:
        return None            # `cond ? "..."` without an else: nothing when the condition does not hold
    return unconditional(text[colon + 1:])


def unescape(body):
    out = []
    i = 0
    while i < len(body):
        c = body[i]
        if c == "\\" and i + 1 < len(body):
            out.append(ESCAPES.get(body[i + 1], "\\" + body[i + 1]))
            i += 2
        else:
            out.append(c)
            i += 1
    return "".join(out)


def as_string(text):
    """The text of a plain string literal, or None."""
    text = text.strip()
    if len(text) >= 2 and text[0] in "\"'":
        try:
            end = scan_string(text, 0)
        except ValueError:
            return None
        if end == len(text):
            return unescape(text[1:-1])
    return None


def adjacent_strings(text):
    """`("a" "b")`: strings side by side are one text (it happens inside `( )`); None when it is not just that."""
    out = []
    i = 0
    while i < len(text):
        if text[i].isspace():
            i += 1
            continue
        if text[i] not in "\"'":
            return None
        try:
            j = scan_string(text, i)
        except ValueError:
            return None
        out.append(unescape(text[i + 1:j - 1]))
        i = j
    return out or None


def strings_of(text):
    """Every text an expression can say, conditions ignored (see the module's docstring)."""
    text = unconditional(text)
    if text is None:
        return []
    text = text.strip()
    literal = as_string(text)
    if literal is not None:
        return [literal]
    picked = re.fullmatch(r"pick\(\s*(\[.*\])\s*\)", text, re.S)
    if picked is not None:     # `pick([...])`: one of them at random; the tool takes the file's own order
        text = picked.group(1)
    if text.startswith("[") and match_bracket(text, 0) == len(text) - 1:
        out = []
        for item in split_top(text[1:-1]):
            if item.strip():
                out.extend(strings_of(item))
        return out
    joined = adjacent_strings(text)
    if joined is not None:     # `("a" ... "b")`: one text, built part by part
        return ["".join(joined)]
    return []                  # a number, a name of the file's own, a call: not a line


# ---------------------------------------------------------------------------------------------------------------
# the line texts: placeholders out, punctuation and capitals tidied

PLACEHOLDER = re.compile(r"\{[^{}]*\}")
POSSESSIVE = re.compile(r"\{([^{}]*)\}'s\b")

# a word that would dangle in front of the hole a placeholder leaves: "Peace with {enemy}." is "Peace with.",
# which becomes "Peace."
PREPOSITIONS = frozenset("with to for by at in on of from against about into over".split())


def strip_placeholders(text):
    """Remove every `{...}`, and a preposition left hanging in front of one ("Peace with {enemy}." -> "Peace.")."""
    while True:
        m = PLACEHOLDER.search(text)
        if m is None:
            return text
        start = m.start()
        rest = text[m.end():].lstrip()
        if rest == "" or rest[0] in ",.;:!?":
            word = re.search(r"\s+([A-Za-z]+)\s*$", text[:start])
            if word is not None and word.group(1).lower() in PREPOSITIONS:
                start = word.start()
        text = text[:start] + " " + text[m.end():]


def clean_text(text):
    """A line with its `{placeholders}` taken out, as plain readable text (see the module's docstring)."""
    text = POSSESSIVE.sub(lambda m: " my" if m.group(1).strip() == "me" else " their", text)
    text = strip_placeholders(text)
    text = re.sub(r"\s+", " ", text)
    text = re.sub(r"([.!?])\s+([.!?])", r"\1", text)     # a sentence's full stop beside the one a hole left
    text = re.sub(r"\s+([,.;:!?])", r"\1", text)
    text = re.sub(r"[,;:](\s*[,;:])+", lambda m: ":" if ":" in m.group(0) else m.group(0)[0], text)
    text = re.sub(r"([,;:])\s*([.?!])", r"\2", text)
    text = re.sub(r"([.!?])\s+-\s+", r"\1 ", text)      # the hole took the word a dash led to
    text = re.sub(r"^[\s,;:.!?\-]+", "", text)
    text = re.sub(r"[\s,;:]+$", "", text)
    text = re.sub(r"^([a-z])", lambda m: m.group(1).upper(), text)
    # a new sentence's first letter - but not after an ellipsis, which continues one
    text = re.sub(r"(?<![.!?])([.!?])(\s+)([a-z])",
                  lambda m: m.group(1) + m.group(2) + m.group(3).upper(), text)
    if not re.search(r"[A-Za-z0-9]", text):
        return ""
    return text


def line_texts(text):
    """The cleaned lines of a situation's value, in file order, without repeats."""
    out = []
    for raw in strings_of(text):
        line = clean_text(raw)
        if line and line not in out:
            out.append(line)
    return out


# ---------------------------------------------------------------------------------------------------------------
# a file

def find_record(text, name):
    """The inside of `name = { ... }` at the start of a line, or None."""
    m = re.search(r"(?m)^[ \t]*" + re.escape(name) + r"[ \t]*=[ \t]*\{", text)
    if m is None:
        return None
    opened = m.end() - 1
    return text[opened + 1:match_bracket(text, opened)]


def record_entries(block):
    """The `key = value` / `key => value` entries of a record's inside, in file order, as (key, value)."""
    out = []
    for part in split_top(block):
        if not part.strip():
            continue
        i, operator = find_top(part, ("=>", "="))
        if i < 0:
            continue
        key = part[:i].strip()
        if re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", key):
            out.append((key, part[i + len(operator):].strip()))
    return out


def first_text(text):
    for s in strings_of(text):
        return s
    return None


def as_percent(text):
    """`NN%` (or a plain 0..100; `0.2` counts as a fifth) as an integer, else None."""
    if text is None:
        return None
    value = unconditional(text)
    if value is None:
        return None
    value = value.strip()
    m = re.fullmatch(r"(\d+(?:\.\d+)?)\s*%", value)
    if m is not None:
        return int(round(float(m.group(1))))
    m = re.fullmatch(r"(\d+(?:\.\d+)?)", value)
    if m is not None:
        number = float(m.group(1))
        return int(round(number * 100)) if "." in m.group(1) and number <= 1 else int(round(number))
    return None


def parse_file(path):
    text = strip_comments(path.read_text(encoding="utf-8"))
    if re.search(r"(?m)^[ \t]*register_personality[ \t]*\(", text) is None:
        return None            # a library or the template, not a personality
    record = find_record(text, "personality")
    if record is None:
        raise ValueError("%s: a personality without its `personality = { ... }` record" % path.name)
    fields = dict(record_entries(record))

    name = first_text(fields.get("name", ""))
    if not name:
        raise ValueError("%s: a personality without a name" % path.name)

    traits = {}
    for key in TRAITS:
        value = as_percent(fields.get(key))
        if value is not None:
            traits[key] = value
    leanings = {}
    for source, port in LEANINGS:
        value = as_percent(fields.get(source))
        if value is not None:
            leanings[port] = value

    color = None
    cm = re.search(r'\[\s*"([A-Za-z]+)"', fields.get("color", ""))
    if cm is not None:
        color = COLORS.get(cm.group(1).lower())

    lines = {}
    block = find_record(text, "lines")
    if block is not None:
        entries = dict(record_entries(block))
        for situation, sources in SITUATIONS:
            for source in sources:
                if source in entries:
                    texts = line_texts(entries[source])
                    if texts:
                        lines[situation] = texts
                        break

    return {
        "id": path.stem,
        "name": name,
        "traits": traits,
        "leanings": leanings,
        "character": first_text(fields.get("character", "")) or "",
        "civ": first_text(fields.get("civilization", "")) or "",
        "color": color,
        "lines": lines,
    }


def read_personalities():
    if not SRC.is_dir():
        raise SystemExit("personas: %s is not there" % SRC)
    out = []
    for path in sorted(SRC.glob("*.eeai"), key=lambda p: p.name):
        persona = parse_file(path)
        if persona is not None:
            out.append(persona)
    if not out:
        raise SystemExit("personas: no personality in %s (a file declares register_personality())" % SRC)
    return out


# ---------------------------------------------------------------------------------------------------------------
# the Lua file

HEADER = """\
-- GENERATED by tools/personas.py - do not edit: `python tools/personas.py --write` writes it from
-- data/ai/ageofai/personalities/*.eeai, and `python tools/personas.py --check` fails while it is behind.
--
-- The personalities of Grille's Empire Earth: Age of AI (upstream config/personalities at 7a17182), vendored
-- unchanged in personalities/ with their README: GPL-3.0, Copyright (C) 2025-2026 Grille and the Age of AI
-- contributors (personalities/NOTICE). The port is under the same licence.
--
-- Per personality: the in-game name, the six traits and the six leanings 0..100 (50 is neutral; the files'
-- air_force and early_anti_air are the port's air and anti_air), the `character` text the AI voice plays, the
-- `civ` name the mod shows for its civilization, and the lines for the situations the port has. The .eeai
-- expressions are NOT evaluated: a situation holds the
-- unconditional lines - the `?? [ ... ]` list that is said when no condition holds, else the last `: ...` branch
-- of a conditional chain - with every {placeholder} stripped from the text (tools/personas.py has the mapping and
-- the tidy-up). A situation a personality has no line for is left out: ageofai/personas.lua falls back to its own
-- table for it.""".splitlines()

def lua_string(text):
    out = []
    for c in text:
        if c == "\\":
            out.append("\\\\")
        elif c == '"':
            out.append('\\"')
        elif c == "\n":
            out.append("\\n")
        elif c == "\r":
            out.append("\\r")
        elif c == "\t":
            out.append("\\t")
        else:
            out.append(c)
    return '"' + "".join(out) + '"'


def render(personalities):
    out = list(HEADER)
    out.append("return {")
    out.append("    list = {%s}," % ", ".join(lua_string(p["id"]) for p in personalities))
    out.append("    personas = {")
    for p in personalities:
        out.append("        %s = {" % p["id"])
        out.append("            name = %s," % lua_string(p["name"]))
        traits = ", ".join("%s = %d" % (k, p["traits"][k]) for k in TRAITS if k in p["traits"])
        out.append("            traits = {%s}," % traits)
        leanings = ", ".join("%s = %d" % (k, p["leanings"][k]) for _, k in LEANINGS if k in p["leanings"])
        out.append("            leanings = {%s}," % leanings)
        out.append("            character = %s," % lua_string(p["character"]))
        if p.get("civ"):
            out.append("            civ = %s," % lua_string(p["civ"]))
        if p.get("color") is not None:
            out.append("            color = {%d, %d, %d}," % (p["color"][0], p["color"][1], p["color"][2]))
        if p["lines"]:
            out.append("            lines = {")
            for situation, _ in SITUATIONS:
                if situation not in p["lines"]:
                    continue
                out.append("                %s = {" % situation)
                for line in p["lines"][situation]:
                    out.append("                    %s," % lua_string(line))
                out.append("                },")
            out.append("            },")
        else:
            out.append("            lines = {},")
        out.append("        },")
    out.append("    },")
    out.append("}")
    return "\n".join(out) + "\n"


# ---------------------------------------------------------------------------------------------------------------

def main(argv=None):
    parser = argparse.ArgumentParser(
        description=__doc__.split("\n")[0],
        formatter_class=argparse.RawDescriptionHelpFormatter, epilog=__doc__)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--write", action="store_true", help="write the Lua file (the default)")
    mode.add_argument("--check", action="store_true", help="regenerate and compare; exit 1 when it differs")
    parser.add_argument("-v", "--verbose", action="store_true", help="print every field that was read")
    args = parser.parse_args(argv)

    personalities = read_personalities()
    text = render(personalities)

    for p in personalities:
        counts = ", ".join("%s %d" % (s, len(p["lines"][s])) for s, _ in SITUATIONS if s in p["lines"])
        print("personas: %-10s %-8s traits %d/6 leanings %d/6 lines: %s"
              % (p["id"], p["name"], len(p["traits"]), len(p["leanings"]), counts or "none"))
        if args.verbose:
            print("          traits   " + ", ".join("%s %d" % (k, p["traits"][k]) for k in TRAITS if k in p["traits"]))
            print("          leanings " + ", ".join("%s %d" % (k, p["leanings"][k]) for _, k in LEANINGS if k in p["leanings"]))
            print("          character " + (p["character"] or "(none)"))
            for situation, _ in SITUATIONS:
                for line in p["lines"].get(situation, []):
                    print("          %-11s %s" % (situation, line))

    # bytes, not text: a CRLF or a missing final newline is a difference too
    current = OUT.read_bytes() if OUT.is_file() else b""
    if current == text.encode("utf-8"):
        print("personas: %s is current" % OUT.relative_to(ROOT))
        return 0
    if args.check:
        print("personas: %s is out of date - run `python tools/personas.py --write`" % OUT.relative_to(ROOT))
        if not current:
            print("personas: the file is missing")
            return 1
        diff = list(difflib.unified_diff(current.decode("utf-8", "replace").splitlines(), text.splitlines(),
                                         str(OUT.relative_to(ROOT)), "regenerated", lineterm="", n=2))
        for line in diff[:40]:
            print(line)
        if len(diff) > 40:
            print("... (%d more lines)" % (len(diff) - 40))
        return 1

    OUT.write_text(text, encoding="utf-8", newline="\n")
    print("personas: %d personalities, %s written (%d lines)"
          % (len(personalities), OUT.relative_to(ROOT), text.count("\n")))
    return 0


if __name__ == "__main__":
    sys.exit(main())
