use super::*;
use crate::hud_ctl::replay::{cases, hex_f, Replay};

fn digest(r: &EeScreensPingFields) -> String {
    format!("n {}", r.mNumBars)
}

#[test]
fn replays_the_golden() {
    let all = cases(0);
    assert!(all.len() > 50, "{} ping cases", all.len());
    let mut calls = 0;
    for (n, case) in all.iter().enumerate() {
        let mut fields = EeScreensPingFields::default();
        let rec = unsafe { Rec::new(&mut fields) };
        let mut h = Replay::new(&case.init, format!("case {n} init"));
        init(&rec, &mut h, case.head[0], case.head[1]);
        h.done();
        assert_eq!(rec.get(digest), case.init.record, "case {n} init");
        for (i, call) in case.calls.iter().enumerate() {
            let ctx = format!("case {n} call {i} {:?}", call.what);
            let mut h = Replay::new(call, ctx.clone());
            let result = match call.what[0].as_str() {
                "D" => draw(&rec, &mut h),
                _ => {
                    set_ping(&rec, &mut h, hex_f(&call.what[1]));
                    0
                }
            };
            h.done();
            assert_eq!(result, call.result, "{ctx}");
            assert_eq!(rec.get(digest), call.record, "{ctx}");
            calls += 1;
        }
    }
    assert!(calls > 300);
}

/// The bars of a meter at (100, 50): 12 apart, 10 wide, 10 high.
#[test]
fn the_bars_step_twelve_pixels() {
    struct Bars(Vec<[f32; 4]>);
    impl HudHost for Bars {
        fn op(&mut self, op: u32, a: i64, _b: i64, f: &mut [f32]) -> i64 {
            assert_eq!((op, a as usize), (EE_SCREENS_PING_BAR, self.0.len()));
            self.0.push([f[0], f[1], f[2], f[3]]);
            0
        }
    }
    let mut fields = EeScreensPingFields { mNumBars: 9 };
    let rec = unsafe { Rec::new(&mut fields) };
    let mut h = Bars(Vec::new());
    init(&rec, &mut h, 100.0, 50.0);
    assert_eq!(h.0[4], [148.0, 50.0, 158.0, 60.0]);
    assert_eq!(fields.mNumBars, 0);
}
