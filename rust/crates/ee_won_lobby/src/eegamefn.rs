//! The EE lobby's network game functions in the game (R6 WONLobby round 30): EELobbyGlueHost.cpp's
//! EELobbyHostGame, EELobbyJoinGame, EELobbyStartHostedGame, EELobbyRunSierraUpdate and EEGetLocalAddress, their
//! decisions and counts here, each C++ statement one of the glue host's ops (`EeWonEeGlueOp` from
//! `SetPlayerNameFromClient` on). The game's network objects (`gCommunications`, the server and the client), its
//! options and the functions' locals that are C++ objects (the player made, the join's strings and request, the
//! update's file and message) stay the host's. won_eewindow_test runs the deleted functions against these over its
//! stand-in world.

use super::{EeWonEeGlue, EeWonEeGlueOp as Op};

impl EeWonEeGlue {
    /// EELobbyHostGame: the game's server made in place of any network object (false when its game was not
    /// published), then the host's own connection request with the lobby player's options.
    pub(crate) fn host_game(&self) -> bool {
        self.op1(Op::HostMeNew, 0);
        self.op1(Op::SetPlayerNameFromClient, 0);
        self.sync_network_adapter();
        let behind_firewall = self.op1(Op::BehindFirewall, 0) != 0;
        if self.op1(Op::CommExists, 0) != 0 {
            self.op1(Op::CommDelete, 0);
            self.op1(Op::CommClear, 0);
        }
        self.op1(Op::ServerNew, 0);
        self.op1(Op::ServerStartThreads, 0);
        if self.op1(Op::ServerPublished, 0) == 0 {
            self.op1(Op::CommDelete, 0);
            self.op1(Op::CommClear, 0);
            return false;
        }
        self.fill_game_options(self.op1(Op::HostMe, 0));
        self.op1(Op::HostMeReady, 0);
        self.op1(Op::HostMeHuman, 0);
        self.op1(Op::HostRequest, behind_firewall as isize);
        true
    }

    /// EELobbyJoinGame: a game client (a server is replaced), the lobby's options as the game's, then a connection
    /// to the lobby game's host and the connection request.
    pub(crate) fn join_game(&self) {
        self.op1(Op::SetPlayerNameFromClient, 0);
        self.sync_network_adapter();
        let behind_firewall = self.op1(Op::BehindFirewall, 0) != 0;
        if self.op1(Op::CommExists, 0) == 0 {
            self.op1(Op::ClientNew, 0);
            self.op1(Op::ClientStartThreads, 0);
        } else if self.op1(Op::CommIsServer, 0) != 0 {
            self.op1(Op::CommDelete, 0);
            self.op1(Op::ClientNew, 0);
            self.op1(Op::ClientStartThreadsDynamic, 0);
        }
        if self.op1(Op::CommExists, 0) == 0 {
            return;
        }
        self.op1(Op::OptionsBegin, 0);
        self.op1(Op::OptionsEnd, 0);
        self.op1(Op::JoinSetService, 0);
        self.op1(Op::JoinSetGameName, 0);
        self.op1(Op::JoinSetAddress, 0);
        self.op1(Op::JoinSetName, 0);
        if self.op1(Op::JoinConnect, 0) != 0 {
            self.op1(Op::JoinRequestNew, behind_firewall as isize);
            self.op1(Op::JoinSendStart, 0);
            self.op1(Op::JoinProcessRequest, 0);
        }
    }

    /// EELobbyStartHostedGame: the server closed to new players and loading, the human players counted, the
    /// start-loading message sent with a fresh random seed, the options kept as the last game's.
    pub(crate) fn start_hosted_game(&self) {
        let mut humans = 0u8;
        self.op1(Op::OptionsReadLock, 0);
        self.op1(Op::StartTimeSet, 0);
        self.op1(Op::ServerAllowNewPlayersOff, 0);
        self.op1(Op::ServerSetLoadingState, 0);
        self.op1(Op::ServerClearLoadResponses, 0);
        for i in 0..16 {
            self.op1(Op::StartPlayerCopy, i);
            let kind = self.op1(Op::StartPlayerType, 0);
            if kind == 1 || kind == 2 {
                humans = humans.wrapping_add(1);
            }
        }
        self.op1(Op::ServerStopPing, 0);
        self.op1(Op::ServerResetXfer, 0);
        self.op1(Op::ServerSetLoadState, 0);
        self.op1(Op::ServerAllowNewPlayersOff, 0);
        self.op1(Op::SeedRandom, 0);
        self.op1(Op::StartLoading, humans as isize);
        self.op1(Op::SetLastGameOptions, 0);
        self.op1(Op::OptionsReadUnlock, 0);
    }

    /// EELobbyRunSierraUpdate: the update program launched and the game told to quit, or a message that it could
    /// not be launched.
    pub(crate) fn run_sierra_update(&self) {
        let file = self.op1(Op::SierraFileNew, 0);
        let message = self.op1(Op::MessageNew, 0);
        self.op1(Op::SierraSetVolume, file);
        self.op1(Op::SierraSetDirectory, file);
        self.op1(Op::SierraSetFile, file);
        let launched = self.op1(Op::SierraOpen, file) != 0 && {
            self.op1(Op::SierraClose, file);
            self.op1(Op::SierraLaunch, file) != 0
        };
        if launched {
            if self.op1(Op::HasUserInterface, 0) != 0 {
                self.op1(Op::PostQuit, 0);
            }
        } else {
            self.op1(Op::SierraFailMessage, message);
            self.op1(Op::MessageShow, message);
        }
        self.op1(Op::MessageFree, message);
        self.op1(Op::SierraFileFree, file);
    }

    /// EEGetLocalAddress: the lobby game's host address in a multiplayer game with a lobby game, else the protocol's
    /// own; the `UWideString&` the C++ returns (as an address).
    pub(crate) fn local_address(&self, protocol: isize, adapter: isize) -> isize {
        let game = if self.op1(Op::IsMultiplayerGame, 0) != 0 { self.op1(Op::MyGame, 0) } else { 0 };
        if game == 0 {
            return self.op2(Op::ProtocolLocalAddress, protocol, adapter);
        }
        self.op1(Op::GameAddressSet, game);
        self.op1(Op::GameAddressRef, 0)
    }
}

#[cfg(test)]
mod tests {
    use super::super::{EeWonEeGlue, EeWonEeGlueOp as Op};
    use std::cell::RefCell;
    use std::sync::atomic::Ordering;

    thread_local! {
        static LOG: RefCell<Vec<Op>> = const { RefCell::new(Vec::new()) };
    }

    // no network object, a published server, players 0..3 human (type 1), 4 a computer (type 2), the rest open
    unsafe extern "C" fn host(op: Op, a: isize, _: isize, _: isize, _: isize, _: isize, _: isize) -> isize {
        LOG.with(|l| l.borrow_mut().push(op));
        match op {
            Op::ServerPublished | Op::SierraOpen => 1,
            Op::StartPlayerCopy => {
                LOG.with(|l| l.borrow_mut().pop());
                PLAYER.with(|p| *p.borrow_mut() = a);
                0
            }
            Op::StartPlayerType => match PLAYER.with(|p| *p.borrow()) {
                0..=3 => 1,
                4 => 2,
                _ => 0,
            },
            Op::StartLoading => {
                HUMANS.with(|h| *h.borrow_mut() = a);
                0
            }
            _ => 0,
        }
    }
    thread_local! {
        static PLAYER: RefCell<isize> = const { RefCell::new(0) };
        static HUMANS: RefCell<isize> = const { RefCell::new(-1) };
    }

    fn glue() -> EeWonEeGlue {
        let g = EeWonEeGlue::new();
        let f: unsafe extern "C" fn(Op, isize, isize, isize, isize, isize, isize) -> isize = host;
        g.host.store(f as usize, Ordering::Relaxed);
        g
    }

    #[test]
    fn hosting_counts_the_players_and_launch_failures_show_the_message() {
        let g = glue();
        assert!(g.host_game());
        let log = LOG.with(|l| std::mem::take(&mut *l.borrow_mut()));
        assert_eq!(log.first(), Some(&Op::HostMeNew));
        assert_eq!(log.last(), Some(&Op::HostRequest));
        assert!(!log.contains(&Op::CommDelete));
        g.start_hosted_game();
        assert_eq!(HUMANS.with(|h| *h.borrow()), 5);
        LOG.with(|l| l.borrow_mut().clear());
        // the file opens and the launch answers 0: the message, then the frees
        g.run_sierra_update();
        let log = LOG.with(|l| std::mem::take(&mut *l.borrow_mut()));
        let tail = &log[log.len() - 4..];
        assert_eq!(tail, [Op::SierraFailMessage, Op::MessageShow, Op::MessageFree, Op::SierraFileFree]);
        assert!(log.contains(&Op::SierraClose));
        // no multiplayer game: the protocol's address
        g.local_address(0x10, 2);
        let log = LOG.with(|l| std::mem::take(&mut *l.borrow_mut()));
        assert_eq!(log, [Op::IsMultiplayerGame, Op::ProtocolLocalAddress]);
    }
}
