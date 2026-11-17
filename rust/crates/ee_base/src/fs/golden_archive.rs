//! The golden replay of `FSArchive` (LLE lane round 5): `tests/fs/fs_archive_golden.cpp` built source trees of random
//! files in a real directory and ran random sequences on three archives through the unmodified units
//! (`tests/fs/fs_archive_orig.h`): builds from the directory walk and from name lists, opens, case-insensitive finds
//! (each member's length, digest and names), tags, listings, extractions (their progress lines), assignments. This
//! replay makes the same calls on Rust records in a directory of its own (its path the same length) and regenerates
//! every line. The odd sequences hold only archive calls; the even ones (marked `p`) also drive `FSPath`, whose
//! vectors are C++: those are checked by the driver relinked over the new units, and skipped here.
//! `golden_archive.txt` holds the first 40 sequences; `EE_FS_ARCHIVE_GOLDEN=<file> cargo test -p ee_fs
//! golden_archive_replays_long_run` replays a whole file. Windows only (the native errors, the search order).

use std::ffi::{c_long, c_ulong};
use std::fmt::Write as _;
use std::ptr;
use std::sync::Mutex;

use crate::ustring::{ee_ustring_assign, ee_ustring_free, ee_ustring_init_text};

use crate::fs::archive_obj::*;
use crate::fs::spec::*;

const DIRS: [&str; 5] = ["src\\", "src\\Sub\\", "src\\Sub\\deep\\", "src\\Other\\", "alt\\"];
const PATTERNS: [&str; 7] = ["*.*", "*", "*.txt", "*.dat", "a*", "?.*", "none"];
const LIST_PATTERNS: [&str; 8] = ["*", "*.txt", "sub\\*", "*a*", "c.dat", "*.", "*sub*.dat", ""];
const TAGS: [&str; 5] = ["Name", "name", "Author", "Description", "Empty"];
const VALUES: [&str; 5] = ["", "x", "Campaign One", "\u{e9}t\u{e9}", "a longer value with spaces"];

static LOCK: Mutex<()> = Mutex::new(());

fn unhex(s: &str) -> Vec<u8> {
    if s == "-" {
        return Vec::new();
    }
    (0..s.len()).step_by(2).map(|i| u8::from_str_radix(&s[i..i + 2], 16).unwrap()).collect()
}

fn byte(seed: u32, i: u32) -> u8 {
    if seed & 1 != 0 {
        b"abcabcabdEE "[((i / 3).wrapping_add(seed) % 12) as usize]
    } else {
        (seed.wrapping_add(i.wrapping_mul(2_654_435_761)) >> 13) as u8
    }
}

fn digest(data: &[u8]) -> u32 {
    data.iter().fold(2_166_136_261u32, |h, &b| (h ^ b as u32).wrapping_mul(16_777_619))
}

struct Str(EeFsString);
impl Str {
    fn new(text: &[u8]) -> Str {
        let mut t = text.to_vec();
        t.push(0);
        let mut s = EeFsString::blank();
        // SAFETY: a record to initialise, a C string
        unsafe { ee_ustring_init_text(s.us(), t.as_ptr().cast()) };
        Str(s)
    }
}
impl Drop for Str {
    fn drop(&mut self) {
        // SAFETY: a live record
        unsafe { ee_ustring_free(self.0.us()) };
    }
}

struct Replay {
    root_dir: String,
    root: String,
    volume: Str,
    directory: Str,
    #[allow(clippy::vec_box)] // (boxed: records the archives point into stay put)
    archives: Vec<Box<EeFsArchiveObj>>,
}

impl Replay {
    fn hex(&self, text: &[u8]) -> String {
        let mut t = text.to_vec();
        let r = self.root_dir.as_bytes();
        while let Some(p) = t.windows(r.len()).position(|w| w == r) {
            t.splice(p..p + r.len(), [1u8]);
        }
        if t.is_empty() {
            return "-".into();
        }
        t.iter().fold(String::new(), |mut o, b| {
            let _ = write!(o, "{b:02x}");
            o
        })
    }

    fn spec_text(&self, s: &EeFsSpec) -> String {
        format!("{}|{}|{}", self.hex(s.volume.bytes()), self.hex(s.directory.bytes()), self.hex(s.file.bytes()))
    }

    /// The driver's `Dir(relative)`: `FSFileSpec()` with the volume and the root's directory set.
    unsafe fn dir(&self, relative: &str) -> TempSpec {
        let mut s = TempSpec::new();
        ee_fs_basic_init(&mut s.0);
        ee_fs_spec_init(&mut s.0, &self.volume.0, &self.directory.0);
        let v = Str::new(&self.root.as_bytes()[..2]);
        ee_ustring_assign(s.0.volume.us(), v.0.usc());
        let d = Str::new(format!("{}{relative}", self.root_dir).as_bytes());
        ee_ustring_assign(s.0.directory.us(), d.0.usc());
        s
    }

    unsafe fn archive_named(&self, file: &str) -> Box<EeFsArchiveObj> {
        let mut spec = self.dir("");
        let f = Str::new(file.as_bytes());
        ee_ustring_assign(spec.0.file.us(), f.0.usc());
        let mut a = Box::new(EeFsArchiveObj {
            vtable: ptr::null(),
            archive_file: TempSpec::new().into_inner(),
            table: ptr::null_mut(),
            mapping: ptr::null_mut(),
            view: ptr::null(),
            view_length: 0,
        });
        ee_fs_basic_init(&mut a.archive_file);
        ee_fs_spec_init(&mut a.archive_file, &self.volume.0, &self.directory.0);
        ee_fs_arch_init_file(&mut *a, &spec.0);
        a
    }

    unsafe fn drop_archives(&mut self) {
        for mut a in self.archives.drain(..) {
            ee_fs_arch_finish(&mut *a);
            // the member FSFileSpec's destructor
            drop(TempSpecOwner(ptr::read(&a.archive_file)));
        }
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
}

/// Frees a spec record that lived inside an archive record.
struct TempSpecOwner(EeFsSpec);
impl Drop for TempSpecOwner {
    fn drop(&mut self) {
        // SAFETY: a live record, dead afterwards
        unsafe {
            ee_fs_spec_finish(&mut self.0);
            ee_ustring_free(self.0.volume.us());
            ee_ustring_free(self.0.directory.us());
            ee_ustring_free(self.0.file.us());
        }
    }
}

fn replay(golden: &str, tag: &str) -> Result<usize, String> {
    let _guard = LOCK.lock().unwrap_or_else(|e| e.into_inner());
    let lines: Vec<&str> = golden.lines().collect();
    let root_length: usize = lines[0].strip_prefix("root ").unwrap().parse().unwrap();
    let base = std::env::temp_dir().display().to_string();
    let base = if base.ends_with('\\') { base } else { base + "\\" };
    let name = format!("eefsa{tag}{}", std::process::id());
    let pad = root_length - (base.len() - 2) - name.len() - 1;
    let root = format!("{base}{name}{}\\", "x".repeat(pad));
    let root_dir = root[2..].to_string();
    let mut r = Replay {
        volume: Str::new(&root.as_bytes()[..2]),
        directory: Str::new(root_dir.as_bytes()),
        root_dir,
        root,
        archives: Vec::new(),
    };
    let mut sequences = 0;
    let mut skipping = false;
    let mut i = 1;
    let result = (|| -> Result<(), String> {
        while i < lines.len() {
            let line = lines[i];
            i += 1;
            if let Some(rest) = line.strip_prefix("seq ") {
                // SAFETY: the archives are live records
                unsafe { r.drop_archives() };
                skipping = rest.ends_with(" p");
                if skipping {
                    continue;
                }
                r.wipe();
                for d in DIRS {
                    let _ = std::fs::create_dir(format!("{}{d}", r.root));
                }
                // SAFETY: fresh records
                r.archives = (0..3).map(|k| unsafe { r.archive_named(&format!("arc{k}.ssa")) }).collect();
                sequences += 1;
                continue;
            }
            if skipping {
                // the native error a skipped sequence left (process-wide in the driver): this replay's from now on
                if let Some((_, ne)) = line.rsplit_once(" ne ") {
                    if let Ok(ne) = ne.parse() {
                        LAST_NATIVE.with(|l| l.set(ne));
                    }
                }
                continue;
            }
            let w: Vec<&str> = line.split(' ').collect();
            let mine = if w[0] == "file" {
                let (rel, length, seed): (Vec<u8>, u32, u32) =
                    (unhex(w[1]), w[2].parse().unwrap(), w[3].parse().unwrap());
                let data: Vec<u8> = (0..length).map(|j| byte(seed, j)).collect();
                std::fs::write(format!("{}{}", r.root, String::from_utf8(rel).unwrap()), data).unwrap();
                continue;
            } else if w[0] == "extract" {
                let k: usize = w[1].parse().unwrap();
                // out\ and, when the driver made them, its subdirectories
                let mut made = vec!["out\\"];
                if w[2] == "1" {
                    made.extend(["out\\sub\\", "out\\sub\\deep\\", "out\\other\\"]);
                }
                for d in made {
                    let _ = std::fs::create_dir(format!("{}{d}", r.root));
                }
                let mut out = Vec::new();
                let destination = unsafe { r.dir("out\\") };
                // SAFETY: live records
                let result = unsafe {
                    extract_all_to(&mut *r.archives[k], &destination.0, &r.volume.0, &r.directory.0, &mut out)
                };
                let text = String::from_utf8_lossy(&out).into_owned();
                for mine in text.lines() {
                    let golden = lines[i];
                    i += 1;
                    // the golden's root (after "Extracting file " and the volume) stands for this replay's
                    let golden_root = &golden[18..18 + r.root_dir.len()];
                    let mine = mine.replace(&r.root_dir, golden_root);
                    if mine != golden {
                        return Err(format!("line {i}:\n  golden {golden}\n  replay {mine}"));
                    }
                }
                let ne = LAST_NATIVE.with(|l| l.get());
                format!("extracted {k} = {result} ne {ne}")
            } else {
                // SAFETY: live records
                let mine = unsafe { line_of(&mut r, &w) };
                format!("{mine} ne {}", LAST_NATIVE.with(|l| l.get()))
            };
            let golden = if w[0] == "extract" {
                i += 1;
                lines[i - 1].to_string()
            } else {
                line.to_string()
            };
            if mine != golden {
                return Err(format!(
                    "line {i}:
  golden {golden}
  replay {mine}"
                ));
            }
        }
        Ok(())
    })();
    // SAFETY: live records
    unsafe { r.drop_archives() };
    r.wipe();
    let _ = std::fs::remove_dir(&r.root);
    result.map(|_| sequences)
}

unsafe fn line_of(r: &mut Replay, w: &[&str]) -> String {
    let k: usize = w[1].parse().unwrap();
    let n = |i: usize| -> i64 { w[i].parse().unwrap() };
    let a: *mut EeFsArchiveObj = &mut *r.archives[k];
    match w[0] {
        "create" => {
            let root = r.dir(DIRS[n(2) as usize]);
            let mut pattern = PATTERNS[n(3) as usize].as_bytes().to_vec();
            pattern.push(0);
            let result = ee_fs_arch_create_walk(
                a,
                &root.0,
                pattern.as_ptr().cast(),
                n(4) as c_ulong,
                n(5) != 0,
                &r.volume.0,
                &r.directory.0,
            );
            format!("create {k} {} {} {} {} = {result}", w[2], w[3], w[4], w[5])
        }
        "createl" => {
            let root = r.dir(DIRS[n(2) as usize]);
            let count = n(3) as usize;
            let names: Vec<Str> = (0..count).map(|j| Str::new(&unhex(w[4 + j]))).collect();
            let pointers: Vec<*const EeFsString> = names.iter().map(|s| &s.0 as *const EeFsString).collect();
            let result = ee_fs_arch_create_list(a, &root.0, pointers.as_ptr(), count, 0x1000);
            format!("{} = {result}", w[..4 + count].join(" "))
        }
        "open" => format!("open {k} = {}", ee_fs_arch_open(a)),
        "close" => format!("close {k} = {}", ee_fs_arch_close(a)),
        "find" => {
            let mut lowered = unhex(w[2]).to_ascii_lowercase();
            let lowered_text = Str::new(&lowered);
            lowered.push(0);
            let mut member = ptr::null_mut();
            let result = ee_fs_arch_find(a, lowered.as_ptr().cast(), &mut member);
            let mut out = format!("find {k} {} = {result}", w[2]);
            if !member.is_null() {
                let mut spec = TempSpec::new();
                ee_fs_basic_init(&mut spec.0);
                ee_fs_spec_init(&mut spec.0, &r.volume.0, &r.directory.0);
                ee_fs_arch_name_member(a, &mut spec.0, &lowered_text.0);
                let file = Box::from_raw(member);
                let _ =
                    write!(out, " {} {} {}", file.length(), digest(file.contents()) as c_long, r.spec_text(&spec.0));
            }
            out
        }
        "settag" => {
            let (t, v) = (n(2) as usize, n(3) as usize);
            (*(*a).table).set_tag(TAGS[t].as_bytes(), VALUES[v].as_bytes());
            format!("settag {k} {t} {v} = 0")
        }
        "gettag" => {
            let t = n(2) as usize;
            let value = (*(*a).table).get_tag(TAGS[t].as_bytes()).map(|v| v.to_vec());
            let (result, shown) = match value {
                Some(v) => (0, r.hex(&v)),
                None => (3, r.hex(b"unset")),
            };
            format!("gettag {k} {t} = {result} {shown}")
        }
        "list" => {
            let (pi, clear) = (n(2) as usize, n(3) != 0);
            let mut set: std::collections::BTreeSet<Vec<u8>> = std::collections::BTreeSet::new();
            if !clear {
                set.insert(b"stale".to_vec());
            }
            for name in (*(*a).table).list_matching(LIST_PATTERNS[pi].as_bytes()) {
                set.insert(name);
            }
            let mut out = format!("list {k} {pi} {} = 0", w[3]);
            for s in &set {
                let _ = write!(out, " {}", r.hex(s));
            }
            out
        }
        "asg" => {
            let j: usize = w[2].parse().unwrap();
            if j != k {
                let from: *const EeFsArchiveObj = &*r.archives[j];
                ee_fs_arch_assign(a, from);
            }
            format!("asg {k} {j}")
        }
        "setarc" => {
            let parts: Vec<&str> = w[2].split('|').collect();
            let mut spec = r.dir("");
            let f = Str::new(&unhex(parts[2]));
            ee_ustring_assign(spec.0.file.us(), f.0.usc());
            let result = ee_fs_arch_set_archive(a, &spec.0);
            format!("setarc {k} {} = {result}", w[2])
        }
        other => panic!("unknown line {other}"),
    }
}

#[test]
fn golden_archive_replays() {
    let n = replay(include_str!("golden_archive.txt"), "a").unwrap_or_else(|e| panic!("{e}"));
    assert_eq!(n, 20);
}

#[test]
fn golden_archive_replays_long_run() {
    let Ok(path) = std::env::var("EE_FS_ARCHIVE_GOLDEN") else {
        return;
    };
    let golden = std::fs::read_to_string(path).unwrap();
    let n = replay(&golden, "b").unwrap_or_else(|e| panic!("{e}"));
    eprintln!("{n} archive sequences replayed");
}
