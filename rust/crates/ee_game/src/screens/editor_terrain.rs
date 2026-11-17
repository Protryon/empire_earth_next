//! The scenario editor's terrain tab whole in Rust (`EEUIFSETerrain`, once `Scenario Editor/EEUIFSETerrain.cpp`
//! 0x1ab2ab..0x1aca71): its constructor (the in-editor colour types and terrains, the terrain / bulldozer radios, the
//! titles, the type list with each terrain's colour set, the colour list, the paint area and the preview bitmap), the
//! destructor, `Enter` (the previews: an ambient-only terrain shows its first unit's button), `Leave`, `Update`,
//! `MouseMove` (the brush highlight), `SelectTerrain` (the load thread's loads and unloads), the type and colour
//! lists' events, the paint radios' `Check` (the terrain controls shown or hidden) and the paint area's
//! `CreatePaintAction`.
//!
//! The titles are the shared maker's records (`EeScreensSeplCtl`, the host `SeplMake`); the lists, the radios, the
//! paint area and the bitmap (classes only this tab makes), the tables and the load thread are the tab's host,
//! `ee_screenscpp_setn` (ops from 1000, `Scenario Editor/EEUIFSETerrain.cpp`). The list boxes' own draws stay C++.
//! The mouse and the highlight are the elevation tab's host ops (900..), a control by id the map tab's.

use core::sync::atomic::{AtomicU32, Ordering};

use crate::screens::editor_elevation::{EE_SCREENS_SEEL_OP_HIGHLIGHT, EE_SCREENS_SEEL_OP_MOUSE};
use crate::screens::editor_map::EE_SCREENS_SEMP_OP_CONTROL;
use crate::screens::editor_players::{
    rect, EeScreensSeplCtl, Maker, SeplHost, EE_SCREENS_SEPL_ALIGN_CENTER, EE_SCREENS_SEPL_FG,
    EE_SCREENS_SEPL_FONT_HEIGHT, EE_SCREENS_SEPL_HIDDEN, EE_SCREENS_SEPL_LABEL, EE_SCREENS_SEPL_OP_DELETE,
    EE_SCREENS_SEPL_OP_RADIO_IS_CHECKED, EE_SCREENS_SEPL_OP_SET_DISPLAY, EE_SCREENS_SEPL_SHOWN,
};

/// The in-editor tables into the buffer p0 (up to i1 entries; p0 0: none written) -> their count: i0 0 the colour
/// types (`EETerrainTypeTable`), 1 the terrains (`EETerrainTable`, sorted by their localized names without case).
pub const EE_SCREENS_SETN_OP_TABLES: u32 = 1000;
/// The terrain p0 -> its first unit class (-1: none); i0 its id, i1 its colour type, i2 ambients only, i3 its unit
/// class count; its preview texture into p1.
pub const EE_SCREENS_SETN_OP_TERRAIN: u32 = 1001;
/// The colour type p0 -> its id.
pub const EE_SCREENS_SETN_OP_COLOR_TYPE: u32 = 1002;
/// The colour type table of the id i0 (`EETerrainTypeTable::Get`) into p0.
pub const EE_SCREENS_SETN_OP_TYPE_TABLE: u32 = 1003;
/// A paint type radio (`EEUIFSETRBPaintType`, id i0, the string i1, the index i2, checked i3) at (f0, f1) on the
/// form p1, its label's background at 0.4, its text yellow -> into p0 (not added).
pub const EE_SCREENS_SETN_OP_RADIO: u32 = 1004;
/// The type list (`EESEUITerrainTypeLB`, id 0xcc) on the form p1 at (f0, f1), f2 x 85, the index i1: the i0 terrains
/// of p2[1] named, each with the colour set p2[2][k]; its event over the tab p2[0], its text yellow, the selection's
/// background clear, its own at 0.4, added to group 3 -> into p0.
pub const EE_SCREENS_SETN_OP_TYPE_LIST: u32 = 1005;
/// The colour list (`EESEUITerrainColorLB`, id 0xcf) on the form p1 at (f0, f1), f2 x 85, the index i1: the i0 colour
/// types of p2[1], each its colours; its event over the tab p2[0], its line i2 selected, the selection's background
/// clear, its own at 0.4, added to group 3 -> into p0.
pub const EE_SCREENS_SETN_OP_COLOR_LIST: u32 = 1006;
/// The paint area (`EEUIFSETerrainPaint`, id 205, 1024 x 606 at the top left) over the scroll p2[0], the brushes
/// p2[1] and the tab p2[2] on the form p1, the index i0, added to group 3 -> into p0.
pub const EE_SCREENS_SETN_OP_PAINT: u32 = 1007;
/// The preview bitmap (`UIBitmap`, id 0xc9, 64 x 64, a new `GEBitmap`) on the form p1 at (f0, f1), the index i0,
/// added to group 3 -> into p0.
pub const EE_SCREENS_SETN_OP_BITMAP: u32 = 1008;
/// The terrain previews loaded (i0 1) or released (0).
pub const EE_SCREENS_SETN_OP_PREVIEWS: u32 = 1009;
/// The control p0 added to the form p1's group i0.
pub const EE_SCREENS_SETN_OP_ADD: u32 = 1010;
/// -> the selected line of the type list (i0 0) or colour list (1) p0, -1: none.
pub const EE_SCREENS_SETN_OP_SELECTION: u32 = 1011;
/// The type list p0's line i0 given the texture p1 (the old one dereferenced, the new one referenced).
pub const EE_SCREENS_SETN_OP_SET_TEXTURE: u32 = 1012;
/// The unit class i0's button texture into p0 -> whether the class exists.
pub const EE_SCREENS_SETN_OP_CLASS_BUTTON: u32 = 1013;
/// -> whether the type list p0's line i0 has a texture.
pub const EE_SCREENS_SETN_OP_LINE_TEXTURE: u32 = 1014;
/// The bitmap p1 shows the type list p0's line i0's texture, stretched over the bitmap's rectangle.
pub const EE_SCREENS_SETN_OP_SHOW_PREVIEW: u32 = 1015;
/// The colour list p0's line i0 selected (no event).
pub const EE_SCREENS_SETN_OP_COLOR_SELECT: u32 = 1016;
/// The control p0's selection-changed event fired.
pub const EE_SCREENS_SETN_OP_FIRE: u32 = 1017;
/// The bitmap p0 marked dirty.
pub const EE_SCREENS_SETN_OP_DIRTY: u32 = 1018;
/// -> the colour count of the colour list p0's line i0.
pub const EE_SCREENS_SETN_OP_COLOR_COUNT: u32 = 1019;
/// The bitmap p1's corners: the colour list p0's line i0's colours 0, i1, i2 and i3.
pub const EE_SCREENS_SETN_OP_TINT: u32 = 1020;
/// The load thread of the form p0 asked to load (i0 0x408) or unload (0x409) the terrain id i1.
pub const EE_SCREENS_SETN_OP_POST: u32 = 1021;
/// The paint area p0's centring set to i3 (-1: kept); a new paint action of i0 (0 terrain with the terrain i1 and the
/// colour type i2, 1 the bulldozer, -1 none) into p1.
pub const EE_SCREENS_SETN_OP_PAINT_ACTION: u32 = 1022;

/// `EE_SCREENS_SEEL_OP_HIGHLIGHT`'s i0: the terrain tab's (the map point under the mouse, centred by i1).
pub const EE_SCREENS_SEEL_HIGHLIGHT_POINT: i32 = 1;

/// The tab's members (`ee_screens_setn_call`).
pub const EE_SCREENS_SETN_UPDATE: u32 = 1;
pub const EE_SCREENS_SETN_MOUSE_MOVE: u32 = 2;
pub const EE_SCREENS_SETN_ENTER: u32 = 3;
pub const EE_SCREENS_SETN_LEAVE: u32 = 4;
/// The type list's and the colour list's selection-changed events (the list at p).
pub const EE_SCREENS_SETN_TYPE_CHANGE: u32 = 5;
pub const EE_SCREENS_SETN_COLOR_CHANGE: u32 = 6;

/// The gate's reads (`ee_screens_setn_get`): the selected terrain, the terrains' count and ids, the colour types'.
pub const EE_SCREENS_SETN_GET_SELECTED: u32 = 1;
pub const EE_SCREENS_SETN_GET_TERRAIN_COUNT: u32 = 2;
pub const EE_SCREENS_SETN_GET_TERRAIN_ID: u32 = 3;
pub const EE_SCREENS_SETN_GET_COLOR_COUNT: u32 = 4;
pub const EE_SCREENS_SETN_GET_COLOR_ID: u32 = 5;

const IDS_SE_TERRAIN: i32 = 20508;
const IDS_BULLDOZER: i32 = 20510;
const IDS_TERRAIN_TYPE: u32 = 20511;
const IDS_TERRAIN_COLOR: u32 = 20542;

const ID_TERRAIN_BUTTON: u32 = 0xc8;
const ID_BITMAP: u32 = 0xc9;
const ID_MOUNTAINS_BUTTON: u32 = 0xca;
const ID_TYPE_LABEL: u32 = 0xcb;
const ID_TYPE_LIST: u32 = 0xcc;
const ID_COLOR_LABEL: u32 = 0xce;
const ID_COLOR_LIST: u32 = 0xcf;

const GROUP: u32 = 3;
const LOAD: i32 = 0x408;
const UNLOAD: i32 = 0x409;
const NONE: u32 = u32::MAX;

fn call<H: SeplHost + ?Sized>(
    h: &mut H,
    op: u32,
    p: [usize; 3],
    i: [i32; 4],
    f: [f32; 4],
) -> (i32, [i32; 4], [f32; 4]) {
    let (mut i, mut f) = (i, f);
    let r = h.op(op, p, &mut i, &mut f);
    (r, i, f)
}

fn op<H: SeplHost + ?Sized>(h: &mut H, op: u32, p: [usize; 3], i: [i32; 4]) -> i32 {
    call(h, op, p, i, [0.0; 4]).0
}

/// A control the host makes into its p0.
fn make<H: SeplHost + ?Sized>(h: &mut H, op: u32, p: [usize; 2], i: [i32; 4], f: [f32; 4]) -> usize {
    let mut out = 0usize;
    call(h, op, [&mut out as *mut usize as usize, p[0], p[1]], i, f);
    out
}

/// `UIControl::GetWidth`: the rectangle's width plus one, floored.
fn width(r: [f32; 4]) -> f32 {
    (r[2] - r[0] + 1.0).floor() as u32 as f32
}

/// A selection (`EE_SCREENS_SETN_OP_SELECTION`): `None` when the list has none.
fn selection<H: SeplHost + ?Sized>(h: &mut H, list: usize, colour: bool) -> Option<u32> {
    let s = op(h, EE_SCREENS_SETN_OP_SELECTION, [list, 0, 0], [i32::from(colour), 0, 0, 0]);
    (s >= 0).then_some(s as u32)
}

/// One terrain as the tab reads it.
#[derive(Debug, Default, Clone, Copy, PartialEq)]
struct Terrain {
    table: usize,
    id: i32,
    colour_type: i32,
}

/// The paint area's action (`CreatePaintAction`): none, a terrain (its id, the colour type's id), the bulldozer.
#[derive(Debug, Clone, Copy, PartialEq)]
pub enum PaintAction {
    None,
    Terrain(i32, i32),
    Bulldozer,
}

/// The terrain tab (`EEUIFSETerrain`). Every member but the selected terrain is set by the constructor; the events
/// run while another member is on the stack (the type list's event fires the colour list's), so they all take `&self`.
#[derive(Debug, Default)]
pub struct TerrainTab {
    form: usize,
    terrains: Vec<Terrain>,
    colours: Vec<(usize, i32)>,
    /// each terrain's colour set (the colour list's line)
    sets: Vec<i32>,
    terrain_button: usize,
    mountains_button: usize,
    type_label: usize,
    colour_label: usize,
    type_list: usize,
    colour_list: usize,
    paint: usize,
    bitmap: usize,
    selected: AtomicU32,
}

fn tables<H: SeplHost + ?Sized>(h: &mut H, kind: i32) -> Vec<usize> {
    let n = op(h, EE_SCREENS_SETN_OP_TABLES, [0; 3], [kind, 0, 0, 0]).max(0) as usize;
    let mut v = vec![0usize; n];
    op(h, EE_SCREENS_SETN_OP_TABLES, [v.as_mut_ptr() as usize, 0, 0], [kind, n as i32, 0, 0]);
    v
}

impl TerrainTab {
    /// The constructor on `form` (the C++ tab `tab`, the form's `scroll` and `brushes`) from the control index
    /// `*index` (advanced).
    pub fn new<H: SeplHost + ?Sized>(
        h: &mut H,
        tab: usize,
        form: usize,
        scroll: usize,
        brushes: usize,
        index: &mut u32,
    ) -> Self {
        let mut t = TerrainTab { form, ..Default::default() };
        for c in tables(h, 0) {
            let id = op(h, EE_SCREENS_SETN_OP_COLOR_TYPE, [c, 0, 0], [0; 4]);
            t.colours.push((c, id));
        }
        for table in tables(h, 1) {
            let (_, i, _) = call(h, EE_SCREENS_SETN_OP_TERRAIN, [table, 0, 0], [0; 4], [0.0; 4]);
            t.terrains.push(Terrain { table, id: i[0], colour_type: i[1] });
        }
        t.selected.store(0, Ordering::Relaxed);

        let radio = |h: &mut H, id: u32, string: i32, y: f32, ix: u32, on: bool| {
            make(
                h,
                EE_SCREENS_SETN_OP_RADIO,
                [form, 0],
                [id as i32, string, ix as i32, i32::from(on)],
                [115.0, y, 0.0, 0.0],
            )
        };
        t.terrain_button = radio(h, ID_TERRAIN_BUTTON, IDS_SE_TERRAIN, 621.0, *index, true);
        *index += 1;
        let y = rect(h, t.terrain_button)[3] + 5.0;
        t.mountains_button = radio(h, ID_MOUNTAINS_BUTTON, IDS_BULLDOZER, y, *index, false);
        *index += 1;
        op(h, EE_SCREENS_SETN_OP_ADD, [t.mountains_button, form, 0], [GROUP as i32, 0, 0, 0]);

        let label = |id: u32, string: u32, x: f32, w: f32| {
            EeScreensSeplCtl::new(EE_SCREENS_SEPL_LABEL, id, x, 621.0, w, 0.0)
                .string(string)
                .flags(EE_SCREENS_SEPL_FG | EE_SCREENS_SEPL_FONT_HEIGHT)
                .align(EE_SCREENS_SEPL_ALIGN_CENTER)
                .bg(0.4)
                .group(GROUP)
        };
        let mut m = Maker { h: &mut *h, form, index: *index };
        t.type_label = m.make(label(ID_TYPE_LABEL, IDS_TERRAIN_TYPE, 250.0, 200.0));
        t.colour_label = m.make(label(ID_COLOR_LABEL, IDS_TERRAIN_COLOR, 455.0, 120.0));
        *index = m.index;

        // each terrain's colour set: the colour list's line of its colour type (0 without one)
        let mut sets = vec![0i32; t.terrains.len()];
        let mut tables_of = Vec::with_capacity(t.terrains.len());
        for (k, terrain) in t.terrains.iter().enumerate() {
            tables_of.push(terrain.table);
            let mut ty = 0usize;
            op(
                h,
                EE_SCREENS_SETN_OP_TYPE_TABLE,
                [&mut ty as *mut usize as usize, 0, 0],
                [terrain.colour_type, 0, 0, 0],
            );
            if ty == 0 {
                continue;
            }
            if let Some(j) = t.colours.iter().position(|c| c.0 == ty) {
                sets[k] = j as i32;
            }
        }
        let r = rect(h, t.type_label);
        let p2 = [tab, tables_of.as_ptr() as usize, sets.as_ptr() as usize];
        t.type_list = make(
            h,
            EE_SCREENS_SETN_OP_TYPE_LIST,
            [form, p2.as_ptr() as usize],
            [t.terrains.len() as i32, *index as i32, 0, 0],
            [r[0], r[3] + 5.0, width(r), 0.0],
        );
        *index += 1;

        let r = rect(h, t.colour_label);
        let colour_tables: Vec<usize> = t.colours.iter().map(|c| c.0).collect();
        let p2 = [tab, colour_tables.as_ptr() as usize];
        t.colour_list = make(
            h,
            EE_SCREENS_SETN_OP_COLOR_LIST,
            [form, p2.as_ptr() as usize],
            [t.colours.len() as i32, *index as i32, sets.first().copied().unwrap_or(0), 0],
            [r[0], r[3] + 5.0, width(r), 0.0],
        );
        *index += 1;
        t.sets = sets;

        let p2 = [scroll, brushes, tab];
        t.paint = make(h, EE_SCREENS_SETN_OP_PAINT, [form, p2.as_ptr() as usize], [*index as i32, 0, 0, 0], [0.0; 4]);
        *index += 1;
        op(h, EE_SCREENS_SETN_OP_ADD, [t.terrain_button, form, 0], [GROUP as i32, 0, 0, 0]);
        let r = rect(h, t.colour_list);
        let at = [r[2] + 10.0, (r[3] - r[1]) * 0.5 + r[1] - 32.0, 0.0, 0.0];
        t.bitmap = make(h, EE_SCREENS_SETN_OP_BITMAP, [form, 0], [*index as i32, 0, 0, 0], at);
        *index += 1;
        op(h, EE_SCREENS_SETN_OP_PREVIEWS, [0; 3], [1, 0, 0, 0]);
        t
    }

    /// The destructor's deletes in the original's order, then the previews released.
    pub fn destroy<H: SeplHost + ?Sized>(&self, h: &mut H) {
        for c in [
            self.terrain_button,
            self.mountains_button,
            self.type_list,
            self.type_label,
            self.paint,
            self.colour_list,
            self.colour_label,
            self.bitmap,
        ] {
            if c != 0 {
                op(h, EE_SCREENS_SEPL_OP_DELETE, [c, 0, 0], [0; 4]);
            }
        }
        op(h, EE_SCREENS_SETN_OP_PREVIEWS, [0; 3], [0; 4]);
    }

    fn post<H: SeplHost + ?Sized>(&self, h: &mut H, message: i32, terrain: usize) {
        op(h, EE_SCREENS_SETN_OP_POST, [self.form, 0, 0], [message, self.terrains[terrain].id, 0, 0]);
    }

    /// `Enter`: every line's preview (an ambient-only terrain with unit classes: its first class's button, when the
    /// class exists), the selected one shown with its colours, the selected terrain loaded.
    pub fn enter<H: SeplHost + ?Sized>(&self, h: &mut H) {
        for (k, terrain) in self.terrains.iter().enumerate() {
            let mut preview = 0usize;
            let (first, i, _) = call(
                h,
                EE_SCREENS_SETN_OP_TERRAIN,
                [terrain.table, &mut preview as *mut usize as usize, 0],
                [0; 4],
                [0.0; 4],
            );
            if i[3] != 0 && i[2] != 0 {
                let mut button = 0usize;
                if i[3] > 0
                    && op(
                        h,
                        EE_SCREENS_SETN_OP_CLASS_BUTTON,
                        [&mut button as *mut usize as usize, 0, 0],
                        [first, 0, 0, 0],
                    ) != 0
                {
                    op(h, EE_SCREENS_SETN_OP_SET_TEXTURE, [self.type_list, button, 0], [k as i32, 0, 0, 0]);
                }
            } else {
                op(h, EE_SCREENS_SETN_OP_SET_TEXTURE, [self.type_list, preview, 0], [k as i32, 0, 0, 0]);
            }
        }
        if let Some(s) = selection(h, self.type_list, false) {
            if op(h, EE_SCREENS_SETN_OP_LINE_TEXTURE, [self.type_list, 0, 0], [s as i32, 0, 0, 0]) != 0 {
                op(h, EE_SCREENS_SETN_OP_SHOW_PREVIEW, [self.type_list, self.bitmap, 0], [s as i32, 0, 0, 0]);
                op(h, EE_SCREENS_SETN_OP_FIRE, [self.colour_list, 0, 0], [0; 4]);
            }
        }
        match selection(h, self.type_list, false) {
            Some(s) if (s as usize) < self.terrains.len() => {
                self.post(h, LOAD, s as usize);
                self.selected.store(s, Ordering::Relaxed);
            }
            _ => self.selected.store(NONE, Ordering::Relaxed),
        }
    }

    /// `Leave`: the selected terrain unloaded, every line's preview dropped.
    pub fn leave<H: SeplHost + ?Sized>(&self, h: &mut H) {
        if let Some(s) = selection(h, self.type_list, false) {
            if (s as usize) < self.terrains.len() {
                self.post(h, UNLOAD, s as usize);
            }
        }
        for k in 0..self.terrains.len() {
            op(h, EE_SCREENS_SETN_OP_SET_TEXTURE, [self.type_list, 0, 0], [k as i32, 0, 0, 0]);
        }
    }

    /// `SelectTerrain(line)`: the old terrain unloaded, the new one loaded (none out of range).
    pub fn select_terrain<H: SeplHost + ?Sized>(&self, h: &mut H, line: u32) {
        let old = self.selected.load(Ordering::Relaxed);
        if (old as usize) < self.terrains.len() {
            self.post(h, UNLOAD, old as usize);
        }
        if (line as usize) < self.terrains.len() {
            self.selected.store(line, Ordering::Relaxed);
            self.post(h, LOAD, line as usize);
        } else {
            self.selected.store(NONE, Ordering::Relaxed);
        }
    }

    /// The type list's event: the selected line's preview, its colour set selected (the colour list's event fired),
    /// then the line made the selected terrain.
    pub fn type_change<H: SeplHost + ?Sized>(&self, h: &mut H, list: usize) {
        let Some(s) = selection(h, list, false) else { return };
        if op(h, EE_SCREENS_SETN_OP_LINE_TEXTURE, [list, 0, 0], [s as i32, 0, 0, 0]) != 0 {
            op(h, EE_SCREENS_SETN_OP_SHOW_PREVIEW, [list, self.bitmap, 0], [s as i32, 0, 0, 0]);
            let set = self.sets.get(s as usize).copied().unwrap_or(0);
            op(h, EE_SCREENS_SETN_OP_COLOR_SELECT, [self.colour_list, 0, 0], [set, 0, 0, 0]);
            op(h, EE_SCREENS_SETN_OP_FIRE, [self.colour_list, 0, 0], [0; 4]);
            op(h, EE_SCREENS_SETN_OP_DIRTY, [self.bitmap, 0, 0], [0; 4]);
        }
        self.select_terrain(h, s);
    }

    /// The colour list's event: the preview's corners the selected set's first, one-third, two-thirds and last
    /// colours.
    pub fn colour_change<H: SeplHost + ?Sized>(&self, h: &mut H, list: usize) {
        let Some(s) = selection(h, list, true) else { return };
        let n = op(h, EE_SCREENS_SETN_OP_COLOR_COUNT, [list, 0, 0], [s as i32, 0, 0, 0]).max(0) as u32;
        if n > 0 {
            let third = if n > 2 { (n - 2) / 3 } else { 0 };
            op(
                h,
                EE_SCREENS_SETN_OP_TINT,
                [list, self.bitmap, 0],
                [s as i32, third as i32, (n - third - 1) as i32, (n - 1) as i32],
            );
            op(h, EE_SCREENS_SETN_OP_DIRTY, [self.bitmap, 0, 0], [0; 4]);
        }
    }

    /// `Update`: the highlight follows the mouse while the editor is the current form.
    pub fn update<H: SeplHost + ?Sized>(&self, h: &mut H) {
        let (current, _, f) = call(h, EE_SCREENS_SEEL_OP_MOUSE, [self.form, 0, 0], [0; 4], [0.0; 4]);
        if current != 0 {
            self.mouse_move(h, [f[0], f[1]]);
        }
    }

    /// `MouseMove(location)`: the brush highlight at the map point under it inside the game view (centred unless
    /// the bulldozer is checked), else hidden.
    pub fn mouse_move<H: SeplHost + ?Sized>(&self, h: &mut H, at: [f32; 2]) {
        let bulldozer = op(h, EE_SCREENS_SEPL_OP_RADIO_IS_CHECKED, [self.mountains_button, 0, 0], [0; 4]) != 0;
        call(
            h,
            EE_SCREENS_SEEL_OP_HIGHLIGHT,
            [self.form, 0, 0],
            [EE_SCREENS_SEEL_HIGHLIGHT_POINT, i32::from(!bulldozer), 0, 0],
            [at[0], at[1], 0.0, 0.0],
        );
    }

    /// `CreatePaintAction`'s choice: the terrain radio checked, a terrain with its colour type (the selected one, else
    /// the terrain's own type used as a line, an id 0 past the list's end; none without a selected terrain), else the
    /// bulldozer.
    pub fn paint_action<H: SeplHost + ?Sized>(&self, h: &mut H) -> (PaintAction, Option<bool>) {
        if op(h, EE_SCREENS_SEPL_OP_RADIO_IS_CHECKED, [self.terrain_button, 0, 0], [0; 4]) == 0 {
            return (PaintAction::Bulldozer, Some(false));
        }
        let Some(ty) = selection(h, self.type_list, false) else { return (PaintAction::None, None) };
        let Some(terrain) = self.terrains.get(ty as usize) else { return (PaintAction::None, None) };
        let colour = selection(h, self.colour_list, true).unwrap_or(terrain.colour_type as u32);
        let colour_id = self.colours.get(colour as usize).map_or(0, |c| c.1);
        (PaintAction::Terrain(terrain.id, colour_id), Some(true))
    }

    /// `CreatePaintAction` on the paint area `paint` -> the new action (0: none).
    pub fn create_paint_action<H: SeplHost + ?Sized>(&self, h: &mut H, paint: usize) -> usize {
        let (action, centred) = self.paint_action(h);
        let (kind, a, b) = match action {
            PaintAction::None => (-1, 0, 0),
            PaintAction::Terrain(t, c) => (0, t, c),
            PaintAction::Bulldozer => (1, 0, 0),
        };
        let mut out = 0usize;
        let centred = centred.map_or(-1, i32::from);
        op(h, EE_SCREENS_SETN_OP_PAINT_ACTION, [paint, &mut out as *mut usize as usize, 0], [kind, a, b, centred]);
        out
    }

    /// The gate's reads (`EE_SCREENS_SETN_GET_*`).
    pub fn get(&self, what: u32, k: u32) -> i32 {
        match what {
            EE_SCREENS_SETN_GET_SELECTED => self.selected.load(Ordering::Relaxed) as i32,
            EE_SCREENS_SETN_GET_TERRAIN_COUNT => self.terrains.len() as i32,
            EE_SCREENS_SETN_GET_TERRAIN_ID => self.terrains.get(k as usize).map_or(-1, |t| t.id),
            EE_SCREENS_SETN_GET_COLOR_COUNT => self.colours.len() as i32,
            EE_SCREENS_SETN_GET_COLOR_ID => self.colours.get(k as usize).map_or(-1, |c| c.1),
            _ => 0,
        }
    }
}

/// The paint type radios' `Check` (after `UIRadioButton::Check`): the terrain radio shows the type and colour lists,
/// their titles and the preview, the bulldozer hides them.
pub fn paint_type_check<H: SeplHost + ?Sized>(h: &mut H, form: usize, id: u32) {
    let mode = match id {
        ID_TERRAIN_BUTTON => EE_SCREENS_SEPL_SHOWN,
        ID_MOUNTAINS_BUTTON => EE_SCREENS_SEPL_HIDDEN,
        _ => return,
    };
    for c in [ID_TYPE_LABEL, ID_TYPE_LIST, ID_COLOR_LABEL, ID_COLOR_LIST, ID_BITMAP] {
        let mut control = 0usize;
        op(h, EE_SCREENS_SEMP_OP_CONTROL, [form, &mut control as *mut usize as usize, 0], [c as i32, 0, 0, 0]);
        op(h, EE_SCREENS_SEPL_OP_SET_DISPLAY, [control, 0, 0], [mode, 0, 0, 0]);
    }
}

#[cfg(test)]
mod tests;
