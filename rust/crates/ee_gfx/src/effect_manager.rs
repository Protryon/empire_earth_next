//! The effect manager's algorithms (`EEEffectManager`, `src/Empire Earth/Graphics Thread/EEEffectManager.cpp`) over its
//! containers (`effect_store.rs`): which effect an entry point creates for a graphic effect's kind (the detail
//! test, the units a kind needs, the result codes, the multi-effect that is never saved), the registration after a
//! creation (`SetUp`, then `AddEffect` under a new handle), `AddEffect`/`RemoveEffect`, `DestroyQueued`,
//! `ClearEffects`, `RepairPointers`' walk, `Load` and `Save` (the handle counter, the effects by kind, the goal
//! markers, the destruction queue) and `Update`'s step loop.
//!
//! The effects are still C++ objects (opaque pointers here): their constructors, `SetUp`, `Step`, `Load`, `Save`,
//! `RepairPointers` and references are the host's, as are the goal markers. The host also reads and writes an
//! effect's kind, handle and saved flag (the record's). No borrow of the store is held across a host call: each
//! store operation is a short `with`, since a step, a set-up or a load creates and destroys effects in between.
//!
//! This is simulation state (RUST_PLAN 2.1): the list order is the order the tick steps the effects in, and the
//! handles and the queue are saved. Every call is the original's, in its order: an effect removed by the update or
//! the destruction queue is released twice (once for `AddEffect`'s reference, once in `RemoveEffect`).

use crate::effect_state::EffectFile;
use crate::effect_store::{EffectPtr, EffectStore};

/// `kNoEffectHandle`.
const NO_HANDLE: u32 = 0;
/// `Step`'s and `RepairPointers`' "remove me".
const REMOVE: i32 = 7;
/// An entry point's "not this kind / not without a unit".
const REFUSED: i32 = 10;
/// `kEffect3DMultiEffect`: created unsaved.
const MULTI_EFFECT: i32 = 0x3b;

/// The manager's creation entry points (the effects' constructors differ by entry; the C++ keeps them).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeGfxEffectEntry {
    /// `CreateUnitEffect`: on a unit
    Unit = 0,
    /// `CreateEffectBetweenUnits`: from a unit to a unit
    BetweenUnits = 1,
    /// `CreateEffect`: from a unit to a point
    UnitToPoint = 2,
    /// `CreatePlacedEffect`: at a point
    Placed = 3,
    /// `CreateTransformationEffect`: at a transformation
    Transformation = 4,
    /// `CreateLineEffect`: along a line
    Line = 5,
    /// `CreateEffectBetween`: between two points
    Between = 6,
    /// `CreateWeatherEffect`: over tiles
    Weather = 7,
}

/// What an entry point does for a kind: create the effect (saved or not), or return `result`.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct EeGfxEffectPlan {
    pub create: bool,
    pub saved: bool,
    pub result: i32,
}

/// The units a kind needs to be created by an entry point.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
enum Needs {
    Nothing,
    /// a living (complex) first unit
    From,
    /// both units living
    Both,
}

/// The kinds an entry point creates and the units each needs (None: the entry's default).
fn needs(entry: EeGfxEffectEntry, kind: i32) -> Option<Needs> {
    use EeGfxEffectEntry::*;
    use Needs::*;
    match entry {
        Unit => match kind {
            4 | 5 | 6 | 7 | 3 | 14 | 13 | 8 | 2 | 0x28 | 0x2e | 0x41 | 0x3f | 0x3e | 0x3b | 0x3a | 0x44 => {
                Some(Nothing)
            }
            9 | 18 | 10 | 16 | 15 | 0x18 | 0x27 | 0x24 | 0x23 | 0x1e | 0x29 | 0x34 | 0x2f | 0x2c | 0x2b | 0x35
            | 0x42 | 0x47 | 0x46 | 0x45 => Some(From),
            _ => None,
        },
        // muzzle blast, tracers, 2D projectile, tracer beam: both; winter breath: the first; shells: none
        BetweenUnits => match kind {
            0x14 | 0x21 | 0x30 | 0x40 => Some(Both),
            0x16 => Some(From),
            0x26 => Some(Nothing),
            _ => None,
        },
        // muzzle blast, 2D projectile, citizen beam, tracer beam, pulse cannon ball: the unit; confirmation: none
        UnitToPoint => match kind {
            0x14 | 0x30 | 0x33 | 0x40 | 0x43 => Some(From),
            0x41 => Some(Nothing),
            _ => None,
        },
        Placed => match kind {
            2 | 6 | 8 | 0xb | 0x12 | 0x18 | 0x1b | 0x20 | 0x22 | 0x2b | 0x2e | 0x31 | 0x32 | 0x39 | 0x3a | 0x3b
            | 0x3c => Some(Nothing),
            _ => None,
        },
        Transformation => match kind {
            0x22 | 0x32 => Some(Nothing),
            _ => None,
        },
        Line => match kind {
            0xc | 0x2e | 0x39 | 0x3a | 0x3b => Some(Nothing),
            _ => None,
        },
        // earthquake, 3D projectile, marching line
        Between => match kind {
            0x1a | 0x25 | 0x2d => Some(Nothing),
            _ => None,
        },
        // rain, snow
        Weather => match kind {
            0x2a | 0x3d => Some(Nothing),
            _ => None,
        },
    }
}

/// The entry point's choice for a graphic effect of `kind`: nothing above the detail setting (0); a kind the entry
/// does not create (10, but 0 for the point pair and the weather); a kind without the living units it needs (10);
/// else create it (the multi-effect unsaved).
pub fn plan(entry: EeGfxEffectEntry, kind: i32, detail_ok: bool, from_alive: bool, to_alive: bool) -> EeGfxEffectPlan {
    let none = |result| EeGfxEffectPlan { create: false, saved: true, result };
    if !detail_ok {
        return none(0);
    }
    let units_ok = match needs(entry, kind) {
        None => {
            let default = match entry {
                EeGfxEffectEntry::Between | EeGfxEffectEntry::Weather => 0,
                _ => REFUSED,
            };
            return none(default);
        }
        Some(Needs::Nothing) => true,
        Some(Needs::From) => from_alive,
        Some(Needs::Both) => from_alive && to_alive,
    };
    if !units_ok {
        return none(REFUSED);
    }
    EeGfxEffectPlan { create: true, saved: kind != MULTI_EFFECT, result: 0 }
}

/// The store, borrowed for one operation at a time.
pub trait StoreAccess {
    fn with<R>(&mut self, f: impl FnOnce(&mut EffectStore) -> R) -> R;
}

/// The effects and the goal markers as the manager calls them (the C++'s).
pub trait ManagerHost {
    fn kind(&mut self, e: EffectPtr) -> i32;
    fn handle(&mut self, e: EffectPtr) -> u32;
    fn set_handle(&mut self, e: EffectPtr, handle: u32);
    fn saved(&mut self, e: EffectPtr) -> bool;
    fn set_saved(&mut self, e: EffectPtr, saved: bool);
    /// `UReferenceCounter::Reference`
    fn reference(&mut self, e: EffectPtr);
    /// `EEEffect::Release` (may delete the effect)
    fn release(&mut self, e: EffectPtr);
    fn set_up(&mut self, e: EffectPtr) -> i32;
    fn step(&mut self, e: EffectPtr) -> i32;
    fn repair(&mut self, e: EffectPtr) -> i32;
    /// the effect's graphic effect is above the detail setting
    fn above_detail(&mut self, e: EffectPtr) -> bool;
    /// a load-time effect of a saved kind (0: a kind that has none)
    fn new_loaded(&mut self, kind: u32) -> EffectPtr;
    fn load<F: EffectFile>(&mut self, e: EffectPtr, file: &mut F) -> i32;
    fn save<F: EffectFile>(&mut self, e: EffectPtr, file: &mut F) -> i32;
    fn goals_update(&mut self);
    fn goals_load<F: EffectFile>(&mut self, file: &mut F) -> i32;
    fn goals_repair(&mut self) -> i32;
}

/// `AddEffect`: the effect first in the list, indexed under a new handle (`new_handle`) or its own, referenced.
pub fn add_effect<S: StoreAccess, H: ManagerHost>(s: &mut S, h: &mut H, e: EffectPtr, new_handle: bool) {
    let own = h.handle(e);
    let handle = s.with(|s| s.add(e, new_handle, own));
    h.set_handle(e, handle);
    h.reference(e);
}

/// `RemoveEffect`: unlinked and released.
pub fn remove_effect<S: StoreAccess, H: ManagerHost>(s: &mut S, h: &mut H, e: EffectPtr) {
    s.with(|s| s.unlink(e));
    h.release(e);
}

/// The registration of an entry point's new effect (null: none, 0): unsaved when the plan says so, set up, added
/// under a new handle even when the set-up fails; returns the set-up's result and the handle (`None`: no effect).
pub fn create<S: StoreAccess, H: ManagerHost>(s: &mut S, h: &mut H, e: EffectPtr, saved: bool) -> (i32, Option<u32>) {
    if e == 0 {
        return (0, None);
    }
    if !saved {
        h.set_saved(e, false);
    }
    let result = h.set_up(e);
    add_effect(s, h, e, true);
    (result, Some(h.handle(e)))
}

/// `DestroyQueued`: every queued handle leaves the queue; an effect still indexed under it is released, removed
/// and unindexed. Returns 3 or 9 for the last handle looked at (9: no handle), 0 for an empty queue.
pub fn destroy_queued<S: StoreAccess, H: ManagerHost>(s: &mut S, h: &mut H) -> i32 {
    let mut result = 0;
    while let Some(handle) = s.with(|s| s.take_destroyed()) {
        result = 9;
        if handle != NO_HANDLE {
            result = 3;
            let e = s.with(|s| s.find(handle));
            if e != 0 {
                h.release(e);
                remove_effect(s, h, e);
                s.with(|s| s.erase(handle));
            }
        }
    }
    result
}

/// `ClearEffects`: every indexed effect, in handle order, released, removed and unindexed.
pub fn clear<S: StoreAccess, H: ManagerHost>(s: &mut S, h: &mut H) {
    while let Some((handle, e)) = s.with(|s| s.at_or_after(0)) {
        h.release(e);
        remove_effect(s, h, e);
        s.with(|s| s.erase(handle));
    }
}

/// `RepairPointers`: each effect in handle order repairs its pointers; one whose unit is gone (7) or that is above
/// the detail setting is released, removed and unindexed; the walk stops at another error. Then the goal markers'.
pub fn repair<S: StoreAccess, H: ManagerHost>(s: &mut S, h: &mut H) -> i32 {
    let mut result = 0;
    let mut cursor: u64 = 0;
    while result == 0 {
        let Some((handle, e)) = s.with(|s| s.at_or_after(cursor)) else { break };
        cursor = handle as u64 + 1;
        result = h.repair(e);
        if result == REMOVE || h.above_detail(e) {
            h.release(e);
            remove_effect(s, h, e);
            s.with(|s| s.erase(handle));
            result = 0;
        }
    }
    if result == 0 {
        result = h.goals_repair();
    }
    result
}

/// `Load`: the handle counter, the effect count, each effect's kind and its own load (a kind without a load-time
/// effect reads nothing more); an effect is added even when its load fails, under its own handle unless a save
/// before 0.200 has none or one past the counter; then the goal markers and the destruction queue. Returns the
/// first error.
pub fn load<S: StoreAccess, H: ManagerHost, F: EffectFile>(
    s: &mut S,
    h: &mut H,
    file: &mut F,
    major: u32,
    minor: u32,
) -> i32 {
    // the original's local starts uninitialized (the counter of a failed read is garbage there)
    let mut last: u32 = 0;
    let mut result = file.read_u32(&mut last);
    s.with(|s| s.set_last_handle(last));
    if result != 0 {
        return result;
    }
    let mut count: i32 = 0;
    result = file.read_i32(&mut count);
    if result != 0 {
        return result;
    }
    let mut i: i32 = 0;
    while i < count {
        if result != 0 {
            return result;
        }
        let mut kind: u32 = 0;
        result = file.read_u32(&mut kind);
        if result != 0 {
            return result;
        }
        let e = h.new_loaded(kind);
        if e != 0 {
            result = h.load(e, file);
            let handle = h.handle(e);
            let new_handle = major == 0 && minor <= 0xc8 && (handle == NO_HANDLE || handle > last);
            add_effect(s, h, e, new_handle);
        }
        i += 1;
    }
    if result == 0 {
        result = h.goals_load(file);
        if result == 0 {
            let mut queued: u32 = 0;
            result = file.read_u32(&mut queued);
            let mut j: u32 = 0;
            while j < queued && result == 0 {
                let mut handle: u32 = 0;
                result = file.read_u32(&mut handle);
                if result == 0 && handle != NO_HANDLE {
                    s.with(|s| s.queue_destroy(handle));
                }
                j += 1;
            }
        }
    }
    result
}

/// `Save`: the handle counter, the saved effects' count, each saved effect in handle order (its kind, then its own
/// save: a write error can be overwritten by the next effect's result), the destruction queue.
pub fn save<S: StoreAccess, H: ManagerHost, F: EffectFile>(s: &mut S, h: &mut H, file: &mut F) -> i32 {
    let last = s.with(|s| s.last_handle());
    let mut result = file.write_u32(last);
    if result != 0 {
        return result;
    }
    let mut effects = Vec::new();
    let mut cursor: u64 = 0;
    while let Some((handle, e)) = s.with(|s| s.at_or_after(cursor)) {
        cursor = handle as u64 + 1;
        effects.push(e);
    }
    let mut count: i32 = 0;
    for &e in &effects {
        if h.saved(e) {
            count = count.wrapping_add(1);
        }
    }
    result = file.write_i32(count);
    if result != 0 {
        return result;
    }
    for &e in &effects {
        if !h.saved(e) {
            continue;
        }
        let kind = h.kind(e) as u32;
        result = file.write_u32(kind);
        if result == 0 {
            result = h.save(e, file);
        }
    }
    if result == 0 {
        let queued = s.with(|s| s.destroyed_len()) as u32;
        result = file.write_u32(queued);
        let mut i: u32 = 0;
        while i < queued && result == 0 {
            let handle = s.with(|s| s.destroyed_at(i as usize));
            result = file.write_u32(handle);
            i += 1;
        }
    }
    result
}

/// `Update`: the goal markers, the destruction queue, then every effect in list order (the next one taken before
/// the step); one whose step returns 7 is unindexed, removed and released.
pub fn update<S: StoreAccess, H: ManagerHost>(s: &mut S, h: &mut H) {
    h.goals_update();
    destroy_queued(s, h);
    let mut e = s.with(|s| s.head());
    while e != 0 {
        let next = s.with(|s| s.next(e));
        if h.step(e) == REMOVE {
            let handle = h.handle(e);
            s.with(|s| s.erase(handle));
            remove_effect(s, h, e);
            h.release(e);
        }
        e = next;
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::collections::BTreeMap;

    struct Plain(EffectStore);

    impl StoreAccess for Plain {
        fn with<R>(&mut self, f: impl FnOnce(&mut EffectStore) -> R) -> R {
            f(&mut self.0)
        }
    }

    #[derive(Clone, Debug, Default)]
    struct Fx {
        kind: i32,
        handle: u32,
        saved: bool,
        refs: i32,
        data: u32,
        /// step results to give, front first (then 0)
        steps: Vec<i32>,
        repair: i32,
        above: bool,
        /// a step creates an effect of this kind
        spawns: Option<i32>,
    }

    /// Effects by address; a log of every call, in order.
    #[derive(Default)]
    struct Host {
        fx: BTreeMap<EffectPtr, Fx>,
        next_ptr: EffectPtr,
        log: Vec<String>,
        goals: i32,
        /// the effects steps made (registered by the test)
        spawned: Vec<EffectPtr>,
    }

    impl Host {
        fn new_fx(&mut self, fx: Fx) -> EffectPtr {
            self.next_ptr += 16;
            self.fx.insert(self.next_ptr, Fx { refs: 1, ..fx });
            self.next_ptr
        }
    }

    impl ManagerHost for Host {
        fn kind(&mut self, e: EffectPtr) -> i32 {
            self.fx[&e].kind
        }
        fn handle(&mut self, e: EffectPtr) -> u32 {
            self.fx[&e].handle
        }
        fn set_handle(&mut self, e: EffectPtr, handle: u32) {
            self.fx.get_mut(&e).unwrap().handle = handle;
        }
        fn saved(&mut self, e: EffectPtr) -> bool {
            self.fx[&e].saved
        }
        fn set_saved(&mut self, e: EffectPtr, saved: bool) {
            self.log.push(format!("unsaved {e}"));
            self.fx.get_mut(&e).unwrap().saved = saved;
        }
        fn reference(&mut self, e: EffectPtr) {
            self.fx.get_mut(&e).unwrap().refs += 1;
        }
        fn release(&mut self, e: EffectPtr) {
            self.log.push(format!("release {e}"));
            let f = self.fx.get_mut(&e).expect("released after delete");
            f.refs -= 1;
            if f.refs == 0 {
                self.fx.remove(&e);
                self.log.push(format!("delete {e}"));
            }
        }
        fn set_up(&mut self, e: EffectPtr) -> i32 {
            self.log.push(format!("setup {e}"));
            self.fx[&e].kind
        }
        fn step(&mut self, e: EffectPtr) -> i32 {
            self.log.push(format!("step {e}"));
            let f = self.fx.get_mut(&e).unwrap();
            let spawn = f.spawns.take();
            let r = if f.steps.is_empty() { 0 } else { f.steps.remove(0) };
            if let Some(kind) = spawn {
                let n = self.new_fx(Fx { kind, saved: true, ..Fx::default() });
                self.spawned.push(n);
            }
            r
        }
        fn repair(&mut self, e: EffectPtr) -> i32 {
            self.log.push(format!("repair {e}"));
            self.fx[&e].repair
        }
        fn above_detail(&mut self, e: EffectPtr) -> bool {
            self.fx[&e].above
        }
        fn new_loaded(&mut self, kind: u32) -> EffectPtr {
            if kind == 0x11 || kind > 0x47 {
                return 0;
            }
            self.new_fx(Fx { kind: kind as i32, saved: true, ..Fx::default() })
        }
        fn load<F: EffectFile>(&mut self, e: EffectPtr, file: &mut F) -> i32 {
            let mut handle = 0;
            let mut r = file.read_u32(&mut handle);
            let mut data = 0;
            if r == 0 {
                r = file.read_u32(&mut data);
            }
            let f = self.fx.get_mut(&e).unwrap();
            f.handle = handle;
            f.data = data;
            r
        }
        fn save<F: EffectFile>(&mut self, e: EffectPtr, file: &mut F) -> i32 {
            let f = self.fx[&e].clone();
            let r = file.write_u32(f.handle);
            if r != 0 {
                return r;
            }
            file.write_u32(f.data)
        }
        fn goals_update(&mut self) {
            self.log.push("goals".into());
        }
        fn goals_load<F: EffectFile>(&mut self, file: &mut F) -> i32 {
            let mut g = 0;
            let r = file.read_i32(&mut g);
            self.goals = g;
            r
        }
        fn goals_repair(&mut self) -> i32 {
            self.log.push("goals repair".into());
            self.goals
        }
    }

    #[derive(Default)]
    struct Mem {
        items: Vec<(char, u32)>,
        at: usize,
        fail_at: Option<usize>,
    }

    impl Mem {
        fn read(&mut self, ty: char) -> Result<u32, i32> {
            if self.fail_at == Some(self.at) || self.at >= self.items.len() {
                return Err(5);
            }
            let (t, v) = self.items[self.at];
            assert_eq!(t, ty, "item {}", self.at);
            self.at += 1;
            Ok(v)
        }
        fn write(&mut self, ty: char, v: u32) -> i32 {
            if self.fail_at == Some(self.items.len()) {
                // one failure: the next write goes through
                self.fail_at = None;
                return 5;
            }
            self.items.push((ty, v));
            0
        }
    }

    impl EffectFile for Mem {
        fn is_open(&mut self) -> bool {
            true
        }
        fn read_u32(&mut self, v: &mut u32) -> i32 {
            self.read('u').map(|x| *v = x).err().unwrap_or(0)
        }
        fn read_i32(&mut self, v: &mut i32) -> i32 {
            self.read('i').map(|x| *v = x as i32).err().unwrap_or(0)
        }
        fn read_f32(&mut self, v: &mut f32) -> i32 {
            self.read('f').map(|x| *v = f32::from_bits(x)).err().unwrap_or(0)
        }
        fn read_bool(&mut self, v: &mut bool) -> i32 {
            self.read('b').map(|x| *v = x != 0).err().unwrap_or(0)
        }
        fn write_u32(&mut self, v: u32) -> i32 {
            self.write('u', v)
        }
        fn write_i32(&mut self, v: i32) -> i32 {
            self.write('i', v as u32)
        }
        fn write_f32(&mut self, v: f32) -> i32 {
            self.write('f', v.to_bits())
        }
        fn write_bool(&mut self, v: bool) -> i32 {
            self.write('b', v as u32)
        }
    }

    fn list(s: &Plain) -> Vec<EffectPtr> {
        let mut v = Vec::new();
        let mut e = s.0.head();
        while e != 0 {
            v.push(e);
            e = s.0.next(e);
        }
        v
    }

    fn made(s: &mut Plain, h: &mut Host, kind: i32) -> EffectPtr {
        let e = h.new_fx(Fx { kind, saved: true, ..Fx::default() });
        let p = plan(EeGfxEffectEntry::Placed, kind, true, false, false);
        create(s, h, e, p.saved);
        e
    }

    #[test]
    fn plans_follow_the_entry_points() {
        use EeGfxEffectEntry::*;
        let c = |saved| EeGfxEffectPlan { create: true, saved, result: 0 };
        let n = |result| EeGfxEffectPlan { create: false, saved: true, result };
        // above the detail setting: nothing, 0, whatever the kind
        assert_eq!(plan(Unit, 4, false, true, true), n(0));
        assert_eq!(plan(Unit, 99, false, true, true), n(0));
        // unknown kinds: 10, but 0 for the point pair and the weather
        for entry in [Unit, BetweenUnits, UnitToPoint, Placed, Transformation, Line] {
            assert_eq!(plan(entry, 0x11, true, true, true), n(10), "{entry:?}");
            assert_eq!(plan(entry, -1, true, true, true), n(10), "{entry:?}");
        }
        assert_eq!(plan(Between, 0x11, true, true, true), n(0));
        assert_eq!(plan(Weather, 4, true, true, true), n(0));
        // a unit's kinds with and without the living unit
        assert_eq!(plan(Unit, 4, true, false, false), c(true));
        assert_eq!(plan(Unit, 9, true, false, false), n(10));
        assert_eq!(plan(Unit, 9, true, true, false), c(true));
        assert_eq!(plan(Unit, 0x3b, true, false, false), c(false));
        // two units: both, the first, none
        assert_eq!(plan(BetweenUnits, 0x40, true, true, false), n(10));
        assert_eq!(plan(BetweenUnits, 0x40, true, true, true), c(true));
        assert_eq!(plan(BetweenUnits, 0x16, true, true, false), c(true));
        assert_eq!(plan(BetweenUnits, 0x26, true, false, false), c(true));
        assert_eq!(plan(UnitToPoint, 0x41, true, false, false), c(true));
        assert_eq!(plan(UnitToPoint, 0x43, true, false, false), n(10));
        assert_eq!(plan(Placed, 0x3b, true, false, false), c(false));
        assert_eq!(plan(Placed, 0x3c, true, false, false), c(true));
        assert_eq!(plan(Line, 0x3b, true, false, false), c(false));
        assert_eq!(plan(Transformation, 0x32, true, false, false), c(true));
        assert_eq!(plan(Between, 0x2d, true, false, false), c(true));
        assert_eq!(plan(Weather, 0x3d, true, false, false), c(true));
        // every kind each entry creates (the C++ switches' cases)
        let count = |entry| (-1..0x60).filter(|&k| needs(entry, k).is_some()).count();
        assert_eq!(
            [Unit, BetweenUnits, UnitToPoint, Placed, Transformation, Line, Between, Weather].map(count),
            [37, 6, 6, 17, 2, 5, 3, 2]
        );
    }

    #[test]
    fn creation_sets_up_then_adds_under_a_new_handle() {
        let mut s = Plain(EffectStore::new());
        let mut h = Host::default();
        assert_eq!(create(&mut s, &mut h, 0, true), (0, None));
        let a = h.new_fx(Fx { kind: 0x3b, saved: true, handle: 77, ..Fx::default() });
        assert_eq!(create(&mut s, &mut h, a, false), (0x3b, Some(1)));
        assert_eq!(h.log, vec![format!("unsaved {a}"), format!("setup {a}")]);
        assert!(!h.fx[&a].saved);
        assert_eq!(h.fx[&a].refs, 2);
        let b = made(&mut s, &mut h, 6);
        assert_eq!(h.fx[&b].handle, 2);
        assert_eq!(list(&s), vec![b, a]);
    }

    #[test]
    fn update_steps_newest_first_and_removes_finished_effects() {
        let mut s = Plain(EffectStore::new());
        let mut h = Host::default();
        let a = made(&mut s, &mut h, 4);
        let b = made(&mut s, &mut h, 5);
        let c = made(&mut s, &mut h, 6);
        h.fx.get_mut(&b).unwrap().steps = vec![7];
        h.fx.get_mut(&c).unwrap().spawns = Some(8);
        // the queue: a's handle, a handle with no effect, no handle
        s.0.queue_destroy(1);
        s.0.queue_destroy(40);
        s.0.queue_destroy(0);
        h.log.clear();
        update(&mut s, &mut h);
        // the effect c's step made, registered after the walk
        for e in std::mem::take(&mut h.spawned) {
            create(&mut s, &mut h, e, true);
        }
        assert_eq!(
            h.log,
            vec![
                "goals".to_string(),
                format!("release {a}"),
                format!("release {a}"),
                format!("delete {a}"),
                format!("step {c}"),
                format!("step {b}"),
                format!("release {b}"),
                format!("release {b}"),
                format!("delete {b}"),
                format!("setup {}", h.next_ptr),
            ]
        );
        assert_eq!(list(&s), vec![h.next_ptr, c]);
        assert_eq!(s.0.count(), 2);
        assert_eq!(s.0.destroyed_len(), 0);
        // the queue's result: the last handle looked at
        s.0.queue_destroy(3);
        assert_eq!(destroy_queued(&mut s, &mut h), 3);
        s.0.queue_destroy(0);
        assert_eq!(destroy_queued(&mut s, &mut h), 9);
        assert_eq!(destroy_queued(&mut s, &mut h), 0);
    }

    #[test]
    fn repair_drops_lost_and_detailed_effects_and_stops_at_errors() {
        let mut s = Plain(EffectStore::new());
        let mut h = Host::default();
        let a = made(&mut s, &mut h, 4);
        let b = made(&mut s, &mut h, 5);
        let c = made(&mut s, &mut h, 6);
        let d = made(&mut s, &mut h, 7);
        h.fx.get_mut(&a).unwrap().repair = 7;
        h.fx.get_mut(&b).unwrap().above = true;
        h.fx.get_mut(&c).unwrap().repair = 3;
        h.log.clear();
        assert_eq!(repair(&mut s, &mut h), 3);
        assert_eq!(
            h.log,
            vec![
                format!("repair {a}"),
                format!("release {a}"),
                format!("release {a}"),
                format!("delete {a}"),
                format!("repair {b}"),
                format!("release {b}"),
                format!("release {b}"),
                format!("delete {b}"),
                format!("repair {c}"),
            ]
        );
        assert_eq!(list(&s), vec![d, c]);
        h.fx.get_mut(&c).unwrap().repair = 0;
        h.goals = 4;
        h.log.clear();
        assert_eq!(repair(&mut s, &mut h), 4);
        assert_eq!(h.log, vec![format!("repair {c}"), format!("repair {d}"), "goals repair".to_string()]);
        clear(&mut s, &mut h);
        assert_eq!((s.0.count(), s.0.list_len()), (0, 0));
        assert!(h.fx.is_empty());
    }

    #[test]
    fn save_and_load_round_trip() {
        let mut s = Plain(EffectStore::new());
        let mut h = Host::default();
        let a = made(&mut s, &mut h, 4);
        let b = made(&mut s, &mut h, 0x3b);
        let c = made(&mut s, &mut h, 6);
        assert!(!h.fx[&b].saved);
        h.fx.get_mut(&a).unwrap().data = 11;
        h.fx.get_mut(&c).unwrap().data = 33;
        s.0.queue_destroy(9);
        s.0.queue_destroy(2);
        let mut f = Mem::default();
        assert_eq!(save(&mut s, &mut h, &mut f), 0);
        assert_eq!(
            f.items,
            vec![
                ('u', 3),
                ('i', 2),
                ('u', 4),
                ('u', 1),
                ('u', 11),
                ('u', 6),
                ('u', 3),
                ('u', 33),
                ('u', 2),
                ('u', 9),
                ('u', 2)
            ]
        );
        // the goal markers' read sits before the queue
        f.items.insert(8, ('i', 0));
        let mut t = Plain(EffectStore::new());
        let mut g = Host::default();
        assert_eq!(load(&mut t, &mut g, &mut f, 0, 0x10c), 0);
        assert_eq!(t.0.last_handle(), 3);
        let l = list(&t);
        assert_eq!(
            l.iter().map(|e| (g.fx[e].kind, g.fx[e].handle, g.fx[e].data)).collect::<Vec<_>>(),
            vec![(6, 3, 33), (4, 1, 11)]
        );
        assert_eq!((t.0.find(1), t.0.find(3)), (l[1], l[0]));
        assert_eq!((t.0.destroyed_len(), t.0.destroyed_at(0), t.0.destroyed_at(1)), (2, 9, 2));
    }

    #[test]
    fn old_saves_give_handles_anew_and_errors_stop_the_load() {
        // 0.200 and before: an effect without a handle or past the counter takes a new one
        let file = |items: Vec<(char, u32)>| Mem { items, ..Mem::default() };
        // two effects (no handle; handle 9 past the counter 5), then kind 0x11, which has no load-time effect and
        // reads nothing more, then the goal markers and a queue of one handle 0 (not queued)
        let items2 = vec![
            ('u', 5),
            ('i', 3),
            ('u', 4),
            ('u', 0),
            ('u', 1),
            ('u', 4),
            ('u', 9),
            ('u', 2),
            ('u', 0x11),
            ('i', 0),
            ('u', 1),
            ('u', 0),
        ];
        let mut s = Plain(EffectStore::new());
        let mut h = Host::default();
        assert_eq!(load(&mut s, &mut h, &mut file(items2.clone()), 0, 0xc8), 0);
        let got: Vec<_> = list(&s).iter().map(|e| (h.fx[e].kind, h.fx[e].handle)).collect();
        assert_eq!(got, vec![(4, 7), (4, 6)]);
        assert_eq!(s.0.last_handle(), 7);
        assert_eq!(s.0.destroyed_len(), 0);
        // later saves keep the handles
        let mut s = Plain(EffectStore::new());
        let mut h = Host::default();
        assert_eq!(load(&mut s, &mut h, &mut file(items2), 0, 0xc9), 0);
        let got: Vec<_> = list(&s).iter().map(|e| (h.fx[e].kind, h.fx[e].handle)).collect();
        assert_eq!(got, vec![(4, 9), (4, 0)]);
        // an effect's failed load: it is still added, and the load stops there
        let mut s = Plain(EffectStore::new());
        let mut h = Host::default();
        let mut f = file(vec![('u', 5), ('i', 2), ('u', 4), ('u', 3)]);
        assert_eq!(load(&mut s, &mut h, &mut f, 0, 0x10c), 5);
        assert_eq!(s.0.list_len(), 1);
        // a failed counter read still sets the counter (to the local's value)
        let mut s = Plain(EffectStore::new());
        s.0.set_last_handle(9);
        assert_eq!(load(&mut s, &mut h, &mut file(vec![]), 0, 0x10c), 5);
        assert_eq!(s.0.last_handle(), 0);
        // a negative count loads nothing and goes on to the goals
        let mut s = Plain(EffectStore::new());
        assert_eq!(
            load(&mut s, &mut h, &mut file(vec![('u', 1), ('i', (-2i32) as u32), ('i', 0), ('u', 0)]), 0, 0x10c),
            0
        );
    }

    #[test]
    fn a_save_error_can_be_overwritten_by_the_next_effect() {
        let mut s = Plain(EffectStore::new());
        let mut h = Host::default();
        made(&mut s, &mut h, 4);
        made(&mut s, &mut h, 5);
        // the first effect's kind write fails; the second's succeed: the save goes on to the queue
        let mut f = Mem { fail_at: Some(2), ..Mem::default() };
        assert_eq!(save(&mut s, &mut h, &mut f), 0);
        assert_eq!(f.items, vec![('u', 2), ('i', 2), ('u', 5), ('u', 2), ('u', 0), ('u', 0)]);
        // the last effect's error stops it before the queue
        let mut f = Mem { fail_at: Some(5), ..Mem::default() };
        assert_eq!(save(&mut s, &mut h, &mut f), 5);
        assert_eq!(f.items.len(), 5);
    }
}
