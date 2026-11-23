//! ChatCtrl's link (LobbyNetCtrlHost.cpp's ChatLink, R6 WON round 33): the constructor's body, the original's five
//! constructors by kind (a room, an Accept or Reject, a client, a friend, a friend's location), in Rust over
//! one-statement C++ ops (`ChatLinkHost`), in the original's order. The members stay the C++ link's.
//!
//! Reach: the LAN game makes no chat link (round 32's probe); the kinds run in won_lobby33_test's golden cases.

use crate::chatctl::EeWonLobbyCtlLink;
use std::ffi::c_void;

/// The host: `(link, op, a) -> result`.
pub type EeWonChatLinkHostFn = Option<unsafe extern "C" fn(link: *mut c_void, op: EeWonChatLinkOp, a: isize) -> isize>;

/// The host's ops: each one C++ statement.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonChatLinkOp {
    /// `k->mClient = (LobbyClient*)a`.
    SetClient = 1,
    /// `k->mText = k->mClient->GetName()`.
    TextClientName,
    /// `k->mText = Txt(a)`.
    SetText,
    /// `k->mGroupId = (unsigned short)a`.
    SetGroupId,
    /// `k->mAddress = ((LobbyServer*)a)->GetAddress()`.
    AddressOfServer,
    /// `k->mFriend = (LobbyFriend*)a`.
    SetFriend,
    /// `k->mText = k->mFriend->GetName()`.
    TextFriendName,
    /// `k->mAddress = IPAddr::FromValue(ee_won_ipaddr_clone((const EeWonIpAddr*)a))`.
    AddressClone,
    /// `k->mColor = (int)a`.
    SetColor,
    /// `k->mFont = (Font*)a`.
    SetFont,
    /// `k->mEnabled = true`.
    SetEnabled,
    /// `k->mPrevious = NULL`.
    ClearPrevious,
}

use EeWonChatLinkOp as Op;

fn init(l: &EeWonLobbyCtlLink, op: &dyn Fn(Op, isize) -> isize) {
    match l.kind {
        3 => {
            op(Op::SetClient, l.obj);
            op(Op::TextClientName, 0);
        }
        0 => {
            op(Op::SetText, l.text);
            op(Op::SetGroupId, l.id as isize);
            if l.addr != 0 {
                op(Op::AddressOfServer, l.addr);
            }
        }
        4 => {
            op(Op::SetFriend, l.obj);
            op(Op::TextFriendName, 0);
        }
        5 => {
            op(Op::SetFriend, l.obj);
            op(Op::AddressClone, l.addr);
            op(Op::SetText, l.text);
        }
        _ => {
            op(Op::SetClient, l.obj);
            op(Op::SetText, l.text);
        }
    }
    op(Op::SetColor, l.color as isize);
    op(Op::SetFont, l.font);
    op(Op::SetEnabled, 0);
    op(Op::ClearPrevious, 0);
}

/// ChatLink's constructor body on `link` (its kind already set) over the host `host`.
///
/// # Safety
/// `l` is valid; `host` answers the ops for `link` (ChatLinkHost).
#[no_mangle]
pub unsafe extern "C" fn ee_won_chatlink_init(
    link: *mut c_void,
    l: *const EeWonLobbyCtlLink,
    host: EeWonChatLinkHostFn,
) {
    let op = |o: Op, a: isize| match host {
        // SAFETY: the contract
        Some(f) => unsafe { f(link, o, a) },
        None => 0,
    };
    // SAFETY: valid (the contract)
    init(unsafe { &*l }, &op);
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::cell::RefCell;

    #[test]
    fn the_five_constructors_by_kind() {
        let run = |l: EeWonLobbyCtlLink| {
            let log = RefCell::new(Vec::new());
            init(&l, &|o, a| {
                log.borrow_mut().push(format!("{o:?} {a}"));
                0
            });
            log.into_inner()
        };
        let l = EeWonLobbyCtlLink { kind: 0, text: 7, id: 3, addr: 0, color: 5, font: 9, ..Default::default() };
        assert_eq!(run(l)[..3], ["SetText 7", "SetGroupId 3", "SetColor 5"]);
        let l = EeWonLobbyCtlLink { kind: 5, obj: 2, addr: 4, text: 7, ..Default::default() };
        assert_eq!(run(l)[..3], ["SetFriend 2", "AddressClone 4", "SetText 7"]);
        let l = EeWonLobbyCtlLink { kind: 1, obj: 2, text: 7, ..Default::default() };
        assert_eq!(run(l)[..2], ["SetClient 2", "SetText 7"]);
        assert_eq!(run(EeWonLobbyCtlLink { kind: 3, obj: 2, ..Default::default() }).len(), 6);
    }
}
