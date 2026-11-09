"""Generate the Rust table specs from tools/eedata/specs.json (docs/RUST_PLAN.md R2.2):
rust/crates/ee_data/src/generated/specs_gen.rs: the spec tables the text compiler reads (the same data as the C++
EEDataTextSpecs.h), one #[repr(C)] record mirror per struct with size and offset asserts, and a typed text struct
per table (one row's keys, read with the compiler's grammar).

  python -I tools/eedata/eedata.py rustgen [--check]

`eedata.py check` fails when the file is stale.
"""
import pathlib
import re

import binfmt
import scalars

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parent.parent
OUT = ROOT / 'rust' / 'crates' / 'ee_data' / 'src' / 'generated' / 'specs_gen.rs'

RUST_INT = {'s8': 'i8', 'u8': 'u8', 'bool': 'u8', 's16': 'i16', 'u16': 'u16', 's32': 'i32', 'u32': 'u32', 'f32': 'f32'}
SPEC_TYPE = {'s8': 'S8', 'u8': 'U8', 'bool': 'Bool', 's16': 'S16', 'u16': 'U16', 's32': 'S32', 'u32': 'U32', 'f32': 'F32',
             'pair_f32': 'PairF32', 'str': 'Str'}
KEYWORDS = {'type', 'match', 'loop', 'self', 'fn', 'use', 'mod', 'ref', 'in', 'if', 'as'}


def snake(name):
    if name.startswith('m') and name[1:2].isupper():
        name = name[1:]
    parts = re.findall(r'[A-Z]+(?=[A-Z][a-z])|[A-Z]?[a-z]+|[A-Z]+|\d+', name)
    s = '_'.join(p.lower() for p in parts) or name
    if s in KEYWORDS:
        s += '_'
    if s[0].isdigit():
        s = 'f_' + s
    return s


def camel(name):
    return ''.join(p[:1].upper() + p[1:] for p in re.split(r'[_\W]', name) if p)


def c_str(s):
    return '"' + s.replace('\\', '\\\\').replace('"', '\\"') + '"'


def byte_literal(b):
    """A byte string as a Rust &[u8] slice literal."""
    return 'b"' + ''.join(f'\\x{x:02x}' for x in b) + '"'


def opt(s):
    return f'Some({c_str(s)})' if s else 'None'


def ident_map(fields):
    """Field name -> a unique Rust identifier."""
    out, used = {}, set()
    for f in fields:
        n = snake(f['name'])
        k, i = n, 2
        while k in used:
            k, i = f'{n}_{i}', i + 1
        used.add(k)
        out[f['name']] = k
    return out


def field_size(f):
    return {'s8': 1, 'u8': 1, 'bool': 1, 's16': 2, 'u16': 2, 's32': 4, 'u32': 4, 'f32': 4, 'pair_f32': 8, 'str': 1,
            'raw': 1, 'pad': 1}[f['type']] * f['count']


def field_type(f):
    t, n = f['type'], f['count']
    if t in ('str', 'raw', 'pad'):
        return f'[u8; {n}]'
    if t == 'pair_f32':
        return f'[[f32; 2]; {n}]'
    return RUST_INT[t] if n == 1 else f'[{RUST_INT[t]}; {n}]'


def field_align(f):
    return {'s8': 1, 'u8': 1, 'bool': 1, 's16': 2, 'u16': 2, 's32': 4, 'u32': 4, 'f32': 4, 'pair_f32': 4, 'str': 1,
            'raw': 1, 'pad': 1}[f['type']]


def gen_mirror(sname, st):
    """A #[repr(C)] struct, with gaps made explicit; packed when a field is not naturally aligned."""
    fields = sorted(st['fields'], key=lambda f: f['off'])
    ids = ident_map(fields)
    packed = any(f['off'] % field_align(f) for f in fields)
    o = ['#[derive(Clone, Copy)]', '#[allow(non_camel_case_types, dead_code)]']
    o.append('#[repr(C, packed)]' if packed else '#[repr(C)]')
    o.append(f'pub struct {sname} {{')
    at, gap = 0, 0
    for f in fields:
        if f['off'] > at:
            o.append(f'    pub _gap{gap}: [u8; {f["off"] - at}],')
            gap += 1
        elif f['off'] < at:
            raise ValueError(f'{sname}.{f["name"]}: overlaps the previous field')
        o.append(f'    pub {ids[f["name"]]}: {field_type(f)},')
        at = f['off'] + field_size(f)
    if at < st['size']:
        o.append(f'    pub _gap{gap}: [u8; {st["size"] - at}],')
    o.append('}')
    o.append(f'const _: () = assert!(core::mem::size_of::<{sname}>() == {st["size"]});')
    for f in fields:
        o.append(f'const _: () = assert!(core::mem::offset_of!({sname}, {ids[f["name"]]}) == {f["off"]});')
    return o


def gen_text(name, t, st, spec):
    """A typed text struct: one row's keys, read with the compiler's grammar."""
    fields = binfmt.exported(st['fields'])
    ty = camel(name) + 'Text'
    o = ['#[derive(Clone, Debug)]', f'pub struct {ty} {{', '    pub row: Option<i32>,', '    pub id: Option<Vec<u8>>,']
    body = ['        let mut g = Getter::new(m, where_);', '        let row = textrow::int(&mut g, "row", Type::S32)?.map(|v| v as i32);',
            '        let id = textrow::raw(&mut g, "id")?;']
    names = []
    ids = {'row': 'row', 'id': 'id'}
    used = {'row', 'id'}

    def add(key, rust_type, reader):
        nonlocal names
        base = snake(key)
        k, i = base, 2
        while k in used:
            k, i = f'{base}_{i}', i + 1
        used.add(k)
        ids[key] = k
        o.append(f'    pub {k}: {rust_type},')
        body.append(f'        let {k} = {reader};')
        names.append(k)

    # the leading strings
    for lead in t.get('lead', []):
        m = re.fullmatch(r'([^|\[]+)(?:\|([^\[]+))?(?:\[(\d+)\])?', lead)
        a, b, n = m.group(1), m.group(2), m.group(3)
        if n is None:
            add(a, 'Option<Vec<u8>>', f'textrow::text(&mut g, {c_str(a)})?')
        else:
            add(a, 'Vec<Option<Vec<u8>>>', f'textrow::text_array(&mut g, {c_str(a)}, {n})?')
            if b:
                add(b, 'Vec<Option<Vec<u8>>>', f'textrow::text_array(&mut g, {c_str(b)}, {n})?')
    epoch_flags = (t.get('epoch_flags') or [None])[0]
    keyed_levels = t.get('keyed_levels')
    refs = t.get('refs', {})
    for f in fields:
        if f['name'] == epoch_flags:
            continue
        if keyed_levels and f['name'] == keyed_levels[0]:
            continue
        key = f['name']
        tyname = f['type']
        n = f['count']
        if key in refs:
            if n == 1:
                add(key, 'Option<Vec<u8>>', f'textrow::raw(&mut g, {c_str(key)})?')
            else:
                add(key, 'Vec<Option<Vec<u8>>>', f'textrow::raw_array(&mut g, {c_str(key)}, {n})?')
        elif tyname == 'str':
            add(key, 'Option<Vec<u8>>', f'textrow::text(&mut g, {c_str(key)})?')
        elif tyname == 'pair_f32':
            add(key, 'Vec<[Option<f32>; 2]>', f'textrow::pairs(&mut g, {c_str(key)}, {n})?')
        elif n == 1:
            add(key, f'Option<{RUST_INT[tyname]}>', f'textrow::num::<{RUST_INT[tyname]}>(&mut g, {c_str(key)}, Type::{SPEC_TYPE[tyname]})?')
        else:
            add(key, f'Vec<Option<{RUST_INT[tyname]}>>', f'textrow::num_array::<{RUST_INT[tyname]}>(&mut g, {c_str(key)}, Type::{SPEC_TYPE[tyname]}, {n})?')
    if t.get('list'):
        lst = t['list'][0]
        add(lst, 'Vec<Vec<u8>>', f'textrow::raw_list(&mut g, {c_str(lst)})?')
    extra = []
    if t['container'] == 'techtree':
        extra = ['epoch']
    elif t['container'] == 'epochs':
        extra = ['slots', 'music', 'citizen']
    if keyed_levels:
        extra.append(keyed_levels[1])
    for key in extra:
        add(key, 'Option<Node>', f'g.get({c_str(key)}).cloned()')
    o.append('}')
    o.append(f'impl {ty} {{')
    o.append('    /// One record mapping (a YAML entry or a CSV row made a mapping) -> its typed keys; an unknown key is an error.')
    o.append("    pub fn from_node(m: &Node, where_: &[u8]) -> Res<Self> {")
    o += body
    o.append('        g.leftover()?;')
    o.append(f'        Ok(Self {{ row, id, {", ".join(names)} }})' if names else '        Ok(Self { row, id })')
    o.append('    }')
    o.append('}')
    return o


def generate(spec):
    o = ['// GENERATED by tools/eedata/rustgen.py from tools/eedata/specs.json: do not edit (eedata.py rustgen).',
         '#![allow(clippy::all, dead_code, unused_variables, non_upper_case_globals, non_snake_case)]',
         '',
         'use crate::node::Node;',
         'use crate::spec::{Container, Derived, Field, Ref, RowName, Struct, Table, Type};',
         'use crate::text::{Getter, Res};',
         'use crate::textrow;',
         '',
         f'pub const FORMAT_VERSION: i64 = {spec["format"]};',
         '']
    structs = spec['structs']
    names = list(structs)
    sidx = {n: i for i, n in enumerate(names)}
    for sname, st in structs.items():
        o.append(f'static FIELDS_{sidx[sname]}: &[Field] = &[')
        for f in binfmt.exported(st['fields']):
            o.append(f'    Field {{ name: {c_str(f["name"])}, offset: {f["off"]}, count: {f["count"]}, ty: Type::{SPEC_TYPE[f["type"]]} }},')
        o.append('];')
        rec = binfmt.base_record(st)
        base = f'Some({byte_literal(rec)})' if any(rec) else 'None'
        o.append(f'static STRUCT_{sidx[sname]}: Struct = Struct {{ name: {c_str(sname)}, size: {st["size"]}, fields: FIELDS_{sidx[sname]}, base: {base} }};')
    o.append('')
    for name, t in spec['tables'].items():
        i = len(o)
        if t.get('refs'):
            o.append(f'static REFS_{name}: &[Ref] = &[')
            for fn, r in t['refs'].items():
                o.append(f'    Ref {{ field: {c_str(fn)}, table: {c_str(r["table"])}, none: &[{", ".join(map(str, r["none"]))}], never: {"true" if r.get("never") else "false"} }},')
            o.append('];')
        if t.get('derived'):
            o.append(f'static DERIVED_{name}: &[Derived] = &[' +
                     ', '.join(f'Derived {{ field: {c_str(k)}, a: {a}, b: {b} }}' for k, (a, b) in t['derived'].items()) + '];')
        if t.get('lead'):
            o.append(f'static LEAD_{name}: &[&str] = &[' + ', '.join(c_str(x) for x in t['lead']) + '];')
        rn = spec.get('row_names', {}).get(name)
        if rn:
            rf = t.get('row_fields', {})
            o.append(f'static ROWS_{name}: &[RowName] = &[')
            for k, v in sorted(rn.items(), key=lambda kv: int(kv[0])):
                o.append(f'    RowName {{ row: {int(k)}, name: {c_str(v)}, fields: {c_str(" ".join(rf.get(k, [])))} }},')
            o.append('];')
    o.append('')
    o.append('pub static TABLES: &[Table] = &[')
    for name, t in spec['tables'].items():
        st = structs[t['struct']]
        hole = binfmt.hole_record(t, st)
        key = t.get('key', 'row')
        key = f'Some({c_str(key)})' if key not in ('row', 'position') else 'None'
        ef = t.get('epoch_flags') or (None, None)
        kl = t.get('keyed_levels') or (None, None)
        selff = t.get('self') or (None, 0)
        o.append('    Table {')
        o.append(f'        name: {c_str(name)},')
        o.append(f'        container: Container::{camel(t["container"])},')
        o.append(f'        yaml: {"true" if t["format"] == "yaml" else "false"},')
        o.append(f'        record: &STRUCT_{sidx[t["struct"]]},')
        o.append(f'        lead: {"LEAD_" + name if t.get("lead") else "&[]"},')
        o.append(f'        list_name: {opt(t["list"][0]) if t.get("list") else "None"},')
        o.append(f'        list_signed: {"true" if t.get("list") and t["list"][1] == "s32" else "false"},')
        o.append(f'        row_names: {"ROWS_" + name if spec.get("row_names", {}).get(name) else "&[]"},')
        o.append(f'        hole: {"Some(" + byte_literal(hole) + ")" if any(hole) else "None"},')
        o.append(f'        key_field: {key},')
        o.append(f'        self_field: {opt(selff[0])},')
        o.append(f'        self_offset: {selff[1]},')
        o.append(f'        refs: {"REFS_" + name if t.get("refs") else "&[]"},')
        o.append(f'        derived: {"DERIVED_" + name if t.get("derived") else "&[]"},')
        o.append(f'        text_only: {"true" if t.get("text_only") else "false"},')
        o.append(f'        epoch_flags: {opt(ef[0])},')
        o.append(f'        epoch_key: {opt(ef[1])},')
        o.append(f'        levels_field: {opt(kl[0])},')
        o.append(f'        levels_key: {opt(kl[1])},')
        o.append('    },')
    o.append('];')
    o.append('')
    o.append('/// CP-1252 byte -> Unicode code point (the five undefined bytes map to the C1 code point of the same value).')
    o.append('pub static CODE_PAGE: [u16; 256] = [')
    cps = [ord(scalars.decode_str(bytes([b]))) for b in range(256)]
    for i in range(0, 256, 16):
        o.append('    ' + ', '.join(f'0x{c:04x}' for c in cps[i:i + 16]) + ',')
    o.append('];')
    o.append('')
    o.append('pub mod records {')
    for sname, st in structs.items():
        o += ['    ' + x if x else x for x in gen_mirror(sname, st)]
        o.append('')
    o.append('}')
    o.append('')
    o.append('pub mod text_rows {')
    o.append('    use super::*;')
    for name, t in spec['tables'].items():
        o.append('')
        o += ['    ' + x if x else x for x in gen_text(name, t, structs[t['struct']], spec)]
    o.append('}')
    return ('\n'.join(o) + '\n').encode('utf-8')


def run(a, spec):
    new = generate(spec)
    if a.check:
        ok = OUT.exists() and OUT.read_bytes() == new
        print(f'{OUT.name} is ' + ('up to date' if ok else 'STALE: run eedata.py rustgen'))
        return 0 if ok else 1
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_bytes(new)
    print(f'-> {OUT}')
    return 0
