//! `NESynchronousServer` whole (the original NESynchronousServer.cpp, LLE 0x10047400..0x10049eee, with its inline
//! members; PORT_LEARNINGS "LLE Network Engine, round 4"): the lock-step multiplayer server. Its main thread
//! ("CommMain") starts, pauses and stops the game, hands the clients' packets to the recorder, waits for the answers to
//! a send-to-all-and-wait (resending, then booting the laggers) and for reconnecting clients; its send thread
//! ("CommSend") sends a communications tick every send interval (`send_communications_tick`, the lock-step heart: the
//! inputs queued since the last tick, the physics ticks it lets run, the checksums it asks for); its transfer thread
//! ("CommXfer") pumps the queued file transfers. The message manager (`msgmgr`), the tuning (`tune`) and the recorder
//! (`recplay`) are this object's own; their calls out that reach the server are its methods.
//!
//! The original's critical sections are re-entrant locks held where it held them (the tick across its whole send;
//! the send-to-all-and-wait queue across the tick and the main thread's answer; pause and unpause; the incoming
//! inputs); the queues are reached only under them and never across a call out. Every other field is an atomic: the
//! original shared them between its threads without a lock. The C++ class keeps the type callers hold, its virtual
//! members (the game's `EEServer` overrides them), the base class's fields and the published name, all reached
//! through the host.

use std::cell::UnsafeCell;
use std::collections::{BTreeMap, VecDeque};
use std::sync::atomic::{AtomicBool, AtomicU32, AtomicU8, AtomicUsize, Ordering::SeqCst};
use std::sync::{Arc, Mutex};

use crate::ffi::EeNetMmOp;
use crate::msgmgr::MsgMgr;
use crate::qhost::*;
use crate::recplay::RecPlay;
use crate::rlock::RLock;
use crate::tune::{f32_to_u32, Field as TuField, Tune, TuneHost};

pub use crate::comms::Comms;
pub use crate::ffi::{MmCall, MmVia};
pub use crate::ffi_comms::EeNetCommsField as BaseField;
pub use crate::ffi_server::{
    EeNetSvEvent as SvEvent, EeNetSvGet as SvGet, EeNetSvMsgField as MsgField, EeNetSvMsgFlag as MsgFlag,
    EeNetSvNewMsg as NewMsg, EeNetSvSet as SvSet,
};

/// `ESVERIFY_AT`'s unit for this file (QHost::verify)
pub const UNIT: u32 = 4;

/// The server's own slot, and none
pub const SLOT_SERVER: u8 = 0xfe;
pub const SLOT_NONE: u8 = 0xff;

/// `TSEvent::WaitAny`'s answers besides an index
pub const WAIT_ANY_TIMEOUT: i32 = -1;
pub const WAIT_ANY_FAILED: i32 = -2;

/// The thread kinds this object starts (`QHost::thread_new`; 0 to 5 are the queues' and the client's)
pub const THREAD_SV_PLAYBACK: u32 = 6;
pub const THREAD_SV_MAIN: u32 = 7;
pub const THREAD_SV_SEND: u32 = 8;
pub const THREAD_SV_XFER: u32 = 9;

// the game states (NEServerGameState)
pub const STATE_OPEN: u8 = 0;
pub const STATE_CLOSED: u8 = 1;
pub const STATE_RUNNING: u8 = 3;
pub const STATE_PAUSED: u8 = 4;

// message types (NEMessageID.h)
pub const MSG_ADD_PLAYER_RESPONSE: u8 = 0x21;
pub const MSG_CLIENT_CONNECTION_REQUEST: u8 = 0x22;
pub const MSG_CLIENT_DISCONNECTION_NOTIFICATION: u8 = 0x24;
pub const MSG_COMMUNICATIONS_TICK_RESPONSE: u8 = 0x27;
pub const MSG_PLAYER_CONNECTION_CHECK_RESPONSE: u8 = 0x2a;
pub const MSG_START_GAME_RESPONSE: u8 = 0x2e;
pub const MSG_STOP_GAME_RESPONSE: u8 = 0x30;
pub const MSG_WORLD_DATABASE_CHECKSUM_RESPONSE: u8 = 0x32;
pub const MSG_PAUSE_GAME: u8 = 0x33;
pub const MSG_PAUSE_GAME_RESPONSE: u8 = 0x34;
pub const MSG_UNPAUSE_GAME: u8 = 0x35;
pub const MSG_UNPAUSE_GAME_RESPONSE: u8 = 0x36;
pub const MSG_CLIENT_RECONNECTION_REQUEST: u8 = 0x37;

/// A received packet's buffer (`unsigned char theBuffer[0x200]`)
const PACKET_SIZE: usize = 0x200;
/// A tick's packet: its inputs stop before their export reaches it (with 0x1c bytes of header)
const TICK_PACKET: u32 = 0x240;

/// The server's calls out besides the queues' (`QHost`): the engine, the protocol and its server, the messages, the
/// message manager's calls that reach the C++ and the C++ class's virtual members.
pub trait ServerHost: QHost {
    /// `TSEvent::WaitAny`: the index of the set event (the lowest), WAIT_ANY_TIMEOUT or WAIT_ANY_FAILED
    fn wait_any(&self, events: &[Ev], timeout: u32) -> i32;
    /// `SetPriority(PLAT_PRIORITY_TIME_CRITICAL)`
    fn thread_priority(&self, t: Thr);
    /// `PlatSleep(ms)`
    fn sleep(&self, ms: u32);
    /// `gPlatSimMode` (TEST ONLY: the headless simulation's physics thread sends the ticks)
    fn sim_mode(&self) -> bool;
    /// the message manager's calls out that reach the C++ (the messages, the protocol server, the base class, the
    /// game, the files): NESynchronousServer::MessageManagerHost's op
    fn mm(&self, op: EeNetMmOp, a: usize, b: usize, c: usize, d: usize) -> isize;

    /// `NEProtocol::GetCount()`
    fn protocol_count(&self) -> u32;
    /// `NEProtocol::GetProtocol(0, out)`
    fn protocol_get(&self, out: &mut usize) -> i32;
    /// `Publish("EEService", mName, 2, input, xfer input, xfer accept, out, allow, announce, adapter)` (the name, the
    /// announcement and the adapter are the C++ constructor's)
    fn publish(&self, protocol: usize, events: &[Ev; 3], server: &mut usize, allow: bool) -> i32;
    /// `AllowNewConnections(server, allow)`
    fn allow_new_connections(&self, protocol: usize, server: usize, allow: bool);
    /// `AcceptForXfer(server, connection)` (the connection a local the original never read)
    fn accept_for_xfer(&self, protocol: usize, server: usize);
    /// `SearchForGame(UString("Anything"), UString("EEService"), start)`
    fn search_for_game(&self, protocol: usize, start: bool) -> i32;
    /// `NEServer::SendTo(slot, data, size)`
    fn server_send_to(&self, server: usize, slot: u8, data: usize, size: u32) -> i32;
    /// `NEServer::ReceiveFrom(size, buffer, size out, index out)`
    fn server_receive_from(&self, server: usize, buf: &mut [u8], size: &mut u32, index: &mut u32) -> i32;
    /// `NEServer::Close()`
    fn server_close(&self, server: usize);

    fn msg_type(&self, m: Msg) -> u8;
    /// `SetForSpecificSlot` (false) or `SetOriginatingSlot` (true)
    fn msg_set_slot(&self, m: Msg, originating: bool, slot: u8);
    fn msg_flag(&self, m: Msg, f: MsgFlag) -> bool;
    fn msg_get(&self, m: Msg, f: MsgField) -> u32;
    /// a new message (RemovePlayer: player, booted | out of sync << 1 | exiting << 2; CommunicationsTick: tick,
    /// PTs | PTs a second << 8 | checksum << 16, out of sync; ReconnectionRequestResponse: error, slot, tick)
    fn msg_new(&self, kind: NewMsg, a: u32, b: u32, c: u32) -> Msg;
    /// `NEMCommunicationsTickResponse::GetPlayerInput(i, out)`
    fn msg_tick_response_input(&self, m: Msg, i: u32, out: &mut Msg) -> i32;
    /// `NEMCommunicationsTick::AddPlayerInput(input)`
    fn msg_add_input(&self, tick: Msg, input: Msg) -> i32;
    /// `SetSendToAllAndWaitMessageID(id)`
    fn msg_set_s2aw_id(&self, m: Msg, id: u32);
    /// `ExportMessage(&data, size)`: the export (C++ `new[]`) and its size
    fn msg_export_new(&self, m: Msg, size: &mut u32) -> usize;
    /// `delete[]` of an export
    fn bytes_free(&self, data: usize);

    // the C++ class's virtual members (the game's EEServer)
    fn boot_player(&self, slot: u8, was_booted: bool, game_is_exiting: bool);
    fn server_slot28(&self) -> bool;
    fn is_migrating(&self) -> bool;
    fn before_removing_players(&self);
    fn handle_reconnect_wait_timed_out(&self);
    fn player_send_to_slot(&self, player: u8) -> u8;
    fn filter_player_input(&self, m: Msg) -> bool;
    fn prepare_local_input(&self, m: Msg);
    fn is_player_present(&self, player: u8) -> bool;
    fn report_laggers(&self, slots: &[u8], warning: bool);
    fn process_system_message(&self, m: Msg);
    /// the virtual `ProcessLocalUserInput(message, player)` (this class's own unless the game overrides it)
    fn process_local_user_input(&self, m: Msg, player: u8) -> i32;
}

/// A pointer back to the server for the objects it owns (it frees them first)
pub struct Back<H: ServerHost + 'static> {
    srv: *const Server<H>,
}

unsafe impl<H: ServerHost> Send for Back<H> {}
unsafe impl<H: ServerHost> Sync for Back<H> {}

impl<H: ServerHost> Back<H> {
    fn s(&self) -> &Server<H> {
        // SAFETY: the server outlives the objects it owns and is shared by its threads as the original was
        unsafe { &*self.srv }
    }
}

/// The message manager's calls out: the server's fields here, the rest the C++ host's
impl<H: ServerHost> MmCall for Back<H> {
    fn mm_call(&self, op: EeNetMmOp, a: usize, b: usize, c: usize, d: usize) -> isize {
        let s = self.s();
        match op {
            EeNetMmOp::ForceCt => {
                s.host.event_set(s.ev(EV_SEND_FORCE_CT));
                0
            }
            EeNetMmOp::LastCtMessageId => s.last_ct_message_id.load(SeqCst) as isize,
            EeNetMmOp::StartSendToAllAndWait => {
                s.s2aw_number.store(a as u32, SeqCst);
                s.s2aw_type.store(b as u8, SeqCst);
                s.s2aw_timeout.store(c as u32, SeqCst);
                s.s2aw_retries.store(d as u32, SeqCst);
                s.host.event_set(s.ev(EV_WAIT_FOR_RESPONSE));
                0
            }
            EeNetMmOp::Time => s.host.time() as isize,
            // the base class's fields
            EeNetMmOp::LastTick => s.comms.last_tick_received() as isize,
            EeNetMmOp::SetLastTick => {
                s.comms.set(BaseField::LastTickReceived, a as u32);
                0
            }
            EeNetMmOp::CtsProcessed => s.comms.get(BaseField::CtsProcessed) as isize,
            EeNetMmOp::AvgPhysics => s.comms.get(BaseField::AveragePhysicsUpdateTime) as isize,
            _ => s.host.mm(op, a, b, c, d),
        }
    }
}

/// The tuning's calls out: the server's fields, events, message manager and the game
impl<H: ServerHost> TuneHost for Back<H> {
    fn get(&self, f: TuField) -> u32 {
        let s = self.s();
        match f {
            TuField::SendInterval => s.send_interval.load(SeqCst),
            TuField::SendIntervalIncrement => s.send_interval_increment.load(SeqCst),
            TuField::MaxSendInterval => s.max_send_interval.load(SeqCst),
            TuField::PTsPerSecond => s.pts_per_second.load(SeqCst) as u32,
            TuField::MaxPTsPerSecond => s.max_pts_per_second.load(SeqCst),
            TuField::MinPTsPerSecond => s.min_pts_per_second.load(SeqCst),
            TuField::CostFactor => s.cost_factor.load(SeqCst),
            TuField::DefaultSendInterval => s.default_send_interval.load(SeqCst),
            TuField::LastCtMessageId => s.last_ct_message_id.load(SeqCst),
            TuField::GameState => s.game_state.load(SeqCst) as u32,
            TuField::IsMultiplayerSession => s.comms.get(BaseField::IsMultiplayerSession),
            TuField::CurrentCommunicationsTick => s.current_ct.load(SeqCst),
            TuField::PTsPerCt => s.pts_per_ct.load(SeqCst) as u32,
        }
    }
    fn set(&self, f: TuField, value: u32) {
        let s = self.s();
        match f {
            TuField::SendInterval => s.send_interval.store(value, SeqCst),
            TuField::PTsPerSecond => s.pts_per_second.store(value as u8, SeqCst),
            TuField::PTsPerCt => s.pts_per_ct.store(value as u8, SeqCst),
            _ => {}
        }
    }
    fn event_set(&self, which: u32) {
        let s = self.s();
        let e = match which {
            0 => EV_CHANGE_SEND_INTERVAL,
            1 => EV_PAUSE_GAME,
            _ => EV_UNPAUSE_GAME,
        };
        s.host.event_set(s.ev(e));
    }
    fn mm_outstanding(&self, number: u32) -> u8 {
        self.s().mm().outstanding(number)
    }
    fn mm_highest_response_time(&self) -> u32 {
        self.s().mm().highest_response_time()
    }
    fn mm_highest_cpu(&self) -> u16 {
        self.s().mm().highest_cpu()
    }
    fn mm_lowest_ct(&self) -> u32 {
        self.s().mm().lowest_ct()
    }
    fn mm_resend(&self, number: u32) {
        self.s().mm().resend(number);
    }
    fn mm_who(&self, number: u32) -> u8 {
        self.s().mm().who_has_not_responded(number)
    }
    fn mm_who_next(&self, slot: u8, number: u32) -> u8 {
        self.s().mm().who_has_not_responded_next(slot, number)
    }
    fn mm_num_clients(&self) -> u8 {
        self.s().mm().num_clients()
    }
    fn mm_slowest_responder(&self) -> u8 {
        self.s().mm().slowest_responder()
    }
    fn mm_slowest_client_cpu(&self) -> u8 {
        self.s().mm().slowest_client_cpu()
    }
    fn mm_set_force_ct(&self, on: bool) {
        self.s().mm().set_force_ct(on)
    }
    fn unregister_slot(&self, slot: u8) {
        self.s().unregister_slot(slot)
    }
    fn before_removing_players(&self) {
        self.s().host.before_removing_players()
    }
    fn boot_player(&self, slot: u8, was_booted: bool, game_is_exiting: bool) {
        self.s().host.boot_player(slot, was_booted, game_is_exiting)
    }
    fn report_laggers(&self, slots: &[u8], warning: bool) {
        self.s().host.report_laggers(slots, warning)
    }
    fn remove_player_input(&self, slot: u8) {
        let s = self.s();
        let m = s.host.msg_new(NewMsg::RemovePlayer, slot as u32, 1, 0);
        s.host.process_local_user_input(m, SLOT_NONE);
    }
}

/// The recorder's calls out: the server's, except that its callback is the server's method and its thread the
/// server's kind
impl<H: ServerHost> QHost for Back<H> {
    fn time(&self) -> u32 {
        self.s().host.time()
    }
    fn verify(&self, unit: u32, line: u32) {
        self.s().host.verify(unit, line)
    }
    fn fpu_start(&self) {
        self.s().host.fpu_start()
    }
    fn fpu_check(&self) {
        self.s().host.fpu_check()
    }
    fn event_new(&self) -> Ev {
        self.s().host.event_new()
    }
    fn event_free(&self, e: Ev) {
        self.s().host.event_free(e)
    }
    fn event_set(&self, e: Ev) {
        self.s().host.event_set(e)
    }
    fn event_clear(&self, e: Ev) {
        self.s().host.event_clear(e)
    }
    fn event_wait(&self, e: Ev, timeout: u32) -> i32 {
        self.s().host.event_wait(e, timeout)
    }
    fn thread_new(&self, _kind: u32, obj: usize, out: &mut Thr) -> i32 {
        self.s().host.thread_new(THREAD_SV_PLAYBACK, obj, out)
    }
    fn thread_name(&self, t: Thr, name: &str) {
        self.s().host.thread_name(t, name)
    }
    fn thread_free(&self, t: Thr) {
        self.s().host.thread_free(t)
    }
    fn msg_reference(&self, m: Msg) {
        self.s().host.msg_reference(m)
    }
    fn msg_dereference(&self, m: Msg) {
        self.s().host.msg_dereference(m)
    }
    fn msg_export_size(&self, m: Msg) -> u32 {
        self.s().host.msg_export_size(m)
    }
    fn msg_export(&self, m: Msg, buf: &mut [u8]) -> u32 {
        self.s().host.msg_export(m, buf)
    }
    fn msg_from_bytes(&self, data: Option<&[u8]>) -> Msg {
        self.s().host.msg_from_bytes(data)
    }
    fn deliver(&self, _kind: u32, m: Msg) -> i32 {
        self.s().record_playback_callback(m);
        0
    }
    fn file_create(&self, f: FileH) -> i32 {
        self.s().host.file_create(f)
    }
    fn file_delete(&self, f: FileH) -> i32 {
        self.s().host.file_delete(f)
    }
    fn file_open(&self, f: FileH, access: u32) -> i32 {
        self.s().host.file_open(f, access)
    }
    fn file_close(&self, f: FileH) {
        self.s().host.file_close(f)
    }
    fn file_write(&self, f: FileH, data: &[u8]) -> i32 {
        self.s().host.file_write(f, data)
    }
    fn file_write_u32(&self, f: FileH, value: u32) -> i32 {
        self.s().host.file_write_u32(f, value)
    }
    fn file_read_u32(&self, f: FileH, value: &mut u32) -> i32 {
        self.s().host.file_read_u32(f, value)
    }
    fn file_read(&self, f: FileH, buf: &mut [u8]) -> i32 {
        self.s().host.file_read(f, buf)
    }
}

// the events, by the original's member order
const EV_CHANGE_SEND_INTERVAL: usize = 0;
const EV_INPUT_AVAILABLE: usize = 1;
const EV_XFER_INPUT_AVAILABLE: usize = 2;
const EV_XFER_ACCEPT: usize = 3;
const EV_XFER_STUFF_WAITING: usize = 4;
const EV_XFER_STARTED: usize = 5;
const EV_XFER_EXIT: usize = 6;
const EV_XFER_FINISHED: usize = 7;
const EV_MAIN_EXIT: usize = 8;
const EV_MAIN_STARTED: usize = 9;
const EV_MAIN_FINISHED: usize = 10;
const EV_PAUSE_GAME: usize = 11;
const EV_UNPAUSE_GAME: usize = 12;
const EV_RESUME_MIGRATED: usize = 13;
const EV_SEND_EXIT: usize = 14;
const EV_SEND_STARTED: usize = 15;
const EV_SEND_FINISHED: usize = 16;
const EV_SEND_FORCE_CT: usize = 17;
const EV_START_GAME: usize = 18;
const EV_STOP_GAME: usize = 19;
const EV_WAIT_FOR_RESPONSE: usize = 20;
const EV_WAIT_FOR_RECONNECTS: usize = 21;
const EV_START_PING: usize = 22;
const EV_STOP_PING: usize = 23;
const EVENTS: usize = 24;

/// The constructor's events before Publish, with their ESVERIFY lines
const EVENTS_BEFORE_PUBLISH: [(usize, u32); 4] =
    [(EV_INPUT_AVAILABLE, 231), (EV_XFER_INPUT_AVAILABLE, 234), (EV_XFER_ACCEPT, 237), (EV_XFER_STUFF_WAITING, 240)];
/// ... and after it
const EVENTS_AFTER_PUBLISH: [(usize, u32); 20] = [
    (EV_CHANGE_SEND_INTERVAL, 255),
    (EV_MAIN_STARTED, 256),
    (EV_MAIN_EXIT, 257),
    (EV_MAIN_FINISHED, 258),
    (EV_SEND_STARTED, 259),
    (EV_SEND_EXIT, 260),
    (EV_SEND_FINISHED, 261),
    (EV_SEND_FORCE_CT, 262),
    (EV_START_GAME, 263),
    (EV_PAUSE_GAME, 264),
    (EV_UNPAUSE_GAME, 265),
    (EV_RESUME_MIGRATED, 266),
    (EV_STOP_GAME, 267),
    (EV_WAIT_FOR_RESPONSE, 268),
    (EV_WAIT_FOR_RECONNECTS, 269),
    (EV_START_PING, 270),
    (EV_STOP_PING, 271),
    (EV_XFER_STARTED, 272),
    (EV_XFER_EXIT, 273),
    (EV_XFER_FINISHED, 274),
];
/// The destructor's order (a published server's; then the four of before Publish)
const EVENTS_FREED: [usize; 18] = [
    EV_CHANGE_SEND_INTERVAL,
    EV_MAIN_STARTED,
    EV_MAIN_EXIT,
    EV_MAIN_FINISHED,
    EV_RESUME_MIGRATED,
    EV_SEND_STARTED,
    EV_SEND_EXIT,
    EV_SEND_FINISHED,
    EV_SEND_FORCE_CT,
    EV_START_GAME,
    EV_PAUSE_GAME,
    EV_UNPAUSE_GAME,
    EV_STOP_GAME,
    EV_WAIT_FOR_RESPONSE,
    EV_WAIT_FOR_RECONNECTS,
    EV_XFER_STARTED,
    EV_XFER_EXIT,
    EV_XFER_FINISHED,
];

/// The constructor's arguments
#[derive(Clone, Copy, Debug)]
pub struct ServerArgs {
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
}

/// The checksums of each tick: (checksum, slot) in a `std::multimap`'s order (by checksum, equal ones as they came)
#[derive(Default)]
struct Checks {
    responses: BTreeMap<u32, Vec<(u32, u8)>>,
    oos_slots: Vec<u8>,
}

impl Checks {
    fn insert(&mut self, tick: u32, checksum: u32, slot: u8) {
        let list = self.responses.entry(tick).or_default();
        let at = list.partition_point(|e| e.0 <= checksum);
        list.insert(at, (checksum, slot));
    }
}

/// A queued send-to-all-and-wait: the message (referenced), its answers' timeout and retries
#[derive(Clone, Copy)]
struct S2aw {
    m: Msg,
    timeout: u32,
    retries: u32,
}

pub struct Server<H: ServerHost + 'static> {
    pub host: H,
    /// the base class's fields (NESynchronousCommunications, shared with the C++ base)
    pub comms: Arc<Comms>,
    mm: UnsafeCell<Option<MsgMgr<MmVia<Back<H>>>>>,
    tune: UnsafeCell<Option<Tune<Back<H>>>>,
    rec: UnsafeCell<Option<RecPlay<Back<H>>>>,
    /// mChecksumResponses and mOOSSlots under mChecksumResponsesLock
    checks: Mutex<Checks>,
    is_oos: AtomicBool,
    inputs_lock: RLock,
    inputs: UnsafeCell<VecDeque<Msg>>,
    s2aw_lock: RLock,
    s2aw: UnsafeCell<Vec<S2aw>>,
    pause_lock: RLock,
    sct_lock: RLock,
    send_to_slot: Mutex<BTreeMap<u8, u8>>,
    protocol: AtomicUsize,
    server: AtomicUsize,
    main_thread: AtomicUsize,
    send_thread: AtomicUsize,
    xfer_thread: AtomicUsize,
    events: [AtomicUsize; EVENTS],
    cost_factor: AtomicU32,
    game_state: AtomicU8,
    current_physics_tick: AtomicU32,
    checksum_interval: AtomicU32,
    next_checksum_ct: AtomicU32,
    restart_ct: AtomicU32,
    application_start_time: AtomicU32,
    current_ct: AtomicU32,
    send_interval: AtomicU32,
    default_send_interval: AtomicU32,
    last_ct_message_id: AtomicU32,
    last_ping_time: AtomicU32,
    s2aw_number: AtomicU32,
    s2aw_timeout: AtomicU32,
    s2aw_retries: AtomicU32,
    max_pts_per_second: AtomicU32,
    min_pts_per_second: AtomicU32,
    wait_for_reconnects_time: AtomicU32,
    max_send_interval: AtomicU32,
    send_interval_increment: AtomicU32,
    reconnect_count: AtomicU32,
    lowest_reconnect_ct: AtomicU32,
    s2aw_type: AtomicU8,
    pts_per_ct: AtomicU8,
    pts_per_second: AtomicU8,
    max_players: AtomicU8,
    number_of_players: AtomicU8,
    temp_oos_pts: AtomicU8,
    game_was_published: AtomicBool,
    ping: AtomicBool,
    processing_s2aw: AtomicBool,
}

// The queues are reached only under their locks, never across a call out; the owned objects are set once by the
// constructor and only read after.
unsafe impl<H: ServerHost + Sync> Sync for Server<H> {}

impl<H: ServerHost> Server<H> {
    /// The constructor: protocol 0, the fields, the message manager with the server's own slot, the tuning, four
    /// events and Publish ("EEService"); published, the threads' events and the recorder passing.
    pub fn new(host: H, comms: Arc<Comms>, a: ServerArgs) -> Box<Self> {
        let s = Box::new(Server {
            host,
            comms,
            mm: UnsafeCell::new(None),
            tune: UnsafeCell::new(None),
            rec: UnsafeCell::new(None),
            checks: Mutex::new(Checks::default()),
            is_oos: AtomicBool::new(false),
            inputs_lock: RLock::new(),
            inputs: UnsafeCell::new(VecDeque::new()),
            s2aw_lock: RLock::new(),
            s2aw: UnsafeCell::new(Vec::new()),
            pause_lock: RLock::new(),
            sct_lock: RLock::new(),
            send_to_slot: Mutex::new(BTreeMap::new()),
            protocol: AtomicUsize::new(0),
            server: AtomicUsize::new(0),
            main_thread: AtomicUsize::new(0),
            send_thread: AtomicUsize::new(0),
            xfer_thread: AtomicUsize::new(0),
            events: std::array::from_fn(|_| AtomicUsize::new(0)),
            cost_factor: AtomicU32::new(a.cost_factor.to_bits()),
            game_state: AtomicU8::new(STATE_OPEN),
            current_physics_tick: AtomicU32::new(a.current_physics_tick),
            checksum_interval: AtomicU32::new(1),
            next_checksum_ct: AtomicU32::new(40),
            restart_ct: AtomicU32::new(0),
            application_start_time: AtomicU32::new(0),
            current_ct: AtomicU32::new(a.current_communications_tick),
            send_interval: AtomicU32::new(WAIT_INFINITE),
            default_send_interval: AtomicU32::new(a.default_send_interval),
            last_ct_message_id: AtomicU32::new(0xffff_ffff),
            last_ping_time: AtomicU32::new(0),
            s2aw_number: AtomicU32::new(0),
            s2aw_timeout: AtomicU32::new(3000),
            s2aw_retries: AtomicU32::new(40),
            max_pts_per_second: AtomicU32::new(a.max_pts_per_second),
            min_pts_per_second: AtomicU32::new(a.min_pts_per_second),
            wait_for_reconnects_time: AtomicU32::new(0),
            max_send_interval: AtomicU32::new(a.max_send_interval),
            send_interval_increment: AtomicU32::new(a.send_interval_increment),
            reconnect_count: AtomicU32::new(0),
            lowest_reconnect_ct: AtomicU32::new(0xffff_ffff),
            s2aw_type: AtomicU8::new(0),
            pts_per_ct: AtomicU8::new(a.pts_per_ct),
            // (1000 / interval * PTs, in unsigned long, kept as a byte; an interval of 0 crashed the original)
            pts_per_second: AtomicU8::new(
                1000u32.checked_div(a.default_send_interval).unwrap_or(0).wrapping_mul(a.pts_per_ct as u32) as u8,
            ),
            max_players: AtomicU8::new(a.max_players),
            number_of_players: AtomicU8::new(0),
            temp_oos_pts: AtomicU8::new(0),
            game_was_published: AtomicBool::new(false),
            ping: AtomicBool::new(false),
            processing_s2aw: AtomicBool::new(false),
        });
        s.verify(s.host.protocol_count() > 0, 138);
        let mut protocol = 0;
        let r = s.host.protocol_get(&mut protocol);
        s.protocol.store(protocol, SeqCst);
        s.verify(r == ERROR_OK, 140);
        s.verify((a.max_players as u32) < 0xff + 1, 149);
        let me: *const Server<H> = &*s;
        let mm = MsgMgr::new(MmVia(Back { srv: me }));
        // SAFETY: set once, before any reader
        unsafe { *s.mm.get() = Some(mm) };
        s.mm().register_slot(SLOT_SERVER);
        s.application_start_time.store(s.host.time(), SeqCst);
        let tune = Tune::new(Back { srv: me }, a.send_interval_decrement);
        unsafe { *s.tune.get() = Some(tune) };
        for (e, line) in EVENTS_BEFORE_PUBLISH {
            s.new_event(e, line);
        }
        let mut server = 0;
        let events = [s.ev(EV_INPUT_AVAILABLE), s.ev(EV_XFER_INPUT_AVAILABLE), s.ev(EV_XFER_ACCEPT)];
        let r = s.host.publish(protocol, &events, &mut server, a.allow_new_connections);
        s.server.store(server, SeqCst);
        if r != ERROR_OK {
            return s;
        }
        s.game_was_published.store(true, SeqCst);
        for (e, line) in EVENTS_AFTER_PUBLISH {
            s.new_event(e, line);
        }
        let rec = RecPlay::new(Back { srv: me });
        unsafe { *s.rec.get() = Some(rec) };
        s.rec().unwrap().pass_mode();
        s
    }

    fn new_event(&self, e: usize, line: u32) {
        let ev = self.host.event_new();
        self.events[e].store(ev, SeqCst);
        self.verify(ev != 0, line);
    }

    /// The destructor's work: a published server's recorder, events, threads and server; the four events of before
    /// Publish and the message manager (its transfers' files released)
    pub fn release(&self) {
        let published = self.game_was_published.load(SeqCst);
        if published {
            if let Some(r) = self.rec() {
                r.release();
            }
            for e in EVENTS_FREED {
                self.host.event_free(self.ev(e));
            }
            self.host.thread_free(self.send_thread.load(SeqCst));
            self.host.thread_free(self.main_thread.load(SeqCst));
            self.host.thread_free(self.xfer_thread.load(SeqCst));
        }
        for e in [EV_INPUT_AVAILABLE, EV_XFER_INPUT_AVAILABLE, EV_XFER_ACCEPT, EV_XFER_STUFF_WAITING] {
            self.host.event_free(self.ev(e));
        }
        self.mm().release();
        if published {
            self.host.server_close(self.server.load(SeqCst));
        }
    }

    pub fn mm(&self) -> &MsgMgr<MmVia<Back<H>>> {
        // SAFETY: set by the constructor
        unsafe { (*self.mm.get()).as_ref().unwrap() }
    }
    pub fn tune(&self) -> &Tune<Back<H>> {
        unsafe { (*self.tune.get()).as_ref().unwrap() }
    }
    /// The recorder (none when the server was not published)
    pub fn rec(&self) -> Option<&RecPlay<Back<H>>> {
        unsafe { (*self.rec.get()).as_ref() }
    }
    fn ev(&self, i: usize) -> Ev {
        self.events[i].load(SeqCst)
    }
    fn verify(&self, ok: bool, line: u32) {
        if !ok {
            self.host.verify(UNIT, line);
        }
    }
    fn multiplayer(&self) -> bool {
        self.comms.get(BaseField::IsMultiplayerSession) != 0
    }
    /// The recorder's Process; a server whose Publish failed has none and passes nothing on (it crashed before)
    fn record(&self, m: Msg) {
        if let Some(r) = self.rec() {
            r.process(m);
        }
    }
    fn with_inputs<R>(&self, f: impl FnOnce(&mut VecDeque<Msg>) -> R) -> R {
        // SAFETY: callers hold inputs_lock; `f` makes no call out
        f(unsafe { &mut *self.inputs.get() })
    }
    fn with_s2aw<R>(&self, f: impl FnOnce(&mut Vec<S2aw>) -> R) -> R {
        // SAFETY: callers hold s2aw_lock; `f` makes no call out
        f(unsafe { &mut *self.s2aw.get() })
    }
    fn checks(&self) -> std::sync::MutexGuard<'_, Checks> {
        self.checks.lock().unwrap_or_else(|e| e.into_inner())
    }
    fn slots(&self) -> std::sync::MutexGuard<'_, BTreeMap<u8, u8>> {
        self.send_to_slot.lock().unwrap_or_else(|e| e.into_inner())
    }
    /// A message to all, guaranteed, then given back (the original's `new X; SetOriginatingSlot(server); ...`)
    fn new_server_message(&self, kind: NewMsg, a: u32) -> Msg {
        let m = self.host.msg_new(kind, a, 0, 0);
        if m != 0 {
            self.host.msg_set_slot(m, true, SLOT_SERVER);
        }
        m
    }

    // ---- the accessors (the header's inline members)
    pub fn field(&self, f: SvGet) -> u32 {
        match f {
            SvGet::CurrentCommunicationsTick => self.current_ct.load(SeqCst),
            SvGet::CurrentPhysicsTick => self.current_physics_tick.load(SeqCst),
            SvGet::GameState => self.game_state.load(SeqCst) as u32,
            SvGet::GameWasPublished => self.game_was_published.load(SeqCst) as u32,
            SvGet::LastCtMessageId => self.last_ct_message_id.load(SeqCst),
            SvGet::MaxPlayers => self.max_players.load(SeqCst) as u32,
            SvGet::NumberOfPlayers => self.number_of_players.load(SeqCst) as u32,
            SvGet::ProcessingSendToAllAndWait => self.processing_s2aw.load(SeqCst) as u32,
            SvGet::PTsPerCt => self.pts_per_ct.load(SeqCst) as u32,
            SvGet::RestartCt => self.restart_ct.load(SeqCst),
            SvGet::SendInterval => self.send_interval.load(SeqCst),
            SvGet::IsOos => self.is_oos.load(SeqCst) as u32,
            SvGet::PTsPerSecond => self.pts_per_second.load(SeqCst) as u32,
            SvGet::NextChecksumCt => self.next_checksum_ct.load(SeqCst),
            SvGet::ReconnectCount => self.reconnect_count.load(SeqCst),
            SvGet::LowestReconnectCt => self.lowest_reconnect_ct.load(SeqCst),
            SvGet::WaitForReconnectsTime => self.wait_for_reconnects_time.load(SeqCst),
            SvGet::Ping => self.ping.load(SeqCst) as u32,
            SvGet::Protocol => self.protocol.load(SeqCst) as u32,
            SvGet::TempOutOfSynchPTs => self.temp_oos_pts.load(SeqCst) as u32,
        }
    }
    pub fn protocol(&self) -> usize {
        self.protocol.load(SeqCst)
    }
    pub fn server(&self) -> usize {
        self.server.load(SeqCst)
    }
    pub fn set_field(&self, f: SvSet, v: u32) {
        match f {
            SvSet::CurrentCommunicationsTick => self.current_ct.store(v, SeqCst),
            SvSet::CurrentPhysicsTick => self.current_physics_tick.store(v, SeqCst),
            SvSet::GameState => self.game_state.store(v as u8, SeqCst),
            SvSet::LastCtMessageId => self.last_ct_message_id.store(v, SeqCst),
            SvSet::RestartCt => self.restart_ct.store(v, SeqCst),
            SvSet::TempOutOfSynchPTs => self.temp_oos_pts.store(v as u8, SeqCst),
            SvSet::Ping => self.ping.store(v != 0, SeqCst),
        }
    }
    /// AllowNewPlayers: open (or closed) to new players, and the protocol's server told
    pub fn allow_new_players(&self, allow: bool) {
        if allow {
            self.game_state.store(STATE_OPEN, SeqCst);
        }
        if !allow {
            self.game_state.store(STATE_CLOSED, SeqCst);
        }
        self.host.allow_new_connections(self.protocol(), self.server(), allow);
    }
    pub fn clear_send_to_slot_array(&self) {
        self.slots().clear();
    }
    /// SetSendToSlotForPlayer: a player's slot, unless it has one (std::map::insert)
    pub fn set_send_to_slot_for_player(&self, player: u8, slot: u8) {
        self.slots().entry(player).or_insert(slot);
    }
    pub fn send_to_slot_for_player(&self, player: u8) -> u8 {
        self.slots().get(&player).copied().unwrap_or(0xff)
    }
    pub fn set_send_interval(&self, interval: u32) {
        self.send_interval.store(interval, SeqCst);
        self.host.event_set(self.ev(EV_CHANGE_SEND_INTERVAL));
    }
    /// The events the inline members set: StartGame, StopGame, ResumeMigratedGame, ForceNextCT
    pub fn set_event(&self, which: SvEvent) {
        let e = match which {
            SvEvent::StartGame => EV_START_GAME,
            SvEvent::StopGame => EV_STOP_GAME,
            SvEvent::ResumeMigratedGame => EV_RESUME_MIGRATED,
            SvEvent::ForceNextCt => EV_SEND_FORCE_CT,
        };
        self.host.event_set(self.ev(e));
    }
    pub fn start_waiting_for_reconnects(&self, time: u32) {
        self.wait_for_reconnects_time.store(time, SeqCst);
        self.host.event_set(self.ev(EV_WAIT_FOR_RECONNECTS));
    }
    /// SendExitMessage: the send thread, then the main thread told to end and waited for
    pub fn send_exit_message(&self) {
        self.host.event_set(self.ev(EV_SEND_EXIT));
        self.host.event_wait(self.ev(EV_SEND_FINISHED), WAIT_INFINITE);
        self.host.event_set(self.ev(EV_MAIN_EXIT));
        self.host.event_wait(self.ev(EV_MAIN_FINISHED), WAIT_INFINITE);
    }
    pub fn lock_send_communications_tick(&self) {
        self.sct_lock.enter();
    }
    pub fn unlock_send_communications_tick(&self) {
        self.sct_lock.leave();
    }
    /// SimGameRunning (TEST ONLY): running or paused
    pub fn sim_game_running(&self) -> bool {
        let g = self.game_state.load(SeqCst);
        g == STATE_RUNNING || g == STATE_PAUSED
    }
    pub fn record_playback_mode(&self) -> u8 {
        self.rec().map_or(crate::recplay::MODE_PASSING, |r| r.mode())
    }

    // ---- the members

    pub fn abort_xfer(&self, id: u32) {
        self.mm().abort_xfer(id)
    }
    pub fn boot_all_laggers(&self, number: u32) {
        self.tune().boot_all_laggers(number)
    }

    /// BroadcastMessage: the message to every present player's slot; the first send error stops it and is answered
    pub fn broadcast_message(&self, m: Msg) -> i32 {
        let mut size = 0;
        let data = self.host.msg_export_new(m, &mut size);
        let mut result = ERROR_OK;
        let mut i: u8 = 0;
        while i < self.max_players.load(SeqCst) {
            if self.host.is_player_present(i) {
                let slot = self.host.player_send_to_slot(i);
                if slot != SLOT_NONE {
                    result = self.host.server_send_to(self.server(), slot, data, size);
                    if result != ERROR_OK {
                        break;
                    }
                }
            }
            i = i.wrapping_add(1);
        }
        self.host.bytes_free(data);
        result
    }

    pub fn is_oos(&self) -> bool {
        self.is_oos.load(SeqCst)
    }

    /// LogChecksumResponse: collects a tick's checksums; once as many have come as there are humans, a tick whose
    /// checksums differ names its out-of-sync slots (two humans: every client; more: the slots off the most common
    /// checksum, the server's winning a tie, the server's slot first) and the server is out of sync. The ticks before
    /// it are forgotten (the tick itself is kept, as the original's erase left it).
    pub fn log_checksum_response(&self, tick: u32, checksum: u32, slot: u8) {
        let num_players = self.comms.get(BaseField::NumHumanPlayers) as u8;
        let mut c = self.checks();
        if tick == 0 || num_players <= 1 {
            return;
        }
        let known = c.responses.contains_key(&tick);
        c.insert(tick, checksum, slot);
        if !known {
            return;
        }
        let list = c.responses[&tick].clone();
        if list.len() != num_players as usize {
            return;
        }
        let mut counts: BTreeMap<u32, u32> = BTreeMap::new();
        for e in &list {
            *counts.entry(e.0).or_insert(0) += 1;
        }
        if counts.len() != 1 {
            // (the field read again under the lock, as the original did; a getter, no call back)
            if self.comms.get(BaseField::NumHumanPlayers) as u8 == 2 {
                for e in &list {
                    if e.1 != SLOT_SERVER {
                        c.oos_slots.push(e.1);
                    }
                }
            } else {
                let servers = list.iter().find(|e| e.1 == SLOT_SERVER).map_or(0, |e| e.0);
                let mut highest = 0u32;
                let mut most_common = 0u32;
                for (&k, &n) in &counts {
                    if n > highest || (n == highest && k == servers) {
                        most_common = k;
                        highest = n;
                    }
                }
                for e in &list {
                    if e.0 != most_common {
                        c.oos_slots.push(e.1);
                    }
                }
                if c.oos_slots.len() > 1 {
                    // std::sort with "the server's slot before any other" over a few slots: an insertion sort, so
                    // the others keep their order
                    c.oos_slots.sort_by_key(|&s| s != SLOT_SERVER);
                }
            }
            self.is_oos.store(true, SeqCst);
        }
        c.responses.retain(|&t, _| t >= tick);
    }

    pub fn log_response(&self, m: Msg) {
        self.mm().log_response(m)
    }

    /// PauseGame and UnpauseGame: the request queued to every client, its answers awaited
    pub fn pause_game(&self, pause: bool) {
        let _l = self.pause_lock.lock();
        let kind = if pause { NewMsg::PauseGame } else { NewMsg::UnPauseGame };
        let m = self.new_server_message(kind, self.current_ct.load(SeqCst));
        if m != 0 {
            self.send_to_all_and_wait(m, 3000, 40);
            self.host.msg_dereference(m);
        }
    }

    /// ProcessCommunicationsTickResponse: a client's inputs replayed, its response time and answer logged
    fn process_communications_tick_response(&self, m: Msg) {
        let mut i = 0;
        while i < self.host.msg_get(m, MsgField::NumberOfPlayerInputs) {
            let mut input = 0;
            if self.host.msg_tick_response_input(m, i, &mut input) == ERROR_OK {
                self.host.msg_reference(input);
                self.record(input);
            }
            i += 1;
        }
        let slot = self.msg_slot(m);
        let ct = self.host.msg_get(m, MsgField::LastCtProcessed);
        let cpu = self.host.msg_get(m, MsgField::AveragePhysicsTickTime);
        self.mm().log_response_time(slot, ct, cpu as u16);
        self.mm().log_response(m);
        self.host.msg_dereference(m);
    }

    /// ProcessLocalUserInput: the server's player's input, unless paused in a multiplayer game (or not to be sent), to
    /// the recorder
    pub fn process_local_user_input(&self, m: Msg, player: u8) -> i32 {
        let send = (!self.multiplayer()
            || self.comms.get(BaseField::IsGamePaused) == 0
            || self.host.msg_flag(m, MsgFlag::CanSendWhilePaused))
            && self.host.msg_flag(m, MsgFlag::IsAvailableToSend);
        if send {
            self.host.msg_set_slot(m, true, SLOT_SERVER);
            self.host.msg_set_slot(m, false, player);
            self.host.prepare_local_input(m);
            self.record(m);
        } else {
            self.host.msg_dereference(m);
        }
        ERROR_OK
    }

    /// ProcessLocalUserInput2 (the recorder's callback for the server's own input): queued for the next tick, unless
    /// migrating (but the two types a migration sends) or paused in a multiplayer game
    fn process_local_user_input2(&self, m: Msg) {
        if (self.host.is_migrating() && self.host.msg_type(m) != 0x7c && self.host.msg_type(m) != 0x91)
            || (self.game_state.load(SeqCst) == STATE_PAUSED
                && self.multiplayer()
                && !self.host.msg_flag(m, MsgFlag::IsAvailableToSend))
        {
            self.host.msg_dereference(m);
        } else {
            self.host.msg_set_slot(m, true, SLOT_SERVER);
            self.queue_input(m);
        }
    }

    fn queue_input(&self, m: Msg) {
        if self.host.filter_player_input(m) {
            let _l = self.inputs_lock.lock();
            self.with_inputs(|q| q.push_back(m));
        }
    }

    /// ProcessPendingPlayerInputs: one client packet read, its message to the recorder with the client's slot
    fn process_pending_player_inputs(&self) {
        let mut buf = [0u8; PACKET_SIZE];
        let mut size = 0;
        let mut index = 0;
        if self.host.server_receive_from(self.server(), &mut buf, &mut size, &mut index) != ERROR_OK {
            return;
        }
        let m = self.host.msg_from_bytes(Some(&buf));
        if m != 0 {
            self.host.msg_set_slot(m, true, index as u8);
            self.record(m);
        }
    }

    /// ProcessPendingPlayerInputs2 (the recorder's callback for a client's message): connection and reconnection
    /// requests, tick responses, checksums and pause answers handled; any other input queued
    fn process_pending_player_inputs2(&self, m: Msg) {
        self.verify(m != 0, 958);
        match self.host.msg_type(m) {
            MSG_ADD_PLAYER_RESPONSE | MSG_START_GAME_RESPONSE | MSG_STOP_GAME_RESPONSE => {
                self.host.msg_dereference(m);
            }
            MSG_CLIENT_CONNECTION_REQUEST => {
                if self.game_state.load(SeqCst) == STATE_OPEN {
                    self.mm().register_slot(self.msg_slot(m));
                    self.host.process_system_message(m);
                }
            }
            MSG_CLIENT_DISCONNECTION_NOTIFICATION => self.host.process_system_message(m),
            MSG_COMMUNICATIONS_TICK_RESPONSE => self.process_communications_tick_response(m),
            MSG_PLAYER_CONNECTION_CHECK_RESPONSE => {
                self.host.process_system_message(m);
                self.host.msg_dereference(m);
            }
            MSG_WORLD_DATABASE_CHECKSUM_RESPONSE => {
                let tick = self.host.msg_get(m, MsgField::ChecksumTick);
                let checksum = self.host.msg_get(m, MsgField::Checksum);
                self.log_checksum_response(tick, checksum, self.msg_slot(m));
                self.host.msg_dereference(m);
            }
            MSG_PAUSE_GAME_RESPONSE | MSG_UNPAUSE_GAME_RESPONSE => {
                self.mm().log_response(m);
                self.host.msg_dereference(m);
            }
            MSG_CLIENT_RECONNECTION_REQUEST => {
                let _l = self.sct_lock.lock();
                if self.reconnect_count.load(SeqCst) < self.wait_for_reconnects_time.load(SeqCst)
                    && self.game_state.load(SeqCst) == STATE_OPEN
                {
                    self.mm().register_slot(self.msg_slot(m));
                    if self.host.msg_get(m, MsgField::ReconnectionCt) < self.lowest_reconnect_ct.load(SeqCst) {
                        let ct = self.host.msg_get(m, MsgField::ReconnectionCt);
                        self.lowest_reconnect_ct.store(ct, SeqCst);
                        self.restart_ct.store(ct, SeqCst);
                    }
                    let slot = self.msg_slot(m);
                    let response = self.host.msg_new(
                        NewMsg::ReconnectionRequestResponse,
                        0,
                        self.msg_slot(m) as u32,
                        self.current_ct.load(SeqCst),
                    );
                    self.mm().send_to(response, slot, true);
                    self.host.process_system_message(m);
                    self.host.msg_dereference(m);
                    let count = self.reconnect_count.fetch_add(1, SeqCst).wrapping_add(1);
                    if count == self.wait_for_reconnects_time.load(SeqCst) {
                        self.comms.set(BaseField::NumHumanPlayers, (count as u8).wrapping_add(1) as u32);
                        self.host.event_set(self.ev(EV_RESUME_MIGRATED));
                    }
                } else {
                    self.host.msg_dereference(m);
                }
            }
            _ => self.queue_input(m),
        }
    }

    fn msg_slot(&self, m: Msg) -> u8 {
        self.host.msg_get(m, MsgField::OriginatingSlot) as u8
    }

    /// RecordPlaybackCallback: the server's own messages to ProcessLocalUserInput2, the clients' to
    /// ProcessPendingPlayerInputs2
    fn record_playback_callback(&self, m: Msg) {
        if self.msg_slot(m) == SLOT_SERVER {
            self.process_local_user_input2(m);
        } else {
            self.process_pending_player_inputs2(m);
        }
    }

    pub fn register_slot(&self, slot: u8) {
        self.mm().register_slot(slot);
    }

    /// RemovePlayerFromGameInProgress: the player whose send slot is `slot` (the server's slot: its own player 0xfe)
    /// is announced removed; a booted client's slot is unregistered and counted out of the humans
    pub fn remove_player_from_game_in_progress(&self, slot: u8, was_booted: bool, is_oos: bool, game_is_exiting: bool) {
        let mut player: u8 = SLOT_SERVER;
        let mut found = true;
        if slot != SLOT_SERVER {
            found = false;
            player = 1;
            while player < self.max_players.load(SeqCst) {
                self.host.player_send_to_slot(player);
                if !self.host.is_player_present(player) || self.host.player_send_to_slot(player) != slot {
                    player = player.wrapping_add(1);
                    continue;
                }
                if player != SLOT_SERVER && was_booted {
                    self.unregister_slot(slot);
                    let n = self.comms.get(BaseField::NumHumanPlayers) as u8;
                    self.comms.set(BaseField::NumHumanPlayers, n.wrapping_sub(1) as u32);
                }
                found = true;
                break;
            }
        }
        if !found {
            return;
        }
        let flags = was_booted as u32 | (is_oos as u32) << 1 | (game_is_exiting as u32) << 2;
        let m = self.host.msg_new(NewMsg::RemovePlayer, player as u32, flags, 0);
        if m != 0 {
            self.host.process_local_user_input(m, SLOT_NONE);
        }
    }

    pub fn search_for_game(&self, start: bool) -> i32 {
        self.host.search_for_game(self.protocol(), start)
    }

    /// SendCommunicationsTick: the next tick to every client with the inputs queued since the last (a keep-alive
    /// instead while the tuning holds the game); the first queued send-to-all-and-wait goes out first and holds the
    /// physics until its answers come; out of sync, the out-of-sync players are removed after the tick
    pub fn send_communications_tick(&self, generate_checksum: bool, tune: bool) {
        let _t = self.sct_lock.lock();
        let _s = self.s2aw_lock.lock();
        if !self.is_oos() && tune && !self.tune().tune_server() {
            let m = self.new_server_message(NewMsg::ServerIsAlive, 0);
            self.mm().send(m, false);
            self.host.msg_dereference(m);
            return;
        }
        if !self.processing_s2aw.load(SeqCst) {
            if let Some(r) = self.with_s2aw(|q| q.first().copied()) {
                self.host.msg_set_s2aw_id(r.m, self.current_ct.load(SeqCst));
                self.mm().send_to_all_and_wait(r.m, r.timeout, r.retries);
                self.host.msg_dereference(r.m);
                self.with_s2aw(|q| q.remove(0));
                self.processing_s2aw.store(true, SeqCst);
            }
        }
        let pts: u8;
        if self.game_state.load(SeqCst) != STATE_RUNNING {
            pts = if self.game_state.load(SeqCst) == STATE_PAUSED || self.processing_s2aw.load(SeqCst) {
                0
            } else {
                0xff
            };
            if self.ping.load(SeqCst) && self.host.time().wrapping_sub(self.last_ping_time.load(SeqCst)) > 3000 {
                let now = self.host.time();
                self.last_ping_time.store(now, SeqCst);
                let m = self.host.msg_new(NewMsg::PlayerConnectionCheck, now, 0, 0);
                self.host.process_local_user_input(m, SLOT_NONE);
            }
        } else if !self.is_oos()
            && !self.processing_s2aw.load(SeqCst)
            && (!self.host.server_slot28() || !self.multiplayer())
        {
            pts = self.pts_per_ct.load(SeqCst);
            self.current_physics_tick.fetch_add(pts as u32, SeqCst);
        } else {
            pts = 0;
        }
        let checksum = ((generate_checksum && pts > 0) || self.is_oos()) && self.multiplayer();
        let flags = pts as u32 | (self.pts_per_second.load(SeqCst) as u32) << 8 | (checksum as u32) << 16;
        let tick =
            self.host.msg_new(NewMsg::CommunicationsTick, self.current_ct.load(SeqCst), flags, self.is_oos() as u32);
        if tick == 0 {
            return;
        }
        let mut size = self.host.msg_export_size(tick);
        if !self.processing_s2aw.load(SeqCst) {
            let _i = self.inputs_lock.lock();
            while let Some(input) = self.with_inputs(|q| q.front().copied()) {
                let input_size = self.host.msg_export_size(input);
                if input_size.wrapping_add(size).wrapping_add(0x1c) >= TICK_PACKET {
                    break;
                }
                self.with_inputs(|q| q.pop_front());
                self.host.msg_add_input(tick, input);
                size = size.wrapping_add(self.host.msg_export_size(input));
            }
        }
        self.host.msg_set_slot(tick, true, SLOT_SERVER);
        let id = self.mm().send(tick, true);
        self.last_ct_message_id.store(id, SeqCst);
        self.current_ct.fetch_add(1, SeqCst);
        self.host.msg_dereference(tick);
        self.remove_oos_players();
    }

    /// SendCommunicationsTick's out-of-sync block (after the tick went): the game is told players go, each
    /// out-of-sync slot's player is removed as booted, and the server is in sync again
    pub fn remove_oos_players(&self) {
        if !self.is_oos() {
            return;
        }
        self.host.before_removing_players();
        let mut i = 0;
        loop {
            let slot = {
                let c = self.checks();
                if i >= c.oos_slots.len() {
                    break;
                }
                c.oos_slots[i]
            };
            self.remove_player_from_game_in_progress(slot, true, true, false);
            i += 1;
        }
        self.is_oos.store(false, SeqCst);
        self.checks().oos_slots.clear();
    }

    /// SimCommunicationsTick (TEST ONLY, platform/PlatSim.h): the headless simulation's next tick, sent by its physics
    /// thread: the configured maximum rate and the default interval's PTs per tick, never tuned
    pub fn sim_communications_tick(&self) {
        {
            let _t = self.sct_lock.lock();
            let rate = self.max_pts_per_second.load(SeqCst) as u8;
            self.pts_per_second.store(rate, SeqCst);
            let pts = f32_to_u32(rate as f32 / (1000.0f32 / self.default_send_interval.load(SeqCst) as f32));
            self.pts_per_ct.store(if pts < 0xff { pts as u8 } else { 0xfe }, SeqCst);
        }
        self.send_communications_tick(false, false);
    }

    pub fn send_message_to(&self, slot: u8, m: Msg, guaranteed: bool) -> u32 {
        self.mm().send_to(m, slot, guaranteed)
    }
    pub fn send_message_to_all(&self, m: Msg, guaranteed: bool) -> u32 {
        self.mm().send(m, guaranteed)
    }

    /// SendToAllAndWait: queued for the next tick (referenced), with its answers' timeout and retries
    pub fn send_to_all_and_wait(&self, m: Msg, timeout: u32, retries: u32) -> i32 {
        let _l = self.s2aw_lock.lock();
        self.host.msg_reference(m);
        self.with_s2aw(|q| q.push(S2aw { m, timeout, retries }));
        ERROR_OK
    }

    /// StartThreads: the main, send and transfer threads of a published server, started and named
    pub fn start_threads(&self, me: usize) {
        if !self.game_was_published.load(SeqCst) {
            return;
        }
        for (kind, slot, line) in [
            (THREAD_SV_MAIN, &self.main_thread, 1584),
            (THREAD_SV_SEND, &self.send_thread, 1585),
            (THREAD_SV_XFER, &self.xfer_thread, 1586),
        ] {
            let mut t = 0;
            let r = self.host.thread_new(kind, me, &mut t);
            slot.store(t, SeqCst);
            self.verify(r == ERROR_OK, line);
        }
        self.host.thread_priority(self.main_thread.load(SeqCst));
        self.host.thread_priority(self.send_thread.load(SeqCst));
        self.host.event_wait(self.ev(EV_MAIN_STARTED), WAIT_INFINITE);
        self.host.event_wait(self.ev(EV_SEND_STARTED), WAIT_INFINITE);
        self.host.event_wait(self.ev(EV_XFER_STARTED), WAIT_INFINITE);
        self.host.thread_name(self.main_thread.load(SeqCst), "CommMain");
        self.host.thread_name(self.send_thread.load(SeqCst), "CommSend");
        self.host.thread_name(self.xfer_thread.load(SeqCst), "CommXfer");
    }

    /// StopThreads: the send, main and transfer threads told to end and waited for
    pub fn stop_threads(&self) {
        if !self.game_was_published.load(SeqCst) {
            return;
        }
        for (exit, finished) in
            [(EV_SEND_EXIT, EV_SEND_FINISHED), (EV_MAIN_EXIT, EV_MAIN_FINISHED), (EV_XFER_EXIT, EV_XFER_FINISHED)]
        {
            self.host.event_set(self.ev(exit));
            self.host.event_wait(self.ev(finished), WAIT_INFINITE);
        }
    }

    pub fn tune_server(&self) -> bool {
        self.tune().tune_server()
    }

    /// UnregisterSlot: a client's slot (never the server's or none) out of the message manager
    pub fn unregister_slot(&self, slot: u8) {
        if slot != SLOT_SERVER && slot != SLOT_NONE {
            self.mm().unregister_slot(slot);
        }
    }

    /// XferFileToClient: a file transfer queued to a client's slot, the transfer thread woken
    pub fn xfer_file_to_client(&self, spec: usize, user_data: u32, slot: u8, id: &mut u32) -> bool {
        match self.mm().add_xfer(spec, user_data, slot) {
            Some(x) => {
                *id = x;
                self.host.event_set(self.ev(EV_XFER_STUFF_WAITING));
                true
            }
            None => false,
        }
    }

    /// The main thread ("CommMain"): the game started, paused, unpaused and stopped; the clients' packets read; a
    /// send-to-all-and-wait's answers awaited (the send interval 3 s meanwhile in a multiplayer game; resent at each
    /// timeout, the laggers booted after the retries); reconnecting clients awaited (20 s, cut to 3 s once all came)
    pub fn main_thread(&self) -> u32 {
        let mut saved_send_interval = WAIT_INFINITE;
        let mut exit = false;
        let mut deadline: u32 = 0;
        let mut timed_out = false;
        let mut wait_state = 0u32;
        let mut retries: u32 = 0;
        self.host.event_set(self.ev(EV_MAIN_STARTED));
        self.host.fpu_start();
        let events = [
            self.ev(EV_START_GAME),
            self.ev(EV_PAUSE_GAME),
            self.ev(EV_STOP_GAME),
            self.ev(EV_MAIN_EXIT),
            self.ev(EV_INPUT_AVAILABLE),
            self.ev(EV_WAIT_FOR_RESPONSE),
            self.ev(EV_WAIT_FOR_RECONNECTS),
            self.ev(EV_XFER_INPUT_AVAILABLE),
            self.ev(EV_XFER_ACCEPT),
            self.ev(EV_RESUME_MIGRATED),
            self.ev(EV_UNPAUSE_GAME),
        ];
        self.game_state.store(STATE_OPEN, SeqCst);
        self.send_interval.store(self.default_send_interval.load(SeqCst), SeqCst);
        self.host.event_set(self.ev(EV_CHANGE_SEND_INTERVAL));
        let mut timeout = WAIT_INFINITE;
        loop {
            self.host.fpu_check();
            match self.host.wait_any(&events, timeout) {
                0 => {
                    self.host.event_clear(events[0]);
                    self.game_state.store(STATE_RUNNING, SeqCst);
                    self.send_interval.store(self.default_send_interval.load(SeqCst), SeqCst);
                    self.host.event_set(self.ev(EV_CHANGE_SEND_INTERVAL));
                    if wait_state == 2 {
                        timeout = WAIT_INFINITE;
                        wait_state = 0;
                    }
                }
                1 => {
                    self.host.event_clear(events[1]);
                    if wait_state != 2 && self.game_state.load(SeqCst) == STATE_RUNNING {
                        self.game_state.store(STATE_PAUSED, SeqCst);
                        self.comms.set(BaseField::IsGamePaused, 1);
                    }
                }
                WAIT_ANY_TIMEOUT => timed_out = true,
                WAIT_ANY_FAILED => {}
                2 => {
                    self.host.event_clear(events[2]);
                    if wait_state != 2 {
                        let m = self.host.msg_new(NewMsg::StopGame, 0, 0, 0);
                        if m != 0 {
                            self.host.msg_set_slot(m, true, SLOT_SERVER);
                            self.mm().send(m, true);
                            self.host.msg_dereference(m);
                            self.game_state.store(STATE_OPEN, SeqCst);
                            self.send_interval.store(WAIT_INFINITE, SeqCst);
                            self.host.event_set(self.ev(EV_CHANGE_SEND_INTERVAL));
                        }
                    }
                }
                3 => {
                    self.host.event_clear(events[3]);
                    if wait_state != 2 {
                        exit = true;
                    }
                }
                4 => {
                    self.host.event_clear(events[4]);
                    self.process_pending_player_inputs();
                    if wait_state == 1 {
                        timed_out = true;
                    }
                    if wait_state == 2 && self.host.time() > deadline {
                        self.host.handle_reconnect_wait_timed_out();
                    }
                }
                5 => {
                    self.host.event_clear(events[5]);
                    saved_send_interval = self.send_interval.load(SeqCst);
                    if self.multiplayer() {
                        self.send_interval.store(3000, SeqCst);
                        self.host.event_set(self.ev(EV_CHANGE_SEND_INTERVAL));
                    }
                    timeout = saved_send_interval;
                    deadline = self.host.time().wrapping_add(self.s2aw_timeout.load(SeqCst));
                    retries = 0;
                    wait_state = 1;
                }
                6 => {
                    self.host.event_clear(events[6]);
                    timeout = 1000;
                    deadline = self.host.time().wrapping_add(20000);
                    wait_state = 2;
                }
                7 => self.host.event_clear(events[7]),
                8 => {
                    self.host.event_clear(events[8]);
                    self.host.accept_for_xfer(self.protocol(), self.server());
                }
                9 => {
                    self.host.event_clear(events[9]);
                    self.wait_for_reconnects_time.store(0, SeqCst);
                    let now = self.host.time();
                    if now < deadline && deadline.wrapping_sub(now) > 3500 {
                        deadline = now.wrapping_add(3000);
                    }
                }
                10 => {
                    self.host.event_clear(events[10]);
                    if wait_state != 2 && self.game_state.load(SeqCst) == STATE_PAUSED {
                        self.comms.set(BaseField::IsGamePaused, 0);
                        self.game_state.store(STATE_RUNNING, SeqCst);
                    }
                }
                _ => {}
            }
            if timed_out {
                match wait_state {
                    1 => {
                        let number = self.s2aw_number.load(SeqCst);
                        if self.mm().outstanding(number) == 0 {
                            let _l = self.s2aw_lock.lock();
                            match self.s2aw_type.load(SeqCst) {
                                MSG_PAUSE_GAME if self.game_state.load(SeqCst) != STATE_PAUSED => {
                                    self.host.event_set(self.ev(EV_PAUSE_GAME));
                                }
                                MSG_UNPAUSE_GAME if self.game_state.load(SeqCst) == STATE_PAUSED => {
                                    self.host.event_set(self.ev(EV_UNPAUSE_GAME));
                                }
                                _ => {}
                            }
                            self.processing_s2aw.store(false, SeqCst);
                            self.send_interval.store(saved_send_interval, SeqCst);
                            self.host.event_set(self.ev(EV_CHANGE_SEND_INTERVAL));
                            timeout = WAIT_INFINITE;
                            wait_state = 0;
                        } else if self.host.time() > deadline {
                            retries = retries.wrapping_add(1);
                            if retries >= self.s2aw_retries.load(SeqCst) {
                                self.boot_all_laggers(self.s2aw_number.load(SeqCst));
                                self.send_interval.store(self.default_send_interval.load(SeqCst), SeqCst);
                                self.host.event_set(self.ev(EV_CHANGE_SEND_INTERVAL));
                                timeout = WAIT_INFINITE;
                            } else {
                                self.mm().resend(self.s2aw_number.load(SeqCst));
                                deadline = self.host.time().wrapping_add(self.s2aw_timeout.load(SeqCst));
                            }
                        }
                    }
                    2 if self.host.time() > deadline => self.host.handle_reconnect_wait_timed_out(),
                    _ => {}
                }
                timed_out = false;
            }
            if exit {
                break;
            }
        }
        self.host.event_set(self.ev(EV_MAIN_FINISHED));
        0
    }

    /// The send thread ("CommSend"): a tick every send interval (50 ms early, the lateness carried to the next),
    /// with a checksum from tick 40 on; a forced tick at once; a new interval from the change event
    pub fn send_thread(&self) -> u32 {
        let wait = |interval: u32| -> i32 {
            // (long arithmetic: 32 bits, as on Windows)
            if interval == WAIT_INFINITE {
                WAIT_INFINITE as i32
            } else {
                let t = interval.wrapping_sub(50) as i32;
                if t < 0 {
                    1
                } else {
                    t
                }
            }
        };
        let mut send_interval = self.send_interval.load(SeqCst);
        let mut timeout: i32 = wait(send_interval);
        let mut last_time = self.host.time();
        self.host.event_set(self.ev(EV_SEND_STARTED));
        self.host.fpu_start();
        let events = [self.ev(EV_CHANGE_SEND_INTERVAL), self.ev(EV_SEND_EXIT), self.ev(EV_SEND_FORCE_CT)];
        loop {
            self.host.fpu_check();
            let result = self.host.wait_any(&events, timeout as u32);
            let now = self.host.time();
            let drift = last_time.wrapping_sub(now).wrapping_add(send_interval) as i32;
            match result {
                0 => {
                    self.host.event_clear(events[0]);
                    send_interval = self.send_interval.load(SeqCst);
                    timeout = wait(send_interval);
                    last_time = now;
                }
                1 => {
                    self.host.event_clear(events[1]);
                    self.host.event_set(self.ev(EV_SEND_FINISHED));
                    return 0;
                }
                2 => {
                    self.host.event_clear(events[2]);
                    if !(self.host.sim_mode() && self.sim_game_running()) {
                        self.send_communications_tick(false, true);
                    }
                    self.mm().set_force_ct(false);
                    timeout = wait(send_interval);
                    last_time = now;
                }
                WAIT_ANY_TIMEOUT => {
                    if self.host.sim_mode() && self.sim_game_running() {
                        timeout = 50;
                        last_time = now;
                        continue;
                    }
                    if self.current_ct.load(SeqCst) >= self.next_checksum_ct.load(SeqCst) {
                        self.send_communications_tick(true, true);
                        self.next_checksum_ct.fetch_add(self.checksum_interval.load(SeqCst), SeqCst);
                    } else {
                        self.send_communications_tick(false, true);
                    }
                    timeout = WAIT_INFINITE as i32;
                    if send_interval != WAIT_INFINITE {
                        timeout = (drift as u32).wrapping_add(send_interval).wrapping_sub(50) as i32;
                        if timeout < 0 {
                            timeout = 1;
                        }
                    }
                    last_time = (drift as u32).wrapping_add(now);
                }
                _ => {}
            }
        }
    }

    /// The transfer thread ("CommXfer"): the queued transfers pumped every 500 ms until none remain
    pub fn xfer_thread(&self) -> u32 {
        let mut exit = false;
        let mut waiting = false;
        let mut timeout = WAIT_INFINITE;
        self.host.event_set(self.ev(EV_XFER_STARTED));
        self.host.fpu_start();
        let events = [self.ev(EV_XFER_EXIT), self.ev(EV_XFER_STUFF_WAITING)];
        while !exit {
            self.host.fpu_check();
            match self.host.wait_any(&events, timeout) {
                0 => {
                    self.host.event_clear(events[0]);
                    exit = true;
                }
                1 => {
                    self.host.event_clear(events[1]);
                    waiting = true;
                    timeout = 500;
                }
                WAIT_ANY_TIMEOUT => {
                    if waiting {
                        waiting = self.mm().process_xfers();
                        if !waiting {
                            timeout = WAIT_INFINITE;
                        }
                    }
                    self.host.sleep(1);
                }
                _ => {}
            }
        }
        self.host.event_set(self.ev(EV_XFER_FINISHED));
        0
    }

    /// The checksums' state (the tests): "tick:checksum/slot,...;..." then the out-of-sync slots
    pub fn checks_state(&self) -> String {
        let c = self.checks();
        let mut s = String::new();
        for (t, list) in &c.responses {
            s += &format!("{t}:");
            for (i, e) in list.iter().enumerate() {
                s += &format!("{}{}/{}", if i > 0 { "," } else { "" }, e.0, e.1);
            }
            s += ";";
        }
        s += " oos";
        for o in &c.oos_slots {
            s += &format!(" {o}");
        }
        s
    }

    /// The queues' lengths (the tests): the incoming inputs, the send-to-all-and-waits
    pub fn queue_lengths(&self) -> (usize, usize) {
        let i = {
            let _l = self.inputs_lock.lock();
            self.with_inputs(|q| q.len())
        };
        let s = {
            let _l = self.s2aw_lock.lock();
            self.with_s2aw(|q| q.len())
        };
        (i, s)
    }
}
