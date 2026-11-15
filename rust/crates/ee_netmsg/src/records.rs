//! The game messages without a C++ class of their own (RUST_PLAN R5b.4, game messages round 3): `EEMRecord`
//! (ee/EEMessage.h) carries any of these types: its codec class's fixed members after the selection id, packed in
//! wire order in a byte record, and at most one string (round 4: the chat lines, the lobby's options and slots),
//! which the C++ object keeps beside the record (`mString`); round 5: at most one list (records of a size: unit ids,
//! civilization upgrades) or blob (the box selection's compressed units), kept beside it too (`mList`), and members
//! that are not on the wire (`locals`: the box selection's rectangle and unit type, which the scenario editor reads).
//! The C++ asks here for the layout, the type's flags, its send-to-all-and-wait member, its response rule and its
//! debug label. The classes these types had (`EEMCheatCode`,
//! `EEMPayTribute`, ...) were touched only by their senders' constructors, their own `Execute` and the capture (which
//! reads every message through its codec slots).

use crate::classes::*;
use crate::schema::{Field, Kind};
use std::ffi::c_char;

/// `GetCanSendWhilePaused` answers true.
pub const EE_NETMSG_RECORD_PAUSED: u8 = 1;
/// A sender's message may be sent while the local player is out (`EEMessage::mCanSendWhenOut`; an imported one never).
pub const EE_NETMSG_RECORD_WHEN_OUT: u8 = 2;
/// The record's room: at least the largest record type's fixed members after the selection id
/// (`EEMFullGameOptions`' 48 bytes).
pub const EE_NETMSG_RECORD_BYTES: usize = 64;

const PAUSED: u8 = EE_NETMSG_RECORD_PAUSED;
const WHEN_OUT: u8 = EE_NETMSG_RECORD_WHEN_OUT;

/// One record type: its codec class, its message type when the class has several (0: the class's one type in
/// `TYPES`), its flags, its label (NUL-terminated: `GetMessageLabel` hands it to C++); round 5: its members that are
/// not on the wire (after the wire's), its send-to-all-and-wait member (0: none) and its response rule (`IsResponse`:
/// the response's type, 0 for none, then this message's member and the response's, equal as 32-bit integers).
pub struct RecordType {
    pub class: u32,
    pub message_type: u8,
    pub flags: u8,
    pub label: &'static str,
    pub locals: &'static [Field],
    pub wait_field: u8,
    pub response: (u8, u8, u8),
}

const fn rec(class: u32, flags: u8, label: &'static str) -> RecordType {
    RecordType { class, message_type: 0, flags, label, locals: &[], wait_field: 0, response: (0, 0, 0) }
}

/// A record type of a class that several message types share (`EEMAddUnitToSelectedGroup`'s toggle and Always
/// Add), or of a type that is in no `TYPES` row (`EEMEndCinematic`'s 0x9d: the factory builds none).
const fn rec_type(class: u32, message_type: u8, flags: u8, label: &'static str) -> RecordType {
    RecordType { class, message_type, flags, label, locals: &[], wait_field: 0, response: (0, 0, 0) }
}

/// The box selection's members that are not on the wire: the picked rectangle and the unit type (the scenario
/// editor's lasso reads both from its message).
static SELECT_UNITS_LOCALS: [Field; 2] = [Field::new("mRectangle", Kind::Raw(16)), Field::new("mL30", Kind::I32)];

/// The box selections (0x4e, and the two the lasso builds with Shift and Control): sent paused and while out.
const fn select_units(message_type: u8, label: &'static str) -> RecordType {
    RecordType {
        locals: &SELECT_UNITS_LOCALS,
        ..rec_type(EE_NETMSG_SELECT_UNITS, message_type, PAUSED | WHEN_OUT, label)
    }
}

/// The record types, by codec class (a `rec` row: the class's one message type in `TYPES`; a `rec_type` row names
/// its type).
pub static RECORD_TYPES: [RecordType; 108] = [
    rec(EE_NETMSG_ADD_REMOVE_RESOURCES, PAUSED, "Add Remove Resources\0"),
    rec(EE_NETMSG_ATTACK_PLAYERS, 0, "Attack Players\0"),
    rec(EE_NETMSG_CAMERA_LOCATION_INFO, 0, "Camera Location\0"),
    rec(EE_NETMSG_CHANGE_SELECTED_UNIT_COLLISION_SPHERE, 0, "ChangeSelectedUnitCollisionSphere\0"),
    rec(EE_NETMSG_CHEAT_CODE, 0, "Cheat Code\0"),
    rec(EE_NETMSG_EDICT_PRE_PROPOSAL, 0, "Edict Pre-Proposal\0"),
    rec(EE_NETMSG_EDICT_PRE_PROPOSAL_RESPONSE, 0, "Edict Pre-Proposal Response\0"),
    rec(EE_NETMSG_ENTER_SHIFT_MODE, 0, "Enter Shift Mode\0"),
    rec(EE_NETMSG_GAME_SPEED, 0, "Game Speed\0"),
    rec(EE_NETMSG_HALT_SELECTED_GROUP, 0, "Halt Selected Group\0"),
    rec(EE_NETMSG_INCREMENT_EPIC, 0, "Increment Epic\0"),
    rec(EE_NETMSG_INCREMENT_RALLY_TIMER, 0, "Increment Rally Timer\0"),
    rec(EE_NETMSG_LAG_STATUS, WHEN_OUT, "Lag Status\0"),
    rec(EE_NETMSG_LOAD_MAIN_SCREEN, PAUSED | WHEN_OUT, "Load Main Screen\0"),
    rec(EE_NETMSG_LOAD_MAIN_SCREEN_RESPONSE, PAUSED | WHEN_OUT, "Load Main Screen Response\0"),
    rec(EE_NETMSG_LOAD_OR_SAVE_GAME_RESPONSE, PAUSED | WHEN_OUT, "Load or Save Repsponse\0"),
    rec(EE_NETMSG_LOAD_PERCENT, 0, "Load Percent\0"),
    rec(EE_NETMSG_LOAD_UNIT_DATA, PAUSED, "Load Unit Data\0"),
    rec(EE_NETMSG_MIGRATION_SLOT_INFO, WHEN_OUT, "Migration Slot Info\0"),
    rec(EE_NETMSG_PAY_TRIBUTE, PAUSED, "Pay Tribute\0"),
    rec(EE_NETMSG_PLAYER_INFORMATION, PAUSED, "Player Information\0"),
    rec(EE_NETMSG_RECORD_FRAMES, 0, "Record Frames\0"),
    rec(EE_NETMSG_REMOVE_IMPROVEMENT, 0, "Remove Improvement\0"),
    rec(EE_NETMSG_REMOVE_UNIT_FROM_SELECTED_GROUP, PAUSED | WHEN_OUT, "Remove Unit From Selected Group\0"),
    rec(EE_NETMSG_RETURN_TO_BASE, 0, "Return To Base\0"),
    rec(EE_NETMSG_SAVED_GROUP_ADD_SELECTION, PAUSED | WHEN_OUT, "Add Selection To Saved Group\0"),
    rec(EE_NETMSG_SAVED_GROUP_CLEAR, 0, "Clear Saved Group\0"),
    rec(EE_NETMSG_SAVED_GROUP_CREATE, 0, "Create Saved Group\0"),
    rec(EE_NETMSG_SAVED_GROUP_SELECT, PAUSED | WHEN_OUT, "Select Saved Group\0"),
    rec(EE_NETMSG_SCRAMBLE_AIRCRAFT, 0, "Scramble Aircraft\0"),
    rec(EE_NETMSG_SELECT_CUSTOM_PLAYER_FOR_CHAT, PAUSED, "Select Custom Player For Chat\0"),
    rec(EE_NETMSG_SELECT_PLAYER_CHAT_TYPE, PAUSED, "Select Player Chat Type\0"),
    rec(EE_NETMSG_SELECT_UNIT, PAUSED | WHEN_OUT, "Select Unit\0"),
    rec(EE_NETMSG_SEND_FLARE, 0, "Send Flare\0"),
    rec(EE_NETMSG_SET_AIRPORT_COMMAND, 0, "Set Airport Command\0"),
    rec(EE_NETMSG_SET_DIPLOMATIC_STANCE, PAUSED, "Set Diplomatic Stance\0"),
    rec(EE_NETMSG_SET_WATER_LEVEL, 0, "Set Water Level\0"),
    rec(EE_NETMSG_SNAP_TO_DAYLIGHT, 0, "Snap to Daylight\0"),
    rec(EE_NETMSG_TOGGLE_COLLISION_DETECTION, 0, "Toggle Collision Detection\0"),
    rec(EE_NETMSG_TOGGLE_DAY_NIGHT, 0, "Toggle Day/Night\0"),
    rec(EE_NETMSG_UPDATE_PING_STAT, PAUSED, "Update Ping Stat\0"),
    rec(EE_NETMSG_ZOOM_INFO, 0, "Zoom Info\0"),
    // the member-less ones
    rec(EE_NETMSG_CLEAR_AIRPORT_COMMANDS, 0, "Clear Airport Commands\0"),
    rec(EE_NETMSG_DISBAND, 0, "Disband\0"),
    rec(EE_NETMSG_GAME_RESTARTING, PAUSED | WHEN_OUT, "Game Restarting\0"),
    rec(EE_NETMSG_GO_FANATIC, 0, "Go Fanatic\0"),
    rec(EE_NETMSG_J_I_T_MANUFACTURING, 0, "JIT Manufacturing\0"),
    rec(EE_NETMSG_RESIGN, PAUSED | WHEN_OUT, "Resign\0"),
    rec(EE_NETMSG_SELF_DESTRUCT, 0, "Self Destruct\0"),
    rec(EE_NETMSG_UNSELECT_ALL_UNITS, PAUSED | WHEN_OUT, "Unselect All Units\0"),
    rec(EE_NETMSG_EXPLORE, 0, "Explore\0"),
    rec(EE_NETMSG_STOP_ACTIVE_GROUP, 0, "Stop Active Group\0"),
    rec(EE_NETMSG_MAKE_WORLD_VISIBLE, 0, "Make World Visible\0"),
    // round 4: the unit orders (the physics' order log reads them through the record)
    rec(EE_NETMSG_ADD_PATROL_POINT, 0, "Add Patrol Point\0"),
    rec(EE_NETMSG_ATTACK_GROUND, 0, "Attack Ground\0"),
    rec(EE_NETMSG_ATTACK_UNIT, 0, "Attack Unit\0"),
    rec(EE_NETMSG_BUILD_GATE, 0, "Build Gate\0"),
    rec(EE_NETMSG_BUILD_UNIT, 0, "Build Unit\0"),
    rec(EE_NETMSG_BUILD_WALL, 0, "Build Wall\0"),
    rec(EE_NETMSG_CHANGE_FORMATION, 0, "ChangeFormation\0"),
    rec(EE_NETMSG_DROPOFF_RESOURCE, 0, "Dropoff Resource\0"),
    rec(EE_NETMSG_GATHER_RESOURCE, 0, "Gather Resource\0"),
    rec(EE_NETMSG_HEAL_UNIT, 0, "Heal Unit\0"),
    rec(EE_NETMSG_IMMEDIATE_CARGO_UNLOAD, 0, "Immediate Cargo Unload\0"),
    rec(EE_NETMSG_IMPROVE_UNIT, 0, "Upgrade Unit\0"),
    rec(EE_NETMSG_LOAD_GARRISON, 0, "Load Unit(s) Into Garrison\0"),
    rec(EE_NETMSG_LOAD_TRANSPORT, 0, "Load Unit(s) Into Transport\0"),
    rec(EE_NETMSG_PACK_UNPACK, 0, "Pack Unpack\0"),
    rec(EE_NETMSG_PATH_TO, 0, "Path To\0"),
    rec(EE_NETMSG_REPAIR_UNIT, 0, "Repair Unit\0"),
    rec(EE_NETMSG_RESEARCH_TECHNOLOGY, 0, "Research Technology\0"),
    rec(EE_NETMSG_SET_RALLY_GOAL, 0, "Set Rally Goal\0"),
    rec(EE_NETMSG_SET_UNIT_BEHAVIOR, 0, "Set Unit Behavior\0"),
    rec(EE_NETMSG_UNLOAD_TRANSPORT, 0, "Unload Unit(s) From Transport\0"),
    // round 4: the Lua AI's stack messages with members
    rec(EE_NETMSG_EXCHANGE_RESOURCES, 0, "Exchange Resources\0"),
    rec(EE_NETMSG_HACK_A_NEW_UNIT, 0, "Hack A New Unit\0"),
    rec(EE_NETMSG_KILL_UNIT, 0, "Delete Unit\0"),
    rec(EE_NETMSG_LOCK_UNLOCK, 0, "Lock Unlock\0"),
    rec(EE_NETMSG_REMOVE_TECHNOLOGY, 0, "Remove Technology\0"),
    rec(EE_NETMSG_SMART_FARMERS, 0, "Smart Farmers\0"),
    // round 4: the out-of-line constructors elsewhere, the other readers' types, the selection toggles
    rec(EE_NETMSG_CAST_CALAMITY, 0, "Cast Calamity\0"),
    rec(EE_NETMSG_CIV_TIMER_EXPIRED, 0, "Civ Timer Expired\0"),
    rec(EE_NETMSG_EDICT_PROPOSAL, 0, "Edict Proposal\0"),
    rec(EE_NETMSG_EDICT_VOTE, 0, "Edict Vote\0"),
    rec(EE_NETMSG_EXIT_SHIFT_MODE, 0, "Exit Shift Mode\0"),
    rec(EE_NETMSG_NEW_EDICT_ENACTED, 0, "New Edict Enabled\0"),
    rec(EE_NETMSG_PATROL, 0, "Patrol\0"),
    rec(EE_NETMSG_REPLANT_FARMS, 0, "Replant Farms\0"),
    rec(EE_NETMSG_REPORT_STATISTICS, 0, "Report Statistics\0"),
    rec(EE_NETMSG_SELECT_PATROL_GROUP, 0, "Select Patrol Group\0"),
    rec(EE_NETMSG_START_LOADING_RESPONSE, PAUSED | WHEN_OUT, "Start Loading Response\0"),
    rec_type(EE_NETMSG_ADD_UNIT_TO_SELECTED_GROUP, 0x49, PAUSED | WHEN_OUT, "Add Unit To Selected Group\0"),
    rec_type(EE_NETMSG_ADD_UNIT_TO_SELECTED_GROUP, 0x76, PAUSED | WHEN_OUT, "Always Add Units To Selected Group\0"),
    rec_type(EE_NETMSG_END_CINEMATIC, 0x9d, 0, "End Cinematic\0"),
    // round 4: the types with one string member
    rec(EE_NETMSG_CHAT, 0, "Chat\0"),
    rec(EE_NETMSG_FULL_GAME_OPTIONS, 0, "Full Game Options\0"),
    rec(EE_NETMSG_FULL_PLAYER_INFORMATION, PAUSED, "Full Player Information\0"),
    rec(EE_NETMSG_GAME_OPTIONS, 0, "Game Options\0"),
    rec(EE_NETMSG_IN_GAME_CHAT, PAUSED | WHEN_OUT, "In Game Chat\0"),
    rec(EE_NETMSG_PLAYER_SLOT_INFO, WHEN_OUT, "Player Slot Info\0"),
    // round 5: the eight with a list, a blob or a rule
    select_units(0x4e, "Select Units\0"),
    select_units(0x4f, "Add Units To Selected Group\0"),
    select_units(0x50, "Remove Units From Selected Group\0"),
    rec(EE_NETMSG_SELECT_THESE_UNITS, PAUSED | WHEN_OUT, "Select These Units\0"),
    rec_type(EE_NETMSG_CIV_UPGRADES, 0x7e, PAUSED, "Civilization Builder requesting upgrades from server\0"),
    rec_type(
        EE_NETMSG_CIV_UPGRADES,
        0x7f,
        PAUSED | WHEN_OUT,
        "Civilization Builder upgrades validated and purchased from server\0",
    ),
    // the wait id is member 6; the response (0x9a) answers this request's tick (member 3) with its member 1
    RecordType {
        wait_field: 6,
        response: (0x9a, 3, 1),
        ..rec(EE_NETMSG_LOAD_OR_SAVE_GAME, PAUSED | WHEN_OUT, "Load or Save\0")
    },
    rec(EE_NETMSG_START_LOADING, PAUSED, "Start Loading\0"),
];

/// The record type of a message type, if it is one: a row naming the type, else the row of the type's class.
pub fn record_type(message_type: u8) -> Option<&'static RecordType> {
    if let Some(r) = RECORD_TYPES.iter().find(|r| r.message_type != 0 && r.message_type == message_type) {
        return Some(r);
    }
    let class = TYPES.iter().find(|t| t.0 == message_type)?.1;
    RECORD_TYPES.iter().find(|r| r.class == class && r.message_type == 0)
}

/// A record member: its kind (`EE_NETMSG_RECORD_FIELD_*`, NONE past the last member), its offset in the record and
/// its size.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct EeNetMsgRecordField {
    pub kind: u8,
    pub offset: u32,
    pub size: u32,
}

/// No such member.
pub const EE_NETMSG_RECORD_FIELD_NONE: u8 = 0;
/// An integer (U8, U16, I32, U32): a sender's value is truncated to its size.
pub const EE_NETMSG_RECORD_FIELD_INTEGER: u8 = 1;
/// A `bool`: a sender's value is stored as value != 0.
pub const EE_NETMSG_RECORD_FIELD_BOOL: u8 = 2;
/// A `float`.
pub const EE_NETMSG_RECORD_FIELD_FLOAT: u8 = 3;
/// A `GE2DPoint` (two floats).
pub const EE_NETMSG_RECORD_FIELD_POINT: u8 = 4;
/// A `GEVector` (three floats).
pub const EE_NETMSG_RECORD_FIELD_VECTOR: u8 = 5;
/// The type's string (`UWideString`; the C++ object's `mString`, not in the record: offset and size 0 of it).
pub const EE_NETMSG_RECORD_FIELD_STRING: u8 = 6;
/// The type's list (the C++ object's `mList`, not in the record: offset 0, size one record's).
pub const EE_NETMSG_RECORD_FIELD_LIST: u8 = 7;
/// The type's blob (the C++ object's `mList`, as many bytes as the member before it says: offset 0, size 1).
pub const EE_NETMSG_RECORD_FIELD_BLOB: u8 = 8;
/// A member that is not on the wire, as bytes (the box selection's rectangle).
pub const EE_NETMSG_RECORD_FIELD_RAW: u8 = 9;

fn field_kind(kind: Kind) -> Option<u8> {
    Some(match kind {
        Kind::U8 | Kind::U16 | Kind::I32 | Kind::U32 => EE_NETMSG_RECORD_FIELD_INTEGER,
        Kind::Bool => EE_NETMSG_RECORD_FIELD_BOOL,
        Kind::F32 => EE_NETMSG_RECORD_FIELD_FLOAT,
        Kind::Point2 => EE_NETMSG_RECORD_FIELD_POINT,
        Kind::Vec3 => EE_NETMSG_RECORD_FIELD_VECTOR,
        Kind::WStr => EE_NETMSG_RECORD_FIELD_STRING,
        Kind::Records8(_) | Kind::Records32(_) => EE_NETMSG_RECORD_FIELD_LIST,
        Kind::Blob => EE_NETMSG_RECORD_FIELD_BLOB,
        Kind::Raw(_) => EE_NETMSG_RECORD_FIELD_RAW,
        Kind::Inputs => return None,
    })
}

/// The size a member reports (a list's: one record's; a blob's: 1; a string's: 0).
fn field_size(kind: Kind) -> usize {
    match kind {
        Kind::Records8(n) | Kind::Records32(n) => n as usize,
        Kind::Blob => 1,
        k => k.fixed_size().unwrap_or(0),
    }
}

/// The bytes a member takes in the record (the string, the list and the blob are beside it).
pub fn record_bytes(kind: Kind) -> usize {
    match kind {
        Kind::WStr | Kind::Records8(_) | Kind::Records32(_) | Kind::Blob => 0,
        k => k.fixed_size().unwrap_or(0),
    }
}

fn member(fields: &[&Field], index: u32) -> EeNetMsgRecordField {
    let none = EeNetMsgRecordField { kind: EE_NETMSG_RECORD_FIELD_NONE, offset: 0, size: 0 };
    if index == 0 || index as usize >= fields.len() {
        return none;
    }
    let offset: usize = fields[1..index as usize].iter().map(|f| record_bytes(f.kind)).sum();
    let f = fields[index as usize];
    match field_kind(f.kind) {
        Some(kind) => EeNetMsgRecordField { kind, offset: offset as u32, size: field_size(f.kind) as u32 },
        None => none,
    }
}

/// Member `index` of a record class (1 = the first after the selection id), the wire's only.
pub fn record_field(class_id: u32, index: u32) -> EeNetMsgRecordField {
    let fields: Vec<&Field> = CLASSES.get(class_id as usize).map_or(Vec::new(), |c| c.fields.iter().collect());
    member(&fields, index)
}

/// Member `index` of a record type: the wire's, then the type's locals (after the wire's in the record).
pub fn record_member(message_type: u8, index: u32) -> EeNetMsgRecordField {
    let Some(r) = record_type(message_type) else { return member(&[], index) };
    let fields: Vec<&Field> = CLASSES[r.class as usize].fields.iter().chain(r.locals.iter()).collect();
    member(&fields, index)
}

/// The codec class of a record message type, or `u32::MAX` for a type with a class of its own (or none).
#[no_mangle]
pub extern "C" fn ee_netmsg_record_class(message_type: u8) -> u32 {
    record_type(message_type).map_or(u32::MAX, |r| r.class)
}

/// A record type's flags (`EE_NETMSG_RECORD_PAUSED`, `EE_NETMSG_RECORD_WHEN_OUT`); 0 for any other type.
#[no_mangle]
pub extern "C" fn ee_netmsg_record_flags(message_type: u8) -> u8 {
    record_type(message_type).map_or(0, |r| r.flags)
}

/// A record type's debug label (`GetMessageLabel`), a static C string; "" for any other type.
#[no_mangle]
pub extern "C" fn ee_netmsg_record_label(message_type: u8) -> *const c_char {
    record_type(message_type).map_or("\0", |r| r.label).as_ptr() as *const c_char
}

/// Member `index` of a record class (1 = the first after the selection id; NONE past the last).
#[no_mangle]
pub extern "C" fn ee_netmsg_record_field(class_id: u32, index: u32) -> EeNetMsgRecordField {
    record_field(class_id, index)
}

/// Member `index` of a record type: the wire's, then the members that are not on the wire (NONE past the last).
#[no_mangle]
pub extern "C" fn ee_netmsg_record_member(message_type: u8, index: u32) -> EeNetMsgRecordField {
    record_member(message_type, index)
}

/// A record type's rules: its send-to-all-and-wait member (0: none) and `IsResponse`'s (the response's type, 0
/// for none; this message's member and the response's, equal as 32-bit integers).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct EeNetMsgRecordRules {
    pub wait_field: u8,
    pub response_type: u8,
    pub response_mine: u8,
    pub response_theirs: u8,
}

/// A record type's rules (all 0 for any other type).
#[no_mangle]
pub extern "C" fn ee_netmsg_record_rules(message_type: u8) -> EeNetMsgRecordRules {
    let (wait_field, (response_type, response_mine, response_theirs)) =
        record_type(message_type).map_or((0, (0, 0, 0)), |r| (r.wait_field, r.response));
    EeNetMsgRecordRules { wait_field, response_type, response_mine, response_theirs }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn every_record_type_is_one_message_type_of_fixed_members_that_fit() {
        for r in &RECORD_TYPES {
            let c = &CLASSES[r.class as usize];
            if r.message_type == 0 {
                assert_eq!(TYPES.iter().filter(|t| t.1 == r.class).count(), 1, "{}", c.name);
            }
            // EEMChangeFormation's import stops before its flag (the record keeps it 0)
            if r.class != EE_NETMSG_CHANGE_FORMATION {
                assert_eq!(c.import_fields, c.fields.len(), "{}", c.name);
            }
            let strings = c.fields[1..].iter().filter(|f| f.kind == Kind::WStr).count();
            assert!(strings <= 1, "{}", c.name);
            let lists = c.fields[1..]
                .iter()
                .filter(|f| matches!(f.kind, Kind::Records8(_) | Kind::Records32(_) | Kind::Blob))
                .count();
            assert!(lists <= 1, "{}", c.name);
            let size: usize = c.fields[1..].iter().chain(r.locals.iter()).map(|f| record_bytes(f.kind)).sum();
            assert!(size <= EE_NETMSG_RECORD_BYTES, "{}", c.name);
            assert!(r.label.ends_with('\0') && !r.label[..r.label.len() - 1].contains('\0'));
            for i in 1..c.fields.len() as u32 {
                assert_ne!(record_field(r.class, i).kind, EE_NETMSG_RECORD_FIELD_NONE, "{}", c.name);
            }
            assert_eq!(record_field(r.class, c.fields.len() as u32).kind, EE_NETMSG_RECORD_FIELD_NONE);
        }
        let mut types: Vec<(u32, u8)> = RECORD_TYPES.iter().map(|r| (r.class, r.message_type)).collect();
        types.sort_unstable();
        types.dedup();
        assert_eq!(types.len(), RECORD_TYPES.len());
    }

    #[test]
    fn layouts_flags_and_labels() {
        // EEMPayTribute (0x44): food, gold, iron, stone, wood, receiver
        assert_eq!(ee_netmsg_record_class(0x44), EE_NETMSG_PAY_TRIBUTE);
        assert_eq!(
            ee_netmsg_record_field(EE_NETMSG_PAY_TRIBUTE, 6),
            EeNetMsgRecordField { kind: EE_NETMSG_RECORD_FIELD_INTEGER, offset: 20, size: 1 }
        );
        assert_eq!(ee_netmsg_record_flags(0x44), PAUSED);
        // EEMSetAirportCommand: the point first
        let f = |i| record_field(EE_NETMSG_SET_AIRPORT_COMMAND, i);
        assert_eq!(f(1), EeNetMsgRecordField { kind: EE_NETMSG_RECORD_FIELD_POINT, offset: 0, size: 8 });
        assert_eq!(f(2), EeNetMsgRecordField { kind: EE_NETMSG_RECORD_FIELD_INTEGER, offset: 8, size: 4 });
        assert_eq!(f(0).kind, EE_NETMSG_RECORD_FIELD_NONE);
        assert_eq!(record_field(EE_NETMSG_CAMERA_LOCATION_INFO, 1).kind, EE_NETMSG_RECORD_FIELD_VECTOR);
        assert_eq!(record_field(EE_NETMSG_RETURN_TO_BASE, 2).kind, EE_NETMSG_RECORD_FIELD_BOOL);
        assert_eq!(record_field(EE_NETMSG_GAME_SPEED, 1).kind, EE_NETMSG_RECORD_FIELD_FLOAT);
        // EEMPathTo (0x4d, round 4): facing, x, y, then four flags
        assert_eq!(ee_netmsg_record_class(0x4d), EE_NETMSG_PATH_TO);
        let p = |i| record_field(EE_NETMSG_PATH_TO, i);
        assert_eq!(p(1), EeNetMsgRecordField { kind: EE_NETMSG_RECORD_FIELD_FLOAT, offset: 0, size: 4 });
        assert_eq!(p(3), EeNetMsgRecordField { kind: EE_NETMSG_RECORD_FIELD_FLOAT, offset: 8, size: 4 });
        assert_eq!(p(7), EeNetMsgRecordField { kind: EE_NETMSG_RECORD_FIELD_BOOL, offset: 15, size: 1 });
        assert_eq!(p(8).kind, EE_NETMSG_RECORD_FIELD_NONE);
        // EEMBuildWall: two points, the technology, the player (the largest order: 24 bytes)
        assert_eq!(
            record_field(EE_NETMSG_BUILD_WALL, 4),
            EeNetMsgRecordField { kind: EE_NETMSG_RECORD_FIELD_INTEGER, offset: 20, size: 4 }
        );
        // EEMChangeFormation: the flag is a member the import leaves 0
        assert_eq!(record_field(EE_NETMSG_CHANGE_FORMATION, 2).kind, EE_NETMSG_RECORD_FIELD_BOOL);
        assert_eq!(CLASSES[EE_NETMSG_CHANGE_FORMATION as usize].import_fields, 2);
        // the Lua AI's: EEMHackANewUnit (0x6b) the class, the tile, the player; EEMKillUnit (0x5c)
        let h = |i| record_field(EE_NETMSG_HACK_A_NEW_UNIT, i);
        assert_eq!(h(2), EeNetMsgRecordField { kind: EE_NETMSG_RECORD_FIELD_POINT, offset: 4, size: 8 });
        assert_eq!(h(3), EeNetMsgRecordField { kind: EE_NETMSG_RECORD_FIELD_INTEGER, offset: 12, size: 1 });
        assert_eq!(ee_netmsg_record_class(0x5c), EE_NETMSG_KILL_UNIT);
        assert_eq!(ee_netmsg_record_flags(0x5c), 0);
        // the two selection toggles share a class, each with its label; End Cinematic is a record no factory row has
        assert_eq!(ee_netmsg_record_class(0x49), EE_NETMSG_ADD_UNIT_TO_SELECTED_GROUP);
        assert_eq!(ee_netmsg_record_class(0x76), EE_NETMSG_ADD_UNIT_TO_SELECTED_GROUP);
        assert_eq!(ee_netmsg_record_flags(0x76), PAUSED | WHEN_OUT);
        assert_eq!(ee_netmsg_record_class(0x9d), EE_NETMSG_END_CINEMATIC);
        assert!(!TYPES.iter().any(|t| t.0 == 0x9d));
        // the string types: EEMPlayerSlotInfo (0x6a) the player, slot, firewall, then the address; EEMFullPlayerInformation
        // (0x5b) the name between the loaded type and the ping, which follows the three integers
        let s = |i| record_field(EE_NETMSG_PLAYER_SLOT_INFO, i);
        assert_eq!(s(3), EeNetMsgRecordField { kind: EE_NETMSG_RECORD_FIELD_BOOL, offset: 2, size: 1 });
        assert_eq!(s(4), EeNetMsgRecordField { kind: EE_NETMSG_RECORD_FIELD_STRING, offset: 3, size: 0 });
        assert_eq!(s(5).kind, EE_NETMSG_RECORD_FIELD_NONE);
        let f = |i| record_field(EE_NETMSG_FULL_PLAYER_INFORMATION, i);
        assert_eq!(f(4).kind, EE_NETMSG_RECORD_FIELD_STRING);
        assert_eq!(f(5), EeNetMsgRecordField { kind: EE_NETMSG_RECORD_FIELD_FLOAT, offset: 12, size: 4 });
        assert_eq!(ee_netmsg_record_flags(0x5b), PAUSED);
        assert_eq!(ee_netmsg_record_flags(0x7d), PAUSED | WHEN_OUT); // EEMInGameChat
        assert_eq!(record_field(EE_NETMSG_FULL_GAME_OPTIONS, 18).offset, 44); // the last fixed member
                                                                              // EEMStartLoadingResponse (0x5e)
        assert_eq!(ee_netmsg_record_flags(0x5e), PAUSED | WHEN_OUT);
        // no type
        assert_eq!(ee_netmsg_record_class(0x40), u32::MAX);
        assert_eq!(ee_netmsg_record_flags(0x40), 0);
        let label = |t| unsafe { std::ffi::CStr::from_ptr(ee_netmsg_record_label(t)) }.to_str().unwrap().to_string();
        assert_eq!(label(0x44), "Pay Tribute");
        assert_eq!(label(0x4d), "Path To");
        assert_eq!(label(0x5c), "Delete Unit");
        assert_eq!(label(0x49), "Add Unit To Selected Group");
        assert_eq!(label(0x76), "Always Add Units To Selected Group");
        assert_eq!(label(0x9d), "End Cinematic");
        assert_eq!(label(0x40), "");
        let select = TYPES.iter().find(|t| t.1 == EE_NETMSG_SELECT_UNIT).unwrap().0;
        assert_eq!(ee_netmsg_record_flags(select), PAUSED | WHEN_OUT);
    }

    #[test]
    fn every_factory_type_is_a_record() {
        for &(t, class) in &TYPES {
            assert_eq!(ee_netmsg_record_class(t), class, "{t:#x}");
        }
    }

    #[test]
    fn lists_blobs_locals_and_rules() {
        let none = EeNetMsgRecordField { kind: EE_NETMSG_RECORD_FIELD_NONE, offset: 0, size: 0 };
        // the box selections: the count, the blob, then the rectangle and the unit type off the wire
        for t in [0x4e, 0x4f, 0x50] {
            assert_eq!(ee_netmsg_record_class(t), EE_NETMSG_SELECT_UNITS);
            assert_eq!(ee_netmsg_record_flags(t), PAUSED | WHEN_OUT);
            let m = |i| record_member(t, i);
            assert_eq!(m(1), EeNetMsgRecordField { kind: EE_NETMSG_RECORD_FIELD_INTEGER, offset: 0, size: 1 });
            assert_eq!(m(2), EeNetMsgRecordField { kind: EE_NETMSG_RECORD_FIELD_BLOB, offset: 1, size: 1 });
            assert_eq!(m(3), EeNetMsgRecordField { kind: EE_NETMSG_RECORD_FIELD_RAW, offset: 1, size: 16 });
            assert_eq!(m(4), EeNetMsgRecordField { kind: EE_NETMSG_RECORD_FIELD_INTEGER, offset: 17, size: 4 });
            assert_eq!(m(5), none);
            // the wire's members stop at the blob
            assert_eq!(record_field(EE_NETMSG_SELECT_UNITS, 3), none);
        }
        // the id list: 4-byte records
        assert_eq!(
            record_member(0x96, 1),
            EeNetMsgRecordField { kind: EE_NETMSG_RECORD_FIELD_LIST, offset: 0, size: 4 }
        );
        assert_eq!(ee_netmsg_record_flags(0x96), PAUSED | WHEN_OUT);
        // the upgrades: the player, the name string, the upgrades, the name; only the purchase may be sent while out
        assert_eq!(record_member(0x7f, 3).kind, EE_NETMSG_RECORD_FIELD_LIST);
        assert_eq!(
            record_member(0x7f, 4),
            EeNetMsgRecordField { kind: EE_NETMSG_RECORD_FIELD_STRING, offset: 5, size: 0 }
        );
        assert_eq!(ee_netmsg_record_flags(0x7e), PAUSED);
        assert_eq!(ee_netmsg_record_flags(0x7f), PAUSED | WHEN_OUT);
        // the load or save request: the tick at 2, the wait id at 11, the file name; its rules
        assert_eq!(
            record_member(0x64, 3),
            EeNetMsgRecordField { kind: EE_NETMSG_RECORD_FIELD_INTEGER, offset: 2, size: 4 }
        );
        assert_eq!(
            record_member(0x64, 6),
            EeNetMsgRecordField { kind: EE_NETMSG_RECORD_FIELD_INTEGER, offset: 11, size: 4 }
        );
        assert_eq!(record_member(0x64, 7).kind, EE_NETMSG_RECORD_FIELD_STRING);
        assert_eq!(
            ee_netmsg_record_rules(0x64),
            EeNetMsgRecordRules { wait_field: 6, response_type: 0x9a, response_mine: 3, response_theirs: 1 }
        );
        assert_eq!(
            ee_netmsg_record_rules(0x5d),
            EeNetMsgRecordRules { wait_field: 0, response_type: 0, response_mine: 0, response_theirs: 0 }
        );
        // the start: the humans, then the seed (the legacy harness writes it at record offset 1)
        assert_eq!(
            record_member(0x5d, 2),
            EeNetMsgRecordField { kind: EE_NETMSG_RECORD_FIELD_INTEGER, offset: 1, size: 4 }
        );
        assert_eq!(ee_netmsg_record_flags(0x5d), PAUSED);
    }
}
