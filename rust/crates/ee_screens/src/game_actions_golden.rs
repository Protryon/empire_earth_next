//! The golden replay of the game's actions (`golden_game_actions.txt`): the host ops the new side made in the
//! actions round's golden driver (`build/ga/drv`, the original units against `EEARust` over one fake world), replayed
//! against `game_actions.rs`: each call's record loaded, the call made, every op and record message checked in order
//! and answered from the log, the record compared after it; the destructor's ops after the last call.

use std::collections::HashMap;

use crate::game_actions::*;

pub(crate) const GOLDEN: &str = include_str!("golden_game_actions.txt");

/// The log's pointers by name (`u3`, `t0`, `m12`, `cal`, `rect`, `file`) as stand-in values, both ways.
#[derive(Default)]
struct Handles {
    by_name: HashMap<String, usize>,
    by_value: HashMap<usize, String>,
}

impl Handles {
    fn value(&mut self, token: &str) -> i64 {
        if let Ok(v) = token.parse::<i64>() {
            return v;
        }
        let next = 0x7000_0000 + self.by_name.len() * 0x100;
        let v = *self.by_name.entry(token.to_string()).or_insert(next);
        self.by_value.insert(v, token.to_string());
        v as i64
    }
    fn name(&self, v: i64) -> String {
        match self.by_value.get(&(v as usize)) {
            Some(n) if v != 0 => n.clone(),
            _ => v.to_string(),
        }
    }
}

fn hex(t: &str) -> f32 {
    f32::from_bits(u32::from_str_radix(t, 16).unwrap())
}

fn ints(h: &Handles, i: &[i64]) -> String {
    let mut n = i.len();
    while n > 0 && i[n - 1] == 0 {
        n -= 1;
    }
    i[..n].iter().map(|v| h.name(*v)).collect::<Vec<_>>().join(" ")
}

fn floats(f: &[f32]) -> String {
    let mut n = f.len();
    while n > 0 && f[n - 1].to_bits() == 0 {
        n -= 1;
    }
    f[..n].iter().map(|v| format!("{:08x}", v.to_bits())).collect::<Vec<_>>().join(" ")
}

/// The host: the expected lines of one call, answered in order.
struct Replay<'a> {
    lines: Vec<&'a str>,
    next: usize,
    handles: &'a mut Handles,
    case: String,
}

impl Replay<'_> {
    fn take(&mut self) -> &str {
        assert!(self.next < self.lines.len(), "{}: an op more than the log's", self.case);
        let l = self.lines[self.next];
        self.next += 1;
        l
    }
}

impl GaHost for Replay<'_> {
    fn op(&mut self, op: u32, _r: *mut EeScreensGaFields, p: usize, i: &mut [i64; 8], f: &mut [f32; 8]) -> i64 {
        let got = format!("o {op} {} {}|{}", self.handles.name(p as i64), ints(self.handles, i), floats(f));
        let case = self.case.clone();
        let line = self.take().to_string();
        let parts: Vec<&str> = line.split('|').collect();
        let want = format!("{}|{}", parts[0], parts[1]);
        assert_eq!(got, want, "{case}");
        let ret = self.handles.value(parts[2]);
        *i = [0; 8];
        for (k, t) in parts[3].split_whitespace().enumerate() {
            i[k] = self.handles.value(t);
        }
        *f = [0.0; 8];
        for (k, t) in parts[4].split_whitespace().enumerate() {
            f[k] = hex(t);
        }
        ret
    }
    fn record(&mut self, kind: u32, values: &[EeScreensGaValue]) -> usize {
        let mut got = format!("R {kind} {}", values.len());
        for v in values {
            got += &format!(" {}:{}:{:08x}:{:08x}", v.kind, v.i, v.f[0].to_bits(), v.f[1].to_bits());
        }
        let case = self.case.clone();
        let line = self.take().to_string();
        let (want, ret) = line.split_once('|').unwrap();
        assert_eq!(got, want, "{case}");
        self.handles.value(ret) as usize
    }
}

fn load(h: &mut Handles, line: &str) -> EeScreensGaFields {
    let t: Vec<&str> = line.split_whitespace().skip(1).collect();
    assert_eq!(t.len(), 37, "{line}");
    let n = |k: usize| t[k].parse::<i64>().unwrap();
    let b = |k: usize| n(k) != 0;
    let mut r = EeScreensGaFields::zeroed(n(0) as u32);
    r.mMessage = h.value(t[1]) as usize as *mut _;
    r.mCalamityClass = h.value(t[2]) as usize as *mut _;
    r.mFile = h.value(t[3]) as usize as *mut _;
    r.mTechnology = n(4) as i32;
    r.mTargetID = n(5) as i32;
    r.mCasterID = n(6) as i32;
    r.mX = n(7) as i32;
    r.mY = n(8) as i32;
    r.mValid = n(9) as i32;
    r.mLocation = [hex(t[10]), hex(t[11]), hex(t[12])];
    r.mUnitID = n(13) as i32;
    r.mMode = n(14) as i32;
    r.mModifiers = n(15) as u32;
    r.mElapsed = n(16) as u32;
    r.mStringID = n(17) as i32;
    r.mImprovement = n(18) as i32;
    r.mUnit = n(19) as u32;
    r.mJoinMode = n(20) as i32;
    r.mIsQueued = n(21) as u8;
    r.mScrollLeft = b(22);
    r.mScrollRight = b(23);
    r.mScrollUp = b(24);
    r.mScrollDown = b(25);
    r.mAccelerate = b(26);
    r.mPlayer = n(27) as u8;
    r.mB0D = b(28);
    r.mIsExiting = b(29);
    r.mNoPop = b(30);
    r.mRaise = b(31);
    r.mIsInGame = b(32);
    r.mFlag = b(33);
    r.mFromLocalPlayer = b(34);
    r.mIsLoad = b(35);
    r.mWriteToFile = b(36);
    r
}

fn show(h: &Handles, r: &EeScreensGaFields) -> String {
    let p = |v: *mut core::ffi::c_void| h.name(v as usize as i64);
    let f = |v: f32| format!("{:08x}", v.to_bits());
    format!(
        "{} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {}",
        r.mKind,
        p(r.mMessage),
        p(r.mCalamityClass),
        if r.mFile.is_null() { "0".to_string() } else { "file".to_string() },
        r.mTechnology,
        r.mTargetID,
        r.mCasterID,
        r.mX,
        r.mY,
        r.mValid,
        f(r.mLocation[0]),
        f(r.mLocation[1]),
        f(r.mLocation[2]),
        r.mUnitID,
        r.mMode,
        r.mModifiers,
        r.mElapsed,
        r.mStringID,
        r.mImprovement,
        r.mUnit,
        r.mJoinMode,
        r.mIsQueued,
        r.mScrollLeft as u8,
        r.mScrollRight as u8,
        r.mScrollUp as u8,
        r.mScrollDown as u8,
        r.mAccelerate as u8,
        r.mPlayer,
        r.mB0D as u8,
        r.mIsExiting as u8,
        r.mNoPop as u8,
        r.mRaise as u8,
        r.mIsInGame as u8,
        r.mFlag as u8,
        r.mFromLocalPlayer as u8,
        r.mIsLoad as u8,
        r.mWriteToFile as u8
    )
}

/// One call of the log on the record: the members it sets, then the call over the host.
fn call(r: *mut EeScreensGaFields, host: &mut Replay<'_>, lookup: impl Fn(&str) -> usize, line: &str) {
    let a: Vec<&str> = line.split_whitespace().skip(1).collect();
    let n = |k: usize| a[k].parse::<i64>().unwrap();
    let x = |k: usize| hex(a[k]);
    // SAFETY: r is the test's record, no Ga over it yet
    let rec = unsafe { &mut *r };
    let kind = rec.mKind;
    match (kind, a[0]) {
        (EE_SCREENS_GA_MINI_MAP, "V") => {
            rec.mMode = n(1) as i32;
            rec.mModifiers = n(4) as u32;
        }
        (EE_SCREENS_GA_BUILD, "S") => {
            rec.mTechnology = n(1) as i32;
            rec.mIsQueued = n(2) as u8;
            return;
        }
        (EE_SCREENS_GA_CALAMITY, "S") => {
            rec.mTechnology = n(1) as i32;
            rec.mCalamityClass = lookup("cal") as *mut _;
            return;
        }
        (EE_SCREENS_GA_CALAMITY, "I") => {
            rec.mCasterID = n(1) as i32;
            return;
        }
        (EE_SCREENS_GA_QUICK | EE_SCREENS_GA_EDITOR_QUICK, "S") => {
            rec.mIsLoad = n(1) != 0;
            rec.mWriteToFile = n(2) != 0;
            return;
        }
        _ => {}
    }
    let mut g = unsafe { Ga::new(r, host) };
    match (kind, a[0]) {
        (_, "V") => g.run(),
        (EE_SCREENS_GA_MOUSE_CLICK, "X") => {
            g.click_execute(x(1), x(2), n(3) != 0, n(4) as u32, n(5) != 0, x(6), n(7) != 0);
        }
        (EE_SCREENS_GA_MOUSE_CLICK, "M") => g.select_at_mouse(n(1) as u32),
        (EE_SCREENS_GA_LASSO, "L") => g.lasso_select(lookup("rect"), n(5) as i32, n(6) as u32, n(7) as i32),
        (EE_SCREENS_GA_BUILD, "B") => g.build(x(1), x(2)),
        (EE_SCREENS_GA_BUILD, "G") => g.build_gate(x(1), x(2), n(3) as i32),
        (EE_SCREENS_GA_BUILD, "W") => g.build_wall(x(1), x(2), x(3), x(4)),
        (EE_SCREENS_GA_CALAMITY, "C") => g.cast((x(1), x(2))),
        (EE_SCREENS_GA_CALAMITY, "K") => {
            // (the answer is mValid, compared with the record)
            g.check((x(1), x(2)));
        }
        (EE_SCREENS_GA_CENTER_CAMERA, "L") => g.set_location(n(1) as i32, n(2) as i32),
        (EE_SCREENS_GA_CENTER_CAMERA, "U") => {
            let k = n(1);
            g.set_unit(if (0..8).contains(&k) { lookup(&format!("u{k}")) } else { 0 });
        }
        (EE_SCREENS_GA_SCROLL, "U") => {
            // (the answer is the edge flags, compared with the record)
            g.scroll_update((x(5), x(6)));
        }
        _ => panic!("an unknown call {line}"),
    }
}

/// Every case of the log replayed; the number of calls.
pub(crate) fn replay(golden: &str) -> usize {
    let lines: Vec<&str> = golden.lines().collect();
    let mut k = 0;
    let mut calls = 0;
    scroll_velocity_reset();
    while k < lines.len() {
        let head = lines[k];
        assert!(head.starts_with("case "), "{head}");
        k += 1;
        let mut handles = Handles::default();
        // the constructor's ops (the improvement's begin: pinned by game_actions_tests)
        while !lines[k].starts_with("w ") && lines[k] != "end" {
            k += 1;
        }
        let mut last: Option<String> = None;
        while lines[k].starts_with("w ") {
            let mut r = load(&mut handles, lines[k]);
            let call_line = lines[k + 1];
            assert!(call_line.starts_with("> "), "{call_line}");
            k += 2;
            let start = k;
            while !lines[k].starts_with("r ") {
                k += 1;
            }
            let ops: Vec<&str> = lines[start..k].to_vec();
            let after = lines[k];
            k += 1;
            let case = format!("{head} {call_line}");
            let units: Vec<(String, usize)> = (0..8)
                .map(|u| format!("u{u}"))
                .chain(["rect".to_string(), "cal".to_string()])
                .map(|n| {
                    let v = handles.value(&n) as usize;
                    (n, v)
                })
                .collect();
            let lookup = |n: &str| units.iter().find(|(m, _)| m == n).unwrap().1;
            let mut host = Replay { lines: ops, next: 0, handles: &mut handles, case: case.clone() };
            call(&mut r, &mut host, lookup, call_line);
            assert_eq!(host.next, host.lines.len(), "{case}: ops the log has and the call did not make");
            assert_eq!(show(&handles, &r), after[2..], "{case}: the record after");
            last = Some(after.to_string());
            calls += 1;
        }
        // the destructor's ops
        let start = k;
        while lines[k] != "end" {
            k += 1;
        }
        if let Some(after) = last {
            let mut r = load(&mut handles, &after);
            let mut host =
                Replay { lines: lines[start..k].to_vec(), next: 0, handles: &mut handles, case: head.to_string() };
            unsafe { Ga::new(&mut r, &mut host) }.free();
            assert_eq!(host.next, host.lines.len(), "{head}: the destructor's ops");
        }
        k += 1;
    }
    calls
}
