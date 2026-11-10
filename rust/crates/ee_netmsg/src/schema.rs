//! What a message's members look like on the wire.

/// One member's wire form. The fixed kinds are their little-endian bytes (the C++ copied each member with `memcpy`,
/// so a float or a point travels as its bit pattern).
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Kind {
    U8,
    Bool,
    U16,
    I32,
    U32,
    F32,
    /// `GE2DPoint`: two f32
    Point2,
    /// `GEVector`: three f32
    Vec3,
    /// `UWideString::Export`: u32 (byte length + 2), the bytes, u16 0. The u32 says how far the next member is.
    WStr,
    /// a u8 count (the C++ `(unsigned char)size()`, so it wraps past 255), then every record of the given size
    Records8(u32),
    /// a u32 count, then the records
    Records32(u32),
    /// as many bytes as the member before it (a u8) says (`EEMSelectUnits`' compressed buffer)
    Blob,
}

impl Kind {
    /// The wire size of a fixed kind; `None` for the variable ones.
    pub const fn fixed_size(self) -> Option<usize> {
        match self {
            Kind::U8 | Kind::Bool => Some(1),
            Kind::U16 => Some(2),
            Kind::I32 | Kind::U32 | Kind::F32 => Some(4),
            Kind::Point2 => Some(8),
            Kind::Vec3 => Some(12),
            Kind::WStr | Kind::Records8(_) | Kind::Records32(_) | Kind::Blob => None,
        }
    }
}

/// A member: the C++ name (for the reader and the error messages) and its kind.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct Field {
    pub name: &'static str,
    pub kind: Kind,
}

impl Field {
    pub const fn new(name: &'static str, kind: Kind) -> Field {
        Field { name, kind }
    }
}

/// A message class's layout: the members after the 4-byte header (reserved, type, target slot, originating slot), in
/// wire order. `import_fields` is how many of them the import reads (fewer than the export writes for
/// `EEMChangeFormation`, whose import leaves its last member to the constructor).
#[derive(Clone, Copy, Debug)]
pub struct MsgClass {
    pub name: &'static str,
    pub fields: &'static [Field],
    pub import_fields: usize,
}

/// The header's size: reserved, type, target slot, originating slot.
pub const HEADER_SIZE: usize = 4;

impl MsgClass {
    /// The wire size when every member is fixed (`ExportSize`'s constant), else `None`.
    pub fn fixed_size(&self) -> Option<usize> {
        let mut size = HEADER_SIZE;
        for f in self.fields {
            size += f.kind.fixed_size()?;
        }
        Some(size)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn fixed_sizes_add_up() {
        static C: MsgClass = MsgClass {
            name: "T",
            fields: &[Field::new("a", Kind::U8), Field::new("b", Kind::Point2), Field::new("c", Kind::Vec3)],
            import_fields: 3,
        };
        assert_eq!(C.fixed_size(), Some(4 + 1 + 8 + 12));
        static V: MsgClass =
            MsgClass { name: "V", fields: &[Field::new("a", Kind::U8), Field::new("s", Kind::WStr)], import_fields: 2 };
        assert_eq!(V.fixed_size(), None);
        assert_eq!(Kind::Records8(4).fixed_size(), None);
        assert_eq!(Kind::U16.fixed_size(), Some(2));
    }
}
