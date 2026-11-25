"""Generate the rules code from tools/eedata/rules_schema.json (docs/RUST_PLAN.md R2.1): the rules documents' enums,
structs, defaults, the records C++ reads (Rust writes them in place) and the content dump. Output:
rust/crates/ee_data/src/generated/rules_gen.rs. The C++ records and enums (EECombatRules, EECivStat, ...) are the
`#[repr(C)]` ones of that file, emitted by cbindgen into include/rust/data/generated.h (`rust_build.py header`), which
ee/EERules.h includes; there is no generated C++ any more (once include/ee/EERulesGen.h).

  python -I tools/eedata/eedata.py rulesgen [--check]

The rules' document readers (the semantic checks) are hand written in Rust (ee_data/src/rules.rs), as rules.py's
validator is in Python; both read this schema's name tables.
"""
import json
import pathlib
import re

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parent.parent
SCHEMA = HERE / 'rules_schema.json'
INC = ROOT / 'include' / 'ee'
RUST_OUT = ROOT / 'rust' / 'crates' / 'ee_data' / 'src' / 'generated' / 'rules_gen.rs'
HEADERS = [('../rust/sim/unit_ids.h', 'kUnitCategory'), ('../rust/sim/unit_ids.h', 'kUnitType'), ('EEStringIDs.h', 'IDS_'),
           ('../rust/sim/tech_tree_ids.h', 'kTech'), ('EECivPowerIDs.h', 'kCivPower')]


def load():
    return json.loads(SCHEMA.read_text(encoding='utf-8'))


def _symbols(schema):
    """{symbol: int}: the constants the schema's defaults name, from the C++ headers and the schema's enums."""
    out = {}
    for header, prefix in HEADERS:
        text = (INC / header).read_text(encoding='latin-1')
        out.update({n: int(v, 0) for n, v in re.findall(r'\b(' + prefix + r'\w*)\s*=\s*(0x[0-9a-fA-F]+|\d+)', text)})
        out.update({n: int(v, 0) for n, v in re.findall(r'#define\s+(' + prefix + r'\w*)\s+(0x[0-9a-fA-F]+|\d+)', text)})
    for e in schema['enums'].values():
        if 'cpp_prefix' in e:
            for i, v in enumerate(e['cpp_values']):
                out[e['cpp_prefix'] + v] = (1 << i) if e.get('bits') else i
    return out


class Gen:
    def __init__(self):
        self.s = load()
        self.sym = _symbols(self.s)
        self.structs = {x['name']: x for x in self.s['structs']}

    def value(self, v):
        """A default: an int, or 'SYM', 'SYM+n', 'SYM-SYM' (a constant expression over the headers' constants)."""
        if isinstance(v, (int, float)):
            return v
        total, sign = 0, 1
        for tok in re.findall(r'[+-]|[A-Za-z_0-9]+', v):
            if tok == '+':
                sign = 1
            elif tok == '-':
                sign = -1
            else:
                total += sign * (int(tok, 0) if tok[0].isdigit() else self.sym[tok])
                sign = 1
        return total

    def snake(self, cpp):
        name = cpp[1:] if cpp.startswith('m') and cpp[1:2].isupper() else cpp
        return '_'.join(p.lower() for p in re.findall(r'[A-Z]+(?=[A-Z][a-z])|[A-Z]?[a-z]+|[A-Z]+|\d+', name))


def _lit(x):
    """A float as a shortest literal (the same text for both languages' f32 parsers)."""
    r = repr(float(x))
    return r if 'e' in r or '.' in r else r + '.0'


def _rdoc(lines, indent):
    return [indent + '/// ' + x if x else indent + '///' for x in lines.split('\n')]


# ------------------------------------------------------------------------------------------- Rust


def rust_type(f, gen):
    t = f['type']
    m = {'int': 'i32', 'float': 'f32', 'bool': 'bool', 'number': 'RuleNumber', 'string': 'Vec<u8>',
         'int_list': 'Vec<i32>', 'float_list': 'Vec<f32>'}
    if t in m:
        return m[t]
    if t == 'float_array':
        return f'[f32; {f["count"]}]'
    if t.startswith('list:'):
        return f'Vec<{gen.structs[t[5:]]["rust"]}>'
    raise ValueError(t)


def rust_value(v, ty, gen):
    """A value in Rust's terms: the schema's default or a list_defaults entry."""
    if ty == 'int':
        return str(gen.value(v["const"] if isinstance(v, dict) else v))
    if ty == 'float':
        return _lit(v) + '_f32'
    if ty == 'bool':
        return 'true' if v else 'false'
    if ty == 'string':
        return f'b"{v}".to_vec()'
    if ty == 'number':
        if 'world' in v:
            return f'RuleNumber::world({v["world"][0]}, {v["world"][1]})'
        return f'RuleNumber::constant({_lit(v["const"])}_f32)'
    if ty == 'int_list':
        return 'vec![' + ', '.join(str(gen.value(x)) for x in v) + ']'
    if ty == 'float_list':
        return 'vec![' + ', '.join(_lit(x) + '_f32' for x in v) + ']'
    if ty == 'float_array':
        return '[' + ', '.join(_lit(x) + '_f32' for x in v) + ']'
    if ty.startswith('list:'):
        st = gen.structs[ty[5:]]
        return 'vec![' + ', '.join(rust_literal(st, x, gen) for x in v) + ']'
    raise ValueError(ty)


def rust_literal(st, row, gen):
    """One struct value from a list_defaults entry: fields it names, the struct's default for the rest."""
    known = {f['cpp']: f for f in st['fields']}
    bad = set(row) - set(known)
    if bad:
        raise ValueError(f'{st["name"]}: {bad}')
    parts = []
    for cpp, v in row.items():
        f = known[cpp]
        parts.append(f'{gen.snake(cpp)}: {rust_value(v, f["type"], gen)}')
    return st['rust'] + ' { ' + ', '.join(parts) + ', ..Default::default() }'


def rust_default_expr(f, gen):
    if 'default_rust' in f:
        return f'default_{f["default_rust"]}()'
    t = f['type']
    if 'default' not in f:
        return 'Default::default()' if t != 'float_array' else '[0.0; 5]'
    return rust_value(f['default'], t, gen)


def rust_mirror_type(f, gen):
    t = f['type']
    m = {'int': 'c_long', 'float': 'f32', 'bool': 'bool', 'number': 'RuleNumberC', 'string': 'RuleTextC',
         'int_list': 'RuleListC<c_long>', 'float_list': 'RuleListC<f32>'}
    if t in m:
        return m[t]
    if t == 'float_array':
        return f'[f32; {f["count"]}]'
    if t.startswith('list:'):
        return f'RuleListC<{t[5:]}>'
    if t.startswith('struct:'):
        return t[7:]
    raise ValueError(t)


def rust_mirror(f, gen, v):
    t = f['type']
    if t == 'int':
        return f'{v} as c_long'
    if t in ('float', 'bool', 'float_array'):
        return v
    if t == 'number':
        return f'RuleNumberC::of(&{v})'
    if t == 'string':
        return f's.text(&{v})'
    if t == 'int_list':
        return f's.list({v}.iter().map(|x| *x as c_long).collect())'
    if t == 'float_list':
        return f's.list({v}.clone())'
    if t.startswith('list:'):
        return f'{{ let l = {v}.iter().map(|x| x.mirror(s)).collect(); s.list(l) }}'
    if t.startswith('struct:'):
        return f'{v}.mirror(s)'
    raise ValueError(t)


def rust_dump(f, gen, v):
    t, k = f['type'], f['dump']
    if t == 'int':
        return f'j.int(b"{k}", {v});'
    if t == 'float':
        return f'j.float(b"{k}", {v});'
    if t == 'bool':
        return f'j.int(b"{k}", i32::from({v}));'
    if t == 'number':
        return f'j.number(b"{k}", &{v});'
    if t == 'string':
        return f'j.string(b"{k}", &{v});'
    if t == 'int_list':
        return f'j.list(b"{k}", &{v});'
    if t == 'float_array':
        return f'for (k, x) in {v}.iter().enumerate() {{ j.float(format!("{k}{{k}}").as_bytes(), *x); }}'
    if t.startswith('list:'):
        return (f'j.open(Some(b"{k}"), b\'[\'); for x in &{v} {{ j.open(None, b\'{{\'); x.dump(j); '
                f'j.close(b\'}}\'); }} j.close(b\']\');')
    raise ValueError(t)


def generate_rust(gen):
    o = ['// GENERATED by tools/eedata/rulesgen.py from tools/eedata/rules_schema.json: do not edit (eedata.py rulesgen).',
         '// The rules documents\' data as Rust reads it (`CombatRules`, ...) and as C++ does (`EECombatRules`, ...: the',
         '// records and enums cbindgen emits into rust/data/generated.h, which ee/EERules.h includes). `mirror` writes the',
         '// C++ records in place (EERules::Refresh); their lists and texts point into the Rust rules\' storage.',
         '#![allow(clippy::all)]',
         '#![allow(non_snake_case, non_camel_case_types, non_upper_case_globals)]',
         '',
         'use crate::rules::{Json, MirrorStore, RuleListC, RuleNumber, RuleNumberC, RuleTextC};',
         'use std::os::raw::c_long;',
         '']
    for name, e in gen.s['enums'].items():
        o.append(f'pub const {name.upper()}_NAMES: &[&str] = &[' + ', '.join(f'"{n}"' for n in e['names']) + '];')
        if 'cpp_prefix' in e:
            for i, v in enumerate(e['cpp_values']):
                sym = gen.snake(e['cpp_prefix'] + v).upper()
                o.append(f'pub const {sym}: i32 = {(1 << i) if e.get("bits") else i};')
            if 'cpp_count' in e and not e.get('bits'):
                o.append(f'pub const {gen.snake(e["cpp_count"]).upper()}: usize = {len(e["cpp_values"])};')
        if 'cpp_enum' in e:
            # the C++ enum (cbindgen emits it back, unscoped)
            o += _rdoc(e.get('doc', e['cpp_enum']), '')
            o.append('/// cbindgen:enum-class=false')
            o.append('#[repr(C)]')
            o.append('#[derive(Clone, Copy, Debug, PartialEq, Eq)]')
            o.append(f'pub enum {e["cpp_enum"]} {{')
            for i, v in enumerate(e['cpp_values']):
                if e.get('value_docs') and e['value_docs'][i]:
                    o += _rdoc(e['value_docs'][i], '    ')
                o.append(f'    {e["cpp_prefix"]}{v} = {(1 << i) if e.get("bits") else i},')
            if e.get('bits'):
                o.append(f'    {e["cpp_all"]} = {(1 << len(e["cpp_values"])) - 1},')
            elif 'cpp_count' in e:
                o.append(f'    {e["cpp_count"]} = {len(e["cpp_values"])},')
            o.append('}')
    for sym in gen.s['epoch_constants']:
        o.append(f'pub const {gen.snake(sym).upper()}: i32 = {gen.sym[sym]};')
    o.append('')
    for st in gen.s['structs']:
        o.append('#[derive(Clone, Debug, PartialEq)]')
        o.append(f'pub struct {st["rust"]} {{')
        for f in st['fields']:
            o.append(f'    pub {gen.snake(f["cpp"])}: {rust_type(f, gen)},')
        o.append('}')
        o.append(f'impl Default for {st["rust"]} {{')
        o.append('    fn default() -> Self {')
        o.append('        Self {')
        for f in st['fields']:
            o.append(f'            {gen.snake(f["cpp"])}: {rust_default_expr(f, gen)},')
        o.append('        }')
        o.append('    }')
        o.append('}')
        o += _rdoc((st['doc'] + '\n' if 'doc' in st else '') + f'(`{st["rust"]}` as C++ reads it'
                   + (f': {st["section"]}' if 'section' in st else '') + ')', '')
        o.append('#[repr(C)]')
        o.append(f'pub struct {st["name"]} {{')
        for f in st['fields']:
            if 'doc' in f:
                o += _rdoc(f['doc'], '    ')
            o.append(f'    pub {f["cpp"]}: {rust_mirror_type(f, gen)},')
        o.append('}')
        o.append(f'impl {st["rust"]} {{')
        uses = any(rust_mirror(f, gen, 'v').find('s.') >= 0 for f in st['fields'])
        o.append(f'    pub fn mirror(&self, {"s" if uses else "_s"}: &mut MirrorStore) -> {st["name"]} {{')
        o.append(f'        {st["name"]} {{')
        for f in st['fields']:
            n = gen.snake(f['cpp'])
            o.append(f'            {f["cpp"]}: {rust_mirror(f, gen, "self." + n)},')
        o.append('        }')
        o.append('    }')
        if st.get('dump', True):
            o.append('    pub fn dump(&self, j: &mut Json<\'_>) {')
            for f in st['fields']:
                if f.get('dump'):
                    o.append('        ' + rust_dump(f, gen, 'self.' + gen.snake(f['cpp'])))
            o.append('    }')
        o.append('}')
        o.append('')
    for n, sname in (('attributes', 'EECivAttribute'), ('powers', 'EECivPowerRule'), ('personalities', 'EEAIPersonalityRule')):
        st = gen.structs[sname]
        o.append(f'pub fn default_{n}() -> Vec<{st["rust"]}> {{')
        o.append('    vec![')
        for row in gen.s['list_defaults'][n]:
            o.append('        ' + rust_literal(st, row, gen) + ',')
        o.append('    ]')
        o.append('}')
    o.append('')
    o.append('pub const DOCUMENTS: &[&str] = &[' + ', '.join(f'"{n}"' for n in gen.s['documents']) + '];')
    o.append('')
    o.append('/// The rules\' data (the C++ EERules derives from it)')
    o.append('#[repr(C)]')
    o.append('pub struct EERulesData {')
    for f in gen.s['top']:
        o.append(f'    pub {f["cpp"]}: {rust_mirror_type(f, gen)},')
    o.append('}')
    o.append('')
    o.append('pub fn mirror_top(r: &crate::rules::Rules, s: &mut MirrorStore) -> EERulesData {')
    o.append('    EERulesData {')
    for f in gen.s['top']:
        n = gen.snake(f['cpp'])
        o.append(f'        {f["cpp"]}: {rust_mirror(f, gen, "r." + n)},')
    o.append('    }')
    o.append('}')
    return ('\n'.join(o) + '\n').encode('utf-8')


def outputs():
    gen = Gen()
    return [(RUST_OUT, generate_rust(gen))]


def run(a):
    status = 0
    for path, data in outputs():
        if a.check:
            ok = path.exists() and path.read_bytes() == data
            print(f'{path.name} is ' + ('up to date' if ok else 'STALE: run eedata.py rulesgen'))
            status |= 0 if ok else 1
        else:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
            print(f'-> {path}')
    return status
