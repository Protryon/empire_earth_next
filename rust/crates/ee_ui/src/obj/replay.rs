//! The whole objects' golden replay (tests only). A golden is the trace a C++ driver printed over the unmodified
//! class (round 10's drivers, `build/ui10`): its world answered the class's calls out, and the Rust object must make
//! the same calls out, in the same order, with the same arguments. The lines:
//!
//! - `seq N`: a new sequence (the world's state is forgotten);
//! - `new ...` / `free`: the object made or freed (the class's test reads them);
//! - `call C a b f0 f1 f2 f3`: an inward call by the driver (`C` the class's call number, floats as hex bits),
//!   answered by `ret r f0 f1 f2 f3` after the calls out it made;
//! - `op O a b c d f0 f1 f2 f3`: a call out that is not pure (a string op's bytes as their FNV-1a hash: `StringNew`,
//!   `TextSetBytes`, `ClipboardSet`, `TranslatedKey`, `FormMusicStart`), which the
//!   next lines answer: `set` lines, `do` lines
//!   (an inward call the world made meanwhile, like `call`, answered by `ret`), then `= r f0 f1 f2 f3`;
//! - `set O a r f0 f1 f2 f3`: the world's answer to the pure op `O` on `a` from now on (`EeUiOp::is_pure`); a pure op
//!   is answered from these and is not traced.

use core::cell::{Cell, RefCell};
use core::ffi::c_void;
use std::collections::BTreeMap;

use super::{read_buf, write_buf, EeUiHostFn, EeUiOp};

/// A class's inward call: `(object, call, a, b, f)` -> its result.
pub type Dispatch = unsafe fn(target: *const c_void, call: u32, a: isize, b: isize, f: *mut f32) -> isize;

/// The world's answers to the pure ops: (op, a) -> (result, floats).
type Model = BTreeMap<(u32, isize), (isize, [f32; 4])>;

pub struct Replay<'a> {
    lines: Vec<&'a str>,
    pos: Cell<usize>,
    model: RefCell<Model>,
    /// The Rust object the `call` and `do` lines go to.
    pub target: Cell<*const c_void>,
    dispatch: Dispatch,
    /// Calls and ops checked.
    pub checked: Cell<usize>,
    /// The first failure inside a call out.
    failed: RefCell<Option<String>>,
    /// Rewrites a pure op's answer (`(replay, op, a, the world's answer)`): a test's own objects for an answer that
    /// names one (the radio button's `RadioOf`).
    pure_hook: Option<PureHook>,
    /// The bytes of the last op that passed bytes by address (an edit box's `TranslatedKey`, `ClipboardSet`), for a
    /// class's dispatch to hand back in a `do` line.
    pub last_bytes: RefCell<Vec<u8>>,
}

/// A replay's rewrite of a pure op's answer.
pub type PureHook = fn(&Replay, EeUiOp, isize, isize) -> isize;

pub fn hex4(f: &[f32; 4]) -> String {
    format!("{:08x} {:08x} {:08x} {:08x}", f[0].to_bits(), f[1].to_bits(), f[2].to_bits(), f[3].to_bits())
}

fn floats(w: &[&str]) -> [f32; 4] {
    let mut f = [0.0f32; 4];
    for (i, x) in w.iter().take(4).enumerate() {
        f[i] = f32::from_bits(u32::from_str_radix(x, 16).expect("hex float"));
    }
    f
}

/// FNV-1a over NUL-terminated bytes at `p` (the drivers' trace of a string op's bytes).
///
/// # Safety
/// `p` points at NUL-terminated bytes.
pub unsafe fn fnv(p: isize) -> isize {
    // SAFETY: the contract
    let bytes = unsafe { core::ffi::CStr::from_ptr(p as *const core::ffi::c_char) }.to_bytes();
    let mut h: u32 = 2166136261;
    for &b in bytes {
        h = (h ^ b as u32).wrapping_mul(16777619);
    }
    h as isize
}

fn int(w: &str) -> isize {
    w.parse().expect("integer")
}

unsafe extern "C" fn host(obj: *mut c_void, op: EeUiOp, a: isize, b: isize, c: isize, d: isize, f: *mut f32) -> isize {
    // SAFETY: the replay passes itself as the object and outlives the Rust object's calls
    let r = unsafe { &*(obj as *const Replay) };
    if r.failed.borrow().is_some() {
        return 0;
    }
    // a panic may not unwind out of an extern "C" function: the first is kept and raised after the inward call
    // SAFETY: the Rust objects pass four live floats
    let got = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| unsafe { r.on_op(op, a, b, c, d, f) }));
    got.unwrap_or_else(|e| {
        let m = e
            .downcast_ref::<String>()
            .cloned()
            .or_else(|| e.downcast_ref::<&str>().map(|s| s.to_string()))
            .unwrap_or_default();
        r.failed.borrow_mut().get_or_insert(m);
        0
    })
}

impl<'a> Replay<'a> {
    pub fn new(golden: &'a str, dispatch: Dispatch) -> Self {
        Replay {
            lines: golden.lines().filter(|l| !l.is_empty()).collect(),
            pos: Cell::new(0),
            model: RefCell::new(BTreeMap::new()),
            target: Cell::new(core::ptr::null()),
            dispatch,
            checked: Cell::new(0),
            failed: RefCell::new(None),
            pure_hook: None,
            last_bytes: RefCell::new(Vec::new()),
        }
    }

    /// The replay with a rewrite of the pure ops' answers.
    pub fn with_pure_hook(mut self, hook: PureHook) -> Self {
        self.pure_hook = Some(hook);
        self
    }

    /// The world's current answer to the pure op `op` on `a` (a `set` line's), if it gave one.
    pub fn model_value(&self, op: u32, a: isize) -> Option<isize> {
        self.model.borrow().get(&(op, a)).map(|v| v.0)
    }

    /// The host function and object to give a Rust object made in this replay.
    pub fn host(&self) -> (*mut c_void, EeUiHostFn) {
        (self as *const Replay as *mut c_void, Some(host))
    }

    fn next(&self) -> &'a str {
        let p = self.pos.get();
        assert!(p < self.lines.len(), "the golden ended early");
        self.pos.set(p + 1);
        self.lines[p]
    }

    fn peek(&self) -> Option<&'a str> {
        self.lines.get(self.pos.get()).copied()
    }

    fn expect(&self, want: &str) {
        let line = self.pos.get() + 1;
        let got = self.next();
        assert_eq!(got, want, "golden line {line}");
        self.checked.set(self.checked.get() + 1);
    }

    fn set(&self, w: &[&str]) {
        let op: u32 = w[1].parse().unwrap();
        self.model.borrow_mut().insert((op, int(w[2])), (int(w[3]), floats(&w[4..])));
    }

    unsafe fn on_op(&self, op: EeUiOp, a: isize, b: isize, c: isize, d: isize, f: *mut f32) -> isize {
        if op.is_pure() {
            let got = self.model.borrow().get(&(op as u32, a)).copied();
            let (r, out) =
                got.unwrap_or_else(|| panic!("golden line {}: no world value for op {op:?} {a}", self.pos.get()));
            // SAFETY: the contract
            unsafe { write_buf(f, out) };
            return match self.pure_hook {
                Some(h) => h(self, op, a, r),
                None => r,
            };
        }
        // SAFETY: the contract
        let inb = unsafe { read_buf(f) };
        // the string ops' bytes are traced as their hash (the driver's world did the same)
        // SAFETY: those ops pass NUL-terminated bytes
        let (ta, tb) = unsafe {
            match op {
                EeUiOp::StringNew | EeUiOp::FormMusicStart => (fnv(a), b),
                EeUiOp::TextSetBytes => (a, fnv(b)),
                // a combo box's inserted line (R6 ui round 15)
                EeUiOp::ComboAct if a == 1 => (a, fnv(b)),
                EeUiOp::ClipboardSet | EeUiOp::TranslatedKey => {
                    let bytes = core::ffi::CStr::from_ptr(a as *const core::ffi::c_char).to_bytes_with_nul();
                    *self.last_bytes.borrow_mut() = bytes.to_vec();
                    (fnv(a), b)
                }
                _ => (a, b),
            }
        };
        // a combo box's typed key (R6 ui round 15)
        // SAFETY: that action passes NUL-terminated bytes
        let tc = if op == EeUiOp::ComboAct && a == 13 { unsafe { fnv(c) } } else { c };
        self.expect(&format!("op {} {ta} {tb} {tc} {d} {}", op as u32, hex4(&inb)));
        loop {
            let l = self.next();
            let w: Vec<&str> = l.split(' ').collect();
            match w[0] {
                "set" => self.set(&w),
                // SAFETY: the contract
                "do" => unsafe { self.call(&w) },
                "=" => {
                    let out = floats(&w[2..]);
                    // SAFETY: the contract
                    unsafe { write_buf(f, out) };
                    return int(w[1]);
                }
                _ => panic!("golden line {}: unexpected {l:?} in an op's answer", self.pos.get()),
            }
        }
    }

    /// A `call` or `do` line's call, checked against its `ret` line.
    unsafe fn call(&self, w: &[&str]) {
        let call: u32 = w[1].parse().unwrap();
        let mut buf = floats(&w[4..]);
        // SAFETY: the target is the live Rust object the test made
        let r = unsafe { (self.dispatch)(self.target.get(), call, int(w[2]), int(w[3]), buf.as_mut_ptr()) };
        self.raise();
        self.expect(&format!("ret {r} {}", hex4(&buf)));
    }

    /// Runs the whole golden: `other` handles the class's own lines (`new`, `free`) and says whether it did.
    pub fn run(&self, mut other: impl FnMut(&Self, &[&str]) -> bool) {
        while let Some(l) = self.peek() {
            self.next();
            let w: Vec<&str> = l.split(' ').collect();
            match w[0] {
                "seq" => self.model.borrow_mut().clear(),
                "set" => self.set(&w),
                // SAFETY: the target is the live Rust object the test made
                "call" => unsafe { self.call(&w) },
                _ => assert!(other(self, &w), "golden line {}: unexpected {l:?}", self.pos.get()),
            }
            self.raise();
        }
    }

    /// Raises the first failure a call out met.
    fn raise(&self) {
        if let Some(m) = self.failed.borrow_mut().take() {
            panic!("{m}");
        }
    }

    /// An `op` line the class's own lines expect next (the constructor's and destructor's calls out run through
    /// `on_op` already; this is for tests that read a line themselves).
    pub fn take(&self) -> &'a str {
        self.next()
    }
}
