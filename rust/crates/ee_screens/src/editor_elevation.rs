//! The scenario editor's elevation tab whole in Rust (`EEUIFSEElevation`, once `Scenario Editor/EEUIFSEElevation.cpp`
//! 0x1a098e..0x1a1397): its constructor (the paint area, the hills / cliffs / cliff paths radio buttons and their
//! tool event, the titles, the strength label, the elevation and strength sliders), the destructor, `Update` and
//! `MouseMove` (the brush highlight at the tile under the mouse).
//!
//! The radio buttons and labels are the shared maker's records (`EeScreensSeplCtl`, the host `SeplMake`); what only
//! this tab makes (the paint area `EEUIFSEPaintElevation`, the elevation slider `EEUIFSEESElevation`, the strength
//! slider and its label event) and the world it reads (the mouse, the view, the tile under the mouse, the brush
//! group) are the tab's host, `ee_screenscpp_seel` (ops from 900, `Scenario Editor/EEUIFSEElevation.cpp`).

use crate::editor_players::{
    rect, EeScreensSeplCtl, Maker, SeplHost, EE_SCREENS_SEPL_ALIGN_CENTER, EE_SCREENS_SEPL_FG, EE_SCREENS_SEPL_FONT_534,
    EE_SCREENS_SEPL_FONT_HEIGHT, EE_SCREENS_SEPL_LABEL, EE_SCREENS_SEPL_OP_DELETE, EE_SCREENS_SEPL_OP_LABEL_NUMBER,
    EE_SCREENS_SEPL_OP_SCREEN_EVENT, EE_SCREENS_SEPL_RADIO,
};

/// `EeScreensSeplCtl::flags`: a radio button's or check box's background opacity (`bg`) is its text label's.
pub const EE_SCREENS_SEPL_LABEL_BG: u32 = 1024;

/// The paint area (`EEUIFSEPaintElevation`, id 0xa1, 0x400 x 0x25e at the top left, over the edge scroll and brush
/// group, the two words at p2) at the index i0; group 2 -> into p0.
pub const EE_SCREENS_SEEL_OP_PAINT: u32 = 900;
/// The elevation slider (`EEUIFSEESElevation`, id 0x9d) at (f0, f1), f2 high, the index i0, 0 .. 0x40 at 0x28,
/// page 1, grey at 0.4 -> into p0; its value's strength (max - value - 0x18) returned.
pub const EE_SCREENS_SEEL_OP_ELEVATION_SLIDER: u32 = 901;
/// The strength slider (id 0xa2) at (f0, f1), 20 x f2, the index i0, 0 .. 0x10 at 10, page 1, grey at 0.4, its
/// changes shown in the label p2 (`UIESliderChange(-4, 0x28)`) -> into p0.
pub const EE_SCREENS_SEEL_OP_STRENGTH_SLIDER: u32 = 902;
/// -> the screen font's height.
pub const EE_SCREENS_SEEL_OP_FONT_HEIGHT: u32 = 903;
/// The control p0 added to the form p1's group 2 / hidden.
pub const EE_SCREENS_SEEL_OP_ADD: u32 = 904;
/// -> whether the form p0 is its manager's current form; the mouse's position into f0, f1.
pub const EE_SCREENS_SEEL_OP_MOUSE: u32 = 905;
/// The brush highlight of the form p0 at the tile under (f0, f1) when the view holds it (shown), else hidden.
pub const EE_SCREENS_SEEL_OP_HIGHLIGHT: u32 = 906;

/// The tab's members (`ee_screens_seel_call`).
pub const EE_SCREENS_SEEL_UPDATE: u32 = 1;
pub const EE_SCREENS_SEEL_MOUSE_MOVE: u32 = 2;

const IDS_PAINT_HILLS: u32 = 30465;
const IDS_PAINT_CLIFFS: u32 = 30466;
const IDS_PAINT_CLIFF_PATHS: u32 = 30467;
const IDS_SE_MAP_TERRAIN: u32 = 20018;
const IDS_SE_MAP_ELEVATION: u32 = 20019;

const ID_RAISE: u32 = 0x96;
const ID_LOWER: u32 = 0x97;
const ID_LEVEL: u32 = 0xa0;
const ID_ELEVATION_TITLE: u32 = 0x98;
const ID_STRENGTH_TITLE: u32 = 0x99;
const ID_STRENGTH_LABEL: u32 = 0x9f;

const GROUP: u32 = 2;
const SCREEN_EVENT_ON_CHECKED: i32 = 1;

fn call<H: SeplHost + ?Sized>(h: &mut H, op: u32, p: [usize; 3], i: [i32; 4], f: [f32; 4]) -> (i32, [f32; 4]) {
    let (mut i, mut f) = (i, f);
    let r = h.op(op, p, &mut i, &mut f);
    (r, f)
}

fn make<H: SeplHost + ?Sized>(h: &mut H, op: u32, form: usize, p2: usize, index: u32, f: [f32; 4]) -> (usize, i32) {
    let mut out = 0usize;
    let r = call(h, op, [&mut out as *mut usize as usize, form, p2], [index as i32, 0, 0, 0], f).0;
    (out, r)
}

/// One control of the shared maker at `*index` (advanced).
fn mk<H: SeplHost + ?Sized>(h: &mut H, form: usize, index: &mut u32, c: EeScreensSeplCtl) -> usize {
    let mut m = Maker { h, form, index: *index };
    let r = m.make(c);
    *index = m.index;
    r
}

/// The elevation tab (`EEUIFSEElevation`).
#[derive(Debug, Default)]
pub struct ElevationTab {
    form: usize,
    paint: usize,
    raise: usize,
    lower: usize,
    level: usize,
    elevation_title: usize,
    strength_title: usize,
    strength_label: usize,
    elevation_slider: usize,
    strength_slider: usize,
}

impl ElevationTab {
    /// The constructor on `form` from the control index `index` (advanced); `scroll` and `brushes` the form's.
    pub fn new<H: SeplHost + ?Sized>(h: &mut H, form: usize, scroll: usize, brushes: usize, index: &mut u32) -> Self {
        let mut t = ElevationTab { form, ..Default::default() };
        let pair = [scroll, brushes];
        t.paint = make(h, EE_SCREENS_SEEL_OP_PAINT, form, pair.as_ptr() as usize, *index, [0.0; 4]).0;
        *index += 1;

        let radio = |id: u32, string: u32, y: f32, on: bool| {
            EeScreensSeplCtl::new(EE_SCREENS_SEPL_RADIO, id, 130.0, y, 140.0, 20.0)
                .font(EE_SCREENS_SEPL_FONT_534)
                .string(string)
                .flags(EE_SCREENS_SEPL_FG | EE_SCREENS_SEPL_LABEL_BG)
                .bg(0.4)
                .extra([i32::from(on), 0, 0, 0])
                .group(GROUP)
        };
        let mut y = 616.0;
        for (k, (id, string)) in [(ID_RAISE, IDS_PAINT_HILLS), (ID_LOWER, IDS_PAINT_CLIFFS), (ID_LEVEL, IDS_PAINT_CLIFF_PATHS)]
            .into_iter()
            .enumerate()
        {
            let c = mk(h, form, index, radio(id, string, y, k == 0));
            call(h, EE_SCREENS_SEPL_OP_SCREEN_EVENT, [c, 0, 0], [crate::scenario_editor::EE_SCREENS_SE_ELEVATION_TOOL as i32, SCREEN_EVENT_ON_CHECKED, 0, 0], [0.0; 4]);
            y = rect(h, c)[3] + 1.0;
            match k {
                0 => t.raise = c,
                1 => t.lower = c,
                _ => t.level = c,
            }
        }

        let label = |id: u32, string: u32, x: f32, y: f32, w: f32| {
            EeScreensSeplCtl::new(EE_SCREENS_SEPL_LABEL, id, x, y, w, 0.0)
                .string(string)
                .flags(EE_SCREENS_SEPL_FG | EE_SCREENS_SEPL_FONT_HEIGHT)
                .align(EE_SCREENS_SEPL_ALIGN_CENTER)
                .bg(0.4)
                .group(GROUP)
        };
        t.elevation_title = mk(h, form, index, label(ID_ELEVATION_TITLE, IDS_SE_MAP_TERRAIN, 280.0, 616.0, 60.0));
        let y = rect(h, t.elevation_title)[3] + 1.0;
        t.strength_title = mk(h, form, index, label(ID_STRENGTH_TITLE, IDS_SE_MAP_ELEVATION, 280.0, y, 60.0));
        let font = call(h, EE_SCREENS_SEEL_OP_FONT_HEIGHT, [0; 3], [0; 4], [0.0; 4]).0;
        t.strength_label = mk(h, form, index, label(ID_STRENGTH_LABEL, 0, 297.0, 732.0 - font as f32, 20.0));

        let top = rect(h, t.strength_title)[3] + 1.0;
        let bottom = rect(h, t.strength_label)[1];
        let (slider, strength) = make(h, EE_SCREENS_SEEL_OP_ELEVATION_SLIDER, form, 0, *index, [297.0, top, (bottom - top) as u32 as f32, 0.0]);
        t.elevation_slider = slider;
        *index += 1;
        t.strength_slider = make(h, EE_SCREENS_SEEL_OP_STRENGTH_SLIDER, form, t.strength_label, *index, [297.0, top, bottom - top, 0.0]).0;
        *index += 1;
        call(h, EE_SCREENS_SEPL_OP_LABEL_NUMBER, [t.strength_label, 0, 0], [strength, 1, 0, 0], [0.0; 4]);
        call(h, EE_SCREENS_SEEL_OP_ADD, [t.strength_slider, form, 0], [0; 4], [0.0; 4]);
        call(h, EE_SCREENS_SEEL_OP_ADD, [t.elevation_slider, form, 0], [0; 4], [0.0; 4]);
        call(h, EE_SCREENS_SEEL_OP_ADD, [t.strength_slider, 0, 0], [1, 0, 0, 0], [0.0; 4]);
        t
    }

    /// The destructor's deletes, in the original's order.
    pub fn destroy<H: SeplHost + ?Sized>(&self, h: &mut H) {
        for c in [
            self.paint,
            self.raise,
            self.lower,
            self.level,
            self.elevation_slider,
            self.strength_label,
            self.elevation_title,
            self.strength_title,
            self.strength_slider,
        ] {
            if c != 0 {
                call(h, EE_SCREENS_SEPL_OP_DELETE, [c, 0, 0], [0; 4], [0.0; 4]);
            }
        }
    }

    /// `Update`: the highlight follows the mouse while the editor is the current form.
    pub fn update<H: SeplHost + ?Sized>(&self, h: &mut H) {
        let (current, f) = call(h, EE_SCREENS_SEEL_OP_MOUSE, [self.form, 0, 0], [0; 4], [0.0; 4]);
        if current != 0 {
            self.mouse_move(h, [f[0], f[1]]);
        }
    }

    /// `MouseMove(location)`: the brush highlight on the tile under it inside the game view, else hidden.
    pub fn mouse_move<H: SeplHost + ?Sized>(&self, h: &mut H, at: [f32; 2]) {
        call(h, EE_SCREENS_SEEL_OP_HIGHLIGHT, [self.form, 0, 0], [0; 4], [at[0], at[1], 0.0, 0.0]);
    }
}

#[cfg(test)]
mod tests;
