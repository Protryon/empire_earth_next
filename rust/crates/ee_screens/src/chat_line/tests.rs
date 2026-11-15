use super::*;
use crate::hud_ctl::replay::{cases, f, flag, hex_f, rect, value, Replay};

fn digest(r: &EeScreensChatLineFields) -> String {
    format!("{} {} {} {}", rect(&r.mTextRectangle), f(r.mOpacity), r.mShadowOffset, flag(r.mShowing))
}

#[test]
fn replays_the_golden() {
    let all = cases(1);
    assert!(all.len() > 50, "{} chat line cases", all.len());
    let mut calls = 0;
    for (n, case) in all.iter().enumerate() {
        let mut fields = EeScreensChatLineFields::default();
        let rec = unsafe { Rec::new(&mut fields) };
        let mut h = Replay::new(&case.init, format!("case {n} init"));
        init(&rec, hex_f(&format!("{:08x}", case.head[2].to_bits())));
        h.done();
        assert_eq!(rec.get(digest), case.init.record, "case {n} init");
        for (i, call) in case.calls.iter().enumerate() {
            let ctx = format!("case {n} call {i} {:?}", call.what);
            let mut h = Replay::new(call, ctx.clone());
            let result = match call.what[0].as_str() {
                "C" => copy_data(&rec, &mut h),
                "Y" => {
                    let p = |k: &str| value(k) as *const c_void;
                    display(&mut h, p("#11"), p("#12"), p("#13"), call.what[1].parse().unwrap()) as i64
                }
                "D" => draw(&rec, &mut h),
                "T" => draw_text(&rec, &mut h),
                _ => {
                    let o: u32 = call.what[1].parse().unwrap();
                    rec.set(|r| r.mShadowOffset = o);
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
