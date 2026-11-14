//! The C ABI of the server's tuning and booting (`tune.rs`): NESynchronousServer holds an `EeNetTune*`; its calls out
//! go to the server's host function by `EeNetTuOp`.

use std::ffi::c_void;

use crate::tune::{Tune, TuneHost};

/// The server's fields the tuning reads and writes
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeNetTuField {
    /// mSendInterval (read, written)
    SendInterval = 0,
    /// mSendIntervalIncrement
    SendIntervalIncrement = 1,
    /// mMaxSendInterval
    MaxSendInterval = 2,
    /// mPTsPerSecond (read, written)
    PTsPerSecond = 3,
    /// mMaxPhysicsTicksPerSecond
    MaxPTsPerSecond = 4,
    /// mMinPhysicsTicksPerSecond
    MinPTsPerSecond = 5,
    /// mPhysicsTickCostFactor (the f32's bits)
    CostFactor = 6,
    /// mDefaultSendInterval
    DefaultSendInterval = 7,
    /// mLastCTMessageID
    LastCtMessageId = 8,
    /// mGameState
    GameState = 9,
    /// mIsMultiplayerSession
    IsMultiplayerSession = 10,
    /// the virtual GetCurrentCommunicationsTick()
    CurrentCommunicationsTick = 11,
    /// mPTsPerCT (written)
    PTsPerCt = 12,
}

/// The tuning's calls out; a, b, c as each op says, the answer as isize.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeNetTuOp {
    /// a field (EeNetTuField) -> its value
    Get = 0,
    /// a field, a value
    Set = 1,
    /// 0 mChangeSendIntervalEvent, 1 mPauseGameEvent, 2 mUnpauseGameEvent: Set()
    EventSet = 2,
    /// a message number -> ee_net_mm_outstanding
    MmOutstanding = 3,
    /// -> ee_net_mm_highest_response_time
    MmHighestResponseTime = 4,
    /// -> ee_net_mm_highest_cpu
    MmHighestCpu = 5,
    /// -> ee_net_mm_lowest_ct
    MmLowestCt = 6,
    /// a message number: ee_net_mm_resend
    MmResend = 7,
    /// a message number -> ee_net_mm_who_has_not_responded
    MmWho = 8,
    /// a slot, a message number -> ee_net_mm_who_has_not_responded_next
    MmWhoNext = 9,
    /// -> ee_net_mm_num_clients
    MmNumClients = 10,
    /// -> ee_net_mm_slowest_responder
    MmSlowestResponder = 11,
    /// -> ee_net_mm_slowest_client_cpu
    MmSlowestClientCpu = 12,
    /// on: ee_net_mm_set_force_ct
    MmSetForceCt = 13,
    /// a slot: UnregisterSlot
    UnregisterSlot = 14,
    /// BeforeRemovingPlayers()
    BeforeRemovingPlayers = 15,
    /// slot, was booted, game is exiting: BootPlayer
    BootPlayer = 16,
    /// slots, their count, the warning: ReportLaggers(std::vector of the slots, warning)
    ReportLaggers = 17,
    /// a slot: ProcessLocalUserInput(new NEMRemovePlayer(slot, true, false, false), kSlotNone)
    RemovePlayerInput = 18,
}

pub type EeNetTuHostFn =
    unsafe extern "C" fn(ctx: *mut c_void, op: EeNetTuOp, a: usize, b: usize, c: usize, d: usize) -> isize;

pub struct TuFfiHost {
    ctx: *mut c_void,
    f: EeNetTuHostFn,
}

// The context is the C++ server, used from its threads as the original's members were.
unsafe impl Send for TuFfiHost {}
unsafe impl Sync for TuFfiHost {}

impl TuFfiHost {
    fn call(&self, op: EeNetTuOp, a: usize, b: usize, c: usize) -> isize {
        unsafe { (self.f)(self.ctx, op, a, b, c, 0) }
    }
}

impl TuneHost for TuFfiHost {
    fn get(&self, f: EeNetTuField) -> u32 {
        self.call(EeNetTuOp::Get, f as usize, 0, 0) as u32
    }
    fn set(&self, f: EeNetTuField, value: u32) {
        self.call(EeNetTuOp::Set, f as usize, value as usize, 0);
    }
    fn event_set(&self, which: u32) {
        self.call(EeNetTuOp::EventSet, which as usize, 0, 0);
    }
    fn mm_outstanding(&self, number: u32) -> u8 {
        self.call(EeNetTuOp::MmOutstanding, number as usize, 0, 0) as u8
    }
    fn mm_highest_response_time(&self) -> u32 {
        self.call(EeNetTuOp::MmHighestResponseTime, 0, 0, 0) as u32
    }
    fn mm_highest_cpu(&self) -> u16 {
        self.call(EeNetTuOp::MmHighestCpu, 0, 0, 0) as u16
    }
    fn mm_lowest_ct(&self) -> u32 {
        self.call(EeNetTuOp::MmLowestCt, 0, 0, 0) as u32
    }
    fn mm_resend(&self, number: u32) {
        self.call(EeNetTuOp::MmResend, number as usize, 0, 0);
    }
    fn mm_who(&self, number: u32) -> u8 {
        self.call(EeNetTuOp::MmWho, number as usize, 0, 0) as u8
    }
    fn mm_who_next(&self, slot: u8, number: u32) -> u8 {
        self.call(EeNetTuOp::MmWhoNext, slot as usize, number as usize, 0) as u8
    }
    fn mm_num_clients(&self) -> u8 {
        self.call(EeNetTuOp::MmNumClients, 0, 0, 0) as u8
    }
    fn mm_slowest_responder(&self) -> u8 {
        self.call(EeNetTuOp::MmSlowestResponder, 0, 0, 0) as u8
    }
    fn mm_slowest_client_cpu(&self) -> u8 {
        self.call(EeNetTuOp::MmSlowestClientCpu, 0, 0, 0) as u8
    }
    fn mm_set_force_ct(&self, on: bool) {
        self.call(EeNetTuOp::MmSetForceCt, on as usize, 0, 0);
    }
    fn unregister_slot(&self, slot: u8) {
        self.call(EeNetTuOp::UnregisterSlot, slot as usize, 0, 0);
    }
    fn before_removing_players(&self) {
        self.call(EeNetTuOp::BeforeRemovingPlayers, 0, 0, 0);
    }
    fn boot_player(&self, slot: u8, was_booted: bool, game_is_exiting: bool) {
        self.call(EeNetTuOp::BootPlayer, slot as usize, was_booted as usize, game_is_exiting as usize);
    }
    fn report_laggers(&self, slots: &[u8], warning: bool) {
        self.call(EeNetTuOp::ReportLaggers, slots.as_ptr() as usize, slots.len(), warning as usize);
    }
    fn remove_player_input(&self, slot: u8) {
        self.call(EeNetTuOp::RemovePlayerInput, slot as usize, 0, 0);
    }
}

/// The server's tuning and booting state (NESynchronousServer's network and CPU tuning).
pub struct EeNetTune(Tune<TuFfiHost>);

fn tu<'a>(t: *const EeNetTune) -> &'a Tune<TuFfiHost> {
    unsafe { &(*t).0 }
}

/// # Safety
/// `host` answers every op for `ctx` until `ee_net_tu_free`.
#[no_mangle]
pub unsafe extern "C" fn ee_net_tu_new(
    ctx: *mut c_void,
    host: EeNetTuHostFn,
    send_interval_decrement: u32,
) -> *mut EeNetTune {
    Box::into_raw(Box::new(EeNetTune(Tune::new(TuFfiHost { ctx, f: host }, send_interval_decrement))))
}

/// # Safety
/// `t` from `ee_net_tu_new` (or NULL), used by nothing after.
#[no_mangle]
pub unsafe extern "C" fn ee_net_tu_free(t: *mut EeNetTune) {
    if !t.is_null() {
        drop(Box::from_raw(t));
    }
}

#[no_mangle]
pub extern "C" fn ee_net_tu_tune_server(t: *const EeNetTune) -> bool {
    tu(t).tune_server()
}

#[no_mangle]
pub extern "C" fn ee_net_tu_tune_for_slow_network(t: *const EeNetTune) -> bool {
    tu(t).tune_for_slow_network()
}

#[no_mangle]
pub extern "C" fn ee_net_tu_tune_for_slow_cpu(t: *const EeNetTune) -> bool {
    tu(t).tune_for_slow_cpu()
}

#[no_mangle]
pub extern "C" fn ee_net_tu_boot_all_laggers(t: *const EeNetTune, number: u32) {
    tu(t).boot_all_laggers(number)
}

#[no_mangle]
pub extern "C" fn ee_net_tu_boot_the_slowest_responder(t: *const EeNetTune) {
    tu(t).boot_the_slowest_responder()
}

#[no_mangle]
pub extern "C" fn ee_net_tu_boot_the_slowest_client_cpu(t: *const EeNetTune) {
    tu(t).boot_the_slowest_client_cpu()
}

#[no_mangle]
pub extern "C" fn ee_net_tu_slowest_client_cpu(t: *const EeNetTune) -> u8 {
    tu(t).slowest_client_cpu()
}

/// The tuning's own state (the tests): 12 values, `tune.rs` Tune::state's order.
///
/// # Safety
/// `out` holds 12 values.
#[no_mangle]
pub unsafe extern "C" fn ee_net_tu_state(t: *const EeNetTune, out: *mut u32) {
    let s = tu(t).state();
    std::ptr::copy_nonoverlapping(s.as_ptr(), out, 12);
}
