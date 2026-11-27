//! Splits cbindgen's one header into a header per module, so that a C++ unit depends only on the modules it uses.
//!
//! cbindgen runs once over `ee_rust` (renames, the order of declarations and the dropped duplicates stay those of one
//! shared header); this module cuts its output into its top-level items and gives each to the crate that defines it,
//! found in the crates' sources. A name defined in more than one crate goes to the crate whose definition matches the
//! item (the doc comment, then the field and variant names, then the numbers); identical ones (bare `OK = 0`
//! constants) and the items of `ee_rust` itself go to the common header. A header includes the headers of the crates
//! whose items its items name. The text of every item is cbindgen's, unchanged, but for the C++ value classes in the
//! records headers.
//!
//! Records headers (`rust/<crate>/records.h`): the records a crate's `records.rs` lists (`pub use` of their
//! definitions) or defines, and every type they name, go to their crate's records header instead of their module's.
//! It declares no function and includes only other records headers (and the C++ value classes' headers,
//! [`CPP_VALUES`]), so a C++ class header that needs only its records is not rebuilt by a change to a function.

use std::collections::{BTreeMap, BTreeSet, HashMap};
use std::path::Path;

/// The header with the items of `ee_rust` itself and the names no one crate owns.
pub const COMMON: &str = "common";
/// The stem of a crate's records header (`sim/records`): the records its `records.rs` lists and the types they name.
pub const RECORDS: &str = "records";

/// The C++ value classes whose storage is a Rust mirror (the mirror's C name, the class, its header). A records
/// header declares a member of a mirror's type with the class itself, includes the class's header and asserts that
/// the two have one size and alignment; base's and render's do not, since the classes' headers include theirs.
pub const CPP_VALUES: &[(&str, &str, &str)] = &[
    ("EeGe2DPoint", "GE2DPoint", "lle/GE2DPoint.h"),
    ("EeGe3DPoint", "GE3DPoint", "lle/GE3DPoint.h"),
    ("EeGeVector", "GEVector", "lle/GEVector.h"),
    ("EeGe3DLine", "GE3DLine", "lle/GE3DLine.h"),
    ("EeGe3DPlane", "GE3DPlane", "lle/GE3DPlane.h"),
    ("EeGeTransformation", "GETransformation", "lle/GETransformation.h"),
    ("EeGeOrientation", "GEOrientation", "lle/GEOrientation.h"),
    ("EeGeQuaternion", "GEQuaternion", "lle/GEQuaternion.h"),
    ("EeGeBoundingBox", "GEBoundingBox", "lle/GEBoundingBox.h"),
    ("EeGeRectangle", "GERectangle", "lle/GERectangle.h"),
    ("EeGeColor", "GEColor", "lle/GEColor.h"),
    ("EeU2DPointLong", "U2DPoint<long>", "lle/U2DPoint.h"),
    ("EeU2DPointFloat", "U2DPoint<float>", "lle/U2DPoint.h"),
];

/// The lle math headers a records header that uses a C++ value class includes first, in the engine's order (they
/// need each other: `lle/LowLevelEngine.h`'s geometry block).
const CPP_VALUE_CHAIN: &[&str] = &[
    "lle/LowLevelEngineCore.h",
    "lle/UMFastMath.h",
    "lle/GE2DPoint.h",
    "lle/GEVector.h",
    "lle/GE3DPoint.h",
    "lle/GE3DLine.h",
    "lle/GE3DPlane.h",
    "lle/GERectangle.h",
    "lle/GEOrientation.h",
    "lle/GETransformation.h",
];

/// The crate directories whose records headers keep the mirrors' C names (see [`CPP_VALUES`]).
const NO_CPP_VALUES: &[&str] = &["base", "render"];
/// The umbrella that includes every header.
pub const UMBRELLA: &str = "all";

const EXTERN_OPEN: &str = "extern \"C\" {";
const EXTERN_CLOSE: &str = "}  // extern \"C\"";

/// A top-level declaration of the header: its lines (doc comment included) and its name.
#[derive(Clone, Debug, PartialEq, Eq, PartialOrd, Ord)]
pub struct Item {
    pub name: String,
    pub function: bool,
    pub lines: Vec<String>,
}

/// A generated header cut into its parts.
#[derive(Debug)]
pub struct Parsed {
    /// The `#include <...>` lines of the preamble.
    pub sys_includes: Vec<String>,
    pub types: Vec<Item>,
    pub functions: Vec<Item>,
}

fn code_part(line: &str) -> &str {
    line.split("//").next().unwrap_or("")
}

fn is_comment(line: &str) -> bool {
    line.trim_start().starts_with("//")
}

/// Cuts lines into items at the blank lines outside braces.
fn chunks(lines: &[&str]) -> Vec<Vec<String>> {
    let mut out = Vec::new();
    let mut cur: Vec<String> = Vec::new();
    let mut depth = 0i32;
    for line in lines {
        if line.is_empty() && depth == 0 {
            if !cur.is_empty() {
                out.push(std::mem::take(&mut cur));
            }
            continue;
        }
        let code = code_part(line);
        depth += code.matches('{').count() as i32 - code.matches('}').count() as i32;
        cur.push((*line).to_string());
    }
    if !cur.is_empty() {
        out.push(cur);
    }
    out
}

fn is_ident_char(c: char) -> bool {
    c.is_ascii_alphanumeric() || c == '_'
}

/// The identifiers of a text, in order.
pub fn identifiers(text: &str) -> Vec<&str> {
    let mut out = Vec::new();
    let mut start = None;
    for (i, c) in text.char_indices() {
        match (start, is_ident_char(c)) {
            (None, true) => start = Some(i),
            (Some(s), false) => {
                let word = &text[s..i];
                if !word.starts_with(|c: char| c.is_ascii_digit()) {
                    out.push(word);
                }
                start = None;
            }
            _ => {}
        }
    }
    if let Some(s) = start {
        let word = &text[s..];
        if !word.starts_with(|c: char| c.is_ascii_digit()) {
            out.push(word);
        }
    }
    out
}

/// The number tokens of a text (`16`, `0.5`, `0x1F`).
fn numbers(text: &str) -> Vec<&str> {
    let mut out = Vec::new();
    let mut start = None;
    for (i, c) in text.char_indices() {
        let part = is_ident_char(c) || (c == '.' && start.is_some());
        match (start, part) {
            (None, true) => start = Some(i),
            (Some(s), false) => {
                if text[s..i].starts_with(|c: char| c.is_ascii_digit()) {
                    out.push(&text[s..i]);
                }
                start = None;
            }
            _ => {}
        }
    }
    if let Some(s) = start {
        if text[s..].starts_with(|c: char| c.is_ascii_digit()) {
            out.push(&text[s..]);
        }
    }
    out
}

fn code_text(lines: &[String]) -> String {
    lines.iter().filter(|l| !is_comment(l)).map(|l| code_part(l)).collect::<Vec<_>>().join(" ")
}

fn type_name(lines: &[String]) -> Result<String, String> {
    let first = lines.iter().find(|l| !is_comment(l)).ok_or("an item without code")?;
    let first = code_part(first).trim();
    let after =
        |prefix: &str| first.strip_prefix(prefix).and_then(|rest| identifiers(rest).first().map(|s| s.to_string()));
    let name = if let Some(rest) = first.strip_prefix("constexpr static const ") {
        let left = rest.split(" = ").next().unwrap_or(rest);
        let left = left.split('[').next().unwrap_or(left);
        identifiers(left).last().map(|s| s.to_string())
    } else if first.starts_with("typedef ") {
        identifiers(code_text(lines).split(';').next().unwrap_or("")).last().map(|s| s.to_string())
    } else {
        after("enum class ")
            .or_else(|| after("struct "))
            .or_else(|| after("union "))
            .or_else(|| after("enum "))
            .or_else(|| after("using "))
    };
    name.ok_or_else(|| format!("cannot name the header item `{first}`"))
}

fn function_name(lines: &[String]) -> Result<String, String> {
    let code = code_text(lines);
    let open = code.find('(').ok_or_else(|| format!("no `(` in the function `{code}`"))?;
    identifiers(&code[..open]).last().map(|s| s.to_string()).ok_or_else(|| format!("cannot name the function `{code}`"))
}

/// Parses a header cbindgen (or [`render`]) wrote.
pub fn parse(text: &str) -> Result<Parsed, String> {
    let lines: Vec<&str> = text.lines().collect();
    let open = lines.iter().position(|l| *l == EXTERN_OPEN);
    let close = lines.iter().position(|l| *l == EXTERN_CLOSE);
    let types_end = open.unwrap_or_else(|| lines.iter().rposition(|l| l.starts_with("#endif")).unwrap_or(lines.len()));
    let start = lines[..types_end]
        .iter()
        .rposition(|l| l.starts_with("#include") || l.starts_with("#define"))
        .map_or(0, |i| i + 1);
    let sys_includes = lines[..start].iter().filter(|l| l.starts_with("#include <")).map(|l| l.to_string()).collect();
    let mut types = Vec::new();
    for chunk in chunks(&lines[start..types_end]) {
        if chunk.iter().find(|l| !is_comment(l)).is_some_and(|l| l.starts_with("static_assert")) {
            continue; // a records header's checks of the C++ value classes
        }
        types.push(Item { name: type_name(&chunk)?, function: false, lines: chunk });
    }
    let mut functions = Vec::new();
    if let (Some(open), Some(close)) = (open, close) {
        for chunk in chunks(&lines[open + 1..close]) {
            functions.push(Item { name: function_name(&chunk)?, function: true, lines: chunk });
        }
    }
    Ok(Parsed { sys_includes, types, functions })
}

/// A definition in a crate's sources.
#[derive(Debug)]
pub struct Def {
    pub krate: String,
    pub doc: Vec<String>,
    pub body: String,
}

/// The name a source line defines (`pub struct Name`, `pub unsafe extern "C" fn name`, ...), if any.
fn defined_name(line: &str) -> Option<&str> {
    let mut words = line.split_whitespace().peekable();
    while let Some(word) = words.next() {
        match word {
            "pub" | "unsafe" | "extern" | "\"C\"" | "\"C-unwind\"" | "async" => continue,
            w if w.starts_with("pub(") => continue,
            "const" if matches!(words.peek(), Some(&"fn") | Some(&"unsafe") | Some(&"extern")) => continue,
            "fn" | "struct" | "enum" | "union" | "type" | "const" | "static" => {
                let next = words.next()?;
                let next = next.strip_prefix("mut ").unwrap_or(next);
                let end = next.find(|c: char| !is_ident_char(c)).unwrap_or(next.len());
                return (end > 0).then(|| &next[..end]);
            }
            _ => return None,
        }
    }
    None
}

fn snippet(lines: &[&str], at: usize) -> String {
    let mut out = Vec::new();
    let mut depth = 0i32;
    for line in lines.iter().skip(at).take(400) {
        let code = code_part(line);
        out.push(code);
        depth += code.matches('{').count() as i32 - code.matches('}').count() as i32;
        let trimmed = code.trim_end();
        if depth <= 0 && (trimmed.ends_with(';') || (depth == 0 && code.contains('}'))) {
            break;
        }
    }
    out.join(" ")
}

fn is_test_source(path: &Path) -> bool {
    let name = path.file_name().and_then(|n| n.to_str()).unwrap_or("");
    name == "tests.rs" || name.ends_with("_tests.rs") || path.components().any(|c| c.as_os_str() == "tests")
}

fn rust_files(dir: &Path, out: &mut Vec<std::path::PathBuf>) {
    let Ok(entries) = std::fs::read_dir(dir) else { return };
    let mut entries: Vec<_> = entries.flatten().map(|e| e.path()).collect();
    entries.sort();
    for path in entries {
        if path.is_dir() {
            rust_files(&path, out);
        } else if path.extension().is_some_and(|e| e == "rs") {
            out.push(path);
        }
    }
}

/// The crates and their header directories: `rust/<dir>/<module>.h` holds the C interface of one top-level module of
/// the crate (`rust/sim/units.h`: `ee_sim::units`), `rust/<dir>/<dir>.h` the crate root's own items.
const CRATE_DIRS: &[(&str, &str)] = &[
    ("ee_base", "base"),
    ("ee_data", "data"),
    ("ee_sim", "sim"),
    ("ee_render", "render"),
    ("ee_audio", "audio"),
    ("ee_net", "net"),
    ("ee_ui", "ui"),
    ("ee_won_net", "won_net"),
    ("ee_won_gui", "won_gui"),
    ("ee_won_lobby", "won_lobby"),
    ("ee_game", "game"),
];

/// Modules whose C interfaces name each other share one header (a header cannot include one that includes it).
const SHARED: &[(&str, &str)] =
    &[("won_net/cppcore", "won_net/hostop"), ("won_net/opobj", "won_net/hostop"), ("won_net/thread", "won_net/hostop")];

/// The header a source file's items go to: its crate's directory and its top-level module; ee_game's root (the
/// library's own items) is the common header.
fn header_of(crates_dir: &Path, krate: &str, file: &Path) -> String {
    if krate == "ee_net_tcp" {
        return "net/net_tcp".to_string(); // the plug-in, a crate of its own beside ee_net
    }
    let dir = CRATE_DIRS.iter().find(|(k, _)| *k == krate).map_or(krate, |(_, d)| *d);
    let src = crates_dir.join(krate).join("src");
    let first = file.strip_prefix(&src).ok().and_then(|p| p.components().next()).and_then(|c| c.as_os_str().to_str());
    let unit = match first {
        Some("lib.rs") | None if krate == "ee_game" => return UMBRELLA.to_string(),
        Some("lib.rs") | None => format!("{dir}/{dir}"),
        Some(name) => format!("{dir}/{}", name.strip_suffix(".rs").unwrap_or(name)),
    };
    SHARED.iter().find(|(m, _)| *m == unit).map_or(unit.clone(), |(_, h)| h.to_string())
}

/// The module a header holds, for its first line (`sim/units`: `ee_sim::units`).
fn module_of(header: &str) -> String {
    let (dir, module) = header.split_once('/').unwrap_or((header, header));
    let krate = CRATE_DIRS.iter().find(|(_, d)| *d == dir).map_or(dir, |(k, _)| *k);
    if module == dir {
        format!("the crate {krate}'s root")
    } else {
        format!("{krate}::{module}")
    }
}

/// Every definition in the sources of the named crates (`rust/crates/<name>/src`), test sources left out.
pub fn scan(crates_dir: &Path, crates: &[String]) -> HashMap<String, Vec<Def>> {
    let mut defs: HashMap<String, Vec<Def>> = HashMap::new();
    for krate in crates {
        let mut files = Vec::new();
        rust_files(&crates_dir.join(krate).join("src"), &mut files);
        for file in files.iter().filter(|f| !is_test_source(f)) {
            let Ok(text) = std::fs::read_to_string(file) else { continue };
            scan_text(&header_of(crates_dir, krate, file), &text, &mut defs);
        }
    }
    defs
}

/// The definitions of one source file.
pub fn scan_text(krate: &str, text: &str, defs: &mut HashMap<String, Vec<Def>>) {
    let lines: Vec<&str> = text.lines().collect();
    for (i, line) in lines.iter().enumerate() {
        let Some(name) = defined_name(line) else { continue };
        let mut doc = Vec::new();
        let mut j = i;
        while j > 0 {
            let above = lines[j - 1].trim();
            if above.starts_with("///") {
                doc.push(above.to_string());
            } else if !above.starts_with("#[") {
                break;
            }
            j -= 1;
        }
        doc.reverse();
        defs.entry(name.to_string()).or_default().push(Def { krate: krate.to_string(), doc, body: snippet(&lines, i) });
    }
}

/// The records each crate's `records.rs` lists or defines, by the crate's header directory (`sim`), under their C
/// names: the type names (capitalized identifiers, but those after `as`) of its `use` declarations and the types it
/// defines.
pub fn record_roots(
    crates_dir: &Path,
    crates: &[String],
    renames: &HashMap<String, String>,
) -> BTreeMap<String, BTreeSet<String>> {
    let mut out = BTreeMap::new();
    for krate in crates {
        let Some((_, dir)) = CRATE_DIRS.iter().find(|(k, _)| k == krate) else { continue };
        let Ok(text) = std::fs::read_to_string(crates_dir.join(krate).join("src").join("records.rs")) else { continue };
        out.insert(
            dir.to_string(),
            records_listed(&text).into_iter().map(|n| renames.get(&n).cloned().unwrap_or(n)).collect(),
        );
    }
    out
}

/// The Rust names of the records a `records.rs` text lists or defines.
fn records_listed(text: &str) -> BTreeSet<String> {
    let code: String = text.lines().map(|l| code_part(l)).collect::<Vec<_>>().join("\n");
    let mut out = BTreeSet::new();
    for line in code.lines() {
        if let Some(name) = defined_name(line) {
            if name.starts_with(|c: char| c.is_ascii_uppercase()) {
                out.insert(name.to_string());
            }
        }
    }
    for statement in code.split(';') {
        let words = identifiers(statement);
        let Some(at) = words.iter().position(|w| *w == "use") else { continue };
        for (i, word) in words.iter().enumerate().skip(at + 1) {
            if word.starts_with(|c: char| c.is_ascii_uppercase()) && words[i - 1] != "as" {
                out.insert(word.to_string());
            }
        }
    }
    out
}

/// The crate directory of a header (`sim/units`: `sim`); none for the common header.
fn dir_of(header: &str) -> Option<&str> {
    header.split_once('/').map(|(dir, _)| dir)
}

/// Moves the records (and every type they name) to their crates' records headers, and with them every type another
/// module's header names: a function header then includes only records headers, so a change to one module's
/// functions rebuilds only the units that include that module's header. `all`: the types, then the functions.
fn assign_records(
    all: &[&Item],
    n_types: usize,
    owner: &mut [String],
    roots: &BTreeMap<String, BTreeSet<String>>,
) -> Result<(), String> {
    let types = &all[..n_types];
    let index: HashMap<&str, usize> = types.iter().enumerate().map(|(i, t)| (t.name.as_str(), i)).collect();
    let mut todo: Vec<usize> = Vec::new();
    for (i, item) in all.iter().enumerate() {
        for word in identifiers(&code_text(&item.lines)) {
            if let Some(&at) = index.get(word) {
                if at != i && owner[at] != owner[i] && dir_of(&owner[at]).is_some() {
                    todo.push(at);
                }
            }
        }
    }
    for (dir, names) in roots {
        for name in names {
            let at = types
                .iter()
                .zip(owner.iter())
                .position(|(t, o)| t.name == *name && dir_of(o) == Some(dir.as_str()))
                .ok_or_else(|| {
                    format!("{dir}'s records.rs lists `{name}`, which its crate's headers do not declare")
                })?;
            todo.push(at);
        }
    }
    let mut done = BTreeSet::new();
    while let Some(at) = todo.pop() {
        if !done.insert(at) {
            continue;
        }
        let Some(dir) = dir_of(&owner[at]).map(str::to_string) else {
            return Err(format!("the record `{}` is in the common header (no one crate defines it)", types[at].name));
        };
        owner[at] = format!("{dir}/{RECORDS}");
        for word in identifiers(&code_text(&types[at].lines)) {
            if let Some(&other) = index.get(word) {
                if other != at {
                    todo.push(other);
                }
            }
        }
    }
    Ok(())
}

/// Whether a header is a records header.
pub fn is_records(header: &str) -> bool {
    header.rsplit('/').next() == Some(RECORDS) && header.contains('/')
}

/// A records header's items with the C++ value classes for their mirrors (see [`CPP_VALUES`]), and the headers and
/// checks those need.
fn with_cpp_values(header: &str, types: &[&Item]) -> (Vec<Item>, Vec<String>) {
    let mut used: BTreeSet<usize> = BTreeSet::new();
    let skip = dir_of(header).is_some_and(|d| NO_CPP_VALUES.contains(&d));
    let items = types
        .iter()
        .map(|item| {
            let mut item = (*item).clone();
            if skip {
                return item;
            }
            for line in item.lines.iter_mut().filter(|l| !is_comment(l)) {
                for (i, (mirror, class, _)) in CPP_VALUES.iter().enumerate() {
                    let replaced = replace_word(line, mirror, class);
                    if replaced != *line {
                        used.insert(i);
                        *line = replaced;
                    }
                }
            }
            item
        })
        .collect();
    let mut preamble: Vec<String> = Vec::new();
    let quoted = |h: &str| if h.starts_with('<') { format!("#include {h}") } else { format!("#include \"{h}\"") };
    if !used.is_empty() {
        preamble.extend(CPP_VALUE_CHAIN.iter().map(|h| quoted(h)));
    }
    for &i in &used {
        let include = quoted(CPP_VALUES[i].2);
        if !preamble.contains(&include) {
            preamble.push(include);
        }
    }
    if !used.is_empty() {
        preamble.push(String::new());
    }
    for &i in &used {
        let (mirror, class, _) = CPP_VALUES[i];
        preamble.push(format!(
            "static_assert(sizeof({class}) == sizeof({mirror}) && alignof({class}) == alignof({mirror}), \
             \"{class} is its Rust mirror {mirror}\");"
        ));
    }
    (items, preamble)
}

/// A line with every whole-word `from` replaced by `to`.
fn replace_word(line: &str, from: &str, to: &str) -> String {
    let mut out = String::with_capacity(line.len());
    let mut rest = line;
    while let Some(at) = rest.find(from) {
        let before = rest[..at].chars().next_back();
        let after = rest[at + from.len()..].chars().next();
        out.push_str(&rest[..at]);
        if before.is_some_and(is_ident_char) || after.is_some_and(is_ident_char) {
            out.push_str(from);
        } else {
            out.push_str(to);
        }
        rest = &rest[at + from.len()..];
    }
    out.push_str(rest);
    out
}

const CPP_WORDS: &[&str] = &[
    "constexpr",
    "static",
    "const",
    "struct",
    "enum",
    "class",
    "union",
    "using",
    "typedef",
    "int8_t",
    "int16_t",
    "int32_t",
    "int64_t",
    "uint8_t",
    "uint16_t",
    "uint32_t",
    "uint64_t",
    "size_t",
    "ptrdiff_t",
    "intptr_t",
    "uintptr_t",
    "bool",
    "float",
    "double",
    "char",
    "void",
    "unsigned",
    "long",
    "signed",
    "short",
    "int",
];

/// How well a definition matches a header item: the doc comment, then its names, then its numbers.
fn score(item: &Item, def: &Def, unrename: &HashMap<&str, &str>) -> (bool, i64, i64) {
    let doc: Vec<String> =
        item.lines.iter().map(|l| l.trim()).filter(|l| l.starts_with("///")).map(str::to_string).collect();
    let code = code_text(&item.lines);
    let names: BTreeSet<&str> = identifiers(&code)
        .into_iter()
        .filter(|w| *w != item.name && !CPP_WORDS.contains(w))
        .map(|w| *unrename.get(w).unwrap_or(&w))
        .collect();
    let body: BTreeSet<&str> = identifiers(&def.body).into_iter().collect();
    let found = names.iter().filter(|n| body.contains(*n)).count() as i64;
    let body_numbers: BTreeSet<&str> = numbers(&def.body).into_iter().collect();
    let numbers_found = numbers(&code).iter().filter(|n| body_numbers.contains(*n)).count() as i64;
    (!doc.is_empty() && doc == def.doc, found - (names.len() as i64 - found), numbers_found)
}

/// The header (crate name or [`COMMON`]) of every item, in the order given.
pub fn owners(items: &[&Item], defs: &HashMap<String, Vec<Def>>, renames: &HashMap<String, String>) -> Vec<String> {
    let unrename: HashMap<&str, &str> = renames.iter().map(|(rust, cpp)| (cpp.as_str(), rust.as_str())).collect();
    let mut out: Vec<Option<String>> = items
        .iter()
        .map(|item| {
            let rust = *unrename.get(item.name.as_str()).unwrap_or(&item.name.as_str());
            let defs = defs.get(rust)?;
            let crates: BTreeSet<&str> = defs.iter().map(|d| d.krate.as_str()).collect();
            if crates.len() == 1 {
                return Some(defs[0].krate.clone());
            }
            let mut best: BTreeMap<&str, (bool, i64, i64)> = BTreeMap::new();
            for def in defs {
                let s = score(item, def, &unrename);
                let e = best.entry(def.krate.as_str()).or_insert(s);
                *e = (*e).max(s);
            }
            let top = *best.values().max()?;
            let winners: Vec<&str> = best.iter().filter(|(_, s)| **s == top).map(|(k, _)| *k).collect();
            Some(if winners.len() == 1 { winners[0].to_string() } else { COMMON.to_string() })
        })
        .collect();
    // associated constants (`EeGeTransformation_IDENTITY`) go with their type
    let named: HashMap<&str, String> =
        items.iter().zip(&out).filter_map(|(i, o)| o.clone().map(|o| (i.name.as_str(), o))).collect();
    for (item, owner) in items.iter().zip(out.iter_mut()) {
        if owner.is_none() {
            *owner = item.name.match_indices('_').find_map(|(at, _)| named.get(&item.name[..at]).cloned());
        }
    }
    out.into_iter()
        .map(|o| match o {
            Some(k) if k == UMBRELLA => COMMON.to_string(),
            Some(k) => k,
            None => COMMON.to_string(),
        })
        .collect()
}

/// Every header's text, by file stem (`ee_won`, ..., [`COMMON`], [`UMBRELLA`]).
pub fn split(
    single: &str,
    defs: &HashMap<String, Vec<Def>>,
    renames: &HashMap<String, String>,
    roots: &BTreeMap<String, BTreeSet<String>>,
    note: &str,
) -> Result<BTreeMap<String, String>, String> {
    let parsed = parse(single)?;
    let all: Vec<&Item> = parsed.types.iter().chain(&parsed.functions).collect();
    let mut owner = owners(&all, defs, renames);
    let before = owner.clone();
    assign_records(&all, parsed.types.len(), &mut owner, roots)?;
    let by_name: HashMap<&str, &str> = all.iter().zip(&owner).map(|(i, o)| (i.name.as_str(), o.as_str())).collect();
    let mut seen = BTreeSet::new();
    for item in &all {
        if !seen.insert(item.name.as_str()) {
            return Err(format!("`{}` is declared twice in the generated header", item.name));
        }
    }
    let mut deps: BTreeMap<&str, BTreeSet<&str>> = BTreeMap::new();
    for (item, own) in all.iter().zip(&owner) {
        let entry = deps.entry(own.as_str()).or_default();
        for word in identifiers(&code_text(&item.lines)) {
            if let Some(other) = by_name.get(word) {
                if *other != own.as_str() {
                    entry.insert(other);
                }
            }
        }
    }
    // a module whose every item went to its records header keeps a header that includes it, so its includers stay
    for header in &before {
        if deps.contains_key(header.as_str()) || is_records(header) {
            continue;
        }
        let Some(dir) = dir_of(header) else { continue };
        if let Some(records) = owner.iter().find(|o| dir_of(o) == Some(dir) && is_records(o)) {
            deps.entry(header.as_str()).or_default().insert(records.as_str());
        }
    }
    check_acyclic(&deps)?;
    let mut out = BTreeMap::new();
    for (header, includes) in &deps {
        let types: Vec<&Item> = parsed.types.iter().zip(&owner).filter(|(_, o)| o == header).map(|(i, _)| i).collect();
        let functions: Vec<&Item> = parsed
            .functions
            .iter()
            .zip(&owner[parsed.types.len()..])
            .filter(|(_, o)| o == header)
            .map(|(i, _)| i)
            .collect();
        let text = if is_records(header) {
            if !functions.is_empty() {
                return Err(format!("{header}.h would declare the function `{}`", functions[0].name));
            }
            if let Some(other) = includes.iter().find(|h| !is_records(h)) {
                return Err(format!("{header}.h would include {other}.h, which is not a records header"));
            }
            let (items, preamble) = with_cpp_values(header, &types);
            let items: Vec<&Item> = items.iter().collect();
            render(header, note, &parsed.sys_includes, includes, &preamble, &items, &[])
        } else {
            render(header, note, &parsed.sys_includes, includes, &[], &types, &functions)
        };
        out.insert(header.to_string(), text);
    }
    let mut umbrella = format!(
        "{note}\n// Every module's C interface. Include only the headers of the modules a file uses \
         (rust/<crate>/<module>.h).\n\n\
         #ifndef EE_RUST_H\n#define EE_RUST_H\n\n"
    );
    for header in out.keys() {
        umbrella.push_str(&format!("#include \"rust/{header}.h\"\n"));
    }
    umbrella.push_str("\n#endif  // EE_RUST_H\n");
    out.insert(UMBRELLA.to_string(), umbrella);
    Ok(out)
}

fn check_acyclic(deps: &BTreeMap<&str, BTreeSet<&str>>) -> Result<(), String> {
    fn visit<'a>(
        node: &'a str,
        deps: &BTreeMap<&'a str, BTreeSet<&'a str>>,
        state: &mut HashMap<&'a str, bool>,
        path: &mut Vec<&'a str>,
    ) -> Result<(), String> {
        match state.get(node) {
            Some(true) => return Ok(()),
            Some(false) => {
                path.push(node);
                return Err(format!("the modules' headers include each other: {}", path.join(" -> ")));
            }
            None => {}
        }
        state.insert(node, false);
        path.push(node);
        for next in deps.get(node).into_iter().flatten() {
            visit(next, deps, state, path)?;
        }
        path.pop();
        state.insert(node, true);
        Ok(())
    }
    let mut state = HashMap::new();
    for node in deps.keys() {
        visit(node, deps, &mut state, &mut Vec::new())?;
    }
    Ok(())
}

fn guard(header: &str) -> String {
    format!("EE_RUST_{}_H", header.replace('/', "_").to_ascii_uppercase())
}

/// One header's text: the preamble, the types in cbindgen's order, the functions in an `extern "C"` block.
pub fn render(
    header: &str,
    note: &str,
    sys_includes: &[String],
    includes: &BTreeSet<&str>,
    preamble: &[String],
    types: &[&Item],
    functions: &[&Item],
) -> String {
    let what = if header == COMMON {
        "The C interface of ee_game's root and the names more than one module defines alike".to_string()
    } else if is_records(header) {
        let krate = dir_of(header).and_then(|d| CRATE_DIRS.iter().find(|(_, x)| *x == d)).map_or("", |(k, _)| *k);
        format!(
            "The records of {krate} that C++ classes derive from or embed (its records.rs), and the types they name; \
             no function"
        )
    } else {
        format!("The C interface of {}", module_of(header))
    };
    let guard = guard(header);
    let mut text =
        format!("{note}\n// {what} (rust/all.h includes every module's).\n\n#ifndef {guard}\n#define {guard}\n\n");
    for line in sys_includes {
        text.push_str(line);
        text.push('\n');
    }
    for include in includes {
        text.push_str(&format!("#include \"rust/{include}.h\"\n"));
    }
    for line in preamble {
        text.push_str(line);
        text.push('\n');
    }
    text.push('\n');
    for item in types {
        for line in &item.lines {
            text.push_str(line);
            text.push('\n');
        }
        text.push('\n');
    }
    if !functions.is_empty() {
        text.push_str(EXTERN_OPEN);
        text.push_str("\n\n");
        for item in functions {
            for line in &item.lines {
                text.push_str(line);
                text.push('\n');
            }
            text.push('\n');
        }
        text.push_str(EXTERN_CLOSE);
        text.push_str("\n\n");
    }
    text.push_str(&format!("#endif  // {guard}\n"));
    text
}

/// Every item of the per-crate headers, sorted; an error for a name in two of them.
#[cfg(test)]
pub fn union_of(headers: &BTreeMap<String, String>) -> Result<Vec<Item>, String> {
    let mut items = Vec::new();
    let mut names = BTreeMap::new();
    for (header, text) in headers.iter().filter(|(h, _)| *h != UMBRELLA) {
        let parsed = parse(text)?;
        for mut item in parsed.types.into_iter().chain(parsed.functions) {
            if is_records(header) {
                for line in item.lines.iter_mut().filter(|l| !is_comment(l)) {
                    for (mirror, class, _) in CPP_VALUES {
                        *line = replace_word(line, class, mirror);
                    }
                }
            }
            if let Some(other) = names.insert(item.name.clone(), header.clone()) {
                return Err(format!("`{}` is in {other}.h and {header}.h", item.name));
            }
            items.push(item);
        }
    }
    items.sort();
    Ok(items)
}

#[cfg(test)]
pub fn sorted_items(single: &str) -> Vec<Item> {
    let parsed = parse(single).unwrap();
    let mut items: Vec<Item> = parsed.types.into_iter().chain(parsed.functions).collect();
    items.sort();
    items
}

#[cfg(test)]
mod tests {
    use super::*;

    const SINGLE: &str = "// note\n\n#ifndef EE_RUST_H\n#define EE_RUST_H\n\n#include <stdint.h>\n#include <stddef.h>\n\n\
        constexpr static const int32_t OK = 0;\n\n\
        /// A point.\nstruct EeGePoint {\n  /// `{` in a comment\n  float x;\n\n  float y;\n};\n\n\
        constexpr static const uint8_t BITS[4] = { 2, 4, 8, 16, };\n\n\
        struct EeWonThing {\n  EeGePoint at;\n};\n\n\
        extern \"C\" {\n\n/// Makes one.\nEeWonThing ee_won_make(EeGePoint at);\n\nvoid ee_ge_draw(const EeGePoint *p);\n\n\
        }  // extern \"C\"\n\n#endif  // EE_RUST_H\n";

    fn defs() -> HashMap<String, Vec<Def>> {
        let mut defs = HashMap::new();
        scan_text("ee_ge", "/// A point.\n#[repr(C)]\npub struct Point { pub x: f32, pub y: f32 }\npub const OK: i32 = 0;\npub const BITS: [u8; 4] = [2, 4, 8, 16];\n#[no_mangle]\npub unsafe extern \"C\" fn ee_ge_draw(p: *const Point) {}\n", &mut defs);
        scan_text("ee_won", "pub struct EeWonThing { pub at: Point }\npub const OK: i32 = 0;\n/// Another point.\npub struct Point { a: u8 }\npub extern \"C\" fn ee_won_make(at: Point) -> EeWonThing { todo!() }\n", &mut defs);
        defs
    }

    fn renames() -> HashMap<String, String> {
        [("Point".to_string(), "EeGePoint".to_string())].into_iter().collect()
    }

    #[test]
    fn items_are_cut_at_blank_lines_outside_braces() {
        let parsed = parse(SINGLE).unwrap();
        let names: Vec<&str> = parsed.types.iter().map(|i| i.name.as_str()).collect();
        assert_eq!(names, ["OK", "EeGePoint", "BITS", "EeWonThing"]);
        assert_eq!(parsed.types[1].lines.len(), 7, "the blank line inside the struct stays in it");
        let functions: Vec<&str> = parsed.functions.iter().map(|i| i.name.as_str()).collect();
        assert_eq!(functions, ["ee_won_make", "ee_ge_draw"]);
        assert_eq!(parsed.sys_includes, ["#include <stdint.h>", "#include <stddef.h>"]);
    }

    #[test]
    fn source_lines_name_their_definitions() {
        assert_eq!(defined_name("pub unsafe extern \"C\" fn ee_x(a: u8) {"), Some("ee_x"));
        assert_eq!(defined_name("pub unsafe extern \"C-unwind\" fn ee_y(a: u8) {"), Some("ee_y"));
        assert_eq!(defined_name("pub(crate) struct Foo<'a> {"), Some("Foo"));
        assert_eq!(defined_name("    pub const IDENTITY: Self = Self;"), Some("IDENTITY"));
        assert_eq!(defined_name("pub const fn make() -> u8 {"), Some("make"));
        assert_eq!(defined_name("let x = 1;"), None);
        assert_eq!(defined_name("/// pub struct Doc"), None);
    }

    #[test]
    fn a_name_two_crates_define_goes_to_the_matching_one_or_to_common() {
        let parsed = parse(SINGLE).unwrap();
        let all: Vec<&Item> = parsed.types.iter().chain(&parsed.functions).collect();
        let owner = owners(&all, &defs(), &renames());
        // OK: the same in both, so common; EeGePoint: ee_ge's doc comment matches
        assert_eq!(owner, ["common", "ee_ge", "ee_ge", "ee_won", "ee_won", "ee_ge"]);
    }

    #[test]
    fn each_header_includes_the_headers_its_items_name() {
        let headers = split(SINGLE, &defs(), &renames(), &BTreeMap::new(), "// note").unwrap();
        let won = &headers["ee_won"];
        assert!(won.contains("#include \"rust/ee_ge.h\"\n"));
        assert!(!headers["ee_ge"].contains("#include \"rust/ee_won.h\""));
        assert!(won.contains(
            "extern \"C\" {\n\n/// Makes one.\nEeWonThing ee_won_make(EeGePoint at);\n\n}  // extern \"C\"\n"
        ));
        assert!(headers["all"]
            .contains("#include \"rust/common.h\"\n#include \"rust/ee_ge.h\"\n#include \"rust/ee_won.h\"\n"));
        assert!(!headers["ee_ge"].contains("extern \"C\" {\n\n}"));
    }

    #[test]
    fn the_union_of_the_headers_is_the_single_header() {
        let headers = split(SINGLE, &defs(), &renames(), &BTreeMap::new(), "// note").unwrap();
        assert_eq!(union_of(&headers).unwrap(), sorted_items(SINGLE));
    }

    #[test]
    fn headers_that_include_each_other_are_refused() {
        let deps: BTreeMap<&str, BTreeSet<&str>> =
            [("a", ["b"].into_iter().collect()), ("b", ["a"].into_iter().collect())].into_iter().collect();
        assert!(check_acyclic(&deps).unwrap_err().contains("a -> b -> a"));
    }
}

#[cfg(test)]
mod records_tests {
    use super::*;

    const SINGLE: &str = r#"// note

#ifndef EE_RUST_H
#define EE_RUST_H

#include <stdint.h>

/// A point.
struct EeGe3DPoint {
  float mX;
};

enum class Kind : uint8_t {
  A,
};

struct EeUnitsFields {
  EeGe3DPoint mPos;
  Kind mKind;
};

struct EeUnitsArgs {
  int32_t a;
};

extern "C" {

void ee_units_go(EeUnitsFields *f, EeUnitsArgs a);

}  // extern "C"

#endif  // EE_RUST_H
"#;

    const UNITS_RS: &str = r#"pub enum Kind { A }
pub struct EeUnitsFields { pub pos: Ge3DPoint, pub kind: Kind }
pub struct EeUnitsArgs { pub a: i32 }
pub extern "C" fn ee_units_go(f: *mut EeUnitsFields, a: EeUnitsArgs) {}
"#;

    fn defs() -> HashMap<String, Vec<Def>> {
        let mut defs = HashMap::new();
        scan_text("base/math", "pub struct Ge3DPoint { pub x: f32 }", &mut defs);
        scan_text("sim/units", UNITS_RS, &mut defs);
        defs
    }

    fn headers() -> BTreeMap<String, String> {
        let renames = [("Ge3DPoint".to_string(), "EeGe3DPoint".to_string())].into_iter().collect();
        let roots = [("sim".to_string(), ["EeUnitsFields".to_string()].into_iter().collect())].into_iter().collect();
        split(SINGLE, &defs(), &renames, &roots, "// note").unwrap()
    }

    #[test]
    fn records_rs_lists_its_uses_and_definitions() {
        let text = r#"//! Doc `Ignored`.
pub use crate::a::{B, C as D};
pub use crate::e::F;
#[repr(C)]
pub struct G { pub x: u8 }
"#;
        let names: Vec<String> = records_listed(text).into_iter().collect();
        assert_eq!(names, ["B", "C", "F", "G"]);
    }

    #[test]
    fn a_record_and_the_types_it_names_go_to_the_records_header() {
        let headers = headers();
        let records = &headers["sim/records"];
        assert!(records.contains("struct EeUnitsFields {\n  GE3DPoint mPos;\n  Kind mKind;\n};"), "{records}");
        assert!(records.contains("enum class Kind"));
        assert!(
            records.contains("#include \"rust/base/records.h\"\n#include \"lle/LowLevelEngineCore.h\"\n"),
            "{records}"
        );
        assert!(records.contains("#include \"lle/GE3DPoint.h\"\n"), "{records}");
        assert!(records.contains("static_assert(sizeof(GE3DPoint) == sizeof(EeGe3DPoint)"));
        assert!(!records.contains("extern \"C\""));
        assert!(headers["base/records"].contains("struct EeGe3DPoint {"));
        let units = &headers["sim/units"];
        assert!(units.contains("#include \"rust/sim/records.h\""));
        assert!(units.contains("struct EeUnitsArgs") && units.contains("void ee_units_go("));
        assert!(!headers["base/math"].contains("struct"), "its one type is a record's");
    }

    #[test]
    fn the_records_headers_declare_what_the_single_header_declares() {
        assert_eq!(union_of(&headers()).unwrap(), sorted_items(SINGLE));
    }

    #[test]
    fn a_listed_record_the_header_lacks_is_an_error() {
        let roots = [("sim".to_string(), ["EeNoSuch".to_string()].into_iter().collect())].into_iter().collect();
        let err = split(SINGLE, &defs(), &HashMap::new(), &roots, "// note").unwrap_err();
        assert!(err.contains("EeNoSuch"), "{err}");
    }

    #[test]
    fn a_type_another_module_names_goes_to_the_records_header_and_an_emptied_module_keeps_a_header() {
        let single = SINGLE.replace(
            "void ee_units_go(EeUnitsFields *f, EeUnitsArgs a);\n",
            "void ee_units_go(EeUnitsFields *f, EeUnitsArgs a);\n\nvoid ee_app_go(EeUnitsArgs a);\n",
        );
        let mut defs = defs();
        scan_text("sim/app", "pub extern \"C\" fn ee_app_go(a: EeUnitsArgs) {}", &mut defs);
        let renames = [("Ge3DPoint".to_string(), "EeGe3DPoint".to_string())].into_iter().collect();
        let headers = split(&single, &defs, &renames, &BTreeMap::new(), "// note").unwrap();
        assert!(headers["sim/records"].contains("struct EeUnitsArgs"), "{:?}", headers.keys());
        assert!(headers["sim/app"].contains("#include \"rust/sim/records.h\""));
        assert!(!headers["sim/app"].contains("rust/sim/units.h"), "a function header includes only records headers");
        assert!(headers["base/math"].contains("#include \"rust/base/records.h\""), "its one type went to records");
    }

    #[test]
    fn words_are_replaced_whole() {
        let line = "  EeGe3DPoint mA; EeGe3DPointX mB;";
        assert_eq!(replace_word(line, "EeGe3DPoint", "GE3DPoint"), "  GE3DPoint mA; EeGe3DPointX mB;");
    }
}

#[cfg(test)]
mod module_tests {
    use super::*;

    #[test]
    fn a_file_belongs_to_its_crates_top_level_module() {
        let crates = Path::new("/r/rust/crates");
        let h = |k: &str, f: &str| header_of(crates, k, &crates.join(k).join("src").join(f));
        assert_eq!(h("ee_sim", "units/mod.rs"), "sim/units");
        assert_eq!(h("ee_sim", "units/world/save.rs"), "sim/units");
        assert_eq!(h("ee_base", "png.rs"), "base/png");
        assert_eq!(h("ee_ui", "lib.rs"), "ui/ui");
        assert_eq!(h("ee_game", "lib.rs"), UMBRELLA);
        assert_eq!(h("ee_won_net", "opobj.rs"), "won_net/hostop");
        assert_eq!(module_of("sim/units"), "ee_sim::units");
        assert_eq!(module_of("ui/ui"), "the crate ee_ui's root");
        assert_eq!(guard("sim/units"), "EE_RUST_SIM_UNITS_H");
    }
}
