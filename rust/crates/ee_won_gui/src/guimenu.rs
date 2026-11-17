//! WONGUI's menus' keys and choices (MenuBehavior.cpp's PopupMenu, MenuBar.cpp). The C++ keeps the menus, their
//! items and submenus, the popups' windows and events, the recursion of `TryKeyDown` into the open submenu and the
//! paint; these answer:
//!
//! - a popup menu's key after its open submenu declined it (`TryKeyDown`): Left and Escape close the submenu,
//!   Right selects the submenu's first item when it has no selection; without a submenu Right opens the selected
//!   item's submenu, Up and Down move the selection (the list's keys, `guilist`);
//! - when the selection opens, keeps or closes a submenu (`CheckSubMenu`), the submenu's selection kept on its
//!   parent item (`HandleComponentEvent`), cleared when the mouse moves to another item (`ListSelChangedHook`);
//! - the menu bar: a key's state, item and step (`KeyDown`: Escape, Left and Right wrapping round the items,
//!   Return or Down opening the menu), which item the mouse picks and whether its menu opens (`CheckItem`), its
//!   menu's place, flags and avoided rectangle, a press (`MouseDown`), the events (`HandleComponentEvent`: a
//!   menu's end, a chosen item), the moves that change nothing (`MouseMove`), a new item's place (`AddItem`) and
//!   size (`MenuBarItem::SetText`).
//!
//! The original has no mnemonics: a letter key does nothing in a menu. Ints wrap (the game builds with -fwrapv).

const VK_RETURN: i32 = 0x0d;
const VK_ESCAPE: i32 = 0x1b;
const VK_LEFT: i32 = 0x25;
const VK_UP: i32 = 0x26;
const VK_RIGHT: i32 = 0x27;
const VK_DOWN: i32 = 0x28;

/// `MenuBarState`.
const BAR_NONE: i32 = 0;
const BAR_UP: i32 = 1;
const BAR_DOWN: i32 = 2;

/// WONLobbyGUI.h's event numbers.
const EVENT_LIST_SEL_CHANGED: i32 = 22;
const EVENT_LIST_ITEM_UP_CLICKED: i32 = 24;
const EVENT_END_POPUP: i32 = 30;

/// `PopupFlag_StandardPopup` without `WantInput` and `EndOnEscape`: the menu bar's menus.
const BAR_POPUP_FLAGS: i32 = 0x23e7 & !0x80 & !0x20;
/// A submenu's popup flags (`CheckSubMenu`).
const SUBMENU_POPUP_FLAGS: i32 = 0x33c7;

/// `PopupMenu::TryKeyDown` after the open submenu (if any) declined the key: 0 declined, 1 close the submenu
/// (used), 2 select the submenu's first item (used), 3 check the selection's submenu (used if one opened: then its
/// first item is selected), 4 the list's key (used).
pub fn popup_key(has_sub: bool, key: i32, sub_has_sel: bool) -> u8 {
    if has_sub {
        if key == VK_LEFT || key == VK_ESCAPE {
            1
        } else if key == VK_RIGHT && !sub_has_sel {
            2
        } else {
            0
        }
    } else if key == VK_RIGHT {
        3
    } else if key == VK_UP || key == VK_DOWN {
        4
    } else {
        0
    }
}

/// `CheckSubMenu`: 0 nothing (the selection's submenu is the open one), 1 close the open submenu (the selection is
/// no menu item), 2 close the open one and open the selection's (it has an enabled submenu), 3 close the open one
/// only.
pub fn check_sub(is_item: bool, has_cur_sub: bool, same_sub: bool, item_has_sub: bool, disabled: bool) -> u8 {
    if !is_item {
        return if has_cur_sub { 1 } else { 0 };
    }
    if same_sub {
        0
    } else if item_has_sub && !disabled {
        2
    } else {
        3
    }
}

/// `PopupMenu::HandleComponentEvent`: 0 the scroll area's, 1 the submenu's selection changed (its parent item is
/// selected again when the submenu has a selection).
pub fn popup_event(from_sub: bool, event_type: i32) -> u8 {
    u8::from(from_sub && event_type == EVENT_LIST_SEL_CHANGED)
}

/// `ListSelChangedHook`: whether the open submenu's selection is cleared (the mouse chose another item).
pub fn sel_clears_sub(keyboard: bool, has_sub: bool, sel_is_sub_item: bool) -> bool {
    !keyboard && has_sub && !sel_is_sub_item
}

/// `MenuBar::KeyDown`'s first step: whether the open menu is asked first (the bar is down), else the bar's state
/// becomes `BAR_UP`.
pub fn bar_key_asks_popup(state: i32) -> bool {
    state == BAR_DOWN
}

/// `MenuBar::KeyDown`'s step after the menu declined the key (`state` as the first step left it, `id` the current
/// item's, `count` the items): the state, the item to check and 1 Escape (the menu is closed and the current item
/// checked first), 2 the current item is dropped (Return or Down opened the bar).
pub fn bar_key(state: i32, key: i32, id: i32, count: usize) -> (i32, i32, u8) {
    let (mut state, mut id, mut step) = (state, id, 0);
    if key == VK_ESCAPE {
        state = BAR_UP;
        step = 1;
    } else if key == VK_RIGHT {
        id = id.wrapping_add(1);
        if id as isize as usize >= count {
            id = 0;
        }
    } else if key == VK_LEFT {
        id = id.wrapping_sub(1);
        if id < 0 {
            id = count.wrapping_sub(1) as i32;
        }
    } else if state == BAR_UP && (key == VK_RETURN || key == VK_DOWN) {
        state = BAR_DOWN;
        step = 2;
    }
    (state, id, step)
}

/// `CheckItem`: bits 1 the current item goes back to none (another or no component), 2 the component becomes the
/// current item (shown up; the bar is not down), 4 the component (a bar item) becomes the current item with its
/// menu open (the bar is down: the old one's state reset and its menu closed).
pub fn bar_check(state: i32, has_cur: bool, has_check: bool, check_is_cur: bool, check_is_item: bool) -> u8 {
    if state == BAR_NONE || state == BAR_UP {
        let mut r = 0;
        if has_cur && !check_is_cur {
            r |= 1;
        }
        if has_check {
            r |= 2;
        }
        r
    } else if state == BAR_DOWN && check_is_item && !check_is_cur {
        4
    } else {
        0
    }
}

/// `CheckItem`'s menu place in the bar's coordinates: under the item.
pub fn bar_popup_place(left: i32, top: i32, height: i32) -> (i32, i32) {
    (left, top.wrapping_add(height))
}

/// `CheckItem`'s avoided rectangle (x, y, width, height) from the menu's root y: the item's row.
pub fn bar_popup_avoid(root_y: i32, height: i32) -> [i32; 4] {
    [0, root_y.wrapping_sub(height), 0, height]
}

/// `MenuBar::HandleComponentEvent`: 0 the container's, 1 a menu's end while the bar is down, 2 an item clicked.
pub fn bar_event(event_type: i32, state: i32) -> u8 {
    if event_type == EVENT_END_POPUP && state == BAR_DOWN {
        1
    } else if event_type == EVENT_LIST_ITEM_UP_CLICKED {
        2
    } else {
        0
    }
}

/// A clicked item that is chosen (a menu item without a submenu, enabled).
pub fn item_chosen(is_item: bool, has_sub: bool, disabled: bool) -> bool {
    is_item && !has_sub && !disabled
}

/// `MenuBar::MouseDown` on a bar item with the left button: the new state and whether the bar opens (the current
/// item dropped and a release queued), else it closes (the current item's menu closed).
pub fn bar_press(state: i32) -> (i32, bool) {
    if state == BAR_DOWN {
        (BAR_UP, false)
    } else {
        (BAR_DOWN, true)
    }
}

/// `MenuBar::MouseMove`: whether the point moved (the item under it is checked).
pub fn bar_moved(x: i32, y: i32, last_x: i32, last_y: i32) -> bool {
    !(x == last_x && y == last_y)
}

/// `AddItem`: a new item's place, right of the last one (its left, top and width) or at (2, 2) for the first.
pub fn bar_item_place(prev: Option<(i32, i32, i32)>) -> (i32, i32) {
    match prev {
        Some((left, top, width)) => (left.wrapping_add(width).wrapping_add(1), top),
        None => (2, 2),
    }
}

/// `MenuBarItem::SetText`: the item's size from its text's width and the font's height.
pub fn bar_item_size(text_width: i32, font_height: i32) -> (i32, i32) {
    (text_width.wrapping_add(10), font_height.wrapping_add(4))
}

// ---- the C ABI

#[no_mangle]
pub extern "C" fn ee_won_gui_menu_popup_key(has_sub: bool, key: i32, sub_has_sel: bool) -> u8 {
    popup_key(has_sub, key, sub_has_sel)
}

#[no_mangle]
pub extern "C" fn ee_won_gui_menu_check_sub(
    is_item: bool,
    has_cur_sub: bool,
    same_sub: bool,
    item_has_sub: bool,
    disabled: bool,
) -> u8 {
    check_sub(is_item, has_cur_sub, same_sub, item_has_sub, disabled)
}

#[no_mangle]
pub extern "C" fn ee_won_gui_menu_submenu_flags() -> i32 {
    SUBMENU_POPUP_FLAGS
}

#[no_mangle]
pub extern "C" fn ee_won_gui_menu_popup_event(from_sub: bool, event_type: i32) -> u8 {
    popup_event(from_sub, event_type)
}

#[no_mangle]
pub extern "C" fn ee_won_gui_menu_sel_clears_sub(keyboard: bool, has_sub: bool, sel_is_sub_item: bool) -> bool {
    sel_clears_sub(keyboard, has_sub, sel_is_sub_item)
}

#[no_mangle]
pub extern "C" fn ee_won_gui_menubar_key_asks_popup(state: i32) -> bool {
    bar_key_asks_popup(state)
}

/// `bar_key`: the step; the state and the item into the outs.
///
/// # Safety
/// The outs are writable.
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_menubar_key(
    state: i32,
    key: i32,
    id: i32,
    count: usize,
    out_state: *mut i32,
    out_id: *mut i32,
) -> u8 {
    let (s, i, step) = bar_key(state, key, id, count);
    // SAFETY: writable (the contract)
    unsafe {
        *out_state = s;
        *out_id = i;
    }
    step
}

#[no_mangle]
pub extern "C" fn ee_won_gui_menubar_check(
    state: i32,
    has_cur: bool,
    has_check: bool,
    check_is_cur: bool,
    check_is_item: bool,
) -> u8 {
    bar_check(state, has_cur, has_check, check_is_cur, check_is_item)
}

#[no_mangle]
pub extern "C" fn ee_won_gui_menubar_popup_flags() -> i32 {
    BAR_POPUP_FLAGS
}

/// `bar_popup_place`: x, y into `out_y`.
///
/// # Safety
/// `out_y` is writable.
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_menubar_popup_place(left: i32, top: i32, height: i32, out_y: *mut i32) -> i32 {
    let (x, y) = bar_popup_place(left, top, height);
    // SAFETY: writable (the contract)
    unsafe { *out_y = y };
    x
}

/// `bar_popup_avoid` into `out` (x, y, width, height).
///
/// # Safety
/// `out` has room for 4.
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_menubar_popup_avoid(root_y: i32, height: i32, out: *mut i32) {
    let r = bar_popup_avoid(root_y, height);
    // SAFETY: room for 4 (the contract)
    unsafe { std::ptr::copy_nonoverlapping(r.as_ptr(), out, 4) };
}

#[no_mangle]
pub extern "C" fn ee_won_gui_menubar_event(event_type: i32, state: i32) -> u8 {
    bar_event(event_type, state)
}

#[no_mangle]
pub extern "C" fn ee_won_gui_menu_item_chosen(is_item: bool, has_sub: bool, disabled: bool) -> bool {
    item_chosen(is_item, has_sub, disabled)
}

/// `bar_press`: whether the bar opens; the state into `out_state`.
///
/// # Safety
/// `out_state` is writable.
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_menubar_press(state: i32, out_state: *mut i32) -> bool {
    let (s, opens) = bar_press(state);
    // SAFETY: writable (the contract)
    unsafe { *out_state = s };
    opens
}

#[no_mangle]
pub extern "C" fn ee_won_gui_menubar_moved(x: i32, y: i32, last_x: i32, last_y: i32) -> bool {
    bar_moved(x, y, last_x, last_y)
}

/// `bar_item_place`: x, y into `out_y` (`has_prev` false: the first item).
///
/// # Safety
/// `out_y` is writable.
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_menubar_item_place(
    has_prev: bool,
    left: i32,
    top: i32,
    width: i32,
    out_y: *mut i32,
) -> i32 {
    let (x, y) = bar_item_place(has_prev.then_some((left, top, width)));
    // SAFETY: writable (the contract)
    unsafe { *out_y = y };
    x
}

/// `bar_item_size`: width, height into `out_h`.
///
/// # Safety
/// `out_h` is writable.
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_menubar_item_size(text_width: i32, font_height: i32, out_h: *mut i32) -> i32 {
    let (w, h) = bar_item_size(text_width, font_height);
    // SAFETY: writable (the contract)
    unsafe { *out_h = h };
    w
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn popup_keys() {
        assert_eq!(popup_key(true, VK_LEFT, true), 1);
        assert_eq!(popup_key(true, VK_ESCAPE, false), 1);
        assert_eq!(popup_key(true, VK_RIGHT, false), 2);
        assert_eq!(popup_key(true, VK_RIGHT, true), 0);
        assert_eq!(popup_key(true, VK_DOWN, false), 0);
        assert_eq!(popup_key(false, VK_RIGHT, false), 3);
        assert_eq!(popup_key(false, VK_UP, false), 4);
        assert_eq!(popup_key(false, VK_DOWN, true), 4);
        assert_eq!(popup_key(false, VK_LEFT, false), 0);
        assert_eq!(popup_key(false, i32::from(b'A'), false), 0);
    }

    #[test]
    fn submenus() {
        assert_eq!(check_sub(false, true, false, false, false), 1);
        assert_eq!(check_sub(false, false, false, false, false), 0);
        assert_eq!(check_sub(true, true, true, true, false), 0);
        assert_eq!(check_sub(true, true, false, true, false), 2);
        assert_eq!(check_sub(true, true, false, true, true), 3);
        assert_eq!(check_sub(true, false, false, false, false), 3);
        assert_eq!(popup_event(true, EVENT_LIST_SEL_CHANGED), 1);
        assert_eq!(popup_event(false, EVENT_LIST_SEL_CHANGED), 0);
        assert!(sel_clears_sub(false, true, false));
        assert!(!sel_clears_sub(true, true, false) && !sel_clears_sub(false, true, true));
        assert_eq!(SUBMENU_POPUP_FLAGS, 0x33c7);
        assert_eq!(BAR_POPUP_FLAGS, 0x2347);
    }

    #[test]
    fn bar() {
        assert!(bar_key_asks_popup(BAR_DOWN) && !bar_key_asks_popup(BAR_UP));
        assert_eq!(bar_key(BAR_UP, VK_RIGHT, 2, 3), (BAR_UP, 0, 0));
        assert_eq!(bar_key(BAR_UP, VK_LEFT, 0, 3), (BAR_UP, 2, 0));
        assert_eq!(bar_key(BAR_DOWN, VK_ESCAPE, 1, 3), (BAR_UP, 1, 1));
        assert_eq!(bar_key(BAR_UP, VK_DOWN, 1, 3), (BAR_DOWN, 1, 2));
        assert_eq!(bar_key(BAR_DOWN, VK_RETURN, 1, 3), (BAR_DOWN, 1, 0));
        assert_eq!(bar_key(BAR_UP, VK_LEFT, 0, 0), (BAR_UP, -1, 0));
        assert_eq!(bar_check(BAR_UP, true, true, false, true), 3);
        assert_eq!(bar_check(BAR_NONE, true, false, false, false), 1);
        assert_eq!(bar_check(BAR_DOWN, true, true, false, true), 4);
        assert_eq!(bar_check(BAR_DOWN, true, true, true, true), 0);
        assert_eq!(bar_check(3, true, true, false, true), 0);
        assert_eq!(bar_popup_place(5, 2, 18), (5, 20));
        assert_eq!(bar_popup_avoid(40, 18), [0, 22, 0, 18]);
        assert_eq!(bar_event(EVENT_END_POPUP, BAR_DOWN), 1);
        assert_eq!(bar_event(EVENT_END_POPUP, BAR_UP), 0);
        assert_eq!(bar_event(EVENT_LIST_ITEM_UP_CLICKED, BAR_UP), 2);
        assert!(
            item_chosen(true, false, false) && !item_chosen(true, true, false) && !item_chosen(false, false, false)
        );
        assert_eq!(bar_press(BAR_DOWN), (BAR_UP, false));
        assert_eq!(bar_press(BAR_NONE), (BAR_DOWN, true));
        assert!(!bar_moved(1, 2, 1, 2) && bar_moved(1, 3, 1, 2));
        assert_eq!(bar_item_place(None), (2, 2));
        assert_eq!(bar_item_place(Some((2, 2, 40))), (43, 2));
        assert_eq!(bar_item_size(30, 12), (40, 16));
    }
}
