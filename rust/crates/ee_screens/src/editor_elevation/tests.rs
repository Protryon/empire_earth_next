use super::*;
use crate::editor_players::{EE_SCREENS_SEPL_OP_MAKE, EE_SCREENS_SEPL_OP_RECT};

/// A recording host: each made control a counted handle, its rectangle the record's (font height 11).
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
                let h = if c.flags & EE_SCREENS_SEPL_FONT_HEIGHT != 0 { 11.0 + c.h } else { c.h };
                let handle = 0x100 + self.next;
                self.rects.push((handle, [c.x, c.y, c.x + (c.w - 1.0), c.y + (h - 1.0)]));
                self.log.push(format!(
                    "make {:#x} ix {} at {},{} {}x{} flags {} bg {} on {}",
                    c.id, c.index, c.x, c.y, c.w, h, c.flags, c.bg, c.extra[0]
                ));
                unsafe { *(p[0] as *mut usize) = handle };
                return 0;
            }
            EE_SCREENS_SEPL_OP_RECT => {
                *f = self.rects.iter().find(|r| r.0 == p[0]).map_or([0.0; 4], |r| r.1);
                return 0;
            }
            EE_SCREENS_SEEL_OP_FONT_HEIGHT => return 11,
            EE_SCREENS_SEEL_OP_PAINT | EE_SCREENS_SEEL_OP_ELEVATION_SLIDER | EE_SCREENS_SEEL_OP_STRENGTH_SLIDER => {
                self.next += 1;
                self.log.push(format!("{op} ix {} {:?}", i[0], f));
                unsafe { *(p[0] as *mut usize) = 0x100 + self.next };
                return 0;
            }
            _ => {}
        }
        self.log.push(format!("{op} {:#x} {:?}", p[0], i));
        0
    }
}

/// The constructor: the paint area, the three radios one under the other (the first checked) with the tool event,
/// the titles, the strength label 732 - the font's height, both sliders between the title and the label, the
/// label's number, the sliders added and the strength one hidden; the indices from 38.
#[test]
fn constructor_lays_out_the_tab() {
    let mut h = Rec::default();
    let mut index = 38;
    let t = ElevationTab::new(&mut h, 0x50, 0x60, 0x70, &mut index);
    assert_eq!(index, 47);
    let log = h.log.join("\n");
    assert!(h.log[0].starts_with("900 ix 38"), "{log}");
    assert!(log.contains("make 0x96 ix 39 at 130,616 140x20"));
    assert!(log.contains("make 0x97 ix 40 at 130,636 140x20"));
    assert!(log.contains("make 0xa0 ix 41 at 130,656 140x20"));
    assert!(log.contains("on 1") && log.matches("on 0").count() >= 2);
    assert!(log.contains("make 0x98 ix 42 at 280,616 60x11"));
    assert!(log.contains("make 0x99 ix 43 at 280,627 60x11"));
    assert!(log.contains("make 0x9f ix 44 at 297,721 20x11"));
    // the sliders from 638 (the strength title's bottom + 1) to 721: 83 high
    assert!(log.contains("901 ix 45 [297.0, 638.0, 83.0, 0.0]"), "{log}");
    assert!(log.contains("902 ix 46 [297.0, 638.0, 83.0, 0.0]"));
    assert_eq!(log.matches(&format!("{} ", crate::editor_players::EE_SCREENS_SEPL_OP_SCREEN_EVENT)).count(), 3);
    assert!(h.log.last().unwrap().starts_with(&format!("904 {:#x} [1,", t.strength_slider)));
    h.log.clear();
    t.destroy(&mut h);
    assert_eq!(h.log.len(), 9);
}
