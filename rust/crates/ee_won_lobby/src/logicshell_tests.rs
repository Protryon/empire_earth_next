//! The shell's routes against the six classes' methods as they were (WONLobbyAccountLogic.h, WONLobbyDialogLogic.h,
//! WONLobbyRoutingLogic.h, WONLobbyStaging.h, WONLobbyLan.h and their units at 71c097464). The C++ golden is
//! won_shell36_test (the classes as they were, their calls logged).

use super::*;
use EeWonLogicShellDone as D;
use EeWonLogicShellKind as K;
use EeWonLogicShellV as V;

const KINDS: [K; 6] = [K::Account, K::Dialog, K::Init, K::Routing, K::Staging, K::Lan];
const VIRTUALS: [V; 19] = [
    V::KillHook,
    V::InitComponent,
    V::Reset,
    V::HandleControlEvent,
    V::HandleComponentEvent,
    V::HostGame,
    V::JoinGame,
    V::GetMyClient,
    V::SendToCaptain,
    V::BroadcastToGame,
    V::SendToClient,
    V::AdvertiseGame,
    V::RemoveGameAdvertisement,
    V::KickClient,
    V::SetGameInProgress,
    V::LeaveGame,
    V::Suspend,
    V::Resume,
    V::Logout,
];
const DONES: [D; 19] = [
    D::InitDns,
    D::InitDocument,
    D::InitPromotionImage,
    D::InitDirectory,
    D::InitVersion,
    D::InitAccept,
    D::InitFirewall,
    D::RouteUserStatus,
    D::RouteGroups,
    D::RouteClosed,
    D::RouteNotify,
    D::LanSocketClosed,
    D::LanNetworkTimer,
    D::LanRecvMsg,
    D::LanDiscoveryTimer,
    D::LanDiscoveryRecv,
    D::LanAccept,
    D::LanPeerAuthDone,
    D::DlgStatusOp,
];

fn r(kind: K, v: V) -> Route {
    route(kind, v, 11, 22, 33, 44)
}

#[test]
fn the_account_and_init_logics_virtuals() {
    // AccountLogic: HandleControlEvent(id) and HandleComponentEvent(event, its type); InitComponent only looked the
    // dialogs up; the rest LobbyLogic's empty virtuals
    assert_eq!(r(K::Account, V::HandleControlEvent), Route::Logic(Lc::AcctControlId, 11, 0));
    assert_eq!(r(K::Account, V::HandleComponentEvent), Route::Logic(Lc::AcctEvent, 11, 22));
    // InitLogic: KillHook, HandleControlEvent(id), HandleComponentEvent(the type alone)
    assert_eq!(r(K::Init, V::KillHook), Route::Logic(Lc::InitKillHook, 0, 0));
    assert_eq!(r(K::Init, V::HandleControlEvent), Route::Logic(Lc::InitControlId, 11, 0));
    assert_eq!(r(K::Init, V::HandleComponentEvent), Route::Logic(Lc::InitEvent, 22, 0));
    for (kind, own) in [
        (K::Account, &[V::HandleControlEvent, V::HandleComponentEvent][..]),
        (K::Init, &[V::KillHook, V::HandleControlEvent, V::HandleComponentEvent][..]),
    ] {
        for v in VIRTUALS {
            if !own.contains(&v) {
                assert_eq!(r(kind, v), Route::Answer(0), "{kind:?} {v:?}");
            }
        }
    }
}

#[test]
fn the_dialog_logics_virtuals() {
    assert_eq!(r(K::Dialog, V::KillHook), Route::Dlg(Dc::KillHook, 0));
    assert_eq!(r(K::Dialog, V::InitComponent), Route::Dlg(Dc::Init, 11));
    assert_eq!(r(K::Dialog, V::Reset), Route::Dlg(Dc::Reset, 0));
    for v in &VIRTUALS[3..] {
        assert_eq!(r(K::Dialog, *v), Route::Answer(0), "{v:?}");
    }
}

#[test]
fn the_routing_logics_virtuals() {
    let want = [
        (V::KillHook, Route::Answer(0)),
        (V::InitComponent, Route::Answer(0)),
        (V::Reset, Route::Routing(Rc::Reset, 0, 0)),
        (V::HandleControlEvent, Route::Routing(Rc::ControlId, 11, 0)),
        (V::HandleComponentEvent, Route::Routing(Rc::Event, 11, 22)),
        (V::HostGame, Route::Routing(Rc::HostGame, 11, 0)),
        (V::JoinGame, Route::Routing(Rc::JoinGame, 11, 0)),
        (V::GetMyClient, Route::Routing(Rc::MyClient, 0, 0)),
        (V::SendToCaptain, Route::Routing(Rc::SendToCaptain, 11, 0)),
        (V::BroadcastToGame, Route::Routing(Rc::BroadcastToGame, 11, 0)),
        // SendToClient(client, data): the shell's message arguments are the buffer, then the client
        (V::SendToClient, Route::Routing(Rc::SendToClient, 22, 11)),
        (V::AdvertiseGame, Route::Routing(Rc::Advertise, 11, 0)),
        (V::RemoveGameAdvertisement, Route::Routing(Rc::RemoveAdvertisement, 0, 0)),
        (V::KickClient, Route::Routing(Rc::KickClient, 11, 22)),
        (V::SetGameInProgress, Route::Routing(Rc::SetGameInProgress, 11, 0)),
        (V::LeaveGame, Route::Routing(Rc::LeaveGame, 0, 0)),
        (V::Suspend, Route::Routing(Rc::Suspend, 0, 0)),
        (V::Resume, Route::Routing(Rc::Resume, 0, 0)),
        (V::Logout, Route::Routing(Rc::Logout, 0, 0)),
    ];
    assert_eq!(want.len(), VIRTUALS.len());
    for (v, w) in want {
        assert_eq!(r(K::Routing, v), w, "{v:?}");
    }
}

#[test]
fn the_staging_logics_virtuals_and_buttons() {
    assert_eq!(staging_control(ID_HostGame as isize), 1);
    assert_eq!(staging_control(ID_LeaveGame as isize), 2);
    assert_eq!(staging_control(ID_Logout as isize), 3);
    assert_eq!(staging_control(ID_StartGame as isize), 4);
    assert_eq!(staging_control(ID_ShowLanScreen as isize), 0);
    assert_eq!(staging_control(0), 0);
    for (id, which) in [(ID_HostGame, 1), (ID_LeaveGame, 2), (ID_Logout, 3), (ID_StartGame, 4)] {
        assert_eq!(
            route(K::Staging, V::HandleControlEvent, id as isize, 0, 0, 0),
            Route::Staging(Sc::Control, 0, 0, which, 0)
        );
    }
    // another control: no call at all
    assert_eq!(route(K::Staging, V::HandleControlEvent, ID_RefreshGameList as isize, 0, 0, 0), Route::Answer(0));
    assert_eq!(r(K::Staging, V::HandleComponentEvent), Route::Staging(Sc::ComponentEvent, 11, 0, 22, 0));
    for v in VIRTUALS {
        if v != V::HandleControlEvent && v != V::HandleComponentEvent {
            assert_eq!(r(K::Staging, v), Route::Answer(0), "{v:?}");
        }
    }
}

#[test]
fn the_lan_logics_virtuals_and_commands() {
    for (id, which) in [(ID_AddLANName, 1), (ID_ChangeLANBroadcast, 2), (ID_RefreshGameList, 3), (ID_ShowLanScreen, 4)]
    {
        assert_eq!(lan_control(id as isize), which);
        assert_eq!(route(K::Lan, V::HandleControlEvent, id as isize, 0, 0, 0), Route::Lan(Nc::Control, 0, 0, which, 0));
    }
    assert_eq!(lan_control(ID_HostGame as isize), 0);
    assert_eq!(route(K::Lan, V::HandleControlEvent, ID_HostGame as isize, 0, 0, 0), Route::Answer(0));
    let want = [
        (V::KillHook, Route::Answer(0)),
        (V::InitComponent, Route::Answer(0)),
        (V::Reset, Route::Answer(0)),
        (V::HandleComponentEvent, Route::Lan(Nc::ComponentEvent, 11, 0, 22, 0)),
        (V::HostGame, Route::Lan(Nc::HostGame, 11, 0, 0, 0)),
        (V::JoinGame, Route::Lan(Nc::JoinGame, 11, 0, 0, 0)),
        (V::GetMyClient, Route::Lan(Nc::MyClient, 0, 0, 0, 0)),
        // CallMessage: the buffer, the client (none), the bytes and their length
        (V::SendToCaptain, Route::Lan(Nc::SendToCaptain, 11, 0, 33, 44)),
        (V::BroadcastToGame, Route::Lan(Nc::BroadcastToGame, 11, 0, 33, 44)),
        (V::SendToClient, Route::Lan(Nc::SendToClient, 11, 22, 33, 44)),
        (V::AdvertiseGame, Route::Lan(Nc::AdvertiseGame, 11, 0, 0, 0)),
        (V::RemoveGameAdvertisement, Route::Lan(Nc::RemoveGameAdvertisement, 11, 0, 0, 0)),
        // KickClient(client, ban): Call(KickClient, NULL, client)
        (V::KickClient, Route::Lan(Nc::KickClient, 0, 11, 0, 0)),
        (V::SetGameInProgress, Route::Answer(0)),
        (V::LeaveGame, Route::Lan(Nc::LeaveGame, 0, 0, 0, 0)),
        (V::Suspend, Route::Answer(0)),
        // Resume: the C++ runs LobbyLeaveGame(0)
        (V::Resume, Route::Answer(1)),
        (V::Logout, Route::Lan(Nc::Logout, 0, 0, 0, 0)),
    ];
    assert_eq!(want.len() + 1, VIRTUALS.len());
    for (v, w) in want {
        assert_eq!(r(K::Lan, v), w, "{v:?}");
    }
}

#[test]
fn only_lan_resume_asks_for_the_leave() {
    for kind in KINDS {
        for v in VIRTUALS {
            let asks = r(kind, v) == Route::Answer(1);
            assert_eq!(asks, kind == K::Lan && v == V::Resume, "{kind:?} {v:?}");
        }
    }
}

#[test]
fn the_completions() {
    let want = [
        (K::Init, D::InitDns, Route::Logic(Lc::InitDns, 9, 0), true),
        (K::Init, D::InitDocument, Route::Logic(Lc::InitDocument, 9, 0), true),
        (K::Init, D::InitPromotionImage, Route::Logic(Lc::InitPromotionImage, 9, 0), true),
        (K::Init, D::InitDirectory, Route::Logic(Lc::InitDirectory, 9, 0), true),
        (K::Init, D::InitVersion, Route::Logic(Lc::InitVersion, 9, 0), true),
        (K::Init, D::InitAccept, Route::Logic(Lc::InitAccept, 9, 0), true),
        (K::Init, D::InitFirewall, Route::Logic(Lc::InitFirewall, 9, 0), true),
        (K::Routing, D::RouteUserStatus, Route::Routing(Rc::UserStatusDone, 9, 0), true),
        (K::Routing, D::RouteGroups, Route::Routing(Rc::GroupsDone, 9, 0), true),
        (K::Routing, D::RouteClosed, Route::Routing(Rc::ConnectionClosed, 9, 0), true),
        // RoutingOpCompletion: its code, then the op; no test of the logic or the tracker
        (K::Routing, D::RouteNotify, Route::Routing(Rc::Notify, 5, 9), false),
        (K::Lan, D::LanSocketClosed, Route::Lan(Nc::SocketClosed, 9, 0, 0, 0), true),
        (K::Lan, D::LanNetworkTimer, Route::Lan(Nc::NetworkTimer, 9, 0, 0, 0), true),
        (K::Lan, D::LanRecvMsg, Route::Lan(Nc::RecvMsg, 9, 0, 0, 0), true),
        (K::Lan, D::LanDiscoveryTimer, Route::Lan(Nc::DiscoveryTimer, 9, 0, 0, 0), true),
        (K::Lan, D::LanDiscoveryRecv, Route::Lan(Nc::DiscoveryRecv, 9, 0, 0, 0), true),
        (K::Lan, D::LanAccept, Route::Lan(Nc::Accept, 9, 0, 0, 0), true),
        (K::Lan, D::LanPeerAuthDone, Route::Lan(Nc::PeerAuthDone, 9, 0, 0, 0), true),
        (K::Dialog, D::DlgStatusOp, Route::Dlg(Dc::StatusOpComplete, 9), true),
    ];
    assert_eq!(want.len(), DONES.len());
    for (kind, done, route, checked) in want {
        assert_eq!(done_route(kind, done, 5, 9), Some((route, checked)), "{done:?}");
        // a completion of another kind's logic is no call
        for other in KINDS {
            if other != kind {
                assert_eq!(done_route(other, done, 5, 9), None, "{other:?} {done:?}");
            }
        }
    }
}

#[test]
fn the_dialogs_each_init_component_looked_up() {
    let names =
        |kind| dialogs(kind).iter().map(|r| (r.slot, r.kind, r.name.to_str().unwrap(), r.line)).collect::<Vec<_>>();
    assert_eq!(names(K::Account), [(0, Dk::Credits, "CreditsDialog", 43), (1, Dk::Tou, "TOUDialog", 44)]);
    assert_eq!(names(K::Init), [(0, Dk::ChooseAdapter, "ChooseNetworkAdapterDialog", 49)]);
    // the lookups in the original's order; the slots in the members' order (Room 0x84 before Password 0x88)
    assert_eq!(
        names(K::Routing),
        [
            (0, Dk::AddFriend, "AddFriendDialog", 125),
            (1, Dk::AdminAction, "AdminActionDialog", 126),
            (2, Dk::BadUser, "BadUserDialog", 127),
            (3, Dk::CreateRoom, "CreateRoomDialog", 128),
            (5, Dk::Password, "PasswordDialog", 129),
            (4, Dk::Room, "RoomDialog", 130),
        ]
    );
    assert_eq!(names(K::Staging), [(0, Dk::CreateGame, "CreateGameDialog", 41)]);
    assert_eq!(
        names(K::Lan),
        [
            (0, Dk::DirectConnect, "DirectConnectDialog", 105),
            (1, Dk::LanName, "LanNameDialog", 106),
            (2, Dk::LanPort, "LanPortDialog", 107),
            (3, Dk::Password, "PasswordDialog", 108),
        ]
    );
    assert!(dialogs(K::Dialog).is_empty());
    // the files a missing dialog's error names
    assert!(dialogs(K::Account).iter().all(|r| r.file.to_str().unwrap().ends_with("\\WONLobby\\AccountLogic.cpp")));
    assert!(dialogs(K::Init).iter().all(|r| r.file.to_str().unwrap().ends_with("\\WONLobby\\InitLogic.cpp")));
    assert!(dialogs(K::Routing).iter().all(|r| r.file.to_str().unwrap().ends_with("\\WONLobby\\RoutingLogic.cpp")));
    assert!(dialogs(K::Staging).iter().all(|r| r.file.to_str().unwrap() == "StagingLogic.cpp"));
    assert!(dialogs(K::Lan).iter().all(|r| r.file.to_str().unwrap() == "LanLogic.cpp"));
    // every slot within the shell's six, none twice in a kind
    for kind in KINDS {
        let mut slots: Vec<i32> = dialogs(kind).iter().map(|r| r.slot).collect();
        slots.sort_unstable();
        slots.dedup();
        assert_eq!(slots.len(), dialogs(kind).len());
        assert!(slots.iter().all(|s| (0..6).contains(s)));
    }
}

thread_local! {
    static FOUND: std::cell::RefCell<Vec<(i32, Dk, String, i32)>> = const { std::cell::RefCell::new(Vec::new()) };
    static THROW_AT: std::cell::Cell<i32> = const { std::cell::Cell::new(-1) };
}

unsafe extern "C" fn find(
    logic: *mut c_void,
    config: *mut c_void,
    slot: i32,
    kind: Dk,
    name: *const c_char,
    _file: *const c_char,
    line: i32,
) -> isize {
    assert_eq!(logic as usize, 0x1000);
    assert_eq!(config as usize, 0x2000);
    // SAFETY: the shell's NUL-ended static
    let name = unsafe { CStr::from_ptr(name) }.to_str().unwrap().to_owned();
    FOUND.with(|f| f.borrow_mut().push((slot, kind, name, line)));
    if THROW_AT.get() == slot {
        EE_WON_LOBBY_THROWN
    } else {
        0
    }
}

#[test]
fn init_component_looks_the_dialogs_up_and_stops_at_a_throw() {
    FOUND.with(|f| f.borrow_mut().clear());
    THROW_AT.set(-1);
    // the LAN kind's InitComponent has no call on the Rust object: a null object is not entered
    // SAFETY: no object is entered (the route is an answer); the hook is the test's
    let got = unsafe {
        ee_won_logicshell_virtual(
            K::Lan,
            std::ptr::null_mut(),
            V::InitComponent,
            0x2000,
            0,
            0,
            0,
            0x1000 as *mut c_void,
            Some(find),
        )
    };
    assert_eq!(got, 0);
    assert_eq!(FOUND.with(|f| f.borrow().len()), 4);
    // the third lookup throws: the fourth is not made and the answer is the thrown mark
    FOUND.with(|f| f.borrow_mut().clear());
    THROW_AT.set(2);
    // SAFETY: as above
    let got = unsafe {
        ee_won_logicshell_virtual(
            K::Lan,
            std::ptr::null_mut(),
            V::InitComponent,
            0x2000,
            0,
            0,
            0,
            0x1000 as *mut c_void,
            Some(find),
        )
    };
    assert_eq!(got, EE_WON_LOBBY_THROWN);
    assert_eq!(FOUND.with(|f| f.borrow().iter().map(|x| x.0).collect::<Vec<_>>()), [0, 1, 2]);
    // another virtual looks nothing up
    FOUND.with(|f| f.borrow_mut().clear());
    THROW_AT.set(-1);
    // SAFETY: as above (Suspend of the LAN kind is an answer)
    let got = unsafe {
        ee_won_logicshell_virtual(
            K::Lan,
            std::ptr::null_mut(),
            V::Suspend,
            0,
            0,
            0,
            0,
            0x1000 as *mut c_void,
            Some(find),
        )
    };
    assert_eq!(got, 0);
    assert!(FOUND.with(|f| f.borrow().is_empty()));
}

unsafe extern "C" fn untrack_never(_logic: *mut c_void, _op: *mut c_void) -> bool {
    panic!("a completion of another kind tests nothing");
}

#[test]
fn a_completion_of_another_kind_does_nothing() {
    // SAFETY: no object is entered: the completion is not the kind's
    unsafe {
        ee_won_logicshell_done(
            K::Staging,
            std::ptr::null_mut(),
            D::InitDns,
            0,
            std::ptr::null_mut(),
            false,
            true,
            untrack_never,
            std::ptr::null_mut(),
        )
    };
}

#[test]
fn the_routes_as_numbers() {
    let x = ee_won_logicshell_route(K::Lan, V::SendToClient, 1, 2, 3, 4);
    assert_eq!((x.target, x.call, x.args, x.answer), (5, Nc::SendToClient as u32, [1, 2, 3, 4], 0));
    let x = ee_won_logicshell_route(K::Lan, V::Resume, 1, 2, 3, 4);
    assert_eq!((x.target, x.call, x.args, x.answer), (0, 0, [0; 4], 1));
    let x = ee_won_logicshell_done_route(K::Routing, D::RouteNotify, 21, 7);
    assert_eq!((x.target, x.call, x.args, x.checked), (3, Rc::Notify as u32, [21, 7, 0, 0], false));
    let x = ee_won_logicshell_done_route(K::Account, D::RouteNotify, 21, 7);
    assert_eq!(x.target, -1);
    let (mut slot, mut kind, mut name, mut file, mut line) = (0, Dk::LanName, std::ptr::null(), std::ptr::null(), 0);
    // SAFETY: the outs are the locals
    assert!(unsafe { ee_won_logicshell_dialog(K::Routing, 4, &mut slot, &mut kind, &mut name, &mut file, &mut line) });
    assert_eq!((slot, kind, line), (5, Dk::Password, 129));
    // SAFETY: as above
    assert!(!unsafe { ee_won_logicshell_dialog(K::Routing, 6, &mut slot, &mut kind, &mut name, &mut file, &mut line) });
}
