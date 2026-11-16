//! PlatCursor (WONLobbyPlatform.h) as a whole Rust object (R6 WON round 41): a pointer shape, one of the
//! system's or pixels (0xAARRGGBB) made into a platform cursor on first use, and the cursor last made the
//! pointer's shape (a destroyed cursor that is still the shape restores the default first).
//!
//! The C++ class keeps the type (a Cursor: the standard cursors and the components hold it by reference count)
//! and forwards here. The platform calls are platform/PlatWindow.h's, handed in by the shell (`EeWonPlatCursorFns`)
//! and made on the window system's thread as PlatGUIWindow::PlatformSetCursor makes them.

use std::ffi::c_void;
use std::sync::atomic::{AtomicPtr, Ordering};

/// `PLAT_CURSOR_HAND` (platform/PlatWindow.h): what a cursor whose pixels could not be made shows.
const PLAT_CURSOR_HAND: u32 = 1;

/// The platform's cursor calls (platform/PlatWindow.h), as the C++ shell hands them: PlatCursorSetSystem,
/// PlatCursorCreate, PlatCursorSet, PlatCursorDestroy.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct EeWonPlatCursorFns {
    pub set_system: extern "C" fn(cursor: u32),
    pub create: extern "C" fn(bgra: *const u32, width: i32, height: i32, hot_x: i32, hot_y: i32) -> *mut c_void,
    pub set: extern "C" fn(cursor: *mut c_void),
    pub destroy: extern "C" fn(cursor: *mut c_void),
}

/// One pointer shape.
pub struct EeWonPlatCursor {
    fns: EeWonPlatCursorFns,
    system: Option<u32>,
    pixels: Vec<u32>,
    size: [i32; 4],
    handle: *mut c_void,
    failed: bool,
}

/// The cursor last made the pointer's shape.
static APPLIED: AtomicPtr<EeWonPlatCursor> = AtomicPtr::new(std::ptr::null_mut());

impl EeWonPlatCursor {
    /// `PlatCursor::Apply`: makes it the pointer's shape (its platform cursor made on first use; a system cursor,
    /// or the hand when the pixels could not be made, otherwise).
    pub fn apply(&mut self) {
        if self.system.is_none() && self.handle.is_null() && !self.failed {
            let [w, h, x, y] = self.size;
            self.handle = (self.fns.create)(self.pixels.as_ptr(), w, h, x, y);
            self.failed = self.handle.is_null();
        }
        if !self.handle.is_null() {
            (self.fns.set)(self.handle);
        } else {
            (self.fns.set_system)(self.system.unwrap_or(PLAT_CURSOR_HAND));
        }
        APPLIED.store(self, Ordering::Relaxed);
    }
}

impl Drop for EeWonPlatCursor {
    fn drop(&mut self) {
        let me: *mut EeWonPlatCursor = self;
        if APPLIED.compare_exchange(me, std::ptr::null_mut(), Ordering::Relaxed, Ordering::Relaxed).is_ok() {
            (self.fns.set)(std::ptr::null_mut());
        }
        if !self.handle.is_null() {
            (self.fns.destroy)(self.handle);
        }
    }
}

/// A system cursor (`PlatSystemCursor`).
#[no_mangle]
pub extern "C" fn ee_won_plat_cursor_system(fns: &EeWonPlatCursorFns, cursor: u32) -> *mut EeWonPlatCursor {
    Box::into_raw(Box::new(EeWonPlatCursor {
        fns: *fns,
        system: Some(cursor),
        pixels: Vec::new(),
        size: [0; 4],
        handle: std::ptr::null_mut(),
        failed: false,
    }))
}

/// A cursor of `width` x `height` pixels (copied) with its hot spot.
///
/// # Safety
/// `pixels` holds `width * height` values (or is NULL with a size of 0).
#[no_mangle]
pub unsafe extern "C" fn ee_won_plat_cursor_pixels(
    fns: &EeWonPlatCursorFns,
    pixels: *const u32,
    width: i32,
    height: i32,
    hot_x: i32,
    hot_y: i32,
) -> *mut EeWonPlatCursor {
    let n = (width.max(0) as usize) * (height.max(0) as usize);
    // SAFETY: the contract
    let px =
        if n == 0 || pixels.is_null() { Vec::new() } else { unsafe { std::slice::from_raw_parts(pixels, n) }.to_vec() };
    Box::into_raw(Box::new(EeWonPlatCursor {
        fns: *fns,
        system: None,
        pixels: px,
        size: [width, height, hot_x, hot_y],
        handle: std::ptr::null_mut(),
        failed: false,
    }))
}

/// # Safety
/// `c` came from a constructor above, freed once.
#[no_mangle]
pub unsafe extern "C" fn ee_won_plat_cursor_free(c: *mut EeWonPlatCursor) {
    if !c.is_null() {
        // SAFETY: the contract
        drop(unsafe { Box::from_raw(c) });
    }
}

/// `PlatCursor::Apply`.
#[no_mangle]
pub extern "C" fn ee_won_plat_cursor_apply(c: &mut EeWonPlatCursor) {
    c.apply();
}

#[cfg(test)]
mod tests {
    use super::*;

    mod plat {
        use std::cell::RefCell;
        use std::ffi::c_void;
        thread_local! {
            pub static LOG: RefCell<Vec<String>> = const { RefCell::new(Vec::new()) };
            /// When set, PlatCursorCreate fails.
            pub static FAIL: RefCell<bool> = const { RefCell::new(false) };
        }
        fn log(s: String) {
            LOG.with(|l| l.borrow_mut().push(s));
        }
        pub extern "C" fn set_system(c: u32) {
            log(format!("system {c}"));
        }
        pub extern "C" fn create(_px: *const u32, w: i32, h: i32, x: i32, y: i32) -> *mut c_void {
            log(format!("create {w}x{h} at {x},{y}"));
            if FAIL.with(|f| *f.borrow()) {
                std::ptr::null_mut()
            } else {
                0x1000 as *mut c_void
            }
        }
        pub extern "C" fn set(c: *mut c_void) {
            log(format!("set {:x}", c as usize));
        }
        pub extern "C" fn destroy(c: *mut c_void) {
            log(format!("destroy {:x}", c as usize));
        }
    }
    const FNS: EeWonPlatCursorFns = EeWonPlatCursorFns {
        set_system: plat::set_system,
        create: plat::create,
        set: plat::set,
        destroy: plat::destroy,
    };

    fn take() -> Vec<String> {
        plat::LOG.with(|l| std::mem::take(&mut *l.borrow_mut()))
    }

    // one test: the applied cursor is a process-wide static
    #[test]
    fn cursors_make_apply_and_go() {
        a_pixel_cursor_is_made_once_and_destroyed();
        system_cursors_and_failed_pixels();
    }

    fn a_pixel_cursor_is_made_once_and_destroyed() {
        take();
        let px = [0xff00_00ffu32; 6];
        // SAFETY: six pixels
        let c = unsafe { ee_won_plat_cursor_pixels(&FNS, px.as_ptr(), 3, 2, 1, 0) };
        // SAFETY: the box above
        let r = unsafe { &mut *c };
        r.apply();
        r.apply();
        assert_eq!(take(), ["create 3x2 at 1,0", "set 1000", "set 1000"]);
        // the shape it still is goes back to the default before the cursor goes
        // SAFETY: freed once
        unsafe { ee_won_plat_cursor_free(c) };
        assert_eq!(take(), ["set 0", "destroy 1000"]);
    }

    fn system_cursors_and_failed_pixels() {
        take();
        let a = ee_won_plat_cursor_system(&FNS, 2);
        // SAFETY: the box above
        unsafe { &mut *a }.apply();
        assert_eq!(take(), ["system 2"]);
        plat::FAIL.with(|f| *f.borrow_mut() = true);
        let px = [1u32; 4];
        // SAFETY: four pixels
        let b = unsafe { ee_won_plat_cursor_pixels(&FNS, px.as_ptr(), 2, 2, 0, 0) };
        // SAFETY: the box above
        let rb = unsafe { &mut *b };
        rb.apply();
        // a failed create is not tried again: the hand shows
        rb.apply();
        plat::FAIL.with(|f| *f.borrow_mut() = false);
        assert_eq!(take(), ["create 2x2 at 0,0", "system 1", "system 1"]);
        // a cursor that is not the shape any more leaves the shape alone when it goes
        // SAFETY: freed once each
        unsafe { ee_won_plat_cursor_free(a) };
        assert_eq!(take(), Vec::<String>::new());
        unsafe { ee_won_plat_cursor_free(b) };
        assert_eq!(take(), ["set 0"]);
    }
}
