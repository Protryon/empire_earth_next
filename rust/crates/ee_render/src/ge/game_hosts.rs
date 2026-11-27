//! The game's constructors of the GE objects whose inline C++ constructors name their host (lle/GEBitmap.h,
//! GELine.h, GERhombus.h, GEText.h, GEMaterial.h, GEFont.h): the same objects over the game's two hosts,
//! `GEVertexHost` and `GEObjectHost` (GEBitmap.cpp, C linkage), which this module names, so no unit that constructs
//! one names a symbol of GEBitmap.cpp (tools/cpp_deps.py: the UI's units drew 40 edges to it for the hosts alone).
//! The hosted constructors stay for the tests' own hosts.

use std::ffi::c_void;
use std::os::raw::c_ulong;

use ee_base::math::types::Ge2DPoint;

use crate::ge::bmpobj::{ee_ge_bmp_new, EeGeBitmap, EeGeBmpOp};
use crate::ge::prim::{ee_ge_prim_new, ee_ge_prim_new_line, EeGePrim, EeGePrimKind};
use crate::ge::smallobj::{ee_ge_font_new, ee_ge_mat_new, ee_ge_text_new, EeGeFont, EeGeMat, EeGeObjOp, EeGeText};

#[cfg(not(test))]
extern "C" {
    fn GEVertexHost(owner: *mut c_void, op: EeGeBmpOp, obj: *mut c_void, a: isize, b: isize) -> isize;
    fn GEObjectHost(owner: *mut c_void, op: EeGeObjOp, obj: *mut c_void, a: isize, b: isize) -> isize;
}

// the tests construct over their own hosts: these two stand in for the game's (never called)
#[cfg(test)]
#[allow(non_snake_case)]
unsafe extern "C" fn GEVertexHost(_: *mut c_void, _: EeGeBmpOp, _: *mut c_void, _: isize, _: isize) -> isize {
    unreachable!("the game's vertex host in a test")
}
#[cfg(test)]
#[allow(non_snake_case)]
unsafe extern "C" fn GEObjectHost(_: *mut c_void, _: EeGeObjOp, _: *mut c_void, _: isize, _: isize) -> isize {
    unreachable!("the game's object host in a test")
}

/// `GEBitmap()` over the game's vertex host.
///
/// # Safety
/// `owner` is the C++ bitmap, live until `ee_ge_bmp_destroy`.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_game_bmp_new(owner: *mut c_void) -> *mut EeGeBitmap {
    // SAFETY: the caller's contract; the game's host
    unsafe { ee_ge_bmp_new(GEVertexHost, owner) }
}

/// `GELine()`, `GEPoint()`, `GERhombus()` over the game's vertex host.
///
/// # Safety
/// As [`ee_ge_game_bmp_new`].
#[no_mangle]
pub unsafe extern "C" fn ee_ge_game_prim_new(owner: *mut c_void, kind: EeGePrimKind) -> *mut EeGePrim {
    // SAFETY: as above
    unsafe { ee_ge_prim_new(GEVertexHost, owner, kind) }
}

/// `GELine(start, end, start colour, end colour, opacity)` over the game's vertex host.
///
/// # Safety
/// As [`ee_ge_game_bmp_new`]; the pointers are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_game_prim_new_line(
    owner: *mut c_void,
    start: *const Ge2DPoint,
    end: *const Ge2DPoint,
    start_color: *const c_void,
    end_color: *const c_void,
    opacity: f32,
) -> *mut EeGePrim {
    // SAFETY: as above
    unsafe { ee_ge_prim_new_line(GEVertexHost, owner, start, end, start_color, end_color, opacity) }
}

/// `GEText(font)` over the game's object host.
///
/// # Safety
/// `owner` is the C++ text, live until `ee_ge_text_destroy`.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_game_text_new(owner: *mut c_void, font: *mut c_void) -> *mut EeGeText {
    // SAFETY: as above
    unsafe { ee_ge_text_new(GEObjectHost, owner, font) }
}

/// `GEMaterial(screens, lods)` over the game's object host.
///
/// # Safety
/// `owner` is the C++ material, live until `ee_ge_mat_destroy`.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_game_mat_new(owner: *mut c_void, screens: c_ulong, lods: c_ulong) -> *mut EeGeMat {
    // SAFETY: as above
    unsafe { ee_ge_mat_new(GEObjectHost, owner, screens, lods) }
}

/// `GEFont(name, size, style, quality)` over the game's object host.
///
/// # Safety
/// `owner` is the C++ font, live until `ee_ge_font_destroy`; `name` is a `UString`.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_game_font_new(
    owner: *mut c_void,
    name: *const c_void,
    size: c_ulong,
    style: c_ulong,
    quality: c_ulong,
) -> *mut EeGeFont {
    // SAFETY: as above
    unsafe { ee_ge_font_new(GEObjectHost, owner, name, size, style, quality) }
}

#[cfg(test)]
mod tests {
    // the constructors are the hosted ones (bmpobj.rs, prim.rs and smallobj.rs test those); here only that each
    // names the game's host and nothing else
    #[test]
    fn the_game_constructors_exist() {
        let f: [usize; 6] = [
            super::ee_ge_game_bmp_new as *const () as usize,
            super::ee_ge_game_prim_new as *const () as usize,
            super::ee_ge_game_prim_new_line as *const () as usize,
            super::ee_ge_game_text_new as *const () as usize,
            super::ee_ge_game_mat_new as *const () as usize,
            super::ee_ge_game_font_new as *const () as usize,
        ];
        assert!(f.iter().all(|p| *p != 0));
    }
}
