//! The lobby shells' event maps (R6 WONLobby round 29): which of the shell's event kinds a `ComponentEvent`'s
//! `mType` is. The C++ shells' `HandleComponentEvent` made the choice (LobbyCtrlHost.cpp's `CtlEventOf`, LobbyDialogHost.cpp's
//! `LobbyRustDialog::HandleComponentEvent` and `LobbyRustPopup::HandleComponentEvent`); they now pass the raw type
//! (`RawEvent` calls) and the choice is here. `ee_won_lobby_event_of` is the drivers' (won_evmap_test compares it with
//! the original switches over every type).

use crate::lobbyctl::EeWonLobbyCtlEvent as Ev;
use crate::lobbydlg::EeWonLobbyDlgEvent as Dv;
use crate::lobbypopup::EeWonLobbyPopupEvent as Pv;

// WONGUI's `WindowEventType` (ee/wonapi/WONEventTypes.h: the enum's places)
const BUTTON_PRESSED: i32 = 15;
const TEXT_LINK_ACTIVATED: i32 = 17;
const INPUT_RETURN: i32 = 20;
const INPUT_TEXT_CHANGED: i32 = 21;
const LIST_SEL_CHANGED: i32 = 22;
const LIST_ITEM_CLICKED: i32 = 23;
const LIST_ITEM_UP_CLICKED: i32 = 24;
const TREE_ITEM_OPENED: i32 = 26;
const COMBO_SEL_CHANGED: i32 = 27;
const BROWSER_STATUS_CHANGE: i32 = 31;
const BROWSER_BUTTON_CHANGE: i32 = 33;
// the lobby's (ee/wonapi/WONLobbyEventTypes.h)
const GOT_HTTP_DOCUMENT: i32 = 100024;

/// `CtlEventOf(inType)`: the controls' event kind.
pub fn ctl_event_of(t: i32) -> Ev {
    match t {
        BUTTON_PRESSED => Ev::ButtonPressed,
        COMBO_SEL_CHANGED => Ev::ComboSelChanged,
        LIST_SEL_CHANGED => Ev::ListSelChanged,
        LIST_ITEM_CLICKED => Ev::ListItemClicked,
        BROWSER_BUTTON_CHANGE => Ev::BrowserButtonChange,
        BROWSER_STATUS_CHANGE => Ev::BrowserStatusChange,
        100029 => Ev::JoinedGame,
        100032 => Ev::LeftGame,
        100054 => Ev::SetRoomSpecFlags,
        100061 => Ev::ReadyChanged,
        100006 => Ev::ClientChanged,
        100038 => Ev::PlayerChanged,
        100014 => Ev::SyncOptions,
        100050 => Ev::LanOptions,
        100055 => Ev::PublicAddress,
        100065 => Ev::AllOptions,
        100062 => Ev::SyncOption,
        100063 => Ev::SyncPlayer,
        100064 => Ev::ColorChanged,
        INPUT_TEXT_CHANGED => Ev::InputTextChanged,
        INPUT_RETURN => Ev::InputReturn,
        100020 => Ev::GameChanged,
        100057 => Ev::SyncGameList,
        100034 => Ev::FiltersChanged,
        100059 => Ev::RefilterAll,
        100058 => Ev::RefilterShown,
        TEXT_LINK_ACTIVATED => Ev::TextLinkActivated,
        100001 => Ev::Alert,
        100003 => Ev::AnnounceFriend,
        100004 => Ev::AnnounceClientLocation,
        100007 => Ev::ClientModerated,
        100009 => Ev::ChatCommandError,
        100022 => Ev::GroupInvitation,
        100023 => Ev::GroupJoinAttempt,
        100030 => Ev::JoinedRoom,
        100033 => Ev::LeftRoom,
        100039 => Ev::PlayerTeamChanged,
        100044 => Ev::RecvChat,
        100048 => Ev::SetChatInput,
        TREE_ITEM_OPENED => Ev::TreeItemOpened,
        100021 => Ev::GroupChanged,
        100047 => Ev::ServerChanged,
        100053 => Ev::SetRoomList,
        100016 => Ev::FriendChanged,
        100056 => Ev::RefreshFriends,
        100036 => Ev::LoginInitStatus,
        100051 => Ev::SetLoginFields,
        100052 => Ev::FillUserNames,
        100041 => Ev::QueryAccountResult,
        GOT_HTTP_DOCUMENT => Ev::GotHttpDocument,
        _ => Ev::Other,
    }
}

/// `LobbyRustDialog::HandleComponentEvent`'s choice: the dialogs' event kind.
pub fn dlg_event_of(t: i32) -> Dv {
    match t {
        INPUT_TEXT_CHANGED => Dv::InputTextChanged,
        BUTTON_PRESSED => Dv::ButtonPressed,
        COMBO_SEL_CHANGED => Dv::ComboSelChanged,
        GOT_HTTP_DOCUMENT => Dv::GotHttpDocument,
        LIST_ITEM_CLICKED => Dv::ListItemClicked,
        _ => Dv::Other,
    }
}

/// `LobbyRustPopup::HandleComponentEvent`'s choice: the popups' event kind.
pub fn popup_event_of(t: i32) -> Pv {
    if t == LIST_ITEM_UP_CLICKED {
        Pv::ItemUpClicked
    } else {
        Pv::Other
    }
}

/// The map `map` (0 the controls', 1 the dialogs', 2 the popups') of the type `t`, as its enum's number (the drivers').
#[no_mangle]
pub extern "C" fn ee_won_lobby_event_of(map: i32, t: i32) -> u32 {
    match map {
        0 => ctl_event_of(t) as u32,
        1 => dlg_event_of(t) as u32,
        _ => popup_event_of(t) as u32,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_maps_name_their_types() {
        assert_eq!(ctl_event_of(15), Ev::ButtonPressed);
        assert_eq!(ctl_event_of(100024), Ev::GotHttpDocument);
        assert_eq!(ctl_event_of(100002), Ev::Other);
        assert_eq!(ctl_event_of(0), Ev::Other);
        assert_eq!(dlg_event_of(23), Dv::ListItemClicked);
        assert_eq!(dlg_event_of(22), Dv::Other);
        assert_eq!(popup_event_of(24), Pv::ItemUpClicked);
        assert_eq!(popup_event_of(23), Pv::Other);
        assert_eq!(ee_won_lobby_event_of(0, 100065), Ev::AllOptions as u32);
    }

    #[test]
    fn every_control_kind_but_other_has_one_type() {
        let mut seen = std::collections::BTreeMap::new();
        for t in (-10..200).chain(100000..100100) {
            let e = ctl_event_of(t) as u32;
            if e != 0 {
                assert!(seen.insert(e, t).is_none(), "two types for {e}");
            }
        }
        assert_eq!(seen.len(), 50);
    }
}
