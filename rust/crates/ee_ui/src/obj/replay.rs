//! The whole objects' golden replay (tests only). A golden is the trace a C++ driver printed over the unmodified
//! class (round 10's drivers, `build/ui10`): its world answered the class's calls out, and the Rust object must make
//! the same calls out, in the same order, with the same arguments. The lines:
//!
//! - `seq N`: a new sequence (the world's state is forgotten);
//! - `new ...` / `free`: the object made or freed (the class's test reads them);
//! - `call C a b f0 f1 f2 f3`: an inward call by the driver (`C` the class's call number, floats as hex bits),
//!   answered by `ret r f0 f1 f2 f3` after the calls out it made;
//! - `op O a b c d f0 f1 f2 f3`: a call out that is not pure, which the next lines answer: `set` lines, `do` lines
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
}

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

fn int(w: &str) -> isize {
    w.parse().expect("integer")
}

unsafe extern "C" fn host(obj: *mut c_void, op: EeUiOp, a: isize, b: isize, c: isize, d: isize, f: *mut f32) -> isize {
    // SAFETY: the replay passes itself as the object and outlives the Rust object's calls
    let r = unsafe { &*(obj as *const Replay) };
    // SAFETY: the Rust objects pass four live floats
    unsafe { r.on_op(op, a, b, c, d, f) }
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
        }
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
            return r;
        }
        // SAFETY: the contract
        let inb = unsafe { read_buf(f) };
        self.expect(&format!("op {} {a} {b} {c} {d} {}", op as u32, hex4(&inb)));
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
        }
    }

    /// An `op` line the class's own lines expect next (the constructor's and destructor's calls out run through
    /// `on_op` already; this is for tests that read a line themselves).
    pub fn take(&self) -> &'a str {
        self.next()
    }
}
