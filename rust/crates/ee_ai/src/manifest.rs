//! The AI manifest (`Data/ai/ais.json`; `EECPEngine.cpp` `EECPAIReadManifest`, docs/AI_LUA_PLAN.md "AI manifest"):
//! the document's entries checked one by one, a bad entry skipped with a log line. The C++ reads the file, answers
//! whether a script's file exists and writes the log lines; the checks, their order and the lines' text are here.

use crate::json::{self, Value};

/// The manifest's entries past this many are skipped (`kEECPMaxAIs`).
pub(crate) const MAX_AIS: usize = 64;

/// An AI of the manifest.
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct Entry {
    pub id: Vec<u8>,
    /// the lobby's text: UTF-8, a malformed byte as U+FFFD (the id when the entry has no name)
    pub name: Vec<u8>,
    pub script: Vec<u8>,
    pub description: Vec<u8>,
    pub random: bool,
    pub sees_all: bool,
    pub fills_garrison: bool,
}

/// What a read gives: the entries kept and the log lines, in order. `listed`: the document had its "ais" array (the
/// C++ then writes its summary line after these).
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct Manifest {
    pub entries: Vec<Entry>,
    pub log: Vec<Vec<u8>>,
    pub listed: bool,
}

/// `EECPAIIdOK`: 1 to 64 of a-z, 0-9 and _.
pub fn id_ok(id: &[u8]) -> bool {
    !id.is_empty() && id.len() <= 64 && id.iter().all(|&c| c.is_ascii_lowercase() || c.is_ascii_digit() || c == b'_')
}

/// `EECPAIModuleOK`: a Lua module name as require takes it (letters, digits and _, in segments separated by dots).
pub fn module_ok(name: &[u8]) -> bool {
    if name.is_empty() || name.len() > 96 {
        return false;
    }
    let mut start = true;
    for &c in name {
        if c == b'.' {
            if start {
                return false;
            }
            start = true;
        } else if c.is_ascii_alphanumeric() || c == b'_' {
            start = false;
        } else {
            return false;
        }
    }
    !start
}

/// `EECPAIToGameText`: the text up to an embedded NUL, a byte that is not part of a well-formed sequence as U+FFFD.
pub fn to_game_text(text: &[u8]) -> Vec<u8> {
    use ee_plat_core::utf8;
    let end = text.iter().position(|&c| c == 0).unwrap_or(text.len());
    let text = &text[..end];
    let mut out = Vec::with_capacity(text.len());
    let mut i = 0;
    while i < text.len() {
        let (mut c, n) = utf8::decode(&text[i..]);
        if (utf8::STRAY_BASE + 0x80..=utf8::STRAY_BASE + 0xFF).contains(&c) {
            c = utf8::REPLACEMENT;
        }
        utf8::push(&mut out, c);
        i += n;
    }
    out
}

fn cat(parts: &[&[u8]]) -> Vec<u8> {
    parts.concat()
}

/// A text as a C string argument prints (`%s` of `c_str()`): up to its first NUL.
fn cs(text: &[u8]) -> &[u8] {
    &text[..text.iter().position(|&c| c == 0).unwrap_or(text.len())]
}

/// The manifest of the document `text` read from `path`; `script_exists(module)` answers whether the module's file
/// is there (EE_AI_SCRIPTS's folder or Data\ai).
pub fn read(text: &[u8], path: &[u8], script_exists: &mut dyn FnMut(&[u8]) -> bool) -> Manifest {
    let mut m = Manifest::default();
    let root = match json::parse(text) {
        Ok(v) => v,
        Err(e) => {
            m.log.push(cat(&[path, b": ", e.as_bytes(), b": no AIs read (only classic and random)"]));
            return m;
        }
    };
    let list = match root.field(b"ais") {
        Some(Value::Array(items)) => items,
        _ => {
            m.log.push(cat(&[
                path,
                b": the document must be an object with an array \"ais\": no AIs read (only classic and random)",
            ]));
            return m;
        }
    };
    m.listed = true;
    for (k, e) in list.iter().enumerate() {
        let k_text = k.to_string();
        let where_ = cat(&[path, b": ais[", k_text.as_bytes(), b"]"]);
        let fields = match e {
            Value::Object(f) => f,
            _ => {
                m.log.push(cat(&[&where_, b": not an object: skipped"]));
                continue;
            }
        };
        let mut ai = Entry::default();
        let mut problem: Vec<u8> = Vec::new();
        for (key, v) in fields {
            let key = key.as_slice();
            match key {
                b"id" | b"name" | b"script" | b"description" => {
                    let Value::String(s) = v else {
                        problem = cat(&[key, b" must be a string"]);
                        break;
                    };
                    let slot = match key {
                        b"id" => &mut ai.id,
                        b"name" => &mut ai.name,
                        b"script" => &mut ai.script,
                        _ => &mut ai.description,
                    };
                    *slot = s.clone();
                }
                b"random" | b"sees_all" | b"fills_garrison" => {
                    let Value::Bool(b) = v else {
                        problem = cat(&[key, b" must be true or false"]);
                        break;
                    };
                    match key {
                        b"random" => ai.random = *b,
                        b"sees_all" => ai.sees_all = *b,
                        _ => ai.fills_garrison = *b,
                    }
                }
                _ => m.log.push(cat(&[&where_, b": unknown key \"", cs(key), b"\" ignored"])),
            }
        }
        if problem.is_empty() && !id_ok(&ai.id) {
            problem = cat(&[b"id '", &ai.id, b"' must be 1 to 64 of a-z, 0-9 and _"]);
        }
        if problem.is_empty() && (ai.id == b"classic" || ai.id == b"random" || ai.id == b"next") {
            problem = cat(&[b"id '", &ai.id, b"' is reserved (classic, random, next)"]);
        }
        if problem.is_empty() && m.entries.iter().any(|o| o.id == ai.id) {
            problem = cat(&[b"id '", &ai.id, b"' is listed twice"]);
        }
        if problem.is_empty() && !module_ok(&ai.script) {
            problem = cat(&[
                b"script '",
                &ai.script,
                b"' is not a module name (letters, digits and _, in segments separated by dots)",
            ]);
        }
        if problem.is_empty() && !script_exists(&ai.script) {
            problem = cat(&[b"script '", &ai.script, b"': no such file under Data/ai"]);
        }
        if problem.is_empty() && m.entries.len() >= MAX_AIS {
            problem = format!("more than {MAX_AIS} AIs").into_bytes();
        }
        if !problem.is_empty() {
            m.log.push(cat(&[&where_, b": ", cs(&problem), b": skipped"]));
            continue;
        }
        ai.name = to_game_text(if ai.name.is_empty() { &ai.id } else { &ai.name });
        m.entries.push(ai);
    }
    m
}

#[cfg(test)]
mod tests {
    use super::*;

    fn read_all(text: &str) -> Manifest {
        read(text.as_bytes(), b"Data\\ai\\ais.json", &mut |m| !m.windows(7).any(|w| w == b"missing"))
    }

    #[test]
    fn entries_and_problems() {
        let m = read_all(
            r#"{"ais": [
                {"id": "boom", "script": "boom", "random": true, "x": 1},
                {"id": "Bad", "script": "a"},
                {"id": "next", "script": "a"},
                {"id": "boom", "script": "a"},
                {"id": "b", "script": "a..b"},
                {"id": "c", "script": "missing"},
                {"id": "d", "name": 3},
                7,
                {"id": "e", "name": "N\u0000x", "script": "e.main", "sees_all": true, "fills_garrison": false}
            ]}"#,
        );
        assert_eq!(m.entries.len(), 2);
        assert_eq!(m.entries[0].name, b"boom");
        assert!(m.entries[0].random);
        assert_eq!(m.entries[1].name, b"N");
        assert!(m.entries[1].sees_all);
        let log: Vec<String> = m.log.iter().map(|l| String::from_utf8_lossy(l).into_owned()).collect();
        assert_eq!(log[0], "Data\\ai\\ais.json: ais[0]: unknown key \"x\" ignored");
        assert_eq!(log[1], "Data\\ai\\ais.json: ais[1]: id 'Bad' must be 1 to 64 of a-z, 0-9 and _: skipped");
        assert_eq!(log[2], "Data\\ai\\ais.json: ais[2]: id 'next' is reserved (classic, random, next): skipped");
        assert_eq!(log[3], "Data\\ai\\ais.json: ais[3]: id 'boom' is listed twice: skipped");
        assert!(
            log[4].ends_with("is not a module name (letters, digits and _, in segments separated by dots): skipped")
        );
        assert_eq!(log[5], "Data\\ai\\ais.json: ais[5]: script 'missing': no such file under Data/ai: skipped");
        assert_eq!(log[6], "Data\\ai\\ais.json: ais[6]: name must be a string: skipped");
        assert_eq!(log[7], "Data\\ai\\ais.json: ais[7]: not an object: skipped");
        assert!(m.listed);
        // a NUL in a text ends it in the line, as the C string argument did
        let m = read_all(r#"{"ais": [{"id": "a\u0000b", "script": "e", "k\u0000x": 1}]}"#);
        assert_eq!(m.log[0], b"Data\\ai\\ais.json: ais[0]: unknown key \"k\" ignored");
        assert_eq!(m.log[1], b"Data\\ai\\ais.json: ais[0]: id 'a: skipped");
    }

    #[test]
    fn whole_document_errors() {
        let m = read_all("[]");
        assert_eq!(m.log.len(), 1);
        assert!(!m.listed);
        assert!(m.log[0].ends_with(b"an array \"ais\": no AIs read (only classic and random)"));
        let m = read_all("{\"ais\": [}");
        assert_eq!(
            m.log[0],
            b"Data\\ai\\ais.json: line 1: unexpected character: no AIs read (only classic and random)"
        );
    }

    #[test]
    fn game_text() {
        assert_eq!(to_game_text(b"a\xffb\xe2\x82"), "a\u{fffd}b\u{fffd}\u{fffd}".as_bytes());
        assert_eq!(to_game_text("é".as_bytes()), "é".as_bytes());
        assert_eq!(to_game_text(b"\xed\xa0\x80"), "\u{fffd}\u{fffd}\u{fffd}".as_bytes());
    }

    #[test]
    fn the_cap() {
        let items: Vec<String> = (0..66).map(|k| format!("{{\"id\": \"a{k}\", \"script\": \"s\"}}")).collect();
        let m = read_all(&format!("{{\"ais\": [{}]}}", items.join(",")));
        assert_eq!(m.entries.len(), MAX_AIS);
        assert_eq!(m.log.len(), 2);
        assert!(m.log[0].ends_with(b"ais[64]: more than 64 AIs: skipped"));
    }
}
