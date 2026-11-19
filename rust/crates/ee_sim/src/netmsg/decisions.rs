//! TEST ONLY: the simulation capture's NMDC records, the `Execute` decisions (`execute`) over plain edge values and
//! over the real players (was EEMessageCapture.cpp's `DecisionTables` and `EEMessageCaptureDecisionSweep`). A record:
//! the decision's kind, the values' count, the values (i64 each: the inputs, then the answer).
//!
//! The tables are recorded once a process; the players' part at every sweep: each resource's market price both ways
//! (`EEGetTradePrice` over the world table's rates the C++ hands in), the exchange's steps and the take's test from
//! what the player has, and every unit's disband refund (through the world views).
#![allow(clippy::unnecessary_cast)] // c_long and c_ulong are 32 bits on Windows, 64 on LP64

use std::sync::atomic::{AtomicBool, Ordering};

use crate::app::capture;
use crate::netmsg::execute::ffi::*;
use crate::world::{WorldRead, WorldView};

/// The decisions' kinds (ee/EEMessageCapture.h's `kDecision*`)
const SELECTION_MARKS_LOCAL: u32 = 1;
const SELECTION_COPIES: u32 = 2;
const COMMAND_RUNS: u32 = 3;
const SELECTION_PASSES: u32 = 4;
const ADD_REMOVE: u32 = 5;
const TRADE_PRICE: u32 = 6;
const EXCHANGE_STEP: u32 = 7;
const TRIBUTE_ORDER: u32 = 8;
const TRIBUTE_VIEWER: u32 = 9;
const DISBAND_REFUND: u32 = 10;

/// The record's tag, "NMDC"
const NMDC: u32 = 0x43444d4e;

/// The resources food (2) .. iron (6), gold's index
const FOOD: usize = 2;
const IRON: usize = 6;
const GOLD: usize = 5;

/// A record's payload: the kind, the count, the values.
pub fn payload(kind: u32, values: &[i64]) -> Vec<u8> {
    let mut out = Vec::with_capacity(8 + values.len() * 8);
    out.extend_from_slice(&kind.to_le_bytes());
    out.extend_from_slice(&(values.len() as u32).to_le_bytes());
    for v in values {
        out.extend_from_slice(&v.to_le_bytes());
    }
    out
}

fn bits(value: f32) -> i64 {
    value.to_bits() as i64
}

/// The decisions as the capture records them, in order, through `out`.
pub struct Decisions<'a> {
    pub out: &'a mut dyn FnMut(u32, &[i64]),
}

impl Decisions<'_> {
    fn put(&mut self, kind: u32, values: &[i64]) {
        (self.out)(kind, values);
    }

    pub fn exchange(&mut self, sell: u8, price: i64, have: i64) {
        let s = ee_netmsg_exchange_step(sell, price, have);
        self.put(EXCHANGE_STEP, &[sell as i64, price, have, s.apply as i64, s.cost, s.amount]);
    }

    pub fn add_remove(&mut self, add: u8, amount: u32, have: i64) {
        self.put(ADD_REMOVE, &[add as i64, amount as i64, have, ee_netmsg_add_remove(add, amount, have) as i64]);
    }

    pub fn disband(&mut self, hit_points: i64, max: i64, can: u8) {
        let mut fraction = -1.0f32;
        // SAFETY: a local out value
        let result = unsafe { ee_netmsg_disband_refund(hit_points, max, can, &mut fraction) };
        let refund = if result != 0 { bits(fraction) } else { 0 };
        self.put(DISBAND_REFUND, &[hit_points, max, can as i64, result as i64, refund]);
    }

    /// The decisions over plain edge values: their whole tables where they are small.
    pub fn tables(&mut self) {
        const IDS: [u8; 7] = [0, 1, 2, 0x7f, 0x80, 0xfe, 0xff];
        const LAST_IDS: [u8; 3] = [0, 1, 0xff];
        for s in 0..2u8 {
            for l in 0..2u8 {
                self.put(SELECTION_MARKS_LOCAL, &[s as i64, l as i64, ee_netmsg_selection_marks_local(s, l) as i64]);
            }
        }
        for l in 0..2u8 {
            for a in IDS {
                for b in IDS {
                    let r = ee_netmsg_selection_copies(l, a, b);
                    self.put(SELECTION_COPIES, &[l as i64, a as i64, b as i64, r as i64]);
                }
            }
        }
        for s in 0..2u8 {
            for t in 0..256u32 {
                for a in IDS {
                    for b in LAST_IDS {
                        let r = ee_netmsg_command_runs(s, t as u8, a, b);
                        self.put(COMMAND_RUNS, &[s as i64, t as i64, a as i64, b as i64, r as i64]);
                    }
                }
            }
        }
        for e in 0..2u8 {
            self.put(SELECTION_PASSES, &[e as i64, ee_netmsg_selection_passes(e) as i64]);
        }
        const AMOUNTS: [u32; 8] = [0, 1, 2, 99, 100, 0x7fffffff, 0x80000000, 0xffffffff];
        const HAVES: [i64; 9] = [-1, 0, 1, 99, 100, 0x7fffffff, -0x80000000, 0x100000000, 0x1ffffffff];
        for a in 0..2u8 {
            for m in AMOUNTS {
                for h in HAVES {
                    self.add_remove(a, m, h);
                }
            }
        }
        const RATES: [f32; 11] = [0.0, 0.25, 0.5, 0.666_666_7, 1.0, 1.5, 2.0, 3.0, 10.0, 1000.0, -1.0];
        const TRADES: [i64; 14] =
            [-1000000, -100000, -1000, -101, -100, -1, 0, 1, 100, 101, 1000, 7500, 100000, 1000000];
        for r in RATES {
            for t in TRADES {
                for b in 0..2u8 {
                    self.put(TRADE_PRICE, &[bits(r), t, b as i64, ee_netmsg_trade_price(r, t, b)]);
                }
            }
        }
        const PRICES: [i64; 10] = [1, 2, 3, 50, 99, 100, 101, 333, 999, 1000];
        const EXCHANGE_HAVES: [i64; 17] =
            [-5000, -50, -1, 0, 1, 2, 3, 49, 50, 99, 100, 101, 333, 999, 1000, 1001, 5000];
        for s in 0..2u8 {
            for p in PRICES {
                for h in EXCHANGE_HAVES {
                    self.exchange(s, p, h);
                }
            }
        }
        for m in 0..32u32 {
            for t in 0..2u8 {
                let mut amounts = [0u32; 7];
                for i in 0..5u32 {
                    if m & (1 << i) != 0 {
                        amounts[2 + i as usize] = if i == 4 { 0xffffffff } else { 1 + i * 37 };
                    }
                }
                let mut order = [0u8; 5];
                // SAFETY: 7 amounts, room for 5 resources
                let n = unsafe { ee_netmsg_tribute_order(amounts.as_ptr(), t, order.as_mut_ptr()) };
                let mut v: Vec<i64> = amounts[2..7].iter().map(|&a| a as i64).collect();
                v.extend([t as i64, n as i64]);
                v.extend(order.iter().map(|&o| o as i64));
                self.put(TRIBUTE_ORDER, &v);
            }
        }
        for f in 0..2u8 {
            for t in 0..2u8 {
                self.put(TRIBUTE_VIEWER, &[f as i64, t as i64, ee_netmsg_tribute_viewer(f, t) as i64]);
            }
        }
        const HIT_POINTS: [i64; 9] = [-1, 0, 1, 3, 50, 99, 100, 250, 1001];
        const MAX_HIT_POINTS: [i64; 6] = [0, 1, 3, 100, 250, 7777];
        for h in HIT_POINTS {
            for m in MAX_HIT_POINTS {
                for c in 0..2u8 {
                    self.disband(h, m, c);
                }
            }
        }
    }

    /// A player's part: `trade[r - 2]` its trade balances, `caps[r]` what it has, `rates[r]` the world table's
    /// rates (`EEGetTradePrice`: 1 for gold), `units` each unit's hit points, maximum and disband flag.
    pub fn player(
        &mut self,
        index: u32,
        trade: &[i64; 5],
        caps: &[i64; 7],
        rates: &[f32; 7],
        units: &[(i64, i64, u8)],
    ) {
        for r in FOOD..=IRON {
            for b in 0..2u8 {
                let price = ee_netmsg_trade_price(rates[r], trade[r - 2], b) as i32 as i64;
                self.put(TRADE_PRICE, &[index as i64, r as i64, b as i64, trade[r - 2], price]);
                let have = caps[if b != 0 { r } else { GOLD }];
                self.exchange(b, price, have);
                self.exchange(b, price, have / 3);
                self.exchange(b, price, price - 1);
            }
            let have = caps[r];
            self.add_remove(0, 1, have);
            self.add_remove(0, have as u32, have);
            self.add_remove(0, (have as u32).wrapping_add(1), have);
            self.add_remove(0, 100, have);
        }
        for &(hit_points, max, can) in units {
            self.disband(hit_points, max, can);
        }
    }
}

fn record(kind: u32, values: &[i64]) {
    capture::commit(NMDC, &payload(kind, values));
}

/// One decision's record (the Rust bodies' `EE_NETMSG_OP_DECISION`).
///
/// # Safety
/// `values` points to `count` values.
#[no_mangle]
pub unsafe extern "C" fn ee_netmsg_capture_decision(kind: u32, values: *const i64, count: u32) {
    if !capture::ee_app_capture_active() {
        return;
    }
    // SAFETY: the caller's contract
    let values = if count == 0 { &[][..] } else { unsafe { std::slice::from_raw_parts(values, count as usize) } };
    record(kind, values);
}

/// The decisions' sweep (EE-SIM's capture, under the world): the tables once a process, then every player's.
/// `rates` holds the world table's trade rates by resource (7 of them; `EEGetTradePrice`'s, 1 for gold).
///
/// # Safety
/// `rates` points to 7 values; the caller holds the world.
#[no_mangle]
pub unsafe extern "C" fn ee_netmsg_capture_decision_sweep(rates: *const f32) {
    static TABLES_DONE: AtomicBool = AtomicBool::new(false);
    if !capture::ee_app_capture_active() {
        return;
    }
    // SAFETY: the caller's contract
    let rates = unsafe { *(rates as *const [f32; 7]) };
    let mut out = record;
    let mut d = Decisions { out: &mut out };
    if !TABLES_DONE.swap(true, Ordering::SeqCst) {
        d.tables();
    }
    // SAFETY: the caller holds the world
    let token = unsafe { WorldRead::begin() };
    let Some(world) = WorldView::global(&token) else { return };
    for p in 1..world.num_players() as usize {
        let Some(player) = world.player(p) else { continue };
        let saved = player.saved();
        let trade: [i64; 5] = crate::world_field!(saved, mResourceTrade).map(|v| v as i64);
        let caps: [i64; 7] = crate::world_field!(saved, mResourceCaps).map(|v| v as i64);
        let mut units = Vec::new();
        for u in player.units() {
            let Some(c) = u.complex_unit() else { continue };
            let (Some(stats), Some(class)) = (c.stats(), c.class()) else { continue };
            let can: bool = crate::world_field!(class.fields(), mCanDisband);
            units.push((c.hit_points(), stats.max_hit_points(), can as u8));
        }
        d.player(p as u32, &trade, &caps, &rates, &units);
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn tables_and_a_player() {
        let mut got: Vec<(u32, Vec<i64>)> = Vec::new();
        let mut out = |k: u32, v: &[i64]| got.push((k, v.to_vec()));
        let mut d = Decisions { out: &mut out };
        d.tables();
        // 4 + 98 + 10752 + 2 + 144 + 308 + 340 + 64 + 4 + 108
        assert_eq!(got.len(), 4 + 98 + 10752 + 2 + 144 + 308 + 340 + 64 + 4 + 108);
        assert_eq!(got[0], (SELECTION_MARKS_LOCAL, vec![0, 0, ee_netmsg_selection_marks_local(0, 0) as i64]));
        let last = got.last().unwrap();
        assert_eq!(last.0, DISBAND_REFUND);
        assert_eq!(&last.1[..4], &[1001, 7777, 1, 1]);
        assert_eq!(last.1[4], bits(1001.0f32 / 7777.0 * 0.75));
        got.clear();
        let mut out = |k: u32, v: &[i64]| got.push((k, v.to_vec()));
        let mut d = Decisions { out: &mut out };
        let rates = [1.0, 1.0, 0.5, 2.0, 1.5, 1.0, 3.0];
        d.player(1, &[0, 10, -10, 0, 5], &[0, 0, 100, 200, 300, 400, 50], &rates, &[(5, 10, 1)]);
        // per resource: 2 x (price + 3 exchanges) + 4 add/remove; then the unit
        assert_eq!(got.len(), 5 * 12 + 1);
        assert_eq!(got[0].0, TRADE_PRICE);
        assert_eq!(&got[0].1[..4], &[1, 2, 0, 0]);
        assert_eq!(got[0].1[4], ee_netmsg_trade_price(0.5, 0, 0));
        // the sell's exchange is from the gold (caps[5])
        assert_eq!(got[1].1[2], 400);
        assert_eq!(got.last().unwrap().0, DISBAND_REFUND);
        assert_eq!(
            payload(7, &[1, -1]),
            [7, 0, 0, 0, 2, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 255, 255, 255, 255, 255, 255, 255, 255]
        );
    }
}
