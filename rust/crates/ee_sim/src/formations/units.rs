//! `EEFormation::Place` and `Arrange` over the world's units, their capture record and the capture's sweep (was
//! `EEFormation.cpp`'s marshalling, `CapturePlace` and `EEFormationCaptureSweep`; game core round 14): the units'
//! outer collision radii are read through the world's views, the slots are written into the caller's
//! `EEFormationSlot`s.

use std::ffi::c_void;
use std::os::raw::c_long;

use ee_base::math::transform::RotationOrder;
use ee_base::math::types::GeTransformation;

use crate::app::capture_record::{self, Record};
use crate::formations::capture::{replay_shape, shape_of, shape_words};
use crate::formations::{EeFormationFrame, EeFormationSlot, Vec3};
use crate::world::view::{GroupView, UnitView, WorldRead, WorldView};

/// `EEFormationSlot` (ee/EEFormation.h): the position, the unit (an `EEComplexUnit*`) and the heading.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct EeFormationUnitSlot {
    pub position: Vec3,
    pub unit: *mut c_void,
    pub heading: f32,
}

/// The types `EEFormation::IsCircle` names.
fn is_circle(kind: i32) -> bool {
    kind == 6 || kind == 7
}

/// The frame of a formation's members (`mOrientation`'s rotation rows, `mCenter`, `mHeading`).
fn frame_of(orientation: &GeTransformation, center: Vec3, heading: f32) -> EeFormationFrame {
    let mut rows = [[0.0f32; 3]; 3];
    for (row, m) in rows.iter_mut().zip(orientation.m.iter()) {
        row.copy_from_slice(&m[..3]);
    }
    EeFormationFrame { orientation: rows, center, heading }
}

/// `CapturePlace`'s record of one `Place`: the type, the frame, the shape's words, the radii and the slots (each
/// with the index of its unit in the input: the first equal one, as the C++'s search).
fn place_record(
    kind: i32,
    min_radius: f32,
    frame: &EeFormationFrame,
    radii: &[f32],
    slots: &[EeFormationSlot],
    same: &dyn Fn(usize, usize) -> bool,
) -> Record {
    let (tag, words) = shape_of(kind, min_radius).map_or((0, Vec::new()), |s| shape_words(&s));
    let mut record = Record::new(tag);
    record.i32(kind);
    for row in &frame.orientation {
        row.iter().for_each(|&v| record.f32(v));
    }
    frame.center.iter().for_each(|&v| record.f32(v));
    record.f32(frame.heading);
    words.iter().for_each(|&w| record.u32(w));
    record.u32(radii.len() as u32);
    radii.iter().for_each(|&r| record.f32(r));
    record.u32(slots.len() as u32);
    for slot in slots {
        slot.position.iter().for_each(|&v| record.f32(v));
        record.f32(slot.heading);
        let index = (0..radii.len()).find(|&j| same(j, slot.unit as usize)).map_or(u32::MAX, |j| j as u32);
        record.u32(index);
    }
    record
}

/// The slots of a `Place` of `kind` over `radii` and, while the capture runs, its record. None for an empty circle
/// (the C++ returned before placing).
fn place(
    kind: i32,
    min_radius: f32,
    frame: &EeFormationFrame,
    radii: &[f32],
    same: &dyn Fn(usize, usize) -> bool,
) -> Option<Vec<EeFormationSlot>> {
    if is_circle(kind) && radii.is_empty() {
        return None;
    }
    let slots = shape_of(kind, min_radius).map_or(Vec::new(), |shape| replay_shape(&shape, frame, radii));
    if capture_record::active() {
        place_record(kind, min_radius, frame, radii, &slots, same).commit();
    }
    Some(slots)
}

/// A complex unit's `mUnitClass->mOuterCollisionSphere`.
fn outer_radius(token: &WorldRead, unit: u64) -> f32 {
    // SAFETY: the caller's units are live complex units of the world it holds
    let class = unsafe { UnitView::of(token, unit) }.and_then(|u| u.class()).expect("a formation's unit has a class");
    crate::world_field!(class.fields(), mOuterCollisionSphere)
}

/// Places `units` and writes one `EEFormationSlot` per unit into `out`.
///
/// # Safety
/// As [`formation_place`]; `out` holds one slot per unit.
unsafe fn place_into(
    kind: i32,
    min_radius: f32,
    frame: &EeFormationFrame,
    units: &[u64],
    out: *mut EeFormationUnitSlot,
) {
    // SAFETY: the caller holds the world (a call from the simulation's C++)
    let token = unsafe { WorldRead::begin() };
    let radii: Vec<f32> = units.iter().map(|&u| outer_radius(&token, u)).collect();
    let Some(slots) = place(kind, min_radius, frame, &radii, &|j, k| units[j] == units[k]) else { return };
    debug_assert!(slots.len() == units.len(), "a Place fills one slot per unit");
    for (i, slot) in slots.iter().enumerate() {
        let value = EeFormationUnitSlot {
            position: slot.position,
            unit: units[slot.unit as usize] as *mut c_void,
            heading: slot.heading,
        };
        // SAFETY: `out` holds one slot per unit (the caller's contract)
        unsafe { out.add(i).write(value) };
    }
}

/// `EEFormation`'s members from `mOrientation` on (ee/EEFormation.h; the units' lane, round 14: the groups' and the
/// herds' formation statements read and write them here). EEWorldLayout.cpp registers where they start in the
/// object (`formation_fields`); EEGroup.cpp checks each member's place.
#[repr(C)]
#[allow(non_snake_case)]
pub struct EeFormationFields {
    /// `mOrientation` (a `GETransformation`: its `mMatrix`)
    pub mOrientation: [[f32; 4]; 4],
    pub mCenter: Vec3,
    pub mHeading: f32,
    /// 0-3 short, long, loose, staggered line; 4-5 deep, shallow wedge; 6-7 inward, outward circle
    pub mType: c_long,
    pub mB5C: bool,
    /// a circle's lower bound of the radius
    pub mMinRadius: f32,
}

/// The fields of the formation `formation` (an `EEFormation*`).
///
/// # Safety
/// `formation` is a live formation, and nothing else holds its fields for the reference's life.
pub unsafe fn formation_fields<'a>(formation: u64) -> &'a mut EeFormationFields {
    let at = crate::world::layout::layout().formation_fields;
    // SAFETY: the caller's contract; the layout's offset of `mOrientation`
    unsafe { &mut *((formation as usize + at as usize) as *mut EeFormationFields) }
}

/// `EEFormation::Place(units, slots)` into a fresh slot list (one slot per unit; a circle's appending is the
/// same into an empty list), and the capture's record of the call. An empty circle places and records nothing.
///
/// # Safety
/// `formation` is a live formation, `units` live complex units; the caller holds the world.
// (`c_long` is 32 bits on Windows and 64 elsewhere: the cast is needed there)
#[allow(clippy::unnecessary_cast)]
pub unsafe fn formation_place(formation: u64, units: &[u64]) -> Vec<EeFormationUnitSlot> {
    // SAFETY: the caller's contract
    let f = unsafe { formation_fields(formation) };
    let frame = frame_of(&GeTransformation { m: f.mOrientation }, f.mCenter, f.mHeading);
    let mut out = vec![EeFormationUnitSlot { position: [0.0; 3], unit: std::ptr::null_mut(), heading: 0.0 }; units.len()];
    // SAFETY: the caller's contract; `out` holds one slot per unit
    unsafe { place_into(f.mType as i32, f.mMinRadius, &frame, units, out.as_mut_ptr()) };
    out
}

/// `EEFormation::Arrange(group, slots)`: `Place` over the group's units, in the group's order.
///
/// # Safety
/// As [`formation_place`]; `group` is a live `EEGroup`.
pub unsafe fn formation_arrange(formation: u64, group: u64) -> Vec<EeFormationUnitSlot> {
    let units: Vec<u64> = {
        // SAFETY: the caller holds the world
        let token = unsafe { WorldRead::begin() };
        // SAFETY: a live group
        let view = unsafe { GroupView::of(&token, group) };
        view.map_or(Vec::new(), |g| (0..g.len()).map(|i| g.unit_handle(i)).collect())
    };
    // SAFETY: the caller's contract
    unsafe { formation_place(formation, &units) }
}

/// `EEGetHeadingBetween` as the C++ sweep called it: the export, which keeps the leaf's note.
#[cfg(not(test))]
fn heading_between(from: Vec3, to: Vec3) -> f32 {
    // SAFETY: two points of three floats
    unsafe { crate::units::uu_ffi::ee_units_uu_heading_between(&from, &to) }
}
#[cfg(test)]
fn heading_between(from: Vec3, to: Vec3) -> f32 {
    crate::units::uu_world::heading_between(from, to)
}

/// The group sizes the sweep places.
const SWEEP_SIZES: [usize; 16] = [1, 2, 3, 4, 5, 6, 9, 10, 11, 16, 20, 21, 22, 23, 43, 64];

/// The frame and minimum radius the sweep gives a formation of `kind` over `size` units from `first` towards
/// `yaw`: odd sizes keep the identity orientation (as the herds' circles), a circle of a size divisible by three
/// gets a minimum radius of its own.
fn sweep_formation(kind: i32, size: usize, first: Vec3, yaw: f32) -> (EeFormationFrame, f32) {
    let mut orientation = GeTransformation::IDENTITY;
    if size.is_multiple_of(2) {
        orientation.set_orientation(RotationOrder::Ypr, yaw, 0.0, 0.0);
    }
    let min_radius = if is_circle(kind) && size.is_multiple_of(3) { size as f32 * 0.75 } else { 0.5 };
    (frame_of(&orientation, first, yaw), min_radius)
}

/// TEST ONLY: `EEFormationCaptureSweep`. With the capture on, EE-SIM calls this every few game minutes: every
/// formation type places groups of each player's newest living units (their real radii and positions), so that
/// lines and wedges, which the computer players never form, are captured too. Reads the world; changes nothing.
///
/// # Safety
/// The caller holds the world (the simulation thread).
pub unsafe fn capture_sweep() {
    // SAFETY: the caller's contract
    let token = unsafe { WorldRead::begin() };
    let Some(world) = WorldView::global(&token) else { return };
    for p in 1..world.num_players() as usize {
        let Some(player) = world.player(p) else { continue };
        let all: Vec<UnitView> =
            player.units().filter(|u| u.hit_points() > 0).filter_map(|u| u.complex_unit()).collect();
        for &size in SWEEP_SIZES.iter().take_while(|&&s| s <= all.len()) {
            let units = &all[all.len() - size..];
            let (first, last) = (units[0].location(), units[size - 1].location());
            let yaw = heading_between(first, last);
            let radii: Vec<f32> = units.iter().map(|u| outer_radius(&token, u.handle())).collect();
            for kind in 0..8 {
                let (frame, min_radius) = sweep_formation(kind, size, first, yaw);
                place(kind, min_radius, &frame, &radii, &|j, k| units[j].handle() == units[k].handle());
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::formations::capture::{TAG_CIRCLE, TAG_LINE};

    fn frame() -> EeFormationFrame {
        frame_of(&GeTransformation::IDENTITY, [10.0, 20.0, 1.0], 0.25)
    }

    #[test]
    fn the_frame_is_the_rotation_rows_the_center_and_the_heading() {
        let mut t = GeTransformation::IDENTITY;
        t.m[0] = [1.0, 2.0, 3.0, 4.0];
        t.m[2] = [7.0, 8.0, 9.0, 10.0];
        let f = frame_of(&t, [5.0, 6.0, 7.0], 0.5);
        assert_eq!(f.orientation, [[1.0, 2.0, 3.0], [0.0, 1.0, 0.0], [7.0, 8.0, 9.0]]);
        assert_eq!((f.center, f.heading), ([5.0, 6.0, 7.0], 0.5));
    }

    #[test]
    fn an_empty_circle_places_nothing_and_an_empty_line_places_no_slot() {
        assert_eq!(place(6, 0.5, &frame(), &[], &|_, _| false), None);
        assert_eq!(place(7, 0.5, &frame(), &[], &|_, _| false), None);
        assert_eq!(place(0, 0.5, &frame(), &[], &|_, _| false), Some(Vec::new()));
    }

    #[test]
    fn a_place_fills_one_slot_per_unit() {
        let radii = [0.7f32, 1.2, 0.4, 1.0, 0.9];
        for kind in 0..8 {
            let slots = place(kind, 0.5, &frame(), &radii, &|j, k| j == k).unwrap();
            assert_eq!(slots.len(), radii.len(), "type {kind}");
            let mut seen: Vec<u32> = slots.iter().map(|s| s.unit).collect();
            seen.sort_unstable();
            assert_eq!(seen, [0, 1, 2, 3, 4], "type {kind}");
        }
    }

    #[test]
    fn the_record_is_the_cpp_layout() {
        let radii = [0.5f32, 0.75];
        let slots = place(1, 0.5, &frame(), &radii, &|j, k| j == k).unwrap();
        let r = place_record(1, 0.5, &frame(), &radii, &slots, &|j, k| j == k);
        assert_eq!(r.tag, TAG_LINE);
        // the type, 9 rotation floats, the center, the heading, a long line's 14 words, the radii, the slots
        assert_eq!(r.data.len(), 4 + 36 + 12 + 4 + 14 * 4 + 4 + 2 * 4 + 4 + 2 * 20);
        assert_eq!(&r.data[..4], &1i32.to_le_bytes());
        assert_eq!(&r.data[4..8], &1.0f32.to_le_bytes());
        let slot0 = 4 + 36 + 12 + 4 + 56 + 4 + 8 + 4;
        assert_eq!(&r.data[slot0 + 16..slot0 + 20], &slots[0].unit.to_le_bytes());
        // a unit listed twice is found at its first place
        let twice = place_record(1, 0.5, &frame(), &radii, &slots, &|_, _| true);
        assert_eq!(&twice.data[slot0 + 16..slot0 + 20], &0u32.to_le_bytes());
        let none = place_record(6, 2.0, &frame(), &radii, &[], &|_, _| false);
        assert_eq!(none.tag, TAG_CIRCLE);
    }

    #[test]
    fn the_sweep_turns_even_sizes_and_widens_circles_of_three() {
        let (odd, r) = sweep_formation(0, 3, [1.0, 2.0, 3.0], 0.5);
        assert_eq!(odd.orientation, [[1.0, 0.0, 0.0], [0.0, 1.0, 0.0], [0.0, 0.0, 1.0]]);
        assert_eq!((odd.center, odd.heading, r), ([1.0, 2.0, 3.0], 0.5, 0.5));
        let (even, r) = sweep_formation(6, 6, [0.0; 3], 0.5);
        assert_ne!(even.orientation, odd.orientation);
        assert_eq!(r, 4.5);
        assert_eq!(sweep_formation(7, 4, [0.0; 3], 0.5).1, 0.5);
        assert_eq!(sweep_formation(2, 6, [0.0; 3], 0.5).1, 0.5);
    }
}
