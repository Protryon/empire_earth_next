//! The content dump (docs/MODDING_PLAN.md gate G1): a db file's bytes, as the loaders read them, as canonical JSON of
//! every field the game reads, by its spec name, row by row. Rows the loaders skip are written as skipped and fields
//! no reader uses are left out, so the dump of a retail .dat and of the text tables compiled from it are identical.

use crate::msg;
use crate::spec::{find_table, Container, Field, Struct, Table, Type};
use crate::text::{Failure, Res};

struct Reader<'a> {
    data: &'a [u8],
    pos: usize,
    bad: bool,
}

impl<'a> Reader<'a> {
    fn take(&mut self, n: usize) -> Option<&'a [u8]> {
        match self.pos.checked_add(n) {
            Some(end) if !self.bad && end <= self.data.len() => {
                let s = &self.data[self.pos..end];
                self.pos = end;
                Some(s)
            }
            _ => {
                self.bad = true;
                None
            }
        }
    }

    fn u32(&mut self) -> u32 {
        self.take(4).map_or(0, |p| u32::from_le_bytes([p[0], p[1], p[2], p[3]]))
    }
}

fn le(p: &[u8], n: usize) -> u32 {
    let mut v = 0u32;
    for (k, b) in p.iter().enumerate().take(n) {
        v |= (*b as u32) << (8 * k);
    }
    v
}

fn int_value(p: &[u8], ty: Type) -> i64 {
    let v = le(p, ty.size());
    match ty {
        Type::S8 => v as i8 as i64,
        Type::S16 => v as i16 as i64,
        Type::S32 => v as i32 as i64,
        _ => v as i64,
    }
}

fn push_str(out: &mut Vec<u8>, p: &[u8], n: usize) {
    out.push(b'"');
    for &c in p.iter().take(n) {
        if c == 0 {
            break;
        }
        if c == b'"' || c == b'\\' {
            out.push(b'\\');
            out.push(c);
        } else if !(0x20..0x7f).contains(&c) {
            out.extend_from_slice(format!("\\u{c:04x}").as_bytes()); // the byte (CP-1252), not decoded: exact and plain ASCII
        } else {
            out.push(c);
        }
    }
    out.push(b'"');
}

fn push_value(out: &mut Vec<u8>, f: &Field, p: &[u8]) {
    if f.ty == Type::F32 {
        out.extend_from_slice(format!("\"0x{:08x}\"", le(p, 4)).as_bytes()); // the bit pattern: exact
    } else {
        out.extend_from_slice(int_value(p, f.ty).to_string().as_bytes());
    }
}

fn push_field(out: &mut Vec<u8>, f: &Field, rec: &[u8]) {
    out.push(b'"');
    out.extend_from_slice(f.name.as_bytes());
    out.extend_from_slice(b"\":");
    let at = &rec[f.offset as usize..];
    if f.ty == Type::Str {
        push_str(out, at, f.count as usize);
    } else if f.ty == Type::PairF32 {
        out.push(b'[');
        for k in 0..f.count as usize {
            let comma = if k > 0 { "," } else { "" };
            out.extend_from_slice(
                format!("{comma}[\"0x{:08x}\",\"0x{:08x}\"]", le(&at[8 * k..], 4), le(&at[8 * k + 4..], 4)).as_bytes(),
            );
        }
        out.push(b']');
    } else if f.count == 1 {
        push_value(out, f, at);
    } else {
        out.push(b'[');
        for k in 0..f.count as usize {
            if k > 0 {
                out.push(b',');
            }
            push_value(out, f, &at[f.ty.size() * k..]);
        }
        out.push(b']');
    }
}

fn find_field(st: &'static Struct, name: &str) -> &'static Field {
    st.fields.iter().find(|f| f.name == name).expect("the field is in the record")
}

/// The rows the loaders skip before reading their fields (docs/DATA_FORMATS.md 8.2): written as skipped.
fn skipped(t: &Table, rec: &[u8]) -> bool {
    let st = t.record;
    match t.name {
        "dbobjects" => {
            int_value(&rec[find_field(st, "mIsEnabled").offset as usize..], Type::Bool) == 0
                || int_value(&rec[find_field(st, "mUnitType").offset as usize..], Type::S32) == -1
        }
        "dbuiforms" => int_value(&rec[find_field(st, "mFormID").offset as usize..], Type::S32) <= 0,
        "dbterrain" => int_value(&rec[find_field(st, "mID").offset as usize..], Type::S32) < 0,
        _ => false,
    }
}

/// `fields` holds the field names as a blank-separated list.
fn in_list(fields: &str, name: &str) -> bool {
    fields.split(' ').any(|f| f == name)
}

struct RowArgs<'a> {
    row: i64,
    rec: &'a [u8],
    lead: &'a [(&'a [u8], u32)],
    list: Option<&'a [u32]>,
    epoch_row: bool,
    epoch_index: i64,
}

fn push_row(out: &mut Vec<u8>, t: &Table, a: &RowArgs) {
    // dbworld: a row's readers use only these fields; a row with none has no reader (not dumped)
    let all = t.row_names.is_empty();
    let only = t.row_names.iter().find(|r| r.row as i64 == a.row).map(|r| r.fields);
    if !all && only.is_none() {
        return;
    }
    out.extend_from_slice(format!("\n  {{\"row\":{}", a.row).as_bytes());
    if skipped(t, a.rec) {
        out.extend_from_slice(b",\"skipped\":true},");
        return;
    }
    let st = t.record;
    for (s, len) in a.lead {
        out.extend_from_slice(b",\"lead\":");
        // the lead string as a C string up to its first NUL (the Rust side stops at the NUL via push_str)
        push_str(out, s, *len as usize);
    }
    for f in st.fields {
        if f.name == "label" {
            continue; // text only: the game never reads it
        }
        if let Some(fields) = only {
            if !in_list(fields, f.name) {
                continue;
            }
        }
        if !a.epoch_row && t.container == Container::Techtree && (f.name == "mFirstNodeID" || f.name == "mTitleID") {
            continue; // read from epoch records only
        }
        out.push(b',');
        if a.epoch_row && f.name == "mTitleID" && int_value(&a.rec[f.offset as usize..], Type::S32) == 0 {
            out.extend_from_slice(format!("\"mTitleID\":{}", 30145 + a.epoch_index).as_bytes()); // what the game uses for 0
            continue;
        }
        push_field(out, f, a.rec);
    }
    if let Some(list) = a.list {
        out.extend_from_slice(b",\"list\":[");
        for (k, v) in list.iter().enumerate() {
            if k > 0 {
                out.push(b',');
            }
            let shown: i64 = if t.list_signed { *v as i32 as i64 } else { *v as i64 };
            out.extend_from_slice(shown.to_string().as_bytes());
        }
        out.push(b']');
    }
    out.extend_from_slice(b"},");
}

fn lead_count(t: &Table) -> usize {
    let mut total = 0;
    for s in t.lead {
        let n: usize = s.find('[').map_or(0, |b| s[b + 1..].trim_end_matches(']').parse().unwrap_or(0));
        total += if n == 0 {
            1
        } else if s.contains('|') {
            2 * n
        } else {
            n
        };
    }
    total
}

/// Appends the content dump of a table's db file bytes (`"name":[...]`) to `out`.
pub fn dump_table(table_name: &str, data: &[u8], out: &mut Vec<u8>) -> Res<()> {
    let found = find_table(table_name.as_bytes()).filter(|t| !t.text_only);
    let Some(t) = found else {
        return Err(Failure::new(msg!(table_name, ": not a db table with a text form")));
    };
    let size = t.record.size as usize;
    let mut r = Reader { data, pos: 0, bad: false };
    let mut text: Vec<u8> = format!("\"{}\":[", t.name).into_bytes();
    let read_list = |r: &mut Reader, list: &mut Vec<u32>| {
        list.clear();
        let n = r.u32();
        let mut k = 0;
        while k < n && !r.bad {
            list.push(r.u32());
            k += 1;
        }
    };
    let mut list: Vec<u32> = Vec::new();
    let no_lead: Vec<(&[u8], u32)> = Vec::new();
    if t.container == Container::Techtree {
        let mut epoch: i64 = 0;
        while r.pos < data.len() && !r.bad {
            let n = r.u32();
            let rec = r.take(size);
            read_list(&mut r, &mut list);
            if r.bad {
                break;
            }
            text.extend_from_slice(format!("\n {{\"epoch\":{epoch},\"rows\":[").as_bytes());
            epoch += 1;
            let args = RowArgs {
                row: -1,
                rec: rec.unwrap(),
                lead: &no_lead,
                list: Some(&list),
                epoch_row: true,
                epoch_index: epoch - 1,
            };
            push_row(&mut text, t, &args);
            let mut k: u32 = 0;
            while k < n && !r.bad {
                let rec = r.take(size);
                read_list(&mut r, &mut list);
                if !r.bad {
                    let args = RowArgs {
                        row: k as i64,
                        rec: rec.unwrap(),
                        lead: &no_lead,
                        list: Some(&list),
                        epoch_row: false,
                        epoch_index: 0,
                    };
                    push_row(&mut text, t, &args);
                }
                k += 1;
            }
            if text.last() == Some(&b',') {
                text.pop();
            }
            text.extend_from_slice(b"]},");
        }
    } else if t.container == Container::Fixed || t.container == Container::Events {
        let mut i: i64 = 0;
        while r.pos < data.len() && !r.bad {
            let rec = r.take(size);
            if t.container == Container::Events {
                read_list(&mut r, &mut list);
            }
            if !r.bad {
                let events = t.container == Container::Events;
                let args = RowArgs {
                    row: i,
                    rec: rec.unwrap(),
                    lead: &no_lead,
                    list: if events { Some(&list) } else { None },
                    epoch_row: false,
                    epoch_index: 0,
                };
                push_row(&mut text, t, &args);
            }
            i += 1;
        }
    } else {
        let n = r.u32();
        let leads = if t.container == Container::Lead { lead_count(t) } else { 0 };
        let mut i: u32 = 0;
        while i < n && !r.bad {
            let mut lead: Vec<(&[u8], u32)> = Vec::new();
            for _ in 0..leads {
                let len = r.u32();
                let s = r.take(len as usize);
                lead.push((s.unwrap_or(&[]), len));
            }
            let rec = r.take(size);
            if !r.bad {
                let args = RowArgs {
                    row: i as i64,
                    rec: rec.unwrap(),
                    lead: &lead,
                    list: None,
                    epoch_row: false,
                    epoch_index: 0,
                };
                push_row(&mut text, t, &args);
            }
            i += 1;
        }
    }
    if r.bad || r.pos != data.len() {
        return Err(Failure::new(msg!(table_name, ": the db file is truncated or has bytes after the last record")));
    }
    if text.last() == Some(&b',') {
        text.pop();
    }
    out.extend_from_slice(&text);
    out.extend_from_slice(b"\n]");
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::content::compile_table;
    use crate::spec::table;

    #[test]
    fn dumps_a_compiled_table_with_the_field_names() {
        let t = table("dbworld");
        let text = "#eedata dbworld\n#format 3\nrow,id,mFloatValue,mFloatValue2,mLongValue,mLongValue2\n88,fanaticism,0.5,1.5,7,2\n";
        let bytes = compile_table(b"dbworld", text.as_bytes()).unwrap();
        let mut out = Vec::new();
        dump_table("dbworld", &bytes, &mut out).unwrap();
        let s = String::from_utf8(out).unwrap();
        // dbworld: only the rows with readers (row 88), only the fields they use; floats as bit patterns
        assert!(s.starts_with("\"dbworld\":["), "{s}");
        assert!(s.contains("\"row\":88"), "{s}");
        assert!(s.contains("{\"row\":88,"), "{s}");
        assert!(s.contains("\"0x3fc00000\""), "{s}");
        assert!(!s.contains("1.5"), "{s}");
        let _ = t;
    }

    #[test]
    fn truncated_files_are_reported() {
        let t = table("dbsounds");
        // a count of 1, then only 4 bytes of the record
        let mut d = 1u32.to_le_bytes().to_vec();
        d.extend_from_slice(&[0; 4]);
        let mut out = Vec::new();
        let e = dump_table(t.name, &d, &mut out).unwrap_err();
        assert_eq!(e.text, b"dbsounds: the db file is truncated or has bytes after the last record".to_vec());
    }

    #[test]
    fn trailing_bytes_are_reported() {
        let t = table("dbworld");
        let d = vec![0u8; t.record.size as usize + 1];
        let mut out = Vec::new();
        assert!(dump_table(t.name, &d, &mut out).is_err());
    }

    #[test]
    fn unknown_and_text_only_tables_are_errors() {
        let mut out = Vec::new();
        assert_eq!(
            dump_table("nosuch", &[], &mut out).unwrap_err().text,
            b"nosuch: not a db table with a text form".to_vec()
        );
        assert!(dump_table("epochs", &[], &mut out).is_err());
    }

    #[test]
    fn empty_count_table_dumps_empty_array() {
        let t = table("dbsounds");
        let d = 0u32.to_le_bytes().to_vec();
        let mut out = Vec::new();
        dump_table(t.name, &d, &mut out).unwrap();
        assert_eq!(out, b"\"dbsounds\":[\n]".to_vec());
    }

    #[test]
    fn strings_stop_at_nul_and_escape_non_ascii() {
        let mut out = Vec::new();
        push_str(&mut out, b"a\"\\\x01\xe9\0tail", 10);
        assert_eq!(out, b"\"a\\\"\\\\\\u0001\\u00e9\"".to_vec());
    }

    #[test]
    fn int_values_sign_extend() {
        assert_eq!(int_value(&[0xff], Type::S8), -1);
        assert_eq!(int_value(&[0xff, 0xff], Type::S16), -1);
        assert_eq!(int_value(&[0xff, 0xff, 0xff, 0xff], Type::S32), -1);
        assert_eq!(int_value(&[0xff, 0xff, 0xff, 0xff], Type::U32), 0xffffffff);
    }

    #[test]
    fn in_list_matches_whole_words() {
        assert!(in_list("mFloatValue mLongValue", "mLongValue"));
        assert!(!in_list("mFloatValue2", "mFloatValue"));
        assert!(!in_list("", "x"));
    }
}
