//! Readers for the generated text structs (`generated::specs_gen::text_rows`): one record's keys typed with the
//! compiler's grammar. The compiler itself is spec-driven (`content.rs`); these structs are the typed view of a row for
//! code that does not want to walk the spec (the mod layer, tools).

use crate::msg;
use crate::node::{Kind, Node};
use crate::spec::Type;
use crate::text::{elems, encode_text, number, parse_int, Failure, Getter, Res};

/// An integer key (`row`, ...), None when absent.
pub fn int(g: &mut Getter, key: &str, ty: Type) -> Res<Option<i64>> {
    let v = g.get(key);
    let Some(v) = v else { return Ok(None) };
    if v.kind != Kind::Scalar {
        return Err(Failure::new(msg!(&g.where_, ": ", key, ": not a number")));
    }
    parse_int(&v.text, ty).map(Some).map_err(|e| Failure::new(msg!(&g.where_, ": ", key, ": ", &e.text)))
}

/// The raw text of a key (a reference or an id), not resolved.
pub fn raw(g: &mut Getter, key: &str) -> Res<Option<Vec<u8>>> {
    let v = g.get(key);
    match v {
        None => Ok(None),
        Some(n) if n.kind == Kind::Scalar => Ok(Some(n.text.to_vec())),
        Some(_) => Err(Failure::new(msg!(&g.where_, ": ", key, ": not a scalar"))),
    }
}

/// A text key, encoded to CP-1252.
pub fn text(g: &mut Getter, key: &str) -> Res<Option<Vec<u8>>> {
    match raw(g, key)? {
        None => Ok(None),
        Some(t) => encode_text(&t).map(Some).map_err(|e| Failure::new(msg!(&g.where_, ": ", key, ": ", &e.text))),
    }
}

/// A numeric value converted to the field's Rust type.
pub trait FromBits {
    fn from_bits(bits: u32, ty: Type) -> Self;
}
macro_rules! from_bits_int {
    ($($t:ty),+) => { $(impl FromBits for $t {
        fn from_bits(bits: u32, _ty: Type) -> Self { bits as $t }
    })+ };
}
from_bits_int!(i8, u8, i16, u16, i32, u32);
impl FromBits for f32 {
    fn from_bits(bits: u32, _ty: Type) -> Self {
        f32::from_bits(bits)
    }
}

/// A numeric field: None when absent; the record's value when present.
pub fn num<T: FromBits>(g: &mut Getter, key: &str, ty: Type) -> Res<Option<T>> {
    let w = g.where_;
    let v = g.get(key);
    let Some(v) = v else { return Ok(None) };
    number(key, ty, Some(v), &w).map(|bits| Some(T::from_bits(bits, ty)))
}

/// An array of a numeric type; a missing element is None.
pub fn num_array<T: FromBits>(g: &mut Getter, key: &str, ty: Type, n: usize) -> Res<Vec<Option<T>>> {
    let w = g.where_;
    let v = g.get(key);
    let e = elems(v, n, &w, key)?;
    let mut out = Vec::with_capacity(n);
    for item in e {
        out.push(match item {
            Some(node) => Some(T::from_bits(number(key, ty, Some(node), &w)?, ty)),
            None => None,
        });
    }
    Ok(out)
}

/// An array of texts (the lead strings).
pub fn text_array(g: &mut Getter, key: &str, n: usize) -> Res<Vec<Option<Vec<u8>>>> {
    raw_array_map(g, key, n, true)
}

/// An array of raw texts (references).
pub fn raw_array(g: &mut Getter, key: &str, n: usize) -> Res<Vec<Option<Vec<u8>>>> {
    raw_array_map(g, key, n, false)
}

fn raw_array_map(g: &mut Getter, key: &str, n: usize, encode: bool) -> Res<Vec<Option<Vec<u8>>>> {
    let w = g.where_;
    let v = g.get(key);
    let e = elems(v, n, &w, key)?;
    let mut out = Vec::with_capacity(n);
    for item in e {
        out.push(match item {
            None => None,
            Some(node) if node.kind == Kind::Scalar => Some(if encode {
                encode_text(&node.text).map_err(|e| Failure::new(msg!(&w, ": ", key, ": ", &e.text)))?
            } else {
                node.text.to_vec()
            }),
            Some(_) => return Err(Failure::new(msg!(&w, ": ", key, ": not a scalar"))),
        });
    }
    Ok(out)
}

/// A pair_f32 array: n pairs of optional floats.
pub fn pairs(g: &mut Getter, key: &str, n: usize) -> Res<Vec<[Option<f32>; 2]>> {
    let w = g.where_;
    let v = g.get(key);
    let e = elems(v, n, &w, key)?;
    let mut out = Vec::with_capacity(n);
    for item in e {
        let p = elems(item, 2, &w, key)?;
        let mut pair = [None, None];
        for (i, x) in p.iter().enumerate().take(2) {
            pair[i] = match x {
                Some(node) => Some(f32::from_bits(number(key, Type::F32, Some(node), &w)?)),
                None => None,
            };
        }
        out.push(pair);
    }
    Ok(out)
}

/// A trailing list as raw texts.
pub fn raw_list(g: &mut Getter, key: &str) -> Res<Vec<Vec<u8>>> {
    let v = g.get(key);
    let Some(v) = v else { return Ok(Vec::new()) };
    if v.kind != Kind::List {
        return Err(Failure::new(msg!(&g.where_, ": ", key, " must be a list")));
    }
    let mut out = Vec::with_capacity(v.items.len());
    for x in &v.items {
        if x.kind != Kind::Scalar {
            return Err(Failure::new(msg!(&g.where_, ": ", key, ": a list where a number belongs")));
        }
        out.push(x.text.to_vec());
    }
    Ok(out)
}

/// A nested value kept as its tree (text-only keys: slots, music, citizen ...).
pub fn node_of(g: &mut Getter, key: &str) -> Option<Node> {
    g.get(key).cloned()
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::generated::specs_gen::text_rows::*;
    use crate::text::parse_yaml;

    #[test]
    fn dbworld_row_typed() {
        let n = parse_yaml(b"row: 3\nid: alpha\nmFloatValue: 1.5\nmLongValue: 7\n").unwrap();
        let r = DbworldText::from_node(&n, b"w").unwrap();
        assert_eq!(r.row, Some(3));
        assert_eq!(r.id.as_deref(), Some(&b"alpha"[..]));
        assert_eq!(r.float_value, Some(1.5));
        assert_eq!(r.long_value, Some(7));
        assert_eq!(r.long_value_2, None);
    }

    #[test]
    fn unknown_key_is_an_error() {
        let n = parse_yaml(b"nosuch: 1\n").unwrap();
        let e = DbworldText::from_node(&n, b"where").unwrap_err();
        assert_eq!(e.text, b"where: unknown key(s) nosuch".to_vec());
    }

    #[test]
    fn bad_numbers_name_the_field() {
        let n = parse_yaml(b"mLongValue: x\n").unwrap();
        let e = DbworldText::from_node(&n, b"w").unwrap_err();
        assert_eq!(e.text, b"w: mLongValue: 'x' is not an integer".to_vec());
    }

    #[test]
    fn events_list_and_references_stay_text() {
        let n = parse_yaml(b"id: x\neffects: [a, b]\n").unwrap();
        let r = DbeventsText::from_node(&n, b"w").unwrap();
        assert_eq!(r.effects, vec![b"a".to_vec(), b"b".to_vec()]);
        let n = parse_yaml(b"id: x\neffects: [[1]]\n").unwrap();
        assert!(DbeventsText::from_node(&n, b"w").is_err());
    }

    #[test]
    fn epochs_keep_their_text_only_keys() {
        let n = parse_yaml(b"id: x\nslots: 3\nmusic: [a]\n").unwrap();
        let r = EpochsText::from_node(&n, b"w").unwrap();
        assert_eq!(r.slots.unwrap().text, b"3".to_vec());
        assert_eq!(r.music.unwrap().items.len(), 1);
        assert!(r.citizen.is_none());
    }

    #[test]
    fn text_keys_are_encoded() {
        let n = parse_yaml("label: \"caf\u{e9}\"\n".as_bytes()).unwrap();
        let r = DbsoundsText::from_node(&n, b"w").unwrap();
        assert_eq!(r.label.as_deref(), Some(&b"caf\xe9"[..]));
    }

    #[test]
    fn arrays_and_gaps() {
        let n = parse_yaml(b"a: [1, ~, 3]\n").unwrap();
        let mut g = Getter::new(&n, b"w");
        let v = num_array::<i32>(&mut g, "a", Type::S32, 4).unwrap();
        assert_eq!(v, vec![Some(1), None, Some(3), None]);
        let n = parse_yaml(b"a: [[1, 2], [3]]\n").unwrap();
        let mut g = Getter::new(&n, b"w");
        let p = pairs(&mut g, "a", 2).unwrap();
        assert_eq!(p[0], [Some(1.0), Some(2.0)]);
        assert_eq!(p[1], [Some(3.0), None]);
    }

    #[test]
    fn scalars_are_required_for_raw_values() {
        let n = parse_yaml(b"id: [1]\n").unwrap();
        let mut g = Getter::new(&n, b"w");
        assert!(raw(&mut g, "id").is_err());
        let mut g = Getter::new(&n, b"w");
        assert_eq!(int(&mut g, "id", Type::S32).unwrap_err().text, b"w: id: not a number".to_vec());
    }
}
