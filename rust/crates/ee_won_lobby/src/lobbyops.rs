//! The logics' host cases whose statements are Rust (R6 WONLobby round 29): an op StagingLogic's and LanLogic's
//! Rust objects send to the C++ host (LobbyLogicHost.cpp, LanLogicHost.cpp) that was a run of C++ statements with
//! decisions between them is answered here in the game; each C++ statement is a one-statement case of the host, or
//! an op netops.rs answers over the network objects' Rust (`OpStatusOf`, `SocketListen`, the new socket ops).
//! Reached from lobbyhost.rs's `Host::op` in the game, after netops; the drivers' scripted hosts never reach it, so
//! the logics' goldens see the ops as before. won_lobbyops_test runs the deleted cases (as they were) against these
//! over one stand-in world.

use crate::lobbyhost::EeWonLobbyOp as Op;
use crate::resource::EeWonLobbyRes as Res;
use std::ffi::c_void;
use std::ptr::null_mut;

/// The calls out: the op through the same dispatch the moved op came by.
pub(crate) type Out<'a> = &'a dyn Fn(Op, *mut c_void, *mut c_void, isize, isize) -> isize;

const WS_SUCCESS: isize = 0;

/// A box's or status wait's two resource rows as the host's `c` (`MessageBoxRows`, `RunOpWithStatusRows`).
fn rows(title: Res, details: Res) -> isize {
    title as isize | (details as isize) << 16
}

fn p(v: isize) -> *mut c_void {
    v as *mut c_void
}

/// [`lobby_op`] in the game only: while the network core is bound (as netops.rs's answers; the model-mode tests
/// and the drivers' scripted hosts never bind it).
pub(crate) fn game_lobby_op(op: Op, a: *mut c_void, b: *mut c_void, c: isize, d: isize, out: Out) -> Option<isize> {
    if !ee_won_net::cppnet::bound() {
        return None;
    }
    lobby_op(op, a, b, c, d, out)
}

/// The op answered here (`None`: the host's).
pub(crate) fn lobby_op(op: Op, a: *mut c_void, b: *mut c_void, c: isize, d: isize, out: Out) -> Option<isize> {
    let on = |op: Op, x: *mut c_void| out(op, x, null_mut(), 0, 0);
    let r = match op {
        // ---- StagingLogic's (LobbyLogicHost.cpp)
        Op::StJoinOpFinish => {
            if on(Op::StJoinOpIs, null_mut()) != 0 {
                on(Op::StJoinOpFinishSuccess, null_mut());
            }
            0
        }
        Op::StJoinOpRun => {
            let r = rows(
                Res::GameStagingLogic_GettingGameInfoTitle_String,
                Res::GameStagingLogic_GettingGameInfoDetails_String,
            );
            out(Op::RunOpWithStatusRows, p(on(Op::StJoinOpGet, null_mut())), null_mut(), r, 0);
            on(Op::StJoinOpClear, null_mut());
            0
        }
        Op::StJoinOpKill => {
            if on(Op::StJoinOpIs, null_mut()) != 0 {
                on(Op::StJoinOpKillNow, null_mut());
                on(Op::StJoinOpClear, null_mut());
            }
            0
        }
        Op::StLogoutTail => {
            on(Op::StCreateDialogResetAll, null_mut());
            on(Op::NetLogout, null_mut());
            out(Op::KillPings, null_mut(), null_mut(), 0, 0);
            on(Op::ShowLoginScreen, null_mut());
            on(Op::SaveGlobalPrefs, null_mut());
            on(Op::SaveUserPrefs, null_mut());
            on(Op::LogonSoundStop, null_mut());
            0
        }
        Op::GameLeaveNet => {
            on(Op::GameClearPlayers, a);
            on(Op::StClearMyClient, null_mut());
            on(Op::GamePreNetLeave, a);
            on(Op::NetLeaveGame, a);
            on(Op::GamePostNetLeave, a);
            0
        }
        Op::GameSetInProgressNet => {
            out(Op::GameSetInProgress, a, null_mut(), (c != 0) as isize, 0);
            out(Op::NetSetGameInProgress, null_mut(), null_mut(), (c != 0) as isize, 0);
            on(Op::NetAdvertiseGame, null_mut());
            0
        }
        Op::GamePingAndNotify => {
            on(Op::LanPingGameNotify, a);
            out(Op::LanGameChanged, a, null_mut(), 2, 0);
            0
        }
        Op::GameShowDetails => {
            on(Op::QueryGameDetails, a);
            out(Op::GameSetShowDetails, a, null_mut(), 1, 0);
            out(Op::LanGameChanged, a, null_mut(), 2, 0);
            0
        }
        Op::GameHideDetails => {
            out(Op::GameSetShowDetails, a, null_mut(), 0, 0);
            out(Op::LanGameChanged, a, null_mut(), 2, 0);
            0
        }
        Op::AllowJoinWithoutPing => {
            let config = on(Op::LobbyConfigGet, null_mut());
            if config == 0 {
                -1
            } else {
                on(Op::ConfigAllowJoinWithoutPing, p(config))
            }
        }
        // ---- LanLogic's (LanLogicHost.cpp)
        Op::LanAuthAutoRefreshOff => {
            let auth = on(Op::AuthGet, null_mut());
            if auth != 0 {
                out(Op::AuthSetAutoRefresh, p(auth), null_mut(), 0, 0);
            }
            0
        }
        Op::LanWhisperReceived => {
            if on(Op::ChatSoundEffects, null_mut()) != 0 {
                on(Op::PlayWhisperSound, null_mut());
            }
            let parser = on(Op::ChatParser, null_mut());
            if parser != 0 {
                out(Op::ParserSetInputName, p(parser), a, 0, 0);
            }
            0
        }
        Op::LanNameDialogSave => {
            on(Op::LanSetLanNameFromDialog, null_mut());
            out(Op::BroadcastLobbyEvent, null_mut(), null_mut(), 0x186d2, 0);
            0
        }
        Op::LanSetBroadcastPort => {
            out(Op::LanSetBroadcastPortTo, null_mut(), null_mut(), c as i32 as isize, 0);
            out(Op::BroadcastLobbyEvent, null_mut(), null_mut(), 0x186d2, 0);
            0
        }
        // a fresh login certificate (anonymous) unless the current one is valid; false on a bad CD key or failure
        Op::LanRefreshCertificate => {
            let auth = p(on(Op::AuthGet, null_mut()));
            if auth.is_null() {
                return Some(0);
            }
            if on(Op::CdKeyValid, null_mut()) == 0 {
                let r = rows(Res::AccountLogic_InvalidCDKeyTitle_String, Res::AccountLogic_InvalidCDKeyDetails_String);
                out(Op::MessageBoxRows, null_mut(), null_mut(), r, 0);
                return Some(0);
            }
            if out(Op::AuthIsExpired, auth, null_mut(), 120, 0) == 0 {
                out(Op::AuthSetAutoRefresh, auth, null_mut(), 1, 0);
                return Some(1);
            }
            on(Op::AuthClearUserName, auth);
            on(Op::AuthClearPassword, auth);
            let cert = p(on(Op::GetCertNew, auth));
            let r = rows(Res::LobbyContainer_LoginTitle_String, Res::LobbyContainer_LoginDetails_String);
            out(Op::RunOpWithStatusRows, cert, null_mut(), r, 0);
            // (the op's reference is given back at the end, as the original's SmartPtr's)
            if on(Op::OpStatusOf, cert) != WS_SUCCESS {
                on(Op::RefRelease, cert);
                return Some(0);
            }
            out(Op::AuthSetAutoRefresh, auth, null_mut(), 1, 0);
            on(Op::RefRelease, cert);
            1
        }
        Op::LanStartPeerAuth => {
            let auth = on(Op::AuthGet, null_mut());
            if auth != 0 {
                out(Op::LanRunPeerAuth, a, p(auth), 0, 0);
            }
            0
        }
        // LanLogic::HandlePeerAuthDone (the host): a peer-authenticated socket whose certificate has the lobby's
        // community is accepted, another closed
        Op::LanPeerAuthDone => {
            let socket = on(Op::PeerAuthSocket, a);
            if on(Op::OpSucceededOf, a) == 0 {
                out(Op::LanSocketClose, p(socket), null_mut(), 1000, 0);
                return Some(0);
            }
            let auth = on(Op::AuthGet, null_mut());
            if auth == 0 {
                return Some(0);
            }
            let community = on(Op::AuthCommunity, p(auth));
            if out(Op::PeerAuthPermits, a, null_mut(), community as u32 as isize, 0) == 0 {
                out(Op::LanSocketClose, p(socket), null_mut(), 1000, 0);
                return Some(0);
            }
            socket
        }
        Op::LanListen => {
            let status = on(Op::LanBindLobbyPort, a);
            if status != 0 {
                out(Op::ErrorDialogRow, null_mut(), null_mut(), Res::LANLogic_SocketBindError_String as isize, status);
                return Some(0);
            }
            let status = out(Op::SocketListen, a, null_mut(), 1000, 0);
            if status != 0 {
                out(
                    Op::ErrorDialogRow,
                    null_mut(),
                    null_mut(),
                    Res::LANLogic_SocketListenError_String as isize,
                    status,
                );
                return Some(0);
            }
            on(Op::LanArmAcceptCompletion, a);
            on(Op::LanArmAcceptOp, a);
            1
        }
        // port: the discovery port is bound with address reuse (the original bound it exclusively), so that two
        // lobbies on one machine both receive the broadcasts
        Op::LanNewDiscovery => {
            on(Op::LanBroadcastAddrAssign, a);
            let socket = on(Op::LanNewQueueSocket, null_mut());
            let status = on(Op::LanBindBroadcast, p(socket));
            // SAFETY: the caller's status slot (lobbyhost.rs documents `b`)
            unsafe { *(b as *mut isize) = status };
            socket
        }
        Op::LanArmDiscovery => {
            on(Op::LanArmDiscoveryCompletion, a);
            on(Op::LanArmDiscoveryOp, a);
            0
        }
        Op::LanConnectTo => {
            let connect = if c != 0 {
                let auth = on(Op::AuthGet, null_mut());
                out(Op::LanNewPeerAuthConnect, a, b, auth, 0)
            } else {
                out(Op::LanNewConnect, a, b, 0, 0)
            };
            let r = rows(Res::LANLogic_ConnectToGameTitle_String, Res::LANLogic_ConnectToGameDetails_String);
            out(Op::RunOpWithStatusRows, p(connect), null_mut(), r, 0);
            let ok = on(Op::OpStatusOf, p(connect)) == WS_SUCCESS;
            on(Op::RefRelease, p(connect));
            ok as isize
        }
        Op::LanRecvReply => {
            let reply = p(on(Op::LanNewRecvMsg, a));
            let r = rows(Res::LANLogic_RegisterTitle_String, Res::LANLogic_RegisterDetails_String);
            out(Op::RunOpWithStatusRows, reply, null_mut(), r, 0);
            let result = if on(Op::OpStatusOf, reply) != WS_SUCCESS {
                -1
            } else {
                (out(Op::LanRecvMsgTake, reply, b, 0, 0) != 0) as isize
            };
            on(Op::RefRelease, reply);
            result
        }
        Op::LanArmSocket => {
            on(Op::LanArmCloseCompletion, a);
            on(Op::LanArmRecvCompletion, a);
            on(Op::LanArmRecvOp, a);
            0
        }
        Op::LanDirectSave => {
            let length = out(Op::LanDirectInputLength, null_mut(), null_mut(), 0, 0);
            let field = crate::lanlogic::ee_won_lanlogic_direct_name_is_address(length as usize) as isize;
            out(Op::GameSetDirectName, a, null_mut(), field, 0);
            out(Op::AddDirectConnectNamed, null_mut(), b, field, 0);
            0
        }
        _ => return None,
    };
    let _ = d;
    Some(r)
}

/// The drivers' entry: `lobby_op` with the host `host` for the calls out, made with the logic `logic`
/// (`INTPTR_MIN`: not an op answered here).
///
/// # Safety
/// `host` answers the ops (a driver's world); `b` is a status slot for `LanNewDiscovery`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lobby_lobbyops_op(
    logic: *mut c_void,
    op: crate::lobbyhost::EeWonLobbyOp,
    a: *mut c_void,
    b: *mut c_void,
    c: isize,
    d: isize,
    host: crate::lobbyhost::EeWonLobbyHostFn,
) -> isize {
    let out = |op: Op, a: *mut c_void, b: *mut c_void, c: isize, d: isize| match host {
        // SAFETY: the contract
        Some(f) => unsafe { f(logic, op, a, b, c, d) },
        None => 0,
    };
    lobby_op(op, a, b, c, d, &out).unwrap_or(isize::MIN)
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::cell::RefCell;

    fn run(op: Op, a: isize, c: isize, answer: &dyn Fn(Op) -> isize) -> (Option<isize>, Vec<String>) {
        let log = RefCell::new(Vec::new());
        let out = |op: Op, a: *mut c_void, b: *mut c_void, c: isize, _: isize| {
            log.borrow_mut().push(format!("{op:?} {:#x} {:#x} {c:#x}", a as isize, b as isize));
            answer(op)
        };
        let r = lobby_op(op, a as *mut c_void, null_mut(), c, 0, &out);
        (r, log.into_inner())
    }

    #[test]
    fn the_certificate_refresh() {
        // no context
        assert_eq!(run(Op::LanRefreshCertificate, 0, 0, &|_| 0), (Some(0), vec!["AuthGet 0x0 0x0 0x0".to_string()]));
        // valid and not expired: auto refresh on
        let (r, ops) = run(Op::LanRefreshCertificate, 0, 0, &|op| if op == Op::AuthIsExpired { 0 } else { 0x40 });
        assert_eq!(r, Some(1));
        assert_eq!(ops.last().unwrap(), "AuthSetAutoRefresh 0x40 0x0 0x1");
        // expired: a new certificate, failed
        let (r, ops) = run(Op::LanRefreshCertificate, 0, 0, &|op| if op == Op::OpStatusOf { -5 } else { 0x40 });
        assert_eq!(r, Some(0));
        assert_eq!(ops.len(), 9);
        assert_eq!(ops[8], "RefRelease 0x40 0x0 0x0");
        let (r, ops) = run(Op::LanRefreshCertificate, 0, 0, &|op| if op == Op::OpStatusOf { 0 } else { 0x40 });
        assert_eq!(r, Some(1));
        assert_eq!(ops[8], "AuthSetAutoRefresh 0x40 0x0 0x1");
        assert_eq!(ops[9], "RefRelease 0x40 0x0 0x0");
    }

    #[test]
    fn the_listen_and_its_errors() {
        let (r, ops) = run(Op::LanListen, 0x10, 0, &|op| if op == Op::LanBindLobbyPort { 7 } else { 0 });
        assert_eq!(r, Some(0));
        assert_eq!(ops[1], format!("ErrorDialogRow 0x0 0x0 {:#x}", Res::LANLogic_SocketBindError_String as isize));
        let (r, ops) = run(Op::LanListen, 0x10, 0, &|_| 0);
        assert_eq!(r, Some(1));
        assert_eq!(ops.len(), 4);
        assert_eq!(run(Op::StGame, 0, 0, &|_| 0).0, None);
    }
}
