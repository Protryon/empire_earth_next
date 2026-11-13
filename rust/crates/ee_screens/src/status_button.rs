//! The main game form's command buttons' presses (`EEUIFMGStatusButton::ButtonUp`, `Command` and `MouseRButtonUp`;
//! `User Interface Thread/EEUIFMGStatusButton.cpp`).
//!
//! A button is either a command (an `EEButtonObject` index) or a tech tree node (a unit to build or train, a
//! technology to research, a calamity). The decisions are here; every read of the world and every call is one host
//! op ([`ButtonHost`], `EE_SCREENS_SB_OP_*`) on the button, asked where and as often as the original asked it. The
//! messages the presses send are one op with their kind (`EE_SCREENS_SB_SEND_*`) and arguments.

use crate::command_panel::command_mode;

/// The host: one op on the button with up to three arguments and its answer.
pub trait ButtonHost {
    fn op(&mut self, op: u32, a: i64, b: i64, c: i64) -> i64;
}

/// The button's `mIsCommand`.
pub const EE_SCREENS_SB_OP_IS_COMMAND: u32 = 1;
/// The button's `mButton` (its `EEButtonObject` index).
pub const EE_SCREENS_SB_OP_BUTTON: u32 = 2;
/// The main game form's entity group a enabled (c != 0) or disabled, then its background b made visible or hidden.
pub const EE_SCREENS_SB_OP_PANEL: u32 = 3;
/// `gUIManager->Fn26FA6A()` (a = 0) or `Fn26FA55()` (a = 1).
pub const EE_SCREENS_SB_OP_MAIN_GROUP: u32 = 4;
/// The local player's selected group's unit count; -1 without a group.
pub const EE_SCREENS_SB_OP_GROUP_COUNT: u32 = 5;
/// Whether the selected group's unit a is set.
pub const EE_SCREENS_SB_OP_UNIT: u32 = 6;
/// The selected unit a's class's `mUnitType`.
pub const EE_SCREENS_SB_OP_UNIT_TYPE: u32 = 7;
/// The selected unit a's `mPower` (b = 0) or `GetPower()` (b = 1).
pub const EE_SCREENS_SB_OP_UNIT_POWER: u32 = 8;
/// `gFortressUnloadCost`.
pub const EE_SCREENS_SB_OP_UNLOAD_COST: u32 = 9;
/// `EEGetFirstSelectedUnit`: 1 when it found one (the host keeps it as the transport).
pub const EE_SCREENS_SB_OP_FIRST_SELECTED: u32 = 10;
/// The transport's class's `mCategory`.
pub const EE_SCREENS_SB_OP_FIRST_CATEGORY: u32 = 11;
/// The transport's class's `mUnitType`.
pub const EE_SCREENS_SB_OP_FIRST_TYPE: u32 = 12;
/// A message sent (`gCommunications->ProcessLocalUserInput`): a = `EE_SCREENS_SB_SEND_*`, b and c its arguments.
pub const EE_SCREENS_SB_OP_SEND: u32 = 13;
/// The form's `mIsCursorLocked = true`.
pub const EE_SCREENS_SB_OP_LOCK_CURSOR: u32 = 14;
/// `gMouse->mPointer = a`.
pub const EE_SCREENS_SB_OP_POINTER: u32 = 15;
/// The form's `SetUIState(a)`.
pub const EE_SCREENS_SB_OP_UI_STATE: u32 = 16;
/// The form's cancel button shown (`kMainGameCancelUIMode`).
pub const EE_SCREENS_SB_OP_SHOW_CANCEL: u32 = 17;
/// The main game form's `mRefreshGroup = true` when there is one.
pub const EE_SCREENS_SB_OP_REFRESH_GROUP: u32 = 18;
/// The form's `StartPatrol()`.
pub const EE_SCREENS_SB_OP_START_PATROL: u32 = 19;
/// The form's `StartFlare()`.
pub const EE_SCREENS_SB_OP_START_FLARE: u32 = 20;
/// `gGameResources`.
pub const EE_SCREENS_SB_OP_GAME_RESOURCES: u32 = 21;
/// `gPhysics->GetGameClock()` (f32 bits).
pub const EE_SCREENS_SB_OP_GAME_CLOCK: u32 = 22;
/// A world table long value (a: the key).
pub const EE_SCREENS_SB_OP_WORLD_LONG: u32 = 23;
/// The string a into the chat log (`gResource->GetString`, `gUserInterface->AddChatLogEntry(..., 0)`).
pub const EE_SCREENS_SB_OP_CHAT_STRING: u32 = 24;
/// `EECanAffordTrade(a)`.
pub const EE_SCREENS_SB_OP_CAN_AFFORD_TRADE: u32 = 25;
/// `EEDecodeTradeButton(a, ...)`: -1 when it fails, else the resource * 2 + the sell flag.
pub const EE_SCREENS_SB_OP_DECODE_TRADE: u32 = 26;
/// `UIControl::ShiftKeyPressed()`.
pub const EE_SCREENS_SB_OP_SHIFT: u32 = 27;
/// `EESoundObject::Get(a)`'s sample played (`gSoundManager->Play`).
pub const EE_SCREENS_SB_OP_SOUND: u32 = 28;
/// The button's technology's node's data `mKind`.
pub const EE_SCREENS_SB_OP_NODE_KIND: u32 = 29;
/// The node's data `mUnitClass`.
pub const EE_SCREENS_SB_OP_NODE_CLASS: u32 = 30;
/// Whether `gUnitClassTable[a]` is set.
pub const EE_SCREENS_SB_OP_CLASS_PRESENT: u32 = 31;
/// The class a is a land building (`IsLandBuilding()` and not a naval building).
pub const EE_SCREENS_SB_OP_CLASS_LAND_BUILDING: u32 = 32;
/// The class a's `mCategory` is the gate's.
pub const EE_SCREENS_SB_OP_CLASS_GATE: u32 = 33;
/// The node's data `mMakesUnit`.
pub const EE_SCREENS_SB_OP_NODE_MAKES_UNIT: u32 = 34;
/// The form's `BuildUnit(mUnitID, mTechnology)`.
pub const EE_SCREENS_SB_OP_BUILD_UNIT: u32 = 35;
/// The form's `StartWall(mTechnology)`.
pub const EE_SCREENS_SB_OP_START_WALL: u32 = 36;
/// The form's `StartGate(mTechnology)`.
pub const EE_SCREENS_SB_OP_START_GATE: u32 = 37;
/// The current player's `CanProduce(mTechnology, mUnitType, a, true, 3, &reason)` (the host keeps the reason).
pub const EE_SCREENS_SB_OP_CAN_PRODUCE: u32 = 38;
/// `TMGetTime()`.
pub const EE_SCREENS_SB_OP_TIME: u32 = 39;
/// The button's `mLastErrorTime`.
pub const EE_SCREENS_SB_OP_LAST_ERROR: u32 = 40;
/// The button's `mLastErrorTime = a`.
pub const EE_SCREENS_SB_OP_SET_LAST_ERROR: u32 = 41;
/// The kept reason into the chat log (`gUserInterface->AddChatLogEntry(reason, 0)`).
pub const EE_SCREENS_SB_OP_CHAT_REASON: u32 = 42;
/// The string a as the kept reason (`gResource->GetString`).
pub const EE_SCREENS_SB_OP_REASON_STRING: u32 = 43;
/// The node's calamity's class's `GetCost()`.
pub const EE_SCREENS_SB_OP_CALAMITY_COST: u32 = 44;
/// The node's `Contains(a)` (a unit type).
pub const EE_SCREENS_SB_OP_NODE_CONTAINS: u32 = 45;
/// The form's `StartCalamity(mTechnology)`.
pub const EE_SCREENS_SB_OP_START_CALAMITY: u32 = 46;
/// `UIButton::ButtonUp()`.
pub const EE_SCREENS_SB_OP_BASE_BUTTON_UP: u32 = 47;
/// `ReleaseCapturedMouse()`.
pub const EE_SCREENS_SB_OP_RELEASE_MOUSE: u32 = 48;
/// The button's (virtual) `ButtonUp()`.
pub const EE_SCREENS_SB_OP_BUTTON_UP: u32 = 49;
/// The mouse is in the button's rectangle.
pub const EE_SCREENS_SB_OP_MOUSE_INSIDE: u32 = 50;

/// `EEMUnloadTransport(the transport's id, tile x, tile y)`.
pub const EE_SCREENS_SB_SEND_UNLOAD: i64 = 1;
/// `EEMStopActiveGroup`.
pub const EE_SCREENS_SB_SEND_STOP: i64 = 2;
/// `EEMDisband`.
pub const EE_SCREENS_SB_SEND_DISBAND: i64 = 3;
/// `EEMSelfDestruct`.
pub const EE_SCREENS_SB_SEND_SELF_DESTRUCT: i64 = 4;
/// `EEMPatrol(-1)`.
pub const EE_SCREENS_SB_SEND_PATROL: i64 = 5;
/// `EEMReplantFarms(-1)`.
pub const EE_SCREENS_SB_SEND_REPLANT: i64 = 6;
/// `EEMClearAirportCommands`.
pub const EE_SCREENS_SB_SEND_CLEAR_AIRPORT: i64 = 7;
/// `EEMExplore`.
pub const EE_SCREENS_SB_SEND_EXPLORE: i64 = 8;
/// `EEMLockUnlock(b != 0)`.
pub const EE_SCREENS_SB_SEND_LOCK: i64 = 9;
/// `EEMPackUnpack(b != 0)`.
pub const EE_SCREENS_SB_SEND_PACK: i64 = 10;
/// `EEMGoFanatic`.
pub const EE_SCREENS_SB_SEND_FANATIC: i64 = 11;
/// `EEMJITManufacturing`.
pub const EE_SCREENS_SB_SEND_JIT: i64 = 12;
/// `EEMExchangeResources(sell, resource, shift)`: b = resource * 2 + sell, c = shift.
pub const EE_SCREENS_SB_SEND_EXCHANGE: i64 = 13;
/// `EEMResearchTechnology(mUnitID, mTechnology, b != 0)`.
pub const EE_SCREENS_SB_SEND_RESEARCH: i64 = 14;
/// `EEMRemoveTechnology(mUnitID, mTechnology, false, b != 0)`.
pub const EE_SCREENS_SB_SEND_REMOVE: i64 = 15;

// the original's constants (the C++ host asserts them against its names)
/// `kUnitTypeTeleporter`.
pub const EE_SCREENS_SB_TYPE_TELEPORTER: i64 = 0x313;
/// `kUnitCategoryFortress`.
pub const EE_SCREENS_SB_CATEGORY_FORTRESS: i64 = 0x1d;
/// `kSoundButton1`, `kSoundButton2`.
pub const EE_SCREENS_SB_SOUND_BUTTON1: i64 = 0x207;
pub const EE_SCREENS_SB_SOUND_BUTTON2: i64 = 0x208;
/// `kWorldDeathmatchJitManufacturingDelay`, `kWorldRefusalMessageInterval`.
pub const EE_SCREENS_SB_WORLD_JIT_DELAY: i64 = 94;
pub const EE_SCREENS_SB_WORLD_REFUSAL_INTERVAL: i64 = 75;
/// `IDS_JUST_IN_TIME_MANUFACTURING_IS_NOT`, `IDS_INSUFFICIENT_POWER`.
pub const EE_SCREENS_SB_IDS_JIT_NOT_YET: i64 = 31892;
pub const EE_SCREENS_SB_IDS_INSUFFICIENT_POWER: i64 = 10062;

struct Ops<'a, H: ButtonHost>(&'a mut H);

impl<H: ButtonHost> Ops<'_, H> {
    fn o(&mut self, op: u32) -> i64 {
        self.0.op(op, 0, 0, 0)
    }
    fn a(&mut self, op: u32, a: i64) -> i64 {
        self.0.op(op, a, 0, 0)
    }
    fn send(&mut self, kind: i64, b: i64, c: i64) {
        self.0.op(EE_SCREENS_SB_OP_SEND, kind, b, c);
    }
    fn sound(&mut self, sound: i64) {
        self.a(EE_SCREENS_SB_OP_SOUND, sound);
    }
    /// The targeting mode's tail: the UI state, the cancel button, the group panel refreshed.
    fn mode(&mut self, ui_state: i64) {
        self.a(EE_SCREENS_SB_OP_UI_STATE, ui_state);
        self.o(EE_SCREENS_SB_OP_SHOW_CANCEL);
        self.o(EE_SCREENS_SB_OP_REFRESH_GROUP);
    }
    /// A refusal: the second sound, and the reason in the chat once per refusal interval.
    fn refuse(&mut self) {
        self.sound(EE_SCREENS_SB_SOUND_BUTTON2);
        let delay = self.a(EE_SCREENS_SB_OP_WORLD_LONG, EE_SCREENS_SB_WORLD_REFUSAL_INTERVAL) as u32;
        let now = self.o(EE_SCREENS_SB_OP_TIME) as u32;
        let last = self.o(EE_SCREENS_SB_OP_LAST_ERROR) as u32;
        if now.wrapping_sub(last) > delay {
            self.o(EE_SCREENS_SB_OP_CHAT_REASON);
            let now = self.o(EE_SCREENS_SB_OP_TIME);
            self.a(EE_SCREENS_SB_OP_SET_LAST_ERROR, now);
        }
    }
}

/// `EEUIFMGStatusButton::Command`: a command button pressed.
pub fn command(host: &mut impl ButtonHost) {
    let mut h = Ops(host);
    let button = h.o(EE_SCREENS_SB_OP_BUTTON) as u32;
    match button {
        0 => {
            h.a(EE_SCREENS_SB_OP_MAIN_GROUP, 0);
        }
        1 => {
            h.a(EE_SCREENS_SB_OP_MAIN_GROUP, 1);
        }
        // unload: a selected unit that can (a teleporter needs the unload cost in power)
        2 => 'unload: {
            let count = h.o(EE_SCREENS_SB_OP_GROUP_COUNT);
            if count < 0 {
                break 'unload;
            }
            let mut found = false;
            let mut i = 0;
            while i < count as u32 as i64 && !found {
                found = h.a(EE_SCREENS_SB_OP_UNIT, i) != 0
                    && !(h.a(EE_SCREENS_SB_OP_UNIT_TYPE, i) == EE_SCREENS_SB_TYPE_TELEPORTER
                        && h.0.op(EE_SCREENS_SB_OP_UNIT_POWER, i, 0, 0) < h.o(EE_SCREENS_SB_OP_UNLOAD_COST));
                i += 1;
            }
            if !found {
                break 'unload;
            }
            if h.o(EE_SCREENS_SB_OP_FIRST_SELECTED) != 0
                && h.o(EE_SCREENS_SB_OP_FIRST_CATEGORY) == EE_SCREENS_SB_CATEGORY_FORTRESS
                && h.o(EE_SCREENS_SB_OP_FIRST_TYPE) != EE_SCREENS_SB_TYPE_TELEPORTER
            {
                h.send(EE_SCREENS_SB_SEND_UNLOAD, 0, 0);
                break 'unload;
            }
            h.o(EE_SCREENS_SB_OP_LOCK_CURSOR);
            h.a(EE_SCREENS_SB_OP_POINTER, 0xc);
            h.mode(3);
        }
        3 | 0x1f | 0x1c => {
            let kind = match button {
                3 => EE_SCREENS_SB_SEND_STOP,
                0x1f => EE_SCREENS_SB_SEND_DISBAND,
                _ => EE_SCREENS_SB_SEND_SELF_DESTRUCT,
            };
            h.send(kind, 0, 0);
            h.a(EE_SCREENS_SB_OP_UI_STATE, 0);
        }
        0x1b | 0x22 => {
            h.o(EE_SCREENS_SB_OP_LOCK_CURSOR);
            h.o(EE_SCREENS_SB_OP_START_PATROL);
        }
        0x14 | 0x23 => {
            h.o(EE_SCREENS_SB_OP_LOCK_CURSOR);
            h.o(EE_SCREENS_SB_OP_START_FLARE);
        }
        0x12 => h.send(EE_SCREENS_SB_SEND_PATROL, 0, 0),
        0x13 => h.send(EE_SCREENS_SB_SEND_REPLANT, 0, 0),
        0x16 => h.send(EE_SCREENS_SB_SEND_CLEAR_AIRPORT, 0, 0),
        0x17 => h.send(EE_SCREENS_SB_SEND_EXPLORE, 0, 0),
        0x19 => h.send(EE_SCREENS_SB_SEND_LOCK, 1, 0),
        0x1a => h.send(EE_SCREENS_SB_SEND_LOCK, 0, 0),
        0x1d => {
            h.0.op(EE_SCREENS_SB_OP_PANEL, 9, 0x97, 1);
        }
        0x1e => {
            h.0.op(EE_SCREENS_SB_OP_PANEL, 8, 0x98, 1);
        }
        0x24 => h.send(EE_SCREENS_SB_SEND_PACK, 1, 0),
        0x25 => h.send(EE_SCREENS_SB_SEND_PACK, 0, 0),
        0x28 => h.send(EE_SCREENS_SB_SEND_FANATIC, 0, 0),
        // just-in-time manufacturing: refused early in a deathmatch
        0x29 | 0x2a => 'jit: {
            if h.o(EE_SCREENS_SB_OP_GAME_RESOURCES) == 4 {
                let clock = f32::from_bits(h.o(EE_SCREENS_SB_OP_GAME_CLOCK) as u32);
                let delay = h.a(EE_SCREENS_SB_OP_WORLD_LONG, EE_SCREENS_SB_WORLD_JIT_DELAY) as i32;
                if clock < delay.wrapping_mul(1000) as f32 {
                    h.a(EE_SCREENS_SB_OP_CHAT_STRING, EE_SCREENS_SB_IDS_JIT_NOT_YET);
                    break 'jit;
                }
            }
            h.send(EE_SCREENS_SB_SEND_JIT, 0, 0);
            h.o(EE_SCREENS_SB_OP_REFRESH_GROUP);
        }
        // the market's trades
        0x5dc..=0x5e3 => {
            if h.a(EE_SCREENS_SB_OP_CAN_AFFORD_TRADE, button as i64) != 0 {
                let decoded = h.a(EE_SCREENS_SB_OP_DECODE_TRADE, button as i64);
                if decoded >= 0 {
                    let shift = h.o(EE_SCREENS_SB_OP_SHIFT);
                    h.send(EE_SCREENS_SB_SEND_EXCHANGE, decoded, shift);
                }
            }
        }
        // the targeting modes
        _ => {
            if let Some(mode) = command_mode(button as _) {
                if mode.lock_cursor {
                    h.o(EE_SCREENS_SB_OP_LOCK_CURSOR);
                    h.a(EE_SCREENS_SB_OP_POINTER, mode.pointer as i64);
                }
                h.mode(mode.ui_state as i64);
            }
        }
    }
    h.sound(EE_SCREENS_SB_SOUND_BUTTON1);
}

/// `EEUIFMGStatusButton::ButtonUp`: a command, or a tech node (build, wall, gate, research with Shift's five,
/// research, or a calamity a selected unit can pay).
pub fn button_up(host: &mut impl ButtonHost) {
    let mut h = Ops(host);
    h.0.op(EE_SCREENS_SB_OP_PANEL, 9, 0x97, 0);
    h.0.op(EE_SCREENS_SB_OP_PANEL, 8, 0x98, 0);
    if h.o(EE_SCREENS_SB_OP_IS_COMMAND) != 0 {
        command(h.0);
    } else {
        tech_up(&mut h);
    }
    h.o(EE_SCREENS_SB_OP_BASE_BUTTON_UP);
}

fn tech_up<H: ButtonHost>(h: &mut Ops<H>) {
    let kind = h.o(EE_SCREENS_SB_OP_NODE_KIND);
    if kind == 5 {
        let cost = h.o(EE_SCREENS_SB_OP_CALAMITY_COST);
        let count = h.o(EE_SCREENS_SB_OP_GROUP_COUNT);
        let mut found = false;
        let mut i = 0;
        while count >= 0 && i < count as u32 as i64 && !found {
            found = h.a(EE_SCREENS_SB_OP_UNIT, i) != 0 && h.0.op(EE_SCREENS_SB_OP_UNIT_POWER, i, 1, 0) >= cost && {
                let t = h.a(EE_SCREENS_SB_OP_UNIT_TYPE, i);
                h.a(EE_SCREENS_SB_OP_NODE_CONTAINS, t) != 0
            };
            i += 1;
        }
        if found {
            h.sound(EE_SCREENS_SB_SOUND_BUTTON1);
            h.o(EE_SCREENS_SB_OP_START_CALAMITY);
        } else {
            h.a(EE_SCREENS_SB_OP_REASON_STRING, EE_SCREENS_SB_IDS_INSUFFICIENT_POWER);
            h.refuse();
        }
        return;
    }
    let class = h.o(EE_SCREENS_SB_OP_NODE_CLASS);
    if class > 0 {
        if h.a(EE_SCREENS_SB_OP_CLASS_PRESENT, class) == 0 {
            return;
        }
        if h.a(EE_SCREENS_SB_OP_CLASS_LAND_BUILDING, class) != 0 {
            let gate = |h: &mut Ops<H>| {
                h.o(EE_SCREENS_SB_OP_NODE_MAKES_UNIT) != 0 && h.a(EE_SCREENS_SB_OP_CLASS_GATE, class) != 0
            };
            if kind != 6 && !gate(h) {
                h.o(EE_SCREENS_SB_OP_BUILD_UNIT);
            } else if kind == 6 {
                h.o(EE_SCREENS_SB_OP_START_WALL);
            } else if gate(h) {
                h.o(EE_SCREENS_SB_OP_START_GATE);
            } else if kind == 10 {
                h.o(EE_SCREENS_SB_OP_START_PATROL);
            }
            return;
        }
        if h.o(EE_SCREENS_SB_OP_SHIFT) != 0 {
            if h.a(EE_SCREENS_SB_OP_CAN_PRODUCE, 5) != 0 {
                h.send(EE_SCREENS_SB_SEND_RESEARCH, 1, 0);
                h.sound(EE_SCREENS_SB_SOUND_BUTTON1);
            }
            return;
        }
    }
    if h.a(EE_SCREENS_SB_OP_CAN_PRODUCE, 1) != 0 {
        h.send(EE_SCREENS_SB_SEND_RESEARCH, 0, 0);
        h.sound(EE_SCREENS_SB_SOUND_BUTTON1);
    } else {
        h.refuse();
    }
}

/// `EEUIFMGStatusButton::MouseRButtonUp`: a command runs; a tech node in the button cancels one queued item
/// (Shift: five, for units only).
pub fn right_button_up(host: &mut impl ButtonHost) {
    let mut h = Ops(host);
    h.o(EE_SCREENS_SB_OP_RELEASE_MOUSE);
    if h.o(EE_SCREENS_SB_OP_IS_COMMAND) != 0 {
        h.o(EE_SCREENS_SB_OP_BUTTON_UP);
        return;
    }
    if h.o(EE_SCREENS_SB_OP_MOUSE_INSIDE) == 0 {
        return;
    }
    let kind = h.o(EE_SCREENS_SB_OP_NODE_KIND);
    if kind != 5 {
        let class = h.o(EE_SCREENS_SB_OP_NODE_CLASS);
        let mut shift = false;
        if class > 0 && h.a(EE_SCREENS_SB_OP_CLASS_PRESENT, class) != 0 {
            shift = h.a(EE_SCREENS_SB_OP_CLASS_LAND_BUILDING, class) == 0
                && kind != 6
                && h.a(EE_SCREENS_SB_OP_CLASS_GATE, class) == 0
                && kind != 10;
        }
        let shift = shift && h.o(EE_SCREENS_SB_OP_SHIFT) != 0;
        h.send(EE_SCREENS_SB_SEND_REMOVE, shift as i64, 0);
    }
    h.o(EE_SCREENS_SB_OP_BASE_BUTTON_UP);
}

#[cfg(test)]
mod tests;
