//! `NESynchronousClient` whole (the original NESynchronousClient.cpp, LLE 0x1004607d..0x100472cf, with its inline
//! members): the lock-step multiplayer client. Its main thread ("CommMain") hands the received messages to the lag
//! simulator (and so to the recorder and `process_message`) and retries or fails a connect request on its timeout;
//! its receive thread ("CommRecv") reads the game connection's packets and the file transfer connection's records
//! and reports a lost connection after 75 s of silence. The lag simulator and the recorder are this object's own
//! (`lagloss`, `recplay`), their callbacks its methods.
//!
//! The original's two critical sections are re-entrant locks held where it held them (the incoming queue across
//! the main thread's whole hand-over, the outgoing queue across a tick's reply); the queues are reached only under
//! them and never across a call out. Every other field is an atomic: the original shared them between its threads
//! without a lock. The C++ class keeps the type callers hold, its virtual members (the game's `EEClient` overrides
//! them) and the base class's fields, all reached through the host.

use std::cell::UnsafeCell;
use std::collections::VecDeque;
use std::sync::atomic::{AtomicU32, AtomicU8, AtomicUsize, Ordering::SeqCst};
use std::sync::Arc;

use crate::lagloss::LagLoss;
use crate::qhost::*;
use crate::recplay::{RecPlay, MODE_PLAYING};
use crate::rlock::RLock;

/// `ESVERIFY_AT`'s unit for this file (QHost::verify)
pub const UNIT: u32 = 3;

pub const SLOT_SERVER: u8 = 0xfe;
pub const SLOT_NONE: u8 = 0xff;
pub const ERROR_NOT_OPEN: i32 = 4;
/// `kConnectionAccepted`
pub const CONNECTION_ACCEPTED: i32 = 1;

/// `TSEvent::WaitAny`'s answers besides an index
pub const WAIT_ANY_TIMEOUT: i32 = -1;
pub const WAIT_ANY_FAILED: i32 = -2;

/// The thread kinds this object starts (`QHost::thread_new`; 0 and 1 are the lone queues' own)
pub const THREAD_CL_LAG: u32 = 2;
pub const THREAD_CL_PLAYBACK: u32 = 3;
pub const THREAD_CL_MAIN: u32 = 4;
pub const THREAD_CL_RECEIVE: u32 = 5;

// message types (NEMessageID.h)
pub const MSG_CLIENT_CONNECTION_REQUEST_RESPONSE: u8 = 0x23;
pub const MSG_CLIENT_DISCONNECTION_NOTIFICATION: u8 = 0x24;
pub const MSG_COMMUNICATIONS_TICK: u8 = 0x26;
pub const MSG_START_GAME: u8 = 0x2d;
pub const MSG_STOP_GAME: u8 = 0x2f;
pub const MSG_WORLD_DATABASE_CHECKSUM: u8 = 0x31;
pub const MSG_PAUSE_GAME: u8 = 0x33;
pub const MSG_UNPAUSE_GAME: u8 = 0x35;
pub const MSG_CLIENT_RECONNECTION_REQUEST: u8 = 0x37;
pub const MSG_CLIENT_RECONNECTION_REQUEST_RESPONSE: u8 = 0x38;
pub const MSG_CLIENT_REQUEST_PAUSE: u8 = 0x39;
pub const MSG_SERVER_IS_ALIVE: u8 = 0x3a;

pub use crate::comms::Comms;
pub use crate::ffi_client::{EeNetClMsgField as MsgField, EeNetClMsgFlag as MsgFlag, EeNetClNewMsg as NewMsg};
pub use crate::ffi_comms::EeNetCommsField as BaseField;

/// The client's calls out besides the queues' (`QHost`): the engine, the protocol and its connections, the messages,
/// the received file and the C++ class's virtual members.
pub trait ClientHost: QHost {
    /// `TSEvent::WaitAny`: the index of the set event (the lowest), WAIT_ANY_TIMEOUT or WAIT_ANY_FAILED
    fn wait_any(&self, events: &[Ev], timeout: u32) -> i32;
    /// `SetPriority(PLAT_PRIORITY_TIME_CRITICAL)`
    fn thread_priority(&self, t: Thr);

    /// `NEProtocol::GetCount()`
    fn protocol_count(&self) -> u32;
    /// `NEProtocol::GetProtocol(0, out)`
    fn protocol_get(&self, out: &mut usize) -> i32;
    /// `Connect(name, host, address, 2, event, out, timeout)`; `strings` is the caller's three, passed through
    fn connect(&self, strings: usize, event: Ev, out: &mut usize, timeout: u32) -> i32;
    /// `ConnectForXfer(connection, 1, event, out)`
    fn connect_for_xfer(&self, connection: usize, event: Ev, out: &mut usize) -> i32;
    /// `NEConnection::Close()`
    fn conn_close(&self, connection: usize) -> i32;
    /// `NEConnection::Receive(buffer's size, buffer, length)`
    fn conn_receive(&self, connection: usize, buf: &mut [u8], length: &mut u32) -> i32;
    /// `NEConnection::Send(data, size)`
    fn conn_send(&self, connection: usize, data: usize, size: u32) -> i32;
    fn detected_game_count(&self) -> u32;
    /// `GetDetectedGame(index, name, host, address)`; `strings` is the caller's three, passed through
    fn detected_game(&self, index: u32, strings: usize) -> i32;
    /// `SearchForGame(UString("Anything"), UString("EEService"), start)`
    fn search_for_game(&self, start: bool) -> i32;
    /// `XferReceive(size, buffer, received)` (`received` passed in, as the original's variable)
    fn xfer_receive(&self, buf: &mut [u8], received: &mut u32) -> i32;
    /// `NEMXferMessage::StaticExportSize()`
    fn xfer_export_size(&self) -> u32;

    fn msg_type(&self, m: Msg) -> u8;
    fn msg_flag(&self, m: Msg, f: MsgFlag) -> bool;
    /// `SetForSpecificSlot` (false) or `SetOriginatingSlot` (true)
    fn msg_set_slot(&self, m: Msg, originating: bool, slot: u8);
    /// `GetSendToAllAndWaitMessageID(out)`
    fn msg_s2aw_id(&self, m: Msg, out: &mut u32) -> bool;
    fn msg_get(&self, m: Msg, f: MsgField) -> usize;
    fn msg_new(&self, kind: NewMsg, a: usize, b: usize, c: usize, d: usize) -> Msg;
    /// `NEMCommunicationsTickResponse::AddPlayerInput(input)`
    fn msg_add_input(&self, response: Msg, input: Msg) -> i32;
    /// `ExportMessage(&data, size)`: the export (C++ `new[]`, 0 none) and its size
    fn msg_export_new(&self, m: Msg, size: &mut u32) -> usize;
    /// `delete` of an export
    fn bytes_free(&self, data: usize);

    /// `FSFile::Write(data, size, written)` of the received file
    fn fsfile_write(&self, f: FileH, data: usize, size: u32, written: &mut u32) -> i32;
    fn fsfile_close(&self, f: FileH);
    fn fsfile_delete(&self, f: FileH);
    /// `delete` of the received file
    fn fsfile_free(&self, f: FileH);

    // the C++ class's virtual members (the game's EEClient)
    /// `CreateXferFile(UWideString(name), size)`; `name` points at the transfer's zero-terminated bytes
    fn create_xfer_file(&self, name: usize, size: u32) -> FileH;
    fn is_playback_recording_session(&self) -> bool;
    fn connect_timeout(&self) -> u32;
    fn client_slot(&self) -> u8;
    fn ignore_connection_timeout(&self) -> bool;
    fn set_connection_state(&self, state: i32);
    fn handle_message(&self, m: Msg);
    fn handle_user_input(&self, m: Msg);
    fn handle_connection_lost(&self);
    fn handle_start_game(&self, offset: u32) -> u32;
    fn handle_stop_game(&self);
    fn handle_connection_timeout_warning(&self, remaining: u32);
    /// the virtual `ProcessLocalUserInput(message, player)` (this class's own unless the game overrides it)
    fn process_local_user_input(&self, m: Msg, player: u8) -> i32;
}

/// The queues' host: the client's, except that their callbacks are the client's methods and their threads the
/// client's kinds.
pub struct ClientQ<H: ClientHost + 'static> {
    client: *const Client<H>,
}

// The client outlives its queues (it frees them first) and is shared by its threads as the original was.
unsafe impl<H: ClientHost> Send for ClientQ<H> {}
unsafe impl<H: ClientHost> Sync for ClientQ<H> {}

impl<H: ClientHost> ClientQ<H> {
    fn c(&self) -> &Client<H> {
        // SAFETY: see above
        unsafe { &*self.client }
    }
    fn h(&self) -> &H {
        &self.c().host
    }
}

impl<H: ClientHost> QHost for ClientQ<H> {
    fn time(&self) -> u32 {
        self.h().time()
    }
    fn verify(&self, unit: u32, line: u32) {
        self.h().verify(unit, line)
    }
    fn fpu_start(&self) {
        self.h().fpu_start()
    }
    fn fpu_check(&self) {
        self.h().fpu_check()
    }
    fn event_new(&self) -> Ev {
        self.h().event_new()
    }
    fn event_free(&self, e: Ev) {
        self.h().event_free(e)
    }
    fn event_set(&self, e: Ev) {
        self.h().event_set(e)
    }
    fn event_clear(&self, e: Ev) {
        self.h().event_clear(e)
    }
    fn event_wait(&self, e: Ev, timeout: u32) -> i32 {
        self.h().event_wait(e, timeout)
    }
    fn thread_new(&self, kind: u32, obj: usize, out: &mut Thr) -> i32 {
        let kind = if kind == THREAD_LAG { THREAD_CL_LAG } else { THREAD_CL_PLAYBACK };
        self.h().thread_new(kind, obj, out)
    }
    fn thread_name(&self, t: Thr, name: &str) {
        self.h().thread_name(t, name)
    }
    fn thread_free(&self, t: Thr) {
        self.h().thread_free(t)
    }
    fn msg_reference(&self, m: Msg) {
        self.h().msg_reference(m)
    }
    fn msg_dereference(&self, m: Msg) {
        self.h().msg_dereference(m)
    }
    fn msg_export_size(&self, m: Msg) -> u32 {
        self.h().msg_export_size(m)
    }
    fn msg_export(&self, m: Msg, buf: &mut [u8]) -> u32 {
        self.h().msg_export(m, buf)
    }
    fn msg_from_bytes(&self, data: Option<&[u8]>) -> Msg {
        self.h().msg_from_bytes(data)
    }
    fn deliver(&self, kind: u32, m: Msg) -> i32 {
        let c = self.c();
        match kind {
            DELIVER_LAG_RECEIVE => {
                c.lag_receive_loss_callback(m);
                0
            }
            DELIVER_LAG_SEND => c.lag_send_loss_callback(m),
            _ => {
                c.record_playback_callback(m);
                0
            }
        }
    }
    fn file_create(&self, f: FileH) -> i32 {
        self.h().file_create(f)
    }
    fn file_delete(&self, f: FileH) -> i32 {
        self.h().file_delete(f)
    }
    fn file_open(&self, f: FileH, access: u32) -> i32 {
        self.h().file_open(f, access)
    }
    fn file_close(&self, f: FileH) {
        self.h().file_close(f)
    }
    fn file_write(&self, f: FileH, data: &[u8]) -> i32 {
        self.h().file_write(f, data)
    }
    fn file_write_u32(&self, f: FileH, value: u32) -> i32 {
        self.h().file_write_u32(f, value)
    }
    fn file_read_u32(&self, f: FileH, value: &mut u32) -> i32 {
        self.h().file_read_u32(f, value)
    }
    fn file_read(&self, f: FileH, buf: &mut [u8]) -> i32 {
        self.h().file_read(f, buf)
    }
}

// the events, by the original's member order
const EV_INCOMING_MESSAGE_AVAILABLE: usize = 0;
const EV_MAIN_EXIT: usize = 1;
const EV_MAIN_STARTED: usize = 2;
const EV_MAIN_FINISHED: usize = 3;
const EV_MAIN_START_CONNECT: usize = 4;
const EV_MAIN_END_CONNECT: usize = 5;
const EV_INCOMING_PACKET_AVAILABLE: usize = 6;
const EV_XFER_INCOMING_AVAILABLE: usize = 7;
const EV_RECEIVE_CAN_GO: usize = 8;
const EV_RECEIVE_EXIT: usize = 9;
const EV_RECEIVE_STARTED: usize = 10;
const EV_RECEIVE_FINISHED: usize = 11;
const EVENTS: usize = 12;

/// The receive thread's packet buffer (`unsigned char theBuffer[0x240]`)
const PACKET_SIZE: usize = 0x240;
/// The connection's silence before a warning, and before it is lost
const SILENCE_STEP: u32 = 10000;
const SILENCE_LOST: u32 = 75000;

pub struct Client<H: ClientHost + 'static> {
    pub host: H,
    /// the base class's fields (NESynchronousCommunications, shared with the C++ base)
    pub comms: Arc<Comms>,
    lag: UnsafeCell<Option<LagLoss<ClientQ<H>>>>,
    rec: UnsafeCell<Option<RecPlay<ClientQ<H>>>>,
    incoming_lock: RLock,
    incoming: UnsafeCell<VecDeque<Msg>>,
    outgoing_lock: RLock,
    outgoing: UnsafeCell<VecDeque<Msg>>,
    last_tick_response: AtomicUsize,
    protocol: AtomicUsize,
    connection: AtomicUsize,
    xfer_connection: AtomicUsize,
    start_connect_message: AtomicUsize,
    main_thread: AtomicUsize,
    receive_thread: AtomicUsize,
    events: [AtomicUsize; EVENTS],
    current_physics_tick: AtomicU32,
    current_communications_tick: AtomicU32,
    last_user_input_time: AtomicU32,
    user_inputs_this_second: AtomicU32,
    last_playback_tick: AtomicU32,
    max_num_connection_trys: AtomicU32,
    num_connection_trys: AtomicU32,
    my_send_to_slot: AtomicU8,
}

// The queues are reached only under their locks, never across a call out; the queue objects are set once by the
// constructor and only read after.
unsafe impl<H: ClientHost + Sync> Sync for Client<H> {}

impl<H: ClientHost> Client<H> {
    /// The constructor: protocol 0, the threads' events, the lag simulator with lag and loss off, the recorder
    /// passing, the receive thread allowed to go.
    pub fn new(host: H, comms: Arc<Comms>) -> Box<Self> {
        if host.protocol_count() == 0 {
            host.verify(UNIT, 38);
        }
        let mut protocol = 0;
        if host.protocol_get(&mut protocol) != ERROR_OK {
            host.verify(UNIT, 40);
        }
        let events: [AtomicUsize; EVENTS] = std::array::from_fn(|_| AtomicUsize::new(0));
        for (i, e) in events.iter().enumerate() {
            let ev = host.event_new();
            e.store(ev, SeqCst);
            if ev == 0 {
                host.verify(UNIT, 57 + i as u32);
            }
        }
        let c = Box::new(Client {
            host,
            comms,
            lag: UnsafeCell::new(None),
            rec: UnsafeCell::new(None),
            incoming_lock: RLock::new(),
            incoming: UnsafeCell::new(VecDeque::new()),
            outgoing_lock: RLock::new(),
            outgoing: UnsafeCell::new(VecDeque::new()),
            last_tick_response: AtomicUsize::new(0),
            protocol: AtomicUsize::new(protocol),
            connection: AtomicUsize::new(0),
            xfer_connection: AtomicUsize::new(0),
            start_connect_message: AtomicUsize::new(0),
            main_thread: AtomicUsize::new(0),
            receive_thread: AtomicUsize::new(0),
            events,
            current_physics_tick: AtomicU32::new(0),
            current_communications_tick: AtomicU32::new(0),
            last_user_input_time: AtomicU32::new(0),
            user_inputs_this_second: AtomicU32::new(0),
            last_playback_tick: AtomicU32::new(0),
            max_num_connection_trys: AtomicU32::new(1),
            num_connection_trys: AtomicU32::new(0),
            my_send_to_slot: AtomicU8::new(SLOT_NONE),
        });
        let me: *const Client<H> = &*c;
        let lag = LagLoss::new(ClientQ { client: me });
        // SAFETY: set once, before any reader
        unsafe { *c.lag.get() = Some(lag) };
        c.lag().disable_lag(false);
        c.lag().disable_receive_loss();
        c.lag().disable_send_loss();
        let rec = RecPlay::new(ClientQ { client: me });
        unsafe { *c.rec.get() = Some(rec) };
        c.rec().pass_mode();
        c.host.event_set(c.ev(EV_RECEIVE_CAN_GO));
        c
    }

    /// The destructor's work: the last tick reply given back, the search stopped, the threads, the recorder, the lag
    /// simulator and the events freed.
    pub fn release(&self) {
        let r = self.last_tick_response.load(SeqCst);
        if r != 0 {
            self.host.msg_dereference(r);
        }
        self.search_for_game(false);
        self.host.thread_free(self.receive_thread.load(SeqCst));
        self.host.thread_free(self.main_thread.load(SeqCst));
        self.rec().release();
        self.lag().release();
        for e in &self.events {
            self.host.event_free(e.load(SeqCst));
        }
    }

    pub fn lag(&self) -> &LagLoss<ClientQ<H>> {
        // SAFETY: set by the constructor
        unsafe { (*self.lag.get()).as_ref().unwrap() }
    }
    pub fn rec(&self) -> &RecPlay<ClientQ<H>> {
        unsafe { (*self.rec.get()).as_ref().unwrap() }
    }
    fn ev(&self, i: usize) -> Ev {
        self.events[i].load(SeqCst)
    }
    fn verify(&self, ok: bool, line: u32) {
        if !ok {
            self.host.verify(UNIT, line);
        }
    }
    fn with_incoming<R>(&self, f: impl FnOnce(&mut VecDeque<Msg>) -> R) -> R {
        // SAFETY: callers hold incoming_lock; `f` makes no call out
        f(unsafe { &mut *self.incoming.get() })
    }
    fn with_outgoing<R>(&self, f: impl FnOnce(&mut VecDeque<Msg>) -> R) -> R {
        // SAFETY: callers hold outgoing_lock; `f` makes no call out
        f(unsafe { &mut *self.outgoing.get() })
    }

    // ---- the accessors (the header's inline members)
    pub fn protocol(&self) -> usize {
        self.protocol.load(SeqCst)
    }
    pub fn connection(&self) -> usize {
        self.connection.load(SeqCst)
    }
    pub fn current_communications_tick(&self) -> u32 {
        self.current_communications_tick.load(SeqCst)
    }
    pub fn set_current_communications_tick(&self, tick: u32) {
        self.current_communications_tick.store(tick, SeqCst)
    }
    pub fn current_physics_tick(&self) -> u32 {
        self.current_physics_tick.load(SeqCst)
    }
    pub fn set_current_physics_tick(&self, tick: u32) {
        self.current_physics_tick.store(tick, SeqCst)
    }
    pub fn my_send_to_slot(&self) -> u8 {
        self.my_send_to_slot.load(SeqCst)
    }
    pub fn set_my_send_to_slot(&self, slot: u8) {
        self.my_send_to_slot.store(slot, SeqCst)
    }
    pub fn num_connection_trys(&self) -> u32 {
        self.num_connection_trys.load(SeqCst)
    }
    pub fn set_max_num_connection_trys(&self, max: u32) {
        self.max_num_connection_trys.store(max, SeqCst)
    }
    pub fn detected_game_count(&self) -> u32 {
        self.host.detected_game_count()
    }
    pub fn record_playback_mode(&self) -> u8 {
        self.rec().mode()
    }

    /// `ClearIncomingMessages`: every received message not yet handed over given back.
    pub fn clear_incoming_messages(&self) {
        let _g = self.incoming_lock.lock();
        while let Some(m) = self.with_incoming(|q| q.pop_front()) {
            self.host.msg_dereference(m);
        }
    }

    /// `SendStartConnectMessage`: the main thread told to time the connect request (which this holds).
    pub fn send_start_connect_message(&self, m: Msg) {
        self.host.event_set(self.ev(EV_MAIN_START_CONNECT));
        self.host.set_connection_state(1);
        self.start_connect_message.store(m, SeqCst);
        self.host.msg_reference(m);
    }

    pub fn send_end_connect_message(&self) {
        self.host.event_set(self.ev(EV_MAIN_END_CONNECT));
    }

    /// `SendExitMessage` and `StopThreads`: the receive thread, then the main thread, told to exit and waited for.
    pub fn stop_threads(&self) {
        self.host.event_set(self.ev(EV_RECEIVE_EXIT));
        self.host.event_wait(self.ev(EV_RECEIVE_FINISHED), WAIT_INFINITE);
        self.host.event_set(self.ev(EV_MAIN_EXIT));
        self.host.event_wait(self.ev(EV_MAIN_FINISHED), WAIT_INFINITE);
    }

    /// `StartThreads`: the main and receive threads made at time-critical priority, waited for and named. `me` is
    /// this object as the thread bodies' argument.
    pub fn start_threads(&self, me: usize) {
        let mut t = 0;
        let r = self.host.thread_new(THREAD_CL_MAIN, me, &mut t);
        self.main_thread.store(t, SeqCst);
        self.verify(r == ERROR_OK, 1245);
        let mut t = 0;
        let r = self.host.thread_new(THREAD_CL_RECEIVE, me, &mut t);
        self.receive_thread.store(t, SeqCst);
        self.verify(r == ERROR_OK, 1246);
        self.host.thread_priority(self.main_thread.load(SeqCst));
        self.host.thread_priority(self.receive_thread.load(SeqCst));
        self.host.event_wait(self.ev(EV_MAIN_STARTED), WAIT_INFINITE);
        self.host.event_wait(self.ev(EV_RECEIVE_STARTED), WAIT_INFINITE);
        self.host.thread_name(self.main_thread.load(SeqCst), "CommMain");
        self.host.thread_name(self.receive_thread.load(SeqCst), "CommRecv");
    }

    // ---- the connections

    pub fn connect(&self, strings: usize) -> i32 {
        let timeout = self.host.connect_timeout();
        let mut out = self.connection.load(SeqCst);
        let r = self.host.connect(strings, self.ev(EV_INCOMING_PACKET_AVAILABLE), &mut out, timeout);
        self.connection.store(out, SeqCst);
        r
    }

    /// `ConnectForXfer`: the file transfer connection opened when connected; not-open whatever the outcome.
    pub fn connect_for_xfer(&self) -> i32 {
        let c = self.connection.load(SeqCst);
        if c != 0 {
            let mut out = self.xfer_connection.load(SeqCst);
            self.host.connect_for_xfer(c, self.ev(EV_XFER_INCOMING_AVAILABLE), &mut out);
            self.xfer_connection.store(out, SeqCst);
        }
        ERROR_NOT_OPEN
    }

    pub fn disconnect(&self) -> i32 {
        let mut r = ERROR_ALREADY_EXISTS;
        let c = self.connection.load(SeqCst);
        if c != 0 {
            r = self.host.conn_close(c);
            self.connection.store(0, SeqCst);
        }
        self.disconnect_from_xfer();
        r
    }

    pub fn disconnect_from_xfer(&self) -> i32 {
        let mut r = ERROR_ALREADY_EXISTS;
        let c = self.xfer_connection.load(SeqCst);
        if c != 0 {
            r = self.host.conn_close(c);
            self.xfer_connection.store(0, SeqCst);
        }
        r
    }

    pub fn detected_game(&self, index: u32, strings: usize) -> i32 {
        self.host.detected_game(index, strings)
    }

    /// `SearchForGame`: "EEService" games searched for (or no longer).
    pub fn search_for_game(&self, start: bool) -> i32 {
        self.host.search_for_game(start)
    }

    // ---- the queues' callbacks

    /// `LagReceiveLossCallback`: a received message on to the recorder.
    fn lag_receive_loss_callback(&self, m: Msg) {
        self.rec().process(m);
    }

    /// `LagSendLossCallback`: a message exported and sent on the connection unless a recording plays back; given
    /// back either way.
    fn lag_send_loss_callback(&self, m: Msg) -> i32 {
        let result = if self.rec().mode() == MODE_PLAYING {
            ERROR_OK
        } else {
            let mut size = 0u32;
            let data = self.host.msg_export_new(m, &mut size);
            self.verify(data != 0 && size > 0, 296);
            let mut sent = ERROR_OK;
            let c = self.connection.load(SeqCst);
            if c != 0 {
                sent = self.host.conn_send(c, data, size);
            }
            self.host.bytes_free(data);
            sent
        };
        self.host.msg_dereference(m);
        result
    }

    /// `RecordPlaybackCallback`: a message processed, except a send-to-all-and-wait message of a tick already seen
    /// (given back).
    fn record_playback_callback(&self, m: Msg) {
        let mut tick = 0u32;
        if !self.host.msg_s2aw_id(m, &mut tick) {
            self.process_message(m);
        } else if tick > self.last_playback_tick.load(SeqCst) {
            self.last_playback_tick.store(tick, SeqCst);
            self.process_message(m);
        } else {
            self.host.msg_dereference(m);
        }
    }

    // ---- the threads

    /// `Main`: the received messages handed to the lag simulator (given back when not connected), the exit, and the
    /// connect request's timing: 10 s after its start, then a retry every 5 s while tries are left, else it fails.
    pub fn main_thread(&self) -> u32 {
        let mut timeout = WAIT_INFINITE;
        self.host.event_set(self.ev(EV_MAIN_STARTED));
        self.host.fpu_start();
        let events = [
            self.ev(EV_INCOMING_MESSAGE_AVAILABLE),
            self.ev(EV_MAIN_EXIT),
            self.ev(EV_MAIN_START_CONNECT),
            self.ev(EV_MAIN_END_CONNECT),
        ];
        loop {
            self.host.fpu_check();
            match self.host.wait_any(&events, timeout) {
                0 => {
                    self.host.event_clear(events[0]);
                    {
                        let _g = self.incoming_lock.lock();
                        while let Some(m) = self.with_incoming(|q| q.pop_front()) {
                            if self.connection.load(SeqCst) == 0 {
                                self.host.msg_dereference(m);
                            } else {
                                self.lag().process(m);
                            }
                        }
                    }
                    self.host.event_set(self.ev(EV_RECEIVE_CAN_GO));
                }
                1 => {
                    self.host.event_clear(events[1]);
                    self.host.event_set(self.ev(EV_MAIN_FINISHED));
                    return 0;
                }
                2 => {
                    self.host.event_clear(events[2]);
                    timeout = 10000;
                }
                3 => {
                    self.host.event_clear(events[3]);
                    self.host.set_connection_state(2);
                    timeout = WAIT_INFINITE;
                }
                WAIT_ANY_TIMEOUT => {
                    let trys = self.num_connection_trys.load(SeqCst).wrapping_add(1);
                    self.num_connection_trys.store(trys, SeqCst);
                    let m = self.start_connect_message.load(SeqCst);
                    if trys < self.max_num_connection_trys.load(SeqCst) {
                        self.host.msg_reference(m);
                        timeout = 5000;
                        self.host.process_local_user_input(m, SLOT_NONE);
                    } else {
                        self.max_num_connection_trys.store(1, SeqCst);
                        self.num_connection_trys.store(0, SeqCst);
                        self.host.msg_dereference(m);
                        timeout = WAIT_INFINITE;
                        self.start_connect_message.store(0, SeqCst);
                        self.host.set_connection_state(0);
                    }
                }
                // a failed wait (the original read the error and went on); any other answer is not made
                _ => {}
            }
        }
    }

    /// `Receive`: the game connection's packets read into messages for the main thread (one batch at a time: the
    /// main thread lets the next go), the transfer connection's records assembled and written to the received
    /// file, and the connection's silence: a warning every 10 s, after 75 s the connection lost (when connected,
    /// in a game, and the game does not ignore it).
    pub fn receive_thread(&self) -> u32 {
        let mut exit = false;
        let mut timed_out = false;
        let mut xfer_buffer: Vec<u8> = Vec::new();
        let mut xfer_received: u32 = 0;
        let mut file: FileH = 0;
        let mut last_time: u32 = 0;
        let mut elapsed: u32 = 0;
        let mut timeout: u32 = SILENCE_STEP;
        // the original's uninitialized variables (an in-value of the receives): 0 here
        let mut length: u32 = 0;
        let mut received: u32 = 0;
        let mut buffer = [0u8; PACKET_SIZE];
        let mut xfer_id: u32;
        let percent_done: u32 = 100;
        let status_id: u32 = 3;
        self.host.event_set(self.ev(EV_RECEIVE_STARTED));
        self.host.fpu_start();
        let events =
            [self.ev(EV_INCOMING_PACKET_AVAILABLE), self.ev(EV_RECEIVE_EXIT), self.ev(EV_XFER_INCOMING_AVAILABLE)];
        loop {
            self.host.fpu_check();
            self.host.time();
            match self.host.wait_any(&events, timeout) {
                0 => 'packets: {
                    self.host.event_clear(events[0]);
                    self.host.event_wait(self.ev(EV_RECEIVE_CAN_GO), WAIT_INFINITE);
                    self.host.event_clear(self.ev(EV_RECEIVE_CAN_GO));
                    let mut count = 0u32;
                    let c = self.connection.load(SeqCst);
                    if c != 0 {
                        loop {
                            let r = self.host.conn_receive(c, &mut buffer, &mut length);
                            if r == ERROR_OK {
                                let m = self.host.msg_from_bytes(Some(&buffer));
                                if m != 0 {
                                    let _g = self.incoming_lock.lock();
                                    self.with_incoming(|q| q.push_back(m));
                                    count += 1;
                                }
                            }
                            if r != ERROR_OK {
                                break;
                            }
                        }
                        if count > 0 {
                            self.host.event_set(self.ev(EV_INCOMING_MESSAGE_AVAILABLE));
                            timeout = SILENCE_STEP;
                            last_time = self.host.time();
                            elapsed = 0;
                            break 'packets;
                        }
                    }
                    if self.host.time().wrapping_sub(last_time) < SILENCE_STEP {
                        timeout = SILENCE_STEP.wrapping_sub(self.host.time().wrapping_sub(last_time));
                    } else {
                        timed_out = true;
                    }
                    self.host.event_set(self.ev(EV_RECEIVE_CAN_GO));
                }
                1 => {
                    timeout = SILENCE_STEP;
                    elapsed = 0;
                    self.host.event_clear(events[1]);
                    exit = true;
                }
                2 => {
                    timeout = SILENCE_STEP;
                    elapsed = 0;
                    self.host.event_clear(events[2]);
                    // (NEMXferMessage::StaticExportSize(), a constant, read once)
                    let size = self.host.xfer_export_size();
                    if xfer_received == 0 {
                        xfer_buffer = vec![0u8; size as usize];
                    }
                    let want = size.wrapping_sub(xfer_received);
                    let from = (xfer_received as usize).min(xfer_buffer.len());
                    let to = from.saturating_add(want as usize).min(xfer_buffer.len());
                    self.host.xfer_receive(&mut xfer_buffer[from..to], &mut received);
                    xfer_received = xfer_received.wrapping_add(received);
                    if xfer_received > size {
                        self.host.verify(UNIT, 1038);
                    } else if xfer_received == size {
                        let x = self.host.msg_new(NewMsg::XferMessage, xfer_buffer.as_ptr() as usize, 0, 0, 0);
                        xfer_buffer = Vec::new();
                        xfer_received = 0;
                        match self.host.msg_get(x, MsgField::XferId) as u8 {
                            0 => {
                                let data = self.host.msg_get(x, MsgField::XferData);
                                // SAFETY: the message's data holds at least its id and size
                                let head = unsafe { std::slice::from_raw_parts(data as *const u8, 8) };
                                xfer_id = u32::from_le_bytes(head[0..4].try_into().unwrap());
                                let file_size = u32::from_le_bytes(head[4..8].try_into().unwrap());
                                file = self.host.create_xfer_file(data + 8, file_size);
                                if file == 0 {
                                    let slot = self.host.client_slot();
                                    let s = self.host.msg_new(
                                        NewMsg::XferStatus,
                                        slot as usize,
                                        percent_done as usize,
                                        xfer_id as usize,
                                        status_id as usize,
                                    );
                                    self.host.process_local_user_input(s, SLOT_SERVER);
                                }
                            }
                            1 => {
                                if file != 0 {
                                    let data = self.host.msg_get(x, MsgField::XferData);
                                    let n = self.host.msg_get(x, MsgField::XferDataSize) as u32;
                                    self.host.fsfile_write(file, data, n, &mut received);
                                } else {
                                    xfer_id = 0xffff;
                                    let slot = self.host.client_slot();
                                    let s = self.host.msg_new(
                                        NewMsg::XferStatus,
                                        slot as usize,
                                        percent_done as usize,
                                        xfer_id as usize,
                                        status_id as usize,
                                    );
                                    self.host.process_local_user_input(s, SLOT_SERVER);
                                }
                            }
                            2 if file != 0 => {
                                self.host.fsfile_close(file);
                                self.host.fsfile_free(file);
                                file = 0;
                            }
                            3 if file != 0 => {
                                self.host.fsfile_close(file);
                                self.host.fsfile_delete(file);
                                self.host.fsfile_free(file);
                                file = 0;
                            }
                            _ => {}
                        }
                        self.host.msg_dereference(x);
                    }
                }
                WAIT_ANY_TIMEOUT => timed_out = true,
                WAIT_ANY_FAILED => {
                    timeout = SILENCE_STEP;
                    elapsed = 0;
                }
                _ => {}
            }
            if timed_out {
                elapsed = elapsed.wrapping_add(SILENCE_STEP);
                if elapsed >= SILENCE_LOST {
                    if self.connection.load(SeqCst) != 0
                        && self.current_communications_tick.load(SeqCst) != 0
                        && !self.host.ignore_connection_timeout()
                    {
                        self.host.time();
                        self.clear_incoming_messages();
                        self.host.handle_connection_lost();
                    }
                    elapsed = 0;
                } else {
                    self.host.handle_connection_timeout_warning(SILENCE_LOST - elapsed);
                }
                timed_out = false;
                timeout = SILENCE_STEP;
                last_time = self.host.time();
            }
            if exit {
                break;
            }
        }
        self.host.event_set(self.ev(EV_RECEIVE_FINISHED));
        0
    }

    // ---- the game's inputs

    /// `ProcessGuaranteedLocalUserInput` / `ProcessLocalUserInput` (`guaranteed` false): `ProcessUserInputMessage`.
    /// At most 10 inputs a second that count towards the throttle; an accepted input is sent (or queued for the
    /// next tick's reply when guaranteed) unless a recording plays back or the game is paused (some messages go
    /// anyway) or the message is not available to send. Dropped inputs are given back and answer 7.
    pub fn process_user_input(&self, m: Msg, player: u8, guaranteed: bool) -> i32 {
        let mut result = 7;
        let mut accepted = true;
        let time = self.host.time();
        if time.wrapping_sub(self.last_user_input_time.load(SeqCst)) > 1000 {
            self.user_inputs_this_second.store(0, SeqCst);
            self.last_user_input_time.store(time, SeqCst);
        } else if self.user_inputs_this_second.load(SeqCst) < 10
            && self.host.msg_flag(m, MsgFlag::CountsTowardsPlayerInputThrottle)
        {
            self.user_inputs_this_second.fetch_add(1, SeqCst);
        } else {
            accepted = false;
        }
        if accepted {
            self.host.msg_set_slot(m, false, player);
            let send = (!self.host.is_playback_recording_session()
                || self.host.msg_type(m) == MSG_CLIENT_RECONNECTION_REQUEST)
                && (self.comms.get(BaseField::IsGamePaused) == 0
                    || self.comms.get(BaseField::IsMultiplayerSession) == 0
                    || self.host.msg_flag(m, MsgFlag::CanSendWhilePaused)
                    || self.host.msg_type(m) == MSG_CLIENT_REQUEST_PAUSE
                    || self.host.msg_type(m) == MSG_CLIENT_RECONNECTION_REQUEST
                    || self.host.msg_type(m) == MSG_CLIENT_DISCONNECTION_NOTIFICATION)
                && self.host.msg_flag(m, MsgFlag::IsAvailableToSend);
            if send {
                self.host.msg_set_slot(m, true, self.my_send_to_slot.load(SeqCst));
                self.host.handle_user_input(m);
                if !guaranteed {
                    result = self.send_a_message(m);
                } else {
                    let _g = self.outgoing_lock.lock();
                    self.with_outgoing(|q| q.push_back(m));
                    result = ERROR_OK;
                }
            } else {
                self.host.msg_dereference(m);
            }
        } else {
            self.host.msg_dereference(m);
        }
        result
    }

    /// `SendAMessage`: through the lag simulator.
    fn send_a_message(&self, m: Msg) -> i32 {
        self.lag().send(m)
    }

    fn new_tick_response(&self, tick: u32) -> Msg {
        let average = self.comms.get(BaseField::AveragePhysicsUpdateTime);
        let processed = self.comms.get(BaseField::CtsProcessed);
        self.host.msg_new(NewMsg::TickResponse, processed as usize, tick as usize, average as usize, 0)
    }

    /// `ProcessCommunicationsTickMessage`: a new tick handled and answered with the queued inputs (the answer kept);
    /// an old tick answered again (the kept answer when it was that tick's); nothing before the connection's reply.
    fn process_communications_tick_message(&self, m: Msg) {
        let tick = self.host.msg_get(m, MsgField::TickCt) as u32;
        let last = self.comms.get(BaseField::LastTickReceived);
        if last as i32 != -2 {
            let response;
            if tick <= last && last as i32 != -1 {
                let kept = self.last_tick_response.load(SeqCst);
                if kept == 0 || tick != self.host.msg_get(kept, MsgField::ServersTick) as u32 {
                    response = self.new_tick_response(tick);
                } else {
                    response = kept;
                    self.host.msg_reference(response);
                }
            } else {
                self.current_communications_tick.fetch_add(1, SeqCst);
                self.comms.set(BaseField::LastTickReceived, tick);
                self.host.handle_message(m);
                response = self.new_tick_response(tick);
                {
                    let _g = self.outgoing_lock.lock();
                    if !self.host.is_playback_recording_session() {
                        while let Some(input) = self.with_outgoing(|q| q.front().copied()) {
                            if self.host.msg_add_input(response, input) != ERROR_OK {
                                break;
                            }
                            self.with_outgoing(|q| q.pop_front());
                        }
                    }
                }
                let kept = self.last_tick_response.load(SeqCst);
                if kept != 0 {
                    self.host.msg_dereference(kept);
                }
                self.last_tick_response.store(response, SeqCst);
                self.host.msg_reference(response);
            }
            self.host.msg_set_slot(response, true, self.my_send_to_slot.load(SeqCst));
            self.send_a_message(response);
        }
        self.host.msg_dereference(m);
    }

    /// `ProcessMessage`: a message from the server.
    fn process_message(&self, m: Msg) {
        match self.host.msg_type(m) {
            MSG_STOP_GAME => {
                self.host.handle_stop_game();
                let r = self.host.msg_new(NewMsg::StopGameResponse, 0, 0, 0, 0);
                self.send_a_message(r);
            }
            MSG_START_GAME => {
                let offset = self.host.msg_get(m, MsgField::OffsetStartTime) as u32;
                let time = self.host.handle_start_game(offset);
                let r = self.host.msg_new(NewMsg::StartGameResponse, time as usize, 0, 0, 0);
                self.send_a_message(r);
                self.host.handle_message(m);
            }
            MSG_COMMUNICATIONS_TICK => self.process_communications_tick_message(m),
            MSG_CLIENT_CONNECTION_REQUEST_RESPONSE => {
                let slot = self.host.msg_get(m, MsgField::SendToSlot) as u8;
                if slot != SLOT_NONE {
                    self.my_send_to_slot.store(self.host.msg_get(m, MsgField::SendToSlot) as u8, SeqCst);
                    self.connect_for_xfer();
                }
                self.comms.set(BaseField::LastTickReceived, -2i32 as u32);
                self.host.event_set(self.ev(EV_MAIN_END_CONNECT));
                self.host.handle_message(m);
                if self.host.msg_get(m, MsgField::ConnectionError) as i32 == CONNECTION_ACCEPTED {
                    let ct = self.host.msg_get(m, MsgField::ConnectionCt) as u32;
                    self.comms.set(BaseField::LastTickReceived, ct.wrapping_sub(1));
                }
            }
            MSG_WORLD_DATABASE_CHECKSUM => self.host.handle_message(m),
            MSG_SERVER_IS_ALIVE => self.host.msg_dereference(m),
            MSG_CLIENT_RECONNECTION_REQUEST_RESPONSE => {
                let slot = self.host.msg_get(m, MsgField::NewSendToSlot) as u8;
                if slot != SLOT_NONE {
                    self.my_send_to_slot.store(self.host.msg_get(m, MsgField::NewSendToSlot) as u8, SeqCst);
                }
                self.host.handle_message(m);
                let ct = self.host.msg_get(m, MsgField::ReconnectionCt) as u32;
                self.comms.set(BaseField::LastTickReceived, ct.wrapping_sub(1));
            }
            MSG_UNPAUSE_GAME => {
                let id = self.host.msg_get(m, MsgField::UniqueId) as u32;
                let r = self.host.msg_new(NewMsg::UnPauseGameResponse, id as usize, 0, 0, 0);
                self.host.msg_set_slot(r, true, self.my_send_to_slot.load(SeqCst));
                self.send_a_message(r);
                self.comms.set(BaseField::IsGamePaused, 0);
            }
            MSG_PAUSE_GAME => {
                let id = self.host.msg_get(m, MsgField::UniqueId) as u32;
                let r = self.host.msg_new(NewMsg::PauseGameResponse, id as usize, 0, 0, 0);
                self.host.msg_set_slot(r, true, self.my_send_to_slot.load(SeqCst));
                self.send_a_message(r);
                self.comms.set(BaseField::IsGamePaused, 1);
            }
            _ => self.host.handle_message(m),
        }
    }
}
