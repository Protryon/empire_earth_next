//! The framed background against the real C++: a scratch driver (`build/sc13/frm_driver.cpp`) ran the unmodified
//! `EEUIBGenericBackground.cpp`'s `DrawBackground` (with its `DrawCorners`, `DrawEdges`, `DrawFrameCorners`,
//! `DrawSides`, `DrawEdge` and `DrawCorner`) over a fake bitmap (each stretch printed, its scales and texture
//! coordinates drawn from it, each reorder and location printed), a fake rhombus and rasterizer (each draw printed, its
//! result drawn) and the real `GERectangle` (ee_ge's `And`), on 2,000 cases of rectangles (some inverted or
//! fractional), scales, flags (all five bits) and redraw rectangles (the screen, the background, a part or none of it).
//! The driver relinked over the new unit (this module over `ee_screenscpp_frame_fill` and `_piece`) printed the same
//! on seeds 1-4 (2,000 cases; 20,000 on seeds 2-4). Its host log is `golden_frame.txt`: per case the background's
//! rectangle, scales, the redraw rectangle (float bits) and the flags, then the host calls of the relinked run
//! (`fill` with the panel's rectangle and its result, `piece` with every field). The replay draws each case over a
//! host that checks every call against the log and answers the fill's result.

use super::*;

const GOLDEN: &str = include_str!("../golden_frame.txt");

fn floats(words: &[&str]) -> Vec<f32> {
    words.iter().map(|w| f32::from_bits(u32::from_str_radix(w, 16).unwrap())).collect()
}

struct Replay<'a> {
    calls: std::slice::Iter<'a, Vec<&'a str>>,
    case: &'a str,
}

fn bits(values: &[f32]) -> Vec<String> {
    values.iter().map(|v| format!("{:08x}", v.to_bits())).collect()
}

impl FrameHost for Replay<'_> {
    fn fill(&mut self, inner: [f32; 4]) -> c_long {
        let want = self.calls.next().unwrap_or_else(|| panic!("{}: an extra fill", self.case));
        assert_eq!(want[0], "fill", "{}", self.case);
        assert_eq!(bits(&inner), want[1..5], "{}", self.case);
        want[5].parse().unwrap()
    }
    fn piece(&mut self, piece: &EeScreensFramePiece) {
        let want = self.calls.next().unwrap_or_else(|| panic!("{}: an extra piece", self.case));
        assert_eq!(want[0], "piece", "{}", self.case);
        let mut got = bits(&piece.texture);
        got.extend(bits(&piece.destination));
        got.extend(bits(&piece.location));
        got.push(piece.cell.to_string());
        got.extend(piece.orders.iter().map(|o| format!("{o:x}")));
        got.push((piece.swap_scale as u32).to_string());
        assert_eq!(got, want[1..], "{}", self.case);
    }
}

#[test]
fn replays_the_golden() {
    let mut cases: Vec<(&str, Vec<Vec<&str>>)> = Vec::new();
    for line in GOLDEN.lines() {
        if let Some(call) = line.strip_prefix(' ') {
            cases.last_mut().unwrap().1.push(call.split(' ').collect());
        } else {
            cases.push((line, Vec::new()));
        }
    }
    assert_eq!(cases.len(), 2000);
    let (mut pieces, mut turned, mut flipped) = (0, 0, 0);
    for (header, calls) in &cases {
        let words: Vec<&str> = header.split(' ').collect();
        let v = floats(&words[2..12]);
        let frame = Frame {
            rectangle: GeRectangle::new(v[0], v[1], v[2], v[3]),
            scale_x: v[4],
            scale_y: v[5],
            flags: words[12].parse().unwrap(),
        };
        let clip = GeRectangle::new(v[6], v[7], v[8], v[9]);
        let mut h = Replay { calls: calls.iter(), case: header };
        frame.draw(&mut h, &clip);
        assert!(h.calls.next().is_none(), "{header}: calls left");
        for call in calls.iter().filter(|c| c[0] == "piece") {
            pieces += 1;
            turned += (call[15] == "1") as u32;
            flipped += (call[13] != "100" || call[14] != "100") as u32;
        }
    }
    assert!(pieces > 5000 && turned > 1000 && flipped > 1000, "{pieces} {turned} {flipped}");
}

/// A host that records the pieces.
#[derive(Default)]
struct Record(Vec<EeScreensFramePiece>);

impl FrameHost for Record {
    fn fill(&mut self, _inner: [f32; 4]) -> c_long {
        7
    }
    fn piece(&mut self, piece: &EeScreensFramePiece) {
        self.0.push(*piece);
    }
}

#[test]
fn a_full_frame_draws_four_edges_four_corners_and_two_sides() {
    let frame = Frame { rectangle: GeRectangle::new(0.0, 0.0, 400.0, 300.0), scale_x: 1.0, scale_y: 1.0, flags: 7 };
    let mut h = Record::default();
    assert_eq!(frame.draw(&mut h, &GeRectangle::new(0.0, 0.0, 1000.0, 1000.0)), 7);
    let cells: Vec<u32> = h.0.iter().map(|p| p.cell).collect();
    assert_eq!(cells, [0, 0, 0, 0, 1, 1, 1, 1, 3, 3]);
    // the bottom edge: turned and flipped top to bottom
    assert_eq!(h.0[3].orders, [FRAME_ROTATE, EE_SCREENS_FRAME_KEEP, FRAME_FLIP_V]);
    assert!(h.0[3].swap_scale);
}

#[test]
fn a_redraw_rectangle_off_the_background_still_draws_the_frame() {
    // (the original's: GERectangle::And leaves the background's visible part whole when the redraw rectangle misses
    // it, so the pieces are clipped to the whole background)
    let frame = Frame { rectangle: GeRectangle::new(0.0, 0.0, 400.0, 300.0), scale_x: 1.0, scale_y: 1.0, flags: 7 };
    let mut h = Record::default();
    frame.draw(&mut h, &GeRectangle::new(500.0, 500.0, 600.0, 600.0));
    assert_eq!(h.0.len(), 10);
}
