//! The scenario editor's brush size group in Rust (`EEUIFSEBrushGroup`, `Scenario Editor/EEUIEOpenCivBuilder.cpp`
//! 0x1a0315..0x1a0778): the constructor's controls (the "Brush Size" title and the tiny .. huge radios, tiny checked,
//! all on the terrain and elevation tabs' groups 2 and 3) and `SetHighlight`'s square. The group stays a C++ record
//! (the paint areas, the tabs and the form read its size, highlight and rectangle); the title is the shared maker's,
//! the radios (`EEUIFSERBBrush`, a class of their own) the terrain tab's host's (`ee_screenscpp_setn`, 1030..).

use crate::editor_players::{
    rect, EeScreensSeplCtl, Maker, SeplHost, EE_SCREENS_SEPL_ALIGN_CENTER, EE_SCREENS_SEPL_FG,
    EE_SCREENS_SEPL_FONT_HEIGHT, EE_SCREENS_SEPL_LABEL,
};
use crate::editor_terrain::EE_SCREENS_SETN_OP_ADD;

/// A brush radio (`EEUIFSERBBrush` of the group p2, the brush size i0, id i1, the string i2, the index i3, checked
/// when the size is 0) at (f0, f1) on the form p1, its text yellow -> into p0 (not added).
pub const EE_SCREENS_SETN_OP_BRUSH: u32 = 1030;

const IDS_BRUSH_SIZE: u32 = 20500;
const IDS_SE_TINY: i32 = 20501;
const ID_TITLE: u32 = 7;

/// The group's controls: the title, then the five radios (sizes 0 .. 4, ids 2 .. 6).
pub type BrushControls = [usize; 6];

/// The constructor's controls on `form` at (`x`, `y`) from the control index `*index` (advanced), the radios over the
/// C++ group `group`: each one under the last, added to groups 2 and 3, the tiny one last.
pub fn build<H: SeplHost + ?Sized>(
    h: &mut H,
    group: usize,
    form: usize,
    at: [f32; 2],
    index: &mut u32,
) -> BrushControls {
    let mut out = [0usize; 6];
    let title = EeScreensSeplCtl::new(EE_SCREENS_SEPL_LABEL, ID_TITLE, at[0], at[1], 100.0, 0.0)
        .string(IDS_BRUSH_SIZE)
        .flags(EE_SCREENS_SEPL_FG | EE_SCREENS_SEPL_FONT_HEIGHT)
        .align(EE_SCREENS_SEPL_ALIGN_CENTER)
        .bg(0.4)
        .group(2)
        .group(3);
    let mut m = Maker { h: &mut *h, form, index: *index };
    out[0] = m.make(title);
    *index = m.index;
    let mut y = rect(h, out[0])[3] + 1.0;
    for size in 0..5usize {
        let (mut i, mut f) =
            ([size as i32, 2 + size as i32, IDS_SE_TINY + size as i32, *index as i32], [at[0], y, 0.0, 0.0]);
        let mut button = 0usize;
        h.op(EE_SCREENS_SETN_OP_BRUSH, [&mut button as *mut usize as usize, form, group], &mut i, &mut f);
        *index += 1;
        out[1 + size] = button;
        if size > 0 {
            add(h, button, form);
        }
        if size < 4 {
            y = rect(h, button)[3] + 1.0;
        }
    }
    add(h, out[1], form);
    out
}

fn add<H: SeplHost + ?Sized>(h: &mut H, control: usize, form: usize) {
    for g in [2, 3] {
        let (mut i, mut f) = ([g, 0, 0, 0], [0.0; 4]);
        h.op(EE_SCREENS_SETN_OP_ADD, [control, form, 0], &mut i, &mut f);
    }
}

/// `SetHighlight(x, y, centred)`: the brush square around the map position, each side truncated toward zero; one tile
/// larger to the top left of the half-tile-shifted position when centred.
pub fn highlight(x: f32, y: f32, size: u32, centred: bool) -> [f32; 4] {
    let s = size as f32;
    let t = |v: f32| v as i32 as f32;
    if !centred {
        [t(x - s), t(y - s), t(x + s), t(y + s)]
    } else {
        let (x, y) = (x + 0.5, y + 0.5);
        [t(x - s - 1.0), t(y - s - 1.0), t(x + s), t(y + s)]
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::editor_players::{EE_SCREENS_SEPL_OP_MAKE, EE_SCREENS_SEPL_OP_RECT};

    #[derive(Default)]
    struct Rec {
        log: Vec<String>,
        next: usize,
        rects: Vec<(usize, [f32; 4])>,
    }

    impl SeplHost for Rec {
        fn op(&mut self, op: u32, p: [usize; 3], i: &mut [i32; 4], f: &mut [f32; 4]) -> i32 {
            match op {
                EE_SCREENS_SEPL_OP_MAKE => {
                    let c = unsafe { *(p[1] as *const EeScreensSeplCtl) };
                    self.next += 1;
                    self.rects.push((self.next, [c.x, c.y, c.x + 99.0, c.y + 11.0]));
                    self.log.push(format!("title {} ix {} groups {:#b} at {},{}", c.id, c.index, c.groups, c.x, c.y));
                    unsafe { *(p[0] as *mut usize) = self.next };
                }
                EE_SCREENS_SEPL_OP_RECT => *f = self.rects.iter().find(|r| r.0 == p[0]).map_or([0.0; 4], |r| r.1),
                EE_SCREENS_SETN_OP_BRUSH => {
                    self.next += 1;
                    self.rects.push((self.next, [f[0], f[1], f[0] + 99.0, f[1] + 19.0]));
                    self.log.push(format!("brush {i:?} at {},{} over {:#x}", f[0], f[1], p[2]));
                    unsafe { *(p[0] as *mut usize) = self.next };
                }
                _ => self.log.push(format!("{op} {} {}", p[0], i[0])),
            }
            0
        }
    }

    /// The title at the location in groups 2 and 3, the radios 20 high one under the other (sizes 0 .. 4, ids 2 .. 6,
    /// the strings from Tiny), each added to groups 2 and 3 as it is made but the tiny one, added last.
    #[test]
    fn build_lays_out_the_group() {
        let mut h = Rec::default();
        let mut index = 10;
        let c = build(&mut h, 0x77, 0x50, [140.0, 616.0], &mut index);
        assert_eq!(index, 16);
        assert_eq!(c, [1, 2, 3, 4, 5, 6]);
        assert_eq!(h.log[0], "title 7 ix 10 groups 0b1100 at 140,616");
        assert_eq!(h.log[1], "brush [0, 2, 20501, 11] at 140,628 over 0x77");
        assert_eq!(h.log[2], "brush [1, 3, 20502, 12] at 140,648 over 0x77");
        assert_eq!(h.log[3..5], ["1010 3 2".to_string(), "1010 3 3".to_string()]);
        assert_eq!(h.log[11], "brush [4, 6, 20505, 15] at 140,708 over 0x77");
        assert_eq!(h.log[14..], ["1010 2 2".to_string(), "1010 2 3".to_string()]);
    }

    /// The square: truncated toward zero (negative values up), centred one tile larger to the top left of the
    /// shifted position.
    #[test]
    fn highlight_squares() {
        assert_eq!(highlight(10.3, 20.7, 2, false), [8.0, 18.0, 12.0, 22.0]);
        assert_eq!(highlight(10.3, 20.7, 2, true), [7.0, 18.0, 12.0, 23.0]);
        assert_eq!(highlight(-3.6, 5.5, 1, true), [-5.0, 4.0, -2.0, 7.0]);
        assert_eq!(highlight(0.2, 0.2, 0, false), [0.0, 0.0, 0.0, 0.0]);
    }
}
