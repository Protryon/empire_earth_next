//! The logics' host cases whose statements are Rust (R6 WONLobby round 29): an op StagingLogic's and LanLogic's
//! Rust objects send to the C++ host (LobbyLogicHost.cpp, LanLogicHost.cpp) that was a run of C++ statements with
//! decisions between them is answered here in the game; each C++ statement is a one-statement case of the host, or
//! an op netops.rs answers over the network objects' Rust (`OpStatusOf`, `SocketListen`, the new socket ops).
//! Reached from lobbyhost.rs's `Host::op` in the game, after netops; the drivers' scripted hosts never reach it, so
//! the logics' goldens see the ops as before. won_lobbyops_test runs the deleted cases (as they were) against these
//! over one stand-in world.

use crate::lobbyhost::EeWonLobbyOp as Op;
use crate::resource::EeWonLobbyRes as Res;
use ee_won_net::ipobj::EeWonIpAddr;
use std::ffi::c_void;
use std::ptr::null_mut;

/// The calls out: the op through the same dispatch the moved op came by.
pub(crate) type Out<'a> = &'a dyn Fn(Op, *mut c_void, *mut c_void, isize, isize) -> isize;

const WS_SUCCESS: isize = 0;
/// A read past its buffer's end (the host's `EeWonLobbyResult::Threw`).
const THREW: isize = crate::lobbyhost::EeWonLobbyResult::Threw as isize;

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
        // ---- the cases composed over one-statement cases (R6 WON round 32)
        Op::LanNewClient => {
            let client = on(Op::LanClientAlloc, null_mut());
            out(Op::LanClientSetName, p(client), null_mut(), c, d);
            client
        }
        Op::LanPortDialogRun => {
            on(Op::LanPortSetGamePort, null_mut());
            out(Op::LanPortSetPort, null_mut(), null_mut(), c, 0);
            on(Op::LanPortReset, null_mut());
            on(Op::LanPortDo, null_mut())
        }
        Op::LanNewTimer => {
            let timer = on(Op::LanTimerAlloc, null_mut());
            out(Op::LanTimerSetCompletion, p(timer), null_mut(), c, 0);
            timer
        }
        Op::StCreateDialogRun => {
            out(Op::StCreateSetType, null_mut(), null_mut(), c, 0);
            on(Op::StCreateReset, null_mut());
            on(Op::StCreateDo, null_mut())
        }
        // the dialog's game (held: the original's local SmartPtr's reference handed out) with this machine's address
        Op::StTakeDialogGame => {
            let game = on(Op::StDialogGameHeld, null_mut());
            out(Op::GameSetMyAddress, p(game), null_mut(), c, 0);
            on(Op::StSetGame, p(game));
            game
        }
        Op::GameNewCopy => {
            let game = on(Op::GameCreateHeld, a);
            out(Op::GameCopyFrom, p(game), a, 0, 0);
            game
        }
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
        // ---- R6 WON lobby and network round 36: the hosts' last compound cases
        // LobbyLogicHost.cpp's: every client's player made not ready, each change broadcast
        Op::GameResetReadyFlags => {
            let iter = on(Op::GameClientsIterNew, a);
            while on(Op::ClientsIterHasMore, p(iter)) != 0 {
                let mut client: *mut c_void = null_mut();
                let player = out(Op::ClientsIterNext, p(iter), &mut client as *mut *mut c_void as *mut c_void, 0, 0);
                if player != 0 && !client.is_null() {
                    out(Op::PlayerSetReady, p(player), null_mut(), 0, 0);
                    out(Op::ClientChanged, client, null_mut(), 2, 0);
                }
            }
            on(Op::ClientsIterFree, p(iter));
            0
        }
        // the game's join reply for the client `d` (read from `b`), sent to it: 1 sent, 0 none, Threw
        Op::GameJoinReplySend => {
            let reply = out(Op::GameJoinReplyHeld, a, b, 0, d);
            if reply == THREW || reply == 0 {
                return Some(reply);
            }
            out(Op::StSendToClientBuf, p(reply), p(d), 0, 0);
            on(Op::RefRelease, p(reply));
            1
        }
        // a new player read from `b` and set on the client `a`: 1, 0 when the read failed, Threw
        Op::ClientReadPlayer => {
            let player = on(Op::PlayerCreateHeld, null_mut());
            let read = out(Op::PlayerReadFrom, p(player), b, 0, 0);
            if read != THREW && read != 0 {
                out(Op::ClientSetPlayer, a, p(player), 0, 0);
            }
            on(Op::RefRelease, p(player));
            match read {
                THREW => THREW,
                0 => 0,
                _ => 1,
            }
        }
        // the player-joined message of the client `a` (its head, then the player) broadcast to the game
        Op::ClientBroadcastJoined => {
            let id = on(Op::ClientId, a) as u16;
            let head =
                crate::staging::room_message(&crate::staging::RoomMsg { kind: 3, id, flag: false, ping: 0, rest: 0 });
            let buffer = on(Op::WbufNew, null_mut());
            out(Op::WbufAppend, p(buffer), null_mut(), head.as_ptr() as isize, head.len() as isize);
            out(Op::ClientWritePlayer, a, p(buffer), 0, 0);
            on(Op::StBroadcastWbuf, p(buffer));
            on(Op::WbufFree, p(buffer));
            0
        }
        // a new op run behind the status box with the texts `c` (the text code) in the mode `d` for half a second:
        // whether it was killed (the box cancelled)
        Op::RunWaitOp => {
            let wait = on(Op::LanTimerAlloc, null_mut());
            out(Op::RunOpWithStatusMode, p(wait), null_mut(), crate::lobbyhost::lobby_text(c), d);
            let killed = on(Op::OpIsKilled, p(wait));
            on(Op::RefRelease, p(wait));
            killed
        }
        // LanLogicHost.cpp's: the third and fourth address bytes of the socket's peer and of this machine
        Op::LanSubnets => on(Op::LanPeerSubnet, a) | on(Op::LanLocalSubnet, null_mut()) << 16,
        // a staging message past its type byte to the staging logic (nothing when the byte is missing: the
        // original's read threw and was caught)
        Op::LanStagingRead => {
            if d >= 1 {
                out(Op::LanStagingBytes, null_mut(), b, c + 1, d - 1);
            }
            0
        }
        // the game's summary appended to the sink `b`
        Op::LanSummaryTake => {
            let buffer = on(Op::WbufNew, null_mut());
            out(Op::GameWriteSummary, a, p(buffer), 0, 0);
            out(Op::WbufSink, p(buffer), b, 0, 0);
            on(Op::WbufFree, p(buffer));
            0
        }
        // the summary in the receive op `a`'s packet, past its type byte and port, read into the game `b`: its
        // result, or Threw (a packet too short for the head, or the summary's own read)
        Op::LanReadSummary => {
            let mut packet: Vec<u8> = Vec::new();
            out(Op::LanDiscoveryTake, a, &mut packet as *mut Vec<u8> as *mut c_void, 0, 0);
            match packet.get(3..) {
                Some(rest) => out(Op::LanGameReadSummary, null_mut(), b, rest.as_ptr() as isize, rest.len() as isize),
                None => THREW,
            }
        }
        // the direct-connect dialog run and its address into `a` (resolved by a DNS op when it is no dotted
        // address): 1 with the address, 0 cancelled or unresolved (`a` untouched)
        Op::LanDirectAsk => {
            on(Op::LanDirectReset, null_mut());
            if on(Op::LanDirectDo, null_mut()) != 0x186a1 {
                return Some(0);
            }
            let config = on(Op::LobbyConfigGet, null_mut());
            let config_port = if config != 0 { on(Op::ConfigLobbyPortTest, p(config)) } else { 0 };
            let port = crate::lanlogic::direct_default_port(
                config != 0,
                config_port as i32,
                on(Op::LobbyPort, null_mut()) as i32,
            ) as u16;
            let mut host: Vec<u8> = Vec::new();
            out(Op::LanDirectHostTake, null_mut(), &mut host as *mut Vec<u8> as *mut c_void, 0, 0);
            let mut address = EeWonIpAddr::default();
            address.set_no_dns(&host, port);
            if !address.valid {
                let dns = on(Op::LanDirectDnsHeld, null_mut());
                let r = rows(Res::LANLogic_CheckingAddressTitle_String, Res::LANLogic_CheckingAddressDetails_String);
                out(Op::RunOpWithStatusRows, p(dns), null_mut(), r, 0);
                if on(Op::OpSucceededOf, p(dns)) == 0 {
                    on(Op::RefRelease, p(dns));
                    return Some(0);
                }
                out(Op::DnsOpAddrAssign, &mut address as *mut EeWonIpAddr as *mut c_void, p(dns), 0, 0);
                if address.port() == 0 {
                    address.set_port(port);
                }
                on(Op::RefRelease, p(dns));
            }
            if !a.is_null() {
                // SAFETY: the op's contract: `a` is the caller's live address
                unsafe { ee_won_net::ipobj::ee_won_ipaddr_assign(a as *mut EeWonIpAddr, &address) };
            }
            1
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
    fn the_round_32_compositions_keep_the_statement_order() {
        use std::cell::RefCell;
        let log = RefCell::new(Vec::new());
        let out = |op: Op, a: *mut c_void, _b: *mut c_void, c: isize, _d: isize| {
            log.borrow_mut().push(format!("{op:?} {} {c}", a as isize));
            if matches!(op, Op::LanClientAlloc | Op::LanTimerAlloc | Op::StDialogGameHeld | Op::GameCreateHeld) {
                77
            } else {
                5
            }
        };
        let run = |op: Op, a: isize, c: isize| {
            log.borrow_mut().clear();
            let r = lobby_op(op, p(a), null_mut(), c, 3, &out);
            (r, log.borrow().clone())
        };
        assert_eq!(
            run(Op::LanNewClient, 0, 9),
            (Some(77), vec!["LanClientAlloc 0 0".into(), "LanClientSetName 77 9".into()])
        );
        assert_eq!(
            run(Op::LanPortDialogRun, 0, 2300),
            (
                Some(5),
                vec![
                    "LanPortSetGamePort 0 0".into(),
                    "LanPortSetPort 0 2300".into(),
                    "LanPortReset 0 0".into(),
                    "LanPortDo 0 0".into()
                ]
            )
        );
        assert_eq!(run(Op::LanNewTimer, 0, 1).1, vec!["LanTimerAlloc 0 0", "LanTimerSetCompletion 77 1"]);
        assert_eq!(
            run(Op::StCreateDialogRun, 0, 1).1,
            vec!["StCreateSetType 0 1", "StCreateReset 0 0", "StCreateDo 0 0"]
        );
        assert_eq!(
            run(Op::StTakeDialogGame, 0, 4),
            (Some(77), vec!["StDialogGameHeld 0 0".into(), "GameSetMyAddress 77 4".into(), "StSetGame 77 0".into()])
        );
        assert_eq!(
            run(Op::GameNewCopy, 40, 0),
            (Some(77), vec!["GameCreateHeld 40 0".into(), "GameCopyFrom 77 0".into()])
        );
    }

    /// `lobby_op` with all four arguments, the calls logged with theirs.
    fn run4(
        op: Op,
        a: isize,
        b: isize,
        c: isize,
        d: isize,
        answer: &dyn Fn(Op, usize) -> isize,
    ) -> (Option<isize>, Vec<String>) {
        let log = RefCell::new(Vec::new());
        let out = |op: Op, a: *mut c_void, b: *mut c_void, c: isize, d: isize| {
            let n = log.borrow().len();
            // (the appended bytes are the arm's own: their address is not a value to compare)
            let known = |x: isize| if op == Op::WbufAppend && x == c { "ptr".to_string() } else { format!("{x:#x}") };
            log.borrow_mut().push(format!(
                "{op:?} {} {} {} {}",
                known(a as isize),
                known(b as isize),
                known(c),
                known(d)
            ));
            answer(op, n)
        };
        let r = lobby_op(op, a as *mut c_void, b as *mut c_void, c, d, &out);
        (r, log.into_inner())
    }

    #[test]
    fn round_36_the_ready_flags_reset() {
        // two clients: the first with a player, the second without (Next leaves the client and answers 0)
        let (r, ops) = run4(Op::GameResetReadyFlags, 0x10, 0, 0, 0, &|op, n| match op {
            Op::GameClientsIterNew => 0x50,
            Op::ClientsIterHasMore => (n < 6) as isize,
            Op::ClientsIterNext => 0, // the out client stays NULL: no player change
            _ => 0,
        });
        assert_eq!(r, Some(0));
        assert_eq!(ops.first().unwrap(), "GameClientsIterNew 0x10 0x0 0x0 0x0");
        assert_eq!(ops.last().unwrap(), "ClientsIterFree 0x50 0x0 0x0 0x0");
        assert!(ops.iter().all(|o| !o.starts_with("PlayerSetReady") && !o.starts_with("ClientChanged")));
        // an empty game: the iterator made and freed
        let (_, ops) =
            run4(Op::GameResetReadyFlags, 0x10, 0, 0, 0, &|op, _| if op == Op::GameClientsIterNew { 0x50 } else { 0 });
        assert_eq!(ops.len(), 3);
    }

    #[test]
    fn round_36_the_join_reply() {
        let threw = THREW;
        // the read threw: nothing sent, nothing released
        let (r, ops) = run4(Op::GameJoinReplySend, 0x10, 0x20, 0, 0x30, &|_, _| threw);
        assert_eq!((r, ops.len()), (Some(threw), 1));
        assert_eq!(ops[0], "GameJoinReplyHeld 0x10 0x20 0x0 0x30");
        // no reply
        let (r, ops) = run4(Op::GameJoinReplySend, 0x10, 0x20, 0, 0x30, &|_, _| 0);
        assert_eq!((r, ops.len()), (Some(0), 1));
        // a reply: sent to the client, then released
        let (r, ops) = run4(Op::GameJoinReplySend, 0x10, 0x20, 0, 0x30, &|op, _| {
            if op == Op::GameJoinReplyHeld {
                0x40
            } else {
                0
            }
        });
        assert_eq!(r, Some(1));
        assert_eq!(
            ops[1..],
            ["StSendToClientBuf 0x40 0x30 0x0 0x0".to_string(), "RefRelease 0x40 0x0 0x0 0x0".to_string()]
        );
    }

    #[test]
    fn round_36_the_player_read() {
        for (read, want, set) in [(1, 1, true), (0, 0, false), (THREW, THREW, false)] {
            let (r, ops) = run4(Op::ClientReadPlayer, 0x10, 0x20, 0, 0, &|op, _| match op {
                Op::PlayerCreateHeld => 0x60,
                Op::PlayerReadFrom => read,
                _ => 0,
            });
            assert_eq!(r, Some(want));
            assert_eq!(ops[0], "PlayerCreateHeld 0x0 0x0 0x0 0x0");
            assert_eq!(ops[1], "PlayerReadFrom 0x60 0x20 0x0 0x0");
            assert_eq!(ops.iter().any(|o| o == "ClientSetPlayer 0x10 0x60 0x0 0x0"), set);
            // the player's reference goes on every path
            assert_eq!(ops.last().unwrap(), "RefRelease 0x60 0x0 0x0 0x0");
        }
    }

    #[test]
    fn round_36_the_joined_broadcast_and_the_summary() {
        let (r, ops) = run4(Op::ClientBroadcastJoined, 0x10, 0, 0, 0, &|op, _| match op {
            Op::ClientId => 7,
            Op::WbufNew => 0x70,
            _ => 0,
        });
        assert_eq!(r, Some(0));
        let head =
            crate::staging::room_message(&crate::staging::RoomMsg { kind: 3, id: 7, flag: false, ping: 0, rest: 0 });
        assert_eq!(
            ops,
            [
                "ClientId 0x10 0x0 0x0 0x0".to_string(),
                "WbufNew 0x0 0x0 0x0 0x0".to_string(),
                format!("WbufAppend 0x70 0x0 ptr {:#x}", head.len()),
                "ClientWritePlayer 0x10 0x70 0x0 0x0".to_string(),
                "StBroadcastWbuf 0x70 0x0 0x0 0x0".to_string(),
                "WbufFree 0x70 0x0 0x0 0x0".to_string(),
            ]
        );
        let (r, ops) = run4(Op::LanSummaryTake, 0x10, 0x20, 0, 0, &|op, _| if op == Op::WbufNew { 0x70 } else { 0 });
        assert_eq!(r, Some(0));
        assert_eq!(
            ops,
            [
                "WbufNew 0x0 0x0 0x0 0x0",
                "GameWriteSummary 0x10 0x70 0x0 0x0",
                "WbufSink 0x70 0x20 0x0 0x0",
                "WbufFree 0x70 0x0 0x0 0x0"
            ]
        );
    }

    #[test]
    fn round_36_the_wait_the_subnets_and_the_staging_bytes() {
        // the wait op: made, run with the text code's rows in the mode, asked, released
        let (r, ops) = run4(Op::RunWaitOp, 0, 0, 6, 2, &|op, _| match op {
            Op::LanTimerAlloc => 0x80,
            Op::OpIsKilled => 1,
            _ => 0,
        });
        assert_eq!(r, Some(1));
        assert_eq!(ops[0], "LanTimerAlloc 0x0 0x0 0x0 0x0");
        assert_eq!(ops[1], format!("RunOpWithStatusMode 0x80 0x0 {:#x} 0x2", crate::lobbyhost::lobby_text(6)));
        assert_eq!(ops[2..], ["OpIsKilled 0x80 0x0 0x0 0x0".to_string(), "RefRelease 0x80 0x0 0x0 0x0".to_string()]);
        // the subnets: the peer's two bytes low, this machine's high
        let (r, _) =
            run4(Op::LanSubnets, 0x10, 0, 0, 0, &|op, _| if op == Op::LanPeerSubnet { 0x0201 } else { 0x0403 });
        assert_eq!(r, Some(0x0403_0201));
        // a staging message: past its type byte; an empty one is not passed on
        let (r, ops) = run4(Op::LanStagingRead, 0, 0x20, 0x5000, 4, &|_, _| 9);
        assert_eq!((r, ops), (Some(0), vec!["LanStagingBytes 0x0 0x20 0x5001 0x3".to_string()]));
        let (r, ops) = run4(Op::LanStagingRead, 0, 0x20, 0x5000, 1, &|_, _| 9);
        assert_eq!((r, ops), (Some(0), vec!["LanStagingBytes 0x0 0x20 0x5001 0x0".to_string()]));
        let (r, ops) = run4(Op::LanStagingRead, 0, 0x20, 0x5000, 0, &|_, _| 9);
        assert_eq!((r, ops.len()), (Some(0), 0));
    }

    /// A packet handed to the take op's sink.
    fn give(b: *mut c_void, bytes: &[u8]) {
        // SAFETY: the sink is the arm's Vec
        unsafe { crate::lobbyhost::ee_won_lobby_sink(b, bytes.as_ptr(), bytes.len()) };
    }

    #[test]
    fn round_36_the_summary_read() {
        let run = |packet: &'static [u8], answer: isize| {
            let log = RefCell::new(Vec::new());
            let out = |op: Op, a: *mut c_void, b: *mut c_void, _c: isize, d: isize| {
                log.borrow_mut().push(format!("{op:?} {:#x} {d}", a as isize));
                if op == Op::LanDiscoveryTake {
                    give(b, packet);
                    return 0;
                }
                answer
            };
            let r = lobby_op(Op::LanReadSummary, p(0x10), p(0x20), 0, 0, &out);
            (r, log.into_inner())
        };
        // the head (a type byte and a port) and three summary bytes
        assert_eq!(
            run(&[1, 2, 3, 4, 5, 6], 1),
            (Some(1), vec!["LanDiscoveryTake 0x10 0".to_string(), "LanGameReadSummary 0x0 3".to_string()])
        );
        assert_eq!(run(&[1, 2, 3], 0).0, Some(0));
        assert_eq!(run(&[1, 2, 3], THREW).0, Some(THREW));
        // too short for the head: the original's ReadByte or ReadShort threw
        for short in [&[][..], &[1][..], &[1, 2][..]] {
            let short: &'static [u8] = short.to_vec().leak();
            assert_eq!(run(short, 1), (Some(THREW), vec!["LanDiscoveryTake 0x10 0".to_string()]));
        }
    }

    #[test]
    fn round_36_the_direct_connect_address() {
        let ask = |host: &'static [u8], dialog: isize, config: isize, dns_ok: isize, dns_port: u16| {
            let mut address = EeWonIpAddr::default();
            let log = RefCell::new(Vec::new());
            let out = |op: Op, a: *mut c_void, b: *mut c_void, _c: isize, _d: isize| {
                log.borrow_mut().push(format!("{op:?}"));
                match op {
                    Op::LanDirectDo => dialog,
                    Op::LobbyConfigGet => config,
                    Op::ConfigLobbyPortTest => 2500,
                    Op::LobbyPort => 2300,
                    Op::LanDirectHostTake => {
                        give(b, host);
                        0
                    }
                    Op::LanDirectDnsHeld => 0x90,
                    Op::OpSucceededOf => dns_ok,
                    Op::DnsOpAddrAssign => {
                        let mut found = EeWonIpAddr::default();
                        found.set_no_dns(b"10.1.2.3", dns_port);
                        // SAFETY: `a` is the arm's address
                        unsafe { ee_won_net::ipobj::ee_won_ipaddr_assign(a as *mut EeWonIpAddr, &found) };
                        0
                    }
                    _ => 0,
                }
            };
            let r = lobby_op(Op::LanDirectAsk, &mut address as *mut EeWonIpAddr as *mut c_void, null_mut(), 0, 0, &out);
            (r, address.valid, address.port(), log.into_inner())
        };
        // cancelled: only the dialog
        let (r, valid, _, ops) = ask(b"1.2.3.4", 0x186a2, 0x40, 1, 0);
        assert_eq!((r, valid), (Some(0), false));
        assert_eq!(ops, ["LanDirectReset", "LanDirectDo"]);
        // a dotted address without a port: the config's port, no DNS
        let (r, valid, port, ops) = ask(b"1.2.3.4", 0x186a1, 0x40, 1, 0);
        assert_eq!((r, valid, port), (Some(1), true, crate::lanlogic::direct_default_port(true, 2500, 2300) as u16));
        assert!(!ops.iter().any(|o| o == "LanDirectDnsHeld"));
        // its own port wins
        let (_, _, port, _) = ask(b"1.2.3.4:999", 0x186a1, 0x40, 1, 0);
        assert_eq!(port, 999);
        // no config: the lobby's port, the config's not asked
        let (_, _, port, ops) = ask(b"1.2.3.4", 0x186a1, 0, 1, 0);
        assert_eq!(port, crate::lanlogic::direct_default_port(false, 0, 2300) as u16);
        assert!(!ops.iter().any(|o| o == "ConfigLobbyPortTest"));
        // a name: the DNS op run behind the status box; failed: released, no address
        let (r, valid, _, ops) = ask(b"some.host", 0x186a1, 0x40, 0, 0);
        assert_eq!((r, valid), (Some(0), false));
        assert_eq!(ops[ops.len() - 4..], ["LanDirectDnsHeld", "RunOpWithStatusRows", "OpSucceededOf", "RefRelease"]);
        // resolved without a port: the default port; with one: its own
        let (r, valid, port, ops) = ask(b"some.host", 0x186a1, 0x40, 1, 0);
        assert_eq!((r, valid, port), (Some(1), true, crate::lanlogic::direct_default_port(true, 2500, 2300) as u16));
        assert_eq!(ops[ops.len() - 2..], ["DnsOpAddrAssign", "RefRelease"]);
        let (_, _, port, _) = ask(b"some.host", 0x186a1, 0x40, 1, 4242);
        assert_eq!(port, 4242);
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
