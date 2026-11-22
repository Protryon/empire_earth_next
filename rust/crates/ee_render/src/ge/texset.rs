//! `GETextureSet` whole in Rust (LLE round 9): a width x height grid of the terrain's textures, each referenced while
//! it is in the grid. The C++ class keeps one member, this object; a texture's `Dereference` (which may delete it) is
//! the C++'s one-statement callback.

use std::ffi::{c_ulong, c_void};

use crate::ge::cppobj;
use crate::ge::texture;

/// `static_cast<GETexture*>(texture)->Dereference()`.
pub type EeGeTextureDereferenceFn = unsafe extern "C" fn(texture: *mut c_void);

/// The grid (`mTextures[x][y]`) and its sizes.
pub struct EeGeTextureSet {
    textures: Vec<*mut c_void>,
    width: c_ulong,
    height: c_ulong,
    coverage: c_ulong,
    dereference: EeGeTextureDereferenceFn,
}

impl EeGeTextureSet {
    fn at(&self, x: c_ulong, y: c_ulong) -> usize {
        x as usize * self.height as usize + y as usize
    }
}

/// `GETextureSet(inNumTexturesInWidth, inNumTexturesInHeight, inTextureTileCoverage)`: every slot empty.
#[no_mangle]
pub extern "C" fn ee_ge_tset_new(
    width: c_ulong,
    height: c_ulong,
    coverage: c_ulong,
    dereference: EeGeTextureDereferenceFn,
) -> *mut EeGeTextureSet {
    let textures = vec![std::ptr::null_mut(); width as usize * height as usize];
    Box::into_raw(Box::new(EeGeTextureSet { textures, width, height, coverage, dereference }))
}

/// `~GETextureSet`: every texture in the grid released, column by column.
///
/// # Safety
/// `p` is live and not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_tset_free(p: *mut EeGeTextureSet) {
    // SAFETY: the caller's contract
    let p = unsafe { Box::from_raw(p) };
    for &t in p.textures.iter().filter(|t| !t.is_null()) {
        // SAFETY: a texture the grid references
        unsafe { (p.dereference)(t) };
    }
}

/// `SetTexture(inX, inY, inTexture)`: the old texture released, the new one referenced.
///
/// # Safety
/// `p` is live, the slot inside the grid, `texture` null or a live `GETexture`.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_tset_set(p: *mut EeGeTextureSet, x: c_ulong, y: c_ulong, texture: *mut c_void) {
    // SAFETY: the caller's contract
    unsafe {
        let p = &mut *p;
        let i = p.at(x, y);
        if !p.textures[i].is_null() {
            (p.dereference)(p.textures[i]);
        }
        p.textures[i] = texture;
        if !texture.is_null() {
            cppobj::reference(texture as usize);
        }
    }
}

/// `GetTextureAt(inColumn, inRow)` (`slot` false) or `GetTexture(inX, inY)` (`slot`: the tile's slot in the grid).
///
/// # Safety
/// `p` is live, the slot inside the grid.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_tset_get(p: *const EeGeTextureSet, x: c_ulong, y: c_ulong, slot: bool) -> *mut c_void {
    // SAFETY: the caller's contract
    let p = unsafe { &*p };
    let (x, y) = if slot { texture::texture_set_slot(x, y, p.coverage, p.width, p.height) } else { (x, y) };
    p.textures[p.at(x, y)]
}

/// `GetNumTexturesInWidth()` (0), `GetNumTexturesInHeight()` (1), `GetTextureTileCoverage()` (2).
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_tset_size(p: *const EeGeTextureSet, which: u32) -> c_ulong {
    // SAFETY: the caller's contract
    let p = unsafe { &*p };
    [p.width, p.height, p.coverage][which as usize]
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::sync::Mutex;

    static RELEASED: Mutex<Vec<usize>> = Mutex::new(Vec::new());

    unsafe extern "C" fn release(t: *mut c_void) {
        RELEASED.lock().unwrap().push(t as usize);
    }

    #[test]
    fn the_grid_references_and_releases_its_textures() {
        // two fake counted objects: vtable, count, rust
        let mut a = cppobj::CppCounted { vtable: std::ptr::null(), count: 1, rust: std::ptr::null_mut() };
        let mut b = cppobj::CppCounted { vtable: std::ptr::null(), count: 1, rust: std::ptr::null_mut() };
        let (pa, pb) = (&mut a as *mut _ as *mut c_void, &mut b as *mut _ as *mut c_void);
        unsafe {
            let s = ee_ge_tset_new(2, 3, 4, release);
            assert_eq!((ee_ge_tset_size(s, 0), ee_ge_tset_size(s, 1), ee_ge_tset_size(s, 2)), (2, 3, 4));
            assert!(ee_ge_tset_get(s, 1, 2, false).is_null());
            ee_ge_tset_set(s, 1, 2, pa);
            assert_eq!(ee_ge_tset_get(s, 1, 2, false), pa);
            ee_ge_tset_set(s, 1, 2, pb);
            ee_ge_tset_set(s, 0, 0, pa);
            assert_eq!((a.count, b.count), (3, 2));
            assert_eq!(*RELEASED.lock().unwrap(), [pa as usize]);
            ee_ge_tset_free(s);
        }
        assert_eq!(*RELEASED.lock().unwrap(), [pa as usize, pa as usize, pb as usize]);
    }
}
