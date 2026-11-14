//! The C ABI of NESynchronousServer whole in Rust (`server.rs`): the C++ server holds an `EeNetServer*` and forwards
//! its members; the Rust's calls out go to three host functions: the server's (`EeNetSvOp`), the message manager's
//! that reach the C++ (`EeNetMmOp`: NESynchronousServer::MessageManagerHost) and the queues' common ones (`EeNetQOp`).

use std::ffi::c_void;
use std::sync::atomic::{AtomicUsize, Ordering::SeqCst};

use crate::ffi::{EeNetMmHostFn, EeNetMmOp, EeNetQHostFn, QFfiHost};
use crate::ffi_comms::{comms_of, EeNetComms};
use crate::ffi_protocol::*;
use crate::qhost::*;
use crate::recplay::RecPlay;
use crate::server::{Back, Server, ServerArgs, ServerHost};

/// The typed messages' fields the server reads
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeNetSvMsgField {
    /// any message: GetOriginatingSlot()
    OriginatingSlot = 0,
    /// NEMCommunicationsTickResponse: GetNumberOfPlayerInputs()
    NumberOfPlayerInputs = 1,
    /// NEMCommunicationsTickResponse: GetLastCommunicationsTickProcessed()
    LastCtProcessed = 2,
    /// NEMCommunicationsTickResponse: GetAveragePhysicsTickTime()
    AveragePhysicsTickTime = 3,
    /// NEMClientReconnectionRequest: GetCurrentCT()
    ReconnectionCt = 4,
    /// NEMWorldDatabaseChecksumResponse: GetCommunicationsTick()
    ChecksumTick = 5,
    /// NEMWorldDatabaseChecksumResponse: GetWorldDatabaseChecksum()
    Checksum = 6,
}

/// The messages' flags the server reads
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeNetSvMsgFlag {
    CanSendWhilePaused = 0,
    IsAvailableToSend = 1,
}

/// The messages the server makes (`EeNetSvOp::MsgNew`)
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeNetSvNewMsg {
    /// NEMRemovePlayer(a, b & 1, b & 2, b & 4)
    RemovePlayer = 0,
    /// NEMPauseGame(a)
    PauseGame = 1,
    /// NEMUnPauseGame(a)
    UnPauseGame = 2,
    /// NEMServerIsAlive()
    ServerIsAlive = 3,
    /// NEMStopGame()
    StopGame = 4,
    /// NEMPlayerConnectionCheck(a)
    PlayerConnectionCheck = 5,
    /// NEMCommunicationsTick(a, b & 0xff, (b >> 8) & 0xff, b >> 16 & 1, c)
    CommunicationsTick = 6,
    /// NEMClientReconnectionRequestResponse(a, b, c)
    ReconnectionRequestResponse = 7,
}

/// The server's calls out; a, b, c, d as each op says, the answer as isize.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeNetSvOp {
    /// events, their count, a timeout -> TSEvent::WaitAny (the index, kTSWaitTimeout, kTSWaitFailed)
    WaitAny = 0,
    /// a thread: SetPriority(PLAT_PRIORITY_TIME_CRITICAL)
    ThreadPriority = 1,
    /// ms: PlatSleep
    Sleep = 2,
    /// -> gPlatSimMode
    SimMode = 3,
    /// -> NEProtocol::GetCount()
    ProtocolCount = 6,
    /// an NEProtocol** -> NEProtocol::GetProtocol(0, out)
    ProtocolGet = 7,
    /// protocol, the three events (input, xfer input, xfer accept), an NEServer**, allow -> Publish's result
    Publish = 8,
    /// protocol, server, allow: AllowNewConnections
    AllowNewConnections = 9,
    /// protocol, server: AcceptForXfer
    AcceptForXfer = 10,
    /// protocol, start -> SearchForGame
    SearchForGame = 11,
    /// server, slot, data, size -> SendTo
    ServerSendTo = 12,
    /// server, a 0x200-byte buffer, a uint32_t* size, a uint32_t* index -> ReceiveFrom
    ServerReceiveFrom = 13,
    /// server: Close()
    ServerClose = 14,
    /// a message -> GetType()
    MsgType = 15,
    /// a message, originating (else for a specific slot), a slot
    MsgSetSlot = 16,
    /// a message, a flag (EeNetSvMsgFlag)
    MsgFlag = 17,
    /// a message, a field (EeNetSvMsgField)
    MsgGet = 18,
    /// kind (EeNetSvNewMsg), a, b, c -> the new message
    MsgNew = 19,
    /// a tick response, an index, an NEMessage** -> GetPlayerInput(i, out)
    MsgTickResponseInput = 20,
    /// a tick, an input -> AddPlayerInput
    MsgAddInput = 21,
    /// a message, an id: SetSendToAllAndWaitMessageID
    MsgSetS2awId = 22,
    /// a message, a uint32_t* size -> ExportMessage's buffer (new[])
    MsgExportNew = 23,
    /// a buffer: delete[]
    BytesFree = 24,
    /// slot, booted, exiting: BootPlayer
    BootPlayer = 25,
    /// -> ServerSlot28()
    ServerSlot28 = 26,
    /// -> IsMigrating()
    IsMigrating = 27,
    /// BeforeRemovingPlayers()
    BeforeRemovingPlayers = 28,
    /// HandleReconnectWaitTimedOut()
    HandleReconnectWaitTimedOut = 29,
    /// a player -> GetPlayerSendToSlot
    PlayerSendToSlot = 30,
    /// a message -> FilterPlayerInput
    FilterPlayerInput = 31,
    /// a message: PrepareLocalInput
    PrepareLocalInput = 32,
    /// a player -> IsPlayerPresent
    IsPlayerPresent = 33,
    /// slots, their count, the warning: ReportLaggers(std::vector of the slots, warning)
    ReportLaggers = 34,
    /// a message: ProcessSystemMessage
    ProcessSystemMessage = 35,
    /// a message, a player -> the virtual ProcessLocalUserInput
    ProcessLocalUserInput = 36,
}

/// The fields the accessors read (`ee_net_sv_get`)
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeNetSvGet {
    CurrentCommunicationsTick = 0,
    CurrentPhysicsTick = 1,
    GameState = 2,
    GameWasPublished = 3,
    LastCtMessageId = 4,
    MaxPlayers = 5,
    NumberOfPlayers = 6,
    ProcessingSendToAllAndWait = 7,
    PTsPerCt = 8,
    RestartCt = 9,
    SendInterval = 10,
    IsOos = 11,
    PTsPerSecond = 12,
    NextChecksumCt = 13,
    ReconnectCount = 14,
    LowestReconnectCt = 15,
    WaitForReconnectsTime = 16,
    Ping = 17,
    Protocol = 18,
    TempOutOfSynchPTs = 19,
}

/// The fields the accessors write (`ee_net_sv_set`)
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeNetSvSet {
    CurrentCommunicationsTick = 0,
    CurrentPhysicsTick = 1,
    GameState = 2,
    LastCtMessageId = 3,
    RestartCt = 4,
    TempOutOfSynchPTs = 5,
    Ping = 6,
}

/// The events the inline members set (`ee_net_sv_set_event`)
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeNetSvEvent {
    StartGame = 0,
    StopGame = 1,
    ResumeMigratedGame = 2,
    ForceNextCt = 3,
}

pub type EeNetSvHostFn =
    unsafe extern "C" fn(ctx: *mut c_void, op: EeNetSvOp, a: usize, b: usize, c: usize, d: usize) -> isize;

/// The constructor's arguments (NESynchronousServer's; with `tcp` the server calls the protocol itself (ffi_protocol)
/// and publishes under the name (bytes), the announcement and the network adapter; without it the host answers the
/// protocol ops (the goldens' worlds) and the last four are not read)
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeNetSvArgs {
    pub max_players: u8,
    pub default_send_interval: u32,
    pub pts_per_ct: u8,
    pub max_pts_per_second: u32,
    pub min_pts_per_second: u32,
    pub cost_factor: f32,
    pub max_send_interval: u32,
    pub send_interval_increment: u32,
    pub send_interval_decrement: u32,
    pub current_communications_tick: u32,
    pub current_physics_tick: u32,
    pub allow_new_connections: bool,
    pub tcp: bool,
    pub name: *const u8,
    pub name_length: usize,
    pub announce: bool,
    pub network_adapter: u32,
}

/// The protocol the server calls itself: Publish's arguments and the protocol it got (protocol 0)
struct SvTcp {
    name: Vec<u8>,
    announce: bool,
    adapter: u32,
    protocol: AtomicUsize,
}

impl SvTcp {
    fn p(&self) -> *const EeNetProtocol {
        self.protocol.load(SeqCst) as *const EeNetProtocol
    }
}

/// The service the game publishes and searches for
const SERVICE: &[u8] = b"EEService";

pub struct SvFfiHost {
    ctx: *mut c_void,
    f: EeNetSvHostFn,
    mm: EeNetMmHostFn,
    q: QFfiHost,
    tcp: Option<SvTcp>,
}

// The context is the C++ server, used from its threads as the original's members were.
unsafe impl Send for SvFfiHost {}
unsafe impl Sync for SvFfiHost {}

impl SvFfiHost {
    fn call(&self, op: EeNetSvOp, a: usize, b: usize, c: usize, d: usize) -> isize {
        unsafe { (self.f)(self.ctx, op, a, b, c, d) }
    }
}

impl QHost for SvFfiHost {
    fn time(&self) -> u32 {
        self.q.time()
    }
    fn verify(&self, unit: u32, line: u32) {
        self.q.verify(unit, line)
    }
    fn fpu_start(&self) {
        self.q.fpu_start()
    }
    fn fpu_check(&self) {
        self.q.fpu_check()
    }
    fn event_new(&self) -> Ev {
        self.q.event_new()
    }
    fn event_free(&self, e: Ev) {
        self.q.event_free(e)
    }
    fn event_set(&self, e: Ev) {
        self.q.event_set(e)
    }
    fn event_clear(&self, e: Ev) {
        self.q.event_clear(e)
    }
    fn event_wait(&self, e: Ev, timeout: u32) -> i32 {
        self.q.event_wait(e, timeout)
    }
    fn thread_new(&self, kind: u32, obj: usize, out: &mut Thr) -> i32 {
        self.q.thread_new(kind, obj, out)
    }
    fn thread_name(&self, t: Thr, name: &str) {
        self.q.thread_name(t, name)
    }
    fn thread_free(&self, t: Thr) {
        self.q.thread_free(t)
    }
    fn msg_reference(&self, m: Msg) {
        self.q.msg_reference(m)
    }
    fn msg_dereference(&self, m: Msg) {
        self.q.msg_dereference(m)
    }
    fn msg_export_size(&self, m: Msg) -> u32 {
        self.q.msg_export_size(m)
    }
    fn msg_export(&self, m: Msg, buf: &mut [u8]) -> u32 {
        self.q.msg_export(m, buf)
    }
    fn msg_from_bytes(&self, data: Option<&[u8]>) -> Msg {
        self.q.msg_from_bytes(data)
    }
    fn deliver(&self, kind: u32, m: Msg) -> i32 {
        self.q.deliver(kind, m)
    }
    fn file_create(&self, f: FileH) -> i32 {
        self.q.file_create(f)
    }
    fn file_delete(&self, f: FileH) -> i32 {
        self.q.file_delete(f)
    }
    fn file_open(&self, f: FileH, access: u32) -> i32 {
        self.q.file_open(f, access)
    }
    fn file_close(&self, f: FileH) {
        self.q.file_close(f)
    }
    fn file_write(&self, f: FileH, data: &[u8]) -> i32 {
        self.q.file_write(f, data)
    }
    fn file_write_u32(&self, f: FileH, value: u32) -> i32 {
        self.q.file_write_u32(f, value)
    }
    fn file_read_u32(&self, f: FileH, value: &mut u32) -> i32 {
        self.q.file_read_u32(f, value)
    }
    fn file_read(&self, f: FileH, buf: &mut [u8]) -> i32 {
        self.q.file_read(f, buf)
    }
}

impl ServerHost for SvFfiHost {
    fn wait_any(&self, events: &[Ev], timeout: u32) -> i32 {
        self.call(EeNetSvOp::WaitAny, events.as_ptr() as usize, events.len(), timeout as usize, 0) as i32
    }
    fn thread_priority(&self, t: Thr) {
        self.call(EeNetSvOp::ThreadPriority, t, 0, 0, 0);
    }
    fn sleep(&self, ms: u32) {
        self.call(EeNetSvOp::Sleep, ms as usize, 0, 0, 0);
    }
    fn sim_mode(&self) -> bool {
        self.call(EeNetSvOp::SimMode, 0, 0, 0, 0) != 0
    }
    fn mm(&self, op: EeNetMmOp, a: usize, b: usize, c: usize, d: usize) -> isize {
        unsafe { (self.mm)(self.ctx, op, a, b, c, d) }
    }
    fn protocol_count(&self) -> u32 {
        if self.tcp.is_some() {
            return ee_net_pr_count();
        }
        self.call(EeNetSvOp::ProtocolCount, 0, 0, 0, 0) as u32
    }
    fn protocol_get(&self, out: &mut usize) -> i32 {
        if let Some(t) = &self.tcp {
            let r = unsafe { ee_net_pr_get(0, out as *mut usize as *mut *mut EeNetProtocol) };
            t.protocol.store(*out, SeqCst);
            return r;
        }
        self.call(EeNetSvOp::ProtocolGet, out as *mut usize as usize, 0, 0, 0) as i32
    }
    fn publish(&self, protocol: usize, events: &[Ev; 3], server: &mut usize, allow: bool) -> i32 {
        if let Some(t) = &self.tcp {
            let (n, a) = (SERVICE, &t.name);
            let e = [events[0] as *const c_void, events[1] as *const c_void, events[2] as *const c_void];
            let out = server as *mut usize as *mut *mut c_void;
            let p = protocol as *const EeNetProtocol;
            return unsafe {
                ee_net_pr_publish(p, n.as_ptr(), n.len(), a.as_ptr(), a.len(), 2, e.as_ptr(), out, allow, t.announce, t.adapter)
            };
        }
        self.call(EeNetSvOp::Publish, protocol, events.as_ptr() as usize, server as *mut usize as usize, allow as usize)
            as i32
    }
    fn allow_new_connections(&self, protocol: usize, server: usize, allow: bool) {
        if self.tcp.is_some() {
            return unsafe { ee_net_pr_allow_new_connections(protocol as *const EeNetProtocol, server as *mut c_void, allow) };
        }
        self.call(EeNetSvOp::AllowNewConnections, protocol, server, allow as usize, 0);
    }
    fn accept_for_xfer(&self, protocol: usize, server: usize) {
        if self.tcp.is_some() {
            // (the connection a local the original never read)
            let mut connection = std::ptr::null_mut();
            unsafe { ee_net_pr_accept(protocol as *const EeNetProtocol, server as *mut c_void, true, &mut connection) };
            return;
        }
        self.call(EeNetSvOp::AcceptForXfer, protocol, server, 0, 0);
    }
    fn search_for_game(&self, protocol: usize, start: bool) -> i32 {
        if self.tcp.is_some() {
            let p = protocol as *const EeNetProtocol;
            return unsafe { ee_net_pr_search_for_game(p, SERVICE.as_ptr(), SERVICE.len(), start) };
        }
        self.call(EeNetSvOp::SearchForGame, protocol, start as usize, 0, 0) as i32
    }
    fn server_send_to(&self, server: usize, slot: u8, data: usize, size: u32) -> i32 {
        if let Some(t) = &self.tcp {
            return unsafe { ee_net_pr_send_to(t.p(), server as *mut c_void, false, slot as u32, data as *mut u8, size) };
        }
        self.call(EeNetSvOp::ServerSendTo, server, slot as usize, data, size as usize) as i32
    }
    fn server_receive_from(&self, server: usize, buf: &mut [u8], size: &mut u32, index: &mut u32) -> i32 {
        debug_assert_eq!(buf.len(), 0x200);
        if let Some(t) = &self.tcp {
            (*size, *index) = (0, 0);
            let b = buf.as_mut_ptr();
            return unsafe { ee_net_pr_receive_from(t.p(), server as *mut c_void, 0x200, b, size, index) };
        }
        self.call(
            EeNetSvOp::ServerReceiveFrom,
            server,
            buf.as_mut_ptr() as usize,
            size as *mut u32 as usize,
            index as *mut u32 as usize,
        ) as i32
    }
    fn server_close(&self, server: usize) {
        if let Some(t) = &self.tcp {
            unsafe { ee_net_pr_close(t.p(), server as *mut c_void, true) };
            return;
        }
        self.call(EeNetSvOp::ServerClose, server, 0, 0, 0);
    }
    fn msg_type(&self, m: Msg) -> u8 {
        self.call(EeNetSvOp::MsgType, m, 0, 0, 0) as u8
    }
    fn msg_set_slot(&self, m: Msg, originating: bool, slot: u8) {
        self.call(EeNetSvOp::MsgSetSlot, m, originating as usize, slot as usize, 0);
    }
    fn msg_flag(&self, m: Msg, f: EeNetSvMsgFlag) -> bool {
        self.call(EeNetSvOp::MsgFlag, m, f as usize, 0, 0) != 0
    }
    fn msg_get(&self, m: Msg, f: EeNetSvMsgField) -> u32 {
        self.call(EeNetSvOp::MsgGet, m, f as usize, 0, 0) as u32
    }
    fn msg_new(&self, kind: EeNetSvNewMsg, a: u32, b: u32, c: u32) -> Msg {
        self.call(EeNetSvOp::MsgNew, kind as usize, a as usize, b as usize, c as usize) as Msg
    }
    fn msg_tick_response_input(&self, m: Msg, i: u32, out: &mut Msg) -> i32 {
        self.call(EeNetSvOp::MsgTickResponseInput, m, i as usize, out as *mut Msg as usize, 0) as i32
    }
    fn msg_add_input(&self, tick: Msg, input: Msg) -> i32 {
        self.call(EeNetSvOp::MsgAddInput, tick, input, 0, 0) as i32
    }
    fn msg_set_s2aw_id(&self, m: Msg, id: u32) {
        self.call(EeNetSvOp::MsgSetS2awId, m, id as usize, 0, 0);
    }
    fn msg_export_new(&self, m: Msg, size: &mut u32) -> usize {
        self.call(EeNetSvOp::MsgExportNew, m, size as *mut u32 as usize, 0, 0) as usize
    }
    fn bytes_free(&self, data: usize) {
        self.call(EeNetSvOp::BytesFree, data, 0, 0, 0);
    }
    fn boot_player(&self, slot: u8, was_booted: bool, game_is_exiting: bool) {
        self.call(EeNetSvOp::BootPlayer, slot as usize, was_booted as usize, game_is_exiting as usize, 0);
    }
    fn server_slot28(&self) -> bool {
        self.call(EeNetSvOp::ServerSlot28, 0, 0, 0, 0) != 0
    }
    fn is_migrating(&self) -> bool {
        self.call(EeNetSvOp::IsMigrating, 0, 0, 0, 0) != 0
    }
    fn before_removing_players(&self) {
        self.call(EeNetSvOp::BeforeRemovingPlayers, 0, 0, 0, 0);
    }
    fn handle_reconnect_wait_timed_out(&self) {
        self.call(EeNetSvOp::HandleReconnectWaitTimedOut, 0, 0, 0, 0);
    }
    fn player_send_to_slot(&self, player: u8) -> u8 {
        self.call(EeNetSvOp::PlayerSendToSlot, player as usize, 0, 0, 0) as u8
    }
    fn filter_player_input(&self, m: Msg) -> bool {
        self.call(EeNetSvOp::FilterPlayerInput, m, 0, 0, 0) != 0
    }
    fn prepare_local_input(&self, m: Msg) {
        self.call(EeNetSvOp::PrepareLocalInput, m, 0, 0, 0);
    }
    fn is_player_present(&self, player: u8) -> bool {
        self.call(EeNetSvOp::IsPlayerPresent, player as usize, 0, 0, 0) != 0
    }
    fn report_laggers(&self, slots: &[u8], warning: bool) {
        self.call(EeNetSvOp::ReportLaggers, slots.as_ptr() as usize, slots.len(), warning as usize, 0);
    }
    fn process_system_message(&self, m: Msg) {
        self.call(EeNetSvOp::ProcessSystemMessage, m, 0, 0, 0);
    }
    fn process_local_user_input(&self, m: Msg, player: u8) -> i32 {
        self.call(EeNetSvOp::ProcessLocalUserInput, m, player as usize, 0, 0) as i32
    }
}

/// NESynchronousServer whole in Rust.
pub struct EeNetServer(Box<Server<SvFfiHost>>);

fn sv<'a>(s: *const EeNetServer) -> &'a Server<SvFfiHost> {
    unsafe { &(*s).0 }
}

/// The constructor (published or not: `ee_net_sv_get(s, GameWasPublished)`).
///
/// # Safety
/// `host`, `mmhost` and `qhost` answer every op for `ctx` until `ee_net_sv_free`; `args` points at the arguments;
/// `comms` from `ee_net_comms_new` (the C++ base class's).
#[no_mangle]
pub unsafe extern "C" fn ee_net_sv_new(
    ctx: *mut c_void,
    host: EeNetSvHostFn,
    mmhost: EeNetMmHostFn,
    qhost: EeNetQHostFn,
    args: *const EeNetSvArgs,
    comms: *const EeNetComms,
) -> *mut EeNetServer {
    let a = &*args;
    let tcp = a.tcp.then(|| SvTcp {
        name: if a.name_length == 0 { Vec::new() } else { std::slice::from_raw_parts(a.name, a.name_length).to_vec() },
        announce: a.announce,
        adapter: a.network_adapter,
        protocol: AtomicUsize::new(0),
    });
    let h = SvFfiHost { ctx, f: host, mm: mmhost, q: QFfiHost { ctx, f: qhost }, tcp };
    let a = ServerArgs {
        max_players: a.max_players,
        default_send_interval: a.default_send_interval,
        pts_per_ct: a.pts_per_ct,
        max_pts_per_second: a.max_pts_per_second,
        min_pts_per_second: a.min_pts_per_second,
        cost_factor: a.cost_factor,
        max_send_interval: a.max_send_interval,
        send_interval_increment: a.send_interval_increment,
        send_interval_decrement: a.send_interval_decrement,
        current_communications_tick: a.current_communications_tick,
        current_physics_tick: a.current_physics_tick,
        allow_new_connections: a.allow_new_connections,
    };
    Box::into_raw(Box::new(EeNetServer(Server::new(h, comms_of(comms), a))))
}

/// The destructor's work, then the object freed.
///
/// # Safety
/// `s` from `ee_net_sv_new` (or NULL), its threads ended, used by nothing after.
#[no_mangle]
pub unsafe extern "C" fn ee_net_sv_free(s: *mut EeNetServer) {
    if !s.is_null() {
        let b = Box::from_raw(s);
        b.0.release();
    }
}

/// The main thread's body (the host's `ThreadNew` of kind 7 runs it with the server).
#[no_mangle]
pub extern "C" fn ee_net_sv_main_thread(s: *const EeNetServer) -> u32 {
    sv(s).main_thread()
}

/// The send thread's body (kind 8).
#[no_mangle]
pub extern "C" fn ee_net_sv_send_thread(s: *const EeNetServer) -> u32 {
    sv(s).send_thread()
}

/// The transfer thread's body (kind 9).
#[no_mangle]
pub extern "C" fn ee_net_sv_xfer_thread(s: *const EeNetServer) -> u32 {
    sv(s).xfer_thread()
}

/// The server's playback thread's body (kind 6, with the object the recorder handed over).
#[no_mangle]
pub extern "C" fn ee_net_sv_playback_thread(obj: *const c_void) -> u32 {
    unsafe { &*(obj as *const RecPlay<Back<SvFfiHost>>) }.playback_thread()
}

#[no_mangle]
pub extern "C" fn ee_net_sv_start_threads(s: *const EeNetServer) {
    sv(s).start_threads(s as usize)
}

#[no_mangle]
pub extern "C" fn ee_net_sv_stop_threads(s: *const EeNetServer) {
    sv(s).stop_threads()
}

#[no_mangle]
pub extern "C" fn ee_net_sv_send_exit_message(s: *const EeNetServer) {
    sv(s).send_exit_message()
}

/// A field (`SvGet`) for the accessors
#[no_mangle]
pub extern "C" fn ee_net_sv_get(s: *const EeNetServer, field: EeNetSvGet) -> u32 {
    sv(s).field(field)
}

/// A field (`SvSet`) for the setters
#[no_mangle]
pub extern "C" fn ee_net_sv_set(s: *const EeNetServer, field: EeNetSvSet, value: u32) {
    sv(s).set_field(field, value)
}

/// The protocol (`NEProtocol*`)
#[no_mangle]
pub extern "C" fn ee_net_sv_protocol(s: *const EeNetServer) -> *mut c_void {
    sv(s).protocol() as *mut c_void
}

/// The published server (`NEServer*`)
#[no_mangle]
pub extern "C" fn ee_net_sv_server(s: *const EeNetServer) -> *mut c_void {
    sv(s).server() as *mut c_void
}

#[no_mangle]
pub extern "C" fn ee_net_sv_set_event(s: *const EeNetServer, which: EeNetSvEvent) {
    sv(s).set_event(which)
}

#[no_mangle]
pub extern "C" fn ee_net_sv_allow_new_players(s: *const EeNetServer, allow: bool) {
    sv(s).allow_new_players(allow)
}

#[no_mangle]
pub extern "C" fn ee_net_sv_clear_send_to_slot_array(s: *const EeNetServer) {
    sv(s).clear_send_to_slot_array()
}

#[no_mangle]
pub extern "C" fn ee_net_sv_set_send_to_slot_for_player(s: *const EeNetServer, player: u8, slot: u8) {
    sv(s).set_send_to_slot_for_player(player, slot)
}

#[no_mangle]
pub extern "C" fn ee_net_sv_send_to_slot_for_player(s: *const EeNetServer, player: u8) -> u8 {
    sv(s).send_to_slot_for_player(player)
}

#[no_mangle]
pub extern "C" fn ee_net_sv_set_send_interval(s: *const EeNetServer, interval: u32) {
    sv(s).set_send_interval(interval)
}

#[no_mangle]
pub extern "C" fn ee_net_sv_start_waiting_for_reconnects(s: *const EeNetServer, time: u32) {
    sv(s).start_waiting_for_reconnects(time)
}

#[no_mangle]
pub extern "C" fn ee_net_sv_lock_send_communications_tick(s: *const EeNetServer) {
    sv(s).lock_send_communications_tick()
}

#[no_mangle]
pub extern "C" fn ee_net_sv_unlock_send_communications_tick(s: *const EeNetServer) {
    sv(s).unlock_send_communications_tick()
}

/// SimGameRunning (TEST ONLY)
#[no_mangle]
pub extern "C" fn ee_net_sv_sim_game_running(s: *const EeNetServer) -> bool {
    sv(s).sim_game_running()
}

/// SimCommunicationsTick (TEST ONLY)
#[no_mangle]
pub extern "C" fn ee_net_sv_sim_communications_tick(s: *const EeNetServer) {
    sv(s).sim_communications_tick()
}

/// The recorder's mode (1 passing, 2 recording, 3 playing back; a server that was not published: passing)
#[no_mangle]
pub extern "C" fn ee_net_sv_record_playback_mode(s: *const EeNetServer) -> u8 {
    sv(s).record_playback_mode()
}

#[no_mangle]
pub extern "C" fn ee_net_sv_set_slow_playback(s: *const EeNetServer) {
    if let Some(r) = sv(s).rec() {
        r.set_slow_playback()
    }
}

#[no_mangle]
pub extern "C" fn ee_net_sv_set_normal_playback(s: *const EeNetServer) {
    if let Some(r) = sv(s).rec() {
        r.set_normal_playback()
    }
}

#[no_mangle]
pub extern "C" fn ee_net_sv_abort_xfer(s: *const EeNetServer, id: u32) {
    sv(s).abort_xfer(id)
}

#[no_mangle]
pub extern "C" fn ee_net_sv_boot_all_laggers(s: *const EeNetServer, number: u32) {
    sv(s).boot_all_laggers(number)
}

#[no_mangle]
pub extern "C" fn ee_net_sv_broadcast_message(s: *const EeNetServer, msg: *mut c_void) -> i32 {
    sv(s).broadcast_message(msg as Msg)
}

#[no_mangle]
pub extern "C" fn ee_net_sv_message_manager_current_number(s: *const EeNetServer) -> u32 {
    sv(s).mm().current_number()
}

#[no_mangle]
pub extern "C" fn ee_net_sv_set_message_manager_current_number(s: *const EeNetServer, number: u32) {
    sv(s).mm().set_current_number(number)
}

#[no_mangle]
pub extern "C" fn ee_net_sv_set_message_manager_lowest_cts(s: *const EeNetServer, ct: u32) {
    sv(s).mm().set_lowest_cts(ct)
}

#[no_mangle]
pub extern "C" fn ee_net_sv_is_oos(s: *const EeNetServer) -> bool {
    sv(s).is_oos()
}

#[no_mangle]
pub extern "C" fn ee_net_sv_log_checksum_response(s: *const EeNetServer, tick: u32, checksum: u32, slot: u8) {
    sv(s).log_checksum_response(tick, checksum, slot)
}

#[no_mangle]
pub extern "C" fn ee_net_sv_log_response(s: *const EeNetServer, msg: *mut c_void) {
    sv(s).log_response(msg as Msg)
}

/// PauseGame (`pause`) and UnpauseGame
#[no_mangle]
pub extern "C" fn ee_net_sv_pause_game(s: *const EeNetServer, pause: bool) {
    sv(s).pause_game(pause)
}

#[no_mangle]
pub extern "C" fn ee_net_sv_process_local_user_input(s: *const EeNetServer, msg: *mut c_void, player: u8) -> i32 {
    sv(s).process_local_user_input(msg as Msg, player)
}

#[no_mangle]
pub extern "C" fn ee_net_sv_register_slot(s: *const EeNetServer, slot: u8) {
    sv(s).register_slot(slot)
}

#[no_mangle]
pub extern "C" fn ee_net_sv_unregister_slot(s: *const EeNetServer, slot: u8) {
    sv(s).unregister_slot(slot)
}

#[no_mangle]
pub extern "C" fn ee_net_sv_remove_player_from_game_in_progress(
    s: *const EeNetServer,
    slot: u8,
    was_booted: bool,
    is_oos: bool,
    game_is_exiting: bool,
) {
    sv(s).remove_player_from_game_in_progress(slot, was_booted, is_oos, game_is_exiting)
}

#[no_mangle]
pub extern "C" fn ee_net_sv_search_for_game(s: *const EeNetServer, start: bool) -> i32 {
    sv(s).search_for_game(start)
}

#[no_mangle]
pub extern "C" fn ee_net_sv_send_message_to(
    s: *const EeNetServer,
    slot: u8,
    msg: *mut c_void,
    guaranteed: bool,
) -> u32 {
    sv(s).send_message_to(slot, msg as Msg, guaranteed)
}

#[no_mangle]
pub extern "C" fn ee_net_sv_send_message_to_all(s: *const EeNetServer, msg: *mut c_void, guaranteed: bool) -> u32 {
    sv(s).send_message_to_all(msg as Msg, guaranteed)
}

#[no_mangle]
pub extern "C" fn ee_net_sv_send_to_all_and_wait(
    s: *const EeNetServer,
    msg: *mut c_void,
    timeout: u32,
    retries: u32,
) -> i32 {
    sv(s).send_to_all_and_wait(msg as Msg, timeout, retries)
}

#[no_mangle]
pub extern "C" fn ee_net_sv_tune_server(s: *const EeNetServer) -> bool {
    sv(s).tune_server()
}

/// XferFileToClient: `spec` an FSFileSpec*, copied by the message manager
///
/// # Safety
/// `id` points at a uint32_t.
#[no_mangle]
pub unsafe extern "C" fn ee_net_sv_xfer_file_to_client(
    s: *const EeNetServer,
    spec: *mut c_void,
    user_data: u32,
    slot: u8,
    id: *mut u32,
) -> bool {
    sv(s).xfer_file_to_client(spec as usize, user_data, slot, &mut *id)
}

/// The checksums' state (the tests): writes at most `size` bytes of `Server::checks_state` (NUL-terminated) and
/// answers its length.
///
/// # Safety
/// `out` holds `size` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_net_sv_checks_state(s: *const EeNetServer, out: *mut u8, size: usize) -> usize {
    let text = sv(s).checks_state();
    if size > 0 {
        let n = text.len().min(size - 1);
        std::ptr::copy_nonoverlapping(text.as_ptr(), out, n);
        *out.add(n) = 0;
    }
    text.len()
}

/// The queues' lengths (the tests): the incoming inputs, the queued send-to-all-and-waits
///
/// # Safety
/// `inputs` and `s2aw` point at uint32_t's.
#[no_mangle]
pub unsafe extern "C" fn ee_net_sv_queue_lengths(s: *const EeNetServer, inputs: *mut u32, s2aw: *mut u32) {
    let (i, w) = sv(s).queue_lengths();
    *inputs = i as u32;
    *s2aw = w as u32;
}
