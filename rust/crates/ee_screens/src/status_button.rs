//! The main game form's command buttons' presses (`EEUIFMGStatusButton::ButtonUp`, `Command` and `MouseRButtonUp`;
//! `User Interface Thread/EEUIFMGStatusButton.cpp`).
//!
//! A button is either a command (an `EEButtonObject` index) or a tech tree node (a unit to build or train, a
//! technology to research, a calamity). The decisions are here; every read of the world and every call is one host
//! op ([`ButtonHost`], `EE_SCREENS_SB_OP_*`) on the button, asked where and as often as the original asked it. The
//! messages the presses send are one op with their kind (`EE_SCREENS_SB_SEND_*`) and arguments.

use core::ffi::{c_long, c_ulong};

use crate::command_panel::{
    self, command_mode, EeScreensButtonFacts, EeScreensButtonUnit, EE_SCREENS_BUTTON_FANATIC, EE_SCREENS_BUTTON_JIT,
    EE_SCREENS_BUTTON_POWER, EE_SCREENS_BUTTON_PRODUCE, EE_SCREENS_BUTTON_TRADE, EE_SCREENS_BUTTON_UNLOAD,
};

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
/// `EEDecodeTradeButton(a, ...)`: -1 when it fails (b 0), else (or b 1) the resource * 2 + the sell flag.
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

/// The button's `mShowCount = a != 0`.
pub const EE_SCREENS_SB_OP_SET_SHOW_COUNT: u32 = 51;
/// The button's `mCount`.
pub const EE_SCREENS_SB_OP_COUNT: u32 = 52;
/// The queue count a as the build queue's text ("%ld").
pub const EE_SCREENS_SB_OP_COUNT_TEXT: u32 = 53;
/// The node's data `GetCalamity()`.
pub const EE_SCREENS_SB_OP_NODE_CALAMITY: u32 = 54;
/// The calamity manager's class a's `GetCost()` (unchecked, as `CopyData`).
pub const EE_SCREENS_SB_OP_CALAMITY_CLASS_COST: u32 = 55;
/// `gUnitSetManager.GetSet(0x2f)->Contains(a)` (the fanatics' set).
pub const EE_SCREENS_SB_OP_IN_FANATIC_SET: u32 = 56;
/// The selected unit a's `IsCloaked()`.
pub const EE_SCREENS_SB_OP_UNIT_CLOAKED: u32 = 57;
/// The button's `mAvailable = a != 0`.
pub const EE_SCREENS_SB_OP_SET_AVAILABLE: u32 = 58;
/// Whether the button has its `mButtonObject`.
pub const EE_SCREENS_SB_OP_HAS_OBJECT: u32 = 59;
/// The button's `GetState()`.
pub const EE_SCREENS_SB_OP_STATE: u32 = 60;
/// The button's bitmaps: 1 normal is pressed, 2 focused is pressed, 4 no pressed bitmap.
pub const EE_SCREENS_SB_OP_BITMAPS: u32 = 61;
/// The icon drawn (a = `EE_SCREENS_BUTTON_ICON_*`): none (`mDrawIcon = false`), or the button object's pressed or
/// face bitmap set to the icon over the button's rectangle and `mDrawIcon = true`.
pub const EE_SCREENS_SB_OP_ICON: u32 = 62;
/// `UIButton::CopyData()`'s result.
pub const EE_SCREENS_SB_OP_BASE_COPY_DATA: u32 = 63;
/// The current player's `CanProduce(mTechnology, mUnitType, 1, true, 3, NULL)` (`CopyData`'s, no reason).
pub const EE_SCREENS_SB_OP_CAN_PRODUCE_QUIET: u32 = 64;

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

/// `EEUIFMGStatusButton::CopyData`: the queue count, whether the button is available under its test
/// ([`command_panel`]'s), and its icon; then `UIButton::CopyData`.
pub fn copy_data(host: &mut impl ButtonHost) -> i64 {
    let mut h = Ops(host);
    let mut facts = EeScreensButtonFacts::default();
    let mut units = Vec::new();
    h.a(EE_SCREENS_SB_OP_SET_SHOW_COUNT, 0);
    let test = if h.o(EE_SCREENS_SB_OP_IS_COMMAND) != 0 {
        command_panel::command_test(h.o(EE_SCREENS_SB_OP_BUTTON) as c_ulong)
    } else {
        let count = h.o(EE_SCREENS_SB_OP_COUNT);
        if count != 0 {
            h.a(EE_SCREENS_SB_OP_COUNT_TEXT, count);
            h.a(EE_SCREENS_SB_OP_SET_SHOW_COUNT, 1);
        }
        let kind = h.o(EE_SCREENS_SB_OP_NODE_KIND) as c_long;
        let calamity = h.o(EE_SCREENS_SB_OP_NODE_CALAMITY) as c_ulong;
        let is_calamity = command_panel::tech_is_calamity(kind, calamity);
        if is_calamity {
            facts.calamity_cost = h.a(EE_SCREENS_SB_OP_CALAMITY_CLASS_COST, calamity as i64) as c_long;
        }
        command_panel::tech_test(is_calamity, facts.calamity_cost)
    };
    match test {
        EE_SCREENS_BUTTON_FANATIC | EE_SCREENS_BUTTON_UNLOAD | EE_SCREENS_BUTTON_POWER => {
            let count = h.o(EE_SCREENS_SB_OP_GROUP_COUNT);
            facts.has_group = count >= 0;
            facts.unload_cost = h.o(EE_SCREENS_SB_OP_UNLOAD_COST) as c_long;
            for i in 0..count.max(0) as u32 as i64 {
                let mut unit =
                    EeScreensButtonUnit { present: h.a(EE_SCREENS_SB_OP_UNIT, i) != 0, ..Default::default() };
                if unit.present {
                    unit.unit_type = h.a(EE_SCREENS_SB_OP_UNIT_TYPE, i) as c_ulong;
                    unit.power = h.0.op(EE_SCREENS_SB_OP_UNIT_POWER, i, 1, 0) as c_long;
                    if test == EE_SCREENS_BUTTON_FANATIC {
                        unit.in_set = h.a(EE_SCREENS_SB_OP_IN_FANATIC_SET, unit.unit_type as i64) != 0;
                        unit.cloaked = h.a(EE_SCREENS_SB_OP_UNIT_CLOAKED, i) != 0;
                    } else if test == EE_SCREENS_BUTTON_POWER {
                        unit.in_node = h.a(EE_SCREENS_SB_OP_NODE_CONTAINS, unit.unit_type as i64) != 0;
                    }
                }
                units.push(unit);
            }
        }
        EE_SCREENS_BUTTON_TRADE => {
            let button = h.o(EE_SCREENS_SB_OP_BUTTON);
            facts.affordable = h.a(EE_SCREENS_SB_OP_CAN_AFFORD_TRADE, button) != 0;
        }
        EE_SCREENS_BUTTON_JIT => {
            facts.game_resources = h.o(EE_SCREENS_SB_OP_GAME_RESOURCES) as c_long;
            facts.clock = f32::from_bits(h.o(EE_SCREENS_SB_OP_GAME_CLOCK) as u32);
            facts.jit_delay = h.a(EE_SCREENS_SB_OP_WORLD_LONG, EE_SCREENS_SB_WORLD_JIT_DELAY) as c_ulong;
        }
        EE_SCREENS_BUTTON_PRODUCE => facts.can_produce = h.o(EE_SCREENS_SB_OP_CAN_PRODUCE_QUIET) != 0,
        _ => {}
    }
    let available = command_panel::available(test, &facts, &units);
    h.a(EE_SCREENS_SB_OP_SET_AVAILABLE, available as i64);
    let has_object = h.o(EE_SCREENS_SB_OP_HAS_OBJECT) != 0;
    let button = h.o(EE_SCREENS_SB_OP_BUTTON) as c_ulong;
    let state = h.o(EE_SCREENS_SB_OP_STATE) as c_ulong;
    let bitmaps = h.o(EE_SCREENS_SB_OP_BITMAPS);
    let icon = command_panel::icon(has_object, button, state, bitmaps & 1 != 0, bitmaps & 2 != 0, bitmaps & 4 != 0);
    h.a(EE_SCREENS_SB_OP_ICON, icon as i64);
    h.o(EE_SCREENS_SB_OP_BASE_COPY_DATA)
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

/// The help text's held text = a text of kind a (`EE_SCREENS_SB_TEXT_*`) with b; its length in bytes, -1 when there is
/// none (a button without a hot key).
pub const EE_SCREENS_SB_OP_HELP_TEXT: u32 = 65;
/// The held text's bytes copied to a (b bytes at most); its length.
pub const EE_SCREENS_SB_OP_HELP_TEXT_READ: u32 = 66;
/// The help string = the b bytes at a (UTF-8).
pub const EE_SCREENS_SB_OP_HELP_OUT: u32 = 67;
/// `EEGetTradePrice(current player, a, b != 0)`.
pub const EE_SCREENS_SB_OP_TRADE_PRICE: u32 = 68;
/// The selected group's unit a exists and its class is a priest.
pub const EE_SCREENS_SB_OP_UNIT_PRIEST: u32 = 69;
/// `EEGetConversionCost` of the selected group's unit a.
pub const EE_SCREENS_SB_OP_CONVERSION_COST: u32 = 70;
/// The button's tech tree node exists.
pub const EE_SCREENS_SB_OP_NODE_PRESENT: u32 = 71;
/// The node's `mData->mNameID`.
pub const EE_SCREENS_SB_OP_NODE_NAME_ID: u32 = 72;
/// The current player's upgrade of the class at table index a has a name.
pub const EE_SCREENS_SB_OP_CLASS_UPGRADE_NAMED: u32 = 73;
/// The class at table index a's `GetPopulationCost()`.
pub const EE_SCREENS_SB_OP_CLASS_POPULATION: u32 = 74;
/// The class at table index a's `mNameStringID`.
pub const EE_SCREENS_SB_OP_CLASS_NAME_ID: u32 = 75;

/// `gResource->GetString(b)`.
pub const EE_SCREENS_SB_TEXT_STRING: i64 = 1;
/// The command's button object's hot key's name (none without an object or hot key).
pub const EE_SCREENS_SB_TEXT_OBJECT_HOT_KEY: i64 = 2;
/// The node's hot key's name (none without one).
pub const EE_SCREENS_SB_TEXT_NODE_HOT_KEY: i64 = 3;
/// The current player's upgrade name of the class at table index b.
pub const EE_SCREENS_SB_TEXT_UPGRADE_NAME: i64 = 4;
/// The node's `GetCostString(mUnitType, 1.0f)`.
pub const EE_SCREENS_SB_TEXT_COST: i64 = 5;

/// `IDS_TRADE_BUY`, `IDS_TRADE_SELL`, `IDS_TRADE_FOR`, `IDS_RESOURCE_NAME_BASE`, `IDS_RESOURCE_NAME_GOLD`,
/// `IDS_IMPROVEMENT_POWER`, `IDS_POP_COST`.
pub const EE_SCREENS_SB_IDS_TRADE_BUY: i64 = 5538;
pub const EE_SCREENS_SB_IDS_TRADE_SELL: i64 = 5539;
pub const EE_SCREENS_SB_IDS_TRADE_FOR: i64 = 5540;
pub const EE_SCREENS_SB_IDS_RESOURCE_NAME_BASE: i64 = 30114;
pub const EE_SCREENS_SB_IDS_RESOURCE_NAME_GOLD: i64 = 30119;
pub const EE_SCREENS_SB_IDS_IMPROVEMENT_POWER: i64 = 30024;
pub const EE_SCREENS_SB_IDS_POP_COST: i64 = 30469;

impl<H: ButtonHost> Ops<'_, H> {
    /// A text of `kind` with `arg`, `None` when there is none.
    fn text(&mut self, kind: i64, arg: i64) -> Option<Vec<u8>> {
        let len = self.0.op(EE_SCREENS_SB_OP_HELP_TEXT, kind, arg, 0);
        if len < 0 {
            return None;
        }
        let mut out = vec![0u8; len as usize];
        self.0.op(EE_SCREENS_SB_OP_HELP_TEXT_READ, out.as_mut_ptr() as i64, len, 0);
        Some(out)
    }
    fn string(&mut self, id: i64) -> Vec<u8> {
        self.text(EE_SCREENS_SB_TEXT_STRING, id).unwrap_or_default()
    }
}

/// `" (" + name + ")"` after `out` when there is a hot key.
fn hot_key(out: &mut Vec<u8>, name: Option<Vec<u8>>) {
    if let Some(name) = name {
        out.extend_from_slice(b" (");
        out.extend_from_slice(&name);
        out.push(b')');
    }
}

/// `EEUIFMGStatusButton::GetHelpString` (0x26ebdf): the tooltip, handed to the host (`EE_SCREENS_SB_OP_HELP_OUT`);
/// always true. A command: the market's trade and its price, or the command's help (a priest's conversion cost for
/// the flares), then its hot key. A technology: the name, the cost (a calamity's power), the population cost, the
/// unit's name, the hot key.
pub fn help_string(host: &mut impl ButtonHost) -> bool {
    let mut h = Ops(host);
    let mut out: Vec<u8> = Vec::new();
    if h.o(EE_SCREENS_SB_OP_IS_COMMAND) != 0 {
        let button = h.o(EE_SCREENS_SB_OP_BUTTON) as u32;
        if (0x5dc..=0x5e3).contains(&button) {
            // the decode's outputs, resource * 2 + sell, whether it accepted the button or not (b 1)
            let decoded = h.0.op(EE_SCREENS_SB_OP_DECODE_TRADE, button as i64, 1, 0);
            let (resource, sell) = (decoded >> 1, decoded & 1 != 0);
            let name = h.string(if sell { EE_SCREENS_SB_IDS_TRADE_SELL } else { EE_SCREENS_SB_IDS_TRADE_BUY });
            let price = h.0.op(EE_SCREENS_SB_OP_TRADE_PRICE, resource, sell as i64, 0) as i32;
            let cost = h.string(resource + EE_SCREENS_SB_IDS_RESOURCE_NAME_BASE);
            // "%s %d %s"
            out.extend_from_slice(&name);
            out.extend_from_slice(b" 100 ");
            out.extend_from_slice(&cost);
            let cost = h.string(EE_SCREENS_SB_IDS_TRADE_FOR);
            out.push(b' ');
            out.extend_from_slice(&cost);
            let cost = h.string(EE_SCREENS_SB_IDS_RESOURCE_NAME_GOLD);
            // " %ld %s"
            out.extend_from_slice(format!(" {price} ").as_bytes());
            out.extend_from_slice(&cost);
        } else {
            out = h.string(command_panel::command_help_string(button as c_ulong) as i64);
            if command_panel::command_help_priest(button as c_ulong)
                && h.a(EE_SCREENS_SB_OP_GROUP_COUNT, 0) > 0
                && h.a(EE_SCREENS_SB_OP_UNIT_PRIEST, 0) != 0
            {
                out.extend_from_slice(b": ");
                let power = h.string(EE_SCREENS_SB_IDS_IMPROVEMENT_POWER);
                out.extend_from_slice(&power);
                out.extend_from_slice(b": ");
                let cost = h.a(EE_SCREENS_SB_OP_CONVERSION_COST, 0) as i32;
                out.extend_from_slice(cost.to_string().as_bytes());
            }
        }
        let name = h.text(EE_SCREENS_SB_TEXT_OBJECT_HOT_KEY, 0);
        hot_key(&mut out, name);
    } else if h.o(EE_SCREENS_SB_OP_NODE_PRESENT) != 0 {
        let index = h.o(EE_SCREENS_SB_OP_NODE_CLASS);
        let has_class = index > 0 && h.a(EE_SCREENS_SB_OP_CLASS_PRESENT, index) != 0;
        let (named, population) = if has_class {
            let named = h.a(EE_SCREENS_SB_OP_CLASS_UPGRADE_NAMED, index) != 0;
            (named, h.a(EE_SCREENS_SB_OP_CLASS_POPULATION, index) as c_long)
        } else {
            (false, 0)
        };
        let kind = h.o(EE_SCREENS_SB_OP_NODE_KIND) as c_long;
        let calamity_cost = if kind == 5 { h.o(EE_SCREENS_SB_OP_CALAMITY_COST) as c_long } else { 0 };
        let makes_unit = h.o(EE_SCREENS_SB_OP_NODE_MAKES_UNIT) != 0;
        let name_id = h.o(EE_SCREENS_SB_OP_NODE_NAME_ID) as c_long;
        let plan = command_panel::tech_help(has_class, named, population, kind, calamity_cost, makes_unit, name_id);
        out = match plan.name {
            1 => h.text(EE_SCREENS_SB_TEXT_UPGRADE_NAME, index).unwrap_or_default(),
            0 => {
                let id = h.a(EE_SCREENS_SB_OP_CLASS_NAME_ID, index);
                h.string(id)
            }
            // "%s"
            _ => h.string(name_id as i64),
        };
        out.extend_from_slice(b": ");
        if plan.power {
            let power = h.string(EE_SCREENS_SB_IDS_IMPROVEMENT_POWER);
            out.extend_from_slice(&power);
            out.extend_from_slice(format!(": {calamity_cost}").as_bytes());
        } else {
            let cost = h.text(EE_SCREENS_SB_TEXT_COST, 0).unwrap_or_default();
            out.extend_from_slice(&cost);
        }
        if plan.population {
            // " %s: %ld" of the (unsigned long) count
            let pop = h.string(EE_SCREENS_SB_IDS_POP_COST);
            out.push(b' ');
            out.extend_from_slice(&pop);
            out.extend_from_slice(format!(": {}", population as c_ulong as i32).as_bytes());
        }
        if plan.unit_name {
            let name = h.string(name_id as i64);
            out.push(b' ');
            out.extend_from_slice(&name);
        }
        let name = h.text(EE_SCREENS_SB_TEXT_NODE_HOT_KEY, 0);
        hot_key(&mut out, name);
    }
    h.0.op(EE_SCREENS_SB_OP_HELP_OUT, out.as_ptr() as i64, out.len() as i64, 0);
    true
}

#[cfg(test)]
mod tests;
