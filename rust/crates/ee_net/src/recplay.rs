//! `NERecordPlayback` whole (the original NERecordPlayback.cpp, LLE 0x1004555d..0x10045c00, with its inline
//! members): the message stream between the communications and the game, passed straight on (the game's only
//! mode), recorded to a file with timestamps, or played back from one by the playback thread ("CommRecPlay") at the
//! recorded pace (45 times slower while slow playback is on).
//!
//! The record file: the start time (4 bytes), then per message `[time 4][size 4][message]` (`export_timestamped`).
//! The network targets were never written ("Functionality not implemented yet"): they fail the original's
//! verification and only switch the mode.
//!
//! The fields the playback thread shares with the caller's thread are atomics, as the original shared them without
//! a lock. Kept as the original did: a live message passed while playing back is dropped without being given back;
//! a message read by the playback thread when it is told to quit is never given back.
//! Changed on paths where the original crashed: `PlaybackComplete`, `SetSlowPlayback` and `SetNormalPlayback`
//! without the playback thread's events (a network playback, or no playback) answer false / do nothing.

use std::sync::atomic::{AtomicU32, AtomicU8, AtomicUsize, Ordering::SeqCst};

use crate::qhost::*;

pub const MODE_PASSING: u8 = 1;
pub const MODE_RECORDING: u8 = 2;
pub const MODE_PLAYING: u8 = 3;
pub const TARGET_FILE: u8 = 1;
pub const TARGET_NETWORK: u8 = 2;
const UNIT: u32 = 1;
const SLOW_FACTOR: i32 = 45;

pub struct RecPlay<H: QHost> {
    pub host: H,
    thread: AtomicUsize,
    quit: AtomicUsize,
    exiting: AtomicUsize,
    slow_toggle: AtomicUsize,
    normal_toggle: AtomicUsize,
    record_file: AtomicUsize,
    playback_file: AtomicUsize,
    start_time: AtomicU32,
    mode: AtomicU8,
    record_target: AtomicU8,
    playback_target: AtomicU8,
}

impl<H: QHost> RecPlay<H> {
    /// The constructor: passing messages on, both targets the file.
    pub fn new(host: H) -> Self {
        RecPlay {
            host,
            thread: AtomicUsize::new(0),
            quit: AtomicUsize::new(0),
            exiting: AtomicUsize::new(0),
            slow_toggle: AtomicUsize::new(0),
            normal_toggle: AtomicUsize::new(0),
            record_file: AtomicUsize::new(0),
            playback_file: AtomicUsize::new(0),
            start_time: AtomicU32::new(0),
            mode: AtomicU8::new(MODE_PASSING),
            record_target: AtomicU8::new(TARGET_FILE),
            playback_target: AtomicU8::new(TARGET_FILE),
        }
    }

    fn verify(&self, ok: bool, line: u32) {
        if !ok {
            self.host.verify(UNIT, line);
        }
    }

    /// The destructor's work: back to passing (the playback stopped, the files closed).
    pub fn release(&self) {
        self.switch_modes(MODE_PASSING);
        let f = self.record_file.load(SeqCst);
        if f != 0 {
            self.host.file_close(f);
        }
    }

    pub fn mode(&self) -> u8 {
        self.mode.load(SeqCst)
    }

    pub fn set_slow_playback(&self) {
        let e = self.slow_toggle.load(SeqCst);
        if e != 0 {
            self.host.event_set(e);
        }
    }

    pub fn set_normal_playback(&self) {
        let e = self.normal_toggle.load(SeqCst);
        if e != 0 {
            self.host.event_set(e);
        }
    }

    /// `PlaybackComplete`: whether the playback thread has finished.
    pub fn playback_complete(&self) -> bool {
        self.verify(self.mode() == MODE_PLAYING, 86);
        let e = self.exiting.load(SeqCst);
        e != 0 && self.host.event_wait(e, 0) == ERROR_OK
    }

    /// The playback thread's body (`PlaybackThreadFunction`).
    pub fn playback_thread(&self) -> u32 {
        let mut slow = false;
        let mut record_start = 0u32;
        let mut size = 0u32;
        let mut message_time = 0u32;
        let mut keep_going = true;
        let file = self.playback_file.load(SeqCst);
        self.host.fpu_start();
        let start = self.host.time();
        match self.playback_target.load(SeqCst) {
            TARGET_FILE => {
                let r = self.host.file_read_u32(file, &mut record_start);
                self.verify(r == ERROR_OK, 142);
            }
            TARGET_NETWORK => self.host.verify(UNIT, 146),
            _ => self.host.verify(UNIT, 150),
        }
        while keep_going {
            self.host.fpu_check();
            let mut payload: Option<Vec<u8>> = None;
            match self.playback_target.load(SeqCst) {
                TARGET_FILE => {
                    if self.host.file_read_u32(file, &mut message_time) != ERROR_OK {
                        self.host.event_set(self.exiting.load(SeqCst));
                        return 0;
                    }
                    let r = self.host.file_read_u32(file, &mut size);
                    self.verify(r == ERROR_OK, 180);
                    let mut buf = vec![0u8; size as usize];
                    let r = self.host.file_read(file, &mut buf);
                    self.verify(r == ERROR_OK, 185);
                    payload = Some(buf);
                }
                TARGET_NETWORK => self.host.verify(UNIT, 189),
                _ => self.host.verify(UNIT, 193),
            }
            let m = self.host.msg_from_bytes(payload.as_deref());
            drop(payload);
            // the original's unsigned 32-bit sum taken as a signed delay
            let mut delay =
                message_time.wrapping_sub(record_start).wrapping_add(start).wrapping_sub(self.host.time()) as i32;
            if delay < 0 {
                delay = 0;
            }
            if slow {
                delay = delay.wrapping_mul(SLOW_FACTOR);
            }
            while delay > 0 {
                if self.host.event_wait(self.quit.load(SeqCst), 10) == ERROR_OK {
                    keep_going = false;
                    break;
                }
                delay = delay.wrapping_sub(20);
                if self.host.event_wait(self.slow_toggle.load(SeqCst), 10) == ERROR_OK && !slow {
                    slow = true;
                    delay = delay.wrapping_mul(SLOW_FACTOR);
                }
                delay = delay.wrapping_sub(10);
                if self.host.event_wait(self.normal_toggle.load(SeqCst), 10) == ERROR_OK && slow {
                    slow = false;
                    delay = delay / SLOW_FACTOR + 1;
                }
            }
            if delay <= 0 && m != 0 {
                self.host.deliver(DELIVER_RECORD_PLAYBACK, m);
            }
        }
        self.host.event_set(self.exiting.load(SeqCst));
        0
    }

    /// `Process`: a message passed to the callback; while recording, first written with the time to the record file
    /// (referenced across the callback, as the timestamped message held it). While playing back it is dropped.
    pub fn process(&self, m: Msg) {
        match self.mode() {
            MODE_PASSING => {
                self.host.deliver(DELIVER_RECORD_PLAYBACK, m);
            }
            MODE_RECORDING => {
                let time = self.host.time();
                let data = export_timestamped(&self.host, m, time);
                match self.record_target.load(SeqCst) {
                    TARGET_FILE => {
                        self.host.file_write(self.record_file.load(SeqCst), &data);
                    }
                    TARGET_NETWORK => self.host.verify(UNIT, 308),
                    _ => self.host.verify(UNIT, 312),
                }
                self.host.msg_reference(m);
                self.host.deliver(DELIVER_RECORD_PLAYBACK, m);
                self.host.msg_dereference(m);
            }
            _ => {}
        }
    }

    /// `PassMode`
    pub fn pass_mode(&self) -> i32 {
        self.switch_modes(MODE_PASSING);
        ERROR_OK
    }

    /// `RecordMode(FSFileSpec*)`: the file made anew and opened, the start time its first word; the write's result.
    pub fn record_mode_file(&self, f: FileH) -> i32 {
        self.switch_modes(MODE_RECORDING);
        if self.host.file_create(f) == ERROR_ALREADY_EXISTS {
            let r = self.host.file_delete(f);
            self.verify(r == ERROR_OK, 368);
            let r = self.host.file_create(f);
            self.verify(r == ERROR_OK, 369);
        }
        let r = self.host.file_open(f, ACCESS_WRITE);
        self.verify(r == ERROR_OK, 371);
        self.record_file.store(f, SeqCst);
        self.record_target.store(TARGET_FILE, SeqCst);
        let t = self.host.time();
        self.start_time.store(t, SeqCst);
        self.host.file_write_u32(f, t)
    }

    /// `RecordMode(address, port)`: never written; fails the verification, then switches.
    pub fn record_mode_network(&self) -> i32 {
        self.host.verify(UNIT, 396);
        self.switch_modes(MODE_RECORDING);
        self.record_target.store(TARGET_NETWORK, SeqCst);
        self.start_time.store(self.host.time(), SeqCst);
        ERROR_OK
    }

    /// `PlaybackMode(FSFileSpec*)`: the file opened, four events made and the playback thread started (`me` is this
    /// object as its argument) and named; the thread's creation result.
    pub fn playback_mode_file(&self, me: usize, f: FileH) -> i32 {
        self.switch_modes(MODE_PLAYING);
        let r = self.host.file_open(f, ACCESS_READ);
        self.verify(r == ERROR_OK, 428);
        self.playback_file.store(f, SeqCst);
        self.playback_target.store(TARGET_FILE, SeqCst);
        for (slot, line) in
            [(&self.quit, 435), (&self.exiting, 436), (&self.slow_toggle, 437), (&self.normal_toggle, 438)]
        {
            let e = self.host.event_new();
            slot.store(e, SeqCst);
            self.verify(e != 0, line);
        }
        let mut t = 0;
        let result = self.host.thread_new(THREAD_PLAYBACK, me, &mut t);
        self.thread.store(t, SeqCst);
        self.host.thread_name(t, "CommRecPlay");
        result
    }

    /// `PlaybackMode(address, port)`: never written; fails the verification, then switches.
    pub fn playback_mode_network(&self) -> i32 {
        self.host.verify(UNIT, 463);
        self.switch_modes(MODE_PLAYING);
        self.playback_target.store(TARGET_NETWORK, SeqCst);
        ERROR_OK
    }

    /// `SwitchModes`: the old mode's playback thread stopped (told to quit unless it has finished) and freed with
    /// its events, its file closed.
    fn switch_modes(&self, mode: u8) {
        match self.mode() {
            MODE_PLAYING => {
                if self.thread.load(SeqCst) != 0 {
                    if !self.playback_complete() {
                        self.host.event_set(self.quit.load(SeqCst));
                        self.host.event_wait(self.exiting.load(SeqCst), WAIT_INFINITE);
                    }
                    self.host.thread_free(self.thread.load(SeqCst));
                    for e in [&self.quit, &self.exiting, &self.slow_toggle, &self.normal_toggle] {
                        self.host.event_free(e.load(SeqCst));
                    }
                    self.thread.store(0, SeqCst);
                    for e in [&self.quit, &self.exiting, &self.slow_toggle, &self.normal_toggle] {
                        e.store(0, SeqCst);
                    }
                }
                let f = self.playback_file.swap(0, SeqCst);
                if f != 0 {
                    self.host.file_close(f);
                }
            }
            MODE_RECORDING => {
                let f = self.record_file.swap(0, SeqCst);
                if f != 0 {
                    self.host.file_close(f);
                }
            }
            _ => {}
        }
        self.mode.store(mode, SeqCst);
    }
}
