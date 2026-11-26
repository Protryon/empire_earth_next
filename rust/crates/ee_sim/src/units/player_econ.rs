//! The player's economy and improvement decisions as `EEPlayer`'s members call them, each with its TEST ONLY PLEC
//! record (the world lane, round 2: were EEPlayer.cpp's members over `EEPlayerEconScope`): the outermost decision on
//! the thread that runs the players' ticks (the PLST capture's, [`note_capture_thread`]) records its kind, the
//! player's id, its arguments, its result, its reason text's length (-1 without one), then the player's resources
//! and population after it. A decision another one calls (`CanAffordScaled` inside `CanAffordRepair`) is not
//! recorded. The decisions are player.rs's and player_units.rs's.

#![allow(clippy::unnecessary_cast)] // c_long and c_ulong are 32 bits on Windows, 64 on LP64

use std::cell::Cell;
use std::os::raw::{c_long, c_ulong};
use std::sync::atomic::{AtomicU64, Ordering};

use crate::units::player::{Player, PlayerHost};

/// The decisions (the record's kind): the C++ member each stands for.
pub const EE_UNITS_ECON_CAN_PRODUCE: u32 = 1;
pub const EE_UNITS_ECON_CAN_AFFORD: u32 = 2;
pub const EE_UNITS_ECON_CAN_AFFORD_SCALED: u32 = 3;
pub const EE_UNITS_ECON_PAY: u32 = 6;
pub const EE_UNITS_ECON_PAY_IMPROVEMENT: u32 = 7;
pub const EE_UNITS_ECON_REFUND_TECHNOLOGY: u32 = 8;
pub const EE_UNITS_ECON_CAN_BUY: u32 = 10;
pub const EE_UNITS_ECON_CAN_IMPROVE: u32 = 11;
pub const EE_UNITS_ECON_IS_IMPROVEMENT_MAXED: u32 = 12;
pub const EE_UNITS_ECON_IMPROVEMENT_LEVEL: u32 = 13;
pub const EE_UNITS_ECON_COUNT_QUEUED: u32 = 14;
pub const EE_UNITS_ECON_ADVANCE_IMPROVEMENT: u32 = 15;
pub const EE_UNITS_ECON_QUEUE_IMPROVEMENT: u32 = 16;
pub const EE_UNITS_ECON_REMOVE_QUEUED: u32 = 17;
pub const EE_UNITS_ECON_CAN_AFFORD_REPAIR: u32 = 18;
pub const EE_UNITS_ECON_TOTAL_POPULATION: u32 = 19;
pub const EE_UNITS_ECON_NUM_HEROES: u32 = 21;
pub const EE_UNITS_ECON_APPLY_IMPROVEMENT: u32 = 22;

static NEXT_THREAD: AtomicU64 = AtomicU64::new(1);
/// The thread that runs the players' ticks (0: none yet)
static CAPTURE_THREAD: AtomicU64 = AtomicU64::new(0);

thread_local! {
    static THIS_THREAD: u64 = NEXT_THREAD.fetch_add(1, Ordering::Relaxed);
    static DEPTH: Cell<u32> = const { Cell::new(0) };
}

fn this_thread() -> u64 {
    THIS_THREAD.with(|t| *t)
}

/// The players' ticks run on this thread (the PLST record's, while the capture is on).
pub fn note_capture_thread() {
    CAPTURE_THREAD.store(this_thread(), Ordering::Relaxed);
}

/// One decision's arguments (the members' own, in their order) and what the record reads.
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EconArgs {
    /// a tech node, a unit class, or the repaired unit (the operation's object)
    pub object: u64,
    pub a: i64,
    pub b: i64,
    pub c: i64,
    pub d: i64,
    pub e: i64,
    pub f: f32,
    /// the reason text (a `UWideString*`, 0 none)
    pub text: u64,
}

/// The C++ the decisions read beyond the player's host.
pub trait EconHost {
    /// the record's: on, and its writer
    fn capturing(&mut self) -> bool;
    fn commit(&mut self, record: &[i32]);
    /// the node's id (-1: none or no data)
    fn node_id(&mut self, node: u64) -> i32;
    /// a unit class's `mUnitType`
    fn class_type(&mut self, class: u64) -> i64;
    /// a unit's `mStats->mMaxHitPoints`, `mUnitClass->GetTechnology()`, `mUnitClass->GetUnitType()`
    fn unit_max_hit_points(&mut self, unit: u64) -> c_ulong;
    fn unit_technology(&mut self, unit: u64) -> c_long;
    fn unit_type(&mut self, unit: u64) -> i64;
    /// a `UWideString`'s `GetLength()`
    fn text_length(&mut self, text: u64) -> i32;
    /// the player's id, resources 2..6 and population
    fn player_record(&mut self) -> (i32, [i32; 5], i32);
    /// player_units.rs's `ee_units_player_units(what, 0, a, b)` and `ee_units_player_apply_improvement`
    fn units(&mut self, what: u32, a: bool, b: bool) -> c_long;
    fn apply_improvement(&mut self, unit_type: c_long, improvement: c_long, lower: bool);
}

fn bits(f: f32) -> i64 {
    f.to_bits() as i32 as i64
}

/// The decision `kind` over the player `p` (player.rs's), its record's arguments and its result.
pub fn decide<H: PlayerHost + ?Sized, E: EconHost + ?Sized>(
    p: &mut Player<H>,
    e: &mut E,
    kind: u32,
    x: &EconArgs,
) -> (i64, Vec<i64>) {
    let node = x.object as i64;
    let flag = |v: i64| v != 0;
    let r = |v: bool| i64::from(v);
    let ut = x.a as c_ulong;
    let imp = x.b as c_long;
    match kind {
        EE_UNITS_ECON_CAN_PRODUCE => (
            r(p.can_produce(x.a as c_long, x.b as c_long, x.c as c_long, flag(x.d), x.e as c_long, x.text as i64)),
            vec![x.a, x.b, x.c, x.d, x.e, r(x.text != 0)],
        ),
        EE_UNITS_ECON_CAN_AFFORD => {
            let id = e.node_id(x.object) as i64;
            (r(p.can_afford(node, x.a as c_long, x.b as c_long)), vec![id, x.a, x.b])
        }
        EE_UNITS_ECON_CAN_AFFORD_SCALED => {
            let id = e.node_id(x.object) as i64;
            (r(p.can_afford_scaled(node, x.a as c_long, x.f, x.b as c_long)), vec![id, x.a, bits(x.f), x.b])
        }
        EE_UNITS_ECON_PAY => {
            p.pay(node, x.a as c_long, x.f);
            (0, vec![e.node_id(x.object) as i64, x.a, bits(x.f)])
        }
        EE_UNITS_ECON_PAY_IMPROVEMENT => {
            let class_type = if x.object != 0 { e.class_type(x.object) } else { -1 };
            if x.object != 0 {
                p.pay_improvement(class_type as c_ulong, x.a as c_long);
            }
            (0, vec![class_type as i32 as i64, x.a])
        }
        EE_UNITS_ECON_REFUND_TECHNOLOGY => {
            p.refund_technology(node, x.a as c_long, x.f);
            (0, vec![e.node_id(x.object) as i64, x.a, bits(x.f)])
        }
        EE_UNITS_ECON_CAN_BUY => (r(p.can_buy(ut, imp, x.text as i64)), vec![x.a, x.b, r(x.text != 0)]),
        // (the reason is recorded, never written: the original passed none on)
        EE_UNITS_ECON_CAN_IMPROVE => (r(p.can_improve(ut, imp)), vec![x.a, x.b, r(x.text != 0)]),
        EE_UNITS_ECON_IS_IMPROVEMENT_MAXED => (r(p.is_improvement_maxed(ut, imp)), vec![x.a, x.b]),
        EE_UNITS_ECON_IMPROVEMENT_LEVEL => (p.level(ut, imp) as i64, vec![x.a, x.b]),
        EE_UNITS_ECON_COUNT_QUEUED => (p.count_queued(ut, imp) as i64, vec![x.a, x.b]),
        EE_UNITS_ECON_ADVANCE_IMPROVEMENT => (r(p.advance(ut)), vec![x.a]),
        EE_UNITS_ECON_QUEUE_IMPROVEMENT => {
            p.queue_improvement(ut, x.b as c_ulong, flag(x.c), flag(x.d));
            (0, vec![x.a, x.b, x.c, x.d])
        }
        EE_UNITS_ECON_REMOVE_QUEUED => {
            p.remove_queued(ut, imp, flag(x.c));
            (0, vec![x.a, x.b, x.c])
        }
        EE_UNITS_ECON_CAN_AFFORD_REPAIR => {
            let (unit, other) = (x.object, x.a as u64);
            let maximum = e.unit_max_hit_points(unit);
            let tech = e.unit_technology(unit);
            let node = p.node(tech);
            let other_type = e.unit_type(other);
            let scale = (x.b as c_ulong) as f32 / maximum as f32;
            let result = p.can_afford_scaled(node, other_type as c_long, scale, 1);
            (r(result), vec![e.unit_type(unit), other_type, x.b])
        }
        EE_UNITS_ECON_TOTAL_POPULATION => (e.units(6, flag(x.a), flag(x.b)) as i64, vec![x.a, x.b]),
        EE_UNITS_ECON_NUM_HEROES => (e.units(11, flag(x.a), flag(x.b)) as i64, vec![x.a, x.b]),
        EE_UNITS_ECON_APPLY_IMPROVEMENT => {
            e.apply_improvement(x.a as c_long, x.b as c_long, flag(x.c));
            (0, vec![x.a, x.b, x.c])
        }
        _ => (0, Vec::new()),
    }
}

/// The record's fields after the decision (PLEC: kind, the player's id, the arguments, the result, the text's
/// length, the resources 2..6, the population), when it is the outermost one on the ticks' thread.
pub fn record<E: EconHost + ?Sized>(e: &mut E, outer: bool, kind: u32, args: &[i64], result: i64, text: u64) {
    if !outer || !e.capturing() {
        return;
    }
    let thread = CAPTURE_THREAD.load(Ordering::Relaxed);
    if thread == 0 || thread != this_thread() {
        return;
    }
    let (id, caps, population) = e.player_record();
    let mut out = vec![kind as i32, id];
    out.extend(args.iter().map(|&a| a as i32));
    out.push(result as i32);
    out.push(if text != 0 { e.text_length(text) } else { -1 });
    out.extend_from_slice(&caps);
    out.push(population);
    e.commit(&out);
}

/// Runs decision `kind` in its scope (the depth counts the nested ones) and records it.
pub fn run<H: PlayerHost + ?Sized, E: EconHost + ?Sized>(p: &mut Player<H>, e: &mut E, kind: u32, x: &EconArgs) -> i64 {
    let outer = DEPTH.with(|d| {
        let n = d.get();
        d.set(n + 1);
        n == 0
    });
    let (result, args) = decide(p, e, kind, x);
    // (the text the record measures: CanBuy's, CanProduce's and CanImprove's)
    let text = match kind {
        EE_UNITS_ECON_CAN_BUY | EE_UNITS_ECON_CAN_PRODUCE | EE_UNITS_ECON_CAN_IMPROVE => x.text,
        _ => 0,
    };
    record(e, outer, kind, &args, result, text);
    DEPTH.with(|d| d.set(d.get() - 1));
    result
}

#[cfg(not(test))]
mod game {
    use super::*;
    use crate::app::capture_record;
    use crate::units::player::{with, EeUnitsPlayerRefs};
    use crate::world::{ClassView, UnitView, WorldRead};
    use std::ffi::c_void;

    extern "C" {
        // EEPlayer.cpp
        fn ee_units_player_wide_length(text: *const c_void) -> u32;
    }

    struct GameEcon {
        refs: *const EeUnitsPlayerRefs,
    }

    impl EconHost for GameEcon {
        fn capturing(&mut self) -> bool {
            capture_record::active()
        }
        fn commit(&mut self, record: &[i32]) {
            let mut r = capture_record::Record::new(u32::from_le_bytes(*b"PLEC"));
            for &v in record {
                r.i32(v);
            }
            r.commit();
        }
        fn node_id(&mut self, node: u64) -> i32 {
            // SAFETY: the C++ passes a live node (or 0); the world is held by the decision's caller
            let t = unsafe { WorldRead::begin() };
            unsafe { crate::world::tech_view::NodeView::of(&t, node) }
                .and_then(|n| n.data().map(|_| n.id() as i32))
                .unwrap_or(-1)
        }
        fn class_type(&mut self, class: u64) -> i64 {
            // SAFETY: a live unit class
            let t = unsafe { WorldRead::begin() };
            unsafe { ClassView::of(&t, class) }.map_or(0, |c| c.unit_type() as c_ulong as i64)
        }
        fn unit_max_hit_points(&mut self, unit: u64) -> c_ulong {
            // SAFETY: a live unit
            let t = unsafe { WorldRead::begin() };
            unsafe { UnitView::of(&t, unit) }.and_then(|u| u.stats()).map_or(0, |s| s.max_hit_points() as c_ulong)
        }
        fn unit_technology(&mut self, unit: u64) -> c_long {
            // SAFETY: a live unit
            let t = unsafe { WorldRead::begin() };
            unsafe { UnitView::of(&t, unit) }.and_then(|u| u.class()).map_or(0, |c| c.technology() as c_long)
        }
        fn unit_type(&mut self, unit: u64) -> i64 {
            // SAFETY: a live unit
            let t = unsafe { WorldRead::begin() };
            unsafe { UnitView::of(&t, unit) }.and_then(|u| u.class()).map_or(0, |c| c.unit_type() as c_ulong as i64)
        }
        fn text_length(&mut self, text: u64) -> i32 {
            // SAFETY: the decision's live UWideString
            unsafe { ee_units_player_wide_length(text as *const c_void) as i32 }
        }
        fn player_record(&mut self) -> (i32, [i32; 5], i32) {
            // SAFETY: the decision's live player and its record
            unsafe {
                let r = &*self.refs;
                let s = &*r.saved;
                let t = WorldRead::begin();
                let id = crate::world::PlayerView::of(&t, r.player as u64).map_or(0, |p| p.id() as i32);
                let mut caps = [0i32; 5];
                for (i, c) in caps.iter_mut().enumerate() {
                    *c = s.mResourceCaps[i + 2] as i32;
                }
                (id, caps, s.mPopulationCount as i32)
            }
        }
        fn units(&mut self, what: u32, a: bool, b: bool) -> c_long {
            // SAFETY: the decision's live refs
            unsafe { crate::units::player_units::ee_units_player_units(self.refs, what, std::ptr::null_mut(), a, b) }
        }
        fn apply_improvement(&mut self, unit_type: c_long, improvement: c_long, lower: bool) {
            // SAFETY: as above
            unsafe {
                crate::units::player_units::ee_units_player_apply_improvement(self.refs, unit_type, improvement, lower)
            }
        }
    }

    /// The member `kind` (`EE_UNITS_ECON_*`) of the player of `refs` with its arguments (see [`decide`]).
    ///
    /// # Safety
    /// As player.rs's `with`; `object` and `text` are live objects of the member (or 0).
    #[no_mangle]
    pub unsafe extern "C" fn ee_units_player_econ(
        refs: *const EeUnitsPlayerRefs,
        kind: u32,
        object: *mut c_void,
        a: i64,
        b: i64,
        c: i64,
        d: i64,
        e: i64,
        f: f32,
        text: *mut c_void,
    ) -> i64 {
        let x = EconArgs { object: object as u64, a, b, c, d, e, f, text: text as u64 };
        let mut host = GameEcon { refs };
        // SAFETY: the caller's contract
        unsafe { with(refs, |p| run(p, &mut host, kind, &x)) }
    }

    /// EEPlayer's virtuals GetHeroCount, GetPopulationLimit, CanAfford and ScaleCost (`op` the player host's
    /// operation: HERO_COUNT `a`, `b` the two flags; POPULATION_LIMIT `a` the argument; CAN_AFFORD `object` the node,
    /// `a` the unit type, `b` the count; SCALE_COST `a` the cost): EEComputerPlayer's overrides (cp player_game.rs)
    /// when the player is a computer player (`mComputerPlayer` set), else, or with `base` (`EEPlayer::CanAfford`'s
    /// qualified calls), EEPlayer's own.
    ///
    /// # Safety
    /// As [`ee_units_player_econ`].
    #[no_mangle]
    pub unsafe extern "C" fn ee_units_player_virtual(
        refs: *const EeUnitsPlayerRefs,
        op: i32,
        object: *mut c_void,
        a: i64,
        b: i64,
        base: bool,
    ) -> i64 {
        use crate::cp::player_game as cp;
        use crate::units::player::{
            EE_UNITS_PLAYER_CAN_AFFORD, EE_UNITS_PLAYER_HERO_COUNT, EE_UNITS_PLAYER_POPULATION_LIMIT,
        };
        use crate::world::layout::layout;
        // SAFETY (every block): the caller's contract: a live player and its records
        let p = unsafe { (*refs).player } as u64;
        let l = layout();
        let computer = !base && unsafe { *((p as usize + l.player_computer_player as usize) as *const usize) } != 0;
        let econ =
            |kind: u32| unsafe { ee_units_player_econ(refs, kind, object, a, b, 0, 0, 0, 0.0, std::ptr::null_mut()) };
        unsafe {
            match op {
                EE_UNITS_PLAYER_HERO_COUNT if computer => cp::ee_cp_pl_hero_count(p, a != 0, b != 0) as i64,
                EE_UNITS_PLAYER_HERO_COUNT => econ(EE_UNITS_ECON_NUM_HEROES) as c_long as i64,
                EE_UNITS_PLAYER_POPULATION_LIMIT if computer => cp::ee_cp_pl_population_limit(p, a as c_long) as i64,
                EE_UNITS_PLAYER_POPULATION_LIMIT => {
                    // mPopulationLimitBonus + mShareBonus + gWorld.mPlayerPopulationLimit
                    let f = &*((*refs).fields);
                    let t = WorldRead::begin();
                    let world = crate::world::WorldView::global(&t).map_or(0, |w| w.player_population_limit());
                    f.mPopulationLimitBonus.wrapping_add(f.mShareBonus).wrapping_add(world as c_long) as i64
                }
                EE_UNITS_PLAYER_CAN_AFFORD if computer => {
                    i64::from(cp::ee_cp_pl_can_afford(p, object as u64, a as c_long, b as c_long))
                }
                EE_UNITS_PLAYER_CAN_AFFORD => i64::from(econ(EE_UNITS_ECON_CAN_AFFORD) != 0),
                // SCALE_COST
                _ if computer => {
                    crate::cp::ffi::ee_cp_scale_cost((p as usize + l.cp_fields as usize) as *const _, a as c_long)
                        as i64
                }
                _ => a,
            }
        }
    }
}

#[cfg(not(test))]
pub use game::ee_units_player_virtual;

#[cfg(test)]
#[path = "player_econ_tests.rs"]
mod tests;
