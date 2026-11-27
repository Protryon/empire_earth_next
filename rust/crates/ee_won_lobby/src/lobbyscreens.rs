//! The lobby's top container (LobbyContainer.cpp, R6 WON lobby/net round 40): its three screens (the login, Internet
//! and LAN screens, a reference each) and every method (InitComponent's look-ups, the Show* and On* methods,
//! GetGameScreenType) in Rust over one-statement C++ ops (WONLobbyPrv.cpp's `ScreensHost`). The container itself
//! stays a C++ ScreenContainer (its `mCurScreen`, `AddScreen` and `ShowScreen` are the GUI's).
//!
//! Reach: the LAN game builds the lobby (InitComponent), shows the LAN screen and asks which screen is shown.

use std::ffi::c_void;
use std::sync::Mutex;

/// The host: `(container, op, a, b) -> result`.
pub type EeWonLobbyScreensHostFn =
    Option<unsafe extern "C" fn(container: *mut c_void, op: EeWonLobbyScreensOp, a: isize, b: isize) -> isize>;

/// The host's ops: each one C++ statement.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonLobbyScreensOp {
    /// `SafeGetComponent((ComponentConfig*)a, login, "LoginScreen", file, 37)`: the screen with a reference added
    /// (NULL when missing; guarded).
    GetLogin = 1,
    /// `SafeGetComponent((ComponentConfig*)a, root, b ? "LanScreen" : "InternetScreen", file, b ? 39 : 38)`: as
    /// GetLogin.
    GetRoot,
    /// `c->AddScreen((Component*)a)`.
    AddScreen,
    /// `c->ShowScreen((Component*)a, true)`.
    ShowScreen,
    /// `c->mCurScreen`.
    CurScreen,
    /// `((LoginScreen*)a)->ShowMOTD()`.
    ShowMotd,
    /// `((RootScreen*)a)->OnLanTab()`.
    OnLanTab,
    /// `((RootScreen*)a)->OnDirectConnectTab()`.
    OnDirectConnectTab,
    /// `((RefCount*)a)->Release()`.
    Release,
}

/// The calls in.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonLobbyScreensCall {
    /// `InitComponent` of a config init (`a` the config): the three screens found, added, the login one shown
    /// (`EE_WON_LOBBY_THROWN` when a look-up threw).
    Init = 1,
    /// The destructor: the three screens released.
    Drop,
    ShowInternet,
    ShowLan,
    /// The login screen shown, then its message of the day.
    ShowLogin,
    OnInternet,
    OnLan,
    OnLogin,
    /// 2 Internet, 1 the LAN screen's games tab, 3 its direct connect tab, else 0.
    GameScreenType,
    /// The screen `a` (0 login, 1 Internet, 2 LAN), borrowed (NULL before InitComponent).
    Screen,
}

use EeWonLobbyScreensCall as C;
use EeWonLobbyScreensOp as Op;

/// Each container's screens: login, Internet, LAN.
static SCREENS: Mutex<Vec<(usize, [isize; 3])>> = Mutex::new(Vec::new());

fn screens(c: usize) -> [isize; 3] {
    let g = SCREENS.lock().unwrap_or_else(|e| e.into_inner());
    g.iter().find(|e| e.0 == c).map_or([0; 3], |e| e.1)
}

fn set_screen(c: usize, i: usize, p: isize) {
    let mut g = SCREENS.lock().unwrap_or_else(|e| e.into_inner());
    match g.iter_mut().find(|e| e.0 == c) {
        Some(e) => e.1[i] = p,
        None => {
            let mut s = [0; 3];
            s[i] = p;
            g.push((c, s));
        }
    }
}

fn call(c: usize, call: C, a: isize, op: &dyn Fn(Op, isize, isize) -> isize) -> isize {
    let [login, internet, lan] = screens(c);
    let on = |s: isize| op(Op::CurScreen, 0, 0) == s;
    match call {
        C::Init => {
            // the SmartPtr assignments: a found screen replaces (and releases) the one held
            let looks: [(usize, Op, isize); 3] = [(0, Op::GetLogin, 0), (1, Op::GetRoot, 0), (2, Op::GetRoot, 1)];
            for (i, look, b) in looks {
                let p = op(look, a, b);
                if p == crate::lobbyprv::EE_WON_LOBBY_THROWN {
                    return p;
                }
                let old = screens(c)[i];
                set_screen(c, i, p);
                if old != 0 {
                    op(Op::Release, old, 0);
                }
            }
            let [login, internet, lan] = screens(c);
            for s in [login, internet, lan] {
                op(Op::AddScreen, s, 0);
            }
            op(Op::ShowScreen, login, 0);
            0
        }
        C::Drop => {
            SCREENS.lock().unwrap_or_else(|e| e.into_inner()).retain(|e| e.0 != c);
            // the members' destructors, last first
            for s in [lan, internet, login] {
                if s != 0 {
                    op(Op::Release, s, 0);
                }
            }
            0
        }
        C::ShowInternet => {
            op(Op::ShowScreen, internet, 0);
            0
        }
        C::ShowLan => {
            op(Op::ShowScreen, lan, 0);
            0
        }
        C::ShowLogin => {
            op(Op::ShowScreen, login, 0);
            op(Op::ShowMotd, screens(c)[0], 0);
            0
        }
        C::OnInternet => on(internet) as isize,
        C::OnLan => on(lan) as isize,
        C::OnLogin => on(login) as isize,
        C::GameScreenType => {
            if on(internet) {
                2
            } else if on(lan) {
                if op(Op::OnLanTab, lan, 0) != 0 {
                    1
                } else if op(Op::OnDirectConnectTab, lan, 0) != 0 {
                    3
                } else {
                    0
                }
            } else {
                0
            }
        }
        C::Screen => [login, internet, lan].get(a as usize).copied().unwrap_or(0),
    }
}

/// One of the container `container`'s calls over the host `host`.
///
/// # Safety
/// `host` answers the ops for `container` (a live LobbyContainer).
#[no_mangle]
pub unsafe extern "C" fn ee_won_lobbyscreens_call(
    container: *mut c_void,
    host: EeWonLobbyScreensHostFn,
    which: EeWonLobbyScreensCall,
    a: isize,
) -> isize {
    let op = |o: Op, x: isize, y: isize| match host {
        // SAFETY: the contract
        Some(f) => unsafe { f(container, o, x, y) },
        None => 0,
    };
    call(container as usize, which, a, &op)
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::cell::RefCell;

    fn run(c: usize, which: C, a: isize, answer: &dyn Fn(Op, isize, isize) -> isize) -> (isize, Vec<String>) {
        let log = RefCell::new(Vec::new());
        let op = |o: Op, x: isize, y: isize| {
            log.borrow_mut().push(format!("{o:?} {x} {y}"));
            answer(o, x, y)
        };
        let r = call(c, which, a, &op);
        (r, log.into_inner())
    }

    #[test]
    fn init_shows_the_login_screen_and_the_screens_answer() {
        let c = 0x1000;
        let (r, log) = run(c, C::Init, 7, &|o, _, b| match o {
            Op::GetLogin => 10,
            Op::GetRoot => 20 + b,
            _ => 0,
        });
        assert_eq!(r, 0);
        assert_eq!(
            log,
            [
                "GetLogin 7 0",
                "GetRoot 7 0",
                "GetRoot 7 1",
                "AddScreen 10 0",
                "AddScreen 20 0",
                "AddScreen 21 0",
                "ShowScreen 10 0"
            ]
        );
        assert_eq!(run(c, C::Screen, 2, &|_, _, _| 0).0, 21);
        assert_eq!(run(c, C::OnLan, 0, &|o, _, _| (o == Op::CurScreen) as isize * 21).0, 1);
        let lan_direct = |o: Op, _: isize, _: isize| match o {
            Op::CurScreen => 21,
            Op::OnDirectConnectTab => 1,
            _ => 0,
        };
        assert_eq!(run(c, C::GameScreenType, 0, &lan_direct).0, 3);
        assert_eq!(run(c, C::GameScreenType, 0, &|o, _, _| (o == Op::CurScreen) as isize * 20).0, 2);
        let (_, log) = run(c, C::ShowLogin, 0, &|_, _, _| 0);
        assert_eq!(log, ["ShowScreen 10 0", "ShowMotd 10 0"]);
        let (_, log) = run(c, C::Drop, 0, &|_, _, _| 0);
        assert_eq!(log, ["Release 21 0", "Release 20 0", "Release 10 0"]);
        assert_eq!(run(c, C::Screen, 0, &|_, _, _| 0).0, 0);
    }

    #[test]
    fn a_missing_screen_is_null_and_no_screen_is_shown_without_init() {
        let c = 0x2000;
        // no screen yet and none shown: the null screen is the shown one, as the original's compare
        assert_eq!(run(c, C::OnLogin, 0, &|_, _, _| 0).0, 1);
        assert_eq!(run(c, C::OnLogin, 0, &|_, _, _| 9).0, 0);
        let (r, log) = run(c, C::Init, 1, &|o, _, _| {
            if o == Op::GetRoot {
                crate::lobbyprv::EE_WON_LOBBY_THROWN
            } else {
                5
            }
        });
        assert_eq!((r, log.len()), (crate::lobbyprv::EE_WON_LOBBY_THROWN, 2));
        run(c, C::Drop, 0, &|_, _, _| 0);
    }
}
