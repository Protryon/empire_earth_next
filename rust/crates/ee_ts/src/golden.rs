//! Replays the original Thread System's trace (`tests/thread/ts_golden.cpp`, format in its header) on the Rust
//! objects. Each platform call the Rust makes must be the next recorded one (name and arguments) and is answered
//! with the recorded result; every other line is regenerated. `golden_ts.txt` holds the first 60 sequences;
//! `EE_TS_GOLDEN=<file>` replays a longer run.

use super::*;
use std::cell::{Cell, RefCell};

const OUTPUT: [&str; 11] = ["plat", "native", "verify", "pump", "fn", "=", "CS", "EV", "RW", "TH", "S"];

fn is_output(line: &str) -> bool {
    OUTPUT.contains(&line.split(' ').next().unwrap_or(""))
}

struct Recorded<'a> {
    lines: &'a [&'a str],
    pos: Cell<usize>,
    failed: RefCell<Option<String>>,
    current: Cell<u32>,
    last_error: Cell<u32>,
}

thread_local! {
    static RECORDED: Cell<*const Recorded<'static>> = const { Cell::new(std::ptr::null()) };
}

impl Recorded<'_> {
    fn fail(&self, what: String) {
        let mut f = self.failed.borrow_mut();
        if f.is_none() {
            *f = Some(what);
        }
    }
    /// Checks one regenerated line against the next recorded one.
    fn line(&self, got: String) {
        let k = self.pos.get();
        match self.lines.get(k) {
            Some(&want) if want == got => self.pos.set(k + 1),
            want => self.fail(format!("line {k}: golden {want:?}, rust {got:?}")),
        }
    }
    /// A platform call with a result: "plat <call> -> <result>" -> the recorded result's fields.
    fn call(&self, call: String) -> Vec<i64> {
        let k = self.pos.get();
        let prefix = format!("plat {call} -> ");
        match self.lines.get(k) {
            Some(want) if want.starts_with(&prefix) => {
                self.pos.set(k + 1);
                want[prefix.len()..].split(' ').map(|v| v.parse().unwrap()).collect()
            }
            want => {
                self.fail(format!("line {k}: golden {want:?}, rust {prefix:?}"));
                vec![0, 0]
            }
        }
    }
    fn void(&self, call: String) {
        self.line(format!("plat {call}"));
    }
}

fn h(p: *const c_void) -> usize {
    p as usize
}
fn p(v: i64) -> *mut c_void {
    v as usize as *mut c_void
}

impl Host for Recorded<'_> {
    fn current_thread_id(&self) -> u32 {
        self.current.get()
    }
    fn last_error(&self) -> u32 {
        self.last_error.get()
    }
    fn record_native(&self, error: u32) {
        self.line(format!("native {error}"));
    }
    fn verify(&self, line: u32, expression: &str) {
        self.line(format!("verify {line} {expression}"));
    }
    fn mutex_create(&self) -> *mut c_void {
        p(self.call("mcreate".into())[0])
    }
    fn mutex_destroy(&self, m: *mut c_void) {
        self.void(format!("mdestroy {}", h(m)))
    }
    fn mutex_lock(&self, m: *mut c_void) {
        self.void(format!("mlock {}", h(m)))
    }
    fn mutex_unlock(&self, m: *mut c_void) {
        self.void(format!("munlock {}", h(m)))
    }
    fn mutex_try_lock(&self, m: *mut c_void) -> bool {
        self.call(format!("mtry {}", h(m)))[0] != 0
    }
    fn event_create(&self, manual: bool, set: bool) -> *mut c_void {
        p(self.call(format!("ecreate {} {}", manual as u8, set as u8))[0])
    }
    fn event_destroy(&self, e: *mut c_void) {
        self.void(format!("edestroy {}", h(e)))
    }
    fn event_set(&self, e: *mut c_void) -> bool {
        self.call(format!("eset {}", h(e)))[0] != 0
    }
    fn event_reset(&self, e: *mut c_void) -> bool {
        self.call(format!("ereset {}", h(e)))[0] != 0
    }
    fn event_wait(&self, e: *mut c_void, t: u32) -> i32 {
        self.call(format!("ewait {} {t}", h(e)))[0] as i32
    }
    fn event_wait_any(&self, events: &[*mut c_void], t: u32) -> i32 {
        let list: String = events.iter().map(|&e| format!(" {}", h(e))).collect();
        self.call(format!("eany{list} {t}"))[0] as i32
    }
    fn thread_create(&self, _thread: *mut c_void, stack: u32, suspended: bool, id: &mut u32) -> *mut c_void {
        let r = self.call(format!("tcreate {stack} {}", suspended as u8));
        if r[0] != 0 {
            *id = r[1] as u32;
        }
        p(r[0])
    }
    fn thread_open_current(&self) -> *mut c_void {
        p(self.call("topen".into())[0])
    }
    fn thread_close(&self, t: *mut c_void) {
        self.void(format!("tclose {}", h(t)))
    }
    fn thread_exit_code(&self, t: *mut c_void, code: &mut u32) -> bool {
        let r = self.call(format!("texit {}", h(t)));
        if r[0] != 0 {
            *code = r[1] as u32;
        }
        r[0] != 0
    }
    fn thread_priority(&self, t: *mut c_void) -> i32 {
        self.call(format!("tprio {}", h(t)))[0] as i32
    }
    fn thread_set_priority(&self, t: *mut c_void, priority: i32) -> bool {
        self.call(format!("tsetprio {} {priority}", h(t)))[0] != 0
    }
    fn thread_suspend(&self, t: *mut c_void) -> u32 {
        self.call(format!("tsuspend {}", h(t)))[0] as u32
    }
    fn thread_resume(&self, t: *mut c_void) -> u32 {
        self.call(format!("tresume {}", h(t)))[0] as u32
    }
    fn thread_terminate(&self, t: *mut c_void) -> bool {
        self.call(format!("tterm {}", h(t)))[0] != 0
    }
    fn sleep(&self, ms: u32) {
        self.void(format!("sleep {ms}"))
    }
    fn pump(&self, _pump: unsafe extern "C" fn() -> bool) -> bool {
        let k = self.pos.get();
        match self.lines.get(k) {
            Some(&"pump -> 1") => {
                self.pos.set(k + 1);
                true
            }
            Some(&"pump -> 0") => {
                self.pos.set(k + 1);
                false
            }
            want => {
                self.fail(format!("line {k}: golden {want:?}, rust pump"));
                false
            }
        }
    }
}

unsafe extern "C" fn pump() -> bool {
    unreachable!("the recorded host answers the pump")
}
unsafe extern "C" fn thread_function(param: *mut c_void) -> c_ulong {
    RECORDED.with(|r| (*r.get()).line(format!("fn {}", param as usize)));
    (param as usize as c_ulong).wrapping_mul(3).wrapping_add(1)
}

struct Objects {
    sections: [Option<EeTsSection>; 4],
    events: [Option<Box<EeTsEvent>>; 4],
    locks: [Option<Box<EeTsLock>>; 4],
    threads: [*mut EeTsThread; 4],
    sch: Scheduler,
}

fn section_text(s: (*mut c_void, u32, u32)) -> String {
    format!("{} {} {}", h(s.0), s.1, s.2)
}

impl Objects {
    fn dump_section(&self, r: &Recorded, i: usize) {
        r.line(match &self.sections[i] {
            Some(s) => format!("CS {i} {}", section_text(s.state())),
            None => format!("CS {i} -"),
        })
    }
    fn dump_event(&self, r: &Recorded, i: usize) {
        r.line(match &self.events[i] {
            Some(e) => format!("EV {i} {} {}", h(e.handle()), e.is_manual() as u8),
            None => format!("EV {i} -"),
        })
    }
    fn dump_lock(&self, r: &Recorded, i: usize) {
        r.line(match &self.locks[i] {
            Some(l) => {
                let (w, a, b) = l.state();
                format!("RW {i} {} {} {} {}", section_text(w), h(a), h(b), l.read_count())
            }
            None => format!("RW {i} -"),
        })
    }
    fn dump_thread(&self, r: &Recorded, i: usize) {
        let t = self.threads[i];
        if t.is_null() {
            return r.line(format!("TH {i} -"));
        }
        // SAFETY: a live slot
        let t = unsafe { &*t };
        let (handle, id, susp, post, lock, ids, posted) = t.state();
        let ids: Vec<String> = ids.iter().map(|v| v.to_string()).collect();
        let posted: Vec<String> =
            posted.iter().map(|m| format!("{}/{}/{}", m.message, h(m.parameter1), h(m.parameter2))).collect();
        let name = t.name();
        r.line(format!(
            "TH {i} {} {id} {} {} {} [{}] [{}] {}",
            h(handle),
            susp as u8,
            h(post),
            section_text(lock),
            ids.join(" "),
            posted.join(" "),
            if name.is_empty() { "-".into() } else { String::from_utf8_lossy(&name).into_owned() }
        ))
    }
    fn dump_scheduler(&self, r: &Recorded) {
        // SAFETY: registered threads are live
        let list: String =
            self.sch.list().iter().map(|&t| unsafe { format!(" {}/{}", h((*t).handle()), (*t).thread_id()) }).collect();
        r.line(format!("S{list}"));
    }

    fn act(&mut self, r: &Recorded, f: &[&str]) {
        let n = |k: usize| -> i64 { f[k].parse().unwrap() };
        let i = if f.len() > 1 { f[1].parse::<usize>().unwrap_or(0) } else { 0 };
        match f[0] {
            "cur" => r.current.set(n(1) as u32),
            "err" => r.last_error.set(n(1) as u32),
            "cs.new" => {
                self.sections[i] = Some(EeTsSection::new(r));
                r.line("= 0".into());
            }
            "cs.del" => self.sections[i].take().unwrap().destroy(r),
            "cs.enter" => self.sections[i].as_ref().unwrap().enter(r),
            "cs.leave" => self.sections[i].as_ref().unwrap().leave(r),
            "cs.try" => {
                let v = self.sections[i].as_ref().unwrap().try_enter(r);
                r.line(format!("= {}", v as u8));
            }
            "ev.new" => {
                if let Some(e) = self.events[i].take() {
                    e.destroy(r);
                }
                let result = match EeTsEvent::create(r, n(2) != 0) {
                    Ok(e) => {
                        self.events[i] = Some(e);
                        EE_TS_OK
                    }
                    Err(v) => v,
                };
                r.line(format!("= {result}"));
            }
            "ev.del" => self.events[i].take().unwrap().destroy(r),
            "ev.set" => r.line(format!("= {}", self.events[i].as_ref().unwrap().set(r))),
            "ev.clear" => r.line(format!("= {}", self.events[i].as_ref().unwrap().clear(r))),
            "ev.wait" => r.line(format!("= {}", self.events[i].as_ref().unwrap().wait(r, n(2) as u32))),
            "ev.manual" => r.line(format!("= {}", self.events[i].as_ref().unwrap().is_manual() as u8)),
            "ev.handle" => r.line(format!("= {}", h(self.events[i].as_ref().unwrap().handle()))),
            "ev.any" => {
                let count = n(3) as usize;
                let list: Vec<&EeTsEvent> =
                    (0..count).map(|k| &**self.events[n(4 + k) as usize].as_ref().unwrap()).collect();
                r.line(format!("= {}", EeTsEvent::wait_any(r, &list, n(2) as u32)));
            }
            "rw.new" => match EeTsLock::create(r) {
                Ok(l) => {
                    self.locks[i] = Some(l);
                    r.line("= 0".into());
                }
                Err(v) => r.line(format!("= {v}")),
            },
            "rw.del" => self.locks[i].take().unwrap().destroy(r),
            "rw.rl" => self.locks[i].as_ref().unwrap().read_lock(r),
            "rw.ru" => self.locks[i].as_ref().unwrap().read_unlock(r),
            "rw.wl" => self.locks[i].as_ref().unwrap().write_lock(r),
            "rw.wu" => self.locks[i].as_ref().unwrap().write_unlock(r),
            "rw.count" => r.line(format!("= {}", self.locks[i].as_ref().unwrap().read_count())),
            "th.new" => {
                let (t, result) =
                    self.sch.create_thread(r, Some(thread_function), p(16 * (i as i64 + 1)), n(2) as u32, n(3) != 0);
                self.threads[i] = t;
                r.line(format!("= {result}"));
                self.dump_thread(r, i);
                self.dump_scheduler(r);
                return;
            }
            "th.del" => {
                let result = unsafe { self.sch.destroy_thread(r, self.threads[i]) };
                r.line(format!("= {result}"));
                self.threads[i] = std::ptr::null_mut();
                self.dump_scheduler(r);
                return;
            }
            "th.del0" => {
                r.line(format!("= {}", unsafe { self.sch.destroy_thread(r, std::ptr::null_mut()) }));
                self.dump_thread(r, i);
                return;
            }
            "shutdown" => {
                self.sch.shutdown(r);
                self.dump_scheduler(r);
                let live = self.sch.list();
                for t in self.threads.iter_mut() {
                    if !t.is_null() && !live.contains(t) {
                        *t = std::ptr::null_mut();
                    }
                }
                return;
            }
            "init" => {
                let registered = r.call("shutdownreg".into())[0];
                let result = if registered != 0 { 9 } else { self.sch.initialize(r) };
                r.line(format!("= {result}"));
                self.dump_scheduler(r);
                return;
            }
            "sch.cur" => {
                let t = self.sch.current(r);
                r.line(format!("= {}", if t.is_null() { 0 } else { unsafe { h((*t).handle()) } }));
                return;
            }
            "sch.idx" => {
                r.line(format!("= {}", self.sch.current_index(r).map_or(u32::MAX, |v| v as u32)));
                return;
            }
            "setpump" => {
                self.sch.set_pump(if n(1) != 0 { Some(pump) } else { None }, n(2) as u32);
                return;
            }
            "dispatch" => {
                self.sch.dispatch(r);
                return;
            }
            "end" => {
                for k in 0..4 {
                    if let Some(s) = self.sections[k].take() {
                        s.destroy(r);
                    }
                    if let Some(e) = self.events[k].take() {
                        e.destroy(r);
                    }
                    if let Some(l) = self.locks[k].take() {
                        l.destroy(r);
                    }
                    self.threads[k] = std::ptr::null_mut();
                }
                while let Some(&t) = self.sch.list().first() {
                    unsafe { self.sch.destroy_thread(r, t) };
                }
                self.sch.set_pump(None, 0);
                return;
            }
            other => {
                // the thread calls
                // SAFETY: a live slot
                let t = unsafe { &*self.threads[i] };
                let line = match other {
                    "th.post" => Some(format!("= {}", t.post(r, n(2) as c_ulong, p(n(3)), p(n(4)), n(5) != 0))),
                    "th.recv" => {
                        let (result, m) = t.receive(r, &self.sch);
                        let (msg, a, b) =
                            m.map_or((7, 8, 9), |m| (m.message as usize, h(m.parameter1), h(m.parameter2)));
                        Some(format!("= {result} {msg} {a} {b}"))
                    }
                    "th.resume" => Some(format!("= {}", t.resume(r))),
                    "th.suspend" => Some(format!("= {}", t.suspend(r))),
                    "th.term" => Some(format!("= {}", t.terminate(r))),
                    "th.prio" => Some(format!("= {}", t.priority(r))),
                    "th.setprio" => Some(format!("= {}", t.set_priority(r, n(2) as i32))),
                    "th.exit" => Some(format!("= {}", t.exit_code(r))),
                    "th.lock" => {
                        t.message_lock().enter(r);
                        None
                    }
                    "th.unlock" => {
                        t.message_lock().leave(r);
                        None
                    }
                    "th.run" => Some(format!("= {}", t.run())),
                    "th.issusp" => Some(format!("= {}", t.is_suspended() as u8)),
                    "th.isterm" => Some(format!("= {}", (t.exit_code(r) != 259) as u8)),
                    "th.name" => {
                        t.set_name(f[2].as_bytes());
                        None
                    }
                    _ => panic!("unknown input {other}"),
                };
                if let Some(l) = line {
                    r.line(l);
                }
                self.dump_thread(r, i);
                return;
            }
        }
        match f[0] {
            c if c.starts_with("cs.") => self.dump_section(r, i),
            c if c.starts_with("ev.") => self.dump_event(r, i),
            c if c.starts_with("rw.") => self.dump_lock(r, i),
            _ => {}
        }
    }
}

/// -> (lines checked, sequences with a difference, the first difference)
fn replay(text: &str) -> (usize, usize, Option<String>) {
    let all: Vec<&str> = text.lines().collect();
    let starts: Vec<usize> = all.iter().enumerate().filter(|(_, l)| l.starts_with("seq ")).map(|(k, _)| k).collect();
    let (mut failed, mut first) = (0, None);
    for (s, &start) in starts.iter().enumerate() {
        let end = starts.get(s + 1).copied().unwrap_or(all.len());
        let lines = &all[start..end];
        let r = Recorded {
            lines,
            pos: Cell::new(1),
            failed: RefCell::new(None),
            current: Cell::new(0),
            last_error: Cell::new(0),
        };
        let mut objects = Objects {
            sections: Default::default(),
            events: Default::default(),
            locks: Default::default(),
            threads: [std::ptr::null_mut(); 4],
            sch: Scheduler::new(),
        };
        // SAFETY: the pointer is cleared before `r` goes away
        RECORDED.with(|c| c.set((&r as *const Recorded<'_>).cast::<Recorded<'static>>()));
        while r.failed.borrow().is_none() && r.pos.get() < lines.len() {
            let k = r.pos.get();
            let line = lines[k];
            if is_output(line) {
                r.fail(format!("line {k}: golden {line:?}, rust made no such line"));
                break;
            }
            r.pos.set(k + 1);
            let f: Vec<&str> = line.split(' ').collect();
            objects.act(&r, &f);
        }
        RECORDED.with(|c| c.set(std::ptr::null()));
        if let Some(what) = r.failed.take() {
            failed += 1;
            if first.is_none() {
                first = Some(format!("{}: {what}", lines[0]));
            }
            // the objects of a failed sequence are leaked: their state no longer matches the recording
            std::mem::forget(objects);
        }
    }
    (all.len(), failed, first)
}

#[test]
fn golden_ts_trace_replays() {
    let (checked, failed, first) = replay(include_str!("golden_ts.txt"));
    assert!(checked > 5000, "{checked}");
    assert_eq!(failed, 0, "{first:?}");
}

/// `EE_TS_GOLDEN=<file> cargo test -p ee_ts long_run -- --nocapture`
#[test]
fn golden_ts_trace_replays_long_run() {
    let Ok(path) = std::env::var("EE_TS_GOLDEN") else { return };
    let text = std::fs::read_to_string(path).unwrap();
    let (checked, failed, first) = replay(&text);
    println!("{checked} lines, {failed} sequences differ; first: {first:?}");
    assert_eq!(failed, 0, "{first:?}");
}
