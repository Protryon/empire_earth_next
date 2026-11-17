//! `golden_authlogin.txt` replayed: each sequence's setters, file hashes (the files' bytes made again) and secrets
//! on a new login, and after each step the members and the three blocks compared with what the original's
//! statements did; then the steps one by one.

use super::*;

fn unhex(s: &str) -> Vec<u8> {
    if s == "-" {
        return Vec::new();
    }
    (0..s.len() / 2).map(|i| u8::from_str_radix(&s[2 * i..2 * i + 2], 16).unwrap()).collect()
}

fn wide(s: &str) -> Vec<u16> {
    text::utf16_from_le_bytes(&unhex(s))
}

/// the driver's `FileContent`
fn file_content(seed: i64, size: usize) -> Vec<u8> {
    (0..size as i64).map(|i| ((seed * 7 + i * 13 + (i >> 9)) & 0xff) as u8).collect()
}

fn blocks(l: &Login) -> String {
    let h = crate::reqop::hex;
    format!(" | {} {} {}", h(&l.community_data()), h(&l.hashes(b"ch")), h(&l.secrets.login_bytes()))
}

#[test]
fn golden() {
    let text = include_str!("golden_authlogin.txt");
    let mut l = Login::default();
    let (mut seqs, mut steps, mut hashed, mut failed_files) = (0, 0, 0, 0);
    for line in text.lines() {
        if line == "S" {
            l = Login::default();
            seqs += 1;
            continue;
        }
        let (step, want) = line.split_once(" => ").unwrap();
        let f: Vec<&str> = step.split(' ').collect();
        assert_eq!(f[0], "L");
        let op: i32 = f[1].parse().unwrap();
        let k = wide(f[2]);
        match op {
            1 | 2 => l.set(op, &[], &unhex(f[3]), 0),
            15 => l.set(op, &k, &unhex(f[3]), 0),
            8 | 9 | 11 => l.set(op, &k, &unhex(f[3]), f[4].parse().unwrap()),
            4..=16 => l.set(op, &k, &[], 0),
            0 | 17 => {
                let n: usize = f[3].parse().unwrap();
                let mut files = Vec::new();
                let mut contents = Vec::new();
                for p in &f[4..4 + n] {
                    let (size, seed) = p.split_once(':').unwrap();
                    let size: i64 = size.parse().unwrap();
                    files.push(format!("f{}", files.len()).into_bytes());
                    contents.push((size >= 0).then(|| file_content(seed.parse().unwrap(), size as usize)));
                }
                let ok = l.hash_files(&k, &files, |path| {
                    let i = (path[1] - b'0') as usize;
                    contents[i].clone().map(|c| Box::new(Bytes::new(c)) as Box<dyn Stream>)
                });
                assert_eq!(format!("={}", u8::from(ok)), f[4 + n], "{line}");
                hashed += 1;
                failed_files += usize::from(!ok);
            }
            18 | 19 => {
                let mut kk = [0u8; 8];
                kk.copy_from_slice(&unhex(f[3]));
                let flags: i64 = f[4].parse().unwrap();
                if op == 18 {
                    if flags & 1 != 0 {
                        l.secrets.remove(&kk);
                    } else {
                        l.secrets.add(kk);
                    }
                } else {
                    l.secrets.commit(1000 + flags * 400, 300);
                }
            }
            _ => {}
        }
        assert_eq!(format!("{}{}", l.describe(), blocks(&l)), want, "{line}");
        steps += 1;
    }
    assert!(seqs >= 300 && steps > 4000 && hashed > 500 && failed_files > 50, "{seqs} {steps} {hashed} {failed_files}");
}

#[test]
fn set_community_keeps_its_data_and_drops_the_rest() {
    let mut l = Login::default();
    let a: Vec<u16> = "A".encode_utf16().collect();
    let b: Vec<u16> = "B".encode_utf16().collect();
    l.set(EE_WON_AUTH_LOGIN_CDKEY, &a, b"k", 3);
    l.set(EE_WON_AUTH_LOGIN_ADD_COMMUNITY, &b, &[], 0);
    l.set(EE_WON_AUTH_LOGIN_SET_COMMUNITY, &a, &[], 0);
    assert_eq!(l.communities.len(), 1);
    assert_eq!(l.communities[&a].cd_key.as_deref(), Some(&b"k"[..]));
    l.set(EE_WON_AUTH_LOGIN_SET_COMMUNITY, &b, &[], 0);
    assert_eq!(l.communities.keys().collect::<Vec<_>>(), vec![&b]);
    assert_eq!(l.communities[&b], LoginCommunity::default());
}

#[test]
fn file_hashes_run_sections_across_files() {
    let mut l = Login::default();
    let c: Vec<u16> = "C".encode_utf16().collect();
    let a = file_content(1, 10000);
    let b = file_content(2, 10000);
    let files = [b"a".to_vec(), b"b".to_vec()];
    assert!(l.hash_files(&c, &files, |p| {
        let d = if p == b"a" { a.clone() } else { b.clone() };
        Some(Box::new(Bytes::new(d)) as Box<dyn Stream>)
    }));
    let mut all = a.clone();
    all.extend_from_slice(&b);
    let data = &l.communities[&c];
    // one MD5 of everything, one per 16 KB section across the two files, then the short last one
    assert_eq!(data.simple_hash, Md5::digest(&all).to_vec());
    let mut keyed = Md5::digest(&all[..SECTION]).to_vec();
    keyed.extend_from_slice(&Md5::digest(&all[SECTION..]));
    assert_eq!(data.keyed_hash_data, keyed);
    // a missing file: false, its community's hashes cleared
    assert!(!l.hash_files(&c, &files, |_| None));
    assert!(l.communities[&c].simple_hash.is_empty());
}

#[test]
fn the_blocks_read_valid_cd_keys_only() {
    let mut l = Login::default();
    let a: Vec<u16> = "A".encode_utf16().collect();
    l.set(EE_WON_AUTH_LOGIN_CDKEY, &a, b"key", 1); // present, not valid
    let without = l.community_data();
    l.set(EE_WON_AUTH_LOGIN_CDKEY, &a, b"key", 3);
    let with = l.community_data();
    assert!(with.len() > without.len());
    assert!(with.windows(3).any(|w| w == b"key"));
}
