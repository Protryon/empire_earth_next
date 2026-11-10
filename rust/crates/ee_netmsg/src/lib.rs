//! The game messages' wire codec (RUST_PLAN R5b.4, the first leaf of the world): the layout of every game message
//! (`EEMessage.cpp`'s 104 classes with a codec of their own, 107 message types), encoding and decoding by it, and the
//! box selection's compact unit list. The C++ classes keep their members, constructors and `Execute`; their
//! `ExportSize`/`ExportMessage`/`ImportMessage` hand their members to `ffi` as slots in wire order.
//!
//! - `schema`: the member kinds and a class's layout;
//! - `classes`: the table (written once from the original bodies; now the format);
//! - `codec`: plain values in, bytes out, and bytes in, member spans out;
//! - `units`: `CompressUnits`/`DecompressUnits`;
//! - `ffi`: the C ABI.

pub mod classes;
pub mod codec;
pub mod ffi;
pub mod schema;
pub mod units;

#[cfg(test)]
mod tests {
    use crate::classes::{CLASSES, TYPES};
    use crate::schema::Kind;

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
