//! The golden replay of `FSBasicFile`/`FSFileSpec` (LLE lane round 5): `tests/fs/fs_spec_golden.cpp` ran random
//! sequences on four specs through the unmodified classes (`tests/fs/fs_spec_orig.h`) in a real directory, emptied
//! before each sequence, and printed every call with its result and the specs' states. This replay makes the same
//! calls on Rust records in a directory of its own (its path the same length, so the paths near `PLAT_MAX_PATH`
//! fail alike) and regenerates every line. `golden_spec.txt` holds the first 60 sequences; a whole file is replayed
//! with `EE_FS_SPEC_GOLDEN=<file> cargo test -p ee_fs golden_spec_replays_long_run`.
//!
//! Windows only: the native error numbers and the search order are the system's.

use std::ffi::{c_long, c_ulong};
use std::fmt::Write as _;
use std::sync::Mutex;

use ee_ustring::{ee_ustring_assign, ee_ustring_free, ee_ustring_init_text};

use crate::spec::*;
use crate::status;

const PATTERNS: [&str; 11] = ["*.*", "*", "*.txt", "a", "b.*", "s*", "?", "*.dat", "none*", "SUB*", "*."];
const MODES: [&std::ffi::CStr; 6] = [c"r", c"rb", c"w", c"wb", c"a+", c"r+b"];

/// The replays share the native error and the working tree's state: one at a time.
static LOCK: Mutex<()> = Mutex::new(());

struct Replay {
    root_dir: String, // the root's directory part ("\...\")
    root: String,     // volume + directory
    volume: EeFsString,
    directory: EeFsString,
    slots: Vec<Option<TempSpec>>,
}

fn unhex(s: &str) -> Vec<u8> {
    if s == "-" {
        return Vec::new();
    }
    (0..s.len()).step_by(2).map(|i| u8::from_str_radix(&s[i..i + 2], 16).unwrap()).collect()
}

fn mk(text: &[u8]) -> EeFsString {
    let mut t = text.to_vec();
    t.push(0);
    let mut s = EeFsString::blank();
    // SAFETY: a record to initialise, a C string
    unsafe { ee_ustring_init_text(s.us(), t.as_ptr().cast()) };
    s
}

fn free(mut s: EeFsString) {
    // SAFETY: a live record
    unsafe { ee_ustring_free(s.us()) };
}

impl Replay {
    fn new(root_length: usize, tag: &str) -> Replay {
        let base = std::env::temp_dir().display().to_string();
        let base = if base.ends_with('\\') { base } else { base + "\\" };
        let base_dir = &base[2..];
        let name = format!("eefsg{tag}{}", std::process::id());
        assert!(base_dir.len() + name.len() < root_length, "the temporary directory {base} is too long");
        let pad = root_length - base_dir.len() - name.len() - 1;
        let root = format!("{base}{name}{}\\", "x".repeat(pad));
        let root_dir = root[2..].to_string();
        assert_eq!(root_dir.len(), root_length);
        Replay {
            volume: mk(&root.as_bytes()[..2]),
            directory: mk(root_dir.as_bytes()),
            root_dir,
            root,
            slots: Vec::new(),
        }
    }

    fn hex(&self, text: &[u8]) -> String {
        let mut t = text.to_vec();
        let r = self.root_dir.as_bytes();
        while let Some(p) = t.windows(r.len()).position(|w| w == r) {
            t.splice(p..p + r.len(), [1u8]);
        }
        // the root's parent (GetParentDirectory of the root): byte 02
        let parent = &r[..r[..r.len() - 1].iter().rposition(|&c| c == b'\\').unwrap() + 1];
        while let Some(p) = t.windows(parent.len()).position(|w| w == parent) {
            t.splice(p..p + parent.len(), [2u8]);
        }
        if t.is_empty() {
            return "-".into();
        }
        t.iter().fold(String::new(), |mut o, b| {
            let _ = write!(o, "{b:02x}");
            o
        })
    }

    fn decode(&self, s: &str) -> Vec<u8> {
        let mut out = Vec::new();
        for b in unhex(s) {
            if b == 1 {
                out.extend_from_slice(self.root_dir.as_bytes());
            } else {
                out.push(b);
            }
        }
        out
    }

    fn state(&self, k: usize) -> String {
        let s = &self.slots[k].as_ref().unwrap().0;
        format!(
            "= {} {} {} {}/{}/{} {} {} {} {} {}",
            self.hex(s.volume.bytes()),
            self.hex(s.directory.bytes()),
            self.hex(s.file.bytes()),
            s.volume.capacity,
            s.directory.capacity,
            s.file.capacity,
            s.file_access,
            s.version,
            s.is_open as i32,
            !s.handle.is_null() as i32,
            !s.stream.is_null() as i32
        )
    }

    fn wipe(&self) {
        fn clear(dir: &std::path::Path) {
            if let Ok(entries) = std::fs::read_dir(dir) {
                for e in entries.flatten() {
                    let p = e.path();
                    if let Ok(m) = std::fs::symlink_metadata(&p) {
                        let mut perm = m.permissions();
                        #[allow(clippy::permissions_set_readonly_false)]
                        perm.set_readonly(false);
                        let _ = std::fs::set_permissions(&p, perm);
                        if m.is_dir() {
                            clear(&p);
                            let _ = std::fs::remove_dir(&p);
                        } else {
                            let _ = std::fs::remove_file(&p);
                        }
                    }
                }
            }
        }
        let _ = std::fs::create_dir_all(&self.root);
        clear(std::path::Path::new(&self.root));
    }

    fn drop_slot(&mut self, k: usize) {
        if let Some(mut s) = self.slots[k].take() {
            // SAFETY: a live record
            unsafe { ee_fs_spec_finish(&mut s.0) };
        }
    }

    fn fresh(&self) -> TempSpec {
        let mut s = TempSpec::new();
        // SAFETY: live records
        unsafe {
            ee_fs_basic_init(&mut s.0);
            ee_fs_spec_init(&mut s.0, &self.volume, &self.directory);
        }
        s
    }

    fn spec(&mut self, k: usize) -> *mut EeFsSpec {
        &mut self.slots[k].as_mut().unwrap().0
    }

    /// Replays one line (after `seq`); returns the regenerated line without its `ne` part.
    unsafe fn line(&mut self, w: &[&str]) -> String {
        let op = w[0];
        let k: usize = w[1].parse().unwrap();
        let n = |i: usize| -> i64 { w[i].parse().unwrap() };
        let x = if op.starts_with("new") { std::ptr::null_mut() } else { self.spec(k) };
        let ok = |r: c_long| r.to_string();
        match op {
            "newd" | "newp" | "newc" => {
                let fresh = match op {
                    "newd" => self.fresh(),
                    "newp" => {
                        let mut s = TempSpec::new();
                        let p = mk(&self.decode(w[2]));
                        ee_fs_basic_init(&mut s.0);
                        ee_fs_spec_init_path(&mut s.0, &p, &self.volume, &self.directory);
                        free(p);
                        s
                    }
                    _ => TempSpec::copy_of(self.spec(n(2) as usize)),
                };
                self.drop_slot(k);
                self.slots[k] = Some(fresh);
                match op {
                    "newd" => format!("newd {k} {}", self.state(k)),
                    _ => format!("{op} {k} {} {}", w[2], self.state(k)),
                }
            }
            "asg" => {
                let j = n(2) as usize;
                if j != k {
                    ee_fs_spec_assign(x, self.spec(j));
                }
                format!("asg {k} {j} {}", self.state(k))
            }
            "setf" | "setd" | "setv" => {
                let t = mk(&self.decode(w[2]));
                let target = match op {
                    "setf" => &mut (*x).file,
                    "setd" => &mut (*x).directory,
                    _ => &mut (*x).volume,
                };
                ee_ustring_assign(target.us(), t.usc());
                free(t);
                format!("{op} {k} {} {}", w[2], self.state(k))
            }
            "create" | "mkdir" | "del" | "rmdir" | "lock" | "unlock" | "close" => {
                let r = match op {
                    "create" => ee_fs_spec_create(x),
                    "mkdir" => ee_fs_spec_create_directory(x),
                    "del" => ee_fs_basic_delete(x),
                    "rmdir" => ee_fs_spec_delete_directory(x),
                    "lock" => ee_fs_spec_lock(x),
                    "unlock" => ee_fs_spec_unlock(x),
                    _ => ee_fs_spec_close(x),
                };
                format!("{op} {k} = {} {}", ok(r), self.state(k))
            }
            "isdir" => format!("isdir {k} = {} {}", ee_fs_spec_is_directory(x) as i32, self.state(k)),
            "copy" => {
                let mut d = self.decode(w[2]);
                d.push(0);
                let r = ee_fs_spec_copy(x, d.as_ptr().cast(), n(3) != 0, n(4) != 0);
                format!("copy {k} {} {} {} = {} {}", w[2], w[3], w[4], ok(r), self.state(k))
            }
            "open" => {
                if !(*x).handle.is_null() {
                    ee_fs_spec_close_search(x);
                }
                let r = ee_fs_spec_open(x, n(2) as c_ulong);
                format!("open {k} {} = {} {}", w[2], ok(r), self.state(k))
            }
            "fopen" => {
                let r = ee_fs_spec_fopen(x, MODES[n(2) as usize].as_ptr());
                format!("fopen {k} {} = {} {}", w[2], ok(r), self.state(k))
            }
            "read" => {
                let len = n(2) as usize;
                let mut data = vec![0xccu8; len + 1];
                let mut read: c_ulong = 0xdead;
                let r = ee_fs_spec_read(x, data.as_mut_ptr(), len as c_ulong, &mut read);
                let shown = if read as usize <= len { self.hex(&data[..(read as usize).min(64)]) } else { "?".into() };
                format!("read {k} {len} = {} {read} {shown} {}", ok(r), self.state(k))
            }
            "write" => {
                let len = n(2) as usize;
                let seed = n(3) as u32;
                let data: Vec<u8> = (0..=len)
                    .map(|i| (seed.wrapping_add((i as u32).wrapping_mul(2_654_435_761)) >> 13) as u8)
                    .collect();
                let mut written: c_ulong = 0xdead;
                let r = ee_fs_spec_write(x, data.as_ptr(), len as c_ulong, &mut written);
                format!("write {k} {len} {seed} = {} {written} {}", ok(r), self.state(k))
            }
            "rds" => {
                let t = n(2) as usize;
                let size = [4, 4, 2, 2, 1, 1, 4, 8, 1][t];
                let mut v = [0xccu8; 8];
                let r = ee_fs_spec_read_value(x, v.as_mut_ptr(), size);
                format!("rds {k} {t} = {} {} {}", ok(r), self.hex(&v), self.state(k))
            }
            "wrs" => {
                let t = n(2) as usize;
                let size = [4, 4, 2, 2, 1, 1, 4, 8, 1][t];
                let v = unhex(w[3]);
                let r = ee_fs_spec_write_value(x, v.as_ptr(), size);
                format!("wrs {k} {t} {} = {} {}", w[3], ok(r), self.state(k))
            }
            "len" => {
                let mut l: c_ulong = 0xdead;
                let r = ee_fs_spec_get_length(x, &mut l);
                format!("len {k} = {} {l} {}", ok(r), self.state(k))
            }
            "pos" => {
                let mut p: c_long = 0xdead;
                let r = ee_fs_spec_get_position(x, &mut p);
                format!("pos {k} = {} {p} {}", ok(r), self.state(k))
            }
            "setlen" => {
                let r = ee_fs_spec_set_length(x, n(2) as c_ulong);
                format!("setlen {k} {} = {} {}", w[2], ok(r), self.state(k))
            }
            "setpos" => {
                let r = ee_fs_spec_set_position(x, n(2) as c_long, n(3) as c_ulong, n(4) != 0);
                format!("setpos {k} {} {} {} = {} {}", w[2], w[3], w[4], ok(r), self.state(k))
            }
            "first" => {
                let (pi, j) = (n(2) as usize, n(3) as usize);
                if !(*x).handle.is_null() {
                    ee_fs_spec_close_search(x);
                }
                let mut pattern = if pi == 0 { "*.*" } else { PATTERNS[pi - 1] }.as_bytes().to_vec();
                pattern.push(0);
                let r = ee_fs_spec_first_file(x, pattern.as_ptr().cast(), self.spec(j), &self.volume, &self.directory);
                format!("first {k} {pi} {j} = {} {} {}", ok(r), self.state(k), self.state(j))
            }
            "next" => {
                let j = n(2) as usize;
                let r = ee_fs_spec_next_file(x, self.spec(j));
                format!("next {k} {j} = {} {} {}", ok(r), self.state(k), self.state(j))
            }
            "parent" => {
                let j = n(2) as usize;
                ee_fs_spec_parent(x, self.spec(j));
                let line = format!("parent {k} {j} {} {}", self.state(k), self.state(j));
                if !(*self.spec(j)).directory.bytes().starts_with(self.root_dir.as_bytes()) {
                    let fresh = self.fresh();
                    self.drop_slot(j);
                    self.slots[j] = Some(fresh);
                }
                line
            }
            "lenc" => {
                // GetLengthFromClosedFile (C++: the virtuals Open, GetLength, Close)
                let l: c_ulong;
                let r = if ee_fs_spec_open(x, FS_ACCESS_READ) != status::OK as c_long {
                    l = 0;
                    status::OK as c_long
                } else {
                    let mut length: c_ulong = 0;
                    ee_fs_spec_get_length(x, &mut length);
                    ee_fs_spec_close(x);
                    l = length;
                    status::OK as c_long
                };
                format!("lenc {k} = {} {l} {}", ok(r), self.state(k))
            }
            "less" => {
                // operator< on one spec (C++: the virtuals Open and Close)
                let mut opened_this = false;
                let mut opened_other = false;
                if (*x).handle.is_null() && ee_fs_spec_open(x, 1) == status::OK as c_long {
                    opened_this = true;
                }
                if (*x).handle.is_null() && ee_fs_spec_open(x, 1) == status::OK as c_long {
                    opened_other = true;
                }
                let result = (*x).handle.is_null(); // both handles set: the same time, not less
                if opened_this {
                    ee_fs_spec_close(x);
                }
                if opened_other {
                    ee_fs_spec_close(x);
                }
                format!("less {k} = {} {}", result as i32, self.state(k))
            }
            "csh" => {
                ee_fs_spec_close_search(x);
                format!("csh {k} {}", self.state(k))
            }
            _ => panic!("unknown line {op}"),
        }
    }
}

/// Replays `golden`; returns the first mismatch.
fn replay(golden: &str, tag: &str) -> Result<usize, String> {
    let _guard = LOCK.lock().unwrap_or_else(|e| e.into_inner());
    let mut lines = golden.lines();
    let first = lines.next().unwrap();
    let root_length: usize = first.strip_prefix("root ").expect("a root line").parse().unwrap();
    let mut r = Replay::new(root_length, tag);
    let mut sequences = 0;
    for (number, line) in lines.enumerate() {
        if let Some(n) = line.strip_prefix("seq ") {
            for k in 0..r.slots.len() {
                r.drop_slot(k);
            }
            r.slots = (0..4).map(|_| None).collect();
            r.wipe();
            sequences += 1;
            let _ = n;
            continue;
        }
        let w: Vec<&str> = line.split(' ').collect();
        // SAFETY: the slots are live records; every call's contract is kept by the trace's guards
        let mine = unsafe { r.line(&w) };
        let ne = LAST_NATIVE.with(|l| l.get());
        let mine = format!("{mine} ne {ne}");
        if mine != line {
            for k in 0..r.slots.len() {
                r.drop_slot(k);
            }
            r.wipe();
            let _ = std::fs::remove_dir(&r.root);
            return Err(format!("line {}:\n  golden {line}\n  replay {mine}", number + 2));
        }
    }
    for k in 0..r.slots.len() {
        r.drop_slot(k);
    }
    r.wipe();
    let _ = std::fs::remove_dir(&r.root);
    free(std::mem::replace(&mut r.volume, EeFsString::blank()));
    free(std::mem::replace(&mut r.directory, EeFsString::blank()));
    Ok(sequences)
}

#[test]
fn golden_spec_replays() {
    let n = replay(include_str!("golden_spec.txt"), "a").unwrap_or_else(|e| panic!("{e}"));
    assert_eq!(n, 60);
}

#[test]
fn golden_spec_replays_long_run() {
    let Ok(path) = std::env::var("EE_FS_SPEC_GOLDEN") else {
        return;
    };
    let golden = std::fs::read_to_string(path).unwrap();
    let n = replay(&golden, "b").unwrap_or_else(|e| panic!("{e}"));
    eprintln!("{n} sequences replayed");
}
