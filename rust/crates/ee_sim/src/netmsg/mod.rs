//! The game messages' wire codec (RUST_PLAN R5b.4, the first leaf of the world): the layout of every game message
//! (`EEMessage.cpp`'s 104 classes with a codec of their own, 107 message types), encoding and decoding by it, and the
//! box selection's compact unit list; and the engine messages' (`NEMessage.cpp`'s 28 classes) with the players'
//! inputs nested in the communications ticks. The C++ classes keep their members, constructors and `Execute`; their
//! `ExportSize`/`ExportMessage`/`ImportMessage` hand their members to `ffi` as slots in wire order.
//!
//! - `schema`: the member kinds and a class's layout;
//! - `classes`: the game messages' table (written once from the original bodies; now the format);
//! - `engine`: the engine messages' table (`NEMessage.cpp`; the players' inputs nested in the ticks);
//! - `codec`: plain values in, bytes out, and bytes in, member spans out;
//! - `units`: `CompressUnits`/`DecompressUnits`;
//! - `records`: the message types without a C++ class of their own (`EEMRecord`): layout, flags, labels;
//! - `ne_records`: the engine messages as one data class (`NEMRecord`): layout, flags, responses, labels; the
//!   factory's builders by type;
//! - `execute`: the `Execute` bodies' decisions (the selection id check, the resources), with their C ABI;
//! - `session`: the game's lock-step server and client (`EEServer.cpp`, `EEClient.cpp`, round 6): the connection
//!   handling, the lobby's slots, the votes, the migration, over one C++ host;
//! - `bodies`: the record types' `Execute` bodies and the physics' two ways to run a message over the C++ host's world
//!   calls (round 5), the unit orders through the units lane's bodies;
//! - `capture`: the simulation capture's sweep payloads (round 6; the capture records what the C++ builds);
//! - `ffi`: the C ABI.

pub mod bodies;
pub mod capture;
pub mod classes;
pub mod codec;
pub mod engine;
pub mod execute;
pub mod ffi;
pub mod ne_records;
pub mod records;
pub mod schema;
pub mod session;
pub mod units;

#[cfg(test)]
mod tests {
    use crate::netmsg::classes::{CLASSES, TYPES};
    use crate::netmsg::schema::Kind;

    #[test]
    fn the_table_is_whole() {
        assert_eq!(CLASSES.len(), 104);
        // every class starts with the selection id, and each class but one imports what it exports
        for c in &CLASSES {
            assert_eq!(c.fields[0].name, "mFastSelectionID", "{}", c.name);
            assert_eq!(c.fields[0].kind, Kind::U8);
            if c.name != "EEMChangeFormation" {
                assert_eq!(c.import_fields, c.fields.len(), "{}", c.name);
            }
        }
        // the types are distinct and in the game range
        let mut seen = std::collections::BTreeSet::new();
        for &(t, _) in &TYPES {
            assert!((0x41..0xfe).contains(&t));
            assert!(seen.insert(t));
        }
    }

    #[test]
    fn known_sizes() {
        let size = |name: &str| CLASSES.iter().find(|c| c.name == name).unwrap().fixed_size();
        // the original ExportSize constants
        assert_eq!(size("EEMAttackUnit"), Some(11));
        assert_eq!(size("EEMBuildWall"), Some(29));
        assert_eq!(size("EEMPayTribute"), Some(26));
        assert_eq!(size("EEMChangeFormation"), Some(10));
        assert_eq!(size("EEMSelectUnits"), None);
        assert_eq!(size("EEMFullPlayerInformation"), None);
    }
}
