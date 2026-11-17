//! Typed, read-only views of the C++ world's objects, read in place through the registered layout (layout.rs):
//! [`UnitView`], [`ClassView`], [`StatsView`], [`PlayerView`], [`MapView`], [`TileView`] and [`WorldView`].
//!
//! The rules (docs/progress/world-views.md):
//! - A view borrows a [`WorldRead`] token and cannot outlive it. A token says that no other thread writes the
//!   objects while it lives: the simulation thread inside a call from C++, or a holder of the world's lock. The
//!   tokens are counted per thread in debug builds (`ee_world_read_begin`/`end` count the C++ side's).
//! - A view is made only from a handle passed in by C++ (`of`, unsafe: the handle must be a live object of the
//!   view's class) or from a store lookup ([`UnitView::by_id`]), and from other views' pointer members.
//! - A view never touches a reference count, holds no Rust reference into the object (each read is a copy of
//!   the member's bytes), and is never kept across ticks.
//! - `bool` members are read as their byte (any non-zero byte is true, as C++ tests it: a hand-edited save can
//!   leave other bytes than 0 and 1); [`world_field!`](crate::world_field) does the same for a record's members.
//! - Debug builds check a unit's object-kind byte (`RTSUnit::mObjectKind`) when the view is made, and that an
//!   `EEUnit` or `EEComplexUnit` member is read only from an object of that class.

// `c_ulong` and `c_long` are 32 bits on Windows and 64 elsewhere: the casts are needed there
#![allow(clippy::unnecessary_cast)]

use std::marker::PhantomData;
use std::os::raw::{c_long, c_ulong};

use crate::world::layout::{layout, EeWorldLayout};
use crate::world::map_fields::{EeUnitsMapFields, EeUnitsRtsMapFields};
use crate::world::player_fields::{EeUnitsPlayerFields, EeUnitsPlayerSaved, EeUnitsRtsPlayerFields};
use crate::world::stats_fields::EeUnitsUnitClassUpgradeFields;
use crate::world::unit_class_fields::EeUnitsUnitClassFields;
use crate::world::unit_fields::{bool_byte, EeUnitsComplexUnitFields, EeUnitsRtsUnitFields, EeUnitsUnitFields};

// `RTSUnit::mObjectKind`'s values: an `EEUnit`, an `EEComplexUnit` (0: an `RTSUnit`).
use crate::units::kinds::{EE_OBJECT_COMPLEX_UNIT as OBJECT_COMPLEX_UNIT, EE_OBJECT_UNIT as OBJECT_UNIT};
/// The world's player slots (`RTSWorld::mPlayers`).
pub const EE_WORLD_PLAYER_SLOTS: usize = 17;

#[cfg(debug_assertions)]
thread_local! {
    static READS: std::cell::Cell<u32> = const { std::cell::Cell::new(0) };
}

fn count_read(delta: i32) {
    #[cfg(debug_assertions)]
    READS.with(|c| {
        let n = c.get();
        assert!(delta > 0 || n > 0, "ee_sim::world: a read token ended twice");
        c.set(n.wrapping_add_signed(delta));
    });
    #[cfg(not(debug_assertions))]
    let _ = delta;
}

/// The open read tokens of this thread (debug builds; 0 in release builds).
pub fn read_depth() -> u32 {
    #[cfg(debug_assertions)]
    return READS.with(|c| c.get());
    #[cfg(not(debug_assertions))]
    0
}

/// The world read lock token every view borrows (not `Send`: a token is its thread's).
pub struct WorldRead {
    _thread: PhantomData<*const ()>,
}

impl WorldRead {
    /// Opens a read of the world. It takes no lock: a token is the caller's statement that it holds the world
    /// (`gWorld.mLock` for read or write, or the simulation thread's ownership), never a lock taken here. A thread
    /// that already holds the world lock for write (the editor's actions, the tab switch) must not take a read on
    /// top of it (EeTsLock deadlocks), and some readers hold no lock at all (the editor's construction thread, the
    /// main menu, the lobby, where no game runs).
    ///
    /// # Safety
    /// No other thread writes the world's objects while the token lives: the caller runs on the simulation thread
    /// inside a call from C++ (which holds the world), or holds the world's lock. A call back into C++ while views
    /// live is allowed (views hold no references), but a view must not be used to read an object such a call
    /// freed.
    pub unsafe fn begin() -> WorldRead {
        count_read(1);
        WorldRead { _thread: PhantomData }
    }
}

impl WorldRead {
    /// The end of a read whose token the C++ side's `ee_world_read_begin` forgot (`ee_world_read_end`).
    pub(crate) fn end_forgotten() {
        count_read(-1);
    }
}

impl Drop for WorldRead {
    fn drop(&mut self) {
        count_read(-1);
    }
}

/// A `Copy` value a view reads from a member's bytes.
pub trait FieldRead: Copy {
    /// The member's value.
    ///
    /// # Safety
    /// `p` points to a readable, aligned member of this type.
    unsafe fn read_field(p: *const Self) -> Self;
}

macro_rules! plain_reads {
    ($($t:ty),*) => {$(
        impl FieldRead for $t {
            unsafe fn read_field(p: *const Self) -> Self {
                // SAFETY: the caller's contract (every bit pattern is a valid value)
                unsafe { std::ptr::read(p) }
            }
        }
    )*};
}
plain_reads!(u8, u16, u32, u64, i8, i16, i32, i64, usize, isize, f32, f64);

impl FieldRead for bool {
    unsafe fn read_field(p: *const Self) -> Self {
        // SAFETY: the caller's contract; the byte, never a Rust `bool` of it
        unsafe { bool_byte(p) != 0 }
    }
}

impl<T> FieldRead for *mut T {
    unsafe fn read_field(p: *const Self) -> Self {
        // SAFETY: the caller's contract
        unsafe { std::ptr::read(p) }
    }
}

impl<T> FieldRead for *const T {
    unsafe fn read_field(p: *const Self) -> Self {
        // SAFETY: the caller's contract
        unsafe { std::ptr::read(p) }
    }
}

impl<T: FieldRead, const N: usize> FieldRead for [T; N] {
    unsafe fn read_field(p: *const Self) -> Self {
        let first = p.cast::<T>();
        // SAFETY: the caller's contract: N readable elements
        std::array::from_fn(|i| unsafe { T::read_field(first.add(i)) })
    }
}

/// One of the Rust records inside a viewed object (the record types of this module's siblings), read member by
/// member with [`world_field!`](crate::world_field); never a reference to the record.
pub struct Record<'t, T> {
    p: *const T,
    _t: PhantomData<&'t WorldRead>,
}

impl<T> Clone for Record<'_, T> {
    fn clone(&self) -> Self {
        *self
    }
}
impl<T> Copy for Record<'_, T> {}

impl<'t, T> Record<'t, T> {
    fn at(object: usize, offset: u64) -> Self {
        Record { p: (object + offset as usize) as *const T, _t: PhantomData }
    }
    /// The record's address (for [`world_field!`](crate::world_field)).
    pub fn ptr(&self) -> *const T {
        self.p
    }
}

/// A member of a view's [`Record`], read in place: `world_field!(unit.complex(), mHeight)`. A `bool` member (or an
/// array of them) reads its bytes as non-zero tests.
#[macro_export]
macro_rules! world_field {
    ($record:expr, $field:ident) => {{
        let record = $record;
        let at = record.ptr();
        // SAFETY: a view's record is a live object's while its token lives (the view's contract); the member is
        // the record's own, aligned
        unsafe { $crate::world::view::FieldRead::read_field(::core::ptr::addr_of!((*at).$field)) }
    }};
}

/// `gUnitClassTable`'s begin and end.
fn class_table() -> (usize, usize) {
    let at = layout().unit_class_table as usize;
    if at == 0 {
        return (0, 0);
    }
    let w = std::mem::size_of::<usize>() as u64;
    // SAFETY: the registered vector's first two words (EEWorldLayout.cpp checks the library's layout)
    unsafe { (pointer(at, 0), pointer(at, w)) }
}

/// The value of type `T` at `object + offset`.
///
/// # Safety
/// The bytes there are a live member of type `T`.
unsafe fn member<T: FieldRead>(object: usize, offset: u64) -> T {
    // SAFETY: the caller's contract
    unsafe { T::read_field((object + offset as usize) as *const T) }
}

/// A pointer member's value at `object + offset` (0: none).
///
/// # Safety
/// The bytes there are a live pointer member.
unsafe fn pointer(object: usize, offset: u64) -> usize {
    // SAFETY: the caller's contract
    unsafe { member::<usize>(object, offset) }
}

// ---- the views

/// A unit object (`RTSUnit`, `EEUnit` or `EEComplexUnit`; its handle is the `RTSUnit*`'s address).
#[derive(Clone, Copy)]
pub struct UnitView<'t> {
    object: usize,
    kind: u8,
    _t: PhantomData<&'t WorldRead>,
}

impl<'t> UnitView<'t> {
    /// The view of the unit `handle` (None for 0).
    ///
    /// # Safety
    /// `handle` is 0 or a live unit object while `token` lives.
    pub unsafe fn of(token: &'t WorldRead, handle: u64) -> Option<UnitView<'t>> {
        let _ = token;
        if handle == 0 {
            return None;
        }
        let object = handle as usize;
        // SAFETY: the caller's contract: a live unit, whose kind byte is readable
        let kind: u8 = unsafe { member(object, layout().unit_object_kind) };
        debug_assert!(kind <= OBJECT_COMPLEX_UNIT, "ee_sim::world: {handle:#x} is not a unit (kind byte {kind})");
        Some(UnitView { object, kind, _t: PhantomData })
    }

    /// The unit of id `id` in the unit store (a player's listed unit), if any.
    pub fn by_id(token: &'t WorldRead, id: u32) -> Option<UnitView<'t>> {
        let object = crate::units::store::with_store(|s| s.object_of(id))?;
        // SAFETY: a store record's object is live while it is listed (RTSPlayer::RemoveKilledUnits drops the
        // record before the object goes)
        unsafe { Self::of(token, object) }
    }

    /// A view this view's pointer member names (a live object of the same world).
    fn other(&self, handle: usize) -> Option<UnitView<'t>> {
        if handle == 0 {
            return None;
        }
        // SAFETY: a live unit's pointer members name live units
        let kind: u8 = unsafe { member(handle, layout().unit_object_kind) };
        debug_assert!(kind <= OBJECT_COMPLEX_UNIT, "ee_sim::world: {handle:#x} is not a unit (kind byte {kind})");
        Some(UnitView { object: handle, kind, _t: PhantomData })
    }

    fn l(&self) -> &'static EeWorldLayout {
        layout()
    }

    fn need_unit(&self) {
        debug_assert!(self.kind >= OBJECT_UNIT, "ee_sim::world: an EEUnit member of an RTSUnit {:#x}", self.object);
    }

    fn need_complex(&self) {
        debug_assert!(
            self.kind == OBJECT_COMPLEX_UNIT,
            "ee_sim::world: an EEComplexUnit member of a kind {} unit {:#x}",
            self.kind,
            self.object
        );
    }

    /// The unit's handle (its `RTSUnit*`'s address).
    pub fn handle(&self) -> u64 {
        self.object as u64
    }
    /// `mObjectKind` (ee_sim::units::kinds' `EE_OBJECT_*`: 0 an `RTSUnit`, 1 an `EEUnit`, 2 an `EEComplexUnit`).
    pub fn kind(&self) -> u8 {
        self.kind
    }
    /// An `EEUnit` (or an `EEComplexUnit`).
    pub fn is_unit(&self) -> bool {
        self.kind >= OBJECT_UNIT
    }
    pub fn is_complex(&self) -> bool {
        self.kind == OBJECT_COMPLEX_UNIT
    }

    // -- the records
    /// `RTSUnit`'s record (`EeUnitsRtsUnitFields`).
    pub fn rts(&self) -> Record<'t, EeUnitsRtsUnitFields> {
        Record::at(self.object, self.l().rts_unit_fields)
    }
    /// `EEUnit`'s record (`EeUnitsUnitFields`): an `EEUnit`'s only.
    pub fn fields(&self) -> Record<'t, EeUnitsUnitFields> {
        self.need_unit();
        Record::at(self.object, self.l().unit_fields)
    }
    /// `EEComplexUnit`'s record (`EeUnitsComplexUnitFields`): an `EEComplexUnit`'s only.
    pub fn complex(&self) -> Record<'t, EeUnitsComplexUnitFields> {
        self.need_complex();
        Record::at(self.object, self.l().complex_unit_fields)
    }

    // -- RTSUnit
    /// `mUnitID` (0xffffffff: none; the owner in its top byte)
    pub fn id(&self) -> u32 {
        let id: c_ulong = crate::world_field!(self.rts(), mUnitID);
        id as u32
    }
    /// `mTileX`, `mTileY` (-10000: off the map)
    pub fn tile_xy(&self) -> (i64, i64) {
        let x: c_long = crate::world_field!(self.rts(), mTileX);
        let y: c_long = crate::world_field!(self.rts(), mTileY);
        (x as i64, y as i64)
    }
    pub fn valid(&self) -> bool {
        crate::world_field!(self.rts(), mIsValid)
    }
    pub fn queued_for_delete(&self) -> bool {
        crate::world_field!(self.rts(), mIsQueuedForDelete)
    }
    pub fn node_freed(&self) -> bool {
        crate::world_field!(self.rts(), mIsNodeFreed)
    }
    /// `mNext`: the next unit of its owner's list
    pub fn next(&self) -> Option<UnitView<'t>> {
        // SAFETY: a live unit's pointer member
        self.other(unsafe { pointer(self.object, self.l().unit_next) })
    }
    /// `mTile` (None off the map)
    pub fn tile(&self) -> Option<TileView<'t>> {
        // SAFETY: as above
        TileView::at(unsafe { pointer(self.object, self.l().unit_tile) })
    }
    /// `mPlayer` (its owner)
    pub fn player(&self) -> Option<PlayerView<'t>> {
        // SAFETY: as above
        PlayerView::at(unsafe { pointer(self.object, self.l().unit_player) })
    }
    /// `mTileNode != NULL` (0: not a valid target)
    pub fn has_tile_node(&self) -> bool {
        // SAFETY: as above
        unsafe { pointer(self.object, self.l().unit_tile_node) != 0 }
    }

    // -- EEUnit
    /// `mHitPoints` (> 0: alive)
    pub fn hit_points(&self) -> i64 {
        let v: c_long = crate::world_field!(self.fields(), mHitPoints);
        v as i64
    }
    pub fn resource_amount(&self) -> i64 {
        let v: c_long = crate::world_field!(self.fields(), mResourceAmount);
        v as i64
    }
    pub fn resource_type(&self) -> u64 {
        let v: c_ulong = crate::world_field!(self.fields(), mResourceType);
        v as u64
    }
    pub fn hidden_flags(&self) -> u8 {
        crate::world_field!(self.fields(), mHiddenFlags)
    }
    /// `mUnitClass`
    pub fn class(&self) -> Option<ClassView<'t>> {
        self.need_unit();
        // SAFETY: an EEUnit's pointer member
        ClassView::at(unsafe { pointer(self.object, self.l().unit_class) })
    }
    /// `mStats` (its player's upgrade of its class)
    pub fn stats(&self) -> Option<StatsView<'t>> {
        self.need_unit();
        // SAFETY: as above
        StatsView::at(unsafe { pointer(self.object, self.l().unit_stats) })
    }
    /// `mComplexUnit`'s handle (0: none; the unit itself for every complex unit on the map)
    pub fn complex_unit_handle(&self) -> u64 {
        self.need_unit();
        // SAFETY: as above
        unsafe { pointer(self.object, self.l().unit_complex_unit) as u64 }
    }
    /// `mComplexUnit`
    pub fn complex_unit(&self) -> Option<UnitView<'t>> {
        self.other(self.complex_unit_handle() as usize)
    }

    // -- EEComplexUnit
    pub fn height(&self) -> f32 {
        crate::world_field!(self.complex(), mHeight)
    }
    pub fn sight(&self) -> f32 {
        crate::world_field!(self.complex(), mSight)
    }
    pub fn facing(&self) -> u64 {
        let v: c_ulong = crate::world_field!(self.complex(), mFacing);
        v as u64
    }
    pub fn is_built(&self) -> bool {
        crate::world_field!(self.complex(), mIsBuilt)
    }
    pub fn commandable(&self) -> bool {
        crate::world_field!(self.complex(), mIsCommandable)
    }
    pub fn cloaked(&self) -> bool {
        crate::world_field!(self.complex(), mIsCloaked)
    }
    pub fn cloakable(&self) -> bool {
        crate::world_field!(self.complex(), mIsCloakable)
    }
    pub fn experience(&self) -> u16 {
        crate::world_field!(self.complex(), mExperience)
    }
    pub fn formation_slot(&self) -> u64 {
        let v: c_ulong = crate::world_field!(self.complex(), mFormationSlot);
        v as u64
    }
    pub fn power(&self) -> i64 {
        let v: c_long = crate::world_field!(self.complex(), mPower);
        v as i64
    }
    /// `mPhysicalModel.GetLocation()`: the transformation's rows
    pub fn transform(&self) -> [[f32; 4]; 4] {
        self.need_complex();
        // SAFETY: an EEComplexUnit's location member
        unsafe { member(self.object, self.l().complex_location) }
    }
    /// The location's x, y, z (`GETransformation::GetX` etc.: the last column)
    pub fn location(&self) -> [f32; 3] {
        let m = self.transform();
        [m[0][3], m[1][3], m[2][3]]
    }
    /// `mCommand`'s handle (an `EEUnitAction*`, 0: none)
    pub fn command_handle(&self) -> u64 {
        self.need_complex();
        // SAFETY: an EEComplexUnit's pointer member
        unsafe { pointer(self.object, self.l().complex_command) as u64 }
    }
    /// `mCommand ? mCommand->mType : 0`
    pub fn command_type(&self) -> u32 {
        match self.command_handle() as usize {
            0 => 0,
            // SAFETY: a live unit's action is live while it points to it
            action => unsafe { member(action, self.l().action_type) },
        }
    }
    /// `mGoalState`'s handle (an `EEUnitGoal*`, 0: none)
    pub fn goal_handle(&self) -> u64 {
        self.need_complex();
        // SAFETY: as above
        unsafe { pointer(self.object, self.l().complex_goal_state) as u64 }
    }
    /// `mGoalState`'s `mKind` and `mID` (None: no goal)
    pub fn goal(&self) -> Option<(u8, u32)> {
        goal_kind_id(self.goal_handle() as usize)
    }
    /// `mActiveGoal`'s handle (0: none)
    pub fn active_goal_handle(&self) -> u64 {
        self.need_complex();
        // SAFETY: as above
        unsafe { pointer(self.object, self.l().complex_active_goal) as u64 }
    }
    /// `mActiveGoal`'s `mKind` and `mID` (None: none)
    pub fn active_goal(&self) -> Option<(u8, u32)> {
        goal_kind_id(self.active_goal_handle() as usize)
    }
    /// `mActiveGroup`'s handle (an `EEActiveGroup*`, 0: none)
    pub fn active_group_handle(&self) -> u64 {
        self.need_complex();
        // SAFETY: as above
        unsafe { pointer(self.object, self.l().complex_active_group) as u64 }
    }
    /// `mHerd`'s handle (an `EEHerd*`, 0: none)
    pub fn herd_handle(&self) -> u64 {
        self.need_complex();
        // SAFETY: as above
        unsafe { pointer(self.object, self.l().complex_herd) as u64 }
    }
    /// `mContainer`'s handle (0: none)
    pub fn container_handle(&self) -> u64 {
        self.need_complex();
        // SAFETY: as above
        unsafe { pointer(self.object, self.l().complex_container) as u64 }
    }
    /// `mContainer` (an airport holding an aircraft, a transport its cargo)
    pub fn container(&self) -> Option<UnitView<'t>> {
        self.other(self.container_handle() as usize)
    }
    /// `mPreviousPart`'s handle (0: none, the first part of a linked group or a single unit)
    pub fn previous_part_handle(&self) -> u64 {
        self.need_complex();
        // SAFETY: an EEComplexUnit's pointer member
        unsafe { pointer(self.object, self.l().complex_previous_part) as u64 }
    }
    /// `mNextPart`'s handle (0: none)
    pub fn next_part_handle(&self) -> u64 {
        self.need_complex();
        // SAFETY: as above
        unsafe { pointer(self.object, self.l().complex_next_part) as u64 }
    }
    /// `mCurrentResearch`
    pub fn current_research(&self) -> i64 {
        let v: c_long = crate::world_field!(self.complex(), mCurrentResearch);
        v as i64
    }
    fn build_queue_range(&self) -> (usize, usize) {
        self.need_complex();
        let at = self.l().complex_build_queue;
        let w = std::mem::size_of::<usize>() as u64;
        // SAFETY: an EEComplexUnit's vector member (begin and end its first two words)
        unsafe { (pointer(self.object, at), pointer(self.object, at + w)) }
    }
    /// `GetBuildQueueSize()`
    pub fn build_queue_len(&self) -> usize {
        let (begin, end) = self.build_queue_range();
        (end - begin) / self.l().build_entry_stride.max(1) as usize
    }
    /// `mBuildQueue[i].mTechnology` (None past the end)
    pub fn build_queue_technology(&self, i: usize) -> Option<i64> {
        if i >= self.build_queue_len() {
            return None;
        }
        let l = self.l();
        let entry = self.build_queue_range().0 + i * l.build_entry_stride as usize;
        // SAFETY: an entry of the vector
        let v: c_long = unsafe { member(entry, l.build_entry_technology) };
        Some(v as i64)
    }
    /// `mCargo`'s begin and end pointers.
    fn cargo_range(&self) -> (usize, usize) {
        self.need_complex();
        let at = self.l().complex_cargo;
        let w = std::mem::size_of::<usize>() as u64;
        // SAFETY: an EEComplexUnit's vector member: its first two words are its begin and end (EEWorldLayout.cpp
        // checks the library's layout)
        unsafe { (pointer(self.object, at), pointer(self.object, at + w)) }
    }
    /// `mCargo.size()`
    pub fn cargo_len(&self) -> usize {
        let (begin, end) = self.cargo_range();
        (end - begin) / std::mem::size_of::<usize>()
    }
    /// `mCargo[i]`'s handle (0 past the end or for a NULL entry)
    pub fn cargo_handle(&self, i: usize) -> u64 {
        if i >= self.cargo_len() {
            return 0;
        }
        // SAFETY: an element of the vector
        unsafe { pointer(self.cargo_range().0, (i * std::mem::size_of::<usize>()) as u64) as u64 }
    }
    /// `mCargo[i]`
    pub fn cargo(&self, i: usize) -> Option<UnitView<'t>> {
        self.other(self.cargo_handle(i) as usize)
    }
}

/// A goal's `mKind` and `mID` (None for 0).
fn goal_kind_id(goal: usize) -> Option<(u8, u32)> {
    if goal == 0 {
        return None;
    }
    let l = layout();
    // SAFETY: a live unit's goal is live while it points to it
    let (kind, id): (u8, c_ulong) = unsafe { (member(goal, l.goal_kind), member(goal, l.goal_id)) };
    Some((kind, id as u32))
}

/// A unit class (`EEUnitClass`).
#[derive(Clone, Copy)]
pub struct ClassView<'t> {
    object: usize,
    _t: PhantomData<&'t WorldRead>,
}

impl<'t> ClassView<'t> {
    fn at(object: usize) -> Option<Self> {
        (object != 0).then_some(ClassView { object, _t: PhantomData })
    }
    /// `gUnitClassTable.size()` (`EEUnitClassCount()`).
    pub fn count(token: &'t WorldRead) -> usize {
        let _ = token;
        let (begin, end) = class_table();
        (end - begin) / std::mem::size_of::<usize>()
    }
    /// `gUnitClassTable[index]` (None for an empty slot or past the table).
    pub fn by_index(token: &'t WorldRead, index: usize) -> Option<Self> {
        if index >= Self::count(token) {
            return None;
        }
        // SAFETY: an element of the table; a listed class is live while the world is read
        Self::at(unsafe { pointer(class_table().0, (index * std::mem::size_of::<usize>()) as u64) })
    }
    /// The view of the class `handle` (an `EEUnitClass*`; None for 0).
    ///
    /// # Safety
    /// `handle` is 0 or a live unit class while `token` lives.
    pub unsafe fn of(token: &'t WorldRead, handle: u64) -> Option<Self> {
        let _ = token;
        Self::at(handle as usize)
    }
    pub fn handle(&self) -> u64 {
        self.object as u64
    }
    /// The class's record (`EeUnitsUnitClassFields`).
    pub fn fields(&self) -> Record<'t, EeUnitsUnitClassFields> {
        Record::at(self.object, layout().unit_class_fields)
    }
    pub fn category(&self) -> u64 {
        let v: c_ulong = crate::world_field!(self.fields(), mCategory);
        v as u64
    }
    pub fn unit_type(&self) -> u64 {
        let v: c_ulong = crate::world_field!(self.fields(), mUnitType);
        v as u64
    }
    pub fn family(&self) -> u64 {
        let v: c_ulong = crate::world_field!(self.fields(), mFamily);
        v as u64
    }
    pub fn range(&self) -> f32 {
        crate::world_field!(self.fields(), mRange)
    }
    pub fn technology(&self) -> i64 {
        let v: c_long = crate::world_field!(self.fields(), mTechnology);
        v as i64
    }
    pub fn ai_kind(&self) -> u64 {
        let v: c_ulong = crate::world_field!(self.fields(), mAIKind);
        v as u64
    }
    /// `IsLandBuilding()` (`mIsLandBuildingClass`)
    pub fn land_building(&self) -> bool {
        crate::world_field!(self.fields(), mIsLandBuildingClass)
    }
    pub fn vital(&self) -> bool {
        crate::world_field!(self.fields(), mIsVital)
    }
    pub fn inactive(&self) -> bool {
        crate::world_field!(self.fields(), mIsInactive)
    }
    pub fn blocks_tile(&self) -> bool {
        crate::world_field!(self.fields(), mBlocksTile)
    }
    pub fn wall(&self) -> bool {
        crate::world_field!(self.fields(), mIsWall)
    }
    pub fn tree(&self) -> bool {
        crate::world_field!(self.fields(), mIsTree)
    }
    pub fn unload_range(&self) -> f32 {
        crate::world_field!(self.fields(), mUnloadRange)
    }
}

/// A unit's stats (`EEUnitClassUpgrade`: its player's upgrade of its class).
#[derive(Clone, Copy)]
pub struct StatsView<'t> {
    object: usize,
    _t: PhantomData<&'t WorldRead>,
}

impl<'t> StatsView<'t> {
    fn at(object: usize) -> Option<Self> {
        (object != 0).then_some(StatsView { object, _t: PhantomData })
    }
    /// The view of the stats `handle` (an `EEUnitClassUpgrade*`; None for 0).
    ///
    /// # Safety
    /// `handle` is 0 or a live stats object while `token` lives.
    pub unsafe fn of(token: &'t WorldRead, handle: u64) -> Option<Self> {
        let _ = token;
        Self::at(handle as usize)
    }
    pub fn handle(&self) -> u64 {
        self.object as u64
    }
    /// The stats' record (`EeUnitsUnitClassUpgradeFields`).
    pub fn fields(&self) -> Record<'t, EeUnitsUnitClassUpgradeFields> {
        Record::at(self.object, layout().stats_fields)
    }
    pub fn range(&self) -> f32 {
        crate::world_field!(self.fields(), mRange)
    }
    pub fn scale(&self) -> f32 {
        crate::world_field!(self.fields(), mScale)
    }
    pub fn max_hit_points(&self) -> i64 {
        let v: c_long = crate::world_field!(self.fields(), mMaxHitPoints);
        v as i64
    }
}

/// A player (`EEPlayer`; its handle is the `RTSPlayer*`'s address, which is the `EEPlayer*`'s).
#[derive(Clone, Copy)]
pub struct PlayerView<'t> {
    object: usize,
    _t: PhantomData<&'t WorldRead>,
}

impl<'t> PlayerView<'t> {
    fn at(object: usize) -> Option<Self> {
        (object != 0).then_some(PlayerView { object, _t: PhantomData })
    }
    /// The view of the player `handle` (None for 0).
    ///
    /// # Safety
    /// `handle` is 0 or a live `EEPlayer` while `token` lives.
    pub unsafe fn of(token: &'t WorldRead, handle: u64) -> Option<Self> {
        let _ = token;
        Self::at(handle as usize)
    }
    pub fn handle(&self) -> u64 {
        self.object as u64
    }
    /// `RTSPlayer`'s record (`EeUnitsRtsPlayerFields`)
    pub fn rts(&self) -> Record<'t, EeUnitsRtsPlayerFields> {
        Record::at(self.object, layout().rts_player_fields)
    }
    /// `EEPlayer`'s saved numbers (`EeUnitsPlayerSaved`)
    pub fn saved(&self) -> Record<'t, EeUnitsPlayerSaved> {
        Record::at(self.object, layout().player_saved)
    }
    /// `EEPlayer`'s other plain members (`EeUnitsPlayerFields`)
    pub fn fields(&self) -> Record<'t, EeUnitsPlayerFields> {
        Record::at(self.object, layout().player_fields)
    }
    /// `mPlayerID` (the index into another player's `mEnemies`)
    pub fn id(&self) -> u32 {
        let v: c_ulong = crate::world_field!(self.rts(), mPlayerID);
        v as u32
    }
    /// `mSlot` (the lobby slot, 0xff none)
    pub fn slot(&self) -> u8 {
        crate::world_field!(self.rts(), mSlot)
    }
    pub fn sees_all(&self) -> bool {
        crate::world_field!(self.rts(), mSeesAll)
    }
    pub fn observer(&self) -> bool {
        crate::world_field!(self.rts(), mObserverMode)
    }
    /// `mColor`'s red, green, blue
    pub fn color(&self) -> [f32; 3] {
        // SAFETY: a live player's colour member
        unsafe { member(self.object, layout().player_color) }
    }
    /// `mSelectedGroup`'s handle (an `EESelectedGroup*`, 0: none)
    pub fn selected_group_handle(&self) -> u64 {
        // SAFETY: a live player's pointer member
        unsafe { pointer(self.object, layout().player_selected_group) as u64 }
    }
    /// `mComputerPlayer != NULL`
    pub fn has_computer_player(&self) -> bool {
        // SAFETY: a live player's pointer member
        unsafe { pointer(self.object, layout().player_computer_player) != 0 }
    }
    /// `mEnemies[player]` (non-zero: at war; 0 past the table)
    pub fn enemy(&self, player: usize) -> u64 {
        let enemies: [c_ulong; 17] = crate::world_field!(self.saved(), mEnemies);
        enemies.get(player).map_or(0, |&v| v as u64)
    }
    /// `mFirstUnit`: the first unit of its list (then [`UnitView::next`])
    pub fn first_unit(&self) -> Option<UnitView<'t>> {
        // SAFETY: a live player's list head is a live unit or 0
        let first = unsafe { pointer(self.object, layout().player_first_unit) };
        if first == 0 {
            return None;
        }
        // SAFETY: as above
        let kind: u8 = unsafe { member(first, layout().unit_object_kind) };
        debug_assert!(kind <= OBJECT_COMPLEX_UNIT, "ee_sim::world: a player's unit {first:#x} of kind {kind}");
        Some(UnitView { object: first, kind, _t: PhantomData })
    }
    /// Its units in list order (`mFirstUnit`, then each unit's `mNext`).
    pub fn units(&self) -> impl Iterator<Item = UnitView<'t>> {
        std::iter::successors(self.first_unit(), |u| u.next())
    }
}

/// The world (`EEWorld`, the game's one world object).
#[derive(Clone, Copy)]
pub struct WorldView<'t> {
    object: usize,
    _t: PhantomData<&'t WorldRead>,
}

impl<'t> WorldView<'t> {
    /// The view of the world `handle` (`&gWorld`; None for 0).
    ///
    /// # Safety
    /// `handle` is 0 or the live world while `token` lives.
    pub unsafe fn of(token: &'t WorldRead, handle: u64) -> Option<Self> {
        let _ = token;
        (handle != 0).then_some(WorldView { object: handle as usize, _t: PhantomData })
    }
    pub fn handle(&self) -> u64 {
        self.object as u64
    }
    /// `mNumPlayers`
    pub fn num_players(&self) -> u64 {
        // SAFETY: the world's member
        let v: c_ulong = unsafe { member(self.object, layout().world_num_players) };
        v as u64
    }
    /// `mCurrentPlayer` (the local player's index)
    pub fn current_player_index(&self) -> i64 {
        // SAFETY: as above
        let v: c_long = unsafe { member(self.object, layout().world_current_player) };
        v as i64
    }
    /// `mPlayers[index]` (None past the 17 slots or for an empty slot)
    pub fn player(&self, index: usize) -> Option<PlayerView<'t>> {
        if index >= EE_WORLD_PLAYER_SLOTS {
            return None;
        }
        let at = layout().world_players + (index * std::mem::size_of::<usize>()) as u64;
        // SAFETY: an element of the world's player array
        PlayerView::at(unsafe { pointer(self.object, at) })
    }
    /// `GetCurrentPlayer()`: `mPlayers[mCurrentPlayer]`
    pub fn current_player(&self) -> Option<PlayerView<'t>> {
        usize::try_from(self.current_player_index()).ok().and_then(|i| self.player(i))
    }
    fn flag(&self, offset: u64) -> bool {
        // SAFETY: one of the world's bool members
        unsafe { member::<bool>(self.object, offset) }
    }
    /// `mGameSpeed`
    pub fn game_speed(&self) -> f32 {
        // SAFETY: the world's member
        unsafe { member(self.object, layout().world_game_speed) }
    }
    /// `mPopulationLimit`
    pub fn population_limit(&self) -> i64 {
        // SAFETY: as above
        let v: c_long = unsafe { member(self.object, layout().world_population_limit) };
        v as i64
    }
    /// `mNumberOfWondersToWin`
    pub fn wonders_to_win(&self) -> u64 {
        // SAFETY: as above
        let v: c_ulong = unsafe { member(self.object, layout().world_wonders_to_win) };
        v as u64
    }
    /// `&mGameOptions` (an `EEGameOptions*`; no view of it yet)
    pub fn game_options_handle(&self) -> u64 {
        (self.object + layout().world_game_options as usize) as u64
    }
    /// `mGhostUnits.size()` and `mGhostUnits[i]` (an `EEGhostUnit*`, 0 past the end); the ghosts' own lock
    /// (`mGhostUnitsLock`) is the caller's to hold
    pub fn ghost_units_len(&self) -> usize {
        let (begin, end) = self.ghost_range();
        (end - begin) / std::mem::size_of::<usize>()
    }
    pub fn ghost_unit_handle(&self, i: usize) -> u64 {
        if i >= self.ghost_units_len() {
            return 0;
        }
        // SAFETY: an element of the vector
        unsafe { pointer(self.ghost_range().0, (i * std::mem::size_of::<usize>()) as u64) as u64 }
    }
    fn ghost_range(&self) -> (usize, usize) {
        let at = layout().world_ghost_units;
        let w = std::mem::size_of::<usize>() as u64;
        // SAFETY: the world's vector member: begin and end are its first two words (EEWorldLayout.cpp checks it)
        unsafe { (pointer(self.object, at), pointer(self.object, at + w)) }
    }
    pub fn victory_enabled(&self) -> bool {
        self.flag(layout().world_victory_enabled)
    }
    pub fn game_screen_active(&self) -> bool {
        self.flag(layout().world_game_screen_active)
    }
    pub fn loading_saved_game(&self) -> bool {
        self.flag(layout().world_loading_saved_game)
    }
    pub fn cheats_enabled(&self) -> bool {
        self.flag(layout().world_cheats_enabled)
    }
    pub fn in_cinematic(&self) -> bool {
        self.flag(layout().world_in_cinematic)
    }
    pub fn user_paused(&self) -> bool {
        self.flag(layout().world_user_paused)
    }
    pub fn using_carbon(&self) -> bool {
        self.flag(layout().world_using_carbon)
    }
    pub fn teams_locked(&self) -> bool {
        self.flag(layout().world_teams_locked)
    }
    /// The world (`&gWorld`, registered with the layout).
    pub fn global(token: &'t WorldRead) -> Option<Self> {
        // SAFETY: the registered address is the one live world
        unsafe { Self::of(token, layout().world_address) }
    }
    /// `mMap`
    pub fn map(&self) -> Option<MapView<'t>> {
        // SAFETY: the world's pointer member
        let map = unsafe { pointer(self.object, layout().world_map) };
        (map != 0).then_some(MapView { object: map, _t: PhantomData })
    }
}

/// The map (`EEMap`; its handle is the `RTSMap*`'s address, which is the `EEMap*`'s).
#[derive(Clone, Copy)]
pub struct MapView<'t> {
    object: usize,
    _t: PhantomData<&'t WorldRead>,
}

impl<'t> MapView<'t> {
    /// The view of the map `handle` (None for 0).
    ///
    /// # Safety
    /// `handle` is 0 or a live `EEMap` while `token` lives.
    pub unsafe fn of(token: &'t WorldRead, handle: u64) -> Option<Self> {
        let _ = token;
        (handle != 0).then_some(MapView { object: handle as usize, _t: PhantomData })
    }
    pub fn handle(&self) -> u64 {
        self.object as u64
    }
    /// `RTSMap`'s record (`EeUnitsRtsMapFields`)
    pub fn rts(&self) -> Record<'t, EeUnitsRtsMapFields> {
        Record::at(self.object, layout().rts_map_fields)
    }
    /// `EEMap`'s record (`EeUnitsMapFields`)
    pub fn fields(&self) -> Record<'t, EeUnitsMapFields> {
        Record::at(self.object, layout().map_fields)
    }
    /// `mSize` (tiles per side)
    pub fn size(&self) -> i64 {
        let v: c_long = crate::world_field!(self.rts(), mSize);
        v as i64
    }
    /// `mU19561C`: the player count the tiles' visibility words are for
    pub fn player_count(&self) -> u64 {
        let v: c_ulong = crate::world_field!(self.rts(), mU19561C);
        v as u64
    }
    /// The visibility words per tile (two players a word)
    pub fn visibility_words(&self) -> usize {
        let n = self.player_count() as usize;
        (n >> 1) + (n & 1)
    }
    /// `GetTile(x, y)`: None off the map (the unsigned compare) or where the map has no tile
    pub fn tile(&self, x: i64, y: i64) -> Option<TileView<'t>> {
        let size = self.size() as u64;
        if x as u64 >= size || y as u64 >= size {
            return None;
        }
        // SAFETY: the map's tile array, `mSize` x `mSize` pointers
        let tiles = unsafe { pointer(self.object, layout().map_tiles) };
        if tiles == 0 {
            return None;
        }
        let at = ((y as u64 * size + x as u64) as usize * std::mem::size_of::<usize>()) as u64;
        // SAFETY: an element of the array
        TileView::at(unsafe { pointer(tiles, at) })
    }
}

/// A map tile (`EETile`; its handle is the `RTSTile*`'s address, which is the `EETile*`'s).
#[derive(Clone, Copy)]
pub struct TileView<'t> {
    object: usize,
    _t: PhantomData<&'t WorldRead>,
}

impl<'t> TileView<'t> {
    fn at(object: usize) -> Option<Self> {
        (object != 0).then_some(TileView { object, _t: PhantomData })
    }
    /// The view of the tile `handle` (None for 0).
    ///
    /// # Safety
    /// `handle` is 0 or a live tile while `token` lives.
    pub unsafe fn of(token: &'t WorldRead, handle: u64) -> Option<Self> {
        let _ = token;
        Self::at(handle as usize)
    }
    pub fn handle(&self) -> u64 {
        self.object as u64
    }
    /// `mTerrainType`
    pub fn terrain_type(&self) -> i64 {
        // SAFETY: the tile's member
        let v: c_long = unsafe { member(self.object, layout().tile_terrain_type) };
        v as i64
    }
    /// `mHeight`
    pub fn height(&self) -> f32 {
        // SAFETY: as above
        unsafe { member(self.object, layout().tile_height) }
    }
    /// `mAIValue`
    pub fn ai_value(&self) -> i8 {
        // SAFETY: as above
        unsafe { member(self.object, layout().tile_ai_value) }
    }
    /// `mVisibility[word]`, None while the tile has no words. `word` must be below the map's
    /// [`MapView::visibility_words`].
    pub fn visibility_word(&self, word: usize) -> Option<u32> {
        // SAFETY: the tile's pointer member
        let words = unsafe { pointer(self.object, layout().tile_visibility) };
        // SAFETY: the caller's bound: one of the tile's words
        (words != 0).then(|| unsafe { member(words, (word * 4) as u64) })
    }
    /// `mVisibility != NULL` (the map gives every tile its words: `EEMap::SetPlayerCount`)
    pub fn has_visibility(&self) -> bool {
        // SAFETY: the tile's pointer member
        unsafe { pointer(self.object, layout().tile_visibility) != 0 }
    }
    /// `GetVisibility(player)`: the player's count in its 16-bit half of the word, as the C++ masks it (0xff); 0
    /// while the tile has no words
    pub fn visibility(&self, player: usize) -> u32 {
        self.visibility_word(player >> 1).map_or(0, |w| (w >> ((player & 1) << 4)) & 0xff)
    }
    /// `mUnits`: the units on the tile, in the node list's order (a node's `mUnit` may be NULL: None)
    pub fn units(&self) -> impl Iterator<Item = Option<UnitView<'t>>> {
        let l = layout();
        // SAFETY: the tile's first node (0: none)
        let first = unsafe { pointer(self.object, l.tile_units) };
        std::iter::successors((first != 0).then_some(first), move |&node| {
            // SAFETY: a live node's next (0: the end)
            let next = unsafe { pointer(node, l.unit_node_next) };
            (next != 0).then_some(next)
        })
        .map(move |node| {
            // SAFETY: a live node's unit member: an `EEUnit*`, whose `RTSUnit` is at its start
            let unit = unsafe { pointer(node, l.unit_node_unit) };
            if unit == 0 {
                return None;
            }
            // SAFETY: a live node references its unit
            let kind: u8 = unsafe { member(unit, l.unit_object_kind) };
            Some(UnitView { object: unit, kind, _t: PhantomData })
        })
    }
}
