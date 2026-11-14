//! mspaint's plans: the golden of the original statements (`golden_mspaint.txt`, recorded by
//! tests/won/won_mspaint_test.cpp `--print` over the C++ before the port) replayed through a logged Graphics that
//! executes the plans as WONGUIDraw.h does, and unit tests of the helpers' corners.

use super::*;
use crate::guidraw::EE_WON_DRAW_PIXEL;

/// The trace line of one executed operation, as the C++ test's logged Graphics writes it.
fn render(o: &EeWonDrawOp, out: &mut Vec<String>) {
    let line = match o.kind {
        EE_WON_DRAW_SET_SYS_COLOR => format!("sys {}", o.a),
        EE_WON_DRAW_FILL_RECT => format!("fill {} {} {} {}", o.a, o.b, o.c, o.d),
        EE_WON_DRAW_RECT => format!("rect {} {} {} {}", o.a, o.b, o.c, o.d),
        EE_WON_DRAW_LINE => format!("line {} {} {} {}", o.a, o.b, o.c, o.d),
        EE_WON_DRAW_TRANSLATE => format!("translate {} {}", o.a, o.b),
        EE_WON_DRAW_PIXEL => format!("pixel {} {} {:x}", o.a, o.b, o.c),
        EE_WON_DRAW_SYS_PIXEL => format!("pixel {} {} {:x}", o.a, o.b, 0x5000 + o.c),
        EE_WON_DRAW_FILL_ELLIPSE => format!("ellipse {} {} {} {}", o.a, o.b, o.c, o.d),
        EE_WON_DRAW_ARC => format!("arc {} {} {} {} {} {}", o.a, o.b, o.c, o.d, o.e, o.f),
        EE_WON_DRAW_CALL if o.a == EE_WON_MSPAINT_CALL_TEXT => format!("text {} {}", o.b, o.c),
        k => format!("op {k}"),
    };
    out.push(line);
}

fn kind_of(v: i32) -> EeWonMsPaintKind {
    type K = EeWonMsPaintKind;
    const ALL: [K; 25] = [
        K::Frame3D,
        K::Frame,
        K::Frame3,
        K::Arrow,
        K::Check,
        K::Radio,
        K::Tab,
        K::CloseX,
        K::Button,
        K::TabButton,
        K::Label,
        K::HeaderSort,
        K::CloseButton,
        K::ScrollThumb,
        K::ArrowButton,
        K::Frame3DComponent,
        K::Popup,
        K::DlgBack,
        K::EtchedFrame,
        K::InputBox,
        K::Scrollbar,
        K::ComboBox,
        K::ScrollContainer,
        K::EtchedGroup,
        K::SkinImage,
    ];
    ALL.into_iter().find(|k| *k as i32 == v).expect("a paint kind")
}

fn plan_lines(p: &EeWonMsPaint) -> Vec<String> {
    let mut plan = Plan::default();
    plan.paint(p);
    let mut out = Vec::new();
    for o in &plan.ops {
        render(o, &mut out);
    }
    out
}

/// The golden's paints: each `P` line's inputs and the original's trace lines after it.
fn golden(text: &str) -> Vec<(EeWonMsPaint, Vec<String>)> {
    let mut v: Vec<(EeWonMsPaint, Vec<String>)> = Vec::new();
    for line in text.lines() {
        if let Some(rest) = line.strip_prefix("P ") {
            let n: Vec<i64> = rest.split(' ').map(|s| s.parse().expect("a number")).collect();
            v.push((
                EeWonMsPaint {
                    kind: kind_of(n[0] as i32),
                    flags: n[1] as u32,
                    width: n[2] as i32,
                    height: n[3] as i32,
                    a: n[4] as i32,
                    b: n[5] as i32,
                    c: n[6] as i32,
                    d: n[7] as i32,
                    e: n[8] as i32,
                },
                Vec::new(),
            ));
        } else if !line.is_empty() {
            v.last_mut().expect("a paint first").1.push(line.to_string());
        }
    }
    v
}

const GOLDEN: &str = include_str!("golden_mspaint.txt");

fn replay(text: &str) -> usize {
    let mut bad = 0;
    for (p, want) in golden(text) {
        if plan_lines(&p) != want {
            bad += 1;
        }
    }
    bad
}

#[test]
fn golden_replays() {
    let paints = golden(GOLDEN);
    assert!(paints.len() >= 400, "{} paints", paints.len());
    let kinds: std::collections::BTreeSet<i32> = paints.iter().map(|(p, _)| p.kind as i32).collect();
    assert_eq!(kinds.len(), 25);
    assert_eq!(replay(GOLDEN), 0);
}

#[test]
fn golden_fails_on_a_changed_line() {
    // one line of a frame moved by a pixel
    let changed = GOLDEN.replacen("line 1 1 ", "line 1 2 ", 1);
    assert_ne!(changed, GOLDEN);
    assert_eq!(replay(&changed), 1);
}

#[test]
fn button_text_is_the_last_operation_shifted_while_pressed() {
    for (flags, off) in [(0, 0), (EE_WON_MSPAINT_PRESSED, 1)] {
        let p =
            EeWonMsPaint { kind: EeWonMsPaintKind::Button, flags, width: 40, height: 20, a: 0, b: 0, c: 0, d: 0, e: 0 };
        let lines = plan_lines(&p);
        assert_eq!(lines.last().unwrap(), &format!("text {off} {off}"));
    }
}

#[test]
fn a_default_pressed_button_uses_the_flat_frame_inside_the_outline() {
    let p = EeWonMsPaint {
        kind: EeWonMsPaintKind::Button,
        flags: EE_WON_MSPAINT_DEFAULT | EE_WON_MSPAINT_PRESSED,
        width: 40,
        height: 20,
        a: 0,
        b: 0,
        c: 0,
        d: 0,
        e: 0,
    };
    let lines = plan_lines(&p);
    assert!(lines.contains(&"rect 0 0 40 20".to_string()));
    // the flat frame when down: a shadow rectangle at the translated origin, 38 x 18
    assert!(lines.contains(&"translate 1 1".to_string()));
    assert!(lines.contains(&"rect 0 0 38 18".to_string()));
}

#[test]
fn arrow_offsets_are_not_read_and_disabled_arrows_get_a_highlight_pixel() {
    let p = EeWonMsPaint {
        kind: EeWonMsPaintKind::Arrow,
        flags: EE_WON_MSPAINT_DISABLED,
        width: 16,
        height: 16,
        a: 1,
        b: 7,
        c: 9,
        d: 0,
        e: 0,
    };
    let lines = plan_lines(&p);
    assert_eq!(lines[0], "sys 6");
    assert_eq!(lines.last().unwrap(), "pixel 10 9 5002");
    let q = EeWonMsPaint { b: 0, c: 0, ..p };
    assert_eq!(plan_lines(&q), lines);
}

#[test]
fn header_sort_none_draws_nothing_and_an_unknown_type_only_the_colour() {
    let p = EeWonMsPaint {
        kind: EeWonMsPaintKind::HeaderSort,
        flags: 0,
        width: 50,
        height: 20,
        a: 2,
        b: 0,
        c: 0,
        d: 0,
        e: 0,
    };
    assert!(plan_lines(&p).is_empty());
    assert_eq!(plan_lines(&EeWonMsPaint { a: 3, ..p }), vec!["sys 5".to_string()]);
}

#[test]
fn unknown_skin_image_types_draw_nothing() {
    let p = EeWonMsPaint {
        kind: EeWonMsPaintKind::SkinImage,
        flags: 0,
        width: 20,
        height: 20,
        a: 18,
        b: 15,
        c: 0,
        d: 0,
        e: 0,
    };
    assert!(plan_lines(&p).is_empty());
    assert!(!plan_lines(&EeWonMsPaint { a: 17, ..p }).is_empty());
}

#[test]
fn the_c_entry_answers_the_length_past_its_room() {
    let p = EeWonMsPaint {
        kind: EeWonMsPaintKind::Check,
        flags: EE_WON_MSPAINT_CHECKED,
        width: 13,
        height: 13,
        a: 0,
        b: 0,
        c: 0,
        d: 0,
        e: 0,
    };
    let mut few = [EeWonDrawOp::default(); 4];
    // SAFETY: the room given is the array's
    let n = unsafe { ee_won_mspaint_plan(&p, few.as_mut_ptr(), few.len()) };
    let mut plan = Plan::default();
    plan.paint(&p);
    assert_eq!(n, plan.ops.len());
    assert_eq!(&few[..], &plan.ops[..4]);
    // SAFETY: NULL with no room
    assert_eq!(unsafe { ee_won_mspaint_plan(&p, std::ptr::null_mut(), 0) }, n);
}
