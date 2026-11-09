//! The db tables' text compiler: every table's text compiled into the binary db file bytes the loaders read, resolving
//! the references between tables by id (tools/eedata tables.Content; the C++ `EEDataText.cpp` before it). The single
//! place where tables meet: a later mod layer feeds it merged entries instead of files.
//!
//! Allocation budget (the game starts under the Windows debug heap in every harness session, where an allocation
//! costs tens of microseconds): a parsed text shares its source (scalars and keys are slices of it), a record is held
//! as its list and index, the places in messages ("dbobjects line 120") are written only when a message is, and the
//! per-row work allocates only what the output keeps (PORT_LEARNINGS "R2 performance").

use crate::msg;
use crate::msg::{At, Part};
use crate::node::{present, Bytes, Kind, Node};
use crate::spec::{find_table, table, Container, Field, Ref, Table, Type};
use crate::text::{
    self, elems, number, parse_csv_shared, parse_int, parse_yaml_shared, scalar_text, text_value, text_value_into,
    Failure, Getter, Res,
};
use crate::{fail, generated::specs_gen::FORMAT_VERSION};
use std::collections::{BTreeMap, HashMap};
use std::rc::Rc;

/// A table's or rules document's text, by name; None when there is no such text.
pub type Reader = Box<dyn FnMut(&str) -> Option<Vec<u8>>>;

/// A listed record: its position (the row's key, or its slot in an epoch block), its mapping (item `at` of the list
/// `list`) and line.
#[derive(Clone)]
struct Placed {
    pos: i64,
    list: Rc<Node>,
    at: usize,
    line: i64,
}

impl Placed {
    fn map(&self) -> &Node {
        &self.list.items[self.at]
    }
}

/// One epoch of the tech tree: the dbtechtree.yaml techs placed in its block's slots (its epochs.yaml entry is item k
/// of `Layout::epochs`).
struct EpochBlock {
    line: i64,
    slots: i64,
    nodes: Vec<Placed>,
}

/// epochs.yaml's records (a list, or empty) and the blocks placed from them.
struct Layout {
    epochs: Rc<Node>,
    blocks: Vec<EpochBlock>,
}

impl Layout {
    fn epoch(&self, k: usize) -> &Node {
        &self.epochs.items[k]
    }
}

/// A table's text, parsed once: its records list and the records placed at their positions (dbtechtree and epochs:
/// placed together).
struct Doc {
    records: Option<Rc<Node>>,
    total: i64,
    rows: Vec<Placed>,
}

/// The ids of a table: id -> key, and which keys are listed (one shared copy of each id).
#[derive(Default)]
pub struct Index {
    pub keys: HashMap<Rc<[u8]>, i64>,
    pub ids: BTreeMap<i64, Rc<[u8]>>,
}

/// A built row: its record is `recs[rec..rec + size]` of the table's record buffer (one buffer for every row).
#[derive(Clone, Default)]
struct Row {
    rec: usize,
    lead: Vec<Vec<u8>>,
    list: Vec<u32>,
    filled: bool, // the C++ mHole, inverted: an unlisted row writes the table's hole record
}

fn put_le(buf: &mut [u8], off: usize, v: u32, size: usize) {
    for k in 0..size {
        buf[off + k] = (v >> (8 * k)) as u8;
    }
}

fn put32(out: &mut Vec<u8>, v: u32) {
    out.extend_from_slice(&v.to_le_bytes());
}

fn new_record(bytes: Option<&'static [u8]>, size: usize) -> Vec<u8> {
    match bytes {
        Some(b) => b.to_vec(),
        None => vec![0; size],
    }
}

/// A new record (the base bytes, or zeros) appended to `recs` -> its offset there.
fn push_record(recs: &mut Vec<u8>, bytes: Option<&'static [u8]>, size: usize) -> usize {
    let at = recs.len();
    match bytes {
        Some(b) => recs.extend_from_slice(b),
        None => recs.resize(at + size, 0),
    }
    at
}

/// "A|B[n]" -> A, B, n (B empty for "A[n]"; n 0 for a single string).
fn lead_spec(spec: &str) -> (&str, &str, usize) {
    let bar = spec.find('|');
    let br = spec.find('[');
    let n = match br {
        Some(b) => spec[b + 1..].trim_end_matches(']').parse().unwrap_or(0),
        None => 0,
    };
    let a = &spec[..bar.or(br).unwrap_or(spec.len())];
    let b = match bar {
        Some(x) => &spec[x + 1..br.unwrap_or(spec.len())],
        None => "",
    };
    (a, b, n)
}

fn lead_count(t: &Table) -> usize {
    let mut total = 0;
    for s in t.lead {
        let (_, b, n) = lead_spec(s);
        total += if n == 0 {
            1
        } else if b.is_empty() {
            n
        } else {
            2 * n
        };
    }
    total
}

fn build_lead(t: &Table, g: &mut Getter) -> Res<Vec<Vec<u8>>> {
    let mut out = Vec::with_capacity(lead_count(t));
    let w = g.where_;
    for s in t.lead {
        let (a, b, n) = lead_spec(s);
        if n == 0 {
            let v = g.get(a);
            out.push(text_value(v, w, a)?);
        } else if b.is_empty() {
            let v = g.get(a);
            let e = elems(v, n, w, a)?;
            for item in e.iter().take(n) {
                out.push(text_value(item, w, a)?);
            }
        } else {
            let va = g.get(a);
            let ea = elems(va, n, w, a)?;
            let vb = g.get(b);
            let eb = elems(vb, n, w, b)?;
            for k in 0..n {
                out.push(text_value(ea.get(k), w, a)?);
                out.push(text_value(eb.get(k), w, b)?);
            }
        }
    }
    Ok(out)
}

/// CSV columns the exporter can write for a table (tables.csv_columns).
fn csv_columns(t: &Table) -> Vec<Vec<u8>> {
    let mut cols: Vec<Vec<u8>> = vec![b"row".to_vec(), b"id".to_vec()];
    for s in t.lead {
        let (a, b, n) = lead_spec(s);
        if n == 0 {
            cols.push(a.as_bytes().to_vec());
        }
        for k in 0..n {
            cols.push(format!("{a}[{k}]").into_bytes());
        }
        if !b.is_empty() {
            for k in 0..n {
                cols.push(format!("{b}[{k}]").into_bytes());
            }
        }
    }
    for f in t.record.fields {
        if t.epoch_flags == Some(f.name) || t.levels_field == Some(f.name) {
            continue; // epochs.yaml's lists; the attribute-keyed levels
        }
        if f.ty == Type::Str || f.count == 1 {
            cols.push(f.name.as_bytes().to_vec());
        } else if f.ty == Type::PairF32 {
            for k in 0..f.count {
                for j in 0..2 {
                    cols.push(format!("{}[{k}][{j}]", f.name).into_bytes());
                }
            }
        } else {
            for k in 0..f.count {
                cols.push(format!("{}[{k}]", f.name).into_bytes());
            }
        }
    }
    cols
}

/// A CSV column name -> base and indices ("mDamage[3]" -> mDamage, {3}).
fn split_column(c: &[u8]) -> Option<(Vec<u8>, Vec<usize>)> {
    let first = c.iter().position(|&x| x == b'[');
    let base = &c[..first.unwrap_or(c.len())];
    if base.is_empty() || base.contains(&b']') {
        return None;
    }
    let mut idx = Vec::new();
    let mut b = first;
    while let Some(start) = b {
        if start >= c.len() {
            break;
        }
        let e = c[start..].iter().position(|&x| x == b']')?;
        let e = start + e;
        if c[start] != b'[' || e == start + 1 {
            return None;
        }
        let digits = &c[start + 1..e];
        if !digits.iter().all(u8::is_ascii_digit) {
            return None;
        }
        // strtoul (32-bit unsigned long on Windows): wraps rather than fails on the huge numbers no spec has
        let mut v: u64 = 0;
        for d in digits {
            v = v.saturating_mul(10).saturating_add((d - b'0') as u64);
        }
        idx.push(v.min(u32::MAX as u64) as usize);
        b = if e + 1 < c.len() { Some(e + 1) } else { None };
    }
    Some((base.to_vec(), idx))
}

/// One CSV row -> the same mapping the YAML side gives (arrays as lists with null gaps). `bases` holds `distinct`
/// different names.
fn csv_record(bases: &[Bytes], idx: &[Vec<usize>], distinct: usize, cells: &[Bytes], line: i64) -> Node {
    let mut m = Node::collection(Kind::Map, line);
    m.keys.reserve_exact(distinct);
    m.items.reserve_exact(distinct);
    for c in 0..cells.len() {
        if cells[c].is_empty() {
            continue;
        }
        let pos = m.keys.iter().position(|k| *k == bases[c]);
        let at = match pos {
            Some(p) => p,
            None => {
                m.keys.push(bases[c].clone());
                m.items.push(Node::default());
                m.keys.len() - 1
            }
        };
        // the path down the indices, made a list at each step
        let mut slot: &mut Node = &mut m.items[at];
        for &i in &idx[c] {
            slot.kind = Kind::List;
            if slot.items.len() <= i {
                slot.items.resize_with(i + 1, Node::default);
            }
            slot = &mut slot.items[i];
        }
        *slot = Node::scalar(cells[c].clone(), true, line);
    }
    m
}

/// The records `ats` of a list -> positions in file order: an explicit row must be after the previous explicit one, a
/// record without one takes the next free position above every explicit row and the count (new rows go above).
fn place_list<W: Part + ?Sized>(
    list: &Rc<Node>,
    ats: &[usize],
    lines: &[i64],
    total: i64,
    where_: &W,
) -> Res<Vec<Placed>> {
    let mut pos = vec![-1i64; ats.len()];
    let mut next = 0i64;
    for k in 0..ats.len() {
        let m = &list.items[ats[k]];
        if m.kind != Kind::Map {
            fail!(where_, " item ", k as i64, ": not a mapping");
        }
        let line = lines[k];
        let Some(v) = m.find(b"row") else { continue };
        if v.is_missing() {
            continue;
        }
        if v.kind != Kind::Scalar {
            fail!(where_, " line ", line, ": row: not a number");
        }
        pos[k] = parse_int(&v.text, Type::S32)
            .map_err(|e| Failure::new(msg!(where_, " line ", line, ": row: ", &e.text)))?;
        if pos[k] < next {
            fail!(where_, " line ", line, ": row ", pos[k], " is not after the previous row (", next - 1, ")");
        }
        next = pos[k] + 1;
    }
    let mut free = total.max(next);
    let mut out = Vec::with_capacity(ats.len());
    for k in 0..ats.len() {
        let mut p = pos[k];
        if p < 0 {
            p = free;
            free += 1;
        }
        out.push(Placed { pos: p, list: list.clone(), at: ats[k], line: lines[k] });
    }
    Ok(out)
}

fn place_records(records: Option<&Rc<Node>>, total: i64, where_: &[u8], lines: Option<&[i64]>) -> Res<Vec<Placed>> {
    let Some(records) = records else { return Ok(Vec::new()) };
    if records.kind != Kind::List {
        fail!(where_, ": must be a list");
    }
    let item_lines: Vec<i64> =
        records.items.iter().enumerate().map(|(k, item)| lines.map_or(item.line, |l| l[k])).collect();
    let ats: Vec<usize> = (0..records.items.len()).collect();
    place_list(records, &ats, &item_lines, total, where_)
}

fn head_int<W: Part + ?Sized>(v: &Node, what: &str, where_: &W) -> Res<i64> {
    if v.kind != Kind::Scalar {
        fail!(where_, ": ", what, ": not a number");
    }
    let n = parse_int(&v.text, Type::U32).map_err(|e| Failure::new(msg!(where_, ": ", what, ": ", &e.text)))?;
    // (long)(uint32_t)ParseInt: a u32 kept whole here (the C++ wraps past 2^31 on 32-bit long; such row counts fail
    // at the allocation there, and at the row count check of build below)
    Ok(n)
}

/// A mapping's "records" value taken out of it (the document is not needed after its head is read).
fn take_records(doc: &mut Node) -> Option<Node> {
    let i = doc.keys.iter().position(|k| *k == b"records")?;
    Some(std::mem::take(&mut doc.items[i]))
}

fn parse_doc(t: &Table, text_bytes: Vec<u8>) -> Res<Doc> {
    let name = t.name;
    let src: Rc<[u8]> = Rc::from(text_bytes);
    let mut head: [Option<Node>; 3] = [None, None, None];
    const HEAD: [&[u8]; 3] = [b"eedata", b"format", b"rows"];
    let mut csv_lines: Vec<i64> = Vec::new();
    let mut doc = Node::default();
    let mut csv_records = Node::collection(Kind::List, 0);
    if t.yaml {
        doc = parse_yaml_shared(&src)?;
        if doc.kind != Kind::Map {
            fail!(name, ": the document is not a mapping");
        }
        for k in &doc.keys {
            let ok = *k == b"records" || *k == b"epochs" || HEAD.iter().any(|h| k == h);
            if !ok {
                fail!(name, ": unknown top-level key ", k);
            }
        }
        for i in 0..3 {
            head[i] = doc.find(HEAD[i]).cloned();
        }
    } else {
        let csv = parse_csv_shared(&src)?;
        for key in csv.directives.keys() {
            if !HEAD.iter().any(|h| key == h) {
                fail!(name, ": unknown directive ", key);
            }
        }
        for i in 0..3 {
            if let Some(v) = csv.directives.get(HEAD[i]) {
                head[i] = Some(Node::scalar(v.clone(), true, 0));
            }
        }
        let known = csv_columns(t);
        let mut bases: Vec<Bytes> = Vec::with_capacity(csv.header.len());
        let mut idx = Vec::with_capacity(csv.header.len());
        for c in &csv.header {
            let split = if known.iter().any(|k| k.as_slice() == &c[..]) { split_column(c) } else { None };
            let Some((b, i)) = split else {
                fail!(name, ": unknown column ", c);
            };
            bases.push(Bytes::from(b));
            idx.push(i);
        }
        let distinct = (0..bases.len()).filter(|&i| !bases[..i].contains(&bases[i])).count();
        csv_records.items.reserve_exact(csv.rows.len());
        for r in 0..csv.rows.len() {
            csv_records.items.push(csv_record(&bases, &idx, distinct, &csv.rows[r], csv.row_lines[r]));
        }
        csv_lines = csv.row_lines;
    }
    if let Some(h) = &head[0] {
        if h.kind == Kind::Scalar && h.text != name.as_bytes() {
            fail!(name, ": the file says it is '", &h.text, "'");
        }
    }
    if let Some(h) = &head[1] {
        if h.kind != Kind::Scalar || h.text != FORMAT_VERSION.to_string().as_bytes() {
            let shown: Vec<u8> = if h.kind == Kind::Scalar { h.text.to_vec() } else { b"?".to_vec() };
            fail!(name, ": format ", shown, ", this loader reads format ", FORMAT_VERSION);
        }
    }
    let total =
        if present(head[2].as_ref()) { head_int(head[2].as_ref().unwrap(), "rows", name.as_bytes())? } else { 0 };
    if t.container == Container::Techtree || t.container == Container::Epochs {
        // placed together with the other text (get_layout)
        for k in &doc.keys {
            if *k == b"epochs" || *k == b"rows" {
                fail!(name, ": unknown top-level key ", k);
            }
        }
        let records = doc.find(b"records");
        if let Some(r) = records {
            if !r.is_missing() && r.kind != Kind::List {
                fail!(name, ": records must be a list");
            }
            if r.kind == Kind::List {
                for m in &r.items {
                    if m.kind != Kind::Map {
                        fail!(name, ": records must be a list of mappings");
                    }
                }
            }
        }
        let records = take_records(&mut doc).map(Rc::new);
        return Ok(Doc { records, total, rows: Vec::new() });
    }
    let records: Option<Rc<Node>> =
        if t.yaml { take_records(&mut doc).map(Rc::new) } else { Some(Rc::new(csv_records)) };
    let records = records.filter(|r| !r.is_missing());
    let rows = place_records(records.as_ref(), total, name.as_bytes(), if t.yaml { None } else { Some(&csv_lines) })?;
    Ok(Doc { records, total, rows })
}

/// Every table's parsed text, the ids of each and the references between them.
pub struct Content {
    read: Reader,
    docs: HashMap<&'static str, Rc<Doc>>,
    indexes: HashMap<&'static str, Rc<Index>>,
    layout: Option<Rc<Layout>>,
    attributes: Option<Rc<HashMap<Vec<u8>, i64>>>,
    scratch: Scratch,
}

/// Buffers a row's build reuses from row to row (its place, its key marks, a string field's bytes).
#[derive(Default)]
struct Scratch {
    place: Vec<u8>,
    used: Vec<bool>,
    text: Vec<u8>,
}

impl Content {
    pub fn new(read: Reader) -> Content {
        Content {
            read,
            docs: HashMap::new(),
            indexes: HashMap::new(),
            layout: None,
            attributes: None,
            scratch: Scratch::default(),
        }
    }

    /// A text of the data path (a table's, or a rules document's), unparsed.
    pub fn read_text(&mut self, name: &str) -> Option<Vec<u8>> {
        (self.read)(name)
    }

    fn get_doc(&mut self, t: &'static Table) -> Res<Rc<Doc>> {
        if let Some(d) = self.docs.get(t.name) {
            return Ok(d.clone());
        }
        let Some(text) = (self.read)(t.name) else {
            fail!(t.name, ": referenced, but the table has no text form here");
        };
        let d = Rc::new(parse_doc(t, text)?);
        self.docs.insert(t.name, d.clone());
        Ok(d)
    }

    /// A record's key: its position, or its key field (dbtechtree mID).
    fn key<W: Part + ?Sized>(&self, t: &Table, p: &Placed, where_: &W) -> Res<i64> {
        let Some(key_field) = t.key_field else { return Ok(p.pos) };
        let f = t.find_field(key_field).expect("the key field is in the record");
        let v = p.map().find(key_field.as_bytes());
        let bits = number(f.name, f.ty, if present(v) { v } else { None }, where_)?;
        Ok(bits as i32 as i64)
    }

    /// epochs.yaml and dbtechtree.yaml placed: every epoch (its position is its key) with its techs in its block.
    fn get_layout(&mut self) -> Res<Rc<Layout>> {
        if let Some(l) = &self.layout {
            return Ok(l.clone());
        }
        let te = table("epochs");
        let tt = table("dbtechtree");
        let de = self.get_doc(te)?;
        let dt = self.get_doc(tt)?;
        let empty = || Rc::new(Node::collection(Kind::List, 0));
        let eps = de.records.clone().filter(|e| e.kind == Kind::List).unwrap_or_else(empty);
        let nodes = dt.records.clone().filter(|n| n.kind == Kind::List).unwrap_or_else(empty);
        let mut index: HashMap<&[u8], usize> = HashMap::new();
        let mut blocks: Vec<EpochBlock> = Vec::with_capacity(eps.items.len());
        for (k, m) in eps.items.iter().enumerate() {
            let where_ = At { what: "epochs", line: m.line };
            if m.find(b"row").is_some() {
                fail!(where_, ": an epoch has no row (its place in the file is its position)");
            }
            let slots = m.find(b"slots");
            let slots = if present(slots) { head_int(slots.unwrap(), "slots", &where_)? } else { 0 };
            let id = m.find(b"id");
            if let Some(id) = id {
                if present(Some(id)) && id.kind == Kind::Scalar && !index.contains_key(&id.text[..]) {
                    index.insert(&id.text[..], k);
                }
            }
            blocks.push(EpochBlock { line: m.line, slots, nodes: Vec::new() });
        }
        let mut groups: Vec<Vec<usize>> = vec![Vec::new(); blocks.len()];
        let mut lines: Vec<Vec<i64>> = vec![Vec::new(); blocks.len()];
        for (n, m) in nodes.items.iter().enumerate() {
            let e = m.find(b"epoch");
            let found = e.filter(|e| present(Some(e)) && e.kind == Kind::Scalar).and_then(|e| index.get(&e.text[..]));
            let Some(&at) = found else {
                let shown: &[u8] = e.filter(|e| present(Some(e)) && e.kind == Kind::Scalar).map_or(&[], |e| &e.text);
                fail!("dbtechtree line ", m.line, ": epoch '", shown, "' is not an id of epochs");
            };
            groups[at].push(n);
            lines[at].push(m.line);
        }
        for k in 0..blocks.len() {
            blocks[k].nodes =
                place_list(&nodes, &groups[k], &lines[k], blocks[k].slots, &msg!("dbtechtree epoch ", k))?;
        }
        drop(index);
        let rc = Rc::new(Layout { epochs: eps, blocks });
        self.layout = Some(rc.clone());
        Ok(rc)
    }

    /// A tech's key: its mID, or (epoch + 1) * 1000 + slot.
    fn node_key<W: Part + ?Sized>(&self, p: &Placed, epoch: i64, where_: &W) -> Res<i64> {
        let v = p.map().find(b"mID");
        if !present(v) {
            return Ok((epoch + 1) * 1000 + p.pos);
        }
        let f = table("dbtechtree").find_field("mID").expect("mID is in the record");
        Ok(number(f.name, f.ty, v, where_)? as i32 as i64)
    }

    /// The ids of a table, indexed on first use (the id rules are checked here).
    pub fn get_index(&mut self, t: &'static Table) -> Res<Rc<Index>> {
        if let Some(i) = self.indexes.get(t.name) {
            return Ok(i.clone());
        }
        let mut placed: Vec<Placed> = Vec::new();
        let mut keys: Vec<i64> = Vec::new();
        let mut files: Vec<&str> = Vec::new();
        let doc: Rc<Doc>;
        let all: &[Placed] = if t.container == Container::Techtree || t.container == Container::Epochs {
            let layout = self.get_layout()?;
            for (k, block) in layout.blocks.iter().enumerate() {
                placed.push(Placed { pos: k as i64, list: layout.epochs.clone(), at: k, line: block.line });
                keys.push(k as i64);
                files.push("epochs");
            }
            if t.container == Container::Techtree {
                for (k, block) in layout.blocks.iter().enumerate() {
                    for p in &block.nodes {
                        placed.push(p.clone());
                        keys.push(self.node_key(p, k as i64, &At { what: "dbtechtree", line: p.line })?);
                        files.push("dbtechtree");
                    }
                }
            }
            &placed
        } else {
            doc = self.get_doc(t)?;
            keys.reserve_exact(doc.rows.len());
            for p in &doc.rows {
                keys.push(self.key(t, p, &At { what: t.name, line: p.line })?);
                files.push(t.name);
            }
            &doc.rows
        };
        let mut idx = Index::default();
        idx.keys.reserve(all.len());
        for (n, p) in all.iter().enumerate() {
            let where_ = At { what: files[n], line: p.line };
            let key = keys[n];
            let id = match p.map().find(b"id") {
                Some(v) if present(Some(v)) && v.kind == Kind::Scalar => &v.text,
                _ => {
                    fail!(where_, ": no id");
                }
            };
            if !text::is_id(id) {
                fail!(where_, ": id '", id, "': an id is a lowercase letter, then a-z, 0-9 or _");
            }
            if idx.keys.contains_key(&id[..]) {
                // the first row with this id (the earlier rows all passed)
                let first = (0..n).find(|&m| all[m].map().find(b"id").is_some_and(|v| v.text == *id));
                let also = first.map_or(At { what: "", line: 0 }, |m| At { what: files[m], line: all[m].line });
                fail!(where_, ": duplicate id '", id, "' (also ", also, ")");
            }
            if let Some(other) = idx.ids.get(&key) {
                fail!(where_, ": id '", id, "': key ", key, " is also '", &other[..], "'");
            }
            for rn in t.row_names {
                if rn.row as i64 == key && *id != rn.name.as_bytes() {
                    fail!(where_, ": id '", id, "' does not match row ", key, " ('", rn.name, "')");
                }
            }
            let shared: Rc<[u8]> = Rc::from(&id[..]);
            idx.keys.insert(shared.clone(), key);
            idx.ids.insert(key, shared);
        }
        let rc = Rc::new(idx);
        self.indexes.insert(t.name, rc.clone());
        Ok(rc)
    }

    /// A reference cell -> the integer the record holds.
    fn resolve<W: Part + ?Sized>(&mut self, r: &Ref, ty: Type, v: Option<&Node>, where_: &W, field: &str) -> Res<u32> {
        let Some(v) = v else { return Ok(0) };
        if v.kind != Kind::Scalar {
            fail!(where_, ": ", field, ": a list where a reference belongs");
        }
        let target = table(r.table);
        if r.never && v.text == b"never" {
            return Ok(self.get_index(target)?.keys.len() as u32); // the target's count: no row
        }
        if text::is_id(&v.text) {
            let idx = self.get_index(target)?;
            let Some(&key) = idx.keys.get(&v.text[..]) else {
                fail!(where_, ": ", field, ": '", &v.text, "' is not an id of ", r.table);
            };
            let (lo, hi) = int_limits(ty);
            if key < lo || key > hi {
                fail!(where_, ": ", field, ": '", &v.text, "' is key ", key, ", out of range");
            }
            return Ok(key as u32);
        }
        let n = parse_int(&v.text, ty).map_err(|e| Failure::new(msg!(where_, ": ", field, ": ", &e.text)))?;
        let none = r.none.iter().any(|&x| x as i64 == n);
        let idx = self.get_index(target)?;
        if !none && (-2147483648..=2147483647).contains(&n) && idx.ids.contains_key(&n) {
            fail!(where_, ": ", field, ": ", &v.text, " names a row of ", r.table, ": write its id");
        }
        Ok(n as u32)
    }

    fn build_list(&mut self, t: &'static Table, g: &mut Getter) -> Res<Vec<u32>> {
        let name = t.list_name.expect("the table has a list");
        let v = g.get(name);
        let Some(v) = v else { return Ok(Vec::new()) };
        let w = g.where_;
        if v.kind != Kind::List {
            fail!(w, ": ", name, " must be a list");
        }
        let mut out = Vec::with_capacity(v.items.len());
        let r = t.find_ref("@list");
        let ty = if t.list_signed { Type::S32 } else { Type::U32 };
        for x in &v.items {
            if x.kind != Kind::Scalar {
                fail!(w, ": ", name, ": a list where a number belongs");
            }
            if let Some(r) = r {
                out.push(self.resolve(r, ty, Some(x), w, name)?);
            } else {
                out.push(parse_int(&x.text, ty).map_err(|e| Failure::new(msg!(w, ": ", name, ": ", &e.text)))? as u32);
            }
        }
        Ok(out)
    }

    fn attribute_levels(&mut self) -> Res<Rc<HashMap<Vec<u8>, i64>>> {
        if let Some(a) = &self.attributes {
            return Ok(a.clone());
        }
        let mut map: HashMap<Vec<u8>, i64> = HashMap::new();
        if let Some(text) = (self.read)("civbuilder") {
            let d = parse_yaml_shared(&Rc::from(text))?;
            if let Some(list) = d.find(b"attributes").filter(|l| l.kind == Kind::List) {
                for a in &list.items {
                    let id = a.find(b"id").filter(|i| present(Some(i)) && i.kind == Kind::Scalar);
                    let level = a.find(b"level");
                    if let (Some(id), Some(level)) = (id, level) {
                        if present(Some(level)) {
                            let n = text::node_int(Some(level), &At { what: "civbuilder", line: a.line }, "level")?;
                            map.insert(id.text.to_vec(), n);
                        }
                    }
                }
            }
        } else {
            for a in crate::generated::rules_gen::default_attributes() {
                map.insert(a.id.clone(), a.level as i64);
            }
        }
        let rc = Rc::new(map);
        self.attributes = Some(rc.clone());
        Ok(rc)
    }

    /// The attribute-keyed levels -> the record's level pairs; levels past the field's count are the game's alone.
    fn levels(&mut self, t: &'static Table, f: &Field, g: &mut Getter, buf: &mut [u8]) -> Res<()> {
        let key = t.levels_key.expect("the table has a levels key");
        let v = g.get(key);
        let Some(v) = v else { return Ok(()) };
        let w = g.where_;
        if v.kind != Kind::List {
            fail!(w, ": ", key, " must be a list");
        }
        let levels = self.attribute_levels()?;
        for x in &v.items {
            if x.kind != Kind::Map {
                fail!(w, ": ", key, ": a level is a mapping (attribute, value, bonus)");
            }
            for k in &x.keys {
                if *k != b"attribute" && *k != b"level" && *k != b"value" && *k != b"bonus" {
                    fail!(w, ": ", key, ": unknown key '", k, "'");
                }
            }
            let a = x.find(b"attribute");
            let l = x.find(b"level");
            if present(a) == present(l) {
                fail!(w, ": ", key, ": name the attribute (or a level without one)");
            }
            let lv: i64 = if present(a) {
                let id = scalar_text(a.unwrap(), w, key, "an attribute id")?;
                match levels.get(id) {
                    Some(n) => *n,
                    None => {
                        fail!(w, ": ", key, ": attribute '", id, "' is not an attribute of civbuilder");
                    }
                }
            } else {
                let lv = number("level", Type::S32, l, w)? as i32 as i64;
                if lv < 1 {
                    fail!(w, ": ", key, ": level must be at least 1");
                }
                lv
            };
            if lv > f.count as i64 {
                continue;
            }
            let off = f.offset as usize + 8 * (lv as usize - 1);
            let vv = x.find(b"value");
            let bv = x.find(b"bonus");
            put_le(buf, off, number("value", Type::F32, if present(vv) { vv } else { None }, w)?, 4);
            put_le(buf, off + 4, number("bonus", Type::F32, if present(bv) { bv } else { None }, w)?, 4);
        }
        Ok(())
    }

    fn build_record(
        &mut self,
        t: &'static Table,
        g: &mut Getter,
        has_key: bool,
        key: i64,
        recs: &mut Vec<u8>,
    ) -> Res<usize> {
        let st = t.record;
        let at = push_record(recs, st.base, st.size as usize);
        let buf = &mut recs[at..at + st.size as usize];
        let w = g.where_;
        let mut text = std::mem::take(&mut self.scratch.text);
        for f in st.fields {
            if t.epoch_flags == Some(f.name) {
                continue; // set from epochs.yaml's lists (epoch_flags)
            }
            if t.levels_field == Some(f.name) {
                self.levels(t, f, g, buf)?; // the attribute-keyed levels
                continue;
            }
            let r = t.find_ref(f.name);
            let off = f.offset as usize;
            if f.ty == Type::Str {
                let v = g.get(f.name);
                text_value_into(v, w, f.name, &mut text)?;
                if text.len() >= f.count as usize {
                    fail!(
                        w,
                        ": ",
                        f.name,
                        ": ",
                        text.len(),
                        " bytes, the field holds ",
                        f.count as i64 - 1,
                        " (+ NUL)"
                    );
                }
                buf[off..off + f.count as usize].fill(0);
                buf[off..off + text.len()].copy_from_slice(&text);
            } else if f.ty == Type::PairF32 {
                let v = g.get(f.name);
                let e = elems(v, f.count as usize, w, f.name)?;
                for k in 0..f.count as usize {
                    let p = elems(e.get(k), 2, w, f.name)?;
                    put_le(buf, off + 8 * k, number(f.name, Type::F32, p.get(0), w)?, 4);
                    put_le(buf, off + 8 * k + 4, number(f.name, Type::F32, p.get(1), w)?, 4);
                }
            } else {
                let size = f.ty.size();
                if f.count == 1 {
                    let v = g.get(f.name);
                    if let Some(n) = v {
                        if n.kind == Kind::List {
                            fail!(w, ": ", f.name, ": a list where a number belongs");
                        }
                    }
                    let x = if v.is_none() && has_key && t.self_field == Some(f.name) {
                        (key + t.self_offset as i64) as u32
                    } else if let Some(r) = r {
                        self.resolve(r, f.ty, v, w, f.name)?
                    } else {
                        number(f.name, f.ty, v, w)?
                    };
                    put_le(buf, off, x, size);
                } else {
                    let v = g.get(f.name);
                    let e = elems(v, f.count as usize, w, f.name)?;
                    for k in 0..f.count as usize {
                        let item = e.get(k);
                        if let Some(n) = item {
                            if n.kind == Kind::List {
                                fail!(w, ": ", f.name, ": a list where a number belongs");
                            }
                        }
                        let x = if let Some(r) = r {
                            self.resolve(r, f.ty, item, w, f.name)?
                        } else {
                            number(f.name, f.ty, item, w)?
                        };
                        put_le(buf, off + size * k, x, size);
                    }
                }
            }
        }
        self.scratch.text = text;
        Ok(at)
    }

    fn build_row(
        &mut self,
        t: &'static Table,
        p: &Placed,
        has_key: bool,
        text_keys: &[&str],
        recs: &mut Vec<u8>,
    ) -> Res<Row> {
        let m = p.map();
        let mut where_ = std::mem::take(&mut self.scratch.place);
        where_.clear();
        At { what: if t.container == Container::Epochs { "epochs" } else { t.name }, line: p.line }.put(&mut where_);
        if let Some(id) = m.find(b"id") {
            if present(Some(id)) && id.kind == Kind::Scalar {
                where_.extend_from_slice(b" (id ");
                where_.extend_from_slice(&id.text);
                where_.push(b')');
            }
        }
        let mut g = Getter::reusing(m, &where_, std::mem::take(&mut self.scratch.used));
        g.get("row");
        g.get("id");
        for k in text_keys {
            g.get(k);
        }
        let mut r = Row { filled: true, ..Row::default() };
        r.rec = self.build_record(t, &mut g, has_key, p.pos, recs)?;
        if !t.lead.is_empty() {
            r.lead = build_lead(t, &mut g)?;
        }
        if t.list_name.is_some() {
            r.list = self.build_list(t, &mut g)?;
        }
        g.leftover()?;
        self.scratch.used = g.into_used();
        self.scratch.place = where_;
        Ok(r)
    }

    /// A per-epoch flag array (dbmusic mInEpoch) from epochs.yaml's lists.
    fn epoch_flags(&mut self, t: &'static Table, rows: &[Row], listed: &[bool], recs: &mut [u8]) -> Res<()> {
        let f = t.find_field(t.epoch_flags.expect("flags")).expect("the field is in the record");
        let key = t.epoch_key.expect("the epoch key");
        let layout = self.get_layout()?;
        let self_ref = Ref { field: key, table: t.name, none: &[], never: false };
        for k in 0..layout.blocks.len().min(f.count as usize) {
            let v = layout.epoch(k).find(key.as_bytes());
            let where_ = At { what: "epochs", line: layout.blocks[k].line };
            let Some(v) = v.filter(|v| present(Some(v))) else { continue };
            if v.kind != Kind::List {
                fail!(where_, ": ", key, " must be a list");
            }
            for x in &v.items {
                let pos = self.resolve(&self_ref, Type::S32, Some(x), &where_, key)? as i32 as i64;
                if pos < 0 || pos as usize >= rows.len() || !listed[pos as usize] {
                    fail!(where_, ": ", key, ": '", &x.text, "' names no row of ", t.name);
                }
                recs[rows[pos as usize].rec + f.offset as usize + k] = 1;
            }
        }
        Ok(())
    }

    fn write_record(rec_or_hole: &Row, hole: &[u8], recs: &[u8], out: &mut Vec<u8>) {
        let size = hole.len();
        out.extend_from_slice(if rec_or_hole.filled { &recs[rec_or_hole.rec..rec_or_hole.rec + size] } else { hole });
    }

    fn write_list(r: &Row, out: &mut Vec<u8>) {
        put32(out, r.list.len() as u32);
        for v in &r.list {
            put32(out, *v);
        }
    }

    fn write_lead(r: &Row, count: usize, out: &mut Vec<u8>) {
        for k in 0..count {
            if r.filled {
                put32(out, r.lead[k].len() as u32);
                out.extend_from_slice(&r.lead[k]);
            } else {
                put32(out, 0);
            }
        }
    }

    /// Row storage for n rows, or a failure as the C++ vector's allocation failure (`Run` reports it the same way).
    fn row_slots(name: &str, n: i64) -> Res<Vec<Row>> {
        let mut rows: Vec<Row> = Vec::new();
        if n < 0 || rows.try_reserve_exact(n as usize).is_err() {
            fail!(name, ": bad allocation");
        }
        rows.resize_with(n as usize, Row::default);
        Ok(rows)
    }

    /// Compiles a table's text into its db file bytes; the first problem is returned with its place.
    pub fn compile(&mut self, t: &'static Table) -> Res<Vec<u8>> {
        if t.text_only {
            fail!(t.name, ": a text-only table (it compiles into dbtechtree)");
        }
        self.get_index(t)?; // the ids are checked even where nothing references the table
        let hole = new_record(t.hole, t.record.size as usize);
        let leads = lead_count(t);
        let mut out: Vec<u8> = Vec::new();
        if t.container == Container::Techtree {
            let te = table("epochs");
            self.get_index(te)?;
            const EPOCH_KEYS: [&str; 3] = ["slots", "music", "citizen"];
            const NODE_KEYS: [&str; 1] = ["epoch"];
            let the_id = t.find_field("mID").expect("mID");
            let layout = self.get_layout()?;
            let mut recs: Vec<u8> = Vec::new();
            for (k, b) in layout.blocks.iter().enumerate() {
                let k64 = k as i64;
                let at = Placed { pos: k64, list: layout.epochs.clone(), at: k, line: b.line };
                recs.clear();
                let _ = recs.try_reserve(t.record.size as usize * (b.nodes.len() + 1));
                let ep = self.build_row(te, &at, false, &EPOCH_KEYS, &mut recs)?;
                for dv in te.derived {
                    if !present(layout.epoch(k).find(dv.field.as_bytes())) {
                        let f = te.find_field(dv.field).expect("derived field");
                        put_le(&mut recs[ep.rec..], f.offset as usize, (dv.a as i64 * k64 + dv.b as i64) as u32, 4);
                    }
                }
                let mut n = b.slots;
                for p in &b.nodes {
                    n = n.max(p.pos + 1);
                }
                let mut rows = Content::row_slots(t.name, n)?;
                for p in &b.nodes {
                    let r = self.build_row(t, p, false, &NODE_KEYS, &mut recs)?;
                    if !present(p.map().find(b"mID")) {
                        put_le(&mut recs[r.rec..], the_id.offset as usize, ((k64 + 1) * 1000 + p.pos) as u32, 4);
                    }
                    rows[p.pos as usize] = r;
                }
                let _ = out.try_reserve(8 + (rows.len() + 1) * (t.record.size as usize + 8));
                put32(&mut out, rows.len() as u32);
                Content::write_record(&ep, &hole, &recs, &mut out);
                Content::write_list(&ep, &mut out);
                for r in &rows {
                    Content::write_record(r, &hole, &recs, &mut out);
                    Content::write_list(r, &mut out);
                }
            }
            self.docs.remove("epochs");
            self.layout = None;
        } else {
            let d = self.get_doc(t)?;
            let mut n = d.total;
            for p in &d.rows {
                n = n.max(p.pos + 1);
            }
            let mut rows = Content::row_slots(t.name, n)?;
            let mut listed = vec![false; n as usize];
            let mut recs: Vec<u8> = Vec::new();
            let _ = recs.try_reserve(t.record.size as usize * d.rows.len());
            for p in &d.rows {
                rows[p.pos as usize] = self.build_row(t, p, true, &[], &mut recs)?;
                listed[p.pos as usize] = true;
            }
            if t.epoch_flags.is_some() {
                self.epoch_flags(t, &rows, &listed, &mut recs)?;
            }
            let _ = out.try_reserve(4 + rows.len() * t.record.size as usize);
            if t.container == Container::Count || t.container == Container::Lead {
                put32(&mut out, rows.len() as u32);
            }
            for r in &rows {
                if t.container == Container::Lead {
                    Content::write_lead(r, leads, &mut out);
                }
                Content::write_record(r, &hole, &recs, &mut out);
                if t.container == Container::Events {
                    Content::write_list(r, &mut out);
                }
            }
        }
        // the parsed text is no longer needed once the table is built (its ids stay indexed)
        self.docs.remove(t.name);
        Ok(out)
    }

    /// A document of the data path parsed: a rules document, or a db table's text read for its text-only keys.
    /// None when the reader has no such text; the rules overrides are applied (EE-SIM --set).
    pub fn document(&mut self, name: &str) -> Res<Option<Node>> {
        let Some(text) = (self.read)(name) else { return Ok(None) };
        let mut doc = parse_yaml_shared(&Rc::from(text)).map_err(|e| Failure::new(msg!(name, " ", &e.text)))?;
        if doc.kind != Kind::Map {
            fail!(name, ": the document is not a mapping");
        }
        if let Some(n) = doc.find(b"eedata").filter(|n| !n.is_missing()) {
            if n.kind != Kind::Scalar || n.text != name.as_bytes() {
                let shown: &[u8] = if n.kind == Kind::Scalar { &n.text } else { &[] };
                fail!(name, ": the file says it is '", shown, "'");
            }
        }
        if let Some(f) = doc.find(b"format").filter(|n| !n.is_missing()) {
            if f.kind != Kind::Scalar || f.text != FORMAT_VERSION.to_string().as_bytes() {
                let shown: &[u8] = if f.kind == Kind::Scalar { &f.text } else { &[] };
                fail!(name, ": format ", shown, ", the game reads format ", FORMAT_VERSION);
            }
        }
        crate::rules::apply_overrides(name, &mut doc);
        Ok(Some(doc))
    }

    /// The key of the row of a table that the node names (its row position; dbtechtree and epochs: the tech id).
    pub fn key_of<W: Part + ?Sized>(
        &mut self,
        table_name: &str,
        id: Option<&Node>,
        where_: &W,
        field: &str,
    ) -> Res<i64> {
        let Some(t) = find_table(table_name.as_bytes()) else {
            fail!(where_, ": ", field, ": ", table_name, " is not a db table");
        };
        let text = text::node_scalar(id, where_, field)?;
        if !text::is_id(text) {
            fail!(where_, ": ", field, ": '", text, "' is not an id (a lowercase letter, then a-z, 0-9 or _)");
        }
        let idx = self.get_index(t)?;
        match idx.keys.get(text) {
            Some(k) => Ok(*k),
            None => {
                fail!(where_, ": ", field, ": '", text, "' is not an id of ", table_name);
            }
        }
    }

    /// The number of listed rows of a table (epochs: the epoch count).
    pub fn count(&mut self, table_name: &str) -> Res<i64> {
        let Some(t) = find_table(table_name.as_bytes()) else {
            fail!(table_name, " is not a db table");
        };
        Ok(self.get_index(t)?.keys.len() as i64)
    }

    /// The ids of a table's listed rows by key (`Index::ids`).
    pub fn ids(&mut self, table_name: &str) -> Res<Rc<Index>> {
        let Some(t) = find_table(table_name.as_bytes()) else {
            fail!(table_name, ": not a db table with a text form");
        };
        self.get_index(t)
    }
}

fn int_limits(t: Type) -> (i64, i64) {
    match t {
        Type::S8 => (-128, 127),
        Type::U8 | Type::Bool => (0, 255),
        Type::S16 => (-32768, 32767),
        Type::U16 => (0, 65535),
        Type::S32 => (-2147483648, 2147483647),
        _ => (0, 4294967295),
    }
}

/// One table on its own (no other table's text: a reference by id to another table is an error).
pub fn compile_table(table_name: &[u8], text: &[u8]) -> Res<Vec<u8>> {
    let Some(t) = find_table(table_name) else {
        fail!(table_name, ": not a db table with a text form");
    };
    let owned = text.to_vec();
    let name = t.name;
    let mut c = Content::new(Box::new(move |n: &str| if n == name { Some(owned.clone()) } else { None }));
    c.compile(t)
}

/// The text file extension of a db table (or a rules document).
pub fn text_extension(name: &[u8]) -> Option<&'static str> {
    if let Some(t) = find_table(name) {
        return Some(if t.yaml { ".yaml" } else { ".csv" });
    }
    if crate::generated::rules_gen::DOCUMENTS.iter().any(|d| d.as_bytes() == name) {
        return Some(".yaml");
    }
    None
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::generated::specs_gen::CODE_PAGE;
    use std::collections::HashMap as Map;

    fn content_of(files: &[(&str, &str)]) -> Content {
        let map: Map<String, Vec<u8>> = files.iter().map(|(n, t)| (n.to_string(), t.as_bytes().to_vec())).collect();
        Content::new(Box::new(move |n: &str| map.get(n).cloned()))
    }

    fn err_text(r: Res<Vec<u8>>) -> String {
        String::from_utf8_lossy(&r.unwrap_err().text).into_owned()
    }

    // dbworld: 24-byte records { i32 mLongValue? ... } are synthetic here: rows are built from the spec's fields, so
    // these tests pin the compiler's shape, not retail values.
    fn dbworld_csv(rows: &str) -> String {
        format!("#eedata dbworld\n#format 3\nrow,id,mFloatValue,mFloatValue2,mLongValue,mLongValue2\n{rows}")
    }

    #[test]
    fn compile_single_csv_table_gives_the_records() {
        let text = dbworld_csv("0,gatherers_per_resource_max,1.5,0,7,2\n");
        let t = table("dbworld");
        let mut c = content_of(&[("dbworld", &text)]);
        // row 0's id must match the spec's row name when the spec names it
        let out = c.compile(t);
        // rows without a spec name are allowed; row 0 has no required name in dbworld's row_names unless listed
        let bytes = match out {
            Ok(b) => b,
            Err(e) => panic!("{}", String::from_utf8_lossy(&e.text)),
        };
        assert_eq!(bytes.len(), 4 + t.record.size as usize);
        let rec = &bytes[4..4 + t.record.size as usize];
        let f = t.find_field("mFloatValue").unwrap();
        let v = u32::from_le_bytes(rec[f.offset as usize..f.offset as usize + 4].try_into().unwrap());
        assert_eq!(v, 1.5f32.to_bits());
        let f = t.find_field("mLongValue").unwrap();
        let v = i32::from_le_bytes(rec[f.offset as usize..f.offset as usize + 4].try_into().unwrap());
        assert_eq!(v, 7);
    }

    #[test]
    fn duplicate_ids_are_rejected() {
        let text = dbworld_csv("0,a_b,1,0,0,0\n1,a_b,1,0,0,0\n");
        let mut c = content_of(&[("dbworld", &text)]);
        assert!(err_text(c.compile(table("dbworld"))).contains("duplicate id 'a_b'"));
    }

    #[test]
    fn missing_id_is_rejected() {
        let text = dbworld_csv("0,,1,0,0,0\n");
        let mut c = content_of(&[("dbworld", &text)]);
        assert!(err_text(c.compile(table("dbworld"))).contains("no id"));
    }

    #[test]
    fn bad_rows_are_rejected_with_their_place() {
        let text = dbworld_csv("5,x,1,0,0,0\n3,y,1,0,0,0\n");
        let mut c = content_of(&[("dbworld", &text)]);
        assert!(err_text(c.compile(table("dbworld"))).contains("row 3 is not after the previous row (5)"));
    }

    #[test]
    fn wrong_table_name_and_format_are_errors() {
        let mut c = content_of(&[("dbworld", "#eedata dbsounds\n#format 3\nrow,id,mFloatValue\n")]);
        assert!(err_text(c.compile(table("dbworld"))).contains("the file says it is 'dbsounds'"));
        let mut c = content_of(&[("dbworld", "#format 2\nrow,id\n")]);
        assert!(err_text(c.compile(table("dbworld"))).contains("this loader reads format 3"));
    }

    #[test]
    fn unknown_column_and_directive_are_errors() {
        let mut c = content_of(&[("dbworld", "#colour blue\nrow,id\n")]);
        assert!(err_text(c.compile(table("dbworld"))).contains("unknown directive colour"));
        let mut c = content_of(&[("dbworld", "row,id,nosuch\n0,x,1\n")]);
        assert!(err_text(c.compile(table("dbworld"))).contains("unknown column nosuch"));
    }

    #[test]
    fn missing_table_text_is_reported_when_referenced() {
        let mut c = content_of(&[]);
        let err = err_text(c.compile(table("dbworld")));
        assert!(err.contains("referenced, but the table has no text form here"), "{err}");
    }

    #[test]
    fn text_only_table_cannot_be_compiled_on_its_own() {
        let mut c = content_of(&[]);
        assert!(err_text(c.compile(table("epochs"))).contains("a text-only table"));
    }

    #[test]
    fn huge_row_counts_fail_as_allocation_failures() {
        let text = "#rows 4294967295\nrow,id\n";
        let mut c = content_of(&[("dbworld", text)]);
        assert!(err_text(c.compile(table("dbworld"))).contains("bad allocation"));
    }

    #[test]
    fn hole_rows_and_gaps_are_written() {
        let text = dbworld_csv("2,two,1,0,0,0\n");
        let t = table("dbworld");
        let mut c = content_of(&[("dbworld", &text)]);
        let bytes = c.compile(t).unwrap();
        let size = t.record.size as usize;
        assert_eq!(bytes.len(), 4 + 3 * size); // dbworld: a count, then the rows
                                               // an unlisted row carries the table's hole record (zeros here unless the spec says otherwise)
        let hole = new_record(t.hole, size);
        assert_eq!(&bytes[4..4 + size], hole.as_slice());
    }

    #[test]
    fn ids_and_counts_and_keys() {
        let text = dbworld_csv("0,alpha,1,0,0,0\n1,beta,1,0,0,0\n");
        let mut c = content_of(&[("dbworld", &text)]);
        assert_eq!(c.count("dbworld").unwrap(), 2);
        let ids = c.ids("dbworld").unwrap();
        assert_eq!(&*ids.ids[&1], b"beta");
        let id = Node::scalar(b"alpha".to_vec(), false, 1);
        assert_eq!(c.key_of("dbworld", Some(&id), b"w", "f").unwrap(), 0);
        let bad = Node::scalar(b"zeta".to_vec(), false, 1);
        assert!(String::from_utf8_lossy(&c.key_of("dbworld", Some(&bad), b"w", "f").unwrap_err().text)
            .contains("is not an id of dbworld"));
        let not_id = Node::scalar(b"Bad".to_vec(), false, 1);
        assert!(String::from_utf8_lossy(&c.key_of("dbworld", Some(&not_id), b"w", "f").unwrap_err().text)
            .contains("is not an id (a lowercase letter"));
        assert!(c.ids("nosuch").is_err());
        assert!(c.count("nosuch").is_err());
    }

    #[test]
    fn extension_of_tables_and_documents() {
        assert_eq!(text_extension(b"dbWorld"), Some(".csv"));
        assert_eq!(text_extension(b"combat"), Some(".yaml"));
        assert_eq!(text_extension(b"nope"), None);
        assert_eq!(CODE_PAGE[0x80], 0x20ac);
    }

    #[test]
    fn compile_table_alone() {
        let text = dbworld_csv("0,solo,2,0,0,0\n");
        assert!(compile_table(b"dbworld", text.as_bytes()).is_ok());
        assert!(compile_table(b"nosuch", b"").unwrap_err().text.starts_with(b"nosuch: not a db table"));
    }

    #[test]
    fn lead_specs() {
        assert_eq!(lead_spec("Name"), ("Name", "", 0));
        assert_eq!(lead_spec("Name[4]"), ("Name", "", 4));
        assert_eq!(lead_spec("A|B[8]"), ("A", "B", 8));
    }

    #[test]
    fn column_names() {
        assert_eq!(split_column(b"mDamage[3]").unwrap(), (b"mDamage".to_vec(), vec![3]));
        assert_eq!(split_column(b"mPair[1][0]").unwrap(), (b"mPair".to_vec(), vec![1, 0]));
        assert_eq!(split_column(b"plain").unwrap(), (b"plain".to_vec(), vec![]));
        assert!(split_column(b"x[]").is_none());
        assert!(split_column(b"x[a]").is_none());
        assert!(split_column(b"[1]").is_none());
    }
}
