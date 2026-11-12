//! Splits cbindgen's one header into a header per crate, so that a C++ unit depends only on the crates it uses.
//!
//! cbindgen runs once over `ee_rust` (renames, the order of declarations and the dropped duplicates stay those of one
//! shared header); this module cuts its output into its top-level items and gives each to the crate that defines it,
//! found in the crates' sources. A name defined in more than one crate goes to the crate whose definition matches the
//! item (the doc comment, then the field and variant names, then the numbers); identical ones (bare `OK = 0`
//! constants) and the items of `ee_rust` itself go to the common header. A header includes the headers of the crates
//! whose items its items name. The text of every item is cbindgen's, unchanged.

use std::collections::{BTreeMap, BTreeSet, HashMap};
use std::path::Path;

/// The header with the items of `ee_rust` itself and the names no one crate owns.
pub const COMMON: &str = "ee_rust_common";
/// The umbrella that includes every header.
pub const UMBRELLA: &str = "ee_rust";

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

/// Every definition in the sources of the named crates (`rust/crates/<name>/src`), test sources left out.
pub fn scan(crates_dir: &Path, crates: &[String]) -> HashMap<String, Vec<Def>> {
    let mut defs: HashMap<String, Vec<Def>> = HashMap::new();
    for krate in crates {
        let mut files = Vec::new();
        rust_files(&crates_dir.join(krate).join("src"), &mut files);
        for file in files.iter().filter(|f| !is_test_source(f)) {
            let Ok(text) = std::fs::read_to_string(file) else { continue };
            scan_text(krate, &text, &mut defs);
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
    note: &str,
) -> Result<BTreeMap<String, String>, String> {
    let parsed = parse(single)?;
    let all: Vec<&Item> = parsed.types.iter().chain(&parsed.functions).collect();
    let owner = owners(&all, defs, renames);
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
        out.insert(header.to_string(), render(header, note, &parsed.sys_includes, includes, &types, &functions));
    }
    let mut umbrella = format!(
        "{note}\n// Every crate's C interface. Include only the headers of the crates a file uses (rust/<crate>.h).\n\n\
         #ifndef EE_RUST_H\n#define EE_RUST_H\n\n"
    );
    for header in out.keys() {
        umbrella.push_str(&format!("#include \"{header}.h\"\n"));
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
                return Err(format!("the crates' headers include each other: {}", path.join(" -> ")));
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
    format!("EE_RUST_{}_H", header.to_ascii_uppercase())
}

/// One header's text: the preamble, the types in cbindgen's order, the functions in an `extern "C"` block.
pub fn render(
    header: &str,
    note: &str,
    sys_includes: &[String],
    includes: &BTreeSet<&str>,
    types: &[&Item],
    functions: &[&Item],
) -> String {
    let what = if header == COMMON {
        "The C interface of rust/crates/ee_rust and the names more than one crate defines alike".to_string()
    } else {
        format!("The C interface of rust/crates/{header}")
    };
    let guard = guard(header);
    let mut text =
        format!("{note}\n// {what} (rust/ee_rust.h includes every crate's).\n\n#ifndef {guard}\n#define {guard}\n\n");
    for line in sys_includes {
        text.push_str(line);
        text.push('\n');
    }
    for include in includes {
        text.push_str(&format!("#include \"{include}.h\"\n"));
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
        for item in parsed.types.into_iter().chain(parsed.functions) {
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
        assert_eq!(owner, ["ee_rust_common", "ee_ge", "ee_ge", "ee_won", "ee_won", "ee_ge"]);
    }

    #[test]
    fn each_header_includes_the_headers_its_items_name() {
        let headers = split(SINGLE, &defs(), &renames(), "// note").unwrap();
        let won = &headers["ee_won"];
        assert!(won.contains("#include \"ee_ge.h\"\n"));
        assert!(!headers["ee_ge"].contains("#include \"ee_won.h\""));
        assert!(won.contains(
            "extern \"C\" {\n\n/// Makes one.\nEeWonThing ee_won_make(EeGePoint at);\n\n}  // extern \"C\"\n"
        ));
        assert!(headers["ee_rust"].contains("#include \"ee_rust_common.h\"\n#include \"ee_won.h\"\n"));
        assert!(!headers["ee_ge"].contains("extern \"C\" {\n\n}"));
    }

    #[test]
    fn the_union_of_the_headers_is_the_single_header() {
        let headers = split(SINGLE, &defs(), &renames(), "// note").unwrap();
        assert_eq!(union_of(&headers).unwrap(), sorted_items(SINGLE));
    }

    #[test]
    fn headers_that_include_each_other_are_refused() {
        let deps: BTreeMap<&str, BTreeSet<&str>> =
            [("a", ["b"].into_iter().collect()), ("b", ["a"].into_iter().collect())].into_iter().collect();
        assert!(check_acyclic(&deps).unwrap_err().contains("a -> b -> a"));
    }
}
