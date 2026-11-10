//! The UI database's font table (`User Interface/UIFontTable.cpp`): a font record read into an entry (`Create`), the
//! point size for a pixel height (`CreateClosestFont`) and the shared-font search (`FindOrCreateFont`).
//!
//! The entries, the shared `GEFont` list and the fonts stay C++ objects; the search sees the list through
//! [`FontHost`].

use core::ffi::{c_char, c_long, c_ulong};

/// `UIDbDataFonts`: one font record of the UI database (0x80 bytes on every target).
#[repr(C)]
#[derive(Clone, Copy)]
pub struct DbFont {
    /// Empty: no font.
    pub font_name: [c_char; 0x6c],
    /// The face name's string-table ID.
    pub string_id: u32,
    pub size: u32,
    pub quality: u32,
    pub bold: u8,
    pub italic: u8,
    pub underline: u8,
    pub style10: u8,
    pub style08: u8,
    pub pad: [c_char; 3],
}

const _: () = assert!(core::mem::size_of::<DbFont>() == 128);

/// `kFontStyleBold`, `kFontStyleItalic`, `kFontStyleUnderline`, `kFontStyleShadow`.
pub const STYLE_BOLD: c_ulong = 1;
pub const STYLE_ITALIC: c_ulong = 2;
pub const STYLE_UNDERLINE: c_ulong = 4;
pub const STYLE_SHADOW: c_ulong = 0x10;

/// What `Create` makes of a record: no entry for an empty font name, else its face (by string ID), style, quality
/// and size.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct FontEntry {
    pub present: bool,
    pub string_id: c_ulong,
    pub style: c_ulong,
    pub quality: c_ulong,
    pub size: c_ulong,
}

/// `Create`'s reading of one record: the style bits are 0x08, then shadow, underline, italic, bold (the C++'s
/// order, each OR-ed in).
pub fn entry(record: &DbFont) -> FontEntry {
    if record.font_name[0] == 0 {
        return FontEntry::default();
    }
    let mut style: c_ulong = 0;
    if record.style08 != 0 {
        style = 8;
    }
    if record.style10 != 0 {
        style |= STYLE_SHADOW;
    }
    if record.underline != 0 {
        style |= STYLE_UNDERLINE;
    }
    if record.italic != 0 {
        style |= STYLE_ITALIC;
    }
    if record.bold != 0 {
        style |= STYLE_BOLD;
    }
    FontEntry {
        present: true,
        string_id: record.string_id as c_ulong,
        style,
        quality: record.quality as c_ulong,
        size: record.size as c_ulong,
    }
}

/// The screen's `LOGPIXELSY` as the game saw it (a DPI-unaware process): fonts are sized in game pixels.
pub const LOG_PIXELS_Y: i64 = 96;

/// `CreateClosestFont`'s point size for a pixel height: `height * 72 / 96 + 1` (64-bit, truncated).
pub fn closest_size(pixel_height: c_ulong) -> c_long {
    ((pixel_height as i64 * 72 / LOG_PIXELS_Y) as c_long).wrapping_add(1)
}

/// The shared fonts as `FindOrCreateFont` asks about them.
pub trait FontHost {
    /// `sFont.size()`.
    fn count(&self) -> usize;
    /// `sFont[i]->CompareFont(name, style, quality, size)`.
    fn matches(&mut self, i: usize) -> bool;
    /// `sFont[i]->Reference()` -> the font.
    fn take(&mut self, i: usize) -> usize;
    /// `new GEFont(name, size, style, quality)`, referenced, pushed onto `sFont` -> its index.
    fn create(&mut self) -> usize;
}

/// `FindOrCreateFont`: the first shared font that matches, referenced; else a new one -> its index in the list.
pub fn find_or_create(h: &mut impl FontHost) -> usize {
    let mut i = 0;
    while i < h.count() {
        if h.matches(i) {
            return h.take(i);
        }
        i += 1;
    }
    h.create()
}

#[cfg(test)]
mod tests;
