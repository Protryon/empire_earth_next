//! The C ABI of the Network Engine's Rust objects (`rust/ee_net.h`). Each object is created with its C++ owner's
//! context and one host function, which answers every call out by its op.

use std::ffi::{c_char, c_void, CStr};

use crate::net::native;

use crate::net::lagloss::LagLoss;
use crate::net::msgmgr::{File, Host, Msg, MsgMgr};
use crate::net::qhost::{Ev, FileH, Msg as QMsg, QHost, Thr};
use crate::net::recplay::RecPlay;

/// A message manager's calls out (`msgmgr::Host`); a, b, c, d as each op says, the answer as isize.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeNetMmOp {
    /// a message: Reference()
    MsgReference = 0,
    /// a message: Dereference()
    MsgDereference = 1,
    /// a message -> GetType()
    MsgType = 2,
    /// a sent message, a response -> sent->IsResponse(response)
    MsgIsResponse = 3,
    /// a message -> GetOriginatingSlot()
    MsgSlot = 4,
    /// a slot, a message: exported and sent by the protocol's SendTo -> its result
    MsgSendTo = 5,
    /// a communications tick -> GetCurrentCommunicationsTick()
    MsgTick = 6,
    /// a kind (tick, pause, unpause), the server's message -> its response from the server's slot (one reference)
    NewResponse = 7,
    /// -> the server's mLastTickReceived
    LastTick = 8,
    /// a tick: the server's mLastTickReceived = tick
    SetLastTick = 9,
    /// -> the server's mNumberOfCTsProcessedInPhysics
    CtsProcessed = 10,
    /// -> GetAveragePhysicsUpdateTime()
    AvgPhysics = 11,
    /// a message: ProcessSystemMessage(message)
    ProcessSystemMessage = 12,
    /// mSendThreadForceCTEvent->Set()
    ForceCt = 13,
    /// -> mLastCTMessageID
    LastCtMessageId = 14,
    /// number, type, timeout, retries: the server's send-to-all-and-wait fields, then mWaitForResponseEvent->Set()
    StartSendToAllAndWait = 15,
    /// -> TMGetTime()
    Time = 16,
    /// a slot: ResetClientConnection(slot)
    ResetClientConnection = 17,
    /// slot, percent, transfer id, status: HandleXferStatus
    XferStatus = 18,
    /// slot, status, data, size: an NEMXferMessage sent by XferSendTo -> 1 when it went
    XferSend = 19,
    /// an FSFileSpec& -> a new FSFileSpec, its copy
    FileClone = 20,
    /// a copy: deleted
    FileFree = 21,
    /// a copy -> GetIsOpen()
    FileIsOpen = 22,
    /// a copy -> 1 when Open(kFileAccessRead) is kErrorOK
    FileOpen = 23,
    /// a copy: Close()
    FileClose = 24,
    /// a copy -> GetLength()
    FileLength = 25,
    /// a copy, a 1 KB buffer -> the size Read gave, or -1 when it failed
    FileRead = 26,
    /// a copy -> GetFile(), a zero-terminated name
    FileName = 27,
}

pub type EeNetMmHostFn =
    unsafe extern "C" fn(ctx: *mut c_void, op: EeNetMmOp, a: usize, b: usize, c: usize, d: usize) -> isize;

/// The message manager's calls out as one function: the C++ host's (`RawMm`), or the Rust server's, which answers
/// a few itself (server.rs)
pub trait MmCall {
    fn mm_call(&self, op: EeNetMmOp, a: usize, b: usize, c: usize, d: usize) -> isize;
}

pub struct RawMm {
    ctx: *mut c_void,
    f: EeNetMmHostFn,
}

// The context is the C++ server, used from its threads as the original's manager was.
unsafe impl Send for RawMm {}
unsafe impl Sync for RawMm {}

impl MmCall for RawMm {
    fn mm_call(&self, op: EeNetMmOp, a: usize, b: usize, c: usize, d: usize) -> isize {
        unsafe { (self.f)(self.ctx, op, a, b, c, d) }
    }
}

/// The manager's host (`msgmgr::Host`) over a call
pub struct MmVia<C: MmCall>(pub C);

/// The manager's host over the C++ function
pub type FfiHost = MmVia<RawMm>;

impl<C: MmCall> MmVia<C> {
    fn call(&self, op: EeNetMmOp, a: usize, b: usize, c: usize, d: usize) -> isize {
        self.0.mm_call(op, a, b, c, d)
    }
}

impl<C: MmCall> Host for MmVia<C> {
    fn msg_reference(&self, m: Msg) {
        self.call(EeNetMmOp::MsgReference, m, 0, 0, 0);
    }
    fn msg_dereference(&self, m: Msg) {
        self.call(EeNetMmOp::MsgDereference, m, 0, 0, 0);
    }
    fn msg_type(&self, m: Msg) -> u8 {
        self.call(EeNetMmOp::MsgType, m, 0, 0, 0) as u8
    }
    fn msg_is_response(&self, sent: Msg, resp: Msg) -> bool {
        self.call(EeNetMmOp::MsgIsResponse, sent, resp, 0, 0) != 0
    }
    fn msg_slot(&self, m: Msg) -> u8 {
        self.call(EeNetMmOp::MsgSlot, m, 0, 0, 0) as u8
    }
    fn msg_send_to(&self, slot: u8, m: Msg) -> u32 {
        self.call(EeNetMmOp::MsgSendTo, slot as usize, m, 0, 0) as u32
    }
    fn msg_tick(&self, m: Msg) -> u32 {
        self.call(EeNetMmOp::MsgTick, m, 0, 0, 0) as u32
    }
    fn new_response(&self, kind: u8, m: Msg) -> Msg {
        self.call(EeNetMmOp::NewResponse, kind as usize, m, 0, 0) as Msg
    }
    fn last_tick(&self) -> i32 {
        self.call(EeNetMmOp::LastTick, 0, 0, 0, 0) as i32
    }
    fn set_last_tick(&self, tick: u32) {
        self.call(EeNetMmOp::SetLastTick, tick as usize, 0, 0, 0);
    }
    fn cts_processed(&self) -> u32 {
        self.call(EeNetMmOp::CtsProcessed, 0, 0, 0, 0) as u32
    }
    fn avg_physics(&self) -> u32 {
        self.call(EeNetMmOp::AvgPhysics, 0, 0, 0, 0) as u32
    }
    fn process_system_message(&self, m: Msg) {
        self.call(EeNetMmOp::ProcessSystemMessage, m, 0, 0, 0);
    }
    fn force_ct(&self) {
        self.call(EeNetMmOp::ForceCt, 0, 0, 0, 0);
    }
    fn last_ct_message_id(&self) -> u32 {
        self.call(EeNetMmOp::LastCtMessageId, 0, 0, 0, 0) as u32
    }
    fn start_send_to_all_and_wait(&self, number: u32, kind: u8, timeout: u32, retries: u32) {
        self.call(EeNetMmOp::StartSendToAllAndWait, number as usize, kind as usize, timeout as usize, retries as usize);
    }
    fn time(&self) -> u32 {
        self.call(EeNetMmOp::Time, 0, 0, 0, 0) as u32
    }
    fn reset_client_connection(&self, slot: u8) {
        self.call(EeNetMmOp::ResetClientConnection, slot as usize, 0, 0, 0);
    }
    fn xfer_status(&self, slot: u8, percent: u8, id: u32, status: u8) {
        self.call(EeNetMmOp::XferStatus, slot as usize, percent as usize, id as usize, status as usize);
    }
    fn xfer_send(&self, slot: u8, status: u8, data: &[u8]) -> bool {
        self.call(EeNetMmOp::XferSend, slot as usize, status as usize, data.as_ptr() as usize, data.len()) != 0
    }
    fn file_clone(&self, spec: usize) -> File {
        self.call(EeNetMmOp::FileClone, spec, 0, 0, 0) as File
    }
    fn file_free(&self, f: File) {
        self.call(EeNetMmOp::FileFree, f, 0, 0, 0);
    }
    fn file_is_open(&self, f: File) -> bool {
        self.call(EeNetMmOp::FileIsOpen, f, 0, 0, 0) != 0
    }
    fn file_open(&self, f: File) -> bool {
        self.call(EeNetMmOp::FileOpen, f, 0, 0, 0) != 0
    }
    fn file_close(&self, f: File) {
        self.call(EeNetMmOp::FileClose, f, 0, 0, 0);
    }
    fn file_length(&self, f: File) -> u32 {
        self.call(EeNetMmOp::FileLength, f, 0, 0, 0) as u32
    }
    fn file_read(&self, f: File, buf: &mut [u8; 0x400]) -> Option<u32> {
        let r = self.call(EeNetMmOp::FileRead, f, buf.as_mut_ptr() as usize, 0, 0);
        (r >= 0).then_some(r as u32)
    }
    fn file_name(&self, f: File) -> Vec<u8> {
        let p = self.call(EeNetMmOp::FileName, f, 0, 0, 0) as *const c_char;
        if p.is_null() {
            return Vec::new();
        }
        unsafe { CStr::from_ptr(p) }.to_bytes().to_vec()
    }
}

/// The server's message manager (`NEMessageManager`).
pub struct EeNetMsgMgr(MsgMgr<FfiHost>);

fn mm<'a>(m: *const EeNetMsgMgr) -> &'a MsgMgr<FfiHost> {
    unsafe { &(*m).0 }
}

/// # Safety
/// `host` answers every op for `ctx` until `ee_net_mm_free`.
#[no_mangle]
pub unsafe extern "C" fn ee_net_mm_new(ctx: *mut c_void, host: EeNetMmHostFn) -> *mut EeNetMsgMgr {
    Box::into_raw(Box::new(EeNetMsgMgr(MsgMgr::new(MmVia(RawMm { ctx, f: host })))))
}

/// # Safety
/// `m` from `ee_net_mm_new` (or NULL), used by nothing after.
#[no_mangle]
pub unsafe extern "C" fn ee_net_mm_free(m: *mut EeNetMsgMgr) {
    if !m.is_null() {
        let b = Box::from_raw(m);
        b.0.release();
    }
}

#[no_mangle]
pub extern "C" fn ee_net_mm_abort_xfer(m: *const EeNetMsgMgr, id: u32) {
    mm(m).abort_xfer(id)
}

/// The new transfer's id in `out_id` and true, or false for a slot with no queue.
///
/// # Safety
/// `out_id` writable.
#[no_mangle]
pub unsafe extern "C" fn ee_net_mm_add_xfer(
    m: *const EeNetMsgMgr,
    spec: *mut c_void,
    user_data: u32,
    slot: u8,
    out_id: *mut u32,
) -> bool {
    match mm(m).add_xfer(spec as usize, user_data, slot) {
        Some(id) => {
            *out_id = id;
            true
        }
        None => false,
    }
}

#[no_mangle]
pub extern "C" fn ee_net_mm_highest_cpu(m: *const EeNetMsgMgr) -> u16 {
    mm(m).highest_cpu()
}

#[no_mangle]
pub extern "C" fn ee_net_mm_highest_response_time(m: *const EeNetMsgMgr) -> u32 {
    mm(m).highest_response_time()
}

#[no_mangle]
pub extern "C" fn ee_net_mm_lowest_ct(m: *const EeNetMsgMgr) -> u32 {
    mm(m).lowest_ct()
}

#[no_mangle]
pub extern "C" fn ee_net_mm_slowest_responder(m: *const EeNetMsgMgr) -> u8 {
    mm(m).slowest_responder()
}

#[no_mangle]
pub extern "C" fn ee_net_mm_slowest_client_cpu(m: *const EeNetMsgMgr) -> u8 {
    mm(m).slowest_client_cpu()
}

#[no_mangle]
pub extern "C" fn ee_net_mm_log_response(m: *const EeNetMsgMgr, msg: *mut c_void) {
    mm(m).log_response(msg as Msg)
}

#[no_mangle]
pub extern "C" fn ee_net_mm_log_response_time(m: *const EeNetMsgMgr, slot: u8, ct: u32, cpu: u16) {
    mm(m).log_response_time(slot, ct, cpu)
}

#[no_mangle]
pub extern "C" fn ee_net_mm_outstanding(m: *const EeNetMsgMgr, number: u32) -> u8 {
    mm(m).outstanding(number)
}

#[no_mangle]
pub extern "C" fn ee_net_mm_process_xfers(m: *const EeNetMsgMgr) -> bool {
    mm(m).process_xfers()
}

#[no_mangle]
pub extern "C" fn ee_net_mm_register_slot(m: *const EeNetMsgMgr, slot: u8) -> bool {
    mm(m).register_slot(slot)
}

#[no_mangle]
pub extern "C" fn ee_net_mm_resend(m: *const EeNetMsgMgr, number: u32) -> bool {
    mm(m).resend(number)
}

#[no_mangle]
pub extern "C" fn ee_net_mm_send(m: *const EeNetMsgMgr, msg: *mut c_void, guaranteed: bool) -> u32 {
    mm(m).send(msg as Msg, guaranteed)
}

#[no_mangle]
pub extern "C" fn ee_net_mm_send_to(m: *const EeNetMsgMgr, msg: *mut c_void, slot: u8, guaranteed: bool) -> u32 {
    mm(m).send_to(msg as Msg, slot, guaranteed)
}

#[no_mangle]
pub extern "C" fn ee_net_mm_send_to_all_and_wait(
    m: *const EeNetMsgMgr,
    msg: *mut c_void,
    timeout: u32,
    retries: u32,
) -> u32 {
    mm(m).send_to_all_and_wait(msg as Msg, timeout, retries)
}

#[no_mangle]
pub extern "C" fn ee_net_mm_set_lowest_cts(m: *const EeNetMsgMgr, ct: u32) {
    mm(m).set_lowest_cts(ct)
}

#[no_mangle]
pub extern "C" fn ee_net_mm_unregister_slot(m: *const EeNetMsgMgr, slot: u8) {
    mm(m).unregister_slot(slot)
}

#[no_mangle]
pub extern "C" fn ee_net_mm_who_has_not_responded(m: *const EeNetMsgMgr, number: u32) -> u8 {
    mm(m).who_has_not_responded(number)
}

#[no_mangle]
pub extern "C" fn ee_net_mm_who_has_not_responded_next(m: *const EeNetMsgMgr, previous: u8, number: u32) -> u8 {
    mm(m).who_has_not_responded_next(previous, number)
}

#[no_mangle]
pub extern "C" fn ee_net_mm_num_clients(m: *const EeNetMsgMgr) -> u8 {
    mm(m).num_clients()
}

#[no_mangle]
pub extern "C" fn ee_net_mm_current_number(m: *const EeNetMsgMgr) -> u32 {
    mm(m).current_number()
}

#[no_mangle]
pub extern "C" fn ee_net_mm_set_current_number(m: *const EeNetMsgMgr, number: u32) {
    mm(m).set_current_number(number)
}

#[no_mangle]
pub extern "C" fn ee_net_mm_set_force_ct(m: *const EeNetMsgMgr, on: bool) {
    mm(m).set_force_ct(on)
}

// ---- the client-side queues: NELagLossSimulator (lagloss.rs) and NERecordPlayback (recplay.rs)

/// The queues' calls out (`qhost::QHost`); a, b, c, d as each op says, the answer as isize.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeNetQOp {
    /// -> TMGetTime()
    Time = 0,
    /// unit (0 NELagLossSimulator.cpp, 1 NERecordPlayback.cpp, 2 NETimestampedMessage.cpp), line: a failed ESVERIFY_AT
    Verify = 1,
    /// UMSetFloatingPointPrecision(kFPPrecision53)
    FpuStart = 2,
    /// the thread loop's precision and rounding check
    FpuCheck = 3,
    /// -> a new TSEvent (TSScheduler::CreateEvent), NULL when it failed
    EventNew = 4,
    /// an event: TSScheduler::DestroyEvent
    EventFree = 5,
    /// an event: Set()
    EventSet = 6,
    /// an event: Clear()
    EventClear = 7,
    /// an event, a timeout -> Wait(timeout)
    EventWait = 8,
    /// kind (0 the lag thread, 1 the playback thread), the Rust object, a TSThread** -> CreateThread's result
    ThreadNew = 9,
    /// a thread, a name, its length: SetName
    ThreadName = 10,
    /// a thread: TSScheduler::DestroyThread
    ThreadFree = 11,
    /// a message: Reference()
    MsgReference = 12,
    /// a message: Dereference()
    MsgDereference = 13,
    /// a message -> ExportSize()
    MsgExportSize = 14,
    /// a message, a buffer, its size: ExportMessage, at most that many bytes copied -> the export's size
    MsgExport = 15,
    /// bytes (NULL or a buffer), their count -> NEMessage::ConvertMemoryToMessage(bytes)
    MsgFromBytes = 16,
    /// kind (0 the lag simulator's receive callback, 1 its send callback, 2 the recorder's callback), a message ->
    /// the send callback's result
    Deliver = 17,
    /// a file -> Create()
    FileCreate = 18,
    /// a file -> Delete()
    FileDelete = 19,
    /// a file, an access -> Open(access)
    FileOpen = 20,
    /// a file: Close()
    FileClose = 21,
    /// a file, bytes, their count -> Write(bytes, count, written)
    FileWrite = 22,
    /// a file, a value -> Write(value) (4 bytes)
    FileWriteU32 = 23,
    /// a file, a uint32_t* -> Read(*value)
    FileReadU32 = 24,
    /// a file, a buffer, its size -> Read(buffer, size, read)
    FileRead = 25,
}

pub type EeNetQHostFn =
    unsafe extern "C" fn(ctx: *mut c_void, op: EeNetQOp, a: usize, b: usize, c: usize, d: usize) -> isize;

/// With `native` (the game's server and client) the clock, the events and the threads are the engine's own, called
/// directly (`native.rs`: ee_time, ee_ts); without it (the goldens' worlds) the host answers those ops too.
pub struct QFfiHost {
    pub(crate) ctx: *mut c_void,
    pub(crate) f: EeNetQHostFn,
    pub(crate) native: bool,
}

// The context is the C++ client or server, used from its threads as the original's objects were.
unsafe impl Send for QFfiHost {}
unsafe impl Sync for QFfiHost {}

impl QFfiHost {
    fn call(&self, op: EeNetQOp, a: usize, b: usize, c: usize, d: usize) -> isize {
        unsafe { (self.f)(self.ctx, op, a, b, c, d) }
    }
}

impl QHost for QFfiHost {
    fn time(&self) -> u32 {
        if self.native {
            return native::time();
        }
        self.call(EeNetQOp::Time, 0, 0, 0, 0) as u32
    }
    fn verify(&self, unit: u32, line: u32) {
        self.call(EeNetQOp::Verify, unit as usize, line as usize, 0, 0);
    }
    fn fpu_start(&self) {
        self.call(EeNetQOp::FpuStart, 0, 0, 0, 0);
    }
    fn fpu_check(&self) {
        self.call(EeNetQOp::FpuCheck, 0, 0, 0, 0);
    }
    fn event_new(&self) -> Ev {
        if self.native {
            return native::event_new();
        }
        self.call(EeNetQOp::EventNew, 0, 0, 0, 0) as Ev
    }
    fn event_free(&self, e: Ev) {
        if self.native {
            return native::event_free(e);
        }
        self.call(EeNetQOp::EventFree, e, 0, 0, 0);
    }
    fn event_set(&self, e: Ev) {
        if self.native {
            return native::event_set(e);
        }
        self.call(EeNetQOp::EventSet, e, 0, 0, 0);
    }
    fn event_clear(&self, e: Ev) {
        if self.native {
            return native::event_clear(e);
        }
        self.call(EeNetQOp::EventClear, e, 0, 0, 0);
    }
    fn event_wait(&self, e: Ev, timeout: u32) -> i32 {
        if self.native {
            return native::event_wait(e, timeout);
        }
        self.call(EeNetQOp::EventWait, e, timeout as usize, 0, 0) as i32
    }
    fn thread_new(&self, kind: u32, obj: usize, out: &mut Thr) -> i32 {
        if self.native {
            return native::thread_new(kind, obj, out);
        }
        self.call(EeNetQOp::ThreadNew, kind as usize, obj, out as *mut Thr as usize, 0) as i32
    }
    fn thread_name(&self, t: Thr, name: &str) {
        if self.native {
            return native::thread_name(t, name);
        }
        self.call(EeNetQOp::ThreadName, t, name.as_ptr() as usize, name.len(), 0);
    }
    fn thread_free(&self, t: Thr) {
        if self.native {
            return native::thread_free(t);
        }
        self.call(EeNetQOp::ThreadFree, t, 0, 0, 0);
    }
    fn msg_reference(&self, m: QMsg) {
        self.call(EeNetQOp::MsgReference, m, 0, 0, 0);
    }
    fn msg_dereference(&self, m: QMsg) {
        self.call(EeNetQOp::MsgDereference, m, 0, 0, 0);
    }
    fn msg_export_size(&self, m: QMsg) -> u32 {
        self.call(EeNetQOp::MsgExportSize, m, 0, 0, 0) as u32
    }
    fn msg_export(&self, m: QMsg, buf: &mut [u8]) -> u32 {
        self.call(EeNetQOp::MsgExport, m, buf.as_mut_ptr() as usize, buf.len(), 0) as u32
    }
    fn msg_from_bytes(&self, data: Option<&[u8]>) -> QMsg {
        let (p, n) = data.map_or((0, 0), |d| (d.as_ptr() as usize, d.len()));
        self.call(EeNetQOp::MsgFromBytes, p, n, 0, 0) as QMsg
    }
    fn deliver(&self, kind: u32, m: QMsg) -> i32 {
        self.call(EeNetQOp::Deliver, kind as usize, m, 0, 0) as i32
    }
    fn file_create(&self, f: FileH) -> i32 {
        self.call(EeNetQOp::FileCreate, f, 0, 0, 0) as i32
    }
    fn file_delete(&self, f: FileH) -> i32 {
        self.call(EeNetQOp::FileDelete, f, 0, 0, 0) as i32
    }
    fn file_open(&self, f: FileH, access: u32) -> i32 {
        self.call(EeNetQOp::FileOpen, f, access as usize, 0, 0) as i32
    }
    fn file_close(&self, f: FileH) {
        self.call(EeNetQOp::FileClose, f, 0, 0, 0);
    }
    fn file_write(&self, f: FileH, data: &[u8]) -> i32 {
        self.call(EeNetQOp::FileWrite, f, data.as_ptr() as usize, data.len(), 0) as i32
    }
    fn file_write_u32(&self, f: FileH, value: u32) -> i32 {
        self.call(EeNetQOp::FileWriteU32, f, value as usize, 0, 0) as i32
    }
    fn file_read_u32(&self, f: FileH, value: &mut u32) -> i32 {
        self.call(EeNetQOp::FileReadU32, f, value as *mut u32 as usize, 0, 0) as i32
    }
    fn file_read(&self, f: FileH, buf: &mut [u8]) -> i32 {
        self.call(EeNetQOp::FileRead, f, buf.as_mut_ptr() as usize, buf.len(), 0) as i32
    }
}

/// The client's lag and loss simulator (`NELagLossSimulator`).
pub struct EeNetLagLoss(LagLoss<QFfiHost>);
/// The client's and the server's message recorder (`NERecordPlayback`).
pub struct EeNetRecPlay(RecPlay<QFfiHost>);

fn ll<'a>(l: *const EeNetLagLoss) -> &'a LagLoss<QFfiHost> {
    unsafe { &(*l).0 }
}

fn rp<'a>(r: *const EeNetRecPlay) -> Option<&'a RecPlay<QFfiHost>> {
    unsafe { r.as_ref().map(|r| &r.0) }
}

/// # Safety
/// `host` answers every op for `ctx` until `ee_net_ll_free`.
#[no_mangle]
pub unsafe extern "C" fn ee_net_ll_new(ctx: *mut c_void, host: EeNetQHostFn) -> *mut EeNetLagLoss {
    Box::into_raw(Box::new(EeNetLagLoss(LagLoss::new(QFfiHost { ctx, f: host, native: false }))))
}

/// # Safety
/// `l` from `ee_net_ll_new` (or NULL), used by nothing after.
#[no_mangle]
pub unsafe extern "C" fn ee_net_ll_free(l: *mut EeNetLagLoss) {
    if !l.is_null() {
        let b = Box::from_raw(l);
        b.0.release();
    }
}

/// The lag thread's body (the host's `ThreadNew` of kind 0 runs it with the object it was given).
#[no_mangle]
pub extern "C" fn ee_net_ll_thread(l: *const EeNetLagLoss) -> u32 {
    ll(l).lag_thread()
}

#[no_mangle]
pub extern "C" fn ee_net_ll_enable_lag(l: *const EeNetLagLoss, minimum: u32, maximum: u32) {
    ll(l).enable_lag(l as usize, minimum, maximum)
}

#[no_mangle]
pub extern "C" fn ee_net_ll_disable_lag(l: *const EeNetLagLoss, flush: bool) {
    ll(l).disable_lag(flush)
}

#[no_mangle]
pub extern "C" fn ee_net_ll_enable_receive_loss(l: *const EeNetLagLoss, percentage: f32) {
    ll(l).enable_receive_loss(percentage)
}

#[no_mangle]
pub extern "C" fn ee_net_ll_enable_send_loss(l: *const EeNetLagLoss, percentage: f32) {
    ll(l).enable_send_loss(percentage)
}

#[no_mangle]
pub extern "C" fn ee_net_ll_disable_receive_loss(l: *const EeNetLagLoss) {
    ll(l).disable_receive_loss()
}

#[no_mangle]
pub extern "C" fn ee_net_ll_disable_send_loss(l: *const EeNetLagLoss) {
    ll(l).disable_send_loss()
}

#[no_mangle]
pub extern "C" fn ee_net_ll_process(l: *const EeNetLagLoss, msg: *mut c_void) -> i32 {
    ll(l).process(msg as QMsg)
}

#[no_mangle]
pub extern "C" fn ee_net_ll_send(l: *const EeNetLagLoss, msg: *mut c_void) -> i32 {
    ll(l).send(msg as QMsg)
}

/// # Safety
/// `host` answers every op for `ctx` until `ee_net_rp_free`.
#[no_mangle]
pub unsafe extern "C" fn ee_net_rp_new(ctx: *mut c_void, host: EeNetQHostFn) -> *mut EeNetRecPlay {
    Box::into_raw(Box::new(EeNetRecPlay(RecPlay::new(QFfiHost { ctx, f: host, native: false }))))
}

/// # Safety
/// `r` from `ee_net_rp_new` (or NULL), used by nothing after.
#[no_mangle]
pub unsafe extern "C" fn ee_net_rp_free(r: *mut EeNetRecPlay) {
    if !r.is_null() {
        let b = Box::from_raw(r);
        b.0.release();
    }
}

/// The playback thread's body (the host's `ThreadNew` of kind 1 runs it with the object it was given).
#[no_mangle]
pub extern "C" fn ee_net_rp_thread(r: *const EeNetRecPlay) -> u32 {
    rp(r).map_or(0, |r| r.playback_thread())
}

/// The mode (1 passing, 2 recording, 3 playing back); a NULL recorder (a server that was not published) passes.
#[no_mangle]
pub extern "C" fn ee_net_rp_mode(r: *const EeNetRecPlay) -> u8 {
    rp(r).map_or(crate::net::recplay::MODE_PASSING, |r| r.mode())
}

/// A message for the game (a NULL recorder: nothing, where the original crashed).
#[no_mangle]
pub extern "C" fn ee_net_rp_process(r: *const EeNetRecPlay, msg: *mut c_void) {
    if let Some(r) = rp(r) {
        r.process(msg as QMsg)
    }
}

#[no_mangle]
pub extern "C" fn ee_net_rp_pass_mode(r: *const EeNetRecPlay) -> i32 {
    rp(r).map_or(0, |r| r.pass_mode())
}

#[no_mangle]
pub extern "C" fn ee_net_rp_record_mode_file(r: *const EeNetRecPlay, file: *mut c_void) -> i32 {
    rp(r).map_or(0, |r| r.record_mode_file(file as FileH))
}

#[no_mangle]
pub extern "C" fn ee_net_rp_record_mode_network(r: *const EeNetRecPlay) -> i32 {
    rp(r).map_or(0, |r| r.record_mode_network())
}

#[no_mangle]
pub extern "C" fn ee_net_rp_playback_mode_file(r: *const EeNetRecPlay, file: *mut c_void) -> i32 {
    rp(r).map_or(0, |x| x.playback_mode_file(r as usize, file as FileH))
}

#[no_mangle]
pub extern "C" fn ee_net_rp_playback_mode_network(r: *const EeNetRecPlay) -> i32 {
    rp(r).map_or(0, |r| r.playback_mode_network())
}

#[no_mangle]
pub extern "C" fn ee_net_rp_playback_complete(r: *const EeNetRecPlay) -> bool {
    rp(r).is_some_and(|r| r.playback_complete())
}

#[no_mangle]
pub extern "C" fn ee_net_rp_set_slow_playback(r: *const EeNetRecPlay) {
    if let Some(r) = rp(r) {
        r.set_slow_playback()
    }
}

#[no_mangle]
pub extern "C" fn ee_net_rp_set_normal_playback(r: *const EeNetRecPlay) {
    if let Some(r) = rp(r) {
        r.set_normal_playback()
    }
}
