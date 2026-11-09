//! The `extern "C"` surface of the vertex and polygon arrays, called by the C++ trampoline classes in
//! `Common/DX7Trampolines.cpp`.
//!
//! Every pointer argument is a live array object created by the matching `_new`; float pointers are 2 or 3 floats.
#![allow(clippy::missing_safety_doc)] // the module-level contract above

use crate::vertex::*;

#[no_mangle]
pub extern "C" fn ee_dx7_std_va_new(size: u32) -> *mut StandardVertexArray {
    Box::into_raw(Box::new(StandardVertexArray::new(size)))
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7_std_va_destroy(a: *mut StandardVertexArray) {
    drop(unsafe { Box::from_raw(a) });
}

/// The vertex memory (GX reads it directly).
#[no_mangle]
pub unsafe extern "C" fn ee_dx7_std_va_vertices(a: *mut StandardVertexArray) -> *mut Dx7Vertex {
    unsafe { (*a).as_mut_ptr() }
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7_std_va_copy_from(a: *mut StandardVertexArray, b: *const StandardVertexArray) {
    unsafe { (*a).copy_from(&*b) };
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7_std_va_color(a: *const StandardVertexArray, index: u32, out: *mut f32) {
    let c = unsafe { (*a).color(index) };
    unsafe { core::ptr::copy_nonoverlapping(c.as_ptr(), out, 3) };
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7_std_va_normal(a: *const StandardVertexArray, index: u32, out: *mut f32) {
    let c = unsafe { (*a).normal(index) };
    unsafe { core::ptr::copy_nonoverlapping(c.as_ptr(), out, 3) };
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7_std_va_opacity(a: *const StandardVertexArray, index: u32) -> f32 {
    unsafe { (*a).opacity(index) }
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7_std_va_point(a: *const StandardVertexArray, index: u32, out: *mut f32) {
    let c = unsafe { (*a).point(index) };
    unsafe { core::ptr::copy_nonoverlapping(c.as_ptr(), out, 3) };
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7_std_va_size(a: *const StandardVertexArray) -> u32 {
    unsafe { (*a).len() }
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7_std_va_texture_coordinate(a: *const StandardVertexArray, index: u32, out: *mut f32) {
    let c = unsafe { (*a).texture_coordinate(index) };
    unsafe { core::ptr::copy_nonoverlapping(c.as_ptr(), out, 2) };
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7_std_va_set_normal(a: *mut StandardVertexArray, n: *const f32, index: u32) {
    unsafe { (*a).set_normal([*n, *n.add(1), *n.add(2)], index) };
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7_std_va_set_point(a: *mut StandardVertexArray, p: *const f32, index: u32) {
    unsafe { (*a).set_point([*p, *p.add(1), *p.add(2)], index) };
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7_std_va_set_texture_coordinate(a: *mut StandardVertexArray, uv: *const f32, index: u32) {
    unsafe { (*a).set_texture_coordinate([*uv, *uv.add(1)], index) };
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7_std_va_set_vertex(
    a: *mut StandardVertexArray,
    p: *const f32,
    n: *const f32,
    uv: *const f32,
    index: u32,
) {
    unsafe { (*a).set_vertex([*p, *p.add(1), *p.add(2)], [*n, *n.add(1), *n.add(2)], [*uv, *uv.add(1)], index) };
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7_std_va_size_of(a: *const StandardVertexArray) -> u32 {
    unsafe { (*a).size_of() }
}

#[no_mangle]
pub extern "C" fn ee_dx7_color_va_new(size: u32) -> *mut StdColorVertexArray {
    Box::into_raw(Box::new(StdColorVertexArray::new(size)))
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7_color_va_destroy(a: *mut StdColorVertexArray) {
    drop(unsafe { Box::from_raw(a) });
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7_color_va_vertices(a: *mut StdColorVertexArray) -> *mut Dx7StdColorVertex {
    unsafe { (*a).as_mut_ptr() }
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7_color_va_copy_from(a: *mut StdColorVertexArray, b: *const StdColorVertexArray) {
    unsafe { (*a).copy_from(&*b) };
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7_color_va_color(a: *const StdColorVertexArray, index: u32, out: *mut f32) {
    let c = unsafe { (*a).color(index) };
    unsafe { core::ptr::copy_nonoverlapping(c.as_ptr(), out, 3) };
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7_color_va_normal(a: *const StdColorVertexArray, index: u32, out: *mut f32) {
    let c = unsafe { (*a).normal(index) };
    unsafe { core::ptr::copy_nonoverlapping(c.as_ptr(), out, 3) };
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7_color_va_opacity(a: *const StdColorVertexArray, index: u32) -> f32 {
    unsafe { (*a).opacity(index) }
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7_color_va_point(a: *const StdColorVertexArray, index: u32, out: *mut f32) {
    let c = unsafe { (*a).point(index) };
    unsafe { core::ptr::copy_nonoverlapping(c.as_ptr(), out, 3) };
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7_color_va_size(a: *const StdColorVertexArray) -> u32 {
    unsafe { (*a).len() }
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7_color_va_texture_coordinate(a: *const StdColorVertexArray, index: u32, out: *mut f32) {
    let c = unsafe { (*a).texture_coordinate(index) };
    unsafe { core::ptr::copy_nonoverlapping(c.as_ptr(), out, 2) };
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7_color_va_set_color(a: *mut StdColorVertexArray, c: *const f32, index: u32) {
    unsafe { (*a).set_color([*c, *c.add(1), *c.add(2)], index) };
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7_color_va_set_normal(a: *mut StdColorVertexArray, n: *const f32, index: u32) {
    unsafe { (*a).set_normal([*n, *n.add(1), *n.add(2)], index) };
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7_color_va_set_opacity(a: *mut StdColorVertexArray, opacity: f32, index: u32) {
    unsafe { (*a).set_opacity(opacity, index) };
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7_color_va_set_point(a: *mut StdColorVertexArray, p: *const f32, index: u32) {
    unsafe { (*a).set_point([*p, *p.add(1), *p.add(2)], index) };
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7_color_va_set_texture_coordinate(
    a: *mut StdColorVertexArray,
    uv: *const f32,
    index: u32,
) {
    unsafe { (*a).set_texture_coordinate([*uv, *uv.add(1)], index) };
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7_color_va_set_vertex(
    a: *mut StdColorVertexArray,
    c: *const f32,
    p: *const f32,
    n: *const f32,
    uv: *const f32,
    opacity: f32,
    index: u32,
) {
    unsafe {
        (*a).set_vertex(
            [*c, *c.add(1), *c.add(2)],
            [*p, *p.add(1), *p.add(2)],
            [*n, *n.add(1), *n.add(2)],
            [*uv, *uv.add(1)],
            opacity,
            index,
        )
    };
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7_color_va_size_of(a: *const StdColorVertexArray) -> u32 {
    unsafe { (*a).size_of() }
}

#[no_mangle]
pub extern "C" fn ee_dx7_poly_new(size: u32) -> *mut PolygonArray {
    Box::into_raw(Box::new(PolygonArray::new(size)))
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7_poly_destroy(p: *mut PolygonArray) {
    drop(unsafe { Box::from_raw(p) });
}

/// The 16-bit index memory (GX reads it directly).
#[no_mangle]
pub unsafe extern "C" fn ee_dx7_poly_indices(p: *mut PolygonArray) -> *mut u16 {
    unsafe { (*p).as_mut_ptr() }
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7_poly_size(p: *const PolygonArray) -> u32 {
    unsafe { (*p).size() }
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7_poly_vertex_index(p: *const PolygonArray, polygon: u32, corner: u32) -> u32 {
    unsafe { (*p).vertex_index(polygon, corner) }
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7_poly_set_vertex_index(p: *mut PolygonArray, polygon: u32, corner: u32, index: u32) {
    unsafe { (*p).set_vertex_index(polygon, corner, index) };
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7_poly_set_vertex_indices(p: *mut PolygonArray, polygon: u32, i1: u32, i2: u32, i3: u32) {
    unsafe { (*p).set_vertex_indices(polygon, i1, i2, i3) };
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7_poly_size_of(p: *const PolygonArray) -> u32 {
    unsafe { (*p).size_of() }
}
