"""Parse the on-disk record structs out of the empire_earth_next sources and compute their layouts."""
import re, os, json, sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
sys.path.insert(0, os.path.join(ROOT, 'tools', 'eedata'))
import overlay  # noqa: E402  (the record sources: C++ structs, or Rust records in rust/crates/ee_data tables.rs)
import specgen  # noqa: E402  (their layouts, both kinds)

# The record structs and their sources: eedata's (overlay.STRUCT_SOURCES), so a record that moved to Rust is read there
SRC = dict(overlay.STRUCT_SOURCES)
def parse(name):
    """The record's layout (specgen.source_fields: x86 natural alignment, pad entries) as this tool's field dicts."""
    entries, size = specgen.source_fields(name)
    fields = []
    for off, code, n, cnt, cmt in entries:
        sz = 1 if code == 'pad' else specgen.SIZES[code]
        fields.append(dict(off=off, type=code, name=n if code != 'pad' else '(pad)', count=cnt, size=sz * cnt,
                           comment=cmt or ''))
    return dict(name=name, size=size, fields=fields)


GAPNAME = re.compile(r'^(_gap\w*|_u\w*|m[LUCBF][0-9A-F]{2,3}|mU[0-9a-f]{3}|mUnknown\w*|mUndecoded\w*|mBoolPlaceholder\d|mPad|mIntParam\d|mFloatParam\d|mParam\d+|mL\w{2,3}|mB\w{2,3}|mC\w{2,3}|mF\w{2,3})$')


def is_undecoded(f):
    return f['type'] == 'pad' or bool(GAPNAME.match(f['name']))


if __name__ == '__main__':
    out = {}
    for n in SRC:
        r = parse(n)
        dec = sum(f['size'] for f in r['fields'] if not is_undecoded(f))
        r['decoded_bytes'] = dec
        out[n] = r
        print(f"{n:30s} size={r['size']:5d} decoded={dec:5d} ({100*dec/r['size']:.0f}%)")
    json.dump(out, open(os.path.join(os.path.dirname(__file__), 'structs.json'), 'w'), indent=1)
