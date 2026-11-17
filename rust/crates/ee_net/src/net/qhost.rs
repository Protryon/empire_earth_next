//! The calls out of the client-side message queues (`lagloss`, `recplay`): the engine's events, threads, clock and
//! floating-point control, the messages, the owner's callbacks and the record files, in the original's terms. One
//! host serves both objects; the C++ owner (the client, the server) answers them.

/// A C++ `NEMessage*` (0: none).
pub type Msg = usize;
/// A C++ `TSEvent*` (0: none).
pub type Ev = usize;
/// A C++ `TSThread*` (0: none).
pub type Thr = usize;
/// A C++ `FSFileSpec*` the caller owns (0: none).
pub type FileH = usize;

pub const ERROR_OK: i32 = 0;
pub const ERROR_ALREADY_EXISTS: i32 = 2;
pub const ERROR_TIMEOUT: i32 = 0x11;
/// `kTSWaitInfinite`
pub const WAIT_INFINITE: u32 = 0xffff_ffff;
/// `kFileAccessRead` / `kFileAccessWrite`
pub const ACCESS_READ: u32 = 1;
pub const ACCESS_WRITE: u32 = 2;

/// The thread a `thread_new` starts: its body is `LagLoss::lag_thread` / `RecPlay::playback_thread` of `obj`.
pub const THREAD_LAG: u32 = 0;
pub const THREAD_PLAYBACK: u32 = 1;

/// The owner's callbacks (`deliver`'s kind).
pub const DELIVER_LAG_RECEIVE: u32 = 0;
pub const DELIVER_LAG_SEND: u32 = 1;
pub const DELIVER_RECORD_PLAYBACK: u32 = 2;

pub trait QHost {
    /// `TMGetTime`
    fn time(&self) -> u32;
    /// `ESVERIFY_AT(false, line)` of the original unit (`unit`: 0 NELagLossSimulator.cpp, 1 NERecordPlayback.cpp,
    /// 2 NETimestampedMessage.cpp); the game's verification ends the process
    fn verify(&self, unit: u32, line: u32);
    /// A thread's start: `UMSetFloatingPointPrecision(kFPPrecision53)`
    fn fpu_start(&self);
    /// A thread loop's top: the precision set back to 53 bits and the rounding mode to nearest when they changed
    fn fpu_check(&self);
    /// `TSScheduler::CreateEvent`: the event, 0 when it failed
    fn event_new(&self) -> Ev;
    fn event_free(&self, e: Ev);
    fn event_set(&self, e: Ev);
    fn event_clear(&self, e: Ev);
    /// `TSEvent::Wait`: kErrorOK, kErrorTimeout or another error
    fn event_wait(&self, e: Ev, timeout: u32) -> i32;
    /// `TSScheduler::CreateThread` over the body of `kind` on `obj`: its result, the thread in `out`
    fn thread_new(&self, kind: u32, obj: usize, out: &mut Thr) -> i32;
    fn thread_name(&self, t: Thr, name: &str);
    fn thread_free(&self, t: Thr);
    fn msg_reference(&self, m: Msg);
    fn msg_dereference(&self, m: Msg);
    fn msg_export_size(&self, m: Msg) -> u32;
    /// `ExportMessage`: the export's first `buf.len()` bytes in `buf` (at most), its size returned
    fn msg_export(&self, m: Msg, buf: &mut [u8]) -> u32;
    /// `NEMessage::ConvertMemoryToMessage` (None: the original's NULL buffer)
    fn msg_from_bytes(&self, data: Option<&[u8]>) -> Msg;
    /// The owner's callback of `kind` (`DELIVER_*`): the send callback's result, 0 for the others
    fn deliver(&self, kind: u32, m: Msg) -> i32;
    fn file_create(&self, f: FileH) -> i32;
    fn file_delete(&self, f: FileH) -> i32;
    fn file_open(&self, f: FileH, access: u32) -> i32;
    fn file_close(&self, f: FileH);
    fn file_write(&self, f: FileH, data: &[u8]) -> i32;
    fn file_write_u32(&self, f: FileH, value: u32) -> i32;
    /// `Read(unsigned long&)`: `value` keeps what it held where the read wrote nothing
    fn file_read_u32(&self, f: FileH, value: &mut u32) -> i32;
    fn file_read(&self, f: FileH, buf: &mut [u8]) -> i32;
}

/// `NETimestampedMessage::ExportMessage`: `[timestamp 4][message size 4][message]`, little-endian, the size from
/// `ExportSize` and the bytes from `ExportMessage` (a shorter export leaves zeros; a different size fails the
/// original's verification, NETimestampedMessage.cpp line 59).
pub fn export_timestamped<H: QHost>(host: &H, m: Msg, timestamp: u32) -> Vec<u8> {
    let size = host.msg_export_size(m);
    let total = (size as usize).wrapping_add(8);
    let mut out = vec![0u8; total];
    out[..4].copy_from_slice(&timestamp.to_le_bytes());
    out[4..8].copy_from_slice(&size.to_le_bytes());
    let written = host.msg_export(m, &mut out[8..]);
    if (written as usize).wrapping_add(8) != total {
        host.verify(2, 59);
    }
    out
}
