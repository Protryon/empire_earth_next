//! WONGUI's StretchImage as a Rust object (R6 WON round 38): the nine-piece border image every skinned frame,
//! button and scroll bar draws. The original's StretchImage.cpp: the four borders, the tiling and skip-centre
//! settings, whether the cached image must be rebuilt (a size change marks it; the border and image setters do not,
//! as the original's did not), the attributes a configuration copies, the duplicate's settings and the paint's plan
//! (`guidraw`'s `stretch_paint`).
//!
//! The C++ `StretchImage` stays the image type (the configurations, frames and skins hold it); it keeps its
//! C++-object members (the source raw image and the cached native image) and the Image base's size, and builds the
//! cached image with a StretchImageFilter set from this object (`ee_won_stretch_get`).

use crate::guidraw::{stretch_paint, EeWonDrawOp};

/// The Rust StretchImage.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct EeWonStretchImg {
    pub left: i32,
    pub right: i32,
    pub top: i32,
    pub bottom: i32,
    pub tile_horz: bool,
    pub tile_vert: bool,
    pub skip_center: bool,
    pub need_recalc: bool,
}

/// A setting (`ee_won_stretch_get`).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonStretchField {
    Left = 1,
    Top,
    Right,
    Bottom,
    TileHorz,
    TileVert,
    SkipCenter,
    NeedRecalc,
}

impl EeWonStretchImg {
    /// The constructor: the borders, tiling both ways, the centre drawn, the cache to build.
    pub fn new(left: i32, top: i32, right: i32, bottom: i32) -> Self {
        EeWonStretchImg {
            left,
            right,
            top,
            bottom,
            tile_horz: true,
            tile_vert: true,
            skip_center: false,
            need_recalc: true,
        }
    }

    /// `SetSize` against the image's current size: whether it changes (then the cache must be rebuilt).
    pub fn set_size(&mut self, cur_width: i32, cur_height: i32, width: i32, height: i32) -> bool {
        if width == cur_width && height == cur_height {
            return false;
        }
        self.need_recalc = true;
        true
    }

    /// `CopyAttributes`' settings (the C++ copies the size): borders, tiling, skip-centre.
    pub fn copy_attributes(&mut self, from: &EeWonStretchImg) {
        self.left = from.left;
        self.top = from.top;
        self.right = from.right;
        self.bottom = from.bottom;
        self.tile_horz = from.tile_horz;
        self.tile_vert = from.tile_vert;
        self.skip_center = from.skip_center;
    }

    /// `Duplicate`'s settings: the borders, tiling and skip-centre, a cache to build.
    pub fn duplicate(&self) -> Self {
        EeWonStretchImg { need_recalc: true, ..*self }
    }

    pub fn get(&self, f: EeWonStretchField) -> i32 {
        use EeWonStretchField as F;
        match f {
            F::Left => self.left,
            F::Top => self.top,
            F::Right => self.right,
            F::Bottom => self.bottom,
            F::TileHorz => self.tile_horz as i32,
            F::TileVert => self.tile_vert as i32,
            F::SkipCenter => self.skip_center as i32,
            F::NeedRecalc => self.need_recalc as i32,
        }
    }

    /// `Paint`'s plan at the image's size (after the C++ rebuilt the cache when it had to).
    pub fn paint(&self, width: i32, height: i32) -> Vec<EeWonDrawOp> {
        stretch_paint(self.skip_center, self.left, self.right, self.top, self.bottom, width, height)
    }
}

/// A new StretchImage's settings (the constructor's borders).
#[no_mangle]
pub extern "C" fn ee_won_stretch_new(left: i32, top: i32, right: i32, bottom: i32) -> *mut EeWonStretchImg {
    Box::into_raw(Box::new(EeWonStretchImg::new(left, top, right, bottom)))
}

/// `Duplicate()`'s settings for the copy.
///
/// # Safety
/// `o` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_stretch_duplicate(o: *const EeWonStretchImg) -> *mut EeWonStretchImg {
    // SAFETY: the contract
    Box::into_raw(Box::new(unsafe { (*o).duplicate() }))
}

/// # Safety
/// `o` is null or from `ee_won_stretch_new` / `ee_won_stretch_duplicate`, not freed before.
#[no_mangle]
pub unsafe extern "C" fn ee_won_stretch_free(o: *mut EeWonStretchImg) {
    if !o.is_null() {
        // SAFETY: the contract
        drop(unsafe { Box::from_raw(o) });
    }
}

/// `SetSize(width, height)` against the image's size: true when the C++ must store the new size.
///
/// # Safety
/// `o` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_stretch_set_size(
    o: *mut EeWonStretchImg,
    cur_w: i32,
    cur_h: i32,
    w: i32,
    h: i32,
) -> bool {
    // SAFETY: the contract
    unsafe { (*o).set_size(cur_w, cur_h, w, h) }
}

/// `SetBorders(left, top, right, bottom)` (the cache not marked, as the original's).
///
/// # Safety
/// `o` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_stretch_set_borders(
    o: *mut EeWonStretchImg,
    left: i32,
    top: i32,
    right: i32,
    bottom: i32,
) {
    // SAFETY: the contract
    let o = unsafe { &mut *o };
    o.left = left;
    o.top = top;
    o.right = right;
    o.bottom = bottom;
}

/// `SetTile(horz, vert)`.
///
/// # Safety
/// `o` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_stretch_set_tile(o: *mut EeWonStretchImg, horz: bool, vert: bool) {
    // SAFETY: the contract
    unsafe {
        (*o).tile_horz = horz;
        (*o).tile_vert = vert;
    }
}

/// `SetSkipCenter(skip)`.
///
/// # Safety
/// `o` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_stretch_set_skip(o: *mut EeWonStretchImg, skip: bool) {
    // SAFETY: the contract
    unsafe { (*o).skip_center = skip }
}

/// The cache built (`CalcImage`'s end).
///
/// # Safety
/// `o` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_stretch_built(o: *mut EeWonStretchImg) {
    // SAFETY: the contract
    unsafe { (*o).need_recalc = false }
}

/// `CopyAttributes(from)`'s settings.
///
/// # Safety
/// `o` and `from` are live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_stretch_copy(o: *mut EeWonStretchImg, from: *const EeWonStretchImg) {
    // SAFETY: the contract
    unsafe {
        let f = *from;
        (*o).copy_attributes(&f)
    }
}

/// A setting.
///
/// # Safety
/// `o` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_stretch_get(o: *const EeWonStretchImg, f: EeWonStretchField) -> i32 {
    // SAFETY: the contract
    unsafe { (*o).get(f) }
}

/// `Paint`'s plan at the image's size: the first `cap` operations into `out`, their count returned.
///
/// # Safety
/// `o` is live; `out` has room for `cap` operations (or is NULL).
#[no_mangle]
pub unsafe extern "C" fn ee_won_stretch_paint(
    o: *const EeWonStretchImg,
    width: i32,
    height: i32,
    out: *mut EeWonDrawOp,
    cap: usize,
) -> usize {
    // SAFETY: the contract
    let ops = unsafe { (*o).paint(width, height) };
    if !out.is_null() {
        for (i, op) in ops.iter().take(cap).enumerate() {
            // SAFETY: room for `cap` (the contract)
            unsafe { *out.add(i) = *op };
        }
    }
    ops.len()
}

#[cfg(test)]
#[path = "stretchobj_tests.rs"]
mod tests;
