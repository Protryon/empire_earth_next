//! The UI's game messages built in Rust (the UI thread lane, round 1): the host cases whose whole body was
//! `gCommunications->ProcessLocalUserInput(new EEMRecord(kind, {values}), kSlotNone)` over their arguments are one
//! [`Msg`] each, made by `EEMRecord`'s constructor through `ee_screenscpp_ga_record` (`EEMRecordValue`'s
//! conversions: an integer as `long long`, a float, a point) and sent through `ee_screenscpp_send_local`: the game
//! screens' (`EEScreens.cpp`: the hot keys', chat, main game form, diplomacy and options events), the icon bar's,
//! the portrait's, the command buttons' and the main form's mouse modes'. The values keep the C++'s casts: a
//! `(long)` of a 32- or 64-bit argument is [`long`] (32 bits on Windows), an `unsigned char` [`byte`], a `bool`
//! `!= 0`. The ones reading a C++ object (the first selected unit, the button's tech, the physics' time) stay.

// (the casts of `long`s are no-ops on Windows and conversions on LP64)
#![allow(clippy::unnecessary_cast)]

use std::os::raw::c_long;

use crate::screens::chat::EE_SCREENS_GAME_CH_CHAT_TYPE;
use crate::screens::diplomacy::{EE_SCREENS_GAME_DS_CHAT, EE_SCREENS_GAME_DS_RESOURCES, EE_SCREENS_GAME_DS_STANCE};
use crate::screens::game_actions::{
    EeScreensGaValue, EE_SCREENS_GA_VALUE_FLOAT, EE_SCREENS_GA_VALUE_INT, EE_SCREENS_GA_VALUE_POINT,
};
use crate::screens::hot_keys::{EE_SCREENS_GAME_HK_ADD_UNIT, EE_SCREENS_GAME_HK_GROUP, EE_SCREENS_GAME_HK_SELECT_UNIT};
use crate::screens::icon_bar::*;
use crate::screens::main_form::EeScreensMainFormIo;
use crate::screens::main_game::*;
use crate::screens::main_game_options::EE_SCREENS_GAME_GO_RESIGN;
use crate::screens::mouse_modes::*;
use crate::screens::portrait::{EE_SCREENS_PT_OP_REMOVE_IMPROVEMENT, EE_SCREENS_PT_OP_REMOVE_TECHNOLOGY};
use crate::screens::status_button::*;

/// The message kinds (`kMessage*`, ee/EEMessageID.h; private: not the C interface's).
const MSG_PAY_TRIBUTE: u8 = 0x44;
const MSG_SELECT_UNIT: u8 = 0x48;
const MSG_REMOVE_UNIT_FROM_SELECTED_GROUP: u8 = 0x4a;
const MSG_REMOVE_TECHNOLOGY: u8 = 0x53;
const MSG_KILL_UNIT: u8 = 0x5c;
const MSG_GAME_SPEED: u8 = 0x5f;
const MSG_ATTACK_PLAYERS: u8 = 0x70;
const MSG_PATROL: u8 = 0x73;
const MSG_ALWAYS_ADD_UNIT_TO_SELECTED_GROUP: u8 = 0x76;
const MSG_EXPLORE: u8 = 0x79;
const MSG_SET_AIRPORT_COMMAND: u8 = 0x80;
const MSG_REPLANT_FARMS: u8 = 0x81;
const MSG_SAVED_GROUP_ADD_SELECTION: u8 = 0x82;
const MSG_SAVED_GROUP_CREATE: u8 = 0x83;
const MSG_SAVED_GROUP_SELECT: u8 = 0x84;
const MSG_CLEAR_AIRPORT_COMMANDS: u8 = 0x87;
const MSG_STOP_ACTIVE_GROUP: u8 = 0x8a;
const MSG_REMOVE_IMPROVEMENT: u8 = 0x8b;
const MSG_LOCK_UNLOCK: u8 = 0x8c;
const MSG_INCREMENT_RALLY_TIMER: u8 = 0x8e;
const MSG_IMMEDIATE_CARGO_UNLOAD: u8 = 0x95;
const MSG_SET_DIPLOMATIC_STANCE: u8 = 0x99;
const MSG_ADD_REMOVE_RESOURCES: u8 = 0x9b;
const MSG_SEND_FLARE: u8 = 0x9c;
const MSG_SET_UNIT_BEHAVIOR: u8 = 0x9e;
const MSG_ATTACK_GROUND: u8 = 0x9f;
const MSG_SCRAMBLE_AIRCRAFT: u8 = 0xa0;
const MSG_SELECT_PLAYER_CHAT_TYPE: u8 = 0xa1;
const MSG_SELECT_CUSTOM_PLAYER_FOR_CHAT: u8 = 0xa2;
const MSG_RESIGN: u8 = 0xa3;
const MSG_DISBAND: u8 = 0xa4;
const MSG_SELF_DESTRUCT: u8 = 0xa5;
const MSG_PACK_UNPACK: u8 = 0xab;
const MSG_GO_FANATIC: u8 = 0xac;
const MSG_JIT_MANUFACTURING: u8 = 0xad;
const MSG_EXCHANGE_RESOURCES: u8 = 0xae;

/// One record message: its kind and its members in wire order.
#[derive(Clone, Debug, PartialEq)]
pub(crate) struct Msg {
    pub kind: u8,
    pub values: Vec<EeScreensGaValue>,
}

fn msg(kind: u8, values: &[EeScreensGaValue]) -> Option<Msg> {
    Some(Msg { kind, values: values.to_vec() })
}

/// An integer member (`long long`).
pub(crate) fn int(v: i64) -> EeScreensGaValue {
    EeScreensGaValue { kind: EE_SCREENS_GA_VALUE_INT, i: v, f: [0.0; 2] }
}

/// `(long)v`: the low bits as a C `long` (32 bits on Windows).
pub(crate) fn long(v: i64) -> EeScreensGaValue {
    int(v as c_long as i64)
}

/// `(unsigned char)v`.
pub(crate) fn byte(v: i64) -> EeScreensGaValue {
    int(v as u8 as i64)
}

/// `v != 0`.
pub(crate) fn flag(v: i64) -> EeScreensGaValue {
    int((v != 0) as i64)
}

fn float(v: f32) -> EeScreensGaValue {
    EeScreensGaValue { kind: EE_SCREENS_GA_VALUE_FLOAT, i: 0, f: [v, 0.0] }
}

fn point(x: f32, y: f32) -> EeScreensGaValue {
    EeScreensGaValue { kind: EE_SCREENS_GA_VALUE_POINT, i: 0, f: [x, y] }
}

/// The game screens' host (`ee_screenscpp_call`, `EEScreens.cpp`): its sends of their arguments (unsigned 32-bit;
/// `current` the world's `mCurrentPlayer`, read only by `MG_ATTACK`).
pub(crate) fn screens(op: u32, a: u32, b: u32, c: u32, d: u32, current: impl FnOnce() -> i64) -> Option<Msg> {
    let (a, b, c, d) = (a as i64, b as i64, c as i64, d as i64);
    match op {
        EE_SCREENS_GAME_HK_GROUP => match a {
            0 => msg(MSG_SAVED_GROUP_SELECT, &[byte(b), flag(c)]),
            1 => msg(MSG_SAVED_GROUP_ADD_SELECTION, &[byte(b)]),
            _ => msg(MSG_SAVED_GROUP_CREATE, &[byte(b)]),
        },
        EE_SCREENS_GAME_HK_SELECT_UNIT => msg(MSG_SELECT_UNIT, &[int(a)]),
        EE_SCREENS_GAME_HK_ADD_UNIT => msg(MSG_ALWAYS_ADD_UNIT_TO_SELECTED_GROUP, &[int(a)]),
        EE_SCREENS_GAME_CH_CHAT_TYPE => msg(MSG_SELECT_PLAYER_CHAT_TYPE, &[long(a)]),
        EE_SCREENS_GAME_MG_RALLY => msg(MSG_INCREMENT_RALLY_TIMER, &[byte(a)]),
        EE_SCREENS_GAME_MG_KILL => msg(MSG_KILL_UNIT, &[flag(a)]),
        EE_SCREENS_GAME_MG_ATTACK => msg(MSG_ATTACK_PLAYERS, &[long(current()), flag(a)]),
        EE_SCREENS_GAME_MG_SEND_SPEED => msg(MSG_GAME_SPEED, &[float(f32::from_bits(a as u32))]),
        EE_SCREENS_GAME_MG_BEHAVIOR => msg(MSG_SET_UNIT_BEHAVIOR, &[long(a)]),
        EE_SCREENS_GAME_DS_RESOURCES => msg(MSG_ADD_REMOVE_RESOURCES, &[int(c), int(a), long(b), byte(d)]),
        EE_SCREENS_GAME_DS_STANCE => msg(MSG_SET_DIPLOMATIC_STANCE, &[long(a), long(b)]),
        EE_SCREENS_GAME_DS_CHAT => msg(MSG_SELECT_CUSTOM_PLAYER_FOR_CHAT, &[long(a), flag(b)]),
        EE_SCREENS_GAME_GO_RESIGN => msg(MSG_RESIGN, &[]),
        _ => None,
    }
}

/// Whether the screens' op answers the input's result (`MG_KILL`, `MG_ATTACK`: `(uint32_t)` of it); the others 0.
pub(crate) fn screens_answers(op: u32) -> bool {
    op == EE_SCREENS_GAME_MG_KILL || op == EE_SCREENS_GAME_MG_ATTACK
}

/// `EEMPayTribute` to player `to` of the diplomacy form's amounts (food, wood, gold, stone, iron: the members are
/// food, gold, iron, stone, wood).
pub(crate) fn tribute(to: u32, amounts: &[u32; 5]) -> Msg {
    let m = |i: usize| int(amounts[i] as i64);
    Msg { kind: MSG_PAY_TRIBUTE, values: vec![m(0), m(2), m(4), m(3), m(1), byte(to as i64)] }
}

/// The icon bar's own ops (`op` without `EE_SCREENS_HC_CLASS`): its sends.
pub(crate) fn icon_bar(op: u32, a: i64, b: i64) -> Option<Msg> {
    match op {
        EE_SCREENS_IB_OP_SELECT => msg(MSG_SELECT_UNIT, &[long(a)]),
        EE_SCREENS_IB_OP_UNLOAD => msg(MSG_IMMEDIATE_CARGO_UNLOAD, &[long(a), long(b), flag(0)]),
        EE_SCREENS_IB_OP_SCRAMBLE => msg(MSG_SCRAMBLE_AIRCRAFT, &[long(b as i32 as i64), long(b >> 32), long(a)]),
        EE_SCREENS_IB_OP_REMOVE => msg(MSG_REMOVE_UNIT_FROM_SELECTED_GROUP, &[long(a)]),
        _ => None,
    }
}

/// The portrait's own ops: the cancels of the unit `id`'s research (`b` the tech) and improvement (`b` its kind).
pub(crate) fn portrait(op: u32, id: impl FnOnce() -> i64, b: i64) -> Option<Msg> {
    match op {
        EE_SCREENS_PT_OP_REMOVE_TECHNOLOGY => msg(MSG_REMOVE_TECHNOLOGY, &[int(id()), long(b), flag(1), flag(0)]),
        EE_SCREENS_PT_OP_REMOVE_IMPROVEMENT => msg(MSG_REMOVE_IMPROVEMENT, &[int(id()), long(b)]),
        _ => None,
    }
}

/// The command buttons' `EE_SCREENS_SB_OP_SEND` of the kind `which` (`EE_SCREENS_SB_SEND_*`) and its arguments
/// (the unload, the research and its cancel read the first unit or the button: the C++'s).
pub(crate) fn status_button(which: i64, b: i64, c: i64) -> Option<Msg> {
    match which {
        EE_SCREENS_SB_SEND_STOP => msg(MSG_STOP_ACTIVE_GROUP, &[]),
        EE_SCREENS_SB_SEND_DISBAND => msg(MSG_DISBAND, &[]),
        EE_SCREENS_SB_SEND_SELF_DESTRUCT => msg(MSG_SELF_DESTRUCT, &[]),
        EE_SCREENS_SB_SEND_PATROL => msg(MSG_PATROL, &[int(-1)]),
        EE_SCREENS_SB_SEND_REPLANT => msg(MSG_REPLANT_FARMS, &[int(-1)]),
        EE_SCREENS_SB_SEND_CLEAR_AIRPORT => msg(MSG_CLEAR_AIRPORT_COMMANDS, &[]),
        EE_SCREENS_SB_SEND_EXPLORE => msg(MSG_EXPLORE, &[]),
        EE_SCREENS_SB_SEND_LOCK => msg(MSG_LOCK_UNLOCK, &[flag(b)]),
        EE_SCREENS_SB_SEND_PACK => msg(MSG_PACK_UNPACK, &[flag(b)]),
        EE_SCREENS_SB_SEND_FANATIC => msg(MSG_GO_FANATIC, &[]),
        EE_SCREENS_SB_SEND_JIT => msg(MSG_JIT_MANUFACTURING, &[]),
        EE_SCREENS_SB_SEND_EXCHANGE => msg(MSG_EXCHANGE_RESOURCES, &[flag(b & 1), long(b >> 1), flag(c)]),
        _ => None,
    }
}

/// The main form's mouse modes' `EE_SCREENS_MF_OP_MD_SEND` of the kind `which` (`EE_SCREENS_MF_SEND_*`): the ones
/// of the op's arguments (the picked unit's and the first selected unit's stay the C++'s).
pub(crate) fn mouse_mode(which: i64, b: i64, io: &EeScreensMainFormIo) -> Option<Msg> {
    match which {
        EE_SCREENS_MF_SEND_AIRPORT => {
            msg(MSG_SET_AIRPORT_COMMAND, &[point(io.f[0], io.f[1]), long(b), long(io.n[0]), byte(io.n[1])])
        }
        EE_SCREENS_MF_SEND_ATTACK_GROUND => msg(MSG_ATTACK_GROUND, &[float(io.f[0]), float(io.f[1])]),
        EE_SCREENS_MF_SEND_FLARE => msg(MSG_SEND_FLARE, &[long(io.n[0]), long(io.n[1])]),
        _ => None,
    }
}

/// Sends `m` to the local input (`ProcessLocalUserInput(message, kSlotNone)`): the input's result.
#[cfg(not(test))]
pub(crate) fn send(m: &Msg) -> i64 {
    /// cbindgen:ignore
    mod sys {
        use crate::screens::game_actions::EeScreensGaValue;
        use core::ffi::c_void;
        extern "C" {
            pub fn ee_screenscpp_ga_record(kind: u32, values: *const EeScreensGaValue, count: u32) -> *mut c_void;
            pub fn ee_screenscpp_send_local(message: *mut c_void) -> i64;
        }
    }
    // SAFETY: the values' count; the record message goes to the input, which owns it
    unsafe {
        let message = sys::ee_screenscpp_ga_record(m.kind as u32, m.values.as_ptr(), m.values.len() as u32);
        sys::ee_screenscpp_send_local(message)
    }
}

#[cfg(test)]
#[path = "hud_sends_tests.rs"]
mod tests;
