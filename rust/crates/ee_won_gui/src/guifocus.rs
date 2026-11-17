//! WONGUI's focus rules, the hit test's verdict and the buttons' state machine (ComponentTraversal.cpp,
//! ContainerOrdering.cpp, ContainerInteraction.cpp, Container.cpp, Button.cpp, WONButton.cpp). The C++ keeps the
//! components, their child lists, the virtual calls (`TabFocus`, `HitTest`, `RequestFocus`, the events, the
//! sounds and timers) and walks the lists; these answer:
//!
//! - whether a component can take focus (`WantFocus`), the next focus id (`GetNextFocusId`), which child had
//!   focus most recently (`SetFocusToLastFocusChild`), `TabFocus`'s start (the component itself, nothing, or its
//!   children) and its loop's bound;
//! - what a hit child means for `FindChildAt` (itself, or nothing when it is disabled or a modal child blocks
//!   it);
//! - a button's release (`ButtonUp`), drag (`MouseDrag`), keys (`KeyDown`, `KeyUp`) and repeat
//!   (`TimerEvent`), and a skinned button's state (`WONButton::GetButtonState`).
//!
//! The button flags are the original's: 0x1 highlight on hover, 0x2 repeat, 0x4 checked, 0x8 radio, 0x10 mouse
//! over, 0x20 pressed, 0x40 activate on press, 0x200 no activate, 0x400 default on focus.

/// The component flags these rules read.
pub(crate) const FLAG_WANT_FOCUS: u32 = 0x8;
pub(crate) const FLAG_WANT_TAB_FOCUS: u32 = 0x4000;
/// A child a modal state does not block.
pub(crate) const FLAG_MODAL_OK: u32 = 0x8000;

/// `Component::WantFocus`: it asks for focus, is visible and enabled, and no modal state of its parent blocks it.
pub fn want_focus(flags: u32, visible: bool, disabled: bool, parent_modal: bool) -> bool {
    flags & FLAG_WANT_FOCUS != 0 && visible && !disabled && !(parent_modal && flags & FLAG_MODAL_OK == 0)
}

/// `FindChildAt`'s answer for the topmost child under the point: it, unless it is disabled or a modal state
/// blocks it (then no child at all).
pub fn hit_child(disabled: bool, modal: bool, flags: u32) -> bool {
    !disabled && !(modal && flags & FLAG_MODAL_OK == 0)
}

/// `GetNextFocusId`: the byte after `id`, skipping 0.
pub fn next_focus_id(id: u8) -> u8 {
    let n = id.wrapping_add(1);
    if n == 0 {
        1
    } else {
        n
    }
}

/// `SetFocusToLastFocusChild`'s pick among the children's focus ids: the one focused most recently before
/// `current` (the smallest byte difference), skipping id 0 and, when `skip_current`, `current` itself. The
/// difference is compared as an `int` and kept as a byte, as the original did, so an id above `current` (a
/// negative difference) wins over every kept one until a smaller byte replaces it.
pub fn last_focus_child(current: u8, skip_current: bool, ids: &[u8]) -> Option<usize> {
    let mut minimum: u8 = 0;
    let mut last = None;
    for (i, &id) in ids.iter().enumerate() {
        if id == 0 || (id == current && skip_current) {
            continue;
        }
        if minimum == 0 || i32::from(current) - i32::from(id) < i32::from(minimum) {
            minimum = current.wrapping_sub(id);
            last = Some(i);
        }
    }
    last
}

/// `Container::TabFocus`'s start: 0 refuse (false), 1 take the focus itself (true), 2 walk the children.
pub fn tab_focus_start(want_focus: bool, init: bool, isolate: bool, flags: u32, has_children: bool) -> i32 {
    if !want_focus {
        return 0;
    }
    if init {
        if isolate {
            return 0;
        }
        if flags & FLAG_WANT_TAB_FOCUS != 0 {
            return 1;
        }
    }
    if has_children {
        2
    } else {
        0
    }
}

/// `TabFocus`'s loop: whether to stop after the `count`-th child tried (the original's `++count >= size`).
pub fn tab_focus_done(count: u64, children: usize) -> bool {
    count.wrapping_add(1) >= children as u64
}

/// What `ButtonUp` does.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonButtonUp {
    /// Still held (or not pressed): nothing.
    Ignore = 0,
    /// Released: a repeating button stops its timer.
    StopRepeat = 1,
    /// Released without activating.
    Release = 2,
    /// Released and activated.
    Activate = 3,
}

/// `Button::ButtonUp`: a press the mouse (state 2) or Space still holds stays; a press dragged off (state 1)
/// never activates; an activate-on-press button activated already.
pub fn button_up(flags: u32, mouse_down_state: i32, key_down: bool, activate: bool) -> EeWonButtonUp {
    if flags & 0x20 == 0 || mouse_down_state == 2 || key_down {
        return EeWonButtonUp::Ignore;
    }
    let activate = activate && mouse_down_state != 1;
    if flags & 0x2 != 0 {
        EeWonButtonUp::StopRepeat
    } else if activate && flags & 0x40 == 0 {
        EeWonButtonUp::Activate
    } else {
        EeWonButtonUp::Release
    }
}

/// `Button::MouseDrag`: -1 nothing (no press), 0 inside (state 2; press it again if released, then the repeat
/// delay is 500), 1 outside (state 1; release without activating).
pub fn button_drag(mouse_down_state: i32, x: i32, y: i32, width: i32, height: i32) -> i32 {
    if mouse_down_state == 0 {
        -1
    } else if x >= 0 && y >= 0 && x < width && y < height {
        0
    } else {
        1
    }
}

/// What `Button::KeyDown` does.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonButtonKey {
    /// Not handled (false).
    None = 0,
    /// Enter on the default button: activate (true).
    Activate = 1,
    /// Space: held down and pressed (true).
    Press = 2,
    /// An arrow in a radio group: the previous radio activated and focused (true).
    RadioBack = 3,
    /// The next radio.
    RadioForward = 4,
}

pub fn button_key(key: i32, is_default: bool, key_down: bool, radio: bool) -> EeWonButtonKey {
    if key == 13 {
        if is_default {
            return EeWonButtonKey::Activate;
        }
    } else if key == 32 && !key_down {
        return EeWonButtonKey::Press;
    }
    if radio {
        match key {
            38 | 37 => EeWonButtonKey::RadioBack,
            40 | 39 => EeWonButtonKey::RadioForward,
            _ => EeWonButtonKey::None,
        }
    } else {
        EeWonButtonKey::None
    }
}

/// `Button::TimerEvent`: (the delay, fire): after 500 ms held, every 100 ms.
pub fn button_repeat(delay: i32, delta: i32) -> (i32, bool) {
    let d = delay.wrapping_add(delta);
    if d > 500 {
        (d - 100, true)
    } else {
        (d, false)
    }
}

/// `WONButton::GetButtonState`'s state: 2 disabled, 1 pressed, 3 the mouse over a highlighting button, 4 the
/// default button, 0 normal; and the checked column (1 for a checked button that shows its check).
pub fn button_state(disabled: bool, flags: u32, is_default: bool, has_check: bool) -> (usize, usize) {
    let state = if disabled {
        2
    } else if flags & 0x20 != 0 {
        1
    } else if flags & 0x10 != 0 && flags & 1 != 0 {
        3
    } else if is_default {
        4
    } else {
        0
    };
    (state, usize::from(has_check && flags & 0x4 != 0))
}

/// The cells of `GetButtonState`'s fallback, in the order it tries them: the state's own, the normal state's in
/// the same column, and for the checked column the normal unchecked one. A missing image, colour (-1) or text
/// each takes the first cell that has it; a colour still missing is the button's default text colour.
pub fn button_state_cells(state: usize, checked: usize) -> Vec<(usize, usize)> {
    let mut v = vec![(state, checked), (0, checked)];
    if checked == 1 {
        v.push((0, 0));
    }
    v
}

// ---- the C ABI

#[no_mangle]
pub extern "C" fn ee_won_gui_want_focus(flags: u32, visible: bool, disabled: bool, parent_modal: bool) -> bool {
    want_focus(flags, visible, disabled, parent_modal)
}

#[no_mangle]
pub extern "C" fn ee_won_gui_hit_child(disabled: bool, modal: bool, flags: u32) -> bool {
    hit_child(disabled, modal, flags)
}

#[no_mangle]
pub extern "C" fn ee_won_gui_next_focus_id(id: u8) -> u8 {
    next_focus_id(id)
}

/// `last_focus_child` over `count` ids: the index, or -1.
///
/// # Safety
/// `ids` holds `count` bytes (or is NULL with 0).
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_last_focus_child(
    current: u8,
    skip_current: bool,
    ids: *const u8,
    count: usize,
) -> i64 {
    let s = if ids.is_null() || count == 0 {
        &[][..]
    } else {
        // SAFETY: `count` bytes (the contract)
        unsafe { std::slice::from_raw_parts(ids, count) }
    };
    last_focus_child(current, skip_current, s).map_or(-1, |i| i as i64)
}

#[no_mangle]
pub extern "C" fn ee_won_gui_tab_focus_start(
    want_focus: bool,
    init: bool,
    isolate: bool,
    flags: u32,
    has_children: bool,
) -> i32 {
    tab_focus_start(want_focus, init, isolate, flags, has_children)
}

#[no_mangle]
pub extern "C" fn ee_won_gui_tab_focus_done(count: u64, children: usize) -> bool {
    tab_focus_done(count, children)
}

#[no_mangle]
pub extern "C" fn ee_won_gui_button_up(
    flags: u32,
    mouse_down_state: i32,
    key_down: bool,
    activate: bool,
) -> EeWonButtonUp {
    button_up(flags, mouse_down_state, key_down, activate)
}

#[no_mangle]
pub extern "C" fn ee_won_gui_button_drag(mouse_down_state: i32, x: i32, y: i32, width: i32, height: i32) -> i32 {
    button_drag(mouse_down_state, x, y, width, height)
}

#[no_mangle]
pub extern "C" fn ee_won_gui_button_key(key: i32, is_default: bool, key_down: bool, radio: bool) -> EeWonButtonKey {
    button_key(key, is_default, key_down, radio)
}

/// `button_repeat`: the new delay into `out_delay`, fire returned.
///
/// # Safety
/// `out_delay` is writable.
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_button_repeat(delay: i32, delta: i32, out_delay: *mut i32) -> bool {
    let (d, f) = button_repeat(delay, delta);
    // SAFETY: writable (the contract)
    unsafe { *out_delay = d };
    f
}

/// `button_state`: the state into `out_state`, the checked column returned.
///
/// # Safety
/// `out_state` is writable.
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_button_state(
    disabled: bool,
    flags: u32,
    is_default: bool,
    has_check: bool,
    out_state: *mut i32,
) -> i32 {
    let (s, c) = button_state(disabled, flags, is_default, has_check);
    // SAFETY: writable (the contract)
    unsafe { *out_state = s as i32 };
    c as i32
}

/// `button_state_cells`: cell `index` (0, 1, 2) into the outs; false past the last.
///
/// # Safety
/// The outs are writable.
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_button_state_cell(
    state: i32,
    checked: i32,
    index: i32,
    out_state: *mut i32,
    out_checked: *mut i32,
) -> bool {
    let cells = button_state_cells(state.max(0) as usize, checked.clamp(0, 1) as usize);
    match usize::try_from(index).ok().and_then(|i| cells.get(i)) {
        Some(&(s, c)) => {
            // SAFETY: writable (the contract)
            unsafe {
                *out_state = s as i32;
                *out_checked = c as i32;
            }
            true
        }
        None => false,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn focus() {
        assert!(want_focus(8, true, false, false));
        assert!(
            !want_focus(0, true, false, false)
                && !want_focus(8, false, false, false)
                && !want_focus(8, true, true, false)
        );
        assert!(!want_focus(8, true, false, true) && want_focus(0x8008, true, false, true));
        assert!(
            hit_child(false, false, 0)
                && !hit_child(true, false, 0)
                && !hit_child(false, true, 0)
                && hit_child(false, true, 0x8000)
        );
        assert_eq!((next_focus_id(3), next_focus_id(255)), (4, 1));
        assert_eq!(last_focus_child(10, true, &[0, 4, 9, 10, 7]), Some(2));
        // the current id itself gives a difference of 0, which counts as no pick: the next id is taken
        assert_eq!(last_focus_child(10, false, &[0, 4, 9, 10, 7]), Some(4));
        // a newer id (above the current one) gives a negative difference: it wins, and keeps the byte 254
        assert_eq!(last_focus_child(10, true, &[4, 12, 9]), Some(2));
        assert_eq!(last_focus_child(10, true, &[12, 4]), Some(1));
        assert_eq!(last_focus_child(10, true, &[0, 10]), None);
        assert_eq!(tab_focus_start(false, true, false, 0x4000, true), 0);
        assert_eq!(tab_focus_start(true, true, true, 0x4000, true), 0);
        assert_eq!(tab_focus_start(true, true, false, 0x4000, false), 1);
        assert_eq!(tab_focus_start(true, false, false, 0x4000, true), 2);
        assert_eq!(tab_focus_start(true, false, false, 0, false), 0);
        assert!(!tab_focus_done(0, 2) && tab_focus_done(1, 2) && tab_focus_done(0, 0));
    }

    #[test]
    fn buttons() {
        assert_eq!(button_up(0, 0, false, true), EeWonButtonUp::Ignore);
        assert_eq!(button_up(0x20, 2, false, true), EeWonButtonUp::Ignore);
        assert_eq!(button_up(0x20, 0, true, true), EeWonButtonUp::Ignore);
        assert_eq!(button_up(0x20, 0, false, true), EeWonButtonUp::Activate);
        assert_eq!(button_up(0x20, 1, false, true), EeWonButtonUp::Release);
        assert_eq!(button_up(0x60, 0, false, true), EeWonButtonUp::Release);
        assert_eq!(button_up(0x22, 0, false, true), EeWonButtonUp::StopRepeat);
        assert_eq!((button_drag(0, 1, 1, 5, 5), button_drag(1, 1, 1, 5, 5), button_drag(2, 5, 1, 5, 5)), (-1, 0, 1));
        assert_eq!(button_key(13, true, false, false), EeWonButtonKey::Activate);
        assert_eq!(button_key(13, false, false, true), EeWonButtonKey::None);
        assert_eq!(button_key(32, false, false, false), EeWonButtonKey::Press);
        assert_eq!(button_key(32, false, true, false), EeWonButtonKey::None);
        assert_eq!(button_key(37, false, false, true), EeWonButtonKey::RadioBack);
        assert_eq!(button_key(39, false, false, true), EeWonButtonKey::RadioForward);
        assert_eq!(button_key(39, false, false, false), EeWonButtonKey::None);
        assert_eq!((button_repeat(450, 50), button_repeat(450, 51)), ((500, false), (401, true)));
        assert_eq!(button_state(true, 0x20, true, true), (2, 0));
        assert_eq!(button_state(false, 0x24, false, true), (1, 1));
        assert_eq!(button_state(false, 0x10, true, false), (4, 0));
        assert_eq!(button_state(false, 0x11, true, false), (3, 0));
        assert_eq!(button_state_cells(3, 1), vec![(3, 1), (0, 1), (0, 0)]);
        assert_eq!(button_state_cells(4, 0), vec![(4, 0), (0, 0)]);
    }
}
