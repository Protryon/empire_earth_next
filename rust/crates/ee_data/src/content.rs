//! The db tables' text compiler: every table's text compiled into the binary db file bytes the loaders read, resolving
//! the references between tables by id (tools/eedata tables.Content; the C++ `EEDataText.cpp` before it). The single
//! place where tables meet: a later mod layer feeds it merged entries instead of files.

use crate::msg;
use crate::node::{present, Kind, Node};
use crate::spec::{find_table, table, Container, Field, Ref, Table, Type};
use crate::text::{
    self, elems, number, parse_csv, parse_int, parse_yaml, scalar_text, text_value, Failure, Getter, Res,
};
use crate::{fail, generated::specs_gen::FORMAT_VERSION};
use std::collections::{BTreeMap, HashMap};
use std::rc::Rc;

/// A table's or rules document's text, by name; None when there is no such text.
pub type Reader = Box<dyn FnMut(&str) -> Option<Vec<u8>>>;

/// A listed record: its position (the row's key, or its slot in an epoch block), its mapping and line.
#[derive(Clone)]
struct Placed {
    pos: i64,
    map: Rc<Node>,
    line: i64,
}

/// One epoch of the tech tree: its epochs.yaml entry and the dbtechtree.yaml techs placed in its block's slots.
struct EpochBlock {
    epoch: Rc<Node>,
    line: i64,
    slots: i64,
    nodes: Vec<Placed>,
}

/// A table's text, parsed once: the records placed at their positions (dbtechtree and epochs: placed together).
struct Doc {
    doc: Rc<Node>,
    total: i64,
    rows: Vec<Placed>,
}

/// The ids of a table: id -> key, and which keys are listed.
#[derive(Default)]
pub struct Index {
    pub keys: HashMap<Vec<u8>, i64>,
    pub ids: BTreeMap<i64, Vec<u8>>,
}

#[derive(Clone, Default)]
struct Row {
    rec: Vec<u8>,
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
    let mut out = Vec::new();
    for s in t.lead {
        let (a, b, n) = lead_spec(s);
        let w = g.where_.clone();
        if n == 0 {
            let v = g.get(a);
            out.push(text_value(v, &w, a)?);
        } else if b.is_empty() {
            let v = g.get(a);
            let e = elems(v, n, &w, a)?;
            for item in e.iter().take(n) {
                out.push(text_value(*item, &w, a)?);
            }
        } else {
            let va = g.get(a);
            let ea = elems(va, n, &w, a)?;
            let vb = g.get(b);
            let eb = elems(vb, n, &w, b)?;
            for k in 0..n {
                out.push(text_value(ea[k], &w, a)?);
                out.push(text_value(eb[k], &w, b)?);
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

/// One CSV row -> the same mapping the YAML side gives (arrays as lists with null gaps).
fn csv_record(bases: &[Vec<u8>], idx: &[Vec<usize>], cells: &[Vec<u8>], line: i64) -> Node {
    let mut m = Node::collection(Kind::Map, line);
    for c in 0..cells.len() {
        if cells[c].is_empty() {
            continue;
        }
        let pos = m.keys.iter().position(|k| k == &bases[c]);
        let at = match pos {
            Some(p) => p,
            None => {
                m.keys.push(bases[c].clone());
                m.items.push(Rc::new(Node::default()));
                m.keys.len() - 1
            }
        };
        // the path down the indices, made a list at each step
        let mut slot: &mut Rc<Node> = &mut m.items[at];
        for &i in &idx[c] {
            let node = Rc::get_mut(slot).expect("a CSV record is built before it is shared");
            node.kind = Kind::List;
            if node.items.len() <= i {
                node.items.resize_with(i + 1, || Rc::new(Node::default()));
            }
            slot = &mut Rc::get_mut(slot).expect("not shared").items[i];
        }
        *slot = Rc::new(Node::scalar(cells[c].clone(), true, line));
    }
    m
}

/// The records of a list -> positions in file order: an explicit row must be after the previous explicit one, a
/// record without one takes the next free position above every explicit row and the count (new rows go above).
fn place_list(items: &[Rc<Node>], lines: &[i64], total: i64, where_: &[u8]) -> Res<Vec<Placed>> {
    let mut pos = vec![-1i64; items.len()];
    let mut next = 0i64;
    for k in 0..items.len() {
        let m = &items[k];
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
    let mut out = Vec::with_capacity(items.len());
    for k in 0..items.len() {
        let mut p = pos[k];
        if p < 0 {
            p = free;
            free += 1;
        }
        out.push(Placed { pos: p, map: items[k].clone(), line: lines[k] });
    }
    Ok(out)
}

fn place_records(records: Option<&Node>, total: i64, where_: &[u8], lines: Option<&[i64]>) -> Res<Vec<Placed>> {
    let Some(records) = records else { return Ok(Vec::new()) };
    if records.kind != Kind::List {
        fail!(where_, ": must be a list");
    }
    let item_lines: Vec<i64> =
        records.items.iter().enumerate().map(|(k, item)| lines.map_or(item.line, |l| l[k])).collect();
    place_list(&records.items, &item_lines, total, where_)
}

fn head_int(v: &Node, what: &str, where_: &[u8]) -> Res<i64> {
    if v.kind != Kind::Scalar {
        fail!(where_, ": ", what, ": not a number");
    }
    let n = parse_int(&v.text, Type::U32).map_err(|e| Failure::new(msg!(where_, ": ", what, ": ", &e.text)))?;
    // (long)(uint32_t)ParseInt: a u32 kept whole here (the C++ wraps past 2^31 on 32-bit long; such row counts fail
    // at the allocation there, and at the row count check of build below)
    Ok(n)
}

fn parse_doc(t: &Table, text_bytes: &[u8]) -> Res<Doc> {
    let name = t.name;
    let mut head: [Option<Rc<Node>>; 3] = [None, None, None];
    const HEAD: [&[u8]; 3] = [b"eedata", b"format", b"rows"];
    let mut csv_lines: Vec<i64> = Vec::new();
    let mut doc = Node::default();
    let mut csv_records = Node::collection(Kind::List, 0);
    if t.yaml {
        doc = parse_yaml(text_bytes)?;
        if doc.kind != Kind::Map {
            fail!(name, ": the document is not a mapping");
        }
        for k in &doc.keys {
            let ok = k == b"records" || k == b"epochs" || HEAD.iter().any(|h| k == h);
            if !ok {
                fail!(name, ": unknown top-level key ", k);
            }
        }
        for i in 0..3 {
            head[i] = doc.find(HEAD[i]).cloned();
        }
    } else {
        let csv = parse_csv(text_bytes)?;
        for key in csv.directives.keys() {
            if !HEAD.iter().any(|h| key == h) {
                fail!(name, ": unknown directive ", key);
            }
        }
        for i in 0..3 {
            if let Some(v) = csv.directives.get(HEAD[i]) {
                head[i] = Some(Rc::new(Node::scalar(v.clone(), true, 0)));
            }
        }
        let known = csv_columns(t);
        let mut bases = Vec::new();
        let mut idx = Vec::new();
        for c in &csv.header {
            let split = if known.contains(c) { split_column(c) } else { None };
            let Some((b, i)) = split else {
                fail!(name, ": unknown column ", c);
            };
            bases.push(b);
            idx.push(i);
        }
        for r in 0..csv.rows.len() {
            csv_records.items.push(Rc::new(csv_record(&bases, &idx, &csv.rows[r], csv.row_lines[r])));
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
            let shown: Vec<u8> = if h.kind == Kind::Scalar { h.text.clone() } else { b"?".to_vec() };
            fail!(name, ": format ", shown, ", this loader reads format ", FORMAT_VERSION);
        }
    }
    let total =
        if present(head[2].as_deref()) { head_int(head[2].as_ref().unwrap(), "rows", name.as_bytes())? } else { 0 };
    if t.container == Container::Techtree || t.container == Container::Epochs {
        // placed together with the other text (get_layout)
        for k in &doc.keys {
            if k == b"epochs" || k == b"rows" {
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
        return Ok(Doc { doc: Rc::new(doc), total, rows: Vec::new() });
    }
    let records: Option<&Node> = if t.yaml { doc.find(b"records").map(|r| &**r) } else { Some(&csv_records) };
    let records = records.filter(|r| !r.is_missing());
    let rows = place_records(records, total, name.as_bytes(), if t.yaml { None } else { Some(&csv_lines) })?;
    Ok(Doc { doc: Rc::new(doc), total, rows })
}

/// Every table's parsed text, the ids of each and the references between them.
pub struct Content {
    read: Reader,
    docs: HashMap<&'static str, Rc<Doc>>,
    indexes: HashMap<&'static str, Rc<Index>>,
    layout: Option<Rc<Vec<EpochBlock>>>,
    attributes: Option<Rc<HashMap<Vec<u8>, i64>>>,
}

impl Content {
    pub fn new(read: Reader) -> Content {
        Content { read, docs: HashMap::new(), indexes: HashMap::new(), layout: None, attributes: None }
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
        let d = Rc::new(parse_doc(t, &text)?);
        self.docs.insert(t.name, d.clone());
        Ok(d)
    }

    /// A record's key: its position, or its key field (dbtechtree mID).
    fn key(&self, t: &Table, p: &Placed, where_: &[u8]) -> Res<i64> {
        let Some(key_field) = t.key_field else { return Ok(p.pos) };
        let f = t.find_field(key_field).expect("the key field is in the record");
        let v = p.map.find(key_field.as_bytes()).map(|r| &**r);
        let bits = number(f.name, f.ty, if present(v) { v } else { None }, where_)?;
        Ok(bits as i32 as i64)
    }

    /// epochs.yaml and dbtechtree.yaml placed: every epoch (its position is its key) with its techs in its block.
    fn get_layout(&mut self) -> Res<Rc<Vec<EpochBlock>>> {
        if let Some(l) = &self.layout {
            return Ok(l.clone());
        }
        let te = table("epochs");
        let tt = table("dbtechtree");
        let de = self.get_doc(te)?;
        let dt = self.get_doc(tt)?;
        let eps = de.doc.find(b"records");
        let nodes = dt.doc.find(b"records");
        let mut index: HashMap<Vec<u8>, usize> = HashMap::new();
        let mut layout: Vec<EpochBlock> = Vec::new();
        if let Some(eps) = eps.filter(|e| e.kind == Kind::List) {
            for (k, m) in eps.items.iter().enumerate() {
                let where_ = msg!("epochs line ", m.line);
                if m.find(b"row").is_some() {
                    fail!(&where_, ": an epoch has no row (its place in the file is its position)");
                }
                let slots = m.find(b"slots").map(|r| &**r);
                let slots = if present(slots) { head_int(slots.unwrap(), "slots", &where_)? } else { 0 };
                let id = m.find(b"id");
                if let Some(id) = id {
                    if present(Some(id)) && id.kind == Kind::Scalar && !index.contains_key(&id.text) {
                        index.insert(id.text.clone(), k);
                    }
                }
                layout.push(EpochBlock { epoch: m.clone(), line: m.line, slots, nodes: Vec::new() });
            }
        }
        let mut groups: Vec<Vec<Rc<Node>>> = vec![Vec::new(); layout.len()];
        let mut lines: Vec<Vec<i64>> = vec![Vec::new(); layout.len()];
        if let Some(nodes) = nodes.filter(|n| n.kind == Kind::List) {
            for m in &nodes.items {
                let e = m.find(b"epoch").map(|r| &**r);
                let found = e.filter(|e| present(Some(e)) && e.kind == Kind::Scalar).and_then(|e| index.get(&e.text));
                let Some(&at) = found else {
                    let shown =
                        e.filter(|e| present(Some(e)) && e.kind == Kind::Scalar).map_or(Vec::new(), |e| e.text.clone());
                    fail!("dbtechtree line ", m.line, ": epoch '", shown, "' is not an id of epochs");
                };
                groups[at].push(m.clone());
                lines[at].push(m.line);
            }
        }
        for k in 0..layout.len() {
            layout[k].nodes = place_list(&groups[k], &lines[k], layout[k].slots, &msg!("dbtechtree epoch ", k))?;
        }
        let rc = Rc::new(layout);
        self.layout = Some(rc.clone());
        Ok(rc)
    }

    /// A tech's key: its mID, or (epoch + 1) * 1000 + slot.
    fn node_key(&self, p: &Placed, epoch: i64, where_: &[u8]) -> Res<i64> {
        let v = p.map.find(b"mID").map(|r| &**r);
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
        let mut all: Vec<Placed> = Vec::new();
        let mut keys: Vec<i64> = Vec::new();
        let mut files: Vec<&str> = Vec::new();
        if t.container == Container::Techtree || t.container == Container::Epochs {
            let layout = self.get_layout()?;
            for (k, block) in layout.iter().enumerate() {
                all.push(Placed { pos: k as i64, map: block.epoch.clone(), line: block.line });
                keys.push(k as i64);
                files.push("epochs");
            }
            if t.container == Container::Techtree {
                for (k, block) in layout.iter().enumerate() {
                    for p in &block.nodes {
                        all.push(p.clone());
                        keys.push(self.node_key(p, k as i64, &msg!("dbtechtree line ", p.line))?);
                        files.push("dbtechtree");
                    }
                }
            }
        } else {
            let d = self.get_doc(t)?;
            all = d.rows.clone();
            for p in &all {
                keys.push(self.key(t, p, &msg!(t.name, " line ", p.line))?);
                files.push(t.name);
            }
        }
        let mut idx = Index::default();
        let mut line_of: HashMap<Vec<u8>, Vec<u8>> = HashMap::new();
        for (n, p) in all.iter().enumerate() {
            let where_ = msg!(files[n], " line ", p.line);
            let key = keys[n];
            let v = p.map.find(b"id");
            let id = match v {
                Some(v) if present(Some(v)) && v.kind == Kind::Scalar => v,
                _ => {
                    fail!(&where_, ": no id");
                }
            };
            if !text::is_id(&id.text) {
                fail!(&where_, ": id '", &id.text, "': an id is a lowercase letter, then a-z, 0-9 or _");
            }
            if idx.keys.contains_key(&id.text) {
                fail!(&where_, ": duplicate id '", &id.text, "' (also ", &line_of[&id.text], ")");
            }
            if idx.ids.contains_key(&key) {
                fail!(&where_, ": id '", &id.text, "': key ", key, " is also '", &idx.ids[&key], "'");
            }
            for rn in t.row_names {
                if rn.row as i64 == key && id.text != rn.name.as_bytes() {
                    fail!(&where_, ": id '", &id.text, "' does not match row ", key, " ('", rn.name, "')");
                }
            }
            idx.keys.insert(id.text.clone(), key);
            idx.ids.insert(key, id.text.clone());
            line_of.insert(id.text.clone(), where_);
        }
        let rc = Rc::new(idx);
        self.indexes.insert(t.name, rc.clone());
        Ok(rc)
    }

    /// A reference cell -> the integer the record holds.
    fn resolve(&mut self, r: &Ref, ty: Type, v: Option<&Node>, where_: &[u8], field: &str) -> Res<u32> {
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
            let Some(&key) = idx.keys.get(&v.text) else {
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
        let mut out = Vec::new();
        let Some(v) = v else { return Ok(out) };
        let w = g.where_.clone();
        if v.kind != Kind::List {
            fail!(&w, ": ", name, " must be a list");
        }
        let r = t.find_ref("@list");
        let ty = if t.list_signed { Type::S32 } else { Type::U32 };
        for x in &v.items {
            if x.kind != Kind::Scalar {
                fail!(&w, ": ", name, ": a list where a number belongs");
            }
            if let Some(r) = r {
                out.push(self.resolve(r, ty, Some(x), &w, name)?);
            } else {
                out.push(parse_int(&x.text, ty).map_err(|e| Failure::new(msg!(&w, ": ", name, ": ", &e.text)))? as u32);
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
            let d = parse_yaml(&text)?;
            if let Some(list) = d.find(b"attributes").filter(|l| l.kind == Kind::List) {
                for a in &list.items {
                    let id = a.find(b"id").filter(|i| present(Some(i)) && i.kind == Kind::Scalar);
                    let level = a.find(b"level");
                    if let (Some(id), Some(level)) = (id, level) {
                        if present(Some(level)) {
                            let n = text::node_int(Some(level), &msg!("civbuilder line ", a.line), "level")?;
                            map.insert(id.text.clone(), n);
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
        let w = g.where_.clone();
        if v.kind != Kind::List {
            fail!(&w, ": ", key, " must be a list");
        }
        let levels = self.attribute_levels()?;
        for x in &v.items {
            if x.kind != Kind::Map {
                fail!(&w, ": ", key, ": a level is a mapping (attribute, value, bonus)");
            }
            for k in &x.keys {
                if k != b"attribute" && k != b"level" && k != b"value" && k != b"bonus" {
                    fail!(&w, ": ", key, ": unknown key '", k, "'");
                }
            }
            let a = x.find(b"attribute").map(|r| &**r);
            let l = x.find(b"level").map(|r| &**r);
            if present(a) == present(l) {
                fail!(&w, ": ", key, ": name the attribute (or a level without one)");
            }
            let lv: i64 = if present(a) {
                let id = scalar_text(a.unwrap(), &w, key, "an attribute id")?;
                match levels.get(id) {
                    Some(n) => *n,
                    None => {
                        fail!(&w, ": ", key, ": attribute '", id, "' is not an attribute of civbuilder");
                    }
                }
            } else {
                let lv = number("level", Type::S32, l, &w)? as i32 as i64;
                if lv < 1 {
                    fail!(&w, ": ", key, ": level must be at least 1");
                }
                lv
            };
            if lv > f.count as i64 {
                continue;
            }
            let off = f.offset as usize + 8 * (lv as usize - 1);
            let vv = x.find(b"value").map(|r| &**r);
            let bv = x.find(b"bonus").map(|r| &**r);
            put_le(buf, off, number("value", Type::F32, if present(vv) { vv } else { None }, &w)?, 4);
            put_le(buf, off + 4, number("bonus", Type::F32, if present(bv) { bv } else { None }, &w)?, 4);
        }
        Ok(())
    }

    fn build_record(&mut self, t: &'static Table, g: &mut Getter, has_key: bool, key: i64) -> Res<Vec<u8>> {
        let st = t.record;
        let mut buf = new_record(st.base, st.size as usize);
        for f in st.fields {
            if t.epoch_flags == Some(f.name) {
                continue; // set from epochs.yaml's lists (epoch_flags)
            }
            if t.levels_field == Some(f.name) {
                self.levels(t, f, g, &mut buf)?; // the attribute-keyed levels
                continue;
            }
            let w = g.where_.clone();
            let r = t.find_ref(f.name);
            let off = f.offset as usize;
            if f.ty == Type::Str {
                let v = g.get(f.name);
                let text = text_value(v, &w, f.name)?;
                if text.len() >= f.count as usize {
                    fail!(
                        &w,
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
                let e = elems(v, f.count as usize, &w, f.name)?;
                for k in 0..f.count as usize {
                    let p = elems(e[k], 2, &w, f.name)?;
                    put_le(&mut buf, off + 8 * k, number(f.name, Type::F32, p[0], &w)?, 4);
                    put_le(&mut buf, off + 8 * k + 4, number(f.name, Type::F32, p[1], &w)?, 4);
                }
            } else {
                let size = f.ty.size();
                if f.count == 1 {
                    let v = g.get(f.name);
                    if let Some(n) = v {
                        if n.kind == Kind::List {
                            fail!(&w, ": ", f.name, ": a list where a number belongs");
                        }
                    }
                    let x = if v.is_none() && has_key && t.self_field == Some(f.name) {
                        (key + t.self_offset as i64) as u32
                    } else if let Some(r) = r {
                        self.resolve(r, f.ty, v, &w, f.name)?
                    } else {
                        number(f.name, f.ty, v, &w)?
                    };
                    put_le(&mut buf, off, x, size);
                } else {
                    let v = g.get(f.name);
                    let e = elems(v, f.count as usize, &w, f.name)?;
                    for k in 0..f.count as usize {
                        if let Some(n) = e[k] {
                            if n.kind == Kind::List {
                                fail!(&w, ": ", f.name, ": a list where a number belongs");
                            }
                        }
                        let x = if let Some(r) = r {
                            self.resolve(r, f.ty, e[k], &w, f.name)?
                        } else {
                            number(f.name, f.ty, e[k], &w)?
                        };
                        put_le(&mut buf, off + size * k, x, size);
                    }
                }
            }
        }
        Ok(buf)
    }

    fn build_row(&mut self, t: &'static Table, p: &Placed, has_key: bool, text_keys: &[&str]) -> Res<Row> {
        let m = p.map.clone();
        let mut where_ = msg!(if t.container == Container::Epochs { "epochs" } else { t.name }, " line ", p.line);
        if let Some(id) = m.find(b"id") {
            if present(Some(id)) && id.kind == Kind::Scalar {
                where_.extend_from_slice(b" (id ");
                where_.extend_from_slice(&id.text);
                where_.push(b')');
            }
        }
        let mut g = Getter::new(&m, &where_);
        g.get("row");
        g.get("id");
        for k in text_keys {
            g.get(k);
        }
        let mut r = Row { filled: true, ..Row::default() };
        r.rec = self.build_record(t, &mut g, has_key, p.pos)?;
        r.lead = build_lead(t, &mut g)?;
        if t.list_name.is_some() {
            r.list = self.build_list(t, &mut g)?;
        }
        g.leftover()?;
        Ok(r)
    }

    /// A per-epoch flag array (dbmusic mInEpoch) from epochs.yaml's lists.
    fn epoch_flags(&mut self, t: &'static Table, rows: &mut [Row], listed: &[bool]) -> Res<()> {
        let f = t.find_field(t.epoch_flags.expect("flags")).expect("the field is in the record");
        let key = t.epoch_key.expect("the epoch key");
        let layout = self.get_layout()?;
        let self_ref = Ref { field: key, table: t.name, none: &[], never: false };
        for k in 0..layout.len().min(f.count as usize) {
            let v = layout[k].epoch.find(key.as_bytes());
            let where_ = msg!("epochs line ", layout[k].line);
            let Some(v) = v.filter(|v| present(Some(v))) else { continue };
            if v.kind != Kind::List {
                fail!(&where_, ": ", key, " must be a list");
            }
            for x in &v.items {
                let pos = self.resolve(&self_ref, Type::S32, Some(x), &where_, key)? as i32 as i64;
                if pos < 0 || pos as usize >= rows.len() || !listed[pos as usize] {
                    fail!(&where_, ": ", key, ": '", &x.text, "' names no row of ", t.name);
                }
                rows[pos as usize].rec[f.offset as usize + k] = 1;
            }
        }
        Ok(())
    }

    fn write_record(rec_or_hole: &Row, hole: &[u8], out: &mut Vec<u8>) {
        out.extend_from_slice(if rec_or_hole.filled { &rec_or_hole.rec } else { hole });
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
            for (k, b) in layout.iter().enumerate() {
                let k64 = k as i64;
                let mut ep =
                    self.build_row(te, &Placed { pos: k64, map: b.epoch.clone(), line: b.line }, false, &EPOCH_KEYS)?;
                for dv in te.derived {
                    if !present(b.epoch.find(dv.field.as_bytes()).map(|r| &**r)) {
                        let f = te.find_field(dv.field).expect("derived field");
                        put_le(&mut ep.rec, f.offset as usize, (dv.a as i64 * k64 + dv.b as i64) as u32, 4);
                    }
                }
                let mut n = b.slots;
                for p in &b.nodes {
                    n = n.max(p.pos + 1);
                }
                let mut rows = Content::row_slots(t.name, n)?;
                for p in &b.nodes {
                    let mut r = self.build_row(t, p, false, &NODE_KEYS)?;
                    if !present(p.map.find(b"mID").map(|m| &**m)) {
                        put_le(&mut r.rec, the_id.offset as usize, ((k64 + 1) * 1000 + p.pos) as u32, 4);
                    }
                    rows[p.pos as usize] = r;
                }
                put32(&mut out, rows.len() as u32);
                Content::write_record(&ep, &hole, &mut out);
                Content::write_list(&ep, &mut out);
                for r in &rows {
                    Content::write_record(r, &hole, &mut out);
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
            for p in &d.rows {
                rows[p.pos as usize] = self.build_row(t, p, true, &[])?;
                listed[p.pos as usize] = true;
            }
            if t.epoch_flags.is_some() {
                self.epoch_flags(t, &mut rows, &listed)?;
            }
            if t.container == Container::Count || t.container == Container::Lead {
                put32(&mut out, rows.len() as u32);
            }
            for r in &rows {
                if t.container == Container::Lead {
                    Content::write_lead(r, leads, &mut out);
                }
                Content::write_record(r, &hole, &mut out);
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
        let mut doc = parse_yaml(&text).map_err(|e| Failure::new(msg!(name, " ", &e.text)))?;
        if doc.kind != Kind::Map {
            fail!(name, ": the document is not a mapping");
        }
        if let Some(n) = doc.find(b"eedata").filter(|n| !n.is_missing()) {
            if n.kind != Kind::Scalar || n.text != name.as_bytes() {
                let shown: Vec<u8> = if n.kind == Kind::Scalar { n.text.clone() } else { Vec::new() };
                fail!(name, ": the file says it is '", shown, "'");
            }
        }
        if let Some(f) = doc.find(b"format").filter(|n| !n.is_missing()) {
            if f.kind != Kind::Scalar || f.text != FORMAT_VERSION.to_string().as_bytes() {
                let shown: Vec<u8> = if f.kind == Kind::Scalar { f.text.clone() } else { Vec::new() };
                fail!(name, ": format ", shown, ", the game reads format ", FORMAT_VERSION);
            }
        }
        crate::rules::apply_overrides(name, &mut doc);
        Ok(Some(doc))
    }

    /// The key of the row of a table that the node names (its row position; dbtechtree and epochs: the tech id).
    pub fn key_of(&mut self, table_name: &str, id: Option<&Node>, where_: &[u8], field: &str) -> Res<i64> {
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

    /// The ids of a table's listed rows by key.
    pub fn ids(&mut self, table_name: &str) -> Res<BTreeMap<i64, Vec<u8>>> {
        let Some(t) = find_table(table_name.as_bytes()) else {
            fail!(table_name, ": not a db table with a text form");
        };
        Ok(self.get_index(t)?.ids.clone())
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
        assert_eq!(ids[&1], b"beta".to_vec());
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
