//! The main game form's mouse modes (`EEUIFMainGame::UpdateMouse`'s test for the interface state, the original's
//! `EEModeNormal` .. `EEModeConvert` in `EEUIFMGStatusButton.cpp`'s range) and the wall and gate placements' mouse
//! (`EEUIBuildWall::Update`, `EEUIBuildGate::Update`), over the main game form's host ([`FormHost`],
//! `ee_screenscpp_main_form`): the airport and rally targets, attack ground, building placement, flares, garrison,
//! unload, heal, convert, calamities, the normal mode's selection and command starts, the camera mode.
//!
//! Every call on the form and the world is one host op, asked where the original asked it; the reads the original
//! repeats (the mouse buttons, the shift key) are pure and read once per decision. The host holds the picked unit
//! (`EE_SCREENS_MF_OP_MD_PICK`, released by `EE_SCREENS_MF_OP_RELEASE`), the first selected unit
//! (`EE_SCREENS_MF_OP_MD_FIRST`) and the pointed tile (`EE_SCREENS_MF_OP_MD_POINT`). The wall and gate placements
//! (the game form's and the scenario editor's subclasses, whose `Build` and `Cancel` differ) keep their virtual
//! `Update`, which comes back here with the placement's members as a record (reached by address and read afresh
//! around every op) over a call of its own (its placement held by the host).

use core::cell::Cell;

use crate::screens::hud_ctl::{EeScreensPoint, Rec};
use crate::screens::main_form::*;
use ee_render::ge::types::Ge2DPoint;
use ee_render::ge::um::floor;

// ---- the mouse modes' host ops (after main_form.rs' 151)

/// `GetIsMouseCaptured()`.
pub const EE_SCREENS_MF_OP_MD_CAPTURED: u32 = 152;
/// `gMouse->mPointer = a`.
pub const EE_SCREENS_MF_OP_MD_POINTER: u32 = 153;
/// `SetUIState(a)` (b 0), or the placement's virtual `Cancel()` (b 1).
pub const EE_SCREENS_MF_OP_MD_UI_STATE: u32 = 154;
/// The mouse buttons' states as `EE_SCREENS_MF_BTN_*` bits.
pub const EE_SCREENS_MF_OP_MD_BUTTONS: u32 = 155;
/// The shift key's `GetValue()`.
pub const EE_SCREENS_MF_OP_MD_SHIFT: u32 = 156;
/// Whether the view rectangle contains io.f[0..2]: `gViewport`'s (a 0) or the rasterizer's viewport of
/// `gScreenIndex` (a 1).
pub const EE_SCREENS_MF_OP_MD_VIEW: u32 = 157;
/// `EEPointUnitOrTile(io.f[0..2], tile, point, true)` (a 0: the point, from (0, 0, 0), into io.f[0..3]) or
/// `EEPointUnitOrTileXY` (a 1: the tile, from (-10000, -10000), into io.n): found. The tile is held.
pub const EE_SCREENS_MF_OP_MD_POINT: u32 = 158;
/// The held tile's `IsSeenBy(the current player)`.
pub const EE_SCREENS_MF_OP_MD_TILE_SEEN: u32 = 159;
/// `gUnitPicker.PickUnit(io.f[0..2], 0)`, held: whether a unit was picked.
pub const EE_SCREENS_MF_OP_MD_PICK: u32 = 160;
/// The picked unit's `EE_SCREENS_MF_UNIT_*` a.
pub const EE_SCREENS_MF_OP_MD_UNIT: u32 = 161;
/// `EEDamageFactor(a, b)` into io.f[0].
pub const EE_SCREENS_MF_OP_MD_DAMAGE: u32 = 162;
/// a 0: `EEGetFirstSelectedUnit(first)`, held: bit 0 its answer, bit 1 the unit non-null; a 1: the selected group's
/// `GetFirstUnit()`, held: non-null.
pub const EE_SCREENS_MF_OP_MD_FIRST: u32 = 163;
/// The held first unit's `EE_SCREENS_MF_FIRST_*` a (io.f[0..3] the point of `EE_SCREENS_MF_FIRST_IN_RANGE`).
pub const EE_SCREENS_MF_OP_MD_FIRST_FIELD: u32 = 164;
/// `EECanConvert(the selected group's unit a, the picked unit's complex unit, in range = b)`: bit 0 its answer, bit
/// 1 in range.
pub const EE_SCREENS_MF_OP_MD_CAN_CONVERT: u32 = 165;
/// Sends the message `EE_SCREENS_MF_SEND_*` a (`ProcessLocalUserInput`) with b and io.
pub const EE_SCREENS_MF_OP_MD_SEND: u32 = 166;
/// `EEOrderAtCursor(io.f[0..2], unit)`: the unit none (a 0), the picked one (1), its complex unit (2).
pub const EE_SCREENS_MF_OP_MD_ORDER_AT: u32 = 167;
/// The current player's `mEvents.AddEvent((io.n[0], io.n[1]), 2, player, a, false)`.
pub const EE_SCREENS_MF_OP_MD_EVENT: u32 = 168;
/// `EEMakeCalamityEffect(io.f[0..3], the picked unit)`.
pub const EE_SCREENS_MF_OP_MD_CALAMITY_EFFECT: u32 = 169;
/// `EEPreviewPlacement(GetMouseLocation())`.
pub const EE_SCREENS_MF_OP_MD_PREVIEW: u32 = 170;
/// a -1: `gWorld.GetNumGhostUnits()`; else the ghost a's `EE_SCREENS_MF_GHOST_*` b.
pub const EE_SCREENS_MF_OP_MD_GHOST: u32 = 171;
/// `mBuild->mIsQueued = a`, then `mBuild->Build(io.f[0..2])`.
pub const EE_SCREENS_MF_OP_MD_BUILD: u32 = 172;
/// `mCalamity->Check(io.f[0..2])`.
pub const EE_SCREENS_MF_OP_MD_CALAMITY_CHECK: u32 = 173;
/// `IsInGameView(io.f[0..2])`.
pub const EE_SCREENS_MF_OP_MD_IN_GAME_VIEW: u32 = 174;
/// `GetEntityGroup(a)->GetIsEnabled()`.
pub const EE_SCREENS_MF_OP_MD_ENTITY_ENABLED: u32 = 175;
/// `SetEntityGroup(a)`.
pub const EE_SCREENS_MF_OP_MD_SET_ENTITY_GROUP: u32 = 176;
/// The camera: a 0 `ClearUnitA()` and `ClearUnitB()`, a 1 `Follow(NULL, false, 1, 0, 0)`.
pub const EE_SCREENS_MF_OP_MD_CAMERA: u32 = 177;
/// The unit hack's `Fn157269(io.f[0..2])` (a 0) or `PlaceUnit(io.f[0..2])` (a 1).
pub const EE_SCREENS_MF_OP_MD_HACK: u32 = 178;
/// `StartSelection(io.f[0..2])` of the select-units control (a 0) or `StartCommand` of the command-units one (a 1).
pub const EE_SCREENS_MF_OP_MD_START: u32 = 179;
/// The placement's `Build()` (a 0, the wall) or `Build(io.f[0..2], b)` (a 1, the gate): virtual (the scenario
/// editor's placements build their own way).
pub const EE_SCREENS_MF_OP_MD_BUILD_WALL: u32 = 180;
/// The gate placement's `ClearGhostUnits()`.
pub const EE_SCREENS_MF_OP_MD_GATE_CLEAR: u32 = 181;
/// `EEGhostUnit::CreateGate(0, the gate's mUnitClass, gWorld.GetPlayer(the gate's mPlayer))`, then
/// `PreviewGate(io.f[0..2], io.n[0])` (the direction back in io.n[0]).
pub const EE_SCREENS_MF_OP_MD_GATE_PREVIEW: u32 = 182;
/// The form's `mBuildWall.Update(GetMouseLocation())` (a 0) or `mBuildGate.Update` (a 1): virtual, each back in
/// [`wall_update`] / [`gate_update`] over a call of its own.
pub const EE_SCREENS_MF_OP_MD_PLACEMENT: u32 = 183;
/// `GetMouseLocation()` into io.f[0..2]; the answer `mMouseModes[mUIState]` (`EE_SCREENS_MF_MODE_*`).
pub const EE_SCREENS_MF_OP_MD_MOUSE_AT: u32 = 184;
/// `gISKeyboard->GetEntity(a, key)`: `EE_SCREENS_MF_KEY_*` bits (the entity found, `GetValue()`, `JustPressed()`).
pub const EE_SCREENS_MF_OP_MD_KEY: u32 = 185;
/// The cinematic's calls (`EE_SCREENS_MF_CINE_*` a): the end-cinematic event posted, the scenario manager's
/// `mIsEndingCinematic`, the pause screen event posted, `PostScreenShot(-1, true, false)`.
pub const EE_SCREENS_MF_OP_MD_CINEMATIC: u32 = 186;

/// The key's bits (`EE_SCREENS_MF_OP_MD_KEY`).
pub const EE_SCREENS_MF_KEY_FOUND: i64 = 1;
pub const EE_SCREENS_MF_KEY_HELD: i64 = 2;
pub const EE_SCREENS_MF_KEY_JUST_PRESSED: i64 = 4;
/// The cinematic's calls (`EE_SCREENS_MF_OP_MD_CINEMATIC`).
pub const EE_SCREENS_MF_CINE_END: i64 = 0;
pub const EE_SCREENS_MF_CINE_ENDING: i64 = 1;
pub const EE_SCREENS_MF_CINE_PAUSE: i64 = 2;
pub const EE_SCREENS_MF_CINE_SCREENSHOT: i64 = 3;
/// The keyboard modes (`mKeyboardModes[state]`): none (the original's two `return false` tests), the cinematic's.
pub const EE_SCREENS_MF_KEYBOARD_CINEMATIC: i64 = 2;
/// The cinematic's keys (DirectInput's codes: `kKeyEscape`, `kKeyF1`, `kKeyF3`, `kKeyF9`).
pub const EE_SCREENS_MF_KEY_ESCAPE: i64 = 0x01;
pub const EE_SCREENS_MF_KEY_F1: i64 = 0x3b;
pub const EE_SCREENS_MF_KEY_F3: i64 = 0x3d;
pub const EE_SCREENS_MF_KEY_F9: i64 = 0x43;

/// The mouse buttons (`EE_SCREENS_MF_OP_MD_BUTTONS`): left just pressed, held, just released; the right's.
pub const EE_SCREENS_MF_BTN_L_PRESSED: i64 = 1;
pub const EE_SCREENS_MF_BTN_L_HELD: i64 = 2;
pub const EE_SCREENS_MF_BTN_L_RELEASED: i64 = 4;
pub const EE_SCREENS_MF_BTN_R_PRESSED: i64 = 8;
pub const EE_SCREENS_MF_BTN_R_HELD: i64 = 16;
pub const EE_SCREENS_MF_BTN_R_RELEASED: i64 = 32;

/// The picked unit's reads (`EE_SCREENS_MF_OP_MD_UNIT`): `mUnitID`; another player's unit, the current player's
/// enemy, seen (`EEIsSeenBy(unit, player, true)`); its class's `mFamily`, `CanBeAttacked()`, `mIsWall`; its tile;
/// its complex unit non-null, `mIsBuilt`, garrison capacity, room (capacity != `mGarrisonCount`); whether the first
/// unit's wonder set holds that type; whether the first unit's complex unit has the complex unit's player.
pub const EE_SCREENS_MF_UNIT_ID: i64 = 0;
pub const EE_SCREENS_MF_UNIT_HOSTILE_SEEN: i64 = 1;
pub const EE_SCREENS_MF_UNIT_FAMILY: i64 = 2;
pub const EE_SCREENS_MF_UNIT_CAN_BE_ATTACKED: i64 = 3;
pub const EE_SCREENS_MF_UNIT_IS_WALL: i64 = 4;
pub const EE_SCREENS_MF_UNIT_TILE_X: i64 = 5;
pub const EE_SCREENS_MF_UNIT_TILE_Y: i64 = 6;
pub const EE_SCREENS_MF_UNIT_HAS_COMPLEX: i64 = 7;
pub const EE_SCREENS_MF_UNIT_COMPLEX_BUILT: i64 = 8;
pub const EE_SCREENS_MF_UNIT_COMPLEX_CAPACITY: i64 = 9;
pub const EE_SCREENS_MF_UNIT_COMPLEX_ROOM: i64 = 10;
pub const EE_SCREENS_MF_UNIT_IN_WONDER_SET: i64 = 11;
pub const EE_SCREENS_MF_UNIT_SAME_PLAYER: i64 = 12;

/// The first unit's reads (`EE_SCREENS_MF_OP_MD_FIRST_FIELD`): `mHitPoints > 0`; its complex unit non-null; its
/// complex unit's `DistanceSquared3D(io.f) >= UMSquare(its class's mMinRange)`; its class's `mCanSetRallyPoint`;
/// `mUnitID == mCalamity->mCasterID`.
pub const EE_SCREENS_MF_FIRST_ALIVE: i64 = 0;
pub const EE_SCREENS_MF_FIRST_HAS_COMPLEX: i64 = 1;
pub const EE_SCREENS_MF_FIRST_IN_RANGE: i64 = 2;
pub const EE_SCREENS_MF_FIRST_CAN_RALLY: i64 = 3;
pub const EE_SCREENS_MF_FIRST_IS_CASTER: i64 = 4;

/// The ghost units' reads (`EE_SCREENS_MF_OP_MD_GHOST`): `mCanPlace`, its class's unit type, `mTileX`, `mTileY`.
pub const EE_SCREENS_MF_GHOST_CAN_PLACE: i64 = 0;
pub const EE_SCREENS_MF_GHOST_TYPE: i64 = 1;
pub const EE_SCREENS_MF_GHOST_TILE_X: i64 = 2;
pub const EE_SCREENS_MF_GHOST_TILE_Y: i64 = 3;

/// The messages (`EE_SCREENS_MF_OP_MD_SEND`): the airport command b at io.f[0..2] on the target io.n[0], queued
/// io.n[1]; attack ground at io.f[0..2]; load the first unit's complex unit into the picked unit; unload the first
/// unit at io.n; heal the picked unit's complex unit; attack (convert) it, queued b; the first unit's rally goal at
/// io.f[0..2] on the target io.n[0], queued io.n[1]; a flare at io.n.
pub const EE_SCREENS_MF_SEND_AIRPORT: i64 = 0;
pub const EE_SCREENS_MF_SEND_ATTACK_GROUND: i64 = 1;
pub const EE_SCREENS_MF_SEND_LOAD_GARRISON: i64 = 2;
pub const EE_SCREENS_MF_SEND_UNLOAD: i64 = 3;
pub const EE_SCREENS_MF_SEND_HEAL: i64 = 4;
pub const EE_SCREENS_MF_SEND_CONVERT: i64 = 5;
pub const EE_SCREENS_MF_SEND_RALLY: i64 = 6;
pub const EE_SCREENS_MF_SEND_FLARE: i64 = 7;

/// The modes in the form's table order (main_form.rs' mouse tests: `mMouseTests[state]` holds one).
pub const EE_SCREENS_MF_MODE_NORMAL: i64 = 0;
pub const EE_SCREENS_MF_MODE_BOMBER_RALLY: i64 = 1;
pub const EE_SCREENS_MF_MODE_FIGHTER_RALLY: i64 = 2;
pub const EE_SCREENS_MF_MODE_ATOMIC_BOMBER_RALLY: i64 = 3;
pub const EE_SCREENS_MF_MODE_BUILD: i64 = 4;
pub const EE_SCREENS_MF_MODE_RALLY: i64 = 5;
pub const EE_SCREENS_MF_MODE_UNLOAD: i64 = 6;
pub const EE_SCREENS_MF_MODE_GARRISON: i64 = 7;
pub const EE_SCREENS_MF_MODE_CALAMITY: i64 = 8;
pub const EE_SCREENS_MF_MODE_WALL: i64 = 9;
pub const EE_SCREENS_MF_MODE_GATE: i64 = 10;
pub const EE_SCREENS_MF_MODE_TIMED_ATOMIC_BOMBER_RALLY: i64 = 11;
pub const EE_SCREENS_MF_MODE_TIMED_BOMBER_RALLY: i64 = 12;
pub const EE_SCREENS_MF_MODE_TIMED_FIGHTER_RALLY: i64 = 13;
pub const EE_SCREENS_MF_MODE_TIMED_RALLY: i64 = 14;
pub const EE_SCREENS_MF_MODE_CAMERA: i64 = 15;
pub const EE_SCREENS_MF_MODE_HEAL: i64 = 16;
pub const EE_SCREENS_MF_MODE_NONE: i64 = 17;
pub const EE_SCREENS_MF_MODE_FLARE: i64 = 18;
pub const EE_SCREENS_MF_MODE_ATTACK_GROUND: i64 = 19;
pub const EE_SCREENS_MF_MODE_PARATROOPER_RALLY: i64 = 20;
pub const EE_SCREENS_MF_MODE_CONVERT: i64 = 21;

/// `kUnitTypeFarm`, `kUnitTypeRoboticFarm` (the build mode's farms may overlap when several are placed).
pub const EE_SCREENS_MF_TYPE_FARM: i64 = 0x2a;
pub const EE_SCREENS_MF_TYPE_ROBOTIC_FARM: i64 = 0x33a;

/// The pointer shapes the modes set (`gMouse->mPointer`).
const EE_SCREENS_MM_PTR_NORMAL: i64 = 0;
const EE_SCREENS_MM_PTR_ATTACK: i64 = 1;
const EE_SCREENS_MM_PTR_NO_RANGE: i64 = 3;
const EE_SCREENS_MM_PTR_UNSEEN: i64 = 7;
const EE_SCREENS_MM_PTR_CONVERT: i64 = 10;
const EE_SCREENS_MM_PTR_CALAMITY: i64 = 0xd;
const EE_SCREENS_MM_PTR_CALAMITY_1: i64 = 0xe;
const EE_SCREENS_MM_PTR_TARGET: i64 = 0xf;
const EE_SCREENS_MM_PTR_CONVERT_RANGE: i64 = 0x10;
const EE_SCREENS_MM_PTR_CALAMITY_2: i64 = 0x11;
const EE_SCREENS_MM_PTR_HEAL: i64 = 0x12;
const EE_SCREENS_MM_PTR_CANNOT: i64 = 0x13;
const EE_SCREENS_MM_PTR_FLARE: i64 = 0x16;

/// The events the refusals add (the language's 0x7c3a "cannot garrison here", 0x7c41 "nothing to garrison in",
/// 0x7c3b "nothing to unload").
const EE_SCREENS_MM_EVENT_GARRISON_FULL: i64 = 0x7c3a;
const EE_SCREENS_MM_EVENT_GARRISON_NONE: i64 = 0x7c41;
const EE_SCREENS_MM_EVENT_UNLOAD_NONE: i64 = 0x7c3b;

/// `EEUIBuildWall`'s members from `mState` on (the class's vptr before them): placing (0 no start, 1 dragging),
/// the tile centres, the wall's technology and player.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeScreensBuildWallFields {
    pub state: u32,
    pub start: EeScreensPoint,
    pub end: EeScreensPoint,
    pub technology: i32,
    pub player: i32,
}

/// `EEUIBuildGate`'s members from `mUnitClass` on: the gate's class, the tile of the last build, the technology,
/// the highlighted walls' type, the gate's direction, the player.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct EeScreensBuildGateFields {
    pub unit_class: *mut core::ffi::c_void,
    pub tile_x: i32,
    pub tile_y: i32,
    pub technology: i32,
    pub wall_type: i32,
    pub direction: i32,
    pub player: i32,
}

thread_local! {
    /// The airport commands' and the rally point's shift state (0x5652f0, 0x5652f1; the UI thread's).
    static AIRPORT_SHIFT: Cell<bool> = const { Cell::new(false) };
    static RALLY_SHIFT: Cell<bool> = const { Cell::new(false) };
}

/// The mouse at (x, y) in the form's interface state's mode (`mMouseTests[mUIState]`): the mode's answer (always 0).
pub fn mouse(h: &mut impl FormHost, mode: i64, x: f32, y: f32) -> i64 {
    let m = Modes { x, y };
    match mode {
        EE_SCREENS_MF_MODE_NORMAL => m.normal(h),
        EE_SCREENS_MF_MODE_BOMBER_RALLY => m.airport(h, 9, 0),
        EE_SCREENS_MF_MODE_FIGHTER_RALLY => m.airport(h, 8, 0xc),
        EE_SCREENS_MF_MODE_ATOMIC_BOMBER_RALLY => m.airport(h, 0xa, 0),
        EE_SCREENS_MF_MODE_BUILD => m.build(h),
        EE_SCREENS_MF_MODE_RALLY => m.rally(h),
        EE_SCREENS_MF_MODE_UNLOAD => m.unload(h),
        EE_SCREENS_MF_MODE_GARRISON => m.garrison(h),
        EE_SCREENS_MF_MODE_CALAMITY => m.calamity(h),
        EE_SCREENS_MF_MODE_WALL => {
            h.set_field(EE_SCREENS_MF_FIELD_CURSOR_LOCKED, 1);
            if !m.captured(h) {
                h.a(EE_SCREENS_MF_OP_MD_POINTER, EE_SCREENS_MM_PTR_NORMAL);
                h.a(EE_SCREENS_MF_OP_MD_PLACEMENT, 0);
            }
        }
        EE_SCREENS_MF_MODE_GATE => {
            h.set_field(EE_SCREENS_MF_FIELD_CURSOR_LOCKED, 1);
            if !m.captured(h) {
                h.a(EE_SCREENS_MF_OP_MD_POINTER, EE_SCREENS_MM_PTR_NORMAL);
                h.a(EE_SCREENS_MF_OP_MD_PLACEMENT, 1);
            }
        }
        EE_SCREENS_MF_MODE_TIMED_ATOMIC_BOMBER_RALLY => m.airport(h, 0xc, 0),
        EE_SCREENS_MF_MODE_TIMED_BOMBER_RALLY => m.airport(h, 0xd, 0),
        EE_SCREENS_MF_MODE_TIMED_FIGHTER_RALLY => m.airport(h, 0xe, 0xc),
        EE_SCREENS_MF_MODE_TIMED_RALLY => m.airport(h, 0xf, 0xc),
        EE_SCREENS_MF_MODE_CAMERA => m.camera(h),
        EE_SCREENS_MF_MODE_HEAL => m.heal(h),
        EE_SCREENS_MF_MODE_FLARE => m.flare(h),
        EE_SCREENS_MF_MODE_ATTACK_GROUND => m.attack_ground(h),
        EE_SCREENS_MF_MODE_PARATROOPER_RALLY => m.airport(h, 0x14, 0),
        EE_SCREENS_MF_MODE_CONVERT => m.convert(h),
        // EEModeMouseNone and anything else
        _ => {}
    }
    0
}

/// `EEUIFMainGame::UpdateMouse` (0x2712b8): the unit hack, the form's controls, then the interface state's mode
/// (read after the controls ran: a button may change the state); the last `UpdateMouse` result (the modes answer 0).
pub fn update_mouse(h: &mut impl FormHost) -> i64 {
    h.set_field(EE_SCREENS_MF_FIELD_B5F0, 0);
    if h.field(EE_SCREENS_MF_FIELD_UI_STATE) == EE_SCREENS_MF_STATE_NO_SCROLL {
        return 0;
    }
    let mut io = EeScreensMainFormIo::default();
    h.op(EE_SCREENS_MF_OP_MD_MOUSE_AT, 0, 0, &mut io);
    h.op(EE_SCREENS_MF_OP_MD_HACK, 2, 0, &mut io);
    if h.t(EE_SCREENS_MF_OP_MOUSE_CAPTURED) {
        return h.a(EE_SCREENS_MF_OP_BASE, EE_SCREENS_MF_BASE_UPDATE_MOUSE);
    }
    let result = h.a(EE_SCREENS_MF_OP_BASE, EE_SCREENS_MF_BASE_UPDATE_MOUSE);
    if h.t(EE_SCREENS_MF_OP_MOUSE_CAPTURED) {
        return result;
    }
    let mut io = EeScreensMainFormIo::default();
    let mode = h.op(EE_SCREENS_MF_OP_MD_MOUSE_AT, 0, 0, &mut io);
    mouse(h, mode, io.f[0], io.f[1])
}

/// `EEUIFMainGame::UpdateKeyboard` (0x271290) in the keyboard mode `mode` (`mKeyboardModes[mUIState]`): the mode's
/// keys, else the form's (`UIForm::UpdateKeyboard`'s result; 0 when the mode took the keys).
pub fn update_keyboard(h: &mut impl FormHost, mode: i64) -> i64 {
    if mode == EE_SCREENS_MF_KEYBOARD_CINEMATIC {
        cinematic_keys(h);
        return 0;
    }
    h.a(EE_SCREENS_MF_OP_BASE, EE_SCREENS_MF_BASE_UPDATE_KEYBOARD)
}

/// `EEModeKeyboardCinematic` (0x27115c): Escape or F1 ends the cinematic (unless the game is paused), else F3 pauses
/// (unless the cinematic is ending), else F9 takes a screenshot; it always takes the keys.
fn cinematic_keys(h: &mut impl FormHost) {
    let pressed = |h: &mut _, key, bits: i64| FormHost::a(h, EE_SCREENS_MF_OP_MD_KEY, key) & bits == bits;
    let down = EE_SCREENS_MF_KEY_FOUND | EE_SCREENS_MF_KEY_HELD | EE_SCREENS_MF_KEY_JUST_PRESSED;
    let just = EE_SCREENS_MF_KEY_FOUND | EE_SCREENS_MF_KEY_JUST_PRESSED;
    if (pressed(h, EE_SCREENS_MF_KEY_ESCAPE, down) || pressed(h, EE_SCREENS_MF_KEY_F1, down))
        && h.a(EE_SCREENS_MF_OP_COMMS_PAUSED, 0) == 0
    {
        h.a(EE_SCREENS_MF_OP_MD_CINEMATIC, EE_SCREENS_MF_CINE_END);
    } else if pressed(h, EE_SCREENS_MF_KEY_F3, just)
        && h.a(EE_SCREENS_MF_OP_MD_CINEMATIC, EE_SCREENS_MF_CINE_ENDING) == 0
    {
        h.a(EE_SCREENS_MF_OP_MD_CINEMATIC, EE_SCREENS_MF_CINE_PAUSE);
    } else if pressed(h, EE_SCREENS_MF_KEY_F9, just) {
        h.a(EE_SCREENS_MF_OP_MD_CINEMATIC, EE_SCREENS_MF_CINE_SCREENSHOT);
    }
}

/// The mouse location of one call and the ops' shapes.
struct Modes {
    x: f32,
    y: f32,
}

impl Modes {
    fn loc(&self) -> EeScreensMainFormIo {
        EeScreensMainFormIo { f: [self.x, self.y, 0.0, 0.0], ..Default::default() }
    }
    fn captured(&self, h: &mut impl FormHost) -> bool {
        h.t(EE_SCREENS_MF_OP_MD_CAPTURED)
    }
    fn lock(&self, h: &mut impl FormHost, locked: bool) {
        h.set_field(EE_SCREENS_MF_FIELD_CURSOR_LOCKED, locked as i64);
    }
    fn pointer(&self, h: &mut impl FormHost, pointer: i64) {
        h.a(EE_SCREENS_MF_OP_MD_POINTER, pointer);
    }
    fn ui_state(&self, h: &mut impl FormHost) {
        h.ab(EE_SCREENS_MF_OP_MD_UI_STATE, 0, 0);
    }
    fn buttons(&self, h: &mut impl FormHost) -> i64 {
        h.a(EE_SCREENS_MF_OP_MD_BUTTONS, 0)
    }
    fn shift(&self, h: &mut impl FormHost) -> i64 {
        h.a(EE_SCREENS_MF_OP_MD_SHIFT, 0)
    }
    /// The view rectangle (`gViewport`'s, or the rasterizer's with `raster`) contains the location.
    fn in_view(&self, h: &mut impl FormHost, raster: bool) -> bool {
        h.op(EE_SCREENS_MF_OP_MD_VIEW, raster as i64, 0, &mut self.loc()) != 0
    }
    /// `EEPointUnitOrTile`: found, the point.
    fn point(&self, h: &mut impl FormHost) -> (bool, [f32; 3]) {
        let mut io = self.loc();
        let found = h.op(EE_SCREENS_MF_OP_MD_POINT, 0, 0, &mut io) != 0;
        (found, [io.f[0], io.f[1], io.f[2]])
    }
    /// `EEPointUnitOrTileXY`: found, the tile.
    fn point_xy(&self, h: &mut impl FormHost) -> (bool, [i64; 2]) {
        let mut io = self.loc();
        let found = h.op(EE_SCREENS_MF_OP_MD_POINT, 1, 0, &mut io) != 0;
        (found, io.n)
    }
    fn pick(&self, h: &mut impl FormHost) -> bool {
        h.op(EE_SCREENS_MF_OP_MD_PICK, 0, 0, &mut self.loc()) != 0
    }
    fn unit(&self, h: &mut impl FormHost, field: i64) -> i64 {
        h.a(EE_SCREENS_MF_OP_MD_UNIT, field)
    }
    fn release(&self, h: &mut impl FormHost) {
        h.a(EE_SCREENS_MF_OP_RELEASE, 0);
    }
    /// `EEGetFirstSelectedUnit(first) == true` (the first unit held; non-null in the second).
    fn first(&self, h: &mut impl FormHost) -> (bool, bool) {
        let bits = h.a(EE_SCREENS_MF_OP_MD_FIRST, 0);
        (bits & 1 != 0, bits & 2 != 0)
    }
    fn first_field(&self, h: &mut impl FormHost, field: i64) -> bool {
        h.a(EE_SCREENS_MF_OP_MD_FIRST_FIELD, field) != 0
    }
    fn send(&self, h: &mut impl FormHost, message: i64, b: i64, io: EeScreensMainFormIo) {
        h.io(EE_SCREENS_MF_OP_MD_SEND, message, b, io);
    }
    fn order_at(&self, h: &mut impl FormHost, unit: i64) {
        h.op(EE_SCREENS_MF_OP_MD_ORDER_AT, unit, 0, &mut self.loc());
    }
    fn event(&self, h: &mut impl FormHost, tile: [i64; 2], message: i64) {
        h.io(EE_SCREENS_MF_OP_MD_EVENT, message, 0, EeScreensMainFormIo { n: tile, ..Default::default() });
    }
    fn clear_ghosts(&self, h: &mut impl FormHost) {
        h.a(EE_SCREENS_MF_OP_GHOST_LOCK, 1);
        h.a(EE_SCREENS_MF_OP_CLEAR_GHOSTS, 0);
        h.a(EE_SCREENS_MF_OP_GHOST_LOCK, 0);
    }
    /// The mode's end: the normal state, the cursor free.
    fn leave(&self, h: &mut impl FormHost) {
        self.ui_state(h);
        self.lock(h, false);
    }

    /// `EEModeAirport` (0x26fc22): the airport command `command` (`kind` the attack's damage column) at the clicked
    /// point and unit; Shift keeps the mode.
    fn airport(&self, h: &mut impl FormHost, command: i64, kind: i64) {
        let mut target = -1;
        self.lock(h, true);
        if self.captured(h) || !self.in_view(h, false) {
            self.pointer(h, EE_SCREENS_MM_PTR_NORMAL);
            return;
        }
        let shift = self.shift(h) > 0;
        self.pointer(h, EE_SCREENS_MM_PTR_TARGET);
        let (_, point) = self.point(h);
        if command != 0xf {
            if self.pick(h) {
                target = self.unit(h, EE_SCREENS_MF_UNIT_ID) as i32 as i64;
                if self.unit(h, EE_SCREENS_MF_UNIT_HOSTILE_SEEN) != 0 {
                    let mut chance = 0.0f32;
                    let family = self.unit(h, EE_SCREENS_MF_UNIT_FAMILY);
                    if self.unit(h, EE_SCREENS_MF_UNIT_CAN_BE_ATTACKED) != 0 {
                        chance = h.io(EE_SCREENS_MF_OP_MD_DAMAGE, family, kind, EeScreensMainFormIo::default()).f[0];
                    }
                    if chance > 0.0 {
                        self.pointer(h, EE_SCREENS_MM_PTR_ATTACK);
                    }
                }
                self.release(h);
            } else {
                target = -1;
            }
        }
        let b = self.buttons(h);
        if b & EE_SCREENS_MF_BTN_L_PRESSED == 0 && b & EE_SCREENS_MF_BTN_R_RELEASED == 0 {
            if shift || !AIRPORT_SHIFT.with(Cell::get) {
                return;
            }
            AIRPORT_SHIFT.with(|s| s.set(false));
        } else {
            AIRPORT_SHIFT.with(|s| s.set(shift));
            let io = EeScreensMainFormIo { f: [point[0], point[1], 0.0, 0.0], n: [target, shift as i64] };
            self.send(h, EE_SCREENS_MF_SEND_AIRPORT, command, io);
            if shift {
                return;
            }
        }
        self.leave(h);
        self.pointer(h, EE_SCREENS_MM_PTR_NORMAL);
    }

    /// `EEModeAttackGround` (0x26ff0c): the range cursor, then an attack-ground order (and the calamity effect) at
    /// the clicked point.
    fn attack_ground(&self, h: &mut impl FormHost) {
        if self.captured(h) {
            return;
        }
        let b = self.buttons(h);
        if b & EE_SCREENS_MF_BTN_L_PRESSED == 0 && b & EE_SCREENS_MF_BTN_R_PRESSED == 0 {
            let (found, point) = self.point(h);
            if found {
                if h.t(EE_SCREENS_MF_OP_MD_TILE_SEEN) {
                    if self.first(h).0 {
                        let in_range = self.in_range(h, point);
                        self.pointer(h, if in_range { EE_SCREENS_MM_PTR_ATTACK } else { EE_SCREENS_MM_PTR_NO_RANGE });
                    } else {
                        self.pointer(h, EE_SCREENS_MM_PTR_NORMAL);
                    }
                } else {
                    self.pointer(h, EE_SCREENS_MM_PTR_UNSEEN);
                }
            }
            return;
        }
        let (found, point) = self.point(h);
        if !found || !h.t(EE_SCREENS_MF_OP_MD_TILE_SEEN) {
            return;
        }
        let picked = self.pick(h);
        if self.first(h).0 {
            if self.in_range(h, point) {
                let io = EeScreensMainFormIo { f: [point[0], point[1], 0.0, 0.0], ..Default::default() };
                self.send(h, EE_SCREENS_MF_SEND_ATTACK_GROUND, 0, io);
            }
            let io = EeScreensMainFormIo { f: [point[0], point[1], point[2], 0.0], ..Default::default() };
            h.io(EE_SCREENS_MF_OP_MD_CALAMITY_EFFECT, 0, 0, io);
        }
        self.ui_state(h);
        self.pointer(h, EE_SCREENS_MM_PTR_NORMAL);
        self.lock(h, false);
        if picked {
            self.release(h);
        }
    }

    fn in_range(&self, h: &mut impl FormHost, point: [f32; 3]) -> bool {
        let io = EeScreensMainFormIo { f: [point[0], point[1], point[2], 0.0], ..Default::default() };
        h.op(EE_SCREENS_MF_OP_MD_FIRST_FIELD, EE_SCREENS_MF_FIRST_IN_RANGE, 0, &mut { io }) != 0
    }

    /// `EEModeBuild` (0x27012b): the building's ghost at the mouse, placed on a left release when every ghost can
    /// be placed (a lone farm too; several farms may overlap); Shift queues and keeps the mode; a right release
    /// cancels.
    fn build(&self, h: &mut impl FormHost) {
        if self.captured(h) {
            return;
        }
        h.a(EE_SCREENS_MF_OP_MD_PREVIEW, 0);
        self.pointer(h, EE_SCREENS_MM_PTR_NORMAL);
        let b = self.buttons(h);
        if b & (EE_SCREENS_MF_BTN_L_PRESSED | EE_SCREENS_MF_BTN_L_HELD) != 0 {
            return;
        }
        if b & EE_SCREENS_MF_BTN_L_RELEASED != 0 {
            h.a(EE_SCREENS_MF_OP_GHOST_LOCK, 1);
            let count = h.ab(EE_SCREENS_MF_OP_MD_GHOST, -1, 0) as u32;
            let blocked = (0..count as i64).any(|i| {
                let ty = h.ab(EE_SCREENS_MF_OP_MD_GHOST, i, EE_SCREENS_MF_GHOST_TYPE);
                let farm = ty == EE_SCREENS_MF_TYPE_FARM || ty == EE_SCREENS_MF_TYPE_ROBOTIC_FARM;
                h.ab(EE_SCREENS_MF_OP_MD_GHOST, i, EE_SCREENS_MF_GHOST_CAN_PLACE) == 0 && (farm && count == 1 || !farm)
            });
            if !blocked {
                let shift = self.shift(h);
                let tile_x = h.ab(EE_SCREENS_MF_OP_MD_GHOST, 0, EE_SCREENS_MF_GHOST_TILE_X) as i32 as f32;
                let tile_y = h.ab(EE_SCREENS_MF_OP_MD_GHOST, 0, EE_SCREENS_MF_GHOST_TILE_Y) as i32 as f32;
                let io = EeScreensMainFormIo { f: [tile_x, tile_y, 0.0, 0.0], ..Default::default() };
                h.io(EE_SCREENS_MF_OP_MD_BUILD, (shift > 0) as i64, 0, io);
                if shift == 0 {
                    h.a(EE_SCREENS_MF_OP_CLEAR_GHOSTS, 0);
                    self.ui_state(h);
                }
            }
            h.a(EE_SCREENS_MF_OP_GHOST_LOCK, 0);
        } else if b & (EE_SCREENS_MF_BTN_R_PRESSED | EE_SCREENS_MF_BTN_R_HELD) == 0
            && b & EE_SCREENS_MF_BTN_R_RELEASED != 0
        {
            self.clear_ghosts(h);
            self.ui_state(h);
        }
    }

    /// `EEModeFlare` (0x2703af): a click on the map sends a flare there and leaves the mode.
    fn flare(&self, h: &mut impl FormHost) {
        self.lock(h, true);
        self.pointer(h, EE_SCREENS_MM_PTR_FLARE);
        if self.captured(h) {
            return;
        }
        let b = self.buttons(h);
        if b & EE_SCREENS_MF_BTN_L_PRESSED == 0 && b & EE_SCREENS_MF_BTN_R_PRESSED == 0 {
            return;
        }
        let (found, tile) = self.point_xy(h);
        if found {
            self.order_at(h, 0);
            self.send(h, EE_SCREENS_MF_SEND_FLARE, 0, EeScreensMainFormIo { n: tile, ..Default::default() });
        }
        self.ui_state(h);
        self.pointer(h, EE_SCREENS_MM_PTR_NORMAL);
        self.lock(h, false);
    }

    /// The press tests of the garrison and unload modes: not captured, in the rasterizer's view, a press, a tile.
    fn pressed_tile(&self, h: &mut impl FormHost) -> Option<[i64; 2]> {
        if self.captured(h) || !self.in_view(h, true) {
            return None;
        }
        let b = self.buttons(h);
        if b & EE_SCREENS_MF_BTN_R_PRESSED == 0 && b & EE_SCREENS_MF_BTN_L_PRESSED == 0 {
            return None;
        }
        let (found, tile) = self.point_xy(h);
        found.then_some(tile)
    }

    /// `EEModeGarrison` (0x2704c9): garrisons the first selected unit into the clicked building of its player with
    /// room, else an event says why.
    fn garrison(&self, h: &mut impl FormHost) {
        let Some(tile) = self.pressed_tile(h) else {
            return;
        };
        if self.pick(h) {
            let complex = self.unit(h, EE_SCREENS_MF_UNIT_HAS_COMPLEX) != 0;
            let message = 'msg: {
                if !self.first(h).0 {
                    break 'msg EE_SCREENS_MM_EVENT_GARRISON_NONE;
                }
                let own = self.first_field(h, EE_SCREENS_MF_FIRST_HAS_COMPLEX);
                if own
                    && complex
                    && self.unit(h, EE_SCREENS_MF_UNIT_COMPLEX_BUILT) != 0
                    && self.unit(h, EE_SCREENS_MF_UNIT_SAME_PLAYER) != 0
                    && self.unit(h, EE_SCREENS_MF_UNIT_COMPLEX_ROOM) != 0
                {
                    self.send(h, EE_SCREENS_MF_SEND_LOAD_GARRISON, 0, EeScreensMainFormIo::default());
                    self.order_at(h, 2);
                    break 'msg 0;
                }
                if complex && self.unit(h, EE_SCREENS_MF_UNIT_COMPLEX_CAPACITY) != 0 {
                    EE_SCREENS_MM_EVENT_GARRISON_FULL
                } else {
                    EE_SCREENS_MM_EVENT_GARRISON_NONE
                }
            };
            if message != 0 {
                self.event(h, tile, message);
            }
            self.release(h);
        } else {
            self.event(h, tile, EE_SCREENS_MM_EVENT_GARRISON_NONE);
        }
        self.ui_state(h);
        self.pointer(h, EE_SCREENS_MM_PTR_NORMAL);
        self.lock(h, false);
    }

    /// `EEModeUnload` (0x270703): unloads the selected transport at the clicked tile.
    fn unload(&self, h: &mut impl FormHost) {
        let Some(tile) = self.pressed_tile(h) else {
            return;
        };
        let picked = self.pick(h);
        let (first, present) = self.first(h);
        if first {
            if present {
                self.send(h, EE_SCREENS_MF_SEND_UNLOAD, 0, EeScreensMainFormIo { n: tile, ..Default::default() });
                self.order_at(h, 1);
            }
        } else {
            self.event(h, tile, EE_SCREENS_MM_EVENT_UNLOAD_NONE);
        }
        self.ui_state(h);
        self.pointer(h, EE_SCREENS_MM_PTR_NORMAL);
        self.lock(h, false);
        if picked {
            self.release(h);
        }
    }

    /// `EEModeHeal` (0x270898): a release on a unit the healer's wonder set holds sends a heal order.
    fn heal(&self, h: &mut impl FormHost) {
        self.lock(h, true);
        self.pointer(h, EE_SCREENS_MM_PTR_HEAL);
        if self.captured(h) || !self.in_view(h, true) {
            return;
        }
        if !self.first(h).0 {
            self.leave(h);
            return;
        }
        let picked = self.pick(h);
        let b = self.buttons(h);
        let done = 'done: {
            if picked && self.unit(h, EE_SCREENS_MF_UNIT_HAS_COMPLEX) != 0 {
                if self.unit(h, EE_SCREENS_MF_UNIT_IN_WONDER_SET) != 0 {
                    if b & (EE_SCREENS_MF_BTN_L_RELEASED | EE_SCREENS_MF_BTN_R_RELEASED) == 0 {
                        break 'done true;
                    }
                    self.send(h, EE_SCREENS_MF_SEND_HEAL, 0, EeScreensMainFormIo::default());
                } else {
                    self.pointer(h, EE_SCREENS_MM_PTR_CANNOT);
                    break 'done true;
                }
            } else if b & EE_SCREENS_MF_BTN_R_RELEASED == 0 {
                break 'done true;
            }
            false
        };
        if !done {
            self.leave(h);
        }
        if picked {
            self.release(h);
        }
    }

    /// `EEModeConvert` (0x2709ee): whether a selected unit can convert the unit under the cursor (and is in range),
    /// and the order on a release (Shift keeps the mode).
    fn convert(&self, h: &mut impl FormHost) {
        let first = h.a(EE_SCREENS_MF_OP_MD_FIRST, 1) != 0;
        self.lock(h, true);
        self.pointer(h, EE_SCREENS_MM_PTR_CONVERT);
        if self.captured(h) || !self.in_view(h, true) {
            return;
        }
        if !first {
            self.leave(h);
            return;
        }
        let picked = self.pick(h);
        let done = 'done: {
            if !picked || self.unit(h, EE_SCREENS_MF_UNIT_HAS_COMPLEX) == 0 {
                if self.buttons(h) & EE_SCREENS_MF_BTN_R_RELEASED == 0 {
                    self.pointer(h, EE_SCREENS_MM_PTR_CANNOT);
                    break 'done true;
                }
            } else {
                let mut found = false;
                let mut in_range = false;
                let mut i = 0i64;
                while (i as u32) < h.a(EE_SCREENS_MF_OP_SELECTED_COUNT, 0) as u32 && !found {
                    if in_range {
                        self.pointer(h, EE_SCREENS_MM_PTR_CONVERT_RANGE);
                        break 'done true;
                    }
                    let bits = h.ab(EE_SCREENS_MF_OP_MD_CAN_CONVERT, i, in_range as i64);
                    found = bits & 1 != 0;
                    in_range = bits & 2 != 0;
                    i += 1;
                }
                if !found {
                    self.pointer(h, if in_range { EE_SCREENS_MM_PTR_CONVERT_RANGE } else { EE_SCREENS_MM_PTR_CANNOT });
                    break 'done true;
                }
                if self.buttons(h) & (EE_SCREENS_MF_BTN_L_RELEASED | EE_SCREENS_MF_BTN_R_RELEASED) == 0 {
                    break 'done true;
                }
                let shift = self.shift(h);
                self.send(h, EE_SCREENS_MF_SEND_CONVERT, (shift > 0) as i64, EeScreensMainFormIo::default());
                if shift != 0 {
                    break 'done true;
                }
            }
            false
        };
        if !done {
            self.leave(h);
        }
        if picked {
            self.release(h);
        }
    }

    /// `EEModeCalamity` (0x270c08): the calamity's ghost at the cursor, its cursor by the check, the cast by its
    /// caster on a release; the mode ends when the check passes (or no unit is selected).
    fn calamity(&self, h: &mut impl FormHost) {
        self.lock(h, true);
        if self.captured(h) {
            return;
        }
        h.a(EE_SCREENS_MF_OP_MD_PREVIEW, 0);
        if !self.in_view(h, true) {
            self.pointer(h, EE_SCREENS_MM_PTR_NORMAL);
            return;
        }
        match h.op(EE_SCREENS_MF_OP_MD_CALAMITY_CHECK, 0, 0, &mut self.loc()) as i32 {
            0 => {
                if self.first(h).0 && self.first_field(h, EE_SCREENS_MF_FIRST_ALIVE) {
                    self.pointer(h, EE_SCREENS_MM_PTR_CALAMITY);
                    if self.first_field(h, EE_SCREENS_MF_FIRST_IS_CASTER) {
                        if self.buttons(h) & (EE_SCREENS_MF_BTN_L_RELEASED | EE_SCREENS_MF_BTN_R_RELEASED) == 0 {
                            return;
                        }
                        h.a(EE_SCREENS_MF_OP_CALAMITY_CAST, 0);
                    }
                }
            }
            check @ (1 | 2) => {
                if self.first(h).0 {
                    let alive = self.first_field(h, EE_SCREENS_MF_FIRST_ALIVE);
                    let shown = if check == 1 { EE_SCREENS_MM_PTR_CALAMITY_1 } else { EE_SCREENS_MM_PTR_CALAMITY_2 };
                    self.pointer(h, if alive { shown } else { EE_SCREENS_MM_PTR_NORMAL });
                    return;
                }
            }
            _ => return,
        }
        self.clear_ghosts(h);
        self.leave(h);
    }

    /// `EEModeNormal` (0x270d77): a left press starts the box selection (and the unit hack's pick), its release
    /// places the hack's unit, a right press starts the command drag; a press first brings back the main panel.
    fn normal(&self, h: &mut impl FormHost) {
        let in_view = h.op(EE_SCREENS_MF_OP_MD_IN_GAME_VIEW, 0, 0, &mut self.loc()) != 0;
        if self.captured(h) {
            return;
        }
        let b = self.buttons(h);
        if b & EE_SCREENS_MF_BTN_L_PRESSED != 0 {
            self.main_panel(h);
            h.a(EE_SCREENS_MF_OP_MD_CAMERA, 0);
            if in_view {
                let (found, point) = self.point(h);
                if found {
                    let io = EeScreensMainFormIo { f: [point[0], point[1], 0.0, 0.0], ..Default::default() };
                    h.io(EE_SCREENS_MF_OP_MD_HACK, 0, 0, io);
                }
                h.op(EE_SCREENS_MF_OP_MD_START, 0, 0, &mut self.loc());
            }
        } else if b & EE_SCREENS_MF_BTN_L_HELD == 0 {
            if b & EE_SCREENS_MF_BTN_L_RELEASED != 0 {
                let (found, point) = self.point(h);
                if in_view && found {
                    let io = EeScreensMainFormIo { f: [point[0], point[1], 0.0, 0.0], ..Default::default() };
                    h.io(EE_SCREENS_MF_OP_MD_HACK, 1, 0, io);
                }
            } else if b & EE_SCREENS_MF_BTN_R_PRESSED != 0 {
                self.main_panel(h);
                if in_view {
                    h.op(EE_SCREENS_MF_OP_MD_START, 1, 0, &mut self.loc());
                }
            }
        }
    }

    /// The main panel's entity group back when it is off.
    fn main_panel(&self, h: &mut impl FormHost) {
        if h.a(EE_SCREENS_MF_OP_MD_ENTITY_ENABLED, 1) == 0 {
            h.set_field(EE_SCREENS_MF_FIELD_CURRENT_GROUP, 0);
            h.a(EE_SCREENS_MF_OP_MD_SET_ENTITY_GROUP, 1);
        }
    }

    /// `EEModeRally` (0x270f20): the selected building's rally goal at the clicked point and unit; Shift keeps the
    /// mode.
    fn rally(&self, h: &mut impl FormHost) {
        self.lock(h, true);
        let mut done = false;
        if !self.captured(h) && self.in_view(h, false) {
            self.pointer(h, EE_SCREENS_MM_PTR_TARGET);
            let shift = self.shift(h) > 0;
            let b = self.buttons(h);
            if b & EE_SCREENS_MF_BTN_L_PRESSED == 0 && b & EE_SCREENS_MF_BTN_R_RELEASED == 0 {
                if shift || !RALLY_SHIFT.with(Cell::get) {
                    return;
                }
                RALLY_SHIFT.with(|s| s.set(false));
                done = true;
            } else {
                RALLY_SHIFT.with(|s| s.set(shift));
                if self.first(h).0 {
                    if self.first_field(h, EE_SCREENS_MF_FIRST_CAN_RALLY) {
                        let (_, point) = self.point(h);
                        let target = if self.pick(h) {
                            let id = self.unit(h, EE_SCREENS_MF_UNIT_ID) as i32 as i64;
                            self.release(h);
                            id
                        } else {
                            -1
                        };
                        let io = EeScreensMainFormIo { f: [point[0], point[1], 0.0, 0.0], n: [target, shift as i64] };
                        self.send(h, EE_SCREENS_MF_SEND_RALLY, 0, io);
                    }
                } else {
                    done = true;
                }
                if !shift {
                    done = true;
                }
            }
        } else {
            self.pointer(h, EE_SCREENS_MM_PTR_NORMAL);
        }
        if !done {
            return;
        }
        self.leave(h);
        self.pointer(h, EE_SCREENS_MM_PTR_NORMAL);
    }

    /// `EEModeCamera` (0x2710d0): a left press stops following a unit and starts the box selection in the view.
    fn camera(&self, h: &mut impl FormHost) {
        let in_view = self.in_view(h, false);
        if self.captured(h) {
            return;
        }
        if self.buttons(h) & EE_SCREENS_MF_BTN_L_PRESSED != 0 {
            h.a(EE_SCREENS_MF_OP_MD_CAMERA, 1);
            if in_view {
                h.op(EE_SCREENS_MF_OP_MD_START, 0, 0, &mut self.loc());
            }
        }
    }
}

/// A tile's centre (`theTile.GetX() + 0.5f` of the tile's long).
fn centre(tile: [i64; 2]) -> EeScreensPoint {
    EeScreensPoint { x: tile[0] as i32 as f32 + 0.5, y: tile[1] as i32 as f32 + 0.5 }
}

/// `EEUIBuildWall::Update` (0x2564df): a left release in the view fixes the start (then builds each segment and
/// chains from its end); a right release cancels; a move previews the wall ghosts along the line.
pub fn wall_update(h: &mut impl FormHost, wall: &Rec<EeScreensBuildWallFields>, x: f32, y: f32) {
    let m = Modes { x, y };
    let (_, tile) = m.point_xy(h);
    let b = m.buttons(h);
    if b & (EE_SCREENS_MF_BTN_L_PRESSED | EE_SCREENS_MF_BTN_L_HELD) != 0 {
        return;
    }
    if b & EE_SCREENS_MF_BTN_L_RELEASED != 0 && m.in_view(h, false) {
        match wall.get(|w| w.state) {
            0 => {
                wall.set(|w| {
                    w.state = 1;
                    w.start = centre(tile);
                    w.end = w.start;
                });
                h.a(EE_SCREENS_MF_OP_MD_BUILD_WALL, 0);
            }
            1 => {
                wall.set(|w| w.end = centre(tile));
                h.a(EE_SCREENS_MF_OP_MD_BUILD_WALL, 0);
                wall.set(|w| w.start = centre(tile));
            }
            _ => {}
        }
    } else if b & (EE_SCREENS_MF_BTN_R_PRESSED | EE_SCREENS_MF_BTN_R_HELD) == 0 {
        if b & EE_SCREENS_MF_BTN_R_RELEASED != 0 {
            m.clear_ghosts(h);
            if wall.get(|w| w.state) <= 1 {
                h.ab(EE_SCREENS_MF_OP_MD_UI_STATE, 0, 1);
            }
            return;
        }
        let (tx, ty) = (tile[0] as i32 as f32, tile[1] as i32 as f32);
        let (start, end) = wall.get(|w| (w.start, w.end));
        let off = |p: EeScreensPoint| floor(p.x) as f32 != tx || floor(p.y) as f32 != ty;
        if off(start) && off(end) {
            if wall.get(|w| w.state) == 0 {
                h.a(EE_SCREENS_MF_OP_GHOST_LOCK, 1);
                h.preview_wall(&[[tile[0] as i32, tile[1] as i32]]);
                h.a(EE_SCREENS_MF_OP_GHOST_LOCK, 0);
            }
            if wall.get(|w| w.state) == 1 {
                wall.set(|w| w.end = centre(tile));
                let (start, end) = wall.get(|w| (w.start, w.end));
                let mut tiles = Vec::new();
                Ge2DPoint::new(start.x, start.y)
                    .interpolate_to(Ge2DPoint::new(end.x, end.y), 1.0, |p| tiles.push([p.x as i32, p.y as i32]));
                h.a(EE_SCREENS_MF_OP_GHOST_LOCK, 1);
                h.preview_wall(&tiles);
                h.a(EE_SCREENS_MF_OP_GHOST_LOCK, 0);
            }
        }
    }
}

/// `EEFindGateWall(tile, direction)`: found, the direction.
fn find_gate_wall(h: &mut impl FormHost, tile: [i64; 2], direction: i64) -> (bool, i64) {
    let mut io = EeScreensMainFormIo { n: [direction, 0], ..Default::default() };
    let found = h.op(EE_SCREENS_MF_OP_FIND_GATE_WALL, tile[0], tile[1], &mut io) != 0;
    (found, io.n[0])
}

/// `EEUIBuildGate::Update` (0x25627d): a gate ghost on the wall under the mouse (a picked wall's tile, else the
/// pointed tile), built on a left release (then the mode ends), cancelled on a right release.
pub fn gate_update(h: &mut impl FormHost, gate: &Rec<EeScreensBuildGateFields>, x: f32, y: f32) {
    let m = Modes { x, y };
    let picked = m.pick(h);
    let tile = if picked && m.unit(h, EE_SCREENS_MF_UNIT_IS_WALL) != 0 {
        [m.unit(h, EE_SCREENS_MF_UNIT_TILE_X), m.unit(h, EE_SCREENS_MF_UNIT_TILE_Y)]
    } else {
        m.point_xy(h).1
    };
    if picked {
        m.release(h);
    }
    let direction = gate.get(|g| g.direction) as i64;
    let b = m.buttons(h);
    if b & (EE_SCREENS_MF_BTN_L_PRESSED | EE_SCREENS_MF_BTN_L_HELD) != 0 {
        return;
    }
    if b & EE_SCREENS_MF_BTN_L_RELEASED != 0 {
        let (found, dir) = find_gate_wall(h, tile, direction);
        if found {
            gate.set(|g| {
                g.tile_x = tile[0] as i32;
                g.tile_y = tile[1] as i32;
            });
            let io = EeScreensMainFormIo {
                f: [tile[0] as i32 as f32, tile[1] as i32 as f32, 0.0, 0.0],
                ..Default::default()
            };
            h.io(EE_SCREENS_MF_OP_MD_BUILD_WALL, 1, dir, io);
        }
        h.a(EE_SCREENS_MF_OP_MD_GATE_CLEAR, 0);
        h.ab(EE_SCREENS_MF_OP_MD_UI_STATE, 0, 1);
    } else if b & (EE_SCREENS_MF_BTN_R_PRESSED | EE_SCREENS_MF_BTN_R_HELD) == 0 {
        if b & EE_SCREENS_MF_BTN_R_RELEASED != 0 {
            h.a(EE_SCREENS_MF_OP_MD_GATE_CLEAR, 0);
            h.ab(EE_SCREENS_MF_OP_MD_UI_STATE, 0, 1);
        } else if gate.get(|g| tile[0] as i32 == g.tile_x && tile[1] as i32 == g.tile_y) {
            // (the original stores the same tile back)
        } else {
            let (found, dir) = find_gate_wall(h, tile, direction);
            if found {
                let mut io = m.loc();
                io.n[0] = dir;
                h.op(EE_SCREENS_MF_OP_MD_GATE_PREVIEW, 0, 0, &mut io);
                gate.set(|g| g.direction = io.n[0] as i32);
            } else {
                m.clear_ghosts(h);
            }
        }
    }
}

#[cfg(test)]
#[path = "mouse_modes/tests.rs"]
mod tests;
