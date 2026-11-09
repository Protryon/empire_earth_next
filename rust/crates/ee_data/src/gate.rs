//! The data gate's driver (`ee_data_gate`, tools/eedata/cppgate.py): compiles every table of a text tree, the edited
//! parity cases, the rules parity cases, the float sweep, and the content dumps, for comparison with the C++ compiler
//! (until it is deleted) and with the Python importer (always).

use crate::content::{text_extension, Content, Reader};
use crate::dump::dump_table;
use crate::generated::specs_gen::TABLES;
use crate::node::{Kind, Node};
use crate::rules::Rules;
use crate::spec::table;
use crate::text::parse_f32;
use std::fs;
use std::path::{Path, PathBuf};

fn text_reader(dir: PathBuf) -> Reader {
    Box::new(move |name: &str| {
        let ext = text_extension(name.as_bytes())?;
        fs::read(dir.join(format!("{name}{ext}"))).ok()
    })
}

/// A Content reading `dir`, except that `over`'s text replaces the table it names.
fn reader_with_override(dir: PathBuf, over_name: String, over_text: Vec<u8>) -> Reader {
    Box::new(move |name: &str| {
        if name == over_name {
            return Some(over_text.clone());
        }
        let ext = text_extension(name.as_bytes())?;
        fs::read(dir.join(format!("{name}{ext}"))).ok()
    })
}

fn compile_tree(textdir: &str, wantdir: &str) -> i32 {
    let (mut same, mut bad, mut missing, mut count) = (0, 0, 0, 0);
    let mut c = Content::new(text_reader(PathBuf::from(textdir)));
    for t in TABLES.iter().filter(|t| !t.text_only) {
        count += 1;
        let ext = text_extension(t.name.as_bytes()).expect("a table has an extension");
        let text_ok = Path::new(textdir).join(format!("{}{ext}", t.name)).exists();
        let want = fs::read(Path::new(wantdir).join(format!("{}.dat", t.name)));
        let Ok(want) = want.and_then(|w| if text_ok { Ok(w) } else { Err(std::io::ErrorKind::NotFound.into()) }) else {
            println!("  {:<24} MISSING", t.name);
            missing += 1;
            continue;
        };
        match c.compile(t) {
            Err(e) => {
                println!("  {:<24} ERROR {}", t.name, String::from_utf8_lossy(&e.text));
                bad += 1;
            }
            Ok(got) if got != want => {
                let k = got.iter().zip(want.iter()).take_while(|(a, b)| a == b).count();
                println!("  {:<24} DIFF {} bytes for {}, first difference at {}", t.name, got.len(), want.len(), k);
                bad += 1;
            }
            Ok(got) => {
                println!("  {:<24} {:9} B identical", t.name, got.len());
                same += 1;
            }
        }
    }
    println!("{same}/{count} tables compile to identical bytes ({bad} differ or fail, {missing} missing)");
    i32::from(bad + missing != 0)
}

fn batch(dir: &str, textdir: &str) -> i32 {
    let Ok(rd) = fs::read_dir(dir) else { return 2 };
    let mut n = 0;
    for entry in rd {
        let Ok(entry) = entry else { return 2 };
        let file = entry.file_name().to_string_lossy().into_owned();
        let (Some(sep), Some(dot)) = (file.find("__"), file.rfind('.')) else { continue };
        if dot < sep {
            continue;
        }
        let name = &file[..sep];
        let table_name = &file[sep + 2..dot];
        let Ok(text) = fs::read(entry.path()) else { return 2 };
        let mut c = Content::new(reader_with_override(PathBuf::from(textdir), table_name.to_string(), text));
        let text_only = TABLES.iter().any(|t| t.name == table_name && t.text_only);
        let target = if text_only { table("dbtechtree") } else { table(table_name) };
        let (out, data) = match c.compile(target) {
            Ok(bytes) => (format!("{name}.out"), bytes),
            Err(e) => (format!("{name}.err"), e.text),
        };
        if fs::write(Path::new(dir).join(out), data).is_err() {
            return 2;
        }
        n += 1;
    }
    println!("{n} parity cases compiled");
    0
}

fn rules_dump_line(rules: &Rules, json: &mut Vec<u8>) {
    let d = rules.dump();
    let mut at = 0;
    while at < d.len() {
        let end = d[at..].iter().position(|&b| b == b'\n').map_or(d.len(), |x| x + at);
        json.extend_from_slice(b",\n");
        json.extend_from_slice(&d[at..end]);
        at = end + 1;
    }
}

fn dump_tree(text: bool, dir: &str, out: &str) -> i32 {
    let mut c = Content::new(text_reader(PathBuf::from(dir)));
    let mut json: Vec<u8> = b"{".to_vec();
    let mut first = true;
    for t in TABLES.iter().filter(|t| !t.text_only) {
        let data = if text {
            match c.compile(t) {
                Ok(d) => d,
                Err(e) => {
                    eprintln!("{}", String::from_utf8_lossy(&e.text));
                    return 1;
                }
            }
        } else {
            match fs::read(Path::new(dir).join(format!("{}.dat", t.name))) {
                Ok(d) => d,
                Err(_) => {
                    eprintln!("{}.dat: cannot be read", t.name);
                    return 1;
                }
            }
        };
        if !first {
            json.extend_from_slice(b",\n");
        }
        first = false;
        if let Err(e) = dump_table(t.name, &data, &mut json) {
            eprintln!("{}", String::from_utf8_lossy(&e.text));
            return 1;
        }
    }
    let mut rules = Rules::default();
    if text {
        if let Err(e) = rules.load(&mut c) {
            eprintln!("{}", String::from_utf8_lossy(&e.text));
            return 1;
        }
    } else {
        let Ok(raw) = fs::read(Path::new(dir).join("dbmusic.dat")) else {
            eprintln!("dbmusic.dat: cannot be read");
            return 1;
        };
        if !music_from_dat(&raw, &mut rules) {
            eprintln!("dbmusic.dat: cannot be read");
            return 1;
        }
    }
    rules_dump_line(&rules, &mut json);
    json.extend_from_slice(b"}\n");
    if fs::write(out, &json).is_err() {
        return 2;
    }
    let dumped = TABLES.iter().filter(|t| !t.text_only).count();
    println!("{dumped} tables -> {out} ({} bytes)", json.len());
    0
}

fn music_from_dat(data: &[u8], rules: &mut Rules) -> bool {
    let t = table("dbmusic");
    let Some(f) = t.find_field("mInEpoch") else { return false };
    if data.len() < 4 {
        return false;
    }
    let n = u32::from_le_bytes([data[0], data[1], data[2], data[3]]) as usize;
    let size = t.record.size as usize;
    if data.len() < 4 + n * size {
        return false;
    }
    for r in 0..n {
        let flags: Vec<bool> =
            (0..(f.count as usize).min(64)).map(|k| data[4 + r * size + f.offset as usize + k] != 0).collect();
        rules.set_music_from_flags(r as i32, &flags);
    }
    true
}

/// Edited rules documents: `<case>__<doc>.yaml`; each -> `<case>.out` (the rules dump) or `<case>.err`.
fn rules_batch(dir: &str, textdir: &str) -> i32 {
    let Ok(rd) = fs::read_dir(dir) else { return 2 };
    let mut n = 0;
    for entry in rd {
        let Ok(entry) = entry else { return 2 };
        let file = entry.file_name().to_string_lossy().into_owned();
        let (Some(sep), Some(dot)) = (file.find("__"), file.rfind('.')) else { continue };
        let name = &file[..sep];
        let doc = &file[sep + 2..dot];
        let Ok(text) = fs::read(entry.path()) else { return 2 };
        let mut c = Content::new(reader_with_override(PathBuf::from(textdir), doc.to_string(), text));
        let mut rules = Rules::default();
        let (out, data) = match rules.load(&mut c) {
            Ok(()) => (format!("{name}.out"), rules.dump()),
            Err(e) => (format!("{name}.err"), e.text),
        };
        if fs::write(Path::new(dir).join(out), data).is_err() {
            return 2;
        }
        n += 1;
    }
    println!("{n} rules cases compiled");
    0
}

/// One decimal per line -> its bits as `XXXXXXXX`, or `E <message>`.
fn floats(input: &str, output: &str) -> i32 {
    let Ok(data) = fs::read(input) else { return 2 };
    let mut out = Vec::new();
    for line in data.split(|&b| b == b'\n') {
        let mut n = Node::scalar(line.to_vec(), false, 1);
        n.kind = Kind::Scalar;
        match parse_f32(&n.text) {
            Ok(bits) => out.extend_from_slice(format!("{bits:08x}\n").as_bytes()),
            Err(e) => {
                out.extend_from_slice(b"E ");
                out.extend_from_slice(&e.text);
                out.push(b'\n');
            }
        }
    }
    if fs::write(output, &out).is_err() {
        return 2;
    }
    0
}

/// The gate's command line; returns the exit code.
pub fn run(args: &[String]) -> i32 {
    let refs: Vec<&str> = args.iter().map(String::as_str).collect();
    match refs.as_slice() {
        ["--batch", dir, textdir] => batch(dir, textdir),
        ["--dump-text", dir, out] => dump_tree(true, dir, out),
        ["--dump-dat", dir, out] => dump_tree(false, dir, out),
        ["--rules-batch", dir, textdir] => rules_batch(dir, textdir),
        ["--floats", input, output] => floats(input, output),
        [text, want] => compile_tree(text, want),
        _ => {
            eprintln!("usage: ee_data_gate <text dir> <expected .dat dir>");
            2
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::collections::BTreeMap as Map;

    fn tmp(name: &str) -> PathBuf {
        let d = std::env::temp_dir().join(format!("ee_data_gate_{}_{name}", std::process::id()));
        let _ = fs::remove_dir_all(&d);
        fs::create_dir_all(&d).unwrap();
        d
    }

    #[test]
    fn floats_write_bits_and_errors() {
        let d = tmp("floats");
        let (input, output) = (d.join("in.txt"), d.join("out.txt"));
        fs::write(&input, b"1.5\n0.1\nxx\n1e39").unwrap();
        assert_eq!(run(&[String::from("--floats"), input.display().to_string(), output.display().to_string()]), 0);
        let text = fs::read_to_string(&output).unwrap();
        let lines: Vec<&str> = text.lines().collect();
        assert_eq!(lines[0], "3fc00000");
        assert_eq!(lines[1], "3dcccccd");
        assert_eq!(lines[2], "E 'xx' is not a number");
        assert_eq!(lines[3], "E '1e39' is outside the float range");
        let _ = fs::remove_dir_all(&d);
    }

    #[test]
    fn usage_for_bad_arguments() {
        assert_eq!(run(&[String::from("--nonsense")]), 2);
    }

    #[test]
    fn rules_batch_writes_err_for_a_broken_document() {
        let d = tmp("rules");
        let cases = d.join("cases");
        fs::create_dir_all(&cases).unwrap();
        let text = d.join("text");
        fs::create_dir_all(&text).unwrap();
        fs::write(cases.join("bad__combat.yaml"), b"damage:\n  minimum: x\n").unwrap();
        assert_eq!(run(&[String::from("--rules-batch"), cases.display().to_string(), text.display().to_string()]), 0);
        let err = fs::read_to_string(cases.join("bad.err")).unwrap();
        assert_eq!(err, "combat line 2: minimum: 'x' is not an integer");
        let _ = fs::remove_dir_all(&d);
    }

    #[test]
    fn music_from_dat_reports_short_files() {
        let mut r = Rules::default();
        assert!(!music_from_dat(&[1, 0], &mut r));
        let _unused: Map<i32, i32> = Map::new();
    }
}
