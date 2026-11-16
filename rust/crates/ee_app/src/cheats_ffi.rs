//! `EECheatCodes`' C ABI (include/rust/ee_app.h): the table as an object the C++ class owns, the parse and the
//! dispatch over one callback host.

use crate::cheats::{CheatAction, CheatHost, EeAppCheats};
use std::ffi::c_void;
use std::os::raw::{c_long, c_ulong};

/// One host operation's arguments and answers.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeAppCheatIo {
    /// `STRING`'s resource id; `ACTION`'s action
    pub id: u32,
    pub flag: bool,
    /// the player count; a player; a unit id
    pub n: c_ulong,
    pub x: f32,
    pub y: f32,
    /// an action's numbers
    pub a: c_long,
    pub b: c_long,
    /// a text's bytes (`STRING`'s answer lives until the next call)
    pub text: *const u8,
    pub len: usize,
}

impl Default for EeAppCheatIo {
    fn default() -> Self {
        EeAppCheatIo { id: 0, flag: false, n: 0, x: 0.0, y: 0.0, a: 0, b: 0, text: std::ptr::null(), len: 0 }
    }
}

/// The host: `(ctx, op, io)`.
pub type EeAppCheatFn = unsafe extern "C" fn(*mut c_void, i32, *mut EeAppCheatIo);

// ---- the operations (io's fields: in -> out)
/// id -> text, len: the string resource
pub const EE_APP_CHEAT_OP_STRING: i32 = 1;
/// -> flag: a multiplayer session; -> flag: cheats enabled; -> n: the player count
pub const EE_APP_CHEAT_OP_MULTIPLAYER: i32 = 2;
pub const EE_APP_CHEAT_OP_CHEATS_ENABLED: i32 = 3;
pub const EE_APP_CHEAT_OP_PLAYERS: i32 = 4;
/// text, len: a chat log line
pub const EE_APP_CHEAT_OP_CHAT: i32 = 5;
/// x, y: "goto"; n: "find"
pub const EE_APP_CHEAT_OP_GOTO: i32 = 6;
pub const EE_APP_CHEAT_OP_FIND: i32 = 7;
/// id (the action below), n (the player), a, b: a cheat's work
pub const EE_APP_CHEAT_OP_ACTION: i32 = 8;

// ---- the actions (a, b)
/// a: all (else the local player's selection and viewed unit are spared)
pub const EE_APP_CHEAT_KILL: u32 = 1;
pub const EE_APP_CHEAT_EXPLORE: u32 = 2;
pub const EE_APP_CHEAT_MAKE_VISIBLE: u32 = 3;
pub const EE_APP_CHEAT_RESEARCH_ALL: u32 = 4;
/// a: the epoch
pub const EE_APP_CHEAT_ADVANCE_EPOCH: u32 = 5;
pub const EE_APP_CHEAT_RESTORE_POWER: u32 = 6;
pub const EE_APP_CHEAT_HEAL: u32 = 7;
pub const EE_APP_CHEAT_REFUEL: u32 = 8;
pub const EE_APP_CHEAT_TOGGLE_INSTANT_BUILD: u32 = 9;
/// a: the game ends as a victory when the player is the local one (else as a defeat)
pub const EE_APP_CHEAT_END_GAME: u32 = 10;
/// a: the resource, b: the amount
pub const EE_APP_CHEAT_ADD_RESOURCE: u32 = 11;
/// a: the resource
pub const EE_APP_CHEAT_CLEAR_RESOURCE: u32 = 12;

struct Host {
    call: EeAppCheatFn,
    ctx: *mut c_void,
}

impl Host {
    fn op(&mut self, op: i32, io: EeAppCheatIo) -> EeAppCheatIo {
        let mut io = io;
        // SAFETY: the C++ passed a live context and its callback for the entry's duration
        unsafe { (self.call)(self.ctx, op, &mut io) };
        io
    }
}

impl CheatHost for Host {
    fn string(&mut self, id: u32) -> Vec<u8> {
        let io = self.op(EE_APP_CHEAT_OP_STRING, EeAppCheatIo { id, ..Default::default() });
        if io.text.is_null() {
            return Vec::new();
        }
        // SAFETY: the C++ answers `len` bytes that live until its next call
        unsafe { std::slice::from_raw_parts(io.text, io.len) }.to_vec()
    }
    fn multiplayer(&mut self) -> bool {
        self.op(EE_APP_CHEAT_OP_MULTIPLAYER, EeAppCheatIo::default()).flag
    }
    fn cheats_enabled(&mut self) -> bool {
        self.op(EE_APP_CHEAT_OP_CHEATS_ENABLED, EeAppCheatIo::default()).flag
    }
    fn players(&mut self) -> c_ulong {
        self.op(EE_APP_CHEAT_OP_PLAYERS, EeAppCheatIo::default()).n
    }
    fn chat(&mut self, line: &[u8]) {
        self.op(EE_APP_CHEAT_OP_CHAT, EeAppCheatIo { text: line.as_ptr(), len: line.len(), ..Default::default() });
    }
    fn goto(&mut self, x: f32, y: f32) {
        self.op(EE_APP_CHEAT_OP_GOTO, EeAppCheatIo { x, y, ..Default::default() });
    }
    fn find(&mut self, id: c_ulong) {
        self.op(EE_APP_CHEAT_OP_FIND, EeAppCheatIo { n: id, ..Default::default() });
    }
    fn action(&mut self, action: CheatAction, player: c_ulong) {
        use CheatAction::*;
        let (id, a, b) = match action {
            Kill { all } => (EE_APP_CHEAT_KILL, all as c_long, 0),
            Explore => (EE_APP_CHEAT_EXPLORE, 0, 0),
            MakeVisible => (EE_APP_CHEAT_MAKE_VISIBLE, 0, 0),
            ResearchAll => (EE_APP_CHEAT_RESEARCH_ALL, 0, 0),
            AdvanceEpoch(e) => (EE_APP_CHEAT_ADVANCE_EPOCH, e, 0),
            RestorePower => (EE_APP_CHEAT_RESTORE_POWER, 0, 0),
            Heal => (EE_APP_CHEAT_HEAL, 0, 0),
            Refuel => (EE_APP_CHEAT_REFUEL, 0, 0),
            ToggleInstantBuild => (EE_APP_CHEAT_TOGGLE_INSTANT_BUILD, 0, 0),
            EndGame { win } => (EE_APP_CHEAT_END_GAME, win as c_long, 0),
            AddResource(r, n) => (EE_APP_CHEAT_ADD_RESOURCE, r, n),
            ClearResource(r) => (EE_APP_CHEAT_CLEAR_RESOURCE, r, 0),
        };
        self.op(EE_APP_CHEAT_OP_ACTION, EeAppCheatIo { id, n: player, a, b, ..Default::default() });
    }
}

/// `Init`: the table from the string resources.
///
/// # Safety
/// `call` and `ctx` are live for the call.
#[no_mangle]
pub unsafe extern "C" fn ee_app_cheats_new(call: EeAppCheatFn, ctx: *mut c_void) -> *mut EeAppCheats {
    Box::into_raw(Box::new(EeAppCheats::new(&mut Host { call, ctx })))
}

/// # Safety
/// `p` came from [`ee_app_cheats_new`] (or is null) and is not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_app_cheats_free(p: *mut EeAppCheats) {
    if !p.is_null() {
        // SAFETY: the function's contract
        drop(unsafe { Box::from_raw(p) });
    }
}

/// `ParseCheatCode(text)`: `code` and (when the line ends in a number) `parameter`.
///
/// # Safety
/// `p` is live, `text` holds `len` bytes, the outputs are live; `call` and `ctx` are live for the call.
#[no_mangle]
pub unsafe extern "C" fn ee_app_cheats_parse(
    p: *const EeAppCheats,
    call: EeAppCheatFn,
    ctx: *mut c_void,
    text: *const u8,
    len: usize,
    code: *mut c_long,
    parameter: *mut c_long,
) {
    // SAFETY: the function's contract
    unsafe {
        let text = if text.is_null() { &[][..] } else { std::slice::from_raw_parts(text, len) };
        (*p).parse(&mut Host { call, ctx }, text, &mut *code, &mut *parameter);
    }
}

/// `ApplyCheatCode(code, player, parameter)`.
///
/// # Safety
/// `p` is live; `call` and `ctx` are live for the call.
#[no_mangle]
pub unsafe extern "C" fn ee_app_cheats_apply(
    p: *const EeAppCheats,
    call: EeAppCheatFn,
    ctx: *mut c_void,
    code: c_long,
    player: c_long,
    parameter: c_long,
) {
    // SAFETY: the function's contract
    unsafe { (*p).apply(&mut Host { call, ctx }, code, player, parameter) };
}
