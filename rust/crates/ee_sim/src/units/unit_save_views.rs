//! The units' saves' reads over the world views (the units' state lane, round 1, branch rust-unitstate1): the
//! queries of [`EEUnitSaveOperation`](unit_save.rs) that are a member of the unit, its stats, its class or an object
//! its pointer members name (the maxima, the floats, the collision tile, every id the original computed from a
//! pointer) and the sections whose containers are vectors or the contact list are answered here from the unit in
//! place; the sections write through the host's file primitives with the original's overloads and loop shapes.
//! [`ViewSaveHost`] wraps the C++ callback: everything else (the deques, the goal queues, the composite members'
//! `Save`, the path manager, the file's primitives) is still the C++ host's.
//!
//! The ids are the original's `unsigned long` (`(unsigned long)-1` for none) widened to the io's i64; the effect
//! handles its `(long)` of the `uint32_t` value.

// `c_ulong` and `c_long` are 32 bits on Windows and 64 elsewhere: the casts are needed there
#![allow(clippy::unnecessary_cast)]

use std::os::raw::{c_long, c_ulong};

use crate::units::unit_load::*;
use crate::units::unit_save::*;
use crate::world::view::{herd_id, patrol_group_id, UnitView, WorldRead};
use crate::world_field;

/// `(unsigned long)-1`, the original's "no unit" id
const NONE: c_ulong = c_ulong::MAX;

/// A section's values gathered under the read token, then written through the host (no view across its calls).
enum Section {
    /// `long` count, then the `unsigned long` ids (the cargo, the held units)
    Ids(Vec<c_ulong>),
    /// `long` count, then `long`s (the unit variables)
    Longs(Vec<i64>),
    /// `long` count, then (`long` technology, `unsigned long` flag) (the build queue)
    BuildQueue(Vec<(i64, i64)>),
    /// `unsigned long` count, then (`long` x, `long` y) (the bounded path tiles)
    Tiles(Vec<(i64, i64)>),
    /// `unsigned long` count (the list's own), then the `unsigned long` ids (the contacts)
    Contacts(u64, Vec<c_ulong>),
}

/// A unit's `GetUnitID()` (`mUnitID`).
fn id_of(u: &UnitView<'_>) -> c_ulong {
    world_field!(u.rts(), mUnitID)
}

/// The id of the unit a pointer member names (`(unsigned long)-1` when none).
fn unit_id_or_none(u: &UnitView<'_>, handle: u64) -> c_ulong {
    u.unit_at(handle).map_or(NONE, |o| id_of(&o))
}

/// `EEUnitBehavior::sTable[i]`'s handle (the db list's entry; 0: none).
fn behavior(i: usize) -> u64 {
    ee_data::tables::ee_data_list_get(ee_data::tables::EeDataList::UnitBehaviors, i) as usize as u64
}

/// The answer to a query (`EE_UNITS_UNIT_SAVE_MAX`, `_FLOAT`, `_COLLISION_TILE`, `_ID`) of the unit `unit`; None
/// for one the C++ host answers.
fn query(t: &WorldRead, unit: u64, op: i32, index: u32) -> Option<EeUnitsUnitSaveIo> {
    // SAFETY: the caller's handle is the unit being saved
    let u = unsafe { UnitView::of(t, unit) }?;
    let mut io = EeUnitsUnitSaveIo::default();
    match op {
        EE_UNITS_UNIT_SAVE_MAX if u.is_unit() => {
            io.i = match index {
                EE_UNITS_UNIT_SAVE_MAX_HIT_POINTS => world_field!(u.stats()?.fields(), mMaxHitPoints) as i64,
                EE_UNITS_UNIT_SAVE_MAX_FLIGHT_TIME => world_field!(u.stats()?.fields(), mFlightTime) as i64,
                EE_UNITS_UNIT_SAVE_MAX_POWER => world_field!(u.stats()?.fields(), mPower) as i64,
                _ => world_field!(u.class()?.fields(), mGarrisonCapacity) as i64,
            }
        }
        EE_UNITS_UNIT_SAVE_FLOAT if u.is_complex() => {
            let c = u.contact_location();
            io.f = match index {
                EE_UNITS_UNIT_SAVE_FLOAT_SCALE => u.model_scale(),
                EE_UNITS_UNIT_SAVE_FLOAT_CONTACT_X => c[0],
                EE_UNITS_UNIT_SAVE_FLOAT_CONTACT_Y => c[1],
                _ => c[2],
            }
        }
        EE_UNITS_UNIT_SAVE_COLLISION_TILE if u.is_complex() => {
            let (x, y) = u.collision_tile();
            io.i = if index == 0 { x } else { y };
        }
        EE_UNITS_UNIT_SAVE_ID if u.is_complex() => {
            let id: c_ulong = match index {
                EE_UNITS_UNIT_SAVE_ID_FRAME => u.model_frame() as c_ulong,
                EE_UNITS_UNIT_SAVE_ID_LAUNCHER_CLASS => {
                    // SAFETY: a live unit's class pointer names a live class
                    match unsafe { crate::world::view::ClassView::of(t, u.launcher_class_handle()) } {
                        None => 0,
                        Some(c) => c.unit_type() as c_ulong,
                    }
                }
                EE_UNITS_UNIT_SAVE_ID_PATROL_GROUP => match u.patrol_group_handle() {
                    0 => NONE,
                    // SAFETY: a live unit's patrol group is live while it points to it
                    g => (unsafe { patrol_group_id(g) }) as c_ulong,
                },
                EE_UNITS_UNIT_SAVE_ID_PREVIOUS_PART => unit_id_or_none(&u, u.previous_part_handle()),
                EE_UNITS_UNIT_SAVE_ID_NEXT_PART => unit_id_or_none(&u, u.next_part_handle()),
                EE_UNITS_UNIT_SAVE_ID_HOME_BASE => unit_id_or_none(&u, u.home_base_handle()),
                EE_UNITS_UNIT_SAVE_ID_CONTAINER => match u.container() {
                    Some(c) if world_field!(c.class()?.fields(), mTransportCapacity) != 0 => id_of(&c),
                    _ => NONE,
                },
                EE_UNITS_UNIT_SAVE_ID_COMMAND => u.command_type() as c_ulong,
                EE_UNITS_UNIT_SAVE_ID_GATHERED_RESOURCE => unit_id_or_none(&u, u.gathered_resource_handle()),
                EE_UNITS_UNIT_SAVE_ID_LAST_ATTACKER => unit_id_or_none(&u, u.last_attacker_handle()),
                EE_UNITS_UNIT_SAVE_ID_OWNER => unit_id_or_none(&u, u.owner_handle()),
                EE_UNITS_UNIT_SAVE_ID_MINE => unit_id_or_none(&u, u.mine_handle()),
                EE_UNITS_UNIT_SAVE_ID_HERD => match u.herd_handle() {
                    0 => 0,
                    // SAFETY: a live unit's herd is live while it points to it
                    h => c_ulong::from(unsafe { herd_id(h) }),
                },
                EE_UNITS_UNIT_SAVE_ID_HOLDER => unit_id_or_none(&u, u.holder_handle()),
                EE_UNITS_UNIT_SAVE_ID_BEHAVIOR => {
                    let b = u.behavior_handle();
                    (0..8).find(|&i| behavior(i) == b).unwrap_or(8) as c_ulong
                }
                EE_UNITS_UNIT_SAVE_ID_LAST_RESOURCE => unit_id_or_none(&u, u.last_resource_handle()),
                EE_UNITS_UNIT_SAVE_ID_CONVERSION_EFFECT => {
                    io.i = u.conversion_effect() as c_long as i64;
                    return Some(io);
                }
                EE_UNITS_UNIT_SAVE_ID_HEAL_EFFECT => {
                    io.i = u.heal_effect() as c_long as i64;
                    return Some(io);
                }
                _ => NONE,
            };
            io.i = id as i64;
        }
        _ => return None,
    }
    Some(io)
}

/// The values of a section the views answer (None: the C++ host's section).
fn section(t: &WorldRead, unit: u64, index: u32) -> Option<Section> {
    // SAFETY: the caller's handle is the unit being saved
    let u = unsafe { UnitView::of(t, unit) }?;
    if !u.is_complex() {
        return None;
    }
    let ids = |handles: Vec<u64>| -> Option<Vec<c_ulong>> {
        handles.into_iter().map(|h| u.unit_at(h).map(|o| id_of(&o))).collect()
    };
    Some(match index {
        EE_UNITS_UNIT_SAVE_SECTION_BUILD_QUEUE => Section::BuildQueue(u.build_queue()),
        EE_UNITS_UNIT_SAVE_SECTION_CARGO => Section::Ids(ids((0..u.cargo_len()).map(|i| u.cargo_handle(i)).collect())?),
        EE_UNITS_UNIT_SAVE_SECTION_HELD_UNITS => Section::Ids(ids(u.held_unit_handles())?),
        EE_UNITS_UNIT_SAVE_SECTION_UNIT_VARIABLES => Section::Longs(u.unit_variables()),
        EE_UNITS_UNIT_SAVE_SECTION_BOUNDED_PATH_TILES => Section::Tiles(u.bounded_path_tiles()),
        EE_UNITS_UNIT_SAVE_SECTION_CONTACTS => {
            let (count, handles) = u.contacts();
            Section::Contacts(count, ids(handles)?)
        }
        _ => return None,
    })
}

/// The C++ host with the view answers in front of it.
pub(crate) struct ViewSaveHost<'a, H: SaveHost> {
    pub(crate) inner: &'a mut H,
    /// the unit's handle (its `RTSUnit*`)
    pub(crate) unit: u64,
}

impl<H: SaveHost> ViewSaveHost<'_, H> {
    fn write(&mut self, op: i32, v: i64) -> i32 {
        self.inner.op(op, 0, &mut EeUnitsUnitSaveIo { i: v, ..Default::default() })
    }
    fn long(&mut self, v: i64) -> i32 {
        self.write(EE_UNITS_UNIT_SAVE_WRITE_LONG, v)
    }
    fn ulong(&mut self, v: i64) -> i32 {
        self.write(EE_UNITS_UNIT_SAVE_WRITE_ULONG, v)
    }

    /// A section with the original's loop: the count's result, then the elements while each result is 0.
    fn write_section(&mut self, s: Section) -> i32 {
        match s {
            Section::Ids(v) => {
                let mut r = self.long(v.len() as i64);
                for id in v {
                    if r != 0 {
                        break;
                    }
                    r = self.ulong(id as i64);
                }
                r
            }
            Section::Longs(v) => {
                let mut r = self.long(v.len() as i64);
                for x in v {
                    if r != 0 {
                        break;
                    }
                    r = self.long(x);
                }
                r
            }
            Section::BuildQueue(v) => {
                let mut r = self.long(v.len() as i64);
                for (tech, flag) in v {
                    if r != 0 {
                        break;
                    }
                    r = self.long(tech);
                    if r == 0 {
                        // `unsigned long theSecond = mFlag` (a long)
                        r = self.ulong(flag as c_long as c_ulong as i64);
                    }
                }
                r
            }
            Section::Tiles(v) => {
                let mut r = self.ulong(v.len() as c_ulong as i64);
                for (x, y) in v {
                    if r != 0 {
                        break;
                    }
                    r = self.long(x);
                    if r == 0 {
                        r = self.long(y);
                    }
                }
                r
            }
            Section::Contacts(count, v) => {
                let mut r = self.ulong(count as c_ulong as i64);
                for id in v {
                    if r != 0 {
                        break;
                    }
                    r = self.ulong(id as i64);
                }
                r
            }
        }
    }
}

impl<H: SaveHost> SaveHost for ViewSaveHost<'_, H> {
    fn op(&mut self, op: i32, index: u32, io: &mut EeUnitsUnitSaveIo) -> i32 {
        match op {
            EE_UNITS_UNIT_SAVE_MAX | EE_UNITS_UNIT_SAVE_FLOAT | EE_UNITS_UNIT_SAVE_COLLISION_TILE | EE_UNITS_UNIT_SAVE_ID => {
                // SAFETY: the save runs on the thread that holds the world (the saved game's writer)
                let answer = query(&unsafe { WorldRead::begin() }, self.unit, op, index);
                if let Some(a) = answer {
                    *io = a;
                    return 0;
                }
            }
            EE_UNITS_UNIT_SAVE_SECTION => {
                // SAFETY: as above; the token ends before the writes
                let s = section(&unsafe { WorldRead::begin() }, self.unit, index);
                if let Some(s) = s {
                    return self.write_section(s);
                }
            }
            _ => {}
        }
        self.inner.op(op, index, io)
    }
}

// ---- the loads' plain stores (unit_load.rs `EE_UNITS_UNIT_LOAD_SET`)

/// A pointer member's value from a saved id as `reinterpret_cast<T*>(theValue)` (the `unsigned long` zero-extended).
fn as_pointer(v: c_ulong) -> usize {
    v as usize
}

/// `USavedPointer<T>(theValue)`: the id's low 32 bits, signed, sign-extended (the none id is `(T*)-1`).
fn saved_pointer(v: c_ulong) -> usize {
    v as u32 as i32 as isize as usize
}

/// Stores `value` (a `T`) at `object + offset`.
///
/// # Safety
/// The bytes there are a live member of type `T` of an object no other code reads or writes meanwhile.
unsafe fn store<T>(object: u64, offset: u64, value: T) {
    // SAFETY: the caller's contract
    unsafe { std::ptr::write((object as usize + offset as usize) as *mut T, value) }
}

/// The load's plain member stores into the unit being loaded (`EEComplexUnitLoad`'s set cases that are one store);
/// false for one the C++ host does (a method, a reference count, the class's action, the scale's rule).
fn load_set(unit: u64, index: u32, io: &EeUnitsUnitSaveIo) -> bool {
    let Some(l) = crate::world::layout::get() else { return false };
    // SAFETY: the unit being loaded is the C++ caller's live object; the views are read only, so the store reads
    // the kind byte itself
    let kind: u8 = unsafe { std::ptr::read((unit as usize + l.unit_object_kind as usize) as *const u8) };
    if kind != crate::units::kinds::EE_OBJECT_COMPLEX_UNIT {
        return false;
    }
    let v = io.i as c_ulong;
    let (offset, value) = match index {
        EE_UNITS_UNIT_LOAD_SET_DEFAULT_BEHAVIOR => (l.complex_behavior, behavior(5) as usize),
        EE_UNITS_UNIT_LOAD_SET_FRAME => {
            // SAFETY: the physical model's unsigned long member (`SetFrame`, a plain store)
            unsafe { store::<c_ulong>(unit, l.complex_frame, v) };
            return true;
        }
        EE_UNITS_UNIT_LOAD_SET_COLLISION_TILE_X | EE_UNITS_UNIT_LOAD_SET_COLLISION_TILE_Y => {
            let at = l.complex_collision_tile
                + if index == EE_UNITS_UNIT_LOAD_SET_COLLISION_TILE_Y { std::mem::size_of::<c_long>() as u64 } else { 0 };
            // SAFETY: `mCollisionTile`'s long members
            unsafe { store::<c_long>(unit, at, io.i as c_long) };
            return true;
        }
        EE_UNITS_UNIT_LOAD_SET_LAUNCHER_CLASS => {
            // `theValue == 0 || UIsNone32(theValue) ? NULL : gUnitClassTable[theValue]`
            let class = if v == 0 || v as u32 == u32::MAX {
                0
            } else {
                // SAFETY: the load runs on the thread that holds the world
                let t = unsafe { WorldRead::begin() };
                crate::world::view::ClassView::by_index(&t, v as usize).map_or(0, |c| c.handle() as usize)
            };
            (l.complex_launcher_class, class)
        }
        EE_UNITS_UNIT_LOAD_SET_PATROL_GROUP => (l.complex_patrol_group, saved_pointer(v)),
        EE_UNITS_UNIT_LOAD_SET_PREVIOUS_PART => (l.complex_previous_part, as_pointer(v)),
        EE_UNITS_UNIT_LOAD_SET_NEXT_PART => (l.complex_next_part, as_pointer(v)),
        EE_UNITS_UNIT_LOAD_SET_HOME_BASE => (l.complex_home_base, as_pointer(v)),
        EE_UNITS_UNIT_LOAD_SET_CONTAINER => (l.complex_container, as_pointer(v)),
        EE_UNITS_UNIT_LOAD_SET_GATHERED_RESOURCE => (l.complex_gathered_resource, as_pointer(v)),
        EE_UNITS_UNIT_LOAD_SET_LAST_ATTACKER => (l.complex_last_attacker, as_pointer(v)),
        EE_UNITS_UNIT_LOAD_SET_OWNER => (l.complex_owner, saved_pointer(v)),
        EE_UNITS_UNIT_LOAD_SET_MINE => (l.complex_mine, saved_pointer(v)),
        EE_UNITS_UNIT_LOAD_SET_MINE_UNSET => (l.complex_mine, usize::MAX),
        EE_UNITS_UNIT_LOAD_SET_HERD => (l.complex_herd, as_pointer(v)),
        EE_UNITS_UNIT_LOAD_SET_HOLDER => (l.complex_holder, saved_pointer(v)),
        EE_UNITS_UNIT_LOAD_SET_BEHAVIOR => (l.complex_behavior, as_pointer(v)),
        EE_UNITS_UNIT_LOAD_SET_LAST_RESOURCE => (l.complex_last_resource, as_pointer(v)),
        _ => return false,
    };
    // SAFETY: a pointer member of the unit being loaded (the layout's offset of one)
    unsafe { store::<usize>(unit, offset, value) };
    true
}

/// The C++ host with the load's plain stores in front of it.
pub(crate) struct ViewLoadHost<'a, H: SaveHost> {
    pub(crate) inner: &'a mut H,
    /// the unit's handle (its `RTSUnit*`)
    pub(crate) unit: u64,
}

impl<H: SaveHost> SaveHost for ViewLoadHost<'_, H> {
    fn op(&mut self, op: i32, index: u32, io: &mut EeUnitsUnitSaveIo) -> i32 {
        if op == EE_UNITS_UNIT_LOAD_SET && load_set(self.unit, index, io) {
            return 0;
        }
        self.inner.op(op, index, io)
    }
}

#[cfg(test)]
#[path = "unit_save_views_tests.rs"]
mod tests;
