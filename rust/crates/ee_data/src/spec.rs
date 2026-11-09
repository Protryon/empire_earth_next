//! The table description the text compiler reads: the Rust form of `EEDataTextSpecs.h`. The data is generated
//! (`generated/specs_gen.rs`, tools/eedata/rustgen.py) from tools/eedata/specs.json, one layout description for
//! Python, C++ and Rust.

/// A field's element type (EEDTType).
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Type {
    S8,
    U8,
    Bool,
    S16,
    U16,
    S32,
    U32,
    F32,
    PairF32,
    Str,
}

impl Type {
    /// Size in bytes of one element (kTypeSize).
    pub const fn size(self) -> usize {
        match self {
            Type::S8 | Type::U8 | Type::Bool | Type::Str => 1,
            Type::S16 | Type::U16 => 2,
            Type::S32 | Type::U32 | Type::F32 => 4,
            Type::PairF32 => 8,
        }
    }
}

/// A record field (EEDTField).
#[derive(Clone, Copy, Debug)]
pub struct Field {
    pub name: &'static str,
    pub offset: u16,
    pub count: u16,
    pub ty: Type,
}

/// A record layout (EEDTStruct); `base`: the record every row starts from (None: all zero).
#[derive(Clone, Copy, Debug)]
pub struct Struct {
    pub name: &'static str,
    pub size: u32,
    pub fields: &'static [Field],
    pub base: Option<&'static [u8]>,
}

/// dbworld: a row's id and the fields its readers use.
#[derive(Clone, Copy, Debug)]
pub struct RowName {
    pub row: i32,
    pub name: &'static str,
    pub fields: &'static str,
}

/// A reference: `field` (or "@list", the trailing list) holds a row of `table`, written as its id.
#[derive(Clone, Copy, Debug)]
pub struct Ref {
    pub field: &'static str,
    pub table: &'static str,
    pub none: &'static [i32],
    pub never: bool,
}

/// A field that follows from the position (epochs.yaml): a * position + b where left out.
#[derive(Clone, Copy, Debug)]
pub struct Derived {
    pub field: &'static str,
    pub a: i32,
    pub b: i32,
}

/// How a table's rows sit in its db file (EEDTContainer).
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Container {
    Count,
    Lead,
    Fixed,
    Events,
    Techtree,
    Epochs,
}

/// One db table (EEDTTable).
#[derive(Debug)]
pub struct Table {
    pub name: &'static str,
    pub container: Container,
    pub yaml: bool,
    pub record: &'static Struct,
    pub lead: &'static [&'static str],
    pub list_name: Option<&'static str>,
    pub list_signed: bool,
    pub row_names: &'static [RowName],
    pub hole: Option<&'static [u8]>,
    pub key_field: Option<&'static str>,
    pub self_field: Option<&'static str>,
    pub self_offset: i32,
    pub refs: &'static [Ref],
    pub derived: &'static [Derived],
    pub text_only: bool,
    pub epoch_flags: Option<&'static str>,
    pub epoch_key: Option<&'static str>,
    pub levels_field: Option<&'static str>,
    pub levels_key: Option<&'static str>,
}

impl Table {
    pub fn find_field(&self, name: &str) -> Option<&'static Field> {
        self.record.fields.iter().find(|f| f.name == name)
    }
    pub fn find_ref(&self, field: &str) -> Option<&'static Ref> {
        self.refs.iter().find(|r| r.field == field)
    }
}

/// A table by file stem, case-insensitive (the loaders spell some names in mixed case).
pub fn find_table(name: &[u8]) -> Option<&'static Table> {
    let lower: Vec<u8> = name.iter().map(u8::to_ascii_lowercase).collect();
    crate::generated::specs_gen::TABLES.iter().find(|t| t.name.as_bytes() == lower.as_slice())
}

/// A table that must exist in the specs (the names in code).
pub fn table(name: &str) -> &'static Table {
    find_table(name.as_bytes()).expect("the table is in specs.json")
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn type_sizes_match_the_format() {
        assert_eq!(Type::S8.size(), 1);
        assert_eq!(Type::Bool.size(), 1);
        assert_eq!(Type::S16.size(), 2);
        assert_eq!(Type::F32.size(), 4);
        assert_eq!(Type::PairF32.size(), 8);
        assert_eq!(Type::Str.size(), 1);
    }

    #[test]
    fn lookup_is_case_insensitive() {
        assert_eq!(find_table(b"DBWorld").map(|t| t.name), Some("dbworld"));
        assert!(find_table(b"nosuchtable").is_none());
    }

    #[test]
    fn specs_hold_the_38_tables_with_consistent_fields() {
        let tables = crate::generated::specs_gen::TABLES;
        assert_eq!(tables.len(), 38);
        for t in tables {
            for f in t.record.fields {
                let end = f.offset as usize + f.ty.size() * f.count as usize;
                assert!(end <= t.record.size as usize, "{}.{}", t.name, f.name);
            }
            if let Some(hole) = t.hole {
                assert_eq!(hole.len(), t.record.size as usize);
            }
        }
    }

    #[test]
    fn table_finders_resolve_fields_and_refs() {
        let t = table("dbworld");
        assert!(t.find_field("mFloatValue").is_some());
        assert!(t.find_field("nope").is_none());
        let t = table("dbtechtree");
        assert_eq!(t.find_ref("@list").map(|r| r.table), Some("dbobjects"));
    }
}
