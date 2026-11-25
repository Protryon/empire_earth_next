//! The game's actions' host ops composed in Rust (game core round 12): `game_actions.rs` asks one host op at a time
//! (`EE_SCREENS_GA_OP_*` with its field in i0, the protocol its golden replays); here each is run as one or more
//! one-statement C++ cases of `ee_screenscpp_ga` (`Actions/EEARust.cpp`), keyed `op << 8 | field` (an op without
//! fields: the op itself). The sequences the C++ host's compound cases ran (the network's server and client swaps,
//! the migration, the editor's quick save and load, the camera's follow, the chat box's scrolling, the map's and the
//! group's points) are here, in the original's order, with the C++ objects they needed in between (a file spec, a
//! memory file, a message, the chat box) handed back and forth as pointers or kept in the host's file statics.

use crate::screens::game_actions::*;
use crate::screens::lobby_form::{EE_SCREENS_LE_ACTIVATE_CLIENT, EE_SCREENS_LE_ACTIVATE_SERVER};

/// The ops whose field (i0) picks a case: their C++ key is `op << 8 | field`.
const FIELD_OPS: [u32; 13] = [
    EE_SCREENS_GA_OP_UNIT,
    EE_SCREENS_GA_OP_CLASS,
    EE_SCREENS_GA_OP_GROUP,
    EE_SCREENS_GA_OP_PLAYER,
    EE_SCREENS_GA_OP_TILE,
    EE_SCREENS_GA_OP_MAP,
    EE_SCREENS_GA_OP_TECH,
    EE_SCREENS_GA_OP_CALAMITY,
    EE_SCREENS_GA_OP_CAMERA,
    EE_SCREENS_GA_OP_MINI_MAP,
    EE_SCREENS_GA_OP_SCROLL,
    EE_SCREENS_GA_OP_NET,
    EE_SCREENS_GA_OP_EDITOR,
];

// ---- the cases the compound ones became (`op << 8 | 100..`) ----
// the units': the physical model's location x and y into f0
pub const EE_SCREENS_GAH_U_LOC_X: u32 = EE_SCREENS_GA_OP_UNIT << 8 | 100;
pub const EE_SCREENS_GAH_U_LOC_Y: u32 = EE_SCREENS_GA_OP_UNIT << 8 | 101;
// the selected group's bounds into f0..f3 (the record's order: bottom, left, right, top)
pub const EE_SCREENS_GAH_G_BOUNDS: u32 = EE_SCREENS_GA_OP_GROUP << 8 | 100;
// the current player's next event source (whether there is one): its two longs into the slots from i1
pub const EE_SCREENS_GAH_P_EVENT: u32 = EE_SCREENS_GA_OP_PLAYER << 8 | 100;
// EEGetFirstSelectedUnit over the unit in i1 (in and out)
pub const EE_SCREENS_GAH_FS_CALL: u32 = EE_SCREENS_GA_OP_FIRST_SELECTED << 8 | 101;
// EEPointUnitOrTileXY at f0, f1, i0 over the tile, x and y in i1, i2, i3 (in and out)
pub const EE_SCREENS_GAH_PT_CALL: u32 = EE_SCREENS_GA_OP_POINT_TILE << 8 | 103;
// the tile's unit list: its first node, a node's next, a node's unit
pub const EE_SCREENS_GAH_T_FIRST: u32 = EE_SCREENS_GA_OP_TILE << 8 | 100;
pub const EE_SCREENS_GAH_T_NEXT: u32 = EE_SCREENS_GA_OP_TILE << 8 | 101;
pub const EE_SCREENS_GAH_T_NODE_UNIT: u32 = EE_SCREENS_GA_OP_TILE << 8 | 102;
// the farm's drop sites into the kept list (cleared, filled, its size, its first unit's id)
pub const EE_SCREENS_GAH_FD_CLEAR: u32 = EE_SCREENS_GA_OP_FARM_DROP_SITES << 8 | 100;
pub const EE_SCREENS_GAH_FD_FILL: u32 = EE_SCREENS_GA_OP_FARM_DROP_SITES << 8 | 101;
pub const EE_SCREENS_GAH_FD_SIZE: u32 = EE_SCREENS_GA_OP_FARM_DROP_SITES << 8 | 102;
pub const EE_SCREENS_GAH_FD_FIRST_ID: u32 = EE_SCREENS_GA_OP_FARM_DROP_SITES << 8 | 103;
// the map: tile (i1, i2) (0 off the map), a tile's height; the closest tile to (f0, f1, f2) into the kept x and y;
// the terrain mesh's left, right, front and back planes into f0..f5 (point, normal)
pub const EE_SCREENS_GAH_M_TILE_PTR: u32 = EE_SCREENS_GA_OP_MAP << 8 | 100;
pub const EE_SCREENS_GAH_M_TILE_HEIGHT: u32 = EE_SCREENS_GA_OP_MAP << 8 | 101;
pub const EE_SCREENS_GAH_M_CLOSEST: u32 = EE_SCREENS_GA_OP_MAP << 8 | 102;
pub const EE_SCREENS_GAH_M_PLANE: u32 = EE_SCREENS_GA_OP_MAP << 8 | 103;
// the effect created at (f0, f1, f2) on `p` (i0 the graphic effect) into the kept handle; the kept effect's owner i1
pub const EE_SCREENS_GAH_FX_CREATE: u32 = EE_SCREENS_GA_OP_EFFECT << 8 | 100;
pub const EE_SCREENS_GAH_FX_LOCAL: u32 = EE_SCREENS_GA_OP_EFFECT << 8 | 101;
// the current player's technology i1's unit class (0 if none); a class's tile size
pub const EE_SCREENS_GAH_K_CLASS: u32 = EE_SCREENS_GA_OP_TECH << 8 | 100;
pub const EE_SCREENS_GAH_K_CLASS_TILE_SIZE: u32 = EE_SCREENS_GA_OP_TECH << 8 | 101;
// the screen point (f0, f1) into the world (f3..f5); the world's transformation of f3..f5 into f0..f2
pub const EE_SCREENS_GAH_A_SCREEN_TO_WORLD: u32 = EE_SCREENS_GA_OP_CALAMITY << 8 | 100;
pub const EE_SCREENS_GAH_A_TRANSFORM: u32 = EE_SCREENS_GA_OP_CALAMITY << 8 | 101;
// the camera: SetZoom(f0), SetMode(i1), MoveTo the unit `p` (both ends, no location), Update(f0)
pub const EE_SCREENS_GAH_CAM_SET_ZOOM: u32 = EE_SCREENS_GA_OP_CAMERA << 8 | 100;
pub const EE_SCREENS_GAH_CAM_SET_MODE: u32 = EE_SCREENS_GA_OP_CAMERA << 8 | 101;
pub const EE_SCREENS_GAH_CAM_MOVE_TO: u32 = EE_SCREENS_GA_OP_CAMERA << 8 | 102;
pub const EE_SCREENS_GAH_CAM_UPDATE: u32 = EE_SCREENS_GA_OP_CAMERA << 8 | 103;
// the network's (only a LAN or WON game reaches these)
/// The lobby's chat box (a `UITextLabel*`); `p` a chat box: `mText` appended, its scroll bar, its slider's maximum
/// and current value, its starting display line set to i1.
pub const EE_SCREENS_GAH_N_CHAT: u32 = EE_SCREENS_GA_OP_NET << 8 | 100;
pub const EE_SCREENS_GAH_N_CHAT_APPEND: u32 = EE_SCREENS_GA_OP_NET << 8 | 101;
pub const EE_SCREENS_GAH_N_CHAT_BAR: u32 = EE_SCREENS_GA_OP_NET << 8 | 102;
pub const EE_SCREENS_GAH_N_CHAT_MAX: u32 = EE_SCREENS_GA_OP_NET << 8 | 103;
pub const EE_SCREENS_GAH_N_CHAT_CURRENT: u32 = EE_SCREENS_GA_OP_NET << 8 | 104;
pub const EE_SCREENS_GAH_N_CHAT_START_LINE: u32 = EE_SCREENS_GA_OP_NET << 8 | 105;
/// `gWonNet` set; its game list cleared; its room rejoined.
pub const EE_SCREENS_GAH_N_WON: u32 = EE_SCREENS_GA_OP_NET << 8 | 106;
pub const EE_SCREENS_GAH_N_WON_CLEAR: u32 = EE_SCREENS_GA_OP_NET << 8 | 107;
pub const EE_SCREENS_GAH_N_WON_REJOIN: u32 = EE_SCREENS_GA_OP_NET << 8 | 108;
/// The string i1 into the kept message; the kept message shown (i2 set: closing it restarts the client).
pub const EE_SCREENS_GAH_N_STRING: u32 = EE_SCREENS_GA_OP_NET << 8 | 109;
pub const EE_SCREENS_GAH_N_SHOW: u32 = EE_SCREENS_GA_OP_NET << 8 | 110;
/// The `EEServer` paused, unpaused; the UI told the pause (i1); `mUserPaused` = i1.
pub const EE_SCREENS_GAH_N_PAUSE: u32 = EE_SCREENS_GA_OP_NET << 8 | 111;
pub const EE_SCREENS_GAH_N_UNPAUSE: u32 = EE_SCREENS_GA_OP_NET << 8 | 112;
pub const EE_SCREENS_GAH_N_POST_PAUSE: u32 = EE_SCREENS_GA_OP_NET << 8 | 113;
pub const EE_SCREENS_GAH_N_SET_USER_PAUSED: u32 = EE_SCREENS_GA_OP_NET << 8 | 114;
/// `gCommunications` deleted; cleared.
pub const EE_SCREENS_GAH_N_DELETE: u32 = EE_SCREENS_GA_OP_NET << 8 | 115;
pub const EE_SCREENS_GAH_N_CLEAR: u32 = EE_SCREENS_GA_OP_NET << 8 | 116;
/// A new client; its threads started; a new server for `mGameName` (i1: published to WON); its threads started.
pub const EE_SCREENS_GAH_N_NEW_CLIENT: u32 = EE_SCREENS_GA_OP_NET << 8 | 117;
pub const EE_SCREENS_GAH_N_CLIENT_START: u32 = EE_SCREENS_GA_OP_NET << 8 | 118;
pub const EE_SCREENS_GAH_N_NEW_SERVER: u32 = EE_SCREENS_GA_OP_NET << 8 | 119;
pub const EE_SCREENS_GAH_N_SERVER_START: u32 = EE_SCREENS_GA_OP_NET << 8 | 120;
/// `gPlayerName` = `mPlayerName`; the host game name setting = `mGameName`.
pub const EE_SCREENS_GAH_N_PLAYER_NAME: u32 = EE_SCREENS_GA_OP_NET << 8 | 121;
pub const EE_SCREENS_GAH_N_HOST_NAME: u32 = EE_SCREENS_GA_OP_NET << 8 | 122;
/// The game options' write lock; the last game options copied; multiplayer; no cheats; the unlock.
pub const EE_SCREENS_GAH_N_OPT_LOCK: u32 = EE_SCREENS_GA_OP_NET << 8 | 123;
pub const EE_SCREENS_GAH_N_OPT_COPY: u32 = EE_SCREENS_GA_OP_NET << 8 | 124;
pub const EE_SCREENS_GAH_N_OPT_MULTI: u32 = EE_SCREENS_GA_OP_NET << 8 | 125;
pub const EE_SCREENS_GAH_N_OPT_NO_CHEATS: u32 = EE_SCREENS_GA_OP_NET << 8 | 126;
pub const EE_SCREENS_GAH_N_OPT_UNLOCK: u32 = EE_SCREENS_GA_OP_NET << 8 | 127;
/// `PlatSleep(i1)`; the string i1 into the kept format; the kept message printed from it with `mText`, with
/// `mGameName`; the kept message as a chat log line.
pub const EE_SCREENS_GAH_N_SLEEP: u32 = EE_SCREENS_GA_OP_NET << 8 | 128;
pub const EE_SCREENS_GAH_N_FORMAT: u32 = EE_SCREENS_GA_OP_NET << 8 | 129;
pub const EE_SCREENS_GAH_N_PRINT_TEXT: u32 = EE_SCREENS_GA_OP_NET << 8 | 130;
pub const EE_SCREENS_GAH_N_PRINT_GAME: u32 = EE_SCREENS_GA_OP_NET << 8 | 131;
pub const EE_SCREENS_GAH_N_CHAT_LOG: u32 = EE_SCREENS_GA_OP_NET << 8 | 132;
/// The kept address = `mGameName`, = `mName`; `SearchForGame(false)`; `Connect(address, "EEService", "")`.
pub const EE_SCREENS_GAH_N_ADDRESS_GAME: u32 = EE_SCREENS_GA_OP_NET << 8 | 133;
pub const EE_SCREENS_GAH_N_ADDRESS_NAME: u32 = EE_SCREENS_GA_OP_NET << 8 | 134;
pub const EE_SCREENS_GAH_N_SEARCH_OFF: u32 = EE_SCREENS_GA_OP_NET << 8 | 135;
pub const EE_SCREENS_GAH_N_CONNECT: u32 = EE_SCREENS_GA_OP_NET << 8 | 136;
/// The connection request (i1 the firewall flag); `p` (a message) sent as the start-connect message; `p` given as
/// local user input.
pub const EE_SCREENS_GAH_N_NEW_REQUEST: u32 = EE_SCREENS_GA_OP_NET << 8 | 137;
pub const EE_SCREENS_GAH_N_SEND_START: u32 = EE_SCREENS_GA_OP_NET << 8 | 138;
pub const EE_SCREENS_GAH_N_INPUT: u32 = EE_SCREENS_GA_OP_NET << 8 | 139;
/// The screen event i1; the lobby's open event `p`; `p` dereferenced.
pub const EE_SCREENS_GAH_N_EVENT: u32 = EE_SCREENS_GA_OP_NET << 8 | 140;
pub const EE_SCREENS_GAH_N_OPEN_EVENT: u32 = EE_SCREENS_GA_OP_NET << 8 | 141;
pub const EE_SCREENS_GAH_N_EVENT_RELEASE: u32 = EE_SCREENS_GA_OP_NET << 8 | 142;
/// The Sierra user id setting = `mPlayerName`.
pub const EE_SCREENS_GAH_N_SIERRA_ID: u32 = EE_SCREENS_GA_OP_NET << 8 | 143;
/// The migration's begin: `mText`, `mGameName`, `mName` emptied; the owed ticks run; the client migrating.
pub const EE_SCREENS_GAH_N_CLEAR_TEXT: u32 = EE_SCREENS_GA_OP_NET << 8 | 144;
pub const EE_SCREENS_GAH_N_CLEAR_GAME_NAME: u32 = EE_SCREENS_GA_OP_NET << 8 | 145;
pub const EE_SCREENS_GAH_N_CLEAR_NAME: u32 = EE_SCREENS_GA_OP_NET << 8 | 146;
pub const EE_SCREENS_GAH_N_RUN_OWED: u32 = EE_SCREENS_GA_OP_NET << 8 | 147;
pub const EE_SCREENS_GAH_N_CLIENT_MIGRATING: u32 = EE_SCREENS_GA_OP_NET << 8 | 148;
/// Player i1's slot, firewall flag, human flag, is the current player.
pub const EE_SCREENS_GAH_N_PLAYER_SLOT: u32 = EE_SCREENS_GA_OP_NET << 8 | 149;
pub const EE_SCREENS_GAH_N_PLAYER_FIREWALL: u32 = EE_SCREENS_GA_OP_NET << 8 | 150;
pub const EE_SCREENS_GAH_N_PLAYER_HUMAN: u32 = EE_SCREENS_GA_OP_NET << 8 | 151;
pub const EE_SCREENS_GAH_N_PLAYER_CURRENT: u32 = EE_SCREENS_GA_OP_NET << 8 | 152;
/// Player i1's active group cleared; the current player's selection copied into its active group; the client's fast
/// selection reset.
pub const EE_SCREENS_GAH_N_ACTIVE_CLEAR: u32 = EE_SCREENS_GA_OP_NET << 8 | 153;
pub const EE_SCREENS_GAH_N_COPY_SELECTION: u32 = EE_SCREENS_GA_OP_NET << 8 | 154;
pub const EE_SCREENS_GAH_N_RESET_FAST: u32 = EE_SCREENS_GA_OP_NET << 8 | 155;
/// `mText` = player i1's name; `mName` = its human name; `mGameName` = its name.
pub const EE_SCREENS_GAH_N_KEEP_TEXT: u32 = EE_SCREENS_GA_OP_NET << 8 | 156;
pub const EE_SCREENS_GAH_N_KEEP_HUMAN: u32 = EE_SCREENS_GA_OP_NET << 8 | 157;
pub const EE_SCREENS_GAH_N_KEEP_GAME_NAME: u32 = EE_SCREENS_GA_OP_NET << 8 | 158;
/// The CTs processed in the physics; the physics' current tick; the client's fast selection id.
pub const EE_SCREENS_GAH_N_CTS: u32 = EE_SCREENS_GA_OP_NET << 8 | 159;
pub const EE_SCREENS_GAH_N_CURRENT_TICK: u32 = EE_SCREENS_GA_OP_NET << 8 | 160;
pub const EE_SCREENS_GAH_N_FAST_ID: u32 = EE_SCREENS_GA_OP_NET << 8 | 161;
/// The promoted host: the "Migrated Server" (ticks i1, i2); the server messages cleared; a multiplayer session; the
/// current player sent to the server's slot; its slot 0xfe; the fast selection id i1; migrating.
pub const EE_SCREENS_GAH_N_NEW_MIGRATED: u32 = EE_SCREENS_GA_OP_NET << 8 | 162;
pub const EE_SCREENS_GAH_N_CLEAR_SERVER_MESSAGES: u32 = EE_SCREENS_GA_OP_NET << 8 | 163;
pub const EE_SCREENS_GAH_N_MULTI_SESSION: u32 = EE_SCREENS_GA_OP_NET << 8 | 164;
pub const EE_SCREENS_GAH_N_SEND_TO_SERVER: u32 = EE_SCREENS_GA_OP_NET << 8 | 165;
pub const EE_SCREENS_GAH_N_SLOT_FE: u32 = EE_SCREENS_GA_OP_NET << 8 | 166;
pub const EE_SCREENS_GAH_N_SERVER_FAST_ID: u32 = EE_SCREENS_GA_OP_NET << 8 | 167;
pub const EE_SCREENS_GAH_N_SERVER_MIGRATING: u32 = EE_SCREENS_GA_OP_NET << 8 | 168;
/// The server's reconnect wait timed out; its wait for i1 players started.
pub const EE_SCREENS_GAH_N_WAIT_TIMED_OUT: u32 = EE_SCREENS_GA_OP_NET << 8 | 169;
pub const EE_SCREENS_GAH_N_WAIT_START: u32 = EE_SCREENS_GA_OP_NET << 8 | 170;
/// The last tick received -2; my send-to slot none; the reconnection request (the tick, the player).
pub const EE_SCREENS_GAH_N_LAST_TICK: u32 = EE_SCREENS_GA_OP_NET << 8 | 171;
pub const EE_SCREENS_GAH_N_MY_SLOT_NONE: u32 = EE_SCREENS_GA_OP_NET << 8 | 172;
pub const EE_SCREENS_GAH_N_NEW_RECONNECT: u32 = EE_SCREENS_GA_OP_NET << 8 | 173;
// the scenario editor's and the saved games'
/// The improvement applied to the unit `p` (i1: lowered); its unit tab shown; the help string updated.
pub const EE_SCREENS_GAH_E_APPLY_IMPROVEMENT: u32 = EE_SCREENS_GA_OP_EDITOR << 8 | 100;
pub const EE_SCREENS_GAH_E_SHOW_UNIT: u32 = EE_SCREENS_GA_OP_EDITOR << 8 | 101;
pub const EE_SCREENS_GAH_E_HELP: u32 = EE_SCREENS_GA_OP_EDITOR << 8 | 102;
/// Player list entry i1's civilization file applied; its start epoch posted (406).
pub const EE_SCREENS_GAH_E_APPLY_CIV: u32 = EE_SCREENS_GA_OP_EDITOR << 8 | 103;
pub const EE_SCREENS_GAH_E_POST406: u32 = EE_SCREENS_GA_OP_EDITOR << 8 | 104;
/// The master volume, the music volume into f0; each set to f0; the physics' owed ticks cleared.
pub const EE_SCREENS_GAH_E_MASTER: u32 = EE_SCREENS_GA_OP_EDITOR << 8 | 105;
pub const EE_SCREENS_GAH_E_MUSIC: u32 = EE_SCREENS_GA_OP_EDITOR << 8 | 106;
pub const EE_SCREENS_GAH_E_SET_MASTER: u32 = EE_SCREENS_GA_OP_EDITOR << 8 | 107;
pub const EE_SCREENS_GAH_E_SET_MUSIC: u32 = EE_SCREENS_GA_OP_EDITOR << 8 | 108;
pub const EE_SCREENS_GAH_E_NO_OWED: u32 = EE_SCREENS_GA_OP_EDITOR << 8 | 109;
/// The synchronous server paused, unpaused.
pub const EE_SCREENS_GAH_E_PAUSE: u32 = EE_SCREENS_GA_OP_EDITOR << 8 | 110;
pub const EE_SCREENS_GAH_E_UNPAUSE: u32 = EE_SCREENS_GA_OP_EDITOR << 8 | 111;
/// A new file spec; `p`'s volume the start-up volume; its directory data\Scenarios; its file `EEQuickSave<mName>`,
/// `mFileName`; the saved game loaded from it (non-zero: failed); deleted.
pub const EE_SCREENS_GAH_E_NEW_SPEC: u32 = EE_SCREENS_GA_OP_EDITOR << 8 | 112;
pub const EE_SCREENS_GAH_E_SPEC_VOLUME: u32 = EE_SCREENS_GA_OP_EDITOR << 8 | 113;
pub const EE_SCREENS_GAH_E_SPEC_DIRECTORY: u32 = EE_SCREENS_GA_OP_EDITOR << 8 | 114;
pub const EE_SCREENS_GAH_E_SPEC_QUICK: u32 = EE_SCREENS_GA_OP_EDITOR << 8 | 115;
pub const EE_SCREENS_GAH_E_SPEC_FILE_NAME: u32 = EE_SCREENS_GA_OP_EDITOR << 8 | 116;
pub const EE_SCREENS_GAH_E_LOAD: u32 = EE_SCREENS_GA_OP_EDITOR << 8 | 117;
pub const EE_SCREENS_GAH_E_DELETE_SPEC: u32 = EE_SCREENS_GA_OP_EDITOR << 8 | 118;
/// A new memory file; `mFileName` the quick save's text name; the save written into `p`.
pub const EE_SCREENS_GAH_E_NEW_MEMORY: u32 = EE_SCREENS_GA_OP_EDITOR << 8 | 119;
pub const EE_SCREENS_GAH_E_TEXT_NAME: u32 = EE_SCREENS_GA_OP_EDITOR << 8 | 120;
pub const EE_SCREENS_GAH_E_WRITE: u32 = EE_SCREENS_GA_OP_EDITOR << 8 | 121;
/// The spec `p` created; opened for writing (1: opened); the memory file `p` saved into the spec i1; `p` closed.
pub const EE_SCREENS_GAH_E_CREATE: u32 = EE_SCREENS_GA_OP_EDITOR << 8 | 122;
pub const EE_SCREENS_GAH_E_OPEN: u32 = EE_SCREENS_GA_OP_EDITOR << 8 | 123;
pub const EE_SCREENS_GAH_E_SAVE_INTO: u32 = EE_SCREENS_GA_OP_EDITOR << 8 | 124;
pub const EE_SCREENS_GAH_E_CLOSE: u32 = EE_SCREENS_GA_OP_EDITOR << 8 | 125;

// the original's string ids (ee/EEStringIDs.h; checked by game_actions_cpp_consts_tests.rs)
pub const EE_SCREENS_GA_IDS_HOST_IS_GONE: i64 = 30608;
pub const EE_SCREENS_GA_IDS_YOU_ARE_BEING_PROMOTED_TO_HOST: i64 = 30610;
pub const EE_SCREENS_GA_IDS_ATTEMPTING_TO_CONNECT_TO_NEW_HOST: i64 = 30611;

/// The C++ cases (`ee_screenscpp_ga`): one statement each.
pub trait GaCpp {
    fn cpp(&mut self, key: u32, r: *mut EeScreensGaFields, p: usize, i: &mut [i64; 8], f: &mut [f32; 8]) -> i64;
}

struct Run<'a, C: GaCpp> {
    c: &'a mut C,
    r: *mut EeScreensGaFields,
}

impl<C: GaCpp> Run<'_, C> {
    /// A case over fresh arguments: its answer, i and f.
    fn call(&mut self, key: u32, p: usize, args: &[i64], fl: &[f32]) -> (i64, [i64; 8], [f32; 8]) {
        let mut i = [0i64; 8];
        let mut f = [0f32; 8];
        i[..args.len()].copy_from_slice(args);
        f[..fl.len()].copy_from_slice(fl);
        let ret = self.c.cpp(key, self.r, p, &mut i, &mut f);
        (ret, i, f)
    }
    /// A case on `p` with i1 = `a`: its answer.
    fn k(&mut self, key: u32, p: usize, a: i64) -> i64 {
        self.call(key, p, &[0, a], &[]).0
    }
    /// A case on `p` with i1, i2 = `a`, `b`: its answer.
    fn k2(&mut self, key: u32, p: usize, a: i64, b: i64) -> i64 {
        self.call(key, p, &[0, a, b], &[]).0
    }
    /// A case with no arguments: its answer.
    fn net(&mut self, key: u32) -> i64 {
        self.k(key, 0, 0)
    }
    /// A case on `p` over the caller's i (copied): its f0.
    fn f0(&mut self, key: u32, p: usize, i: &[i64; 8]) -> f32 {
        self.call(key, p, &i[..], &[]).2[0]
    }
    /// A case with f0 = `x`.
    fn set_f(&mut self, key: u32, x: f32) {
        self.call(key, 0, &[], &[x]);
    }
    /// The string `id` as the kept format, printed with `mText` (`game` false) or `mGameName`, as a chat log line.
    fn chat_line(&mut self, id: i64, game: bool) {
        self.k(EE_SCREENS_GAH_N_FORMAT, 0, id);
        self.net(if game { EE_SCREENS_GAH_N_PRINT_GAME } else { EE_SCREENS_GAH_N_PRINT_TEXT });
        self.net(EE_SCREENS_GAH_N_CHAT_LOG);
    }
    /// `gCommunications` deleted and cleared.
    fn delete_comms(&mut self) {
        self.net(EE_SCREENS_GAH_N_DELETE);
        self.net(EE_SCREENS_GAH_N_CLEAR);
    }
    /// A new file spec in data\Scenarios named `EEQuickSave<mName>` (`quick`) or `mFileName`.
    fn spec(&mut self, quick: bool) -> usize {
        let s = self.net(EE_SCREENS_GAH_E_NEW_SPEC) as usize;
        self.k(EE_SCREENS_GAH_E_SPEC_VOLUME, s, 0);
        self.k(EE_SCREENS_GAH_E_SPEC_DIRECTORY, s, 0);
        self.k(if quick { EE_SCREENS_GAH_E_SPEC_QUICK } else { EE_SCREENS_GAH_E_SPEC_FILE_NAME }, s, 0);
        s
    }
}

/// Runs the host op `op` (`EE_SCREENS_GA_OP_*`, its field in i0) over the C++ cases as the C++ host's compound
/// cases did; a one-statement case is passed on as is.
pub fn ee_screens_gah_run<C: GaCpp>(
    c: &mut C,
    op: u32,
    r: *mut EeScreensGaFields,
    p: usize,
    i: &mut [i64; 8],
    f: &mut [f32; 8],
) -> i64 {
    let mut h = Run { c, r };
    let field = i[0];
    let key = if FIELD_OPS.contains(&op) { op << 8 | field as u32 } else { op };
    match (op, field) {
        (EE_SCREENS_GA_OP_UNIT, EE_SCREENS_GA_U_LOCATION) => {
            f[0] = h.f0(EE_SCREENS_GAH_U_LOC_X, p, i);
            f[1] = h.f0(EE_SCREENS_GAH_U_LOC_Y, p, i);
            0
        }
        (EE_SCREENS_GA_OP_GROUP, EE_SCREENS_GA_G_CENTER) => {
            // GERectangle::GetCenter over the bounds (the record's order: bottom, left, right, top)
            let b = h.call(EE_SCREENS_GAH_G_BOUNDS, p, &[], &[]).2;
            let (bottom, left, right, top) = (b[0], b[1], b[2], b[3]);
            f[0] = (right - left) * 0.5 + left;
            f[1] = (bottom - top) * 0.5 + top;
            0
        }
        (EE_SCREENS_GA_OP_PLAYER, EE_SCREENS_GA_P_NEXT_EVENT) => {
            // the source (an EETileLocation: two longs) comes back in the slots from i1
            let (found, o, _) = h.call(EE_SCREENS_GAH_P_EVENT, 0, &[], &[]);
            if found == 0 {
                return 0;
            }
            // SAFETY: two longs fit in the seven slots after i0
            let source = unsafe { o[1..].as_ptr().cast::<[std::ffi::c_long; 2]>().read_unaligned() };
            i[0] = i64::from(source[0]);
            i[1] = i64::from(source[1]);
            1
        }
        (EE_SCREENS_GA_OP_FIRST_SELECTED, _) => {
            // the unit is in and out (EEGetFirstSelectedUnit's reference)
            let (found, o, _) = h.call(EE_SCREENS_GAH_FS_CALL, 0, &[0, i[0]], &[]);
            i[0] = o[1];
            found
        }
        (EE_SCREENS_GA_OP_POINT_TILE, _) => {
            // the tile, x and y are in and out (EEPointUnitOrTileXY's references; x and y longs in their slots)
            let (found, o, _) = h.call(EE_SCREENS_GAH_PT_CALL, 0, &[i[0], i[1], i[2], i[3]], &[f[0], f[1]]);
            i[1] = o[1];
            i[2] = i64::from(o[2] as std::ffi::c_long);
            i[3] = i64::from(o[3] as std::ffi::c_long);
            found
        }
        (EE_SCREENS_GA_OP_TILE, EE_SCREENS_GA_T_UNIT_AT) => {
            let mut node = h.net_on(EE_SCREENS_GAH_T_FIRST, p);
            let mut k = 0;
            while node != 0 && k < i[1] {
                node = h.net_on(EE_SCREENS_GAH_T_NEXT, node as usize);
                k += 1;
            }
            if node != 0 {
                h.net_on(EE_SCREENS_GAH_T_NODE_UNIT, node as usize)
            } else {
                0
            }
        }
        (EE_SCREENS_GA_OP_FARM_DROP_SITES, _) => {
            h.net(EE_SCREENS_GAH_FD_CLEAR);
            h.net_on(EE_SCREENS_GAH_FD_FILL, p);
            let n = h.net(EE_SCREENS_GAH_FD_SIZE);
            if n == 1 {
                i[0] = h.net(EE_SCREENS_GAH_FD_FIRST_ID);
            }
            n
        }
        (EE_SCREENS_GA_OP_MAP, EE_SCREENS_GA_M_TILE) => {
            let tile = h.k2(EE_SCREENS_GAH_M_TILE_PTR, 0, i[1], i[2]);
            if tile == 0 {
                return 0;
            }
            f[0] = h.call(EE_SCREENS_GAH_M_TILE_HEIGHT, tile as usize, &[], &[]).2[0];
            1
        }
        (EE_SCREENS_GA_OP_MAP, EE_SCREENS_GA_M_CLOSEST) => {
            // the closest tile's x and y start at 0 (they are in and out: longs in the slots i1 and i2)
            let o = h.call(EE_SCREENS_GAH_M_CLOSEST, 0, &[], &[f[0], f[1], 0.0]).1;
            i[0] = i64::from(o[1] as std::ffi::c_long);
            i[1] = i64::from(o[2] as std::ffi::c_long);
            0
        }
        (EE_SCREENS_GA_OP_MAP, EE_SCREENS_GA_M_PLANE) => {
            // the left, right, front or back plane (any other i1: the back)
            let which = if (0..3).contains(&i[1]) { i[1] } else { 3 };
            let plane = h.call(EE_SCREENS_GAH_M_PLANE, 0, &[0, which], &[]).2;
            f[..6].copy_from_slice(&plane[..6]);
            0
        }
        (EE_SCREENS_GA_OP_EFFECT, _) => {
            // the effect's handle (32 bits) comes back in i1 and goes to the next case in i2
            let handle = h.call(EE_SCREENS_GAH_FX_CREATE, p, &[i[0]], &[f[0], f[1], f[2]]).1[1];
            h.k2(EE_SCREENS_GAH_FX_LOCAL, 0, i[1], handle);
            0
        }
        (EE_SCREENS_GA_OP_TECH, EE_SCREENS_GA_K_TILE_SIZE) => {
            let class = h.k(EE_SCREENS_GAH_K_CLASS, 0, i[1]);
            if class != 0 {
                h.net_on(EE_SCREENS_GAH_K_CLASS_TILE_SIZE, class as usize)
            } else {
                -1
            }
        }
        (EE_SCREENS_GA_OP_CALAMITY, EE_SCREENS_GA_A_WORLD_POINT) => {
            let mut fl = [f[0], f[1], 0.0, 0.0, 0.0, 0.0, 0.0, 0.0];
            h.c.cpp(EE_SCREENS_GAH_A_SCREEN_TO_WORLD, r, 0, &mut [0; 8], &mut fl);
            h.c.cpp(EE_SCREENS_GAH_A_TRANSFORM, r, 0, &mut [0; 8], &mut fl);
            f[..3].copy_from_slice(&fl[..3]);
            0
        }
        (EE_SCREENS_GA_OP_CAMERA, EE_SCREENS_GA_CAM_FOLLOW) => {
            h.set_f(EE_SCREENS_GAH_CAM_SET_ZOOM, 0.0);
            h.k(EE_SCREENS_GAH_CAM_SET_MODE, 0, 3);
            h.net_on(EE_SCREENS_GAH_CAM_MOVE_TO, p);
            h.net(EE_SCREENS_GA_OP_CAMERA << 8 | EE_SCREENS_GA_CAM_CHANGED as u32);
            h.set_f(EE_SCREENS_GAH_CAM_UPDATE, 0.0);
            h.k(EE_SCREENS_GAH_CAM_SET_MODE, 0, 2);
            0
        }
        (EE_SCREENS_GA_OP_SCROLL, EE_SCREENS_GA_S_EDGES) => {
            // the edges in the record's order (bottom, left, right, top) as left, top, right, bottom
            let e = h.call(key, 0, &[field], &[]).2;
            f[..4].copy_from_slice(&[e[1], e[3], e[2], e[0]]);
            0
        }
        (EE_SCREENS_GA_OP_NET, _) => net(&mut h, key, field, i, f),
        (EE_SCREENS_GA_OP_EDITOR, _) => editor(&mut h, key, field, p, i, f),
        _ => h.c.cpp(key, r, p, i, f),
    }
}

impl<C: GaCpp> Run<'_, C> {
    /// A case on `p` with no arguments: its answer.
    fn net_on(&mut self, key: u32, p: usize) -> i64 {
        self.k(key, p, 0)
    }
}

/// The network's compound ops (only a LAN or WON game reaches them).
fn net<C: GaCpp>(h: &mut Run<'_, C>, key: u32, field: i64, i: &mut [i64; 8], f: &mut [f32; 8]) -> i64 {
    match field {
        EE_SCREENS_GA_N_CHAT_SCROLL | EE_SCREENS_GA_N_CHAT_APPEND | EE_SCREENS_GA_N_CHAT_TO_END => {
            let chat = h.net(EE_SCREENS_GAH_N_CHAT) as usize;
            if field == EE_SCREENS_GA_N_CHAT_APPEND {
                h.net_on(EE_SCREENS_GAH_N_CHAT_APPEND, chat);
                return 0;
            }
            if h.net_on(EE_SCREENS_GAH_N_CHAT_BAR, chat) == 0 {
                return 0;
            }
            let max = h.net_on(EE_SCREENS_GAH_N_CHAT_MAX, chat);
            if field == EE_SCREENS_GA_N_CHAT_TO_END {
                h.k(EE_SCREENS_GAH_N_CHAT_START_LINE, chat, max);
                return 0;
            }
            i[0] = max;
            i[1] = h.net_on(EE_SCREENS_GAH_N_CHAT_CURRENT, chat);
            1
        }
        EE_SCREENS_GA_N_WON_REJOIN => {
            if h.net(EE_SCREENS_GAH_N_WON) != 0 {
                h.net(EE_SCREENS_GAH_N_WON_CLEAR);
                h.net(EE_SCREENS_GAH_N_WON_REJOIN);
            }
            0
        }
        EE_SCREENS_GA_N_SHOW_STRING => {
            h.k(EE_SCREENS_GAH_N_STRING, 0, i[1]);
            h.k2(EE_SCREENS_GAH_N_SHOW, 0, 0, i[2]);
            0
        }
        EE_SCREENS_GA_N_USER_PAUSE => {
            h.net(if i[1] != 0 { EE_SCREENS_GAH_N_PAUSE } else { EE_SCREENS_GAH_N_UNPAUSE });
            h.k(EE_SCREENS_GAH_N_POST_PAUSE, 0, i[1]);
            h.k(EE_SCREENS_GAH_N_SET_USER_PAUSED, 0, i[1]);
            0
        }
        EE_SCREENS_GA_N_DELETE_COMMS => {
            h.delete_comms();
            0
        }
        EE_SCREENS_GA_N_NEW_CLIENT => {
            h.net(EE_SCREENS_GAH_N_NEW_CLIENT);
            h.net(EE_SCREENS_GAH_N_CLIENT_START);
            0
        }
        EE_SCREENS_GA_N_NEW_SERVER => {
            h.k(EE_SCREENS_GAH_N_NEW_SERVER, 0, i[1]);
            h.net(EE_SCREENS_GAH_N_SERVER_START);
            0
        }
        EE_SCREENS_GA_N_REMEMBER_HOST => {
            h.net(EE_SCREENS_GAH_N_PLAYER_NAME);
            h.net(EE_SCREENS_GAH_N_HOST_NAME);
            0
        }
        EE_SCREENS_GA_N_OPTIONS => {
            for k in [
                EE_SCREENS_GAH_N_OPT_LOCK,
                EE_SCREENS_GAH_N_OPT_COPY,
                EE_SCREENS_GAH_N_OPT_MULTI,
                EE_SCREENS_GAH_N_OPT_NO_CHEATS,
                EE_SCREENS_GAH_N_OPT_UNLOCK,
            ] {
                h.net(k);
            }
            0
        }
        EE_SCREENS_GA_N_CONNECT_ADDRESS | EE_SCREENS_GA_N_RECONNECT => {
            if field == EE_SCREENS_GA_N_RECONNECT {
                h.k(EE_SCREENS_GAH_N_SLEEP, 0, 7000);
                h.chat_line(EE_SCREENS_GA_IDS_HOST_IS_GONE, false);
                h.chat_line(EE_SCREENS_GA_IDS_ATTEMPTING_TO_CONNECT_TO_NEW_HOST, true);
                h.net(EE_SCREENS_GAH_N_ADDRESS_NAME);
            } else {
                h.net(EE_SCREENS_GAH_N_ADDRESS_GAME);
            }
            h.net(EE_SCREENS_GAH_N_SEARCH_OFF);
            h.net(EE_SCREENS_GAH_N_CONNECT)
        }
        EE_SCREENS_GA_N_REQUEST => {
            let message = h.k(EE_SCREENS_GAH_N_NEW_REQUEST, 0, i[1]) as usize;
            if i[2] != 0 {
                h.net_on(EE_SCREENS_GAH_N_SEND_START, message);
            }
            h.net_on(EE_SCREENS_GAH_N_INPUT, message);
            0
        }
        EE_SCREENS_GA_N_LOBBY_EVENT => {
            let kind = if i[1] != 0 { EE_SCREENS_LE_ACTIVATE_CLIENT } else { EE_SCREENS_LE_ACTIVATE_SERVER };
            let event = h.k(EE_SCREENS_GAH_N_EVENT, 0, i64::from(kind)) as usize;
            h.net_on(EE_SCREENS_GAH_N_OPEN_EVENT, event);
            h.net_on(EE_SCREENS_GAH_N_EVENT_RELEASE, event);
            0
        }
        EE_SCREENS_GA_N_SET_NAME => {
            h.net(if i[1] != 0 { EE_SCREENS_GAH_N_SIERRA_ID } else { EE_SCREENS_GAH_N_PLAYER_NAME });
            0
        }
        EE_SCREENS_GA_N_MIGRATE_BEGIN => {
            for k in [
                EE_SCREENS_GAH_N_CLEAR_TEXT,
                EE_SCREENS_GAH_N_CLEAR_GAME_NAME,
                EE_SCREENS_GAH_N_CLEAR_NAME,
                EE_SCREENS_GAH_N_RUN_OWED,
                EE_SCREENS_GAH_N_CLIENT_MIGRATING,
            ] {
                h.net(k);
            }
            0
        }
        EE_SCREENS_GA_N_PLAYER => {
            let k = i[1];
            i[0] = h.k(EE_SCREENS_GAH_N_PLAYER_SLOT, 0, k);
            i[1] = h.k(EE_SCREENS_GAH_N_PLAYER_FIREWALL, 0, k);
            i[2] = h.k(EE_SCREENS_GAH_N_PLAYER_HUMAN, 0, k);
            i[3] = h.k(EE_SCREENS_GAH_N_PLAYER_CURRENT, 0, k);
            0
        }
        EE_SCREENS_GA_N_CLEAR_SELECTIONS => {
            let players = h.net(EE_SCREENS_GA_OP_NET << 8 | EE_SCREENS_GA_N_NUM_PLAYERS as u32);
            for k in 0..players {
                h.k(EE_SCREENS_GAH_N_ACTIVE_CLEAR, 0, k);
            }
            h.net(EE_SCREENS_GAH_N_COPY_SELECTION);
            h.net(EE_SCREENS_GAH_N_RESET_FAST);
            0
        }
        EE_SCREENS_GA_N_KEEP_NAME => {
            if i[2] == 0 {
                h.k(EE_SCREENS_GAH_N_KEEP_TEXT, 0, i[1]);
            } else {
                h.k(EE_SCREENS_GAH_N_KEEP_HUMAN, 0, i[1]);
                h.k(EE_SCREENS_GAH_N_KEEP_GAME_NAME, 0, i[1]);
            }
            0
        }
        EE_SCREENS_GA_N_PROMOTE => {
            h.chat_line(EE_SCREENS_GA_IDS_HOST_IS_GONE, false);
            h.k(EE_SCREENS_GAH_N_STRING, 0, EE_SCREENS_GA_IDS_YOU_ARE_BEING_PROMOTED_TO_HOST);
            h.net(EE_SCREENS_GAH_N_CHAT_LOG);
            let tick = h.net(EE_SCREENS_GAH_N_CTS);
            let current = h.net(EE_SCREENS_GAH_N_CURRENT_TICK);
            let selection = h.net(EE_SCREENS_GAH_N_FAST_ID);
            h.delete_comms();
            h.k2(EE_SCREENS_GAH_N_NEW_MIGRATED, 0, tick, current);
            for k in [
                EE_SCREENS_GAH_N_SERVER_START,
                EE_SCREENS_GAH_N_CLEAR_SERVER_MESSAGES,
                EE_SCREENS_GAH_N_MULTI_SESSION,
                EE_SCREENS_GAH_N_SEND_TO_SERVER,
                EE_SCREENS_GAH_N_SLOT_FE,
            ] {
                h.net(k);
            }
            h.k(EE_SCREENS_GAH_N_SERVER_FAST_ID, 0, selection);
            h.net(EE_SCREENS_GAH_N_SERVER_MIGRATING);
            0
        }
        EE_SCREENS_GA_N_RECONNECT_WAIT => {
            if i[1] == 0 {
                h.net(EE_SCREENS_GAH_N_WAIT_TIMED_OUT);
            } else {
                h.k(EE_SCREENS_GAH_N_WAIT_START, 0, i[1]);
            }
            0
        }
        EE_SCREENS_GA_N_RECONNECT_REQUEST => {
            h.net(EE_SCREENS_GAH_N_LAST_TICK);
            h.net(EE_SCREENS_GAH_N_MY_SLOT_NONE);
            let message = h.net(EE_SCREENS_GAH_N_NEW_RECONNECT) as usize;
            h.net_on(EE_SCREENS_GAH_N_INPUT, message);
            0
        }
        _ => h.c.cpp(key, h.r, 0, i, f),
    }
}

/// The scenario editor's and the saved games' compound ops.
fn editor<C: GaCpp>(h: &mut Run<'_, C>, key: u32, field: i64, p: usize, i: &mut [i64; 8], f: &mut [f32; 8]) -> i64 {
    match field {
        EE_SCREENS_GA_E_APPLY_IMPROVEMENT => {
            h.k(EE_SCREENS_GAH_E_APPLY_IMPROVEMENT, p, i[1]);
            h.net_on(EE_SCREENS_GAH_E_SHOW_UNIT, p);
            h.net(EE_SCREENS_GAH_E_HELP);
            0
        }
        EE_SCREENS_GA_E_RESET_APPLY => {
            h.k(EE_SCREENS_GAH_E_APPLY_CIV, 0, i[1]);
            h.k(EE_SCREENS_GAH_E_POST406, 0, i[1]);
            0
        }
        EE_SCREENS_GA_E_MUTE => {
            let master = h.call(EE_SCREENS_GAH_E_MASTER, 0, &[], &[]).2[0];
            let music = h.call(EE_SCREENS_GAH_E_MUSIC, 0, &[], &[]).2[0];
            h.set_f(EE_SCREENS_GAH_E_SET_MASTER, 0.0);
            h.set_f(EE_SCREENS_GAH_E_SET_MUSIC, 0.0);
            f[0] = master;
            f[1] = music;
            0
        }
        EE_SCREENS_GA_E_UNMUTE => {
            h.set_f(EE_SCREENS_GAH_E_SET_MASTER, f[0]);
            h.set_f(EE_SCREENS_GAH_E_SET_MUSIC, f[1]);
            h.net(EE_SCREENS_GAH_E_NO_OWED);
            0
        }
        EE_SCREENS_GA_E_SERVER_PAUSE => {
            h.net(if i[1] != 0 { EE_SCREENS_GAH_E_PAUSE } else { EE_SCREENS_GAH_E_UNPAUSE });
            h.k(EE_SCREENS_GAH_N_POST_PAUSE, 0, i[1]);
            0
        }
        EE_SCREENS_GA_E_QUICK_LOAD => {
            let spec = h.spec(true);
            if h.net_on(EE_SCREENS_GAH_E_LOAD, spec) != 0 {
                h.net(EE_SCREENS_GA_OP_EDITOR << 8 | EE_SCREENS_GA_E_SAVE_ERROR as u32);
            }
            h.net_on(EE_SCREENS_GAH_E_DELETE_SPEC, spec);
            0
        }
        EE_SCREENS_GA_E_QUICK_SAVE => {
            let file = h.net(EE_SCREENS_GAH_E_NEW_MEMORY);
            h.net(EE_SCREENS_GAH_E_TEXT_NAME);
            h.net_on(EE_SCREENS_GAH_E_WRITE, file as usize);
            file
        }
        EE_SCREENS_GA_E_QUICK_WRITE => {
            let spec = h.spec(false);
            h.net_on(EE_SCREENS_GAH_E_CREATE, spec);
            if h.net_on(EE_SCREENS_GAH_E_OPEN, spec) != 0 {
                h.k(EE_SCREENS_GAH_E_SAVE_INTO, p, spec as i64);
                h.net_on(EE_SCREENS_GAH_E_CLOSE, spec);
            }
            h.net_on(EE_SCREENS_GAH_E_DELETE_SPEC, spec);
            0
        }
        _ => h.c.cpp(key, h.r, p, i, f),
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    /// The C++ cases recorded (key, p, i1, i2) and answered by key; f answered by key too.
    #[derive(Default)]
    struct Fake {
        log: Vec<(u32, usize, i64, i64)>,
        answers: Vec<(u32, i64)>,
        floats: Vec<(u32, [f32; 8])>,
    }

    impl GaCpp for Fake {
        fn cpp(&mut self, key: u32, _r: *mut EeScreensGaFields, p: usize, i: &mut [i64; 8], f: &mut [f32; 8]) -> i64 {
            self.log.push((key, p, i[1], i[2]));
            if let Some((_, v)) = self.floats.iter().find(|(k, _)| *k == key) {
                *f = *v;
            }
            self.answers.iter().find(|(k, _)| *k == key).map_or(0, |(_, v)| *v)
        }
    }

    fn run(fake: &mut Fake, op: u32, p: usize, i: &[i64], f: &[f32]) -> (i64, [i64; 8], [f32; 8]) {
        let mut ii = [0i64; 8];
        let mut ff = [0f32; 8];
        ii[..i.len()].copy_from_slice(i);
        ff[..f.len()].copy_from_slice(f);
        let ret = ee_screens_gah_run(fake, op, core::ptr::null_mut(), p, &mut ii, &mut ff);
        (ret, ii, ff)
    }

    fn keys(fake: &Fake) -> Vec<u32> {
        fake.log.iter().map(|l| l.0).collect()
    }

    #[test]
    fn keys_are_op_and_field() {
        assert_eq!(EE_SCREENS_GAH_U_LOC_X, 12 * 256 + 100);
        assert_eq!(EE_SCREENS_GAH_N_CHAT, 30 * 256 + 100);
        assert_eq!(EE_SCREENS_GAH_E_CLOSE, 31 * 256 + 125);
        let mut fake = Fake {
            answers: vec![(12 << 8 | EE_SCREENS_GA_U_ID as u32, 77), (EE_SCREENS_GA_OP_POST, 5)],
            ..Fake::default()
        };
        assert_eq!(run(&mut fake, EE_SCREENS_GA_OP_UNIT, 9, &[EE_SCREENS_GA_U_ID, 1], &[]).0, 77);
        assert_eq!(fake.log[0], (12 << 8, 9, 1, 0));
        assert_eq!(run(&mut fake, EE_SCREENS_GA_OP_POST, 0, &[3], &[]).0, 5);
        assert_eq!(fake.log[1].0, EE_SCREENS_GA_OP_POST);
    }

    #[test]
    fn group_center_is_the_bounds_center() {
        // bottom 10, left 2, right 6, top 4
        let mut fake = Fake {
            floats: vec![(EE_SCREENS_GAH_G_BOUNDS, [10.0, 2.0, 6.0, 4.0, 0.0, 0.0, 0.0, 0.0])],
            ..Fake::default()
        };
        let (_, _, f) = run(&mut fake, EE_SCREENS_GA_OP_GROUP, 0, &[EE_SCREENS_GA_G_CENTER], &[]);
        assert_eq!((f[0], f[1]), (4.0, 7.0));
    }

    #[test]
    fn scroll_edges_in_left_top_right_bottom() {
        let key = EE_SCREENS_GA_OP_SCROLL << 8 | EE_SCREENS_GA_S_EDGES as u32;
        let mut fake = Fake { floats: vec![(key, [4.0, 1.0, 3.0, 2.0, 0.0, 0.0, 0.0, 0.0])], ..Fake::default() };
        let (_, _, f) = run(&mut fake, EE_SCREENS_GA_OP_SCROLL, 0, &[EE_SCREENS_GA_S_EDGES], &[]);
        assert_eq!(&f[..4], &[1.0, 2.0, 3.0, 4.0]);
    }

    #[test]
    fn tile_unit_at_walks_the_nodes() {
        let mut fake = Fake {
            answers: vec![(EE_SCREENS_GAH_T_FIRST, 100), (EE_SCREENS_GAH_T_NEXT, 200), (EE_SCREENS_GAH_T_NODE_UNIT, 9)],
            ..Fake::default()
        };
        assert_eq!(run(&mut fake, EE_SCREENS_GA_OP_TILE, 1, &[EE_SCREENS_GA_T_UNIT_AT, 2], &[]).0, 9);
        assert_eq!(
            keys(&fake),
            [EE_SCREENS_GAH_T_FIRST, EE_SCREENS_GAH_T_NEXT, EE_SCREENS_GAH_T_NEXT, EE_SCREENS_GAH_T_NODE_UNIT]
        );
        assert_eq!(fake.log[3].1, 200);
        // no list: 0
        let mut fake = Fake::default();
        assert_eq!(run(&mut fake, EE_SCREENS_GA_OP_TILE, 1, &[EE_SCREENS_GA_T_UNIT_AT, 2], &[]).0, 0);
        assert_eq!(keys(&fake), [EE_SCREENS_GAH_T_FIRST]);
    }

    #[test]
    fn map_tile_and_tech_tile_size_off_the_table() {
        let mut fake = Fake::default();
        assert_eq!(run(&mut fake, EE_SCREENS_GA_OP_MAP, 0, &[EE_SCREENS_GA_M_TILE, 3, 4], &[]).0, 0);
        assert_eq!(fake.log, [(EE_SCREENS_GAH_M_TILE_PTR, 0, 3, 4)]);
        let mut fake = Fake::default();
        assert_eq!(run(&mut fake, EE_SCREENS_GA_OP_TECH, 0, &[EE_SCREENS_GA_K_TILE_SIZE, 5], &[]).0, -1);
        let mut fake = Fake {
            answers: vec![(EE_SCREENS_GAH_K_CLASS, 40), (EE_SCREENS_GAH_K_CLASS_TILE_SIZE, 3)],
            ..Fake::default()
        };
        assert_eq!(run(&mut fake, EE_SCREENS_GA_OP_TECH, 0, &[EE_SCREENS_GA_K_TILE_SIZE, 5], &[]).0, 3);
        assert_eq!(fake.log[1], (EE_SCREENS_GAH_K_CLASS_TILE_SIZE, 40, 0, 0));
    }

    #[test]
    fn point_tile_round_trips_its_kept_values() {
        // the fake leaves the slots as they came: the tile, x and y go in and come back in one call
        let mut fake = Fake { answers: vec![(EE_SCREENS_GAH_PT_CALL, 1)], ..Fake::default() };
        let (ret, i, _) = run(&mut fake, EE_SCREENS_GA_OP_POINT_TILE, 0, &[1, 7, 2, -3], &[1.0, 2.0]);
        assert_eq!((ret, i[1], i[2], i[3]), (1, 7, 2, -3));
        assert_eq!(fake.log, [(EE_SCREENS_GAH_PT_CALL, 0, 7, 2)]);
        // the first selected unit likewise
        let mut fake = Fake { answers: vec![(EE_SCREENS_GAH_FS_CALL, 1)], ..Fake::default() };
        let (ret, i, _) = run(&mut fake, EE_SCREENS_GA_OP_FIRST_SELECTED, 0, &[9], &[]);
        assert_eq!((ret, i[0]), (1, 9));
        assert_eq!(fake.log, [(EE_SCREENS_GAH_FS_CALL, 0, 9, 0)]);
    }

    #[test]
    fn camera_follow_in_the_original_order() {
        let mut fake = Fake::default();
        run(&mut fake, EE_SCREENS_GA_OP_CAMERA, 4, &[EE_SCREENS_GA_CAM_FOLLOW], &[]);
        assert_eq!(
            fake.log,
            [
                (EE_SCREENS_GAH_CAM_SET_ZOOM, 0, 0, 0),
                (EE_SCREENS_GAH_CAM_SET_MODE, 0, 3, 0),
                (EE_SCREENS_GAH_CAM_MOVE_TO, 4, 0, 0),
                (EE_SCREENS_GA_OP_CAMERA << 8 | EE_SCREENS_GA_CAM_CHANGED as u32, 0, 0, 0),
                (EE_SCREENS_GAH_CAM_UPDATE, 0, 0, 0),
                (EE_SCREENS_GAH_CAM_SET_MODE, 0, 2, 0),
            ]
        );
    }

    #[test]
    fn chat_scroll_without_a_bar_answers_0() {
        let mut fake = Fake { answers: vec![(EE_SCREENS_GAH_N_CHAT, 50)], ..Fake::default() };
        assert_eq!(run(&mut fake, EE_SCREENS_GA_OP_NET, 0, &[EE_SCREENS_GA_N_CHAT_SCROLL], &[]).0, 0);
        assert_eq!(keys(&fake), [EE_SCREENS_GAH_N_CHAT, EE_SCREENS_GAH_N_CHAT_BAR]);
        let answers = vec![
            (EE_SCREENS_GAH_N_CHAT, 50),
            (EE_SCREENS_GAH_N_CHAT_BAR, 1),
            (EE_SCREENS_GAH_N_CHAT_MAX, 30),
            (EE_SCREENS_GAH_N_CHAT_CURRENT, 12),
        ];
        let mut fake = Fake { answers: answers.clone(), ..Fake::default() };
        let (ret, i, _) = run(&mut fake, EE_SCREENS_GA_OP_NET, 0, &[EE_SCREENS_GA_N_CHAT_SCROLL], &[]);
        assert_eq!((ret, i[0], i[1]), (1, 30, 12));
        let mut fake = Fake { answers, ..Fake::default() };
        run(&mut fake, EE_SCREENS_GA_OP_NET, 0, &[EE_SCREENS_GA_N_CHAT_TO_END], &[]);
        assert_eq!(fake.log.last(), Some(&(EE_SCREENS_GAH_N_CHAT_START_LINE, 50, 30, 0)));
    }

    #[test]
    fn promote_keeps_the_ticks_and_selection() {
        let mut fake = Fake {
            answers: vec![
                (EE_SCREENS_GAH_N_CTS, 11),
                (EE_SCREENS_GAH_N_CURRENT_TICK, 22),
                (EE_SCREENS_GAH_N_FAST_ID, 7),
            ],
            ..Fake::default()
        };
        run(&mut fake, EE_SCREENS_GA_OP_NET, 0, &[EE_SCREENS_GA_N_PROMOTE], &[]);
        let k = keys(&fake);
        let at = |key| k.iter().position(|v| *v == key).unwrap();
        assert!(at(EE_SCREENS_GAH_N_FAST_ID) < at(EE_SCREENS_GAH_N_DELETE));
        assert!(at(EE_SCREENS_GAH_N_DELETE) < at(EE_SCREENS_GAH_N_NEW_MIGRATED));
        assert!(fake.log.contains(&(EE_SCREENS_GAH_N_NEW_MIGRATED, 0, 11, 22)));
        assert!(fake.log.contains(&(EE_SCREENS_GAH_N_SERVER_FAST_ID, 0, 7, 0)));
        assert_eq!(fake.log[0], (EE_SCREENS_GAH_N_FORMAT, 0, EE_SCREENS_GA_IDS_HOST_IS_GONE, 0));
    }

    #[test]
    fn quick_write_saves_only_when_open() {
        let mut fake = Fake { answers: vec![(EE_SCREENS_GAH_E_NEW_SPEC, 60)], ..Fake::default() };
        run(&mut fake, EE_SCREENS_GA_OP_EDITOR, 70, &[EE_SCREENS_GA_E_QUICK_WRITE], &[]);
        assert!(!keys(&fake).contains(&EE_SCREENS_GAH_E_SAVE_INTO));
        assert_eq!(fake.log.last(), Some(&(EE_SCREENS_GAH_E_DELETE_SPEC, 60, 0, 0)));
        let mut fake =
            Fake { answers: vec![(EE_SCREENS_GAH_E_NEW_SPEC, 60), (EE_SCREENS_GAH_E_OPEN, 1)], ..Fake::default() };
        run(&mut fake, EE_SCREENS_GA_OP_EDITOR, 70, &[EE_SCREENS_GA_E_QUICK_WRITE], &[]);
        assert!(fake.log.contains(&(EE_SCREENS_GAH_E_SAVE_INTO, 70, 60, 0)));
        assert!(fake.log.contains(&(EE_SCREENS_GAH_E_SPEC_FILE_NAME, 60, 0, 0)));
    }

    #[test]
    fn mute_answers_the_volumes() {
        let mut fake = Fake {
            floats: vec![(EE_SCREENS_GAH_E_MASTER, [0.5; 8]), (EE_SCREENS_GAH_E_MUSIC, [0.25; 8])],
            ..Fake::default()
        };
        let (_, _, f) = run(&mut fake, EE_SCREENS_GA_OP_EDITOR, 0, &[EE_SCREENS_GA_E_MUTE], &[]);
        assert_eq!((f[0], f[1]), (0.5, 0.25));
        assert_eq!(keys(&fake)[2..], [EE_SCREENS_GAH_E_SET_MASTER, EE_SCREENS_GAH_E_SET_MUSIC]);
    }
}
