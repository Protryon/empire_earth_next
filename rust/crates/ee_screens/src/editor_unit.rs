//! The scenario editor's unit tab whole in Rust (`EESEUnit`, once `EESEUnit.cpp`'s tab class): the controls it makes
//! (records for the shared maker, `EeScreensSeplCtl`, and the tab's own control classes through its host), the owner,
//! sort and palette radios and the class list they fill, the cargo of the selected units, the placement state (a
//! unit, a wall or a gate being placed, the unit under the mouse) and the mouse and keys over the game view. The
//! controls, the actions and the build tools stay C++ objects (`mBuildWall`, `mBuildGate` are by-value members of the
//! C++ shell); the tab drives them through the shared control ops (`ee_screenscpp_sepl`, below 100) and its own
//! (`ee_screenscpp_seun`, from 300: the world, the mouse, the build tools, the unit classes).
//!
//! A host call can come back into the tab (a build tool's `Build` or `Cancel` calls `PlaceWall` or `EndGate`, a sort
//! radio's event relists the classes): it is only used through `&self`, its changing values are `Cell`s.

use core::cell::Cell;

use crate::editor_players::*;

// ---- the unit tab's host ops (`ee_screenscpp_seun`)

/// The sort event (`EESEERust(kSEESortUnitList)`) made -> its handle in p0.
pub const EE_SCREENS_SEUN_OP_SORT_EVENT: u32 = 300;
/// The control p0's trigger i0 set to the event p1 (`SetTriggerEvent`; the event's reference kept by the control).
pub const EE_SCREENS_SEUN_OP_SET_EVENT: u32 = 301;
/// A unit type button: p0 -> three handles (the button, its normal and its pressed bitmap), made on the form p1 at
/// the control index i0 for the palette i1 (its cells f0, f1 normal and f2, f3 pressed in the button texture), the
/// help string i2, at x i3; its button-up event the sort event p2; added to group 6.
pub const EE_SCREENS_SEUN_OP_TYPE_BUTTON: u32 = 302;
/// The remove cargo button: p0 -> its handle, made on the form p1 at the index i0, at x f0.
pub const EE_SCREENS_SEUN_OP_REMOVE_CARGO: u32 = 303;
/// The rotation button: p0 -> its handle, made on the form p1 at the index i0.
pub const EE_SCREENS_SEUN_OP_ROTATION: u32 = 304;
/// An action made (i0: `EE_SCREENS_SEUN_ACTION_*`) -> its handle in p0.
pub const EE_SCREENS_SEUN_OP_ACTION: u32 = 305;
/// The selection tool: p0 -> its handle (`EEUICSelectUnits` on the form p1 at the index i0, the click and the lasso
/// in p2: two handles), added to group 0.
pub const EE_SCREENS_SEUN_OP_SELECT_UNITS: u32 = 306;
/// The command tool: p0 -> its handle (`EESEUICCommandUnits` on the form p1 at the index i0, the click p2), added
/// to group 0.
pub const EE_SCREENS_SEUN_OP_COMMAND_UNITS: u32 = 307;
/// The panel's background (`textures\scenunit`) -> its handle in p0.
pub const EE_SCREENS_SEUN_OP_BACKGROUND: u32 = 308;
/// The upgrade panel: p0 -> its handle, made on the form p1 from the index i0 -> the next index.
pub const EE_SCREENS_SEUN_OP_UPGRADE_PANEL: u32 = 309;
/// p0 released: i0 0 an action, 1 a bitmap, 2 an event (`Dereference`), 3 the upgrade panel (`delete`).
pub const EE_SCREENS_SEUN_OP_RELEASE: u32 = 310;
/// The wall (p0 the shell, p1 the place wall action) placed from the build tool's start to its end for the player
/// i0 / the gate (p1 the place gate action) on its tile and direction.
pub const EE_SCREENS_SEUN_OP_PLACE_WALL: u32 = 311;
pub const EE_SCREENS_SEUN_OP_PLACE_GATE: u32 = 312;
/// The ghosts cleared (i0: inside the ghosts' lock).
pub const EE_SCREENS_SEUN_OP_CLEAR_GHOSTS: u32 = 313;
/// The colour of the gate tool's player's walls restored (p0 the shell; `EEClearGateHighlights`).
pub const EE_SCREENS_SEUN_OP_GATE_HIGHLIGHTS: u32 = 314;
/// A frame's preview (p0 the shell): i0 0 the unit ghost at the mouse, 1 the wall ghosts, 2 the gate on a wall.
pub const EE_SCREENS_SEUN_OP_PREVIEW: u32 = 315;
/// The game view (i0: 1 right of the panel, 0 the full view), the camera flags cleared, the mini map's view, the
/// manager redrawn and the current form's controls dirty.
pub const EE_SCREENS_SEUN_OP_GAME_VIEW: u32 = 316;
/// The form of the control p0: its manager redrawn / the editor's selection cleared.
pub const EE_SCREENS_SEUN_OP_REDRAW: u32 = 317;
pub const EE_SCREENS_SEUN_OP_CLEAR_SELECTION: u32 = 318;
/// The move action p0: -> whether a unit is picked up / cancelled / its unit i0 picked up / put down.
pub const EE_SCREENS_SEUN_OP_MOVING: u32 = 319;
pub const EE_SCREENS_SEUN_OP_CANCEL_MOVE: u32 = 320;
pub const EE_SCREENS_SEUN_OP_PICK_UP: u32 = 321;
pub const EE_SCREENS_SEUN_OP_PUT_DOWN: u32 = 322;
/// The place unit action p0's unit placed for the player i0.
pub const EE_SCREENS_SEUN_OP_PLACE_UNIT: u32 = 323;
/// The upgrade panel p0 shows the unit p1 (NULL: none).
pub const EE_SCREENS_SEUN_OP_SHOW_UNIT: u32 = 324;
/// The background p1 drawn on the form where the shell p0's panel rectangle overlaps the rectangle p2.
pub const EE_SCREENS_SEUN_OP_DRAW: u32 = 325;
/// The rotation button p0's yaw -> f0.
pub const EE_SCREENS_SEUN_OP_ANGLE: u32 = 326;
/// -> the palette the type buttons set (`EEUIFSEBUnitType::Check`).
pub const EE_SCREENS_SEUN_OP_TYPE_MODE: u32 = 327;
/// The shell p1's panel rectangle set to (0, 0)-(224, 605) and added to the dirty rectangles of the manager of the
/// control p0's form.
pub const EE_SCREENS_SEUN_OP_DIRTY: u32 = 328;
/// The list p0's selection -> it, -1 for none / its line count / the line i0 selected / its scroll at 0.
pub const EE_SCREENS_SEUN_OP_LIST_SELECTION: u32 = 329;
pub const EE_SCREENS_SEUN_OP_LIST_COUNT: u32 = 330;
pub const EE_SCREENS_SEUN_OP_LIST_SELECT: u32 = 331;
pub const EE_SCREENS_SEUN_OP_LIST_TOP: u32 = 332;
/// The build tool i0 (0 the wall, 1 the gate) of the shell p0 cancelled (`Cancel`).
pub const EE_SCREENS_SEUN_OP_CANCEL_BUILD: u32 = 333;
/// The unit list p0's line i1 for the player i0: -> the player's id; i1 whether the player may place the class, i2
/// what it is (`EE_SCREENS_SEUN_KIND_*`).
pub const EE_SCREENS_SEUN_OP_PLACE_INFO: u32 = 334;
/// The wall (i2 = 0) or gate (1) tool of the shell p0 set from the unit list p1's line i1 and started for the
/// player i0.
pub const EE_SCREENS_SEUN_OP_START_BUILD: u32 = 335;
/// The unit list p1's line i0's class and the player id i1 posted to the editor's load thread (form p0; 0x402).
pub const EE_SCREENS_SEUN_OP_POST_GHOST: u32 = 336;
/// The cargo list p0 emptied / the editor's selected units (form p1) -> their count / the selected unit i0 -> its
/// cargo count / its cargo i1 added to the list p0.
pub const EE_SCREENS_SEUN_OP_CARGO_CLEAR: u32 = 337;
pub const EE_SCREENS_SEUN_OP_SELECTED_COUNT: u32 = 338;
pub const EE_SCREENS_SEUN_OP_CARGO_COUNT: u32 = 339;
pub const EE_SCREENS_SEUN_OP_CARGO_ADD: u32 = 340;
/// The unit list p0: its comparator for the sort i0 / emptied / the class i0 inserted at its sorted place.
pub const EE_SCREENS_SEUN_OP_LIST_SORT: u32 = 341;
pub const EE_SCREENS_SEUN_OP_LIST_EMPTY: u32 = 342;
pub const EE_SCREENS_SEUN_OP_LIST_ADD: u32 = 343;
/// -> the unit class count / the class i0's flags (`EE_SCREENS_SEUN_CLASS_*`; 0 for none), its type in i1.
pub const EE_SCREENS_SEUN_OP_CLASS_COUNT: u32 = 344;
pub const EE_SCREENS_SEUN_OP_CLASS: u32 = 345;
/// -> whether the game view contains f0, f1 / the mouse buttons (`EE_SCREENS_SEUN_MOUSE_*`).
pub const EE_SCREENS_SEUN_OP_IN_VIEW: u32 = 346;
pub const EE_SCREENS_SEUN_OP_MOUSE: u32 = 347;
/// The unit at f0, f1 -> its id (-1: none), i1 whether it is a gate.
pub const EE_SCREENS_SEUN_OP_PICK: u32 = 348;
/// The gate i0 lifted for re-placing: the shell p0's gate tool set from it, the gate killed, the tool started.
pub const EE_SCREENS_SEUN_OP_HOLD_GATE: u32 = 349;
/// The build tool i0 (0 wall, 1 gate) of the shell p0 follows the mouse at f0, f1 (`Update`).
pub const EE_SCREENS_SEUN_OP_BUILD_UPDATE: u32 = 350;
/// The selection tool p0 starts a selection / the command tool a command at f0, f1.
pub const EE_SCREENS_SEUN_OP_START_SELECTION: u32 = 351;
pub const EE_SCREENS_SEUN_OP_START_COMMAND: u32 = 352;
/// The ghosts' lock entered (i0 = 1) or left.
pub const EE_SCREENS_SEUN_OP_LOCK: u32 = 353;
/// -> the keys: bit 0 Delete just pressed, bit 1 Shift down.
pub const EE_SCREENS_SEUN_OP_KEYS: u32 = 354;
/// The current player's selection deleted (i0: the group flag, Shift).
pub const EE_SCREENS_SEUN_OP_DELETE_SELECTION: u32 = 355;
/// The rotation button p0's keys (`UpdateKeyboard`).
pub const EE_SCREENS_SEUN_OP_ROTATION_KEYS: u32 = 356;

/// The actions: the click, the lasso, the wall, gate and unit placements, the move.
pub const EE_SCREENS_SEUN_ACTION_CLICK: i32 = 0;
pub const EE_SCREENS_SEUN_ACTION_LASSO: i32 = 1;
pub const EE_SCREENS_SEUN_ACTION_PLACE_UNIT: i32 = 2;
pub const EE_SCREENS_SEUN_ACTION_PLACE_GATE: i32 = 3;
pub const EE_SCREENS_SEUN_ACTION_PLACE_WALL: i32 = 4;
pub const EE_SCREENS_SEUN_ACTION_MOVE: i32 = 5;

/// What a picked class places: a unit, a wall (its node a wall technology), a gate.
pub const EE_SCREENS_SEUN_KIND_UNIT: i32 = 0;
pub const EE_SCREENS_SEUN_KIND_WALL: i32 = 1;
pub const EE_SCREENS_SEUN_KIND_GATE: i32 = 2;

/// A unit class's flags for the palettes: it exists and the editor lists it, gaia or players may place it, a hero
/// (by AI kind or category), its domains, a land building.
pub const EE_SCREENS_SEUN_CLASS_SELECTABLE: i32 = 1;
pub const EE_SCREENS_SEUN_CLASS_GAIA: i32 = 2;
pub const EE_SCREENS_SEUN_CLASS_PLAYER: i32 = 4;
pub const EE_SCREENS_SEUN_CLASS_HERO: i32 = 8;
pub const EE_SCREENS_SEUN_CLASS_AIR: i32 = 16;
pub const EE_SCREENS_SEUN_CLASS_BUILDING: i32 = 32;
pub const EE_SCREENS_SEUN_CLASS_LAND: i32 = 64;
pub const EE_SCREENS_SEUN_CLASS_SEA: i32 = 128;
pub const EE_SCREENS_SEUN_CLASS_SPACE: i32 = 256;

/// The mouse buttons: left double pressed, just pressed, just released, down; right just pressed, just released.
pub const EE_SCREENS_SEUN_MOUSE_L_DOUBLE: i32 = 1;
pub const EE_SCREENS_SEUN_MOUSE_L_PRESSED: i32 = 2;
pub const EE_SCREENS_SEUN_MOUSE_L_RELEASED: i32 = 4;
pub const EE_SCREENS_SEUN_MOUSE_L_DOWN: i32 = 8;
pub const EE_SCREENS_SEUN_MOUSE_R_PRESSED: i32 = 16;
pub const EE_SCREENS_SEUN_MOUSE_R_RELEASED: i32 = 32;

/// The unit palettes (`kUnitPalette*`).
pub const EE_SCREENS_SEUN_PALETTE_LAND: i32 = 0;
pub const EE_SCREENS_SEUN_PALETTE_AIR: i32 = 1;
pub const EE_SCREENS_SEUN_PALETTE_NAVAL: i32 = 2;
pub const EE_SCREENS_SEUN_PALETTE_STRUCTURES: i32 = 3;
pub const EE_SCREENS_SEUN_PALETTE_HEROES: i32 = 4;
pub const EE_SCREENS_SEUN_PALETTE_RESOURCES: i32 = 5;

/// The kinds of the shared maker's unit list (`EE_SCREENS_SEPL_UNIT_LIST`'s extra 0): the object screen's list, the
/// unit tab's class list (`EEUIFSELBUnitList`), its cargo list (`EEUIFSELBCargoList`).
pub const EE_SCREENS_SEPL_LIST_OBJECT: i32 = 0;
pub const EE_SCREENS_SEPL_LIST_UNIT_TYPES: i32 = 1;
pub const EE_SCREENS_SEPL_LIST_CARGO: i32 = 2;

// ---- the language's strings (`ee/EEStringIDs.h`; the host `static_assert`s them)

pub const EE_SCREENS_SEUN_IDS_ALPHA: u32 = 20536;
pub const EE_SCREENS_SEUN_IDS_CARGO: u32 = 20540;
pub const EE_SCREENS_SEUN_IDS_SORT: u32 = 20541;
pub const EE_SCREENS_SEUN_IDS_WORLD_PLAYER: u32 = 20553;
const IDS_SE_EPOCH: u32 = EE_SCREENS_SEUN_IDS_ALPHA + 1;
const IDS_FAMILY: u32 = EE_SCREENS_SEUN_IDS_ALPHA + 2;

/// The tab's control group.
const GROUP: u32 = 6;

/// The six unit type buttons: the palette, its normal and pressed cells in the button texture, its help string
/// (0x8c4458).
const TYPE_BUTTONS: [(i32, [f32; 4], i32); 6] = [
    (0, [0.0, 146.0, 37.0, 146.0], 8501),
    (1, [74.0, 146.0, 111.0, 146.0], 8505),
    (2, [148.0, 146.0, 185.0, 146.0], 8502),
    (3, [0.0, 171.0, 37.0, 171.0], 8504),
    (4, [74.0, 171.0, 111.0, 171.0], 8503),
    (5, [148.0, 171.0, 185.0, 171.0], 8500),
];

/// The unit tab (`EESEUnit`).
#[derive(Debug, Default)]
pub struct UnitTab {
    /// the C++ shell (its build tools) and the editor's player combo box
    pub shell: usize,
    pub players: usize,
    pub sort_event: usize,
    pub cargo_title: usize,
    pub world_button: usize,
    pub players_button: usize,
    pub sort_label: usize,
    pub type_buttons: [usize; 6],
    pub bitmaps: [usize; 12],
    pub unit_list: usize,
    pub alpha_button: usize,
    pub epoch_button: usize,
    pub family_button: usize,
    pub cargo_list: usize,
    pub remove_cargo: usize,
    pub rotation: usize,
    pub mouse_click: usize,
    pub lasso: usize,
    pub place_unit: usize,
    pub place_gate: usize,
    pub place_wall: usize,
    pub move_unit: usize,
    pub select_units: usize,
    pub command_units: usize,
    pub background: usize,
    pub upgrade_panel: usize,
    /// the palette, sort and owner the list was last filled for
    pub type_mode: Cell<i32>,
    pub sort_mode: Cell<i32>,
    pub layer: Cell<i32>,
    /// the unit under the mouse while placing (-1: none)
    pub under_mouse: Cell<i32>,
    pub placing: Cell<bool>,
    pub placing_wall: Cell<bool>,
    pub placing_gate: Cell<bool>,
    pub gate_held: Cell<bool>,
}

fn op<H: SeplHost + ?Sized>(h: &mut H, op: u32, p: [usize; 3], ints: [i32; 4], floats: [f32; 4]) -> i32 {
    let (mut i, mut f) = (ints, floats);
    h.op(op, p, &mut i, &mut f)
}

/// A handle the host makes into p0.
fn made<H: SeplHost + ?Sized>(h: &mut H, which: u32, p1: usize, p2: usize, ints: [i32; 4], floats: [f32; 4]) -> usize {
    let mut out = 0usize;
    op(h, which, [&mut out as *mut usize as usize, p1, p2], ints, floats);
    out
}

fn set_event<H: SeplHost + ?Sized>(h: &mut H, control: usize, trigger: i32, event: usize) {
    op(h, EE_SCREENS_SEUN_OP_SET_EVENT, [control, event, 0], [trigger, 0, 0, 0], [0.0; 4]);
}

fn radio_checked<H: SeplHost + ?Sized>(h: &mut H, control: usize) -> bool {
    call(h, EE_SCREENS_SEPL_OP_RADIO_IS_CHECKED, [control, 0, 0], [0; 4]) != 0
}

fn clear_ghosts<H: SeplHost + ?Sized>(h: &mut H, locked: bool) {
    op(h, EE_SCREENS_SEUN_OP_CLEAR_GHOSTS, [0; 3], [locked as i32, 0, 0, 0], [0.0; 4]);
}

fn lock<H: SeplHost + ?Sized>(h: &mut H, enter: bool) {
    op(h, EE_SCREENS_SEUN_OP_LOCK, [0; 3], [enter as i32, 0, 0, 0], [0.0; 4]);
}

/// A yellow left-aligned label of the tab at the font's height.
fn label(id: u32, string: u32, x: f32, y: f32, w: f32) -> EeScreensSeplCtl {
    EeScreensSeplCtl::new(EE_SCREENS_SEPL_LABEL, id, x, y, w, 0.0)
        .string(string)
        .flags(EE_SCREENS_SEPL_FG | EE_SCREENS_SEPL_FONT_HEIGHT)
        .align(EE_SCREENS_SEPL_ALIGN_LEFT)
        .bg(YELLOW_BG)
        .group(GROUP)
}

/// A yellow radio button of the tab at the font's height.
fn radio(id: u32, string: u32, x: f32, y: f32, w: f32, checked: bool) -> EeScreensSeplCtl {
    EeScreensSeplCtl::new(EE_SCREENS_SEPL_RADIO, id, x, y, w, 0.0)
        .string(string)
        .flags(EE_SCREENS_SEPL_FG | EE_SCREENS_SEPL_FONT_HEIGHT)
        .bg(YELLOW_BG)
        .group(GROUP)
        .extra([checked as i32, 0, 0, 0])
}

/// Whether a class with the flags `c` belongs to the palette `mode` (`RefreshUnitList`'s chain: heroes, air,
/// buildings, land, sea, space as air, the rest resources).
pub fn in_palette(c: i32, mode: i32) -> bool {
    let palette = if c & EE_SCREENS_SEUN_CLASS_HERO != 0 {
        EE_SCREENS_SEUN_PALETTE_HEROES
    } else if c & EE_SCREENS_SEUN_CLASS_AIR != 0 {
        EE_SCREENS_SEUN_PALETTE_AIR
    } else if c & EE_SCREENS_SEUN_CLASS_BUILDING != 0 {
        EE_SCREENS_SEUN_PALETTE_STRUCTURES
    } else if c & EE_SCREENS_SEUN_CLASS_LAND != 0 {
        EE_SCREENS_SEUN_PALETTE_LAND
    } else if c & EE_SCREENS_SEUN_CLASS_SEA != 0 {
        EE_SCREENS_SEUN_PALETTE_NAVAL
    } else if c & EE_SCREENS_SEUN_CLASS_SPACE != 0 {
        EE_SCREENS_SEUN_PALETTE_AIR
    } else {
        EE_SCREENS_SEUN_PALETTE_RESOURCES
    };
    palette == mode
}

/// Whether the owner `layer` (0 gaia, 1 players, 2 none) may place a class with the flags `c` that the editor lists.
pub fn placeable(c: i32, layer: i32) -> bool {
    c & EE_SCREENS_SEUN_CLASS_SELECTABLE != 0
        && ((layer == 0 && c & EE_SCREENS_SEUN_CLASS_GAIA != 0)
            || (layer == 1 && c & EE_SCREENS_SEUN_CLASS_PLAYER != 0))
}

impl UnitTab {
    /// `EESEUnit`'s constructor on the form: the sort event, the cargo title, the owner and sort radios, the six
    /// type buttons, the class and cargo lists, the remove and rotation buttons, the actions and the selection and
    /// command tools, the background, the upgrade panel; `index` is the form's next control index (advanced).
    pub fn new<H: SeplHost + ?Sized>(h: &mut H, shell: usize, form: usize, players: usize, index: &mut u32) -> UnitTab {
        let mut u = UnitTab { shell, players, ..UnitTab::default() };
        u.sort_event = made(h, EE_SCREENS_SEUN_OP_SORT_EVENT, 0, 0, [0; 4], [0.0; 4]);
        let ev = u.sort_event;
        let font = EE_SCREENS_SEPL_FONT_SCREEN;
        let mut m = Maker { h, form, index: *index };
        u.cargo_title = m.make(label(0x3f3, EE_SCREENS_SEUN_IDS_CARGO, 260.0, 613.0, 225.0).font(font));
        u.world_button = m.make(radio(1000, EE_SCREENS_SEUN_IDS_WORLD_PLAYER, 0.0, 30.0, 140.0, false).font(font));
        set_event(m.h, u.world_button, EE_SCREENS_SEPL_TRIGGER_CHECKED, ev);
        let y = rect(m.h, u.world_button)[3] + 2.0;
        u.players_button = m.make(radio(0x3ea, EE_SCREENS_SEPL_IDS_SE_PLAYERS, 0.0, y, 140.0, true).font(font));
        set_event(m.h, u.players_button, EE_SCREENS_SEPL_TRIGGER_CHECKED, ev);
        u.sort_label = m.make(label(0x3ed, EE_SCREENS_SEUN_IDS_SORT, 145.0, 30.0, 67.0).font(font));
        let mut x = 0;
        for (palette, cells, help) in TYPE_BUTTONS {
            let mut out = [0usize; 3];
            let ints = [m.index as i32, palette, help, x];
            op(m.h, EE_SCREENS_SEUN_OP_TYPE_BUTTON, [out.as_mut_ptr() as usize, form, ev], ints, cells);
            m.index += 1;
            let k = palette as usize;
            u.type_buttons[k] = out[0];
            u.bitmaps[k * 2] = out[1];
            u.bitmaps[k * 2 + 1] = out[2];
            x += 0x23;
        }
        u.unit_list = m.make(
            EeScreensSeplCtl::new(EE_SCREENS_SEPL_UNIT_LIST, 0x3f1, 0.0, 125.0, 212.0, 481.0)
                .font(EE_SCREENS_SEPL_FONT_530)
                .flags(EE_SCREENS_SEPL_FG)
                .bg(YELLOW_BG)
                .bg2(YELLOW_BG)
                .group(GROUP)
                .extra([EE_SCREENS_SEPL_LIST_UNIT_TYPES, 0, 0, 0]),
        );
        let y = rect(m.h, u.sort_label)[3] + 2.0;
        u.alpha_button = m.make(radio(0x3ee, EE_SCREENS_SEUN_IDS_ALPHA, 145.0, y, 67.0, true).font(font));
        set_event(m.h, u.alpha_button, EE_SCREENS_SEPL_TRIGGER_CHECKED, ev);
        let y = rect(m.h, u.alpha_button)[3] + 2.0;
        u.epoch_button = m.make(radio(0x3ef, IDS_SE_EPOCH, 145.0, y, 67.0, false).font(font));
        set_event(m.h, u.epoch_button, EE_SCREENS_SEPL_TRIGGER_CHECKED, ev);
        let y = rect(m.h, u.epoch_button)[3] + 2.0;
        u.family_button = m.make(radio(0x3f0, IDS_FAMILY, 145.0, y, 67.0, false).font(font));
        set_event(m.h, u.family_button, EE_SCREENS_SEPL_TRIGGER_CHECKED, ev);
        u.cargo_list = m.make(
            EeScreensSeplCtl::new(EE_SCREENS_SEPL_UNIT_LIST, 0x3f2, 260.0, 633.0, 225.0, 99.0)
                .font(font)
                .flags(EE_SCREENS_SEPL_FG)
                .bg(YELLOW_BG)
                .group(GROUP)
                .extra([EE_SCREENS_SEPL_LIST_CARGO, 0, 0, 0]),
        );
        let x = rect(m.h, u.cargo_list)[2] + 2.0;
        u.remove_cargo =
            made(m.h, EE_SCREENS_SEUN_OP_REMOVE_CARGO, form, 0, [m.index as i32, 0, 0, 0], [x, 0.0, 0.0, 0.0]);
        m.index += 1;
        u.rotation = made(m.h, EE_SCREENS_SEUN_OP_ROTATION, form, 0, [m.index as i32, 0, 0, 0], [0.0; 4]);
        m.index += 1;
        let action = |h: &mut H, kind: i32| made(h, EE_SCREENS_SEUN_OP_ACTION, 0, 0, [kind, 0, 0, 0], [0.0; 4]);
        u.mouse_click = action(m.h, EE_SCREENS_SEUN_ACTION_CLICK);
        u.lasso = action(m.h, EE_SCREENS_SEUN_ACTION_LASSO);
        u.place_unit = action(m.h, EE_SCREENS_SEUN_ACTION_PLACE_UNIT);
        u.place_gate = action(m.h, EE_SCREENS_SEUN_ACTION_PLACE_GATE);
        u.place_wall = action(m.h, EE_SCREENS_SEUN_ACTION_PLACE_WALL);
        u.move_unit = action(m.h, EE_SCREENS_SEUN_ACTION_MOVE);
        let tools = [u.mouse_click, u.lasso];
        let ints = [m.index as i32, 0, 0, 0];
        u.select_units = made(m.h, EE_SCREENS_SEUN_OP_SELECT_UNITS, form, tools.as_ptr() as usize, ints, [0.0; 4]);
        m.index += 1;
        let ints = [m.index as i32, 0, 0, 0];
        u.command_units = made(m.h, EE_SCREENS_SEUN_OP_COMMAND_UNITS, form, u.mouse_click, ints, [0.0; 4]);
        m.index += 1;
        u.background = made(m.h, EE_SCREENS_SEUN_OP_BACKGROUND, 0, 0, [0; 4], [0.0; 4]);
        let h = m.h;
        let mut next = m.index;
        u.under_mouse.set(-1);
        u.layer.set(2);
        u.sort_mode.set(3);
        u.type_mode.set(6);
        u.show_cargo(h, false);
        let mut out = 0usize;
        let mut i = [next as i32, 0, 0, 0];
        h.op(EE_SCREENS_SEUN_OP_UPGRADE_PANEL, [&mut out as *mut usize as usize, form, 0], &mut i, &mut [0.0; 4]);
        u.upgrade_panel = out;
        next = i[0] as u32;
        *index = next;
        u
    }

    /// The destructor: the controls deleted, the actions, bitmaps and sort event released, the upgrade panel
    /// deleted (the original's order).
    pub fn destroy<H: SeplHost + ?Sized>(&self, h: &mut H) {
        for c in [self.world_button, self.players_button, self.sort_label, self.cargo_title] {
            delete(h, c);
        }
        for c in self.type_buttons {
            delete(h, c);
        }
        for c in [
            self.alpha_button,
            self.epoch_button,
            self.family_button,
            self.unit_list,
            self.cargo_list,
            self.remove_cargo,
            self.rotation,
            self.select_units,
            self.command_units,
        ] {
            delete(h, c);
        }
        let release = |h: &mut H, p: usize, kind: i32| {
            op(h, EE_SCREENS_SEUN_OP_RELEASE, [p, 0, 0], [kind, 0, 0, 0], [0.0; 4]);
        };
        for a in [self.mouse_click, self.lasso, self.place_unit, self.place_gate, self.place_wall, self.move_unit] {
            release(h, a, 0);
        }
        for b in self.bitmaps {
            release(h, b, 1);
        }
        release(h, self.background, 1);
        release(h, self.sort_event, 2);
        release(h, self.upgrade_panel, 3);
    }

    /// The player the tab places for: 0 with World checked, else the player combo's line + 1.
    pub fn current_player<H: SeplHost + ?Sized>(&self, h: &mut H) -> i32 {
        if radio_checked(h, self.world_button) {
            0
        } else {
            selection(h, self.players) as i32 + 1
        }
    }

    /// `PlaceWall`: the wall from the build tool's start to its end for the tab's player.
    pub fn place_wall<H: SeplHost + ?Sized>(&self, h: &mut H) {
        let player = self.current_player(h);
        op(h, EE_SCREENS_SEUN_OP_PLACE_WALL, [self.shell, self.place_wall, 0], [player, 0, 0, 0], [0.0; 4]);
    }

    /// `EndWall`: the ghosts cleared, the placement ended.
    pub fn end_wall<H: SeplHost + ?Sized>(&self, h: &mut H) {
        clear_ghosts(h, false);
        self.placing_wall.set(false);
        self.placing.set(false);
    }

    /// `Update` (each frame): the unit ghost at the mouse, the wall ghosts, or a gate on the wall under the mouse.
    /// The original's early-out tested the move action, never NULL: it always runs.
    pub fn update<H: SeplHost + ?Sized>(&self, h: &mut H) {
        if !self.placing.get() && self.move_unit == 0 {
            return;
        }
        let what = if !self.placing_wall.get() && !self.placing_gate.get() {
            0
        } else if self.placing_wall.get() {
            1
        } else {
            2
        };
        op(h, EE_SCREENS_SEUN_OP_PREVIEW, [self.shell, 0, 0], [what, 0, 0, 0], [0.0; 4]);
    }

    /// `PlaceGate`: the gate on the build tool's tile and direction for the tab's player.
    pub fn place_gate<H: SeplHost + ?Sized>(&self, h: &mut H) {
        let player = self.current_player(h);
        op(h, EE_SCREENS_SEUN_OP_PLACE_GATE, [self.shell, self.place_gate, 0], [player, 0, 0, 0], [0.0; 4]);
    }

    /// `EndGate`: the ghosts cleared, the placement ended, the player's walls' colour restored.
    pub fn end_gate<H: SeplHost + ?Sized>(&self, h: &mut H) {
        clear_ghosts(h, false);
        self.placing_gate.set(false);
        self.placing.set(false);
        self.gate_held.set(false);
        op(h, EE_SCREENS_SEUN_OP_GATE_HIGHLIGHTS, [self.shell, 0, 0], [0; 4], [0.0; 4]);
    }

    /// `ReselectUnitType`: the list's class picked again while placing.
    pub fn reselect_unit_type<H: SeplHost + ?Sized>(&self, h: &mut H) {
        if self.placing.get() {
            self.select_unit_type(h);
        }
    }

    /// `Leave`: the full game view, the player combo hidden, the selection cleared, a move cancelled, the
    /// placement ended and its ghosts cleared.
    pub fn leave<H: SeplHost + ?Sized>(&self, h: &mut H) {
        op(h, EE_SCREENS_SEUN_OP_GAME_VIEW, [0; 3], [0; 4], [0.0; 4]);
        display(h, self.players, false);
        op(h, EE_SCREENS_SEUN_OP_REDRAW, [self.players, 0, 0], [0; 4], [0.0; 4]);
        op(h, EE_SCREENS_SEUN_OP_CLEAR_SELECTION, [self.players, 0, 0], [0; 4], [0.0; 4]);
        self.under_mouse.set(-1);
        if op(h, EE_SCREENS_SEUN_OP_MOVING, [self.move_unit, 0, 0], [0; 4], [0.0; 4]) != 0 {
            op(h, EE_SCREENS_SEUN_OP_CANCEL_MOVE, [self.move_unit, 0, 0], [0; 4], [0.0; 4]);
        }
        self.placing.set(false);
        clear_ghosts(h, false);
    }

    /// `ShowCargo`: the cargo title, list and remove button shown or hidden, only when the list's state changes.
    pub fn show_cargo<H: SeplHost + ?Sized>(&self, h: &mut H, show: bool) {
        let mode = call(h, EE_SCREENS_SEPL_OP_DISPLAY, [self.cargo_list, 0, 0], [0; 4]);
        let from = if show { EE_SCREENS_SEPL_HIDDEN } else { EE_SCREENS_SEPL_SHOWN };
        if mode != from {
            return;
        }
        for c in [self.cargo_list, self.cargo_title, self.remove_cargo] {
            display(h, c, show);
        }
    }

    /// `ShowUnitStatus`: the upgrade panel shows the unit (0: none).
    pub fn show_unit_status<H: SeplHost + ?Sized>(&self, h: &mut H, unit: usize) {
        op(h, EE_SCREENS_SEUN_OP_SHOW_UNIT, [self.upgrade_panel, unit, 0], [0; 4], [0.0; 4]);
    }

    /// `Draw`: the panel's background where the panel overlaps the rectangle `area` (a `GERectangle`).
    pub fn draw<H: SeplHost + ?Sized>(&self, h: &mut H, area: usize) {
        op(h, EE_SCREENS_SEUN_OP_DRAW, [self.shell, self.background, area], [0; 4], [0.0; 4]);
    }

    /// `GetLayer`: the owner, 0 World, 1 Players, 2 neither.
    pub fn get_layer<H: SeplHost + ?Sized>(&self, h: &mut H) -> i32 {
        if radio_checked(h, self.world_button) {
            0
        } else {
            !radio_checked(h, self.players_button) as i32 + 1
        }
    }

    /// `GetRotationAngle`: the rotation button's yaw.
    pub fn rotation_angle<H: SeplHost + ?Sized>(&self, h: &mut H) -> f32 {
        let (mut i, mut f) = ([0i32; 4], [0.0f32; 4]);
        h.op(EE_SCREENS_SEUN_OP_ANGLE, [self.rotation, 0, 0], &mut i, &mut f);
        f[0]
    }

    /// `GetSortMode`: 0 name, 1 epoch, 2 family, 3 none checked.
    pub fn get_sort_mode<H: SeplHost + ?Sized>(&self, h: &mut H) -> i32 {
        if radio_checked(h, self.alpha_button) {
            0
        } else if radio_checked(h, self.epoch_button) {
            1
        } else if radio_checked(h, self.family_button) {
            2
        } else {
            3
        }
    }

    /// `Enter`: the game view right of the panel, the player combo under the owner radios (shown with Players
    /// checked), the panel's rectangle dirty, the class list refreshed.
    pub fn enter<H: SeplHost + ?Sized>(&self, h: &mut H) {
        let y = rect(h, self.players_button)[3] + 2.0;
        op(h, EE_SCREENS_SEUN_OP_GAME_VIEW, [0; 3], [1, 0, 0, 0], [0.0; 4]);
        op(h, EE_SCREENS_SEPL_OP_SET_LOCATION, [self.players, 0, 0], [0; 4], [0.0, y, 0.0, 0.0]);
        if radio_checked(h, self.players_button) {
            display(h, self.players, true);
        }
        op(h, EE_SCREENS_SEUN_OP_DIRTY, [self.players, self.shell, 0], [0; 4], [0.0; 4]);
        self.refresh_unit_list(h);
    }

    /// `SelectUnitType`: the list's class picked for placing by the tab's player when it may place it (a wall
    /// technology starts the wall tool, a gate the gate tool), then posted to the load thread (its ghost).
    pub fn select_unit_type<H: SeplHost + ?Sized>(&self, h: &mut H) {
        let line = op(h, EE_SCREENS_SEUN_OP_LIST_SELECTION, [self.unit_list, 0, 0], [0; 4], [0.0; 4]);
        if line < 0 {
            return;
        }
        self.cancel_builds(h);
        clear_ghosts(h, false);
        let player = self.current_player(h);
        let (mut i, mut f) = ([player, line, 0, 0], [0.0f32; 4]);
        let id = h.op(EE_SCREENS_SEUN_OP_PLACE_INFO, [self.unit_list, 0, 0], &mut i, &mut f);
        if i[1] == 0 {
            return;
        }
        self.placing_wall.set(false);
        self.placing_gate.set(false);
        self.placing.set(true);
        if i[2] == EE_SCREENS_SEUN_KIND_WALL {
            self.placing_wall.set(true);
            self.placing.set(true);
            op(h, EE_SCREENS_SEUN_OP_START_BUILD, [self.shell, self.unit_list, 0], [player, line, 0, 0], [0.0; 4]);
        } else if i[2] == EE_SCREENS_SEUN_KIND_GATE {
            self.gate_held.set(false);
            self.placing_gate.set(true);
            self.placing.set(true);
            op(h, EE_SCREENS_SEUN_OP_START_BUILD, [self.shell, self.unit_list, 0], [player, line, 1, 0], [0.0; 4]);
        }
        op(h, EE_SCREENS_SEUN_OP_POST_GHOST, [self.cargo_title, self.unit_list, 0], [line, id, 0, 0], [0.0; 4]);
    }

    fn cancel_builds<H: SeplHost + ?Sized>(&self, h: &mut H) {
        if self.placing_gate.get() {
            op(h, EE_SCREENS_SEUN_OP_CANCEL_BUILD, [self.shell, 0, 0], [1, 0, 0, 0], [0.0; 4]);
        }
        if self.placing_wall.get() {
            op(h, EE_SCREENS_SEUN_OP_CANCEL_BUILD, [self.shell, 0, 0], [0, 0, 0, 0], [0.0; 4]);
        }
    }

    /// `UpdateSelection`: the cargo list refilled from the selected units' cargo, its selected line kept where it
    /// can be (else the last line).
    pub fn update_selection<H: SeplHost + ?Sized>(&self, h: &mut H) {
        let mut keep = op(h, EE_SCREENS_SEUN_OP_LIST_SELECTION, [self.cargo_list, 0, 0], [0; 4], [0.0; 4]);
        if keep < 0 {
            keep = 0;
        }
        let list = self.cargo_list;
        op(h, EE_SCREENS_SEUN_OP_CARGO_CLEAR, [list, 0, 0], [0; 4], [0.0; 4]);
        let count = op(h, EE_SCREENS_SEUN_OP_SELECTED_COUNT, [0, list, 0], [0; 4], [0.0; 4]) as u32;
        for k in 0..count {
            let cargo = op(h, EE_SCREENS_SEUN_OP_CARGO_COUNT, [0, list, 0], [k as i32, 0, 0, 0], [0.0; 4]) as u32;
            for j in 0..cargo {
                op(h, EE_SCREENS_SEUN_OP_CARGO_ADD, [list, list, 0], [k as i32, j as i32, 0, 0], [0.0; 4]);
            }
        }
        let lines = op(h, EE_SCREENS_SEUN_OP_LIST_COUNT, [list, 0, 0], [0; 4], [0.0; 4]) as u32;
        if lines == 0 {
            return;
        }
        let line = if (keep as u32) < lines { keep as u32 } else { lines - 1 };
        op(h, EE_SCREENS_SEUN_OP_LIST_SELECT, [list, 0, 0], [line as i32, 0, 0, 0], [0.0; 4]);
    }

    /// `RefreshUnitList`: when the palette, owner or sort changed, any placement cancelled and the classes the
    /// editor lists, the owner may place and the palette holds listed (sorted by the list's comparator).
    pub fn refresh_unit_list<H: SeplHost + ?Sized>(&self, h: &mut H) {
        let mode = op(h, EE_SCREENS_SEUN_OP_TYPE_MODE, [0; 3], [0; 4], [0.0; 4]);
        let sort = self.get_sort_mode(h);
        let layer = self.get_layer(h);
        if mode == self.type_mode.get() && sort == self.sort_mode.get() && layer == self.layer.get() {
            return;
        }
        self.cancel_builds(h);
        clear_ghosts(h, true);
        display(h, self.players, layer != 0);
        let list = self.unit_list;
        op(h, EE_SCREENS_SEUN_OP_LIST_SORT, [list, 0, 0], [sort, 0, 0, 0], [0.0; 4]);
        op(h, EE_SCREENS_SEUN_OP_LIST_EMPTY, [list, 0, 0], [0; 4], [0.0; 4]);
        let count = op(h, EE_SCREENS_SEUN_OP_CLASS_COUNT, [0; 3], [0; 4], [0.0; 4]);
        for k in 0..count {
            let (mut i, mut f) = ([k, 0, 0, 0], [0.0f32; 4]);
            let c = h.op(EE_SCREENS_SEUN_OP_CLASS, [0; 3], &mut i, &mut f);
            if placeable(c, layer) && in_palette(c, mode) {
                op(h, EE_SCREENS_SEUN_OP_LIST_ADD, [list, 0, 0], [i[1], 0, 0, 0], [0.0; 4]);
            }
        }
        if op(h, EE_SCREENS_SEUN_OP_LIST_COUNT, [list, 0, 0], [0; 4], [0.0; 4]) != 0 {
            op(h, EE_SCREENS_SEUN_OP_LIST_TOP, [list, 0, 0], [0; 4], [0.0; 4]);
            op(h, EE_SCREENS_SEUN_OP_LIST_SELECT, [list, 0, 0], [0; 4], [0.0; 4]);
        }
        self.layer.set(layer);
        self.sort_mode.set(sort);
        self.type_mode.set(mode);
    }

    /// `MouseMove` over the game view at `x`, `y`: without a placement, selects (left), commands (right) or picks
    /// up a unit (a double click; a gate is held for re-placing); while placing a unit, tracks the unit under the
    /// mouse, places (left) or cancels (right release); the wall and gate tools follow the mouse.
    pub fn mouse_move<H: SeplHost + ?Sized>(&self, h: &mut H, x: f32, y: f32) {
        let xy = [x, y, 0.0, 0.0];
        let in_view = |h: &mut H| op(h, EE_SCREENS_SEUN_OP_IN_VIEW, [0; 3], [0; 4], xy) != 0;
        let mouse = |h: &mut H| op(h, EE_SCREENS_SEUN_OP_MOUSE, [0; 3], [0; 4], [0.0; 4]);
        let moving = op(h, EE_SCREENS_SEUN_OP_MOVING, [self.move_unit, 0, 0], [0; 4], [0.0; 4]) != 0;
        if !self.placing.get() && !moving {
            if !in_view(h) {
                return;
            }
            let buttons = mouse(h);
            if buttons & EE_SCREENS_SEUN_MOUSE_L_DOUBLE != 0 {
                let (mut i, mut f) = ([0i32; 4], xy);
                let unit = h.op(EE_SCREENS_SEUN_OP_PICK, [0; 3], &mut i, &mut f);
                if unit == -1 {
                    return;
                }
                if i[1] == 0 {
                    op(h, EE_SCREENS_SEUN_OP_PICK_UP, [self.move_unit, 0, 0], [unit, 0, 0, 0], [0.0; 4]);
                } else {
                    self.placing_gate.set(true);
                    self.placing.set(true);
                    self.gate_held.set(true);
                    op(h, EE_SCREENS_SEUN_OP_HOLD_GATE, [self.shell, 0, 0], [unit, 0, 0, 0], [0.0; 4]);
                }
                return;
            }
            if buttons & EE_SCREENS_SEUN_MOUSE_L_PRESSED != 0 {
                op(h, EE_SCREENS_SEUN_OP_START_SELECTION, [self.select_units, 0, 0], [0; 4], xy);
                return;
            }
            if buttons & EE_SCREENS_SEUN_MOUSE_R_PRESSED != 0 {
                op(h, EE_SCREENS_SEUN_OP_START_COMMAND, [self.command_units, 0, 0], [0; 4], xy);
            }
            return;
        }
        if !self.placing_wall.get() && !self.placing_gate.get() {
            lock(h, true);
            let (mut i, mut f) = ([0i32; 4], xy);
            let unit = h.op(EE_SCREENS_SEUN_OP_PICK, [0; 3], &mut i, &mut f);
            self.under_mouse.set(unit);
            if in_view(h) {
                let buttons = mouse(h);
                if buttons & EE_SCREENS_SEUN_MOUSE_L_PRESSED != 0 {
                    let player = self.current_player(h);
                    if self.placing.get() {
                        op(h, EE_SCREENS_SEUN_OP_PLACE_UNIT, [self.place_unit, 0, 0], [player, 0, 0, 0], [0.0; 4]);
                    } else {
                        op(h, EE_SCREENS_SEUN_OP_PUT_DOWN, [self.move_unit, 0, 0], [0; 4], [0.0; 4]);
                        self.under_mouse.set(-1);
                    }
                } else if buttons & EE_SCREENS_SEUN_MOUSE_R_RELEASED != 0 {
                    clear_ghosts(h, false);
                    self.under_mouse.set(-1);
                    self.placing.set(false);
                    op(h, EE_SCREENS_SEUN_OP_CANCEL_MOVE, [self.move_unit, 0, 0], [0; 4], [0.0; 4]);
                }
            }
            lock(h, false);
        } else if self.placing_wall.get() {
            op(h, EE_SCREENS_SEUN_OP_BUILD_UPDATE, [self.shell, 0, 0], [0; 4], xy);
        } else if self.placing_gate.get() {
            if !self.gate_held.get() {
                op(h, EE_SCREENS_SEUN_OP_BUILD_UPDATE, [self.shell, 0, 0], [1, 0, 0, 0], xy);
            } else {
                let buttons = mouse(h);
                let held =
                    EE_SCREENS_SEUN_MOUSE_L_RELEASED | EE_SCREENS_SEUN_MOUSE_L_DOWN | EE_SCREENS_SEUN_MOUSE_L_PRESSED;
                if buttons & held == 0 {
                    self.gate_held.set(false);
                }
            }
        }
    }

    /// `UpdateKeys`: Delete deletes the selected units (Shift: the action's group flag); then the rotation keys.
    pub fn update_keys<H: SeplHost + ?Sized>(&self, h: &mut H) {
        if op(h, EE_SCREENS_SEUN_OP_SELECTED_COUNT, [0, self.rotation, 0], [0; 4], [0.0; 4]) as u32 > 0 {
            let keys = op(h, EE_SCREENS_SEUN_OP_KEYS, [0; 3], [0; 4], [0.0; 4]);
            if keys & 1 != 0 {
                op(h, EE_SCREENS_SEUN_OP_DELETE_SELECTION, [0; 3], [(keys >> 1) & 1, 0, 0, 0], [0.0; 4]);
            }
        }
        op(h, EE_SCREENS_SEUN_OP_ROTATION_KEYS, [self.rotation, 0, 0], [0; 4], [0.0; 4]);
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::collections::HashMap;

    /// A recording host: the controls as records, radios, displays, the list's lines, the classes.
    #[derive(Default)]
    struct Rec {
        made: Vec<EeScreensSeplCtl>,
        next: usize,
        radio: HashMap<usize, bool>,
        mode: HashMap<usize, i32>,
        classes: Vec<i32>,
        listed: Vec<i32>,
        list_selection: i32,
        place: (i32, i32, i32),
        type_mode: i32,
        cargo: Vec<u32>,
        cargo_lines: u32,
        keep: i32,
        cargo_handle: usize,
        buttons: i32,
        pick: (i32, i32),
        log: Vec<(u32, [i32; 4])>,
    }

    impl SeplHost for Rec {
        fn op(&mut self, op: u32, p: [usize; 3], i: &mut [i32; 4], f: &mut [f32; 4]) -> i32 {
            let out = |v: usize| unsafe { *(p[0] as *mut usize) = v };
            match op {
                EE_SCREENS_SEPL_OP_MAKE => {
                    let c = unsafe { *(p[1] as *const EeScreensSeplCtl) };
                    self.made.push(c);
                    out(self.made.len());
                    self.radio.insert(self.made.len(), c.kind == EE_SCREENS_SEPL_RADIO && c.extra[0] != 0);
                    self.mode.insert(self.made.len(), EE_SCREENS_SEPL_SHOWN);
                }
                EE_SCREENS_SEPL_OP_RECT => {
                    let c = self.made[p[0] - 1];
                    *f = [c.x, c.y, c.x + c.w - 1.0, c.y + 20.0 - 1.0];
                }
                EE_SCREENS_SEPL_OP_RADIO_IS_CHECKED => return *self.radio.get(&p[0]).unwrap_or(&false) as i32,
                EE_SCREENS_SEPL_OP_COMBO_SELECTION => return 2,
                EE_SCREENS_SEPL_OP_DISPLAY => return *self.mode.get(&p[0]).unwrap_or(&EE_SCREENS_SEPL_SHOWN),
                EE_SCREENS_SEPL_OP_SET_DISPLAY => {
                    self.mode.insert(p[0], i[0]);
                }
                EE_SCREENS_SEUN_OP_TYPE_BUTTON => {
                    let o = p[0] as *mut usize;
                    self.next += 3;
                    unsafe { *o = 9000 + self.next };
                    unsafe { *o.add(1) = 9001 + self.next };
                    unsafe { *o.add(2) = 9002 + self.next };
                }
                EE_SCREENS_SEUN_OP_SORT_EVENT
                | EE_SCREENS_SEUN_OP_REMOVE_CARGO
                | EE_SCREENS_SEUN_OP_ROTATION
                | EE_SCREENS_SEUN_OP_ACTION
                | EE_SCREENS_SEUN_OP_SELECT_UNITS
                | EE_SCREENS_SEUN_OP_COMMAND_UNITS
                | EE_SCREENS_SEUN_OP_BACKGROUND => {
                    self.next += 1;
                    out(8000 + self.next);
                }
                EE_SCREENS_SEUN_OP_UPGRADE_PANEL => {
                    out(7000);
                    i[0] += 2;
                }
                EE_SCREENS_SEUN_OP_TYPE_MODE => return self.type_mode,
                EE_SCREENS_SEUN_OP_CLASS_COUNT => return self.classes.len() as i32,
                EE_SCREENS_SEUN_OP_CLASS => {
                    i[1] = i[0] + 100;
                    return self.classes[i[0] as usize];
                }
                EE_SCREENS_SEUN_OP_LIST_EMPTY => self.listed.clear(),
                EE_SCREENS_SEUN_OP_LIST_ADD => self.listed.push(i[0]),
                EE_SCREENS_SEUN_OP_LIST_COUNT => {
                    return if p[0] == self.cargo_handle { self.cargo_lines as i32 } else { self.listed.len() as i32 };
                }
                EE_SCREENS_SEUN_OP_LIST_SELECTION => {
                    return if p[0] == self.cargo_handle { self.keep } else { self.list_selection };
                }
                EE_SCREENS_SEUN_OP_PLACE_INFO => {
                    i[1] = self.place.1;
                    i[2] = self.place.2;
                    return self.place.0;
                }
                EE_SCREENS_SEUN_OP_SELECTED_COUNT => return self.cargo.len() as i32,
                EE_SCREENS_SEUN_OP_CARGO_COUNT => return self.cargo[i[0] as usize] as i32,
                EE_SCREENS_SEUN_OP_CARGO_ADD => self.cargo_lines += 1,
                EE_SCREENS_SEUN_OP_MOUSE => return self.buttons,
                EE_SCREENS_SEUN_OP_IN_VIEW => return 1,
                EE_SCREENS_SEUN_OP_PICK => {
                    i[1] = self.pick.1;
                    return self.pick.0;
                }
                _ => {}
            }
            self.log.push((op, *i));
            0
        }
    }

    fn logged(r: &Rec, op: u32) -> Vec<[i32; 4]> {
        r.log.iter().filter(|(o, _)| *o == op).map(|(_, i)| *i).collect()
    }

    fn tab(r: &mut Rec) -> UnitTab {
        let mut index = 55;
        let u = UnitTab::new(r, 1, 2, 3, &mut index);
        assert_eq!(index, 55 + 19 + 2);
        r.cargo_handle = u.cargo_list;
        u
    }

    #[test]
    fn the_constructor_makes_the_controls_in_order_at_their_indexes() {
        let mut r = Rec::default();
        let u = tab(&mut r);
        let ids: Vec<(u32, u32)> = r.made.iter().map(|c| (c.id, c.index)).collect();
        assert_eq!(
            ids,
            [
                (0x3f3, 55),
                (1000, 56),
                (0x3ea, 57),
                (0x3ed, 58),
                (0x3f1, 65),
                (0x3ee, 66),
                (0x3ef, 67),
                (0x3f0, 68),
                (0x3f2, 69)
            ]
        );
        // the type buttons at 59..64, 35 apart, the bitmaps paired
        let buttons = logged(&r, EE_SCREENS_SEUN_OP_TYPE_BUTTON);
        assert_eq!(buttons.iter().map(|i| (i[0], i[1], i[3])).collect::<Vec<_>>()[5], (64, 5, 175));
        assert_eq!(u.bitmaps[11], u.type_buttons[5] + 2);
        // the remove button right of the cargo list, rotation after it
        assert_eq!(logged(&r, EE_SCREENS_SEUN_OP_REMOVE_CARGO)[0][0], 70);
        assert_eq!(logged(&r, EE_SCREENS_SEUN_OP_ROTATION)[0][0], 71);
        // the radios: Players and Alpha checked, under each other
        assert!(r.made[2].extra[0] != 0 && r.made[5].extra[0] != 0 && r.made[6].extra[0] == 0);
        assert_eq!(r.made[2].y, 30.0 + 20.0 - 1.0 + 2.0);
        assert_eq!(r.made[7].y, r.made[6].y + 21.0);
        // the cargo hidden at the end
        assert_eq!(r.mode[&u.cargo_list], EE_SCREENS_SEPL_HIDDEN);
        assert_eq!(r.mode[&u.cargo_title], EE_SCREENS_SEPL_HIDDEN);
        assert_eq!((u.layer.get(), u.sort_mode.get(), u.type_mode.get(), u.under_mouse.get()), (2, 3, 6, -1));
    }

    #[test]
    fn palettes_follow_the_original_chain() {
        let hero_air = EE_SCREENS_SEUN_CLASS_HERO | EE_SCREENS_SEUN_CLASS_AIR;
        assert!(in_palette(hero_air, EE_SCREENS_SEUN_PALETTE_HEROES));
        let air_building = EE_SCREENS_SEUN_CLASS_AIR | EE_SCREENS_SEUN_CLASS_BUILDING;
        assert!(in_palette(air_building, EE_SCREENS_SEUN_PALETTE_AIR));
        let building_land = EE_SCREENS_SEUN_CLASS_BUILDING | EE_SCREENS_SEUN_CLASS_LAND;
        assert!(in_palette(building_land, EE_SCREENS_SEUN_PALETTE_STRUCTURES));
        assert!(in_palette(EE_SCREENS_SEUN_CLASS_LAND | EE_SCREENS_SEUN_CLASS_SEA, EE_SCREENS_SEUN_PALETTE_LAND));
        assert!(in_palette(EE_SCREENS_SEUN_CLASS_SEA, EE_SCREENS_SEUN_PALETTE_NAVAL));
        assert!(in_palette(EE_SCREENS_SEUN_CLASS_SPACE, EE_SCREENS_SEUN_PALETTE_AIR));
        assert!(in_palette(0, EE_SCREENS_SEUN_PALETTE_RESOURCES));
        let s = EE_SCREENS_SEUN_CLASS_SELECTABLE;
        assert!(placeable(s | EE_SCREENS_SEUN_CLASS_GAIA, 0) && !placeable(s | EE_SCREENS_SEUN_CLASS_GAIA, 1));
        assert!(placeable(s | EE_SCREENS_SEUN_CLASS_PLAYER, 1) && !placeable(s | EE_SCREENS_SEUN_CLASS_PLAYER, 2));
        assert!(!placeable(EE_SCREENS_SEUN_CLASS_GAIA | EE_SCREENS_SEUN_CLASS_PLAYER, 0));
    }

    #[test]
    fn refresh_lists_only_on_a_change_and_selects_the_first_line() {
        let mut r = Rec::default();
        let u = tab(&mut r);
        let s = EE_SCREENS_SEUN_CLASS_SELECTABLE | EE_SCREENS_SEUN_CLASS_PLAYER;
        r.classes =
            vec![s | EE_SCREENS_SEUN_CLASS_LAND, s | EE_SCREENS_SEUN_CLASS_SEA, s, s | EE_SCREENS_SEUN_CLASS_LAND];
        r.type_mode = EE_SCREENS_SEUN_PALETTE_LAND;
        r.log.clear();
        u.refresh_unit_list(&mut r);
        assert_eq!(r.listed, [100, 103]);
        assert_eq!(logged(&r, EE_SCREENS_SEUN_OP_LIST_SORT), [[0, 0, 0, 0]]);
        assert_eq!(logged(&r, EE_SCREENS_SEUN_OP_LIST_SELECT), [[0, 0, 0, 0]]);
        assert_eq!(logged(&r, EE_SCREENS_SEUN_OP_CLEAR_GHOSTS), [[1, 0, 0, 0]]);
        assert_eq!((u.layer.get(), u.sort_mode.get(), u.type_mode.get()), (1, 0, 0));
        r.log.clear();
        u.refresh_unit_list(&mut r);
        assert!(r.log.is_empty());
        // World checked: gaia's classes, the combo hidden
        r.radio.insert(u.world_button, true);
        r.radio.insert(u.players_button, false);
        u.refresh_unit_list(&mut r);
        assert!(r.listed.is_empty());
        assert_eq!(r.mode[&u.players], EE_SCREENS_SEPL_HIDDEN);
    }

    #[test]
    fn selecting_a_class_starts_the_right_placement() {
        let mut r = Rec::default();
        let u = tab(&mut r);
        r.list_selection = -1;
        u.select_unit_type(&mut r);
        assert!(!u.placing.get());
        r.list_selection = 4;
        r.place = (3, 0, 0);
        u.select_unit_type(&mut r);
        assert!(!u.placing.get());
        r.place = (3, 1, EE_SCREENS_SEUN_KIND_WALL);
        r.log.clear();
        u.select_unit_type(&mut r);
        assert!(u.placing.get() && u.placing_wall.get() && !u.placing_gate.get());
        assert_eq!(logged(&r, EE_SCREENS_SEUN_OP_START_BUILD), [[3, 4, 0, 0]]);
        assert_eq!(logged(&r, EE_SCREENS_SEUN_OP_POST_GHOST), [[4, 3, 0, 0]]);
        // a gate next: the wall tool cancelled first
        r.place = (3, 1, EE_SCREENS_SEUN_KIND_GATE);
        r.log.clear();
        u.select_unit_type(&mut r);
        assert_eq!(logged(&r, EE_SCREENS_SEUN_OP_CANCEL_BUILD), [[0, 0, 0, 0]]);
        assert!(u.placing_gate.get() && !u.placing_wall.get() && !u.gate_held.get());
        u.end_gate(&mut r);
        assert!(!u.placing.get() && !u.placing_gate.get());
    }

    #[test]
    fn the_cargo_keeps_its_line_or_takes_the_last() {
        let mut r = Rec::default();
        let u = tab(&mut r);
        r.cargo = vec![2, 0, 1];
        r.keep = 5;
        r.log.clear();
        u.update_selection(&mut r);
        assert_eq!(r.cargo_lines, 3);
        assert_eq!(logged(&r, EE_SCREENS_SEUN_OP_LIST_SELECT), [[2, 0, 0, 0]]);
        r.cargo_lines = 0;
        r.keep = 1;
        r.log.clear();
        u.update_selection(&mut r);
        assert_eq!(logged(&r, EE_SCREENS_SEUN_OP_LIST_SELECT), [[1, 0, 0, 0]]);
        // shown once, hidden once
        u.show_cargo(&mut r, true);
        assert_eq!(r.mode[&u.remove_cargo], EE_SCREENS_SEPL_SHOWN);
    }

    #[test]
    fn the_mouse_places_cancels_and_holds_gates() {
        let mut r = Rec::default();
        let u = tab(&mut r);
        r.pick = (77, 1);
        r.buttons = EE_SCREENS_SEUN_MOUSE_L_DOUBLE;
        u.mouse_move(&mut r, 300.0, 200.0);
        assert!(u.placing.get() && u.placing_gate.get() && u.gate_held.get());
        assert_eq!(logged(&r, EE_SCREENS_SEUN_OP_HOLD_GATE), [[77, 0, 0, 0]]);
        r.buttons = EE_SCREENS_SEUN_MOUSE_L_DOWN;
        u.mouse_move(&mut r, 300.0, 200.0);
        assert!(u.gate_held.get());
        r.buttons = 0;
        u.mouse_move(&mut r, 300.0, 200.0);
        assert!(!u.gate_held.get());
        u.end_gate(&mut r);
        // placing a unit: the unit under the mouse tracked, a right release cancels
        u.placing.set(true);
        r.buttons = EE_SCREENS_SEUN_MOUSE_L_PRESSED;
        r.log.clear();
        u.mouse_move(&mut r, 300.0, 200.0);
        assert_eq!(u.under_mouse.get(), 77);
        assert_eq!(logged(&r, EE_SCREENS_SEUN_OP_PLACE_UNIT), [[3, 0, 0, 0]]);
        assert_eq!(logged(&r, EE_SCREENS_SEUN_OP_LOCK), [[1, 0, 0, 0], [0, 0, 0, 0]]);
        r.buttons = EE_SCREENS_SEUN_MOUSE_R_RELEASED;
        u.mouse_move(&mut r, 300.0, 200.0);
        assert!(!u.placing.get());
        assert_eq!(u.under_mouse.get(), -1);
    }

    #[test]
    fn the_destructor_releases_everything_once() {
        let mut r = Rec::default();
        let u = tab(&mut r);
        r.log.clear();
        u.destroy(&mut r);
        assert_eq!(logged(&r, EE_SCREENS_SEPL_OP_DELETE).len(), 4 + 6 + 9);
        let released = logged(&r, EE_SCREENS_SEUN_OP_RELEASE);
        assert_eq!(released.len(), 6 + 12 + 1 + 1 + 1);
        assert_eq!(released.last().unwrap()[0], 3);
    }
}
