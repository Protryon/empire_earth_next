//! The status bar buttons' (`EEUIFMGStatusBar`, an improvement button drawn as an icon, a bar and two value labels)
//! C++ bodies in Rust over one-statement C++ shims (UI lane round 8; the shims are `ee_screenscpp_sba_*` in
//! `User Interface Thread/EEUIFMGStatusBar.cpp`): the constructor's layout, the destructor, `CopyData`,
//! `DrawPolygons` and `DrawTextA`. `bar` is the `EEUIFMGStatusBar*`.

use core::ffi::c_long;

use crate::screens::game_cpp::{Cpp, EeScreensShimText};

/// `kScenarioEditorInset`, `kInset`: the bar's inset in the button's height.
const SBA_EDITOR_INSET: f32 = 0.43;
const SBA_INSET: f32 = 0.46;
/// The value labels (`kSBAValues`): the bar's value under it, the second over it.
const SBA_VALUE: u32 = 0;
const SBA_VALUE2: u32 = 1;

/// A label's font: the closest to `height`, with its bitmap made when it has none and the form draws text itself.
fn sba_font(c: &mut impl Cpp, form: usize, height: f32) -> usize {
    let font = c.sba_closest_font(height as u32);
    if font != 0 && !c.sba_font_has_bitmap(font) && !c.sba_form_gdi(form) {
        c.sba_font_bitmap_new(font, form);
    }
    font
}

/// A value label's look: right aligned, white, transparent.
fn sba_value_style(c: &mut impl Cpp, bar: usize, which: u32) {
    c.sba_value_right(bar, which);
    c.sba_value_white(bar, which);
    c.sba_value_transparent(bar, which);
}

/// The constructor's body (`rect` its `GERectangle*`, `form` its `UIForm*`): the icon, the green bar and the two
/// value labels laid out for the main game (`large_icon`: the icon as high as the inset) or the scenario editor.
pub(crate) fn construct(c: &mut impl Cpp, bar: usize, rect: usize, form: usize, editor: bool, large_icon: bool) {
    let (left, top, right, bottom) =
        (c.sba_rect_left(rect), c.sba_rect_top(rect), c.sba_rect_right(rect), c.sba_rect_bottom(rect));
    let height = c.sba_rect_height(rect);
    c.sba_set_pressed(bar, false);
    let (inset, icon, bar_left);
    if editor {
        inset = height * SBA_EDITOR_INSET;
        icon = inset;
        bar_left = left + c.sba_border(bar);
    } else if large_icon {
        inset = height * SBA_INSET;
        icon = inset;
        bar_left = left + c.sba_border(bar);
    } else {
        inset = height * SBA_INSET;
        let width = c.sba_rect_width(rect);
        let size = if width < height { width } else { height };
        let icon_size = size - (c.sba_border(bar) * 2.0 + 0.125);
        icon = icon_size;
        bar_left = left + icon;
    }
    let border = c.sba_border(bar);
    let icon_left = left + border;
    let icon_top = top + border;
    c.sba_icon_new(bar, form, icon_left, icon_top, icon_left + icon, icon_top + icon);
    c.sba_bar_new(bar, form, bar_left, top + inset, right - (border + 2.0), bottom - inset);
    c.sba_bar_verify(bar);
    c.sba_bar_green(bar);
    c.sba_bar_grey(bar);
    c.sba_bar_opaque(bar);
    let value_top = c.sba_bar_bottom(bar) + 2.0;
    let value_bottom = bottom - border;
    let font = sba_font(c, form, value_bottom - value_top);
    c.sba_value_new(bar, SBA_VALUE, font, form, left + border, value_top, right - border, value_bottom);
    c.sba_value_verify(bar);
    sba_value_style(c, bar, SBA_VALUE);
    let value2_top = top + border + 2.0;
    let value2_bottom = c.sba_bar_top(bar);
    let font = sba_font(c, form, value2_bottom - value2_top);
    c.sba_value_new(bar, SBA_VALUE2, font, form, left + border, value2_top, right - border, value2_bottom);
    c.sba_font_dereference(font);
    sba_value_style(c, bar, SBA_VALUE2);
    c.sba_bitmap140_new(bar);
    c.sba_bitmap144_new(bar);
    c.sba_count_new(bar, font);
    let (x, y) = (c.sba_left(bar) + border * 2.0, c.sba_top(bar) + border * 2.0);
    c.sba_count_at(bar, x, y);
    c.sba_count_white(bar);
    c.sba_count_transparent(bar);
    c.sba_set_show_count(bar, false);
    if editor {
        c.sba_black(bar);
        c.sba_bg_opaque(bar);
    }
}

/// The destructor's body.
pub(crate) fn destroy(c: &mut impl Cpp, bar: usize) {
    c.sba_bar_delete(bar);
    c.sba_icon_delete(bar);
    c.sba_value_delete(bar, SBA_VALUE);
    c.sba_value_delete(bar, SBA_VALUE2);
    c.sba_bitmap140_dereference(bar);
    c.sba_bitmap144_dereference(bar);
    c.sba_count_delete(bar);
}

/// `CopyData`: the button and its parts; the number of queued improvements shown.
pub(crate) fn copy_data(c: &mut impl Cpp, bar: usize) -> i64 {
    c.sba_base_copy_data(bar);
    c.sba_bar_copy_data(bar);
    c.sba_icon_copy_data(bar);
    c.sba_value_copy_data(bar, SBA_VALUE);
    let result = c.sba_value_copy_data(bar, SBA_VALUE2);
    let queued = c.sba_queued(bar) as c_long;
    if queued > 0 {
        let text = format!("{queued}");
        c.sba_count_text(bar, EeScreensShimText::of(text.as_bytes()));
        c.sba_set_show_count(bar, true);
    } else {
        c.sba_set_show_count(bar, false);
    }
    c.sba_copy_enabled(bar);
    result
}

/// `DrawPolygons`: the button's face (when enabled), the icon, the bar and the labels.
pub(crate) fn draw_polygons(c: &mut impl Cpp, bar: usize) -> i64 {
    if c.sba_copied_enabled(bar) {
        c.sba_button_draw_polygons(bar);
    }
    c.sba_icon_draw_polygons(bar);
    c.sba_bar_draw_polygons(bar);
    c.sba_value_draw_polygons(bar, SBA_VALUE);
    c.sba_value_draw_polygons(bar, SBA_VALUE2)
}

/// `DrawTextA`: the parts' texts, the second label when it has text, and the queued count.
pub(crate) fn draw_text(c: &mut impl Cpp, bar: usize) -> i64 {
    let (x, y) = (c.sba_count_x(bar), c.sba_count_y(bar));
    let (bottom, right) = (y + c.sba_height(bar), x + c.sba_width(bar));
    c.sba_control_draw_text(bar);
    c.sba_bar_draw_text(bar);
    c.sba_icon_draw_text(bar);
    let mut result = c.sba_value_draw_text(bar, SBA_VALUE);
    if c.sba_value2_has_text(bar) {
        result = c.sba_value_draw_text(bar, SBA_VALUE2);
    }
    if c.sba_show_count(bar) {
        result = c.sba_draw_count(bar, x, y, right, bottom);
    }
    result
}

#[cfg(not(test))]
mod ffi {
    use crate::screens::game_cpp::Ffi;

    /// `EEUIFMGStatusBar`'s constructor's body (`rect` the `GERectangle*`, `form` the `UIForm*`).
    #[no_mangle]
    pub extern "C" fn ee_screens_sba_construct(bar: usize, rect: usize, form: usize, editor: bool, large_icon: bool) {
        super::construct(&mut Ffi, bar, rect, form, editor, large_icon)
    }
    /// `~EEUIFMGStatusBar`'s body.
    #[no_mangle]
    pub extern "C" fn ee_screens_sba_destroy(bar: usize) {
        super::destroy(&mut Ffi, bar)
    }
    /// `EEUIFMGStatusBar::CopyData`.
    #[no_mangle]
    pub extern "C" fn ee_screens_sba_copy_data(bar: usize) -> i64 {
        super::copy_data(&mut Ffi, bar)
    }
    /// `EEUIFMGStatusBar::DrawPolygons`.
    #[no_mangle]
    pub extern "C" fn ee_screens_sba_draw_polygons(bar: usize) -> i64 {
        super::draw_polygons(&mut Ffi, bar)
    }
    /// `EEUIFMGStatusBar::DrawTextA`.
    #[no_mangle]
    pub extern "C" fn ee_screens_sba_draw_text(bar: usize) -> i64 {
        super::draw_text(&mut Ffi, bar)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::screens::game_cpp::Fake;

    const BAR: usize = 0xb0;

    #[test]
    fn the_main_game_layout() {
        let mut f = Fake::default();
        // a 100 x 50 button at (10, 20), border 1: the icon as wide as the height less the border's
        f.answer("sba_rect_left", &[10]).answer("sba_rect_top", &[20]).answer("sba_rect_right", &[110]);
        f.answer("sba_rect_bottom", &[70]).answer("sba_rect_height", &[50]).answer("sba_rect_width", &[100]);
        f.answer("sba_border", &[1, 1]).answer("sba_bar_bottom", &[50]).answer("sba_bar_top", &[43]);
        f.answer("sba_closest_font", &[5, 6]).answer("sba_left", &[10]).answer("sba_top", &[20]);
        construct(&mut f, BAR, 1, 2, false, false);
        let calls = f.calls();
        assert!(calls.contains("sba_icon_new(176, 2, 11.0, 21.0, 58.875, 68.875)"));
        assert!(calls.contains("sba_bar_new(176, 2, 57.875, 43.0, 107.0, 47.0)"));
        assert!(
            calls.contains("sba_closest_font(17)\nsba_font_has_bitmap(5)\nsba_form_gdi(2)\nsba_font_bitmap_new(5, 2)")
        );
        assert!(calls.contains("sba_value_new(176, 0, 5, 2, 11.0, 52.0, 109.0, 69.0)"));
        assert!(calls.contains("sba_value_new(176, 1, 6, 2, 11.0, 23.0, 109.0, 43.0)\nsba_font_dereference(6)"));
        assert!(calls.contains("sba_count_new(176, 6)\nsba_left(176)\nsba_top(176)\nsba_count_at(176, 12.0, 22.0)"));
        assert!(calls.ends_with("sba_set_show_count(176, false)"));
    }

    #[test]
    fn the_editor_layout_is_black() {
        let mut f = Fake::default();
        f.answer("sba_rect_height", &[100]);
        construct(&mut f, BAR, 1, 2, true, false);
        assert!(f.calls().contains("sba_bar_new(176, 2, 0.0, 43.0, -2.0, -43.0)"));
        assert!(f.calls().ends_with("sba_black(176)\nsba_bg_opaque(176)"));
    }

    #[test]
    fn copy_and_draw() {
        let mut f = Fake::default();
        f.answer("sba_queued", &[3]).answer("sba_value_copy_data", &[0, 9]);
        assert_eq!(copy_data(&mut f, BAR), 9);
        assert!(f.calls().contains("sba_count_text(176, \"3\")\nsba_set_show_count(176, true)\nsba_copy_enabled(176)"));
        let mut f = Fake::default();
        f.answer("sba_value_draw_text", &[4]).answer("sba_show_count", &[1]).answer("sba_draw_count", &[8]);
        f.answer("sba_count_x", &[1]).answer("sba_width", &[10]).answer("sba_height", &[5]);
        assert_eq!(draw_text(&mut f, BAR), 8);
        assert!(f.calls().ends_with("sba_show_count(176)\nsba_draw_count(176, 1.0, 0.0, 11.0, 5.0)"));
        let mut f = Fake::default();
        f.answer("sba_value_draw_polygons", &[0, 2]);
        assert_eq!(draw_polygons(&mut f, BAR), 2);
        destroy(&mut f, BAR);
        assert!(f.calls().ends_with("sba_count_delete(176)"));
    }
}
