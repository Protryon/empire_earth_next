//! The lock-step server's tuning and booting (the original NESynchronousServer.cpp's TuneServer, TuneForSlowNetwork,
//! TuneForSlowCPU, BootAllLaggers, BootTheSlowestResponder, BootTheSlowestClientCPU and GetSlowestClientCPU, LLE
//! 0x10047e85..0x1004969a), with the state only they kept: the network tuning's state machine and counters, the slow
//! CPU count, the laggers to report and the lag flags, the send interval's decrement. The C++ server keeps its other
//! fields (the send interval, the rates, the game state), its message manager and the game's callbacks: each is a
//! host op, in the original's order.
//!
//! The rate arithmetic is the original's f32 (`1000.0f / (cpu * factor)`, `pts / (1000.0f / interval)`), and its
//! conversions are the x86-64 code clang makes for them: a float to `unsigned char` truncates through a 32-bit
//! `cvttss2si` (out of range: 0x80000000, whose low byte is 0), a float to `unsigned long` through a 64-bit one, the
//! low 32 bits kept. The interval sums and differences wrap at 32 bits as the original's `unsigned long`s.
//!
//! The members run on the send thread under its locks (SendCommunicationsTick), or on the main thread; the state is
//! atomics and a mutex never held across a call out (the original shared its fields without a lock).

use std::sync::atomic::{AtomicBool, AtomicU32, Ordering::SeqCst};
use std::sync::Mutex;

pub use crate::net::ffi_tune::EeNetTuField as Field;

pub const SLOT_SERVER: u8 = 0xfe;
pub const SLOT_NONE: u8 = 0xff;
pub const STATE_OPEN: u8 = 0;
pub const STATE_CLOSED: u8 = 1;
pub const STATE_RUNNING: u8 = 3;

/// The events the members set
pub const EVENT_CHANGE_SEND_INTERVAL: u32 = 0;
pub const EVENT_PAUSE: u32 = 1;
pub const EVENT_UNPAUSE: u32 = 2;

pub trait TuneHost {
    /// a field of the server (`Field::CostFactor`: the f32's bits; `CurrentCommunicationsTick`: the virtual getter)
    fn get(&self, f: Field) -> u32;
    /// SendInterval, PTsPerSecond, PTsPerCt
    fn set(&self, f: Field, value: u32);
    fn event_set(&self, which: u32);
    // the message manager (ee_net msgmgr.rs, through the server's pointer)
    fn mm_outstanding(&self, number: u32) -> u8;
    fn mm_highest_response_time(&self) -> u32;
    fn mm_highest_cpu(&self) -> u16;
    fn mm_lowest_ct(&self) -> u32;
    fn mm_resend(&self, number: u32);
    fn mm_who(&self, number: u32) -> u8;
    fn mm_who_next(&self, slot: u8, number: u32) -> u8;
    fn mm_num_clients(&self) -> u8;
    fn mm_slowest_responder(&self) -> u8;
    fn mm_slowest_client_cpu(&self) -> u8;
    fn mm_set_force_ct(&self, on: bool);
    // the server's members and the game's callbacks
    fn unregister_slot(&self, slot: u8);
    fn before_removing_players(&self);
    fn boot_player(&self, slot: u8, was_booted: bool, game_is_exiting: bool);
    fn report_laggers(&self, slots: &[u8], warning: bool);
    /// `ProcessLocalUserInput(new NEMRemovePlayer(slot, true, false, false), kSlotNone)`
    fn remove_player_input(&self, slot: u8);
}

/// `(unsigned char)` of a float: x86-64's 32-bit truncation, the low byte kept
pub fn f32_to_u8(x: f32) -> u8 {
    let v = if x.is_nan() || !(-2147483648.0..2147483648.0).contains(&x) { i32::MIN } else { x as i32 };
    v as u8
}

/// `(unsigned long)` of a float: x86-64's 64-bit truncation, the low 32 bits kept
pub fn f32_to_u32(x: f32) -> u32 {
    let v = if x.is_nan() || !(-9.223372e18..9.223372e18).contains(&x) { i64::MIN } else { x as i64 };
    v as u32
}

pub struct Tune<H: TuneHost> {
    pub host: H,
    net_tune_state: AtomicU32,
    consecutive_late: AtomicU32,
    resend_count: AtomicU32,
    late_episodes: AtomicU32,
    at_max_count: AtomicU32,
    highest_response_time: AtomicU32,
    late_count: AtomicU32,
    slow_cpu_count: AtomicU32,
    send_interval_decrement: AtomicU32,
    lag_warning: AtomicBool,
    lag_report_active: AtomicBool,
    /// the laggers to report (`mSlotsD0`)
    slots: Mutex<Vec<u8>>,
}

impl<H: TuneHost> Tune<H> {
    /// The constructor's part: every count 0, the decrement the server was given.
    pub fn new(host: H, send_interval_decrement: u32) -> Self {
        Tune {
            host,
            net_tune_state: AtomicU32::new(0),
            consecutive_late: AtomicU32::new(0),
            resend_count: AtomicU32::new(0),
            late_episodes: AtomicU32::new(0),
            at_max_count: AtomicU32::new(0),
            highest_response_time: AtomicU32::new(0),
            late_count: AtomicU32::new(0),
            slow_cpu_count: AtomicU32::new(0),
            send_interval_decrement: AtomicU32::new(send_interval_decrement),
            lag_warning: AtomicBool::new(false),
            lag_report_active: AtomicBool::new(false),
            slots: Mutex::new(Vec::new()),
        }
    }

    /// The state, for the tests: tune state, consecutive late, resends, late episodes, at max, highest response
    /// time, late count, slow CPU count, decrement, lag warning, report active, laggers queued.
    pub fn state(&self) -> [u32; 12] {
        [
            self.net_tune_state.load(SeqCst),
            self.consecutive_late.load(SeqCst),
            self.resend_count.load(SeqCst),
            self.late_episodes.load(SeqCst),
            self.at_max_count.load(SeqCst),
            self.highest_response_time.load(SeqCst),
            self.late_count.load(SeqCst),
            self.slow_cpu_count.load(SeqCst),
            self.send_interval_decrement.load(SeqCst),
            self.lag_warning.load(SeqCst) as u32,
            self.lag_report_active.load(SeqCst) as u32,
            self.slots().len() as u32,
        ]
    }

    fn slots(&self) -> std::sync::MutexGuard<'_, Vec<u8>> {
        self.slots.lock().unwrap_or_else(|e| e.into_inner())
    }
    fn push_slot(&self, slot: u8) {
        self.slots().push(slot);
    }

    /// The PTs per CT of the rate and the send interval (0xfe from 0xff up), set on the server.
    fn set_pts_per_ct(&self) {
        let pts_per_second = self.host.get(Field::PTsPerSecond) as u8;
        let interval = self.host.get(Field::SendInterval);
        let pts = f32_to_u32(pts_per_second as f32 / (1000.0f32 / interval as f32));
        self.host.set(Field::PTsPerCt, if pts < 0xff { pts & 0xff } else { 0xfe });
    }

    fn set_send_interval(&self, interval: u32) {
        self.host.set(Field::SendInterval, interval);
        self.host.event_set(EVENT_CHANGE_SEND_INTERVAL);
    }

    /// `GetSlowestClientCPU`
    pub fn slowest_client_cpu(&self) -> u8 {
        self.host.mm_slowest_client_cpu()
    }

    /// `BootAllLaggers`: every client that has not answered the message booted, the game paused around it when
    /// running.
    pub fn boot_all_laggers(&self, number: u32) {
        if self.host.get(Field::GameState) as u8 == STATE_RUNNING {
            self.host.event_set(EVENT_PAUSE);
            self.host.before_removing_players();
            self.host.event_set(EVENT_UNPAUSE);
        }
        // (the original's array of mm_num_clients() entries: a list here)
        self.host.mm_num_clients();
        let mut laggers = Vec::new();
        let mut slot = self.host.mm_who(number);
        if slot != SLOT_NONE {
            laggers.push(slot);
        }
        loop {
            slot = self.host.mm_who_next(slot, number);
            if slot == SLOT_NONE {
                break;
            }
            laggers.push(slot);
        }
        for s in laggers {
            self.host.boot_player(s, true, false);
        }
    }

    /// `BootTheSlowestClientCPU`: in a multiplayer game, the slowest client's slot unregistered and its removal
    /// announced.
    pub fn boot_the_slowest_client_cpu(&self) {
        if self.host.get(Field::IsMultiplayerSession) == 0 {
            return;
        }
        let slot = self.slowest_client_cpu();
        if slot == SLOT_NONE || slot == SLOT_SERVER {
            return;
        }
        self.host.unregister_slot(slot);
        self.host.remove_player_input(slot);
    }

    /// `BootTheSlowestResponder`
    pub fn boot_the_slowest_responder(&self) {
        if self.host.get(Field::GameState) as u8 == STATE_RUNNING {
            self.host.event_set(EVENT_PAUSE);
            self.host.before_removing_players();
            self.host.event_set(EVENT_UNPAUSE);
        }
        let slot = self.host.mm_slowest_responder();
        if slot != SLOT_NONE {
            self.host.boot_player(slot, true, false);
        }
    }

    /// `TuneForSlowCPU`: the rate from the slowest CPU within the limits; a client queued as a lagger after repeated
    /// slow ticks; the PTs per CT from the rate.
    pub fn tune_for_slow_cpu(&self) -> bool {
        self.host.get(Field::CurrentCommunicationsTick);
        self.host.mm_lowest_ct();
        let mut cpu = self.host.mm_highest_cpu();
        if cpu == 0 {
            cpu = 1;
        }
        let factor = f32::from_bits(self.host.get(Field::CostFactor));
        let mut pts_per_second = f32_to_u8(1000.0f32 / (cpu as i32 as f32 * factor));
        self.host.set(Field::PTsPerSecond, pts_per_second as u32);
        if pts_per_second as u32 > self.host.get(Field::MaxPTsPerSecond) {
            pts_per_second = self.host.get(Field::MaxPTsPerSecond) as u8;
            self.host.set(Field::PTsPerSecond, pts_per_second as u32);
            let n = self.slow_cpu_count.load(SeqCst);
            self.slow_cpu_count.store(n.saturating_sub(1), SeqCst);
        } else if pts_per_second as u32 <= self.host.get(Field::MinPTsPerSecond) {
            self.slow_cpu_count.fetch_add(2, SeqCst);
            self.host.set(Field::PTsPerSecond, self.host.get(Field::MinPTsPerSecond) & 0xff);
        }
        if self.slow_cpu_count.load(SeqCst) >= 15 {
            self.slow_cpu_count.store(0, SeqCst);
            if self.slowest_client_cpu() != SLOT_NONE {
                let slot = self.slowest_client_cpu();
                self.push_slot(slot);
            }
        }
        self.set_pts_per_ct();
        true
    }

    /// `TuneForSlowNetwork`: the send interval follows the clients' response times (up to the maximum, down by a
    /// step that depends on it); a late tick is waited for, resent, warned about, and its laggers or the slowest
    /// responder booted. Answers false while the tick is late.
    pub fn tune_for_slow_network(&self) -> bool {
        let mut changed = false;
        let mut result = true;
        let interval = self.host.get(Field::SendInterval);
        self.send_interval_decrement.store(
            if interval < 500 {
                10
            } else if interval < 1000 {
                20
            } else {
                100
            },
            SeqCst,
        );
        let last_ct = || self.host.get(Field::LastCtMessageId);
        let mut state = self.net_tune_state.load(SeqCst);
        if state == 0 {
            let episodes = self.late_episodes.fetch_add(1, SeqCst).wrapping_add(1);
            self.net_tune_state.store(1, SeqCst);
            if episodes == 2 {
                result = false;
                state = u32::MAX; // (break)
            } else {
                state = 1; // (falls through)
            }
        }
        match state {
            1 => 'case: {
                if self.host.mm_outstanding(last_ct()) > 0 {
                    result = false;
                    let late = self.consecutive_late.fetch_add(1, SeqCst).wrapping_add(1);
                    self.late_count.fetch_add(1, SeqCst);
                    if late >= 2 {
                        self.consecutive_late.store(0, SeqCst);
                        self.net_tune_state.store(2, SeqCst);
                    }
                    break 'case;
                }
                self.consecutive_late.store(0, SeqCst);
                let highest = self.host.mm_highest_response_time();
                self.highest_response_time.store(highest, SeqCst);
                let interval = self.host.get(Field::SendInterval);
                if highest > interval {
                    let increment = self.host.get(Field::SendIntervalIncrement);
                    let max = self.host.get(Field::MaxSendInterval);
                    if highest.wrapping_add(increment) > max {
                        if self.at_max_count.load(SeqCst) >= 20 {
                            self.net_tune_state.store(5, SeqCst);
                            self.at_max_count.store(0, SeqCst);
                        } else {
                            self.at_max_count.fetch_add(1, SeqCst);
                            self.set_send_interval(self.host.get(Field::MaxSendInterval));
                            self.set_pts_per_ct();
                            changed = true;
                        }
                    } else {
                        self.set_send_interval(highest.wrapping_add(self.host.get(Field::SendIntervalIncrement)));
                        self.set_pts_per_ct();
                    }
                } else if self.late_count.load(SeqCst) >= 10 {
                    let increment = self.host.get(Field::SendIntervalIncrement);
                    let interval = self.host.get(Field::SendInterval);
                    if increment.wrapping_add(interval) > self.host.get(Field::MaxSendInterval) {
                        self.net_tune_state.store(5, SeqCst);
                    } else {
                        self.set_send_interval(increment.wrapping_add(interval));
                        self.set_pts_per_ct();
                        changed = true;
                    }
                } else {
                    let interval = self.host.get(Field::SendInterval);
                    let decrement = self.send_interval_decrement.load(SeqCst);
                    if interval.wrapping_sub(decrement) > 350 {
                        if self.at_max_count.load(SeqCst) > 0 {
                            self.at_max_count.fetch_sub(1, SeqCst);
                        }
                        self.set_send_interval(interval.wrapping_sub(decrement));
                        self.set_pts_per_ct();
                    }
                }
                self.late_count.store(0, SeqCst);
            }
            2 => {
                if self.host.mm_outstanding(last_ct()) > 0 {
                    self.host.mm_resend(last_ct());
                    self.resend_count.fetch_add(1, SeqCst);
                    result = false;
                    self.net_tune_state.store(3, SeqCst);
                } else {
                    self.resend_count.store(0, SeqCst);
                    self.net_tune_state.store(0, SeqCst);
                    self.late_episodes.store(0, SeqCst);
                }
            }
            3 => {
                if self.host.mm_outstanding(last_ct()) > 0 {
                    if self.resend_count.load(SeqCst).is_multiple_of(3) {
                        self.lag_warning.store(true, SeqCst);
                        self.host.mm_resend(last_ct());
                    }
                    let resends = self.resend_count.fetch_add(1, SeqCst).wrapping_add(1);
                    result = false;
                    if resends > 10 {
                        self.resend_count.store(0, SeqCst);
                        self.net_tune_state.store(4, SeqCst);
                    }
                    let increment = self.host.get(Field::SendIntervalIncrement);
                    let interval = self.host.get(Field::SendInterval);
                    if increment.wrapping_add(interval) <= self.host.get(Field::MaxSendInterval) {
                        self.at_max_count.fetch_add(1, SeqCst);
                        self.set_send_interval(self.host.get(Field::MaxSendInterval));
                        self.set_pts_per_ct();
                    }
                    changed = true;
                } else {
                    self.resend_count.store(0, SeqCst);
                    self.net_tune_state.store(0, SeqCst);
                    self.late_episodes.store(0, SeqCst);
                }
            }
            4 | 5 => 'case: {
                self.net_tune_state.store(0, SeqCst);
                let game_state = self.host.get(Field::GameState) as u8;
                if self.late_episodes.load(SeqCst) < 2 && game_state != STATE_OPEN && game_state != STATE_CLOSED {
                    self.lag_warning.store(true, SeqCst);
                    result = false;
                    break 'case;
                }
                if state == 4 {
                    self.boot_all_laggers(last_ct());
                    self.set_send_interval(self.host.get(Field::DefaultSendInterval));
                } else {
                    self.boot_the_slowest_responder();
                }
                self.set_send_interval(self.host.get(Field::DefaultSendInterval));
                self.late_episodes.store(0, SeqCst);
                result = false;
            }
            _ => {}
        }
        if !changed {
            return result;
        }
        let mut slot = self.host.mm_who(last_ct());
        if slot != SLOT_NONE {
            self.push_slot(slot);
        }
        slot = self.host.mm_who_next(slot, last_ct());
        while slot != SLOT_NONE {
            self.push_slot(slot);
            slot = self.host.mm_who_next(slot, last_ct());
        }
        result
    }

    /// `TuneServer`: the network tuned (a late tick forces the next one), the CPU while the game runs, and the
    /// laggers reported (or the report cleared once). Answers false while lagging.
    pub fn tune_server(&self) -> bool {
        let mut result = self.tune_for_slow_network();
        if !result {
            self.host.mm_set_force_ct(true);
        }
        if self.host.get(Field::GameState) as u8 == STATE_RUNNING {
            if result {
                result = self.tune_for_slow_cpu();
            } else {
                self.tune_for_slow_cpu();
            }
        }
        let slots = self.slots().clone();
        if !slots.is_empty() {
            self.host.report_laggers(&slots, self.lag_warning.load(SeqCst));
            self.lag_report_active.store(true, SeqCst);
            self.slots().clear();
            self.lag_warning.store(false, SeqCst);
        } else if self.lag_report_active.load(SeqCst) {
            self.host.report_laggers(&slots, self.lag_warning.load(SeqCst));
            self.lag_report_active.store(false, SeqCst);
            self.lag_warning.store(false, SeqCst);
        }
        result
    }
}
