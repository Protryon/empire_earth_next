//! The game's actions whole in Rust: one C++ action class (`EEARust`, `ee/EEARust.h`) holds an
//! [`EeScreensGaFields`] record (its base: the members keep the original names, so the forms that set them compile
//! unchanged) and runs every kind here over one host (`ee_screenscpp_ga`, `Actions/EEARust.cpp`). The kinds are the
//! original `Actions` classes and the quick save/load of `Scenario Editor Actions`: the lobby's chat line
//! (`EEAAddChatMessage`), the cancelled game (`EEABootClientFromCanceledGame`), the build, gate and wall placements
//! (`EEABuild`), the calamity cast and its check (`EEACalamity`), the camera's centring (`EEACenterCamera`), the
//! multiplayer join or host (`EEAJoinOrHostAGame`), the box selection's message (`EEALasso`), the mini map's click
//! (`EEAMiniMap`), the right click's order and the plain click's selection (`EEAMouseClick`), the next player event
//! (`EEAPlayerEvent`), the server's boot (`EEARemovePlayer`), the editor's civilization reset
//! (`EEAResetAllPlayerInfo`), the saved game (`EEASaveGame`), the edge scroll (`EEAScroll`), the host migration
//! (`EEAServerMigration`), the improvement raise and lower (`EEAUpgradeImprovement`), the pause (`EEAUserPauseGame`)
//! and the quick save/load (`EEAQuickSaveLoad`, `EESEAQuickSaveLoad`).
//!
//! The C++ objects (the strings, the poll, the mini map's point and the scroll's edges) stay members of the C++
//! class, read by the host. Every world read and call is a host op, in the original's order; the decisions are here.
//! A host op never calls back into an action, so the record is read afresh around each op and never borrowed across
//! one.
#![allow(non_snake_case)]

use core::ffi::c_void;
use std::sync::atomic::{AtomicU32, Ordering::Relaxed};

use ee_ge::hull::trunc_long;
use ee_ge::types::{Ge3DLine, Ge3DPlane, Ge3DPoint, GeVector};
use ee_ge::um::{floor, um_arctan2};

// ---- the kinds (ee/EEARust.h's kEEA*) ----
pub const EE_SCREENS_GA_CHAT: u32 = 1;
pub const EE_SCREENS_GA_BOOT: u32 = 2;
pub const EE_SCREENS_GA_BUILD: u32 = 3;
pub const EE_SCREENS_GA_CALAMITY: u32 = 4;
pub const EE_SCREENS_GA_CENTER_CAMERA: u32 = 5;
pub const EE_SCREENS_GA_JOIN: u32 = 6;
pub const EE_SCREENS_GA_LASSO: u32 = 7;
pub const EE_SCREENS_GA_MINI_MAP: u32 = 8;
pub const EE_SCREENS_GA_MOUSE_CLICK: u32 = 9;
pub const EE_SCREENS_GA_PLAYER_EVENT: u32 = 10;
pub const EE_SCREENS_GA_REMOVE_PLAYER: u32 = 11;
pub const EE_SCREENS_GA_RESET_PLAYERS: u32 = 12;
pub const EE_SCREENS_GA_SAVE_GAME: u32 = 13;
pub const EE_SCREENS_GA_SCROLL: u32 = 14;
pub const EE_SCREENS_GA_MIGRATION: u32 = 15;
pub const EE_SCREENS_GA_UPGRADE: u32 = 16;
pub const EE_SCREENS_GA_PAUSE: u32 = 17;
pub const EE_SCREENS_GA_QUICK: u32 = 18;
pub const EE_SCREENS_GA_EDITOR_QUICK: u32 = 19;
// the scenario editor's (round 2: the classes of `Scenario Editor Actions`, run by `editor_actions.rs` and
// `editor_whole.rs` over their own hosts)
/// `EESEAMouseClick`: the editor's click (its message applied to the editor's selection; `Execute` a deselection).
pub const EE_SCREENS_GA_EDITOR_CLICK: u32 = 20;
/// `EESEALasso`: the editor's lasso (its message applied; `SelectInRectangle` a deselection).
pub const EE_SCREENS_GA_EDITOR_LASSO: u32 = 21;
/// `EESETSAMouseClick`: the editor's click, then the trigger tab's object screen refreshed.
pub const EE_SCREENS_GA_TRIGGER_CLICK: u32 = 22;
/// `EESETSALasso`: the editor's lasso, then the trigger tab's object screen refreshed.
pub const EE_SCREENS_GA_TRIGGER_LASSO: u32 = 23;
/// `EESEAChangeLoadText`: the editor form's load text set to `mText`.
pub const EE_SCREENS_GA_LOAD_TEXT: u32 = 24;
/// `EESEAPaintRust`: the paints (`mPaint`).
pub const EE_SCREENS_GA_EDITOR_PAINT: u32 = 25;
/// `EESEARust`: the editor's whole actions (`mAction`).
pub const EE_SCREENS_GA_EDITOR_WHOLE: u32 = 26;

/// An action's plain members (the C++ class's second base; the names are the original members').
#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct EeScreensGaFields {
    /// The kind (`EE_SCREENS_GA_*`).
    pub mKind: u32,
    /// The message built and not sent yet (`EEMessage*`: the click, lasso, build and calamity).
    pub mMessage: *mut c_void,
    /// The calamity's class (`EECalamityClass*`).
    pub mCalamityClass: *mut c_void,
    /// The quick save's memory file (`FSMemoryFile*`, the host's).
    pub mFile: *mut c_void,
    /// The editor paint's record (`EESEAPaintRust`'s: `editor_actions.rs`).
    pub mPaint: *mut crate::editor_actions::EeScreensSeaPaint,
    /// The editor's whole action (`EESEARust`'s: `editor_whole.rs`).
    pub mAction: *mut crate::editor_whole::EeScreensSewAction,
    /// The build's and the calamity's technology.
    pub mTechnology: i32,
    pub mTargetID: i32,
    pub mCasterID: i32,
    /// The calamity's target tile.
    pub mX: i32,
    pub mY: i32,
    /// The calamity's last check (nonzero: castable).
    pub mValid: i32,
    /// The camera's target (`GE3DPoint`) and the camera object followed (-1: the point).
    pub mLocation: [f32; 3],
    pub mUnitID: i32,
    /// The mini map's click: 0 centre the camera, 1 send the group; the key modifiers at the click.
    pub mMode: i32,
    pub mModifiers: u32,
    /// The edge scroll's last poll.
    pub mElapsed: u32,
    /// The cancelled game's message string (0: none).
    pub mStringID: i32,
    /// The improvement and its unit (by id: `EEUnitHandle`, referenced while the action lives).
    pub mImprovement: i32,
    pub mUnit: u32,
    /// The join's mode (0 host, 1 join a detected game, 2/3 connect to an address, 4 the WON host).
    pub mJoinMode: i32,
    /// The build's queue flag (`EEMBuildUnit`'s).
    pub mIsQueued: u8,
    pub mScrollLeft: bool,
    pub mScrollRight: bool,
    pub mScrollUp: bool,
    pub mScrollDown: bool,
    pub mAccelerate: bool,
    /// The booted player's slot and its flags.
    pub mPlayer: u8,
    pub mB0D: bool,
    pub mIsExiting: bool,
    /// The cancelled game keeps the current form.
    pub mNoPop: bool,
    /// The improvement raised (else lowered); in a game (else the scenario editor).
    pub mRaise: bool,
    pub mIsInGame: bool,
    /// The saved game's flag.
    pub mFlag: bool,
    /// The chat line is the local player's.
    pub mFromLocalPlayer: bool,
    /// The quick save/load: load (else save); a save also written to disk.
    pub mIsLoad: bool,
    pub mWriteToFile: bool,
}

impl EeScreensGaFields {
    /// The C++ constructor's value-initialised record (every member zero, `mKind` set).
    pub fn zeroed(kind: u32) -> Self {
        EeScreensGaFields {
            mKind: kind,
            mMessage: core::ptr::null_mut(),
            mCalamityClass: core::ptr::null_mut(),
            mFile: core::ptr::null_mut(),
            mPaint: core::ptr::null_mut(),
            mAction: core::ptr::null_mut(),
            mTechnology: 0,
            mTargetID: 0,
            mCasterID: 0,
            mX: 0,
            mY: 0,
            mValid: 0,
            mLocation: [0.0; 3],
            mUnitID: 0,
            mMode: 0,
            mModifiers: 0,
            mElapsed: 0,
            mStringID: 0,
            mImprovement: 0,
            mUnit: 0,
            mJoinMode: 0,
            mIsQueued: 0,
            mScrollLeft: false,
            mScrollRight: false,
            mScrollUp: false,
            mScrollDown: false,
            mAccelerate: false,
            mPlayer: 0,
            mB0D: false,
            mIsExiting: false,
            mNoPop: false,
            mRaise: false,
            mIsInGame: false,
            mFlag: false,
            mFromLocalPlayer: false,
            mIsLoad: false,
            mWriteToFile: false,
        }
    }
}

/// One member of a record message (`EEMRecordValue`): an integer, a float or a point.
#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct EeScreensGaValue {
    /// `EE_SCREENS_GA_VALUE_*`.
    pub kind: u32,
    pub i: i64,
    pub f: [f32; 2],
}

pub const EE_SCREENS_GA_VALUE_INT: u32 = 0;
pub const EE_SCREENS_GA_VALUE_FLOAT: u32 = 1;
pub const EE_SCREENS_GA_VALUE_POINT: u32 = 2;

fn vi(v: i64) -> EeScreensGaValue {
    EeScreensGaValue { kind: EE_SCREENS_GA_VALUE_INT, i: v, f: [0.0; 2] }
}
fn vb(v: bool) -> EeScreensGaValue {
    vi(v as i64)
}
fn vf(v: f32) -> EeScreensGaValue {
    EeScreensGaValue { kind: EE_SCREENS_GA_VALUE_FLOAT, i: 0, f: [v, 0.0] }
}
fn vp(x: f32, y: f32) -> EeScreensGaValue {
    EeScreensGaValue { kind: EE_SCREENS_GA_VALUE_POINT, i: 0, f: [x, y] }
}

// ---- the host's ops (EE_SCREENS_GA_OP_*): `p` an object, `i` and `f` in and out ----
/// `PostAction(this)`.
pub const EE_SCREENS_GA_OP_POST: u32 = 1;
/// `gCommunications->ProcessLocalUserInput(p, kSlotNone)`.
pub const EE_SCREENS_GA_OP_SEND: u32 = 2;
/// The message `p` dereferenced.
pub const EE_SCREENS_GA_OP_MESSAGE_RELEASE: u32 = 3;
/// A selection message (`EENewSelectUnits`) over the rectangle `p` (`GERectangle*`): i0 its type (0x4e select,
/// 0x4f add, 0x50 add and remove), i1 the unit type; answers the message.
pub const EE_SCREENS_GA_OP_SELECT_UNITS: u32 = 4;
/// `EEUnitClassCount()`.
pub const EE_SCREENS_GA_OP_CLASS_COUNT: u32 = 5;
/// The current player's saved group 0x15 reset (`mSavedGroups.Reset(0x15, 1)`).
pub const EE_SCREENS_GA_OP_SAVED_GROUP_RESET: u32 = 6;
/// The mouse's position into f0, f1.
pub const EE_SCREENS_GA_OP_MOUSE: u32 = 7;
/// Whether the mouse is frozen.
pub const EE_SCREENS_GA_OP_MOUSE_FROZEN: u32 = 8;
/// `gUnitPicker.PickUnit((f0, f1), i0)`: the unit (referenced) or 0.
pub const EE_SCREENS_GA_OP_PICK: u32 = 9;
/// The unit `p` referenced / dereferenced.
pub const EE_SCREENS_GA_OP_UNIT_REFERENCE: u32 = 10;
pub const EE_SCREENS_GA_OP_UNIT_RELEASE: u32 = 11;
/// A unit's member (i0 `EE_SCREENS_GA_U_*`; i1 set: `p` is an `EEComplexUnit*`, else an `EEUnit*`).
pub const EE_SCREENS_GA_OP_UNIT: u32 = 12;
/// A unit's class's member (`p` an `EEUnit*` or, with i1, an `EEComplexUnit*`; i0 `EE_SCREENS_GA_C_*`).
pub const EE_SCREENS_GA_OP_CLASS: u32 = 13;
/// The current player's selected group (i0 `EE_SCREENS_GA_G_*`, i1 and `p` the arguments).
pub const EE_SCREENS_GA_OP_GROUP: u32 = 14;
/// The current player (i0 `EE_SCREENS_GA_P_*`, i1 the argument).
pub const EE_SCREENS_GA_OP_PLAYER: u32 = 15;
/// `EEGetFirstSelectedUnit(i0)`: i0 in and out (an `EEComplexUnit*`).
pub const EE_SCREENS_GA_OP_FIRST_SELECTED: u32 = 16;
/// `EEPointUnitOrTileXY((f0, f1), i1, i2, i3, i0)`: the tile, x and y in and out.
pub const EE_SCREENS_GA_OP_POINT_TILE: u32 = 17;
/// A tile's (`p`, an `EETile*`) member (i0 `EE_SCREENS_GA_T_*`, i1 the index).
pub const EE_SCREENS_GA_OP_TILE: u32 = 18;
/// `p->CanGather(i0, tile x, tile y, false, false)` (`p` an `EEComplexUnit*`, i0 an `EEUnit*`).
pub const EE_SCREENS_GA_OP_CAN_GATHER: u32 = 19;
/// `EEGetFarmDropSites(p)` (an `EEComplexUnit*`): the count; i0 the first one's id.
pub const EE_SCREENS_GA_OP_FARM_DROP_SITES: u32 = 20;
/// `EECanHeal(p, i0)` (two `EEComplexUnit*`).
pub const EE_SCREENS_GA_OP_CAN_HEAL: u32 = 21;
/// The map (i0 `EE_SCREENS_GA_M_*`).
pub const EE_SCREENS_GA_OP_MAP: u32 = 22;
/// The click's confirmation effect: on `p` (or none) at (f0, f1, f2), graphic effect i0, the player i1's.
pub const EE_SCREENS_GA_OP_EFFECT: u32 = 23;
/// `gWorld.GetUnit(i0)`.
pub const EE_SCREENS_GA_OP_WORLD_UNIT: u32 = 24;
/// The technologies (i0 `EE_SCREENS_GA_K_*`).
pub const EE_SCREENS_GA_OP_TECH: u32 = 25;
/// The calamities (i0 `EE_SCREENS_GA_A_*`).
pub const EE_SCREENS_GA_OP_CALAMITY: u32 = 26;
/// The camera (i0 `EE_SCREENS_GA_CAM_*`).
pub const EE_SCREENS_GA_OP_CAMERA: u32 = 27;
/// The mini map (i0 `EE_SCREENS_GA_MM_*`).
pub const EE_SCREENS_GA_OP_MINI_MAP: u32 = 28;
/// The edge scroll (i0 `EE_SCREENS_GA_S_*`).
pub const EE_SCREENS_GA_OP_SCROLL: u32 = 29;
/// The screens, the lobby and the network (i0 `EE_SCREENS_GA_N_*`, i1.. the arguments).
pub const EE_SCREENS_GA_OP_NET: u32 = 30;
/// The scenario editor and the saved games (i0 `EE_SCREENS_GA_E_*`, i1.. the arguments).
pub const EE_SCREENS_GA_OP_EDITOR: u32 = 31;
/// -> the action itself (its `EEAction*`: the editor's selection posts it).
pub const EE_SCREENS_GA_OP_ACTION: u32 = 32;

// a unit's members
pub const EE_SCREENS_GA_U_ID: i64 = 0;
pub const EE_SCREENS_GA_U_PLAYER_ID: i64 = 1;
pub const EE_SCREENS_GA_U_PLAYER: i64 = 2;
pub const EE_SCREENS_GA_U_COMPLEX: i64 = 3;
pub const EE_SCREENS_GA_U_TILE_NODE: i64 = 4;
pub const EE_SCREENS_GA_U_TILE_X: i64 = 5;
pub const EE_SCREENS_GA_U_TILE_Y: i64 = 6;
pub const EE_SCREENS_GA_U_HIT_POINTS: i64 = 7;
pub const EE_SCREENS_GA_U_MAX_HIT_POINTS: i64 = 8;
pub const EE_SCREENS_GA_U_RESOURCE_TYPE: i64 = 9;
pub const EE_SCREENS_GA_U_RESOURCE_AMOUNT: i64 = 10;
/// `EEHerd::IsAnimalOnMap` of its complex unit.
pub const EE_SCREENS_GA_U_ANIMAL_ON_MAP: i64 = 11;
/// Its complex unit's `GetIsDisabled`, `IsProtected`, `mIsBuilt`.
pub const EE_SCREENS_GA_U_DISABLED: i64 = 12;
pub const EE_SCREENS_GA_U_PROTECTED: i64 = 13;
pub const EE_SCREENS_GA_U_BUILT: i64 = 14;
/// `EEIsSeenBy(unit, the current player, true)`, `EEIsFootprintVisibleTo(unit, the current player)`.
pub const EE_SCREENS_GA_U_SEEN: i64 = 15;
pub const EE_SCREENS_GA_U_FOOTPRINT_VISIBLE: i64 = 16;
/// Its tile (`GetRTSTile`).
pub const EE_SCREENS_GA_U_TILE: i64 = 17;
/// Its complex unit's location's x and y (f0, f1).
pub const EE_SCREENS_GA_U_LOCATION: i64 = 18;
/// `GetClass()` (the pointer, compared).
pub const EE_SCREENS_GA_U_CLASS_PTR: i64 = 19;
/// A complex unit's container (`mContainer`).
pub const EE_SCREENS_GA_U_CONTAINER: i64 = 20;
/// Its tile's height (`mTile->mHeight`, f0).
pub const EE_SCREENS_GA_U_TILE_HEIGHT: i64 = 21;
/// The improvement's level of its stats (i1 the improvement).
pub const EE_SCREENS_GA_U_IMPROVEMENT_LEVEL: i64 = 22;
/// `EEUnitHandleID(unit)`.
pub const EE_SCREENS_GA_U_HANDLE: i64 = 23;
/// `EEUnitHandleResolve(i1)` (the unit or 0).
pub const EE_SCREENS_GA_U_RESOLVE: i64 = 24;
/// Its complex unit's location's `GetPosition` (f0, f1, f2).
pub const EE_SCREENS_GA_U_POSITION: i64 = 25;

// a unit class's members
pub const EE_SCREENS_GA_C_TYPE: i64 = 0;
pub const EE_SCREENS_GA_C_CATEGORY: i64 = 1;
pub const EE_SCREENS_GA_C_FAMILY: i64 = 2;
pub const EE_SCREENS_GA_C_ANIMAL_ID: i64 = 3;
pub const EE_SCREENS_GA_C_AIR_TRAFFIC: i64 = 4;
pub const EE_SCREENS_GA_C_CAN_RALLY: i64 = 5;
pub const EE_SCREENS_GA_C_TOWER: i64 = 6;
pub const EE_SCREENS_GA_C_TREE: i64 = 7;
pub const EE_SCREENS_GA_C_LAND_BUILDING: i64 = 8;
pub const EE_SCREENS_GA_C_DROP_SITE: i64 = 9;
pub const EE_SCREENS_GA_C_TRANSPORT: i64 = 10;
pub const EE_SCREENS_GA_C_AIR_BASE: i64 = 11;
pub const EE_SCREENS_GA_C_MEDIC: i64 = 12;
/// `EEUUGetIsUnitClassTargetable(NULL, class)`.
pub const EE_SCREENS_GA_C_TARGETABLE: i64 = 13;

// the selected group's (and the active group's) members
pub const EE_SCREENS_GA_G_COUNT: i64 = 0;
pub const EE_SCREENS_GA_G_KIND_FLAGS: i64 = 1;
pub const EE_SCREENS_GA_G_COUNT_KIND: i64 = 2;
pub const EE_SCREENS_GA_G_UNIT: i64 = 3;
pub const EE_SCREENS_GA_G_TYPE_COUNT: i64 = 4;
pub const EE_SCREENS_GA_G_FIND_CATEGORY: i64 = 5;
pub const EE_SCREENS_GA_G_FIND_FLAGS: i64 = 6;
pub const EE_SCREENS_GA_G_CONTAINS: i64 = 7;
/// `GetFlags().mFlags[kAITileForest]`.
pub const EE_SCREENS_GA_G_FOREST: i64 = 8;
/// The bounds' centre (f0, f1).
pub const EE_SCREENS_GA_G_CENTER: i64 = 9;
/// `GetFirstUnit()->GetUnitID()`.
pub const EE_SCREENS_GA_G_FIRST_ID: i64 = 10;
/// The active group's count and unit i1.
pub const EE_SCREENS_GA_G_ACTIVE_COUNT: i64 = 11;
pub const EE_SCREENS_GA_G_ACTIVE_UNIT: i64 = 12;

// the current player's
pub const EE_SCREENS_GA_P_PTR: i64 = 0;
pub const EE_SCREENS_GA_P_ID: i64 = 1;
pub const EE_SCREENS_GA_P_ENEMY: i64 = 2;
/// `gWorld.mPlayers[gWorld.mCurrentPlayer]->mPlayerID`.
pub const EE_SCREENS_GA_P_CURRENT_ID: i64 = 3;
/// `mEvents.GetNextEvent()`: whether there is one; its source in i0, i1.
pub const EE_SCREENS_GA_P_NEXT_EVENT: i64 = 4;

// a tile's
pub const EE_SCREENS_GA_T_DOPPEL_COUNT: i64 = 0;
pub const EE_SCREENS_GA_T_DOPPEL_PLAYER: i64 = 1;
pub const EE_SCREENS_GA_T_DOPPEL_UNIT: i64 = 2;
/// `IsVisibleTo(the current player)`.
pub const EE_SCREENS_GA_T_VISIBLE: i64 = 3;
/// The unit of its i1-th unit node (0 past the last).
pub const EE_SCREENS_GA_T_UNIT_AT: i64 = 4;

// the map's
/// `mTerrainMesh.GetIsValidTile((unsigned long)i1, (unsigned long)i2)`.
pub const EE_SCREENS_GA_M_VALID_TILE: i64 = 0;
/// `mTerrainMesh.GetMeshZ(f0, f1)` into f0.
pub const EE_SCREENS_GA_M_MESH_Z: i64 = 1;
/// `GetTile(i1, i2)` exists; its height into f0.
pub const EE_SCREENS_GA_M_TILE: i64 = 2;
/// `GetClosestTile((f0, f1, 0))` into i0, i1.
pub const EE_SCREENS_GA_M_CLOSEST: i64 = 3;
/// The mesh's left, right, front, back plane (i1 0..3) into f0..f5 (point, normal).
pub const EE_SCREENS_GA_M_PLANE: i64 = 4;
/// `gWorld.mMap` is set.
pub const EE_SCREENS_GA_M_EXISTS: i64 = 5;

// the technologies'
/// `CanProduce(i1, 0x20, 1, true, 3, NULL)` of the current player.
pub const EE_SCREENS_GA_K_CAN_PRODUCE: i64 = 0;
/// The tile size of technology i1's unit class (-1: no class).
pub const EE_SCREENS_GA_K_TILE_SIZE: i64 = 1;
/// The current player's technology i1's node contains unit type i2.
pub const EE_SCREENS_GA_K_NODE_CONTAINS: i64 = 2;

// the calamities' (`p` the class)
/// `ScreenToWorld((f0, f1))` through a copy of the world's transformation, into f0..f2.
pub const EE_SCREENS_GA_A_WORLD_POINT: i64 = 0;
/// `GetDefaultTarget((f0, f1, f2), i1, i2, i3)` (the point written back).
pub const EE_SCREENS_GA_A_DEFAULT_TARGET: i64 = 1;
/// `EEOrderAtCursor((f0, f1), i1)`.
pub const EE_SCREENS_GA_A_ORDER_AT_CURSOR: i64 = 2;
/// `CanCastAt(i1, (f0, f1, f2), i2, i3, i4, i5)` (the point written back).
pub const EE_SCREENS_GA_A_CAN_CAST: i64 = 3;

// the camera's
/// `SetLocation((f0, f1, f2), false, true)`.
pub const EE_SCREENS_GA_CAM_SET_LOCATION: i64 = 0;
/// `SetZoom(GetZoom())`.
pub const EE_SCREENS_GA_CAM_KEEP_ZOOM: i64 = 1;
pub const EE_SCREENS_GA_CAM_CHANGED: i64 = 2;
/// The camera object `p` (an `EEUnit*`) followed: zoom 0, mode 3, `MoveTo`, changed, `Update(0)`, mode 2.
pub const EE_SCREENS_GA_CAM_FOLLOW: i64 = 3;
/// `mZoom` into f0.
pub const EE_SCREENS_GA_CAM_ZOOM: i64 = 4;
/// `Scroll(f0, f1)`.
pub const EE_SCREENS_GA_CAM_SCROLL: i64 = 5;
/// `gCameraFollowing = false`.
pub const EE_SCREENS_GA_CAM_FOLLOWING_OFF: i64 = 6;

// the mini map's
/// `gMiniMapTransformation.Apply(mPoint)` into f0, f1.
pub const EE_SCREENS_GA_MM_APPLY: i64 = 0;
pub const EE_SCREENS_GA_MM_REFRESH: i64 = 1;

// the edge scroll's
/// The side scroll speed setting into f0.
pub const EE_SCREENS_GA_S_SPEED: i64 = 0;
/// `mEdges`' left, top, right, bottom into f0..f3.
pub const EE_SCREENS_GA_S_EDGES: i64 = 1;
pub const EE_SCREENS_GA_S_POLL_RUNNING: i64 = 2;
pub const EE_SCREENS_GA_S_POLL: i64 = 3;
pub const EE_SCREENS_GA_S_POLL_START: i64 = 4;
pub const EE_SCREENS_GA_S_POLL_STOP: i64 = 5;

// the screens', the lobby's and the network's
/// The lobby is the current form.
pub const EE_SCREENS_GA_N_LOBBY_CURRENT: i64 = 0;
/// The lobby's chat box: whether it has a scroll bar; its slider's maximum and current value into i0, i1.
pub const EE_SCREENS_GA_N_CHAT_SCROLL: i64 = 1;
/// `mText` appended to the chat box.
pub const EE_SCREENS_GA_N_CHAT_APPEND: i64 = 2;
/// The chat box scrolled to its slider's maximum, when it has a scroll bar.
pub const EE_SCREENS_GA_N_CHAT_TO_END: i64 = 3;
/// `gWonNet` (if any): the game list cleared, the room rejoined.
pub const EE_SCREENS_GA_N_WON_REJOIN: i64 = 4;
/// `gUIManager->Pop()`.
pub const EE_SCREENS_GA_N_POP: i64 = 5;
/// The string i1 shown in a message box (i2 set: closing it restarts the client).
pub const EE_SCREENS_GA_N_SHOW_STRING: i64 = 6;
pub const EE_SCREENS_GA_N_IS_SERVER: i64 = 7;
/// The server boots `mPlayer` (`mB0D`, `mIsExiting`).
pub const EE_SCREENS_GA_N_BOOT_PLAYER: i64 = 8;
pub const EE_SCREENS_GA_N_CTS_PROCESSED: i64 = 9;
pub const EE_SCREENS_GA_N_USER_PAUSED: i64 = 10;
/// The server paused (i1 set) or unpaused, the UI told, `mUserPaused` set to it.
pub const EE_SCREENS_GA_N_USER_PAUSE: i64 = 11;
/// `gWonNet`'s firewall flag.
pub const EE_SCREENS_GA_N_FIREWALL: i64 = 12;
/// `EEIsValidName(mPlayerName)` (i1 0) or `EEIsValidName(mGameName)`.
pub const EE_SCREENS_GA_N_VALID_NAME: i64 = 13;
/// `gCommunications` is set.
pub const EE_SCREENS_GA_N_COMMS: i64 = 14;
/// `gCommunications` deleted (if any).
pub const EE_SCREENS_GA_N_DELETE_COMMS: i64 = 15;
/// A new client started (i1 set: through the `dynamic_cast`, as after a server).
pub const EE_SCREENS_GA_N_NEW_CLIENT: i64 = 16;
/// A new server for `mGameName` started (i1: published to WON).
pub const EE_SCREENS_GA_N_NEW_SERVER: i64 = 17;
pub const EE_SCREENS_GA_N_PUBLISHED: i64 = 18;
/// `gPlayerName` = `mPlayerName`, the host game name setting = `mGameName`.
pub const EE_SCREENS_GA_N_REMEMBER_HOST: i64 = 19;
/// The game options reset from the last ones, multiplayer, no cheats (under their lock).
pub const EE_SCREENS_GA_N_OPTIONS: i64 = 20;
/// `SearchForGame(false)`, then `Connect(mGameName, "EEService", "")`: its answer.
pub const EE_SCREENS_GA_N_CONNECT_ADDRESS: i64 = 21;
/// The connection request (i1 the firewall flag) made, started (i2 set) and given as local input.
pub const EE_SCREENS_GA_N_REQUEST: i64 = 22;
/// The lobby's open event: the client's (i1 set) or the server's.
pub const EE_SCREENS_GA_N_LOBBY_EVENT: i64 = 23;
/// `gPlayerName` = `mPlayerName` (i1 0) or the Sierra user id setting.
pub const EE_SCREENS_GA_N_SET_NAME: i64 = 24;
pub const EE_SCREENS_GA_N_DETECTED_COUNT: i64 = 25;
/// `GetDetectedGame(i1, ...)`'s answer; the address, service and name kept.
pub const EE_SCREENS_GA_N_DETECTED: i64 = 26;
/// The kept service is "EEService" and the kept name `mGameName`.
pub const EE_SCREENS_GA_N_DETECTED_MATCH: i64 = 27;
/// `Connect` to the kept game: its answer.
pub const EE_SCREENS_GA_N_CONNECT_DETECTED: i64 = 28;
/// The migration: `gPhysics->RunOwedTicks()`, the client migrating.
pub const EE_SCREENS_GA_N_MIGRATE_BEGIN: i64 = 29;
pub const EE_SCREENS_GA_N_HAS_LEFT: i64 = 30;
pub const EE_SCREENS_GA_N_NUM_PLAYERS: i64 = 31;
/// Player i1's slot, firewall flag, human flag, is the current player (i0..i3).
pub const EE_SCREENS_GA_N_PLAYER: i64 = 32;
pub const EE_SCREENS_GA_N_LOADING_SAVED: i64 = 33;
/// Every player's active group cleared, the current player's selection copied into its active group, the fast
/// selection reset.
pub const EE_SCREENS_GA_N_CLEAR_SELECTIONS: i64 = 34;
/// Player i1's slot set to i2.
pub const EE_SCREENS_GA_N_SET_SLOT: i64 = 35;
/// Player i1's name kept as the dropped host's (i2 0) or the new host's (its address too).
pub const EE_SCREENS_GA_N_KEEP_NAME: i64 = 36;
/// Promoted: the chat lines, the new server (the ticks kept), its slots and selection id.
pub const EE_SCREENS_GA_N_PROMOTE: i64 = 37;
/// The new server's wait: i1 other players (0: timed out at once).
pub const EE_SCREENS_GA_N_RECONNECT_WAIT: i64 = 38;
/// Not promoted: the sleep, the chat lines, `SearchForGame(false)`, `Connect` to the new host: its answer.
pub const EE_SCREENS_GA_N_RECONNECT: i64 = 39;
/// The reconnection request sent (the tick, the player).
pub const EE_SCREENS_GA_N_RECONNECT_REQUEST: i64 = 40;
/// `gLagStatus = 0`.
pub const EE_SCREENS_GA_N_LAG_CLEAR: i64 = 41;

// the scenario editor's and the saved games'
/// `gUserInterface->UpdateJITManufacturing()`.
pub const EE_SCREENS_GA_E_JIT: i64 = 0;
/// The improvement applied to the unit `p` (an `EEUnit*`; i1 set: lowered), its unit tab and help refreshed.
pub const EE_SCREENS_GA_E_APPLY_IMPROVEMENT: i64 = 1;
/// The button sound i1 (1 or 2).
pub const EE_SCREENS_GA_E_SOUND: i64 = 2;
/// Player list entry i1's civilization file applied, its start epoch posted (406).
pub const EE_SCREENS_GA_E_RESET_APPLY: i64 = 3;
pub const EE_SCREENS_GA_E_RESET_COUNT: i64 = 4;
/// The load thread's 403 and 407 posts for entry i1.
pub const EE_SCREENS_GA_E_RESET_POST403: i64 = 5;
pub const EE_SCREENS_GA_E_RESET_POST407: i64 = 6;
/// `SaveGame(mFileName, now, mFlag, mDescription)`.
pub const EE_SCREENS_GA_E_SAVE: i64 = 7;
/// `gUserInterface->CloseForm(NULL)`.
pub const EE_SCREENS_GA_E_CLOSE_FORM: i64 = 8;
/// The load/save error message box.
pub const EE_SCREENS_GA_E_SAVE_ERROR: i64 = 9;
/// `gCommunications->GetIsGamePaused()`.
pub const EE_SCREENS_GA_E_PAUSED: i64 = 10;
/// The master and music volumes into f0, f1, both set to 0.
pub const EE_SCREENS_GA_E_MUTE: i64 = 11;
/// The server paused (i1 set) or unpaused and the UI told.
pub const EE_SCREENS_GA_E_SERVER_PAUSE: i64 = 12;
/// The quick load of `EEQuickSave<mName>` (its error box).
pub const EE_SCREENS_GA_E_QUICK_LOAD: i64 = 13;
/// The quick save into a new memory file (answered; its file name kept).
pub const EE_SCREENS_GA_E_QUICK_SAVE: i64 = 14;
/// The memory file `p` written to disk under the kept name.
pub const EE_SCREENS_GA_E_QUICK_WRITE: i64 = 15;
/// The volumes (f0, f1) restored, the physics' owed ticks cleared.
pub const EE_SCREENS_GA_E_UNMUTE: i64 = 16;
/// The editor's current tab resumed.
pub const EE_SCREENS_GA_E_RESUME_GROUP: i64 = 17;
/// The memory file `p` deleted.
pub const EE_SCREENS_GA_E_DELETE_FILE: i64 = 18;
/// The trigger tab's object screen's `UpdateSelection`.
pub const EE_SCREENS_GA_E_OBJECT_SELECTION: i64 = 19;
/// The editor form's `SetLoadText(mText)`.
pub const EE_SCREENS_GA_E_LOAD_TEXT: i64 = 20;

// ---- the original's constants (static_assert-ed in the host) ----
pub const EE_SCREENS_GA_MSG_SELECT_UNIT: u32 = 0x48;
pub const EE_SCREENS_GA_MSG_ADD_UNIT: u32 = 0x49;
pub const EE_SCREENS_GA_MSG_UNSELECT_ALL: u32 = 0x4b;
pub const EE_SCREENS_GA_MSG_ATTACK: u32 = 0x4c;
pub const EE_SCREENS_GA_MSG_PATH_TO: u32 = 0x4d;
pub const EE_SCREENS_GA_MSG_SELECT_UNITS: u32 = 0x4e;
pub const EE_SCREENS_GA_MSG_ADD_UNITS: u32 = 0x4f;
pub const EE_SCREENS_GA_MSG_ADD_REMOVE_UNITS: u32 = 0x50;
pub const EE_SCREENS_GA_MSG_GATHER: u32 = 0x51;
pub const EE_SCREENS_GA_MSG_BUILD_UNIT: u32 = 0x56;
pub const EE_SCREENS_GA_MSG_REPAIR: u32 = 0x57;
pub const EE_SCREENS_GA_MSG_BUILD_WALL: u32 = 0x59;
pub const EE_SCREENS_GA_MSG_DROPOFF: u32 = 0x61;
pub const EE_SCREENS_GA_MSG_RALLY: u32 = 0x66;
pub const EE_SCREENS_GA_MSG_LOAD_TRANSPORT: u32 = 0x67;
pub const EE_SCREENS_GA_MSG_PATROL_GROUP: u32 = 0x74;
pub const EE_SCREENS_GA_MSG_CAST_CALAMITY: u32 = 0x75;
pub const EE_SCREENS_GA_MSG_ALWAYS_ADD_UNIT: u32 = 0x76;
pub const EE_SCREENS_GA_MSG_IMPROVE: u32 = 0x77;
pub const EE_SCREENS_GA_MSG_BUILD_GATE: u32 = 0x86;
pub const EE_SCREENS_GA_MSG_REMOVE_IMPROVEMENT: u32 = 0x8b;
pub const EE_SCREENS_GA_MSG_SMART_FARMERS: u32 = 0x8f;
pub const EE_SCREENS_GA_MSG_RETURN_TO_BASE: u32 = 0x90;
pub const EE_SCREENS_GA_MSG_HEAL: u32 = 0x92;

pub const EE_SCREENS_GA_TYPE_TOWN_CENTER: i64 = 0x23;
pub const EE_SCREENS_GA_TYPE_DOCK: i64 = 0x24;
pub const EE_SCREENS_GA_TYPE_CAPITOL: i64 = 0x29;
pub const EE_SCREENS_GA_TYPE_FARM: i64 = 0x2a;
pub const EE_SCREENS_GA_TYPE_SETTLEMENT: i64 = 0x2b;
pub const EE_SCREENS_GA_TYPE_GRANARY: i64 = 0x34;
pub const EE_SCREENS_GA_TYPE_NAVY_YARD: i64 = 0x41;
pub const EE_SCREENS_GA_TYPE_AIRPORT: i64 = 0x42;
pub const EE_SCREENS_GA_TYPE_CAMERA_OBJECT: i64 = 0x167;
pub const EE_SCREENS_GA_TYPE_PATH_POINT: i64 = 0xdf;
pub const EE_SCREENS_GA_TYPE_GRANARY_IMPROVED: i64 = 0x2ba;
pub const EE_SCREENS_GA_TYPE_CP_MISSILE_BASE: i64 = 0x2ed;
pub const EE_SCREENS_GA_CATEGORY_LAND_CITIZEN: i64 = 6;
pub const EE_SCREENS_GA_CATEGORY_NAVAL_BUILDING: i64 = 0x15;
pub const EE_SCREENS_GA_CATEGORY_NAVAL_CITIZEN: i64 = 0x16;
pub const EE_SCREENS_GA_CATEGORY_SPACE_BUILDING: i64 = 0x20;
pub const EE_SCREENS_GA_FAMILY_FISH: i64 = 0x12;
pub const EE_SCREENS_GA_KIND_CITIZEN: u32 = 1;
pub const EE_SCREENS_GA_KIND_BUILDING: u32 = 0x10;
pub const EE_SCREENS_GA_KIND_NAVAL_CITIZEN: u32 = 0x40;
pub const EE_SCREENS_GA_KIND_HELICOPTER_TRANSPORT: u32 = 0x80;
pub const EE_SCREENS_GA_KIND_LAND_TRANSPORT: u32 = 0x100;
pub const EE_SCREENS_GA_KIND_NAVAL_TRANSPORT: u32 = 0x200;
pub const EE_SCREENS_GA_KIND_FORTRESS: u32 = 0x4000;
pub const EE_SCREENS_GA_KIND_SPACE_TRANSPORT: u32 = 0x10000;
pub const EE_SCREENS_GA_EFFECT_CLICK_UNIT: i64 = 0xd3;
pub const EE_SCREENS_GA_EFFECT_CLICK_NO_UNIT: i64 = 0xd1;
pub const EE_SCREENS_GA_TECH_CONQUISTADORS_SPELL: i32 = 7020;
pub const EE_SCREENS_GA_TECH_NONE: i32 = 100000;
pub const EE_SCREENS_GA_SLOT_SERVER: i64 = 0xfe;
pub const EE_SCREENS_GA_SLOT_NONE: i64 = 0xff;
/// The animals' id (a hunted animal).
const ANIMAL_ID_HUNTED: i64 = 1000;
/// The key modifiers (`kModifierShift`, `kModifierControl`).
const SHIFT: u32 = 1;
const CONTROL: u32 = 2;

/// The host (`ee_screenscpp_ga`, `ee_screenscpp_ga_record`).
pub trait GaHost {
    /// One op on the action `r` (`EE_SCREENS_GA_OP_*`): `p` an object, `i` and `f` its arguments and answers.
    fn op(&mut self, op: u32, r: *mut EeScreensGaFields, p: usize, i: &mut [i64; 8], f: &mut [f32; 8]) -> i64;
    /// A record message (`new EEMRecord(kind, {values})`).
    fn record(&mut self, kind: u32, values: &[EeScreensGaValue]) -> usize;
    /// The editor kinds' calls into the editor's actions (`editor_actions.rs`, `editor_whole.rs`, over their own
    /// hosts): the answer of [`GaEditorCall::PaintSetArea`], else 0. (The round-1 golden reaches none.)
    fn editor_call(&mut self, call: GaEditorCall) -> i64 {
        let _ = call;
        0
    }
}

/// An editor kind's call into the editor's actions (`GaHost::editor_call`).
#[derive(Debug, Clone, Copy, PartialEq)]
pub enum GaEditorCall {
    /// `apply_click` of the message (an `EEMessage*`; released by it).
    Click(usize),
    /// `click_execute` for the action (an `EEAction*`), while rotating or not.
    ClickExecute {
        action: usize,
        rotating: bool,
    },
    /// `apply_lasso` of the message (an `EEMSelectUnits*`) over its rectangle.
    Lasso(usize),
    /// `lasso_in_rectangle` for the action (an `EEAction*`) over the rectangle (a `GERectangle*`).
    LassoInRectangle {
        action: usize,
        rectangle: usize,
    },
    /// The paint's run, its area (left, top, right, bottom: 1 when queued) and its release.
    PaintRun(*mut crate::editor_actions::EeScreensSeaPaint),
    PaintSetArea(*mut crate::editor_actions::EeScreensSeaPaint, [i32; 4]),
    PaintFree(*mut crate::editor_actions::EeScreensSeaPaint),
    /// The whole action's run and its release.
    WholeRun(*mut crate::editor_whole::EeScreensSewAction),
    WholeFree(*mut crate::editor_whole::EeScreensSewAction),
}

/// The editor's click kinds.
fn is_editor_click(kind: u32) -> bool {
    kind == EE_SCREENS_GA_EDITOR_CLICK || kind == EE_SCREENS_GA_TRIGGER_CLICK
}

/// The editor's lasso kinds.
fn is_editor_lasso(kind: u32) -> bool {
    kind == EE_SCREENS_GA_EDITOR_LASSO || kind == EE_SCREENS_GA_TRIGGER_LASSO
}

/// An action's record and the host.
pub struct Ga<'a, H: GaHost> {
    r: *mut EeScreensGaFields,
    h: &'a mut H,
}

impl<'a, H: GaHost> Ga<'a, H> {
    /// # Safety
    /// `r` is a live record (the C++ action's base) while the value is used.
    pub unsafe fn new(r: *mut EeScreensGaFields, h: &'a mut H) -> Self {
        Ga { r, h }
    }
    fn get(&self) -> EeScreensGaFields {
        // SAFETY: live (the constructor's contract); copied out, no borrow kept
        unsafe { *self.r }
    }
    fn set(&self, f: impl FnOnce(&mut EeScreensGaFields)) {
        // SAFETY: as get; the borrow ends here, before any op
        f(unsafe { &mut *self.r })
    }
    fn call(&mut self, op: u32, p: usize, args: &[i64], fl: &[f32]) -> (i64, [i64; 8], [f32; 8]) {
        let mut i = [0i64; 8];
        let mut f = [0f32; 8];
        i[..args.len()].copy_from_slice(args);
        f[..fl.len()].copy_from_slice(fl);
        let ret = self.h.op(op, self.r, p, &mut i, &mut f);
        (ret, i, f)
    }
    fn op(&mut self, op: u32, p: usize, args: &[i64]) -> i64 {
        self.call(op, p, args, &[]).0
    }
    fn record(&mut self, kind: u32, values: &[EeScreensGaValue]) -> usize {
        self.h.record(kind, values)
    }

    // ---- typed reads ----
    fn unit(&mut self, unit: usize, field: i64) -> i64 {
        self.op(EE_SCREENS_GA_OP_UNIT, unit, &[field, 0])
    }
    fn cplx(&mut self, unit: usize, field: i64) -> i64 {
        self.op(EE_SCREENS_GA_OP_UNIT, unit, &[field, 1])
    }
    fn class(&mut self, unit: usize, field: i64) -> i64 {
        self.op(EE_SCREENS_GA_OP_CLASS, unit, &[field, 0])
    }
    fn cplx_class(&mut self, unit: usize, field: i64) -> i64 {
        self.op(EE_SCREENS_GA_OP_CLASS, unit, &[field, 1])
    }
    fn group(&mut self, field: i64, arg: i64, p: usize) -> i64 {
        self.op(EE_SCREENS_GA_OP_GROUP, p, &[field, arg])
    }
    fn player(&mut self, field: i64, arg: i64) -> i64 {
        self.op(EE_SCREENS_GA_OP_PLAYER, 0, &[field, arg])
    }
    fn tile(&mut self, tile: usize, field: i64, arg: i64) -> i64 {
        self.op(EE_SCREENS_GA_OP_TILE, tile, &[field, arg])
    }
    fn net(&mut self, field: i64, args: &[i64]) -> i64 {
        let mut a = [field, 0, 0, 0];
        a[1..1 + args.len()].copy_from_slice(args);
        self.op(EE_SCREENS_GA_OP_NET, 0, &a)
    }
    fn editor(&mut self, field: i64, p: usize, arg: i64) -> i64 {
        self.op(EE_SCREENS_GA_OP_EDITOR, p, &[field, arg])
    }
    fn camera(&mut self, field: i64, p: usize, fl: &[f32]) -> (i64, [f32; 8]) {
        let (ret, _, f) = self.call(EE_SCREENS_GA_OP_CAMERA, p, &[field], fl);
        (ret, f)
    }
    fn scroll(&mut self, field: i64) -> (i64, [f32; 8]) {
        let (ret, _, f) = self.call(EE_SCREENS_GA_OP_SCROLL, 0, &[field], &[]);
        (ret, f)
    }
    fn release(&mut self, unit: usize) {
        self.op(EE_SCREENS_GA_OP_UNIT_RELEASE, unit, &[]);
    }
    fn reference(&mut self, unit: usize) {
        self.op(EE_SCREENS_GA_OP_UNIT_REFERENCE, unit, &[]);
    }
    fn post(&mut self) {
        self.op(EE_SCREENS_GA_OP_POST, 0, &[]);
    }
    fn send(&mut self, message: usize) {
        self.op(EE_SCREENS_GA_OP_SEND, message, &[]);
    }
    fn mouse(&mut self) -> (f32, f32) {
        let (_, _, f) = self.call(EE_SCREENS_GA_OP_MOUSE, 0, &[], &[]);
        (f[0], f[1])
    }
    fn pick(&mut self, point: (f32, f32), flag: bool) -> usize {
        self.call(EE_SCREENS_GA_OP_PICK, 0, &[flag as i64], &[point.0, point.1]).0 as usize
    }
    fn world_unit(&mut self, id: i64) -> usize {
        self.op(EE_SCREENS_GA_OP_WORLD_UNIT, 0, &[id]) as usize
    }
    /// `EEGetFirstSelectedUnit(ioUnit)`: whether there is one; `selected` in and out.
    fn first_selected(&mut self, selected: &mut usize) -> bool {
        let (ret, i, _) = self.call(EE_SCREENS_GA_OP_FIRST_SELECTED, 0, &[*selected as i64], &[]);
        *selected = i[0] as usize;
        ret != 0
    }
    /// `EEPointUnitOrTileXY`: the tile, x and y in and out.
    fn point_tile(&mut self, point: (f32, f32), tile: &mut usize, x: &mut i64, y: &mut i64, allow: bool) -> bool {
        let (ret, i, _) =
            self.call(EE_SCREENS_GA_OP_POINT_TILE, 0, &[allow as i64, *tile as i64, *x, *y], &[point.0, point.1]);
        *tile = i[1] as usize;
        *x = i[2];
        *y = i[3];
        ret != 0
    }
    fn map(&mut self, field: i64, args: &[i64], fl: &[f32]) -> (i64, [i64; 8], [f32; 8]) {
        let mut a = [field, 0, 0];
        a[1..1 + args.len()].copy_from_slice(args);
        self.call(EE_SCREENS_GA_OP_MAP, 0, &a, fl)
    }
    fn valid_tile(&mut self, x: f32, y: f32) -> bool {
        // (unsigned long)(long)x
        self.map(EE_SCREENS_GA_M_VALID_TILE, &[trunc_long(x) as i64, trunc_long(y) as i64], &[]).0 != 0
    }
    fn mesh_z(&mut self, x: f32, y: f32) -> f32 {
        self.map(EE_SCREENS_GA_M_MESH_Z, &[], &[x, y]).2[0]
    }
    /// The map's tile (x, y): its height, `None` off the map.
    fn map_tile(&mut self, x: i64, y: i64) -> Option<f32> {
        let (ret, _, f) = self.map(EE_SCREENS_GA_M_TILE, &[x, y], &[]);
        (ret != 0).then_some(f[0])
    }
    fn closest_tile(&mut self, x: f32, y: f32) -> (i64, i64) {
        let (_, i, _) = self.map(EE_SCREENS_GA_M_CLOSEST, &[], &[x, y]);
        (i[0], i[1])
    }
    fn mini_map_point(&mut self) -> (f32, f32) {
        let (_, _, f) = self.call(EE_SCREENS_GA_OP_MINI_MAP, 0, &[EE_SCREENS_GA_MM_APPLY], &[]);
        (f[0], f[1])
    }
    #[allow(clippy::too_many_arguments)]
    fn path_to(
        &mut self,
        facing: f32,
        x: f32,
        y: f32,
        control: bool,
        rotating: bool,
        shift: bool,
        force: bool,
    ) -> usize {
        self.record(
            EE_SCREENS_GA_MSG_PATH_TO,
            &[vf(facing), vf(x), vf(y), vb(!control), vb(rotating), vb(shift), vb(force)],
        )
    }
    fn camera_to(&mut self, p: [f32; 3]) {
        self.camera(EE_SCREENS_GA_CAM_SET_LOCATION, 0, &p);
    }

    // ---- the kinds' constructors, destructor and runs ----

    /// The constructor's members (after the record's value-initialisation).
    pub fn init(&mut self) {
        match self.get().mKind {
            EE_SCREENS_GA_CALAMITY => self.set(|r| {
                r.mTechnology = EE_SCREENS_GA_TECH_NONE;
                r.mValid = 1;
            }),
            // (EEUIFMainGame.cpp's out-of-line constructor 0x666572: no technology chosen)
            EE_SCREENS_GA_BUILD => self.set(|r| r.mTechnology = EE_SCREENS_GA_TECH_NONE),
            EE_SCREENS_GA_SCROLL => self.set(|r| r.mAccelerate = true),
            _ => {}
        }
    }

    /// The destructor: the message not sent released, the improvement's unit released, the quick save's file deleted.
    pub fn free(&mut self) {
        let r = self.get();
        if !r.mMessage.is_null() {
            self.op(EE_SCREENS_GA_OP_MESSAGE_RELEASE, r.mMessage as usize, &[]);
        }
        if r.mKind == EE_SCREENS_GA_UPGRADE {
            let unit = self.upgrade_unit();
            self.release(unit);
        }
        if !r.mFile.is_null() {
            self.editor(EE_SCREENS_GA_E_DELETE_FILE, r.mFile as usize, 0);
            self.set(|r| r.mFile = core::ptr::null_mut());
        }
        if !r.mPaint.is_null() {
            self.h.editor_call(GaEditorCall::PaintFree(r.mPaint));
            self.set(|r| r.mPaint = core::ptr::null_mut());
        }
        if !r.mAction.is_null() {
            self.h.editor_call(GaEditorCall::WholeFree(r.mAction));
            self.set(|r| r.mAction = core::ptr::null_mut());
        }
    }

    /// `Virtual4`: the kind's run.
    pub fn run(&mut self) {
        match self.get().mKind {
            EE_SCREENS_GA_CHAT => self.chat(),
            EE_SCREENS_GA_BOOT => self.boot(),
            EE_SCREENS_GA_BUILD | EE_SCREENS_GA_LASSO | EE_SCREENS_GA_CALAMITY => {
                let message = self.get().mMessage;
                if !message.is_null() {
                    self.send(message as usize);
                    self.set(|r| r.mMessage = core::ptr::null_mut());
                }
            }
            EE_SCREENS_GA_MOUSE_CLICK => {
                let message = self.get().mMessage;
                if message.is_null() {
                    return;
                }
                self.op(EE_SCREENS_GA_OP_SAVED_GROUP_RESET, 0, &[]);
                self.send(message as usize);
                self.set(|r| r.mMessage = core::ptr::null_mut());
            }
            EE_SCREENS_GA_CENTER_CAMERA => self.center_camera(),
            EE_SCREENS_GA_JOIN => self.join(),
            EE_SCREENS_GA_MINI_MAP => match self.get().mMode {
                0 => self.mini_map_center(),
                1 => self.mini_map_send(),
                _ => {}
            },
            EE_SCREENS_GA_PLAYER_EVENT => self.player_event(),
            EE_SCREENS_GA_REMOVE_PLAYER => {
                if self.net(EE_SCREENS_GA_N_IS_SERVER, &[]) != 0 {
                    self.net(EE_SCREENS_GA_N_BOOT_PLAYER, &[]);
                }
            }
            EE_SCREENS_GA_RESET_PLAYERS => self.reset_players(),
            EE_SCREENS_GA_SAVE_GAME => {
                if self.editor(EE_SCREENS_GA_E_SAVE, 0, 0) == 0 {
                    self.editor(EE_SCREENS_GA_E_CLOSE_FORM, 0, 0);
                } else {
                    self.editor(EE_SCREENS_GA_E_SAVE_ERROR, 0, 0);
                }
            }
            EE_SCREENS_GA_SCROLL => self.scroll_run(),
            EE_SCREENS_GA_MIGRATION => self.migrate(),
            EE_SCREENS_GA_UPGRADE => self.upgrade(),
            EE_SCREENS_GA_PAUSE => self.pause(),
            EE_SCREENS_GA_QUICK | EE_SCREENS_GA_EDITOR_QUICK => self.quick(),
            kind @ (EE_SCREENS_GA_EDITOR_CLICK
            | EE_SCREENS_GA_TRIGGER_CLICK
            | EE_SCREENS_GA_EDITOR_LASSO
            | EE_SCREENS_GA_TRIGGER_LASSO) => self.editor_selection(kind),
            EE_SCREENS_GA_LOAD_TEXT => {
                self.editor(EE_SCREENS_GA_E_LOAD_TEXT, 0, 0);
            }
            EE_SCREENS_GA_EDITOR_PAINT => {
                let paint = self.get().mPaint;
                if !paint.is_null() {
                    self.h.editor_call(GaEditorCall::PaintRun(paint));
                }
            }
            EE_SCREENS_GA_EDITOR_WHOLE => {
                let action = self.get().mAction;
                if !action.is_null() {
                    self.h.editor_call(GaEditorCall::WholeRun(action));
                }
            }
            _ => {}
        }
    }

    /// The editor's click and lasso (`EESEAMouseClick`, `EESEALasso` and the trigger tab's `EESETSA*`): the queued
    /// message applied to the editor's selection and dropped; the trigger tab's then refresh its object screen (with
    /// or without a message).
    fn editor_selection(&mut self, kind: u32) {
        let message = self.get().mMessage;
        if !message.is_null() {
            let message = message as usize;
            let call = if is_editor_click(kind) { GaEditorCall::Click(message) } else { GaEditorCall::Lasso(message) };
            self.h.editor_call(call);
            self.set(|r| r.mMessage = core::ptr::null_mut());
        }
        if kind == EE_SCREENS_GA_TRIGGER_CLICK || kind == EE_SCREENS_GA_TRIGGER_LASSO {
            self.editor(EE_SCREENS_GA_E_OBJECT_SELECTION, 0, 0);
        }
    }

    /// The lasso's `SelectInRectangle` (a `GERectangle*`): the editor's deselects the selected units in it and posts
    /// itself; the game's does nothing.
    pub fn select_in_rectangle(&mut self, rectangle: usize) {
        if is_editor_lasso(self.get().mKind) {
            let action = self.op(EE_SCREENS_GA_OP_ACTION, 0, &[]) as usize;
            self.h.editor_call(GaEditorCall::LassoInRectangle { action, rectangle });
        }
    }

    /// The paint's `SetArea`: whether it queued the area (no paint record: no).
    pub fn paint_set_area(&mut self, area: [i32; 4]) -> bool {
        let paint = self.get().mPaint;
        !paint.is_null() && self.h.editor_call(GaEditorCall::PaintSetArea(paint, area)) != 0
    }

    /// `EEAAddChatMessage`: the line appended to the lobby's chat box, scrolled to the end when it was there already
    /// or the line is ours.
    fn chat(&mut self) {
        if self.net(EE_SCREENS_GA_N_LOBBY_CURRENT, &[]) == 0 {
            return;
        }
        let (has_bar, i, _) = self.call(EE_SCREENS_GA_OP_NET, 0, &[EE_SCREENS_GA_N_CHAT_SCROLL], &[]);
        let at_end = !(!self.get().mFromLocalPlayer && has_bar != 0 && i[0] != i[1]);
        self.net(EE_SCREENS_GA_N_CHAT_APPEND, &[]);
        if at_end {
            self.net(EE_SCREENS_GA_N_CHAT_TO_END, &[]);
        }
    }

    /// `EEABootClientFromCanceledGame`: back to the WON room, the form left, the reason shown.
    fn boot(&mut self) {
        self.net(EE_SCREENS_GA_N_WON_REJOIN, &[]);
        let r = self.get();
        if !r.mNoPop {
            self.net(EE_SCREENS_GA_N_POP, &[]);
        }
        if r.mStringID != 0 {
            self.net(EE_SCREENS_GA_N_SHOW_STRING, &[r.mStringID as i64, 0]);
        }
    }

    /// `EEAUserPauseGame`: the server's user pause toggled once physics has processed a command turn.
    fn pause(&mut self) {
        if self.net(EE_SCREENS_GA_N_CTS_PROCESSED, &[]) <= 0 || self.net(EE_SCREENS_GA_N_IS_SERVER, &[]) == 0 {
            return;
        }
        let paused = self.net(EE_SCREENS_GA_N_USER_PAUSED, &[]) != 0;
        self.net(EE_SCREENS_GA_N_USER_PAUSE, &[!paused as i64]);
    }

    /// `EEAResetAllPlayerInfo`: each listed player's civilization reloaded and its epoch posted, then the refresh
    /// requests, then the epochs again.
    fn reset_players(&mut self) {
        for which in [EE_SCREENS_GA_E_RESET_APPLY, EE_SCREENS_GA_E_RESET_POST403, EE_SCREENS_GA_E_RESET_POST407] {
            let mut k = 0;
            while k < self.editor(EE_SCREENS_GA_E_RESET_COUNT, 0, 0) {
                self.editor(which, 0, k);
                k += 1;
            }
        }
    }

    // ---- the mouse click (EEAMouseClick) ----

    /// `SelectAtMouse`: select, add or toggle the unit under the mouse, or unselect all on a plain click on nothing.
    pub fn select_at_mouse(&mut self, modifiers: u32) {
        let at = self.mouse();
        let unit = self.pick(at, false);
        let message;
        if unit != 0 {
            let id = self.unit(unit, EE_SCREENS_GA_U_ID);
            let kind = if modifiers & SHIFT != 0 {
                EE_SCREENS_GA_MSG_ALWAYS_ADD_UNIT
            } else if modifiers & CONTROL != 0 {
                EE_SCREENS_GA_MSG_ADD_UNIT
            } else {
                EE_SCREENS_GA_MSG_SELECT_UNIT
            };
            message = self.record(kind, &[vi(id)]);
            self.release(unit);
        } else {
            if modifiers != 0 {
                return;
            }
            message = self.record(EE_SCREENS_GA_MSG_UNSELECT_ALL, &[]);
        }
        if message != 0 {
            self.set(|r| r.mMessage = message as *mut c_void);
            self.post();
        }
    }

    /// The target under the mouse as the player sees it: a unit it cannot see now is replaced by the player's last
    /// doppelganger on its tile (kept when it is the unit's own), dropped when there is none or its footprint is
    /// hidden. The tile the unit stands on is kept in `tile` when its footprint is visible.
    fn seen_target(&mut self, mut target: usize, player: usize, tile: &mut usize) -> usize {
        if target == 0 {
            return 0;
        }
        let hidden_animal = self.unit(target, EE_SCREENS_GA_U_PLAYER_ID) == 0
            && self.unit(target, EE_SCREENS_GA_U_COMPLEX) != 0
            && self.unit(target, EE_SCREENS_GA_U_ANIMAL_ON_MAP) != 0;
        if hidden_animal || self.unit(target, EE_SCREENS_GA_U_SEEN) != 0 {
            return target;
        }
        if self.unit(target, EE_SCREENS_GA_U_FOOTPRINT_VISIBLE) != 0 && {
            *tile = self.unit(target, EE_SCREENS_GA_U_TILE) as usize;
            *tile != 0
        } {
            let mut last: Option<usize> = None;
            let mut found = false;
            let count = self.tile(*tile, EE_SCREENS_GA_T_DOPPEL_COUNT, 0);
            for j in 0..count {
                if self.tile(*tile, EE_SCREENS_GA_T_DOPPEL_PLAYER, j) as usize != player {
                    continue;
                }
                let unit = self.tile(*tile, EE_SCREENS_GA_T_DOPPEL_UNIT, j) as usize;
                last = Some(unit);
                if unit == target {
                    found = true;
                    break;
                }
            }
            if !found {
                self.release(target);
                target = match last {
                    Some(unit) => {
                        if unit != 0 {
                            self.reference(unit);
                        }
                        unit
                    }
                    None => 0,
                };
            }
        } else {
            self.release(target);
            target = 0;
        }
        target
    }

    /// The unit's location for a move onto it: its complex unit's, else its tile's centre.
    fn unit_spot(&mut self, unit: usize) -> (f32, f32) {
        if self.unit(unit, EE_SCREENS_GA_U_COMPLEX) != 0 {
            let (_, _, f) = self.call(EE_SCREENS_GA_OP_UNIT, unit, &[EE_SCREENS_GA_U_LOCATION, 0], &[]);
            (f[0], f[1])
        } else {
            self.tile_center(unit)
        }
    }
    fn tile_center(&mut self, unit: usize) -> (f32, f32) {
        let x = self.unit(unit, EE_SCREENS_GA_U_TILE_X) as f32 + 0.5;
        let y = self.unit(unit, EE_SCREENS_GA_U_TILE_Y) as f32 + 0.5;
        (x, y)
    }

    /// `CreateLoadTransportMessage`: the group's transport of the clicked kind loads the group, or into the clicked
    /// transport of its class; 0 when nothing fits.
    fn load_transport(&mut self, kind: u32, target_id: u32) -> usize {
        let category = match kind {
            0x80 => 0x11,
            0x100 => 0x10,
            0x200 => 0x12,
            0x4000 => 0x1d,
            0x10000 => 0x21,
            _ => 0x23,
        };
        let transport = self.group(EE_SCREENS_GA_G_FIND_CATEGORY, category, 0) as usize;
        if transport == 0 {
            return 0;
        }
        let transport_id = self.cplx(transport, EE_SCREENS_GA_U_ID) as u32;
        if transport_id == target_id && self.cplx(transport, EE_SCREENS_GA_U_BUILT) != 0 {
            if self.group(EE_SCREENS_GA_G_COUNT, 0, 0) > 1 {
                return self.record(EE_SCREENS_GA_MSG_LOAD_TRANSPORT, &[vi(transport_id as i64), vi(-1), vi(0)]);
            }
            return 0;
        }
        let target = self.world_unit(target_id as i64);
        if target != 0
            && self.unit(target, EE_SCREENS_GA_U_CLASS_PTR) == self.cplx(transport, EE_SCREENS_GA_U_CLASS_PTR)
            && self.group(EE_SCREENS_GA_G_COUNT, 0, 0) > 1
            && self.unit(target, EE_SCREENS_GA_U_TILE_NODE) != 0
            && self.unit(target, EE_SCREENS_GA_U_BUILT) != 0
        {
            self.record(EE_SCREENS_GA_MSG_LOAD_TRANSPORT, &[vi(target_id as i64), vi(-1), vi(0)])
        } else if self.cplx(transport, EE_SCREENS_GA_U_BUILT) != 0 {
            self.record(EE_SCREENS_GA_MSG_LOAD_TRANSPORT, &[vi(transport_id as i64), vi(target_id as i64), vi(0)])
        } else {
            0
        }
    }

    /// `Execute`: the order for a right click at a world point for the selected group (a rally point, a move, an
    /// attack, a gather, a repair, a load...), the click's confirmation effect, and the action posted with it.
    #[allow(clippy::too_many_arguments)]
    pub fn click_execute(
        &mut self,
        x: f32,
        y: f32,
        mv: bool,
        modifiers: u32,
        rotating: bool,
        facing: f32,
        force: bool,
    ) {
        if is_editor_click(self.get().mKind) {
            // the editor's: a selected unit under the mouse toggled off, empty ground clears the selection
            let action = self.op(EE_SCREENS_GA_OP_ACTION, 0, &[]) as usize;
            self.h.editor_call(GaEditorCall::ClickExecute { action, rotating });
            return;
        }
        let player = self.player(EE_SCREENS_GA_P_PTR, 0) as usize;
        let kind_flags = self.group(EE_SCREENS_GA_G_KIND_FLAGS, 0, 0) as u32;
        let count = self.group(EE_SCREENS_GA_G_COUNT, 0, 0);
        let shift = modifiers & SHIFT != 0;
        let control = modifiers & CONTROL != 0;
        let citizens_flag = kind_flags & EE_SCREENS_GA_KIND_CITIZEN != 0;
        let point = self.mouse();
        let mut selected: usize = 0;
        let mut tile: usize = 0;
        let mut message: usize = 0;
        let mut target: usize = 0;
        let mut tile_x: i64 = 0;
        let mut tile_y: i64 = 0;
        let mut move_to = false;
        if count <= 0 {
            return;
        }
        if !force {
            if self.group(EE_SCREENS_GA_G_COUNT_KIND, 2, 0) == count && kind_flags & EE_SCREENS_GA_KIND_BUILDING != 0 {
                // buildings: a rally point (onto the target as seen), town halls picking citizens too
                let mut town_halls = true;
                let mut k = 0;
                while k < self.group(EE_SCREENS_GA_G_COUNT, 0, 0) && town_halls {
                    let unit = self.group(EE_SCREENS_GA_G_UNIT, k, 0) as usize;
                    let ty = if unit != 0 { self.cplx_class(unit, EE_SCREENS_GA_C_TYPE) } else { 0 };
                    town_halls = ty == EE_SCREENS_GA_TYPE_SETTLEMENT
                        || ty == EE_SCREENS_GA_TYPE_CAPITOL
                        || ty == EE_SCREENS_GA_TYPE_TOWN_CENTER;
                    k += 1;
                }
                target = self.pick(point, citizens_flag || town_halls);
                target = self.seen_target(target, player, &mut tile);
                let allow = self.group(EE_SCREENS_GA_G_TYPE_COUNT, EE_SCREENS_GA_TYPE_DOCK, 0) > 0;
                if self.point_tile(point, &mut tile, &mut tile_x, &mut tile_y, allow)
                    && self.first_selected(&mut selected)
                    && self.cplx_class(selected, EE_SCREENS_GA_C_CAN_RALLY) != 0
                {
                    let rally_unit = self.cplx(selected, EE_SCREENS_GA_U_COMPLEX) as usize;
                    let target_id = if target != 0 { self.unit(target, EE_SCREENS_GA_U_ID) } else { -1 };
                    let rally_id = self.cplx(rally_unit, EE_SCREENS_GA_U_ID);
                    let rally =
                        self.record(EE_SCREENS_GA_MSG_RALLY, &[vp(x, y), vi(target_id), vi(rally_id), vb(shift)]);
                    self.send(rally);
                }
            } else {
                let first = self.group(EE_SCREENS_GA_G_UNIT, 0, 0) as usize;
                let can_drop = first != 0
                    && (self.cplx_class(first, EE_SCREENS_GA_C_TOWER) != 0
                        || self.cplx_class(first, EE_SCREENS_GA_C_TYPE) == EE_SCREENS_GA_TYPE_CP_MISSILE_BASE);
                target = self.pick(point, citizens_flag);
                target = self.seen_target(target, player, &mut tile);
                if target == 0
                    && citizens_flag
                    && mv
                    && self.point_tile(point, &mut tile, &mut tile_x, &mut tile_y, false)
                    && tile != 0
                    && self.tile(tile, EE_SCREENS_GA_T_VISIBLE, 0) != 0
                {
                    // citizens moving onto a tile: its first tree
                    let mut k = 0;
                    loop {
                        let unit = self.tile(tile, EE_SCREENS_GA_T_UNIT_AT, k) as usize;
                        if unit == 0 {
                            break;
                        }
                        k += 1;
                        if self.class(unit, EE_SCREENS_GA_C_TREE) == 0 {
                            continue;
                        }
                        target = unit;
                        self.reference(target);
                        break;
                    }
                }
                if target != 0
                    && self.class(target, EE_SCREENS_GA_C_TREE) != 0
                    && !citizens_flag
                    && self.group(EE_SCREENS_GA_G_FOREST, 0, 0) != 0
                {
                    self.release(target);
                    target = 0;
                }
                if target != 0
                    && !(self.unit(target, EE_SCREENS_GA_U_TILE_NODE) != 0
                        && self.unit(target, EE_SCREENS_GA_U_DISABLED) != 0
                        && self.unit(target, EE_SCREENS_GA_U_PROTECTED) == 0)
                {
                    if self.class(target, EE_SCREENS_GA_C_TARGETABLE) != 0 {
                        let (m, to) = self.order_on(
                            target,
                            player,
                            x,
                            y,
                            mv,
                            shift,
                            control,
                            rotating,
                            facing,
                            count,
                            kind_flags,
                            can_drop,
                            &mut selected,
                        );
                        message = m;
                        move_to = to;
                    }
                } else if mv {
                    move_to = true;
                }
            }
            if move_to {
                message = self.path_to(facing, x, y, control, rotating, shift, false);
            }
        } else if mv {
            message = self.path_to(facing, x, y, control, rotating, shift, force);
        }
        let effect = if target != 0
            && (self.unit(target, EE_SCREENS_GA_U_HIT_POINTS) > 0
                || (self.unit(target, EE_SCREENS_GA_U_RESOURCE_AMOUNT) != 0
                    && self.class(target, EE_SCREENS_GA_C_CATEGORY) != EE_SCREENS_GA_CATEGORY_LAND_CITIZEN
                    && self.class(target, EE_SCREENS_GA_C_CATEGORY) != EE_SCREENS_GA_CATEGORY_NAVAL_CITIZEN))
        {
            EE_SCREENS_GA_EFFECT_CLICK_UNIT
        } else {
            EE_SCREENS_GA_EFFECT_CLICK_NO_UNIT
        };
        let mut z = if self.valid_tile(x, y) { self.mesh_z(x, y) } else { 0.0 };
        if z < 0.0 {
            z = 0.0;
        }
        let player_id = self.player(EE_SCREENS_GA_P_ID, 0);
        self.call(EE_SCREENS_GA_OP_EFFECT, target, &[effect, player_id], &[x, y, z]);
        if target != 0 {
            self.release(target);
        }
        if message != 0 {
            self.set(|r| r.mMessage = message as *mut c_void);
            self.post();
        }
    }

    /// The order onto a targetable unit (`Execute`'s middle): the message, and whether it is a plain move instead.
    #[allow(clippy::too_many_arguments)]
    fn order_on(
        &mut self,
        target: usize,
        player: usize,
        x: f32,
        y: f32,
        mv: bool,
        shift: bool,
        control: bool,
        rotating: bool,
        facing: f32,
        count: i64,
        kind_flags: u32,
        can_drop: bool,
        selected: &mut usize,
    ) -> (usize, bool) {
        let citizens_flag = kind_flags & EE_SCREENS_GA_KIND_CITIZEN != 0;
        let owner = self.unit(target, EE_SCREENS_GA_U_PLAYER) as usize;
        let target_id = self.unit(target, EE_SCREENS_GA_U_ID);
        let worker = self.group(EE_SCREENS_GA_G_FIND_FLAGS, EE_SCREENS_GA_KIND_CITIZEN as i64, 0) as usize;
        let naval_worker = self.group(EE_SCREENS_GA_G_FIND_FLAGS, EE_SCREENS_GA_KIND_NAVAL_CITIZEN as i64, 0) as usize;
        let gathers = (worker != 0 && self.op(EE_SCREENS_GA_OP_CAN_GATHER, worker, &[target as i64]) != 0)
            || (naval_worker != 0 && self.op(EE_SCREENS_GA_OP_CAN_GATHER, naval_worker, &[target as i64]) != 0);
        let damaged =
            |g: &mut Self| g.unit(target, EE_SCREENS_GA_U_HIT_POINTS) < g.unit(target, EE_SCREENS_GA_U_MAX_HIT_POINTS);
        if gathers {
            if owner != player {
                return (self.record(EE_SCREENS_GA_MSG_GATHER, &[vi(target_id), vb(shift)]), false);
            }
            if citizens_flag && self.class(target, EE_SCREENS_GA_C_LAND_BUILDING) != 0 && damaged(self) {
                return (self.record(EE_SCREENS_GA_MSG_REPAIR, &[vi(target_id), vb(shift)]), false);
            }
            if self.class(target, EE_SCREENS_GA_C_LAND_BUILDING) != 0 {
                let mut farmers = (0, 0);
                if self.group(EE_SCREENS_GA_G_COUNT, 0, 0) > 1
                    && self.class(target, EE_SCREENS_GA_C_TYPE) == EE_SCREENS_GA_TYPE_FARM
                {
                    let farm = self.unit(target, EE_SCREENS_GA_U_COMPLEX) as usize;
                    let (n, i, _) = self.call(EE_SCREENS_GA_OP_FARM_DROP_SITES, farm, &[], &[]);
                    farmers = (n, i[0]);
                }
                if farmers.0 == 1 {
                    return (self.record(EE_SCREENS_GA_MSG_SMART_FARMERS, &[vi(farmers.1), vb(shift)]), false);
                }
                return (self.record(EE_SCREENS_GA_MSG_GATHER, &[vi(target_id), vb(shift)]), false);
            }
            return (0, false);
        }
        let owner_id = self.unit(target, EE_SCREENS_GA_U_PLAYER_ID);
        if owner != player && self.player(EE_SCREENS_GA_P_ENEMY, owner_id) != 0 {
            let hunted = self.class(target, EE_SCREENS_GA_C_ANIMAL_ID) == ANIMAL_ID_HUNTED;
            if self.unit(target, EE_SCREENS_GA_U_RESOURCE_TYPE) != 0
                && owner_id == 0
                && self.unit(target, EE_SCREENS_GA_U_RESOURCE_AMOUNT) > 0
                && (!can_drop || self.class(target, EE_SCREENS_GA_C_ANIMAL_ID) == ANIMAL_ID_HUNTED)
            {
                if !citizens_flag && hunted {
                    if self.first_selected(selected) {
                        let ty = self.cplx_class(*selected, EE_SCREENS_GA_C_TYPE);
                        let category = self.cplx_class(*selected, EE_SCREENS_GA_C_CATEGORY);
                        if ty == EE_SCREENS_GA_TYPE_AIRPORT
                            || ((category == EE_SCREENS_GA_CATEGORY_NAVAL_BUILDING
                                || category == EE_SCREENS_GA_CATEGORY_SPACE_BUILDING)
                                && self.cplx_class(*selected, EE_SCREENS_GA_C_AIR_TRAFFIC) != 0
                                && self.class(target, EE_SCREENS_GA_C_FAMILY) != EE_SCREENS_GA_FAMILY_FISH)
                        {
                            let selected_id = self.cplx(*selected, EE_SCREENS_GA_U_ID);
                            let rally = [vp(x, y), vi(target_id), vi(selected_id), vb(shift)];
                            return (self.record(EE_SCREENS_GA_MSG_RALLY, &rally), false);
                        }
                        return (self.path_to(facing, x, y, control, rotating, shift, false), false);
                    }
                    return (0, false);
                }
                return (self.record(EE_SCREENS_GA_MSG_GATHER, &[vi(target_id), vb(shift)]), false);
            }
            if self.unit(target, EE_SCREENS_GA_U_HIT_POINTS) > 0 {
                return (self.record(EE_SCREENS_GA_MSG_ATTACK, &[vi(target_id), vb(shift), vb(false)]), false);
            }
            return (self.path_to(facing, x, y, control, rotating, shift, false), false);
        }
        if self.first_selected(selected)
            && *selected != 0
            && self.cplx_class(*selected, EE_SCREENS_GA_C_MEDIC) != 0
            && self.unit(target, EE_SCREENS_GA_U_HIT_POINTS) > 0
            && self.cplx(*selected, EE_SCREENS_GA_U_TILE_NODE) != 0
            && self.unit(target, EE_SCREENS_GA_U_TILE_NODE) != 0
            && {
                let healer = self.cplx(*selected, EE_SCREENS_GA_U_COMPLEX);
                let patient = self.unit(target, EE_SCREENS_GA_U_COMPLEX);
                self.op(EE_SCREENS_GA_OP_CAN_HEAL, healer as usize, &[patient]) != 0
            }
        {
            return (self.record(EE_SCREENS_GA_MSG_HEAL, &[vi(target_id)]), false);
        }
        if owner != player {
            let (lx, ly) = self.unit_spot(target);
            return (self.path_to(facing, lx, ly, control, rotating, shift, false), false);
        }
        // the player's own unit
        if citizens_flag && self.class(target, EE_SCREENS_GA_C_LAND_BUILDING) != 0 && damaged(self) {
            return (self.record(EE_SCREENS_GA_MSG_REPAIR, &[vi(target_id), vb(shift)]), false);
        }
        let ty = self.class(target, EE_SCREENS_GA_C_TYPE);
        if self.class(target, EE_SCREENS_GA_C_DROP_SITE) != 0
            || ((ty == EE_SCREENS_GA_TYPE_DOCK
                || self.class(target, EE_SCREENS_GA_C_TYPE) == EE_SCREENS_GA_TYPE_NAVY_YARD)
                && self.group(EE_SCREENS_GA_G_COUNT_KIND, 3, 0) < count)
        {
            let ty = self.class(target, EE_SCREENS_GA_C_TYPE);
            if ty != EE_SCREENS_GA_TYPE_GRANARY_IMPROVED
                && self.class(target, EE_SCREENS_GA_C_TYPE) != EE_SCREENS_GA_TYPE_GRANARY
            {
                return (self.record(EE_SCREENS_GA_MSG_DROPOFF, &[vi(target_id)]), false);
            }
            return (self.record(EE_SCREENS_GA_MSG_SMART_FARMERS, &[vi(target_id), vb(shift)]), false);
        }
        let all = |k: u32| kind_flags & k == k;
        let load = if all(EE_SCREENS_GA_KIND_HELICOPTER_TRANSPORT) {
            0x80
        } else if all(EE_SCREENS_GA_KIND_LAND_TRANSPORT) {
            0x100
        } else if all(EE_SCREENS_GA_KIND_NAVAL_TRANSPORT) {
            0x200
        } else if all(EE_SCREENS_GA_KIND_SPACE_TRANSPORT) {
            0x10000
        } else if all(EE_SCREENS_GA_KIND_FORTRESS) {
            0x4000
        } else {
            0
        };
        if load != 0 {
            return (self.load_transport(load, target_id as u32), false);
        }
        if self.class(target, EE_SCREENS_GA_C_TRANSPORT) != 0
            && self.unit(target, EE_SCREENS_GA_U_TILE_NODE) != 0
            && self.unit(target, EE_SCREENS_GA_U_BUILT) != 0
        {
            return (self.record(EE_SCREENS_GA_MSG_LOAD_TRANSPORT, &[vi(target_id), vi(-1), vi(0)]), false);
        }
        if self.class(target, EE_SCREENS_GA_C_TYPE) == EE_SCREENS_GA_TYPE_PATH_POINT {
            return (0, true);
        }
        if self.class(target, EE_SCREENS_GA_C_AIR_BASE) != 0 {
            return (self.record(EE_SCREENS_GA_MSG_RETURN_TO_BASE, &[vi(target_id), vb(shift)]), false);
        }
        let complex = self.unit(target, EE_SCREENS_GA_U_COMPLEX);
        if self.group(EE_SCREENS_GA_G_CONTAINS, 0, complex as usize) != 0 && mv {
            return (0, true);
        }
        let (mut lx, mut ly) = self.unit_spot(target);
        if !self.valid_tile(lx, ly) {
            (lx, ly) = self.tile_center(target);
        }
        (self.path_to(facing, lx, ly, control, rotating, shift, false), false)
    }

    // ---- the box selection (EEALasso) ----

    /// `SelectUnits`: the selection message for a lasso over `rect` (flags 0 select, Control add and remove, Shift
    /// add; a known unit type selects that type, the path point type the patrol group) and the action posted.
    pub fn lasso_select(&mut self, rect: usize, flags: i32, unit_type: u32, patrol_group: i32) {
        let known = (unit_type as i64) < self.op(EE_SCREENS_GA_OP_CLASS_COUNT, 0, &[]);
        let message = if known && unit_type as i64 == EE_SCREENS_GA_TYPE_PATH_POINT {
            self.record(EE_SCREENS_GA_MSG_PATROL_GROUP, &[vi(patrol_group as i64)])
        } else {
            let ty = if known { unit_type as i64 } else { 0 };
            let kind = if flags == 0 {
                EE_SCREENS_GA_MSG_SELECT_UNITS
            } else if flags as u32 & CONTROL != 0 {
                EE_SCREENS_GA_MSG_ADD_REMOVE_UNITS
            } else if flags as u32 & SHIFT != 0 {
                EE_SCREENS_GA_MSG_ADD_UNITS
            } else {
                EE_SCREENS_GA_MSG_SELECT_UNITS
            };
            self.op(EE_SCREENS_GA_OP_SELECT_UNITS, rect, &[kind as i64, ty]) as usize
        };
        self.set(|r| r.mMessage = message as *mut c_void);
        self.post();
    }

    // ---- the build (EEABuild) ----

    /// `Build`: the selected building placed at a point (centred for its tile size) and the action posted.
    pub fn build(&mut self, x: f32, y: f32) {
        let tech = self.get().mTechnology as i64;
        if self.op(EE_SCREENS_GA_OP_TECH, 0, &[EE_SCREENS_GA_K_CAN_PRODUCE, tech]) == 0 {
            return;
        }
        let size = self.op(EE_SCREENS_GA_OP_TECH, 0, &[EE_SCREENS_GA_K_TILE_SIZE, tech]);
        let mut offset = 0.0f32;
        if size > 1 {
            offset = if size & 1 != 0 { size as f32 * 0.5 - 0.5 } else { size as f32 * 0.5 };
        }
        let queued = self.get().mIsQueued as i64;
        let message = self.record(EE_SCREENS_GA_MSG_BUILD_UNIT, &[vp(x - offset, y - offset), vi(tech), vi(queued)]);
        self.set(|r| r.mMessage = message as *mut c_void);
        self.post();
    }

    /// `BuildGate`: a gate at a point and wall side, if the player can build it.
    pub fn build_gate(&mut self, x: f32, y: f32, side: i32) {
        let tech = self.get().mTechnology as i64;
        if self.op(EE_SCREENS_GA_OP_TECH, 0, &[EE_SCREENS_GA_K_CAN_PRODUCE, tech]) != 0 {
            let player = self.player(EE_SCREENS_GA_P_CURRENT_ID, 0);
            let values = [vp(x, y), vi(tech), vi(side as i64), vi(player)];
            let message = self.record(EE_SCREENS_GA_MSG_BUILD_GATE, &values);
            self.set(|r| r.mMessage = message as *mut c_void);
            self.post();
        }
    }

    /// `BuildWall`: a wall between two points, if the player can build it.
    pub fn build_wall(&mut self, x0: f32, y0: f32, x1: f32, y1: f32) {
        let tech = self.get().mTechnology as i64;
        if self.op(EE_SCREENS_GA_OP_TECH, 0, &[EE_SCREENS_GA_K_CAN_PRODUCE, tech]) != 0 {
            let player = self.player(EE_SCREENS_GA_P_CURRENT_ID, 0);
            let values = [vp(x0, y0), vp(x1, y1), vi(tech), vi(player)];
            let message = self.record(EE_SCREENS_GA_MSG_BUILD_WALL, &values);
            self.set(|r| r.mMessage = message as *mut c_void);
            self.post();
        }
    }

    // ---- the calamity (EEACalamity) ----

    /// The point under the screen point `at`: (on a unit, the world point); `mX`, `mY` the tile.
    fn calamity_point(&mut self, at: (f32, f32)) -> (bool, [f32; 3]) {
        let r = self.get();
        let (mut tile, mut x, mut y) = (0usize, r.mX as i64, r.mY as i64);
        let on_unit = self.point_tile(at, &mut tile, &mut x, &mut y, true);
        self.set(|r| {
            r.mX = x as i32;
            r.mY = y as i32;
        });
        let class = self.get().mCalamityClass as usize;
        let (_, _, f) = self.call(EE_SCREENS_GA_OP_CALAMITY, class, &[EE_SCREENS_GA_A_WORLD_POINT], &[at.0, at.1]);
        (on_unit, [f[0], f[1], f[2]])
    }
    /// `GetDefaultTarget`: the target's id (the point is the class's to change: written back).
    fn default_target(&mut self, point: &mut [f32; 3], on_unit: bool) -> i64 {
        let r = self.get();
        let args = [EE_SCREENS_GA_A_DEFAULT_TARGET, r.mX as i64, r.mY as i64, on_unit as i64];
        let (id, _, f) = self.call(EE_SCREENS_GA_OP_CALAMITY, r.mCalamityClass as usize, &args, &point[..]);
        point.copy_from_slice(&f[..3]);
        id
    }
    /// The unit's tile, else the record's.
    fn tile_or_record(&mut self, unit: usize) -> (i64, i64) {
        if unit != 0 {
            (self.unit(unit, EE_SCREENS_GA_U_TILE_X), self.unit(unit, EE_SCREENS_GA_U_TILE_Y))
        } else {
            let r = self.get();
            (r.mX as i64, r.mY as i64)
        }
    }

    /// `Cast`: the cast message at a screen point (the Conquistador spell at its caster), the order at the cursor
    /// shown, the action posted.
    pub fn cast(&mut self, at: (f32, f32)) {
        let tech = self.get().mTechnology;
        let unit;
        let (x, y);
        if tech == EE_SCREENS_GA_TECH_CONQUISTADORS_SPELL {
            unit = self.world_unit(self.get().mCasterID as i64);
            (x, y) = self.tile_or_record(unit);
        } else {
            let (on_unit, mut point) = self.calamity_point(at);
            let target = self.default_target(&mut point, on_unit);
            self.set(|r| r.mTargetID = target as i32);
            unit = self.world_unit(target);
            (x, y) = self.tile_or_record(unit);
        }
        let r = self.get();
        let values = [vi(r.mCasterID as i64), vi(r.mTargetID as i64), vi(x), vi(y), vi(tech as i64)];
        let message = self.record(EE_SCREENS_GA_MSG_CAST_CALAMITY, &values);
        self.set(|r| r.mMessage = message as *mut c_void);
        if tech != EE_SCREENS_GA_TECH_CONQUISTADORS_SPELL {
            self.call(EE_SCREENS_GA_OP_CALAMITY, 0, &[EE_SCREENS_GA_A_ORDER_AT_CURSOR, unit as i64], &[at.0, at.1]);
        }
        self.post();
    }

    /// `Check`: whether the active group's casters can cast at a screen point (`mValid`, answered); the target tile.
    pub fn check(&mut self, at: (f32, f32)) -> i32 {
        let caster = self.world_unit(self.get().mCasterID as i64);
        self.set(|r| r.mValid = 1);
        if caster != 0 && self.unit(caster, EE_SCREENS_GA_U_COMPLEX) != 0 {
            let (on_unit, mut point) = self.calamity_point(at);
            let id = self.default_target(&mut point, on_unit);
            let mut target = self.world_unit(id);
            if target == 0 {
                target = self.pick(at, false);
                if target != 0 {
                    self.release(target);
                }
            }
            let (x, y) = self.tile_or_record(target);
            let count = self.group(EE_SCREENS_GA_G_ACTIVE_COUNT, 0, 0);
            let tech = self.get().mTechnology as i64;
            let mut k = 0;
            while k < count && self.get().mValid != 0 {
                let unit = self.group(EE_SCREENS_GA_G_ACTIVE_UNIT, k, 0) as usize;
                let ty = self.cplx_class(unit, EE_SCREENS_GA_C_TYPE);
                if self.op(EE_SCREENS_GA_OP_TECH, 0, &[EE_SCREENS_GA_K_NODE_CONTAINS, tech, ty]) != 0 {
                    let class = self.get().mCalamityClass as usize;
                    let args = [EE_SCREENS_GA_A_CAN_CAST, unit as i64, x, y, on_unit as i64, target as i64];
                    // (the point is the class's to change: written back)
                    let (valid, _, f) = self.call(EE_SCREENS_GA_OP_CALAMITY, class, &args, &point);
                    point.copy_from_slice(&f[..3]);
                    self.set(|r| r.mValid = valid as i32);
                }
                k += 1;
            }
        }
        self.get().mValid
    }

    // ---- the camera (EEACenterCamera, EEAPlayerEvent) ----

    /// `EEACenterCamera::Virtual4`: the camera on the location, or following the camera object.
    fn center_camera(&mut self) {
        let r = self.get();
        if r.mUnitID == -1 {
            self.camera_to(r.mLocation);
            self.camera(EE_SCREENS_GA_CAM_KEEP_ZOOM, 0, &[]);
            self.camera(EE_SCREENS_GA_CAM_CHANGED, 0, &[]);
        } else {
            let unit = self.world_unit(r.mUnitID as i64);
            if unit != 0 && self.unit(unit, EE_SCREENS_GA_U_TILE_NODE) != 0 && {
                let complex = self.unit(unit, EE_SCREENS_GA_U_COMPLEX) as usize;
                self.cplx_class(complex, EE_SCREENS_GA_C_TYPE) == EE_SCREENS_GA_TYPE_CAMERA_OBJECT
            } {
                self.camera(EE_SCREENS_GA_CAM_FOLLOW, unit, &[]);
            }
        }
    }

    /// `SetLocation`: a map tile's centre as the target, the action posted (nothing off the map).
    pub fn set_location(&mut self, x: i32, y: i32) {
        let Some(height) = self.map_tile(x as i64, y as i64) else {
            return;
        };
        self.set(|r| {
            r.mUnitID = -1;
            r.mLocation = [x as f32 + 0.5, y as f32 + 0.5, height];
        });
        self.post();
    }

    /// `SetUnit`: a camera object followed by id, any other unit's outermost container's position, a unit without
    /// a complex unit its tile's centre; the action posted.
    pub fn set_unit(&mut self, unit: usize) {
        if unit == 0 {
            return;
        }
        let mut complex = self.unit(unit, EE_SCREENS_GA_U_COMPLEX) as usize;
        if complex != 0 {
            if self.cplx_class(complex, EE_SCREENS_GA_C_TYPE) == EE_SCREENS_GA_TYPE_CAMERA_OBJECT {
                let id = self.cplx(complex, EE_SCREENS_GA_U_ID);
                self.set(|r| r.mUnitID = id as i32);
            } else {
                self.set(|r| r.mUnitID = -1);
                loop {
                    let container = self.cplx(complex, EE_SCREENS_GA_U_CONTAINER) as usize;
                    if container == 0 {
                        break;
                    }
                    complex = container;
                }
                let (_, _, f) = self.call(EE_SCREENS_GA_OP_UNIT, complex, &[EE_SCREENS_GA_U_POSITION, 1], &[]);
                self.set(|r| r.mLocation = [f[0], f[1], f[2]]);
            }
        } else {
            let (x, y) = self.tile_center(unit);
            let (_, _, f) = self.call(EE_SCREENS_GA_OP_UNIT, unit, &[EE_SCREENS_GA_U_TILE_HEIGHT, 0], &[]);
            self.set(|r| r.mLocation = [x, y, f[0]]);
        }
        self.post();
    }

    /// `EEAPlayerEvent::Virtual4`: the camera to the next player event's source tile.
    fn player_event(&mut self) {
        let (has, i, _) = self.call(EE_SCREENS_GA_OP_PLAYER, 0, &[EE_SCREENS_GA_P_NEXT_EVENT], &[]);
        if has == 0 || i[0] == -10000 || i[1] == -10000 {
            return;
        }
        let x = i[0] as f32 + 0.5;
        let y = i[1] as f32 + 0.5;
        let z = self.mesh_z(x, y);
        self.camera(EE_SCREENS_GA_CAM_CHANGED, 0, &[]);
        self.camera_to([x, y, z]);
        self.call(EE_SCREENS_GA_OP_MINI_MAP, 0, &[EE_SCREENS_GA_MM_REFRESH], &[]);
        self.camera(EE_SCREENS_GA_CAM_FOLLOWING_OFF, 0, &[]);
    }

    // ---- the mini map (EEAMiniMap) ----

    /// `SendGroup`: the selected group sent (or its buildings' rally point set) to the clicked mini map point.
    fn mini_map_send(&mut self) {
        let modifiers = self.get().mModifiers;
        let queue = modifiers & SHIFT != 0;
        let run = (modifiers >> 1) & 1 != 0;
        let (_, _, c) = self.call(EE_SCREENS_GA_OP_GROUP, 0, &[EE_SCREENS_GA_G_CENTER], &[]);
        let (lx, ly) = self.mini_map_point();
        let mut x = trunc_long(lx) as i64;
        let mut y = trunc_long(ly) as i64;
        if self.map_tile(x, y).is_none() {
            (x, y) = self.closest_tile(lx, ly);
        }
        let angle = um_arctan2(-(lx - c[0]), ly - c[1]);
        let buildings = self.group(EE_SCREENS_GA_G_COUNT_KIND, 2, 0);
        let (tx, ty) = (x as f32 + 0.5, y as f32 + 0.5);
        let message = if buildings == self.group(EE_SCREENS_GA_G_COUNT, 0, 0)
            && self.group(EE_SCREENS_GA_G_KIND_FLAGS, 0, 0) as u32 & EE_SCREENS_GA_KIND_BUILDING != 0
        {
            let first = self.group(EE_SCREENS_GA_G_FIRST_ID, 0, 0);
            self.record(EE_SCREENS_GA_MSG_RALLY, &[vp(tx, ty), vi(-1), vi(first), vb(queue)])
        } else {
            self.path_to(angle, tx, ty, run, true, queue, false)
        };
        self.send(message);
    }

    /// `CenterCamera`: the camera on the clicked mini map point, a point off the map clamped to the nearest tile and
    /// into the mesh's side planes.
    fn mini_map_center(&mut self) {
        let (mut lx, mut ly) = self.mini_map_point();
        let y = floor(ly) as i64;
        let x = floor(lx) as i64;
        if self.map_tile(x, y).is_none() {
            let (tx, ty) = self.closest_tile(lx, ly);
            lx = tx as f32;
            ly = ty as f32;
        }
        let z = self.mesh_z(lx, ly);
        let mut target = Ge3DPoint { x: lx + 0.5, y: ly + 0.5, z };
        if self.map(EE_SCREENS_GA_M_EXISTS, &[], &[]).0 != 0 {
            let plane = |g: &mut Self, k: i64| {
                let (_, _, f) = g.map(EE_SCREENS_GA_M_PLANE, &[k], &[]);
                Ge3DPlane { point: Ge3DPoint { x: f[0], y: f[1], z: f[2] }, normal: GeVector::new(f[3], f[4], f[5]) }
            };
            let clamp = |target: &mut Ge3DPoint, plane: &Ge3DPlane| {
                if !plane.is_point_on_or_below(*target) {
                    return false;
                }
                let line = Ge3DLine { start: *target, direction: plane.normal };
                if let Some(t) = line.intersects_plane(plane) {
                    *target = line.point_at(t);
                }
                true
            };
            let (left, right, front, back) = (plane(self, 0), plane(self, 1), plane(self, 2), plane(self, 3));
            if !clamp(&mut target, &left) {
                clamp(&mut target, &right);
            }
            if !clamp(&mut target, &front) {
                clamp(&mut target, &back);
            }
        }
        self.camera_to([target.x, target.y, target.z]);
        self.camera(EE_SCREENS_GA_CAM_KEEP_ZOOM, 0, &[]);
        self.camera(EE_SCREENS_GA_CAM_CHANGED, 0, &[]);
    }

    // ---- the edge scroll (EEAScroll) ----

    /// `Virtual4`: the camera scrolled toward the active edges, accelerating up to the side scroll speed.
    fn scroll_run(&mut self) {
        let (_, f) = self.scroll(EE_SCREENS_GA_S_SPEED);
        let max = f[0] + 0.5;
        let min = SCROLL_MINIMUM_SPEED_FACTOR * max;
        let (_, z) = self.camera(EE_SCREENS_GA_CAM_ZOOM, 0, &[]);
        let r = self.get();
        let elapsed = r.mElapsed as f32;
        let scale = (z[0] + 0.5) * elapsed * SCROLL_SCALE_FACTOR;
        let acceleration = if r.mAccelerate { elapsed * SCROLL_ACCELERATION_FACTOR * max } else { max + max };
        let mut dx = 0.0f32;
        let mut dy = 0.0f32;
        let mut vx = f32::from_bits(SCROLL_VELOCITY_X.load(Relaxed));
        let mut vy = f32::from_bits(SCROLL_VELOCITY_Y.load(Relaxed));
        if r.mScrollLeft {
            if vx > -min {
                vx = -min;
            }
            vx -= acceleration;
            if vx < -max {
                vx = -max;
            }
            dx = vx * scale;
        } else if r.mScrollRight {
            if vx < min {
                vx = min;
            }
            vx += acceleration;
            if vx > max {
                vx = max;
            }
            dx = vx * scale;
        }
        if r.mScrollDown {
            if vy > -min {
                vy = -min;
            }
            vy -= acceleration;
            if vy < -max {
                vy = -max;
            }
            dy = vy * scale;
        } else if r.mScrollUp {
            if vy < min {
                vy = min;
            }
            vy += acceleration;
            if vy > max {
                vy = max;
            }
            dy = vy * scale;
        }
        SCROLL_VELOCITY_X.store(vx.to_bits(), Relaxed);
        SCROLL_VELOCITY_Y.store(vy.to_bits(), Relaxed);
        if dx != 0.0 || dy != 0.0 {
            self.camera(EE_SCREENS_GA_CAM_SCROLL, 0, &[dx, dy]);
        }
    }

    /// `Update`: which edges the mouse touches (none while it is frozen) and the poll's elapsed time; the action
    /// posted while scrolling, the poll stopped and the velocity zeroed when not; whether any edge scrolls.
    pub fn scroll_update(&mut self, at: (f32, f32)) -> bool {
        let mut scrolling = false;
        let mut elapsed: i64 = 1;
        if self.scroll(EE_SCREENS_GA_S_POLL_RUNNING).0 != 0 {
            elapsed = self.scroll(EE_SCREENS_GA_S_POLL).0;
        }
        if self.op(EE_SCREENS_GA_OP_MOUSE_FROZEN, 0, &[]) == 0 {
            let (_, e) = self.scroll(EE_SCREENS_GA_S_EDGES);
            let left = at.0 <= e[0];
            let up = at.1 <= e[1];
            let right = at.0 >= e[2];
            let down = at.1 >= e[3];
            scrolling = left || up || right || down;
            self.set(|r| {
                r.mScrollLeft = left;
                r.mScrollUp = up;
                r.mScrollRight = right;
                r.mScrollDown = down;
            });
        }
        if scrolling {
            if elapsed > 0 {
                self.set(|r| r.mElapsed = elapsed as u32);
                self.post();
            }
            if self.scroll(EE_SCREENS_GA_S_POLL_RUNNING).0 == 0 {
                self.scroll(EE_SCREENS_GA_S_POLL_START);
            }
        } else if self.scroll(EE_SCREENS_GA_S_POLL_RUNNING).0 != 0 {
            self.scroll(EE_SCREENS_GA_S_POLL_STOP);
            SCROLL_VELOCITY_Y.store(0f32.to_bits(), Relaxed);
            SCROLL_VELOCITY_X.store(0f32.to_bits(), Relaxed);
        }
        scrolling
    }

    // ---- the improvement (EEAUpgradeImprovement) ----

    /// The constructor's: the unit referenced and held by id, the action posted.
    pub fn upgrade_begin(&mut self, unit: usize) {
        self.reference(unit);
        let id = self.unit(unit, EE_SCREENS_GA_U_HANDLE);
        self.set(|r| r.mUnit = id as u32);
        self.post();
    }

    fn upgrade_unit(&mut self) -> usize {
        let id = self.get().mUnit as i64;
        self.op(EE_SCREENS_GA_OP_UNIT, 0, &[EE_SCREENS_GA_U_RESOLVE, 0, id]) as usize
    }

    /// `Virtual4`: raised or lowered by message in a game; in the scenario editor applied directly (a level above 1
    /// to lower, with the buttons' sounds) and its unit tab refreshed.
    fn upgrade(&mut self) {
        let r = self.get();
        let unit = self.upgrade_unit();
        if r.mIsInGame {
            let id = self.unit(unit, EE_SCREENS_GA_U_ID);
            let kind = if r.mRaise { EE_SCREENS_GA_MSG_IMPROVE } else { EE_SCREENS_GA_MSG_REMOVE_IMPROVEMENT };
            let message = self.record(kind, &[vi(id), vi(r.mImprovement as i64)]);
            self.send(message);
        } else if r.mRaise {
            self.editor(EE_SCREENS_GA_E_APPLY_IMPROVEMENT, unit, 0);
        } else if self.op(EE_SCREENS_GA_OP_UNIT, unit, &[EE_SCREENS_GA_U_IMPROVEMENT_LEVEL, 0, r.mImprovement as i64])
            > 1
        {
            self.editor(EE_SCREENS_GA_E_APPLY_IMPROVEMENT, unit, 1);
            self.editor(EE_SCREENS_GA_E_SOUND, 0, 1);
        } else {
            self.editor(EE_SCREENS_GA_E_SOUND, 0, 2);
        }
        self.editor(EE_SCREENS_GA_E_JIT, 0, 0);
    }

    // ---- the quick save/load (EEAQuickSaveLoad, EESEAQuickSaveLoad) ----

    /// `Virtual4`: the quick load or save with the sound muted and a hosted game paused (the editor's then resumes
    /// its current tab).
    fn quick(&mut self) {
        let paused = self.editor(EE_SCREENS_GA_E_PAUSED, 0, 0) != 0;
        self.editor(EE_SCREENS_GA_E_JIT, 0, 0);
        let (_, _, volumes) = self.call(EE_SCREENS_GA_OP_EDITOR, 0, &[EE_SCREENS_GA_E_MUTE], &[]);
        if !paused && self.net(EE_SCREENS_GA_N_IS_SERVER, &[]) != 0 {
            self.editor(EE_SCREENS_GA_E_SERVER_PAUSE, 0, 1);
        }
        if self.get().mIsLoad {
            self.editor(EE_SCREENS_GA_E_QUICK_LOAD, 0, 0);
        } else {
            let old = self.get().mFile;
            if !old.is_null() {
                self.editor(EE_SCREENS_GA_E_DELETE_FILE, old as usize, 0);
                self.set(|r| r.mFile = core::ptr::null_mut());
            }
            let file = self.editor(EE_SCREENS_GA_E_QUICK_SAVE, 0, 0) as usize;
            self.set(|r| r.mFile = file as *mut c_void);
            if self.get().mWriteToFile {
                self.editor(EE_SCREENS_GA_E_QUICK_WRITE, file, 0);
            }
        }
        if !paused && self.net(EE_SCREENS_GA_N_IS_SERVER, &[]) != 0 {
            self.editor(EE_SCREENS_GA_E_SERVER_PAUSE, 0, 0);
        }
        self.call(EE_SCREENS_GA_OP_EDITOR, 0, &[EE_SCREENS_GA_E_UNMUTE], &[volumes[0], volumes[1]]);
        if self.get().mKind == EE_SCREENS_GA_EDITOR_QUICK {
            self.editor(EE_SCREENS_GA_E_RESUME_GROUP, 0, 0);
        }
    }

    // ---- the multiplayer join or host (EEAJoinOrHostAGame) ----

    /// The connection request started and given as local input, the lobby's client event set.
    fn client_request(&mut self, firewall: i64) {
        self.net(EE_SCREENS_GA_N_REQUEST, &[firewall, 1]);
    }

    /// `Virtual4`: the client or server started for the mode, connected or listening, any error shown.
    fn join(&mut self) {
        let firewall = self.net(EE_SCREENS_GA_N_FIREWALL, &[]);
        if self.net(EE_SCREENS_GA_N_VALID_NAME, &[0]) == 0 {
            self.net(EE_SCREENS_GA_N_SHOW_STRING, &[EE_SCREENS_GA_IDS_PLAYER_NAME_IS_INVALID, 0]);
            return;
        }
        let mode = self.get().mJoinMode;
        if mode != 0 && mode != 4 {
            if self.net(EE_SCREENS_GA_N_COMMS, &[]) == 0 {
                self.net(EE_SCREENS_GA_N_NEW_CLIENT, &[0]);
            } else if self.net(EE_SCREENS_GA_N_IS_SERVER, &[]) != 0 {
                self.net(EE_SCREENS_GA_N_DELETE_COMMS, &[]);
                self.net(EE_SCREENS_GA_N_NEW_CLIENT, &[1]);
            }
        } else {
            self.net(EE_SCREENS_GA_N_DELETE_COMMS, &[]);
            if self.net(EE_SCREENS_GA_N_VALID_NAME, &[1]) != 0 {
                self.net(EE_SCREENS_GA_N_NEW_SERVER, &[(mode != 4) as i64]);
                if self.net(EE_SCREENS_GA_N_PUBLISHED, &[]) == 0 {
                    self.net(EE_SCREENS_GA_N_SHOW_STRING, &[EE_SCREENS_GA_IDS_GAME_NAME_EXISTS, 1]);
                    self.net(EE_SCREENS_GA_N_DELETE_COMMS, &[]);
                    self.net(EE_SCREENS_GA_N_WON_REJOIN, &[]);
                } else if mode != 4 {
                    self.net(EE_SCREENS_GA_N_REMEMBER_HOST, &[]);
                }
            } else {
                self.net(EE_SCREENS_GA_N_SHOW_STRING, &[EE_SCREENS_GA_IDS_GAME_NAME_IS_INVALID, 1]);
            }
        }
        if self.net(EE_SCREENS_GA_N_COMMS, &[]) == 0 {
            return;
        }
        self.net(EE_SCREENS_GA_N_OPTIONS, &[]);
        match mode {
            2 | 3 => {
                if self.net(EE_SCREENS_GA_N_CONNECT_ADDRESS, &[]) == 0 {
                    self.client_request(firewall);
                    self.net(EE_SCREENS_GA_N_LOBBY_EVENT, &[1]);
                    self.net(EE_SCREENS_GA_N_SET_NAME, &[(mode == 3) as i64]);
                }
            }
            1 => {
                let count = self.net(EE_SCREENS_GA_N_DETECTED_COUNT, &[]);
                for k in 0..count {
                    if self.net(EE_SCREENS_GA_N_DETECTED, &[k]) != 0 {
                        break;
                    }
                    if self.net(EE_SCREENS_GA_N_DETECTED_MATCH, &[]) != 0 {
                        if self.net(EE_SCREENS_GA_N_CONNECT_DETECTED, &[]) == 0 {
                            self.client_request(firewall);
                            self.net(EE_SCREENS_GA_N_SET_NAME, &[0]);
                            self.net(EE_SCREENS_GA_N_LOBBY_EVENT, &[1]);
                        }
                        break;
                    }
                }
            }
            0 | 4 => {
                self.net(EE_SCREENS_GA_N_LOBBY_EVENT, &[0]);
                self.net(EE_SCREENS_GA_N_REQUEST, &[firewall, 0]);
            }
            _ => {}
        }
    }

    // ---- the host migration (EEAServerMigration) ----

    /// `Virtual4`: after the server dropped, the first human who can host (not behind a firewall, unless no other
    /// human can) becomes the server; the current player restarts as the server or reconnects to the new one.
    fn migrate(&mut self) {
        self.net(EE_SCREENS_GA_N_MIGRATE_BEGIN, &[]);
        if self.net(EE_SCREENS_GA_N_HAS_LEFT, &[]) == 0 {
            let player = |g: &mut Self, k: i64| {
                let (_, i, _) = g.call(EE_SCREENS_GA_OP_NET, 0, &[EE_SCREENS_GA_N_PLAYER, k], &[]);
                (i[0], i[1] != 0, i[2] != 0, i[3] != 0)
            };
            let mut human_available = false;
            let mut k = 1;
            while k < self.net(EE_SCREENS_GA_N_NUM_PLAYERS, &[]) {
                let (slot, firewall, human, _) = player(self, k);
                if slot != EE_SCREENS_GA_SLOT_SERVER && !firewall && human && slot != EE_SCREENS_GA_SLOT_NONE {
                    human_available = true;
                    break;
                }
                k += 1;
            }
            if self.net(EE_SCREENS_GA_N_LOADING_SAVED, &[]) == 0 {
                self.net(EE_SCREENS_GA_N_CLEAR_SELECTIONS, &[]);
            }
            let mut count: i64 = 0;
            let mut found = false;
            let mut is_host = false;
            let mut k = 1;
            while k < self.net(EE_SCREENS_GA_N_NUM_PLAYERS, &[]) {
                let (slot, firewall, human, current) = player(self, k);
                if human {
                    if slot == EE_SCREENS_GA_SLOT_SERVER {
                        self.net(EE_SCREENS_GA_N_SET_SLOT, &[k, EE_SCREENS_GA_SLOT_NONE]);
                        self.net(EE_SCREENS_GA_N_KEEP_NAME, &[k, 0]);
                    } else if current {
                        if !found && (!firewall || !human_available) {
                            is_host = true;
                            found = true;
                            self.net(EE_SCREENS_GA_N_SET_SLOT, &[k, EE_SCREENS_GA_SLOT_SERVER]);
                        }
                        count += 1;
                    } else if slot != EE_SCREENS_GA_SLOT_NONE {
                        if !found && (!firewall || !human_available) {
                            is_host = false;
                            found = true;
                            self.net(EE_SCREENS_GA_N_KEEP_NAME, &[k, 1]);
                            self.net(EE_SCREENS_GA_N_SET_SLOT, &[k, EE_SCREENS_GA_SLOT_SERVER]);
                        } else {
                            self.net(EE_SCREENS_GA_N_SET_SLOT, &[k, EE_SCREENS_GA_SLOT_NONE]);
                        }
                        count += 1;
                    }
                }
                k += 1;
            }
            if is_host {
                self.net(EE_SCREENS_GA_N_PROMOTE, &[]);
                self.net(EE_SCREENS_GA_N_RECONNECT_WAIT, &[count - 1]);
            } else if self.net(EE_SCREENS_GA_N_RECONNECT, &[]) == 0 {
                self.net(EE_SCREENS_GA_N_RECONNECT_REQUEST, &[]);
            }
        }
        self.net(EE_SCREENS_GA_N_LAG_CLEAR, &[]);
    }
}

/// The string ids the join shows (`ee/EEStringIDs.h`; checked by the host).
pub const EE_SCREENS_GA_IDS_PLAYER_NAME_IS_INVALID: i64 = 10004;
pub const EE_SCREENS_GA_IDS_GAME_NAME_IS_INVALID: i64 = 10005;
pub const EE_SCREENS_GA_IDS_GAME_NAME_EXISTS: i64 = 10010;

/// `EEAScroll`'s constants (0x439070, 0x43906c, 0x439074).
const SCROLL_MINIMUM_SPEED_FACTOR: f32 = 0.1;
const SCROLL_SCALE_FACTOR: f32 = 0.1;
const SCROLL_ACCELERATION_FACTOR: f32 = 0.01;

/// The camera's scroll velocity, kept between the scroll actions (the main game's and the editor's share it:
/// 0x4e5be4 / 0x4e5be0).
static SCROLL_VELOCITY_X: AtomicU32 = AtomicU32::new(0);
static SCROLL_VELOCITY_Y: AtomicU32 = AtomicU32::new(0);

/// Tests: the velocity back to rest.
#[cfg(test)]
pub(crate) fn scroll_velocity_reset() {
    SCROLL_VELOCITY_X.store(0, Relaxed);
    SCROLL_VELOCITY_Y.store(0, Relaxed);
}
