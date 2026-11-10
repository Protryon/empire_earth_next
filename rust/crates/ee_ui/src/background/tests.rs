//! The backgrounds against the real C++ (`golden_background.txt`): a scratch driver built over the original
//! background units and `UIForm.cpp` (fake bitmaps whose stretch and texture coordinates are trace lines, a
//! rasterizer and a surface whose vtables are tables of traced stubs, a fake layout and manager, traced locks)
//! printed 200 backgrounds of every kind (bitmap, tiled, left-right, top-bottom, stretched, full screen, a form's tiled
//! bitmap) under 3 random redraw rectangles each, then their relayout, a move and a visibility change. The replay
//! below is the same world over this module; the same driver over the forwarders prints the same file.

use super::*;

const GOLDEN: &str = include_str!("../golden_background.txt");

fn h(v: f32) -> String {
    format!("{:08x}", v.to_bits())
}
fn fx(s: &str) -> f32 {
    f32::from_bits(u32::from_str_radix(s, 16).unwrap())
}
fn rect_text(r: &UiRect) -> String {
    format!("{} {} {} {}", h(r.left), h(r.top), h(r.right), h(r.bottom))
}
fn parse_rect(t: &[&str]) -> UiRect {
    UiRect::new(fx(t[0]), fx(t[1]), fx(t[2]), fx(t[3]))
}

/// The traced bitmap draws.
struct Draws<'a> {
    log: &'a mut Vec<String>,
    texture: UiRect,
    id: c_ulong,
    size: (c_ulong, c_ulong),
}

impl Draws<'_> {
    fn stretch(&mut self, id: c_ulong, source: UiRect, destination: UiRect, texture: UiRect) -> c_long {
        self.log.push(format!("  stretch {id} {}", rect_text(&source)));
        self.log.push(format!("   dst {}", rect_text(&destination)));
        self.log.push(format!("   tex {}", rect_text(&texture)));
        let one = h(1.0);
        self.log.push(format!("  draw {id} {} {} scale {one} {one}", h(destination.left), h(destination.top)));
        id as c_long
    }
}

impl TileHost for Draws<'_> {
    fn draw(&mut self, source: UiRect, destination: UiRect) -> c_long {
        let (id, texture) = (self.id, self.texture);
        self.stretch(id, source, destination, texture)
    }
}

impl FullHost for Draws<'_> {
    fn draw(&mut self, index: c_ulong, tile: UiRect, clipped: UiRect) -> c_long {
        let texture = UiRect::new(0.0, 0.0, self.size.0 as f32, self.size.1 as f32);
        self.stretch(10 + index, tile, clipped, texture)
    }
}

impl FormTileHost for Draws<'_> {
    /// `UIForm::DrawBackground(bitmap, piece, location)`: the piece's texture coordinates and scale, then the draw.
    fn draw(&mut self, piece: UiRect, location: (f32, f32)) {
        let (w, ht) = (self.size.0 as f32, self.size.1 as f32);
        let mut r = piece;
        r.right += 1.0;
        r.bottom += 1.0;
        let (tl_u, tl_v, br_u, br_v) = (r.left / w, r.top / ht, r.right / w, r.bottom / ht);
        let (sx, sy) = (r.width() / w, r.height() / ht);
        self.log.push(format!("  coords {} {} {} {}", h(tl_u), h(tl_v), h(br_u), h(br_v)));
        self.log.push(format!("  draw {} {} {} scale {} {}", self.id, h(location.0), h(location.1), h(sx), h(sy)));
    }
}

struct Moving<'a> {
    log: &'a mut Vec<String>,
    visible: bool,
    rect: UiRect,
    current: bool,
}

impl MoveHost for Moving<'_> {
    fn visible(&self) -> bool {
        self.visible
    }
    fn set_visible(&mut self, v: bool) {
        self.visible = v;
    }
    fn rect(&self) -> UiRect {
        self.rect
    }
    fn set_rect(&mut self, r: UiRect) {
        self.rect = r;
    }
    fn form_is_current(&self) -> bool {
        self.current
    }
    fn lock(&mut self) {
        self.log.push("  lock".into());
    }
    fn unlock(&mut self) {
        self.log.push("  unlock".into());
    }
    fn redraw(&mut self, _r: UiRect) {} // the driver's form has no controls
    fn dirty(&mut self, r: UiRect) {
        self.log.push(format!("  mdirty {}", rect_text(&r)));
    }
}

/// A tiling class's `DrawBackground`: the plan, then `DrawTiledBackground`.
fn tiled(d: &mut Draws, kind: Tiling, rect: UiRect, texture: UiRect, redraw: UiRect) -> c_long {
    match tiled_plan(kind, rect, texture, redraw) {
        Some(p) => draw_tiled(d, rect, p.clip, p.first_row, p.last_row, p.first_column, p.last_column, p.w, p.h),
        None => 0,
    }
}

#[test]
fn replays_the_cpp_backgrounds() {
    let mut out: Vec<String> = Vec::new();
    let mut log: Vec<String> = Vec::new();
    let mut size = (0, 0);
    let (mut kind, mut window, mut visible, mut counts, mut rect, mut texture) =
        (String::new(), (0, 0), false, (0.0, 0.0), UiRect::default(), UiRect::default());
    let mut lines = GOLDEN.lines().peekable();
    while let Some(line) = lines.next() {
        if line.starts_with("  ") {
            continue;
        }
        out.append(&mut log);
        out.push(line.to_string());
        let t: Vec<&str> = line.split(' ').collect();
        match t[0] {
            "texture" => size = (t[1].parse().unwrap(), t[2].parse().unwrap()),
            "round" => {
                kind = t[2].to_string();
                window = (t[4].parse().unwrap(), t[5].parse().unwrap());
                visible = t[7] == "1";
                counts = (fx(t[9]), fx(t[10]));
                rect = parse_rect(&t[11..15]);
            }
            "tex" => texture = parse_rect(&t[1..5]),
            "redraw" => {
                let redraw = parse_rect(&t[1..5]);
                let mut d = Draws { log: &mut log, texture, id: 1, size };
                let result = match kind.as_str() {
                    "bitmap" => bitmap_draw(&mut d, rect, redraw),
                    "tiled" => tiled(&mut d, Tiling::Both, rect, texture, redraw),
                    "lr" => tiled(&mut d, Tiling::LeftRight, rect, texture, redraw),
                    "tb" => tiled(&mut d, Tiling::TopBottom, rect, texture, redraw),
                    "stretched" => {
                        tiled(&mut d, Tiling::Stretched { across: counts.0, down: counts.1 }, rect, texture, redraw)
                    }
                    "full" => {
                        // the grid comes on the next (trace) line
                        let g = lines.peek().unwrap().trim_start().to_string();
                        let g: Vec<&str> = g.split(' ').collect();
                        d.log.push(format!("  full {} {} designed {} {}", g[1], g[2], g[4], g[5]));
                        let p = |s: &str| s.parse::<c_ulong>().unwrap();
                        full_draw(&mut d, redraw, size, window, (p(g[4]), p(g[5])), (p(g[1]), p(g[2])))
                    }
                    "form" => {
                        let g = lines.peek().unwrap().trim_start().to_string();
                        let g: Vec<&str> = g.split(' ').collect();
                        let draw = parse_rect(&g[1..5]);
                        d.log.push(format!("  tiles {}", rect_text(&draw)));
                        form_tiles(&mut d, redraw, draw, rect);
                        0
                    }
                    other => panic!("unknown kind {other}"),
                };
                log.push(format!("  = {result}"));
            }
            "relayout" => {
                let screen = screen_rect(window.0, window.1, false);
                log.push(format!("  layout 0 {}", rect_text(&screen)));
                rect = screen;
                log.push(format!("  = {}", rect_text(&rect)));
            }
            "move" => {
                let mut m = Moving { log: &mut log, visible, rect, current: t[2] == "1" };
                set_rectangle(&mut m, parse_rect(&t[3..7]));
                rect = m.rect;
            }
            "visibility" => {
                let mut m = Moving { log: &mut log, visible, rect, current: false };
                set_visibility(&mut m, t[1] == "1");
                visible = m.visible;
                log.push(format!("  visible {}", visible as u8));
            }
            other => panic!("unknown line {other}"),
        }
    }
    out.append(&mut log);
    let golden: Vec<&str> = GOLDEN.lines().collect();
    assert!(golden.len() > 3000, "the golden is the driver's output");
    for (i, (a, b)) in out.iter().zip(golden.iter()).enumerate() {
        assert_eq!(a, b, "line {}", i + 1);
    }
    assert_eq!(out.len(), golden.len());
}

#[test]
fn screen_rectangles() {
    assert_eq!(screen_rect(640, 480, false), UiRect::new(0.0, 0.0, 639.0, 479.0));
    assert_eq!(screen_rect(640, 480, true), UiRect::new(0.0, 0.0, 640.0, 480.0));
}
