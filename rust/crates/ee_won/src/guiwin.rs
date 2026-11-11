//! WONGUI's window manager's popup and modality rules and the popup menus' timing (WindowManager.cpp,
//! MenuBehavior.cpp: the lobby's drop lists, its right-click menus and its dialogs). The C++ keeps the windows,
//! their native backend, the popup list, the modal component, the events, focus and mouse capture; these answer:
//!
//! - where a popup goes: the mouse when no place is given, its own window or the parent's, moved back inside the
//!   screen or the root and flipped to the other side of the rectangle it must not cover (`DoPopup`,
//!   `AdjustPopupPosition`);
//! - whether another popup joins the open ones (`DoPopup`), which popup window a mouse event belongs to
//!   (`FindMouseWindow`), whether a press ends the popups (`CheckPopupClick`);
//! - the modality: whether the windows go modal, whether the dialog is marked modal itself, and the one window
//!   left free (`CalcModality`); whether a window is blocked by another's dialog (`NotModalWindow`) and whether an
//!   `EndDialog` names the current dialog;
//! - a popup menu's submenu delay (`TimerEvent`), its swallowed first release (`MouseUp`), the keyboard mode's end
//!   (`MouseMove`), an item's size (`MenuItem::CalcDimensions`) and the submenu's place (`CheckSubMenu`).
//!
//! Ints wrap (the game builds with -fwrapv).

use crate::guictl::EeWonRect4;

pub const POPUP_WANT_INPUT: i32 = 0x0080;
pub const POPUP_IS_MODAL: i32 = 0x0100;
pub const POPUP_ADJUST_POSITION: i32 = 0x0200;
pub const POPUP_EAT_MOUSE_DOWN: i32 = 0x0800;
pub const POPUP_ADDITIONAL: i32 = 0x1000;
pub const POPUP_DONT_WANT_MOUSE: i32 = 0x4000;
pub const POPUP_NOT_IN_OWN_WINDOW: i32 = 0x8000;
pub const POPUP_IN_OWN_WINDOW: i32 = 0x10000;

fn right(r: &EeWonRect4) -> i32 {
    r.x.wrapping_add(r.w)
}

fn bottom(r: &EeWonRect4) -> i32 {
    r.y.wrapping_add(r.h)
}

/// `AdjustPopupPosition`: the popup's `SetPos` calls (each place in turn; none when it fits) that move it back
/// inside `bounds`, past the side it overflows, and off `avoid` when it then covers it.
pub fn popup_adjust(popup: EeWonRect4, bounds: EeWonRect4, avoid: EeWonRect4) -> Vec<(i32, i32)> {
    let mut p = popup;
    let mut moves = Vec::new();
    let mut set = |p: &mut EeWonRect4, x: i32, y: i32| {
        p.x = x;
        p.y = y;
        moves.push((x, y));
    };
    let covers_x = |p: &EeWonRect4| avoid.w > 0 && p.x <= right(&avoid) && right(p) >= avoid.x;
    let covers_y = |p: &EeWonRect4| avoid.h > 0 && p.y <= bottom(&avoid) && bottom(p) >= avoid.y;
    if right(&p) > right(&bounds) {
        let dx = right(&bounds).wrapping_sub(right(&p));
        let (x, y) = (p.x.wrapping_add(dx), p.y);
        set(&mut p, x, y);
        if covers_x(&p) {
            let (x, y) = (avoid.x.wrapping_sub(p.w), p.y);
            set(&mut p, x, y);
        }
    } else if p.x < bounds.x {
        let dx = bounds.x.wrapping_sub(p.x);
        let (x, y) = (p.x.wrapping_add(dx), p.y);
        set(&mut p, x, y);
        if covers_x(&p) {
            let (x, y) = (right(&avoid), p.y);
            set(&mut p, x, y);
        }
    }
    if bottom(&p) > bottom(&bounds) {
        let dy = bottom(&bounds).wrapping_sub(bottom(&p));
        let (x, y) = (p.x, p.y.wrapping_add(dy));
        set(&mut p, x, y);
        if covers_y(&p) {
            let (x, y) = (p.x, avoid.y.wrapping_sub(p.h));
            set(&mut p, x, y);
        }
    } else if p.y < bounds.y {
        let dy = bounds.y.wrapping_sub(p.y);
        let (x, y) = (p.x, p.y.wrapping_add(dy));
        set(&mut p, x, y);
        if covers_y(&p) {
            let (x, y) = (p.x, bottom(&avoid));
            set(&mut p, x, y);
        }
    }
    moves
}

/// `DoPopup`: whether the popup joins the open ones (flagged additional while some are up).
pub fn popup_additional(flags: i32, open: bool) -> bool {
    flags & POPUP_ADDITIONAL != 0 && open
}

/// `DoPopup`: the popup's place (-1: the parent's mouse).
pub fn popup_place(x: i32, y: i32, mouse_x: i32, mouse_y: i32) -> (i32, i32) {
    (if x == -1 { mouse_x } else { x }, if y == -1 { mouse_y } else { y })
}

/// `DoPopup`: whether the popup gets its own window (the flags win over the manager's default).
pub fn popup_own_window(flags: i32, default: bool) -> bool {
    if flags & POPUP_IN_OWN_WINDOW != 0 {
        true
    } else if flags & POPUP_NOT_IN_OWN_WINDOW != 0 {
        false
    } else {
        default
    }
}

/// `FindMouseWindow`'s test: the point is inside the rectangle (right and bottom edges out).
pub fn rect_contains(r: EeWonRect4, x: i32, y: i32) -> bool {
    x >= r.x && x < right(&r) && y >= r.y && y < bottom(&r)
}

/// `FindMouseWindow`: the topmost of the popups' windows (`rects`, oldest first) under the point.
pub fn popup_window_at(rects: &[EeWonRect4], x: i32, y: i32) -> Option<usize> {
    (0..rects.len()).rev().find(|&i| rect_contains(rects[i], x, y))
}

/// `CheckPopupClick`: whether a press of `button` ends the popups (the flags' low bits name the buttons).
pub fn popup_click_ends(flags: i32, button: i32) -> bool {
    // the original shifted an int: only 0..31 mean anything
    (0..32).contains(&button) && flags & (1i32 << button) != 0
}

/// What `CalcModality` decides (bits): 1 the windows go modal, 2 the dialog is marked modal itself (no modal
/// popup does it).
pub fn modality(popups_open: bool, flags: i32, has_dialog: bool) -> u32 {
    let mut modal = popups_open && flags & POPUP_IS_MODAL != 0;
    let mut bits = 0;
    if has_dialog {
        if !modal {
            bits |= 2;
        }
        modal = true;
    }
    if modal {
        bits |= 1;
    }
    bits
}

/// `CalcModality`'s per-window state: the dialog's own window stays free while no popup is up and the dialog is
/// its root; every other window takes `modal`.
pub fn window_modal(is_dialog_window: bool, popup_up: bool, dialog_is_root: bool, modal: bool) -> bool {
    if is_dialog_window && !popup_up && dialog_is_root {
        false
    } else {
        modal
    }
}

/// `EndDialog`: whether the call names the current dialog (no window or dialog named, or the current ones).
pub fn ends_dialog(window_given: bool, window_is_modal: bool, dialog_given: bool, dialog_is_modal: bool) -> bool {
    !(window_given && !window_is_modal) && !(dialog_given && !dialog_is_modal)
}

/// `PopupMenu::TimerEvent`: (the timer's new total, check the submenu now, keep the timer). In keyboard mode the
/// timer stops untouched.
pub fn menu_timer(keyboard: bool, acc: i32, delta: i32) -> (i32, bool, bool) {
    if keyboard {
        return (acc, false, false);
    }
    let a = acc.wrapping_add(delta);
    if a >= 350 {
        (a, true, false)
    } else {
        (a, false, true)
    }
}

/// `PopupMenu::MouseUp`: (the release count, whether the release goes to the list): the first left release
/// after the menu opened is swallowed.
pub fn menu_mouse_up(button: i32, count: i32) -> (i32, bool) {
    if button == 0 {
        let c = count.wrapping_add(1);
        return (c, c >= 2);
    }
    (count, true)
}

/// `PopupMenu::MouseMove`: in keyboard mode, whether the mouse really moved (which ends the mode); `None`: it did
/// not, the move is ignored.
pub fn menu_mouse_move(keyboard: bool, x: i32, y: i32, last_x: i32, last_y: i32) -> Option<bool> {
    if keyboard && x == last_x && y == last_y {
        return None;
    }
    Some(keyboard)
}

/// `MenuItem::CalcDimensions`: the text's size grown for the icon and the submenu arrow.
pub fn menu_item_size(width: i32, height: i32) -> (i32, i32) {
    (width.wrapping_add(32), height.wrapping_add(4))
}

/// `CheckSubMenu`: the submenu's place in the menu's scroll area (the item's top, 2 in from the right edge) and,
/// from its root place, the rectangle it must not cover (the menu's width, none tall).
pub fn submenu_place(view_width: i32, item_top: i32) -> (i32, i32) {
    (view_width.wrapping_sub(2), item_top)
}

pub fn submenu_avoid(root_x: i32, view_width: i32) -> EeWonRect4 {
    EeWonRect4 { x: root_x.wrapping_sub(view_width).wrapping_add(2), y: 0, w: view_width.wrapping_sub(2), h: 0 }
}

// ---- the C ABI

/// `popup_adjust`: the `SetPos` calls into `out` (room for 4), their count.
///
/// # Safety
/// `out` has room for 4 pairs (8 ints).
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_popup_adjust(
    popup: EeWonRect4,
    bounds: EeWonRect4,
    avoid: EeWonRect4,
    out: *mut i32,
) -> usize {
    let m = popup_adjust(popup, bounds, avoid);
    for (i, (x, y)) in m.iter().enumerate() {
        // SAFETY: room for 4 pairs (the contract; at most 4 moves)
        unsafe {
            *out.add(2 * i) = *x;
            *out.add(2 * i + 1) = *y;
        }
    }
    m.len()
}

#[no_mangle]
pub extern "C" fn ee_won_gui_popup_additional(flags: i32, open: bool) -> bool {
    popup_additional(flags, open)
}

/// `popup_place`: the x, the y into `out_y`.
///
/// # Safety
/// `out_y` is writable.
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_popup_place(x: i32, y: i32, mouse_x: i32, mouse_y: i32, out_y: *mut i32) -> i32 {
    let (a, b) = popup_place(x, y, mouse_x, mouse_y);
    // SAFETY: writable (the contract)
    unsafe { *out_y = b };
    a
}

#[no_mangle]
pub extern "C" fn ee_won_gui_popup_own_window(flags: i32, default: bool) -> bool {
    popup_own_window(flags, default)
}

#[no_mangle]
pub extern "C" fn ee_won_gui_rect_contains(r: EeWonRect4, x: i32, y: i32) -> bool {
    rect_contains(r, x, y)
}

/// `popup_window_at` over `count` rectangles: the index, or -1.
///
/// # Safety
/// `rects` holds `count` rectangles (or is NULL with 0).
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_popup_window_at(rects: *const EeWonRect4, count: usize, x: i32, y: i32) -> i64 {
    let r: &[EeWonRect4] = if count == 0 || rects.is_null() {
        &[]
    } else {
        // SAFETY: `count` rectangles (the contract)
        unsafe { std::slice::from_raw_parts(rects, count) }
    };
    popup_window_at(r, x, y).map_or(-1, |i| i as i64)
}

#[no_mangle]
pub extern "C" fn ee_won_gui_popup_click_ends(flags: i32, button: i32) -> bool {
    popup_click_ends(flags, button)
}

#[no_mangle]
pub extern "C" fn ee_won_gui_modality(popups_open: bool, flags: i32, has_dialog: bool) -> u32 {
    modality(popups_open, flags, has_dialog)
}

#[no_mangle]
pub extern "C" fn ee_won_gui_window_modal(
    is_dialog_window: bool,
    popup_up: bool,
    dialog_is_root: bool,
    modal: bool,
) -> bool {
    window_modal(is_dialog_window, popup_up, dialog_is_root, modal)
}

#[no_mangle]
pub extern "C" fn ee_won_gui_ends_dialog(
    window_given: bool,
    window_is_modal: bool,
    dialog_given: bool,
    dialog_is_modal: bool,
) -> bool {
    ends_dialog(window_given, window_is_modal, dialog_given, dialog_is_modal)
}

/// `menu_timer`: the new total into `acc`, the check into `out_check`, whether the timer stays.
///
/// # Safety
/// `acc` and `out_check` are writable.
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_menu_timer(
    keyboard: bool,
    acc: *mut i32,
    delta: i32,
    out_check: *mut bool,
) -> bool {
    // SAFETY: writable (the contract)
    unsafe {
        let (a, c, keep) = menu_timer(keyboard, *acc, delta);
        *acc = a;
        *out_check = c;
        keep
    }
}

/// `menu_mouse_up`: the count in place, whether the release goes on.
///
/// # Safety
/// `count` is writable.
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_menu_mouse_up(button: i32, count: *mut i32) -> bool {
    // SAFETY: writable (the contract)
    unsafe {
        let (c, go) = menu_mouse_up(button, *count);
        *count = c;
        go
    }
}

/// `menu_mouse_move`: -1 ignore the move, 1 leave keyboard mode first, 0 track it.
#[no_mangle]
pub extern "C" fn ee_won_gui_menu_mouse_move(keyboard: bool, x: i32, y: i32, last_x: i32, last_y: i32) -> i32 {
    match menu_mouse_move(keyboard, x, y, last_x, last_y) {
        None => -1,
        Some(true) => 1,
        Some(false) => 0,
    }
}

/// `menu_item_size` in place.
///
/// # Safety
/// `width` and `height` are writable.
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_menu_item_size(width: *mut i32, height: *mut i32) {
    // SAFETY: writable (the contract)
    unsafe {
        let (w, h) = menu_item_size(*width, *height);
        *width = w;
        *height = h;
    }
}

/// `submenu_place`: the x, the y into `out_y`.
///
/// # Safety
/// `out_y` is writable.
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_submenu_place(view_width: i32, item_top: i32, out_y: *mut i32) -> i32 {
    let (x, y) = submenu_place(view_width, item_top);
    // SAFETY: writable (the contract)
    unsafe { *out_y = y };
    x
}

#[no_mangle]
pub extern "C" fn ee_won_gui_submenu_avoid(root_x: i32, view_width: i32) -> EeWonRect4 {
    submenu_avoid(root_x, view_width)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn r(x: i32, y: i32, w: i32, h: i32) -> EeWonRect4 {
        EeWonRect4 { x, y, w, h }
    }

    #[test]
    fn popups() {
        let screen = r(0, 0, 800, 600);
        // fits: no move
        assert_eq!(popup_adjust(r(10, 10, 100, 100), screen, r(0, 0, 0, 0)), vec![]);
        // past the right edge, then flipped left of the combo it opened under
        assert_eq!(popup_adjust(r(750, 10, 100, 50), screen, r(700, 0, 90, 0)), vec![(700, 10), (600, 10)]);
        // past the bottom, flipped above the avoided row
        assert_eq!(popup_adjust(r(10, 580, 100, 50), screen, r(0, 560, 0, 20)), vec![(10, 550), (10, 510)]);
        // left and top of the bounds
        assert_eq!(popup_adjust(r(-20, -5, 10, 10), screen, r(0, 0, 0, 0)), vec![(0, -5), (0, 0)]);
        assert!(popup_additional(0x1000, true) && !popup_additional(0x1000, false) && !popup_additional(0, true));
        assert_eq!(popup_place(-1, 5, 30, 40), (30, 5));
        assert!(
            popup_own_window(0x10000 | 0x8000, false) && !popup_own_window(0x8000, true) && popup_own_window(0, true)
        );
        assert!(rect_contains(r(0, 0, 10, 10), 9, 0) && !rect_contains(r(0, 0, 10, 10), 10, 0));
        assert_eq!(popup_window_at(&[r(0, 0, 50, 50), r(10, 10, 50, 50)], 20, 20), Some(1));
        assert_eq!(popup_window_at(&[r(0, 0, 50, 50), r(10, 10, 50, 50)], 5, 5), Some(0));
        assert_eq!(popup_window_at(&[], 5, 5), None);
        assert!(popup_click_ends(0x5, 2) && !popup_click_ends(0x5, 1) && !popup_click_ends(-1, 40));
    }

    #[test]
    fn modal() {
        assert_eq!(modality(false, 0x100, false), 0);
        assert_eq!(modality(true, 0x100, false), 1);
        assert_eq!(modality(true, 0x100, true), 1);
        assert_eq!(modality(true, 0, true), 3);
        assert!(!window_modal(true, false, true, true));
        assert!(window_modal(true, true, true, true));
        assert!(!window_modal(false, false, false, false));
        assert!(ends_dialog(false, false, false, false) && !ends_dialog(true, false, false, false));
        assert!(ends_dialog(true, true, true, true) && !ends_dialog(true, true, true, false));
    }

    #[test]
    fn menus() {
        assert_eq!(menu_timer(true, 100, 50), (100, false, false));
        assert_eq!(menu_timer(false, 300, 49), (349, false, true));
        assert_eq!(menu_timer(false, 300, 50), (350, true, false));
        assert_eq!(menu_mouse_up(0, 0), (1, false));
        assert_eq!(menu_mouse_up(0, 1), (2, true));
        assert_eq!(menu_mouse_up(1, 0), (0, true));
        assert_eq!(menu_mouse_move(true, 1, 2, 1, 2), None);
        assert_eq!(menu_mouse_move(true, 1, 3, 1, 2), Some(true));
        assert_eq!(menu_mouse_move(false, 1, 2, 1, 2), Some(false));
        assert_eq!(menu_item_size(40, 14), (72, 18));
        assert_eq!(submenu_place(100, 36), (98, 36));
        assert_eq!(submenu_avoid(300, 100), r(202, 0, 98, 0));
    }
}
