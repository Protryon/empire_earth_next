//! Replays the original checksum sets' trace (`tests/error/es_checksum_golden.cpp`, format in its header) on the Rust
//! sets: every call is made again and every result line must be the golden's. `golden_checksum.txt` holds the first
//! 60 sequences; `EE_CHECKSUM_GOLDEN=<file>` replays a longer run.

use super::checksum::EeErrorChecksum;

fn unhex(s: &str) -> Vec<u8> {
    if s == "-" {
        return Vec::new();
    }
    (0..s.len()).step_by(2).map(|i| u8::from_str_radix(&s[i..i + 2], 16).unwrap()).collect()
}

fn hex(b: &[u8]) -> String {
    if b.is_empty() {
        return "-".into();
    }
    b.iter().map(|c| format!("{c:02x}")).collect()
}

/// -> (lines checked, sequences with a differing line, the first difference)
fn replay(text: &str) -> (usize, usize, Option<String>) {
    let mut sets = vec![EeErrorChecksum::new(true); 3];
    let (mut checked, mut failed, mut skipping, mut first) = (0, 0, false, None);
    for (number, line) in text.lines().enumerate() {
        let f: Vec<&str> = line.split(' ').collect();
        if skipping && f[0] != "seq" {
            continue;
        }
        let n = |k: usize| -> usize { f[k].parse().unwrap() };
        let got = match f[0] {
            "seq" => {
                sets = (0..3).map(|k| EeErrorChecksum::new(f[2 + k] == "1")).collect();
                skipping = false;
                None
            }
            "add" => {
                sets[n(1)].add(&unhex(f[2]), f[3].parse().unwrap());
                None
            }
            "addv" => {
                sets[n(1)].add_value(f[2].parse().unwrap());
                None
            }
            "copy" => {
                sets[n(1)] = sets[n(2)].clone();
                None
            }
            "new" => {
                sets[n(1)] = EeErrorChecksum::new(f[2] == "1");
                None
            }
            "total" => {
                let c = &sets[n(1)];
                Some(format!("total {} = {} {}", f[1], c.total(), c.verbose as i32))
            }
            "dump" => {
                let c = &sets[n(1)];
                let mut s = format!("dump {} = {}", f[1], c.entries.len());
                for e in &c.entries {
                    s += &format!(" {}:{}", hex(&e.description), e.checksum);
                }
                Some(s)
            }
            "cmp" => {
                let (same, errors) = sets[n(1)].compare(&sets[n(2)]);
                let mut s = format!("cmp {} {} = {} {}", f[1], f[2], same as i32, errors.len());
                for e in &errors {
                    s += &format!(" {}", hex(e));
                }
                Some(s)
            }
            other => panic!("line {}: unknown op {other}", number + 1),
        };
        if let Some(got) = got {
            if got != line {
                failed += 1;
                skipping = true;
                first.get_or_insert_with(|| format!("line {}: got {got:?}, golden {line:?}", number + 1));
            }
            checked += 1;
        }
    }
    (checked, failed, first)
}

#[test]
fn golden_checksum_trace_replays() {
    let (checked, failed, first) = replay(include_str!("golden_checksum.txt"));
    assert_eq!(failed, 0, "{first:?}");
    assert!(checked > 1000, "only {checked} lines checked");
}

#[test]
fn golden_checksum_trace_replays_long_run() {
    if let Ok(path) = std::env::var("EE_CHECKSUM_GOLDEN") {
        let text = std::fs::read_to_string(&path).unwrap();
        let (checked, failed, first) = replay(&text);
        eprintln!("{path}: {checked} lines checked, {failed} sequences differ");
        assert_eq!(failed, 0, "{first:?}");
    }
}
