//! Replays the original Time Manager's trace (`tests/time/tm_golden.cpp`, format in its header) on the Rust objects:
//! every input line is acted on again and every output line regenerated; each sequence must be equal line for line.
//! `golden_time.txt` holds the first 60 sequences; `EE_TIME_GOLDEN=<file>` replays a longer run.

use super::*;
use std::cell::RefCell;

thread_local! {
    static CALLBACKS: RefCell<Vec<String>> = const { RefCell::new(Vec::new()) };
}
unsafe extern "C" fn callback_a(param: *mut c_void) {
    CALLBACKS.with(|c| c.borrow_mut().push(format!("cb 1 {}", param as usize)));
}
unsafe extern "C" fn callback_b(param: *mut c_void) {
    CALLBACKS.with(|c| c.borrow_mut().push(format!("cb 2 {}", param as usize)));
}

#[derive(Default)]
struct Platform {
    frequency: Cell<i64>,
    ticks: Cell<i64>,
    ms: Cell<u32>,
    local: Cell<plat::PlatDateTime>,
    next_id: Cell<u32>,
    stop_ok: Cell<bool>,
    out: RefCell<Vec<String>>,
}

impl Clock for Platform {
    fn counter_frequency(&self) -> i64 {
        self.frequency.get()
    }
    fn counter_ticks(&self) -> i64 {
        self.ticks.get()
    }
    fn milliseconds(&self) -> u32 {
        self.ms.get()
    }
}

impl TimerHost for Platform {
    fn milliseconds(&self) -> u32 {
        self.ms.get()
    }
    fn start_callback(&self, interval: u32, periodic: bool, _user: *mut c_void) -> u32 {
        self.out.borrow_mut().push(format!("plat startc {interval} {}", periodic as u8));
        self.next_id.get()
    }
    fn start_event(&self, interval: u32, periodic: bool, event: *mut c_void) -> u32 {
        self.out.borrow_mut().push(format!("plat starte {interval} {} {}", periodic as u8, event as usize));
        self.next_id.get()
    }
    fn stop(&self, id: u32) -> bool {
        self.out.borrow_mut().push(format!("plat stop {id}"));
        self.stop_ok.get()
    }
}

struct Replay {
    p: Platform,
    state: TimeState,
    pinned: i64,
    polls: [EeTimePoll; 3],
    timer: Option<EeTimeTimer>,
}

impl Replay {
    fn new() -> Self {
        Replay {
            p: Platform::default(),
            state: TimeState::new(),
            pinned: -1,
            polls: [EeTimePoll::default(); 3],
            timer: None,
        }
    }
    fn out(&self, s: String) {
        self.p.out.borrow_mut().push(s);
    }
    fn dump_poll(&self, i: usize) {
        let q = &self.polls[i];
        self.out(format!(
            "P {i} {} {} {} {} {} {} {}",
            q.start_time,
            q.pause_remaining,
            q.time_unit,
            q.time_delay,
            q.use_counter as u8,
            q.running as u8,
            self.state.poll_frequency()
        ));
    }
    fn dump_timer(&self) {
        let f = self.timer.as_ref().unwrap().fields();
        let callback = if f.0 == callback_a as *const () as usize {
            1
        } else if f.0 == callback_b as *const () as usize {
            2
        } else {
            0
        };
        self.out(format!(
            "T {callback} {} {} {} {} {} {} {} {} {}",
            f.1, f.2, f.3, f.4, f.5, f.6, f.7, f.8 as u8, f.9 as u8
        ));
    }

    /// Acts on one input line, appending the lines the original printed after it.
    fn act(&mut self, f: &[&str]) {
        let n = |k: usize| -> u64 { f[k].parse().unwrap() };
        let i = || -> usize { f[1].parse().unwrap() };
        let p = &self.p;
        match f[0] {
            "clock" => {
                p.frequency.set(f[1].parse().unwrap());
                p.ticks.set(f[2].parse().unwrap());
                p.ms.set(n(3) as u32);
            }
            "local" => {
                let v: Vec<u16> = (1..9).map(|k| n(k) as u16).collect();
                p.local.set(plat::PlatDateTime {
                    year: v[0],
                    month: v[1],
                    day_of_week: v[2],
                    day: v[3],
                    hour: v[4],
                    minute: v[5],
                    second: v[6],
                    milliseconds: v[7],
                });
            }
            "tid" => {
                p.next_id.set(n(1) as u32);
                p.stop_ok.set(n(2) != 0);
            }
            "init" => {
                self.out("plat begin 1".into());
                self.state.initialize(&self.p);
                let (per_ms, per_us, has) = self.state.base();
                self.out(format!("= {per_ms} {:08x} {}", per_us.to_bits(), has as u8));
            }
            "time" => self.out(format!("= {}", self.state.time(&self.p, self.pinned))),
            "micro" => self.out(format!("= {}", self.state.micro_time(&self.p))),
            "pin" => self.pinned = f[1].parse().unwrap(),
            "tod" => {
                let t = EeTimeOfDay::from_local(&p.local.get());
                self.out(format!("= {} {} {} {} {} {}", t.mMonth, t.mDay, t.mYear, t.mHour, t.mMinute, t.mSecond));
            }
            "new" => {
                let k = i();
                self.polls[k] = EeTimePoll::new(&self.state, &self.p, n(2) as u32, n(3) as u32, n(4) != 0);
                self.dump_poll(k);
            }
            "start" | "stop" | "pause" | "resume" | "poll" | "left" | "paused" | "running" | "setd" => {
                let k = i();
                let (state, clock) = (&self.state, &self.p);
                let q = &mut self.polls[k];
                let result = match f[0] {
                    "start" => {
                        q.start(clock);
                        None
                    }
                    "stop" => {
                        q.running = false;
                        None
                    }
                    "pause" => {
                        q.pause(state, clock);
                        None
                    }
                    "resume" => {
                        q.resume(state, clock);
                        None
                    }
                    "poll" => Some(q.poll(state, clock) as u64),
                    "left" => Some(q.time_left(state, clock) as u64),
                    "paused" => Some(q.is_paused() as u64),
                    "running" => Some(q.running as u64),
                    _ => {
                        q.set_time_delay(n(2) as u32);
                        None
                    }
                };
                if let Some(r) = result {
                    self.out(format!("= {r}"));
                }
                self.dump_poll(k);
            }
            "tnew" | "end" => {
                if let Some(old) = self.timer.take() {
                    old.stop(&self.p);
                }
                if f[0] == "end" {
                    return;
                }
                self.timer = Some(EeTimeTimer::new(n(1) as u32, n(2) as u32, n(3) != 0));
                self.dump_timer();
            }
            _ => {
                let t = self.timer.as_ref().unwrap();
                let result = match f[0] {
                    "tcb" => {
                        let cb: EeTimeCallback = match n(1) {
                            1 => Some(callback_a),
                            2 => Some(callback_b),
                            _ => None,
                        };
                        format!("= {}", t.set_callback(cb, n(2) as usize as *mut c_void))
                    }
                    "tev" => format!("= {}", t.set_event(n(1) as usize as *mut c_void)),
                    "tint" => format!("= {}", t.set_interval(n(1) as u32, n(2) as u32)),
                    "trep" => format!("= {}", t.set_repeating(n(1) != 0)),
                    "ttype" => format!("= {}", t.set_timer_type(n(1) as u32)),
                    "tstart" => format!("= {}", t.start(&self.p)),
                    "tstop" => format!("= {}", t.stop(&self.p)),
                    "tstop2" => {
                        let (r, out) = t.stop_elapsed(&self.p);
                        let (e, m) = out.unwrap_or((7, 9));
                        format!("= {r} {e} {m}")
                    }
                    "tfire" => {
                        t.fire(&self.p);
                        let calls = CALLBACKS.with(|c| std::mem::take(&mut *c.borrow_mut()));
                        self.p.out.borrow_mut().extend(calls);
                        String::new()
                    }
                    other => panic!("unknown input line {other}"),
                };
                if !result.is_empty() {
                    self.out(result);
                }
                self.dump_timer();
            }
        }
    }
}

const OUTPUT: [&str; 5] = ["=", "P", "T", "plat", "cb"];

/// -> (lines checked, sequences with a differing line, the first difference)
fn replay(text: &str) -> (usize, usize, Option<String>) {
    let mut checked = 0;
    let mut failed = 0;
    let mut first = None;
    let mut sequences: Vec<Vec<&str>> = Vec::new();
    for line in text.lines() {
        if line.starts_with("seq ") {
            sequences.push(Vec::new());
        }
        sequences.last_mut().expect("starts with seq").push(line);
    }
    for seq in sequences {
        let mut r = Replay::new();
        let mut got: Vec<String> = Vec::new();
        for line in &seq {
            let f: Vec<&str> = line.split(' ').collect();
            if OUTPUT.contains(&f[0]) {
                continue;
            }
            got.push(line.to_string());
            if f[0] != "seq" {
                r.act(&f);
            }
            got.append(&mut r.p.out.borrow_mut());
        }
        checked += seq.len();
        if got.len() != seq.len() || got.iter().zip(&seq).any(|(a, b)| a != b) {
            failed += 1;
            if first.is_none() {
                let k = got.iter().zip(&seq).position(|(a, b)| a != b).unwrap_or(got.len().min(seq.len()));
                first = Some(format!("{}: golden {:?}, rust {:?}", seq[0], seq.get(k), got.get(k)));
            }
        }
    }
    (checked, failed, first)
}

#[test]
fn golden_time_trace_replays() {
    let (checked, failed, first) = replay(include_str!("golden_time.txt"));
    assert!(checked > 5000, "{checked}");
    assert_eq!(failed, 0, "{first:?}");
}

/// `EE_TIME_GOLDEN=<file> cargo test -p ee_time long_run -- --nocapture`
#[test]
fn golden_time_trace_replays_long_run() {
    let Ok(path) = std::env::var("EE_TIME_GOLDEN") else { return };
    let text = std::fs::read_to_string(path).unwrap();
    let (checked, failed, first) = replay(&text);
    println!("{checked} lines, {failed} sequences differ; first: {first:?}");
    assert_eq!(failed, 0, "{first:?}");
}
