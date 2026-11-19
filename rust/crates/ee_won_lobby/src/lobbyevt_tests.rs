use super::*;
use std::cell::RefCell;

thread_local! {
    static TRACE: RefCell<Vec<(EeWonLobbyEvtRef, isize)>> = const { RefCell::new(Vec::new()) };
}

unsafe extern "C" fn host(op: EeWonLobbyEvtRef, obj: isize) {
    TRACE.with(|t| t.borrow_mut().push((op, obj)));
}

fn trace() -> Vec<(EeWonLobbyEvtRef, isize)> {
    TRACE.with(|t| std::mem::take(&mut *t.borrow_mut()))
}

const ALL: [K; 43] = [
    K::AcceptClient,
    K::Alert,
    K::AnnounceClientLocation,
    K::AnnounceFriend,
    K::FriendAction,
    K::FriendChanged,
    K::GameAction,
    K::GameChanged,
    K::JoinFriendsRoom,
    K::JoinGame,
    K::PingGame,
    K::PlayerChanged,
    K::PlayerTeamChanged,
    K::ServerChanged,
    K::ClientAction,
    K::GotHttpDocument,
    K::ChangeDirectConnect,
    K::ClientChanged,
    K::ClientModerated,
    K::GroupChanged,
    K::GroupInvitation,
    K::GroupJoinAttempt,
    K::JoinedRoom,
    K::RecvChat,
    K::SetRoomSpecFlags,
    K::LoginInitStatus,
    K::SetLoginFields,
    K::QueryAccountResult,
    K::Login,
    K::GetLostPassword,
    K::GetLostUserName,
    K::LeftRoom,
    K::SetChatInput,
    K::ChatCommandError,
    K::CreateRoom,
    K::JoinRoom,
    K::CreateAccount,
    K::SyncGameList,
    K::JumpToRoom,
    K::QuickPlay,
    K::SetCrossPromotion,
    K::UpdateAccount,
    K::QueryServerGroups,
];

#[test]
fn every_kind_has_distinct_members() {
    for k in ALL {
        let l = layout(k);
        assert!(!l.is_empty(), "{k:?}");
        for (i, a) in l.iter().enumerate() {
            assert!(l[i + 1..].iter().all(|b| b.0 != a.0), "{k:?}: {:?} twice", a.0);
        }
    }
}

#[test]
fn references_are_taken_in_order_and_given_back_in_reverse() {
    // GroupChangedEvent(group, server, kind): mServer then mGroup
    let args = [2isize, 0x100, 0x200];
    // SAFETY: three values, objects the host only traces
    let e = unsafe { ee_won_lobbyevt_new(K::GroupChanged, args.as_ptr(), args.len(), Some(host)) };
    assert_eq!(trace(), vec![(EeWonLobbyEvtRef::AddRef, 0x100), (EeWonLobbyEvtRef::AddRef, 0x200)]);
    // SAFETY: live
    unsafe {
        assert_eq!(ee_won_lobbyevt_get(e, F::ChangeKind), 2);
        assert_eq!(ee_won_lobbyevt_get(e, F::Server), 0x100);
        assert_eq!(ee_won_lobbyevt_get(e, F::Group), 0x200);
        assert_eq!(ee_won_lobbyevt_get(e, F::Client), 0);
        ee_won_lobbyevt_free(e);
    }
    assert_eq!(trace(), vec![(EeWonLobbyEvtRef::Release, 0x200), (EeWonLobbyEvtRef::Release, 0x100)]);
}

#[test]
fn null_objects_take_no_reference() {
    let args = [0isize, 0, 0, 0, 0];
    // SAFETY: five values
    let e = unsafe { ee_won_lobbyevt_new(K::RecvChat, args.as_ptr(), args.len(), Some(host)) };
    // SAFETY: live
    unsafe { ee_won_lobbyevt_free(e) };
    assert!(trace().is_empty());
}

#[test]
fn strings_are_copies_and_numbers_are_written() {
    let user: Vec<u8> = b"u\0s\0".to_vec();
    let pass: Vec<u8> = b"p\0".to_vec();
    let args = [user.as_ptr() as isize, user.len() as isize, pass.as_ptr() as isize, pass.len() as isize, 1];
    assert_eq!(arg_count(K::Login), 5);
    // SAFETY: five values, the strings live
    let e = unsafe { ee_won_lobbyevt_new(K::Login, args.as_ptr(), args.len(), None) };
    drop(user);
    // SAFETY: live
    unsafe {
        assert_eq!((*e).bytes(F::UserName), b"u\0s\0");
        assert_eq!(ee_won_lobbyevt_len(e, F::Password), 2);
        assert_eq!(ee_won_lobbyevt_get(e, F::Remember), 1);
        ee_won_lobbyevt_set(e, F::Remember, 0);
        assert_eq!(ee_won_lobbyevt_get(e, F::Remember), 0);
        ee_won_lobbyevt_set_bytes(e, F::Password, b"xyz".as_ptr(), 3);
        assert_eq!((*e).bytes(F::Password), b"xyz");
        // not a number: left alone
        ee_won_lobbyevt_set(e, F::UserName, 7);
        assert_eq!((*e).bytes(F::UserName), b"u\0s\0");
        ee_won_lobbyevt_free(e);
    }
}

#[test]
fn addresses_are_copies() {
    let a = ee_won_net::ipobj::ee_won_ipaddr_new();
    // SAFETY: live values
    unsafe {
        ee_won_net::ipobj::ee_won_ipaddr_set_host_order(a, 0x7f000001, 15101);
        let args = [a as isize, 77];
        let e = ee_won_lobbyevt_new(K::JoinRoom, args.as_ptr(), 2, None);
        ee_won_ipaddr_free(a);
        let copy = ee_won_lobbyevt_get(e, F::Address) as *const EeWonIpAddr;
        assert_eq!(ee_won_net::ipobj::ee_won_ipaddr_port(copy), 15101);
        assert_eq!(ee_won_lobbyevt_get(e, F::GroupId), 77);
        ee_won_lobbyevt_free(e);
    }
}

#[test]
fn shells_are_bound_until_the_end() {
    let args = [5isize];
    // SAFETY: one value
    let e = unsafe { ee_won_lobbyevt_new(K::QuickPlay, args.as_ptr(), 1, None) };
    let shell = 0x5150usize as *mut c_void;
    // SAFETY: live
    unsafe { ee_won_lobbyevt_bind(e, shell) };
    assert_eq!(of(0x5150), Some(e));
    // SAFETY: live
    unsafe { ee_won_lobbyevt_free(e) };
    assert_eq!(of(0x5150), None);
}

#[test]
fn every_kind_reads_back_its_maker_values() {
    for k in ALL {
        let n = arg_count(k);
        let bytes = b"abcd".to_vec();
        let mut args = Vec::new();
        for (i, &(_, t)) in layout(k).iter().enumerate() {
            match t {
                Ty::Bytes => {
                    args.push(bytes.as_ptr() as isize);
                    args.push(i as isize % 4 + 1);
                }
                Ty::Addr => args.push(0),
                _ => args.push(0x40 + i as isize),
            }
        }
        assert_eq!(args.len(), n);
        // SAFETY: n values
        let e = unsafe { ee_won_lobbyevt_new(k, args.as_ptr(), n, Some(host)) };
        for (i, &(f, t)) in layout(k).iter().enumerate() {
            // SAFETY: live
            let v = unsafe { ee_won_lobbyevt_get(e, f) };
            match t {
                Ty::Int | Ty::Ref => assert_eq!(v, 0x40 + i as isize, "{k:?} {f:?}"),
                Ty::Addr => assert_ne!(v, 0),
                // SAFETY: live
                Ty::Bytes => assert_eq!(unsafe { (*e).bytes(f) }, &bytes[..i % 4 + 1]),
            }
        }
        // SAFETY: live
        unsafe { ee_won_lobbyevt_free(e) };
        let t = trace();
        let refs = layout(k).iter().filter(|x| x.1 == Ty::Ref).count();
        assert_eq!(t.len(), refs * 2, "{k:?}");
    }
}
