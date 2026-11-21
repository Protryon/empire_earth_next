//! The HUD classes' remaining multi-statement C++ bodies in Rust over one-statement shims (UI lane round 12):
//! `EEUIFMainGame`'s destructor, `CreateGroup` and `IsInGameView`; `EEUIBUpgrade`'s constructor body and its draw,
//! dirty, mouse-over and press overrides; `EECBUIListBox`'s constructor body, `AppendStringArray` and `CopyData`. Each
//! C++ member is now one forward; the order of the calls is the original's.

// the forwards are the FFI module's (absent from a test binary)
#![cfg_attr(test, allow(dead_code))]

use crate::screens::game_cpp::{Cpp, EeScreensShimOut, EeScreensShimPtrOut};
use crate::screens::panel_groups::{
    EE_SCREENS_GROUP_CHAT, EE_SCREENS_GROUP_CIV_BUILDER, EE_SCREENS_GROUP_DIPLOMACY, EE_SCREENS_GROUP_OPTIONS,
    EE_SCREENS_GROUP_STATISTICS,
};

/// `EEUIFMainGame::CreateGroup(index)`: the main panel group (0) or one of the five Rust groups.
pub(crate) fn main_create_group<C: Cpp + ?Sized>(c: &mut C, form: usize, index: u32) {
    match index {
        0 => c.mfb_group_main(form),
        EE_SCREENS_GROUP_STATISTICS
        | EE_SCREENS_GROUP_CHAT
        | EE_SCREENS_GROUP_DIPLOMACY
        | EE_SCREENS_GROUP_OPTIONS
        | EE_SCREENS_GROUP_CIV_BUILDER => c.mfb_group_rust(form, index),
        _ => {}
    }
}

/// `EEUIFMainGame::IsInGameView(point)`: in the game view and not under the visible unit information panel.
pub(crate) fn main_in_game_view<C: Cpp + ?Sized>(c: &mut C, form: usize, x: f32, y: f32) -> bool {
    let mut outside = true;
    let info = c.mfb_unit_info(form);
    if info != 0 && c.mfb_background_visible(info) {
        outside = !c.mfb_background_contains(info, x, y);
    }
    c.mfb_view_contains(x, y) && outside
}

/// `~EEUIFMainGame`: the form's actions released, the grab scroll let go, the error section destroyed, the six
/// panel groups, the poll and the unit hack deleted.
pub(crate) fn main_destroy<C: Cpp + ?Sized>(c: &mut C, form: usize) {
    c.mfb_grab_scroll_dereference(form);
    c.mfb_release_grab_scroll();
    c.mfb_wheel_zoom_dereference(form);
    c.mfb_scroll_dereference(form);
    c.mfb_mouse_click_dereference(form);
    c.mfb_lasso_dereference(form);
    c.mfb_destroy_error_section(form);
    for i in 0..6 {
        c.mfb_group_delete(form, i);
    }
    c.mfb_poll_delete(form);
    c.mfb_build_dereference(form);
    c.mfb_calamity_dereference(form);
    c.mfb_unit_hack_delete(form);
}

/// `kBgOpacity` (EEUIBUpgrade.cpp).
const UPGRADE_OPACITY: f32 = 0.15;

/// `EEUIBUpgrade(..)`'s body: nothing queued, level 0, no message yet, no cost label, faintly tinted with a thin
/// border, enabled.
pub(crate) fn upgrade_construct<C: Cpp + ?Sized>(c: &mut C, b: usize) {
    c.upb_set_num_queued(b, 0);
    c.upb_set_level(b, 0);
    c.upb_set_last_message(b, 0);
    c.upb_set_cost_label(b, 0);
    c.upb_bg_opacity(b, UPGRADE_OPACITY);
    c.upb_border_opacity(b, UPGRADE_OPACITY * 3.0);
    c.upb_border_thickness(b, 1);
    c.upb_set_disabled(b, false);
}

/// `ButtonDown`: pressed unless disabled; the help text updated.
pub(crate) fn upgrade_button_down<C: Cpp + ?Sized>(c: &mut C, b: usize) {
    if !c.upb_disabled(b) {
        c.upb_base_button_down(b);
    }
    c.upb_update_help(b);
}

/// `CopyData`: the button's, then the cost label's (its rectangle dirtied first).
pub(crate) fn upgrade_copy_data<C: Cpp + ?Sized>(c: &mut C, b: usize) -> i64 {
    let mut result = c.upb_base_copy_data(b);
    let label = c.upb_cost_label(b);
    if result == 0 && label != 0 {
        c.upb_label_dirty_rect(b, label);
        result = c.upb_label_copy_data(label);
    }
    result
}

/// `DrawTextA`: the button's text, then the cost label's.
pub(crate) fn upgrade_draw_text<C: Cpp + ?Sized>(c: &mut C, b: usize) -> i64 {
    let mut result = c.upb_base_draw_text(b);
    let label = c.upb_cost_label(b);
    if result == 0 && label != 0 {
        result = c.upb_label_draw_text(label);
    }
    result
}

/// `MouseOver`: the hover state only when the improvement can be bought.
pub(crate) fn upgrade_mouse_over<C: Cpp + ?Sized>(c: &mut C, b: usize) {
    let mut unit = 0usize;
    if c.upb_can_upgrade(b, EeScreensShimPtrOut::of(&mut unit)) {
        c.upb_base_mouse_over(b);
    }
}

/// `MouseRButtonDown`: the mouse captured, the button pressed -> 0.
pub(crate) fn upgrade_right_down<C: Cpp + ?Sized>(c: &mut C, b: usize) -> i64 {
    c.upb_capture(b);
    c.upb_button_down(b);
    0
}

/// `SetDirty(dirty)`: the cost label too when dirtied.
pub(crate) fn upgrade_set_dirty<C: Cpp + ?Sized>(c: &mut C, b: usize, dirty: bool) {
    if dirty {
        let label = c.upb_cost_label(b);
        if label != 0 {
            c.upb_label_set_dirty(label);
        }
    }
    c.upb_base_set_dirty(b, dirty);
}

/// `kScrollBarBgOpacity` (EECBUIListBox.cpp).
const CB_SCROLL_OPACITY: f32 = 0.2;

/// `EECBUIListBox(..)`'s body: the table's text, a black header, upgrades only, the points shown, sorted, the
/// tabbing flag, a white scroll bar at 0.2, line 0 selected without an event (`select`: the list's own
/// `ChangeSelectionIndex(0, false, false)`).
pub(crate) fn cb_list_construct<C: Cpp + ?Sized>(
    c: &mut C,
    table: usize,
    list: usize,
    tabable: bool,
    mut select: impl FnMut(usize),
) {
    c.cbb_setup_text(table, list);
    c.cbb_set_header_colour(list, 0.0, 0.0, 0.0);
    c.cbb_set_select_upgrades_only(list, true);
    c.cbb_set_show_points(list, true);
    c.cbb_set_sort_lines(list, true);
    c.cbb_set_tabable(list, tabable);
    c.cbb_scroll_bg_colour(list, 1.0, 1.0, 1.0);
    c.cbb_scroll_bg_opacity(list, CB_SCROLL_OPACITY);
    select(list);
}

/// `AppendStringArray(lines)`: each line through the list's `AddLine` (`add`).
pub(crate) fn cb_list_append<C: Cpp + ?Sized>(c: &mut C, list: usize, lines: usize, mut add: impl FnMut(usize, &[u8])) {
    let mut i = 0;
    while i < c.cbb_lines_count(lines) {
        let mut text = Vec::new();
        c.cbb_line(lines, i, EeScreensShimOut::of(&mut text));
        add(list, &text);
        i += 1;
    }
}

/// `CopyData`: the base's, no highlight on line 0, the header colour and the points flag snapshotted.
pub(crate) fn cb_list_copy_data<C: Cpp + ?Sized>(c: &mut C, list: usize) -> i64 {
    let result = c.cbb_base_copy_data(list);
    if c.cbb_selection(list) == 0 {
        c.cbb_copied_no_selection(list);
    }
    c.cbb_copy_header(list);
    c.cbb_copy_show_points(list);
    result
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::screens::game_cpp::Fake;

    #[test]
    fn the_groups_and_the_game_view() {
        let mut fake = Fake::default();
        main_create_group(&mut fake, 9, 0);
        main_create_group(&mut fake, 9, 5);
        main_create_group(&mut fake, 9, 6);
        assert_eq!(fake.calls(), "mfb_group_main(9)\nmfb_group_rust(9, 5)");
        let mut fake = Fake::default();
        fake.answer("mfb_unit_info", &[30, 0]).answer("mfb_background_visible", &[1]);
        fake.answer("mfb_background_contains", &[1]).answer("mfb_view_contains", &[1, 1]);
        assert!(!main_in_game_view(&mut fake, 9, 1.0, 2.0));
        assert!(main_in_game_view(&mut fake, 9, 1.0, 2.0));
    }

    #[test]
    fn the_destructor_deletes_every_group() {
        let mut fake = Fake::default();
        main_destroy(&mut fake, 9);
        let calls = fake.calls();
        assert!(calls.starts_with("mfb_grab_scroll_dereference(9)\nmfb_release_grab_scroll()\n"));
        assert!(calls.contains("mfb_group_delete(9, 5)\nmfb_poll_delete(9)"));
        assert!(calls.ends_with("mfb_unit_hack_delete(9)"));
    }

    #[test]
    fn the_upgrade_button() {
        let mut fake = Fake::default();
        upgrade_construct(&mut fake, 4);
        assert!(fake
            .calls()
            .contains("upb_bg_opacity(4, 0.15)\nupb_border_opacity(4, 0.45000002)\nupb_border_thickness(4, 1)"));
        let mut fake = Fake::default();
        fake.answer("upb_disabled", &[1, 0]);
        upgrade_button_down(&mut fake, 4);
        upgrade_button_down(&mut fake, 4);
        assert_eq!(
            fake.calls(),
            "upb_disabled(4)\nupb_update_help(4)\nupb_disabled(4)\nupb_base_button_down(4)\nupb_update_help(4)"
        );
        let mut fake = Fake::default();
        fake.answer("upb_base_copy_data", &[0]).answer("upb_cost_label", &[7, 7]).answer("upb_label_copy_data", &[3]);
        assert_eq!(upgrade_copy_data(&mut fake, 4), 3);
        assert_eq!(upgrade_draw_text(&mut fake, 4), 0);
        assert!(fake.calls().contains("upb_label_dirty_rect(4, 7)\nupb_label_copy_data(7)"));
        let mut fake = Fake::default();
        fake.answer("upb_cost_label", &[7]);
        upgrade_set_dirty(&mut fake, 4, true);
        upgrade_set_dirty(&mut fake, 4, false);
        assert_eq!(
            fake.calls(),
            "upb_cost_label(4)\nupb_label_set_dirty(7)\nupb_base_set_dirty(4, true)\nupb_base_set_dirty(4, false)"
        );
        let mut fake = Fake::default();
        fake.answer("upb_can_upgrade", &[1]);
        upgrade_mouse_over(&mut fake, 4);
        assert_eq!(upgrade_right_down(&mut fake, 4), 0);
        assert_eq!(fake.calls(), "upb_can_upgrade(4, out)\nupb_base_mouse_over(4)\nupb_capture(4)\nupb_button_down(4)");
    }

    #[test]
    fn the_civilization_list() {
        let mut fake = Fake::default();
        let mut selected = Vec::new();
        cb_list_construct(&mut fake, 1, 2, true, |l| selected.push(l));
        assert_eq!(selected, [2]);
        assert!(fake.calls().ends_with(
            "cbb_set_tabable(2, true)\ncbb_scroll_bg_colour(2, 1.0, 1.0, 1.0)\ncbb_scroll_bg_opacity(2, 0.2)"
        ));
        let mut fake = Fake::default();
        fake.answer("cbb_lines_count", &[2, 2, 2]).text("cbb_line", b"a").text("cbb_line", b"b");
        let mut lines = Vec::new();
        cb_list_append(&mut fake, 2, 8, |_, t| lines.push(t.to_vec()));
        assert_eq!(lines, [b"a".to_vec(), b"b".to_vec()]);
        let mut fake = Fake::default();
        fake.answer("cbb_base_copy_data", &[5]).answer("cbb_selection", &[0]);
        assert_eq!(cb_list_copy_data(&mut fake, 2), 5);
        assert!(fake.calls().contains("cbb_copied_no_selection(2)"));
    }
}
