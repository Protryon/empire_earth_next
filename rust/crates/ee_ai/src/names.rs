//! The next engine's name tables (`EECPNext.cpp`, `EECPNextLua.cpp`): the verbs' result codes and reasons, the
//! events' kinds, the resources, and the classes and techs by their data ids (dbobjects / dbtechtree "id"), with
//! "<table>_<key>" for a row without one.

use std::collections::BTreeMap;
use std::ffi::CStr;

/// `EECPNextCodeName`: kNextDone 0, kNextQueued 1, any other code "refused".
pub fn code_name(code: i64) -> &'static CStr {
    match code {
        0 => c"done",
        1 => c"queued",
        _ => c"refused",
    }
}

/// `EECPNextReasonName` (EECPNextReason 1..13; "-" for any other).
pub fn reason_name(reason: i64) -> &'static CStr {
    match reason {
        1 => c"no_site",
        2 => c"no_producer",
        3 => c"cannot_afford",
        4 => c"not_in_epoch",
        5 => c"unknown_class",
        6 => c"bad_argument",
        7 => c"no_units",
        8 => c"requirements",
        9 => c"busy",
        10 => c"no_target",
        11 => c"unsafe",
        12 => c"unexplored",
        13 => c"no_dropoff",
        _ => c"-",
    }
}

/// The events' kinds (EECPNextEventKind 0..4): `unknown` is what any other kind is called ("unknown" in the script's
/// view, "?" in the trace).
pub fn event_name(kind: i64, unknown: &'static CStr) -> &'static CStr {
    match kind {
        0 => c"unit_lost",
        1 => c"production_lost",
        2 => c"advance_lost",
        3 => c"no_site",
        4 => c"group_wants",
        _ => unknown,
    }
}

/// The resources by index (food, wood, stone, gold, iron).
pub const RESOURCES: [&CStr; 5] = [c"food", c"wood", c"stone", c"gold", c"iron"];

/// A table's names by key and keys by name (`EECPNextNames`).
#[derive(Clone, Debug, Default)]
pub struct Names {
    names: BTreeMap<i64, Vec<u8>>,
    keys: BTreeMap<Vec<u8>, i64>,
    prefix: Vec<u8>,
}

impl Names {
    /// The table of the rows (key, data id) with the prefix of a key without an id ("dbobjects_"). A repeated key
    /// keeps its first id; a repeated id names its largest key.
    pub fn new(rows: impl IntoIterator<Item = (i64, Vec<u8>)>, prefix: &[u8]) -> Names {
        let mut names = BTreeMap::new();
        for (k, n) in rows {
            names.entry(k).or_insert(n);
        }
        let mut keys = BTreeMap::new();
        for (k, n) in &names {
            keys.insert(n.clone(), *k);
        }
        Names { names, keys, prefix: prefix.to_vec() }
    }

    /// The name of a key: its id, else "<prefix><key>".
    pub fn name_of(&self, key: i64) -> std::borrow::Cow<'_, [u8]> {
        match self.names.get(&key) {
            Some(n) => std::borrow::Cow::Borrowed(n.as_slice()),
            None => std::borrow::Cow::Owned([self.prefix.as_slice(), key.to_string().as_bytes()].concat()),
        }
    }

    /// The key of a name: an id's, or "<prefix><digits>" for a key without an id -> -1 for none.
    pub fn key_of(&self, name: &[u8]) -> i64 {
        if let Some(&k) = self.keys.get(name) {
            return k;
        }
        let p = self.prefix.len();
        if name.len() > p && name.starts_with(&self.prefix) {
            let mut key: i64 = 0;
            for &c in &name[p..] {
                if !c.is_ascii_digit() || key > 100_000_000 {
                    return -1;
                }
                key = key * 10 + (c - b'0') as i64;
            }
            return if self.names.contains_key(&key) { -1 } else { key };
        }
        -1
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn tables() {
        assert_eq!(code_name(0), c"done");
        assert_eq!(code_name(-3), c"refused");
        assert_eq!(reason_name(12), c"unexplored");
        assert_eq!(reason_name(13), c"no_dropoff");
        assert_eq!(reason_name(0), c"-");
        assert_eq!(event_name(4, c"?"), c"group_wants");
        assert_eq!(event_name(5, c"unknown"), c"unknown");
    }

    #[test]
    fn names_and_keys() {
        let t = Names::new(
            [(3, b"citizen".to_vec()), (7, b"b_barracks".to_vec()), (9, b"citizen".to_vec())],
            b"dbobjects_",
        );
        assert_eq!(&*t.name_of(3), b"citizen");
        assert_eq!(&*t.name_of(-4), b"dbobjects_-4");
        assert_eq!(t.key_of(b"citizen"), 9);
        assert_eq!(t.key_of(b"dbobjects_12"), 12);
        assert_eq!(t.key_of(b"dbobjects_7"), -1);
        assert_eq!(t.key_of(b"dbobjects_"), -1);
        assert_eq!(t.key_of(b"dbobjects_1x"), -1);
        assert_eq!(t.key_of(b"dbobjects_1000000009"), 1_000_000_009);
        assert_eq!(t.key_of(b"dbobjects_10000000090"), -1);
        assert_eq!(t.key_of(b"x"), -1);
    }
}
