//! The Internet logics' host cases whose statements are Rust (R6 WON round 32): an op AccountLogic's, InitLogic's
//! and RoutingLogic's Rust objects (logics.rs, routelogic.rs) send to their C++ hosts (AccountLogic.cpp,
//! InitLogic.cpp, RoutingLogic.cpp) that was a run of C++ statements, with or without decisions between them, is
//! answered here in the game; each C++ statement is a one-statement case of the same host, sent through the same
//! dispatch. Reached only in the game (the logics while the network core is bound, RoutingLogic in its model mode):
//! the drivers' scripted hosts never reach it, so the logics' goldens see the ops as before. won_lobbyops_test runs
//! the deleted cases (as they were, tests/won/won_netlogic_orig.h) against these over one stand-in world.
//!
//! The objects a moved case held in a `SmartPtr` local are held references here (the new ops answer them with a
//! reference added; `Release` gives it back where the local died).

use crate::lobbydlg::EeWonLobbyDlgCall as Dlg;
use crate::lobbyevt::EeWonLobbyEvtField as F;
use crate::logics::EeWonLogicOp as Op;
use crate::resource::EeWonLobbyRes as Res;
use crate::routelogic::EeWonRouteLogicOp as ROp;

/// The calls out: the op through the same dispatch the moved op came by.
pub(crate) type Out<'a> = &'a dyn Fn(Op, isize, isize, isize, isize) -> isize;
/// RoutingLogic's calls out (`a`..`e`).
pub(crate) type ROut<'a> = &'a dyn Fn(ROp, &[isize]) -> isize;

const ACCOUNT_NEWSLETTER: isize = ee_won_net::reqdir::EE_WON_REQOP_ACCOUNT_NEWSLETTER as isize;
const HTTPDOC_MOTD: isize = 0;
const HTTPDOC_TOU: isize = 1;
const HTTPDOC_ADV: isize = 2;

/// `sizeof(wchar_t)`.
const WCHAR: usize = if cfg!(windows) { 2 } else { 4 };

/// [`logic_op`] in the game only: while the network core is bound (the game binds it with the lobby's WONAPICore,
/// before any logic exists; the drivers' scripted hosts never do).
pub(crate) fn game_logic_op(op: Op, a: isize, b: isize, c: isize, d: isize, out: Out) -> Option<isize> {
    if !ee_won_net::cppnet::bound() {
        return None;
    }
    // the compositions' calls out reach the private state's ops here too
    let out = |o: Op, a: isize, b: isize, c: isize, d: isize| prv_op(o, a, b).unwrap_or_else(|| out(o, a, b, c, d));
    prv_op(op, a, b).or_else(|| logic_op(op, a, b, c, d, &out))
}

/// InitLogic's ops whose C++ cases called a lobby function that was the private state's own Rust (lobbyprv.rs;
/// LobbySetVersionStatus, LobbyResetFirewallTest, LobbySetBehindFirewall, LobbySetTOUTimes and LobbyGetTOUTimes, R6
/// WON round 39: those cases and the four setters are gone), answered on that state.
pub(crate) fn prv_op(op: Op, a: isize, b: isize) -> Option<isize> {
    use crate::lobbyprv::{self as prv, EeWonLobbyPrvField as F};
    match op {
        Op::SetVersionStatus => prv::ee_won_lobbyprv_set(F::VersionStatus, a as i32 as i64),
        Op::ResetFirewallTest => prv::ee_won_lobbyprv_reset_firewall(),
        Op::SetBehindFirewall => prv::ee_won_lobbyprv_set(F::BehindFirewall, (a != 0) as i64),
        Op::SetTOUTimes => prv::ee_won_lobbyprv_set_tou(a as std::ffi::c_long, b as std::ffi::c_long),
        Op::TOUTimes => {
            let (mut system, mut game): (std::ffi::c_long, std::ffi::c_long) = (0, 0);
            // SAFETY: the locals
            unsafe { prv::ee_won_lobbyprv_tou_times(&mut system, &mut game) };
            // SAFETY: the op's two int32 outputs (the caller's locals)
            unsafe {
                *(a as *mut i32) = system as i32;
                *(b as *mut i32) = game as i32;
            }
        }
        _ => return None,
    }
    Some(0)
}

/// A status wait's flags as `RunWithStatusRes`'s `d` (the original's `(a, b)` arguments).
fn wait(cancel_flags: isize) -> isize {
    cancel_flags & 0xff
}

/// A wide string's UTF-16 little-endian bytes (`WideAppendUtf16LE`), from `count` `wchar_t`s at `p`.
fn utf16le_of_wide(p: isize, count: usize) -> Vec<u8> {
    let p = p as *const std::ffi::c_void;
    // SAFETY: `p` holds `count` wchar_t units (an event's or a text's live copy); the size pass writes nothing
    let n = unsafe { ee_won_net::ffi::ee_won_utf16le_from_wide(p, WCHAR as u32, count, std::ptr::null_mut(), 0) };
    let mut v = vec![0u8; n];
    if n > 0 {
        // SAFETY: as above, `v` holds the `n` bytes the size pass counted
        unsafe { ee_won_net::ffi::ee_won_utf16le_from_wide(p, WCHAR as u32, count, v.as_mut_ptr(), n) };
    }
    v
}

/// The Rust event of the C++ shell `e` (a lobby event: the login, account and lost-password events).
fn evt(e: isize) -> Option<&'static mut crate::lobbyevt::EeWonLobbyEvt> {
    // SAFETY: a bound event lives until its shell's destructor unbinds it; the host's call holds the shell
    crate::lobbyevt::of(e).map(|p| unsafe { &mut *p })
}

/// The op answered here (`None`: the host's).
pub(crate) fn logic_op(op: Op, a: isize, b: isize, c: isize, d: isize, out: Out) -> Option<isize> {
    let on = |op: Op, x: isize| out(op, x, 0, 0, 0);
    let res = |op: Op, x: Res, y: Res| out(op, x as isize, y as isize, 0, 0);
    let run = |op: isize, t: Res, dt: Res, flags: isize| out(Op::RunWithStatusRes, op, t as isize, dt as isize, flags);
    let r = match op {
        // ---- AccountLogic's (AccountLogic.cpp)
        Op::OutOfDateDialog => res(
            Op::YesNoRes,
            Res::LobbyContainer_VersionOutOfDataTitle_String,
            Res::LobbyContainer_VersionOutOfDataDetails_String,
        ),
        Op::ShowCredits => {
            on(Op::CreditsLCall, Dlg::ResetScrolling as isize);
            on(Op::CreditsLCall, Dlg::StartScrolling as isize);
            on(Op::CreditsDo, 0);
            on(Op::CreditsLCall, Dlg::StopScrolling as isize);
            0
        }
        Op::ShowTOU => {
            on(Op::TouLCall, Dlg::Reset as isize);
            on(Op::TouDo, 0)
        }
        Op::ShowScreen => {
            on(
                match a {
                    1 => Op::ShowCreateAccount,
                    2 => Op::ShowLostPassword,
                    _ => Op::ShowLostUsername,
                },
                0,
            );
            0
        }
        Op::RequestPasswordEmail => {
            // the name's UTF-16 bytes, as WideAppendUtf16LE made them
            let e = evt(a)?;
            let name = e.bytes(F::UserName);
            let bytes = utf16le_of_wide(name.as_ptr() as isize, name.len() / WCHAR);
            let op = out(Op::PasswordEmailNew, bytes.as_ptr() as isize, bytes.len() as isize, b, 0);
            run(
                op,
                Res::LobbyContainer_RetrievePasswordTitle_String,
                Res::LobbyContainer_RetrievePasswordDetails_String,
                0,
            );
            on(Op::Release, op);
            0
        }
        Op::RequestInfoEmail => {
            let email = evt(a)?.bytes(F::Email).to_vec();
            let op = out(Op::InfoEmailNew, email.as_ptr() as isize, email.len() as isize, b, 0);
            run(
                op,
                Res::LobbyContainer_RetrieveUsernameTitle_String,
                Res::LobbyContainer_RetrieveUsernameDetails_String,
                0,
            );
            on(Op::Release, op);
            0
        }
        Op::QueryAccount => {
            let op = out(Op::GetAccountNew, a, b, 0, 0);
            out(Op::OpSetNum, op, ACCOUNT_NEWSLETTER, 1, 0);
            run(
                op,
                Res::LobbyContainer_QueryAccountTitle_String,
                Res::LobbyContainer_QueryAccountDetails_String,
                wait(1),
            );
            op
        }
        Op::UpdateAccount => {
            let newsletter = evt(c)?.get(F::Newsletter) != 0;
            let op = out(Op::UpdateAccountNew, a, b, c, 0);
            out(Op::OpSetNum, op, ACCOUNT_NEWSLETTER, 1 | if newsletter { 0x10000 } else { 0 }, 0);
            run(op, Res::LobbyContainer_UpdateAccountTitle_String, Res::LobbyContainer_UpdateAccountDetails_String, 0);
            on(Op::Release, op);
            0
        }
        Op::CreateAccount => {
            let op = out(Op::CreateAccountNew, a, b, 0, 0);
            run(op, Res::LobbyContainer_CreateAccountTitle_String, Res::LobbyContainer_CreateAccountDetails_String, 0);
            op
        }
        Op::AccountCreated => {
            let user = on(Op::AddUserNamed, a);
            if user != 0 {
                on(Op::UserSetTOUNow, user);
            }
            on(Op::SaveGlobalPrefsNow, 0);
            on(Op::BroadcastLobby, 100052);
            on(Op::BroadcastLoginFields, a);
            0
        }
        Op::LoginName => {
            // the event's own copy, live while the event is
            let e = evt(a)?;
            // SAFETY: the caller's slot
            unsafe { *(b as *mut usize) = e.bytes(F::UserName).len() / WCHAR };
            e.get(F::UserName)
        }
        Op::LoginStripAt => {
            let e = evt(a)?;
            let rest = e.bytes(F::UserName).get(WCHAR..).unwrap_or(&[]).to_vec();
            e.set_bytes(F::UserName, &rest);
            0
        }
        Op::UserNeedsTOU => {
            let user = on(Op::FindUserOfEvent, a);
            if user == 0 {
                1
            } else {
                (on(Op::UserTOUNewer, user) != 0) as isize
            }
        }
        Op::SetCredentials => {
            out(Op::AuthSetUserNameOf, a, b, 0, 0);
            out(Op::AuthSetPasswordOf, a, b, 0, 0);
            0
        }
        Op::InvalidCDKeyBox => {
            res(
                Op::MessageBoxRes,
                Res::AccountLogic_InvalidCDKeyTitle_String,
                Res::AccountLogic_InvalidCDKeyDetails_String,
            );
            0
        }
        Op::SetAdmin => {
            on(if b != 0 { Op::AuthAddAdmin } else { Op::AuthRemoveAdmin }, a);
            0
        }
        Op::GetCert => {
            let op = on(Op::GetCertNew, a);
            run(op, Res::LobbyContainer_LoginTitle_String, Res::LobbyContainer_LoginDetails_String, wait(1));
            op
        }
        Op::LoginSaveUser => {
            let remember = evt(a)?.get(F::Remember) != 0;
            let user = on(if remember { Op::AddUserWithPassword } else { Op::AddUserNoPassword }, a);
            if user != 0 {
                on(Op::UserSetTOUNow, user);
                on(Op::SaveGlobalPrefsNow, 0);
                on(Op::SetCurrentUser, user);
            }
            0
        }

        // ---- InitLogic's (InitLogic.cpp)
        Op::InitStatus => {
            if b != 0 {
                out(Op::InitStatusText, a, b, 0, d);
            } else if c >= 0 {
                out(Op::InitStatusRes, a, 0, c, d);
            } else {
                out(Op::InitStatusEmpty, a, 0, 0, d);
            }
            0
        }
        Op::ClearServerLists => {
            let lists =
                [on(Op::InitListGet, 2), on(Op::InitListGet, 1), on(Op::InitListGet, 3), on(Op::InitListGet, 4)];
            for l in lists {
                if l != 0 {
                    on(Op::ServerClear, l);
                }
            }
            0
        }
        Op::LoadTOUTimes => {
            let system = on(Op::SysTouModifyTime, 0);
            let game = on(Op::GameTouModifyTime, 0);
            out(Op::SetTOUTimes, system, game, 0, 0);
            0
        }
        Op::Music => {
            on(
                match a {
                    0 => Op::MusicSetLooping,
                    1 => Op::MusicPlay,
                    _ => Op::MusicStop,
                },
                0,
            );
            0
        }
        Op::DnsReady => {
            (on(Op::HasConfig, 0) != 0
                && on(Op::DirServers, 0) != 0
                && on(Op::RoomDirServersGet, 0) != 0
                && on(Op::InitListGet, 1) != 0) as isize
        }
        Op::FillDnsServers => {
            on(Op::RememberHostAddress, 0);
            on(Op::DirServersRemoveAll, 0);
            on(Op::AddAllDir, 0);
            on(Op::AddAllRoomDir, 0);
            on(Op::AddAllAuth, 0);
            0
        }
        Op::PromotionImage => {
            let image = on(Op::PromotionDecode, a);
            on(Op::PromotionBroadcast, image);
            if image != 0 {
                on(Op::Release, image);
            }
            0
        }
        Op::FetchPromotionImage => {
            let op = out(Op::HttpGetNew, a, b, 0, 0);
            on(Op::HttpGetSetCache, op);
            on(Op::HttpGetSetPromotionCompletion, op);
            on(Op::SessionAddOp, op);
            on(Op::Release, op);
            0
        }
        Op::DirReady => {
            (on(Op::HasConfig, 0) != 0
                && on(Op::InitListGet, 2) != 0
                && on(Op::InitListGet, 1) != 0
                && on(Op::InitListGet, 3) != 0
                && on(Op::DirServers, 0) != 0) as isize
        }
        Op::AddDirAddresses => {
            let list = on(Op::InitListGet, a);
            if list != 0 {
                out(Op::ServerAddFromDir, list, b, c, 0);
            }
            0
        }
        Op::VersionNotifyFailed => {
            let versions = on(Op::InitListGet, 4);
            if versions != 0 {
                out(Op::ServerNotifyFailed, versions, a, 0, 0);
            }
            0
        }
        Op::RestartAccept => {
            on(Op::AcceptOpNewFrom, a);
            on(Op::AcceptOpRun, 0);
            0
        }
        Op::StartAccept => {
            on(Op::SetFirewallAddress, a);
            on(Op::AcceptOpNewFirewall, 0);
            on(Op::AcceptOpRun, 0);
            on(Op::FirewallSocketDrop, 0);
            0
        }
        Op::RunDnsTest => {
            let op = on(Op::DnsOpNew, 0);
            on(Op::RunTrackedDns, op);
            on(Op::Release, op);
            0
        }
        Op::StartDocuments => {
            if on(Op::HasConfig, 0) != 0 {
                let mut ops = [0isize; 3];
                for (i, kind) in [HTTPDOC_MOTD, HTTPDOC_TOU, HTTPDOC_ADV].into_iter().enumerate() {
                    ops[i] = on(Op::HttpDocNew, kind);
                    on(Op::HttpDocSetConfig, ops[i]);
                    on(Op::HttpDocSetSession, ops[i]);
                    on(Op::RunTrackedDocument, ops[i]);
                }
                // the array's SmartPtrs, destroyed last to first
                for op in ops.into_iter().rev() {
                    on(Op::Release, op);
                }
            }
            0
        }
        Op::RunDirQuery => {
            let count = if on(Op::HasConfig, 0) != 0 { 4 } else { 3 };
            let op = out(Op::DirQueryNew, a, count, 0, 0);
            on(Op::RunTrackedDirectory, op);
            on(Op::Release, op);
            0
        }
        Op::RunVersionCheck => {
            let op = on(Op::VersionCheckNew, 0);
            on(Op::RunTrackedVersion, op);
            on(Op::Release, op);
            0
        }
        Op::RunFirewallDetect => {
            let op = on(Op::FirewallOpNew, a);
            on(Op::FirewallSetListenPort, op);
            out(Op::FirewallSetDoListen, op, 0, 0, 0);
            out(Op::FirewallSetWaitForConnect, op, 0, 0, 0);
            on(Op::RunTrackedFirewall, op);
            on(Op::Release, op);
            0
        }
        Op::KillAcceptOp => {
            if on(Op::HasAcceptOp, 0) != 0 {
                on(Op::AcceptOpKill, 0);
                if on(Op::AcceptOpHasSocket, 0) != 0 {
                    on(Op::AcceptOpCloseSocket, 0);
                }
                on(Op::ClearAcceptOp, 0);
            }
            0
        }
        Op::AdapterDialog => {
            on(Op::AdapterReset, 0);
            on(Op::AdapterDo, 0)
        }
        _ => return None,
    };
    Some(r)
}

/// The lobby's control numbers (lobbyids.rs, from 200001), as the hosts pass them.
const ID_SHOW_CREATE_ACCOUNT: isize = crate::lobbyids::ID_ShowCreateAccount as isize;
const ID_SHOW_LOST_PASSWORD: isize = crate::lobbyids::ID_ShowLostPassword as isize;
const ID_SHOW_LOST_USER_NAME: isize = crate::lobbyids::ID_ShowLostUserName as isize;
const ID_LOGIN_SCREEN_BACK: isize = crate::lobbyids::ID_LoginScreenBack as isize;
const ID_SHOW_INTERNET_SCREEN: isize = crate::lobbyids::ID_ShowInternetScreen as isize;
const ID_CHANGE_ROOM: isize = crate::lobbyids::ID_ChangeRoom as isize;
const ID_REFRESH_GAME_LIST: isize = crate::lobbyids::ID_RefreshGameList as isize;
const ID_ADD_FRIEND: isize = crate::lobbyids::ID_AddFriend as isize;
const ID_REFRESH_FRIENDS: isize = crate::lobbyids::ID_RefreshFriends as isize;
const ID_FILTER_DIRTY_WORDS_CHECK: isize = crate::lobbyids::ID_FilterDirtyWordsCheck as isize;
const ID_ANONYMOUS_TO_FRIENDS_CHECK: isize = crate::lobbyids::ID_AnonymousToFriendsCheck as isize;
const ID_CHOOSE_NETWORK_ADAPTER: isize = crate::lobbyids::ID_ChooseNetworkAdapter as isize;

/// AccountLogic::HandleControlEvent's choice: 1 create account, 2 lost password, 3 lost user name, 0 none.
pub(crate) fn account_control_of(id: isize) -> isize {
    match id {
        ID_SHOW_CREATE_ACCOUNT => 1,
        ID_SHOW_LOST_PASSWORD => 2,
        ID_SHOW_LOST_USER_NAME => 3,
        _ => 0,
    }
}

/// InitLogic::HandleControlEvent's choice: 1 back, 2 the Internet screen, 3 the network adapter, 0 none.
pub(crate) fn init_control_of(id: isize) -> isize {
    match id {
        ID_LOGIN_SCREEN_BACK => 1,
        ID_SHOW_INTERNET_SCREEN => 2,
        ID_CHOOSE_NETWORK_ADAPTER => 3,
        _ => 0,
    }
}

/// RoutingLogic::HandleControlEvent's choice: 1 change room, 2 add friend, 3 anonymous, 4 dirty words, 5 refresh
/// friends, 6 refresh the game list, 0 none.
pub(crate) fn route_control_of(id: isize) -> isize {
    match id {
        ID_CHANGE_ROOM => 1,
        ID_ADD_FRIEND => 2,
        ID_ANONYMOUS_TO_FRIENDS_CHECK => 3,
        ID_FILTER_DIRTY_WORDS_CHECK => 4,
        ID_REFRESH_FRIENDS => 5,
        ID_REFRESH_GAME_LIST => 6,
        _ => 0,
    }
}

/// The logics' control maps for the drivers (won_lobbyops_test): `which` 0 account, 1 init, 2 routing.
#[no_mangle]
pub extern "C" fn ee_won_lobby_netlogic_control_of(which: i32, id: isize) -> isize {
    match which {
        0 => account_control_of(id),
        1 => init_control_of(id),
        _ => route_control_of(id),
    }
}

/// [`logic_op`] for the drivers (won_lobbyops_test): the moved op `op` of the logic `obj` composed over `host`;
/// an op not answered here goes to `host` as it is.
///
/// # Safety
/// `host` is a live host for `obj`; the ops' arguments are what the logic would pass.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lobby_netlogic_op(
    obj: *mut std::ffi::c_void,
    op: crate::logics::EeWonLogicOp,
    a: isize,
    b: isize,
    c: isize,
    d: isize,
    host: crate::logics::EeWonLogicHostFn,
) -> isize {
    let out = |o: crate::logics::EeWonLogicOp, a: isize, b: isize, c: isize, d: isize| match host {
        // SAFETY: the contract
        Some(h) => unsafe { h(obj, o, a, b, c, d) },
        None => 0,
    };
    logic_op(op, a, b, c, d, &out).unwrap_or_else(|| out(op, a, b, c, d))
}

// ---- RoutingLogic's (RoutingLogic.cpp), in its model mode (the game's)

/// The routing connection's notices: the op type and `RoutingOpCompletion`'s code (routelogic.rs N_*), in the
/// original's order.
const NOTICES: [(isize, isize); 20] = [
    (8, 1),
    (10, 2),
    (7, 3),
    (9, 4),
    (39, 5),
    (40, 6),
    (6, 7),
    (36, 8),
    (24, 9),
    (25, 10),
    (23, 11),
    (13, 12),
    (14, 13),
    (27, 14),
    (28, 15),
    (58, 16),
    (59, 17),
    (48, 18),
    (50, 19),
    (49, 20),
];
const LOBBY_EVENT_CREATE_ROOM: isize = 100011;
const LOBBY_EVENT_JOIN_ROOM: isize = 100028;
const LOBBY_EVENT_QUERY_SERVER_GROUPS: isize = 100042;

/// RoutingLogic's moved op `op` with its arguments `g` (`a`..`e`), answered here (`None`: the host's). `out` is the
/// logic's own dispatch (its model's answers, then the host).
pub(crate) fn route_op(op: ROp, g: [isize; 5], out: ROut) -> Option<isize> {
    let [a, b, c, d, e] = g;
    let r = match op {
        ROp::ConnDo => match b {
            1 => {
                out(ROp::ConnSetAuth, &[a, c]);
                0
            }
            2 => {
                for (kind, code) in NOTICES {
                    out(ROp::ConnSetNotice, &[a, kind, code]);
                }
                0
            }
            3 => {
                out(ROp::ConnSetClose, &[a]);
                0
            }
            4 => {
                out(ROp::ConnSubscribeGames, &[a]);
                0
            }
            5 => out(ROp::ConnIsConnected, &[a]),
            _ => return None,
        },
        ROp::UserIdLookup => {
            // GetUserIdFromNameOp: the name's UTF-16 bytes, the op behind the add-friend status box, its id read back
            let n = out(ROp::TextUnits, &[b, 0]).max(0) as usize;
            let mut units = vec![0u32; n];
            if n != 0 {
                out(ROp::TextUnits, &[b, units.as_mut_ptr() as isize]);
            }
            let name = crate::routelogic::utf16le(&units);
            let op = out(ROp::UserIdOpNew, &[name.as_ptr() as isize, name.len() as isize, a]);
            let res = |r: Res| r as isize;
            out(
                ROp::RunWithStatus,
                &[op, res(Res::AddFriendDialog_StatusTitle_String), res(Res::AddFriendDialog_StatusDetails_String), 1],
            );
            let id = out(ROp::OpUserId, &[op]);
            let status = out(ROp::OpStatus, &[op]) as i32;
            // SAFETY: the logic's status slot, live for the call
            unsafe { *(c as *mut i32) = status };
            out(ROp::Release, &[op]);
            id
        }
        ROp::DirOpRun => {
            let op = out(ROp::DirOpNew, &[a]);
            let res = |r: Res| r as isize;
            out(
                ROp::RunWithStatus,
                &[
                    op,
                    res(Res::LobbyContainer_GettingRoomsTitle_String),
                    res(Res::LobbyContainer_GettingRoomsDetails_String),
                    1,
                ],
            );
            op
        }
        ROp::ConnAdvertise => {
            let buffer = out(ROp::BufNew, &[]);
            out(ROp::GameWriteSummary, &[b, buffer]);
            out(ROp::ConnCreateGameObject, &[a, buffer, c]);
            out(ROp::BufFree, &[buffer]);
            0
        }
        ROp::EventServer => match out(ROp::EventType, &[a]) {
            LOBBY_EVENT_CREATE_ROOM | LOBBY_EVENT_JOIN_ROOM | LOBBY_EVENT_QUERY_SERVER_GROUPS => {
                out(ROp::EventFindServer, &[a])
            }
            _ => 0,
        },
        ROp::StatusOpSetMap => {
            // the requested ids as the original's std::map (sorted, once each, status 0), set over the cleared map
            let ids: std::collections::BTreeSet<u32> = if b == 0 || c <= 0 {
                Default::default()
            } else {
                // SAFETY: the logic passes `c` ids at `b`, live for the call
                unsafe { std::slice::from_raw_parts(b as *const u32, c as usize) }.iter().copied().collect()
            };
            let status = out(ROp::StatusOpRust, &[a]) as *mut ee_won_net::userstatus::EeWonUserStatus;
            // SAFETY: the op's own Rust object, live while the op is
            unsafe {
                ee_won_net::userstatus::ee_won_userstatus_clear(status);
                for id in ids {
                    ee_won_net::userstatus::ee_won_userstatus_put(status, id as u64, 0);
                }
            }
            0
        }
        ROp::StatusOpMap => {
            // the map's copy (std::map: sorted, the last status of an id kept), each id then its status pushed
            let status = out(ROp::StatusOpRust, &[a]) as *const ee_won_net::userstatus::EeWonUserStatus;
            let mut map: std::collections::BTreeMap<std::ffi::c_ulong, u8> = Default::default();
            unsafe extern "C" fn take(ctx: *mut std::ffi::c_void, id: u64, status: u8) {
                // SAFETY: the map below, for the call
                unsafe {
                    (*(ctx as *mut std::collections::BTreeMap<std::ffi::c_ulong, u8>))
                        .insert(id as std::ffi::c_ulong, status)
                };
            }
            // SAFETY: the op's own Rust object; the sink writes the local map
            unsafe {
                ee_won_net::userstatus::ee_won_userstatus_each(
                    status,
                    take,
                    &mut map as *mut _ as *mut std::ffi::c_void,
                )
            };
            for (id, s) in map {
                // SAFETY: the logic's sink, live for the call
                unsafe {
                    crate::routelogic::ee_won_routelogic_push(b as *mut std::ffi::c_void, id as u64);
                    crate::routelogic::ee_won_routelogic_push(b as *mut std::ffi::c_void, s as u64);
                }
            }
            0
        }
        ROp::EvModerated => {
            let kind = if d & 0x100 != 0 { ROp::EvModeratedGroup } else { ROp::EvModeratedServer };
            out(kind, &[a, b, c, d & 0xff, e]);
            0
        }
        ROp::ListFree => {
            out(if b == 2 { ROp::DirListFree } else { ROp::IterFree }, &[a]);
            0
        }
        ROp::EvAlert => {
            out(if a == 1 { ROp::EvAlertTimed } else { ROp::EvAlertPlain }, &[a, b]);
            0
        }
        ROp::DirEntities => {
            let list = out(ROp::DirEntitiesNew, &[a]);
            let empty = out(ROp::DirListEmpty, &[list]);
            // SAFETY: the logic's slot, live for the call
            unsafe { *(b as *mut isize) = empty };
            list
        }
        ROp::GroupQueryRun => {
            let op = out(ROp::GroupQueryNew, &[a]);
            out(ROp::RunTrackedGroups, &[op]);
            out(ROp::Release, &[op]);
            0
        }
        ROp::EventNum => {
            if b == 13 {
                out(ROp::EventComponent, &[a])
            } else {
                0
            }
        }
        ROp::Pref => match a {
            8 => out(ROp::OnInternetScreen, &[]),
            9 => out(ROp::UserCanceledLast, &[]),
            _ => 0,
        },
        _ => return None,
    };
    Some(r)
}

/// [`route_op`] for the drivers (won_lobbyops_test): the moved op `op` of the logic `obj` composed over `host`;
/// an op not answered here goes to `host` as it is.
///
/// # Safety
/// `host` is a live host for `obj`; the ops' arguments are what the logic would pass.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lobby_netlogic_route_op(
    obj: *mut std::ffi::c_void,
    op: crate::routelogic::EeWonRouteLogicOp,
    a: isize,
    b: isize,
    c: isize,
    d: isize,
    e: isize,
    host: crate::routelogic::EeWonRouteLogicHostFn,
) -> isize {
    let out = |o: crate::routelogic::EeWonRouteLogicOp, v: &[isize]| {
        let g = |i: usize| v.get(i).copied().unwrap_or(0);
        // SAFETY: the contract
        unsafe { host(obj, o, g(0), g(1), g(2), g(3), g(4)) }
    };
    route_op(op, [a, b, c, d, e], &out).unwrap_or_else(|| out(op, &[a, b, c, d, e]))
}

/// A sink `ee_won_routelogic_push` fills (a `Vec<u64>`), for the drivers (won_lobbyops_test).
#[no_mangle]
pub extern "C" fn ee_won_lobby_netlogic_sink_new() -> *mut std::ffi::c_void {
    Box::into_raw(Box::new(Vec::<u64>::new())) as *mut std::ffi::c_void
}

/// The sink's values (up to `capacity` written to `out`), then the sink freed; returns the count.
///
/// # Safety
/// `sink` came from [`ee_won_lobby_netlogic_sink_new`]; `out` holds `capacity` values.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lobby_netlogic_sink_take(
    sink: *mut std::ffi::c_void,
    out: *mut u64,
    capacity: usize,
) -> usize {
    // SAFETY: the contract
    let v = unsafe { Box::from_raw(sink as *mut Vec<u64>) };
    for (i, x) in v.iter().take(capacity).enumerate() {
        // SAFETY: the contract
        unsafe { *out.add(i) = *x };
    }
    v.len()
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::cell::RefCell;

    fn run(op: Op, a: isize, answer: isize) -> (Option<isize>, Vec<(Op, isize)>) {
        let log = RefCell::new(Vec::new());
        let out = |o: Op, a: isize, _b: isize, _c: isize, _d: isize| {
            log.borrow_mut().push((o, a));
            answer
        };
        let r = logic_op(op, a, 0, 0, 0, &out);
        (r, log.into_inner())
    }

    #[test]
    fn the_kill_closes_a_socket_only_when_there_is_one() {
        let (r, log) = run(Op::KillAcceptOp, 0, 1);
        assert_eq!(r, Some(0));
        let ops: Vec<Op> = log.iter().map(|x| x.0).collect();
        assert_eq!(
            ops,
            [Op::HasAcceptOp, Op::AcceptOpKill, Op::AcceptOpHasSocket, Op::AcceptOpCloseSocket, Op::ClearAcceptOp]
        );
        let (_, log) = run(Op::KillAcceptOp, 0, 0);
        assert_eq!(log.len(), 1);
    }

    #[test]
    fn the_documents_are_released_last_to_first() {
        let (_, log) = run(Op::StartDocuments, 0, 7);
        assert_eq!(log.len(), 1 + 3 * 4 + 3);
        assert!(log[13..].iter().all(|x| x.0 == Op::Release));
        let (_, log) = run(Op::StartDocuments, 0, 0);
        assert_eq!(log, [(Op::HasConfig, 0)]);
    }

    #[test]
    fn the_control_maps() {
        assert_eq!(account_control_of(200002), 2);
        assert_eq!(account_control_of(200004), 0);
        assert_eq!(init_control_of(200023), 3);
        assert_eq!(route_control_of(200016), 6);
        assert_eq!(route_control_of(200013), 0);
        assert_eq!(ee_won_lobby_netlogic_control_of(1, 200006), 1);
    }

    #[test]
    fn the_routing_notices_and_the_alerts() {
        let log = RefCell::new(Vec::new());
        let out = |o: ROp, v: &[isize]| {
            log.borrow_mut().push((o, v.to_vec()));
            0
        };
        assert_eq!(route_op(ROp::ConnDo, [5, 2, 0, 0, 0], &out), Some(0));
        assert_eq!(log.borrow().len(), 20);
        assert_eq!(log.borrow()[0], (ROp::ConnSetNotice, vec![5, 8, 1]));
        assert_eq!(log.borrow()[19], (ROp::ConnSetNotice, vec![5, 49, 20]));
        log.borrow_mut().clear();
        assert_eq!(route_op(ROp::ConnDo, [5, 6, 0, 0, 0], &out), None);
        route_op(ROp::EvAlert, [1, 9, 0, 0, 0], &out);
        route_op(ROp::EvModerated, [1, 2, 3, 0x105, 7], &out);
        assert_eq!(*log.borrow(), [(ROp::EvAlertTimed, vec![1, 9]), (ROp::EvModeratedGroup, vec![1, 2, 3, 5, 7])]);
    }

    #[test]
    fn an_op_not_moved_is_the_hosts() {
        assert_eq!(run(Op::OpStatus, 0, 0).0, None);
        assert_eq!(run(Op::ShowScreen, 2, 0).1, [(Op::ShowLostPassword, 0)]);
    }
}
