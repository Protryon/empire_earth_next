//! The value types' members that copied, compared and coded their fields in C++ (R4.3, twelfth part):
//! `GETransformation`'s constructors, `operator=`, comparisons, identity, scale and zero-orientation setters and its
//! file codec; the value types' float codecs (`GEVector`, `GE3DPoint`, `GEOrientation`, `GERectangle`, `GEColor`,
//! `GETextureCoordinate`); `GEViewport::operator=`. The pointers are the C++ objects themselves (`this` and the arguments, which may be the same object).
//! The simulation copies transformations everywhere: the copies are the sixteen floats as they are (bit for bit,
//! a NaN's payload and -0 kept), the comparisons IEEE's (a NaN never equals).

use std::ffi::{c_long, c_void};

use crate::geval::io::{cpp_file_io, CppIo, EeGeFieldIo, FieldIo, OK};
use crate::geval::viewport::GeViewport;
use ee_base::fs::CppFile;
use ee_base::math::types::{
    Ge2DPoint, Ge3DLine, Ge3DPlane, Ge3DPoint, GeBoundingBox, GeOrientation, GeRectangle, GeTransformation, GeVector,
};

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
pub(crate) unsafe fn t_io(t: *mut GeTransformation, io: &EeGeFieldIo, write: bool) -> c_long {
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

/// `GETransformation::Load(FSFile&)` over a C++ file ([`t_io`]).
///
/// # Safety
/// `t` is valid; `file` is a live C++ `FSFile*`.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_t_load(t: *mut GeTransformation, file: *mut c_void) -> c_long {
    // SAFETY: the function's contract
    unsafe { t_io(t, &cpp_file_io(file), false) }
}

/// `GETransformation::Save(FSFile&)` over a C++ file ([`t_io`]).
///
/// # Safety
/// As [`ee_ge_t_load`].
#[no_mangle]
pub unsafe extern "C" fn ee_ge_t_save(t: *const GeTransformation, file: *mut c_void) -> c_long {
    // SAFETY: the function's contract (a save only reads the elements)
    unsafe { t_io(t.cast_mut(), &cpp_file_io(file), true) }
}

/// `kErrorNotOpen`.
const NOT_OPEN: c_long = 4;

/// The value types' file codecs (`GEVector`, `GE3DPoint`, `GEOrientation`, `GERectangle`, `GEColor`,
/// `GETextureCoordinate` `Load`/`Save`): the fields in the order given through the file's own float reads or writes,
/// stopping at the first error. `open` is the file's `GetIsOpen()` (false: `kErrorNotOpen` with no access), or -1 for
/// the codecs that do not ask (`GEColor`). `restore` (`GETextureCoordinate`): the position is read first (its error
/// returned as is) and set back after a failed field.
///
/// # Safety
/// `fields` are valid float pointers; `io` is a live field stream.
pub(crate) unsafe fn fields_io(fields: &[*mut f32], io: &EeGeFieldIo, open: i32, write: bool, restore: bool) -> c_long {
    if open == 0 {
        return NOT_OPEN;
    }
    let mut io = CppIo(*io);
    let mut position: c_long = 0;
    if restore {
        let result = io.get_position(&mut position);
        if result != OK {
            return result;
        }
    }
    let mut result = OK;
    for &field in fields {
        // SAFETY: the function's contract
        result = unsafe {
            if write {
                io.write_f32(*field)
            } else {
                io.read_f32(&mut *field)
            }
        };
        if result != OK {
            break;
        }
    }
    if restore && result != OK {
        io.set_position(position);
    }
    result
}

/// [`fields_io`] over a C++ `FSFile*`, asking `GetIsOpen()` first when `ask_open`, with no position to restore.
///
/// # Safety
/// `fields` are valid float pointers; `file` is a live C++ `FSFile*`.
unsafe fn file_fields_io(fields: &[*mut f32], file: *mut c_void, ask_open: bool, write: bool) -> c_long {
    let open = if ask_open { i32::from(CppFile(file).is_open()) } else { -1 };
    // SAFETY: the function's contract
    unsafe { fields_io(fields, &cpp_file_io(file), open, write, false) }
}

/// `GE2DPoint::Load(F&)`: x, y; `kErrorNotOpen` when the file is not open ([`fields_io`]).
///
/// # Safety
/// `value` is valid; `file` is a live C++ `FSFile*`.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_2d_point_load(value: *mut Ge2DPoint, file: *mut c_void) -> c_long {
    // SAFETY: the function's contract
    unsafe { file_fields_io(&[&raw mut (*value).x, &raw mut (*value).y], file, true, false) }
}

/// `GE2DPoint::Save(F&)`: as the load.
///
/// # Safety
/// As [`ee_ge_2d_point_load`].
#[no_mangle]
pub unsafe extern "C" fn ee_ge_2d_point_save(value: *const Ge2DPoint, file: *mut c_void) -> c_long {
    let value = value.cast_mut();
    // SAFETY: the function's contract (a save only reads the fields)
    unsafe { file_fields_io(&[&raw mut (*value).x, &raw mut (*value).y], file, true, true) }
}

/// `GE3DPoint::Load(F&)`: x, y, z; `kErrorNotOpen` when the file is not open ([`fields_io`]).
///
/// # Safety
/// `value` is valid; `file` is a live C++ `FSFile*`.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_3d_point_load(value: *mut Ge3DPoint, file: *mut c_void) -> c_long {
    // SAFETY: the function's contract
    unsafe { file_fields_io(&[&raw mut (*value).x, &raw mut (*value).y, &raw mut (*value).z], file, true, false) }
}

/// `GE3DPoint::Save(F&)`: as the load.
///
/// # Safety
/// As [`ee_ge_3d_point_load`].
#[no_mangle]
pub unsafe extern "C" fn ee_ge_3d_point_save(value: *const Ge3DPoint, file: *mut c_void) -> c_long {
    let value = value.cast_mut();
    // SAFETY: the function's contract (a save only reads the fields)
    unsafe { file_fields_io(&[&raw mut (*value).x, &raw mut (*value).y, &raw mut (*value).z], file, true, true) }
}

/// `GEVector::Load(F&)`: x, y, z; `kErrorNotOpen` when the file is not open ([`fields_io`]).
///
/// # Safety
/// `value` is valid; `file` is a live C++ `FSFile*`.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_vector_load(value: *mut GeVector, file: *mut c_void) -> c_long {
    // SAFETY: the function's contract
    unsafe { file_fields_io(&[&raw mut (*value).x, &raw mut (*value).y, &raw mut (*value).z], file, true, false) }
}

/// `GEVector::Save(F&)`: as the load.
///
/// # Safety
/// As [`ee_ge_vector_load`].
#[no_mangle]
pub unsafe extern "C" fn ee_ge_vector_save(value: *const GeVector, file: *mut c_void) -> c_long {
    let value = value.cast_mut();
    // SAFETY: the function's contract (a save only reads the fields)
    unsafe { file_fields_io(&[&raw mut (*value).x, &raw mut (*value).y, &raw mut (*value).z], file, true, true) }
}

/// `GEOrientation::Load(F&)`: yaw, pitch, roll; `kErrorNotOpen` when the file is not open ([`fields_io`]).
///
/// # Safety
/// `value` is valid; `file` is a live C++ `FSFile*`.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_orientation_load(value: *mut GeOrientation, file: *mut c_void) -> c_long {
    // SAFETY: the function's contract
    unsafe {
        file_fields_io(&[&raw mut (*value).yaw, &raw mut (*value).pitch, &raw mut (*value).roll], file, true, false)
    }
}

/// `GEOrientation::Save(F&)`: as the load.
///
/// # Safety
/// As [`ee_ge_orientation_load`].
#[no_mangle]
pub unsafe extern "C" fn ee_ge_orientation_save(value: *const GeOrientation, file: *mut c_void) -> c_long {
    let value = value.cast_mut();
    // SAFETY: the function's contract (a save only reads the fields)
    unsafe {
        file_fields_io(&[&raw mut (*value).yaw, &raw mut (*value).pitch, &raw mut (*value).roll], file, true, true)
    }
}

/// `GERectangle::Load(F&)`: left, top, right, bottom; `kErrorNotOpen` when the file is not open ([`fields_io`]).
///
/// # Safety
/// `value` is valid; `file` is a live C++ `FSFile*`.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_rect_load(value: *mut GeRectangle, file: *mut c_void) -> c_long {
    // SAFETY: the function's contract
    unsafe {
        file_fields_io(
            &[&raw mut (*value).left, &raw mut (*value).top, &raw mut (*value).right, &raw mut (*value).bottom],
            file,
            true,
            false,
        )
    }
}

/// `GERectangle::Save(F&)`: as the load.
///
/// # Safety
/// As [`ee_ge_rect_load`].
#[no_mangle]
pub unsafe extern "C" fn ee_ge_rect_save(value: *const GeRectangle, file: *mut c_void) -> c_long {
    let value = value.cast_mut();
    // SAFETY: the function's contract (a save only reads the fields)
    unsafe {
        file_fields_io(
            &[&raw mut (*value).left, &raw mut (*value).top, &raw mut (*value).right, &raw mut (*value).bottom],
            file,
            true,
            true,
        )
    }
}

/// `GEColor::Load(F&)`: red, green, blue (it does not ask whether the file is open) ([`fields_io`]).
///
/// # Safety
/// `value` is valid; `file` is a live C++ `FSFile*`.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_color_load(value: *mut [f32; 3], file: *mut c_void) -> c_long {
    // SAFETY: the function's contract
    unsafe { file_fields_io(&[&raw mut (*value)[0], &raw mut (*value)[1], &raw mut (*value)[2]], file, false, false) }
}

/// `GEColor::Save(F&)`: as the load.
///
/// # Safety
/// As [`ee_ge_color_load`].
#[no_mangle]
pub unsafe extern "C" fn ee_ge_color_save(value: *const [f32; 3], file: *mut c_void) -> c_long {
    let value = value.cast_mut();
    // SAFETY: the function's contract (a save only reads the fields)
    unsafe { file_fields_io(&[&raw mut (*value)[0], &raw mut (*value)[1], &raw mut (*value)[2]], file, false, true) }
}

/// `GE3DLine::Load(F&)`: the start, then (when it loaded) the direction ([`ee_ge_3d_point_load`],
/// [`ee_ge_vector_load`]).
///
/// # Safety
/// `line` is valid; `file` is a live C++ `FSFile*`.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_3d_line_load(line: *mut Ge3DLine, file: *mut c_void) -> c_long {
    // SAFETY: the function's contract
    unsafe {
        let result = ee_ge_3d_point_load(&raw mut (*line).start, file);
        if result == OK {
            ee_ge_vector_load(&raw mut (*line).direction, file)
        } else {
            result
        }
    }
}

/// `GE3DLine::Save(F&)`: the start, then the direction.
///
/// # Safety
/// As [`ee_ge_3d_line_load`].
#[no_mangle]
pub unsafe extern "C" fn ee_ge_3d_line_save(line: *const Ge3DLine, file: *mut c_void) -> c_long {
    // SAFETY: the function's contract
    unsafe {
        let result = ee_ge_3d_point_save(&raw const (*line).start, file);
        if result == OK {
            ee_ge_vector_save(&raw const (*line).direction, file)
        } else {
            result
        }
    }
}

/// `GE3DPlane::Load(F&)`: the point, then (when it loaded) the normal.
///
/// # Safety
/// `plane` is valid; `file` is a live C++ `FSFile*`.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_3d_plane_load(plane: *mut Ge3DPlane, file: *mut c_void) -> c_long {
    // SAFETY: the function's contract
    unsafe {
        let result = ee_ge_3d_point_load(&raw mut (*plane).point, file);
        if result == OK {
            ee_ge_vector_load(&raw mut (*plane).normal, file)
        } else {
            result
        }
    }
}

/// `GE3DPlane::Save(F&)`: the point, then the normal.
///
/// # Safety
/// As [`ee_ge_3d_plane_load`].
#[no_mangle]
pub unsafe extern "C" fn ee_ge_3d_plane_save(plane: *const Ge3DPlane, file: *mut c_void) -> c_long {
    // SAFETY: the function's contract
    unsafe {
        let result = ee_ge_3d_point_save(&raw const (*plane).point, file);
        if result == OK {
            ee_ge_vector_save(&raw const (*plane).normal, file)
        } else {
            result
        }
    }
}

/// `GEBoundingBox::Load(F&)`: the two corners (the first error returned as is), then the extents from them (width
/// x, height z, depth y).
///
/// # Safety
/// `bbox` is valid; `file` is a live C++ `FSFile*`.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_bbox_load(bbox: *mut GeBoundingBox, file: *mut c_void) -> c_long {
    // SAFETY: the function's contract
    unsafe {
        let mut result = ee_ge_3d_point_load(&raw mut (*bbox).bottom_left_front, file);
        if result == OK {
            result = ee_ge_3d_point_load(&raw mut (*bbox).top_right_back, file);
        }
        if result != OK {
            return result;
        }
        let b = &mut *bbox;
        b.width = b.top_right_back.x - b.bottom_left_front.x;
        b.height = b.top_right_back.z - b.bottom_left_front.z;
        b.depth = b.top_right_back.y - b.bottom_left_front.y;
        result
    }
}

/// `GEBoundingBox::Save(F&)`: the two corners.
///
/// # Safety
/// As [`ee_ge_bbox_load`].
#[no_mangle]
pub unsafe extern "C" fn ee_ge_bbox_save(bbox: *const GeBoundingBox, file: *mut c_void) -> c_long {
    // SAFETY: the function's contract
    unsafe {
        let result = ee_ge_3d_point_save(&raw const (*bbox).bottom_left_front, file);
        if result == OK {
            ee_ge_3d_point_save(&raw const (*bbox).top_right_back, file)
        } else {
            result
        }
    }
}

/// `GEViewport::operator=`: every field.
///
/// # Safety
/// `out` and `v` are valid (they may be the same viewport).
#[no_mangle]
pub unsafe extern "C" fn ee_ge_viewport_copy(out: *mut GeViewport, v: *const GeViewport) {
    // SAFETY: the function's contract (the source is read whole before the store)
    unsafe {
        let source = *v;
        *out = source;
    }
}

#[cfg(test)]
#[path = "values_tests.rs"]
mod tests;
