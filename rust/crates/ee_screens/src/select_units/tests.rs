use super::*;
use crate::hud_ctl::replay::{cases, f, flag, hex_f, rect, Replay};

fn digest(r: &EeScreensSelectUnitsFields) -> String {
    format!(
        "{} {} {} {} {} {} {}",
        rect(&r.mRA0),
        f(r.mDragStart.x),
        f(r.mDragStart.y),
        f(r.mDragEnd.x),
        f(r.mDragEnd.y),
        flag(r.mDragging),
        flag(r.mD1)
    )
}

/// The record from a digest (after a call that re-entered the control: its nested `MouseMove` is not replayed).
fn load(s: &str) -> EeScreensSelectUnitsFields {
    let v: Vec<&str> = s.split(' ').collect();
    EeScreensSelectUnitsFields {
        mRA0: EeScreensRect::new(hex_f(v[0]), hex_f(v[1]), hex_f(v[2]), hex_f(v[3])),
        mDragStart: EeScreensPoint { x: hex_f(v[4]), y: hex_f(v[5]) },
        mDragEnd: EeScreensPoint { x: hex_f(v[6]), y: hex_f(v[7]) },
        mDragging: v[8] == "1",
        mD1: v[9] == "1",
    }
}

#[test]
fn replays_the_golden() {
    let all = cases(2);
    assert!(all.len() > 50, "{} selection cases", all.len());
    let mut calls = 0;
    for (n, case) in all.iter().enumerate() {
        let mut fields = EeScreensSelectUnitsFields::default();
        assert_eq!(digest(&fields), case.init.record, "case {n} init");
        for (i, call) in case.calls.iter().enumerate() {
            let ctx = format!("case {n} call {i} {:?}", call.what);
            let rec = unsafe { Rec::new(&mut fields) };
            let mut h = Replay::new(call, ctx.clone());
            let at = || (hex_f(&call.what[1]), hex_f(&call.what[2]));
            let result = match call.what[0].as_str() {
                "S" => {
                    let (x, y) = at();
                    start(&rec, &mut h, x, y);
                    0
                }
                "M" => {
                    let (x, y) = at();
                    mouse_move(&rec, &mut h, x, y)
                }
                "U" => mouse_l_up(&rec, &mut h),
                "B" => double_click(&rec, &mut h),
                "C" => copy_data(&rec, &mut h),
                _ => draw(&rec, &mut h),
            };
            h.done();
            assert_eq!(result, call.result, "{ctx}");
            if call.what[0] == "S" {
                fields = load(&call.record);
            }
            assert_eq!(digest(&fields), call.record, "{ctx}");
            calls += 1;
        }
    }
    assert!(calls > 300);
}
