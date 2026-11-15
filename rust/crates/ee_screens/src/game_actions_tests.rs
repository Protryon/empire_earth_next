//! Tests of the game's actions (`game_actions.rs`): a scripted host answering each op, the paths pinned one by one.

use crate::game_actions::*;

/// The scroll's velocity is one static: its tests one at a time.
static SCROLL_LOCK: std::sync::Mutex<()> = std::sync::Mutex::new(());

/// A host answering from a closure and logging every op and record.
struct Script<F: FnMut(u32, usize, &mut [i64; 8], &mut [f32; 8]) -> i64> {
    answer: F,
    log: Vec<String>,
    next_message: usize,
}

impl<F: FnMut(u32, usize, &mut [i64; 8], &mut [f32; 8]) -> i64> GaHost for Script<F> {
    fn op(&mut self, op: u32, _r: *mut EeScreensGaFields, p: usize, i: &mut [i64; 8], f: &mut [f32; 8]) -> i64 {
        let args = format!("{op} {p:#x} {:?}", &i[..4]);
        let ret = (self.answer)(op, p, i, f);
        self.log.push(format!("{args} -> {ret}"));
        ret
    }
    fn record(&mut self, kind: u32, values: &[EeScreensGaValue]) -> usize {
        self.next_message += 0x10;
        let v: Vec<String> = values
            .iter()
            .map(|v| match v.kind {
                EE_SCREENS_GA_VALUE_INT => format!("{}", v.i),
                EE_SCREENS_GA_VALUE_FLOAT => format!("{}f", v.f[0]),
                _ => format!("({},{})", v.f[0], v.f[1]),
            })
            .collect();
        self.log.push(format!("record {kind:#x} [{}] = {:#x}", v.join(" "), self.next_message));
        self.next_message
    }
}

fn script<F: FnMut(u32, usize, &mut [i64; 8], &mut [f32; 8]) -> i64>(answer: F) -> Script<F> {
    Script { answer, log: Vec::new(), next_message: 0x1000 }
}

#[test]
fn the_constructors_defaults() {
    let mut r = EeScreensGaFields::zeroed(EE_SCREENS_GA_CALAMITY);
    let mut h = script(|_, _, _, _| 0);
    unsafe { Ga::new(&mut r, &mut h) }.init();
    assert_eq!((r.mTechnology, r.mValid), (EE_SCREENS_GA_TECH_NONE, 1));
    let mut r = EeScreensGaFields::zeroed(EE_SCREENS_GA_SCROLL);
    unsafe { Ga::new(&mut r, &mut h) }.init();
    assert!(r.mAccelerate);
    assert!(h.log.is_empty());
}

#[test]
fn a_plain_click_on_nothing_unselects_all_and_a_modified_one_does_nothing() {
    let mut r = EeScreensGaFields::zeroed(EE_SCREENS_GA_MOUSE_CLICK);
    let mut h = script(|_, _, _, _| 0);
    unsafe { Ga::new(&mut r, &mut h) }.select_at_mouse(1);
    assert!(r.mMessage.is_null());
    unsafe { Ga::new(&mut r, &mut h) }.select_at_mouse(0);
    assert_eq!(r.mMessage as usize, 0x1010);
    assert!(h.log.iter().any(|l| l.starts_with("record 0x4b []")));
    assert!(h.log.last().unwrap().starts_with(&format!("{EE_SCREENS_GA_OP_POST} ")));
}

#[test]
fn the_lasso_picks_its_message_by_type_and_flags() {
    let mut r = EeScreensGaFields::zeroed(EE_SCREENS_GA_LASSO);
    let mut seen = Vec::new();
    let mut h = script(|op, _, i, _| match op {
        EE_SCREENS_GA_OP_CLASS_COUNT => 0x300,
        EE_SCREENS_GA_OP_SELECT_UNITS => {
            seen.push((i[0], i[1]));
            0x77
        }
        _ => 0,
    });
    for (flags, ty) in [(0, 5u32), (2, 5), (1, 5), (4, 5), (1, 0xffff_ffff), (0, 0xdf)] {
        unsafe { Ga::new(&mut r, &mut h) }.lasso_select(0x55, flags, ty, 9);
    }
    drop(h);
    assert_eq!(seen, vec![(0x4e, 5), (0x50, 5), (0x4f, 5), (0x4e, 5), (0x4f, 0)]);
}

#[test]
fn the_build_centres_its_footprint() {
    let mut r = EeScreensGaFields::zeroed(EE_SCREENS_GA_BUILD);
    r.mTechnology = 12;
    r.mIsQueued = 1;
    for (size, expect) in [(-1, "(10,20)"), (1, "(10,20)"), (2, "(9,19)"), (3, "(9,19)"), (4, "(8,18)")] {
        let mut h = script(|op, _, i, _| match (op, i[0]) {
            (EE_SCREENS_GA_OP_TECH, EE_SCREENS_GA_K_CAN_PRODUCE) => 1,
            (EE_SCREENS_GA_OP_TECH, EE_SCREENS_GA_K_TILE_SIZE) => size,
            _ => 0,
        });
        unsafe { Ga::new(&mut r, &mut h) }.build(10.0, 20.0);
        let record = h.log.iter().find(|l| l.starts_with("record")).unwrap();
        assert_eq!(record, &format!("record 0x56 [{expect} 12 1] = 0x1010"));
    }
}

#[test]
fn the_scroll_accelerates_toward_the_edge_and_stops_off_it() {
    let _scroll = SCROLL_LOCK.lock().unwrap();
    scroll_velocity_reset();
    let mut r = EeScreensGaFields::zeroed(EE_SCREENS_GA_SCROLL);
    r.mAccelerate = true;
    let mut running = false;
    let mut h = script(|op, _, i, f| match (op, i[0]) {
        (EE_SCREENS_GA_OP_SCROLL, EE_SCREENS_GA_S_EDGES) => {
            f[..4].copy_from_slice(&[2.0, 2.0, 100.0, 100.0]);
            0
        }
        (EE_SCREENS_GA_OP_SCROLL, EE_SCREENS_GA_S_POLL_RUNNING) => running as i64,
        (EE_SCREENS_GA_OP_SCROLL, EE_SCREENS_GA_S_POLL_START) => {
            running = true;
            0
        }
        (EE_SCREENS_GA_OP_SCROLL, EE_SCREENS_GA_S_POLL_STOP) => {
            running = false;
            0
        }
        (EE_SCREENS_GA_OP_SCROLL, EE_SCREENS_GA_S_POLL) => 3,
        (EE_SCREENS_GA_OP_SCROLL, EE_SCREENS_GA_S_SPEED) => {
            f[0] = 1.5;
            0
        }
        (EE_SCREENS_GA_OP_CAMERA, EE_SCREENS_GA_CAM_ZOOM) => {
            f[0] = 0.5;
            0
        }
        (EE_SCREENS_GA_OP_CAMERA, EE_SCREENS_GA_CAM_SCROLL) => {
            assert!(f[0] < 0.0 && f[1] == 0.0);
            0
        }
        _ => 0,
    });
    assert!(unsafe { Ga::new(&mut r, &mut h) }.scroll_update((1.0, 50.0)));
    assert!(r.mScrollLeft && !r.mScrollRight && !r.mScrollUp && !r.mScrollDown);
    assert_eq!(r.mElapsed, 1);
    unsafe { Ga::new(&mut r, &mut h) }.run();
    assert!(unsafe { Ga::new(&mut r, &mut h) }.scroll_update((1.0, 50.0)));
    assert_eq!(r.mElapsed, 3);
    assert!(!unsafe { Ga::new(&mut r, &mut h) }.scroll_update((50.0, 50.0)));
    let stops = h
        .log
        .iter()
        .filter(|l| l.starts_with(&format!("{EE_SCREENS_GA_OP_SCROLL} 0x0 [{EE_SCREENS_GA_S_POLL_STOP},")))
        .count();
    assert_eq!(stops, 1);
    scroll_velocity_reset();
}

#[test]
fn replays_the_golden() {
    let _scroll = SCROLL_LOCK.lock().unwrap();
    let calls = crate::game_actions_golden::replay(crate::game_actions_golden::GOLDEN);
    assert!(calls > 2000, "{calls} calls");
}
