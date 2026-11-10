//! `UIControlsTable` (`User Interface/UIControlsTable.cpp`): one control record of the UI database and what the form
//! makes of it. The arithmetic moved here: the record's fields, colours, layout and texture rectangles from the
//! on-disk record ([`DbControl`]), the controls added past the database (`AddControlLike`'s searches and field
//! edits), `MoveControlsHorizontally`, the font size a scaled control gets, the string-table range, and the
//! rectangles `CreateControl` lays a control out with (the parent, the font rectangle, a combo box's text box and list
//! height, a file combo box's size, a scroll bar's length, a slider's page size).
//!
//! The C++ class keeps every member ([`TableFields`] is its record from `mBackgroundOpacity` on; the layout is the
//! Rust [`Layout`], the colours [`UiColor`]s, the texture rectangles [`UiRect`]s), its `UString`s, its static array
//! of tables (seen through [`TablesHost`]) and everything that makes GE objects: the textures, bitmaps, fonts and the
//! controls' constructors.

use core::ffi::{c_long, c_ulong};

use crate::button::UiColor;
use crate::layout::{Layout, PROPORTIONAL};
use crate::rect::{um_floor, UiRect};

/// The control types `CreateControl` dispatches on (`UIDbControlType`; private: the generated header would clash with
/// wingdi's BITMAP).
const BITMAP: c_ulong = 0;
const EDITABLE_COMBO_BOX: c_ulong = 3;
const COMBO_BOX: c_ulong = 4;
const NUMBER_MULTI_LINE_EDIT_BOX: c_ulong = 5;
const MULTI_LINE_EDIT_BOX: c_ulong = 7;
const LIST_BOX: c_ulong = 9;
const AUTO_SELECT_LIST_BOX: c_ulong = 10;
const MULTI_LINE_TEXT_LABEL: c_ulong = 16;
const NUMBER_MULTI_LINE_TEXT_LABEL: c_ulong = 21;
const FILE_LIST_BOX: c_ulong = 22;
const MULTI_LINE_HELP_TEXT_LABEL: c_ulong = 25;
const SCROLLING_TEXT_LABEL: c_ulong = 26;

/// `kScrollBarWidth`: a combo box's closed height when its font is not scaled to it.
const SCROLL_BAR_WIDTH: f32 = 20.0;

/// One record of the controls database file (`UIDbControl`, 860 bytes on every target).
#[repr(C)]
#[derive(Clone, Copy)]
pub struct DbControl {
    pub label: [u8; 100],
    pub normal_file_name: [u8; 100],
    pub pressed_file_name: [u8; 100],
    pub focused_file_name: [u8; 100],
    pub button_down_sound: [u8; 100],
    pub button_up_sound: [u8; 100],
    pub background_opacity: f32,
    pub text_box_opacity: f32,
    pub scroll_bar_bg_opacity: f32,
    pub unused1: [u32; 5],
    pub left_offset: f32,
    pub right_offset: f32,
    pub bottom_offset: f32,
    pub top_offset: f32,
    pub unused_db_record_id: i32,
    pub id: i32,
    pub form_id: i32,
    pub group: u32,
    pub control_type: u32,
    pub form_index: i32,
    pub border_thickness: i32,
    pub left_offset_type: u32,
    pub right_offset_type: u32,
    pub top_offset_type: u32,
    pub bottom_offset_type: u32,
    pub background_red: u32,
    pub background_green: u32,
    pub background_blue: u32,
    pub string_table_begin: i32,
    pub string_table_end: i32,
    pub font_table_index: u32,
    pub text_foreground_red: u32,
    pub text_foreground_green: u32,
    pub text_foreground_blue: u32,
    pub text_alignment: i32,
    pub normal_left: i32,
    pub normal_right: i32,
    pub normal_top: i32,
    pub normal_bottom: i32,
    pub pressed_left: i32,
    pub pressed_right: i32,
    pub pressed_top: i32,
    pub pressed_bottom: i32,
    pub focused_left: i32,
    pub focused_right: i32,
    pub focused_top: i32,
    pub focused_bottom: i32,
    pub max_chars: i32,
    pub selected_list_box_red: u32,
    pub selected_list_box_green: u32,
    pub selected_list_box_blue: u32,
    pub start_current_selection: i32,
    pub focused_line_red: u32,
    pub focused_line_green: u32,
    pub focused_line_blue: u32,
    pub start_range: i32,
    pub end_range: i32,
    pub current_range: i32,
    pub page_size: i32,
    pub string_table_id: i32,
    pub blink_rate: i32,
    pub unused2: [u32; 3],
    pub is_visible: u8,
    pub start_checked: u8,
    pub list_box_always_dirty: u8,
    pub sort_list_box: u8,
    pub focused_has_alpha: u8,
    pub pressed_has_alpha: u8,
    pub normal_has_alpha: u8,
    pub always_dirty: u8,
    pub fit_rect_to_text: u8,
    pub scale_to_control: u8,
    pub loop_movie: u8,
}

const _: () = assert!(core::mem::size_of::<DbControl>() == 860);

/// `UIControlsTable`'s plain members in their C++ order (the record starts at `mBackgroundOpacity`).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct TableFields {
    pub background_opacity: f32,
    pub text_box_opacity: f32,
    pub scroll_bar_bg_opacity: f32,
    /// `mUnknown100`: the record's unused id, copied and never read.
    pub unknown100: c_long,
    pub id: c_long,
    pub form_id: c_long,
    pub group: c_ulong,
    pub control_type: c_ulong,
    pub form_index: c_long,
    pub border_thickness: c_long,
    pub string_table_id: c_ulong,
    pub font_table_index: c_ulong,
    pub blink_rate: c_long,
    pub string_table_begin: c_long,
    pub string_table_end: c_long,
    pub text_alignment: c_long,
    pub max_chars: c_long,
    pub start_current_selection: c_long,
    pub start_range: c_long,
    pub end_range: c_long,
    pub current_range: c_long,
    pub page_size: c_long,
    pub is_visible: bool,
    pub start_checked: bool,
    pub list_box_always_dirty: bool,
    pub sort_list_box: bool,
    pub focused_has_alpha: bool,
    pub pressed_has_alpha: bool,
    pub normal_has_alpha: bool,
    pub always_dirty: bool,
    pub fit_rect_to_text: bool,
    pub scale_to_control: bool,
    pub loop_movie: bool,
}

/// What a record gives the table besides its fields: the layout, the four colours and the three texture rectangles.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Table {
    pub fields: TableFields,
    pub layout: Layout,
    pub background: UiColor,
    pub text_foreground: UiColor,
    pub selected_list_box: UiColor,
    pub focused_line: UiColor,
    pub normal_rect: UiRect,
    pub pressed_rect: UiRect,
    pub focused_rect: UiRect,
}

/// The default constructor's fields: an empty bitmap description. The C++ left `mStringTableID`, `mPageSize`,
/// `mAlwaysDirty`, `mFitRectToText`, `mScaleToControl` and `mLoopMovie` unset; they are zero here.
pub fn init() -> TableFields {
    TableFields {
        background_opacity: 0.0,
        text_box_opacity: 0.0,
        scroll_bar_bg_opacity: 0.0,
        unknown100: 0,
        id: 0,
        form_id: 0,
        group: 0,
        control_type: BITMAP,
        form_index: 0,
        border_thickness: 0,
        string_table_id: 0,
        font_table_index: 0,
        blink_rate: 0,
        string_table_begin: 0,
        string_table_end: 0,
        text_alignment: 0,
        max_chars: 0,
        start_current_selection: 0,
        start_range: 0,
        end_range: 0,
        current_range: 0,
        page_size: 0,
        is_visible: false,
        start_checked: false,
        list_box_always_dirty: false,
        sort_list_box: false,
        focused_has_alpha: false,
        pressed_has_alpha: false,
        normal_has_alpha: false,
        always_dirty: false,
        fit_rect_to_text: false,
        scale_to_control: false,
        loop_movie: false,
    }
}

/// `CalculateColor`: a record's 0..255 components scaled by `1.0f / 255.0f`.
pub fn color(red: c_ulong, green: c_ulong, blue: c_ulong) -> UiColor {
    const SCALE: f32 = 1.0 / 255.0;
    UiColor { red: red as f32 * SCALE, green: green as f32 * SCALE, blue: blue as f32 * SCALE }
}

/// A texture rectangle from the record's `long` edges (`GERectangle(long, long, long, long)`).
fn texture_rect(left: i32, top: i32, right: i32, bottom: i32) -> UiRect {
    UiRect::new(left as f32, top as f32, right as f32, bottom as f32)
}

/// The record constructor: every field, the layout, the colours and the texture rectangles from a database record
/// (the five file and sound names are the C++'s `UString`s). A help string id at or below zero is none.
pub fn from_record(r: &DbControl) -> Table {
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
    let fields = TableFields {
        background_opacity: r.background_opacity,
        text_box_opacity: r.text_box_opacity,
        scroll_bar_bg_opacity: r.scroll_bar_bg_opacity,
        unknown100: r.unused_db_record_id as c_long,
        id: r.id as c_long,
        form_id: r.form_id as c_long,
        group: r.group as c_ulong,
        control_type: r.control_type as c_ulong,
        form_index: r.form_index as c_long,
        border_thickness: r.border_thickness as c_long,
        string_table_id: if r.string_table_id > 0 { r.string_table_id as c_ulong } else { 0 },
        font_table_index: r.font_table_index as c_ulong,
        blink_rate: r.blink_rate as c_long,
        string_table_begin: r.string_table_begin as c_long,
        string_table_end: r.string_table_end as c_long,
        text_alignment: r.text_alignment as c_long,
        max_chars: r.max_chars as c_long,
        start_current_selection: r.start_current_selection as c_long,
        start_range: r.start_range as c_long,
        end_range: r.end_range as c_long,
        current_range: r.current_range as c_long,
        page_size: r.page_size as c_long,
        is_visible: r.is_visible != 0,
        start_checked: r.start_checked != 0,
        list_box_always_dirty: r.list_box_always_dirty != 0,
        sort_list_box: r.sort_list_box != 0,
        focused_has_alpha: r.focused_has_alpha != 0,
        pressed_has_alpha: r.pressed_has_alpha != 0,
        normal_has_alpha: r.normal_has_alpha != 0,
        always_dirty: r.always_dirty != 0,
        fit_rect_to_text: r.fit_rect_to_text != 0,
        scale_to_control: r.scale_to_control != 0,
        loop_movie: r.loop_movie != 0,
    };
    Table {
        fields,
        layout,
        background: color(r.background_red as c_ulong, r.background_green as c_ulong, r.background_blue as c_ulong),
        text_foreground: color(
            r.text_foreground_red as c_ulong,
            r.text_foreground_green as c_ulong,
            r.text_foreground_blue as c_ulong,
        ),
        selected_list_box: color(
            r.selected_list_box_red as c_ulong,
            r.selected_list_box_green as c_ulong,
            r.selected_list_box_blue as c_ulong,
        ),
        focused_line: color(
            r.focused_line_red as c_ulong,
            r.focused_line_green as c_ulong,
            r.focused_line_blue as c_ulong,
        ),
        normal_rect: texture_rect(r.normal_left, r.normal_top, r.normal_right, r.normal_bottom),
        focused_rect: texture_rect(r.focused_left, r.focused_top, r.focused_right, r.focused_bottom),
        pressed_rect: texture_rect(r.pressed_left, r.pressed_top, r.pressed_right, r.pressed_bottom),
    }
}

/// `Create` keeps a record: its id is not negative.
pub fn record_is_control(id: i32) -> bool {
    id >= 0
}

/// A table's identity as `AddControlLike` and `MoveControlsHorizontally` search the static array.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct TableKey {
    pub id: c_long,
    pub form_id: c_long,
    pub form_index: c_long,
}

/// The static array of tables (`sUIControlsArray`), a slot at a time.
#[allow(clippy::len_without_is_empty)]
pub trait TablesHost {
    /// `sUIControlsArray.size()`.
    fn len(&self) -> usize;
    /// Slot `i`'s id, form id and form index; `None` for a NULL slot.
    fn key(&self, i: usize) -> Option<TableKey>;
    /// Slot `i`'s layout (an existing table).
    fn layout(&self, i: usize) -> Layout;
    /// Writes slot `i`'s layout.
    fn set_layout(&mut self, i: usize, layout: &Layout);
}

/// `AddControlLike`'s searches: the slot of the template (the last table with that id) when no table has the new
/// id and no table of the template's form has the form index; `None` is `kErrorNotFound`.
pub fn find_template(h: &impl TablesHost, template_id: c_long, id: c_long, form_index: c_long) -> Option<usize> {
    let mut template = None;
    for i in 0..h.len() {
        if let Some(k) = h.key(i) {
            if k.id == id {
                return None;
            }
            if k.id == template_id {
                template = Some(i);
            }
        }
    }
    let t = template?;
    let form_id = h.key(t)?.form_id;
    for i in 0..h.len() {
        if let Some(k) = h.key(i) {
            if k.form_id == form_id && k.form_index == form_index {
                return None;
            }
        }
    }
    Some(t)
}

/// `AddControlLike`'s edits to the template's copy: its own id, form index and group, a rectangle in fractions of
/// the window, its string range and help string, no start selection (the C++ also clears the events and the font).
#[allow(clippy::too_many_arguments)]
pub fn like(
    fields: &mut TableFields,
    layout: &mut Layout,
    id: c_long,
    form_index: c_long,
    group: c_ulong,
    left: f32,
    right: f32,
    top: f32,
    bottom: f32,
    string_table_begin: c_long,
    string_table_end: c_long,
    help_string_id: c_ulong,
) {
    fields.id = id;
    fields.form_index = form_index;
    fields.group = group;
    *layout = Layout {
        left_offset: left,
        right_offset: right,
        top_offset: top,
        bottom_offset: bottom,
        left_type: PROPORTIONAL,
        right_type: PROPORTIONAL,
        top_type: PROPORTIONAL,
        bottom_type: PROPORTIONAL,
    };
    fields.string_table_begin = string_table_begin;
    fields.string_table_end = string_table_end;
    fields.string_table_id = help_string_id;
    fields.start_current_selection = 0;
}

/// `MoveControlsHorizontally`: the deltas added to the left and right offsets of the form's controls with a form
/// index in `first..=last` whose left and right are fractions of the window -> how many moved.
pub fn move_horizontally(
    h: &mut impl TablesHost,
    form_id: c_long,
    first: c_long,
    last: c_long,
    left_delta: f32,
    right_delta: f32,
) -> c_long {
    let mut count = 0;
    for i in 0..h.len() {
        let Some(k) = h.key(i) else { continue };
        if k.form_id != form_id || k.form_index < first || k.form_index > last {
            continue;
        }
        let mut l = h.layout(i);
        if l.left_type != PROPORTIONAL || l.right_type != PROPORTIONAL {
            continue;
        }
        l.left_offset += left_delta;
        l.right_offset += right_delta;
        h.set_layout(i, &l);
        count += 1;
    }
    count
}

/// `SetupFont`: a scaled font's size follows the window (not the control) when asked, and always for the controls
/// with lines of text (combo boxes, multi-line edit boxes and labels, list boxes, the scrolling label).
pub fn font_scales_to_window(control_type: c_ulong, scale_to_window: bool) -> bool {
    scale_to_window
        || matches!(
            control_type,
            EDITABLE_COMBO_BOX
                | COMBO_BOX
                | NUMBER_MULTI_LINE_EDIT_BOX
                | MULTI_LINE_EDIT_BOX
                | LIST_BOX
                | AUTO_SELECT_LIST_BOX
                | MULTI_LINE_TEXT_LABEL
                | NUMBER_MULTI_LINE_TEXT_LABEL
                | FILE_LIST_BOX
                | MULTI_LINE_HELP_TEXT_LABEL
                | SCROLLING_TEXT_LABEL
        )
}

/// A font scaled to the control: the control's height less twice the border.
pub fn control_font_size(height: f32, border: c_long) -> c_long {
    um_floor(height - border.wrapping_mul(2) as f32)
}

/// A font scaled to the window: the table font's height by the window's height over the form's designed height.
pub fn window_font_size(font_height: c_long, window_height: c_ulong, designed_height: c_ulong) -> c_long {
    um_floor(font_height as f32 * window_height as f32 / designed_height as f32)
}

/// `SetupStringArray`: the strings `begin..=end` of the string table, none unless `begin` is positive and `end` not
/// before it -> their count.
pub fn string_count(begin: c_long, end: c_long) -> c_ulong {
    if begin > 0 && end >= begin {
        (end - begin + 1) as c_ulong
    } else {
        0
    }
}

/// `CreateControl`'s and `RelayoutControl`'s parent: the window, one pixel short on the right and bottom unless the
/// form uses exclusive coordinates.
pub fn parent_rect(window_width: c_ulong, window_height: c_ulong, exclusive: bool) -> UiRect {
    let offset = if exclusive { 0.0 } else { 1.0 };
    UiRect::new(0.0, 0.0, window_width as f32 - offset, window_height as f32 - offset)
}

/// The rectangle a control's font is sized from: the control's, one pixel wider and taller under exclusive
/// coordinates.
pub fn font_rect(rect: &UiRect, exclusive: bool) -> UiRect {
    let mut r = *rect;
    if exclusive {
        r.right += 1.0;
        r.bottom += 1.0;
    }
    r
}

/// `CreateComboBox`: the closed box's rectangle (the font's height plus the borders tall when the font is scaled to
/// the control, else 20 pixels) and the list's height (the rest of the control's rectangle, at least 1).
pub fn combo_list(rect: &UiRect, scale_to_control: bool, font_height: c_long, border: c_long) -> (UiRect, c_long) {
    let bottom = if scale_to_control {
        rect.top + (font_height as f32 + border as f32 * 2.0)
    } else {
        rect.top + SCROLL_BAR_WIDTH
    };
    let list = UiRect::new(rect.left, rect.top, rect.right, bottom);
    let height = if rect.height() > list.height() { um_floor(rect.height() - list.height()) } else { 1 };
    (list, height)
}

/// `CreateFileComboBox`: the box's width (one more than the rectangle's) and height (the rectangle's, floored, plus
/// one).
pub fn file_combo_size(rect: &UiRect) -> (f32, c_long) {
    (rect.width() + 1.0, um_floor(rect.height()) + 1)
}

/// `CreateScrollBar`: a bar's length, one more than the rectangle's height (vertical) or width.
pub fn scroll_bar_length(rect: &UiRect, vertical: bool) -> f32 {
    if vertical {
        rect.height() + 1.0
    } else {
        rect.width() + 1.0
    }
}

/// `CreateSlider`: the page size, capped below the range.
pub fn slider_page_size(start_range: c_long, end_range: c_long, page_size: c_long) -> c_long {
    let range = end_range.wrapping_sub(start_range);
    if page_size >= range {
        range.wrapping_sub(1)
    } else {
        page_size
    }
}

#[cfg(test)]
mod tests;
