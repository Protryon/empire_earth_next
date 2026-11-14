//! The value types' members that copied, compared and coded their fields in C++ (R4.3, twelfth part):
//! `GETransformation`'s constructors, `operator=`, comparisons, identity, scale and zero-orientation setters and its
//! file codec. The pointers are the C++ objects themselves (`this` and the arguments, which may be the same object).
//! The simulation copies transformations everywhere: the copies are the sixteen floats as they are (bit for bit,
//! a NaN's payload and -0 kept), the comparisons IEEE's (a NaN never equals).

use std::ffi::c_long;

use crate::io::{CppIo, EeGeFieldIo, FieldIo, OK};
use crate::types::{Ge3DPoint, GeTransformation};

/// `GETransformation()` and `MakeIdentity()`: the identity.
///
/// # Safety
/// `t` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_t_identity(t: *mut GeTransformation) {
    // SAFETY: the function's contract
    unsafe { *t = GeTransformation::IDENTITY };
}

/// The copy constructor, `operator=` and `GETransformation(const float[4][4])`: the sixteen elements.
///
/// # Safety
/// `t` and `m` are valid (they may be the same matrix).
#[no_mangle]
pub unsafe extern "C" fn ee_ge_t_copy(t: *mut GeTransformation, m: *const [[f32; 4]; 4]) {
    // SAFETY: the function's contract (the source is read whole before the store)
    unsafe {
        let source = *m;
        (*t).m = source;
    }
}

/// `operator==(GETransformation)`: every element equal (a NaN never is).
///
/// # Safety
/// `a` and `b` are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_t_equal(a: *const GeTransformation, b: *const GeTransformation) -> bool {
    // SAFETY: the function's contract
    unsafe { (*a).m == (*b).m }
}

/// `operator==(GE3DPoint)`: the position equals the point.
///
/// # Safety
/// `t` and `p` are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_t_at_point(t: *const GeTransformation, p: *const Ge3DPoint) -> bool {
    // SAFETY: the function's contract
    let (t, p) = unsafe { (&*t, *p) };
    t.x() == p.x && t.y() == p.y && t.z() == p.z
}

/// `SetScale(x, y, z)`: a pure scale, the position kept.
///
/// # Safety
/// `t` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_t_set_scale(t: *mut GeTransformation, x: f32, y: f32, z: f32) {
    // SAFETY: the function's contract
    unsafe { (*t).set_scale(x, y, z) }
}

/// `ZeroOrientation(out)` (and `ZeroOrientation()` with `out` = `t`): the identity rotation at `t`'s position.
///
/// # Safety
/// `t` and `out` are valid (they may be the same matrix).
#[no_mangle]
pub unsafe extern "C" fn ee_ge_t_zero_orientation(t: *const GeTransformation, out: *mut GeTransformation) {
    // SAFETY: the function's contract (the position is read before the store)
    unsafe {
        let (x, y, z) = ((*t).x(), (*t).y(), (*t).z());
        (*out).m = [[1.0, 0.0, 0.0, x], [0.0, 1.0, 0.0, y], [0.0, 0.0, 1.0, z], [0.0, 0.0, 0.0, 1.0]];
    }
}

/// `Load(FSFile&)` (`write` false) or `Save(FSFile&)`: the sixteen elements row by row through the file's own
/// float reads or writes, stopping at the first error (a load leaves the elements read so far).
///
/// # Safety
/// `t` is valid; `io` is a live field stream.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_t_io(t: *mut GeTransformation, io: *const EeGeFieldIo, write: bool) -> c_long {
    // SAFETY: the function's contract
    let (t, mut io) = unsafe { (&mut *t, CppIo(*io)) };
    let mut result = OK;
    'rows: for row in t.m.iter_mut() {
        for element in row.iter_mut() {
            result = if write { io.write_f32(*element) } else { io.read_f32(element) };
            if result != OK {
                break 'rows;
            }
        }
    }
    result
}

#[cfg(test)]
#[path = "values_tests.rs"]
mod tests;
