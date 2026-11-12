//! `cargo run -p xtask -- capture-digest SEED=PATH ...`: tools/sim_capture.py's `digest` lines, the fast way.
//!
//! A capture file (ee/EESimCapture.h) is a run of records, each a 4-byte tag, a little-endian u32 payload size and
//! the payload. Per seed and tag the digest is the record count and FNV-1a 64 over every record's tag, size and
//! payload bytes in file order; the lines are sorted by tag. sim_capture.py computes the same lines in Python (its
//! fallback when cargo is missing) about a hundred times slower. The files are read in pieces and the seeds in
//! parallel, one thread each.

use std::collections::BTreeMap;
use std::io::Read;

const FNV_OFFSET: u64 = 0xcbf2_9ce4_8422_2325;
const FNV_PRIME: u64 = 0x0100_0000_01b3;

fn fnv(mut h: u64, bytes: &[u8]) -> u64 {
    for &b in bytes {
        h = (h ^ u64::from(b)).wrapping_mul(FNV_PRIME);
    }
    h
}

/// The tag as Python's `bytes.decode("ascii", "replace")` names it: one U+FFFD per byte past ASCII.
fn tag_name(tag: [u8; 4]) -> String {
    tag.iter().map(|&b| if b.is_ascii() { char::from(b) } else { '\u{fffd}' }).collect()
}

/// {tag: (records, fnv)} of one capture stream. A record cut short at the end is hashed as far as it goes, as
/// Python's slice does; a header cut short is an error there (struct.error), here too.
fn digests(mut input: impl Read) -> Result<BTreeMap<String, (u64, u64)>, String> {
    let mut out: BTreeMap<String, (u64, u64)> = BTreeMap::new();
    let mut buf = vec![0u8; 1 << 20];
    loop {
        let mut head = [0u8; 8];
        let got = read_full(&mut input, &mut head)?;
        if got == 0 {
            return Ok(out);
        }
        if got < 8 {
            return Err(format!("a record header cut short ({got} bytes)"));
        }
        let tag = [head[0], head[1], head[2], head[3]];
        let mut left = u64::from(u32::from_le_bytes([head[4], head[5], head[6], head[7]]));
        let entry = out.entry(tag_name(tag)).or_insert((0, FNV_OFFSET));
        let mut h = fnv(entry.1, &head);
        while left > 0 {
            let want = buf.len().min(usize::try_from(left).unwrap_or(usize::MAX));
            let n = read_full(&mut input, &mut buf[..want])?;
            h = fnv(h, &buf[..n]);
            if n < want {
                left = 0;
            } else {
                left -= n as u64;
            }
        }
        *entry = (entry.0 + 1, h);
    }
}

fn read_full(input: &mut impl Read, buf: &mut [u8]) -> Result<usize, String> {
    let mut at = 0;
    while at < buf.len() {
        match input.read(&mut buf[at..]) {
            Ok(0) => break,
            Ok(n) => at += n,
            Err(e) if e.kind() == std::io::ErrorKind::Interrupted => {}
            Err(e) => return Err(e.to_string()),
        }
    }
    Ok(at)
}

fn lines(seed: &str, digests: &BTreeMap<String, (u64, u64)>) -> Vec<String> {
    digests.iter().map(|(tag, (n, h))| format!("seed {seed} {tag} {n} {h:016x}")).collect()
}

/// Every argument's lines, in argument order.
pub fn run(args: &[&str]) -> Result<(), String> {
    if args.is_empty() {
        return Err("usage: xtask capture-digest SEED=PATH ...".to_string());
    }
    let jobs: Vec<(String, String)> = args
        .iter()
        .map(|a| {
            a.split_once('=').map(|(s, p)| (s.to_string(), p.to_string())).ok_or_else(|| format!("{a}: not SEED=PATH"))
        })
        .collect::<Result<_, _>>()?;
    let results: Vec<Result<Vec<String>, String>> = std::thread::scope(|scope| {
        let handles: Vec<_> = jobs
            .iter()
            .map(|(seed, path)| {
                scope.spawn(move || {
                    let file = std::fs::File::open(path).map_err(|e| format!("{path}: {e}"))?;
                    let d = digests(std::io::BufReader::with_capacity(1 << 20, file))
                        .map_err(|e| format!("{path}: {e}"))?;
                    Ok(lines(seed, &d))
                })
            })
            .collect();
        handles.into_iter().map(|h| h.join().unwrap_or_else(|_| Err("a digest thread panicked".to_string()))).collect()
    });
    let mut text = String::new();
    for result in results {
        for line in result? {
            text.push_str(&line);
            text.push('\n');
        }
    }
    print!("{text}");
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    fn record(tag: &[u8; 4], payload: &[u8]) -> Vec<u8> {
        let mut v = tag.to_vec();
        v.extend_from_slice(&u32::try_from(payload.len()).unwrap().to_le_bytes());
        v.extend_from_slice(payload);
        v
    }

    #[test]
    fn fnv_1a_64_of_known_strings() {
        assert_eq!(fnv(FNV_OFFSET, b""), 0xcbf2_9ce4_8422_2325);
        assert_eq!(fnv(FNV_OFFSET, b"a"), 0xaf63_dc4c_8601_ec8c);
        assert_eq!(fnv(FNV_OFFSET, b"foobar"), 0x8594_4171_f739_67e8);
    }

    #[test]
    fn records_hash_per_tag_in_file_order_and_sort_by_tag() {
        let mut file = record(b"ZZZZ", b"xy");
        file.extend(record(b"AAAA", b""));
        file.extend(record(b"ZZZZ", b"q"));
        let d = digests(file.as_slice()).unwrap();
        let mut z = fnv(FNV_OFFSET, &record(b"ZZZZ", b"xy"));
        z = fnv(z, &record(b"ZZZZ", b"q"));
        assert_eq!(d["ZZZZ"], (2, z));
        assert_eq!(d["AAAA"], (1, fnv(FNV_OFFSET, &record(b"AAAA", b""))));
        assert_eq!(
            lines("4", &d),
            vec![format!("seed 4 AAAA 1 {:016x}", d["AAAA"].1), format!("seed 4 ZZZZ 2 {z:016x}")]
        );
    }

    #[test]
    fn a_payload_longer_than_the_read_buffer_hashes_whole() {
        let payload: Vec<u8> = (0..(3usize << 20) + 17).map(|i| (i * 31 % 251) as u8).collect();
        let file = record(b"BIG!", &payload);
        assert_eq!(digests(file.as_slice()).unwrap()["BIG!"], (1, fnv(FNV_OFFSET, &file)));
    }

    #[test]
    fn a_tag_past_ascii_names_like_python_and_a_cut_record_hashes_what_is_there() {
        assert_eq!(tag_name([b'A', 0xff, b'B', 0x80]), "A\u{fffd}B\u{fffd}");
        let mut file = record(b"CUT_", b"abcdef");
        file.truncate(11);
        assert_eq!(digests(file.as_slice()).unwrap()["CUT_"], (1, fnv(FNV_OFFSET, &file)));
        assert!(digests(&b"ABCD\x01"[..]).is_err());
    }

    #[test]
    fn an_empty_stream_has_no_tags() {
        assert!(digests(&b""[..]).unwrap().is_empty());
    }
}
