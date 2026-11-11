//! `UIBackgroundTable` (`User Interface/UIBackgroundTable.cpp`): the UI database's background records and what a
//! table entry makes of one ([`from_record`]: the layout, the texture rectangle, the colour, the draw priority), and
//! `Create`'s loop over the file ([`create`]: a nameless record leaves its slot empty, a failed read keeps the
//! previous record's bytes, the result is the last read's).
//!
//! The C++ class keeps every member ([`BackgroundTableFields`] is its record from `mTileWidthPercent` on), its
//! `UString` texture file name and the static array; the file and the array are reached through
//! [`BackgroundTablesHost`].

use core::ffi::{c_long, c_ulong};

use crate::button::UiColor;
use crate::layout::Layout;
use crate::rect::UiRect;

/// `kErrorOK`.
const ERROR_OK: c_long = 0;

/// One record of the backgrounds database file (`UIDbDataBackgrounds`, 308 bytes on every target).
#[repr(C)]
#[derive(Clone, Copy)]
pub struct DbBackground {
    pub name: [u8; 100],
    pub texture_file: [u8; 100],
    pub top_offset: f32,
    pub bottom_offset: f32,
    pub left_offset: f32,
    pub right_offset: f32,
    pub tile_width_percent: f32,
    pub tile_height_percent: f32,
    pub opacity: f32,
    pub id: i32,
    pub background_id: u32,
    pub form_id: u32,
    pub texture_top: i32,
    pub texture_left: i32,
    pub texture_bottom: i32,
    pub texture_right: i32,
    pub background_type: u32,
    pub top_offset_type: u32,
    pub bottom_offset_type: u32,
    pub left_offset_type: u32,
    pub right_offset_type: u32,
    pub num_cols: i32,
    pub num_rows: i32,
    pub draw_priority: i32,
    pub red: i32,
    pub green: i32,
    pub blue: i32,
    pub visible: u8,
    pub has_alpha: u8,
    pub attribute1: u8,
    pub attribute2: u8,
    pub attribute3: u8,
    pub pad: [u8; 3],
}

const _: () = assert!(core::mem::size_of::<DbBackground>() == 308);

impl DbBackground {
    /// A record of zero bytes (the C++'s was uninitialised until the first read).
    pub fn zeroed() -> DbBackground {
        // SAFETY: every field is an integer, a float or a byte array: all bits zero is a value of each
        unsafe { core::mem::zeroed() }
    }

    /// `strlen(mName) > 0`.
    pub fn is_named(&self) -> bool {
        self.name[0] != 0
    }
}

/// `UIBackgroundTable`'s members from `mTileWidthPercent` (+0x50 in the original) to `mAttribute3`, in order
/// (offsets `static_assert`ed in `UIBackgroundTable.cpp`).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct BackgroundTableFields {
    pub tile_width_percent: f32,
    pub tile_height_percent: f32,
    pub opacity: f32,
    pub id: c_long,
    pub background_id: c_ulong,
    pub form_id: c_ulong,
    pub background_type: c_ulong,
    pub num_cols: c_long,
    pub num_rows: c_long,
    pub draw_priority: c_ulong,
    pub visible: bool,
    pub has_alpha: bool,
    pub attribute1: bool,
    pub attribute2: bool,
    pub attribute3: bool,
}

/// What a table entry takes from a record: its scalar members, `mLayout`, `mTextureRect` and `mColor`.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct BackgroundTable {
    pub fields: BackgroundTableFields,
    pub layout: Layout,
    pub texture_rect: UiRect,
    pub color: UiColor,
}

/// `Create`'s record copy: `GERectangle`'s `long` constructor for the texture rectangle (left, top, right, bottom),
/// the colour channels times `1 / 255`, a draw priority of at least 1. The record's bytes are taken as the C++'s
/// `bool`s took them (0 is false).
pub fn from_record(r: &DbBackground) -> BackgroundTable {
    let fields = BackgroundTableFields {
        tile_width_percent: r.tile_width_percent,
        tile_height_percent: r.tile_height_percent,
        opacity: r.opacity,
        id: r.id as c_long,
        background_id: r.background_id as c_ulong,
        form_id: r.form_id as c_ulong,
        background_type: r.background_type as c_ulong,
        num_cols: r.num_cols as c_long,
        num_rows: r.num_rows as c_long,
        draw_priority: if r.draw_priority <= 1 { 1 } else { r.draw_priority as c_ulong },
        visible: r.visible != 0,
        has_alpha: r.has_alpha != 0,
        attribute1: r.attribute1 != 0,
        attribute2: r.attribute2 != 0,
        attribute3: r.attribute3 != 0,
    };
    let layout = Layout {
        left_offset: r.left_offset,
        right_offset: r.right_offset,
        top_offset: r.top_offset,
        bottom_offset: r.bottom_offset,
        left_type: r.left_offset_type as c_ulong,
        right_type: r.right_offset_type as c_ulong,
        top_type: r.top_offset_type as c_ulong,
        bottom_type: r.bottom_offset_type as c_ulong,
    };
    let texture_rect = UiRect::new(
        r.texture_left as c_long as f32,
        r.texture_top as c_long as f32,
        r.texture_right as c_long as f32,
        r.texture_bottom as c_long as f32,
    );
    let k = 1.0f32 / 255.0;
    let color = UiColor { red: r.red as f32 * k, green: r.green as f32 * k, blue: r.blue as f32 * k };
    BackgroundTable { fields, layout, texture_rect, color }
}

/// The C++ side of `Create`: the database file and the static array.
pub trait BackgroundTablesHost {
    /// `inFile.Open(kFileAccessRead)`.
    fn open(&mut self);
    /// `inFile.Read(theCount)`.
    fn read_count(&mut self, count: &mut c_ulong) -> c_long;
    /// `sUIBackgroundArray.resize(count)`.
    fn resize(&mut self, count: c_ulong);
    /// `inFile.Read(&theData, sizeof theData, theRead)` (a failed read may leave `record` as it was).
    fn read_record(&mut self, record: &mut DbBackground) -> c_long;
    /// `sUIBackgroundArray[index]` = a new table made from `record` ([`from_record`], its texture file name copied),
    /// or NULL.
    fn set(&mut self, index: c_ulong, record: Option<&DbBackground>);
    /// `inFile.Close()`.
    fn close(&mut self);
}

/// `UIBackgroundTable::Create`.
pub fn create(h: &mut impl BackgroundTablesHost) -> c_long {
    let mut count: c_ulong = 0;
    let mut record = DbBackground::zeroed();
    h.open();
    let mut result = h.read_count(&mut count);
    if result == ERROR_OK {
        h.resize(count);
        for i in 0..count {
            result = h.read_record(&mut record);
            h.set(i, if record.is_named() { Some(&record) } else { None });
        }
    }
    h.close();
    result
}

#[cfg(test)]
pub(crate) mod tests;
