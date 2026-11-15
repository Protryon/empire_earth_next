//! `UIBitmap` and `UIMovie` whole in Rust (R6 ui round 21): the two controls that show a GE object (a `GEBitmap` at
//! the control's top left, a `GEMovie` rendered in the text pass). Their members are [`EeUiBitmapFields`] and
//! [`EeUiMovieFields`], the C++ classes' private bases (`GetBitmap` reads the record in place), and every method runs
//! here: the constructors' references, the destructors' releases, `CopyData`, `DrawPolygons`, `DrawTextA` and
//! `SetBitmap`.
//!
//! Every call out is an [`EeUiOp`] on the record's host (`UIBitmap.cpp`'s `UIMediaHost`, whose object is the record):
//! `UIControl`'s base members and the bitmap ops every whole object shares (`UIObjHost::Base`/`Media`), and the movie's
//! own (610..614). The GE objects stay C++, held by address.

use core::ffi::c_void;

use crate::obj::{EeUiHostFn, EeUiOp as Op, Host};

/// `UIBitmap`'s members (`include/lle/UIBitmap.h`), the C++ class's private base.
#[repr(C)]
#[allow(non_snake_case)]
#[derive(Debug)]
pub struct EeUiBitmapFields {
    /// The host of the calls out (its object is the record).
    pub mBitmapHost: EeUiHostFn,
    /// The control's top left as of the last `CopyData` (x, y).
    pub mLocation: [f32; 2],
    /// The bitmap (`GEBitmap*`, referenced; NULL for none).
    pub mBitmap: *mut c_void,
    /// The bitmap as of the last `CopyData` (referenced), drawn.
    pub mCopiedBitmap: *mut c_void,
}

/// `UIMovie`'s members (`include/lle/UIMovie.h`), the C++ class's private base.
#[repr(C)]
#[allow(non_snake_case)]
#[derive(Debug)]
pub struct EeUiMovieFields {
    /// The host of the calls out (its object is the record).
    pub mMovieHost: EeUiHostFn,
    /// The movie (`GEMovie*`, referenced).
    pub mMovie: *mut c_void,
}

/// UIBitmap's inward calls.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeUiBitmapCall {
    /// The constructor's part after `UIControl`'s: the bitmap `a` (referenced when not NULL), no copy.
    Init = 1,
    /// The destructor: the bitmap and its copy released.
    Release = 2,
    /// `CopyData()` -> the base's result.
    CopyData = 3,
    /// `DrawPolygons()` -> the last draw's result.
    DrawPolygons = 4,
    /// `DrawTextA()` -> the base's result.
    DrawText = 5,
    /// `SetBitmap(a)`: the old one released, `a` referenced (the original's: never NULL), the control dirty.
    SetBitmap = 6,
}

/// UIMovie's inward calls.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeUiMovieCall {
    /// The constructor's part after `UIControl`'s: the movie `a`, referenced.
    Init = 1,
    /// The destructor: the movie stopped and released.
    Release = 2,
    /// `CopyData()` -> the base's result; the control stays dirty while the movie plays.
    CopyData = 3,
    /// `DrawPolygons()` -> the base's result.
    DrawPolygons = 4,
    /// `DrawTextA()`: the base's, then the movie's frame -> the render's result.
    DrawText = 5,
}

fn bitmap(p: *mut EeUiBitmapFields) -> (Host, *mut c_void, *mut c_void) {
    // SAFETY: a live record (the callers' contract); the copy ends before any call out
    let r = unsafe { &*p };
    (Host { obj: p.cast(), f: r.mBitmapHost }, r.mBitmap, r.mCopiedBitmap)
}

/// One of UIBitmap's methods on its record.
///
/// # Safety
/// `p` is a live record (a C++ `UIBitmap`'s base; for `Init`, its storage); the host's objects are live.
pub unsafe fn bitmap_call(p: *mut EeUiBitmapFields, host: EeUiHostFn, call: EeUiBitmapCall, a: isize) -> isize {
    type C = EeUiBitmapCall;
    if call == C::Init {
        // SAFETY: the contract (the record's storage)
        unsafe {
            (*p).mBitmapHost = host;
            (*p).mLocation = [0.0, 0.0];
            (*p).mBitmap = a as *mut c_void;
            (*p).mCopiedBitmap = core::ptr::null_mut();
        }
        if a != 0 {
            Host { obj: p.cast(), f: host }.op(Op::BitmapReference, a, 0, 0, 0);
        }
        return 0;
    }
    let (h, bmp, copied) = bitmap(p);
    match call {
        C::Init => 0,
        C::Release => {
            if !bmp.is_null() {
                h.op(Op::BitmapRelease, bmp as isize, 0, 0, 0);
            }
            if !copied.is_null() {
                h.op(Op::BitmapRelease, copied as isize, 0, 0, 0);
            }
            0
        }
        C::CopyData => {
            let r = h.op(Op::BaseCopyData, 0, 0, 0, 0);
            let copied = bitmap(p).2;
            if !copied.is_null() {
                h.op(Op::BitmapRelease, copied as isize, 0, 0, 0);
            }
            let bmp = bitmap(p).1;
            let next = if !bmp.is_null() {
                let rect = h.rect();
                // SAFETY: the contract; no call out holds the record
                unsafe { (*p).mLocation = [rect.left, rect.top] };
                bmp
            } else {
                core::ptr::null_mut()
            };
            // SAFETY: as above
            unsafe { (*p).mCopiedBitmap = next };
            if !next.is_null() {
                h.op(Op::BitmapReference, next as isize, 0, 0, 0);
            }
            r
        }
        C::DrawPolygons => {
            let mut r = h.op(Op::BaseDrawPolygons, 0, 0, 0, 0);
            let copied = bitmap(p).2;
            if !copied.is_null() {
                // SAFETY: the contract
                let loc = unsafe { (*p).mLocation };
                h.op_f(Op::BitmapSetLocation, copied as isize, 0, 0, 0, &mut [loc[0], loc[1], 0.0, 0.0]);
                r = h.op(Op::BitmapDraw, copied as isize, 0, 0, 0);
            }
            r
        }
        C::DrawText => h.op(Op::BaseDrawText, 0, 0, 0, 0),
        C::SetBitmap => {
            if !bmp.is_null() {
                h.op(Op::BitmapRelease, bmp as isize, 0, 0, 0);
            }
            // SAFETY: the contract
            unsafe { (*p).mBitmap = a as *mut c_void };
            h.op(Op::BitmapReference, a, 0, 0, 0);
            h.op(Op::SetDirty, 1, 0, 0, 0);
            0
        }
    }
}

/// One of UIMovie's methods on its record.
///
/// # Safety
/// `p` is a live record (a C++ `UIMovie`'s base; for `Init`, its storage); the host's objects are live.
pub unsafe fn movie_call(p: *mut EeUiMovieFields, host: EeUiHostFn, call: EeUiMovieCall, a: isize) -> isize {
    type C = EeUiMovieCall;
    if call == C::Init {
        // SAFETY: the contract (the record's storage)
        unsafe {
            (*p).mMovieHost = host;
            (*p).mMovie = a as *mut c_void;
        }
        Host { obj: p.cast(), f: host }.op(Op::MovieReference, a, 0, 0, 0);
        return 0;
    }
    // SAFETY: the contract; the copies end before any call out
    let (h, movie) = unsafe { (Host { obj: p.cast(), f: (*p).mMovieHost }, (*p).mMovie as isize) };
    match call {
        C::Init => 0,
        C::Release => {
            h.op(Op::MovieStop, movie, 0, 0, 0);
            h.op(Op::MovieRelease, movie, 0, 0, 0);
            // SAFETY: the contract
            unsafe { (*p).mMovie = core::ptr::null_mut() };
            0
        }
        C::CopyData => {
            let r = h.op(Op::BaseCopyData, 0, 0, 0, 0);
            if h.op(Op::MoviePlaying, movie, 0, 0, 0) != 0 {
                h.op(Op::SetDirty, 1, 0, 0, 0);
            }
            r
        }
        C::DrawPolygons => h.op(Op::BaseDrawPolygons, 0, 0, 0, 0),
        C::DrawText => {
            h.op(Op::BaseDrawText, 0, 0, 0, 0);
            // SAFETY: the contract (the base's draw may not change the movie, but read it afresh as the original did)
            let movie = unsafe { (*p).mMovie } as isize;
            if movie != 0 {
                h.op(Op::MovieRender, movie, 0, 0, 0)
            } else {
                0
            }
        }
    }
}

// ---- the C ABI

/// One of UIBitmap's methods ([`EeUiBitmapCall`]); `host` is read by `Init` only.
///
/// # Safety
/// As [`bitmap_call`].
#[no_mangle]
pub unsafe extern "C" fn ee_ui_bitmap_call(
    rec: *mut EeUiBitmapFields,
    host: EeUiHostFn,
    call: EeUiBitmapCall,
    a: isize,
) -> isize {
    // SAFETY: the contract
    unsafe { bitmap_call(rec, host, call, a) }
}

/// One of UIMovie's methods ([`EeUiMovieCall`]); `host` is read by `Init` only.
///
/// # Safety
/// As [`movie_call`].
#[no_mangle]
pub unsafe extern "C" fn ee_ui_movie_call(
    rec: *mut EeUiMovieFields,
    host: EeUiHostFn,
    call: EeUiMovieCall,
    a: isize,
) -> isize {
    // SAFETY: the contract
    unsafe { movie_call(rec, host, call, a) }
}

#[cfg(test)]
#[path = "mediaobj_tests.rs"]
mod tests;
