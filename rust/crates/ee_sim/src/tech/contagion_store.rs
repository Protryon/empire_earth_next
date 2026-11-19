//! The contagion manager and its helpers whole (`EEContagionManager`, `EEContagionHelper`; calamities lane, round 4):
//! the helpers' ids, timers, held units (by handle, each referenced), loaded unit ids, functors and calamities; the
//! manager's helper list, next id and loaded calamity indexes; every member function: creating, finding, destroying
//! and swapping, a unit's infection and cure bookkeeping, the update and spread (over [`contagion::update`]), the
//! checksums, the out-of-sync dump, `Load`, `Save` and `RestorePointers`.
//!
//! What stays C++ is the world: a unit's reference count, its calamity flags, the functors' `Infect`, `Cure`,
//! `Apply` and `Update`, the near-unit search, the immunity effect, the physics random draw, the distance between
//! two units, a unit by id, the calamity's own checksum, index, functor and dump, the file, and the capture's events.
//! [`ContagionWorld`] is that host; `EEContagionManager.cpp`'s `ee_contagion_world` answers it in the game. The units'
//! plain reads (alive, complex, id, player, enemies) are the world views'.
//!
//! The manager's helpers are boxed and listed by pointer, as the C++'s were: `operator=` of the persistent chunk
//! copies the list, not the helpers, and the manager's destructor does not delete them (the original's).

// the record keeps the C++ member names; c_long/c_ulong are 32 bits on Windows
#![allow(non_snake_case, clippy::unnecessary_cast)]

use std::ffi::c_void;
use std::os::raw::{c_long, c_ulong};

use crate::tech::contagion::{
    self, ContagionHost, EeContagionCall, EeContagionOp, EeContagionParams, EeContagionReply, EeContagionState,
    EE_CONTAGION_NEAR_NOT_ENEMY, EE_CONTAGION_NEAR_SAME_PLAYER, EE_CONTAGION_NEAR_TYPE, EE_CONTAGION_NEAR_UNIT,
    EE_CONTAGION_NEW_COMPLEX, EE_CONTAGION_NEW_HELD,
};
use crate::tech::contagion_save::{
    helper_checksum, helper_load, helper_save, manager_load, manager_save, ContagionIo, EeContagionHelperFields,
    EE_CONTAGION_IO_CLEAR, EE_CONTAGION_IO_HELPER_CALAMITY, EE_CONTAGION_IO_HELPER_COUNT, EE_CONTAGION_IO_LOAD_HELPER,
    EE_CONTAGION_IO_PUSH_UNIT_ID, EE_CONTAGION_IO_READ_FLOAT, EE_CONTAGION_IO_READ_LONG, EE_CONTAGION_IO_READ_ULONG,
    EE_CONTAGION_IO_SAVE_HELPER, EE_CONTAGION_IO_UNIT_COUNT, EE_CONTAGION_IO_UNIT_ID, EE_CONTAGION_IO_WRITE_FLOAT,
    EE_CONTAGION_IO_WRITE_LONG, EE_CONTAGION_IO_WRITE_ULONG,
};

/// The capture's contagion events (`EEContagionCaptureOp`, ee/EETechCapture.h) the store sends itself.
pub const EE_CONTAGION_EVENT_UPDATE_UNITS: i32 = 2;
pub const EE_CONTAGION_EVENT_NEW: i32 = 4;
pub const EE_CONTAGION_EVENT_APPLY: i32 = 5;
pub const EE_CONTAGION_EVENT_REMOVE: i32 = 6;
pub const EE_CONTAGION_EVENT_FUNCTOR_UPDATE: i32 = 10;

/// What a unit handle's plain members say (the world views' reads; a fake world's in the tests).
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct ContagionUnit {
    /// `mHitPoints > 0`.
    pub alive: bool,
    /// `mComplexUnit` (0 for none).
    pub complex: u64,
    /// `mUnitID`.
    pub id: c_ulong,
    /// `mPlayer`.
    pub player: u64,
}

/// The world a helper works on (module docs). Units, functors, calamities and files are the C++ objects' addresses.
pub trait ContagionWorld {
    /// The unit's plain members.
    fn unit(&mut self, unit: u64) -> ContagionUnit;
    /// `mPlayer->mEnemies[inOther's player id] != 0` of the near unit's player against the held unit's.
    fn enemies(&mut self, player: u64, other_player: u64) -> bool;
    /// `Reference()` (`inOn`) or `Dereference()`.
    fn reference(&mut self, unit: u64, on: bool);
    /// The calamity class's flags set on (`inOn`) or cleared from the unit's complex unit, if it has one.
    fn mark(&mut self, unit: u64, calamity: u64, on: bool);
    /// The functor's `Infect`.
    fn infect(&mut self, functor: u64, unit: u64, calamity: u64);
    /// The functor's `Cure`.
    fn cure(&mut self, functor: u64, unit: u64, calamity: u64);
    /// The functor's `Apply`.
    fn apply(&mut self, functor: u64, unit: u64, calamity: u64, elapsed: f32);
    /// The functor's `Update`.
    fn functor_update(&mut self, functor: u64, calamity: u64, elapsed: f32);
    /// `EEContagionHelper::GetUnitsInArea`: the complex units around the complex unit within the radius (none while
    /// it is carried), into `out` (cleared first by the caller).
    fn near(&mut self, unit: u64, radius: f32, out: &mut Vec<u64>);
    /// The functor's `CanInfectType` of the unit's class.
    fn can_infect(&mut self, functor: u64, unit: u64) -> bool;
    /// The unit's immunity effect (area effect 3) shown, if it has one: whether it is immune.
    fn immune(&mut self, unit: u64) -> bool;
    /// `gPhysics->mRandom.Generate()`.
    fn random(&mut self) -> f32;
    /// The squared distance between the two units' transformations.
    fn distance_squared(&mut self, unit: u64, other: u64) -> f32;
    /// `gWorld.GetUnit(inID)` (0 for none).
    fn unit_by_id(&mut self, id: c_ulong) -> u64;
    /// The calamity's `GetOutOfSynchChecksum`.
    fn calamity_checksum(&mut self, calamity: u64) -> c_long;
    /// The calamity's index among the active calamities (None if it is not active).
    fn calamity_index(&mut self, calamity: u64) -> Option<c_ulong>;
    /// The calamity's class's contagion functor.
    fn calamity_functor(&mut self, calamity: u64) -> u64;
    /// The calamity's `DumpOutOfSynch` to the file.
    fn calamity_dump(&mut self, calamity: u64, file: u64);
    /// The text written to the out-of-sync dump file.
    fn text(&mut self, file: u64, text: &str);
    /// A file operation (`EE_CONTAGION_IO_READ_*`, `EE_CONTAGION_IO_WRITE_*`) on `value`; the `FSFile` result.
    fn file(&mut self, file: u64, op: u32, value: *mut c_void) -> c_long;
    /// A capture event (TEST ONLY; nothing outside a capture).
    fn capture(&mut self, kind: i32, a: i32, b: i32, x: f32, y: f32);
}

/// One contagion helper (`EEContagionHelper`).
#[derive(Debug, Default)]
pub struct EeContagionHelper {
    pub fields: EeContagionHelperFields,
    /// The held units (`mUnits`, each referenced).
    pub units: Vec<u64>,
    /// The loaded unit ids (`mUnitIDs`, until `RestorePointers`).
    pub unit_ids: Vec<c_ulong>,
    /// `mFunctor`.
    pub functor: u64,
    /// `mCalamity`.
    pub calamity: u64,
}

/// The contagion manager (`EEContagionManager`).
#[derive(Debug, Default, Clone)]
pub struct EeContagionManager {
    /// `mHelpers` (owned by whoever destroys them: `DestroyContagionHelper`, `Load`'s clear).
    pub helpers: Vec<*mut EeContagionHelper>,
    /// `mNextHelperID` (the last id handed out).
    pub next_id: c_long,
    /// `mCalamityIndexes` (each loaded helper's calamity index, until `RestorePointers`).
    pub calamity_indexes: Vec<c_ulong>,
}

impl EeContagionHelper {
    /// `EEContagionHelper(inFunctor, inCalamity)`: no units, no id, both timers 0.
    pub fn new(functor: u64, calamity: u64) -> Self {
        EeContagionHelper {
            fields: EeContagionHelperFields { mHelperID: -1, mTimeSinceLastSpread: 0.0, mTimeSinceLastUpdate: 0.0 },
            functor,
            calamity,
            ..Default::default()
        }
    }

    /// `AddUnit`: a live unit not yet held is referenced, marked and infected if asked; whether it was added.
    pub fn add_unit<W: ContagionWorld>(&mut self, w: &mut W, unit: u64, notify: bool) -> bool {
        if self.units.contains(&unit) || !w.unit(unit).alive {
            return false;
        }
        w.reference(unit, true);
        w.mark(unit, self.calamity, true);
        self.units.push(unit);
        if notify {
            w.infect(self.functor, unit, self.calamity);
        }
        true
    }

    /// `RemoveUnit(iterator)`: the held unit at `index` unmarked, cured if asked, released and erased.
    pub fn remove_at<W: ContagionWorld>(&mut self, w: &mut W, index: usize, notify: bool) {
        let unit = self.units[index];
        w.mark(unit, self.calamity, false);
        if notify {
            w.cure(self.functor, unit, self.calamity);
        }
        w.reference(unit, false);
        self.units.remove(index);
    }

    /// `RemoveUnit(inUnit)`: whether it was held.
    pub fn remove_unit<W: ContagionWorld>(&mut self, w: &mut W, unit: u64, notify: bool) -> bool {
        match self.units.iter().position(|&u| u == unit) {
            Some(i) => {
                self.remove_at(w, i, notify);
                true
            }
            None => false,
        }
    }

    /// `~EEContagionHelper`: every held unit removed and cured.
    pub fn release<W: ContagionWorld>(&mut self, w: &mut W) {
        while !self.units.is_empty() {
            self.remove_at(w, 0, true);
        }
        self.calamity = 0;
    }

    /// `SwapUnit`: the new unit for the old one, without a cure or an infection.
    pub fn swap_unit<W: ContagionWorld>(&mut self, w: &mut W, old: u64, new: u64) {
        if self.units.contains(&old) {
            self.remove_unit(w, old, false);
            self.add_unit(w, new, false);
        }
    }

    /// `GetOutOfSynchChecksum`.
    pub fn checksum<W: ContagionWorld>(&self, w: &mut W) -> c_long {
        let mut ids: c_long = 0;
        for &u in &self.units {
            ids = ids.wrapping_add(w.unit(u).id as c_long);
        }
        let calamity = w.calamity_checksum(self.calamity);
        helper_checksum(&self.fields, ids, calamity)
    }

    /// `DumpOutOfSynch`.
    pub fn dump<W: ContagionWorld>(&self, w: &mut W, file: u64) {
        let mut s = format!(
            "\nOUT OF SYNCH DUMP:  Contangion Helper id is {}\nOUT OF SYNCH DUMP:  Contangion Helper time since last \
             spread: {:.6}\nOUT OF SYNCH DUMP:  Contangion Helper unit list size is {}\n",
            self.fields.mHelperID,
            self.fields.mTimeSinceLastSpread,
            self.units.len()
        );
        for (i, &u) in self.units.iter().enumerate() {
            s += &format!("OUT OF SYNCH DUMP:  Contangion Helper Unit ID for index {} is #{:08x}\n", i, w.unit(u).id);
        }
        w.text(file, &s);
        w.calamity_dump(self.calamity, file);
    }

    /// `RestorePointers`: the loaded ids that still name units, marked and referenced; the ids cleared.
    pub fn restore<W: ContagionWorld>(&mut self, w: &mut W) {
        for id in std::mem::take(&mut self.unit_ids) {
            let unit = w.unit_by_id(id);
            // a unit that is gone cannot be referenced (it used to be dereferenced through NULL on load)
            if unit == 0 {
                continue;
            }
            w.mark(unit, self.calamity, true);
            w.reference(unit, true);
            self.units.push(unit);
        }
    }

    /// `Update`: the functor's numbers, the timers and the spread ([`contagion::update`]) over this helper.
    pub fn update<W: ContagionWorld>(&mut self, w: &mut W, params: &EeContagionParams, elapsed: f32) {
        let mut state = EeContagionState {
            since_spread: self.fields.mTimeSinceLastSpread,
            since_update: self.fields.mTimeSinceLastUpdate,
        };
        contagion::update(
            &mut UpdateHost { helper: self, world: w, near: Vec::new(), new: Vec::new() },
            &mut state,
            params,
            elapsed,
        );
        self.fields.mTimeSinceLastSpread = state.since_spread;
        self.fields.mTimeSinceLastUpdate = state.since_update;
    }

    /// `Load`: the unit ids, the id, the timers (the update timer from 0.231 on); the first read error.
    pub fn load<W: ContagionWorld>(&mut self, w: &mut W, file: u64, major: c_ulong, minor: c_ulong) -> c_long {
        let mut fields = self.fields;
        let result = helper_load(&mut HelperIo { helper: self, world: w, file }, &mut fields, major, minor);
        self.fields = fields;
        result
    }

    /// `Save`: the unit ids, the id, both timers; the first write error.
    pub fn save<W: ContagionWorld>(&mut self, w: &mut W, file: u64) -> c_long {
        let fields = self.fields;
        helper_save(&mut HelperIo { helper: self, world: w, file }, &fields)
    }
}

/// The helper's update host: `contagion::update`'s ops on the helper, its near and new units.
struct UpdateHost<'a, W: ContagionWorld> {
    helper: &'a mut EeContagionHelper,
    world: &'a mut W,
    near: Vec<u64>,
    new: Vec<u64>,
}

impl<W: ContagionWorld> ContagionHost for UpdateHost<'_, W> {
    fn call(&mut self, c: &EeContagionCall) -> EeContagionReply {
        let mut reply = EeContagionReply::default();
        let h = &mut *self.helper;
        let w = &mut *self.world;
        let at = |i: i32| i as usize;
        match c.op {
            EeContagionOp::UpdateUnits => {
                // (every functor's UpdateUnits was empty)
                w.capture(EE_CONTAGION_EVENT_UPDATE_UNITS, 0, 0, 0.0, 0.0);
            }
            EeContagionOp::FunctorUpdate => {
                w.capture(EE_CONTAGION_EVENT_FUNCTOR_UPDATE, 0, 0, 0.0, 0.0);
                w.functor_update(h.functor, h.calamity, c.x);
            }
            EeContagionOp::UnitCount => reply.value = h.units.len() as i32,
            EeContagionOp::UnitAlive => reply.ok = w.unit(h.units[at(c.index)]).alive,
            EeContagionOp::Apply => {
                let unit = h.units[at(c.index)];
                let id = w.unit(unit).id;
                w.capture(EE_CONTAGION_EVENT_APPLY, id as i32, 0, 0.0, 0.0);
                w.apply(h.functor, unit, h.calamity, c.x);
            }
            EeContagionOp::Remove => {
                let id = w.unit(h.units[at(c.index)]).id;
                w.capture(EE_CONTAGION_EVENT_REMOVE, id as i32, 0, 0.0, 0.0);
                h.remove_at(w, at(c.index), true);
            }
            EeContagionOp::HeldComplex => reply.ok = w.unit(h.units[at(c.index)]).complex != 0,
            EeContagionOp::NearUnits => {
                self.near.clear();
                let complex = w.unit(h.units[at(c.index)]).complex;
                w.near(complex, c.x, &mut self.near);
                reply.value = self.near.len() as i32;
            }
            EeContagionOp::NearView => {
                let near = self.near[at(c.index)];
                if near == 0 {
                    return reply;
                }
                let complex = w.unit(h.units[at(c.index2)]).complex;
                let held = w.unit(complex);
                let near_unit = w.unit(near);
                reply.id = near_unit.id as u32;
                reply.flags = EE_CONTAGION_NEAR_UNIT;
                if w.can_infect(h.functor, near) {
                    reply.flags |= EE_CONTAGION_NEAR_TYPE;
                }
                if held.player == near_unit.player {
                    reply.flags |= EE_CONTAGION_NEAR_SAME_PLAYER;
                }
                if !w.enemies(near_unit.player, held.player) {
                    reply.flags |= EE_CONTAGION_NEAR_NOT_ENEMY;
                }
            }
            EeContagionOp::Random => reply.x = w.random(),
            EeContagionOp::DistanceSquared => {
                let held = w.unit(h.units[at(c.index2)]).complex;
                reply.x = w.distance_squared(held, self.near[at(c.index)]);
            }
            EeContagionOp::AddNew => self.new.push(self.near[at(c.index)]),
            EeContagionOp::NewView => {
                let unit = w.unit(self.new[at(c.index)]);
                reply.id = unit.id as u32;
                if unit.complex != 0 {
                    reply.flags |= EE_CONTAGION_NEW_COMPLEX;
                }
                if h.units.contains(&unit.complex) {
                    reply.flags |= EE_CONTAGION_NEW_HELD;
                }
            }
            EeContagionOp::Immune => {
                let unit = self.new[at(c.index)];
                // (the C++ answered the capture's event inside its effect test: the id, 2)
                if w.immune(unit) {
                    reply.ok = true;
                }
            }
            EeContagionOp::AddUnit => {
                let unit = self.new[at(c.index)];
                let id = w.unit(unit).id;
                w.capture(EE_CONTAGION_EVENT_NEW, id as i32, 1, 0.0, 0.0);
                h.add_unit(w, unit, true);
            }
            EeContagionOp::Capture => w.capture(c.value, c.index, c.index2, c.x, c.y),
        }
        reply
    }
}

/// A helper's file and lists for `helper_load` and `helper_save`.
struct HelperIo<'a, W: ContagionWorld> {
    helper: &'a mut EeContagionHelper,
    world: &'a mut W,
    file: u64,
}

fn file_op<W: ContagionWorld>(w: &mut W, file: u64, op: u32, value: *mut c_void) -> Option<c_long> {
    match op {
        EE_CONTAGION_IO_READ_ULONG
        | EE_CONTAGION_IO_READ_LONG
        | EE_CONTAGION_IO_READ_FLOAT
        | EE_CONTAGION_IO_WRITE_ULONG
        | EE_CONTAGION_IO_WRITE_LONG
        | EE_CONTAGION_IO_WRITE_FLOAT => Some(w.file(file, op, value)),
        _ => None,
    }
}

/// `kErrorOK` and `kErrorFailed`.
const OK: c_long = 0;
const FAILED: c_long = 1;

impl<W: ContagionWorld> ContagionIo for HelperIo<'_, W> {
    fn io(&mut self, op: u32, value: *mut c_void) -> c_long {
        if let Some(result) = file_op(self.world, self.file, op, value) {
            return result;
        }
        let v = value as *mut c_ulong;
        // SAFETY: the ops' contract (contagion_save.rs): `value` is a c_ulong
        unsafe {
            match op {
                EE_CONTAGION_IO_PUSH_UNIT_ID => self.helper.unit_ids.push(*v),
                EE_CONTAGION_IO_UNIT_COUNT => *v = self.helper.units.len() as c_ulong,
                EE_CONTAGION_IO_UNIT_ID => *v = self.world.unit(self.helper.units[*v as usize]).id,
                _ => return FAILED,
            }
        }
        OK
    }
}

/// The manager's file and lists for `manager_load` and `manager_save`.
struct ManagerIo<'a, W: ContagionWorld> {
    manager: &'a mut EeContagionManager,
    world: &'a mut W,
    file: u64,
    major: c_ulong,
    minor: c_ulong,
}

impl<W: ContagionWorld> ContagionIo for ManagerIo<'_, W> {
    fn io(&mut self, op: u32, value: *mut c_void) -> c_long {
        if let Some(result) = file_op(self.world, self.file, op, value) {
            return result;
        }
        let v = value as *mut c_ulong;
        let m = &mut *self.manager;
        // SAFETY: the ops' contract (contagion_save.rs): `value` is a c_ulong (or null for the clear); the listed
        // helpers are live boxes
        unsafe {
            match op {
                EE_CONTAGION_IO_CLEAR => {
                    for h in std::mem::take(&mut m.helpers) {
                        destroy(self.world, h);
                    }
                    m.calamity_indexes.clear();
                }
                EE_CONTAGION_IO_LOAD_HELPER => {
                    let h = Box::into_raw(Box::new(EeContagionHelper::default()));
                    (*h).load(self.world, self.file, self.major, self.minor);
                    m.helpers.push(h);
                    m.calamity_indexes.push(*v);
                }
                EE_CONTAGION_IO_HELPER_COUNT => *v = m.helpers.len() as c_ulong,
                EE_CONTAGION_IO_HELPER_CALAMITY => {
                    match self.world.calamity_index((*m.helpers[*v as usize]).calamity) {
                        Some(index) => *v = index,
                        None => return FAILED,
                    }
                }
                EE_CONTAGION_IO_SAVE_HELPER => return (*m.helpers[*v as usize]).save(self.world, self.file),
                _ => return FAILED,
            }
        }
        OK
    }
}

/// A listed helper deleted (`delete theHelper`: its units cured and released).
///
/// # Safety
/// `h` is a live box of this module, listed nowhere after this.
pub(crate) unsafe fn destroy<W: ContagionWorld>(w: &mut W, h: *mut EeContagionHelper) {
    // SAFETY: the caller's contract
    let mut helper = unsafe { Box::from_raw(h) };
    helper.release(w);
}

impl EeContagionManager {
    /// The helper at `i` of the list.
    #[allow(clippy::mut_from_ref)]
    pub fn at(&self, i: usize) -> &mut EeContagionHelper {
        // SAFETY: a listed helper is a live box (module docs)
        let h: *mut EeContagionHelper = self.helpers[i];
        unsafe { &mut *h }
    }

    /// `GetContagionHelper`: the helper of the id.
    pub fn find(&self, id: c_long) -> Option<*mut EeContagionHelper> {
        // SAFETY: as `at`
        self.helpers.iter().copied().find(|&h| unsafe { (*h).fields.mHelperID } == id)
    }

    /// `CreateContagionHelper`: a helper with the next id, the target unit (if any) infected; the id.
    pub fn create<W: ContagionWorld>(&mut self, w: &mut W, functor: u64, calamity: u64, target: u64) -> c_long {
        let mut helper = Box::new(EeContagionHelper::new(functor, calamity));
        self.next_id = self.next_id.wrapping_add(1);
        helper.fields.mHelperID = self.next_id;
        if target != 0 {
            helper.add_unit(w, target, true);
        }
        self.helpers.push(Box::into_raw(helper));
        self.next_id
    }

    /// `DestroyContagionHelper`: the helper of the id deleted (nothing while a loaded game awaits its pointers).
    pub fn destroy<W: ContagionWorld>(&mut self, w: &mut W, id: c_long) -> bool {
        if !self.calamity_indexes.is_empty() {
            return true;
        }
        if let Some(i) = self.helpers.iter().position(|&h| unsafe { (*h).fields.mHelperID } == id) {
            let h = self.helpers.remove(i);
            // SAFETY: a listed helper, now unlisted
            unsafe { destroy(w, h) };
        }
        true
    }

    /// `GetOutOfSynchChecksum`: the helpers' checksums plus the last id.
    pub fn checksum<W: ContagionWorld>(&self, w: &mut W) -> c_long {
        let mut sum: c_long = 0;
        for i in 0..self.helpers.len() {
            sum = sum.wrapping_add(self.at(i).checksum(w));
        }
        self.next_id.wrapping_add(sum)
    }

    /// `DumpOutOfSynch`.
    pub fn dump<W: ContagionWorld>(&self, w: &mut W, file: u64) {
        w.text(
            file,
            &format!(
                "\nOUT OF SYNCH DUMP:  Contangion Manager Dump\n\nOUT OF SYNCH DUMP:  Contangion Manager Help list size {}\n",
                self.helpers.len()
            ),
        );
        for i in 0..self.helpers.len() {
            self.at(i).dump(w, file);
        }
        w.text(file, &format!("OUT OF SYNCH DUMP:  EEContagionManager-Unique Number is {}\n", self.next_id));
    }

    /// `SwapUnit`: every helper's.
    pub fn swap_unit<W: ContagionWorld>(&mut self, w: &mut W, old: u64, new: u64) {
        for i in 0..self.helpers.len() {
            self.at(i).swap_unit(w, old, new);
        }
    }

    /// `Load`: the helpers deleted and read with their calamity indexes, the last id (0.273 on); the first error.
    pub fn load<W: ContagionWorld>(
        &mut self,
        w: &mut W,
        file: u64,
        major: c_ulong,
        minor: c_ulong,
        counter: bool,
    ) -> c_long {
        let mut next_id = self.next_id;
        let result =
            manager_load(&mut ManagerIo { manager: self, world: w, file, major, minor }, &mut next_id, counter);
        self.next_id = next_id;
        result
    }

    /// `Save`: each helper with its calamity's index (9 if one is not active), the last id (0.273 on).
    pub fn save<W: ContagionWorld>(&mut self, w: &mut W, file: u64, counter: bool) -> c_long {
        let next_id = self.next_id;
        manager_save(&mut ManagerIo { manager: self, world: w, file, major: 0, minor: 0 }, next_id, counter)
    }

    /// `RestorePointers`: each loaded helper on its calamity (by index), its functor and units; the helpers
    /// without one destroyed; ids go on after the highest.
    pub fn restore<W: ContagionWorld>(&mut self, w: &mut W, calamities: &[u64]) {
        let mut dead = Vec::new();
        let indexes = self.calamity_indexes.clone();
        for (i, &index) in indexes.iter().enumerate().take(self.helpers.len()) {
            let calamity = calamities.get(index as usize).copied().unwrap_or(0);
            // SAFETY: a listed helper is a live box (module docs)
            let h = unsafe { &mut *self.helpers[i] };
            if calamity != 0 {
                h.calamity = calamity;
                h.functor = w.calamity_functor(calamity);
                h.restore(w);
                self.next_id = self.next_id.max(h.fields.mHelperID);
            } else {
                dead.push(h.fields.mHelperID);
            }
        }
        self.calamity_indexes.clear();
        for id in dead {
            self.destroy(w, id);
        }
    }
}

// ---- the C ABI (`EEContagionManager.cpp`; the world is its `ee_contagion_world`) -----------------------------------

/// `ee_contagion_world`'s operations (`EEContagionManager.cpp`): `a`, `b`, `c` the objects or values, `x` a float,
/// `io` the op's in or out value.
pub const EE_CONTAGION_WORLD_REFERENCE: u32 = 1;
pub const EE_CONTAGION_WORLD_MARK: u32 = 2;
pub const EE_CONTAGION_WORLD_INFECT: u32 = 3;
pub const EE_CONTAGION_WORLD_CURE: u32 = 4;
pub const EE_CONTAGION_WORLD_APPLY: u32 = 5;
pub const EE_CONTAGION_WORLD_FUNCTOR_UPDATE: u32 = 6;
pub const EE_CONTAGION_WORLD_NEAR: u32 = 7;
pub const EE_CONTAGION_WORLD_CAN_INFECT: u32 = 8;
pub const EE_CONTAGION_WORLD_IMMUNE: u32 = 9;
pub const EE_CONTAGION_WORLD_RANDOM: u32 = 10;
pub const EE_CONTAGION_WORLD_DISTANCE: u32 = 11;
pub const EE_CONTAGION_WORLD_UNIT: u32 = 12;
pub const EE_CONTAGION_WORLD_CALAMITY_CHECKSUM: u32 = 13;
pub const EE_CONTAGION_WORLD_CALAMITY_INDEX: u32 = 14;
pub const EE_CONTAGION_WORLD_CALAMITY_FUNCTOR: u32 = 15;
pub const EE_CONTAGION_WORLD_CALAMITY_DUMP: u32 = 16;
pub const EE_CONTAGION_WORLD_TEXT: u32 = 17;
pub const EE_CONTAGION_WORLD_FILE: u32 = 18;
pub const EE_CONTAGION_WORLD_CAPTURE: u32 = 19;

/// A near-unit list the C++ fills (`EE_CONTAGION_WORLD_NEAR`).
pub struct EeContagionUnitList(pub(crate) Vec<u64>);

/// Appends a unit to the list `ee_contagion_world` fills.
///
/// # Safety
/// `list` is the list the call was handed.
#[no_mangle]
pub unsafe extern "C" fn ee_contagion_unit_list_push(list: *mut EeContagionUnitList, unit: u64) {
    // SAFETY: the caller's contract
    unsafe { (*list).0.push(unit) };
}

#[cfg(test)]
#[path = "contagion_store_tests.rs"]
mod tests;
