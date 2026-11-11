//! `UIIMECandidate` against the real C++ (`golden_ime.txt`): a scratch driver (`build/u9_golden`, `driver_ime.cpp`)
//! ran the unmodified `UIIMECandidate.cpp` (the real constructor, `GERhombus.cpp`, `GELine.cpp`; the manager's
//! `AddDirtyRectangle` and the form's `RedrawIntersectControls` as log lines) over 150 windows of 20 random
//! operations: `OpenCandidateWindow` over random screens and controls near every edge, `SetCandidateList` over
//! random lists of UTF-8 candidates with selections on later pages and past the end, `CopyData`, `DrawPolygons`,
//! `DrawTextA` and `CloseCandidateWindow`, printing every draw, dirty rectangle and redraw and the window's state
//! after each. A list into a window lower than a line is skipped (the C++ divides by zero). The replay below is the
//! same window over this module; the driver relinked over the forwarders prints the same file, and the same on two
//! more seeds at ten times the size.

use super::*;

const GOLDEN: &str = include_str!("../golden_ime.txt");
const FONT_HEIGHTS: [c_long; 3] = [8, 12, 23];
const DARK: [f32; 3] = [0.25, 0.125, 0.5];
const LIGHT: [f32; 3] = [0.75, 1.0, 0.875];

fn h(v: f32) -> String {
    format!("{:08x}", v.to_bits())
}

fn fx(s: &str) -> f32 {
    f32::from_bits(u32::from_str_radix(s, 16).unwrap())
}

fn rect_text(r: &UiRect) -> String {
    format!("{} {} {} {}", h(r.left), h(r.top), h(r.right), h(r.bottom))
}

fn unhex(s: &str) -> Vec<u8> {
    if s == "-" {
        return Vec::new();
    }
    (0..s.len()).step_by(2).map(|i| u8::from_str_radix(&s[i..i + 2], 16).unwrap()).collect()
}

fn hex(b: &[u8]) -> String {
    if b.is_empty() {
        return "-".into();
    }
    b.iter().map(|c| format!("{c:02x}")).collect()
}

/// The driver's text measure (`fakes.cpp`).
fn width(text: &[u8], height: c_long) -> f32 {
    let mut w = 0.0f32;
    let mut at = 0;
    while at < text.len() {
        let (c, l) = ee_plat_core::utf8::decode(&text[at..]);
        w += (3 + c.wrapping_mul(7) % 9) as f32 * (height / 8 + 1) as f32;
        at += l.max(1);
    }
    w
}

struct List<'a> {
    strings: &'a [Vec<u8>],
    at: usize,
    lines: Vec<Vec<u8>>,
    font: c_long,
}

impl CandidateHost for List<'_> {
    fn skip(&mut self) {
        self.at += 1;
    }
    fn line(&mut self, number: &[u8]) -> f32 {
        let mut l = number.to_vec();
        l.extend_from_slice(&self.strings[self.at]);
        self.at += 1;
        let w = width(&l, self.font);
        self.lines.push(l);
        w
    }
}

#[derive(Default)]
struct Window {
    rect: UiRect,
    copy: UiRect,
    selection: c_long,
    copy_selection: c_long,
    lines: Vec<Vec<u8>>,
    copy_lines: Vec<Vec<u8>>,
}

fn replay(golden: &str) -> usize {
    let lines: Vec<&str> = golden.lines().collect();
    let mut out: Vec<String> = Vec::new();
    let mut cases = 0;
    let mut w = Window::default();
    let (mut screen, mut ratios, mut font, mut window_width) = (UiRect::default(), (0.0, 0.0), 0, 0);
    let mut op = 0;
    let state = |w: &Window, out: &mut Vec<String>| {
        let mut l = format!(
            "  state sel {} copy {} rect {} copy {} lines {}",
            w.selection,
            w.copy_selection,
            rect_text(&w.rect),
            rect_text(&w.copy),
            w.lines.len()
        );
        for s in &w.lines {
            l += &format!(" {}", hex(s));
        }
        out.push(l);
    };
    for line in &lines {
        let t: Vec<&str> = line.split(' ').collect();
        if t[0] == "ime" {
            cases += 1;
            out.push(line.to_string());
            window_width = t[3].parse().unwrap();
            ratios = (fx(t[6]), fx(t[7]));
            font = FONT_HEIGHTS[t[9].parse::<usize>().unwrap()];
            screen = UiRect::new(fx(t[11]), fx(t[12]), fx(t[13]), fx(t[14]));
            w = Window { selection: -1, copy_selection: -1, ..Window::default() };
            op = 0;
            state(&w, &mut out);
            continue;
        }
        if t[0] != "op" {
            continue;
        }
        out.push(line.to_string());
        let result = 7000 + op;
        op += 1;
        match t[1] {
            "open" => {
                let control = UiRect::new(fx(t[3]), fx(t[4]), fx(t[5]), fx(t[6]));
                // OpenCandidateWindow(screen, control): height ratio first in the constructor, width second
                w.rect = open_rect(screen, control, ratios.1, ratios.0);
            }
            "list" => {
                let count: c_ulong = t[3].parse().unwrap();
                let selection: c_ulong = t[5].parse().unwrap();
                let strings: Vec<Vec<u8>> = t[6..6 + count as usize + 1].iter().map(|s| unhex(s)).collect();
                let left = fx(t[t.len() - 1]);
                if um_floor(w.rect.height() / font as f32) <= 0 {
                    out.push("  skip (a page of no line divides by zero)".into());
                } else {
                    let mut list = List { strings: &strings, at: 0, lines: Vec::new(), font };
                    let fit = candidate_list(&mut list, w.rect.height(), font, selection, count, window_width, left);
                    w.lines = list.lines;
                    out.push(format!("  dirty {}", rect_text(&w.rect)));
                    out.push(format!("  redraw {}", rect_text(&w.rect)));
                    w.rect.left = fit.left;
                    w.rect.right = fit.right;
                    w.selection = fit.selection;
                }
            }
            "copy" => {
                w.copy_lines = w.lines.clone();
                w.copy = w.rect;
                w.copy_selection = w.selection;
            }
            "draw" => {
                let rh = |r: &UiRect, c: [f32; 3]| {
                    format!(
                        "  rhombus {} col {} {} {} op 3f800000 3f800000 3f800000 3f800000 scr 1",
                        rect_text(r),
                        h(c[0]),
                        h(c[1]),
                        h(c[2])
                    )
                };
                out.push(rh(&w.copy, [0.0; 3]));
                let f = frame(w.copy, w.copy_selection, font as c_ulong);
                if f.has_selection {
                    out.push(rh(&f.selection, [0.0, 0.0, 1.0]));
                }
                for l in &f.lines {
                    let c = if l.light { LIGHT } else { DARK };
                    out.push(format!(
                        "  line {} {} {} {} col {} {} {} op 3f800000 n 1 scr 1",
                        h(l.x0),
                        h(l.y0),
                        h(l.x1),
                        h(l.y1),
                        h(c[0]),
                        h(c[1]),
                        h(c[2])
                    ));
                }
                out.push(format!("  polygons -> {result}"));
                for (i, s) in w.copy_lines.iter().enumerate() {
                    let r = row(w.copy, i as c_ulong, font as c_ulong);
                    out.push(format!(
                        "  text {} {} {} fg 3f800000 3f800000 3f800000 rect {}",
                        hex(s),
                        h(r.left),
                        h(r.top),
                        rect_text(&r)
                    ));
                }
                out.push("  text -> 0".into());
            }
            "close" => {
                out.push(format!("  dirty {}", rect_text(&w.rect)));
                out.push(format!("  redraw {}", rect_text(&w.rect)));
            }
            other => panic!("op {other}"),
        }
        state(&w, &mut out);
    }
    for (n, (a, g)) in out.iter().zip(lines.iter()).enumerate() {
        assert_eq!(a, g, "line {}", n + 1);
    }
    assert_eq!(out.len(), lines.len());
    cases
}

#[test]
fn replays_the_cpp_golden() {
    assert_eq!(replay(GOLDEN), 150);
}

/// Another seed's golden (seeds 2 and 3, ten times the size): `EE_UI_GOLDEN=<file> cargo test -- --ignored ime`.
#[test]
#[ignore]
fn replays_another_seed() {
    let path = std::env::var("EE_UI_GOLDEN").expect("EE_UI_GOLDEN");
    assert_eq!(replay(&std::fs::read_to_string(path).unwrap()), 1500);
}

#[test]
fn a_window_lower_than_a_line_has_an_empty_page() {
    let strings = vec![b"a".to_vec(); 3];
    let mut list = List { strings: &strings, at: 0, lines: Vec::new(), font: 12 };
    let fit = candidate_list(&mut list, 5.0, 12, 2, 3, 640, 10.0);
    assert_eq!((list.lines.len(), fit.selection, fit.left, fit.right), (0, 2, 10.0, 10.0));
}

#[test]
fn the_page_holds_the_selection_and_numbers_from_one() {
    let strings: Vec<Vec<u8>> = (0..9).map(|i| vec![b'a' + i]).collect();
    let mut list = List { strings: &strings, at: 0, lines: Vec::new(), font: 8 };
    let fit = candidate_list(&mut list, 32.0, 8, 5, 9, 640, 630.0);
    assert_eq!(list.lines, vec![b" 5: e".to_vec(), b" 6: f".to_vec(), b" 7: g".to_vec(), b" 8: h".to_vec()]);
    assert_eq!(fit.selection, 1);
    assert_eq!(fit.right, 640.0);
}
