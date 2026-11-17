use super::*;

/// Every op and an answer from a closure.
struct Log<F: FnMut(u32, i64, i64, i64) -> i64> {
    ops: Vec<u32>,
    answer: F,
}

impl<F: FnMut(u32, i64, i64, i64) -> i64> UiHost for Log<F> {
    fn op(&mut self, op: u32, a: i64, b: i64, c: i64) -> i64 {
        self.ops.push(op);
        (self.answer)(op, a, b, c)
    }
}

/// The plan: the flare only when shown, the lines by their flags, the clip one pixel inside.
#[test]
fn draw_plan_steps() {
    let d = draw_plan(false, [true, false, false, true], [10.0, 20.0, 110.0, 120.0]);
    assert_eq!(
        &d.steps[..d.count as usize],
        [
            EE_SCREENS_MM_DRAW_ENTER,
            EE_SCREENS_MM_DRAW_BASE,
            EE_SCREENS_MM_DRAW_MAP,
            EE_SCREENS_MM_DRAW_WORLD,
            EE_SCREENS_MM_DRAW_CLIP,
            EE_SCREENS_MM_DRAW_LINE,
            EE_SCREENS_MM_DRAW_LINE + 3,
            EE_SCREENS_MM_DRAW_RESTORE,
            EE_SCREENS_MM_DRAW_LEAVE
        ]
    );
    assert_eq!(d.clip, [11.0, 21.0, 109.0, 119.0]);
}

/// The flare's colour alternates while shown; a hidden flare keeps the blink on.
#[test]
fn copy_data_blinks_the_flare() {
    let mut c = EeScreensMiniMapCopy {
        window_width: 800,
        window_height: 600,
        flare_on: 1,
        flare_blink: 1,
        ..Default::default()
    };
    copy_data(&mut c);
    assert_eq!((c.show_flare, c.flare_color, c.flare_blink, c.set_flare), (1, EE_SCREENS_MM_COLOR_FLARE, 0, 1));
    copy_data(&mut c);
    assert_eq!((c.flare_color, c.flare_blink), (EE_SCREENS_MM_COLOR_BLINK, 1));
    c.free_camera = 1;
    c.flare_blink = 0;
    copy_data(&mut c);
    assert_eq!((c.show_flare, c.flare_color, c.flare_blink, c.set_flare), (0, EE_SCREENS_MM_COLOR_NONE, 1, 0));
    assert_eq!(c.map_scale, [1.0, 600.0 * 0.001_666_666_7]);
}

/// A small map's units texture covers its share of 64; the flare cursor's release sends a flare.
#[test]
fn update_extent_and_flare_release() {
    let mut u = EeScreensMiniMapUpdate { width: 32, height: 48, size: 48, ..Default::default() };
    update(&mut u);
    assert_eq!(u.extent, 0.5);
    let mut h = Log {
        ops: Vec::new(),
        answer: |op, _, _, c| match op {
            EE_SCREENS_MM_OP_HAS_CAPTURE | EE_SCREENS_MM_OP_TILE_EXISTS => 1,
            EE_SCREENS_MM_OP_POINTER => FLARE_POINTER,
            EE_SCREENS_MM_OP_READ_MOUSE => {
                let m = unsafe { &mut *(c as *mut EeScreensMiniMapMouse) };
                *m = EeScreensMiniMapMouse {
                    mouse: [5.0, 5.0],
                    rect: [0.0, 0.0, 10.0, 10.0],
                    pixel_to_map: [1.0, 0.0, 0.0, 0.0, 1.0, 0.0],
                };
                0
            }
            _ => 0,
        },
    };
    assert_eq!(mouse_l_up(&mut h), 0);
    assert!(h.ops.contains(&EE_SCREENS_MM_OP_SEND_FLARE));
    assert_eq!(h.ops.last(), Some(&EE_SCREENS_MM_OP_RELEASE));
}

const GOLDEN: &str = include_str!("../golden_mini_map.txt");

fn unhex(s: &str) -> Vec<u8> {
    (0..s.len()).step_by(2).map(|i| u8::from_str_radix(&s[i..i + 2], 16).unwrap()).collect()
}

/// A record of type T from its bytes.
fn from_bytes<T: Copy + Default>(b: &[u8]) -> T {
    assert_eq!(b.len(), core::mem::size_of::<T>());
    let mut v = T::default();
    unsafe { core::ptr::copy_nonoverlapping(b.as_ptr(), &mut v as *mut T as *mut u8, b.len()) };
    v
}

/// The left release and drag's recorded host ops: `(op, a, b, answer, the mouse record's bytes)`.
struct Replay {
    ops: Vec<(u32, i64, i64, i64, Vec<u8>)>,
    at: usize,
    line: usize,
}

impl UiHost for Replay {
    fn op(&mut self, op: u32, a: i64, b: i64, c: i64) -> i64 {
        let (line, at) = (self.line, self.at);
        let want = self.ops.get(at).unwrap_or_else(|| panic!("line {line}: extra op {op}"));
        assert_eq!((op, a, b), (want.0, want.1, want.2), "line {line}: op {at}");
        if op == EE_SCREENS_MM_OP_READ_MOUSE {
            unsafe { *(c as *mut EeScreensMiniMapMouse) = from_bytes(&want.4) };
        }
        self.at += 1;
        want.3
    }
}

/// The original `CopyData`, `DrawPolygons`, `Update`, `MouseLButtonUp` and `MouseMove` over random fake worlds
/// (`build/sc26/mm`, seed 1): the same records out of the same records in, the same ops in order, the same results.
#[test]
fn golden_mini_map() {
    let mut count = 0;
    for (n, line) in GOLDEN.lines().enumerate() {
        let (body, result) = line.rsplit_once('=').unwrap();
        let mut parts = body.split('|');
        let kind = parts.next().unwrap();
        let mut ops = Vec::new();
        for p in parts {
            let w: Vec<&str> = p.split(' ').collect();
            match w[0] {
                "C" => {
                    let mut c: EeScreensMiniMapCopy = from_bytes(&unhex(w[1]));
                    copy_data(&mut c);
                    assert_eq!(c, from_bytes::<EeScreensMiniMapCopy>(&unhex(w[2])), "line {}", n + 1);
                }
                "D" => {
                    let f = |s: &str| f32::from_bits(u32::from_str_radix(s, 16).unwrap());
                    let l: u32 = w[2].parse().unwrap();
                    let lines = [l & 1 != 0, l & 2 != 0, l & 4 != 0, l & 8 != 0];
                    let d = draw_plan(w[1] == "1", lines, [f(w[3]), f(w[4]), f(w[5]), f(w[6])]);
                    assert_eq!(d, from_bytes::<EeScreensMiniMapDraw>(&unhex(w[7])), "line {}", n + 1);
                }
                "U" => {
                    let mut u: EeScreensMiniMapUpdate = from_bytes(&unhex(w[1]));
                    update(&mut u);
                    assert_eq!(u, from_bytes::<EeScreensMiniMapUpdate>(&unhex(w[2])), "line {}", n + 1);
                }
                _ => {
                    let num = |i: usize| w[i].parse::<i64>().unwrap();
                    ops.push((num(0) as u32, num(1), num(2), num(3), w.get(4).map(|s| unhex(s)).unwrap_or_default()));
                }
            }
        }
        if kind == "L" || kind == "V" {
            let mut h = Replay { ops, at: 0, line: n + 1 };
            let r = if kind == "L" { mouse_l_up(&mut h) } else { mouse_move(&mut h) };
            assert_eq!(h.at, h.ops.len(), "line {}: ops left", n + 1);
            assert_eq!(r, result.parse::<i64>().unwrap(), "line {}: result", n + 1);
        }
        count += 1;
    }
    assert_eq!(count, 1000);
}
