//! Replays the original filter's trace (`tests/error/es_filter_golden.cpp`, format in its header): each sequence's
//! `Filter.txt` text is loaded (when the startup path is set: the original reads nothing without it) and every
//! message's verdict must be the golden's. `golden_filter.txt` holds the first 60 sequences;
//! `EE_ERROR_GOLDEN=<file>` replays a longer run.

use super::Filters;

fn unhex(s: &str) -> Vec<u8> {
    if s == "-" {
        return Vec::new();
    }
    (0..s.len()).step_by(2).map(|i| u8::from_str_radix(&s[i..i + 2], 16).unwrap()).collect()
}

/// -> (messages checked, sequences with a differing verdict, the first difference)
fn replay(text: &str) -> (usize, usize, Option<String>) {
    let mut filters = Filters::default();
    let mut path = false;
    let mut file: Option<Vec<u8>> = None;
    let (mut checked, mut failed, mut skipping, mut first) = (0, 0, false, None);
    for (number, line) in text.lines().enumerate() {
        let f: Vec<&str> = line.split(' ').collect();
        match f[0] {
            "seq" => {
                filters = Filters::default();
                path = f[2] == "1";
                file = (f[3] == "1").then(|| unhex(f[4]));
                skipping = false;
            }
            "msg" if !skipping => {
                // ESFilterDebugOutput: the file is read on the first message that has a startup path
                if !filters.loaded && path {
                    filters.load(file.as_deref());
                }
                let got = filters.passes(&unhex(f[1])) as i32;
                if got.to_string() != f[3] {
                    failed += 1;
                    skipping = true;
                    first.get_or_insert_with(|| format!("line {}: got {got}, golden {line:?}", number + 1));
                }
                checked += 1;
            }
            "msg" => {}
            other => panic!("line {}: unknown op {other}", number + 1),
        }
    }
    (checked, failed, first)
}

#[test]
fn golden_filter_trace_replays() {
    let (checked, failed, first) = replay(include_str!("golden_filter.txt"));
    assert_eq!(failed, 0, "{first:?}");
    assert!(checked > 500, "only {checked} messages checked");
}

#[test]
fn golden_filter_trace_replays_long_run() {
    if let Ok(path) = std::env::var("EE_ERROR_GOLDEN") {
        let text = std::fs::read_to_string(&path).unwrap();
        let (checked, failed, first) = replay(&text);
        eprintln!("{path}: {checked} messages checked, {failed} sequences differ");
        assert_eq!(failed, 0, "{first:?}");
    }
}
