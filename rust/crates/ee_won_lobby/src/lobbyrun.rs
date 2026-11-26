//! Running the lobby (LobbyRunState.cpp, R6 WON round 32): LobbyStart, LobbyRun's modal message loop and exit
//! status, LobbyExit, LobbySuspend and LobbyResume in Rust over one-statement C++ ops (`RunHost` in
//! LobbyRunState.cpp), in the original's order. `gLobbyData` and its members stay the C++ lobby's.
//!
//! Reach: the LAN game opens, runs, suspends (the game starts), resumes and exits the lobby; LobbyRun's failure
//! dialog branch (a lobby whose init failed) runs only in the unit tests.

/// The host: `(op, a) -> result`.
pub type EeWonRunHostFn = Option<unsafe extern "C" fn(op: EeWonRunOp, a: isize) -> isize>;

/// The host's ops: each one C++ statement.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonRunOp {
    /// `gLobbyInitFailed && gLobbyFailureDialog.get() != NULL`.
    InitFailedWithDialog = 1,
    /// `gLobbyFailureDialog->GetRootWindow()`.
    FailureWindow,
    /// `((Window*)a)->PlatformIsVisible()`.
    WindowVisible,
    /// `((Window*)a)->RunDialog(gLobbyFailureDialog.get())`.
    WindowRunFailureDialog,
    /// `gLobbyData.get() != NULL`.
    HasData,
    /// `gLobbyData->mLobbyStatus = a`.
    SetStatus,
    /// `gLobbyData->mLobbyStatus`.
    Status,
    /// `gLobbyData->mRunMode = a`.
    SetRunMode,
    /// `gLobbyData->mRunMode`.
    RunMode,
    /// `gLobbyData->mSuspended = a != 0`.
    SetSuspended,
    /// `GetLobbyWindow()`.
    LobbyWindow,
    /// `((WindowManager*)a)->MessageLoop()`.
    MessageLoop,
    /// `((WindowManager*)a)->ExitMessageLoop()`.
    ExitMessageLoop,
    /// `LobbyBroadcastLobbyEvent(a, true)`.
    BroadcastLobbyEvent,
    /// `LOBBY_SOUND(LobbyGlobal_LobbyMusic_Sound)->Stop()`.
    MusicStop,
    /// `LOBBY_SOUND(LobbyGlobal_SierraLogon_Sound)->Stop()`.
    LogonSoundStop,
    /// `LobbyGetNumPingsToDo()`.
    PingsToDo,
    /// `LobbyKillPings(1)`.
    KillPings,
    /// `LobbyNetSuspend()`.
    NetSuspend,
    /// `LobbyGetLobbyMusic()`.
    MusicOn,
    /// `LOBBY_SOUND(LobbyGlobal_LobbyMusic_Sound)->Resume()`.
    MusicResume,
    /// `LobbyNetResume()`.
    NetResume,
    /// `gLobbyData->mInitLogic->Init()`.
    InitLogicInit,
}

/// The calls in.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonRunCall {
    /// `LobbyRun()`: the status.
    Run = 1,
    /// `LobbyExit(a)`.
    Exit,
    /// `LobbySuspend()`.
    Suspend,
    /// `LobbyResume()`.
    Resume,
    /// `LobbyStart()`.
    Start,
}

use EeWonRunOp as Op;

/// The lobby events the functions broadcast (WONLobbyEvents.h's ids).
const EVENT_LOBBY_EXIT: isize = 100013;
const EVENT_PINGS_KILLED: isize = 100059;

fn call(call: EeWonRunCall, a: isize, op: &dyn Fn(Op, isize) -> isize) -> isize {
    // the lobby music and the logon sound are soundobj.rs's in the game (R6 WON GUI round 41)
    let on = |o: Op| {
        use crate::resource::EeWonLobbyRes as R;
        use ee_won_gui::soundobj::EeWonSoundCall as SC;
        let (row, call) = match o {
            Op::MusicStop => (R::LobbyGlobal_LobbyMusic_Sound, SC::Stop),
            Op::MusicResume => (R::LobbyGlobal_LobbyMusic_Sound, SC::Resume),
            Op::LogonSoundStop => (R::LobbyGlobal_SierraLogon_Sound, SC::Stop),
            _ => return op(o, 0),
        };
        if !ee_won_gui::compobj::is_game() {
            return op(o, 0);
        }
        crate::eesound::res_sound(row, call, 0)
    };
    match call {
        EeWonRunCall::Run => {
            if on(Op::InitFailedWithDialog) != 0 {
                let window = on(Op::FailureWindow);
                if window != 0 && op(Op::WindowVisible, window) != 0 {
                    op(Op::WindowRunFailureDialog, window);
                }
                return 0;
            }
            if on(Op::HasData) == 0 {
                return 0;
            }
            op(Op::SetStatus, 0);
            op(Op::SetRunMode, 1);
            op(Op::SetSuspended, 0);
            let manager = on(Op::LobbyWindow);
            if manager == 0 {
                return 0;
            }
            op(Op::MessageLoop, manager);
            if on(Op::HasData) == 0 {
                return 0;
            }
            on(Op::Status)
        }
        EeWonRunCall::Exit => {
            if on(Op::HasData) == 0 {
                return 0;
            }
            op(Op::SetStatus, a);
            let mode = on(Op::RunMode);
            op(Op::BroadcastLobbyEvent, EVENT_LOBBY_EXIT);
            if mode != 0 {
                let manager = on(Op::LobbyWindow);
                if manager != 0 {
                    op(Op::ExitMessageLoop, manager);
                }
            }
            0
        }
        EeWonRunCall::Suspend => {
            if on(Op::HasData) == 0 {
                return 0;
            }
            op(Op::SetSuspended, 1);
            on(Op::MusicStop);
            on(Op::LogonSoundStop);
            if on(Op::PingsToDo) as i32 > 0 {
                on(Op::KillPings);
                op(Op::BroadcastLobbyEvent, EVENT_PINGS_KILLED);
            }
            on(Op::NetSuspend);
            0
        }
        EeWonRunCall::Resume => {
            if on(Op::HasData) == 0 {
                return 0;
            }
            op(Op::SetSuspended, 0);
            if on(Op::MusicOn) != 0 {
                on(Op::MusicResume);
            }
            on(Op::NetResume);
            0
        }
        EeWonRunCall::Start => {
            if on(Op::HasData) == 0 {
                return 0;
            }
            op(Op::SetSuspended, 0);
            on(Op::InitLogicInit);
            0
        }
    }
}

/// One of the lobby's run functions over the host `host`.
///
/// # Safety
/// `host` answers the ops (RunHost).
#[no_mangle]
pub unsafe extern "C" fn ee_won_lobbyrun_call(host: EeWonRunHostFn, run: EeWonRunCall, a: isize) -> isize {
    let op = |o: Op, a: isize| match host {
        // SAFETY: the contract
        Some(f) => unsafe { f(o, a) },
        None => 0,
    };
    call(run, a, &op)
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::cell::RefCell;

    fn run(c: EeWonRunCall, a: isize, answer: &dyn Fn(Op) -> isize) -> (isize, Vec<String>) {
        let log = RefCell::new(Vec::new());
        let op = |o: Op, a: isize| {
            log.borrow_mut().push(format!("{o:?} {a}"));
            answer(o)
        };
        let r = call(c, a, &op);
        (r, log.into_inner())
    }

    #[test]
    fn the_run_loop_and_its_status() {
        let (r, log) = run(EeWonRunCall::Run, 0, &|o| match o {
            Op::HasData => 1,
            Op::LobbyWindow => 0x40,
            Op::Status => 7,
            _ => 0,
        });
        assert_eq!(r, 7);
        assert_eq!(
            log[1..7],
            ["HasData 0", "SetStatus 0", "SetRunMode 1", "SetSuspended 0", "LobbyWindow 0", "MessageLoop 64"]
        );
        // the failed init's dialog, run only when its window shows
        let (r, log) = run(EeWonRunCall::Run, 0, &|o| {
            matches!(o, Op::InitFailedWithDialog | Op::FailureWindow | Op::WindowVisible) as isize
        });
        assert_eq!((r, log.last().unwrap().as_str()), (0, "WindowRunFailureDialog 1"));
        let (_, log) = run(EeWonRunCall::Run, 0, &|o| (o == Op::InitFailedWithDialog) as isize);
        assert_eq!(log, ["InitFailedWithDialog 0", "FailureWindow 0"]);
        let (r, log) = run(EeWonRunCall::Run, 0, &|_| 0);
        assert_eq!((r, log.len()), (0, 2));
    }

    #[test]
    fn exit_suspend_resume_start() {
        let (_, log) = run(EeWonRunCall::Exit, 3, &|o| match o {
            Op::HasData | Op::RunMode => 1,
            Op::LobbyWindow => 0x40,
            _ => 0,
        });
        assert_eq!(
            log[1..],
            ["SetStatus 3", "RunMode 0", "BroadcastLobbyEvent 100013", "LobbyWindow 0", "ExitMessageLoop 64"]
        );
        let (_, log) = run(EeWonRunCall::Exit, 3, &|o| (o == Op::HasData) as isize);
        assert_eq!(log.len(), 4);
        let (_, log) = run(EeWonRunCall::Suspend, 0, &|o| match o {
            Op::HasData => 1,
            Op::PingsToDo => 2,
            _ => 0,
        });
        assert_eq!(log[5..], ["KillPings 0", "BroadcastLobbyEvent 100059", "NetSuspend 0"]);
        let (_, log) = run(EeWonRunCall::Resume, 0, &|o| matches!(o, Op::HasData | Op::MusicOn) as isize);
        assert_eq!(log[1..], ["SetSuspended 0", "MusicOn 0", "MusicResume 0", "NetResume 0"]);
        let (_, log) = run(EeWonRunCall::Start, 0, &|o| (o == Op::HasData) as isize);
        assert_eq!(log[1..], ["SetSuspended 0", "InitLogicInit 0"]);
        assert_eq!(run(EeWonRunCall::Start, 0, &|_| 0).1, ["HasData 0"]);
    }
}
