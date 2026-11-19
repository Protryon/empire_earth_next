//! The C ABI of the one formation class (`EEFormation`, `src/Empire Earth/Formations/EEFormation.cpp`): a formation is
//! its type, its frame and (circles) its minimum radius; the members the eight constructors once gave it (a line's
//! offsets, a wedge's arms, a circle's mode and arc) are this crate's (`capture::shape_of`). The caller owns every
//! buffer; a `Place` writes one slot per unit (none for an empty circle) and returns the count.

use crate::formations::capture::{replay_shape, shape_of, shape_words};
use crate::formations::{EeFormationFrame, EeFormationSlot};
use std::slice;

/// The most words a shape's capture has (a long line: 4 + 10 offsets).
pub const EE_FORMATION_SHAPE_WORDS: u32 = 16;

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
/// `out` must be valid for `slots.len()` writes when there is a slot.
unsafe fn output(slots: &[EeFormationSlot], out: *mut EeFormationSlot) -> u32 {
    if !slots.is_empty() {
        // SAFETY: the caller's contract
        unsafe { std::ptr::copy_nonoverlapping(slots.as_ptr(), out, slots.len()) };
    }
    slots.len() as u32
}

/// `EEFormation::Place` of the formation type `kind` (0-7) over the units' outer collision radii (`count` of them,
/// in input order); 0 slots for a type `Create` does not make.
///
/// # Safety
/// `frame` must be valid; `radii` valid for `count` reads and `out` for `count` writes when `count` is not 0.
#[no_mangle]
pub unsafe extern "C" fn ee_formation_place(
    kind: i32,
    min_radius: f32,
    frame: *const EeFormationFrame,
    radii: *const f32,
    count: u32,
    out: *mut EeFormationSlot,
) -> u32 {
    let Some(shape) = shape_of(kind, min_radius) else { return 0 };
    // SAFETY: the caller's contract
    unsafe { output(&replay_shape(&shape, &*frame, input(radii, count)), out) }
}

/// The capture record of a `Place` of `kind`: its tag (0 for no type) and the shape's words after the frame (at most
/// `EE_FORMATION_SHAPE_WORDS`), their count returned.
///
/// # Safety
/// `tag` must be valid; `words` valid for `EE_FORMATION_SHAPE_WORDS` writes.
#[no_mangle]
pub unsafe extern "C" fn ee_formation_shape_words(kind: i32, min_radius: f32, tag: *mut u32, words: *mut u32) -> u32 {
    let (t, w) = shape_of(kind, min_radius).map_or((0, Vec::new()), |s| shape_words(&s));
    let n = w.len().min(EE_FORMATION_SHAPE_WORDS as usize);
    // SAFETY: the caller's contract
    unsafe {
        *tag = t;
        std::ptr::copy_nonoverlapping(w.as_ptr(), words, n);
    }
    n as u32
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::formations::capture::{Shape, TAG_CIRCLE, TAG_LINE, TAG_WEDGE};
    use crate::formations::{circle_place, line_offsets, line_place, wedge_geometry, wedge_place, Rotation};

    fn frame() -> EeFormationFrame {
        EeFormationFrame {
            orientation: Rotation::yaw_pitch_roll(0.3, 0.0, 0.0).0,
            center: [10.0, 20.0, 1.0],
            heading: 0.3,
        }
    }

    #[test]
    fn place_of_each_type_matches_its_preset() {
        let radii = [0.7f32, 1.2, 0.4, 1.0, 0.9, 1.1, 0.3];
        for kind in 0..8 {
            let mut out = [EeFormationSlot::default(); 7];
            // SAFETY: buffers sized as the contract asks
            let n = unsafe { ee_formation_place(kind, 0.5, &frame(), radii.as_ptr(), 7, out.as_mut_ptr()) };
            let expected = match shape_of(kind, 0.5).unwrap() {
                Shape::Line(l, o) => line_place(&frame(), &l, &o, &radii),
                Shape::Wedge(w) => wedge_place(&frame(), &w, &radii),
                Shape::Circle(c) => circle_place(&frame(), &c, &radii),
            };
            assert_eq!(out[..n as usize].to_vec(), expected, "type {kind}");
        }
    }

    #[test]
    fn the_presets_are_the_constructors() {
        let Some(Shape::Line(l, o)) = shape_of(1, 0.5) else { panic!() };
        assert_eq!((l.count, l.rows, l.spacing, l.depth), (10, 6, 1.0, 0.0));
        assert_eq!(o, line_offsets(1.0, 10));
        assert_eq!(shape_of(5, 0.5), Some(Shape::Wedge(wedge_geometry(1.0, 1.39626348, 0x15))));
        let Some(Shape::Circle(c)) = shape_of(7, 2.5) else { panic!() };
        assert_eq!((c.mode, c.half_arc, c.min_radius), (2, 3.14159265, 2.5));
        assert_eq!(shape_of(8, 0.5), None);
        assert_eq!(shape_of(-1, 0.5), None);
    }

    #[test]
    fn shape_words_are_the_records_members() {
        let mut tag = 0;
        let mut words = [0u32; EE_FORMATION_SHAPE_WORDS as usize];
        // SAFETY: buffers sized as the contract asks
        let n = unsafe { ee_formation_shape_words(1, 0.5, &mut tag, words.as_mut_ptr()) };
        assert_eq!((tag, n), (TAG_LINE, 14));
        assert_eq!(&words[..4], &[10, 6, 1.0f32.to_bits(), 0]);
        // SAFETY: as above
        let n = unsafe { ee_formation_shape_words(4, 0.5, &mut tag, words.as_mut_ptr()) };
        assert_eq!((tag, n, words[8]), (TAG_WEDGE, 9, 0x15));
        // SAFETY: as above
        let n = unsafe { ee_formation_shape_words(6, 1.75, &mut tag, words.as_mut_ptr()) };
        assert_eq!((tag, n), (TAG_CIRCLE, 3));
        assert_eq!(&words[..3], &[1, 3.14159265f32.to_bits(), 1.75f32.to_bits()]);
        // SAFETY: as above
        let n = unsafe { ee_formation_shape_words(9, 0.5, &mut tag, words.as_mut_ptr()) };
        assert_eq!((tag, n), (0, 0));
    }

    #[test]
    fn empty_inputs_take_null_pointers() {
        // SAFETY: a zero count reads and writes nothing
        let n = unsafe { ee_formation_place(6, 0.5, &frame(), std::ptr::null(), 0, std::ptr::null_mut()) };
        assert_eq!(n, 0);
    }
}
