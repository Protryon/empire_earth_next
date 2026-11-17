use super::*;
use crate::screens::editor_players::{EE_SCREENS_SEPL_OP_MAKE, EE_SCREENS_SEPL_OP_RECT};

/// A recording host over a small world: colour types 0x10, 0x20, 0x30 (ids 1, 2, 3), terrains 0x100 .. 0x300 (ids 7,
/// 8, 9; colour types 2, 9 (no table), 1; the last ambient-only with unit class 44), each made control a counted
/// handle with the record's rectangle, the selections and the radios' checks set by the test.
#[derive(Default)]
struct Rec {
    log: Vec<String>,
    next: usize,
    rects: Vec<(usize, [f32; 4])>,
    selections: [i32; 2],
    checked: Vec<usize>,
    colour_counts: i32,
    line_textures: bool,
    current: bool,
}

impl Rec {
    fn new() -> Self {
        Rec { selections: [-1, -1], colour_counts: 7, line_textures: true, current: true, ..Default::default() }
    }
    fn handle(&mut self, r: [f32; 4]) -> usize {
        self.next += 1;
        let h = 0x1000 + self.next;
        self.rects.push((h, r));
        h
    }
}

impl SeplHost for Rec {
    fn op(&mut self, op: u32, p: [usize; 3], i: &mut [i32; 4], f: &mut [f32; 4]) -> i32 {
        let out = |p0: usize, v: usize| unsafe { *(p0 as *mut usize) = v };
        match op {
            EE_SCREENS_SEPL_OP_MAKE => {
                let c = unsafe { *(p[1] as *const EeScreensSeplCtl) };
                let h = self.handle([c.x, c.y, c.x + c.w - 1.0, c.y + 10.0]);
                self.log.push(format!("label {:#x} ix {} at {},{} w {}", c.id, c.index, c.x, c.y, c.w));
                out(p[0], h);
                0
            }
            EE_SCREENS_SEPL_OP_RECT => {
                *f = self.rects.iter().find(|r| r.0 == p[0]).map_or([0.0; 4], |r| r.1);
                0
            }
            EE_SCREENS_SEPL_OP_RADIO_IS_CHECKED => i32::from(self.checked.contains(&p[0])),
            EE_SCREENS_SETN_OP_TABLES => {
                let v: &[usize] = if i[0] == 0 { &[0x10, 0x20, 0x30] } else { &[0x100, 0x200, 0x300] };
                if p[0] != 0 {
                    for (k, x) in v.iter().enumerate().take(i[1] as usize) {
                        unsafe { *(p[0] as *mut usize).add(k) = *x };
                    }
                }
                v.len() as i32
            }
            EE_SCREENS_SETN_OP_COLOR_TYPE => (p[0] / 0x10) as i32,
            EE_SCREENS_SETN_OP_TERRAIN => {
                let k = (p[0] / 0x100) as i32;
                *i = [6 + k, [2, 9, 1][k as usize - 1], i32::from(k == 3), i32::from(k == 3)];
                if p[1] != 0 {
                    out(p[1], 0x7000 + k as usize);
                }
                if k == 3 {
                    44
                } else {
                    -1
                }
            }
            EE_SCREENS_SETN_OP_TYPE_TABLE => {
                out(p[0], if (1..=3).contains(&i[0]) { 0x10 * i[0] as usize } else { 0 });
                0
            }
            EE_SCREENS_SETN_OP_RADIO
            | EE_SCREENS_SETN_OP_TYPE_LIST
            | EE_SCREENS_SETN_OP_COLOR_LIST
            | EE_SCREENS_SETN_OP_PAINT
            | EE_SCREENS_SETN_OP_BITMAP => {
                let extra = if op == EE_SCREENS_SETN_OP_TYPE_LIST {
                    let q = unsafe { core::slice::from_raw_parts(p[2] as *const usize, 3) };
                    let sets = unsafe { core::slice::from_raw_parts(q[2] as *const i32, i[0] as usize) };
                    format!(" sets {sets:?}")
                } else {
                    String::new()
                };
                let h = self.handle([f[0], f[1], f[0] + 100.0, f[1] + 85.0]);
                self.log.push(format!("{op} {i:?} {f:?}{extra}"));
                out(p[0], h);
                0
            }
            EE_SCREENS_SETN_OP_SELECTION => self.selections[i[0] as usize],
            EE_SCREENS_SETN_OP_LINE_TEXTURE => i32::from(self.line_textures),
            EE_SCREENS_SETN_OP_COLOR_COUNT => self.colour_counts,
            EE_SCREENS_SETN_OP_CLASS_BUTTON => {
                out(p[0], 0x4400);
                1
            }
            EE_SCREENS_SEEL_OP_MOUSE => {
                *f = [5.0, 6.0, 0.0, 0.0];
                i32::from(self.current)
            }
            EE_SCREENS_SEMP_OP_CONTROL => {
                out(p[1], 0x5000 + i[0] as usize);
                0
            }
            EE_SCREENS_SETN_OP_PAINT_ACTION => {
                out(p[1], 0xacc);
                self.log.push(format!("{op} {:#x} {i:?}", p[0]));
                0
            }
            _ => {
                self.log.push(format!("{op} {:#x} {:#x} {i:?} {f:?}", p[0], p[1]));
                0
            }
        }
    }
}

fn tab(h: &mut Rec) -> TerrainTab {
    let mut index = 20;
    let t = TerrainTab::new(h, 0xabc, 0x50, 0x60, 0x70, &mut index);
    assert_eq!(index, 28);
    t
}

/// The constructor: the radios (terrain checked), the mountains radio added first, the titles, the type list under
/// its title with each terrain's colour set (the second's type has no table: 0), the colour list with the first
/// terrain's set selected, the paint area, the terrain radio added, the bitmap right of the colour list and centred
/// on it, the previews loaded.
#[test]
fn constructor_builds_the_tab() {
    let mut h = Rec::new();
    let t = tab(&mut h);
    let log = h.log.join("\n");
    assert!(h.log[0].starts_with("1004 [200, 20508, 20, 1] [115.0, 621.0"), "{log}");
    assert!(h.log[1].starts_with("1004 [202, 20510, 21, 0] [115.0, 711.0"), "{log}");
    assert_eq!(h.log[2], "1010 0x1002 0x50 [3, 0, 0, 0] [0.0, 0.0, 0.0, 0.0]");
    assert_eq!(h.log[3], "label 0xcb ix 22 at 250,621 w 200");
    assert_eq!(h.log[4], "label 0xce ix 23 at 455,621 w 120");
    assert!(
        h.log[5].starts_with("1005 [3, 24, 0, 0] [250.0, 636.0, 200.0") && h.log[5].ends_with("sets [1, 0, 0]"),
        "{log}"
    );
    assert!(h.log[6].starts_with("1006 [3, 25, 1, 0] [455.0, 636.0, 120.0"), "{log}");
    assert!(h.log[7].starts_with("1007 [26, 0, 0, 0]"), "{log}");
    assert_eq!(h.log[8], "1010 0x1001 0x50 [3, 0, 0, 0] [0.0, 0.0, 0.0, 0.0]");
    // the colour list is 455..555 x 636..721: the bitmap at 565, 636 + 42.5 - 32
    assert!(h.log[9].starts_with("1008 [27, 0, 0, 0] [565.0, 646.5"), "{log}");
    assert!(h.log[10].starts_with("1009 0x0 0x0 [1, 0, 0, 0]"), "{log}");
    assert_eq!(t.get(EE_SCREENS_SETN_GET_SELECTED, 0), 0);
    assert_eq!(t.get(EE_SCREENS_SETN_GET_TERRAIN_COUNT, 0), 3);
    assert_eq!(t.get(EE_SCREENS_SETN_GET_TERRAIN_ID, 2), 9);
    assert_eq!(t.get(EE_SCREENS_SETN_GET_COLOR_ID, 1), 2);
    assert_eq!(t.get(EE_SCREENS_SETN_GET_COLOR_ID, 5), -1);
    h.log.clear();
    t.destroy(&mut h);
    assert_eq!(h.log.len(), 9);
    assert!(h.log[0].starts_with("2 0x1001") && h.log[2].starts_with("2 0x1005") && h.log[3].starts_with("2 0x1003"));
    assert!(h.log[8].starts_with("1009 0x0 0x0 [0, 0, 0, 0]"));
}

/// `Enter`: the previews (the ambient-only terrain its unit's button), the selected line shown and the colour list's
/// event fired, the selection loaded; without a selection the selected terrain is none.
#[test]
fn enter_sets_the_previews_and_loads_the_selection() {
    let mut h = Rec::new();
    let t = tab(&mut h);
    h.log.clear();
    h.selections = [1, 0];
    t.enter(&mut h);
    let log = h.log.join("\n");
    assert!(
        log.contains("1012 0x1005 0x7001 [0, 0, 0, 0]") && log.contains("1012 0x1005 0x7002 [1, 0, 0, 0]"),
        "{log}"
    );
    assert!(log.contains("1012 0x1005 0x4400 [2, 0, 0, 0]"), "{log}");
    assert!(log.contains("1015 0x1005 0x1008 [1, 0, 0, 0]") && log.contains("1017 0x1006"), "{log}");
    assert!(log.contains("1021 0x50 0x0 [1032, 8, 0, 0]"), "{log}");
    assert_eq!(t.get(EE_SCREENS_SETN_GET_SELECTED, 0), 1);
    h.selections = [-1, -1];
    t.enter(&mut h);
    assert_eq!(t.get(EE_SCREENS_SETN_GET_SELECTED, 0), -1);
    h.log.clear();
    t.leave(&mut h);
    assert_eq!(h.log.iter().filter(|l| l.starts_with("1012 0x1005 0x0")).count(), 3);
    assert!(!h.log.iter().any(|l| l.starts_with("1021")));
}

/// The type list's event: the preview, the line's colour set selected and fired, the old terrain unloaded and the
/// new loaded; out of range the selected terrain is none.
#[test]
fn type_change_selects_the_colour_set_and_loads() {
    let mut h = Rec::new();
    let t = tab(&mut h);
    h.log.clear();
    h.selections = [0, 2];
    t.type_change(&mut h, 0x1005);
    let log = h.log.join("\n");
    assert!(log.contains("1016 0x1006 0x0 [1, 0, 0, 0]") && log.contains("1018 0x1008"), "{log}");
    assert!(log.contains("1021 0x50 0x0 [1033, 7, 0, 0]") && log.contains("1021 0x50 0x0 [1032, 7, 0, 0]"), "{log}");
    t.select_terrain(&mut h, 7);
    assert_eq!(t.get(EE_SCREENS_SETN_GET_SELECTED, 0), -1);
    h.log.clear();
    t.select_terrain(&mut h, 2);
    assert_eq!(h.log.len(), 1);
    assert_eq!(t.get(EE_SCREENS_SETN_GET_SELECTED, 0), 2);
}

/// The colour list's event: the corners 0, (n - 2) / 3, n - third - 1 and n - 1; small sets use 0 as the third.
#[test]
fn colour_change_tints_the_corners() {
    let mut h = Rec::new();
    let t = tab(&mut h);
    h.selections = [0, 1];
    for (n, want) in [(7, "[1, 1, 5, 6]"), (2, "[1, 0, 1, 1]"), (1, "[1, 0, 0, 0]"), (11, "[1, 3, 7, 10]")] {
        h.log.clear();
        h.colour_counts = n;
        t.colour_change(&mut h, 0x1006);
        assert!(h.log[0].starts_with(&format!("1020 0x1006 0x1008 {want}")), "{n}: {:?}", h.log);
    }
    h.log.clear();
    h.colour_counts = 0;
    t.colour_change(&mut h, 0x1006);
    assert!(h.log.is_empty());
}

/// `CreatePaintAction`: the bulldozer (not centred) unless the terrain radio is checked; a terrain with the selected
/// colour type, else the terrain's own type as a line (id 0 past the list), centred; none without a type.
#[test]
fn paint_action_follows_the_radios_and_lists() {
    let mut h = Rec::new();
    let t = tab(&mut h);
    assert_eq!(t.paint_action(&mut h), (PaintAction::Bulldozer, Some(false)));
    h.checked = vec![0x1001];
    assert_eq!(t.paint_action(&mut h), (PaintAction::None, None));
    h.selections = [0, 2];
    assert_eq!(t.paint_action(&mut h), (PaintAction::Terrain(7, 3), Some(true)));
    h.selections = [1, -1];
    assert_eq!(t.paint_action(&mut h), (PaintAction::Terrain(8, 0), Some(true)));
    h.selections = [2, -1];
    assert_eq!(t.paint_action(&mut h), (PaintAction::Terrain(9, 2), Some(true)));
    h.log.clear();
    assert_eq!(t.create_paint_action(&mut h, 0x1007), 0xacc);
    assert_eq!(h.log[0], "1022 0x1007 [0, 9, 2, 1]");
}

/// The mouse: the highlight centred unless the bulldozer is checked; `Update` only on the current form; the radios'
/// `Check` shows or hides the five controls.
#[test]
fn mouse_and_check() {
    let mut h = Rec::new();
    let t = tab(&mut h);
    h.log.clear();
    t.update(&mut h);
    assert_eq!(h.log[0], "906 0x50 0x0 [1, 1, 0, 0] [5.0, 6.0, 0.0, 0.0]");
    h.checked = vec![0x1002];
    t.mouse_move(&mut h, [1.0, 2.0]);
    assert_eq!(h.log[1], "906 0x50 0x0 [1, 0, 0, 0] [1.0, 2.0, 0.0, 0.0]");
    h.current = false;
    t.update(&mut h);
    assert_eq!(h.log.len(), 2);
    h.log.clear();
    paint_type_check(&mut h, 0x50, 0xca);
    assert_eq!(h.log.len(), 5);
    assert!(h.log.iter().all(|l| l.starts_with("4 0x50") && l.contains("[0, 0, 0, 0]")), "{:?}", h.log);
    assert!(h.log[0].starts_with("4 0x50cb"));
    h.log.clear();
    paint_type_check(&mut h, 0x50, 0xc8);
    assert!(h.log.iter().all(|l| l.contains("[1, 0, 0, 0]")));
    h.log.clear();
    paint_type_check(&mut h, 0x50, 0x99);
    assert!(h.log.is_empty());
}
