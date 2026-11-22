//! The unit tab's host ops that were C++ statement sequences (`ee_screenscpp_seun`, Scenario Editor/EESEUnit.cpp;
//! the scenario editor lane's round 4): the type buttons, the cargo and rotation buttons, the tools, the panel's
//! background, the build tools' wall and gate (placement, previews at the mouse, start, hold, cancel), the game
//! view, the class pick, the mouse and keys; composed here of single C++ statements (`EE_SCREENS_SEUNX_*`,
//! `ee_screenscpp_seunx`) in the C++'s order. The placement action's calls (`ee_screens_sew_*`) are Rust and are
//! called through [`Sew`]. [`compose`] answers an `EE_SCREENS_SEUN_OP_*` op or `None` for the C++'s.

// c_long is 32 bits on Windows, 64 elsewhere: its casts stay for the other targets
#![allow(clippy::unnecessary_cast)]

use std::os::raw::{c_long, c_ulong};

use ee_base::math::types::Ge2DPoint;

use crate::screens::editor_unit::*;

// ---- the statements (p0 the made object's out slot, else as named; p1, p2 the operands)
/// `new GEBitmap`; the form p1's `GetButtonTexture()`; texture p1's `GetWidth()`; bitmap p0's `SetTexture(texture
/// p1, form p2's screen)`; bitmap p1's `SetTextureCoordinates` of the corners (f0, f1)-(f2, f3), `SetXYScale(f0,
/// f1)`.
pub const EE_SCREENS_SEUNX_NEW_BITMAP: u32 = 0;
pub const EE_SCREENS_SEUNX_BUTTON_TEXTURE: u32 = 1;
pub const EE_SCREENS_SEUNX_TEXTURE_WIDTH: u32 = 2;
pub const EE_SCREENS_SEUNX_BITMAP_TEXTURE_FORM: u32 = 3;
pub const EE_SCREENS_SEUNX_BITMAP_SCALE: u32 = 5;
/// A unit type button (`EEUIShellRadio`, index i0, id i1, checked i2, the rectangle f) on form p1; its
/// up checked, down checked, down unchecked, up unchecked bitmaps (p2), no focused bitmap, the draw offset 0, the
/// button-up event p2, the string id i0; added to form p1 (p2 the radio, layer i0).
pub const EE_SCREENS_SEUNX_TYPE_RADIO: u32 = 6;
pub const EE_SCREENS_SEUNX_RADIO_UP_CHECKED: u32 = 7;
pub const EE_SCREENS_SEUNX_RADIO_DOWN_CHECKED: u32 = 8;
pub const EE_SCREENS_SEUNX_RADIO_DOWN_UNCHECKED: u32 = 9;
pub const EE_SCREENS_SEUNX_RADIO_UP_UNCHECKED: u32 = 10;
pub const EE_SCREENS_SEUNX_RADIO_NO_FOCUS: u32 = 11;
pub const EE_SCREENS_SEUNX_RADIO_DRAW_OFFSET: u32 = 12;
pub const EE_SCREENS_SEUNX_RADIO_TRIGGER: u32 = 13;
pub const EE_SCREENS_SEUNX_RADIO_STRING_ID: u32 = 14;
pub const EE_SCREENS_SEUNX_ADD_RADIO: u32 = 15;
/// `IDS_SE_REMOVE` into the host's text; button p1's `SetText(the text, the screen font)`, `SetBgOpacity(1)`, its
/// text white and transparent; added to form p1 (p2, layer i0); button p1's `SetStringTableID(IDS_ROTATE)`.
pub const EE_SCREENS_SEUNX_STRING_REMOVE: u32 = 16;
pub const EE_SCREENS_SEUNX_BUTTON_SET_TEXT: u32 = 18;
pub const EE_SCREENS_SEUNX_BUTTON_OPAQUE: u32 = 19;
pub const EE_SCREENS_SEUNX_BUTTON_TEXT_WHITE: u32 = 20;
pub const EE_SCREENS_SEUNX_BUTTON_TEXT_TRANSPARENT: u32 = 21;
pub const EE_SCREENS_SEUNX_ADD_BUTTON: u32 = 22;
pub const EE_SCREENS_SEUNX_ROTATION_STRING: u32 = 24;
/// The actions: `new EESEAMouseClick`, `new EESEALasso`, `new EESEARust(kSEAPlaceUnit)`, `(kSEAPlaceGate)`,
/// `(kSEAPlaceWall)`, `(kSEAMoveUnit)`.
pub const EE_SCREENS_SEUNX_ACTION_CLICK: u32 = 25;
pub const EE_SCREENS_SEUNX_ACTION_LASSO: u32 = 26;
pub const EE_SCREENS_SEUNX_ACTION_PLACE_UNIT: u32 = 27;
pub const EE_SCREENS_SEUNX_ACTION_PLACE_GATE: u32 = 28;
pub const EE_SCREENS_SEUNX_ACTION_PLACE_WALL: u32 = 29;
pub const EE_SCREENS_SEUNX_ACTION_MOVE_UNIT: u32 = 30;
/// The selection tool (form p1, the tools p2, index i0) and its add to form p1 (p2); the command tool (form p1, the
/// click p2, index i0) and its add.
pub const EE_SCREENS_SEUNX_SELECT_UNITS: u32 = 31;
pub const EE_SCREENS_SEUNX_ADD_SELECT_UNITS: u32 = 32;
pub const EE_SCREENS_SEUNX_COMMAND_UNITS: u32 = 33;
pub const EE_SCREENS_SEUNX_ADD_COMMAND_UNITS: u32 = 34;
/// `textures\scenunit` loaded (mip maps off); texture p1's `SetFrame(0)`, alpha and colour modes texture, min and
/// mag filters 0, no mip filter, mip map bias -1, wrap addressing; bitmap p1's `SetTexture(texture p2,
/// gScreenIndex)`.
pub const EE_SCREENS_SEUNX_LOAD_SCENUNIT: u32 = 35;
pub const EE_SCREENS_SEUNX_TEX_FRAME: u32 = 36;
pub const EE_SCREENS_SEUNX_TEX_ALPHA: u32 = 37;
pub const EE_SCREENS_SEUNX_TEX_COLOR: u32 = 38;
pub const EE_SCREENS_SEUNX_TEX_MIN: u32 = 39;
pub const EE_SCREENS_SEUNX_TEX_MAG: u32 = 40;
pub const EE_SCREENS_SEUNX_TEX_MIP: u32 = 41;
pub const EE_SCREENS_SEUNX_TEX_BIAS: u32 = 42;
pub const EE_SCREENS_SEUNX_TEX_WRAP: u32 = 43;
pub const EE_SCREENS_SEUNX_BITMAP_TEXTURE_SCREEN: u32 = 44;
/// The upgrade panel (p2 the id and the index, two `unsigned long`s it takes by reference, form p1); p1's `Dereference()` as
/// an action, a bitmap, an event; the panel p1 deleted.
pub const EE_SCREENS_SEUNX_UPGRADE_PANEL: u32 = 45;
pub const EE_SCREENS_SEUNX_RELEASE_ACTION: u32 = 46;
pub const EE_SCREENS_SEUNX_RELEASE_BITMAP: u32 = 47;
pub const EE_SCREENS_SEUNX_RELEASE_EVENT: u32 = 48;
pub const EE_SCREENS_SEUNX_DELETE_PANEL: u32 = 49;
/// The placement action p1's Rust action (`mAction`); the tab p1's wall start and end (f0, f1), technology, player,
/// state; its gate's tile x, y, technology, direction.
pub const EE_SCREENS_SEUNX_SEW_ACTION: u32 = 50;
pub const EE_SCREENS_SEUNX_WALL_START: u32 = 51;
pub const EE_SCREENS_SEUNX_WALL_END: u32 = 52;
pub const EE_SCREENS_SEUNX_WALL_TECH: u32 = 53;
pub const EE_SCREENS_SEUNX_WALL_PLAYER: u32 = 54;
pub const EE_SCREENS_SEUNX_WALL_STATE: u32 = 55;
pub const EE_SCREENS_SEUNX_GATE_TILE_X: u32 = 56;
pub const EE_SCREENS_SEUNX_GATE_TILE_Y: u32 = 57;
pub const EE_SCREENS_SEUNX_GATE_TECH: u32 = 58;
pub const EE_SCREENS_SEUNX_GATE_DIRECTION: u32 = 59;
/// `gWorld.mGhostUnitsLock->Enter()`, `->Leave()`, `gWorld.ClearGhostUnits()`, `EEPreviewPlacement(the mouse)`.
pub const EE_SCREENS_SEUNX_LOCK_ENTER: u32 = 60;
pub const EE_SCREENS_SEUNX_LOCK_LEAVE: u32 = 61;
pub const EE_SCREENS_SEUNX_CLEAR_GHOSTS: u32 = 62;
pub const EE_SCREENS_SEUNX_PREVIEW_PLACEMENT: u32 = 63;
/// `EEPointUnitOrTileXY(the mouse, *p0, *p1, *p2, true)` (p1, p2 `long`s); the host's tiles cleared, (i0, i1)
/// pushed, `EEPreviewWall(the tiles, tab p1's wall technology, player)`.
pub const EE_SCREENS_SEUNX_MOUSE_TILE: u32 = 64;
pub const EE_SCREENS_SEUNX_TILES_CLEAR: u32 = 65;
pub const EE_SCREENS_SEUNX_TILES_PUSH: u32 = 66;
pub const EE_SCREENS_SEUNX_PREVIEW_WALL: u32 = 67;
/// `EEFindGateWall(tile (i0, i1), *p2)` (p2 a `long`); the tab p1's gate ghost (`CreateGate`); `PreviewGate(the
/// mouse, *p2)` (it may turn it); the gate's direction set to i0.
pub const EE_SCREENS_SEUNX_FIND_GATE_WALL: u32 = 68;
pub const EE_SCREENS_SEUNX_CREATE_GATE: u32 = 69;
pub const EE_SCREENS_SEUNX_PREVIEW_GATE: u32 = 70;
pub const EE_SCREENS_SEUNX_SET_DIRECTION: u32 = 71;
/// `EESetGameViewRectangle(f)`, `gB529066 = false`, `gCameraFollowing = false`, `gB529068 = false`, `gMiniMap !=
/// NULL`, its `RefreshView()`, `gUIManager->RedrawAll()`, the current form's `SetAllControlsDirty()`.
pub const EE_SCREENS_SEUNX_GAME_VIEW: u32 = 72;
pub const EE_SCREENS_SEUNX_CLEAR_B529066: u32 = 73;
pub const EE_SCREENS_SEUNX_CLEAR_FOLLOWING: u32 = 74;
pub const EE_SCREENS_SEUNX_CLEAR_B529068: u32 = 75;
pub const EE_SCREENS_SEUNX_MINIMAP: u32 = 76;
pub const EE_SCREENS_SEUNX_MINIMAP_REFRESH: u32 = 77;
pub const EE_SCREENS_SEUNX_REDRAW_ALL: u32 = 78;
pub const EE_SCREENS_SEUNX_ALL_DIRTY: u32 = 79;
/// The tab p1's panel rectangle `And` the rectangle p2 into the rectangle *p0; the form's `DrawBackground(bitmap
/// p2, the rectangle *p0, (0, 0, 255, 255), the panel)`; the rotation control p1's angle (bits); the panel's left,
/// top, right, bottom set to f0; the panel added dirty to control p2's form.
pub const EE_SCREENS_SEUNX_PANEL_AND: u32 = 80;
pub const EE_SCREENS_SEUNX_DRAW_PANEL: u32 = 81;
pub const EE_SCREENS_SEUNX_ROTATION_ANGLE: u32 = 82;
pub const EE_SCREENS_SEUNX_PANEL_LEFT: u32 = 83;
pub const EE_SCREENS_SEUNX_PANEL_TOP: u32 = 84;
pub const EE_SCREENS_SEUNX_PANEL_RIGHT: u32 = 85;
pub const EE_SCREENS_SEUNX_PANEL_BOTTOM: u32 = 86;
pub const EE_SCREENS_SEUNX_PANEL_DIRTY: u32 = 87;
/// The tab p1's gate and wall `Cancel()`.
pub const EE_SCREENS_SEUNX_GATE_CANCEL: u32 = 88;
pub const EE_SCREENS_SEUNX_WALL_CANCEL: u32 = 89;
/// The class pick: `gWorld.GetPlayer(i0)`, list p1's unit type at line i0, `gUnitClassTable[i0] != NULL`, its
/// `mTechnology`, player p1's tech tree node i0, `gUnitClassTable[i0]`; player p1's id; class p1's
/// `mIsGaiaPlaceable`, `mIsPlayerPlaceable`, its gate category; node p1's `mData->mKind`, `mData->mID`,
/// `GetUnitClass(0)`.
pub const EE_SCREENS_SEUNX_PLAYER: u32 = 90;
pub const EE_SCREENS_SEUNX_TYPE_AT: u32 = 91;
pub const EE_SCREENS_SEUNX_CLASS_EXISTS: u32 = 92;
pub const EE_SCREENS_SEUNX_CLASS_TECH: u32 = 93;
pub const EE_SCREENS_SEUNX_TREE_NODE: u32 = 94;
pub const EE_SCREENS_SEUNX_CLASS: u32 = 95;
pub const EE_SCREENS_SEUNX_PLAYER_ID: u32 = 96;
pub const EE_SCREENS_SEUNX_GAIA_PLACEABLE: u32 = 97;
pub const EE_SCREENS_SEUNX_PLAYER_PLACEABLE: u32 = 98;
pub const EE_SCREENS_SEUNX_CLASS_IS_GATE: u32 = 99;
pub const EE_SCREENS_SEUNX_NODE_KIND: u32 = 100;
pub const EE_SCREENS_SEUNX_NODE_ID: u32 = 101;
pub const EE_SCREENS_SEUNX_NODE_UNIT_CLASS: u32 = 102;
/// The tab p1's wall: technology = i0, `Initialize(i0)`; its gate: technology = i0, wall type = i0, unit class = p2,
/// `Initialize(i0)`.
pub const EE_SCREENS_SEUNX_WALL_SET_TECH: u32 = 103;
pub const EE_SCREENS_SEUNX_WALL_INITIALIZE: u32 = 104;
pub const EE_SCREENS_SEUNX_GATE_SET_TECH: u32 = 105;
pub const EE_SCREENS_SEUNX_GATE_SET_WALL_TYPE: u32 = 106;
pub const EE_SCREENS_SEUNX_GATE_SET_CLASS: u32 = 107;
pub const EE_SCREENS_SEUNX_GATE_INITIALIZE: u32 = 108;
/// Control p1's editor's load thread `Post402(type i0, i1)`; its selected unit i0's complex unit; complex unit p1's
/// cargo count.
pub const EE_SCREENS_SEUNX_POST_402: u32 = 109;
pub const EE_SCREENS_SEUNX_SELECTED_COMPLEX: u32 = 110;
pub const EE_SCREENS_SEUNX_CARGO_UNITS: u32 = 111;
/// The mouse buttons: left just double pressed, just pressed, just unpressed, down; right just pressed, just
/// unpressed.
pub const EE_SCREENS_SEUNX_L_DOUBLE: u32 = 112;
pub const EE_SCREENS_SEUNX_L_PRESSED: u32 = 113;
pub const EE_SCREENS_SEUNX_L_RELEASED: u32 = 114;
pub const EE_SCREENS_SEUNX_L_DOWN: u32 = 115;
pub const EE_SCREENS_SEUNX_R_PRESSED: u32 = 116;
pub const EE_SCREENS_SEUNX_R_RELEASED: u32 = 117;
/// `gUnitPicker.PickUnit((f0, f1), 0)`; unit p1's `mUnitID`, its gate category, `Dereference()`;
/// `gWorld.GetUnit(i0)`; the tab p1's gate from unit p2: technology, wall type, class; unit p1's `Kill(0, 0, 0)`;
/// the gate's `Initialize(unit p2's player)`.
pub const EE_SCREENS_SEUNX_PICK_UNIT: u32 = 118;
pub const EE_SCREENS_SEUNX_UNIT_ID: u32 = 119;
pub const EE_SCREENS_SEUNX_UNIT_IS_GATE: u32 = 120;
pub const EE_SCREENS_SEUNX_UNIT_DEREFERENCE: u32 = 121;
pub const EE_SCREENS_SEUNX_GET_UNIT: u32 = 122;
pub const EE_SCREENS_SEUNX_GATE_TECH_OF_UNIT: u32 = 123;
pub const EE_SCREENS_SEUNX_GATE_WALL_OF_UNIT: u32 = 124;
pub const EE_SCREENS_SEUNX_GATE_CLASS_OF_UNIT: u32 = 125;
pub const EE_SCREENS_SEUNX_UNIT_KILL: u32 = 126;
pub const EE_SCREENS_SEUNX_GATE_INITIALIZE_OF_UNIT: u32 = 127;
/// The tab p1's gate and wall `Update((f0, f1))`.
pub const EE_SCREENS_SEUNX_GATE_UPDATE: u32 = 128;
pub const EE_SCREENS_SEUNX_WALL_UPDATE: u32 = 129;
/// The keyboard's Delete and Shift entities; entity p1 just pressed, its value nonzero.
pub const EE_SCREENS_SEUNX_KEY_DELETE: u32 = 130;
pub const EE_SCREENS_SEUNX_KEY_SHIFT: u32 = 131;
pub const EE_SCREENS_SEUNX_KEY_PRESSED: u32 = 132;
pub const EE_SCREENS_SEUNX_KEY_DOWN: u32 = 133;
/// `new EESEARust(kSEADeleteUnit, NULL, -1, player i1, i0 != 0)`.
pub const EE_SCREENS_SEUNX_DELETE_ACTION: u32 = 134;

// round 6 (4000..)
/// Bitmap p1's Rust object (`EeGeBitmap`); Rust's own statement (`ge_op`): the bitmap p0's
/// `SetTextureCoordinates((f0, f1), (f2, f1), (f0, f3), (f2, f3))` (the original's corner order).
pub const EE_SCREENS_SEUNX_BITMAP_RUST: u32 = 4000;
pub const EE_SCREENS_SEUNX_GE_UVS: u32 = 4001;
/// `new EEUIShellButton(kind i1, id i2, no bitmaps, the rectangle f, form p1, index i0, border i3)`.
pub const EE_SCREENS_SEUNX_NEW_BUTTON: u32 = 4002;
/// The current player's id.
pub const EE_SCREENS_SEUNX_CURRENT_PLAYER: u32 = 4003;

/// The buttons' kinds and ids (the Remove Cargo button 0x3f4, the Rotation button 0x3f5).
const SEUNX_KIND_REMOVE_CARGO: i32 = crate::screens::shells_unit::EE_SCREENS_SK_REMOVE_CARGO as i32;
const SEUNX_KIND_ROTATION: i32 = crate::screens::shells_unit::EE_SCREENS_SK_ROTATION as i32;

/// Rust's own statements (`EE_SCREENS_SEUNX_GE_*` on the bitmaps' Rust objects), `None` for the C++'s.
///
/// # Safety
/// The operands are the live objects the statement names.
pub unsafe fn ge_op(op: u32, p: [usize; 3], f: &[f32; 4]) -> Option<i32> {
    if op != EE_SCREENS_SEUNX_GE_UVS {
        return None;
    }
    // GETextureCoordinate is u, v
    let mut uv = [[f[0], f[1]], [f[2], f[1]], [f[0], f[3]], [f[2], f[3]]];
    let quad = uv.each_mut().map(|c| c.as_mut_ptr().cast::<core::ffi::c_void>());
    unsafe { ee_render::ge::bmpobj::ee_ge_bmp_set_uvs(p[0] as *mut ee_render::ge::bmpobj::EeGeBitmap, &quad) };
    Some(0)
}

/// The bitmap `bitmap`'s texture coordinates over a cell: `(u0, v0), (u1, v0), (u0, v1), (u1, v1)` (the original's
/// corner order; the diplomacy buttons' cells).
///
/// # Safety
/// `bitmap` is a live bitmap.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_bitmap_uvs(
    bitmap: *mut ee_render::ge::bmpobj::EeGeBitmap,
    u0: f32,
    v0: f32,
    u1: f32,
    v1: f32,
) {
    unsafe { ge_op(EE_SCREENS_SEUNX_GE_UVS, [bitmap as usize, 0, 0], &[u0, v0, u1, v1]) };
}

/// `SeplColor`'s colour for a made control's flags (`EE_SCREENS_SEPL_FG*`): white, red or yellow, none without one.
pub fn sepl_color(flags: u32) -> Option<[f32; 3]> {
    use crate::screens::editor_area::EE_SCREENS_SEPL_FG_RED as RED;
    use crate::screens::editor_map::EE_SCREENS_SEPL_FG_WHITE as WHITE;
    use crate::screens::editor_players::EE_SCREENS_SEPL_FG as FG;
    if flags & (FG | WHITE | RED) == 0 {
        return None;
    }
    Some(if flags & WHITE != 0 {
        [1.0, 1.0, 1.0]
    } else if flags & RED != 0 {
        [1.0, 0.0, 0.0]
    } else {
        [0.996_078_43, 0.968_627_45, 0.125_490_2]
    })
}

/// `SeplColor(text, flags)`: the text coloured by the flags and made transparent (nothing without a colour).
///
/// # Safety
/// `text` is a live text.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_sepl_color(text: *mut ee_render::ge::smallobj::EeGeText, flags: u32) {
    use ee_render::ge::smallobj::{ee_ge_text_color, ee_ge_text_set_transparent};
    if let Some(c) = sepl_color(flags) {
        unsafe {
            core::ptr::copy_nonoverlapping(c.as_ptr(), ee_ge_text_color(text, true), 3);
            ee_ge_text_set_transparent(text, true);
        }
    }
}

/// `GERectangle(GE2DPoint(x, y), width, height)` as left, top, right, bottom (its constructor's sums).
fn sized(x: f32, y: f32, width: f32, height: f32) -> [f32; 4] {
    [x, y, x + (width - 1.0), y + (height - 1.0)]
}

/// The tech tree id of no technology (`kTechNone`).
const TECH_NONE: i32 = 100000;
/// The upgrade panel's id.
const UPGRADE_PANEL_ID: c_ulong = 0x41a;
/// A tech tree node's kind of a wall.
const NODE_KIND_WALL: i32 = 6;

/// One statement: `x(op, p, i, f)` with p the three handles; the answer, i and f as the C++ leaves them.
pub type Seunx<'a> = dyn FnMut(u32, [usize; 3], &mut [i32; 4], &mut [f32; 4]) -> i32 + 'a;

/// The placement action's Rust calls (`ee_screens_sew_*`), the action first.
#[derive(Clone, Copy, Debug, PartialEq)]
pub enum SewCall {
    Wall(usize, f32, f32, f32, f32, i32, i32),
    Gate(usize, i32, i32, i32, i32, i32),
    Moving(usize),
    Cancel(usize),
    PickUp(usize, u32),
    PutDown(usize),
    PlaceUnit(usize, i32),
}

/// The calls' answers (`ee_screens_sew_moving`'s, the others' 0).
pub type Sew<'a> = dyn FnMut(SewCall) -> i32 + 'a;

/// A statement on operands p1, p2 with integers a and floats g.
fn st(x: &mut Seunx, op: u32, p1: usize, p2: usize, a: [i32; 4], g: [f32; 4]) -> i32 {
    let (mut a, mut g) = (a, g);
    x(op, [0, p1, p2], &mut a, &mut g)
}

/// A plain statement on p1.
fn on(x: &mut Seunx, op: u32, p1: usize) -> i32 {
    st(x, op, p1, 0, [0; 4], [0.0; 4])
}

/// A statement on p1 with one integer.
fn on_i(x: &mut Seunx, op: u32, p1: usize, v: i32) -> i32 {
    st(x, op, p1, 0, [v, 0, 0, 0], [0.0; 4])
}

/// A statement's floats as it leaves them.
fn floats(x: &mut Seunx, op: u32, p1: usize) -> [f32; 4] {
    let mut g = [0.0; 4];
    x(op, [0, p1, 0], &mut [0; 4], &mut g);
    g
}

/// A making statement: the object the C++ writes into the out slot p0.
fn mk(x: &mut Seunx, op: u32, p1: usize, p2: usize, a: [i32; 4], g: [f32; 4]) -> usize {
    let mut out: usize = 0;
    let (mut a, mut g) = (a, g);
    x(op, [&mut out as *mut usize as usize, p1, p2], &mut a, &mut g);
    out
}

/// The made object into the op's out slot `p0[k]`.
///
/// # Safety
/// `p0` is the op's out array of at least `k + 1` pointers.
unsafe fn put(p0: usize, k: usize, v: usize) {
    unsafe { *(p0 as *mut usize).add(k) = v };
}

/// The mouse's tile (`EEPointUnitOrTileXY`).
fn mouse_tile(x: &mut Seunx) -> (c_long, c_long) {
    let (mut unit, mut tx, mut ty): (usize, c_long, c_long) = (0, 0, 0);
    let p = [&mut unit as *mut usize as usize, &mut tx as *mut c_long as usize, &mut ty as *mut c_long as usize];
    x(EE_SCREENS_SEUNX_MOUSE_TILE, p, &mut [0; 4], &mut [0.0; 4]);
    (tx, ty)
}

/// `SeunTypeButton`: a unit type button with its normal and pressed bitmaps cut from the form's button texture
/// (p0 the out handles: the button, the normal and the pressed bitmaps).
fn type_button(x: &mut Seunx, p: [usize; 3], i: &[i32; 4], f: &[f32; 4]) -> [usize; 3] {
    const W: f32 = 35.0;
    let form = p[1];
    let texture = mk(x, EE_SCREENS_SEUNX_BUTTON_TEXTURE, form, 0, [0; 4], [0.0; 4]);
    let size = on(x, EE_SCREENS_SEUNX_TEXTURE_WIDTH, texture) as u32 as f32;
    let y_scale = 25.0f32 / size;
    let x_scale = W / size;
    let normal = mk(x, EE_SCREENS_SEUNX_NEW_BITMAP, 0, 0, [0; 4], [0.0; 4]);
    let pressed = mk(x, EE_SCREENS_SEUNX_NEW_BITMAP, 0, 0, [0; 4], [0.0; 4]);
    for b in [normal, pressed] {
        x(EE_SCREENS_SEUNX_BITMAP_TEXTURE_FORM, [b, texture, form], &mut [0; 4], &mut [0.0; 4]);
    }
    for (k, b) in [normal, pressed].into_iter().enumerate() {
        let (cx, cy) = (f[k * 2], f[k * 2 + 1]);
        let corners = [cx / size, cy / size, (cx + W) / size, (cy + 25.0) / size];
        let rust = mk(x, EE_SCREENS_SEUNX_BITMAP_RUST, b, 0, [0; 4], [0.0; 4]);
        x(EE_SCREENS_SEUNX_GE_UVS, [rust, 0, 0], &mut [0; 4], &mut { corners });
    }
    for b in [normal, pressed] {
        st(x, EE_SCREENS_SEUNX_BITMAP_SCALE, b, 0, [0; 4], [x_scale, y_scale, 0.0, 0.0]);
    }
    let rect = sized(i[3] as u32 as f32, 0.0, W, 25.0);
    let button = mk(x, EE_SCREENS_SEUNX_TYPE_RADIO, form, 0, [i[0], i[1] + 0x406, i32::from(i[1] == 0), 0], rect);
    for (op, b) in [
        (EE_SCREENS_SEUNX_RADIO_UP_CHECKED, pressed),
        (EE_SCREENS_SEUNX_RADIO_DOWN_CHECKED, pressed),
        (EE_SCREENS_SEUNX_RADIO_DOWN_UNCHECKED, pressed),
        (EE_SCREENS_SEUNX_RADIO_UP_UNCHECKED, normal),
    ] {
        st(x, op, button, b, [0; 4], [0.0; 4]);
    }
    on(x, EE_SCREENS_SEUNX_RADIO_NO_FOCUS, button);
    on(x, EE_SCREENS_SEUNX_RADIO_DRAW_OFFSET, button);
    st(x, EE_SCREENS_SEUNX_RADIO_TRIGGER, button, p[2], [0; 4], [0.0; 4]);
    on_i(x, EE_SCREENS_SEUNX_RADIO_STRING_ID, button, i[2]);
    st(x, EE_SCREENS_SEUNX_ADD_RADIO, form, button, [6, 0, 0, 0], [0.0; 4]);
    [button, normal, pressed]
}

/// The wall tool's ghosts at the mouse: its first tile, or the tiles from its start to the mouse.
fn preview_wall(x: &mut Seunx, tab: usize) {
    let (tx, ty) = mouse_tile(x);
    let (mut lx, mut ly) = (tx as f32, ty as f32);
    let preview = |x: &mut Seunx| {
        on(x, EE_SCREENS_SEUNX_LOCK_ENTER, 0);
        on(x, EE_SCREENS_SEUNX_PREVIEW_WALL, tab);
        on(x, EE_SCREENS_SEUNX_LOCK_LEAVE, 0);
    };
    if on(x, EE_SCREENS_SEUNX_WALL_STATE, tab) as u32 == 0 {
        lx += 0.5;
        ly += 0.5;
        on(x, EE_SCREENS_SEUNX_TILES_CLEAR, 0);
        st(x, EE_SCREENS_SEUNX_TILES_PUSH, 0, 0, [lx as c_long as i32, ly as c_long as i32, 0, 0], [0.0; 4]);
        preview(x);
    }
    if on(x, EE_SCREENS_SEUNX_WALL_STATE, tab) as u32 == 1 {
        lx += 0.5;
        ly += 0.5;
        on(x, EE_SCREENS_SEUNX_TILES_CLEAR, 0);
        let start = floats(x, EE_SCREENS_SEUNX_WALL_START, tab);
        let mut points = Vec::new();
        Ge2DPoint { x: start[0], y: start[1] }.interpolate_to(Ge2DPoint { x: lx, y: ly }, 1.0, |q| points.push(q));
        for q in points {
            st(x, EE_SCREENS_SEUNX_TILES_PUSH, 0, 0, [q.x as c_long as i32, q.y as c_long as i32, 0, 0], [0.0; 4]);
        }
        preview(x);
    }
}

/// The gate tool's ghost on the wall under the mouse, else no ghost.
fn preview_gate(x: &mut Seunx, tab: usize) {
    let (tx, ty) = mouse_tile(x);
    let mut direction = on(x, EE_SCREENS_SEUNX_GATE_DIRECTION, tab) as c_long;
    let p = [0, 0, &mut direction as *mut c_long as usize];
    if x(EE_SCREENS_SEUNX_FIND_GATE_WALL, p, &mut [tx as i32, ty as i32, 0, 0], &mut [0.0; 4]) != 0 {
        on(x, EE_SCREENS_SEUNX_CREATE_GATE, tab);
        let p = [0, 0, &mut direction as *mut c_long as usize];
        x(EE_SCREENS_SEUNX_PREVIEW_GATE, p, &mut [0; 4], &mut [0.0; 4]);
        on_i(x, EE_SCREENS_SEUNX_SET_DIRECTION, tab, direction as i32);
    } else {
        on(x, EE_SCREENS_SEUNX_LOCK_ENTER, 0);
        on(x, EE_SCREENS_SEUNX_CLEAR_GHOSTS, 0);
        on(x, EE_SCREENS_SEUNX_LOCK_LEAVE, 0);
    }
}

/// `SeunPickClass`: the player, the list line's class and its technology's node (the original's order).
fn pick(x: &mut Seunx, list: usize, player: i32, line: i32) -> (usize, usize, usize) {
    let player = mk(x, EE_SCREENS_SEUNX_PLAYER, 0, 0, [player, 0, 0, 0], [0.0; 4]);
    let ty = on_i(x, EE_SCREENS_SEUNX_TYPE_AT, list, line);
    let tech = if on_i(x, EE_SCREENS_SEUNX_CLASS_EXISTS, 0, ty) != 0 {
        on_i(x, EE_SCREENS_SEUNX_CLASS_TECH, 0, ty)
    } else {
        TECH_NONE
    };
    let node = mk(x, EE_SCREENS_SEUNX_TREE_NODE, player, 0, [tech, 0, 0, 0], [0.0; 4]);
    let class = mk(x, EE_SCREENS_SEUNX_CLASS, 0, 0, [ty, 0, 0, 0], [0.0; 4]);
    (player, class, node)
}

/// The op composed, or `None` for the C++ host's.
///
/// # Safety
/// `p` are the op's handles as `ee_screenscpp_seun` takes them (its out slots live).
pub unsafe fn compose(
    x: &mut Seunx,
    sew: &mut Sew,
    op: u32,
    p: [usize; 3],
    i: &mut [i32; 4],
    f: &mut [f32; 4],
) -> Option<i32> {
    let z = [0.0f32; 4];
    Some(match op {
        EE_SCREENS_SEUN_OP_TYPE_BUTTON => {
            let m = type_button(x, p, i, f);
            for (k, v) in m.into_iter().enumerate() {
                // SAFETY: p0 the op's three out slots
                unsafe { put(p[0], k, v) };
            }
            0
        }
        EE_SCREENS_SEUN_OP_REMOVE_CARGO => {
            on(x, EE_SCREENS_SEUNX_STRING_REMOVE, 0);
            let ints = [i[0], SEUNX_KIND_REMOVE_CARGO, 0x3f4, 2];
            let t = mk(x, EE_SCREENS_SEUNX_NEW_BUTTON, p[1], 0, ints, sized(f[0], 707.0, 100.0, 25.0));
            on(x, EE_SCREENS_SEUNX_BUTTON_SET_TEXT, t);
            on(x, EE_SCREENS_SEUNX_BUTTON_OPAQUE, t);
            on(x, EE_SCREENS_SEUNX_BUTTON_TEXT_WHITE, t);
            on(x, EE_SCREENS_SEUNX_BUTTON_TEXT_TRANSPARENT, t);
            st(x, EE_SCREENS_SEUNX_ADD_BUTTON, p[1], t, [6, 0, 0, 0], z);
            // SAFETY: p0 the op's out slot
            unsafe { put(p[0], 0, t) };
            0
        }
        EE_SCREENS_SEUN_OP_ROTATION => {
            let ints = [i[0], SEUNX_KIND_ROTATION, 0x3f5, 0];
            let t = mk(x, EE_SCREENS_SEUNX_NEW_BUTTON, p[1], 0, ints, sized(500.0, 633.0, 50.0, 50.0));
            on(x, EE_SCREENS_SEUNX_ROTATION_STRING, t);
            st(x, EE_SCREENS_SEUNX_ADD_BUTTON, p[1], t, [6, 0, 0, 0], z);
            // SAFETY: p0 the op's out slot
            unsafe { put(p[0], 0, t) };
            0
        }
        EE_SCREENS_SEUN_OP_ACTION => {
            let what = match i[0] {
                EE_SCREENS_SEUN_ACTION_CLICK => EE_SCREENS_SEUNX_ACTION_CLICK,
                EE_SCREENS_SEUN_ACTION_LASSO => EE_SCREENS_SEUNX_ACTION_LASSO,
                EE_SCREENS_SEUN_ACTION_PLACE_UNIT => EE_SCREENS_SEUNX_ACTION_PLACE_UNIT,
                EE_SCREENS_SEUN_ACTION_PLACE_GATE => EE_SCREENS_SEUNX_ACTION_PLACE_GATE,
                EE_SCREENS_SEUN_ACTION_PLACE_WALL => EE_SCREENS_SEUNX_ACTION_PLACE_WALL,
                _ => EE_SCREENS_SEUNX_ACTION_MOVE_UNIT,
            };
            let a = mk(x, what, 0, 0, [0; 4], z);
            // SAFETY: p0 the op's out slot
            unsafe { put(p[0], 0, a) };
            0
        }
        EE_SCREENS_SEUN_OP_SELECT_UNITS | EE_SCREENS_SEUN_OP_COMMAND_UNITS => {
            let (new, add) = if op == EE_SCREENS_SEUN_OP_SELECT_UNITS {
                (EE_SCREENS_SEUNX_SELECT_UNITS, EE_SCREENS_SEUNX_ADD_SELECT_UNITS)
            } else {
                (EE_SCREENS_SEUNX_COMMAND_UNITS, EE_SCREENS_SEUNX_ADD_COMMAND_UNITS)
            };
            let t = mk(x, new, p[1], p[2], [i[0], 0, 0, 0], z);
            st(x, add, p[1], t, [0; 4], z);
            // SAFETY: p0 the op's out slot
            unsafe { put(p[0], 0, t) };
            0
        }
        EE_SCREENS_SEUN_OP_BACKGROUND => {
            let texture = mk(x, EE_SCREENS_SEUNX_LOAD_SCENUNIT, 0, 0, [0; 4], z);
            for s in [
                EE_SCREENS_SEUNX_TEX_FRAME,
                EE_SCREENS_SEUNX_TEX_ALPHA,
                EE_SCREENS_SEUNX_TEX_COLOR,
                EE_SCREENS_SEUNX_TEX_MIN,
                EE_SCREENS_SEUNX_TEX_MAG,
                EE_SCREENS_SEUNX_TEX_MIP,
                EE_SCREENS_SEUNX_TEX_BIAS,
                EE_SCREENS_SEUNX_TEX_WRAP,
            ] {
                on(x, s, texture);
            }
            let b = mk(x, EE_SCREENS_SEUNX_NEW_BITMAP, 0, 0, [0; 4], z);
            st(x, EE_SCREENS_SEUNX_BITMAP_TEXTURE_SCREEN, b, texture, [0; 4], z);
            // SAFETY: p0 the op's out slot
            unsafe { put(p[0], 0, b) };
            0
        }
        EE_SCREENS_SEUN_OP_UPGRADE_PANEL => {
            // the id and the index (the constructor's references; the id it may change is dropped)
            let mut ids: [c_ulong; 2] = [UPGRADE_PANEL_ID, i[0] as c_ulong];
            let t = mk(x, EE_SCREENS_SEUNX_UPGRADE_PANEL, p[1], ids.as_mut_ptr() as usize, [0; 4], z);
            i[0] = ids[1] as i32;
            // SAFETY: p0 the op's out slot
            unsafe { put(p[0], 0, t) };
            0
        }
        EE_SCREENS_SEUN_OP_RELEASE => {
            let what = match i[0] {
                0 => EE_SCREENS_SEUNX_RELEASE_ACTION,
                1 => EE_SCREENS_SEUNX_RELEASE_BITMAP,
                2 => EE_SCREENS_SEUNX_RELEASE_EVENT,
                _ => EE_SCREENS_SEUNX_DELETE_PANEL,
            };
            on(x, what, p[0]);
            0
        }
        EE_SCREENS_SEUN_OP_PLACE_WALL => {
            let tab = p[0];
            let start = floats(x, EE_SCREENS_SEUNX_WALL_START, tab);
            let end = floats(x, EE_SCREENS_SEUNX_WALL_END, tab);
            let action = mk(x, EE_SCREENS_SEUNX_SEW_ACTION, p[1], 0, [0; 4], z);
            let tech = on(x, EE_SCREENS_SEUNX_WALL_TECH, tab);
            sew(SewCall::Wall(action, start[0], start[1], end[0], end[1], i[0], tech));
            0
        }
        EE_SCREENS_SEUN_OP_PLACE_GATE => {
            let tab = p[0];
            let action = mk(x, EE_SCREENS_SEUNX_SEW_ACTION, p[1], 0, [0; 4], z);
            let tile_x = on(x, EE_SCREENS_SEUNX_GATE_TILE_X, tab);
            let tile_y = on(x, EE_SCREENS_SEUNX_GATE_TILE_Y, tab);
            let tech = on(x, EE_SCREENS_SEUNX_GATE_TECH, tab);
            let direction = on(x, EE_SCREENS_SEUNX_GATE_DIRECTION, tab);
            sew(SewCall::Gate(action, tile_x, tile_y, i[0], tech, direction));
            0
        }
        EE_SCREENS_SEUN_OP_CLEAR_GHOSTS => {
            if i[0] != 0 {
                on(x, EE_SCREENS_SEUNX_LOCK_ENTER, 0);
            }
            on(x, EE_SCREENS_SEUNX_CLEAR_GHOSTS, 0);
            if i[0] != 0 {
                on(x, EE_SCREENS_SEUNX_LOCK_LEAVE, 0);
            }
            0
        }
        EE_SCREENS_SEUN_OP_PREVIEW => {
            match i[0] {
                0 => {
                    on(x, EE_SCREENS_SEUNX_PREVIEW_PLACEMENT, 0);
                }
                1 => preview_wall(x, p[0]),
                _ => preview_gate(x, p[0]),
            }
            0
        }
        EE_SCREENS_SEUN_OP_GAME_VIEW => {
            let left = if i[0] != 0 { 225.0 } else { 0.0 };
            st(x, EE_SCREENS_SEUNX_GAME_VIEW, 0, 0, [0; 4], [left, 0.0, 1024.0, 606.0]);
            on(x, EE_SCREENS_SEUNX_CLEAR_B529066, 0);
            on(x, EE_SCREENS_SEUNX_CLEAR_FOLLOWING, 0);
            on(x, EE_SCREENS_SEUNX_CLEAR_B529068, 0);
            if on(x, EE_SCREENS_SEUNX_MINIMAP, 0) != 0 {
                on(x, EE_SCREENS_SEUNX_MINIMAP_REFRESH, 0);
            }
            on(x, EE_SCREENS_SEUNX_REDRAW_ALL, 0);
            on(x, EE_SCREENS_SEUNX_ALL_DIRTY, 0);
            0
        }
        EE_SCREENS_SEUN_OP_MOVING => {
            let action = mk(x, EE_SCREENS_SEUNX_SEW_ACTION, p[0], 0, [0; 4], z);
            (sew(SewCall::Moving(action)) != 0) as i32
        }
        EE_SCREENS_SEUN_OP_CANCEL_MOVE => {
            let action = mk(x, EE_SCREENS_SEUNX_SEW_ACTION, p[0], 0, [0; 4], z);
            sew(SewCall::Cancel(action));
            0
        }
        EE_SCREENS_SEUN_OP_PICK_UP => {
            let action = mk(x, EE_SCREENS_SEUNX_SEW_ACTION, p[0], 0, [0; 4], z);
            sew(SewCall::PickUp(action, i[0] as u32));
            0
        }
        EE_SCREENS_SEUN_OP_PUT_DOWN => {
            let action = mk(x, EE_SCREENS_SEUNX_SEW_ACTION, p[0], 0, [0; 4], z);
            sew(SewCall::PutDown(action));
            0
        }
        EE_SCREENS_SEUN_OP_PLACE_UNIT => {
            let action = mk(x, EE_SCREENS_SEUNX_SEW_ACTION, p[0], 0, [0; 4], z);
            sew(SewCall::PlaceUnit(action, i[0]));
            0
        }
        EE_SCREENS_SEUN_OP_DRAW => {
            // the clipped panel (the original's local), written by the And, drawn
            let mut clip = ee_base::math::types::GeRectangle::default();
            let c = &mut clip as *mut ee_base::math::types::GeRectangle as usize;
            if x(EE_SCREENS_SEUNX_PANEL_AND, [c, p[0], p[2]], &mut [0; 4], &mut [0.0; 4]) != 0 {
                x(EE_SCREENS_SEUNX_DRAW_PANEL, [c, p[0], p[1]], &mut [0; 4], &mut [0.0; 4]);
            }
            0
        }
        EE_SCREENS_SEUN_OP_ANGLE => {
            f[0] = f32::from_bits(on(x, EE_SCREENS_SEUNX_ROTATION_ANGLE, p[0]) as u32);
            0
        }
        EE_SCREENS_SEUN_OP_DIRTY => {
            for (s, v) in [
                (EE_SCREENS_SEUNX_PANEL_LEFT, 0.0),
                (EE_SCREENS_SEUNX_PANEL_TOP, 0.0),
                (EE_SCREENS_SEUNX_PANEL_RIGHT, 224.0),
                (EE_SCREENS_SEUNX_PANEL_BOTTOM, 605.0),
            ] {
                st(x, s, p[1], 0, [0; 4], [v, 0.0, 0.0, 0.0]);
            }
            st(x, EE_SCREENS_SEUNX_PANEL_DIRTY, p[1], p[0], [0; 4], z);
            0
        }
        EE_SCREENS_SEUN_OP_CANCEL_BUILD => {
            on(x, if i[0] != 0 { EE_SCREENS_SEUNX_GATE_CANCEL } else { EE_SCREENS_SEUNX_WALL_CANCEL }, p[0]);
            0
        }
        EE_SCREENS_SEUN_OP_PLACE_INFO => {
            let (player, class, node) = pick(x, p[0], i[0], i[1]);
            let id = on(x, EE_SCREENS_SEUNX_PLAYER_ID, player) as c_long;
            let refused = (id != 0 || on(x, EE_SCREENS_SEUNX_GAIA_PLACEABLE, class) == 0)
                && (id <= 0 || on(x, EE_SCREENS_SEUNX_PLAYER_PLACEABLE, class) == 0);
            i[1] = (!refused) as i32;
            i[2] = if node != 0 && on(x, EE_SCREENS_SEUNX_NODE_KIND, node) == NODE_KIND_WALL {
                EE_SCREENS_SEUN_KIND_WALL
            } else if class != 0 && on(x, EE_SCREENS_SEUNX_CLASS_IS_GATE, class) != 0 {
                EE_SCREENS_SEUN_KIND_GATE
            } else {
                EE_SCREENS_SEUN_KIND_UNIT
            };
            id as i32
        }
        EE_SCREENS_SEUN_OP_START_BUILD => {
            let tab = p[0];
            let (player, class, node) = pick(x, p[1], i[0], i[1]);
            if i[2] == 0 {
                let tech = on(x, EE_SCREENS_SEUNX_NODE_ID, node);
                on_i(x, EE_SCREENS_SEUNX_WALL_SET_TECH, tab, tech);
                let id = on(x, EE_SCREENS_SEUNX_PLAYER_ID, player);
                on_i(x, EE_SCREENS_SEUNX_WALL_INITIALIZE, tab, id);
            } else {
                let tech = on(x, EE_SCREENS_SEUNX_NODE_ID, node);
                on_i(x, EE_SCREENS_SEUNX_GATE_SET_TECH, tab, tech);
                let wall = on(x, EE_SCREENS_SEUNX_NODE_UNIT_CLASS, node);
                on_i(x, EE_SCREENS_SEUNX_GATE_SET_WALL_TYPE, tab, wall);
                st(x, EE_SCREENS_SEUNX_GATE_SET_CLASS, tab, class, [0; 4], z);
                let id = on(x, EE_SCREENS_SEUNX_PLAYER_ID, player);
                on_i(x, EE_SCREENS_SEUNX_GATE_INITIALIZE, tab, id);
            }
            0
        }
        EE_SCREENS_SEUN_OP_POST_GHOST => {
            let ty = on_i(x, EE_SCREENS_SEUNX_TYPE_AT, p[1], i[0]);
            st(x, EE_SCREENS_SEUNX_POST_402, p[0], 0, [ty, i[1], 0, 0], z);
            0
        }
        EE_SCREENS_SEUN_OP_CARGO_COUNT => {
            let unit = mk(x, EE_SCREENS_SEUNX_SELECTED_COMPLEX, p[1], 0, [i[0], 0, 0, 0], z);
            if unit != 0 {
                on(x, EE_SCREENS_SEUNX_CARGO_UNITS, unit)
            } else {
                0
            }
        }
        EE_SCREENS_SEUN_OP_MOUSE => {
            let mut buttons = 0;
            for (s, bit) in [
                (EE_SCREENS_SEUNX_L_DOUBLE, EE_SCREENS_SEUN_MOUSE_L_DOUBLE),
                (EE_SCREENS_SEUNX_L_PRESSED, EE_SCREENS_SEUN_MOUSE_L_PRESSED),
                (EE_SCREENS_SEUNX_L_RELEASED, EE_SCREENS_SEUN_MOUSE_L_RELEASED),
                (EE_SCREENS_SEUNX_L_DOWN, EE_SCREENS_SEUN_MOUSE_L_DOWN),
                (EE_SCREENS_SEUNX_R_PRESSED, EE_SCREENS_SEUN_MOUSE_R_PRESSED),
                (EE_SCREENS_SEUNX_R_RELEASED, EE_SCREENS_SEUN_MOUSE_R_RELEASED),
            ] {
                if on(x, s, 0) != 0 {
                    buttons |= bit;
                }
            }
            buttons
        }
        EE_SCREENS_SEUN_OP_PICK => {
            let unit = mk(x, EE_SCREENS_SEUNX_PICK_UNIT, 0, 0, [0; 4], [f[0], f[1], 0.0, 0.0]);
            if unit == 0 {
                return Some(-1);
            }
            let id = on(x, EE_SCREENS_SEUNX_UNIT_ID, unit);
            i[1] = on(x, EE_SCREENS_SEUNX_UNIT_IS_GATE, unit);
            on(x, EE_SCREENS_SEUNX_UNIT_DEREFERENCE, unit);
            id
        }
        EE_SCREENS_SEUN_OP_HOLD_GATE => {
            let tab = p[0];
            let unit = mk(x, EE_SCREENS_SEUNX_GET_UNIT, 0, 0, [i[0], 0, 0, 0], z);
            for s in [
                EE_SCREENS_SEUNX_GATE_TECH_OF_UNIT,
                EE_SCREENS_SEUNX_GATE_WALL_OF_UNIT,
                EE_SCREENS_SEUNX_GATE_CLASS_OF_UNIT,
            ] {
                st(x, s, tab, unit, [0; 4], z);
            }
            on(x, EE_SCREENS_SEUNX_UNIT_KILL, unit);
            st(x, EE_SCREENS_SEUNX_GATE_INITIALIZE_OF_UNIT, tab, unit, [0; 4], z);
            0
        }
        EE_SCREENS_SEUN_OP_BUILD_UPDATE => {
            let s = if i[0] != 0 { EE_SCREENS_SEUNX_GATE_UPDATE } else { EE_SCREENS_SEUNX_WALL_UPDATE };
            st(x, s, p[0], 0, [0; 4], [f[0], f[1], 0.0, 0.0]);
            0
        }
        EE_SCREENS_SEUN_OP_LOCK => {
            on(x, if i[0] != 0 { EE_SCREENS_SEUNX_LOCK_ENTER } else { EE_SCREENS_SEUNX_LOCK_LEAVE }, 0);
            0
        }
        EE_SCREENS_SEUN_OP_KEYS => {
            let mut keys = 0;
            let delete = mk(x, EE_SCREENS_SEUNX_KEY_DELETE, 0, 0, [0; 4], z);
            if on(x, EE_SCREENS_SEUNX_KEY_PRESSED, delete) != 0 {
                keys |= 1;
                let shift = mk(x, EE_SCREENS_SEUNX_KEY_SHIFT, 0, 0, [0; 4], z);
                if on(x, EE_SCREENS_SEUNX_KEY_DOWN, shift) != 0 {
                    keys |= 2;
                }
            }
            keys
        }
        EE_SCREENS_SEUN_OP_DELETE_SELECTION => {
            let player = on(x, EE_SCREENS_SEUNX_CURRENT_PLAYER, 0);
            let a = mk(x, EE_SCREENS_SEUNX_DELETE_ACTION, 0, 0, [i[0], player, 0, 0], z);
            on(x, EE_SCREENS_SEUNX_RELEASE_ACTION, a);
            0
        }
        _ => return None,
    })
}

#[cfg(test)]
#[path = "editor_seunx_tests.rs"]
mod tests;
