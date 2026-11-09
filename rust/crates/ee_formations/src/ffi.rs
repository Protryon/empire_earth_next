//! The C ABI the formation classes call (`src/Empire Earth/Formations`): plain data in, slots out. The caller owns
//! every buffer; a `Place` writes one slot per unit (none for an empty circle) and returns the count.

use crate::{
    circle_place, line_offsets, line_place, wedge_geometry, wedge_place, EeFormationCircle, EeFormationFrame,
    EeFormationLine, EeFormationSlot, EeFormationWedge,
};
use std::slice;

/// # Safety
/// `ptr` must be valid for `len` reads when `len` is not 0.
unsafe fn input<'a, T>(ptr: *const T, len: u32) -> &'a [T] {
    if len == 0 {
        &[]
    } else {
        // SAFETY: the caller's contract
        unsafe { slice::from_raw_parts(ptr, len as usize) }
    }
}

/// # Safety
/// `out` must be valid for `slots.len()` writes.
unsafe fn output(slots: &[EeFormationSlot], out: *mut EeFormationSlot) -> u32 {
    if !slots.is_empty() {
        // SAFETY: the caller's contract
        unsafe { std::ptr::copy_nonoverlapping(slots.as_ptr(), out, slots.len()) };
    }
    slots.len() as u32
}

/// `EEFLine`'s constructor: `count` column offsets into `out`.
///
/// # Safety
/// `out` must be valid for `count` writes.
#[no_mangle]
pub unsafe extern "C" fn ee_formation_line_offsets(spacing: f32, count: i32, out: *mut f32) {
    let offsets = line_offsets(spacing, count);
    if !offsets.is_empty() {
        // SAFETY: the caller's contract
        unsafe { std::ptr::copy_nonoverlapping(offsets.as_ptr(), out, offsets.len()) };
    }
}

/// `EEFWedge`'s constructor.
#[no_mangle]
pub extern "C" fn ee_formation_wedge_geometry(spacing: f32, angle: f32, count: u32) -> EeFormationWedge {
    wedge_geometry(spacing, angle, count)
}

/// `EEFLine::Place` over `count` units' collision radii; returns the slots written to `out`.
///
/// # Safety
/// `frame` and `line` must be valid; `offsets` valid for `line.count` reads; `radii` for `count` reads and `out` for
/// `count` writes.
#[no_mangle]
pub unsafe extern "C" fn ee_formation_line_place(
    frame: *const EeFormationFrame,
    line: *const EeFormationLine,
    offsets: *const f32,
    radii: *const f32,
    count: u32,
    out: *mut EeFormationSlot,
) -> u32 {
    // SAFETY: the caller's contract
    unsafe {
        let line = &*line;
        let offsets = input(offsets, line.count.max(0) as u32);
        output(&line_place(&*frame, line, offsets, input(radii, count)), out)
    }
}

/// `EEFWedge::Place`, as `ee_formation_line_place`.
///
/// # Safety
/// `frame` and `wedge` must be valid; `radii` valid for `count` reads and `out` for `count` writes.
#[no_mangle]
pub unsafe extern "C" fn ee_formation_wedge_place(
    frame: *const EeFormationFrame,
    wedge: *const EeFormationWedge,
    radii: *const f32,
    count: u32,
    out: *mut EeFormationSlot,
) -> u32 {
    // SAFETY: the caller's contract
    unsafe { output(&wedge_place(&*frame, &*wedge, input(radii, count)), out) }
}

/// `EEFCircle::Place`, as `ee_formation_line_place` (0 slots for no units).
///
/// # Safety
/// `frame` and `circle` must be valid; `radii` valid for `count` reads and `out` for `count` writes.
#[no_mangle]
pub unsafe extern "C" fn ee_formation_circle_place(
    frame: *const EeFormationFrame,
    circle: *const EeFormationCircle,
    radii: *const f32,
    count: u32,
    out: *mut EeFormationSlot,
) -> u32 {
    // SAFETY: the caller's contract
    unsafe { output(&circle_place(&*frame, &*circle, input(radii, count)), out) }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::Rotation;

    fn frame() -> EeFormationFrame {
        EeFormationFrame {
            orientation: Rotation::yaw_pitch_roll(0.3, 0.0, 0.0).0,
            center: [10.0, 20.0, 1.0],
            heading: 0.3,
        }
    }

    #[test]
    fn line_place_writes_one_slot_per_unit() {
        let line = EeFormationLine { count: 5, rows: 6, spacing: 1.0, depth: 0.0 };
        let mut offsets = [0.0f32; 5];
        // SAFETY: buffers sized as the contract asks
        unsafe { ee_formation_line_offsets(1.0, 5, offsets.as_mut_ptr()) };
        let radii = [1.0f32; 7];
        let mut out = [EeFormationSlot::default(); 7];
        // SAFETY: as above
        let n =
            unsafe { ee_formation_line_place(&frame(), &line, offsets.as_ptr(), radii.as_ptr(), 7, out.as_mut_ptr()) };
        assert_eq!(n, 7);
        assert_eq!(out.to_vec(), line_place(&frame(), &line, &offsets, &radii));
    }

    #[test]
    fn wedge_and_circle_match_the_safe_functions() {
        let wedge = ee_formation_wedge_geometry(1.0, 0.785398185, 21);
        let circle = EeFormationCircle { mode: 1, half_arc: 3.14159265, min_radius: 0.5 };
        let radii = [0.7f32, 1.2, 0.4];
        let mut out = [EeFormationSlot::default(); 3];
        // SAFETY: buffers sized as the contract asks
        let n = unsafe { ee_formation_wedge_place(&frame(), &wedge, radii.as_ptr(), 3, out.as_mut_ptr()) };
        assert_eq!((n, out.to_vec()), (3, wedge_place(&frame(), &wedge, &radii)));
        // SAFETY: as above
        let n = unsafe { ee_formation_circle_place(&frame(), &circle, radii.as_ptr(), 3, out.as_mut_ptr()) };
        assert_eq!((n, out.to_vec()), (3, circle_place(&frame(), &circle, &radii)));
    }

    #[test]
    fn empty_inputs_take_null_pointers() {
        let circle = EeFormationCircle { mode: 0, half_arc: 1.0, min_radius: 0.5 };
        // SAFETY: a zero count reads and writes nothing
        let n = unsafe { ee_formation_circle_place(&frame(), &circle, std::ptr::null(), 0, std::ptr::null_mut()) };
        assert_eq!(n, 0);
    }
}
