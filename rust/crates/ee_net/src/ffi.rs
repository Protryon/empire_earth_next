//! The C ABI of the Network Engine's Rust objects (`rust/ee_net.h`). Each object is created with its C++ owner's
//! context and one host function, which answers every call out by its op.

use std::ffi::{c_char, c_void, CStr};

use crate::msgmgr::{File, Host, Msg, MsgMgr};

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

pub struct FfiHost {
    ctx: *mut c_void,
    f: EeNetMmHostFn,
}

// The context is the C++ server, used from its threads as the original's manager was.
unsafe impl Send for FfiHost {}
unsafe impl Sync for FfiHost {}

impl FfiHost {
    fn call(&self, op: EeNetMmOp, a: usize, b: usize, c: usize, d: usize) -> isize {
        unsafe { (self.f)(self.ctx, op, a, b, c, d) }
    }
}

impl Host for FfiHost {
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
    Box::into_raw(Box::new(EeNetMsgMgr(MsgMgr::new(FfiHost { ctx, f: host }))))
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
