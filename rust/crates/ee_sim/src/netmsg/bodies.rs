//! The game messages' `Execute` bodies (RUST_PLAN R5b.4, game messages round 5): `EEMRecord::Execute` hands its
//! record here (`ee_netmsg_execute`); each type's body reads its members, makes the decisions and asks the C++ for
//! each world call, one host operation per call (`EE_NETMSG_OP_*`: EEMessage.cpp's `EENetMsgHost`, one switch). The
//! unit orders go to the units lane's bodies (ee_units, over EEUnitHost.cpp). The capture's NMDC record sees every
//! decision as before (`EE_NETMSG_OP_DECISION`). A few bodies stay C++ whole (`EE_NETMSG_BODY_*`: the lobby's,
//! the files', the strings' and the screens' work, with nothing to decide).
//!
//! Simulation code (RUST_PLAN 2.1): the original's world calls in its order, its reads where it read them; the
//! conversions the C++ made (`(long)` of a float, a member's type) are made as it made them.

use crate::netmsg::execute::ffi as decide;
use crate::netmsg::records::{record_member, EE_NETMSG_RECORD_FIELD_NONE};
use crate::netmsg::units;
use std::ffi::c_void;
use std::sync::{Mutex, OnceLock};

/// One world call: the message (`EEMRecord*`), two objects (a player, a group, a unit: their addresses), integers
/// and floats; the operation's answer is the host's return value (an object's address, a fact).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeNetMsgCall {
    pub message: u64,
    pub a: u64,
    pub b: u64,
    pub i: [i64; 14],
    pub f: [f32; 2],
}

/// The C++ host (EEMessage.cpp's `EENetMsgHost`).
pub type EeNetMsgHostFn = unsafe extern "C" fn(op: u32, call: *mut EeNetMsgCall) -> i64;

// ---- the host operations (a: the first object, b: the second, i/f: the values) ----
/// the sender (`gWorld.GetPlayerBySlot` of the message's originating slot)
pub const EE_NETMSG_OP_SENDER: u32 = 1;
/// `gWorld.mPlayers[i0]`
pub const EE_NETMSG_OP_PLAYER: u32 = 2;
/// `gWorld.GetCurrentPlayer()`
pub const EE_NETMSG_OP_CURRENT: u32 = 3;
/// a's active group
pub const EE_NETMSG_OP_GROUP: u32 = 4;
/// a's saved group i0
pub const EE_NETMSG_OP_SAVED_GROUP: u32 = 5;
/// the group a's unit count
pub const EE_NETMSG_OP_GROUP_COUNT: u32 = 6;
/// the group a's unit i0
pub const EE_NETMSG_OP_GROUP_UNIT: u32 = 7;
/// the group a's first unit
pub const EE_NETMSG_OP_GROUP_FIRST: u32 = 8;
/// whether the group a holds the unit b
pub const EE_NETMSG_OP_GROUP_CONTAINS: u32 = 9;
/// the group a takes the unit b
pub const EE_NETMSG_OP_GROUP_ADD: u32 = 10;
/// the group a emptied
pub const EE_NETMSG_OP_GROUP_CLEAR: u32 = 11;
/// `gWorld.GetUnit(i0)`
pub const EE_NETMSG_OP_UNIT: u32 = 12;
/// the unit b's fact i0 (`EE_NETMSG_UNIT_*`), i1 an argument
pub const EE_NETMSG_OP_UNIT_FACT: u32 = 13;
/// the player a's fact i0 (`EE_NETMSG_PLAYER_*`), i1.. arguments
pub const EE_NETMSG_OP_PLAYER_FACT: u32 = 14;
/// the world's fact i0 (`EE_NETMSG_WORLD_*`)
pub const EE_NETMSG_OP_WORLD_FACT: u32 = 15;
/// a capture decision record (NMDC): kind i0, the count i1, the values i2..
pub const EE_NETMSG_OP_DECISION: u32 = 16;
/// a chat log line: the text i0 (`EE_NETMSG_TEXT_*`), the kind i1; none for an empty text when i2
pub const EE_NETMSG_OP_CHAT_TEXT: u32 = 17;
/// a sound played (`EE_NETMSG_SOUND_*`)
pub const EE_NETMSG_OP_SOUND: u32 = 18;
/// a body the C++ keeps (`EE_NETMSG_BODY_*`)
pub const EE_NETMSG_OP_BODY: u32 = 19;
/// a's selection from the ids at b (i1 of them): AddAndRemoveUnitsFromSelectedGroup (i0 0), AddUnitsToSelectedGroup
/// (1), SelectUnits (2)
pub const EE_NETMSG_OP_SELECT_LIST: u32 = 20;
/// a's selection: RemoveUnitFromSelectedGroup(i1) (i0 0), AddUnitToSelectedGroup(i1, true) (1), SelectUnit(i1,
/// true, true) (2), UnselectAllUnits (3)
pub const EE_NETMSG_OP_SELECT: u32 = 21;
/// the player a's resource i1: AddResource (i0 0) or TakeResource (1) of i2; the trade's tally (2:
/// `mResourceTrade[i1 - 2] += i2`); a tribute (3: `AddTribute(i3, i1, i2)`)
pub const EE_NETMSG_OP_RESOURCE: u32 = 22;
/// the unit b: Kill(i0, 0, 0)
pub const EE_NETMSG_OP_KILL: u32 = 23;
/// the unit b of the player a: the disband's refund of f0, its effect, Kill(0, 0, 0)
pub const EE_NETMSG_OP_DISBAND: u32 = 24;
/// the unit b: the delete's refund (its technology at the world's delete factor)
pub const EE_NETMSG_OP_DELETE_REFUND: u32 = 25;
/// the unit b: an action (`EE_NETMSG_ACT_*`), i1.. arguments, a another object
pub const EE_NETMSG_OP_UNIT_ACT: u32 = 26;
/// the player a: an action (`EE_NETMSG_PACT_*`), i1.. arguments
pub const EE_NETMSG_OP_PLAYER_ACT: u32 = 27;
/// the world: an action (`EE_NETMSG_WACT_*`), i1.. arguments, f0.., the objects a and b
pub const EE_NETMSG_OP_WORLD_ACT: u32 = 28;
/// the lobby game option i0 set to i1 (2, the map type: to the message's string), under the write lock
pub const EE_NETMSG_OP_OPTION: u32 = 29;
/// the unit class a's fact i0 (`EE_NETMSG_CLASS_*`)
pub const EE_NETMSG_OP_CLASS_FACT: u32 = 30;
/// a tile of the map (i3, i4) and `EECanPlace(a, i0, &GE3DPoint(i1, i2, 0), i3, i4, 0, 0)` on it (false off it)
pub const EE_NETMSG_OP_CAN_PLACE: u32 = 31;
/// the selection framing (`EE_NETMSG_SEL_*`) of the player a: the communications object's fast-selection state
pub const EE_NETMSG_OP_SELECTION: u32 = 32;

/// the fast-selection lock entered (i1 1) or left (0)
pub const EE_NETMSG_SEL_LOCK: i64 = 0;
/// `gWorld.mExecutingLocalSelection = i1`
pub const EE_NETMSG_SEL_MARK_LOCAL: i64 = 1;
/// the communications object's current selection id
pub const EE_NETMSG_SEL_CURRENT_ID: i64 = 2;
/// the player's last selection id (the communications object's per player)
pub const EE_NETMSG_SEL_LAST_ID: i64 = 3;
/// the player's last selection id set to i1
pub const EE_NETMSG_SEL_SET_LAST_ID: i64 = 4;
/// the player's selected group copies its active group (and its viewed unit)
pub const EE_NETMSG_SEL_COPY: i64 = 5;
/// the player's selection undone (UnselectAllUnits, the selected group copies the active one)
pub const EE_NETMSG_SEL_RESET: i64 = 6;
/// the player's JIT flag cleared (the interface refreshed for the local player)
pub const EE_NETMSG_SEL_JIT_OFF: i64 = 7;

// ---- facts ----
pub const EE_NETMSG_UNIT_HIT_POINTS: i64 = 0;
pub const EE_NETMSG_UNIT_TYPE: i64 = 1;
pub const EE_NETMSG_UNIT_CATEGORY: i64 = 2;
pub const EE_NETMSG_UNIT_MAX_HIT_POINTS: i64 = 3;
pub const EE_NETMSG_UNIT_CAN_DISBAND: i64 = 4;
pub const EE_NETMSG_UNIT_IS_PACKED: i64 = 5;
/// the class's packed (or unpacked) class
pub const EE_NETMSG_UNIT_PACKED_CLASS: i64 = 6;
pub const EE_NETMSG_UNIT_FLIGHT_TIME: i64 = 7;
/// the class has a death-spawn class
pub const EE_NETMSG_UNIT_DEATH_SPAWN: i64 = 8;
pub const EE_NETMSG_UNIT_CAN_CHANGE_BEHAVIOR: i64 = 9;
/// `mComplexUnit`
pub const EE_NETMSG_UNIT_COMPLEX: i64 = 10;
/// `mTileNode != NULL`
pub const EE_NETMSG_UNIT_TILE_NODE: i64 = 11;
/// `mPlayer`
pub const EE_NETMSG_UNIT_OWNER: i64 = 12;
pub const EE_NETMSG_UNIT_LAND_BUILDING: i64 = 13;
pub const EE_NETMSG_UNIT_PAID_FOR: i64 = 14;
pub const EE_NETMSG_UNIT_BUILT: i64 = 15;
pub const EE_NETMSG_UNIT_CAN_DELETE: i64 = 16;
pub const EE_NETMSG_UNIT_TILE_X: i64 = 17;
pub const EE_NETMSG_UNIT_TILE_Y: i64 = 18;
pub const EE_NETMSG_UNIT_DOMAINS: i64 = 19;
/// the next unit of its player's chain
pub const EE_NETMSG_UNIT_NEXT: i64 = 20;
/// `EEUnitCast<EEComplexUnit>`: the unit if it is a complex unit, else 0
pub const EE_NETMSG_UNIT_AS_COMPLEX: i64 = 21;
/// whether unit set i1 holds the unit's type
pub const EE_NETMSG_UNIT_IN_SET: i64 = 22;

pub const EE_NETMSG_PLAYER_OBSERVER: i64 = 0;
pub const EE_NETMSG_PLAYER_CIV_CHOSEN: i64 = 1;
pub const EE_NETMSG_PLAYER_SLOT: i64 = 2;
pub const EE_NETMSG_PLAYER_CHAT_TYPE: i64 = 3;
/// `mEnemies[i1]`
pub const EE_NETMSG_PLAYER_ENEMY: i64 = 4;
/// `mSendChatMessage[i1]`
pub const EE_NETMSG_PLAYER_SENDS_CHAT: i64 = 5;
pub const EE_NETMSG_PLAYER_ID: i64 = 6;
/// `mResourceCaps[i1]`
pub const EE_NETMSG_PLAYER_RESOURCE: i64 = 7;
pub const EE_NETMSG_PLAYER_FIRST_UNIT: i64 = 8;
/// `EEGetTradePrice(a, i1, i2)` (it makes its own NMDC record)
pub const EE_NETMSG_PLAYER_TRADE_PRICE: i64 = 9;

pub const EE_NETMSG_WORLD_NUM_PLAYERS: i64 = 0;
pub const EE_NETMSG_WORLD_CURRENT_INDEX: i64 = 1;
/// a selection is being replayed (the communications object's `mIsExecutingSelection`)
pub const EE_NETMSG_WORLD_EXECUTING_SELECTION: i64 = 2;
pub const EE_NETMSG_WORLD_IS_SERVER: i64 = 3;
pub const EE_NETMSG_WORLD_HAS_COMMUNICATIONS: i64 = 4;
/// the client's send-to slot, 0xfe on the server
pub const EE_NETMSG_WORLD_LOCAL_SEND_SLOT: i64 = 5;
/// the message's string's character count
pub const EE_NETMSG_WORLD_STRING_LENGTH: i64 = 6;
/// a recorded game is playing back (on a client or the server)
pub const EE_NETMSG_WORLD_PLAYBACK: i64 = 7;
pub const EE_NETMSG_WORLD_COLLISION_SPHERE_MODE: i64 = 8;
/// the server's humans less its dropped loaders
pub const EE_NETMSG_WORLD_LOADED_HUMANS: i64 = 9;
/// the server's `mNumLoadResponses`
pub const EE_NETMSG_WORLD_LOAD_RESPONSES: i64 = 10;
/// the server's humans
pub const EE_NETMSG_WORLD_HUMANS: i64 = 11;
/// the options' write lock taken (1) or released and the lobby told (0)
pub const EE_NETMSG_WORLD_OPTIONS_LOCK: i64 = 12;

pub const EE_NETMSG_CLASS_TYPE: i64 = 0;
pub const EE_NETMSG_CLASS_TILE_SIZE: i64 = 1;

// ---- actions ----
/// ClearGoals(0), StartAction(idle), the class's ActionChanged
pub const EE_NETMSG_ACT_HALT: i64 = 0;
pub const EE_NETMSG_ACT_GO_FANATIC: i64 = 1;
/// ToggleJITManufacturing, UpdateJITManufacturing
pub const EE_NETMSG_ACT_JIT: i64 = 2;
/// ToggleJITManufacturing alone
pub const EE_NETMSG_ACT_JIT_TOGGLE: i64 = 3;
/// RemoveResearch(i1, 0)
pub const EE_NETMSG_ACT_REMOVE_RESEARCH: i64 = 4;
/// SetLocked(i1) on the gate and its second and fourth parts
pub const EE_NETMSG_ACT_LOCK_GATE: i64 = 5;
/// queue i1's timer + 60 s when it holds goals
pub const EE_NETMSG_ACT_RALLY_TIME: i64 = 6;
/// the behaviour `EEUnitBehavior::sTable[i1]` (when it differs)
pub const EE_NETMSG_ACT_BEHAVIOR: i64 = 7;
/// the class's ReplantFarm(unit, the player a)
pub const EE_NETMSG_ACT_REPLANT: i64 = 8;
/// SetHomeBase(a)
pub const EE_NETMSG_ACT_HOME_BASE: i64 = 9;
/// SetFlightTime(0)
pub const EE_NETMSG_ACT_LAND: i64 = 10;
/// the aircraft goes to (i1, i2): its queues cleared, the move in queues 0 and 1, 1 s on queue 1
pub const EE_NETMSG_ACT_SCRAMBLE: i64 = 11;
/// replaced by the class a for the player i1 and packed
pub const EE_NETMSG_ACT_PACK: i64 = 12;
/// replaced by the class a for the player i1 (the object i2), placed at its old tile, unpacked
pub const EE_NETMSG_ACT_UNPACK: i64 = 13;
/// the class's inner (i1 9) or outer (10) collision sphere grown (i2 1) or shrunk by 0.05; the radius the inner's
pub const EE_NETMSG_ACT_SPHERE: i64 = 14;
/// the radius the unit a's class's inner sphere
pub const EE_NETMSG_ACT_RADIUS: i64 = 15;

/// shift mode i1 (and the mouse's for the local player)
pub const EE_NETMSG_PACT_SHIFT_MODE: i64 = 0;
/// the whole map seen or not (the mini-map redrawn for the local player)
pub const EE_NETMSG_PACT_SEES_ALL: i64 = 1;
pub const EE_NETMSG_PACT_INCREMENT_EPIC: i64 = 2;
/// the player's JIT flag toggled (the interface refreshed for the local player)
pub const EE_NETMSG_PACT_JIT: i64 = 3;
/// ApplyCheatCode(i1, id, i2)
pub const EE_NETMSG_PACT_CHEAT: i64 = 4;
pub const EE_NETMSG_PACT_CHAT_TYPE: i64 = 5;
/// `mSendChatMessage[i1] = i2`
pub const EE_NETMSG_PACT_SEND_CHAT: i64 = 6;
/// statistics 0x14 and 0x15
pub const EE_NETMSG_PACT_REPORT: i64 = 7;
pub const EE_NETMSG_PACT_CLEAR_SAVED: i64 = 8;
/// saved group i1 assigned the selection
pub const EE_NETMSG_PACT_ASSIGN_SAVED: i64 = 9;
/// PlayAcknowledgement(the unit i1)
pub const EE_NETMSG_PACT_ACKNOWLEDGE: i64 = 10;
pub const EE_NETMSG_PACT_LOCK_CIV: i64 = 11;
/// RemovePlayerFromGame(0, id), SetGameResult(2)
pub const EE_NETMSG_PACT_REMOVE: i64 = 12;
/// RemoveQueuedImprovement(i1, i2, CountQueuedImprovements(i1, i2) != 0)
pub const EE_NETMSG_PACT_REMOVE_IMPROVEMENT: i64 = 13;
/// the help string refreshed when it is the local player's id
pub const EE_NETMSG_PACT_UPDATE_HELP: i64 = 14;
/// the JIT button refreshed when it is the local player
pub const EE_NETMSG_PACT_UPDATE_JIT: i64 = 15;
/// the unit i1's patrol group selected
pub const EE_NETMSG_PACT_PATROL_GROUP: i64 = 16;

pub const EE_NETMSG_WACT_ATTACK_PLAYERS: i64 = 0;
/// EEBuildGate at the point f0, f1 ((long) casts), i1..i3
pub const EE_NETMSG_WACT_BUILD_GATE: i64 = 1;
pub const EE_NETMSG_WACT_EDICT: i64 = 2;
pub const EE_NETMSG_WACT_END_CINEMATIC: i64 = 3;
pub const EE_NETMSG_WACT_GAME_SPEED: i64 = 4;
/// the communications object's host migration ended
pub const EE_NETMSG_WACT_MIGRATING_OFF: i64 = 5;
pub const EE_NETMSG_WACT_LAG: i64 = 6;
pub const EE_NETMSG_WACT_LOAD_PERCENT: i64 = 7;
/// the physics thread posted message i1
pub const EE_NETMSG_WACT_POST_PHYSICS: i64 = 8;
pub const EE_NETMSG_WACT_COLLISION_UNIT: i64 = 9;
pub const EE_NETMSG_WACT_COLLISION_FLAGS: i64 = 10;
pub const EE_NETMSG_WACT_ZOOM: i64 = 11;
pub const EE_NETMSG_WACT_RECORD_FRAMES: i64 = 12;
/// the options' write lock: taken (i1 1), released and the lobby told (0)
pub const EE_NETMSG_WACT_OPTIONS_LOCK: i64 = 13;
/// lobby slot i1's field i2 set to i3 (the ping: f0), the row refreshed
pub const EE_NETMSG_WACT_PLAYER_INFO: i64 = 14;
/// the chat line posted (i1: it is our own)
pub const EE_NETMSG_WACT_POST_CHAT: i64 = 15;
pub const EE_NETMSG_WACT_TIMER_EXPIRED: i64 = 16;
pub const EE_NETMSG_WACT_START_GAME: i64 = 17;
pub const EE_NETMSG_WACT_CLEAR_SEND_TO_SLOTS: i64 = 18;
pub const EE_NETMSG_WACT_LOAD_RESPONSES: i64 = 19;
/// the player i1's send-to slot the lobby slot i2's
pub const EE_NETMSG_WACT_SEND_TO_SLOT: i64 = 20;
pub const EE_NETMSG_WACT_ALL_LOADED: i64 = 21;
/// the game log's chat line of the player a
pub const EE_NETMSG_WACT_CHAT_LOG: i64 = 22;
/// the chat line of the player a shown with the texts i1, i2 (-1: none); the sound when a is not the player b
pub const EE_NETMSG_WACT_SHOW_CHAT: i64 = 23;
pub const EE_NETMSG_WACT_LAST_CHAT: i64 = 24;
/// the tribute's chat line from a to b: the viewer i1, i2 resources (bytes of i3), the amounts i4..
pub const EE_NETMSG_WACT_TRIBUTE_TEXT: i64 = 25;
pub const EE_NETMSG_WACT_RESIGN_TEXT: i64 = 26;
/// `gPhysics->FlushWorld()`
pub const EE_NETMSG_WACT_FLUSH: i64 = 27;

// ---- texts and sounds (the C++ maps them to IDS_* and kSound*) ----
pub const EE_NETMSG_TEXT_RUNNING_OUT: i64 = 0;
pub const EE_NETMSG_TEXT_SELECTION_EXPIRED: i64 = 1;
pub const EE_NETMSG_TEXT_MAY_NOT_UNPACK: i64 = 2;
pub const EE_NETMSG_TEXT_SETUP_ALL: i64 = 3;
pub const EE_NETMSG_TEXT_SETUP_ALLIES: i64 = 4;
pub const EE_NETMSG_TEXT_SETUP_ENEMIES: i64 = 5;
pub const EE_NETMSG_TEXT_SETUP_CUSTOM: i64 = 6;
pub const EE_NETMSG_TEXT_OBSERVERS: i64 = 7;
pub const EE_NETMSG_TEXT_NONE: i64 = -1;

pub const EE_NETMSG_SOUND_CLOCK_TICK: i64 = 0;
pub const EE_NETMSG_SOUND_CASH_REGISTER: i64 = 1;

// ---- the bodies the C++ keeps ----
pub const EE_NETMSG_BODY_CIV_UPGRADES_PURCHASED: i64 = 0;
pub const EE_NETMSG_BODY_CIV_UPGRADES_REQUESTED: i64 = 1;
pub const EE_NETMSG_BODY_IMMEDIATE_CARGO_UNLOAD: i64 = 2;
pub const EE_NETMSG_BODY_IMPROVE_UNIT: i64 = 3;
pub const EE_NETMSG_BODY_LOAD_MAIN_SCREEN: i64 = 4;
pub const EE_NETMSG_BODY_LOAD_OR_SAVE_GAME: i64 = 5;
pub const EE_NETMSG_BODY_LOAD_UNIT_DATA: i64 = 6;
pub const EE_NETMSG_BODY_MIGRATION_SLOT_INFO: i64 = 7;
pub const EE_NETMSG_BODY_SAVED_GROUP_SELECT: i64 = 8;
pub const EE_NETMSG_BODY_SEND_FLARE: i64 = 9;
pub const EE_NETMSG_BODY_SET_DIPLOMATIC_STANCE: i64 = 10;
pub const EE_NETMSG_BODY_START_LOADING: i64 = 11;
pub const EE_NETMSG_BODY_CHANGE_FORMATION: i64 = 12;

// the capture's decision kinds (ee/EEMessageCapture.h)
const KIND_SELECTION_MARKS_LOCAL: i64 = 1;
const KIND_SELECTION_COPIES: i64 = 2;
const KIND_COMMAND_RUNS: i64 = 3;
const KIND_SELECTION_PASSES: i64 = 4;
const KIND_ADD_REMOVE: i64 = 5;
const KIND_EXCHANGE_STEP: i64 = 7;
const KIND_TRIBUTE_ORDER: i64 = 8;
const KIND_TRIBUTE_VIEWER: i64 = 9;
const KIND_DISBAND_REFUND: i64 = 10;

const SLOT_NONE: i64 = 0xff;
const RESOURCE_GOLD: i64 = 5;
const UNIT_TYPE_GRANARY: i64 = 0x34;
const UNIT_TYPE_AIRPORT: i64 = 0x42;
const UNIT_TYPE_PATH_POINT: i64 = 0xdf;
const UNIT_TYPE_GRANARY_IMPROVED: i64 = 0x2ba;
const CATEGORY_AIRPLANE: i64 = 3;
const CATEGORY_PLANET_SPACESHIP: i64 = 0x22;
const MOVEMENT_SEA: i64 = 4;
const UNIT_SET_FANATICS: i64 = 0x2f;

// the message types (ee/EEMessageID.h)
mod t {
    pub const EDICT_PROPOSAL: u8 = 0x41;
    pub const PAY_TRIBUTE: u8 = 0x44;
    pub const SELECT_UNIT: u8 = 0x48;
    pub const ADD_UNIT_TO_SELECTED_GROUP: u8 = 0x49;
    pub const REMOVE_UNIT_FROM_SELECTED_GROUP: u8 = 0x4a;
    pub const UNSELECT_ALL_UNITS: u8 = 0x4b;
    pub const ATTACK_UNIT: u8 = 0x4c;
    pub const PATH_TO: u8 = 0x4d;
    pub const SELECT_UNITS: u8 = 0x4e;
    pub const ADD_UNITS_TO_SELECTED_GROUP: u8 = 0x4f;
    pub const ADD_AND_REMOVE_UNITS_FROM_SELECTED_GROUP: u8 = 0x50;
    pub const GATHER_RESOURCE: u8 = 0x51;
    pub const RESEARCH_TECHNOLOGY: u8 = 0x52;
    pub const REMOVE_TECHNOLOGY: u8 = 0x53;
    pub const MAKE_WORLD_VISIBLE: u8 = 0x55;
    pub const BUILD_UNIT: u8 = 0x56;
    pub const REPAIR_UNIT: u8 = 0x57;
    pub const HALT_SELECTED_GROUP: u8 = 0x58;
    pub const BUILD_WALL: u8 = 0x59;
    pub const CHAT: u8 = 0x5a;
    pub const KILL_UNIT: u8 = 0x5c;
    pub const START_LOADING: u8 = 0x5d;
    pub const START_LOADING_RESPONSE: u8 = 0x5e;
    pub const GAME_SPEED: u8 = 0x5f;
    pub const GAME_OPTIONS: u8 = 0x60;
    pub const DROPOFF_RESOURCE: u8 = 0x61;
    pub const TOGGLE_DAY_NIGHT: u8 = 0x62;
    pub const SNAP_TO_DAYLIGHT: u8 = 0x63;
    pub const LOAD_OR_SAVE_GAME: u8 = 0x64;
    pub const TOGGLE_COLLISION_DETECTION: u8 = 0x65;
    pub const SET_RALLY_GOAL: u8 = 0x66;
    pub const LOAD_TRANSPORT: u8 = 0x67;
    pub const UNLOAD_TRANSPORT: u8 = 0x68;
    pub const HACK_A_NEW_UNIT: u8 = 0x6b;
    pub const INCREMENT_EPIC: u8 = 0x6c;
    pub const RECORD_FRAMES: u8 = 0x6d;
    pub const ZOOM_INFO: u8 = 0x6f;
    pub const ATTACK_PLAYERS: u8 = 0x70;
    pub const LOAD_UNIT_DATA: u8 = 0x71;
    pub const ADD_PATROL_POINT: u8 = 0x72;
    pub const PATROL: u8 = 0x73;
    pub const SELECT_PATROL_GROUP: u8 = 0x74;
    pub const CAST_CALAMITY: u8 = 0x75;
    pub const ALWAYS_ADD_UNIT_TO_SELECTED_GROUP: u8 = 0x76;
    pub const IMPROVE_UNIT: u8 = 0x77;
    pub const CHANGE_FORMATION: u8 = 0x78;
    pub const EXPLORE: u8 = 0x79;
    pub const UPDATE_PING_STAT: u8 = 0x7a;
    pub const LOAD_GARRISON: u8 = 0x7b;
    pub const MIGRATION_SLOT_INFO: u8 = 0x7c;
    pub const IN_GAME_CHAT: u8 = 0x7d;
    pub const CIV_UPGRADES_REQUESTED: u8 = 0x7e;
    pub const CIV_UPGRADES_PURCHASED: u8 = 0x7f;
    pub const SET_AIRPORT_COMMAND: u8 = 0x80;
    pub const REPLANT_FARMS: u8 = 0x81;
    pub const SAVED_GROUP_ADD_SELECTION: u8 = 0x82;
    pub const SAVED_GROUP_CREATE: u8 = 0x83;
    pub const SAVED_GROUP_SELECT: u8 = 0x84;
    pub const CHEAT_CODE: u8 = 0x85;
    pub const BUILD_GATE: u8 = 0x86;
    pub const CLEAR_AIRPORT_COMMANDS: u8 = 0x87;
    pub const ENTER_SHIFT_MODE: u8 = 0x88;
    pub const EXIT_SHIFT_MODE: u8 = 0x89;
    pub const STOP_ACTIVE_GROUP: u8 = 0x8a;
    pub const REMOVE_IMPROVEMENT: u8 = 0x8b;
    pub const LOCK_UNLOCK: u8 = 0x8c;
    pub const CHANGE_SELECTED_UNIT_COLLISION_SPHERE: u8 = 0x8d;
    pub const INCREMENT_RALLY_TIMER: u8 = 0x8e;
    pub const SMART_FARMERS: u8 = 0x8f;
    pub const RETURN_TO_BASE: u8 = 0x90;
    pub const GAME_RESTARTING: u8 = 0x91;
    pub const HEAL_UNIT: u8 = 0x92;
    pub const SAVED_GROUP_CLEAR: u8 = 0x93;
    pub const LOAD_PERCENT: u8 = 0x94;
    pub const IMMEDIATE_CARGO_UNLOAD: u8 = 0x95;
    pub const SELECT_THESE_UNITS: u8 = 0x96;
    pub const LOAD_MAIN_SCREEN: u8 = 0x97;
    pub const LOAD_MAIN_SCREEN_RESPONSE: u8 = 0x98;
    pub const SET_DIPLOMATIC_STANCE: u8 = 0x99;
    pub const ADD_REMOVE_RESOURCES: u8 = 0x9b;
    pub const SEND_FLARE: u8 = 0x9c;
    pub const END_CINEMATIC: u8 = 0x9d;
    pub const SET_UNIT_BEHAVIOR: u8 = 0x9e;
    pub const ATTACK_GROUND: u8 = 0x9f;
    pub const SCRAMBLE_AIRCRAFT: u8 = 0xa0;
    pub const SELECT_PLAYER_CHAT_TYPE: u8 = 0xa1;
    pub const SELECT_CUSTOM_PLAYER_FOR_CHAT: u8 = 0xa2;
    pub const RESIGN: u8 = 0xa3;
    pub const DISBAND: u8 = 0xa4;
    pub const SELF_DESTRUCT: u8 = 0xa5;
    pub const REPORT_STATISTICS: u8 = 0xa6;
    pub const CIV_TIMER_EXPIRED: u8 = 0xa7;
    pub const LAG_STATUS: u8 = 0xa8;
    pub const FULL_GAME_OPTIONS: u8 = 0xa9;
    pub const PLAYER_INFORMATION: u8 = 0xaa;
    pub const PACK_UNPACK: u8 = 0xab;
    pub const GO_FANATIC: u8 = 0xac;
    pub const JIT_MANUFACTURING: u8 = 0xad;
    pub const EXCHANGE_RESOURCES: u8 = 0xae;
}

/// A unit order: the units lane's body (ee_units' `ee_units_*`) with the message's members.
#[derive(Clone, Copy, Debug, PartialEq)]
pub enum Order {
    AddPatrolPoint { x: f32, y: f32 },
    AttackGround { x: f32, y: f32 },
    AttackUnit { target: u32, queued: bool, convert: bool },
    BuildUnit { technology: i32, x: i64, y: i64, queued: bool },
    BuildWall { technology: i32, start: (f32, f32), end: (f32, f32) },
    CastCalamity { caster: u32, target: i32, x: i32, y: i32, technology: i32 },
    ClearAirportCommands,
    DropoffResource { target: u32 },
    Explore,
    Gather { target: u32, queued: bool },
    HackNewUnit { unit_type: i32, x: f32, y: f32, player_id: i64 },
    HealUnit { target: u32 },
    LoadGarrison { target: u32 },
    LoadTransport { transport: u32, passenger: i32 },
    PathTo { x: f32, y: f32, facing: f32, forced: bool, rotating: bool, queued: bool, group_movement: bool },
    Patrol { target: i32 },
    Repair { target: u32, queued: bool },
    Research { building: u32, technology: i32, five: u8 },
    SetAirportCommand { x: f32, y: f32, command: i32, target: i32, queued: u8 },
    SetRallyGoal { building: u32, x: f32, y: f32, target: i32, queued: u8 },
    SmartFarmers { target: u32 },
    StopActiveGroup,
    UnloadTransport { x: i64, y: i64 },
}

/// What a unit order answers: a path's point as the order moved it, a hacked unit's class (`.0` as its bits).
pub type OrderResult = (f32, f32);

/// What the bodies ask of the world.
pub trait MsgHost {
    /// One world call.
    fn call(&mut self, op: u32, call: &mut EeNetMsgCall) -> i64;
    /// One unit order for the player and the selection (their addresses; the selection 0 for the orders that take
    /// none).
    fn order(&mut self, player: u64, group: u64, order: Order) -> OrderResult;
}

/// A record's members by wire index (ee_netmsg records.rs: `EEMRecord::Get<T>(i)`).
pub struct Record<'a> {
    message_type: u8,
    bytes: &'a mut [u8],
}

impl<'a> Record<'a> {
    pub fn new(message_type: u8, bytes: &'a mut [u8]) -> Self {
        Record { message_type, bytes }
    }

    fn at(&self, index: u32, size: usize) -> usize {
        let f = record_member(self.message_type, index);
        assert!(
            f.kind != EE_NETMSG_RECORD_FIELD_NONE && f.size as usize == size,
            "ee_netmsg: member {index} of {:#x} is not {size} bytes",
            self.message_type
        );
        f.offset as usize
    }

    fn get<const N: usize>(&self, index: u32) -> [u8; N] {
        let at = self.at(index, N);
        self.bytes[at..at + N].try_into().unwrap()
    }

    pub fn u8(&self, index: u32) -> u8 {
        self.get::<1>(index)[0]
    }
    pub fn bool(&self, index: u32) -> bool {
        self.u8(index) != 0
    }
    pub fn u16(&self, index: u32) -> u16 {
        u16::from_le_bytes(self.get(index))
    }
    pub fn i32(&self, index: u32) -> i32 {
        i32::from_le_bytes(self.get(index))
    }
    pub fn u32(&self, index: u32) -> u32 {
        u32::from_le_bytes(self.get(index))
    }
    pub fn f32(&self, index: u32) -> f32 {
        f32::from_le_bytes(self.get(index))
    }
    pub fn point(&self, index: u32) -> (f32, f32) {
        let b: [u8; 8] = self.get(index);
        (f32::from_le_bytes(b[..4].try_into().unwrap()), f32::from_le_bytes(b[4..].try_into().unwrap()))
    }
    fn set(&mut self, index: u32, value: &[u8]) {
        let at = self.at(index, value.len());
        self.bytes[at..at + value.len()].copy_from_slice(value);
    }
    pub fn set_i32(&mut self, index: u32, value: i32) {
        self.set(index, &value.to_le_bytes());
    }
    pub fn set_u32(&mut self, index: u32, value: u32) {
        self.set(index, &value.to_le_bytes());
    }
    pub fn set_f32(&mut self, index: u32, value: f32) {
        self.set(index, &value.to_le_bytes());
    }
}

/// The C++ `(long)` of a float: truncation, the conversion's "indefinite" value out of range (`long` is 32 bits on
/// Windows, 64 on LP64).
pub fn c_long(value: f32) -> i64 {
    if cfg!(windows) {
        if value > -2_147_483_904.0 && value < 2_147_483_648.0 {
            value as i64
        } else {
            i32::MIN as i64
        }
    } else if value > -9.223_373e18 && value < 9.223_372e18 {
        value as i64
    } else {
        i64::MIN
    }
}

/// The box selection's ids from its compressed bytes (`DecompressUnits`; `EESelectUnitsIDs`).
pub fn box_ids(list: &[u8]) -> Vec<i32> {
    let mut buffer = [0u8; 256];
    let n = list.len().min(256);
    buffer[..n].copy_from_slice(&list[..n]);
    let mut ids = Vec::new();
    units::decompress(&buffer, list.len() as u8, &mut ids);
    ids.truncate(0x100);
    ids
}

/// One body's context: the host, the message, its originating slot, its record, its list.
struct Body<'h, 'r, H: MsgHost + ?Sized> {
    h: &'h mut H,
    message: u64,
    slot: u8,
    m: Record<'r>,
    list: &'r [u8],
}

impl<H: MsgHost + ?Sized> Body<'_, '_, H> {
    fn op(&mut self, op: u32, a: u64, b: u64, i: &[i64], f: &[f32]) -> i64 {
        let mut c = EeNetMsgCall { message: self.message, a, b, ..Default::default() };
        c.i[..i.len()].copy_from_slice(i);
        c.f[..f.len()].copy_from_slice(f);
        self.h.call(op, &mut c)
    }
    fn obj(&mut self, op: u32, a: u64, i: &[i64]) -> u64 {
        self.op(op, a, 0, i, &[]) as u64
    }
    fn sender(&mut self) -> u64 {
        self.obj(EE_NETMSG_OP_SENDER, 0, &[])
    }
    fn player(&mut self, index: i64) -> u64 {
        self.obj(EE_NETMSG_OP_PLAYER, 0, &[index])
    }
    fn current(&mut self) -> u64 {
        self.obj(EE_NETMSG_OP_CURRENT, 0, &[])
    }
    fn group(&mut self, player: u64) -> u64 {
        self.obj(EE_NETMSG_OP_GROUP, player, &[])
    }
    fn count(&mut self, group: u64) -> i64 {
        self.op(EE_NETMSG_OP_GROUP_COUNT, group, 0, &[], &[])
    }
    fn unit_at(&mut self, group: u64, index: i64) -> u64 {
        self.obj(EE_NETMSG_OP_GROUP_UNIT, group, &[index])
    }
    fn first(&mut self, group: u64) -> u64 {
        self.obj(EE_NETMSG_OP_GROUP_FIRST, group, &[])
    }
    fn unit(&mut self, id: i64) -> u64 {
        self.obj(EE_NETMSG_OP_UNIT, 0, &[id])
    }
    fn fact(&mut self, unit: u64, fact: i64) -> i64 {
        self.op(EE_NETMSG_OP_UNIT_FACT, 0, unit, &[fact], &[])
    }
    fn hp(&mut self, unit: u64) -> i64 {
        self.fact(unit, EE_NETMSG_UNIT_HIT_POINTS)
    }
    fn unit_type(&mut self, unit: u64) -> i64 {
        self.fact(unit, EE_NETMSG_UNIT_TYPE)
    }
    fn complex(&mut self, unit: u64) -> u64 {
        self.fact(unit, EE_NETMSG_UNIT_COMPLEX) as u64
    }
    fn player_fact(&mut self, player: u64, fact: i64, i: &[i64]) -> i64 {
        let mut v = vec![fact];
        v.extend_from_slice(i);
        self.op(EE_NETMSG_OP_PLAYER_FACT, player, 0, &v, &[])
    }
    fn world(&mut self, fact: i64) -> i64 {
        self.op(EE_NETMSG_OP_WORLD_FACT, 0, 0, &[fact], &[])
    }
    fn act(&mut self, unit: u64, act: i64, other: u64, i: &[i64]) {
        let mut v = vec![act];
        v.extend_from_slice(i);
        self.op(EE_NETMSG_OP_UNIT_ACT, other, unit, &v, &[]);
    }
    fn pact(&mut self, player: u64, act: i64, i: &[i64]) {
        let mut v = vec![act];
        v.extend_from_slice(i);
        self.op(EE_NETMSG_OP_PLAYER_ACT, player, 0, &v, &[]);
    }
    fn wact(&mut self, act: i64, i: &[i64], f: &[f32]) {
        self.wact_on(act, 0, 0, i, f);
    }
    fn wact_on(&mut self, act: i64, a: u64, b: u64, i: &[i64], f: &[f32]) {
        let mut v = vec![act];
        v.extend_from_slice(i);
        self.op(EE_NETMSG_OP_WORLD_ACT, a, b, &v, f);
    }
    fn decision(&mut self, kind: i64, values: &[i64]) {
        let mut v = vec![kind, values.len() as i64];
        v.extend_from_slice(values);
        self.op(EE_NETMSG_OP_DECISION, 0, 0, &v, &[]);
    }
    fn chat_text(&mut self, text: i64, kind: i64, unless_empty: bool) {
        self.op(EE_NETMSG_OP_CHAT_TEXT, 0, 0, &[text, kind, unless_empty as i64], &[]);
    }
    fn sound(&mut self, sound: i64) {
        self.op(EE_NETMSG_OP_SOUND, 0, 0, &[sound], &[]);
    }
    fn body(&mut self, body: i64) {
        self.op(EE_NETMSG_OP_BODY, 0, 0, &[body], &[]);
    }
    /// The sender's order on its selection.
    fn order(&mut self, order: Order) -> OrderResult {
        let player = self.sender();
        let group = self.group(player);
        self.h.order(player, group, order)
    }

    /// The selection toggles' passes (twice while a selection is replayed), recorded.
    fn passes(&mut self) -> u32 {
        let executing = self.world(EE_NETMSG_WORLD_EXECUTING_SELECTION) as u8;
        let passes = decide::ee_netmsg_selection_passes(executing);
        self.decision(KIND_SELECTION_PASSES, &[executing as i64, passes as i64]);
        passes
    }

    fn select_list(&mut self, player: u64, which: i64, ids: &[i32]) {
        self.op(EE_NETMSG_OP_SELECT_LIST, player, ids.as_ptr() as u64, &[which, ids.len() as i64], &[]);
    }

    /// The selected group's units (re-counting each step when `recount`, as some original loops did).
    fn each_unit(&mut self, group: u64, recount: bool, mut f: impl FnMut(&mut Self, u64)) {
        let n = self.count(group);
        let mut i = 0;
        while i < if recount { self.count(group) } else { n } {
            let u = self.unit_at(group, i);
            f(self, u);
            i += 1;
        }
    }

    /// `ExecuteMessage`'s and the playback bodies' test: a recorded game playing back, the sender the local player.
    fn local_playback(&mut self) -> bool {
        if self.world(EE_NETMSG_WORLD_PLAYBACK) == 0 {
            return false;
        }
        let local = self.current();
        self.slot as i64 == self.player_fact(local, EE_NETMSG_PLAYER_SLOT, &[])
    }
}

fn is_granary(unit_type: i64) -> bool {
    unit_type == UNIT_TYPE_GRANARY_IMPROVED || unit_type == UNIT_TYPE_GRANARY
}

fn is_aircraft(category: i64) -> bool {
    category == CATEGORY_AIRPLANE || category == CATEGORY_PLANET_SPACESHIP
}

/// The two server tallies of the loading (the original's function statics: the slots that answered).
static LOAD_MAIN_SCREEN: Mutex<(Vec<u8>, u8)> = Mutex::new((Vec::new(), 0));
static START_LOADING: Mutex<Vec<u8>> = Mutex::new(Vec::new());

/// `EEMessage::ExecuteSelectionMessage` (0x4b8cf7, `selection`) and `ExecuteMessage` (0x4b8e86): the physics' two
/// ways to run a message. A selection runs under the fast-selection lock, marked local on a client for the local
/// sender, records its selection id for the sender and copies into the local selected group when its id is current;
/// the sender's JIT flag is cleared. A command runs when its selection id is the sender's last one (Game Restarting
/// always; on a client Migration Slot Info too), else the sender's selection is undone. Each decision recorded.
#[allow(clippy::too_many_arguments)]
pub fn execute_framed<H: MsgHost + ?Sized>(
    h: &mut H,
    message: u64,
    message_type: u8,
    slot: u8,
    fast_id: u8,
    record: &mut [u8],
    list: &[u8],
    selection: bool,
) {
    let mut b = Body { h, message, slot, m: Record::new(message_type, record), list };
    let p = b.sender();
    if p == 0 {
        return;
    }
    let server = b.world(EE_NETMSG_WORLD_IS_SERVER) as u8;
    let sel = |b: &mut Body<'_, '_, H>, what: i64, i: &[i64]| {
        let mut v = vec![what];
        v.extend_from_slice(i);
        b.op(EE_NETMSG_OP_SELECTION, p, 0, &v, &[])
    };
    if selection {
        let current = b.current();
        let local =
            (b.player_fact(p, EE_NETMSG_PLAYER_ID, &[]) == b.player_fact(current, EE_NETMSG_PLAYER_ID, &[])) as u8;
        sel(&mut b, EE_NETMSG_SEL_LOCK, &[1]);
        let marks = decide::ee_netmsg_selection_marks_local(server, local);
        b.decision(KIND_SELECTION_MARKS_LOCAL, &[server as i64, local as i64, marks as i64]);
        if marks != 0 {
            sel(&mut b, EE_NETMSG_SEL_MARK_LOCAL, &[1]);
        }
        let Body { h, m, list, .. } = &mut b;
        execute(*h, message, message_type, slot, m.bytes, list);
        if marks != 0 {
            sel(&mut b, EE_NETMSG_SEL_MARK_LOCAL, &[0]);
        }
        let current_id = sel(&mut b, EE_NETMSG_SEL_CURRENT_ID, &[]) as u8;
        let copies = decide::ee_netmsg_selection_copies(local, fast_id, current_id);
        b.decision(KIND_SELECTION_COPIES, &[local as i64, fast_id as i64, current_id as i64, copies as i64]);
        if copies != 0 {
            sel(&mut b, EE_NETMSG_SEL_COPY, &[]);
        }
        sel(&mut b, EE_NETMSG_SEL_SET_LAST_ID, &[fast_id as i64]);
        sel(&mut b, EE_NETMSG_SEL_LOCK, &[0]);
        sel(&mut b, EE_NETMSG_SEL_JIT_OFF, &[]);
    } else {
        sel(&mut b, EE_NETMSG_SEL_LOCK, &[1]);
        let last = sel(&mut b, EE_NETMSG_SEL_LAST_ID, &[]) as u8;
        let runs = decide::ee_netmsg_command_runs(server, message_type, fast_id, last);
        b.decision(KIND_COMMAND_RUNS, &[server as i64, message_type as i64, fast_id as i64, last as i64, runs as i64]);
        if runs == 0 {
            sel(&mut b, EE_NETMSG_SEL_RESET, &[]);
        } else {
            let Body { h, m, list, .. } = &mut b;
            execute(*h, message, message_type, slot, m.bytes, list);
        }
        sel(&mut b, EE_NETMSG_SEL_LOCK, &[0]);
    }
}

/// Runs the type's body (a type with none does nothing): `EEMRecord::Execute`.
pub fn execute<H: MsgHost + ?Sized>(
    h: &mut H,
    message: u64,
    message_type: u8,
    slot: u8,
    record: &mut [u8],
    list: &[u8],
) {
    let mut b = Body { h, message, slot, m: Record::new(message_type, record), list };
    if !selection(&mut b, message_type) && !orders(&mut b, message_type) && !units_bodies(&mut b, message_type) {
        players(&mut b, message_type);
    }
}

/// The selection's messages.
fn selection<H: MsgHost + ?Sized>(b: &mut Body<'_, '_, H>, message_type: u8) -> bool {
    match message_type {
        // EEMSelectUnits (0x4c606b), EEMAddUnitsToSelectedGroup (0x4bb44b): the box selection's units
        t::SELECT_UNITS | t::ADD_UNITS_TO_SELECTED_GROUP => {
            let p = b.sender();
            let ids = box_ids(b.list);
            b.select_list(p, if message_type == t::SELECT_UNITS { 2 } else { 1 }, &ids);
        }
        // EEMAddAndRemoveUnitsFromSelectedGroup (0x4badd0): toggles them (twice while a selection is replayed)
        t::ADD_AND_REMOVE_UNITS_FROM_SELECTED_GROUP => {
            let p = b.sender();
            let ids = box_ids(b.list);
            for _ in 0..b.passes() {
                b.select_list(p, 0, &ids);
            }
        }
        // EEMAddUnitToSelectedGroup (0x4bb2a2): toggles one unit (twice while a selection is replayed)
        t::ADD_UNIT_TO_SELECTED_GROUP => {
            let p = b.sender();
            let id = b.m.u32(1) as i64;
            let u = b.unit(id);
            if u != 0 {
                for _ in 0..b.passes() {
                    let g = b.group(p);
                    let c = b.complex(u);
                    let holds = b.op(EE_NETMSG_OP_GROUP_CONTAINS, g, c, &[], &[]) != 0;
                    b.op(EE_NETMSG_OP_SELECT, p, 0, &[if holds { 0 } else { 1 }, id], &[]);
                }
            }
        }
        // EEMAlwaysAddUnitToSelectedGroup (0x4bb479): adds the unit if it still exists (never removes it)
        t::ALWAYS_ADD_UNIT_TO_SELECTED_GROUP => {
            let p = b.sender();
            let id = b.m.u32(1) as i64;
            if b.unit(id) != 0 {
                b.op(EE_NETMSG_OP_SELECT, p, 0, &[1, id], &[]);
            }
        }
        // EEMRemoveUnitFromSelectedGroup (0x4c3d36), EEMSelectUnit (0x4c5c80), EEMUnselectAllUnits (0x4c889e)
        t::REMOVE_UNIT_FROM_SELECTED_GROUP | t::SELECT_UNIT | t::UNSELECT_ALL_UNITS => {
            let p = b.sender();
            let (which, id) = match message_type {
                t::REMOVE_UNIT_FROM_SELECTED_GROUP => (0, b.m.i32(1) as i64),
                t::SELECT_UNIT => (2, b.m.u32(1) as i64),
                _ => (3, 0),
            };
            b.op(EE_NETMSG_OP_SELECT, p, 0, &[which, id], &[]);
        }
        // EEMSelectTheseUnits (0x4c5aaa): the selection replaced by the listed units that still exist
        t::SELECT_THESE_UNITS => {
            let p = b.sender();
            let g = b.group(p);
            b.op(EE_NETMSG_OP_GROUP_CLEAR, g, 0, &[], &[]);
            let ids: Vec<i32> = b.list.as_chunks::<4>().0.iter().map(|c| i32::from_le_bytes(*c)).collect();
            for id in ids {
                // a listed unit may have died between sending and executing (it used to be read through NULL)
                let u = b.unit(id as i64);
                if u != 0 {
                    let c = b.complex(u);
                    b.op(EE_NETMSG_OP_GROUP_ADD, g, c, &[], &[]);
                }
            }
        }
        // EEMSavedGroupAddSelection (0x4c4fd1): the saved group's units the selection lacks
        t::SAVED_GROUP_ADD_SELECTION => {
            let p = b.sender();
            let saved = b.obj(EE_NETMSG_OP_SAVED_GROUP, p, &[b.m.u8(1) as i64]);
            let active = b.group(p);
            b.each_unit(saved, true, |b, u| {
                if u != 0 && b.op(EE_NETMSG_OP_GROUP_CONTAINS, active, u, &[], &[]) == 0 {
                    b.op(EE_NETMSG_OP_GROUP_ADD, active, u, &[], &[]);
                }
            });
        }
        // EEMSavedGroupClear (0x4c5117): a saved group with units emptied
        t::SAVED_GROUP_CLEAR => {
            let p = b.sender();
            let k = b.m.u8(1) as i64;
            let saved = b.obj(EE_NETMSG_OP_SAVED_GROUP, p, &[k]);
            if saved != 0 && b.count(saved) > 0 {
                b.pact(p, EE_NETMSG_PACT_CLEAR_SAVED, &[k]);
            }
        }
        // EEMSavedGroupCreate (0x4c522e): the selection saved (when anything is selected)
        t::SAVED_GROUP_CREATE => {
            let p = b.sender();
            let g = b.group(p);
            if b.count(g) > 0 {
                b.pact(p, EE_NETMSG_PACT_ASSIGN_SAVED, &[b.m.u8(1) as i64]);
            }
        }
        // EEMSelectPatrolGroup (0x4c587f)
        t::SELECT_PATROL_GROUP => {
            let p = b.sender();
            b.pact(p, EE_NETMSG_PACT_PATROL_GROUP, &[b.m.i32(1) as i64]);
        }
        _ => return false,
    }
    true
}

/// The unit orders (ee_units' bodies).
fn orders<H: MsgHost + ?Sized>(b: &mut Body<'_, '_, H>, message_type: u8) -> bool {
    let m = &b.m;
    let order = match message_type {
        t::ADD_PATROL_POINT => {
            let (x, y) = m.point(1);
            Order::AddPatrolPoint { x, y }
        }
        t::ATTACK_GROUND => Order::AttackGround { x: m.f32(1), y: m.f32(2) },
        t::ATTACK_UNIT => Order::AttackUnit { target: m.i32(1) as u32, queued: m.bool(2), convert: m.bool(3) },
        t::BUILD_UNIT => {
            // the tile: the point's (long) casts, as the original's
            let (x, y) = m.point(1);
            Order::BuildUnit { technology: m.i32(2), x: c_long(x), y: c_long(y), queued: m.u8(3) != 0 }
        }
        t::CAST_CALAMITY => Order::CastCalamity {
            caster: m.i32(1) as u32,
            target: m.i32(2),
            x: m.i32(3),
            y: m.i32(4),
            technology: m.i32(5),
        },
        t::CLEAR_AIRPORT_COMMANDS => Order::ClearAirportCommands,
        t::DROPOFF_RESOURCE => Order::DropoffResource { target: m.i32(1) as u32 },
        t::EXPLORE => Order::Explore,
        t::GATHER_RESOURCE => Order::Gather { target: m.i32(1) as u32, queued: m.bool(2) },
        t::HEAL_UNIT => Order::HealUnit { target: m.i32(1) as u32 },
        t::LOAD_GARRISON => Order::LoadGarrison { target: m.i32(1) as u32 },
        t::LOAD_TRANSPORT => Order::LoadTransport { transport: m.i32(1) as u32, passenger: m.i32(2) },
        t::PATH_TO => Order::PathTo {
            facing: m.f32(1),
            x: m.f32(2),
            y: m.f32(3),
            forced: m.bool(4),
            rotating: m.bool(5),
            queued: m.bool(6),
            group_movement: m.bool(7),
        },
        t::PATROL => Order::Patrol { target: m.i32(1) },
        t::REPAIR_UNIT => Order::Repair { target: m.i32(1) as u32, queued: m.bool(2) },
        t::RESEARCH_TECHNOLOGY => {
            Order::Research { building: m.i32(1) as u32, technology: m.i32(2), five: m.bool(3) as u8 }
        }
        t::SET_AIRPORT_COMMAND => {
            let (x, y) = m.point(1);
            Order::SetAirportCommand { x, y, command: m.i32(2), target: m.i32(3), queued: m.u8(4) }
        }
        t::SMART_FARMERS => Order::SmartFarmers { target: m.i32(1) as u32 },
        t::STOP_ACTIVE_GROUP => Order::StopActiveGroup,
        t::UNLOAD_TRANSPORT => Order::UnloadTransport { x: m.i32(2) as i64, y: m.i32(3) as i64 },
        // the wall: the message's player's selection
        t::BUILD_WALL => {
            let o = Order::BuildWall { technology: m.i32(3), start: m.point(1), end: m.point(2) };
            let p = b.player(b.m.i32(4) as i64);
            let g = b.group(p);
            b.h.order(p, g, o);
            return true;
        }
        // the rally goal and the cheat's unit: the sender alone
        t::SET_RALLY_GOAL => {
            let (x, y) = m.point(1);
            let o = Order::SetRallyGoal { building: m.i32(3) as u32, x, y, target: m.i32(2), queued: m.u8(4) };
            let p = b.sender();
            b.h.order(p, 0, o);
            return true;
        }
        t::HACK_A_NEW_UNIT => {
            // the message keeps the class made
            let (x, y) = m.point(2);
            let o = Order::HackNewUnit { unit_type: m.i32(1), x, y, player_id: m.u8(3) as i64 };
            let p = b.sender();
            let (made, _) = b.h.order(p, 0, o);
            b.m.set_i32(1, made.to_bits() as i32);
            return true;
        }
        _ => return false,
    };
    let (x, y) = b.order(order);
    if message_type == t::PATH_TO {
        // the Rust may move the point: the record keeps it
        b.m.set_f32(2, x);
        b.m.set_f32(3, y);
    }
    true
}

/// The bodies over the sender's selected units.
fn units_bodies<H: MsgHost + ?Sized>(b: &mut Body<'_, '_, H>, message_type: u8) -> bool {
    match message_type {
        // EEMDisband (0x4bd82a): the living, disbandable units refunded 75% of their cost scaled by health, killed;
        // the cash register for the local player
        t::DISBAND => {
            let p = b.sender();
            let g = b.group(p);
            if b.first(g) != 0 {
                b.each_unit(g, false, |b, u| {
                    let hp = b.hp(u);
                    let max = if hp > 0 { b.fact(u, EE_NETMSG_UNIT_MAX_HIT_POINTS) } else { 1 };
                    let can = hp > 0 && b.fact(u, EE_NETMSG_UNIT_CAN_DISBAND) != 0;
                    let mut fraction = 0.0f32;
                    // SAFETY: a local
                    let refund = unsafe { decide::ee_netmsg_disband_refund(hp, max, can as u8, &mut fraction) };
                    b.decision(KIND_DISBAND_REFUND, &[hp, max, can as i64, refund as i64, fraction.to_bits() as i64]);
                    if refund != 0 {
                        b.op(EE_NETMSG_OP_DISBAND, p, u, &[], &[fraction]);
                    }
                });
                if p == b.current() {
                    b.sound(EE_NETMSG_SOUND_CASH_REGISTER);
                }
            }
        }
        // EEMGoFanatic (0x4bf3ba): the living units of unit set 0x2f, last first
        t::GO_FANATIC => {
            let p = b.sender();
            let g = b.group(p);
            let mut i = b.count(g) - 1;
            while i >= 0 {
                let u = b.unit_at(g, i);
                if b.op(EE_NETMSG_OP_UNIT_FACT, 0, u, &[EE_NETMSG_UNIT_IN_SET, UNIT_SET_FANATICS], &[]) != 0
                    && b.hp(u) > 0
                {
                    b.act(u, EE_NETMSG_ACT_GO_FANATIC, 0, &[]);
                }
                i -= 1;
            }
        }
        // EEMHaltSelectedGroup (0x4bf8c8): every unit's goals cleared, idle
        t::HALT_SELECTED_GROUP => {
            let p = b.sender();
            let g = b.group(p);
            b.each_unit(g, false, |b, u| b.act(u, EE_NETMSG_ACT_HALT, 0, &[]));
        }
        // EEMIncrementRallyTimer (0x4c0170): 60 s more on the queue of every selected airport that holds goals
        t::INCREMENT_RALLY_TIMER => {
            let p = b.sender();
            let g = b.group(p);
            let queue = b.m.u8(1) as i64;
            b.each_unit(g, true, |b, u| {
                if u != 0 && b.unit_type(u) == UNIT_TYPE_AIRPORT {
                    b.act(u, EE_NETMSG_ACT_RALLY_TIME, 0, &[queue]);
                }
            });
        }
        // EEMJITManufacturing (0x4c036f): JIT toggled on the living units and on the player
        t::JIT_MANUFACTURING => {
            let p = b.sender();
            let g = b.group(p);
            b.each_unit(g, false, |b, u| {
                let c = b.complex(u);
                if b.hp(u) > 0 && c != 0 {
                    b.act(c, EE_NETMSG_ACT_JIT, 0, &[]);
                }
            });
            b.pact(p, EE_NETMSG_PACT_JIT, &[]);
        }
        // EEMKillUnit (0x4c051c): the first unit (or every unit) that may be deleted, unfinished buildings refunded
        t::KILL_UNIT => {
            let p = b.sender();
            let g = b.group(p);
            if g != 0 {
                if b.m.u8(1) == 0 {
                    let f = b.first(g);
                    let u = if f != 0 { b.fact(f, EE_NETMSG_UNIT_AS_COMPLEX) as u64 } else { 0 };
                    delete_unit(b, u);
                } else {
                    b.each_unit(g, false, |b, u| delete_unit(b, u));
                }
            }
        }
        // EEMLockUnlock (0x4c1fb6): every selected gate (with its second and fourth parts) locked or unlocked
        t::LOCK_UNLOCK => {
            let p = b.sender();
            let g = b.group(p);
            let lock = b.m.bool(1) as i64;
            if b.first(g) != 0 {
                b.each_unit(g, false, |b, u| b.act(u, EE_NETMSG_ACT_LOCK_GATE, 0, &[lock]));
            }
        }
        // EEMPackUnpack (0x4c2683): the living units replaced by their packed or unpacked class; a unit with no
        // room to unpack stays packed, and the local player is told
        t::PACK_UNPACK => {
            let p = b.sender();
            let g = b.group(p);
            let pack = b.m.bool(1);
            let mut no_room = false;
            if b.first(g) != 0 {
                let id = b.player_fact(p, EE_NETMSG_PLAYER_ID, &[]);
                b.each_unit(g, false, |b, u| {
                    if b.hp(u) <= 0 {
                        return;
                    }
                    let packed = b.fact(u, EE_NETMSG_UNIT_IS_PACKED) != 0;
                    if pack && !packed {
                        let class = b.fact(u, EE_NETMSG_UNIT_PACKED_CLASS) as u64;
                        if class != 0 {
                            b.act(u, EE_NETMSG_ACT_PACK, class, &[id]);
                        }
                    } else if !pack && packed {
                        let class = b.fact(u, EE_NETMSG_UNIT_PACKED_CLASS) as u64;
                        if class != 0 {
                            let (x, y) = (b.fact(u, EE_NETMSG_UNIT_TILE_X), b.fact(u, EE_NETMSG_UNIT_TILE_Y));
                            if can_unpack(b, class, p, x, y) {
                                b.act(u, EE_NETMSG_ACT_UNPACK, class, &[id, p as i64]);
                            } else {
                                no_room = true;
                            }
                        }
                    }
                });
            }
            if no_room && p == b.current() {
                b.chat_text(EE_NETMSG_TEXT_MAY_NOT_UNPACK, 0, true);
            }
        }
        // EEMRemoveImprovement (0x4c45b8): the improvement out of the queue of every living selected building of
        // the clicked unit's type
        t::REMOVE_IMPROVEMENT => {
            let p = b.sender();
            let g = b.group(p);
            let u = b.unit(b.m.i32(1) as i64);
            if u != 0 && b.hp(u) > 0 {
                let kind = b.unit_type(u);
                let improvement = b.m.i32(2) as i64;
                b.each_unit(g, false, |b, o| {
                    let other = b.unit_type(o);
                    if b.hp(o) > 0 && other == kind && b.complex(o) != 0 {
                        b.pact(p, EE_NETMSG_PACT_REMOVE_IMPROVEMENT, &[other, improvement]);
                    }
                });
            }
        }
        // EEMRemoveTechnology (0x4c4766): the research cancelled (five times if asked) or JIT toggled at each
        // living selected building of the clicked building's type; the JIT button refreshed for the local player
        t::REMOVE_TECHNOLOGY => {
            let p = b.sender();
            let g = b.group(p);
            let u = b.unit(b.m.i32(1) as i64);
            let (technology, jit, five) = (b.m.i32(2) as i64, b.m.bool(3), b.m.bool(4));
            if u != 0 && b.hp(u) > 0 {
                let kind = b.unit_type(u);
                b.each_unit(g, false, |b, o| {
                    let c = b.complex(o);
                    if b.hp(o) > 0 && b.unit_type(o) == kind && c != 0 {
                        if five {
                            for _ in 0..5 {
                                b.act(c, EE_NETMSG_ACT_REMOVE_RESEARCH, 0, &[technology]);
                            }
                        } else if jit {
                            b.act(c, EE_NETMSG_ACT_JIT_TOGGLE, 0, &[]);
                        } else {
                            b.act(c, EE_NETMSG_ACT_REMOVE_RESEARCH, 0, &[technology]);
                        }
                        b.pact(p, EE_NETMSG_PACT_UPDATE_JIT, &[]);
                    }
                });
            }
        }
        // EEMReplantFarms (0x4c41da): the granary replanted (-1: every selected granary, when the first is one)
        t::REPLANT_FARMS => {
            let id = b.m.i32(1);
            if id == -1 {
                let p = b.sender();
                let g = b.group(p);
                let f = b.first(g);
                if f != 0 && is_granary(b.unit_type(f)) {
                    b.each_unit(g, false, |b, u| {
                        if is_granary(b.unit_type(u)) {
                            b.act(u, EE_NETMSG_ACT_REPLANT, p, &[]);
                        }
                    });
                    b.wact(EE_NETMSG_WACT_FLUSH, &[], &[]);
                }
            } else {
                let u = b.unit(id as i64);
                if u != 0 && b.fact(u, EE_NETMSG_UNIT_TILE_NODE) != 0 {
                    let farm = b.complex(u);
                    if is_granary(b.unit_type(farm)) {
                        let owner = b.fact(farm, EE_NETMSG_UNIT_OWNER) as u64;
                        b.act(farm, EE_NETMSG_ACT_REPLANT, owner, &[]);
                        b.wact(EE_NETMSG_WACT_FLUSH, &[], &[]);
                    }
                }
            }
        }
        // EEMReturnToBase (0x4c4e02): the target the home base of the selected aircraft in flight (their flight
        // time reset unless kept); the last one acknowledges
        t::RETURN_TO_BASE => {
            let p = b.sender();
            let g = b.group(p);
            let base = b.unit(b.m.i32(1) as i64);
            let keep = b.m.bool(2);
            if base != 0 {
                let base = b.complex(base);
                if base != 0 {
                    let mut last = 0;
                    b.each_unit(g, true, |b, u| {
                        if u != 0
                            && is_aircraft(b.fact(u, EE_NETMSG_UNIT_CATEGORY))
                            && b.fact(u, EE_NETMSG_UNIT_FLIGHT_TIME) > 0
                        {
                            b.act(u, EE_NETMSG_ACT_HOME_BASE, base, &[]);
                            if !keep {
                                b.act(u, EE_NETMSG_ACT_LAND, 0, &[]);
                            }
                            last = u;
                        }
                    });
                    if last != 0 {
                        b.pact(p, EE_NETMSG_PACT_ACKNOWLEDGE, &[last as i64]);
                    }
                }
            }
        }
        // EEMScrambleAircraft (0x4c5587): an airplane sent to the point
        t::SCRAMBLE_AIRCRAFT => {
            let u = b.unit(b.m.i32(3) as i64);
            if u != 0 {
                let a = b.complex(u);
                if a != 0 && is_aircraft(b.fact(a, EE_NETMSG_UNIT_CATEGORY)) {
                    let (x, y) = (b.m.i32(1) as i64, b.m.i32(2) as i64);
                    b.act(a, EE_NETMSG_ACT_SCRAMBLE, 0, &[x, y]);
                }
            }
        }
        // EEMSelfDestruct (0x4c626c): the living units with a death-spawn class killed; the first acknowledges
        t::SELF_DESTRUCT => {
            let p = b.sender();
            let g = b.group(p);
            if b.first(g) != 0 {
                b.each_unit(g, false, |b, u| {
                    if b.hp(u) > 0 && b.fact(u, EE_NETMSG_UNIT_DEATH_SPAWN) != 0 {
                        b.op(EE_NETMSG_OP_KILL, 0, u, &[1], &[]);
                    }
                });
                let f = b.first(g);
                b.pact(p, EE_NETMSG_PACT_ACKNOWLEDGE, &[f as i64]);
            }
        }
        // EEMSetUnitBehavior (0x4c8df1): the stance of the units that can change it
        t::SET_UNIT_BEHAVIOR => {
            let p = b.sender();
            let g = b.group(p);
            let behavior = b.m.i32(1) as i64;
            b.each_unit(g, true, |b, u| {
                if b.fact(u, EE_NETMSG_UNIT_CAN_CHANGE_BEHAVIOR) != 0 {
                    b.act(u, EE_NETMSG_ACT_BEHAVIOR, 0, &[behavior]);
                }
            });
        }
        // EEMChangeSelectedUnitCollisionSphere (0x4bcb62), debug: the first unit's class's inner or outer sphere
        // (collision sphere mode 9/10) grown or shrunk by 0.05, its radius refreshed; for a sea unit the other units
        // of its type take the radius too
        t::CHANGE_SELECTED_UNIT_COLLISION_SPHERE => {
            let p = b.sender();
            let g = b.group(p);
            if g == 0 {
                return true;
            }
            let u = b.first(g);
            if u == 0 {
                return true;
            }
            let mut others = Vec::new();
            if b.fact(u, EE_NETMSG_UNIT_DOMAINS) & MOVEMENT_SEA != 0 {
                let kind = b.unit_type(u);
                for i in 1..b.world(EE_NETMSG_WORLD_NUM_PLAYERS) {
                    let pl = b.player(i);
                    if pl == 0 {
                        continue;
                    }
                    let mut o = b.player_fact(pl, EE_NETMSG_PLAYER_FIRST_UNIT, &[]) as u64;
                    while o != 0 {
                        let c = b.complex(o);
                        if c != 0 && b.unit_type(o) == kind {
                            others.push(c);
                        }
                        o = b.fact(o, EE_NETMSG_UNIT_NEXT) as u64;
                    }
                }
            }
            let mode = b.world(EE_NETMSG_WORLD_COLLISION_SPHERE_MODE);
            if mode != 9 && mode != 10 {
                return true;
            }
            b.act(u, EE_NETMSG_ACT_SPHERE, 0, &[mode, b.m.bool(1) as i64]);
            for o in others {
                b.act(o, EE_NETMSG_ACT_RADIUS, u, &[]);
            }
        }
        _ => return false,
    }
    true
}

/// `EEMKillUnit`'s test for one unit: alive (or a path point) and deletable; an unfinished, paid-for land building
/// that is not a granary refunded first (`IsRefundable` 0x4c04d0).
fn delete_unit<H: MsgHost + ?Sized>(b: &mut Body<'_, '_, H>, u: u64) {
    if u == 0 || !(b.unit_type(u) == UNIT_TYPE_PATH_POINT || b.hp(u) > 0) || b.fact(u, EE_NETMSG_UNIT_CAN_DELETE) == 0 {
        return;
    }
    let c = b.complex(u);
    let refundable = c != 0
        && b.fact(c, EE_NETMSG_UNIT_LAND_BUILDING) != 0
        && b.fact(c, EE_NETMSG_UNIT_PAID_FOR) != 0
        && {
            let cc = b.complex(c);
            b.fact(cc, EE_NETMSG_UNIT_BUILT) == 0
        }
        && !is_granary(b.unit_type(c));
    if refundable {
        b.op(EE_NETMSG_OP_DELETE_REFUND, 0, u, &[], &[]);
    }
    b.op(EE_NETMSG_OP_KILL, 0, u, &[1], &[]);
}

/// `CanUnpack` (0x4c259d): the class fits for the player on every tile of its footprint around (x, y).
fn can_unpack<H: MsgHost + ?Sized>(b: &mut Body<'_, '_, H>, class: u64, player: u64, x: i64, y: i64) -> bool {
    let kind = b.op(EE_NETMSG_OP_CLASS_FACT, class, 0, &[EE_NETMSG_CLASS_TYPE], &[]);
    let mut size = b.op(EE_NETMSG_OP_CLASS_FACT, class, 0, &[EE_NETMSG_CLASS_TILE_SIZE], &[]);
    let (start_x, start_y);
    if size <= 1 {
        (start_x, start_y) = (x, y);
        size = 1;
    } else {
        let half = c_long(size as f32 * 0.5);
        (start_x, start_y) = (x - half, y - half);
    }
    let (end_x, end_y) = (start_x + size, start_y + size);
    let mut fits = true;
    for tx in start_x..=end_x {
        for ty in start_y..=end_y {
            let ok = b.op(EE_NETMSG_OP_CAN_PLACE, player, 0, &[kind, x, y, tx, ty], &[]) != 0;
            if tx < end_x && ty < end_y && !ok {
                fits = false;
            }
        }
    }
    fits
}

/// The players', the lobby's and the world's bodies.
fn players<H: MsgHost + ?Sized>(b: &mut Body<'_, '_, H>, message_type: u8) {
    match message_type {
        // EEMAddRemoveResources (0x4bb264): one resource given, or taken if positive and no more than the stock
        t::ADD_REMOVE_RESOURCES => {
            let p = b.player(b.m.i32(2) as i64);
            let (amount, resource, add) = (b.m.u32(1), b.m.i32(3) as i64, b.m.u8(4));
            // the take's test compares unsigned, as on Windows (the decision); an add reads nothing
            let have = if add != 0 { 0 } else { b.player_fact(p, EE_NETMSG_PLAYER_RESOURCE, &[resource]) };
            let decision = decide::ee_netmsg_add_remove(add, amount, have);
            b.decision(KIND_ADD_REMOVE, &[add as i64, amount as i64, have, decision as i64]);
            if decision == 1 || decision == 2 {
                b.op(EE_NETMSG_OP_RESOURCE, p, 0, &[decision as i64 - 1, resource, amount as i64], &[]);
            }
        }
        // EEMExchangeResources (0x4be3b1): 100 of a resource sold for gold or bought with gold at the market
        // price, once or five times (partly when short), the traded amount tallied; the help refreshed
        t::EXCHANGE_RESOURCES => {
            let p = b.sender();
            let (sell, resource) = (b.m.u8(1), b.m.i32(2) as i64);
            let count = if b.m.u8(3) != 0 { 5 } else { 1 };
            for _ in 0..count {
                let price = b.player_fact(p, EE_NETMSG_PLAYER_TRADE_PRICE, &[resource, sell as i64]);
                let have =
                    b.player_fact(p, EE_NETMSG_PLAYER_RESOURCE, &[if sell != 0 { resource } else { RESOURCE_GOLD }]);
                let s = decide::ee_netmsg_exchange_step(sell, price, have);
                b.decision(KIND_EXCHANGE_STEP, &[sell as i64, price, have, s.apply as i64, s.cost, s.amount]);
                if s.apply != 0 {
                    let (cost, amount) = (s.cost, s.amount);
                    if sell != 0 {
                        b.op(EE_NETMSG_OP_RESOURCE, p, 0, &[1, resource, cost], &[]);
                        b.op(EE_NETMSG_OP_RESOURCE, p, 0, &[0, RESOURCE_GOLD, amount], &[]);
                        b.op(EE_NETMSG_OP_RESOURCE, p, 0, &[2, resource, cost.wrapping_neg()], &[]);
                    } else {
                        b.op(EE_NETMSG_OP_RESOURCE, p, 0, &[0, resource, cost], &[]);
                        b.op(EE_NETMSG_OP_RESOURCE, p, 0, &[1, RESOURCE_GOLD, amount], &[]);
                        b.op(EE_NETMSG_OP_RESOURCE, p, 0, &[2, resource, cost], &[]);
                    }
                }
            }
            b.pact(p, EE_NETMSG_PACT_UPDATE_HELP, &[]);
        }
        // EEMPayTribute (0x4c315b): the resources given and tallied for both; the giver and the receiver get a chat
        // line listing the amounts
        t::PAY_TRIBUTE => {
            let from = b.sender();
            let to = b.player(b.m.u8(6) as i64);
            let m = &b.m;
            let amounts = [0, 0, m.u32(1), m.u32(5), m.u32(4), m.u32(2), m.u32(3)];
            let order = tribute_order(b, &amounts, false);
            for &r in &order {
                let amount = amounts[r as usize] as i64;
                b.op(EE_NETMSG_OP_RESOURCE, from, 0, &[3, r as i64, amount, 0], &[]);
                b.op(EE_NETMSG_OP_RESOURCE, to, 0, &[0, r as i64, amount], &[]);
                b.op(EE_NETMSG_OP_RESOURCE, to, 0, &[3, r as i64, amount, 1], &[]);
            }
            let local = b.current();
            let viewer = decide::ee_netmsg_tribute_viewer((local == from) as u8, (local == to) as u8);
            b.decision(KIND_TRIBUTE_VIEWER, &[(local == from) as i64, (local == to) as i64, viewer as i64]);
            if viewer == 1 || viewer == 2 {
                let order = tribute_order(b, &amounts, true);
                let mut i =
                    vec![viewer as i64, order.len() as i64, order.iter().rev().fold(0i64, |v, &r| v << 8 | r as i64)];
                i.extend(amounts[2..].iter().map(|&a| a as i64));
                b.wact_on(EE_NETMSG_WACT_TRIBUTE_TEXT, from, to, &i, &[]);
            }
        }
        // EEMCheatCode (0x4bced6), EEMIncrementEpic (0x4c0150), EEMMakeWorldVisible (0x4c212b), the shift mode
        // (0x4be076, 0x4be185)
        t::CHEAT_CODE => {
            let p = b.sender();
            b.pact(p, EE_NETMSG_PACT_CHEAT, &[b.m.i32(1) as i64, b.m.i32(2) as i64]);
        }
        t::INCREMENT_EPIC => {
            let p = b.sender();
            b.pact(p, EE_NETMSG_PACT_INCREMENT_EPIC, &[]);
        }
        t::MAKE_WORLD_VISIBLE => {
            let p = b.sender();
            b.pact(p, EE_NETMSG_PACT_SEES_ALL, &[]);
        }
        t::ENTER_SHIFT_MODE | t::EXIT_SHIFT_MODE => {
            let p = b.sender();
            b.pact(p, EE_NETMSG_PACT_SHIFT_MODE, &[(message_type == t::ENTER_SHIFT_MODE) as i64]);
        }
        // EEMReportStatistics (0x4c42c8), the chat settings (0x4c5755, 0x4c59ac): a sender that is a player
        t::REPORT_STATISTICS | t::SELECT_CUSTOM_PLAYER_FOR_CHAT | t::SELECT_PLAYER_CHAT_TYPE => {
            let p = b.sender();
            if p != 0 {
                let (act, i) = match message_type {
                    t::REPORT_STATISTICS => (EE_NETMSG_PACT_REPORT, [b.m.i32(1) as i64, b.m.i32(2) as i64]),
                    t::SELECT_CUSTOM_PLAYER_FOR_CHAT => {
                        (EE_NETMSG_PACT_SEND_CHAT, [b.m.i32(1) as i64, b.m.bool(2) as i64])
                    }
                    _ => (EE_NETMSG_PACT_CHAT_TYPE, [b.m.i32(1) as i64, 0]),
                };
                b.pact(p, act, &i);
            }
        }
        // EEMResign (0x4c44bb): the sender out as resigned (observers excepted); the others see it in the chat
        t::RESIGN => {
            let p = b.sender();
            if b.player_fact(p, EE_NETMSG_PLAYER_OBSERVER, &[]) == 0 {
                b.pact(p, EE_NETMSG_PACT_REMOVE, &[]);
                if p != b.current() {
                    b.wact_on(EE_NETMSG_WACT_RESIGN_TEXT, p, 0, &[], &[]);
                }
            }
        }
        // EEMCivTimerExpired (0x4bd008): for a local player still choosing: the warning (a clock tick), or the end
        // of the choice (the timer-expired event; every other player's civilization locked)
        t::CIV_TIMER_EXPIRED => {
            let local = b.current();
            if b.player_fact(local, EE_NETMSG_PLAYER_CIV_CHOSEN, &[]) == 0
                && b.player_fact(local, EE_NETMSG_PLAYER_OBSERVER, &[]) == 0
            {
                if b.m.u8(1) != 0 {
                    b.sound(EE_NETMSG_SOUND_CLOCK_TICK);
                    b.chat_text(EE_NETMSG_TEXT_RUNNING_OUT, 0, false);
                } else {
                    b.wact(EE_NETMSG_WACT_TIMER_EXPIRED, &[], &[]);
                    b.chat_text(EE_NETMSG_TEXT_SELECTION_EXPIRED, 0, false);
                    let current = b.world(EE_NETMSG_WORLD_CURRENT_INDEX);
                    for i in 1..b.world(EE_NETMSG_WORLD_NUM_PLAYERS) {
                        if i != current {
                            let pl = b.player(i);
                            b.pact(pl, EE_NETMSG_PACT_LOCK_CIV, &[]);
                        }
                    }
                }
            }
        }
        // EEMChat (0x4bcd63): a non-empty text posted to the chat log, as our own when it came from our slot
        t::CHAT => {
            if b.world(EE_NETMSG_WORLD_HAS_COMMUNICATIONS) != 0 && b.world(EE_NETMSG_WORLD_STRING_LENGTH) > 0 {
                let local = b.world(EE_NETMSG_WORLD_LOCAL_SEND_SLOT);
                b.wact(EE_NETMSG_WACT_POST_CHAT, &[(local == b.slot as i64) as i64], &[]);
            }
        }
        // EEMInGameChat (0x4bfdd2): the chat line shown if the sender's chat target (all, allies, enemies, chosen
        // players) takes the local player and observers reach only observers; the game log and the scenario's last
        // chat line take it
        t::IN_GAME_CHAT => {
            let s = b.sender();
            let l = b.current();
            if s != 0 {
                b.wact_on(EE_NETMSG_WACT_CHAT_LOG, s, 0, &[], &[]);
            }
            if s != 0 && l != 0 {
                let local_id = b.player_fact(l, EE_NETMSG_PLAYER_ID, &[]);
                let kind = b.player_fact(s, EE_NETMSG_PLAYER_CHAT_TYPE, &[]);
                let reaches = l == s
                    || (b.player_fact(s, EE_NETMSG_PLAYER_SLOT, &[]) != SLOT_NONE
                        && (kind == 0
                            || (kind == 1 && b.player_fact(s, EE_NETMSG_PLAYER_ENEMY, &[local_id]) == 0)
                            || (kind == 2 && b.player_fact(s, EE_NETMSG_PLAYER_ENEMY, &[local_id]) != 0)
                            || (kind == 3 && b.player_fact(s, EE_NETMSG_PLAYER_SENDS_CHAT, &[local_id]) != 0)));
                if reaches {
                    let observer = b.player_fact(s, EE_NETMSG_PLAYER_OBSERVER, &[]) != 0;
                    if !observer || b.player_fact(l, EE_NETMSG_PLAYER_OBSERVER, &[]) != 0 {
                        let text = match kind {
                            3 => EE_NETMSG_TEXT_SETUP_CUSTOM,
                            2 => EE_NETMSG_TEXT_SETUP_ENEMIES,
                            1 => EE_NETMSG_TEXT_SETUP_ALLIES,
                            0 => EE_NETMSG_TEXT_SETUP_ALL,
                            _ => EE_NETMSG_TEXT_NONE,
                        };
                        let observers = if observer { EE_NETMSG_TEXT_OBSERVERS } else { EE_NETMSG_TEXT_NONE };
                        b.wact_on(EE_NETMSG_WACT_SHOW_CHAT, s, l, &[text, observers], &[]);
                    }
                }
                b.wact(EE_NETMSG_WACT_LAST_CHAT, &[], &[]);
            }
        }
        // the lobby: EEMGameOptions (0x4bec38) one option, EEMFullGameOptions (0x4be7b7) all but the B53 flag;
        // EEMPlayerInformation (0x4c3589) and EEMUpdatePingStat (0x4c8972) a slot's field
        t::GAME_OPTIONS => {
            b.wact(EE_NETMSG_WACT_OPTIONS_LOCK, &[1], &[]);
            let (k, v) = (b.m.i32(1) as i64, b.m.u32(2) as i64);
            match k {
                0..=10 => b.op(EE_NETMSG_OP_OPTION, 0, 0, &[k, v], &[]),
                11..=17 => b.op(EE_NETMSG_OP_OPTION, 0, 0, &[k, (v > 0) as i64], &[]),
                _ => 0,
            };
            b.wact(EE_NETMSG_WACT_OPTIONS_LOCK, &[0], &[]);
        }
        t::FULL_GAME_OPTIONS => {
            b.wact(EE_NETMSG_WACT_OPTIONS_LOCK, &[1], &[]);
            // (option, member, a bool): the original's order
            const FULL: [(i64, u32, bool); 18] = [
                (0, 7, false),
                (2, 0, false),
                (1, 11, false),
                (3, 13, false),
                (4, 15, false),
                (5, 5, false),
                (6, 16, false),
                (7, 18, false),
                (8, 8, false),
                (9, 4, false),
                (10, 6, false),
                (11, 12, true),
                (12, 1, true),
                (13, 14, true),
                (14, 10, true),
                (15, 9, true),
                (16, 3, true),
                (17, 17, true),
            ];
            for (k, member, flag) in FULL {
                let v = match (member, flag) {
                    (0, _) => 0,
                    (_, true) => b.m.bool(member) as i64,
                    _ => b.m.i32(member) as i64,
                };
                b.op(EE_NETMSG_OP_OPTION, 0, 0, &[k, v], &[]);
            }
            b.wact(EE_NETMSG_WACT_OPTIONS_LOCK, &[0], &[]);
        }
        t::PLAYER_INFORMATION => {
            let (slot, field, value) = (b.m.i32(4) as i64, b.m.i32(2) as i64, b.m.u32(1) as i64);
            b.wact(EE_NETMSG_WACT_PLAYER_INFO, &[slot, field, value], &[b.m.f32(3)]);
        }
        t::UPDATE_PING_STAT => {
            b.wact(EE_NETMSG_WACT_PLAYER_INFO, &[b.m.u8(1) as i64, 1, 0], &[b.m.f32(2)]);
        }
        // the loading: EEMLoadMainScreenResponse (0x4c0b60) and EEMStartLoadingResponse (0x4c7e84) count each slot's
        // first answer on the server; when all have answered the game starts, or every player shows the main screen
        t::LOAD_MAIN_SCREEN_RESPONSE => {
            if b.world(EE_NETMSG_WORLD_IS_SERVER) == 0 {
                return;
            }
            let slot = b.m.u8(1);
            let start = {
                let mut s = LOAD_MAIN_SCREEN.lock().unwrap();
                if s.1 == 0 {
                    s.0.clear();
                }
                if s.0.contains(&slot) {
                    return;
                }
                s.0.push(slot);
                if b.m.u8(2) != 0xff {
                    s.1 = s.1.wrapping_add(1);
                }
                s.1
            };
            if start as i64 == b.world(EE_NETMSG_WORLD_LOADED_HUMANS) {
                b.wact(EE_NETMSG_WACT_START_GAME, &[], &[]);
                LOAD_MAIN_SCREEN.lock().unwrap().1 = 0;
            }
        }
        t::START_LOADING_RESPONSE => {
            if b.world(EE_NETMSG_WORLD_IS_SERVER) == 0 {
                return;
            }
            let n = b.world(EE_NETMSG_WORLD_LOAD_RESPONSES);
            if n == 0 {
                START_LOADING.lock().unwrap().clear();
                b.wact(EE_NETMSG_WACT_CLEAR_SEND_TO_SLOTS, &[], &[]);
            }
            let slot = b.m.u8(1);
            {
                let mut s = START_LOADING.lock().unwrap();
                if s.contains(&slot) {
                    return;
                }
                s.push(slot);
            }
            b.wact(EE_NETMSG_WACT_LOAD_RESPONSES, &[(n + 1) & 0xff], &[]);
            if b.m.u8(2) != 0xff {
                b.wact(EE_NETMSG_WACT_SEND_TO_SLOT, &[b.m.u8(2) as i64, b.m.u8(3) as i64], &[]);
            }
            if n + 1 == b.world(EE_NETMSG_WORLD_HUMANS) {
                b.wact(EE_NETMSG_WACT_ALL_LOADED, &[], &[]);
            }
        }
        // EEMLoadPercent (0x4c142a): a slot's loading progress, capped at 100 (the message keeps the cap)
        t::LOAD_PERCENT => {
            if b.m.u32(2) > 100 {
                b.m.set_u32(2, 100);
            }
            b.wact(EE_NETMSG_WACT_LOAD_PERCENT, &[b.m.i32(1) as i64, b.m.u32(2) as i64], &[]);
        }
        // the playback's (0x4c3bbc, 0x4c8cbb): the local sender's slow motion and zoom while a recorded game plays
        t::RECORD_FRAMES => {
            if b.local_playback() {
                b.wact(EE_NETMSG_WACT_RECORD_FRAMES, &[b.m.bool(1) as i64], &[]);
            }
        }
        t::ZOOM_INFO => {
            if b.local_playback() {
                b.wact(EE_NETMSG_WACT_ZOOM, &[], &[b.m.f32(1)]);
            }
        }
        // EEMToggleCollisionDetection (0x4c840a), debug: with a negative mode, the collision cell size (if
        // positive) or the next of the collision flags 0..15
        t::TOGGLE_COLLISION_DETECTION => {
            if b.m.i32(2) < 0 {
                if b.m.i32(1) > 0 {
                    b.wact(EE_NETMSG_WACT_COLLISION_UNIT, &[b.m.i32(1) as i64], &[]);
                } else {
                    b.wact(EE_NETMSG_WACT_COLLISION_FLAGS, &[], &[]);
                }
            }
        }
        // the world's one-call bodies
        t::ATTACK_PLAYERS => b.wact(EE_NETMSG_WACT_ATTACK_PLAYERS, &[b.m.i32(1) as i64, b.m.u8(2) as i64], &[]),
        t::BUILD_GATE => {
            let (x, y) = b.m.point(1);
            b.wact(EE_NETMSG_WACT_BUILD_GATE, &[b.m.i32(4) as i64, b.m.i32(2) as i64, b.m.i32(3) as i64], &[x, y]);
        }
        t::EDICT_PROPOSAL => b.wact(EE_NETMSG_WACT_EDICT, &[b.m.u16(1) as i64], &[]),
        t::END_CINEMATIC => b.wact(EE_NETMSG_WACT_END_CINEMATIC, &[], &[]),
        t::GAME_SPEED => b.wact(EE_NETMSG_WACT_GAME_SPEED, &[], &[b.m.f32(1)]),
        t::GAME_RESTARTING => b.wact(EE_NETMSG_WACT_MIGRATING_OFF, &[], &[]),
        t::LAG_STATUS => b.wact(EE_NETMSG_WACT_LAG, &[b.m.u16(1) as i64, b.m.u8(2) as i64], &[]),
        t::SNAP_TO_DAYLIGHT => b.wact(EE_NETMSG_WACT_POST_PHYSICS, &[0x503], &[]),
        t::TOGGLE_DAY_NIGHT => b.wact(EE_NETMSG_WACT_POST_PHYSICS, &[0x502], &[]),
        // the bodies the C++ keeps
        t::CIV_UPGRADES_PURCHASED => b.body(EE_NETMSG_BODY_CIV_UPGRADES_PURCHASED),
        t::CIV_UPGRADES_REQUESTED => b.body(EE_NETMSG_BODY_CIV_UPGRADES_REQUESTED),
        t::IMMEDIATE_CARGO_UNLOAD => b.body(EE_NETMSG_BODY_IMMEDIATE_CARGO_UNLOAD),
        t::IMPROVE_UNIT => b.body(EE_NETMSG_BODY_IMPROVE_UNIT),
        t::LOAD_MAIN_SCREEN => b.body(EE_NETMSG_BODY_LOAD_MAIN_SCREEN),
        t::LOAD_OR_SAVE_GAME => b.body(EE_NETMSG_BODY_LOAD_OR_SAVE_GAME),
        t::LOAD_UNIT_DATA => b.body(EE_NETMSG_BODY_LOAD_UNIT_DATA),
        t::MIGRATION_SLOT_INFO => b.body(EE_NETMSG_BODY_MIGRATION_SLOT_INFO),
        t::SAVED_GROUP_SELECT => b.body(EE_NETMSG_BODY_SAVED_GROUP_SELECT),
        t::SEND_FLARE => b.body(EE_NETMSG_BODY_SEND_FLARE),
        t::SET_DIPLOMATIC_STANCE => b.body(EE_NETMSG_BODY_SET_DIPLOMATIC_STANCE),
        t::START_LOADING => b.body(EE_NETMSG_BODY_START_LOADING),
        t::CHANGE_FORMATION => b.body(EE_NETMSG_BODY_CHANGE_FORMATION),
        _ => {}
    }
}

/// `EEMPayTribute`'s order of the resources (the decision), recorded.
fn tribute_order<H: MsgHost + ?Sized>(b: &mut Body<'_, '_, H>, amounts: &[u32; 7], text: bool) -> Vec<u8> {
    let mut order = [0u8; 5];
    // SAFETY: 7 amounts, room for 5
    let n = unsafe { decide::ee_netmsg_tribute_order(amounts.as_ptr(), text as u8, order.as_mut_ptr()) } as usize;
    let mut v = vec![amounts[2] as i64, amounts[3] as i64, amounts[4] as i64, amounts[5] as i64, amounts[6] as i64];
    v.push(text as i64);
    v.push(n as i64);
    v.extend((0..5).map(|i| if i < n { order[i] as i64 } else { 0 }));
    b.decision(KIND_TRIBUTE_ORDER, &v);
    order[..n].to_vec()
}

/// The units lane's bodies for the orders: `ee_rust` registers them at start (`ee_rust_init`; ee_netmsg cannot depend
/// on ee_units, whose host imports C++).
pub type UnitOrders = fn(player: u64, group: u64, order: Order) -> OrderResult;
static UNIT_ORDERS: OnceLock<UnitOrders> = OnceLock::new();

/// Registers the units lane's bodies for the orders (once).
pub fn register_unit_orders(orders: UnitOrders) {
    let _ = UNIT_ORDERS.set(orders);
}

/// The C++ host and the units lane's orders.
struct GameHost(EeNetMsgHostFn);

impl MsgHost for GameHost {
    fn call(&mut self, op: u32, call: &mut EeNetMsgCall) -> i64 {
        // SAFETY: the C++ host reads and writes the call for its duration
        unsafe { (self.0)(op, call) }
    }

    fn order(&mut self, player: u64, group: u64, order: Order) -> OrderResult {
        UNIT_ORDERS.get().map_or((0.0, 0.0), |orders| orders(player, group, order))
    }
}

/// `EEMRecord::Execute` (`framing` 0), `EEMessage::ExecuteSelectionMessage` (1) or `ExecuteMessage` (2): the type's
/// body over a copy of the message's record (64 bytes, written back when the body changed it: no Rust reference lives
/// across the host's calls) and of its list (`list_len` bytes), with the C++ host.
///
/// # Safety
/// `record` points to the record's bytes, `list` to `list_len` bytes (any pointer when 0); the host is EEMessage.cpp's.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_netmsg_execute(
    host: EeNetMsgHostFn,
    message: *mut c_void,
    message_type: u8,
    slot: u8,
    fast_id: u8,
    record: *mut u8,
    list: *const u8,
    list_len: usize,
    framing: u8,
) {
    let before = std::ptr::read(record as *const [u8; crate::netmsg::records::EE_NETMSG_RECORD_BYTES]);
    let mut copy = before;
    let list = if list_len == 0 { Vec::new() } else { std::slice::from_raw_parts(list, list_len).to_vec() };
    let mut h = GameHost(host);
    match framing {
        0 => execute(&mut h, message as u64, message_type, slot, &mut copy, &list),
        f => execute_framed(&mut h, message as u64, message_type, slot, fast_id, &mut copy, &list, f == 1),
    }
    if copy != before {
        std::ptr::write(record as *mut [u8; crate::netmsg::records::EE_NETMSG_RECORD_BYTES], copy);
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::netmsg::records::EE_NETMSG_RECORD_BYTES;

    /// A host that records every call and answers by a script (0 for anything it is not told).
    #[derive(Default)]
    struct Script {
        calls: Vec<(u32, EeNetMsgCall)>,
        orders: Vec<(u64, u64, Order)>,
        answer: Vec<(u32, i64, i64)>,
    }

    impl Script {
        /// `op` answers `value` when its i0 matches `key` (-1: any)
        fn on(mut self, op: u32, key: i64, value: i64) -> Self {
            self.answer.push((op, key, value));
            self
        }
        fn ops(&self) -> Vec<u32> {
            self.calls.iter().map(|c| c.0).collect()
        }
        fn of(&self, op: u32) -> Vec<EeNetMsgCall> {
            self.calls.iter().filter(|c| c.0 == op).map(|c| c.1).collect()
        }
    }

    impl MsgHost for Script {
        fn call(&mut self, op: u32, call: &mut EeNetMsgCall) -> i64 {
            self.calls.push((op, *call));
            self.answer.iter().rev().find(|a| a.0 == op && (a.1 == -1 || a.1 == call.i[0])).map_or(0, |a| a.2)
        }
        fn order(&mut self, player: u64, group: u64, order: Order) -> OrderResult {
            self.orders.push((player, group, order));
            match order {
                Order::PathTo { x, y, .. } => (x + 1.0, y - 1.0),
                Order::HackNewUnit { .. } => (f32::from_bits(77), 0.0),
                _ => (0.0, 0.0),
            }
        }
    }

    fn record(message_type: u8, members: &[(u32, &[u8])]) -> [u8; EE_NETMSG_RECORD_BYTES] {
        let mut r = [0u8; EE_NETMSG_RECORD_BYTES];
        for &(i, bytes) in members {
            let f = record_member(message_type, i);
            assert_eq!(f.size as usize, bytes.len());
            r[f.offset as usize..f.offset as usize + bytes.len()].copy_from_slice(bytes);
        }
        r
    }

    fn run(h: &mut Script, message_type: u8, r: &mut [u8; EE_NETMSG_RECORD_BYTES], list: &[u8]) {
        execute(h, 0x1000, message_type, 3, r, list);
    }

    #[test]
    fn every_record_type_runs_on_an_empty_world() {
        for t in 0x41..=0xfdu8 {
            if crate::netmsg::records::ee_netmsg_record_class(t) == u32::MAX {
                continue;
            }
            let mut h = Script::default();
            let mut r = [0u8; EE_NETMSG_RECORD_BYTES];
            run(&mut h, t, &mut r, &[]);
        }
    }

    #[test]
    fn the_orders_go_to_the_units_lane_with_the_members() {
        let mut h = Script::default().on(EE_NETMSG_OP_SENDER, -1, 0x50).on(EE_NETMSG_OP_GROUP, -1, 0x60);
        let x = 3.5f32.to_le_bytes();
        let y = (-2.25f32).to_le_bytes();
        let mut r = record(t::PATH_TO, &[(1, &1.0f32.to_le_bytes()), (2, &x), (3, &y), (4, &[1]), (7, &[1])]);
        run(&mut h, t::PATH_TO, &mut r, &[]);
        let path = Order::PathTo {
            x: 3.5,
            y: -2.25,
            facing: 1.0,
            forced: true,
            rotating: false,
            queued: false,
            group_movement: true,
        };
        assert_eq!(h.orders[0], (0x50, 0x60, path));
        // the moved point back in the record
        let m = Record::new(t::PATH_TO, &mut r);
        assert_eq!((m.f32(2), m.f32(3)), (4.5, -3.25));
        // the cheat's unit: the sender alone, the class made kept
        let mut h = Script::default().on(EE_NETMSG_OP_SENDER, -1, 0x50);
        let mut r = record(t::HACK_A_NEW_UNIT, &[(1, &9i32.to_le_bytes()), (3, &[2])]);
        run(&mut h, t::HACK_A_NEW_UNIT, &mut r, &[]);
        assert_eq!(h.orders[0].1, 0);
        assert_eq!(Record::new(t::HACK_A_NEW_UNIT, &mut r).i32(1), 77);
        assert!(!h.ops().contains(&EE_NETMSG_OP_GROUP));
        // the build: the tile through (long)
        let mut h = Script::default();
        let mut p = [0u8; 8];
        p[..4].copy_from_slice(&7.9f32.to_le_bytes());
        p[4..].copy_from_slice(&(-0.5f32).to_le_bytes());
        let mut r = record(t::BUILD_UNIT, &[(1, &p), (2, &5i32.to_le_bytes()), (3, &[2])]);
        run(&mut h, t::BUILD_UNIT, &mut r, &[]);
        assert_eq!(h.orders[0].2, Order::BuildUnit { technology: 5, x: 7, y: 0, queued: true });
    }

    #[test]
    fn the_long_cast() {
        assert_eq!(c_long(7.9), 7);
        assert_eq!(c_long(-7.9), -7);
        assert_eq!(c_long(f32::NAN), if cfg!(windows) { i32::MIN as i64 } else { i64::MIN });
        assert_eq!(c_long(3e9), if cfg!(windows) { i32::MIN as i64 } else { 3_000_000_000 });
    }

    #[test]
    fn the_load_percent_keeps_its_cap() {
        let mut h = Script::default();
        let mut r = record(t::LOAD_PERCENT, &[(1, &4i32.to_le_bytes()), (2, &250u32.to_le_bytes())]);
        run(&mut h, t::LOAD_PERCENT, &mut r, &[]);
        assert_eq!(Record::new(t::LOAD_PERCENT, &mut r).u32(2), 100);
        assert_eq!(&h.of(EE_NETMSG_OP_WORLD_ACT)[0].i[..3], &[EE_NETMSG_WACT_LOAD_PERCENT, 4, 100]);
    }

    #[test]
    fn a_take_is_tested_and_recorded() {
        let mut h = Script::default().on(EE_NETMSG_OP_PLAYER, -1, 0x70).on(
            EE_NETMSG_OP_PLAYER_FACT,
            EE_NETMSG_PLAYER_RESOURCE,
            500,
        );
        let members: [(u32, &[u8]); 3] =
            [(1, &200u32.to_le_bytes()), (2, &1i32.to_le_bytes()), (3, &4i32.to_le_bytes())];
        let mut r = record(t::ADD_REMOVE_RESOURCES, &members);
        run(&mut h, t::ADD_REMOVE_RESOURCES, &mut r, &[]);
        let d = h.of(EE_NETMSG_OP_DECISION)[0];
        assert_eq!(&d.i[..6], &[KIND_ADD_REMOVE, 4, 0, 200, 500, 2]);
        let take = h.of(EE_NETMSG_OP_RESOURCE)[0];
        assert_eq!((take.a, &take.i[..3]), (0x70, &[1, 4, 200][..]));
        // more than the stock: nothing
        let mut h = Script::default().on(EE_NETMSG_OP_PLAYER_FACT, EE_NETMSG_PLAYER_RESOURCE, 100);
        run(&mut h, t::ADD_REMOVE_RESOURCES, &mut r, &[]);
        assert!(h.of(EE_NETMSG_OP_RESOURCE).is_empty());
    }

    #[test]
    fn a_disband_refunds_the_living_scaled_by_health() {
        let mut h = Script::default()
            .on(EE_NETMSG_OP_SENDER, -1, 0x50)
            .on(EE_NETMSG_OP_CURRENT, -1, 0x50)
            .on(EE_NETMSG_OP_GROUP, -1, 0x60)
            .on(EE_NETMSG_OP_GROUP_FIRST, -1, 0x80)
            .on(EE_NETMSG_OP_GROUP_COUNT, -1, 1)
            .on(EE_NETMSG_OP_GROUP_UNIT, 0, 0x80)
            .on(EE_NETMSG_OP_UNIT_FACT, EE_NETMSG_UNIT_HIT_POINTS, 50)
            .on(EE_NETMSG_OP_UNIT_FACT, EE_NETMSG_UNIT_MAX_HIT_POINTS, 100)
            .on(EE_NETMSG_OP_UNIT_FACT, EE_NETMSG_UNIT_CAN_DISBAND, 1);
        let mut r = [0u8; EE_NETMSG_RECORD_BYTES];
        run(&mut h, t::DISBAND, &mut r, &[]);
        let d = h.of(EE_NETMSG_OP_DISBAND);
        assert_eq!(d.len(), 1);
        assert_eq!((d[0].a, d[0].b, d[0].f[0]), (0x50, 0x80, 0.375));
        let rec = h.of(EE_NETMSG_OP_DECISION)[0];
        assert_eq!(&rec.i[..7], &[KIND_DISBAND_REFUND, 5, 50, 100, 1, 1, 0.375f32.to_bits() as i64]);
        assert_eq!(h.of(EE_NETMSG_OP_SOUND)[0].i[0], EE_NETMSG_SOUND_CASH_REGISTER);
    }

    #[test]
    fn the_full_options_in_the_original_order() {
        let mut h = Script::default();
        let members: [(u32, &[u8]); 3] = [(7, &3i32.to_le_bytes()), (17, &[1]), (4, &(-1i32).to_le_bytes())];
        let mut r = record(t::FULL_GAME_OPTIONS, &members);
        run(&mut h, t::FULL_GAME_OPTIONS, &mut r, &[]);
        let o: Vec<(i64, i64)> = h.of(EE_NETMSG_OP_OPTION).iter().map(|c| (c.i[0], c.i[1])).collect();
        assert_eq!(o.len(), 18);
        assert_eq!(o[0], (0, 3));
        assert_eq!(o[9], (9, -1));
        assert_eq!(o[17], (17, 1));
        let locks: Vec<i64> = h.of(EE_NETMSG_OP_WORLD_ACT).iter().map(|c| c.i[1]).collect();
        assert_eq!(locks, [1, 0]);
        // one option: the bools as > 0, an unknown option nothing
        let mut h = Script::default();
        let mut r = record(t::GAME_OPTIONS, &[(1, &13i32.to_le_bytes()), (2, &7u32.to_le_bytes())]);
        run(&mut h, t::GAME_OPTIONS, &mut r, &[]);
        assert_eq!(&h.of(EE_NETMSG_OP_OPTION)[0].i[..2], &[13, 1]);
        let mut r = record(t::GAME_OPTIONS, &[(1, &18i32.to_le_bytes())]);
        let mut h = Script::default();
        run(&mut h, t::GAME_OPTIONS, &mut r, &[]);
        assert!(h.of(EE_NETMSG_OP_OPTION).is_empty());
    }

    #[test]
    fn an_allies_chat_reaches_allies_only() {
        // the sender 0x50 chats to allies; the local player 0x51 (id 2) is its enemy or not
        let script = |enemy: i64| {
            Script::default()
                .on(EE_NETMSG_OP_SENDER, -1, 0x50)
                .on(EE_NETMSG_OP_CURRENT, -1, 0x51)
                .on(EE_NETMSG_OP_PLAYER_FACT, EE_NETMSG_PLAYER_ID, 2)
                .on(EE_NETMSG_OP_PLAYER_FACT, EE_NETMSG_PLAYER_CHAT_TYPE, 1)
                .on(EE_NETMSG_OP_PLAYER_FACT, EE_NETMSG_PLAYER_SLOT, 1)
                .on(EE_NETMSG_OP_PLAYER_FACT, EE_NETMSG_PLAYER_ENEMY, enemy)
        };
        let mut r = [0u8; EE_NETMSG_RECORD_BYTES];
        let shown = |h: &Script| h.of(EE_NETMSG_OP_WORLD_ACT).iter().any(|c| c.i[0] == EE_NETMSG_WACT_SHOW_CHAT);
        let mut h = script(1);
        run(&mut h, t::IN_GAME_CHAT, &mut r, &[]);
        assert!(!shown(&h));
        let mut h = script(0);
        run(&mut h, t::IN_GAME_CHAT, &mut r, &[]);
        assert!(shown(&h));
        let acts: Vec<i64> = h.of(EE_NETMSG_OP_WORLD_ACT).iter().map(|c| c.i[0]).collect();
        assert_eq!(acts, [EE_NETMSG_WACT_CHAT_LOG, EE_NETMSG_WACT_SHOW_CHAT, EE_NETMSG_WACT_LAST_CHAT]);
        let show = h.of(EE_NETMSG_OP_WORLD_ACT)[1];
        assert_eq!(&show.i[1..3], &[EE_NETMSG_TEXT_SETUP_ALLIES, EE_NETMSG_TEXT_NONE]);
    }

    #[test]
    fn the_box_selection_decodes_its_ids() {
        let mut buffer = [0u8; 256];
        let n = units::compress(&[0x0102_0304, 0x0102_0305], 9, &mut buffer);
        let mut h = Script::default().on(EE_NETMSG_OP_SENDER, -1, 0x50);
        let mut r = record(t::SELECT_UNITS, &[(1, &[n])]);
        execute(&mut h, 1, t::SELECT_UNITS, 0, &mut r, &buffer[..n as usize]);
        let c = h.of(EE_NETMSG_OP_SELECT_LIST)[0];
        assert_eq!((c.a, c.i[0], c.i[1]), (0x50, 2, 2));
        assert_eq!(box_ids(&buffer[..n as usize]), [0x0902_0304, 0x0902_0305]);
        // an empty list reads nothing past its end
        assert!(box_ids(&[]).len() <= 0x100);
    }

    #[test]
    fn the_unpack_room_test() {
        // a 3-tile class around (10, 10): tiles 9..=12 asked, the last row and column not counted
        let mut h = Script::default().on(EE_NETMSG_OP_CLASS_FACT, EE_NETMSG_CLASS_TILE_SIZE, 3).on(
            EE_NETMSG_OP_CAN_PLACE,
            -1,
            1,
        );
        let mut r = [0u8; EE_NETMSG_RECORD_BYTES];
        let mut b = Body { h: &mut h, message: 0, slot: 0, m: Record::new(t::PACK_UNPACK, &mut r), list: &[] };
        assert!(can_unpack(&mut b, 0x90, 0x50, 10, 10));
        let tiles: Vec<(i64, i64)> = h.of(EE_NETMSG_OP_CAN_PLACE).iter().map(|c| (c.i[3], c.i[4])).collect();
        assert_eq!(tiles.len(), 16);
        assert_eq!((tiles[0], tiles[15]), ((9, 9), (12, 12)));
    }

    #[test]
    fn the_server_starts_when_every_slot_answered() {
        let script = || {
            Script::default().on(EE_NETMSG_OP_WORLD_FACT, EE_NETMSG_WORLD_IS_SERVER, 1).on(
                EE_NETMSG_OP_WORLD_FACT,
                EE_NETMSG_WORLD_LOADED_HUMANS,
                2,
            )
        };
        let started = |h: &Script| h.of(EE_NETMSG_OP_WORLD_ACT).iter().any(|c| c.i[0] == EE_NETMSG_WACT_START_GAME);
        let mut r = record(t::LOAD_MAIN_SCREEN_RESPONSE, &[(1, &[4]), (2, &[1])]);
        let mut h = script();
        run(&mut h, t::LOAD_MAIN_SCREEN_RESPONSE, &mut r, &[]);
        assert!(!started(&h));
        // the same slot again counts nothing
        let mut h = script();
        run(&mut h, t::LOAD_MAIN_SCREEN_RESPONSE, &mut r, &[]);
        assert!(!started(&h));
        let mut r = record(t::LOAD_MAIN_SCREEN_RESPONSE, &[(1, &[5]), (2, &[1])]);
        let mut h = script();
        run(&mut h, t::LOAD_MAIN_SCREEN_RESPONSE, &mut r, &[]);
        assert!(started(&h));
        assert_eq!(LOAD_MAIN_SCREEN.lock().unwrap().1, 0);
    }

    #[test]
    fn a_command_runs_on_its_selection_id_else_undoes_the_selection() {
        let script = |last: i64| {
            Script::default()
                .on(EE_NETMSG_OP_SENDER, -1, 0x50)
                .on(EE_NETMSG_OP_SELECTION, EE_NETMSG_SEL_LAST_ID, last)
                .on(EE_NETMSG_OP_UNIT, -1, 0x80)
        };
        let sels = |h: &Script| h.of(EE_NETMSG_OP_SELECTION).iter().map(|c| c.i[0]).collect::<Vec<i64>>();
        let mut r = record(t::SELECT_UNIT, &[(1, &7u32.to_le_bytes())]);
        let mut h = script(4);
        execute_framed(&mut h, 1, t::SELECT_UNIT, 0, 4, &mut r, &[], false);
        assert_eq!(sels(&h), [EE_NETMSG_SEL_LOCK, EE_NETMSG_SEL_LAST_ID, EE_NETMSG_SEL_LOCK]);
        assert_eq!(h.of(EE_NETMSG_OP_SELECT).len(), 1);
        assert_eq!(&h.of(EE_NETMSG_OP_DECISION)[0].i[..7], &[KIND_COMMAND_RUNS, 5, 0, 0x48, 4, 4, 1]);
        let mut h = script(5);
        execute_framed(&mut h, 1, t::SELECT_UNIT, 0, 4, &mut r, &[], false);
        assert_eq!(sels(&h), [EE_NETMSG_SEL_LOCK, EE_NETMSG_SEL_LAST_ID, EE_NETMSG_SEL_RESET, EE_NETMSG_SEL_LOCK]);
        assert!(h.of(EE_NETMSG_OP_SELECT).is_empty());
    }

    #[test]
    fn a_local_selection_on_a_client_is_marked_and_copied() {
        let mut h = Script::default().on(EE_NETMSG_OP_SENDER, -1, 0x50).on(EE_NETMSG_OP_CURRENT, -1, 0x50).on(
            EE_NETMSG_OP_SELECTION,
            EE_NETMSG_SEL_CURRENT_ID,
            9,
        );
        let mut r = [0u8; EE_NETMSG_RECORD_BYTES];
        execute_framed(&mut h, 1, t::UNSELECT_ALL_UNITS, 0, 9, &mut r, &[], true);
        let sels: Vec<(i64, i64)> = h.of(EE_NETMSG_OP_SELECTION).iter().map(|c| (c.i[0], c.i[1])).collect();
        assert_eq!(
            sels,
            [
                (EE_NETMSG_SEL_LOCK, 1),
                (EE_NETMSG_SEL_MARK_LOCAL, 1),
                (EE_NETMSG_SEL_MARK_LOCAL, 0),
                (EE_NETMSG_SEL_CURRENT_ID, 0),
                (EE_NETMSG_SEL_COPY, 0),
                (EE_NETMSG_SEL_SET_LAST_ID, 9),
                (EE_NETMSG_SEL_LOCK, 0),
                (EE_NETMSG_SEL_JIT_OFF, 0)
            ]
        );
        // the body ran between the marks
        assert_eq!(h.of(EE_NETMSG_OP_SELECT)[0].i[0], 3);
        // no sender: nothing
        let mut h = Script::default();
        execute_framed(&mut h, 1, t::UNSELECT_ALL_UNITS, 0, 9, &mut r, &[], true);
        assert_eq!(h.ops(), [EE_NETMSG_OP_SENDER]);
    }
}
