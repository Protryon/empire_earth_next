//! Replays the original UString's trace (`tests/string/us_golden.cpp`, format in its header): every search and
//! split again with the Rust functions, each line regenerated (a token's text is its bytes from first to last).
//! `golden_us.txt` holds the first 3,000 cases; `EE_USTRING_GOLDEN=<file>` replays a longer run.

use super::*;

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

fn tokens_text(text: &[u8], tokens: &[(u32, u32)]) -> String {
    tokens.iter().map(|&(a, b)| format!(" {}", hex(&text[a as usize..=b as usize]))).collect()
}

/// -> (lines, lines that differ, the first difference)
fn replay(golden: &str) -> (usize, usize, Option<String>) {
    let (mut failed, mut first) = (0, None);
    let mut count = 0;
    for line in golden.lines() {
        count += 1;
        let f: Vec<&str> = line.split(' ').collect();
        let text = unhex(f[1]);
        let got = match f[0] {
            "find" | "back" => {
                let needle = unhex(f[2]);
                let start: u32 = f[3].parse().unwrap();
                let r =
                    if f[0] == "find" { find(&text, &needle, start) } else { find_backwards(&text, &needle, start) };
                format!("{} {} {} {start} = {r} {r}", f[0], f[1], f[2])
            }
            "tok" => format!("tok {} {} ={}", f[1], f[2], tokens_text(&text, &tokenize(&text, &unhex(f[2])))),
            _ => {
                let brackets = unhex(f[3]);
                let (open, close) = (brackets.first().copied().unwrap_or(0), brackets.get(1).copied().unwrap_or(0));
                let tokens = nested_tokenize(&text, &unhex(f[2]), open, close);
                format!("nest {} {} {} ={}", f[1], f[2], f[3], tokens_text(&text, &tokens))
            }
        };
        if got != line {
            failed += 1;
            if first.is_none() {
                first = Some(format!("golden {line:?}, rust {got:?}"));
            }
        }
    }
    (count, failed, first)
}

#[test]
fn golden_us_trace_replays() {
    let (count, failed, first) = replay(include_str!("golden_us.txt"));
    assert!(count >= 3000, "{count}");
    assert_eq!(failed, 0, "{first:?}");
}

/// `EE_USTRING_GOLDEN=<file> cargo test -p ee_ustring long_run -- --nocapture`
#[test]
fn golden_us_trace_replays_long_run() {
    let Ok(path) = std::env::var("EE_USTRING_GOLDEN") else { return };
    let (count, failed, first) = replay(&std::fs::read_to_string(path).unwrap());
    println!("{count} lines, {failed} sequences differ; first: {first:?}");
    assert_eq!(failed, 0, "{first:?}");
}
