//! `NEMessageManager` whole (the original NEMessageManager.cpp, LLE 0x100411fb..0x100424c7, and the server's
//! `GetSlowestClientCPU` over its clients): the server's ring of the last 50 sent messages, each with the slots that
//! still owe it an answer; the registered clients' last ten response times, their CPU loads (the last 240) and the
//! lowest communications tick each has reached; and one queue of file transfers per slot, which the server's transfer
//! thread advances one step per pass.
//!
//! Every call on a C++ object (a message, the server, the protocol, a file) is a `Host` call. The original's two
//! critical sections are two re-entrant locks, held where it held them (`LogResponse` and `StoreSentMessage` across
//! their message calls, the transfer pass across its sends and status reports, which may queue another transfer on
//! the same thread). The state the original read and wrote without a lock is read and written under its lock here,
//! never across a call out: no Rust reference lives across a host call.
//!
//! Behaviour kept on the original's undefined paths: `WhoHasNotRespondedNext` from a slot that is not registered
//! answers none (the original stepped past `end()`); a transfer pass whose status report removed the slot's queue or
//! its first transfer stops that slot's step (the original used the freed objects); a transfer's start block holds
//! any file name (the original copied it into a 1 KB stack buffer); a resend of a ring entry that never held a
//! message (the number was set past the stored ones) sends nothing (the original sent NULL). Transfers still queued when the manager is freed
//! release their file specs (the original leaked them).

use std::cell::UnsafeCell;
use std::collections::{BTreeMap, VecDeque};
use std::ops::Bound::{Excluded, Unbounded};

use crate::rlock::RLock;

/// A C++ `NEMessage*` (0: none).
pub type Msg = usize;
/// A C++ `FSFileSpec*`, the transfer's own copy (`Host::file_clone`).
pub type File = usize;

pub const SLOT_SERVER: u8 = 0xfe;
pub const SLOT_NONE: u8 = 0xff;
pub const NONE32: u32 = 0xffff_ffff;
const RING: usize = 50;
const BLOCK: usize = 0x400;

pub const MSG_COMMUNICATIONS_TICK: u8 = 0x26;
pub const MSG_PAUSE_GAME: u8 = 0x33;
pub const MSG_UNPAUSE_GAME: u8 = 0x35;
pub const MSG_SERVER_IS_ALIVE: u8 = 0x3a;

/// The manager's calls out, in the original's terms.
pub trait Host {
    fn msg_reference(&self, m: Msg);
    fn msg_dereference(&self, m: Msg);
    fn msg_type(&self, m: Msg) -> u8;
    /// `sent->IsResponse(resp)`
    fn msg_is_response(&self, sent: Msg, resp: Msg) -> bool;
    fn msg_slot(&self, m: Msg) -> u8;
    /// `ExportMessage`, the protocol's `SendTo(slot, ...)`, the export freed: the send's result (0 = kErrorOK)
    fn msg_send_to(&self, slot: u8, m: Msg) -> u32;
    /// `NEMCommunicationsTick::GetCurrentCommunicationsTick`
    fn msg_tick(&self, m: Msg) -> u32;
    /// The server's answer to its own message `m` of `kind` (tick, pause, unpause), from the server's slot
    fn new_response(&self, kind: u8, m: Msg) -> Msg;
    fn last_tick(&self) -> i32;
    fn set_last_tick(&self, tick: u32);
    fn cts_processed(&self) -> u32;
    fn avg_physics(&self) -> u32;
    fn process_system_message(&self, m: Msg);
    /// `mSendThreadForceCTEvent->Set()`
    fn force_ct(&self);
    fn last_ct_message_id(&self) -> u32;
    /// The server's send-to-all-and-wait number, type, timeout and retries, then `mWaitForResponseEvent->Set()`
    fn start_send_to_all_and_wait(&self, number: u32, kind: u8, timeout: u32, retries: u32);
    /// `TMGetTime`
    fn time(&self) -> u32;
    fn reset_client_connection(&self, slot: u8);
    /// `HandleXferStatus(slot, percent, id, status)`
    fn xfer_status(&self, slot: u8, percent: u8, id: u32, status: u8);
    /// An `NEMXferMessage(status, data)` exported and sent by the protocol's `XferSendTo`: true when it went
    fn xfer_send(&self, slot: u8, status: u8, data: &[u8]) -> bool;
    fn file_clone(&self, spec: usize) -> File;
    fn file_free(&self, f: File);
    fn file_is_open(&self, f: File) -> bool;
    /// `Open(kFileAccessRead) == kErrorOK`
    fn file_open(&self, f: File) -> bool;
    fn file_close(&self, f: File);
    fn file_length(&self, f: File) -> u32;
    /// `Read(buf, 0x400, size) == kErrorOK`, the size read
    fn file_read(&self, f: File, buf: &mut [u8; BLOCK]) -> Option<u32>;
    /// `GetFile()` without its terminating zero
    fn file_name(&self, f: File) -> Vec<u8>;
}

#[derive(Default, Clone)]
struct Sent {
    msg: Msg,
    responded: BTreeMap<u8, bool>,
    time_sent: u32,
    outstanding: u8,
}

struct Client {
    times: [u32; 10],
    avg: u32,
    cursor: usize,
    lowest_ct: u32,
    cpu: u16,
    cpu_history: [u16; 240],
    cpu_cursor: usize,
}

#[derive(Clone, Copy)]
struct Xfer {
    file: File,
    bytes_sent: u32,
    length: u32,
    state: u32,
    id: u32,
    last_status: u32,
    user_data: u32,
    retries: u32,
    slot: u8,
    abort: bool,
}

struct SState {
    sent: Vec<Sent>,
    current: usize,
    clients: BTreeMap<u8, Box<Client>>,
    current_number: u32,
    num_clients: u8,
    force_ct: bool,
}

struct XState {
    xfers: BTreeMap<u8, VecDeque<Xfer>>,
    next_id: u32,
}

pub struct MsgMgr<H: Host> {
    pub host: H,
    s_lock: RLock,
    x_lock: RLock,
    s: UnsafeCell<SState>,
    x: UnsafeCell<XState>,
}

// The state is reached only under its lock and never across a call out (see the module's notes).
unsafe impl<H: Host + Sync> Sync for MsgMgr<H> {}
unsafe impl<H: Host + Send> Send for MsgMgr<H> {}

impl SState {
    /// `GetSentMessage`: the ring entry of a message number, while it is one of the last 50
    fn sent_index(&self, number: u32) -> Option<usize> {
        if number < self.current_number && self.current_number - number <= RING as u32 {
            Some((self.current + RING + 1 - (self.current_number - number) as usize) % RING)
        } else {
            None
        }
    }
}

impl<H: Host> MsgMgr<H> {
    pub fn new(host: H) -> Self {
        MsgMgr {
            host,
            s_lock: RLock::new(),
            x_lock: RLock::new(),
            s: UnsafeCell::new(SState {
                sent: vec![Sent::default(); RING],
                current: 0,
                clients: BTreeMap::new(),
                current_number: 0,
                num_clients: 0,
                force_ct: false,
            }),
            x: UnsafeCell::new(XState { xfers: BTreeMap::new(), next_id: 1 }),
        }
    }

    /// The sent-message state, under its lock, for a closure that makes no call out.
    fn with_s<R>(&self, f: impl FnOnce(&mut SState) -> R) -> R {
        let _g = self.s_lock.lock();
        // SAFETY: the lock is held, and no other borrow is alive: every borrow ends before any call out
        f(unsafe { &mut *self.s.get() })
    }

    fn with_x<R>(&self, f: impl FnOnce(&mut XState) -> R) -> R {
        let _g = self.x_lock.lock();
        // SAFETY: as with_s
        f(unsafe { &mut *self.x.get() })
    }

    /// The destructor: the stored messages released, the queued transfers' file specs freed.
    pub fn release(&self) {
        for i in 0..RING {
            let m = self.with_s(|s| std::mem::take(&mut s.sent[i].msg));
            if m != 0 {
                self.host.msg_dereference(m);
            }
        }
        let files: Vec<File> =
            self.with_x(|x| std::mem::take(&mut x.xfers).into_values().flatten().map(|t| t.file).collect());
        for f in files {
            self.host.file_free(f);
        }
        self.with_s(|s| s.clients.clear());
    }

    pub fn abort_xfer(&self, id: u32) {
        self.with_x(|x| {
            if let Some(t) = x.xfers.values_mut().flat_map(|q| q.iter_mut()).find(|t| t.id == id) {
                t.abort = true;
            }
        });
    }

    /// `AddXfer`: the new transfer's id, or None for a slot with no queue (kErrorNotFound)
    pub fn add_xfer(&self, spec: usize, user_data: u32, slot: u8) -> Option<u32> {
        let _g = self.x_lock.lock();
        if !self.with_x(|x| x.xfers.contains_key(&slot)) {
            return None;
        }
        let file = self.host.file_clone(spec);
        self.with_x(|x| {
            let id = x.next_id;
            x.next_id = id.wrapping_add(1);
            let t = Xfer {
                file,
                bytes_sent: 0,
                length: 0,
                state: 0,
                id,
                last_status: 0,
                user_data,
                retries: 0,
                slot,
                abort: false,
            };
            match x.xfers.get_mut(&slot) {
                Some(q) => {
                    q.push_back(t);
                    Some(id)
                }
                None => None, // the file clone's call removed the queue
            }
        })
    }

    pub fn highest_cpu(&self) -> u16 {
        self.with_s(|s| s.clients.values().map(|c| c.cpu).fold(0, u16::max))
    }

    pub fn highest_response_time(&self) -> u32 {
        self.with_s(|s| s.clients.values().map(|c| c.avg).fold(0, u32::max))
    }

    pub fn lowest_ct(&self) -> u32 {
        self.with_s(|s| s.clients.values().map(|c| c.lowest_ct).fold(0xffff_fffe, u32::min))
    }

    pub fn slowest_responder(&self) -> u8 {
        self.with_s(|s| {
            let (mut highest, mut slowest) = (0u32, SLOT_NONE);
            for (slot, c) in &s.clients {
                if c.avg > highest {
                    slowest = *slot;
                    highest = c.avg;
                }
            }
            slowest
        })
    }

    /// The server's `GetSlowestClientCPU`: the client with the highest CPU load over its history (ties: the higher
    /// slot), none when no client has any
    pub fn slowest_client_cpu(&self) -> u8 {
        self.with_s(|s| {
            let (mut highest, mut slowest) = (0u32, SLOT_NONE);
            for (slot, c) in &s.clients {
                let total = c.cpu_history.iter().fold(0u32, |a, v| a.wrapping_add(*v as u32));
                if total > 0 && total >= highest {
                    highest = total;
                    slowest = *slot;
                }
            }
            slowest
        })
    }

    /// `HandleSentMessage`: a message the server sent itself
    fn handle_sent(&self, m: Msg) {
        let kind = self.host.msg_type(m);
        match kind {
            MSG_COMMUNICATIONS_TICK => {
                let tick = self.host.msg_tick(m);
                let last = self.host.last_tick();
                if last == -2 || tick > last as u32 {
                    let ct = self.host.cts_processed().wrapping_add(1);
                    let cpu = self.host.avg_physics() as u16;
                    self.log_response_time(SLOT_SERVER, ct, cpu);
                    self.host.process_system_message(m);
                    self.host.set_last_tick(tick);
                }
                self.respond(kind, m);
            }
            MSG_UNPAUSE_GAME | MSG_PAUSE_GAME => self.respond(kind, m),
            MSG_SERVER_IS_ALIVE => {}
            _ => self.host.process_system_message(m),
        }
    }

    fn respond(&self, kind: u8, m: Msg) {
        let r = self.host.new_response(kind, m);
        self.log_response(r);
        self.host.msg_dereference(r);
    }

    /// `LogResponse`: the first stored message the response answers, from a slot that still owed it, is marked
    pub fn log_response(&self, resp: Msg) {
        let _g = self.s_lock.lock();
        let slot = self.host.msg_slot(resp);
        for i in 0..RING {
            let sent = self.with_s(|s| s.sent[i].msg);
            if sent == 0 || !self.host.msg_is_response(sent, resp) {
                continue;
            }
            let marked = self.with_s(|s| {
                let force = s.force_ct;
                let e = &mut s.sent[i];
                match e.responded.get_mut(&slot) {
                    Some(v) if !*v => {
                        *v = true;
                        e.outstanding = e.outstanding.wrapping_sub(1);
                        Some(force && e.outstanding == 0)
                    }
                    _ => None,
                }
            });
            if let Some(force) = marked {
                if force {
                    self.host.force_ct();
                }
                break;
            }
        }
    }

    /// `LogResponseTime`: a client's time to answer the last tick, its CPU load and the tick it reached; every
    /// tenth answer recomputes the average (of the ten before it)
    pub fn log_response_time(&self, slot: u8, ct: u32, cpu: u16) {
        let known = self.with_s(|s| match s.clients.get_mut(&slot) {
            None => false,
            Some(c) => {
                if c.cursor == 9 {
                    c.cursor = 0;
                    c.avg = c.times.iter().fold(0u32, |a, v| a.wrapping_add(*v)) / 10;
                } else {
                    c.cursor += 1;
                }
                c.cpu_cursor = if c.cpu_cursor == 239 { 0 } else { c.cpu_cursor + 1 };
                true
            }
        });
        if !known {
            return;
        }
        let id = self.host.last_ct_message_id();
        let sent_at = self.with_s(|s| s.sent_index(id).map(|i| s.sent[i].time_sent));
        let time = sent_at.map(|t| if slot == SLOT_SERVER { 0 } else { self.host.time().wrapping_sub(t) });
        self.with_s(|s| {
            if let Some(c) = s.clients.get_mut(&slot) {
                if let Some(time) = time {
                    c.times[c.cursor] = time;
                    c.cpu_history[c.cpu_cursor] = cpu;
                }
                c.lowest_ct = ct;
                c.cpu = cpu;
            }
        });
    }

    /// `NumberOfOutstandingResponses`: the slots that have not answered (the stored count corrected); 0 once gone
    pub fn outstanding(&self, number: u32) -> u8 {
        self.with_s(|s| match s.sent_index(number) {
            None => 0,
            Some(i) => {
                let e = &mut s.sent[i];
                let count = e.responded.values().filter(|v| !**v).count() as u8;
                e.outstanding = count;
                count
            }
        })
    }

    /// The first transfer of a slot's queue when it is still the one with `id`
    fn front_is(&self, slot: u8, id: u32) -> bool {
        self.with_x(|x| x.xfers.get(&slot).and_then(|q| q.front()).is_some_and(|t| t.id == id))
    }

    fn update_front(&self, slot: u8, id: u32, f: impl FnOnce(&mut Xfer)) {
        self.with_x(|x| {
            if let Some(t) = x.xfers.get_mut(&slot).and_then(|q| q.front_mut()).filter(|t| t.id == id) {
                f(t);
            }
        });
    }

    /// The transfer `id` leaves its queue and its file spec is freed (`delete theXfer`); closed first if `close`
    fn finish(&self, slot: u8, id: u32, close: bool) {
        let t = self.with_x(|x| {
            let q = x.xfers.get_mut(&slot)?;
            let at = q.iter().position(|t| t.id == id)?;
            q.remove(at)
        });
        if let Some(t) = t {
            if close {
                self.host.file_close(t.file);
            }
            self.host.file_free(t.file);
        }
    }

    /// The end block after a failed or short read: the end sent, the status reported, the transfer done
    fn end_xfer(&self, t: &Xfer, status: u8) {
        self.host.xfer_send(t.slot, 2, &[]);
        self.host.xfer_status(t.slot, 100, t.id, status);
        if self.front_is(t.slot, t.id) {
            self.finish(t.slot, t.id, true);
        }
    }

    /// `ProcessXfers`: each slot's first transfer one step (abort, start, a 1 KB block, end); true if any was queued
    pub fn process_xfers(&self) -> bool {
        let _g = self.x_lock.lock();
        let mut work = false;
        let mut after: Option<u8> = None;
        loop {
            let next = self.with_x(|x| {
                let mut r = match after {
                    None => x.xfers.range(..),
                    Some(k) => x.xfers.range((Excluded(k), Unbounded)),
                };
                r.next().map(|(k, q)| (*k, q.front().copied()))
            });
            let Some((slot, front)) = next else { break };
            after = Some(slot);
            let Some(t) = front else { continue };
            work = true;
            if t.abort {
                self.host.xfer_send(slot, 3, &[]);
                self.host.xfer_status(slot, 100, t.id, 3);
                if self.front_is(slot, t.id) {
                    self.finish(slot, t.id, true);
                }
                continue;
            }
            match t.state {
                0 => self.start_xfer(t),
                1 => self.send_block(t),
                _ => {}
            }
        }
        work
    }

    fn start_xfer(&self, t: Xfer) {
        if self.host.file_is_open(t.file) {
            self.host.file_close(t.file);
        }
        if !self.host.file_open(t.file) {
            self.finish(t.slot, t.id, false);
            return;
        }
        let length = self.host.file_length(t.file);
        self.update_front(t.slot, t.id, |x| {
            x.state = 1;
            x.bytes_sent = 0;
            x.length = length;
        });
        let mut block = Vec::with_capacity(BLOCK);
        block.extend_from_slice(&t.id.to_le_bytes());
        block.extend_from_slice(&t.user_data.to_le_bytes());
        block.extend_from_slice(&self.host.file_name(t.file));
        block.push(0);
        if self.host.xfer_send(t.slot, 0, &block) {
            self.host.xfer_status(t.slot, 0, t.id, 0);
        } else {
            self.update_front(t.slot, t.id, |x| {
                let tries = x.retries;
                x.retries = tries.wrapping_add(1);
                if tries >= 1000 {
                    x.abort = true;
                }
                x.state = 0;
            });
        }
    }

    fn send_block(&self, t: Xfer) {
        let mut buf = [0u8; BLOCK];
        let Some(read) = self.host.file_read(t.file, &mut buf) else {
            self.end_xfer(&t, 4);
            return;
        };
        let read = read.min(BLOCK as u32);
        let sent = t.bytes_sent.wrapping_add(read);
        self.update_front(t.slot, t.id, |x| x.bytes_sent = sent);
        if !self.host.xfer_send(t.slot, 1, &buf[..read as usize]) {
            self.end_xfer(&t, 4);
            return;
        }
        if (read as usize) < BLOCK {
            self.end_xfer(&t, 2);
            return;
        }
        let now = self.host.time();
        if now.wrapping_sub(t.last_status) > 2000 {
            let now = self.host.time();
            self.update_front(t.slot, t.id, |x| x.last_status = now);
            // (unsigned char)((float)sent / length * 100.0f), clamped to 100
            let mut percent = ((sent as f32 / t.length as f32 * 100.0f32) as i32) as u8;
            if percent > 100 {
                percent = 100;
            }
            self.host.xfer_status(t.slot, percent, t.id, 1);
        }
    }

    /// `RegisterSlot`: the slot added to every sent message (as owing nothing), a client record and a transfer
    /// queue; false when it was registered already
    pub fn register_slot(&self, slot: u8) -> bool {
        let added = self.with_s(|s| {
            if s.sent[0].responded.contains_key(&slot) {
                return false;
            }
            for e in s.sent.iter_mut() {
                e.responded.entry(slot).or_insert(false);
            }
            s.clients.entry(slot).or_insert_with(|| {
                Box::new(Client {
                    times: [0; 10],
                    avg: 0,
                    cursor: 9,
                    lowest_ct: 0xffff_fffe,
                    cpu: 0,
                    cpu_history: [0; 240],
                    cpu_cursor: 239,
                })
            });
            true
        });
        if !added {
            return false;
        }
        self.with_x(|x| {
            x.xfers.entry(slot).or_default();
        });
        self.with_s(|s| s.num_clients = s.num_clients.wrapping_add(1));
        true
    }

    /// `Resend`: the stored message again to every slot that has not answered it; false once it is gone
    pub fn resend(&self, number: u32) -> bool {
        let Some(i) = self.with_s(|s| s.sent_index(number)) else { return false };
        let mut after: Option<u8> = None;
        loop {
            let next = self.with_s(|s| {
                let e = &s.sent[i];
                let mut r = match after {
                    None => e.responded.range(..),
                    Some(k) => e.responded.range((Excluded(k), Unbounded)),
                };
                r.find(|(_, v)| !**v).map(|(k, _)| (*k, e.msg))
            });
            let Some((slot, m)) = next else { break };
            after = Some(slot);
            if m == 0 {
                // an entry that never held a message (the number was set past the stored ones): the original sent NULL
                break;
            }
            self.send_to(m, slot, false);
        }
        true
    }

    /// `Send`: to every client but the server (stored first when guaranteed), then handled as the server's own; the
    /// stored number, or none. With no client registered the message is released.
    pub fn send(&self, m: Msg, guaranteed: bool) -> u32 {
        if self.with_s(|s| s.num_clients) == 0 {
            self.host.msg_dereference(m);
            return NONE32;
        }
        let number = if guaranteed { self.store(m, SLOT_NONE, true) } else { NONE32 };
        let mut after: Option<u8> = None;
        loop {
            let next = self.with_s(|s| {
                let e = &s.sent[s.current];
                let mut r = match after {
                    None => e.responded.range(..),
                    Some(k) => e.responded.range((Excluded(k), Unbounded)),
                };
                r.next().map(|(k, _)| *k)
            });
            let Some(slot) = next else { break };
            after = Some(slot);
            if slot != SLOT_SERVER {
                self.host.msg_send_to(slot, m);
            }
        }
        self.handle_sent(m);
        number
    }

    /// `SendTo`: one registered slot (the server's own handled at once); the stored number, the send's error, or
    /// none for an unknown slot
    pub fn send_to(&self, m: Msg, slot: u8, guaranteed: bool) -> u32 {
        let known = self.with_s(|s| s.sent[s.current].responded.contains_key(&slot));
        if !known {
            return NONE32;
        }
        if slot == SLOT_SERVER {
            self.handle_sent(m);
            self.store(m, slot, guaranteed)
        } else {
            let r = self.host.msg_send_to(slot, m);
            if r == 0 {
                self.store(m, slot, guaranteed)
            } else {
                r
            }
        }
    }

    /// `SendToAllAndWait`: sent to all and stored; the server then waits for every answer
    pub fn send_to_all_and_wait(&self, m: Msg, timeout: u32, retries: u32) -> u32 {
        let number = self.send(m, true);
        if number != NONE32 {
            let kind = self.host.msg_type(m);
            self.with_s(|s| s.force_ct = true);
            self.host.start_send_to_all_and_wait(number, kind, timeout, retries);
        }
        number
    }

    pub fn set_lowest_cts(&self, ct: u32) {
        self.with_s(|s| s.clients.values_mut().for_each(|c| c.lowest_ct = ct));
    }

    /// `StoreSentMessage`: the next ring entry takes the message; which slots owe an answer; its number
    fn store(&self, m: Msg, slot: u8, guaranteed: bool) -> u32 {
        let _g = self.s_lock.lock();
        let old = self.with_s(|s| {
            s.current = if s.current == RING - 1 { 0 } else { s.current + 1 };
            s.sent[s.current].msg
        });
        if old != 0 {
            self.host.msg_dereference(old);
        }
        self.host.msg_reference(m);
        let time = if guaranteed { self.host.time() } else { 0 };
        self.with_s(|s| {
            let num_clients = s.num_clients;
            let e = &mut s.sent[s.current];
            e.msg = m;
            let owed = slot == SLOT_NONE && guaranteed;
            e.responded.values_mut().for_each(|v| *v = !owed);
            if guaranteed {
                if slot != SLOT_NONE {
                    if let Some(v) = e.responded.get_mut(&slot) {
                        *v = false;
                    }
                    e.outstanding = 1;
                } else {
                    e.outstanding = num_clients;
                }
            } else {
                e.outstanding = 0;
            }
            e.time_sent = time;
            let number = s.current_number;
            s.current_number = number.wrapping_add(1);
            number
        })
    }

    /// `UnregisterSlot`: the slot leaves the sent messages (an answer it owed no longer counted), the clients and
    /// the transfers (its queued transfers closed and dropped), and its connection is reset
    pub fn unregister_slot(&self, slot: u8) {
        let known = self.with_s(|s| {
            if !s.sent[0].responded.contains_key(&slot) {
                return false;
            }
            for e in s.sent.iter_mut() {
                if let Some(owed) = e.responded.remove(&slot) {
                    if !owed && e.outstanding != 0 {
                        e.outstanding -= 1;
                    }
                }
            }
            s.clients.remove(&slot);
            true
        });
        if !known {
            return;
        }
        {
            let _g = self.x_lock.lock();
            loop {
                let t = self.with_x(|x| x.xfers.get_mut(&slot).and_then(|q| q.pop_front()));
                let Some(t) = t else { break };
                self.host.file_close(t.file);
                self.host.file_free(t.file);
            }
            self.with_x(|x| x.xfers.remove(&slot));
        }
        self.with_s(|s| s.num_clients = s.num_clients.wrapping_sub(1));
        self.host.reset_client_connection(slot);
    }

    pub fn who_has_not_responded(&self, number: u32) -> u8 {
        self.with_s(|s| {
            s.sent_index(number)
                .and_then(|i| s.sent[i].responded.iter().find(|(_, v)| !**v).map(|(k, _)| *k))
                .unwrap_or(SLOT_NONE)
        })
    }

    pub fn who_has_not_responded_next(&self, previous: u8, number: u32) -> u8 {
        self.with_s(|s| {
            s.sent_index(number)
                .filter(|i| s.sent[*i].responded.contains_key(&previous))
                .and_then(|i| {
                    s.sent[i].responded.range((Excluded(previous), Unbounded)).find(|(_, v)| !**v).map(|(k, _)| *k)
                })
                .unwrap_or(SLOT_NONE)
        })
    }

    pub fn num_clients(&self) -> u8 {
        self.with_s(|s| s.num_clients)
    }

    pub fn current_number(&self) -> u32 {
        self.with_s(|s| s.current_number)
    }

    pub fn set_current_number(&self, n: u32) {
        self.with_s(|s| s.current_number = n);
    }

    pub fn set_force_ct(&self, on: bool) {
        self.with_s(|s| s.force_ct = on);
    }
}
