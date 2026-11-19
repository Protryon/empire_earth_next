//! TEST ONLY: the store gate's reads of the arrays' capture (`geval::vertarr_capture`; tools/eemodel/storegate). The
//! gate makes the game's own `GEVertexArray` and `GEPolygonArray` (no subclass of its own any more), turns the capture
//! on before loading, and digests each array's shadow: what the gate's memory arrays kept before.

use std::ffi::c_void;

use crate::ge::cppobj::CppArray;
use crate::geval::vertarr_capture as capture;

/// A vertex array's shadow (its floats live until the array's next change).
#[repr(C)]
pub struct EeGeStoreVertices {
    pub kind: u32,
    pub out_of_range: u32,
    pub points: *const f32,
    pub points_len: usize,
    pub normals: *const f32,
    pub normals_len: usize,
    pub uvs: *const f32,
    pub uvs_len: usize,
    pub colors: *const f32,
    pub colors_len: usize,
    pub opacities: *const f32,
    pub opacities_len: usize,
}

/// Turns the arrays' capture on or off.
#[no_mangle]
pub extern "C" fn ee_ge_store_capture(on: bool) {
    capture::set_on(on);
}

/// The Rust array of a C++ `GEVertexArray*` or `GEPolygonArray*`.
///
/// # Safety
/// `array` is a live C++ array.
unsafe fn rust_of(array: *const c_void) -> usize {
    // SAFETY: the caller's contract (the layout cppobj.rs reads)
    unsafe { (*(array as *const CppArray)).rust as usize }
}

/// The shadow of the C++ vertex array `array` into `out`; false when it has none.
///
/// # Safety
/// `array` is a live C++ `GEVertexArray*`, `out` valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_store_vertices(array: *const c_void, out: *mut EeGeStoreVertices) -> bool {
    // SAFETY: the caller's contract
    let rust = unsafe { rust_of(array) };
    capture::with_va(rust, |s| {
        let v = EeGeStoreVertices {
            kind: s.kind,
            out_of_range: s.out_of_range,
            points: s.points.as_ptr(),
            points_len: s.points.len(),
            normals: s.normals.as_ptr(),
            normals_len: s.normals.len(),
            uvs: s.uvs.as_ptr(),
            uvs_len: s.uvs.len(),
            colors: s.colors.as_ptr(),
            colors_len: s.colors.len(),
            opacities: s.opacities.as_ptr(),
            opacities_len: s.opacities.len(),
        };
        // SAFETY: the caller's contract
        unsafe { *out = v };
    })
    .is_some()
}

/// The shadow of the C++ polygon array `array`: its indices (three per triangle) and the sets past its end; false
/// when it has none.
///
/// # Safety
/// `array` is a live C++ `GEPolygonArray*`, the outs valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_store_polygons(
    array: *const c_void,
    indices: *mut *const u32,
    count: *mut usize,
    out_of_range: *mut u32,
) -> bool {
    // SAFETY: the caller's contract
    let rust = unsafe { rust_of(array) };
    capture::with_pa(rust, |s| {
        // SAFETY: the caller's contract
        unsafe {
            *indices = s.indices.as_ptr();
            *count = s.indices.len();
            *out_of_range = s.out_of_range;
        }
    })
    .is_some()
}
