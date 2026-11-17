//! The scenario editor's map tab whole in Rust (`EEUIFSEMap`, once `Scenario Editor/EEUIFSEMap.cpp`
//! 0x1a18f1..0x1a3f10): its constructor (the size, terrain, style and seed controls, the kind radio buttons, the
//! create button, the elevation file and its four boxes), the destructor, `CreateMap`, `ShowElevationControls`,
//! `Enter`, and the kind radio buttons' `Check` and `HideControls` (`UIRBMap`, by control id on the form).
//!
//! The controls are the shared maker's records (`EeScreensSeplCtl`, the host `SeplMake`); what only this tab makes
//! (the kind radio buttons `UIRBMap`, the create button `UIBMap`, the elevation file's `EEUIFileComboBox`) and the
//! world it reads and posts to (the editor's terrains, the random map generators, the rules' map sizes, the blank
//! and random map actions, the user interface's seed) are the tab's host, `ee_screenscpp_semp` (ops from 500,
//! `Scenario Editor/EEUIFSEMap.cpp`). The tab is only made and called from the UI thread; a host call does not
//! come back into it.

use crate::screens::editor_object::EE_SCREENS_SEPL_COMBO_PLAIN;
use crate::screens::editor_players::{
    display, rect, EeScreensSeplCtl, Maker, SeplHost, EE_SCREENS_SEPL_ALIGN_CENTER, EE_SCREENS_SEPL_EDIT,
    EE_SCREENS_SEPL_FG, EE_SCREENS_SEPL_FONT_HEIGHT, EE_SCREENS_SEPL_LABEL, EE_SCREENS_SEPL_LIST_COMBO,
    EE_SCREENS_SEPL_OP_COMBO_SELECTION, EE_SCREENS_SEPL_OP_DELETE, EE_SCREENS_SEPL_OP_EDIT_ATOI,
    EE_SCREENS_SEPL_OP_LABEL_NUMBER, EE_SCREENS_SEPL_OP_LIST_SELECTION, EE_SCREENS_SEPL_OP_RADIO_IS_CHECKED,
    EE_SCREENS_SEPL_OP_SET_LOCATION, EE_SCREENS_SEPL_TEXT_NUMBER,
};

// ---- the shared maker's additions (`SeplMake`, `ee_screenscpp_sepl`)

/// `EeScreensSeplCtl::flags`: the text white (and transparent) rather than yellow.
pub const EE_SCREENS_SEPL_FG_WHITE: u32 = 64;
/// A combo box's lines (`extra[1]`): the lines pushed before it (`EE_SCREENS_SEPL_OP_LINE_PUSH`), taken.
pub const EE_SCREENS_SEPL_COMBO_LINES_PENDING: i32 = 3;
/// The control p0's help string set to i0 (`SetStringTableID`).
pub const EE_SCREENS_SEPL_OP_HELP: u32 = 65;
/// The edit box p0's text set to the i0 bytes at p1.
pub const EE_SCREENS_SEPL_OP_EDIT_BYTES: u32 = 66;
/// A line (i0 as `EE_SCREENS_SEPL_TEXT_*`, i1 the string, i2 the number) pushed for the next combo box made with
/// `EE_SCREENS_SEPL_COMBO_LINES_PENDING`.
pub const EE_SCREENS_SEPL_OP_LINE_PUSH: u32 = 67;

// ---- the tab's host (`ee_screenscpp_semp`)

/// -> the terrain tables' count (`EETerrainTable::GetNumTables`) / whether table i0 is one of the editor's (not
/// NULL, `mIsInEditor`), its id into i1 and its name string into i2.
pub const EE_SCREENS_SEMP_OP_TERRAIN_COUNT: u32 = 500;
pub const EE_SCREENS_SEMP_OP_TERRAIN: u32 = 501;
/// The random map generators' display names / the rules' map sizes' names pushed as the next combo box's lines.
pub const EE_SCREENS_SEMP_OP_LINES_GENERATORS: u32 = 502;
pub const EE_SCREENS_SEMP_OP_LINES_MAP_SIZES: u32 = 503;
/// -> the kind radio buttons' height: the form p1's manager's radio bitmap's, at least the screen font's.
pub const EE_SCREENS_SEMP_OP_RADIO_HEIGHT: u32 = 504;
/// A kind radio button (`UIRBMap`) made on the form p1: id i0, the string i1, the index i2, checked i3, at
/// (f0, f1), f2 high; its label's opacity 0.4 and text yellow; group 1 -> its handle into p0.
pub const EE_SCREENS_SEMP_OP_MAKE_RADIO: u32 = 505;
/// The create button (`UIBMap`, id 0x65, 260 wide, the font's height + 10 high, over the tab p2) made on the form
/// p1 at the index i2, at (f0, f1); opacity 1; group 1 -> into p0.
pub const EE_SCREENS_SEMP_OP_MAKE_BUTTON: u32 = 506;
/// The create button p0's text ("Create Map" in the screen font, white).
pub const EE_SCREENS_SEMP_OP_BUTTON_TEXT: u32 = 507;
/// The elevation file box (`EEUIFileComboBox` over `data\Scenarios\Elevation Maps\*.dat`, id 0x75, 200 wide, its
/// selection's `EESEERust` map type event over the tab p2, its help string) made on the form p1 at the index i2, at
/// (f0, f1); group 1 -> into p0.
pub const EE_SCREENS_SEMP_OP_MAKE_FILE_COMBO: u32 = 508;
/// The text label's rectangle (left, top, right, bottom into f) of the combo box p0 (i0 = 0) or the radio button
/// p0 (i0 = 1).
pub const EE_SCREENS_SEMP_OP_LABEL_RECT: u32 = 509;
/// -> the combo box p0's label's first line as `atoi` reads it.
pub const EE_SCREENS_SEMP_OP_COMBO_ATOI: u32 = 510;
/// The blank map action (width i0, height i1, terrain id i2) / the random map action (size i0, style i1) made.
pub const EE_SCREENS_SEMP_OP_BLANK_MAP: u32 = 511;
pub const EE_SCREENS_SEMP_OP_RANDOM_MAP: u32 = 512;
/// -> a new seed (`URandomIntegers` seeded with the time, 0..1000000) given to the user interface / the user
/// interface's seed set to i0.
pub const EE_SCREENS_SEMP_OP_SEED_DRAW: u32 = 513;
pub const EE_SCREENS_SEMP_OP_SEED_SET: u32 = 514;
/// The form p0's control i0 (`GetControlById`) -> into p1.
pub const EE_SCREENS_SEMP_OP_CONTROL: u32 = 515;
/// The control p0's selection event processed (with no form).
pub const EE_SCREENS_SEMP_OP_PROCESS_SELECTION: u32 = 516;
/// The file combo box p0's entries made again (`CreateEntries`).
pub const EE_SCREENS_SEMP_OP_FILE_ENTRIES: u32 = 517;

/// The tab's members (`ee_screens_semp_call`): `Enter`, `CreateMap`, `ShowElevationControls(a != 0)`.
pub const EE_SCREENS_SEMP_ENTER: u32 = 1;
pub const EE_SCREENS_SEMP_CREATE_MAP: u32 = 2;
pub const EE_SCREENS_SEMP_SHOW_ELEVATION: u32 = 3;

// the language's strings (`ee/EEStringIDs.h`; a unit test reads them)
const IDS_MAP_WIDTH: u32 = 20013;
const IDS_SE_MAP_MAP_HEIGHT: u32 = 20001;
const IDS_DEFAULT_TERRAIN: u32 = 20008;
const IDS_SE_MAP_MAP_TYPE: u32 = 20002;
const IDS_BLANK: u32 = 20005;
const IDS_RANDOM: u32 = 20006;
const IDS_SEED: u32 = 20007;
const IDS_INGAME_MAP_SIZE: u32 = 30613;
const IDS_ELEVATION_FILE: u32 = 30482;
const IDS_WATER_DEPTH: u32 = 30484;
const IDS_WATER_CUTOFF: u32 = 30483;
const IDS_LAND_HEIGHT: u32 = 30485;
const IDS_LAND_CUTOFF: u32 = 30486;
const IDS_THIS_IS_MAXIMUM_DEPTH_THAT_WATER: u32 = 11010;
const IDS_THIS_IS_DEPTH_IN_METERS: u32 = 11007;
const IDS_THIS_IS_MAXIMUM_HEIGHT_THAT_LAND: u32 = 11008;
const IDS_THIS_IS_HEIGHT_IN_METERS: u32 = 11009;

// the controls' ids (`kSEMap*`)
const ID_DEFAULT_TERRAIN_LABEL: u32 = 0x64;
const ID_WIDTH_LABEL: u32 = 0x66;
const ID_HEIGHT_LABEL: u32 = 0x67;
const ID_MAP_STYLE_LABEL: u32 = 0x68;
const ID_DEFAULT_TERRAIN: u32 = 0x69;
const ID_MAP_STYLE: u32 = 0x6a;
const ID_WIDTH: u32 = 0x6b;
const ID_HEIGHT: u32 = 0x6c;
const ID_SEED_RADIO: u32 = 0x6d;
const ID_BLANK_RADIO: u32 = 0x6e;
const ID_RANDOM_RADIO: u32 = 0x6f;
const ID_SEED_EDIT: u32 = 0x70;
const ID_SEED_LABEL: u32 = 0x71;
const ID_RANDOM_SIZE: u32 = 0x72;
const ID_RANDOM_SIZE_LABEL: u32 = 0x73;
const ID_ELEVATION_FILE_LABEL: u32 = 0x74;
const ID_ELEVATION_FILE: u32 = 0x75;
const ID_LAND_CUTOFF_LABEL: u32 = 0x76;
const ID_LAND_HEIGHT_LABEL: u32 = 0x77;
const ID_LAND_CUTOFF: u32 = 0x78;
const ID_LAND_HEIGHT: u32 = 0x79;
const ID_WATER_CUTOFF_LABEL: u32 = 0x7a;
const ID_WATER_DEPTH_LABEL: u32 = 0x7b;
const ID_WATER_DEPTH: u32 = 0x7c;
const ID_WATER_CUTOFF: u32 = 0x7d;

/// The map sizes the width and height combo boxes offer (0x4c4f40).
pub const MAP_SIZES: [i32; 25] = [
    15, 25, 50, 60, 70, 75, 80, 90, 100, 125, 130, 135, 140, 145, 150, 155, 160, 165, 175, 200, 225, 250, 300, 350, 400,
];

/// The tab's control group.
const GROUP: u32 = 1;

/// `UIRBMap::HideControls`' controls, in its order.
const MODE_CONTROLS: [u32; 22] = [
    ID_DEFAULT_TERRAIN_LABEL,
    ID_DEFAULT_TERRAIN,
    ID_MAP_STYLE_LABEL,
    ID_MAP_STYLE,
    ID_SEED_LABEL,
    ID_SEED_EDIT,
    ID_WIDTH,
    ID_HEIGHT,
    ID_WIDTH_LABEL,
    ID_HEIGHT_LABEL,
    ID_RANDOM_SIZE,
    ID_RANDOM_SIZE_LABEL,
    ID_ELEVATION_FILE,
    ID_ELEVATION_FILE_LABEL,
    ID_WATER_CUTOFF_LABEL,
    ID_WATER_DEPTH_LABEL,
    ID_LAND_CUTOFF_LABEL,
    ID_LAND_HEIGHT_LABEL,
    ID_LAND_CUTOFF,
    ID_LAND_HEIGHT,
    ID_WATER_DEPTH,
    ID_WATER_CUTOFF,
];

/// What a kind radio button's `Check` shows, in its order (the blank kind then runs the elevation file's event).
pub fn kind_controls(id: u32) -> &'static [u32] {
    match id {
        ID_RANDOM_RADIO => &[ID_MAP_STYLE_LABEL, ID_MAP_STYLE, ID_SEED_LABEL, ID_RANDOM_SIZE, ID_RANDOM_SIZE_LABEL],
        ID_BLANK_RADIO => &[
            ID_DEFAULT_TERRAIN_LABEL,
            ID_DEFAULT_TERRAIN,
            ID_WIDTH,
            ID_HEIGHT,
            ID_WIDTH_LABEL,
            ID_HEIGHT_LABEL,
            ID_ELEVATION_FILE,
            ID_ELEVATION_FILE_LABEL,
        ],
        ID_SEED_RADIO => &[ID_MAP_STYLE_LABEL, ID_MAP_STYLE, ID_SEED_EDIT, ID_RANDOM_SIZE, ID_RANDOM_SIZE_LABEL],
        _ => &[],
    }
}

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

fn op<H: SeplHost + ?Sized>(h: &mut H, op: u32, p0: usize, a: i32, b: i32) -> i32 {
    call(h, op, [p0, 0, 0], [a, b, 0, 0], [0.0; 4]).0
}

/// A control made by the host's op `what` (`EE_SCREENS_SEMP_OP_MAKE_*`): its handle.
fn make_own<H: SeplHost + ?Sized>(h: &mut H, what: u32, form: usize, shell: usize, i: [i32; 4], f: [f32; 4]) -> usize {
    let mut out = 0usize;
    call(h, what, [&mut out as *mut usize as usize, form, shell], i, f);
    out
}

/// `UIControl::GetWidth`: the rectangle's width + 1, floored.
fn control_width(r: [f32; 4]) -> u32 {
    ((r[2] - r[0]) + 1.0).floor() as u32
}

/// A yellow label in the screen font, centred, `h` high (0: the font's height).
fn label(id: u32, string: u32, x: f32, y: f32, w: f32, h: f32) -> EeScreensSeplCtl {
    let c = EeScreensSeplCtl::new(EE_SCREENS_SEPL_LABEL, id, x, y, w, h)
        .string(string)
        .flags(EE_SCREENS_SEPL_FG)
        .align(EE_SCREENS_SEPL_ALIGN_CENTER)
        .bg(0.4)
        .group(GROUP);
    if h == 0.0 {
        c.flags(EE_SCREENS_SEPL_FONT_HEIGHT)
    } else {
        c
    }
}

/// A combo box of the pushed lines, `w` x 20, its list 0x180 high and always dirty, its text box at 0.4.
fn combo(id: u32, x: f32, y: f32, w: f32) -> EeScreensSeplCtl {
    EeScreensSeplCtl::new(EE_SCREENS_SEPL_LIST_COMBO, id, x, y, w, 20.0)
        .bg2(0.4)
        .flags(EE_SCREENS_SEPL_FG)
        .list(0x180, 0xff)
        .extra([EE_SCREENS_SEPL_COMBO_PLAIN, EE_SCREENS_SEPL_COMBO_LINES_PENDING, 0, 1])
        .group(GROUP)
}

/// A white number box of six characters, `w` x `h` (`h` 0: the font's height + 4), its text `text` ("0" or none).
fn number_box(id: u32, x: f32, y: f32, w: f32, h: f32, zero: bool) -> EeScreensSeplCtl {
    let mut c = EeScreensSeplCtl::new(EE_SCREENS_SEPL_EDIT, id, x, y, w, h)
        .bg(1.0)
        .flags(EE_SCREENS_SEPL_FG_WHITE)
        .extra([6, 1, 0, 0])
        .group(GROUP);
    if zero {
        c = c.text(EE_SCREENS_SEPL_TEXT_NUMBER, 0, 0);
    }
    c
}

fn line_push<H: SeplHost + ?Sized>(h: &mut H, kind: u32, string: u32, number: i32) {
    call(h, EE_SCREENS_SEPL_OP_LINE_PUSH, [0; 3], [kind as i32, string as i32, number, 0], [0.0; 4]);
}

fn set_location<H: SeplHost + ?Sized>(h: &mut H, control: usize, x: f32, y: f32) {
    call(h, EE_SCREENS_SEPL_OP_SET_LOCATION, [control, 0, 0], [0; 4], [x, y, 0.0, 0.0]);
}

fn label_rect<H: SeplHost + ?Sized>(h: &mut H, control: usize, radio: bool) -> [f32; 4] {
    call(h, EE_SCREENS_SEMP_OP_LABEL_RECT, [control, 0, 0], [radio as i32, 0, 0, 0], [0.0; 4]).2
}

/// The map tab (`EEUIFSEMap`): its controls and the editor's terrains (their ids, the default terrain box's lines).
#[derive(Debug, Default, Clone, PartialEq)]
pub struct MapTab {
    pub create_map: usize,
    pub default_terrain_label: usize,
    pub random_size_label: usize,
    pub width_label: usize,
    pub height_label: usize,
    pub map_style_label: usize,
    pub default_terrain: usize,
    pub map_style: usize,
    pub width: usize,
    pub height: usize,
    pub random_size: usize,
    pub seed_edit: usize,
    pub seed_label: usize,
    pub elevation_file: usize,
    pub elevation_file_label: usize,
    pub water_depth_label: usize,
    pub water_cutoff_label: usize,
    pub land_height_label: usize,
    pub land_cutoff_label: usize,
    pub water_depth: usize,
    pub water_cutoff: usize,
    pub land_height: usize,
    pub land_cutoff: usize,
    pub map: [usize; 3],
    pub terrains: Vec<i32>,
}

impl MapTab {
    /// The constructor (0x5a1be6) on `form` from the control index `*index` (advanced); `shell` is the C++
    /// `EEUIFSEMap` (the create button's and the elevation event's).
    pub fn new<H: SeplHost + ?Sized>(h: &mut H, shell: usize, form: usize, index: &mut u32) -> MapTab {
        let mut t = MapTab::default();
        let mut names = Vec::new();
        let n = op(h, EE_SCREENS_SEMP_OP_TERRAIN_COUNT, 0, 0, 0);
        for k in 0..n.max(0) {
            let (ok, i, _) = call(h, EE_SCREENS_SEMP_OP_TERRAIN, [0; 3], [k, 0, 0, 0], [0.0; 4]);
            if ok != 0 {
                t.terrains.push(i[1]);
                names.push(i[2] as u32);
            }
        }
        let mut m = Maker { h, form, index: *index };

        t.width_label = m.make(label(ID_WIDTH_LABEL, IDS_MAP_WIDTH, 130.0, 616.0, 75.0, 0.0));
        let wl = rect(m.h, t.width_label);
        let x = control_width(wl) as f32 + 135.0;
        t.height_label = m.make(label(ID_HEIGHT_LABEL, IDS_SE_MAP_MAP_HEIGHT, x, 616.0, 75.0, 0.0));
        let hl = rect(m.h, t.height_label);
        for size in MAP_SIZES {
            line_push(m.h, EE_SCREENS_SEPL_TEXT_NUMBER, 0, size);
        }
        t.width = m.make(combo(ID_WIDTH, wl[0], wl[3] + 5.0, 75.0));
        for size in MAP_SIZES {
            line_push(m.h, EE_SCREENS_SEPL_TEXT_NUMBER, 0, size);
        }
        t.height = m.make(combo(ID_HEIGHT, hl[0], hl[3] + 5.0, 75.0));

        let w = rect(m.h, t.width);
        let wt = label_rect(m.h, t.width, false);
        // the terrain and style labels span the two combo boxes; the map size's the two labels
        let span = ((rect(m.h, t.height)[2] - w[0]) as u32) as f32;
        let label_span = ((hl[2] - wl[0]) as u32) as f32;
        t.default_terrain_label =
            m.make(label(ID_DEFAULT_TERRAIN_LABEL, IDS_DEFAULT_TERRAIN, w[0], wt[3] + 5.0, span, 0.0));
        t.map_style_label = m.make(label(ID_MAP_STYLE_LABEL, IDS_SE_MAP_MAP_TYPE, wl[0], wt[3] + 5.0, span, 0.0));
        op(m.h, EE_SCREENS_SEMP_OP_LINES_GENERATORS, 0, 0, 0);
        let msl = rect(m.h, t.map_style_label);
        t.map_style = m.make(combo(ID_MAP_STYLE, wl[0], msl[3] + 5.0, hl[2] - wl[0]));
        for name in names {
            line_push(m.h, crate::screens::editor_players::EE_SCREENS_SEPL_TEXT_STRING, name, 0);
        }
        let dtl = rect(m.h, t.default_terrain_label);
        t.default_terrain = m.make(combo(ID_DEFAULT_TERRAIN, dtl[0], dtl[3] + 5.0, hl[2] - wl[0]));

        t.seed_edit = m.make(number_box(ID_SEED_EDIT, 0.0, 0.0, 50.0, 4.0, false).flags(EE_SCREENS_SEPL_FONT_HEIGHT));
        let he0 = b"HE0";
        call(m.h, EE_SCREENS_SEPL_OP_EDIT_BYTES, [t.seed_edit, he0.as_ptr() as usize, 0], [3, 0, 0, 0], [0.0; 4]);
        t.seed_label = m.make(
            EeScreensSeplCtl::new(EE_SCREENS_SEPL_LABEL, ID_SEED_LABEL, 0.0, 0.0, 50.0, 4.0)
                .text(EE_SCREENS_SEPL_TEXT_NUMBER, 0, 0)
                .flags(EE_SCREENS_SEPL_FG_WHITE | EE_SCREENS_SEPL_FONT_HEIGHT)
                .bg(0.4)
                .extra([2, 0, 0, 0])
                .group(GROUP),
        );

        let radio_h = call(m.h, EE_SCREENS_SEMP_OP_RADIO_HEIGHT, [0, form, 0], [0; 4], [0.0; 4]).0 as u32 as f32;
        let mut y = 616.0f32;
        let kinds =
            [(ID_BLANK_RADIO, IDS_BLANK, true), (ID_RANDOM_RADIO, IDS_RANDOM, false), (ID_SEED_RADIO, IDS_SEED, false)];
        for (k, (id, string, checked)) in kinds.into_iter().enumerate() {
            if k > 0 {
                y = rect(m.h, t.map[k - 1])[3] + 1.0;
            }
            t.map[k] = make_own(
                m.h,
                EE_SCREENS_SEMP_OP_MAKE_RADIO,
                form,
                0,
                [id as i32, string as i32, m.index as i32, checked as i32],
                [25.0, y, radio_h, 0.0],
            );
            m.index += 1;
        }
        t.create_map =
            make_own(m.h, EE_SCREENS_SEMP_OP_MAKE_BUTTON, form, shell, [0, 0, m.index as i32, 0], [25.0, y, 0.0, 0.0]);
        m.index += 1;
        let r2 = rect(m.h, t.map[2]);
        let l2 = label_rect(m.h, t.map[2], true);
        set_location(m.h, t.seed_edit, l2[0], r2[3] + 5.0);
        set_location(m.h, t.seed_label, l2[0], r2[3] + 5.0);
        let r0 = rect(m.h, t.map[0]);
        let se = rect(m.h, t.seed_edit);
        set_location(m.h, t.create_map, r0[0], se[3] + 5.0);
        op(m.h, EE_SCREENS_SEMP_OP_BUTTON_TEXT, t.create_map, 0, 0);

        t.random_size_label = m.make(label(ID_RANDOM_SIZE_LABEL, IDS_INGAME_MAP_SIZE, wl[0], wl[1], label_span, 0.0));
        op(m.h, EE_SCREENS_SEMP_OP_LINES_MAP_SIZES, 0, 0, 0);
        t.random_size = m.make(combo(ID_RANDOM_SIZE, w[0], w[1], label_span));

        let ex = hl[2] + 15.0;
        t.elevation_file_label = m.make(label(ID_ELEVATION_FILE_LABEL, IDS_ELEVATION_FILE, ex, hl[1], 200.0, 0.0));
        let el = rect(m.h, t.elevation_file_label);
        t.elevation_file = make_own(
            m.h,
            EE_SCREENS_SEMP_OP_MAKE_FILE_COMBO,
            form,
            shell,
            [0, 0, m.index as i32, 0],
            [ex, el[3] + 5.0, 0.0, 0.0],
        );
        m.index += 1;

        // the four boxes: a label 150 x 15 at the elevation label's bottom + dy, the box after it
        let boxes = [
            (ID_WATER_DEPTH_LABEL, IDS_WATER_DEPTH, 30.0, ID_WATER_DEPTH, IDS_THIS_IS_MAXIMUM_DEPTH_THAT_WATER),
            (ID_WATER_CUTOFF_LABEL, IDS_WATER_CUTOFF, 45.0, ID_WATER_CUTOFF, IDS_THIS_IS_DEPTH_IN_METERS),
            (ID_LAND_HEIGHT_LABEL, IDS_LAND_HEIGHT, 60.0, ID_LAND_HEIGHT, IDS_THIS_IS_MAXIMUM_HEIGHT_THAT_LAND),
            (ID_LAND_CUTOFF_LABEL, IDS_LAND_CUTOFF, 75.0, ID_LAND_CUTOFF, IDS_THIS_IS_HEIGHT_IN_METERS),
        ];
        let mut made = [(0usize, 0usize); 4];
        for (k, (label_id, string, dy, box_id, help)) in boxes.into_iter().enumerate() {
            let by = el[3] + dy;
            let l = m.make(label(label_id, string, el[0], by, 150.0, 15.0));
            let lr = rect(m.h, l);
            let b = m.make(number_box(box_id, lr[2] + 1.0, by, 49.0, 15.0, true));
            call(m.h, crate::screens::editor_map::EE_SCREENS_SEPL_OP_HELP, [b, 0, 0], [help as i32, 0, 0, 0], [0.0; 4]);
            made[k] = (l, b);
        }
        (t.water_depth_label, t.water_depth) = made[0];
        (t.water_cutoff_label, t.water_cutoff) = made[1];
        (t.land_height_label, t.land_height) = made[2];
        (t.land_cutoff_label, t.land_cutoff) = made[3];
        *index = m.index;

        for c in [t.map_style, t.map_style_label, t.seed_label, t.seed_edit, t.random_size, t.random_size_label] {
            display(h, c, false);
        }
        t.show_elevation(h, false);
        t
    }

    /// The destructor (0x5a3b27): every control deleted, in its order.
    pub fn destroy<H: SeplHost + ?Sized>(&self, h: &mut H) {
        let order = [
            self.width_label,
            self.height_label,
            self.width,
            self.height,
            self.default_terrain_label,
            self.map_style_label,
            self.map[0],
            self.map[1],
            self.map[2],
            self.seed_label,
            self.seed_edit,
            self.create_map,
            self.default_terrain,
            self.map_style,
            self.random_size,
            self.random_size_label,
            self.elevation_file_label,
            self.land_height,
            self.land_height_label,
            self.water_cutoff,
            self.water_cutoff_label,
            self.land_cutoff,
            self.land_cutoff_label,
            self.water_depth,
            self.water_depth_label,
            self.elevation_file,
        ];
        for c in order {
            op(h, EE_SCREENS_SEPL_OP_DELETE, c, 0, 0);
        }
    }

    fn checked<H: SeplHost + ?Sized>(&self, h: &mut H, k: usize) -> bool {
        op(h, EE_SCREENS_SEPL_OP_RADIO_IS_CHECKED, self.map[k], 0, 0) != 0
    }

    /// `CreateMap` (0x5a3c7d): the blank map of the chosen size and terrain (then a new seed drawn), or the random
    /// map of the chosen size and style with a new seed (shown in the seed label) or the typed one.
    pub fn create_map<H: SeplHost + ?Sized>(&self, h: &mut H) {
        if self.checked(h, 0) {
            let w = op(h, EE_SCREENS_SEMP_OP_COMBO_ATOI, self.width, 0, 0);
            let ht = op(h, EE_SCREENS_SEMP_OP_COMBO_ATOI, self.height, 0, 0);
            let terrain = op(h, EE_SCREENS_SEPL_OP_LIST_SELECTION, self.default_terrain, 0, 0);
            // the original indexes its terrain list unchecked: a selection past it makes terrain 0
            let id = self.terrains.get(terrain as u32 as usize).copied().unwrap_or(0);
            call(h, EE_SCREENS_SEMP_OP_BLANK_MAP, [0; 3], [w, ht, id, 0], [0.0; 4]);
            op(h, EE_SCREENS_SEMP_OP_SEED_DRAW, 0, 0, 0);
        } else {
            let size = op(h, EE_SCREENS_SEPL_OP_COMBO_SELECTION, self.random_size, 0, 0);
            if self.checked(h, 1) {
                let seed = op(h, EE_SCREENS_SEMP_OP_SEED_DRAW, 0, 0, 0);
                op(h, EE_SCREENS_SEPL_OP_LABEL_NUMBER, self.seed_label, seed, 0);
            } else {
                let seed = op(h, EE_SCREENS_SEPL_OP_EDIT_ATOI, self.seed_edit, 0, 0);
                op(h, EE_SCREENS_SEMP_OP_SEED_SET, 0, seed, 0);
            }
            let style = op(h, EE_SCREENS_SEPL_OP_COMBO_SELECTION, self.map_style, 0, 0);
            op(h, EE_SCREENS_SEMP_OP_RANDOM_MAP, 0, size, style);
        }
    }

    /// `ShowElevationControls` (0x5a3dec): in the blank kind the four boxes and their labels shown or hidden (the
    /// height box and label the other way); else all hidden.
    pub fn show_elevation<H: SeplHost + ?Sized>(&self, h: &mut H, show: bool) {
        let boxes = [
            self.water_cutoff,
            self.water_cutoff_label,
            self.water_depth,
            self.water_depth_label,
            self.land_cutoff,
            self.land_cutoff_label,
            self.land_height,
            self.land_height_label,
        ];
        let blank = self.checked(h, 0);
        for c in boxes {
            display(h, c, blank && show);
        }
        for c in [self.height, self.height_label] {
            display(h, c, blank && !show);
        }
    }

    /// `Enter` (0x5a3ebf): the elevation files listed again.
    pub fn enter<H: SeplHost + ?Sized>(&self, h: &mut H) {
        if self.elevation_file != 0 {
            op(h, EE_SCREENS_SEMP_OP_FILE_ENTRIES, self.elevation_file, 0, 0);
        }
    }
}

fn control<H: SeplHost + ?Sized>(h: &mut H, form: usize, id: u32) -> usize {
    let mut out = 0usize;
    call(h, EE_SCREENS_SEMP_OP_CONTROL, [form, &mut out as *mut usize as usize, 0], [id as i32, 0, 0, 0], [0.0; 4]);
    out
}

/// `UIRBMap::HideControls` (0x5a1a6b) on `form`, then for `id` (not 0) `UIRBMap::Check`'s rest (0x5a1907): its
/// kind's controls shown; the blank kind runs the elevation file's selection event. A missing control is skipped
/// (the original reads it).
pub fn kind_check<H: SeplHost + ?Sized>(h: &mut H, form: usize, id: u32) {
    for c in MODE_CONTROLS {
        let c = control(h, form, c);
        if c != 0 {
            display(h, c, false);
        }
    }
    for &c in kind_controls(id) {
        let c = control(h, form, c);
        if c != 0 {
            display(h, c, true);
        }
    }
    if id == ID_BLANK_RADIO {
        let c = control(h, form, ID_ELEVATION_FILE);
        if c != 0 {
            op(h, EE_SCREENS_SEMP_OP_PROCESS_SELECTION, c, 0, 0);
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_strings_match_the_language_header() {
        let header = std::fs::read_to_string(concat!(env!("CARGO_MANIFEST_DIR"), "/../../../include/ee/EEStringIDs.h"))
            .expect("EEStringIDs.h");
        let value = |name: &str| -> u32 {
            let line = header
                .lines()
                .find(|l| l.split_whitespace().nth(1) == Some(name) && l.starts_with("#define"))
                .unwrap_or_else(|| panic!("{name}"));
            line.split_whitespace().nth(2).unwrap().parse().unwrap()
        };
        for (name, v) in [
            ("IDS_MAP_WIDTH", IDS_MAP_WIDTH),
            ("IDS_SE_MAP_MAP_HEIGHT", IDS_SE_MAP_MAP_HEIGHT),
            ("IDS_DEFAULT_TERRAIN", IDS_DEFAULT_TERRAIN),
            ("IDS_SE_MAP_MAP_TYPE", IDS_SE_MAP_MAP_TYPE),
            ("IDS_BLANK", IDS_BLANK),
            ("IDS_RANDOM", IDS_RANDOM),
            ("IDS_SEED", IDS_SEED),
            ("IDS_INGAME_MAP_SIZE", IDS_INGAME_MAP_SIZE),
            ("IDS_ELEVATION_FILE", IDS_ELEVATION_FILE),
            ("IDS_WATER_DEPTH", IDS_WATER_DEPTH),
            ("IDS_WATER_CUTOFF", IDS_WATER_CUTOFF),
            ("IDS_LAND_HEIGHT", IDS_LAND_HEIGHT),
            ("IDS_LAND_CUTOFF", IDS_LAND_CUTOFF),
            ("IDS_THIS_IS_MAXIMUM_DEPTH_THAT_WATER", IDS_THIS_IS_MAXIMUM_DEPTH_THAT_WATER),
            ("IDS_THIS_IS_DEPTH_IN_METERS", IDS_THIS_IS_DEPTH_IN_METERS),
            ("IDS_THIS_IS_MAXIMUM_HEIGHT_THAT_LAND", IDS_THIS_IS_MAXIMUM_HEIGHT_THAT_LAND),
            ("IDS_THIS_IS_HEIGHT_IN_METERS", IDS_THIS_IS_HEIGHT_IN_METERS),
        ] {
            assert_eq!(value(name), v, "{name}");
        }
    }

    #[test]
    fn control_width_is_the_toolkits() {
        // a 75 wide label at x 130: GERectangle(pos, 75, h) ends at 204; GetWidth floors 204 - 130 + 1
        assert_eq!(control_width([130.0, 616.0, 204.0, 626.0]), 75);
        assert_eq!(control_width([0.0, 0.0, 9.5, 1.0]), 10);
    }

    #[test]
    fn kinds_show_their_controls() {
        assert_eq!(kind_controls(ID_RANDOM_RADIO).len(), 5);
        assert!(kind_controls(ID_BLANK_RADIO).contains(&ID_ELEVATION_FILE));
        assert!(kind_controls(ID_SEED_RADIO).contains(&ID_SEED_EDIT));
        assert!(!kind_controls(ID_RANDOM_RADIO).contains(&ID_SEED_EDIT));
        assert!(kind_controls(0).is_empty());
        // every control a kind shows is one HideControls hides
        for id in [ID_RANDOM_RADIO, ID_BLANK_RADIO, ID_SEED_RADIO] {
            assert!(kind_controls(id).iter().all(|c| MODE_CONTROLS.contains(c)));
        }
    }

    /// A host recording the ops; the maker and the own makers answer handles 1, 2, ...; rectangles at 10 per handle.
    #[derive(Default)]
    struct Rec {
        ops: Vec<(u32, usize, [i32; 4])>,
        next: usize,
        checked: bool,
    }

    impl SeplHost for Rec {
        fn op(&mut self, op: u32, p: [usize; 3], i: &mut [i32; 4], f: &mut [f32; 4]) -> i32 {
            match op {
                crate::screens::editor_players::EE_SCREENS_SEPL_OP_MAKE
                | EE_SCREENS_SEMP_OP_MAKE_RADIO
                | EE_SCREENS_SEMP_OP_MAKE_BUTTON
                | EE_SCREENS_SEMP_OP_MAKE_FILE_COMBO => {
                    self.next += 1;
                    // SAFETY: the makers pass their out handle in p0
                    unsafe { *(p[0] as *mut usize) = self.next };
                }
                crate::screens::editor_players::EE_SCREENS_SEPL_OP_RECT | EE_SCREENS_SEMP_OP_LABEL_RECT => {
                    *f = [0.0, 10.0 * p[0] as f32, 74.0, 10.0 * p[0] as f32 + 9.0];
                }
                EE_SCREENS_SEMP_OP_TERRAIN_COUNT => return 3,
                EE_SCREENS_SEMP_OP_TERRAIN => {
                    i[1] = 100 + i[0];
                    i[2] = 7;
                    return (i[0] != 1) as i32;
                }
                EE_SCREENS_SEPL_OP_RADIO_IS_CHECKED => return self.checked as i32,
                EE_SCREENS_SEPL_OP_LIST_SELECTION => return 1,
                EE_SCREENS_SEMP_OP_COMBO_ATOI => return 40,
                _ => {}
            }
            self.ops.push((op, p[0], *i));
            0
        }
    }

    #[test]
    fn new_makes_26_controls_from_the_index_and_skips_hidden_terrains() {
        let mut h = Rec::default();
        let mut index = 12;
        let t = MapTab::new(&mut h, 900, 5, &mut index);
        assert_eq!(index, 12 + 26);
        assert_eq!(t.terrains, vec![100, 102]);
        let mut all = vec![
            t.create_map,
            t.default_terrain_label,
            t.random_size_label,
            t.width_label,
            t.height_label,
            t.map_style_label,
            t.default_terrain,
            t.map_style,
            t.width,
            t.height,
            t.random_size,
            t.seed_edit,
            t.seed_label,
            t.elevation_file,
            t.elevation_file_label,
            t.water_depth_label,
            t.water_cutoff_label,
            t.land_height_label,
            t.land_cutoff_label,
            t.water_depth,
            t.water_cutoff,
            t.land_height,
            t.land_cutoff,
        ];
        all.extend(t.map);
        all.sort();
        assert_eq!(all, (1..=26).collect::<Vec<_>>());
        // the blank map: width and height read, the second editor terrain's id, then a seed drawn
        h.checked = true;
        h.ops.clear();
        t.create_map(&mut h);
        let blank = h.ops.iter().find(|o| o.0 == EE_SCREENS_SEMP_OP_BLANK_MAP).unwrap();
        assert_eq!(&blank.2[..3], &[40, 40, 102]);
        assert_eq!(h.ops.last().unwrap().0, EE_SCREENS_SEMP_OP_SEED_DRAW);
    }

    #[test]
    fn show_elevation_outside_the_blank_kind_hides_all_ten() {
        let mut h = Rec::default();
        let mut index = 0;
        let t = MapTab::new(&mut h, 0, 0, &mut index);
        h.ops.clear();
        t.show_elevation(&mut h, true);
        let shown: Vec<i32> = h.ops.iter().map(|o| o.2[0]).collect();
        assert_eq!(shown, vec![0; 10]);
        h.checked = true;
        h.ops.clear();
        t.show_elevation(&mut h, true);
        let shown: Vec<i32> = h.ops.iter().map(|o| o.2[0]).collect();
        assert_eq!(shown, vec![1, 1, 1, 1, 1, 1, 1, 1, 0, 0]);
    }
}
