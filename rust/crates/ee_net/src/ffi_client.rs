//! The C ABI of the lock-step client (`client.rs`, NESynchronousClient whole in Rust): the C++ class keeps an
//! `EeNetClient*` and forwards each member; the client's calls out go to the class's host function by `EeNetClOp`,
//! the queues' common ones (the clock, events, threads, messages) to its `EeNetQOp` host.

use std::ffi::c_void;
use std::sync::atomic::{AtomicUsize, Ordering::SeqCst};

use crate::client::*;
use crate::ffi::{EeNetQHostFn, QFfiHost};
use crate::ffi_comms::{comms_of, EeNetComms};
use crate::ffi_protocol::*;
use crate::lagloss::LagLoss;
use crate::qhost::{Ev, FileH, Msg, QHost, Thr};
use crate::recplay::RecPlay;

/// The client's calls out besides the queues' (`client::ClientHost`); a, b, c, d as each op says, the answer as
/// isize.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeNetClOp {
    /// events (TSEvent* const*), their count, a timeout -> TSEvent::WaitAny (an index, kTSWaitTimeout, kTSWaitFailed)
    WaitAny = 0,
    /// a thread: SetPriority(PLAT_PRIORITY_TIME_CRITICAL)
    ThreadPriority = 1,
    /// -> NEProtocol::GetCount()
    ProtocolCount = 4,
    /// an NEProtocol** -> NEProtocol::GetProtocol(0, *out)
    ProtocolGet = 5,
    /// const void* strings[3] (name, host, address), an event, an NEConnection**, a timeout -> the client's protocol's
    /// Connect(name, host, address, 2, event, *out, timeout)
    Connect = 6,
    /// a connection, an event, an NEConnection** -> ConnectForXfer(connection, 1, event, *out)
    ConnectForXfer = 7,
    /// a connection -> Close()
    ConnClose = 8,
    /// a connection, a buffer, its size, an uint32_t* length (in and out) -> Receive(size, buffer, length)
    ConnReceive = 9,
    /// a connection, data, its size -> Send(data, size)
    ConnSend = 10,
    /// -> the client's protocol's GetDetectedGameCount()
    DetectedGameCount = 11,
    /// an index, void* strings[3] (UString name, UString host, UWideString address) -> GetDetectedGame
    DetectedGame = 12,
    /// start -> the client's protocol's SearchForGame(UString("Anything"), UString("EEService"), start)
    SearchForGame = 13,
    /// a buffer, its size, an uint32_t* received (in and out) -> the client's protocol's XferReceive
    XferReceive = 14,
    /// -> NEMXferMessage::StaticExportSize()
    XferExportSize = 15,
    /// a message -> GetType()
    MsgType = 16,
    /// a message, a flag (EeNetClMsgFlag) -> the virtual flag
    MsgFlag = 17,
    /// a message, 0 SetForSpecificSlot / 1 SetOriginatingSlot, a slot
    MsgSetSlot = 18,
    /// a message, an uint32_t* -> GetSendToAllAndWaitMessageID(*out)
    MsgS2awId = 19,
    /// a message, a field (EeNetClMsgField) -> the typed message's field
    MsgGet = 20,
    /// a kind (EeNetClNewMsg), then its arguments (the transfer status: slot | percent << 8 | status << 16, the id)
    /// -> the new message
    MsgNew = 21,
    /// a tick response, an input -> AddPlayerInput(input)
    MsgAddInput = 22,
    /// a message, an uint32_t* size -> ExportMessage's data (C++ new[]), its size in *size
    MsgExportNew = 23,
    /// an export: deleted
    BytesFree = 24,
    /// an FSFile*, data, its size, an uint32_t* written -> Write(data, size, written)
    FsWrite = 25,
    /// an FSFile*: Close()
    FsClose = 26,
    /// an FSFile*: Delete()
    FsDelete = 27,
    /// an FSFile*: deleted
    FsFree = 28,
    /// a zero-terminated name, a size -> CreateXferFile(UWideString(name), size)
    CreateXferFile = 29,
    /// -> IsPlaybackRecordingSession()
    IsPlaybackRecordingSession = 30,
    /// -> GetConnectTimeout()
    ConnectTimeout = 31,
    /// -> GetClientSlot()
    ClientSlot = 32,
    /// -> IgnoreConnectionTimeout()
    IgnoreConnectionTimeout = 33,
    /// a state: SetConnectionState(state)
    SetConnectionState = 34,
    /// a message: HandleMessage
    HandleMessage = 35,
    /// a message: HandleUserInput
    HandleUserInput = 36,
    /// HandleConnectionLost()
    HandleConnectionLost = 37,
    /// an offset -> HandleStartGame(offset)
    HandleStartGame = 38,
    /// HandleStopGame()
    HandleStopGame = 39,
    /// the time left: HandleConnectionTimeoutWarning
    HandleConnectionTimeoutWarning = 40,
    /// a message, a player -> the virtual ProcessLocalUserInput
    ProcessLocalUserInput = 41,
}

/// A message's virtual flags
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeNetClMsgFlag {
    CountsTowardsPlayerInputThrottle = 0,
    CanSendWhilePaused = 1,
    IsAvailableToSend = 2,
}

/// The fields of the typed messages this class read (each a `static_cast` and a getter)
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeNetClMsgField {
    /// NEMCommunicationsTick::GetCurrentCommunicationsTick
    TickCt = 0,
    /// NEMCommunicationsTickResponse::GetServersCommunicationTick
    ServersTick = 1,
    /// NEMStartGame::GetOffsetStartTime
    OffsetStartTime = 2,
    /// NEMClientConnectionRequestResponse::GetSendToSlot
    SendToSlot = 3,
    /// NEMClientConnectionRequestResponse::GetError
    ConnectionError = 4,
    /// NEMClientConnectionRequestResponse::GetCurrentCT
    ConnectionCt = 5,
    /// NEMClientReconnectionRequestResponse::GetNewSendToSlot
    NewSendToSlot = 6,
    /// NEMClientReconnectionRequestResponse::GetCurrentCT
    ReconnectionCt = 7,
    /// NEMPauseGame / NEMUnPauseGame::GetUniqueID
    UniqueId = 8,
    /// NEMXferMessage::GetID
    XferId = 9,
    /// NEMXferMessage::GetData (a pointer)
    XferData = 10,
    /// NEMXferMessage::GetDataSize
    XferDataSize = 11,
}

/// The messages this class made (`new`, one reference)
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeNetClNewMsg {
    /// NEMCommunicationsTickResponse(CTs processed, the tick, the average physics update time)
    TickResponse = 0,
    StopGameResponse = 1,
    /// NEMStartGameResponse(time)
    StartGameResponse = 2,
    /// NEMUnPauseGameResponse(id)
    UnPauseGameResponse = 3,
    /// NEMPauseGameResponse(id)
    PauseGameResponse = 4,
    /// NEMXferMessage(the received bytes)
    XferMessage = 5,
    /// NEMXferStatus(slot, percent done, transfer id, status)
    XferStatus = 6,
}

pub type EeNetClHostFn =
    unsafe extern "C" fn(ctx: *mut c_void, op: EeNetClOp, a: usize, b: usize, c: usize, d: usize) -> isize;

/// `Connect`'s strings as bytes (the client calling the protocol itself: `ee_net_cl_new`'s `tcp`)
#[repr(C)]
pub struct EeNetClStrings {
    pub name: *const u8,
    pub name_length: usize,
    pub host: *const u8,
    pub host_length: usize,
    pub address: *const u8,
    pub address_length: usize,
}

/// `GetDetectedGame`'s answers: the name, the host and the address to their sinks (with `tcp`)
#[repr(C)]
pub struct EeNetClSinks {
    pub name: EeNetPrSink,
    pub name_context: *mut c_void,
    pub host: EeNetPrSink,
    pub host_context: *mut c_void,
    pub address: EeNetPrSink,
    pub address_context: *mut c_void,
}

/// The service the game searches for
const SERVICE: &[u8] = b"EEService";

/// The client's host: its own ops and the queues' common ones; with `tcp` (the protocol it got) the client calls the
/// protocol itself (ffi_protocol), else the host answers the protocol ops (the goldens' worlds).
pub struct ClFfiHost {
    ctx: *mut c_void,
    f: EeNetClHostFn,
    q: QFfiHost,
    tcp: Option<AtomicUsize>,
}

// The context is the C++ client, used from its threads as the original's members were.
unsafe impl Send for ClFfiHost {}
unsafe impl Sync for ClFfiHost {}

impl ClFfiHost {
    fn p(&self) -> Option<*const EeNetProtocol> {
        self.tcp.as_ref().map(|t| t.load(SeqCst) as *const EeNetProtocol)
    }
    fn call(&self, op: EeNetClOp, a: usize, b: usize, c: usize, d: usize) -> isize {
        unsafe { (self.f)(self.ctx, op, a, b, c, d) }
    }
}

impl QHost for ClFfiHost {
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

impl ClientHost for ClFfiHost {
    fn wait_any(&self, events: &[Ev], timeout: u32) -> i32 {
        self.call(EeNetClOp::WaitAny, events.as_ptr() as usize, events.len(), timeout as usize, 0) as i32
    }
    fn thread_priority(&self, t: Thr) {
        self.call(EeNetClOp::ThreadPriority, t, 0, 0, 0);
    }
    fn protocol_count(&self) -> u32 {
        if self.tcp.is_some() {
            return ee_net_pr_count();
        }
        self.call(EeNetClOp::ProtocolCount, 0, 0, 0, 0) as u32
    }
    fn protocol_get(&self, out: &mut usize) -> i32 {
        if let Some(t) = &self.tcp {
            let r = unsafe { ee_net_pr_get(0, out as *mut usize as *mut *mut EeNetProtocol) };
            t.store(*out, SeqCst);
            return r;
        }
        self.call(EeNetClOp::ProtocolGet, out as *mut usize as usize, 0, 0, 0) as i32
    }
    fn connect(&self, strings: usize, event: Ev, out: &mut usize, timeout: u32) -> i32 {
        if let Some(p) = self.p() {
            let s = unsafe { &*(strings as *const EeNetClStrings) };
            let (o, e) = (out as *mut usize as *mut *mut c_void, event as *const c_void);
            let (n, h, a) = ((s.name, s.name_length), (s.host, s.host_length), (s.address, s.address_length));
            return unsafe { ee_net_pr_connect(p, n.0, n.1, h.0, h.1, a.0, a.1, 2, e, o, timeout) };
        }
        self.call(EeNetClOp::Connect, strings, event, out as *mut usize as usize, timeout as usize) as i32
    }
    fn connect_for_xfer(&self, connection: usize, event: Ev, out: &mut usize) -> i32 {
        if let Some(p) = self.p() {
            let (c, e, o) = (connection as *mut c_void, event as *const c_void, out as *mut usize as *mut *mut c_void);
            return unsafe { ee_net_pr_connect_for_xfer(p, c, 1, e, o) };
        }
        self.call(EeNetClOp::ConnectForXfer, connection, event, out as *mut usize as usize, 0) as i32
    }
    fn conn_close(&self, connection: usize) -> i32 {
        if let Some(p) = self.p() {
            return unsafe { ee_net_pr_close(p, connection as *mut c_void, false) };
        }
        self.call(EeNetClOp::ConnClose, connection, 0, 0, 0) as i32
    }
    fn conn_receive(&self, connection: usize, buf: &mut [u8], length: &mut u32) -> i32 {
        if let Some(p) = self.p() {
            let (b, n) = (buf.as_mut_ptr(), buf.len() as u32);
            return unsafe { ee_net_pr_receive(p, connection as *mut c_void, n, b, length) };
        }
        let (p, n) = (buf.as_mut_ptr() as usize, buf.len());
        self.call(EeNetClOp::ConnReceive, connection, p, n, length as *mut u32 as usize) as i32
    }
    fn conn_send(&self, connection: usize, data: usize, size: u32) -> i32 {
        if let Some(p) = self.p() {
            return unsafe { ee_net_pr_send(p, connection as *mut c_void, data as *mut u8, size) };
        }
        self.call(EeNetClOp::ConnSend, connection, data, size as usize, 0) as i32
    }
    fn detected_game_count(&self) -> u32 {
        if let Some(p) = self.p() {
            return unsafe { ee_net_pr_detected_game_count(p) };
        }
        self.call(EeNetClOp::DetectedGameCount, 0, 0, 0, 0) as u32
    }
    fn detected_game(&self, index: u32, strings: usize) -> i32 {
        if let Some(p) = self.p() {
            let s = unsafe { &*(strings as *const EeNetClSinks) };
            let (n, h, a) = ((s.name, s.name_context), (s.host, s.host_context), (s.address, s.address_context));
            return unsafe { ee_net_pr_detected_game(p, index, n.0, n.1, h.0, h.1, a.0, a.1) };
        }
        self.call(EeNetClOp::DetectedGame, index as usize, strings, 0, 0) as i32
    }
    fn search_for_game(&self, start: bool) -> i32 {
        if let Some(p) = self.p() {
            return unsafe { ee_net_pr_search_for_game(p, SERVICE.as_ptr(), SERVICE.len(), start) };
        }
        self.call(EeNetClOp::SearchForGame, start as usize, 0, 0, 0) as i32
    }
    fn xfer_receive(&self, buf: &mut [u8], received: &mut u32) -> i32 {
        if let Some(p) = self.p() {
            return unsafe { ee_net_pr_xfer_receive(p, buf.len() as u32, buf.as_mut_ptr(), received) };
        }
        let (p, n) = (buf.as_mut_ptr() as usize, buf.len());
        self.call(EeNetClOp::XferReceive, p, n, received as *mut u32 as usize, 0) as i32
    }
    fn xfer_export_size(&self) -> u32 {
        self.call(EeNetClOp::XferExportSize, 0, 0, 0, 0) as u32
    }
    fn msg_type(&self, m: Msg) -> u8 {
        self.call(EeNetClOp::MsgType, m, 0, 0, 0) as u8
    }
    fn msg_flag(&self, m: Msg, f: MsgFlag) -> bool {
        self.call(EeNetClOp::MsgFlag, m, f as usize, 0, 0) != 0
    }
    fn msg_set_slot(&self, m: Msg, originating: bool, slot: u8) {
        self.call(EeNetClOp::MsgSetSlot, m, originating as usize, slot as usize, 0);
    }
    fn msg_s2aw_id(&self, m: Msg, out: &mut u32) -> bool {
        self.call(EeNetClOp::MsgS2awId, m, out as *mut u32 as usize, 0, 0) != 0
    }
    fn msg_get(&self, m: Msg, f: MsgField) -> usize {
        self.call(EeNetClOp::MsgGet, m, f as usize, 0, 0) as usize
    }
    fn msg_new(&self, kind: NewMsg, a: usize, b: usize, c: usize, d: usize) -> Msg {
        if kind == NewMsg::XferStatus {
            let packed = (a & 0xff) | (b & 0xff) << 8 | (d & 0xff) << 16;
            return self.call(EeNetClOp::MsgNew, kind as usize, packed, c, 0) as Msg;
        }
        self.call(EeNetClOp::MsgNew, kind as usize, a, b, c) as Msg
    }
    fn msg_add_input(&self, response: Msg, input: Msg) -> i32 {
        self.call(EeNetClOp::MsgAddInput, response, input, 0, 0) as i32
    }
    fn msg_export_new(&self, m: Msg, size: &mut u32) -> usize {
        self.call(EeNetClOp::MsgExportNew, m, size as *mut u32 as usize, 0, 0) as usize
    }
    fn bytes_free(&self, data: usize) {
        self.call(EeNetClOp::BytesFree, data, 0, 0, 0);
    }
    fn fsfile_write(&self, f: FileH, data: usize, size: u32, written: &mut u32) -> i32 {
        self.call(EeNetClOp::FsWrite, f, data, size as usize, written as *mut u32 as usize) as i32
    }
    fn fsfile_close(&self, f: FileH) {
        self.call(EeNetClOp::FsClose, f, 0, 0, 0);
    }
    fn fsfile_delete(&self, f: FileH) {
        self.call(EeNetClOp::FsDelete, f, 0, 0, 0);
    }
    fn fsfile_free(&self, f: FileH) {
        self.call(EeNetClOp::FsFree, f, 0, 0, 0);
    }
    fn create_xfer_file(&self, name: usize, size: u32) -> FileH {
        self.call(EeNetClOp::CreateXferFile, name, size as usize, 0, 0) as FileH
    }
    fn is_playback_recording_session(&self) -> bool {
        self.call(EeNetClOp::IsPlaybackRecordingSession, 0, 0, 0, 0) != 0
    }
    fn connect_timeout(&self) -> u32 {
        self.call(EeNetClOp::ConnectTimeout, 0, 0, 0, 0) as u32
    }
    fn client_slot(&self) -> u8 {
        self.call(EeNetClOp::ClientSlot, 0, 0, 0, 0) as u8
    }
    fn ignore_connection_timeout(&self) -> bool {
        self.call(EeNetClOp::IgnoreConnectionTimeout, 0, 0, 0, 0) != 0
    }
    fn set_connection_state(&self, state: i32) {
        self.call(EeNetClOp::SetConnectionState, state as isize as usize, 0, 0, 0);
    }
    fn handle_message(&self, m: Msg) {
        self.call(EeNetClOp::HandleMessage, m, 0, 0, 0);
    }
    fn handle_user_input(&self, m: Msg) {
        self.call(EeNetClOp::HandleUserInput, m, 0, 0, 0);
    }
    fn handle_connection_lost(&self) {
        self.call(EeNetClOp::HandleConnectionLost, 0, 0, 0, 0);
    }
    fn handle_start_game(&self, offset: u32) -> u32 {
        self.call(EeNetClOp::HandleStartGame, offset as usize, 0, 0, 0) as u32
    }
    fn handle_stop_game(&self) {
        self.call(EeNetClOp::HandleStopGame, 0, 0, 0, 0);
    }
    fn handle_connection_timeout_warning(&self, remaining: u32) {
        self.call(EeNetClOp::HandleConnectionTimeoutWarning, remaining as usize, 0, 0, 0);
    }
    fn process_local_user_input(&self, m: Msg, player: u8) -> i32 {
        self.call(EeNetClOp::ProcessLocalUserInput, m, player as usize, 0, 0) as i32
    }
}

/// The lock-step client (`NESynchronousClient`).
pub struct EeNetClient(Box<Client<ClFfiHost>>);

fn cl<'a>(c: *const EeNetClient) -> &'a Client<ClFfiHost> {
    unsafe { &(*c).0 }
}

/// The constructor (protocol 0, the events, the lag simulator, the recorder).
///
/// # Safety
/// `host` and `qhost` answer every op for `ctx` until `ee_net_cl_free`; `comms` from `ee_net_comms_new` (the C++
/// base class's). With `tcp` the client calls the protocol itself (`ee_net_cl_connect` then takes an
/// `EeNetClStrings*`, `ee_net_cl_detected_game` an `EeNetClSinks*`); without it the host answers the protocol ops.
#[no_mangle]
pub unsafe extern "C" fn ee_net_cl_new(
    ctx: *mut c_void,
    host: EeNetClHostFn,
    qhost: EeNetQHostFn,
    comms: *const EeNetComms,
    tcp: bool,
) -> *mut EeNetClient {
    let tcp = tcp.then(|| AtomicUsize::new(0));
    let h = ClFfiHost { ctx, f: host, q: QFfiHost { ctx, f: qhost }, tcp };
    Box::into_raw(Box::new(EeNetClient(Client::new(h, comms_of(comms)))))
}

/// The destructor's work, then the object freed.
///
/// # Safety
/// `c` from `ee_net_cl_new` (or NULL), its threads ended, used by nothing after.
#[no_mangle]
pub unsafe extern "C" fn ee_net_cl_free(c: *mut EeNetClient) {
    if !c.is_null() {
        let b = Box::from_raw(c);
        b.0.release();
    }
}

/// The main thread's body (the host's `ThreadNew` of kind 4 runs it with the client).
#[no_mangle]
pub extern "C" fn ee_net_cl_main_thread(c: *const EeNetClient) -> u32 {
    cl(c).main_thread()
}

/// The receive thread's body (kind 5).
#[no_mangle]
pub extern "C" fn ee_net_cl_receive_thread(c: *const EeNetClient) -> u32 {
    cl(c).receive_thread()
}

/// The client's lag thread's body (kind 2, with the object the lag simulator handed over).
#[no_mangle]
pub extern "C" fn ee_net_cl_lag_thread(obj: *const c_void) -> u32 {
    unsafe { &*(obj as *const LagLoss<ClientQ<ClFfiHost>>) }.lag_thread()
}

/// The client's playback thread's body (kind 3, with the object the recorder handed over).
#[no_mangle]
pub extern "C" fn ee_net_cl_playback_thread(obj: *const c_void) -> u32 {
    unsafe { &*(obj as *const RecPlay<ClientQ<ClFfiHost>>) }.playback_thread()
}

#[no_mangle]
pub extern "C" fn ee_net_cl_start_threads(c: *const EeNetClient) {
    cl(c).start_threads(c as usize)
}

/// `StopThreads` and `SendExitMessage`
#[no_mangle]
pub extern "C" fn ee_net_cl_stop_threads(c: *const EeNetClient) {
    cl(c).stop_threads()
}

#[no_mangle]
pub extern "C" fn ee_net_cl_clear_incoming_messages(c: *const EeNetClient) {
    cl(c).clear_incoming_messages()
}

#[no_mangle]
pub extern "C" fn ee_net_cl_send_start_connect_message(c: *const EeNetClient, msg: *mut c_void) {
    cl(c).send_start_connect_message(msg as Msg)
}

#[no_mangle]
pub extern "C" fn ee_net_cl_send_end_connect_message(c: *const EeNetClient) {
    cl(c).send_end_connect_message()
}

/// `Connect(name, host, address)`: `strings` points at the three (an `EeNetClStrings` with `tcp`), handed to the
/// host's Connect without.
#[no_mangle]
pub extern "C" fn ee_net_cl_connect(c: *const EeNetClient, strings: *const c_void) -> i32 {
    cl(c).connect(strings as usize)
}

#[no_mangle]
pub extern "C" fn ee_net_cl_connect_for_xfer(c: *const EeNetClient) -> i32 {
    cl(c).connect_for_xfer()
}

#[no_mangle]
pub extern "C" fn ee_net_cl_disconnect(c: *const EeNetClient) -> i32 {
    cl(c).disconnect()
}

#[no_mangle]
pub extern "C" fn ee_net_cl_disconnect_from_xfer(c: *const EeNetClient) -> i32 {
    cl(c).disconnect_from_xfer()
}

/// `GetDetectedGame(index, name, host, address)`: `strings` points at the three (an `EeNetClSinks` with `tcp`),
/// handed to the host without.
#[no_mangle]
pub extern "C" fn ee_net_cl_detected_game(c: *const EeNetClient, index: u32, strings: *const c_void) -> i32 {
    cl(c).detected_game(index, strings as usize)
}

#[no_mangle]
pub extern "C" fn ee_net_cl_detected_game_count(c: *const EeNetClient) -> u32 {
    cl(c).detected_game_count()
}

#[no_mangle]
pub extern "C" fn ee_net_cl_search_for_game(c: *const EeNetClient, start: bool) -> i32 {
    cl(c).search_for_game(start)
}

/// `ProcessLocalUserInput` (`guaranteed` false) and `ProcessGuaranteedLocalUserInput`
#[no_mangle]
pub extern "C" fn ee_net_cl_process_user_input(
    c: *const EeNetClient,
    msg: *mut c_void,
    player: u8,
    guaranteed: bool,
) -> i32 {
    cl(c).process_user_input(msg as Msg, player, guaranteed)
}

/// The protocol the client uses (protocol 0, `NEProtocol*`)
#[no_mangle]
pub extern "C" fn ee_net_cl_protocol(c: *const EeNetClient) -> *mut c_void {
    cl(c).protocol() as *mut c_void
}

/// The game connection (`NEConnection*`, NULL when not connected)
#[no_mangle]
pub extern "C" fn ee_net_cl_connection(c: *const EeNetClient) -> *mut c_void {
    cl(c).connection() as *mut c_void
}

#[no_mangle]
pub extern "C" fn ee_net_cl_current_communications_tick(c: *const EeNetClient) -> u32 {
    cl(c).current_communications_tick()
}

#[no_mangle]
pub extern "C" fn ee_net_cl_set_current_communications_tick(c: *const EeNetClient, tick: u32) {
    cl(c).set_current_communications_tick(tick)
}

#[no_mangle]
pub extern "C" fn ee_net_cl_current_physics_tick(c: *const EeNetClient) -> u32 {
    cl(c).current_physics_tick()
}

#[no_mangle]
pub extern "C" fn ee_net_cl_set_current_physics_tick(c: *const EeNetClient, tick: u32) {
    cl(c).set_current_physics_tick(tick)
}

#[no_mangle]
pub extern "C" fn ee_net_cl_my_send_to_slot(c: *const EeNetClient) -> u8 {
    cl(c).my_send_to_slot()
}

#[no_mangle]
pub extern "C" fn ee_net_cl_set_my_send_to_slot(c: *const EeNetClient, slot: u8) {
    cl(c).set_my_send_to_slot(slot)
}

#[no_mangle]
pub extern "C" fn ee_net_cl_num_connection_trys(c: *const EeNetClient) -> u32 {
    cl(c).num_connection_trys()
}

#[no_mangle]
pub extern "C" fn ee_net_cl_set_max_num_connection_trys(c: *const EeNetClient, max: u32) {
    cl(c).set_max_num_connection_trys(max)
}

/// The recorder's mode (1 passing, 2 recording, 3 playing back)
#[no_mangle]
pub extern "C" fn ee_net_cl_record_playback_mode(c: *const EeNetClient) -> u8 {
    cl(c).record_playback_mode()
}

#[no_mangle]
pub extern "C" fn ee_net_cl_set_slow_playback(c: *const EeNetClient) {
    cl(c).rec().set_slow_playback()
}

#[no_mangle]
pub extern "C" fn ee_net_cl_set_normal_playback(c: *const EeNetClient) {
    cl(c).rec().set_normal_playback()
}
