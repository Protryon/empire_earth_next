//! What LanLogic holds when the game makes it (R6 WONLobby round 11): the client list, the LAN and direct-connect
//! game lists, this player's client (the model's objects, modelobj.rs), the pending sockets, the listen, host and
//! discovery sockets, the network and discovery timers (C++ objects, a reference each), the discovery address and
//! the join password. The C++ shell held them as SmartPtrs; LanLogicHost.cpp's cases over them are here, each the
//! original's statements in order: a call on a model object is the model's (`modelobj::call`), a call on a socket,
//! a timer, an event or the lobby a host op of its own (`EeWonLobbyOp::RefHold` on).
//!
//! The object answers the LanLogic ops `lanobj.rs` makes (the golden's op set, which a scripted host answers in
//! `won_lanobj_test`) in place of the host; `won_lanmodel_test` checks these answers against the C++ cases they
//! replace over the model's objects.

use std::cell::{Cell, RefCell};
use std::ffi::c_void;
use std::ptr::null_mut;

use crate::lanlogic;
use crate::lobbyhost::{EeWonLobbyOp as Op, EeWonLobbyResult, Host};
use crate::modelobj::{self, EeWonModelCall as MC, EeWonModelField as MF};
use ee_won_net::ipobj::EeWonIpAddr;

const NETWORK_TIMER: isize = 0;
const DISCOVERY_TIMER: isize = 1;
const DIALOG_OK: isize = 0x186a1;

/// LanLogic's held members (the original's 0x50, 0x54, 0x58, 0x64, 0x68, 0x70, 0x78, 0x7c, 0x80, 0xac, 0xb0).
pub(crate) struct LanOwned {
    lan_games: isize,
    direct_games: isize,
    pending: RefCell<Vec<isize>>,
    listen: Cell<isize>,
    server: Cell<isize>,
    network_timer: Cell<isize>,
    discovery: Cell<isize>,
    discovery_timer: Cell<isize>,
    discovery_address: RefCell<EeWonIpAddr>,
    clients: isize,
    my_client: Cell<isize>,
    join_password: RefCell<Vec<u16>>,
    /// the last discovery packet's sender (`LanRecvFrom`'s; the original's reference into the op)
    recv_from: RefCell<EeWonIpAddr>,
}

fn b(v: bool) -> isize {
    v as isize
}

/// A take op's bytes.
fn take(h: &Host, op: Op, a: isize) -> Vec<u8> {
    let mut v: Vec<u8> = Vec::new();
    h.op(op, a as *mut c_void, &mut v as *mut Vec<u8> as *mut c_void, 0, 0);
    v
}

impl LanOwned {
    /// The shell's new lists (`new LobbyClientList`, `new LobbyGameList(1)`, `new LobbyGameList(3)`): one reference
    /// each taken.
    pub(crate) fn new(clients: isize, lan_games: isize, direct_games: isize) -> LanOwned {
        for l in [clients, lan_games, direct_games] {
            modelobj::hold(l);
        }
        LanOwned {
            lan_games,
            direct_games,
            pending: RefCell::new(Vec::new()),
            listen: Cell::new(0),
            server: Cell::new(0),
            network_timer: Cell::new(0),
            discovery: Cell::new(0),
            discovery_timer: Cell::new(0),
            discovery_address: RefCell::new(EeWonIpAddr::default()),
            clients,
            my_client: Cell::new(0),
            join_password: RefCell::new(Vec::new()),
            recv_from: RefCell::new(EeWonIpAddr::default()),
        }
    }

    /// The members' references given back in reverse member order (the shell's destructor's).
    pub(crate) fn free(&self, h: &Host) {
        modelobj::assign(&self.my_client, 0);
        modelobj::release(self.clients);
        for slot in [&self.discovery_timer, &self.discovery, &self.network_timer, &self.server, &self.listen] {
            set(h, slot, 0);
        }
        for s in self.pending.take() {
            release(h, s);
        }
        modelobj::release(self.direct_games);
        modelobj::release(self.lan_games);
    }

    pub(crate) fn clients(&self) -> isize {
        self.clients
    }

    /// This player's client's id for the room event (the original read it through a NULL client when a join reply
    /// did not name this player: 0xffff, a new client's id, here).
    fn my_id(&self) -> isize {
        let me = self.my_client.get();
        if me == 0 {
            0xffff
        } else {
            modelobj::get(me, MF::ClientId)
        }
    }

    /// `SmartPtr` slot assignment of a host op's held object: the slot's old one released, the new reference
    /// moved in.
    fn put(&self, h: &Host, slot: &Cell<isize>, held: isize) {
        let old = slot.replace(held);
        release(h, old);
    }

    /// `StartNetworkTimer` / `StartDiscoveryTimer`: the timer made with its completion when there is none, run.
    fn start_timer(&self, h: &Host, slot: &Cell<isize>, which: isize, ms: isize) {
        if slot.get() == 0 {
            let t = h.int(Op::LanNewTimer, which);
            self.put(h, slot, t);
        }
        h.op(Op::LanRunTimer, slot.get() as *mut c_void, null_mut(), ms, 0);
    }

    /// `AddPendingSocket`: the close and receive completions and the receive op, the time, the set.
    fn add_pending(&self, h: &Host, socket: isize) {
        let s = socket as *mut c_void;
        h.on(Op::LanArmSocket, s);
        h.on(Op::LanSocketStamp, s);
        let found = self.pending.borrow().binary_search(&socket);
        if let Err(i) = found {
            hold(h, socket);
            self.pending.borrow_mut().insert(i, socket);
        }
    }

    /// `HandleGameSummary`: adds or updates a LAN game from its summary; an unreadable one is removed but reported
    /// as modified.
    fn game_summary(&self, h: &Host, op: *mut c_void, port: u16) {
        let mut address = self.recv_address(h, op);
        address.set_port(port);
        let mut existed = false;
        let addr = &address as *const EeWonIpAddr as isize;
        let game =
            modelobj::call(self.lan_games, MC::GameListGetOrCreateAddr, addr, &mut existed as *mut bool as isize, 0);
        let read = h.op(Op::LanReadSummary, op, game as *mut c_void, 0, 0);
        if read == EeWonLobbyResult::Threw as isize {
            return;
        }
        let outcome = lanlogic::summary_outcome(read != 0, existed);
        modelobj::hold(game);
        if outcome.keep {
            modelobj::set(game, MF::GameLastSeen, h.get(Op::Now));
        } else {
            modelobj::release(modelobj::call(self.lan_games, MC::GameListRemoveAddr, addr, 0, 0));
        }
        if outcome.event >= 0 {
            h.op(Op::LanGameChanged, game as *mut c_void, null_mut(), outcome.event as isize, 0);
        }
        modelobj::release(game);
    }

    /// The receive op's sender address, copied.
    fn recv_address(&self, h: &Host, op: *mut c_void) -> EeWonIpAddr {
        let mut address = EeWonIpAddr::default();
        h.op(Op::LanRecvFromAddr, op, &mut address as *mut EeWonIpAddr as *mut c_void, 0, 0);
        address
    }

    /// `HandleGameKeepAlive`: a known LAN game marked as just seen.
    fn game_keep_alive(&self, h: &Host, op: *mut c_void, port: u16) {
        let from = self.recv_address(h, op);
        let mut address = from.clone();
        address.set_port(port);
        if address.host() != from.host() {
            return;
        }
        let game = modelobj::call(self.lan_games, MC::GameListGetAddr, &address as *const EeWonIpAddr as isize, 0, 0);
        if game != 0 {
            modelobj::set(game, MF::GameLastSeen, h.get(Op::Now));
        }
    }

    /// `HandleGameRemoved`: a LAN game its host announced as gone removed.
    fn game_removed(&self, h: &Host, op: *mut c_void, port: u16) {
        let from = self.recv_address(h, op);
        let mut address = from.clone();
        address.set_port(port);
        if address.host() != from.host() {
            return;
        }
        let game =
            modelobj::call(self.lan_games, MC::GameListRemoveAddr, &address as *const EeWonIpAddr as isize, 0, 0);
        if game != 0 {
            h.op(Op::LanGameChanged, game as *mut c_void, null_mut(), 1, 0);
        }
        modelobj::release(game);
    }

    /// `StopDiscovery`: the LAN games cleared, the discovery timer and socket closed.
    fn stop_discovery(&self, h: &Host) {
        if modelobj::call(self.lan_games, MC::GameListIsEmpty, 0, 0, 0) == 0 {
            modelobj::call(self.lan_games, MC::GameListClear, 0, 0, 0);
            h.on(Op::LanSyncGameList, self.lan_games as *mut c_void);
        }
        let t = self.discovery_timer.get();
        if t != 0 {
            h.on(Op::LanTimerKill, t as *mut c_void);
            set(h, &self.discovery_timer, 0);
        }
        let s = self.discovery.get();
        if s != 0 {
            h.on(Op::LanSocketKill, s as *mut c_void);
            set(h, &self.discovery, 0);
        }
    }

    /// `CloseConnections`' sockets: the listen socket killed, the host socket closed, the network timer killed,
    /// every pending socket closed, this player's client, the socket set and the client list cleared.
    fn close_sockets(&self, h: &Host) {
        let s = self.listen.get();
        if s != 0 {
            h.on(Op::LanSocketKill, s as *mut c_void);
            set(h, &self.listen, 0);
        }
        let s = self.server.get();
        if s != 0 {
            h.op(Op::LanSocketClose, s as *mut c_void, null_mut(), -1, 0);
            set(h, &self.server, 0);
        }
        let t = self.network_timer.get();
        if t != 0 {
            h.on(Op::LanTimerKill, t as *mut c_void);
            set(h, &self.network_timer, 0);
        }
        let pending = self.pending.borrow().clone();
        for s in pending {
            h.op(Op::LanSocketClose, s as *mut c_void, null_mut(), -1, 0);
        }
        modelobj::assign(&self.my_client, 0);
        for s in self.pending.take() {
            release(h, s);
        }
        modelobj::call(self.clients, MC::ListClear, 0, 0, 0);
    }

    /// `EnterLanScreen`'s lists: no current user, the games cleared, the saved direct-connect games, their sync.
    fn enter_screen_lists(&self, h: &Host) {
        h.get(Op::LanNoCurrentUser);
        modelobj::call(self.lan_games, MC::GameListClear, 0, 0, 0);
        modelobj::call(self.direct_games, MC::GameListClear, 0, 0, 0);
        let mut i = 0;
        while i < h.get(Op::LanDirectCount) {
            let mut address = EeWonIpAddr::default();
            let addr = &mut address as *mut EeWonIpAddr;
            h.op(Op::LanDirectAddressAt, addr as *mut c_void, null_mut(), i, 0);
            let game = modelobj::call(self.direct_games, MC::GameListGetOrCreateAddr, addr as isize, 0, 0);
            h.op(Op::LanDirectNameAt, game as *mut c_void, null_mut(), i, 0);
            i += 1;
        }
        h.on(Op::LanSyncGameList, self.direct_games as *mut c_void);
    }

    /// `HandleChangeDirectConnect`'s add: an address asked for (and resolved), the game added or renamed and saved.
    fn direct_add(&self, h: &Host) {
        let mut address = EeWonIpAddr::default();
        let addr = &mut address as *mut EeWonIpAddr;
        if h.on(Op::LanDirectAsk, addr as *mut c_void) == 0 {
            return;
        }
        let mut existed = false;
        let game = modelobj::call(
            self.direct_games,
            MC::GameListGetOrCreateAddr,
            addr as isize,
            &mut existed as *mut bool as isize,
            0,
        );
        h.op(Op::LanDirectSave, game as *mut c_void, addr as *mut c_void, 0, 0);
        h.op(Op::LanGameChanged, game as *mut c_void, null_mut(), lanlogic::direct_event(existed) as isize, 0);
    }

    /// One of the LanLogic ops the shell answered from its members: the answer, or `None` for the host's.
    #[allow(clippy::not_unsafe_ptr_arg_deref)]
    pub(crate) fn op(&self, h: &Host, op: Op, a: *mut c_void, bp: *mut c_void, c: isize, d: isize) -> Option<isize> {
        let ai = a as isize;
        Some(match op {
            Op::LanMyClient => self.my_client.get(),
            Op::LanSetMyClient => {
                modelobj::assign(&self.my_client, ai);
                0
            }
            Op::LanListenSocket => self.listen.get(),
            Op::LanServerSocket => self.server.get(),
            Op::LanNetworkTimer => self.network_timer.get(),
            Op::LanDiscoveryTimer => self.discovery_timer.get(),
            Op::LanDiscoverySocket => self.discovery.get(),
            Op::LanDiscoveryAddress => self.discovery_address.as_ptr() as isize,
            Op::LanCloseSockets => {
                self.close_sockets(h);
                0
            }
            Op::LanForgetSocket => {
                let found = self.pending.borrow().binary_search(&ai);
                if let Ok(i) = found {
                    let s = self.pending.borrow_mut().remove(i);
                    release(h, s);
                }
                0
            }
            Op::LanLoseHost => {
                h.on(Op::LanSocketKill, self.server.get() as *mut c_void);
                set(h, &self.server, 0);
                h.get(Op::LanGameDisconnected);
                0
            }
            Op::LanPendingCount => self.pending.borrow().len() as isize,
            Op::LanPendingAt => self.pending.borrow().get(c.max(0) as usize).copied().unwrap_or(0),
            Op::LanStartNetworkTimer => {
                self.start_timer(h, &self.network_timer, NETWORK_TIMER, 120000);
                0
            }
            Op::LanStartDiscoveryTimer => {
                self.start_timer(h, &self.discovery_timer, DISCOVERY_TIMER, 15000);
                0
            }
            Op::LanAddClient => {
                modelobj::set(ai, MF::ClientId, c as u16 as isize);
                modelobj::call(self.clients, MC::ListAdd, ai, 0, 0);
                0
            }
            Op::LanClientCount => modelobj::call(self.clients, MC::ListCount, 0, 0, 0),
            Op::LanClientAt => modelobj::call(self.clients, MC::ListAt, c, 0, 0),
            Op::LanFindClient => modelobj::call(self.clients, MC::ListFind, c as u16 as isize, 0, 0),
            Op::LanRemoveClient => modelobj::call(self.clients, MC::ListRemove, c as u16 as isize, 0, 0),
            Op::LanServerClientIsCaptain => {
                h.op(Op::LanSocketSetClient, self.server.get() as *mut c_void, a, 0, 0);
                modelobj::call(ai, MC::ClientSetCaptain, 1, 1, 0);
                0
            }
            Op::LanAddPending => {
                self.add_pending(h, ai);
                0
            }
            Op::LanServerSend => h.op(Op::LanSocketSend, a, self.server.get() as *mut c_void, c, d),
            Op::LanClearLanGames => {
                modelobj::call(self.lan_games, MC::GameListClear, 0, 0, 0);
                h.on(Op::LanSyncGameList, self.lan_games as *mut c_void);
                0
            }
            Op::LanPingDirectGames => {
                modelobj::call(self.direct_games, MC::GameListRewind, 0, 0, 0);
                while modelobj::call(self.direct_games, MC::GameListHasMore, 0, 0, 0) != 0 {
                    let game = modelobj::call(self.direct_games, MC::GameListNext, 0, 0, 0);
                    if modelobj::get(game, MF::GameFiltered) == 0 {
                        h.on(Op::LanPingGame, game as *mut c_void);
                    }
                }
                h.int(Op::BroadcastLobbyEvent, 0x186da);
                0
            }
            Op::LanEnterScreenLists => {
                self.enter_screen_lists(h);
                0
            }
            Op::LanHostMyClient => {
                let name = take(h, Op::LanNameTake, 0);
                let units = name.len() / 2;
                let client = h.op(Op::LanNewClient, null_mut(), null_mut(), name.as_ptr() as isize, units as isize);
                modelobj::assign(&self.my_client, client);
                release_client(h, client);
                let me = self.my_client.get();
                modelobj::set(me, MF::ClientId, c as u16 as isize);
                modelobj::call(me, MC::ClientSetCaptain, 1, 1, 0);
                modelobj::call(self.clients, MC::ListAdd, me, 0, 0);
                modelobj::set(ai, MF::GameClients, self.clients);
                0
            }
            Op::LanHostListen => {
                let s = h.get(Op::LanNewSocket);
                self.put(h, &self.listen, s);
                let id = self.my_id();
                if h.op(Op::LanListen, s as *mut c_void, a, 0, 0) == 0 {
                    return Some(0);
                }
                h.op(Op::LanJoinedRoom, a, null_mut(), id, 0);
                1
            }
            Op::LanSendSummary => {
                // the head (the message's bytes), then the game's summary
                // SAFETY: the head's bytes (Rust's, live through the call)
                let mut bytes = unsafe { std::slice::from_raw_parts(c as *const u8, d.max(0) as usize) }.to_vec();
                bytes.extend_from_slice(&take(h, Op::LanSummaryTake, ai));
                let s = self.discovery.get() as *mut c_void;
                h.op(Op::LanSendTo, s, bp, bytes.as_ptr() as isize, bytes.len() as isize)
            }
            Op::LanDiscoverySend => h.op(Op::LanSendTo, self.discovery.get() as *mut c_void, bp, c, d),
            Op::LanRemoveStaleGames => {
                let now = h.get(Op::Now);
                if modelobj::call(self.lan_games, MC::GameListRemoveStale, 30, now, 0) != 0 {
                    h.on(Op::LanSyncGameList, self.lan_games as *mut c_void);
                }
                0
            }
            Op::LanDiscoveryOk => b(h.op(Op::LanOpOnSocket, a, self.discovery.get() as *mut c_void, 0, 0) != 0),
            Op::LanGameSummary => {
                self.game_summary(h, a, c as u16);
                0
            }
            Op::LanGameKeepAlive => {
                self.game_keep_alive(h, a, c as u16);
                0
            }
            Op::LanGameRemoved => {
                self.game_removed(h, a, c as u16);
                0
            }
            Op::LanRecvFrom => {
                *self.recv_from.borrow_mut() = self.recv_address(h, a);
                self.recv_from.as_ptr() as isize
            }
            Op::LanOpenDiscovery => {
                let mut status: isize = 0;
                let addr = self.discovery_address.as_ptr() as *mut c_void;
                let s = h.op(Op::LanNewDiscovery, addr, &mut status as *mut isize as *mut c_void, 0, 0);
                self.put(h, &self.discovery, s);
                status
            }
            Op::LanDiscoveryArm => h.on(Op::LanArmDiscovery, self.discovery.get() as *mut c_void),
            Op::LanStopDiscovery => {
                self.stop_discovery(h);
                0
            }
            Op::LanClearDirectGames => modelobj::call(self.direct_games, MC::GameListClear, 0, 0, 0),
            Op::LanNewServerSocket => {
                let s = h.get(Op::LanNewSocket);
                self.put(h, &self.server, s);
                0
            }
            Op::LanJoinPasswordClear => {
                self.join_password.borrow_mut().clear();
                0
            }
            Op::LanPasswordDialogRun => {
                let r = h.get(Op::LanPasswordDialog);
                if r == DIALOG_OK {
                    let bytes = take(h, Op::LanPasswordTake, 0);
                    *self.join_password.borrow_mut() =
                        bytes.as_chunks::<2>().0.iter().map(|p| u16::from_le_bytes(*p)).collect();
                }
                r
            }
            Op::LanConnect => h.op(Op::LanConnectTo, a, self.server.get() as *mut c_void, c, 0),
            Op::LanJoinPasswordTake => {
                let bytes: Vec<u8> = self.join_password.borrow().iter().flat_map(|u| u.to_le_bytes()).collect();
                // SAFETY: the take op's sink (a Rust buffer)
                unsafe { crate::lobbyhost::ee_won_lobby_sink(bp, bytes.as_ptr(), bytes.len()) };
                0
            }
            Op::LanRecvReplyTake => h.op(Op::LanRecvReply, self.server.get() as *mut c_void, bp, 0, 0),
            Op::LanJoinArm => {
                h.on(Op::LanArmSocket, self.server.get() as *mut c_void);
                self.start_timer(h, &self.network_timer, NETWORK_TIMER, 120000);
                modelobj::set(ai, MF::GameClients, self.clients);
                let id = self.my_id();
                h.op(Op::LanJoinedRoom, a, null_mut(), id, 0);
                0
            }
            Op::LanDirectRemove => {
                h.on(Op::LanRemoveSavedDirect, a);
                // SAFETY: the game's address (a copy, as `GetAddress()` returned)
                let address = unsafe { (*(modelobj::get(ai, MF::GameAddress) as *const EeWonIpAddr)).clone() };
                let addr = &address as *const EeWonIpAddr as isize;
                modelobj::release(modelobj::call(self.direct_games, MC::GameListRemoveAddr, addr, 0, 0));
                h.op(Op::LanGameChanged, a, null_mut(), 1, 0);
                0
            }
            Op::LanDirectAdd => {
                self.direct_add(h);
                0
            }
            Op::LanShowClientOptions => h.op(Op::LanClientOptions, a, self.my_client.get() as *mut c_void, c, 0),
            _ => return None,
        })
    }
}

/// A host object's reference (a socket's, a timer's).
fn hold(h: &Host, p: isize) {
    if p != 0 {
        h.on(Op::RefHold, p as *mut c_void);
    }
}
fn release(h: &Host, p: isize) {
    if p != 0 {
        h.on(Op::RefRelease, p as *mut c_void);
    }
}
/// A `SmartPtr` slot of a host object set (`p` gets a reference; the old one released).
fn set(h: &Host, slot: &Cell<isize>, p: isize) {
    let old = slot.get();
    if old != p {
        release(h, old);
        hold(h, p);
        slot.set(p);
    }
}
/// A held client's reference given back (`ClientRelease`, as the shell's `SmartPtr` did).
fn release_client(h: &Host, client: isize) {
    if client != 0 {
        h.on(Op::ClientRelease, client as *mut c_void);
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::lanobj::{ee_won_lan_call, ee_won_lan_free, ee_won_lan_new_model, EeWonLanCall};
    use crate::modelobj::{ee_won_model_free, ee_won_model_new, EeWonModel, EeWonModelKind, EeWonModelOp};
    use std::cell::RefCell as TestCell;

    /// A stand-in C++ shell (the model at its third word), or a counted socket when `model` is NULL.
    #[repr(C)]
    struct Shell {
        vt: usize,
        rc: i32,
        pad: i32,
        model: *mut EeWonModel,
    }

    thread_local! {
        static OPS: TestCell<Vec<(Op, isize, isize, isize)>> = const { TestCell::new(Vec::new()) };
        static ANSWER: Cell<isize> = const { Cell::new(0) };
    }

    fn shell(kind: Option<EeWonModelKind>) -> isize {
        let s = Box::into_raw(Box::new(Shell { vt: 0, rc: 0, pad: 0, model: null_mut() }));
        if let Some(k) = kind {
            // SAFETY: a new shell
            unsafe { (*s).model = ee_won_model_new(s as *mut c_void, k, model_host, empty() as *mut c_void) };
        }
        s as isize
    }
    fn empty() -> isize {
        thread_local!(static E: Cell<isize> = const { Cell::new(0) });
        E.with(|e| {
            if e.get() == 0 {
                let s = Box::into_raw(Box::new(Shell { vt: 0, rc: 1000, pad: 0, model: null_mut() }));
                e.set(s as isize);
            }
            e.get()
        })
    }
    fn rc(p: isize) -> i32 {
        // SAFETY: a live shell
        unsafe { (*(p as *mut Shell)).rc }
    }
    fn add(p: isize, n: i32) {
        // SAFETY: a live shell; one at 0 is freed as the C++'s Release deletes
        unsafe {
            let s = p as *mut Shell;
            (*s).rc += n;
            if (*s).rc <= 0 {
                if !(*s).model.is_null() {
                    ee_won_model_free((*s).model);
                }
                (*s).vt = 0xdead;
            }
        }
    }
    unsafe extern "C" fn model_host(op: EeWonModelOp, a: isize, _b: isize, _c: isize) -> isize {
        match op {
            EeWonModelOp::AddRef => add(a, 1),
            EeWonModelOp::Release => add(a, -1),
            _ => {}
        }
        0
    }
    unsafe extern "C" fn host(_l: *mut c_void, op: Op, a: *mut c_void, b: *mut c_void, c: isize, _d: isize) -> isize {
        OPS.with(|o| o.borrow_mut().push((op, a as isize, b as isize, c)));
        match op {
            Op::RefHold => add(a as isize, 1),
            Op::RefRelease => add(a as isize, -1),
            Op::LanNewSocket | Op::LanNewTimer => {
                let s = shell(None);
                add(s, 1);
                return s;
            }
            Op::LanNewClient => {
                let s = shell(Some(EeWonModelKind::Client));
                add(s, 1);
                return s;
            }
            Op::ClientRelease => add(a as isize, -1),
            _ => return ANSWER.with(|x| x.get()),
        }
        0
    }
    fn ops() -> Vec<Op> {
        OPS.with(|o| o.borrow_mut().drain(..).map(|e| e.0).collect())
    }

    fn lists() -> (isize, isize, isize) {
        (
            shell(Some(EeWonModelKind::ClientList)),
            shell(Some(EeWonModelKind::GameList)),
            shell(Some(EeWonModelKind::GameList)),
        )
    }

    #[test]
    fn hosting_holds_the_client_and_closing_gives_everything_back() {
        let (clients, lan, direct) = lists();
        let game = shell(Some(EeWonModelKind::Game));
        add(game, 1);
        // SAFETY: live shells; the object is freed once
        unsafe {
            let l = ee_won_lan_new_model(null_mut(), Some(host), clients as _, lan as _, direct as _);
            assert_eq!((rc(clients), rc(lan), rc(direct)), (1, 1, 1));
            ANSWER.with(|x| x.set(1));
            assert_eq!(ee_won_lan_call(l, EeWonLanCall::HostGame, game as _, null_mut(), 0, 0), 1);
            let me = ee_won_lan_call(l, EeWonLanCall::MyClient, null_mut(), null_mut(), 0, 0);
            assert_ne!(me, 0);
            // the player's client: the slot's reference and the list's
            assert_eq!(rc(me), 2);
            assert_eq!(modelobj::call(clients, MC::ListCount, 0, 0, 0), 1);
            assert_eq!(modelobj::get(game, MF::GameClients), clients);
            assert_eq!(ee_won_lan_call(l, EeWonLanCall::ClientList, null_mut(), null_mut(), 0, 0), clients);
            let seen = ops();
            assert!(
                seen.contains(&Op::LanListen) && seen.contains(&Op::LanJoinedRoom) && seen.contains(&Op::LanRunTimer)
            );
            // the pending sockets in address order, one reference each
            let (s1, s2) = (shell(None), shell(None));
            let (lo, hi) = if s1 < s2 { (s1, s2) } else { (s2, s1) };
            ee_won_lan_call(l, EeWonLanCall::AddPending, hi as _, null_mut(), 0, 0);
            ee_won_lan_call(l, EeWonLanCall::AddPending, lo as _, null_mut(), 0, 0);
            ee_won_lan_call(l, EeWonLanCall::AddPending, lo as _, null_mut(), 0, 0);
            assert_eq!((rc(lo), rc(hi)), (1, 1));
            ops();
            ee_won_lan_call(l, EeWonLanCall::LeaveGame, null_mut(), null_mut(), 0, 0);
            let closes: Vec<_> = OPS.with(|o| o.borrow().clone());
            let order: Vec<isize> = closes.iter().filter(|e| e.0 == Op::LanSocketClose).map(|e| e.1).collect();
            assert_eq!(order, vec![lo, hi]);
            assert_eq!((rc(lo), rc(hi)), (0, 0));
            assert_eq!(rc(me), 0);
            assert_eq!(modelobj::call(clients, MC::ListCount, 0, 0, 0), 0);
            ee_won_lan_free(l);
        }
        // the game held the client list (its clients)
        assert_eq!((rc(clients), rc(lan), rc(direct)), (1, 0, 0));
        add(game, -1);
        assert_eq!(rc(clients), 0);
    }

    #[test]
    fn stopping_discovery_without_games_syncs_nothing() {
        let (clients, lan, direct) = lists();
        // SAFETY: live shells; the object is freed once
        unsafe {
            let l = ee_won_lan_new_model(null_mut(), Some(host), clients as _, lan as _, direct as _);
            ops();
            ANSWER.with(|x| x.set(0));
            ee_won_lan_call(l, EeWonLanCall::Logout, null_mut(), null_mut(), 0, 0);
            assert!(!ops().contains(&Op::LanSyncGameList));
            ee_won_lan_free(l);
        }
    }
}
