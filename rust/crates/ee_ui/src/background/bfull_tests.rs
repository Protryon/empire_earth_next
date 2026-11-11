//! `UIBFull` against the real C++ (`golden_bfull.txt`): a scratch driver (`build/u9_golden`, `driver_bfull.cpp`) ran
//! the unmodified `UIBFull.cpp` over 150 full backgrounds (random forms, fades, texture sizes, texture rectangles,
//! grids of 1 x 1 to 6 x 5 and file names with and without an extension) of 12 operations: `DrawBackground` over
//! redraw rectangles inside, across and outside the form, and `LoadBackground`, printing every bitmap's stretch,
//! opacity, location and draw and every texture name loaded. The replay below draws and names the same tiles; the
//! driver relinked over the forwarders prints the same file, and the same on two more seeds at ten times the size.

use super::*;

const GOLDEN: &str = include_str!("../golden_bfull.txt");

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

struct Log<'a> {
    out: &'a mut Vec<String>,
    texture: UiRect,
    fade: f32,
    result: c_long,
}

impl FullHost for Log<'_> {
    fn draw(&mut self, index: c_ulong, tile: UiRect, clipped: UiRect) -> c_long {
        self.out.push(format!(
            "  bitmap {index} stretch {} / {} / {}",
            rect_text(&tile),
            rect_text(&clipped),
            rect_text(&self.texture)
        ));
        self.out.push(format!("  bitmap {index} opacity {}", h(self.fade)));
        self.out.push(format!("  bitmap {index} {} {} scale 00000000 00000000 scr 1", h(clipped.left), h(clipped.top)));
        self.result
    }
}

fn replay(golden: &str) -> usize {
    let lines: Vec<&str> = golden.lines().collect();
    let mut out: Vec<String> = Vec::new();
    let mut cases = 0;
    let (mut form, mut texrect, mut tex, mut grid, mut fade, mut name) =
        (UiRect::default(), UiRect::default(), (0, 0), (0, 0), 0.0, Vec::new());
    let mut op = 0;
    for line in &lines {
        let t: Vec<&str> = line.split(' ').collect();
        if t[0] == "bfull" {
            cases += 1;
            out.push(line.to_string());
            grid = (t[3].parse().unwrap(), t[4].parse().unwrap());
            form = UiRect::new(fx(t[6]), fx(t[7]), fx(t[8]), fx(t[9]));
            texrect = UiRect::new(fx(t[11]), fx(t[12]), fx(t[13]), fx(t[14]));
            tex = (t[16].parse().unwrap(), t[17].parse().unwrap());
            fade = fx(t[19]);
            name = unhex(t[21]);
            op = 0;
            continue;
        }
        if t[0] != "op" {
            continue;
        }
        out.push(line.to_string());
        let result = 7000 + op;
        op += 1;
        if t[1] == "load" {
            for row in 0..grid.1 {
                for column in 0..grid.0 {
                    let n = full_tile_name(&name, row, column);
                    let hex: String = n[..n.len() - 1].iter().map(|c| format!("{c:02x}")).collect();
                    out.push(format!("  load {}", if hex.is_empty() { "-".into() } else { hex }));
                    out.push(format!("  settexture {} scr 1", grid.0 * row + column));
                }
            }
            out.push("  load -> 0".into());
        } else {
            let redraw = UiRect::new(fx(t[3]), fx(t[4]), fx(t[5]), fx(t[6]));
            let texture = UiRect::new(0.0, 0.0, tex.0 as f32, tex.1 as f32);
            let r = bfull_draw(&mut Log { out: &mut out, texture, fade, result }, form, redraw, tex, texrect, grid);
            out.push(format!("  draw -> {r}"));
        }
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

/// Another seed's golden (seeds 2 and 3, ten times the size): `EE_UI_GOLDEN=<file> cargo test -- --ignored bfull`.
#[test]
#[ignore]
fn replays_another_seed() {
    let path = std::env::var("EE_UI_GOLDEN").expect("EE_UI_GOLDEN");
    assert_eq!(replay(&std::fs::read_to_string(path).unwrap()), 1500);
}

#[test]
fn a_redraw_outside_the_form_draws_the_whole_form() {
    let mut out = Vec::new();
    let form = UiRect::new(0.0, 0.0, 99.0, 99.0);
    let tex = UiRect::new(0.0, 0.0, 100.0, 100.0);
    let mut log = Log { out: &mut out, texture: tex, fade: 1.0, result: 5 };
    let r = bfull_draw(&mut log, form, UiRect::new(500.0, 500.0, 600.0, 600.0), (50, 50), tex, (2, 2));
    assert_eq!((r, out.len()), (5, 12));
}
