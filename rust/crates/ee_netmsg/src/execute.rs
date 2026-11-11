//! The game messages' `Execute` decisions (RUST_PLAN R5b.4 leaf 4): the tests and the arithmetic of
//! `EEMessage.cpp`'s bodies over plain values. The C++ keeps the objects, the locks and the world calls; it reads the
//! values, asks here, and applies the answer (ee/EEMessageExecute.h).
//!
//! - the selection id check (`EEMessage::ExecuteSelectionMessage`, `ExecuteMessage`) and the selection toggles'
//!   passes;
//! - the resources: `EEMAddRemoveResources`' take test, the market price (`EEGetTradePrice`) and
//!   `EEMExchangeResources`' step, `EEMPayTribute`'s orders and chat line, `EEMDisband`'s refund.
//!
//! Simulation arithmetic (RUST_PLAN 2.1): the f32 operations in the original's order, `lrintf` never `as`. The C++
//! `long`s travel as i64 (Windows' are 32 bits, LP64's 64: the values here stay far inside both).

/// `EEMGameRestarting`: always runs.
pub const TYPE_GAME_RESTARTING: u8 = 0x91;
/// `EEMMigrationSlotInfo`: runs on a client whatever its selection id.
pub const TYPE_MIGRATION_SLOT_INFO: u8 = 0x7c;

const RESOURCE_FOOD: u8 = 2;
const RESOURCE_WOOD: u8 = 3;
const RESOURCE_STONE: u8 = 4;
const RESOURCE_GOLD: u8 = 5;
const RESOURCE_IRON: u8 = 6;

/// `EEGetTradePrice`'s low limit (the `kHeightEpsilon` word, 0.01) and the curve's two words (0x449af8, 0x449afc).
const TRADE_LIMIT_LOW: f32 = 0.01;
const TRADE_LIMIT_HIGH: f32 = 1000.0;
const TRADE_CURVE: f32 = 0.666_666_7;
const TRADE_SCALE: f32 = 5000.0;
/// `EEMDisband`'s refund (its own .rdata word).
const DISBAND_REFUND: f32 = 0.75;

/// `UMFloatToLong` (`lrintf`) as the x86-64 build computes it: round half to even, the conversion's "indefinite"
/// value for NaN or out of range.
fn lrintf(value: f32) -> i32 {
    let rounded = value.round_ties_even();
    if (-2_147_483_648.0..2_147_483_648.0).contains(&rounded) {
        rounded as i32
    } else {
        i32::MIN
    }
}

/// `ExecuteSelectionMessage`: a client marks the world as executing the local selection while the local sender's
/// selection message runs (`gWorld.mExecutingLocalSelection`); the server does not.
pub fn selection_marks_local(is_server: bool, is_local: bool) -> bool {
    !is_server && is_local
}

/// `ExecuteSelectionMessage`, after `Execute`: the local sender's selected group copies its active group when the
/// message's selection id is the communications object's current one.
pub fn selection_copies(is_local: bool, message_id: u8, current_id: u8) -> bool {
    is_local && message_id == current_id
}

/// `ExecuteMessage`: a command runs when its selection id is the sender's last one; Game Restarting always, and on a
/// client Migration Slot Info too. Otherwise the sender's active group is reset to its selection.
pub fn command_runs(is_server: bool, message_type: u8, message_id: u8, last_id: u8) -> bool {
    if message_id == last_id || message_type == TYPE_GAME_RESTARTING {
        return true;
    }
    !is_server && message_type == TYPE_MIGRATION_SLOT_INFO
}

/// The selection toggles (`EEMAddUnitToSelectedGroup`, `EEMAddAndRemoveUnitsFromSelectedGroup`) run twice while the
/// communications object replays a selection.
pub fn selection_passes(executing_selection: bool) -> u32 {
    if executing_selection {
        2
    } else {
        1
    }
}

/// What `EEMAddRemoveResources` does.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum AddRemove {
    Nothing = 0,
    Add = 1,
    Take = 2,
}

/// `EEMAddRemoveResources`: an add always; a take of a positive amount up to what the player has, compared unsigned
/// (the have's low 32 bits, as on Windows).
pub fn add_remove(add: bool, amount: u32, have: i64) -> AddRemove {
    if add {
        AddRemove::Add
    } else if amount > 0 && amount <= have as u32 {
        AddRemove::Take
    } else {
        AddRemove::Nothing
    }
}

/// `EEGetTradePrice`: a resource's market price (1..1000) from the world table's rate and the player's trade balance
/// in it; 1 lower when `buy` is set, else 1 higher.
#[allow(clippy::manual_clamp)] // the original's two tests, in its order
pub fn trade_price(rate: f32, trade: i64, buy: bool) -> i64 {
    let limit = if trade >= 0 { TRADE_LIMIT_LOW } else { TRADE_LIMIT_HIGH };
    let numerator = (rate - limit) * (TRADE_CURVE * rate - limit) * TRADE_SCALE;
    let denominator = (TRADE_CURVE * rate - limit) * TRADE_SCALE + (rate - TRADE_CURVE * rate) * trade as f32;
    let mut price = 100.0 / (numerator / denominator + limit);
    if buy {
        price -= 1.0;
    } else {
        price += 1.0;
    }
    if price < 1.0 {
        price = 1.0;
    }
    if price > 1000.0 {
        price = 1000.0;
    }
    lrintf(price) as i64
}

/// One step of `EEMExchangeResources` (ee/EEMessageExecute.h): whether it applies, the traded resource's amount
/// (`cost`) and the gold's (`amount`).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct EeNetMsgExchangeStep {
    pub apply: u8,
    pub cost: i64,
    pub amount: i64,
}

/// One step of `EEMExchangeResources` at `price` (gold per 100): selling 100 of a resource (or all of it when less)
/// for the gold, or buying 100 (or what the gold pays for) with it. `have` is the resource to sell, or the gold.
pub fn exchange_step(is_sell: bool, price: i64, have: i64) -> EeNetMsgExchangeStep {
    let mut step = EeNetMsgExchangeStep { apply: 0, cost: 100, amount: price };
    if is_sell {
        if have < 100 {
            step.amount = lrintf(have.wrapping_mul(price) as f32 * 0.01) as i64;
            step.cost = have;
        }
        step.apply = (step.amount > 0) as u8;
    } else {
        step.apply = 1;
        if have < price {
            step.cost = lrintf(100.0 / price as f32 * have as f32) as i64;
            step.amount = have;
            if step.cost <= 0 {
                step.apply = 0;
            }
        }
    }
    step
}

/// `EEMPayTribute`'s orders: the tributes go food, gold, iron, stone, wood; the chat line lists food, wood, stone,
/// gold, iron.
const TRIBUTE_ORDER: [u8; 5] = [RESOURCE_FOOD, RESOURCE_GOLD, RESOURCE_IRON, RESOURCE_STONE, RESOURCE_WOOD];
const TEXT_ORDER: [u8; 5] = [RESOURCE_FOOD, RESOURCE_WOOD, RESOURCE_STONE, RESOURCE_GOLD, RESOURCE_IRON];

/// The resources with an amount (`amounts` by resource index, 7 of them), in the tribute's order or the chat
/// line's.
pub fn tribute_order(amounts: &[u32; 7], text: bool) -> Vec<u8> {
    let order = if text { &TEXT_ORDER } else { &TRIBUTE_ORDER };
    order.iter().copied().filter(|&r| amounts[r as usize] > 0).collect()
}

/// `EEMPayTribute`'s chat line: the giver's (1) when the local player gave, else the receiver's (2), else none.
pub fn tribute_viewer(local_is_from: bool, local_is_to: bool) -> u8 {
    if local_is_from {
        1
    } else if local_is_to {
        2
    } else {
        0
    }
}

/// `EEMDisband`: a living unit whose class can disband is disbanded, refunded its hit points' share of 3/4 of its
/// cost.
pub fn disband_refund(hit_points: i64, max_hit_points: i64, can_disband: bool) -> Option<f32> {
    if hit_points > 0 && can_disband {
        Some(hit_points as f32 / max_hit_points as f32 * DISBAND_REFUND)
    } else {
        None
    }
}

pub mod ffi {
    //! The C ABI (ee/EEMessageExecute.h's declarations; `ee_rust.h`).
    use super::*;

    #[no_mangle]
    pub extern "C" fn ee_netmsg_selection_marks_local(is_server: u8, is_local: u8) -> u8 {
        selection_marks_local(is_server != 0, is_local != 0) as u8
    }

    #[no_mangle]
    pub extern "C" fn ee_netmsg_selection_copies(is_local: u8, message_id: u8, current_id: u8) -> u8 {
        selection_copies(is_local != 0, message_id, current_id) as u8
    }

    #[no_mangle]
    pub extern "C" fn ee_netmsg_command_runs(is_server: u8, message_type: u8, message_id: u8, last_id: u8) -> u8 {
        command_runs(is_server != 0, message_type, message_id, last_id) as u8
    }

    #[no_mangle]
    pub extern "C" fn ee_netmsg_selection_passes(executing_selection: u8) -> u32 {
        selection_passes(executing_selection != 0)
    }

    /// 0 nothing, 1 add, 2 take.
    #[no_mangle]
    pub extern "C" fn ee_netmsg_add_remove(add: u8, amount: u32, have: i64) -> u8 {
        add_remove(add != 0, amount, have) as u8
    }

    #[no_mangle]
    pub extern "C" fn ee_netmsg_trade_price(rate: f32, trade: i64, buy: u8) -> i64 {
        trade_price(rate, trade, buy != 0)
    }

    #[no_mangle]
    pub extern "C" fn ee_netmsg_exchange_step(is_sell: u8, price: i64, have: i64) -> EeNetMsgExchangeStep {
        exchange_step(is_sell != 0, price, have)
    }

    /// The resources in `out_resources`; the count.
    ///
    /// # Safety
    /// `amounts` points to 7 values, `out_resources` to room for 5.
    #[no_mangle]
    pub unsafe extern "C" fn ee_netmsg_tribute_order(amounts: *const u32, text: u8, out_resources: *mut u8) -> u32 {
        let amounts = &*(amounts as *const [u32; 7]);
        let order = tribute_order(amounts, text != 0);
        for (i, &r) in order.iter().enumerate() {
            *out_resources.add(i) = r;
        }
        order.len() as u32
    }

    #[no_mangle]
    pub extern "C" fn ee_netmsg_tribute_viewer(local_is_from: u8, local_is_to: u8) -> u8 {
        tribute_viewer(local_is_from != 0, local_is_to != 0)
    }

    /// 1 and the fraction in `out_fraction` when disbanded; 0 (`out_fraction` untouched) else.
    ///
    /// # Safety
    /// `out_fraction` is writable.
    #[no_mangle]
    pub unsafe extern "C" fn ee_netmsg_disband_refund(
        hit_points: i64,
        max_hit_points: i64,
        can_disband: u8,
        out_fraction: *mut f32,
    ) -> u8 {
        match disband_refund(hit_points, max_hit_points, can_disband != 0) {
            Some(f) => {
                *out_fraction = f;
                1
            }
            None => 0,
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_selection_check() {
        assert!(selection_marks_local(false, true));
        assert!(!selection_marks_local(true, true));
        assert!(!selection_marks_local(false, false));
        assert!(selection_copies(true, 7, 7));
        assert!(!selection_copies(true, 7, 8));
        assert!(!selection_copies(false, 7, 7));
        for server in [false, true] {
            for t in 0..=255u8 {
                assert!(command_runs(server, t, 3, 3));
                let late = command_runs(server, t, 3, 4);
                let want = t == 0x91 || (!server && t == 0x7c);
                assert_eq!(late, want, "server {server} type {t:#x}");
            }
        }
        assert_eq!(selection_passes(false), 1);
        assert_eq!(selection_passes(true), 2);
    }

    #[test]
    fn add_remove_compares_unsigned() {
        assert_eq!(add_remove(true, 0, -5), AddRemove::Add);
        assert_eq!(add_remove(false, 0, 100), AddRemove::Nothing);
        assert_eq!(add_remove(false, 100, 100), AddRemove::Take);
        assert_eq!(add_remove(false, 101, 100), AddRemove::Nothing);
        // a negative have is a huge unsigned one; a have past 32 bits keeps its low word
        assert_eq!(add_remove(false, 5, -1), AddRemove::Take);
        assert_eq!(add_remove(false, 5, 0x1_0000_0000), AddRemove::Nothing);
        assert_eq!(add_remove(false, 5, 0x1_0000_0005), AddRemove::Take);
    }

    #[test]
    fn trade_prices() {
        // a balanced market at rate 1: 100 / (0.99 * 0.6567 * 5000 / (0.6567 * 5000) + 0.01) = 100, then +-1
        assert_eq!(trade_price(1.0, 0, false), 101);
        assert_eq!(trade_price(1.0, 0, true), 99);
        // selling a lot makes the resource cheap, buying a lot dear; the clamps hold
        assert!(trade_price(1.0, -100_000, false) < trade_price(1.0, 0, false));
        assert!(trade_price(1.0, 100_000, false) > trade_price(1.0, 0, false));
        for t in [-1_000_000, -1000, 0, 1000, 1_000_000] {
            for r in [0.25, 0.5, 1.0, 2.0, 3.0] {
                let p = trade_price(r, t, false);
                assert!((1..=1000).contains(&p), "rate {r} trade {t}: {p}");
            }
        }
        // degenerate rates still give a clamped price
        for r in [0.0, -1.0, 1000.0] {
            assert!((1..=1000).contains(&trade_price(r, 0, true)));
        }
        // the curve's word is 2/3 as f32 (0x449af8)
        assert_eq!(TRADE_CURVE.to_bits(), 0x3f2a_aaab);
    }

    #[test]
    fn exchange_steps() {
        // selling 100 at 120 gold
        assert_eq!(exchange_step(true, 120, 500), EeNetMsgExchangeStep { apply: 1, cost: 100, amount: 120 });
        // selling the 40 left: 40 * 120 / 100 = 48
        assert_eq!(exchange_step(true, 120, 40), EeNetMsgExchangeStep { apply: 1, cost: 40, amount: 48 });
        // nothing to sell
        assert_eq!(exchange_step(true, 120, 0).apply, 0);
        // a half rounds to even: 1 * 50 * 0.01 = 0.5 -> 0, 3 * 50 * 0.01 = 1.5 -> 2
        assert_eq!(exchange_step(true, 50, 1).apply, 0);
        assert_eq!(exchange_step(true, 50, 3).amount, 2);
        // buying 100 for 80 gold, or what 40 gold pays for: 100 / 80 * 40 = 50
        assert_eq!(exchange_step(false, 80, 500), EeNetMsgExchangeStep { apply: 1, cost: 100, amount: 80 });
        assert_eq!(exchange_step(false, 80, 40), EeNetMsgExchangeStep { apply: 1, cost: 50, amount: 40 });
        assert_eq!(exchange_step(false, 1000, 4).apply, 0);
    }

    #[test]
    fn tribute_orders() {
        let amounts = [0, 0, 1, 2, 3, 4, 5];
        assert_eq!(tribute_order(&amounts, false), vec![2, 5, 6, 4, 3]);
        assert_eq!(tribute_order(&amounts, true), vec![2, 3, 4, 5, 6]);
        assert_eq!(tribute_order(&[0, 0, 0, 7, 0, 0, 1], false), vec![6, 3]);
        assert!(tribute_order(&[9, 9, 0, 0, 0, 0, 0], true).is_empty());
        assert_eq!(tribute_viewer(true, true), 1);
        assert_eq!(tribute_viewer(false, true), 2);
        assert_eq!(tribute_viewer(false, false), 0);
    }

    #[test]
    fn disband_refunds() {
        assert_eq!(disband_refund(50, 100, true), Some(0.375));
        assert_eq!(disband_refund(0, 100, true), None);
        assert_eq!(disband_refund(50, 100, false), None);
        assert!(disband_refund(5, 0, true).unwrap().is_infinite());
    }

    #[test]
    fn lrintf_is_the_x86_conversion() {
        assert_eq!(lrintf(0.5), 0);
        assert_eq!(lrintf(1.5), 2);
        assert_eq!(lrintf(-2.5), -2);
        assert_eq!(lrintf(f32::NAN), i32::MIN);
        assert_eq!(lrintf(3e9), i32::MIN);
    }

    #[test]
    fn the_ffi_tribute_and_refund() {
        let amounts = [0u32, 0, 1, 0, 3, 0, 5];
        let mut out = [0u8; 5];
        let n = unsafe { ffi::ee_netmsg_tribute_order(amounts.as_ptr(), 0, out.as_mut_ptr()) };
        assert_eq!((n, &out[..n as usize]), (3, &[2u8, 6, 4][..]));
        let mut f = -1.0f32;
        assert_eq!(unsafe { ffi::ee_netmsg_disband_refund(0, 10, 1, &mut f) }, 0);
        assert_eq!(f, -1.0);
        assert_eq!(unsafe { ffi::ee_netmsg_disband_refund(10, 10, 1, &mut f) }, 1);
        assert_eq!(f, 0.75);
    }
}
