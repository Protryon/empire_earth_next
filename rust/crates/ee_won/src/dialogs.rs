//! The lobby dialogs' decisions (WONLobby: CreateGameDialog.cpp, CreateRoomDialog.cpp, LanPortDialog.cpp,
//! AddFriendDialog.cpp, ChooseNetworkAdapterDialog.cpp, StatusDialog.cpp). The C++ keeps the dialogs, their
//! controls, layouts, texts and events; these answer:
//!
//! - when the create button, the OK button or the add button may be pressed (`CheckInput`);
//! - the skill level a game takes from the combo (`CreateGameDialog::GetGame`), whether the Internet-only access
//!   radios show (`SetGameType`);
//! - the LAN port's range and its clash with the game port (`LanPortDialog::CheckInput`);
//! - the network adapters' order (the default entry first) and the address chosen
//!   (`NetworkAdapterListItem::Compare`, `GetSelectedAddress`);
//! - the status dialog's sizes: the message's width (30 'M's to half the root), the padding of a short message,
//!   the widening of a tall one and its first 50 ms that never shrink (`StatusDialog::SetText`).
//!
//! Ints wrap (the game builds with -fwrapv).

/// `CreateGameDialog::CheckInput` and `CreateRoomDialog::CheckInput`: a name, and a password when its field
/// shows.
pub fn create_enabled(name_len: u32, password_shown: bool, password_len: u32) -> bool {
    name_len != 0 && !(password_shown && password_len == 0)
}

/// `AddFriendDialog::CheckInput`: a name.
pub fn name_enabled(name_len: u32) -> bool {
    name_len != 0
}

/// `CreateGameDialog::GetGame`: the combo's selection as a skill level (0..4; anything else is 0).
pub fn skill_level(sel: i32) -> i32 {
    if (0..5).contains(&sel) {
        sel
    } else {
        0
    }
}

/// `CreateGameDialog::SetGameType`: the invite-only and ask-to-join radios show for Internet games (type 2) only.
pub fn internet_access(game_type: i32) -> bool {
    game_type == 2
}

/// `LanPortDialog::CheckInput`: a port of 5000 to 65535 that is not the game port.
pub fn lan_port_ok(port: i32, game_port: u16) -> bool {
    port != i32::from(game_port) && port > 4999 && port < 65536
}

/// `NetworkAdapterListItem::Compare`: the default entry (host 0) first; `None` when the columns decide.
pub fn adapter_order(host: i32, other: i32) -> Option<i32> {
    if host == other {
        Some(0)
    } else if host == 0 {
        Some(-1)
    } else if other == 0 {
        Some(1)
    } else {
        None
    }
}

/// `GetSelectedAddress`: the selected adapter's host, or the local host when none is selected.
pub fn selected_address(selected: Option<u32>, local_host: u32) -> u32 {
    selected.unwrap_or(local_host)
}

/// `StatusDialog::SetText`: the message's width: its text's width, at least 30 'M's, at most half the root.
pub fn status_body_width(text_width: i32, m_width: i32, root_width: i32) -> i32 {
    let mut w = text_width;
    let min = m_width.wrapping_mul(30);
    if w < min {
        w = min;
    }
    if w > root_width / 2 {
        w = root_width / 2;
    }
    w
}

/// `StatusDialog::SetText`: the padding above and below a message of fewer than 5 lines (`None` for longer).
pub fn status_pad(label_height: i32, line_height: i32) -> Option<i32> {
    let lines = label_height / line_height;
    (lines < 5).then(|| (5 - lines).wrapping_mul(line_height) / 2)
}

/// `StatusDialog::SetText`: the extra width a tall dialog takes (it grows to 5/4 of its width) when it is
/// taller than wide or reaches the root's height; `None` when it keeps its shape.
pub fn status_extra_width(
    new_width: i32,
    new_height: i32,
    root_height: i32,
    width_change: i32,
    old_width: i32,
) -> Option<i32> {
    if new_height > new_width || new_height >= root_height {
        Some(new_width.wrapping_mul(5) / 4 - width_change - old_width)
    } else {
        None
    }
}

/// `StatusDialog::SetText`: whether the dialog is still in its first 50 ms (it does not shrink then).
pub fn status_young(now: u32, start: u32) -> bool {
    now.wrapping_sub(start) < 50
}

/// `StatusDialog::SetText`: a size change, never negative.
pub fn growth(new: i32, old: i32) -> i32 {
    new.wrapping_sub(old).max(0)
}

// ---- the C ABI

#[no_mangle]
pub extern "C" fn ee_won_dlg_create_enabled(name_len: u32, password_shown: bool, password_len: u32) -> bool {
    create_enabled(name_len, password_shown, password_len)
}

#[no_mangle]
pub extern "C" fn ee_won_dlg_name_enabled(name_len: u32) -> bool {
    name_enabled(name_len)
}

#[no_mangle]
pub extern "C" fn ee_won_dlg_skill_level(sel: i32) -> i32 {
    skill_level(sel)
}

#[no_mangle]
pub extern "C" fn ee_won_dlg_internet_access(game_type: i32) -> bool {
    internet_access(game_type)
}

#[no_mangle]
pub extern "C" fn ee_won_dlg_lan_port_ok(port: i32, game_port: u16) -> bool {
    lan_port_ok(port, game_port)
}

/// `adapter_order`: the order, or 2 when the columns decide.
#[no_mangle]
pub extern "C" fn ee_won_dlg_adapter_order(host: i32, other: i32) -> i32 {
    adapter_order(host, other).unwrap_or(2)
}

#[no_mangle]
pub extern "C" fn ee_won_dlg_selected_address(has_selection: bool, selected: u32, local_host: u32) -> u32 {
    selected_address(has_selection.then_some(selected), local_host)
}

#[no_mangle]
pub extern "C" fn ee_won_dlg_status_body_width(text_width: i32, m_width: i32, root_width: i32) -> i32 {
    status_body_width(text_width, m_width, root_width)
}

/// `status_pad`: -1 for 5 lines or more.
#[no_mangle]
pub extern "C" fn ee_won_dlg_status_pad(label_height: i32, line_height: i32) -> i32 {
    status_pad(label_height, line_height).unwrap_or(-1)
}

/// `status_extra_width`: false when the dialog keeps its shape, else true with the extra width in `out`.
///
/// # Safety
/// `out` is writable.
#[no_mangle]
pub unsafe extern "C" fn ee_won_dlg_status_extra_width(
    new_width: i32,
    new_height: i32,
    root_height: i32,
    width_change: i32,
    old_width: i32,
    out: *mut i32,
) -> bool {
    match status_extra_width(new_width, new_height, root_height, width_change, old_width) {
        Some(e) => {
            // SAFETY: writable (the contract)
            unsafe { *out = e };
            true
        }
        None => false,
    }
}

#[no_mangle]
pub extern "C" fn ee_won_dlg_status_young(now: u32, start: u32) -> bool {
    status_young(now, start)
}

#[no_mangle]
pub extern "C" fn ee_won_dlg_growth(new: i32, old: i32) -> i32 {
    growth(new, old)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn forms() {
        assert!(create_enabled(3, false, 0) && create_enabled(3, true, 2));
        assert!(!create_enabled(0, false, 0) && !create_enabled(3, true, 0));
        assert!(name_enabled(1) && !name_enabled(0));
        assert_eq!((skill_level(4), skill_level(5), skill_level(-1)), (4, 0, 0));
        assert!(internet_access(2) && !internet_access(1));
        assert!(lan_port_ok(5000, 6000) && lan_port_ok(65535, 6000) && !lan_port_ok(4999, 6000));
        assert!(!lan_port_ok(65536, 6000) && !lan_port_ok(6000, 6000));
        assert_eq!(adapter_order(5, 5), Some(0));
        assert_eq!(adapter_order(0, 5), Some(-1));
        assert_eq!(adapter_order(5, 0), Some(1));
        assert_eq!(adapter_order(5, 6), None);
        assert_eq!(selected_address(None, 7), 7);
        assert_eq!(selected_address(Some(0), 7), 0);
    }

    #[test]
    fn status() {
        assert_eq!(status_body_width(100, 10, 1024), 300);
        assert_eq!(status_body_width(900, 10, 1024), 512);
        assert_eq!(status_pad(26, 13), Some(19));
        assert_eq!(status_pad(65, 13), None);
        assert_eq!(status_extra_width(300, 400, 768, 50, 250), Some(75));
        assert_eq!(status_extra_width(400, 300, 768, 50, 250), None);
        assert_eq!(status_extra_width(400, 300, 300, 50, 250), Some(200));
        assert!(status_young(49, 0) && !status_young(50, 0) && status_young(10, u32::MAX - 5));
        assert_eq!((growth(5, 9), growth(9, 5)), (0, 4));
    }
}
