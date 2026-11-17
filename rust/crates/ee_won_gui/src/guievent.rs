//! WONGUI's event dispatch decisions (Window.cpp, WindowManager.cpp's `DeliverEvent`, `ProcessEvents` and
//! `CheckPopupClick`, RootContainer.cpp, Container.cpp's mouse routing). The C++ keeps the windows, the
//! components, the event queue and every call (the root's handlers, focus, capture, popups' ends, cursors); these
//! decide which call happens:
//!
//! - a window's mouse (`HandleMouseEvent`): when the mouse enters, a move's end of double-click detection (the
//!   original's `a && b || c`), drag or move, a release outside ending the drag with an exit; the double click
//!   (`HandleMouseDown`: the same button again within 250 ms, tick 0 kept for "none"); the size, focus and exit
//!   events (`HandleInputEvent`); the resize's delay (`CheckResize`), the cursor's forced refresh once a second
//!   (`IdleUpdate`), the hover after 100 ms still (`TimerEvent`), the mouse's place after an exit
//!   (`HandleMouseExit`), centring (`Center`), the cursor's refresh (`CheckSetCursor`), closing (`Close`);
//! - the window manager: the 50 ms timer step and its 1 s cap, the popups' end or keep on a focus change, the
//!   resolution's restore after a focus loss (`ProcessEvents`); a press's focus and capture, a release's capture,
//!   the moves merged, a popup's end by a move or a key, the wheel and keys sent to the popup, the size changes
//!   merged, a closing window's popup (`DeliverEvent`); which click ends a popup (`CheckPopupClick`'s walk);
//! - the root container: the window drag (start, place, end), the cursor component's place (`MouseMove`,
//!   `MouseDrag`, `SetCursor`);
//! - a container: the child under the mouse's exit and enter (`UpdateMouseChild`), the release only to the child
//!   that had the press (`MouseUp`), the desired size over the children (`GetDesiredSize`), the insert place under
//!   the always-on-top children (`FindTop`).
//!
//! Ticks are the 32-bit `WONTickCount` and wrap. Ints wrap (the game builds with -fwrapv).

/// WONLobbyGUI.h's event numbers.
const EVENT_MOUSE_DOUBLE_CLICK: i32 = 4;
const EVENT_MOUSE_DOWN_CHECK_DOUBLE: i32 = 5;
const EVENT_KEY_DOWN: i32 = 8;
/// `WindowState_Open`.
const WINDOW_OPEN: i32 = 0;
/// The popup flags these read.
const POPUP_END_ON_MOUSE_MOVE: i32 = 0x0008;
const POPUP_END_ON_KEY_DOWN: i32 = 0x0010;
const POPUP_END_ON_ESCAPE: i32 = 0x0020;
const POPUP_END_ON_FOCUS_CHANGE: i32 = 0x0040;
const POPUP_WANT_INPUT: i32 = 0x0080;
const POPUP_DONT_WANT_MOUSE: i32 = 0x4000;
/// A component that keeps the popup up when clicked (`mComponentBits`).
const KEEPS_POPUP_BIT: u32 = 0x10000;
const VK_ESCAPE: i32 = 0x1b;

// ---- Window

/// `HandleMouseEvent`: whether the event makes the mouse enter (it was outside and the left button is up).
pub fn mouse_enters(in_client: bool, left_down: bool) -> bool {
    !in_client && !left_down
}

/// `HandleMouseEvent`'s move: whether it ends the double-click detection, as the original's
/// `detect && x != dx || y != dy` binds (any change of y ends it, detecting or not).
pub fn move_ends_double(detecting: bool, x: i32, y: i32, dx: i32, dy: i32) -> bool {
    (detecting && x != dx) || y != dy
}

/// `HandleMouseEvent`'s release: whether a left release outside the window ends the drag with an exit (and the
/// root gets no release).
pub fn release_exits(button: i32, in_client: bool) -> bool {
    button == 0 && !in_client
}

/// `HandleMouseDown`'s double click: whether it is one, whether detection starts now (every button's tick
/// cleared, the place kept), and the button's new tick (none: unchanged).
#[derive(Debug, PartialEq, Eq)]
pub struct DoubleClick {
    pub double: bool,
    pub start: bool,
    pub tick: Option<u32>,
}

pub fn double_click(event_type: i32, detecting: bool, prev_tick: u32, now: u32) -> DoubleClick {
    if event_type == EVENT_MOUSE_DOUBLE_CLICK {
        return DoubleClick { double: true, start: false, tick: None };
    }
    if event_type != EVENT_MOUSE_DOWN_CHECK_DOUBLE {
        return DoubleClick { double: false, start: false, tick: None };
    }
    let start = !detecting;
    let prev = if start { 0 } else { prev_tick };
    let now = if now == 0 { 1 } else { now };
    if prev != 0 && now.wrapping_sub(prev) < 250 {
        DoubleClick { double: true, start, tick: Some(0) }
    } else {
        DoubleClick { double: false, start, tick: Some(now) }
    }
}

/// `HandleInputEvent`'s size change: whether it asks for a resize (both sizes given, one differs from the root's).
pub fn size_resizes(width: i32, height: i32, root_width: i32, root_height: i32) -> bool {
    width != -1 && height != -1 && (root_width != width || root_height != height)
}

/// `CheckResize`: whether the pending resize is applied now (past the delay, not suspended).
pub fn resize_due(need: bool, tick: u32, last: u32, delay: u32, suspended: i32) -> bool {
    need && tick.wrapping_sub(last) > delay && suspended <= 0
}

/// `IdleUpdate`: whether the cursor refresh is forced (inside the window, a second since the last forced one).
pub fn cursor_forced(in_client: bool, tick: u32, last: u32) -> bool {
    in_client && tick.wrapping_sub(last) >= 1000
}

/// `TimerEvent`: the hover's elapsed time when the mouse is inside, has moved, no button is down and it has been
/// still 100 ms or more.
pub fn hover_elapsed(in_client: bool, last_move: u32, downs: [bool; 3], now: u32) -> Option<u32> {
    if in_client && last_move != 0 && !downs[0] && !downs[1] && !downs[2] {
        let e = now.wrapping_sub(last_move);
        if e >= 100 {
            return Some(e);
        }
    }
    None
}

/// `HandleMouseExit`: the mouse's place, just past the root's bottom right corner.
pub fn exit_point(left: i32, top: i32, width: i32, height: i32) -> (i32, i32) {
    (left.wrapping_add(width).wrapping_add(1), top.wrapping_add(height).wrapping_add(1))
}

/// `Center`: whether it centres on the parent window (asked for, there is one, it is visible).
pub fn center_on_parent(in_parent: bool, has_parent: bool, parent_visible: bool) -> bool {
    in_parent && has_parent && parent_visible
}

/// `Center`: the window's top left in the screen (or parent) rectangle, clamped to 0.
pub fn center(screen: [i32; 4], width: i32, height: i32) -> (i32, i32) {
    let mut left = screen[0].wrapping_add(screen[2].wrapping_sub(width) / 2);
    let mut top = screen[1].wrapping_add(screen[3].wrapping_sub(height) / 2);
    if left < 0 {
        left = 0;
    }
    if top < 0 {
        top = 0;
    }
    (left, top)
}

/// `CheckSetCursor`: whether the cursor is set again (forced, or the root's changed).
pub fn cursor_refresh(force: bool, same: bool) -> bool {
    force || !same
}

/// `Close`: 0 nothing to do (not open: true), 1 ask the window and its children.
pub fn close_step(state: i32) -> u8 {
    u8::from(state == WINDOW_OPEN)
}

// ---- WindowManager

/// `ProcessEvents`: the timer step's elapsed time (at least 50 ms since the last, capped at 1 s), none before.
pub fn timer_step(tick: u32, last: u32) -> Option<u32> {
    let e = tick.wrapping_sub(last);
    if e >= 50 {
        Some(e.min(1000))
    } else {
        None
    }
}

/// `ProcessEvents`' popups on a focus change: 0 keep (the end pending cleared), 1 the focus went to a popup
/// window (the owner is focused again, the end cleared), 2 end the popups, 3 the end pending (ended next step).
pub fn popup_focus_step(
    popup_up: bool,
    focus_is_owner: bool,
    flags: i32,
    has_focus: bool,
    focus_is_popup: bool,
    end_pending: bool,
) -> u8 {
    if popup_up && !focus_is_owner && flags & POPUP_END_ON_FOCUS_CHANGE != 0 {
        if has_focus && focus_is_popup {
            1
        } else if end_pending {
            2
        } else {
            3
        }
    } else {
        0
    }
}

/// `ProcessEvents`: whether the screen resolution is restored (focus lost more than 50 ms ago, no window
/// focused; the caller checks full screen).
pub fn restore_due(last_lose: u32, has_focus: bool, tick: u32) -> bool {
    last_lose != 0 && !has_focus && tick.wrapping_sub(last_lose) > 50
}

/// `DeliverEvent`'s press: whether the window is focused (not already, not a popup, not blocked by a modal one).
pub fn press_focuses(focus_is_window: bool, is_popup: bool, not_modal: bool) -> bool {
    !focus_is_window && !is_popup && !not_modal
}

/// `DeliverEvent`'s press and release: whether the mouse capture is set or released (the left button, unless a
/// popup is up with capture support).
pub fn capture_changes(button: i32, popup_up: bool, capture_support: bool) -> bool {
    button == 0 && (!popup_up || !capture_support)
}

/// `DeliverEvent`'s move: whether it ends the popups.
pub fn move_ends_popup(popup_up: bool, flags: i32) -> bool {
    popup_up && flags & POPUP_END_ON_MOUSE_MOVE != 0
}

/// `DeliverEvent`'s wheel: whether it goes to the topmost popup instead of the window.
pub fn wheel_to_popup(popup_up: bool, flags: i32) -> bool {
    popup_up && flags & POPUP_DONT_WANT_MOUSE == 0
}

/// `DeliverEvent`'s keys while a popup is up: 1 the key down ends the popups (and goes nowhere), 2 the key goes
/// to the topmost popup's window, 0 to its own window.
pub fn popup_key(popup_up: bool, event_type: i32, flags: i32, key: i32) -> u8 {
    if !popup_up {
        return 0;
    }
    if event_type == EVENT_KEY_DOWN
        && (flags & POPUP_END_ON_KEY_DOWN != 0 || (flags & POPUP_END_ON_ESCAPE != 0 && key == VK_ESCAPE))
    {
        return 1;
    }
    if flags & POPUP_WANT_INPUT != 0 {
        2
    } else {
        0
    }
}

/// `DeliverEvent`'s size changes merged into the next one for the same window: its unset (-1) fields from this
/// one (x, y, width, height).
pub fn size_merge(this: [i32; 4], next: [i32; 4]) -> [i32; 4] {
    let mut n = next;
    for i in 0..4 {
        if n[i] == -1 {
            n[i] = this[i];
        }
    }
    n
}

/// `DeliverEvent`'s window close: whether it ends the popups (its owner or a popup window closes).
pub fn close_ends_popup(popup_up: bool, is_owner: bool, is_popup: bool) -> bool {
    popup_up && (is_owner || is_popup)
}

/// `CheckPopupClick`'s walk down the clicked children: 0 stop, the popup ends (nothing, or no input component to
/// look into, or not a container), 1 stop, it stays (the input component or a child that keeps it), 2 look into
/// the child.
pub fn popup_click_step(has_child: bool, is_input: bool, bits: u32, has_input: bool, is_container: bool) -> u8 {
    if !has_child {
        0
    } else if is_input || bits & KEEPS_POPUP_BIT != 0 {
        1
    } else if !has_input || !is_container {
        0
    } else {
        2
    }
}

// ---- RootContainer

/// `MouseDown`: whether a press starts dragging the window (the left button, dragging allowed, the child under
/// the mouse drags its parent).
pub fn drag_starts(button: i32, allow: bool, child_drags: bool) -> bool {
    button == 0 && allow && child_drags
}

/// `MouseDrag` while dragging: the root's new place, none when it does not move.
pub fn drag_place(left: i32, top: i32, x: i32, y: i32, drag_x: i32, drag_y: i32) -> Option<(i32, i32)> {
    let nx = left.wrapping_add(x.wrapping_sub(drag_x));
    let ny = top.wrapping_add(y.wrapping_sub(drag_y));
    if nx == left && ny == top {
        None
    } else {
        Some((nx, ny))
    }
}

/// `MouseDrag`: the window's new place (its rectangle's place plus the root's).
pub fn drag_window(rx: i32, ry: i32, nx: i32, ny: i32) -> (i32, i32) {
    (rx.wrapping_add(nx), ry.wrapping_add(ny))
}

/// `MouseUp`: whether a release ends the window drag.
pub fn drag_ends(button: i32) -> bool {
    button == 0
}

/// The cursor component's place: the mouse less the cursor's hot spot.
pub fn cursor_place(x: i32, y: i32, hot_x: i32, hot_y: i32) -> (i32, i32) {
    (x.wrapping_sub(hot_x), y.wrapping_sub(hot_y))
}

// ---- Container

/// `UpdateMouseChild`: bits 1 the old child gets the exit (and the default cursor), 2 the new child gets the enter;
/// none when the child under the mouse did not change.
pub fn mouse_child(same: bool, has_old: bool, has_new: bool) -> u8 {
    if same {
        return 0;
    }
    let mut r = 0;
    if has_old {
        r |= 1;
    }
    if has_new {
        r |= 2;
    }
    r
}

/// `MouseUp`: whether the child under the mouse gets the release (it had the press).
pub fn release_to_child(has_child: bool, same_as_old: bool) -> bool {
    has_child && same_as_old
}

/// `GetDesiredSize`: the largest right and bottom edges of the children (left, top, width, height each); as the
/// original, an edge is compared by `Right()` and kept as left + width (the same sum).
pub fn desired_size(children: &[[i32; 4]]) -> (i32, i32) {
    let (mut right, mut bottom) = (0i32, 0i32);
    for c in children {
        if c[0].wrapping_add(c[2]) > right {
            right = c[0].wrapping_add(c[2]);
        }
        if c[1].wrapping_add(c[3]) > bottom {
            bottom = c[1].wrapping_add(c[3]);
        }
    }
    (right, bottom)
}

/// `FindTop`: the insert place at or below `from` that stays under the always-on-top children (`on_top` front to
/// back).
pub fn find_top(on_top: &[bool], from: usize) -> usize {
    let mut i = from;
    while i > 0 {
        if !on_top[i - 1] {
            return i;
        }
        i -= 1;
    }
    0
}

// ---- the C ABI

unsafe fn slice<'a, T>(p: *const T, n: usize) -> &'a [T] {
    if n == 0 || p.is_null() {
        &[]
    } else {
        // SAFETY: `n` entries (the callers' contract)
        unsafe { std::slice::from_raw_parts(p, n) }
    }
}

#[no_mangle]
pub extern "C" fn ee_won_gui_win_mouse_enters(in_client: bool, left_down: bool) -> bool {
    mouse_enters(in_client, left_down)
}

#[no_mangle]
pub extern "C" fn ee_won_gui_win_move_ends_double(detecting: bool, x: i32, y: i32, dx: i32, dy: i32) -> bool {
    move_ends_double(detecting, x, y, dx, dy)
}

#[no_mangle]
pub extern "C" fn ee_won_gui_win_release_exits(button: i32, in_client: bool) -> bool {
    release_exits(button, in_client)
}

/// `double_click`: whether it is a double click; whether detection starts into `out_start`, whether the button's
/// tick changes into `out_set` and the new tick into `out_tick`.
///
/// # Safety
/// The outs are writable.
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_win_double_click(
    event_type: i32,
    detecting: bool,
    prev_tick: u32,
    now: u32,
    out_start: *mut bool,
    out_set: *mut bool,
    out_tick: *mut u32,
) -> bool {
    let d = double_click(event_type, detecting, prev_tick, now);
    // SAFETY: writable (the contract)
    unsafe {
        *out_start = d.start;
        *out_set = d.tick.is_some();
        *out_tick = d.tick.unwrap_or(prev_tick);
    }
    d.double
}

#[no_mangle]
pub extern "C" fn ee_won_gui_win_size_resizes(width: i32, height: i32, root_width: i32, root_height: i32) -> bool {
    size_resizes(width, height, root_width, root_height)
}

#[no_mangle]
pub extern "C" fn ee_won_gui_win_resize_due(need: bool, tick: u32, last: u32, delay: u32, suspended: i32) -> bool {
    resize_due(need, tick, last, delay, suspended)
}

#[no_mangle]
pub extern "C" fn ee_won_gui_win_cursor_forced(in_client: bool, tick: u32, last: u32) -> bool {
    cursor_forced(in_client, tick, last)
}

/// `hover_elapsed`: whether to hover, the elapsed time into `out`.
///
/// # Safety
/// `out` is writable.
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_win_hover(
    in_client: bool,
    last_move: u32,
    down0: bool,
    down1: bool,
    down2: bool,
    now: u32,
    out: *mut u32,
) -> bool {
    match hover_elapsed(in_client, last_move, [down0, down1, down2], now) {
        Some(e) => {
            // SAFETY: writable (the contract)
            unsafe { *out = e };
            true
        }
        None => false,
    }
}

/// `exit_point`: x, y into `out_y`.
///
/// # Safety
/// `out_y` is writable.
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_win_exit_point(
    left: i32,
    top: i32,
    width: i32,
    height: i32,
    out_y: *mut i32,
) -> i32 {
    let (x, y) = exit_point(left, top, width, height);
    // SAFETY: writable (the contract)
    unsafe { *out_y = y };
    x
}

#[no_mangle]
pub extern "C" fn ee_won_gui_win_center_on_parent(in_parent: bool, has_parent: bool, parent_visible: bool) -> bool {
    center_on_parent(in_parent, has_parent, parent_visible)
}

/// `center`: left, top into `out_top`.
///
/// # Safety
/// `out_top` is writable.
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_win_center(
    sx: i32,
    sy: i32,
    sw: i32,
    sh: i32,
    width: i32,
    height: i32,
    out_top: *mut i32,
) -> i32 {
    let (l, t) = center([sx, sy, sw, sh], width, height);
    // SAFETY: writable (the contract)
    unsafe { *out_top = t };
    l
}

#[no_mangle]
pub extern "C" fn ee_won_gui_win_cursor_refresh(force: bool, same: bool) -> bool {
    cursor_refresh(force, same)
}

#[no_mangle]
pub extern "C" fn ee_won_gui_win_close_step(state: i32) -> u8 {
    close_step(state)
}

/// `timer_step`: whether the step runs, the elapsed time into `out`.
///
/// # Safety
/// `out` is writable.
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_wm_timer_step(tick: u32, last: u32, out: *mut u32) -> bool {
    match timer_step(tick, last) {
        Some(e) => {
            // SAFETY: writable (the contract)
            unsafe { *out = e };
            true
        }
        None => false,
    }
}

#[no_mangle]
pub extern "C" fn ee_won_gui_wm_popup_focus_step(
    popup_up: bool,
    focus_is_owner: bool,
    flags: i32,
    has_focus: bool,
    focus_is_popup: bool,
    end_pending: bool,
) -> u8 {
    popup_focus_step(popup_up, focus_is_owner, flags, has_focus, focus_is_popup, end_pending)
}

#[no_mangle]
pub extern "C" fn ee_won_gui_wm_restore_due(last_lose: u32, has_focus: bool, tick: u32) -> bool {
    restore_due(last_lose, has_focus, tick)
}

#[no_mangle]
pub extern "C" fn ee_won_gui_wm_press_focuses(focus_is_window: bool, is_popup: bool, not_modal: bool) -> bool {
    press_focuses(focus_is_window, is_popup, not_modal)
}

#[no_mangle]
pub extern "C" fn ee_won_gui_wm_capture_changes(button: i32, popup_up: bool, capture_support: bool) -> bool {
    capture_changes(button, popup_up, capture_support)
}

#[no_mangle]
pub extern "C" fn ee_won_gui_wm_move_ends_popup(popup_up: bool, flags: i32) -> bool {
    move_ends_popup(popup_up, flags)
}

#[no_mangle]
pub extern "C" fn ee_won_gui_wm_wheel_to_popup(popup_up: bool, flags: i32) -> bool {
    wheel_to_popup(popup_up, flags)
}

#[no_mangle]
pub extern "C" fn ee_won_gui_wm_popup_key(popup_up: bool, event_type: i32, flags: i32, key: i32) -> u8 {
    popup_key(popup_up, event_type, flags, key)
}

/// `size_merge` in place over `next` (x, y, width, height).
///
/// # Safety
/// `this` and `next` have 4 entries.
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_wm_size_merge(this: *const i32, next: *mut i32) {
    // SAFETY: 4 entries each (the contract)
    unsafe {
        let t: [i32; 4] = std::ptr::read(this as *const [i32; 4]);
        let n: [i32; 4] = std::ptr::read(next as *const [i32; 4]);
        std::ptr::write(next as *mut [i32; 4], size_merge(t, n));
    }
}

#[no_mangle]
pub extern "C" fn ee_won_gui_wm_close_ends_popup(popup_up: bool, is_owner: bool, is_popup: bool) -> bool {
    close_ends_popup(popup_up, is_owner, is_popup)
}

#[no_mangle]
pub extern "C" fn ee_won_gui_wm_popup_click_step(
    has_child: bool,
    is_input: bool,
    bits: u32,
    has_input: bool,
    is_container: bool,
) -> u8 {
    popup_click_step(has_child, is_input, bits, has_input, is_container)
}

#[no_mangle]
pub extern "C" fn ee_won_gui_root_drag_starts(button: i32, allow: bool, child_drags: bool) -> bool {
    drag_starts(button, allow, child_drags)
}

/// `drag_place`: whether the root moves, its place into the outs.
///
/// # Safety
/// The outs are writable.
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_root_drag_place(
    left: i32,
    top: i32,
    x: i32,
    y: i32,
    drag_x: i32,
    drag_y: i32,
    out_x: *mut i32,
    out_y: *mut i32,
) -> bool {
    match drag_place(left, top, x, y, drag_x, drag_y) {
        Some((nx, ny)) => {
            // SAFETY: writable (the contract)
            unsafe {
                *out_x = nx;
                *out_y = ny;
            }
            true
        }
        None => false,
    }
}

/// `drag_window`: x, y into `out_y`.
///
/// # Safety
/// `out_y` is writable.
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_root_drag_window(rx: i32, ry: i32, nx: i32, ny: i32, out_y: *mut i32) -> i32 {
    let (x, y) = drag_window(rx, ry, nx, ny);
    // SAFETY: writable (the contract)
    unsafe { *out_y = y };
    x
}

#[no_mangle]
pub extern "C" fn ee_won_gui_root_drag_ends(button: i32) -> bool {
    drag_ends(button)
}

/// `cursor_place`: x, y into `out_y`.
///
/// # Safety
/// `out_y` is writable.
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_root_cursor_place(x: i32, y: i32, hot_x: i32, hot_y: i32, out_y: *mut i32) -> i32 {
    let (px, py) = cursor_place(x, y, hot_x, hot_y);
    // SAFETY: writable (the contract)
    unsafe { *out_y = py };
    px
}

#[no_mangle]
pub extern "C" fn ee_won_gui_ct_mouse_child(same: bool, has_old: bool, has_new: bool) -> u8 {
    mouse_child(same, has_old, has_new)
}

#[no_mangle]
pub extern "C" fn ee_won_gui_ct_release_to_child(has_child: bool, same_as_old: bool) -> bool {
    release_to_child(has_child, same_as_old)
}

/// `desired_size` over `count` children's (left, top, width, height): the width, the height into `out_h`.
///
/// # Safety
/// `rects` has `4 * count` entries (may be NULL when 0); `out_h` is writable.
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_ct_desired_size(rects: *const i32, count: usize, out_h: *mut i32) -> i32 {
    // SAFETY: the contract
    let r = unsafe { slice(rects as *const [i32; 4], count) };
    let (w, h) = desired_size(r);
    unsafe { *out_h = h };
    w
}

/// `find_top` over `count` children's always-on-top flags (non-zero: on top).
///
/// # Safety
/// `on_top` has `count` entries (may be NULL when 0); `from <= count`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_ct_find_top(on_top: *const u8, count: usize, from: usize) -> usize {
    // SAFETY: the contract
    let flags: Vec<bool> = unsafe { slice(on_top, count) }.iter().map(|&b| b != 0).collect();
    find_top(&flags, from)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn window() {
        assert!(mouse_enters(false, false) && !mouse_enters(true, false) && !mouse_enters(false, true));
        assert!(move_ends_double(true, 1, 0, 0, 0));
        assert!(!move_ends_double(false, 1, 0, 0, 0));
        assert!(move_ends_double(false, 0, 1, 0, 0));
        assert!(release_exits(0, false) && !release_exits(1, false) && !release_exits(0, true));
        assert_eq!(double_click(4, false, 9, 9), DoubleClick { double: true, start: false, tick: None });
        assert_eq!(double_click(3, false, 9, 9), DoubleClick { double: false, start: false, tick: None });
        assert_eq!(double_click(5, false, 900, 1000), DoubleClick { double: false, start: true, tick: Some(1000) });
        assert_eq!(double_click(5, true, 900, 1000), DoubleClick { double: true, start: false, tick: Some(0) });
        assert_eq!(double_click(5, true, 700, 1000), DoubleClick { double: false, start: false, tick: Some(1000) });
        assert_eq!(double_click(5, true, 0, 0), DoubleClick { double: false, start: false, tick: Some(1) });
        assert_eq!(double_click(5, true, u32::MAX - 10, 5), DoubleClick { double: true, start: false, tick: Some(0) });
        assert!(size_resizes(10, 10, 5, 10) && !size_resizes(-1, 10, 5, 5) && !size_resizes(5, 5, 5, 5));
        assert!(resize_due(true, 200, 100, 50, 0) && !resize_due(true, 150, 100, 50, 0));
        assert!(!resize_due(true, 200, 100, 50, 1) && !resize_due(false, 200, 100, 50, 0));
        assert!(cursor_forced(true, 2000, 1000) && !cursor_forced(true, 1999, 1000) && !cursor_forced(false, 9000, 0));
        assert_eq!(hover_elapsed(true, 100, [false; 3], 250), Some(150));
        assert_eq!(hover_elapsed(true, 100, [false; 3], 199), None);
        assert_eq!(hover_elapsed(true, 0, [false; 3], 250), None);
        assert_eq!(hover_elapsed(true, 100, [false, true, false], 250), None);
        assert_eq!(exit_point(0, 0, 640, 480), (641, 481));
        assert!(center_on_parent(true, true, true) && !center_on_parent(true, true, false));
        assert_eq!(center([0, 0, 800, 600], 200, 100), (300, 250));
        assert_eq!(center([0, 0, 100, 100], 301, 100), (0, 0));
        assert_eq!(center([0, 0, 100, 100], 103, 100), (0, 0));
        assert_eq!(center([10, 0, 100, 100], 103, 100), (9, 0));
        assert!(cursor_refresh(true, true) && cursor_refresh(false, false) && !cursor_refresh(false, true));
        assert_eq!(close_step(0), 1);
        assert_eq!(close_step(1), 0);
    }

    #[test]
    fn manager() {
        assert_eq!(timer_step(149, 100), None);
        assert_eq!(timer_step(150, 100), Some(50));
        assert_eq!(timer_step(5000, 100), Some(1000));
        assert_eq!(timer_step(10, u32::MAX - 100), Some(111));
        assert_eq!(popup_focus_step(true, false, 0x40, true, true, true), 1);
        assert_eq!(popup_focus_step(true, false, 0x40, true, false, true), 2);
        assert_eq!(popup_focus_step(true, false, 0x40, false, false, false), 3);
        assert_eq!(popup_focus_step(true, true, 0x40, true, false, true), 0);
        assert_eq!(popup_focus_step(true, false, 0x3f, true, false, true), 0);
        assert!(restore_due(100, false, 151) && !restore_due(100, false, 150) && !restore_due(0, false, 999));
        assert!(press_focuses(false, false, false) && !press_focuses(true, false, false));
        assert!(capture_changes(0, false, true) && capture_changes(0, true, false) && !capture_changes(0, true, true));
        assert!(!capture_changes(1, false, false));
        assert!(move_ends_popup(true, 8) && !move_ends_popup(false, 8) && !move_ends_popup(true, 7));
        assert!(wheel_to_popup(true, 0) && !wheel_to_popup(true, 0x4000));
        assert_eq!(popup_key(true, 8, 0x10, 1), 1);
        assert_eq!(popup_key(true, 8, 0x20, 0x1b), 1);
        assert_eq!(popup_key(true, 8, 0xa0, 1), 2);
        assert_eq!(popup_key(true, 9, 0x30, 0x1b), 0);
        assert_eq!(popup_key(false, 8, 0x30, 0x1b), 0);
        assert_eq!(size_merge([1, 2, 3, 4], [-1, 5, -1, 6]), [1, 5, 3, 6]);
        assert!(close_ends_popup(true, false, true) && !close_ends_popup(true, false, false));
        assert_eq!(popup_click_step(false, false, 0, true, true), 0);
        assert_eq!(popup_click_step(true, true, 0, true, false), 1);
        assert_eq!(popup_click_step(true, false, 0x10000, false, false), 1);
        assert_eq!(popup_click_step(true, false, 0, false, true), 0);
        assert_eq!(popup_click_step(true, false, 0, true, false), 0);
        assert_eq!(popup_click_step(true, false, 0, true, true), 2);
    }

    #[test]
    fn root_and_container() {
        assert!(drag_starts(0, true, true) && !drag_starts(0, false, true) && !drag_starts(1, true, true));
        assert_eq!(drag_place(10, 10, 5, 5, 5, 5), None);
        assert_eq!(drag_place(10, 10, 7, 4, 5, 5), Some((12, 9)));
        assert_eq!(drag_window(100, 50, 12, 9), (112, 59));
        assert!(drag_ends(0) && !drag_ends(2));
        assert_eq!(cursor_place(30, 40, 2, 3), (28, 37));
        assert_eq!(mouse_child(true, true, true), 0);
        assert_eq!(mouse_child(false, true, true), 3);
        assert_eq!(mouse_child(false, false, true), 2);
        assert!(release_to_child(true, true) && !release_to_child(true, false));
        assert_eq!(desired_size(&[[0, 0, 10, 20], [5, -5, 30, 10]]), (35, 20));
        assert_eq!(desired_size(&[[-50, -50, 10, 10]]), (0, 0));
        assert_eq!(find_top(&[false, true, true], 3), 1);
        assert_eq!(find_top(&[true, true], 2), 0);
        assert_eq!(find_top(&[false, false], 2), 2);
        assert_eq!(find_top(&[false, true], 1), 1);
        assert_eq!(find_top(&[], 0), 0);
    }
}
