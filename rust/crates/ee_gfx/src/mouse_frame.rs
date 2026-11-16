//! The game pointer's frame (`src/RTS Engine/Graphics/RTSMouse.cpp`, `src/Empire Earth/Graphics Thread/EEMouse.cpp`):
//! the pointer pictures (the engine's ten and the game's fourteen, `GetPointerSurface`'s numbering), the two
//! save-under records and their swap, `BeginFrame` (the frame's pointer and position, the shift mode's overlay
//! composed in the spare slot 9), `SaveUnder`, `Draw`, `RestoreUnder`, `EndFrame` and both classes' `Release`, as a
//! whole Rust object. The C++ classes keep the members other units set (the position, bounds, pointer and overlay
//! numbers, the shift mode, the freeze), the locks, the input and the loading of the files (`LoadPointer`, which
//! fills this object's slots in place); the surfaces are opaque `GESurface*` reached through [`EeGfxMouseSurfaces`].
//!
//! The pointer is drawn by two threads (the graphics thread's frame and the pointer thread), each frame under the
//! render lock the C++ takes in `BeginFrame` and leaves in `EndFrame`.

use std::ffi::c_void;

use ee_ge::types::{Ge2DPoint, GeRectangle};
use ee_ge::um::floor;

/// `RTSMouse`'s ten pointers and `EEMouse`'s fourteen.
pub const EE_GFX_MOUSE_POINTERS: usize = 24;
/// The engine's pointers (0..10; slot 9, a blank pointer, is the shift mode's composition).
const ENGINE_POINTERS: usize = 10;
const SHIFT_SLOT: usize = 9;

/// A save-under record (`RTSMouseSaveUnder`): the screen rectangle saved and the 32x32 surface holding it.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeGfxMouseSaveUnder {
    pub rect: GeRectangle,
    pub surface: *mut c_void,
}

/// The surface calls the frame makes (`GESurface`).
#[repr(C)]
pub struct EeGfxMouseSurfaces {
    pub width: extern "C" fn(surface: *mut c_void) -> u32,
    pub height: extern "C" fn(surface: *mut c_void) -> u32,
    /// `from->CopyTo(to, source, destination, transparent)`
    pub copy: extern "C" fn(
        from: *mut c_void,
        to: *mut c_void,
        source: *const GeRectangle,
        destination: *const GeRectangle,
        transparent: bool,
    ) -> i32,
    pub reference: extern "C" fn(surface: *mut c_void),
    pub dereference: extern "C" fn(surface: *mut c_void),
}

/// The surface calls (the tests' log).
pub trait Surfaces {
    fn width(&mut self, s: *mut c_void) -> u32;
    fn height(&mut self, s: *mut c_void) -> u32;
    fn copy(
        &mut self,
        from: *mut c_void,
        to: *mut c_void,
        source: &GeRectangle,
        destination: &GeRectangle,
        transparent: bool,
    ) -> i32;
    fn reference(&mut self, s: *mut c_void);
    fn dereference(&mut self, s: *mut c_void);
}

impl Surfaces for &EeGfxMouseSurfaces {
    fn width(&mut self, s: *mut c_void) -> u32 {
        (self.width)(s)
    }
    fn height(&mut self, s: *mut c_void) -> u32 {
        (self.height)(s)
    }
    fn copy(
        &mut self,
        from: *mut c_void,
        to: *mut c_void,
        source: &GeRectangle,
        destination: &GeRectangle,
        transparent: bool,
    ) -> i32 {
        (self.copy)(from, to, source, destination, transparent)
    }
    fn reference(&mut self, s: *mut c_void) {
        (self.reference)(s)
    }
    fn dereference(&mut self, s: *mut c_void) {
        (self.dereference)(s)
    }
}

/// The pointer's frame state (`RTSMouse` +0x0c, +0x24..+0x84, +0xa1, +0xa3 and `EEMouse` +0xa8).
#[derive(Debug)]
pub struct EeGfxMouseFrame {
    /// `mPointerSurfaces` (0..10), then `mExtraPointerSurfaces` (10..24)
    pub pointers: [*mut c_void; EE_GFX_MOUSE_POINTERS],
    /// `mSaveUnder24`, `mSaveUnder38`
    pub saves: [EeGfxMouseSaveUnder; 2],
    /// `mCurrentSaveUnder`, `mPreviousSaveUnder` (indices into `saves`; the windowed restore makes them equal)
    pub current: usize,
    pub previous: usize,
    /// `mCurrentSurface`: the frame's pointer, referenced
    pub surface: *mut c_void,
    /// `mDrawnPosition`
    pub drawn: Ge2DPoint,
    pub in_frame: bool,
    pub just_loaded: bool,
}

fn rect(left: f32, top: f32, right: f32, bottom: f32) -> GeRectangle {
    GeRectangle { bottom, left, right, top }
}

/// `inRect` shifted by `origin`.
fn shifted(r: &GeRectangle, origin: Ge2DPoint) -> GeRectangle {
    rect(r.left + origin.x, r.top + origin.y, r.right + origin.x, r.bottom + origin.y)
}

/// A rectangle's size at the origin (a save-under surface's part).
fn sized(r: &GeRectangle) -> GeRectangle {
    rect(0.0, 0.0, r.right - r.left, r.bottom - r.top)
}

impl Default for EeGfxMouseFrame {
    fn default() -> Self {
        Self::new()
    }
}

impl EeGfxMouseFrame {
    /// The constructors': no surfaces, the records zeroed, the first current.
    pub fn new() -> Self {
        let none = EeGfxMouseSaveUnder { rect: GeRectangle::default(), surface: std::ptr::null_mut() };
        EeGfxMouseFrame {
            pointers: [std::ptr::null_mut(); EE_GFX_MOUSE_POINTERS],
            saves: [none; 2],
            current: 0,
            previous: 1,
            surface: std::ptr::null_mut(),
            drawn: Ge2DPoint::default(),
            in_frame: false,
            just_loaded: true,
        }
    }

    /// `GetPointerSurface` (EEMouse's): 0..24, else null.
    pub fn pointer(&self, index: u32) -> *mut c_void {
        self.pointers.get(index as usize).copied().unwrap_or(std::ptr::null_mut())
    }

    /// `BeginFrame` after the render lock: the position and pointer taken (referenced), in shift mode the pointer
    /// copied into slot 9 and the overlay drawn over it transparently (slot 9 the frame's pointer when both copies
    /// succeed), the records swapped.
    pub fn begin<S: Surfaces>(&mut self, s: &mut S, position: Ge2DPoint, pointer: u32, overlay: u32, shift_mode: bool) {
        self.in_frame = true;
        self.drawn = position;
        self.surface = self.pointer(pointer);
        s.reference(self.surface);
        if shift_mode {
            let plus = self.pointers[SHIFT_SLOT];
            let picture = self.pointer(pointer);
            let over = self.pointer(overlay);
            let r = rect(0.0, 0.0, s.width(self.surface) as f32, s.height(self.surface) as f32);
            if s.copy(picture, plus, &r, &r, false) == 0 && s.copy(over, plus, &r, &r, true) == 0 {
                s.dereference(self.surface);
                self.surface = plus;
                s.reference(plus);
            }
        }
        std::mem::swap(&mut self.current, &mut self.previous);
    }

    /// `SaveUnder`: the screen under the drawn position (clipped to the surface), shifted by `origin`, into the
    /// current record.
    pub fn save_under<S: Surfaces>(&mut self, s: &mut S, target: *mut c_void, origin: Ge2DPoint) {
        let x = floor(self.drawn.x) as i64;
        let y = floor(self.drawn.y) as i64;
        let mut width = s.width(self.surface) as i64;
        let mut height = s.height(self.surface) as i64;
        let target_width = s.width(target) as i64;
        if x + width > target_width {
            width = target_width - x;
        }
        let target_height = s.height(target) as i64;
        if y + height > target_height {
            height = target_height - y;
        }
        let destination = rect(0.0, 0.0, width as f32, height as f32);
        let top = origin.y + y as f32;
        let left = origin.x + x as f32;
        let source = rect(left, top, left + width as f32, top + height as f32);
        let record = &mut self.saves[self.current];
        record.rect = rect(x as f32, y as f32, (x + width) as f32, (y + height) as f32);
        let to = record.surface;
        s.copy(target, to, &source, &destination, false);
    }

    /// `RestoreUnder`: the previous record (windowed: the current one, which becomes the previous, unshifted) copied
    /// back; the first call after a load only clears the flag (0).
    pub fn restore_under<S: Surfaces>(
        &mut self,
        s: &mut S,
        target: *mut c_void,
        origin: Ge2DPoint,
        windowed: bool,
    ) -> i32 {
        if self.just_loaded {
            self.just_loaded = false;
            return 0;
        }
        let destination = if windowed {
            self.previous = self.current;
            self.saves[self.previous].rect
        } else {
            shifted(&self.saves[self.previous].rect, origin)
        };
        let source = sized(&destination);
        s.copy(self.saves[self.previous].surface, target, &source, &destination, false)
    }

    /// `Draw`: the frame's pointer over the current record's rectangle, shifted by `origin`, transparently.
    pub fn draw<S: Surfaces>(&mut self, s: &mut S, target: *mut c_void, origin: Ge2DPoint) {
        let destination = shifted(&self.saves[self.current].rect, origin);
        let source = sized(&destination);
        s.copy(self.surface, target, &source, &destination, true);
    }

    /// `EndFrame` before the render lock is left: the frame's pointer released.
    pub fn end<S: Surfaces>(&mut self, s: &mut S) {
        s.dereference(self.surface);
        self.surface = std::ptr::null_mut();
        self.in_frame = false;
    }

    /// `EEMouse::Release`'s own part: the game's fourteen pointers.
    pub fn release_extra<S: Surfaces>(&mut self, s: &mut S) {
        for p in self.pointers[ENGINE_POINTERS..].iter_mut() {
            if !p.is_null() {
                s.dereference(*p);
                *p = std::ptr::null_mut();
            }
        }
    }

    /// `RTSMouse::Release`: the engine's ten pointers, both save-under surfaces and the frame's pointer.
    pub fn release<S: Surfaces>(&mut self, s: &mut S) {
        let saves = self.saves.iter_mut().map(|r| &mut r.surface);
        for p in self.pointers[..ENGINE_POINTERS].iter_mut().chain(saves).chain(std::iter::once(&mut self.surface)) {
            if !p.is_null() {
                s.dereference(*p);
                *p = std::ptr::null_mut();
            }
        }
    }
}

/// A new frame (the constructors').
#[no_mangle]
pub extern "C" fn ee_gfx_mouse_frame_new() -> *mut EeGfxMouseFrame {
    Box::into_raw(Box::new(EeGfxMouseFrame::new()))
}

/// The frame freed (the destructor's; its surfaces are released by `Release`).
///
/// # Safety
/// `frame` came from [`ee_gfx_mouse_frame_new`] (or is null) and is not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_mouse_frame_free(frame: *mut EeGfxMouseFrame) {
    if !frame.is_null() {
        // SAFETY: the function's contract
        drop(unsafe { Box::from_raw(frame) });
    }
}

/// Pointer slot `index` (0..24), for `LoadPointer` to fill in place; save-under record `24 + k`'s surface slot.
///
/// # Safety
/// `frame` is valid; the slot lives as long as the frame.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_mouse_frame_slot(frame: *mut EeGfxMouseFrame, index: u32) -> *mut *mut c_void {
    // SAFETY: the function's contract
    let f = unsafe { &mut *frame };
    let i = index as usize;
    if i < EE_GFX_MOUSE_POINTERS {
        &mut f.pointers[i]
    } else {
        &mut f.saves[i - EE_GFX_MOUSE_POINTERS].surface
    }
}

/// `GetPointerSurface`.
///
/// # Safety
/// `frame` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_mouse_frame_pointer(frame: *const EeGfxMouseFrame, index: u32) -> *mut c_void {
    // SAFETY: the function's contract
    unsafe { (*frame).pointer(index) }
}

/// The frame's calls (`op`): 0 `BeginFrame` (`position`, `pointer`, `overlay`, `shift`), 1 `SaveUnder`, 2 `Draw`,
/// 3 `RestoreUnder` (`shift`: windowed; its copy's result), 4 `EndFrame`, 5 `EEMouse::Release`'s own, 6
/// `RTSMouse::Release`, 7 a load done (the next restore skipped). `target` and `origin` for 1..3.
///
/// # Safety
/// `frame` and `surfaces` are valid; `origin` is valid for 0..3 (`position` for 0); the surfaces are live.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_gfx_mouse_frame_call(
    frame: *mut EeGfxMouseFrame,
    surfaces: *const EeGfxMouseSurfaces,
    op: u32,
    target: *mut c_void,
    origin: *const Ge2DPoint,
    pointer: u32,
    overlay: u32,
    shift: bool,
) -> i32 {
    // SAFETY: the function's contract; the frame's state is Rust's own, the callbacks touch only surfaces
    unsafe {
        let f = &mut *frame;
        let mut s = &*surfaces;
        match op {
            0 => f.begin(&mut s, *origin, pointer, overlay, shift),
            1 => f.save_under(&mut s, target, *origin),
            2 => f.draw(&mut s, target, *origin),
            3 => return f.restore_under(&mut s, target, *origin, shift),
            4 => f.end(&mut s),
            5 => f.release_extra(&mut s),
            6 => f.release(&mut s),
            _ => f.just_loaded = true,
        }
        0
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    /// surfaces are 1-based ids; their sizes by id
    #[derive(Default)]
    struct Log {
        sizes: Vec<(u32, u32)>,
        fail: bool,
        log: Vec<String>,
    }

    fn r(g: &GeRectangle) -> (f32, f32, f32, f32) {
        (g.left, g.top, g.right, g.bottom)
    }

    impl Surfaces for Log {
        fn width(&mut self, s: *mut c_void) -> u32 {
            self.sizes[s as usize - 1].0
        }
        fn height(&mut self, s: *mut c_void) -> u32 {
            self.sizes[s as usize - 1].1
        }
        fn copy(
            &mut self,
            from: *mut c_void,
            to: *mut c_void,
            source: &GeRectangle,
            destination: &GeRectangle,
            t: bool,
        ) -> i32 {
            self.log.push(format!("copy {} {} {:?} {:?} {t}", from as usize, to as usize, r(source), r(destination)));
            if self.fail {
                5
            } else {
                0
            }
        }
        fn reference(&mut self, s: *mut c_void) {
            self.log.push(format!("ref {}", s as usize));
        }
        fn dereference(&mut self, s: *mut c_void) {
            self.log.push(format!("deref {}", s as usize));
        }
    }

    fn id(n: usize) -> *mut c_void {
        n as *mut c_void
    }

    /// pointers 1..=24 in slots 0..24, save-unders 25 and 26, the screen 27 (100 x 80)
    fn loaded() -> (EeGfxMouseFrame, Log) {
        let mut f = EeGfxMouseFrame::new();
        for i in 0..EE_GFX_MOUSE_POINTERS {
            f.pointers[i] = id(i + 1);
        }
        f.saves[0].surface = id(25);
        f.saves[1].surface = id(26);
        let mut sizes = vec![(32, 32); 26];
        sizes.push((100, 80));
        (f, Log { sizes, ..Default::default() })
    }

    #[test]
    fn a_frame_saves_draws_and_restores_with_the_records_swapped() {
        let (mut f, mut s) = loaded();
        let o = Ge2DPoint { x: 5.0, y: 7.0 };
        f.begin(&mut s, Ge2DPoint { x: 80.5, y: -0.5 }, 2, 8, false);
        assert_eq!((f.current, f.previous, f.surface as usize), (1, 0, 3));
        f.save_under(&mut s, id(27), o);
        // clipped on the right (100 - 80), y floored to -1
        assert_eq!(r(&f.saves[1].rect), (80.0, -1.0, 100.0, 31.0));
        f.draw(&mut s, id(27), o);
        // the first restore after a load only clears the flag
        assert_eq!(f.restore_under(&mut s, id(27), o, false), 0);
        f.end(&mut s);
        assert_eq!(
            s.log,
            [
                "ref 3",
                "copy 27 26 (85.0, 6.0, 105.0, 38.0) (0.0, 0.0, 20.0, 32.0) false",
                "copy 3 27 (0.0, 0.0, 20.0, 32.0) (85.0, 6.0, 105.0, 38.0) true",
                "deref 3",
            ]
        );
        s.log.clear();
        // the next frame: the other record; a full-screen restore puts back the previous one, shifted
        f.begin(&mut s, Ge2DPoint { x: 1.0, y: 2.0 }, 0, 8, false);
        f.save_under(&mut s, id(27), o);
        f.restore_under(&mut s, id(27), o, false);
        assert_eq!(s.log[2], "copy 26 27 (0.0, 0.0, 20.0, 32.0) (85.0, 6.0, 105.0, 38.0) false");
        // a windowed restore puts back the current one unshifted and makes it the previous too
        f.restore_under(&mut s, id(27), o, true);
        assert_eq!(s.log[3], "copy 25 27 (0.0, 0.0, 32.0, 32.0) (1.0, 2.0, 33.0, 34.0) false");
        assert_eq!((f.current, f.previous), (0, 0));
    }

    #[test]
    fn shift_mode_composes_the_overlay_in_slot_nine() {
        let (mut f, mut s) = loaded();
        f.begin(&mut s, Ge2DPoint::default(), 4, 8, true);
        assert_eq!(f.surface as usize, 10);
        assert_eq!(
            s.log,
            [
                "ref 5",
                "copy 5 10 (0.0, 0.0, 32.0, 32.0) (0.0, 0.0, 32.0, 32.0) false",
                "copy 9 10 (0.0, 0.0, 32.0, 32.0) (0.0, 0.0, 32.0, 32.0) true",
                "deref 5",
                "ref 10",
            ]
        );
        let (mut f, mut s) = loaded();
        s.fail = true;
        f.begin(&mut s, Ge2DPoint::default(), 4, 8, true);
        // the first copy failed: the pointer itself, the overlay not tried
        assert_eq!(f.surface as usize, 5);
        assert_eq!(s.log.len(), 2);
        assert!(f.pointer(24).is_null() && f.pointer(23) as usize == 24);
    }

    #[test]
    fn release_drops_the_engines_then_the_games() {
        let (mut f, mut s) = loaded();
        f.surface = id(3);
        f.release_extra(&mut s);
        f.release(&mut s);
        let expected: Vec<String> = (11..=24).chain(1..=10).chain([25, 26, 3]).map(|n| format!("deref {n}")).collect();
        assert_eq!(s.log, expected);
        assert!(f.pointers.iter().all(|p| p.is_null()) && f.surface.is_null());
        s.log.clear();
        f.release(&mut s);
        assert!(s.log.is_empty());
    }
}
