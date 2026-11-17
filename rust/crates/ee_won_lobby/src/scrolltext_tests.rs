use super::*;

// ---- the golden replay: golden_scrolltext.txt, recorded by tests/won/won_credits_test.cpp over the original
// statements. Every op the Rust makes must be the next "op" line, answered by its "=" value; every call's result
// must be its "ret" line.

const GOLDEN: &str = include_str!("golden_scrolltext.txt");

struct Replay {
    lines: Vec<&'static str>,
    at: usize,
    failed: Option<String>,
}

thread_local! {
    static REPLAY: RefCell<Option<Replay>> = const { RefCell::new(None) };
}

fn fail(msg: String) {
    REPLAY.with(|r| {
        let mut r = r.borrow_mut();
        let r = r.as_mut().unwrap();
        if r.failed.is_none() {
            r.failed = Some(msg);
        }
    });
}

fn next_line() -> Option<&'static str> {
    REPLAY.with(|r| {
        let mut r = r.borrow_mut();
        let r = r.as_mut().unwrap();
        let l = r.lines.get(r.at).copied();
        r.at += 1;
        l
    })
}

#[allow(clippy::too_many_arguments)]
unsafe extern "C" fn replay_host(
    _comp: *mut c_void,
    op: EeWonScrollTextOp,
    a: isize,
    b: isize,
    x: i32,
    y: i32,
    w: i32,
    h: i32,
) -> isize {
    let made = format!("op {} {a} {b} {x} {y} {w} {h}", op as u32);
    let Some(line) = next_line() else {
        fail(format!("op past the end: {made}"));
        return 0;
    };
    let (head, answer) = line.split_once(" = ").unwrap_or((line, "0"));
    if head != made {
        fail(format!("expected '{line}', the Rust made '{made}'"));
        return 0;
    }
    answer.parse().unwrap()
}

fn call_of(n: u32) -> EeWonScrollTextCall {
    use EeWonScrollTextCall as C;
    [C::AddLine, C::Timer, C::Paint, C::Start, C::Stop, C::Reset, C::Field][n as usize - 1]
}

fn replay(golden: &'static str) -> Result<usize, String> {
    let lines: Vec<&str> = golden.lines().filter(|l| !l.is_empty()).collect();
    let mut starts: Vec<usize> =
        lines.iter().enumerate().filter(|(_, l)| l.starts_with("sseq ")).map(|(i, _)| i).collect();
    starts.push(lines.len());
    for w in starts.windows(2) {
        let st = ee_won_scrolltext_new(0x5C0 as *mut c_void, Some(replay_host));
        REPLAY.with(|r| *r.borrow_mut() = Some(Replay { lines: lines[w[0] + 1..w[1]].to_vec(), at: 0, failed: None }));
        while let Some(line) = next_line() {
            let t: Vec<&str> = line.split(' ').collect();
            match t[0] {
                "call" => {
                    let n: Vec<isize> = t[1..].iter().map(|x| x.parse().unwrap()).collect();
                    // SAFETY: the live object
                    let r = unsafe { ee_won_scrolltext_call(st, call_of(n[0] as u32), n[1], n[2]) };
                    let ret = next_line().unwrap_or("-");
                    if ret != format!("ret {r}") {
                        fail(format!("{line}: expected '{ret}', the Rust returned {r}"));
                    }
                }
                // SAFETY: the live object, freed once
                "free" => unsafe { ee_won_scrolltext_free(st) },
                _ => fail(format!("unexpected line '{line}'")),
            }
            let failed = REPLAY.with(|r| r.borrow().as_ref().unwrap().failed.clone());
            if let Some(f) = failed {
                return Err(format!("{}: {f}", lines[w[0]]));
            }
        }
    }
    Ok(starts.len() - 1)
}

#[test]
fn golden_replay() {
    let n = replay(GOLDEN).unwrap();
    assert!(n >= 20, "{n} sequences");
}

#[test]
fn golden_replay_fails_on_a_changed_op() {
    // the first SysHilight recorded as a SysShadow
    let hi = format!("op {} ", EeWonScrollTextOp::SysHilight as u32);
    let sh = format!("op {} ", EeWonScrollTextOp::SysShadow as u32);
    let changed = GOLDEN.replacen(&hi, &sh, 1);
    assert_ne!(changed, GOLDEN);
    assert!(replay(Box::leak(changed.into_boxed_str())).is_err());
}

// ---- unit tests

#[test]
fn dim_color_fades_towards_white() {
    assert_eq!(dim_color(0x123456, 0), 0xffffff);
    assert_eq!(dim_color(0x123456, -5), 0xffffff);
    assert_eq!(dim_color(0x123456, 100), 0x123456);
    assert_eq!(dim_color(0x123456, 250), 0x123456);
    // half way: 255 - (255 - c) / 2 per channel
    assert_eq!(dim_color(0x000000, 50), 0x808080);
    assert_eq!(dim_color(0xff0000, 50), 0xff8080);
}

thread_local! {
    static HEIGHT: Cell<isize> = const { Cell::new(100) };
    static OPS: RefCell<Vec<EeWonScrollTextOp>> = const { RefCell::new(Vec::new()) };
}

#[allow(clippy::too_many_arguments)]
unsafe extern "C" fn box_host(
    _c: *mut c_void,
    op: EeWonScrollTextOp,
    _a: isize,
    _b: isize,
    _x: i32,
    _y: i32,
    _w: i32,
    _h: i32,
) -> isize {
    OPS.with(|o| o.borrow_mut().push(op));
    match op {
        EeWonScrollTextOp::Height => HEIGHT.with(|h| h.get()),
        EeWonScrollTextOp::LineNew => 0x40,
        EeWonScrollTextOp::FontHeight => 10,
        _ => 0,
    }
}

#[test]
fn a_tick_scrolls_one_pixel_per_period_and_wraps_to_the_bottom() {
    let st = EeWonScrollText::new(std::ptr::null_mut(), Some(box_host));
    st.call(EeWonScrollTextCall::Start, 0, 0);
    assert_eq!(st.call(EeWonScrollTextCall::Field, 0, 0), 100);
    // 59 ms: no scroll; 1 ms more: one pixel
    assert_eq!(st.call(EeWonScrollTextCall::Timer, 59, 0), 1);
    assert_eq!(st.call(EeWonScrollTextCall::Field, 0, 0), 100);
    st.call(EeWonScrollTextCall::Timer, 1, 0);
    assert_eq!(st.call(EeWonScrollTextCall::Field, 0, 0), 99);
    assert_eq!(st.call(EeWonScrollTextCall::Field, 2, 0), 0);
    // two lines of 10: a paint measures 20; the text wraps once its top is above -20
    st.call(EeWonScrollTextCall::AddLine, 0x10, 0xff);
    st.call(EeWonScrollTextCall::AddLine, 0x10, u32::MAX as isize);
    st.call(EeWonScrollTextCall::Paint, 0x99, 0);
    assert_eq!(st.call(EeWonScrollTextCall::Field, 1, 0), 20);
    // the second line kept the first's colour
    assert_eq!(st.call(EeWonScrollTextCall::Field, 3, 0), 0xff);
    for _ in 0..119 {
        st.call(EeWonScrollTextCall::Timer, 60, 0);
    }
    assert_eq!(st.call(EeWonScrollTextCall::Field, 0, 0), -20);
    st.call(EeWonScrollTextCall::Timer, 60, 0);
    assert_eq!(st.call(EeWonScrollTextCall::Field, 0, 0), 100);
    st.free_lines();
}

#[test]
fn stop_only_stops_the_timer_and_free_frees_every_line() {
    let st = EeWonScrollText::new(std::ptr::null_mut(), Some(box_host));
    st.call(EeWonScrollTextCall::AddLine, 0x10, 1);
    st.call(EeWonScrollTextCall::AddLine, 0x10, 2);
    OPS.with(|o| o.borrow_mut().clear());
    st.call(EeWonScrollTextCall::Stop, 0, 0);
    assert_eq!(OPS.with(|o| o.borrow().clone()), [EeWonScrollTextOp::RequestTimer]);
    OPS.with(|o| o.borrow_mut().clear());
    st.free_lines();
    assert_eq!(OPS.with(|o| o.borrow().clone()), [EeWonScrollTextOp::LineFree, EeWonScrollTextOp::LineFree]);
    assert_eq!(st.call(EeWonScrollTextCall::Field, 4, 0), 0);
}
