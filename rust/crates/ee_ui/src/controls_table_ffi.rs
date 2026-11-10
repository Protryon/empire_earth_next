//! `extern "C"` for `UIControlsTable` (`User Interface/UIControlsTable.cpp`): `refs` points at one table's members
//! (its record from `mBackgroundOpacity` on, its layout, colours and texture rectangles); the static array is read
//! through `ee_uicpp_controls_table_*` (UIControlsTable.cpp) a slot at a time, never cached.
#![allow(clippy::missing_safety_doc, clippy::too_many_arguments)]

use core::ffi::{c_long, c_ulong};

use crate::button::UiColor;
use crate::controls_table::{self, DbControl, TableFields, TableKey, TablesHost};
use crate::layout::Layout;
use crate::rect::UiRect;

/// cbindgen:ignore
mod sys {
    use core::ffi::c_ulong;

    use crate::controls_table::TableKey;
    use crate::layout::Layout;

    extern "C" {
        pub fn ee_uicpp_controls_table_count() -> c_ulong;
        pub fn ee_uicpp_controls_table_key(index: c_ulong, key: *mut TableKey) -> bool;
        pub fn ee_uicpp_controls_table_layout(index: c_ulong) -> *mut Layout;
    }
}

/// Where one `UIControlsTable` keeps what the Rust side writes.
#[repr(C)]
pub struct TableRefs {
    pub fields: *mut TableFields,
    pub layout: *mut Layout,
    pub background: *mut UiColor,
    pub text_foreground: *mut UiColor,
    pub selected_list_box: *mut UiColor,
    pub focused_line: *mut UiColor,
    pub normal_rect: *mut UiRect,
    pub pressed_rect: *mut UiRect,
    pub focused_rect: *mut UiRect,
}

struct Array;

impl TablesHost for Array {
    fn len(&self) -> usize {
        unsafe { sys::ee_uicpp_controls_table_count() as usize }
    }
    fn key(&self, i: usize) -> Option<TableKey> {
        let mut k = TableKey { id: 0, form_id: 0, form_index: 0 };
        unsafe { sys::ee_uicpp_controls_table_key(i as c_ulong, &mut k) }.then_some(k)
    }
    fn layout(&self, i: usize) -> Layout {
        unsafe { sys::ee_uicpp_controls_table_layout(i as c_ulong).read() }
    }
    fn set_layout(&mut self, i: usize, layout: &Layout) {
        unsafe { sys::ee_uicpp_controls_table_layout(i as c_ulong).write(*layout) }
    }
}

/// The default constructor's fields (written whole).
#[no_mangle]
pub unsafe extern "C" fn ee_ui_controls_table_init(refs: *const TableRefs) {
    (*refs).fields.write(controls_table::init());
}

/// The record constructor: the fields, layout, colours and texture rectangles from a database record (written whole).
#[no_mangle]
pub unsafe extern "C" fn ee_ui_controls_table_from_record(refs: *const TableRefs, record: *const DbControl) {
    let t = controls_table::from_record(&*record);
    let r = &*refs;
    r.fields.write(t.fields);
    r.layout.write(t.layout);
    r.background.write(t.background);
    r.text_foreground.write(t.text_foreground);
    r.selected_list_box.write(t.selected_list_box);
    r.focused_line.write(t.focused_line);
    r.normal_rect.write(t.normal_rect);
    r.pressed_rect.write(t.pressed_rect);
    r.focused_rect.write(t.focused_rect);
}

/// `Create` keeps a record whose id is not negative.
#[no_mangle]
pub extern "C" fn ee_ui_controls_table_record_is_control(id: i32) -> bool {
    controls_table::record_is_control(id)
}

/// `AddControlLike`'s searches over the static array -> whether a template was found, at `*index`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_controls_table_find_template(
    template_id: c_long,
    id: c_long,
    form_index: c_long,
    index: *mut c_ulong,
) -> bool {
    match controls_table::find_template(&Array, template_id, id, form_index) {
        Some(i) => {
            index.write(i as c_ulong);
            true
        }
        None => false,
    }
}

/// `AddControlLike`'s edits to the template's copy.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_controls_table_like(
    refs: *const TableRefs,
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
    let r = &*refs;
    let mut fields = r.fields.read();
    let mut layout = r.layout.read();
    controls_table::like(
        &mut fields,
        &mut layout,
        id,
        form_index,
        group,
        left,
        right,
        top,
        bottom,
        string_table_begin,
        string_table_end,
        help_string_id,
    );
    r.fields.write(fields);
    r.layout.write(layout);
}

/// `MoveControlsHorizontally` -> how many tables moved.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_controls_table_move_horizontally(
    form_id: c_long,
    first: c_long,
    last: c_long,
    left_delta: f32,
    right_delta: f32,
) -> c_long {
    controls_table::move_horizontally(&mut Array, form_id, first, last, left_delta, right_delta)
}

/// `SetupFont`: the scaled font follows the window.
#[no_mangle]
pub extern "C" fn ee_ui_controls_table_font_scales_to_window(control_type: c_ulong, scale_to_window: bool) -> bool {
    controls_table::font_scales_to_window(control_type, scale_to_window)
}

/// `SetupFont`: the size of a font scaled to the control.
#[no_mangle]
pub extern "C" fn ee_ui_controls_table_control_font_size(height: f32, border: c_long) -> c_long {
    controls_table::control_font_size(height, border)
}

/// `SetupFont`: the size of a font scaled to the window.
#[no_mangle]
pub extern "C" fn ee_ui_controls_table_window_font_size(
    font_height: c_long,
    window_height: c_ulong,
    designed_height: c_ulong,
) -> c_long {
    controls_table::window_font_size(font_height, window_height, designed_height)
}

/// `SetupStringArray`: how many strings the range names (0: none).
#[no_mangle]
pub extern "C" fn ee_ui_controls_table_string_count(begin: c_long, end: c_long) -> c_ulong {
    controls_table::string_count(begin, end)
}

/// `CreateControl`'s parent rectangle.
#[no_mangle]
pub extern "C" fn ee_ui_controls_table_parent_rect(
    window_width: c_ulong,
    window_height: c_ulong,
    exclusive: bool,
) -> UiRect {
    controls_table::parent_rect(window_width, window_height, exclusive)
}

/// `CreateControl`'s font rectangle.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_controls_table_font_rect(rect: *const UiRect, exclusive: bool) -> UiRect {
    controls_table::font_rect(&*rect, exclusive)
}

/// `CreateComboBox`: the closed box's rectangle, the list's height at `*list_height`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_controls_table_combo_list(
    rect: *const UiRect,
    scale_to_control: bool,
    font_height: c_long,
    border: c_long,
    list_height: *mut c_long,
) -> UiRect {
    let (list, height) = controls_table::combo_list(&*rect, scale_to_control, font_height, border);
    list_height.write(height);
    list
}

/// `CreateFileComboBox`: the box's width, its height at `*height`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_controls_table_file_combo_size(rect: *const UiRect, height: *mut c_long) -> f32 {
    let (width, h) = controls_table::file_combo_size(&*rect);
    height.write(h);
    width
}

/// `CreateScrollBar`: the bar's length.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_controls_table_scroll_bar_length(rect: *const UiRect, vertical: bool) -> f32 {
    controls_table::scroll_bar_length(&*rect, vertical)
}

/// `CreateSlider`: the page size, capped below the range.
#[no_mangle]
pub extern "C" fn ee_ui_controls_table_slider_page_size(
    start_range: c_long,
    end_range: c_long,
    page_size: c_long,
) -> c_long {
    controls_table::slider_page_size(start_range, end_range, page_size)
}
