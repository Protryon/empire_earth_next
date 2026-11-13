//! The managers' golden replays (`golden_modman.txt`): sequences of calls recorded by a C++ driver over the original
//! GEModelManager.cpp with traced fakes of the objects it calls (the lock, the paths, the models, the file lookup),
//! replayed here on the Rust object through a mock host that must make the trace's calls in its order.
//!
//! The trace's lines: `call ...` / `ret ...` (a call on the manager and its result; a call inside a model's load comes
//! back into the manager), an operation line per call the manager made (`LockEnter L1`, `ModelSizeOf M3 = 7347`:
//! the fake's answer after ` = `), `~...` (what a fake did on its own: a destructor's releases), `ext ...` (the
//! driver's own reference changes), `file ...` (a dump's text, newlines as `|`) and `end ...` (the objects left).
//! Pure reads (reference counts) are answered from a world the lines keep.

use super::*;
use crate::managers;
use std::collections::BTreeMap as Map;

/// A trace token's object (`M5`: the letter in the high bits, the number in the low ones; `0`: none).
pub(crate) fn id(tok: &[u8]) -> usize {
    if tok == b"0" {
        return 0;
    }
    let n: usize = std::str::from_utf8(&tok[1..]).unwrap().parse().unwrap();
    ((tok[0] as usize) << 32) | n
}

/// The trace token of an object.
pub(crate) fn tok(v: usize) -> Vec<u8> {
    if v == 0 {
        return b"0".to_vec();
    }
    let mut s = vec![(v >> 32) as u8];
    s.extend_from_slice((v & 0xffff_ffff).to_string().as_bytes());
    s
}

pub(crate) fn cat(parts: &[&[u8]]) -> Vec<u8> {
    parts.concat()
}

pub(crate) fn cstr(p: isize) -> Vec<u8> {
    // SAFETY: the Rust object passes NUL-terminated names
    unsafe { c_bytes(p as *const c_char) }.to_vec()
}

/// The bracketed name of a line and the tokens around it.
pub(crate) fn split_named(line: &[u8]) -> (Vec<&[u8]>, Vec<u8>, Vec<&[u8]>) {
    let open = line.iter().position(|&c| c == b'[').unwrap();
    let close = line.iter().rposition(|&c| c == b']').unwrap();
    let before = line[..open].split(|&c| c == b' ').filter(|t| !t.is_empty()).collect();
    let after = line[close + 1..].split(|&c| c == b' ').filter(|t| !t.is_empty()).collect();
    (before, line[open + 1..close].to_vec(), after)
}

pub(crate) fn num(t: &[u8]) -> i64 {
    std::str::from_utf8(t).unwrap().parse().unwrap()
}

#[derive(Default)]
struct World {
    refs: Map<usize, i64>,
    names: Map<usize, Vec<u8>>,
    depth: Map<usize, i64>,
    paths: Map<usize, i64>,
    locks: i64,
    files: i64,
    dumps: Map<usize, Vec<u8>>,
}

struct Replay {
    lines: Vec<Vec<u8>>,
    at: usize,
    world: World,
    mgr: *mut EeGeModelManager,
    last_dump: usize,
    /// A mock host's failure (a panic cannot unwind through the Rust object's `extern "C"` host call).
    failed: Option<String>,
}

impl Replay {
    fn peek(&self) -> &[u8] {
        self.lines.get(self.at).map_or(b"", |l| l.as_slice())
    }

    fn fail(&self, what: &str) -> ! {
        let around: Vec<String> = self.lines[self.at.saturating_sub(6)..(self.at + 3).min(self.lines.len())]
            .iter()
            .map(|l| String::from_utf8_lossy(l).into_owned())
            .collect();
        panic!("{what} at line {} of the sequence:\n{}", self.at, around.join("\n"));
    }

    /// The world's part of a line.
    fn apply(&mut self, line: &[u8]) {
        let line = line.strip_prefix(b"~").unwrap_or(line);
        let t: Vec<&[u8]> = line.split(|&c| c == b' ').collect();
        let w = &mut self.world;
        match t[0] {
            b"ModelNew" | b"ModelNewEmpty" => {
                w.refs.insert(id(t[1]), 1);
                if t[0] == b"ModelNew" {
                    w.names.insert(id(t[1]), split_named(line).1);
                } else {
                    w.names.insert(id(t[1]), Vec::new());
                }
            }
            b"Reference" => *w.refs.get_mut(&id(t[1])).unwrap() += 1,
            b"BaseDereference" => *w.refs.get_mut(&id(t[1])).unwrap() -= 1,
            b"destroy" => {
                w.refs.remove(&id(t[1]));
            }
            b"ext" => {
                let r = w.refs.get_mut(&id(t[2])).unwrap();
                *r += if t[1] == b"ref" { 1 } else { -1 };
            }
            b"LockEnter" => *w.depth.entry(id(t[1])).or_default() += 1,
            b"LockLeave" => *w.depth.entry(id(t[1])).or_default() -= 1,
            b"LockCreate" => w.locks += 1,
            b"LockDestroy" => w.locks -= 1,
            b"PathReference" => *w.paths.entry(id(t[1])).or_insert(1) += 1,
            b"PathDereference" => *w.paths.entry(id(t[1])).or_insert(1) -= 1,
            b"ModelFindFile" if line.ends_with(b"= 2") => {}
            b"ModelFindFile" => w.files += 1,
            b"ModelFreeFile" => w.files -= 1,
            _ => {}
        }
    }

    /// The next line, which must be `want` (then the fakes' own lines after it).
    fn expect(&mut self, want: &[u8]) {
        if self.peek() != want {
            self.fail(&format!("expected `{}`", String::from_utf8_lossy(want)));
        }
        self.take();
    }

    /// The next line, which must start with `want` and ` = `: the answer's tokens.
    fn expect_answer(&mut self, want: &[u8]) -> Vec<Vec<u8>> {
        let line = self.peek().to_vec();
        let prefix = cat(&[want, b" = "]);
        if !line.starts_with(&prefix) {
            self.fail(&format!("expected `{} = ...`", String::from_utf8_lossy(want)));
        }
        self.take();
        line[prefix.len()..].split(|&c| c == b' ').map(|t| t.to_vec()).collect()
    }

    fn take(&mut self) {
        let line = self.peek().to_vec();
        self.apply(&line);
        self.at += 1;
        while self.peek().starts_with(b"~") {
            let side = self.peek().to_vec();
            self.apply(&side);
            self.at += 1;
        }
    }

    /// The calls made back into the manager inside an operation.
    fn nested(&mut self) {
        while self.peek().starts_with(b"call ") {
            self.call();
        }
    }

    fn op(&mut self, op: EeGeManOp, obj: usize, a: isize, b: isize, c: isize) -> isize {
        use EeGeManOp as O;
        let o = tok(obj);
        match op {
            O::LockCreate => id(&self.expect_answer(b"LockCreate")[0]) as isize,
            O::LockDestroy => {
                let d = self.world.depth.get(&obj).copied().unwrap_or(0).to_string();
                self.expect(&cat(&[b"LockDestroy ", &o, b" depth ", d.as_bytes()]));
                0
            }
            O::LockEnter => {
                self.expect(&cat(&[b"LockEnter ", &o]));
                0
            }
            O::LockLeave => {
                self.expect(&cat(&[b"LockLeave ", &o]));
                0
            }
            O::PathReference => {
                self.expect(&cat(&[b"PathReference ", &o]));
                0
            }
            O::PathDereference => {
                self.expect(&cat(&[b"PathDereference ", &o]));
                0
            }
            O::RefCount => self.world.refs[&obj] as isize,
            O::BaseDereference => {
                self.expect(&cat(&[b"BaseDereference ", &o]));
                0
            }
            O::DumpHeader => {
                let d = self.world.dumps.entry(obj).or_default();
                d.extend_from_slice(b"\n\n*********Model Manager - Current Loaded Files In Memory*********\n\n");
                0
            }
            O::DumpLine => {
                let count = self.world.refs[&obj].to_string();
                let line = cat(&[
                    b"The ",
                    &cstr(a),
                    b" model file is loaded in memory with a reference count of ",
                    count.as_bytes(),
                    b".\n",
                ]);
                self.world.dumps.entry(b as usize).or_default().extend_from_slice(&line);
                0
            }
            O::ModelNew | O::ModelNewSub => {
                // SAFETY: the replay's calls pass seven counts
                let n = unsafe { std::slice::from_raw_parts(a as *const c_ulong, 7) };
                let name = if op == O::ModelNew { cstr(b) } else { self.world.names[&obj].clone() };
                let line = self.peek().to_vec();
                let (before, got_name, after) = split_named(&line);
                let mut want = vec![b"ModelNew".to_vec(), before.get(1).map_or(b"?".to_vec(), |t| t.to_vec())];
                want.extend(n[..6].iter().map(|v| v.to_string().into_bytes()));
                let model = id(&want[1]);
                if before.iter().map(|t| t.to_vec()).collect::<Vec<_>>() != want
                    || got_name != name
                    || after != [n[6].to_string().as_bytes(), b"G"]
                {
                    self.fail("a different ModelNew");
                }
                self.take();
                if op == O::ModelNewSub {
                    let index = (b as c_ulong).to_string();
                    self.expect(&cat(&[b"SetSubModel ", &o, b" ", &tok(model), b" ", index.as_bytes()]));
                    self.expect(&cat(&[b"BaseDereference ", &tok(model)]));
                }
                model as isize
            }
            O::ModelFindFile => {
                let want = cat(&[b"ModelFindFile ", &o, b" [", &cstr(a), b"] ", &tok(b as usize)]);
                let r = self.expect_answer(&want);
                let result = num(&r[0]);
                if result == 0 {
                    // SAFETY: the out pointer the Rust object passes
                    unsafe { *(c as *mut usize) = id(&r[1]) };
                }
                result as isize
            }
            O::ModelNewEmpty => {
                let line = self.peek().to_vec();
                let t: Vec<&[u8]> = line.split(|&c| c == b' ').collect();
                if t.len() != 3 || t[0] != b"ModelNewEmpty" || t[2] != b"G" {
                    self.fail("expected ModelNewEmpty");
                }
                let model = id(t[1]);
                self.take();
                model as isize
            }
            O::ModelStoreLoaded => {
                self.expect(&cat(&[b"ModelStoreLoaded ", &o, b" ", &tok(a as usize), b" ", &tok(b as usize)]));
                self.nested();
                let line = self.peek().to_vec();
                if !line.starts_with(b"=> ") {
                    self.fail("expected the load's result");
                }
                self.take();
                num(&line[3..]) as isize
            }
            O::ModelSizeOf => num(&self.expect_answer(&cat(&[b"ModelSizeOf ", &o]))[0]) as isize,
            O::ModelVerify => {
                self.expect(&cat(&[b"ModelVerify ", &o]));
                0
            }
            O::ModelFreeFile => {
                self.expect(&cat(&[b"ModelFreeFile ", &o]));
                0
            }
            _ => self.fail(&format!("a texture operation {op:?}")),
        }
    }

    /// One `call` line and its `ret`.
    fn call(&mut self) {
        let line = self.peek().to_vec();
        self.take();
        let t: Vec<&[u8]> = line.split(|&c| c == b' ').collect();
        let p = self.mgr;
        let me = self as *mut Replay as *mut c_void;
        // SAFETY: the object is the replay's; the mock host serves it
        let ret: Vec<u8> = unsafe {
            match t[1] {
                b"new" => {
                    self.mgr = EeGeModelManager::create(host, me, id(t[2]));
                    Vec::new()
                }
                b"free" => {
                    EeGeModelManager::destroy(p);
                    Vec::new()
                }
                b"create" => {
                    let (before, name, _) = split_named(&line);
                    let n: Vec<c_ulong> = before[2..].iter().map(|v| num(v) as c_ulong).collect();
                    let cname = c_string(&name);
                    tok(EeGeModelManager::create_model(p, n.as_ptr().cast(), cname.as_ptr().cast()))
                }
                b"create_sub" => {
                    let n: Vec<c_ulong> = t[2..9].iter().map(|v| num(v) as c_ulong).collect();
                    let index = num(t[9]) as c_ulong;
                    tok(EeGeModelManager::create_sub_model(p, n.as_ptr().cast(), index, id(t[10])))
                }
                b"load" => {
                    let (_, name, after) = split_named(&line);
                    let cname = c_string(&name);
                    tok(EeGeModelManager::load(p, cname.as_ptr().cast(), id(after[0]) as *mut c_void))
                }
                b"remove" => EeGeModelManager::remove(p, id(t[2])).to_string().into_bytes(),
                b"remove_all" => EeGeModelManager::remove_all(p).to_string().into_bytes(),
                b"set_path" => {
                    EeGeModelManager::set_path(p, id(t[2]));
                    Vec::new()
                }
                b"verify" => {
                    EeGeModelManager::verify(p);
                    Vec::new()
                }
                b"dump" => {
                    self.last_dump = id(t[2]);
                    EeGeModelManager::dump(p, id(t[2]));
                    Vec::new()
                }
                b"footprint" => mm(p).footprint.to_string().into_bytes(),
                b"count" => mm(p).models.len().to_string().into_bytes(),
                _ => self.fail("an unknown call"),
            }
        };
        if let Some(what) = self.failed.take() {
            panic!("{what}");
        }
        let want = if ret.is_empty() { b"ret".to_vec() } else { cat(&[b"ret ", &ret]) };
        self.expect(&want);
    }

    fn sequence(&mut self) {
        loop {
            let line = self.peek().to_vec();
            if line.starts_with(b"call ") {
                self.call();
            } else if line.starts_with(b"ext ") {
                self.take();
            } else if let Some(text) = line.strip_prefix(b"file ") {
                let file = self.last_dump;
                let got: Vec<u8> = self.world.dumps.remove(&file).unwrap_or_default();
                let got: Vec<u8> = got.iter().map(|&c| if c == b'\n' { b'|' } else { c }).collect();
                if got != text {
                    self.fail("a different dump");
                }
                self.at += 1;
            } else if line.starts_with(b"end ") {
                let w = &self.world;
                let paths: Vec<String> = (1..=3)
                    .map(|i| w.paths.get(&id(format!("P{i}").as_bytes())).copied().unwrap_or(1).to_string())
                    .collect();
                let want = format!(
                    "end models {} paths {} locks {} files {}",
                    w.refs.len(),
                    paths.join(" "),
                    w.locks,
                    w.files
                );
                if line != want.as_bytes() {
                    self.fail(&format!("expected `{want}`"));
                }
                return;
            } else {
                self.fail("an unexpected line");
            }
        }
    }
}

unsafe extern "C" fn host(owner: *mut c_void, op: EeGeManOp, obj: *mut c_void, a: isize, b: isize, c: isize) -> isize {
    // SAFETY: the owner is the replay running the sequence
    let r = unsafe { &mut *(owner as *mut Replay) };
    if r.failed.is_some() {
        return 0;
    }
    match std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| r.op(op, obj as usize, a, b, c))) {
        Ok(v) => v,
        Err(e) => {
            let what = e.downcast_ref::<String>().cloned().or_else(|| e.downcast_ref::<&str>().map(|s| s.to_string()));
            r.failed = Some(what.unwrap_or_default());
            0
        }
    }
}

/// The golden's sequences: (number, lines).
pub(crate) fn sequences(golden: &[u8]) -> Vec<(usize, Vec<Vec<u8>>)> {
    let mut out: Vec<(usize, Vec<Vec<u8>>)> = Vec::new();
    for line in golden.split(|&c| c == b'\n') {
        let line = line.strip_suffix(b"\r").unwrap_or(line);
        if let Some(n) = line.strip_prefix(b"seq ") {
            out.push((num(n) as usize, Vec::new()));
        } else if !line.is_empty() {
            out.last_mut().unwrap().1.push(line.to_vec());
        }
    }
    out
}

/// The created models' counter at a sequence's start: the number of its first created name.
fn first_created_number(lines: &[Vec<u8>]) -> Option<u32> {
    let call = lines.iter().position(|l| l.starts_with(b"call create "))?;
    let line = lines[call..].iter().find(|l| l.starts_with(b"ModelNew "))?;
    let (_, name, _) = split_named(line);
    let end = name.iter().rposition(|&c| c == b')')?;
    let start = name[..end].iter().rposition(|&c| c == b'_')? + 1;
    std::str::from_utf8(&name[start..end]).ok()?.parse().ok()
}

#[test]
fn the_model_managers_golden_sequences_replay() {
    let golden = include_bytes!("golden_modman.txt");
    let seqs = sequences(golden);
    assert!(seqs.len() >= 40);
    let _globals = managers::test_globals();
    let mut lines_replayed = 0;
    for (n, lines) in seqs {
        if let Some(number) = first_created_number(&lines) {
            managers::set_numbers_for_test(None, Some(number));
        }
        let count = lines.len();
        let mut r =
            Replay { lines, at: 0, world: World::default(), mgr: std::ptr::null_mut(), last_dump: 0, failed: None };
        let result = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| r.sequence()));
        if let Err(e) = result {
            let what = e.downcast_ref::<String>().cloned().or_else(|| e.downcast_ref::<&str>().map(|s| s.to_string()));
            panic!("sequence {n}: {}", what.unwrap_or_default());
        }
        lines_replayed += count;
    }
    assert!(lines_replayed > 5000, "{lines_replayed}");
}

#[test]
fn the_walk_goes_on_after_a_removed_or_added_entry() {
    let mut m: BTreeMap<Vec<u8>, usize> = BTreeMap::new();
    for (k, v) in [(b"a".as_slice(), 1), (b"b", 2), (b"d", 4)] {
        m.insert(k.to_vec(), v);
    }
    let (k, _) = next_after(&m, None).unwrap();
    assert_eq!(k, b"a");
    m.remove(b"a".as_slice());
    m.insert(b"c".to_vec(), 3);
    assert_eq!(next_after(&m, Some(b"a")).unwrap(), (b"b".to_vec(), 2));
    assert_eq!(next_after(&m, Some(b"b")).unwrap(), (b"c".to_vec(), 3));
    assert_eq!(next_after(&m, Some(b"d")), None);
}

#[test]
fn names_are_strcmp_ordered_bytes() {
    let mut m: BTreeMap<Vec<u8>, usize> = BTreeMap::new();
    for (i, k) in [b"ab".as_slice(), b"a", b"B", b"\xe9", b""].iter().enumerate() {
        m.insert(k.to_vec(), i);
    }
    let keys: Vec<&[u8]> = m.keys().map(|k| k.as_slice()).collect();
    assert_eq!(keys, [b"".as_slice(), b"B", b"a", b"ab", b"\xe9"]);
    assert_eq!(c_string(b"x\0y"), b"x\0");
}
