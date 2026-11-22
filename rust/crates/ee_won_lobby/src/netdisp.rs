//! The lobby's network entry points (LobbyStagingSupport.cpp's dispatch, R6 WON round 32): the current game
//! type's network logic (LAN for LAN and direct-connect games, routing for Internet ones) and the forwards to its
//! virtuals, in Rust over one-statement C++ ops (`NetHost` in LobbyStagingSupport.cpp), in the original's order.
//!
//! Reach: the LAN game hosts, joins, advertises, sends, leaves and logs out through the LAN logic; the routing
//! logic's branches (an Internet game) run only in the unit tests.

/// The host: `(op, a, b, c) -> result`.
pub type EeWonNetDispHostFn = Option<unsafe extern "C" fn(op: EeWonNetDispOp, a: isize, b: isize, c: isize) -> isize>;

/// The host's ops: each one C++ statement. `logic` is a `NetLogic*`.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonNetDispOp {
    /// `gLobbyData.get() != NULL`.
    HasData = 1,
    /// `gLobbyData->mLanLogic.get()`.
    LanLogic,
    /// `gLobbyData->mRoutingLogic.get()`.
    RoutingLogic,
    /// `gLobbyData->mCurGameType`.
    CurGameType,
    /// `gLobbyData->mCurGameType = a`.
    SetCurGameType,
    /// `((LobbyGame*)a)->GetGameType()`.
    GameType,
    /// `logic->HostGame((LobbyGame*)b)`.
    HostGame,
    /// `logic->JoinGame((LobbyGame*)b)`.
    JoinGame,
    /// `logic->GetMyClient()`.
    MyClient,
    /// `LobbyOnInternetScreen()`.
    OnInternetScreen,
    /// `LobbyOnLanScreen()`.
    OnLanScreen,
    /// `gLobbyData->mRoutingLogic->Logout()`.
    RoutingLogout,
    /// `gLobbyData->mLanLogic->Logout()`.
    LanLogout,
    /// `LobbyIAmCaptain()`.
    IAmCaptain,
    /// `GetMyLobbyGame()`.
    MyGame,
    /// `logic->AdvertiseGame((LobbyGame*)b)`.
    AdvertiseGame,
    /// `GetMyLobbyClient()`.
    MyLobbyClient,
    /// `((LobbyClient*)a)->GetPlayer().get()`.
    ClientPlayer,
    /// `logic->SendToCaptain((const ByteBuffer*)b)`.
    SendToCaptain,
    /// `logic->BroadcastToGame((const ByteBuffer*)b)`.
    BroadcastToGame,
    /// `logic->SendToClient((LobbyClient*)b, (const ByteBuffer*)c)`.
    SendToClient,
    /// `logic->RemoveGameAdvertisement((LobbyGame*)b)`.
    RemoveGameAdvertisement,
    /// `logic->LeaveGame((LobbyGame*)b)`.
    LeaveGame,
    /// `logic->KickClient((LobbyClient*)b, c != 0)`.
    KickClient,
    /// `logic->SetGameInProgress(b != 0)`.
    SetGameInProgress,
    /// `logic->Suspend()`.
    Suspend,
    /// `logic->Resume()`.
    Resume,
}

/// The calls in (`a`, `b` as each says).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonNetDispCall {
    /// `LobbyGetNetLogic(a)`.
    NetLogic = 1,
    /// `LobbyGetCurNetLogic()`.
    CurNetLogic,
    /// `LobbyNetHostGame(a)`.
    HostGame,
    /// `LobbyNetJoinGame(a)`.
    JoinGame,
    /// `LobbyGetNetMyClient()`.
    MyClient,
    /// `LobbyNetLogout()`.
    Logout,
    /// `LobbyNetAdvertiseGame()`.
    AdvertiseGame,
    /// `GetMyLobbyPlayer()`.
    MyPlayer,
    /// `LobbyNetSendToCaptain(a)`.
    SendToCaptain,
    /// `LobbyNetBroadcastToGame(a)`.
    BroadcastToGame,
    /// `LobbyNetSendToClient(a, b)`.
    SendToClient,
    /// `LobbyNetRemoveGameAdvertisement(a)`.
    RemoveGameAdvertisement,
    /// `LobbyNetLeaveGame(a)`.
    LeaveGame,
    /// `LobbyNetKickClient(a, b)`.
    KickClient,
    /// `LobbyNetSetGameInProgress(a)`.
    SetGameInProgress,
    /// `LobbyNetSuspend()`.
    Suspend,
    /// `LobbyNetResume()`.
    Resume,
}

use EeWonNetDispCall as C;
use EeWonNetDispOp as Op;

struct Disp<'a> {
    op: &'a dyn Fn(Op, isize, isize, isize) -> isize,
}

impl Disp<'_> {
    fn on(&self, o: Op, a: isize) -> isize {
        (self.op)(o, a, 0, 0)
    }

    /// `LobbyGetNetLogic(type)`.
    fn logic_of(&self, ty: isize) -> isize {
        if self.on(Op::HasData, 0) == 0 {
            return 0;
        }
        match ty as i32 {
            1 | 3 => self.on(Op::LanLogic, 0),
            2 => self.on(Op::RoutingLogic, 0),
            _ => 0,
        }
    }

    /// `LobbyGetCurNetLogic()`.
    fn cur(&self) -> isize {
        if self.on(Op::HasData, 0) == 0 {
            return 0;
        }
        self.logic_of(self.on(Op::CurGameType, 0))
    }

    /// The current logic's virtual `o` with `b`, `c` (nothing without one).
    fn on_cur(&self, o: Op, b: isize, c: isize) {
        let logic = self.cur();
        if logic != 0 {
            (self.op)(o, logic, b, c);
        }
    }

    /// The game's type's logic's virtual `o` with the game.
    fn on_game_logic(&self, o: Op, game: isize) {
        let logic = self.logic_of(self.on(Op::GameType, game));
        if logic != 0 {
            (self.op)(o, logic, game, 0);
        }
    }

    /// `LobbyNetHostGame` / `LobbyNetJoinGame`: the game's type the current one, then its logic's call.
    fn enter_game(&self, o: Op, game: isize) -> isize {
        if self.on(Op::HasData, 0) == 0 {
            return 0;
        }
        self.on(Op::SetCurGameType, self.on(Op::GameType, game));
        let logic = self.cur();
        if logic != 0 {
            return ((self.op)(o, logic, game, 0) != 0) as isize;
        }
        0
    }

    fn call(&self, call: C, a: isize, b: isize) -> isize {
        match call {
            C::NetLogic => return self.logic_of(a),
            C::CurNetLogic => return self.cur(),
            C::HostGame => return self.enter_game(Op::HostGame, a),
            C::JoinGame => return self.enter_game(Op::JoinGame, a),
            C::MyClient => {
                let logic = self.cur();
                if logic == 0 {
                    return 0;
                }
                return self.on(Op::MyClient, logic);
            }
            C::Logout => {
                if self.on(Op::HasData, 0) == 0 {
                    return 0;
                }
                if self.on(Op::OnInternetScreen, 0) != 0 {
                    self.on(Op::RoutingLogout, 0);
                } else if self.on(Op::OnLanScreen, 0) != 0 {
                    self.on(Op::LanLogout, 0);
                }
            }
            C::AdvertiseGame => {
                if self.on(Op::IAmCaptain, 0) == 0 {
                    return 0;
                }
                let logic = self.cur();
                let game = self.on(Op::MyGame, 0);
                if logic != 0 && game != 0 {
                    (self.op)(Op::AdvertiseGame, logic, game, 0);
                }
            }
            C::MyPlayer => {
                let client = self.on(Op::MyLobbyClient, 0);
                return if client != 0 { self.on(Op::ClientPlayer, client) } else { 0 };
            }
            C::SendToCaptain => self.on_cur(Op::SendToCaptain, a, 0),
            C::BroadcastToGame => self.on_cur(Op::BroadcastToGame, a, 0),
            C::SendToClient => self.on_cur(Op::SendToClient, a, b),
            C::RemoveGameAdvertisement => self.on_game_logic(Op::RemoveGameAdvertisement, a),
            C::LeaveGame => self.on_game_logic(Op::LeaveGame, a),
            C::KickClient => self.on_cur(Op::KickClient, a, b),
            C::SetGameInProgress => self.on_cur(Op::SetGameInProgress, a, 0),
            C::Suspend => self.on_cur(Op::Suspend, 0, 0),
            C::Resume => self.on_cur(Op::Resume, 0, 0),
        }
        0
    }
}

/// One of the network entry points over the host `host`.
///
/// # Safety
/// `host` answers the ops (NetHost).
#[no_mangle]
pub unsafe extern "C" fn ee_won_netdisp_call(
    host: EeWonNetDispHostFn,
    call: EeWonNetDispCall,
    a: isize,
    b: isize,
) -> isize {
    let op = |o: Op, a: isize, b: isize, c: isize| match host {
        // SAFETY: the contract
        Some(f) => unsafe { f(o, a, b, c) },
        None => 0,
    };
    Disp { op: &op }.call(call, a, b)
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::cell::{Cell, RefCell};

    struct World {
        ty: Cell<isize>,
        screen: isize,
        log: RefCell<Vec<String>>,
    }

    impl World {
        fn new(ty: isize, screen: isize) -> Self {
            Self { ty: Cell::new(ty), screen, log: RefCell::new(Vec::new()) }
        }
        fn call(&self, call: C, a: isize, b: isize) -> isize {
            let op = |o: Op, a: isize, b: isize, c: isize| -> isize {
                match o {
                    Op::HasData | Op::IAmCaptain => return 1,
                    Op::LanLogic => return 0x100,
                    Op::RoutingLogic => return 0x200,
                    Op::CurGameType => return self.ty.get(),
                    Op::SetCurGameType => {
                        self.ty.set(a);
                        return 0;
                    }
                    Op::GameType => return a & 0xf,
                    Op::OnInternetScreen => return (self.screen == 2) as isize,
                    Op::OnLanScreen => return (self.screen == 1) as isize,
                    Op::MyGame => return 0x31,
                    Op::MyLobbyClient => return 0x40,
                    Op::ClientPlayer => return a + 1,
                    _ => {}
                }
                self.log.borrow_mut().push(format!("{o:?} {a:#x} {b:#x} {c:#x}"));
                7
            };
            Disp { op: &op }.call(call, a, b)
        }
        fn take(&self) -> Vec<String> {
            std::mem::take(&mut *self.log.borrow_mut())
        }
    }

    #[test]
    fn the_logic_by_game_type() {
        let w = World::new(1, 1);
        assert_eq!([0, 1, 2, 3, 4].map(|t| w.call(C::NetLogic, t, 0)), [0, 0x100, 0x200, 0x100, 0]);
        assert_eq!(w.call(C::CurNetLogic, 0, 0), 0x100);
        // hosting an Internet game makes routing the current logic
        assert_eq!(w.call(C::HostGame, 0x52, 0), 1);
        assert_eq!((w.ty.get(), w.take()), (2, vec!["HostGame 0x200 0x52 0x0".to_string()]));
        assert_eq!(w.call(C::JoinGame, 0x34, 0), 0, "type 4 has no logic");
        assert!(w.take().is_empty());
        assert_eq!(w.call(C::MyPlayer, 0, 0), 0x41);
    }

    #[test]
    fn the_forwards() {
        let w = World::new(3, 1);
        w.call(C::SendToClient, 0x40, 0x99);
        w.call(C::KickClient, 0x40, 1);
        w.call(C::LeaveGame, 0x52, 0);
        w.call(C::AdvertiseGame, 0, 0);
        w.call(C::Logout, 0, 0);
        w.call(C::Suspend, 0, 0);
        assert_eq!(
            w.take(),
            vec![
                "SendToClient 0x100 0x40 0x99",
                "KickClient 0x100 0x40 0x1",
                "LeaveGame 0x200 0x52 0x0",
                "AdvertiseGame 0x100 0x31 0x0",
                "LanLogout 0x0 0x0 0x0",
                "Suspend 0x100 0x0 0x0",
            ]
        );
        let w = World::new(0, 2);
        w.call(C::Logout, 0, 0);
        w.call(C::Resume, 0, 0);
        w.call(C::MyClient, 0, 0);
        assert_eq!(w.take(), vec!["RoutingLogout 0x0 0x0 0x0"]);
    }
}
