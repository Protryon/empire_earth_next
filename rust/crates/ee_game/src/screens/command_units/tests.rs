use super::*;
use crate::screens::hud_ctl::replay::{cases, editor_cases, f, flag, hex_f, Case, Replay};

fn digest(r: &EeScreensCommandUnitsFields) -> String {
    let base = digest_base(r);
    if r.mEditor {
        format!("{base} 1")
    } else {
        base
    }
}

fn digest_base(r: &EeScreensCommandUnitsFields) -> String {
    format!(
        "{} {} {} {} {} {} {} {} {} {}",
        f(r.mDownLocation.x),
        f(r.mDownLocation.y),
        f(r.mPosition.x),
        f(r.mPosition.y),
        f(r.mPosition.z),
        f(r.mFacing),
        f(r.mDrawnFacing),
        flag(r.mCanFace),
        flag(r.mHasFacing),
        flag(r.mDrawnHasFacing)
    )
}

/// The record from a digest (after a call that re-entered the control: its nested `MouseMove` is not replayed).
fn load(s: &str) -> EeScreensCommandUnitsFields {
    let v: Vec<&str> = s.split(' ').collect();
    EeScreensCommandUnitsFields {
        mDownLocation: EeScreensPoint { x: hex_f(v[0]), y: hex_f(v[1]) },
        mPosition: EeScreensPoint3 { x: hex_f(v[2]), y: hex_f(v[3]), z: hex_f(v[4]) },
        mFacing: hex_f(v[5]),
        mDrawnFacing: hex_f(v[6]),
        mCanFace: v[7] == "1",
        mHasFacing: v[8] == "1",
        mDrawnHasFacing: v[9] == "1",
        // (the editor's golden: its flag after the original's ten)
        mEditor: v.get(10) == Some(&"1"),
    }
}

#[test]
fn replays_the_golden() {
    let all = cases(3);
    assert!(all.len() > 50, "{} command cases", all.len());
    assert!(replay(&all, false) > 300);
}

#[test]
fn replays_the_editors_golden() {
    let all = editor_cases(4);
    assert!(all.len() > 50, "{} editor command cases", all.len());
    assert!(replay(&all, true) > 300);
}

/// The cases replayed (`editor`: the scenario editor's control) -> the calls.
fn replay(all: &[Case], editor: bool) -> usize {
    let mut calls = 0;
    for (n, case) in all.iter().enumerate() {
        let mut fields = EeScreensCommandUnitsFields { mEditor: editor, ..Default::default() };
        init(&unsafe { Rec::new(&mut fields) });
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
                "R" => mouse_r_up(&rec, &mut h),
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
    calls
}

/// The categories that take a facing: a lone citizen does, a lone building does not.
#[test]
fn a_citizen_takes_a_facing_a_building_not() {
    for (category, can) in [(6, true), (2, false), (0x15, true), (0x22, false)] {
        struct World(i64);
        impl HudHost for World {
            fn op(&mut self, op: u32, _a: i64, _b: i64, _f: &mut [f32]) -> i64 {
                match op {
                    EE_SCREENS_CU_GROUP_COUNT => 1,
                    EE_SCREENS_CU_POINT_AT => 1,
                    EE_SCREENS_CU_CATEGORY => self.0,
                    _ => 0,
                }
            }
        }
        let mut fields = EeScreensCommandUnitsFields::default();
        start(&unsafe { Rec::new(&mut fields) }, &mut World(category), 10.4, 20.5);
        assert_eq!(fields.mCanFace, can, "category {category}");
        assert_eq!((fields.mDownLocation.x, fields.mDownLocation.y), (10.0, 20.0));
    }
}
