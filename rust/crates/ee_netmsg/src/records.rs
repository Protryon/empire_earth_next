//! The game messages without a C++ class of their own (RUST_PLAN R5b.4, game messages round 3): `EEMRecord`
//! (ee/EEMessage.h) carries any of these types: its codec class's members after the selection id, packed in wire
//! order in a byte record (every member of these classes is fixed: no string, record list or blob). The C++ asks here
//! for the layout, the type's two flags and its debug label. The classes these types had (`EEMCheatCode`,
//! `EEMPayTribute`, ...) were touched only by their senders' constructors, their own `Execute` and the capture (which
//! reads every message through its codec slots).

use crate::classes::*;
use crate::schema::Kind;
use std::ffi::c_char;

/// `GetCanSendWhilePaused` answers true.
pub const EE_NETMSG_RECORD_PAUSED: u8 = 1;
/// A sender's message may be sent while the local player is out (`EEMessage::mCanSendWhenOut`; an imported one never).
pub const EE_NETMSG_RECORD_WHEN_OUT: u8 = 2;
/// The record's room: at least the largest record type's members after the selection id (`EEMPayTribute`'s 21 bytes).
pub const EE_NETMSG_RECORD_BYTES: usize = 32;

const PAUSED: u8 = EE_NETMSG_RECORD_PAUSED;
const WHEN_OUT: u8 = EE_NETMSG_RECORD_WHEN_OUT;

/// One record type: its codec class, its message type when the class has several (0: the class's one type in
/// `TYPES`), its flags, its label (NUL-terminated: `GetMessageLabel` hands it to C++).
pub struct RecordType {
    pub class: u32,
    pub message_type: u8,
    pub flags: u8,
    pub label: &'static str,
}

const fn rec(class: u32, flags: u8, label: &'static str) -> RecordType {
    RecordType { class, message_type: 0, flags, label }
}

/// The record types, by codec class (each of these classes is one message type's: `TYPES`).
pub static RECORD_TYPES: [RecordType; 80] = [
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

/// Member `index` of a record class (1 = the first after the selection id).
pub fn record_field(class_id: u32, index: u32) -> EeNetMsgRecordField {
    let none = EeNetMsgRecordField { kind: EE_NETMSG_RECORD_FIELD_NONE, offset: 0, size: 0 };
    let Some(c) = CLASSES.get(class_id as usize) else { return none };
    if index == 0 || index as usize >= c.fields.len() {
        return none;
    }
    let offset: usize = c.fields[1..index as usize].iter().map(|f| f.kind.fixed_size().unwrap_or(0)).sum();
    let f = c.fields[index as usize];
    let kind = match f.kind {
        Kind::U8 | Kind::U16 | Kind::I32 | Kind::U32 => EE_NETMSG_RECORD_FIELD_INTEGER,
        Kind::Bool => EE_NETMSG_RECORD_FIELD_BOOL,
        Kind::F32 => EE_NETMSG_RECORD_FIELD_FLOAT,
        Kind::Point2 => EE_NETMSG_RECORD_FIELD_POINT,
        Kind::Vec3 => EE_NETMSG_RECORD_FIELD_VECTOR,
        _ => return none,
    };
    EeNetMsgRecordField { kind, offset: offset as u32, size: f.kind.fixed_size().unwrap_or(0) as u32 }
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
            let size: usize = c.fields[1..].iter().map(|f| f.kind.fixed_size().expect(c.name)).sum();
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
        // a type with a class of its own, and no type
        assert_eq!(ee_netmsg_record_class(0x4e), u32::MAX); // EEMSelectUnits
        assert_eq!(ee_netmsg_record_class(0x9d), u32::MAX); // EEMEndCinematic: the factory builds none
        assert_eq!(ee_netmsg_record_flags(0x4e), 0);
        let label = |t| unsafe { std::ffi::CStr::from_ptr(ee_netmsg_record_label(t)) }.to_str().unwrap().to_string();
        assert_eq!(label(0x44), "Pay Tribute");
        assert_eq!(label(0x4d), "Path To");
        assert_eq!(label(0x5c), "Delete Unit");
        assert_eq!(label(0x4e), "");
        let select = TYPES.iter().find(|t| t.1 == EE_NETMSG_SELECT_UNIT).unwrap().0;
        assert_eq!(ee_netmsg_record_flags(select), PAUSED | WHEN_OUT);
    }
}
