//! `RTSMouse`' bodies (LLE round 8, from `src/RTS Engine/Graphics/RTSMouse.cpp`): its construction and teardown,
//! the pointer pictures' loads (the texture pack's first), the save-under surfaces, the frame's start and end, and
//! `Update`'s axes.
//!
//! The class stays C++ (`ee/RTSMouse.h`: EEMouse derives from it); its plain members are the record
//! [`EeRtsMouseFields`], a base of the class; the position and the bounds stay C++ value classes (their storage is
//! `ee_base::math`'s mirrors). The frame (the drawn position, the save-under records, the pointer slots) was Rust
//! already (ee_render gfx mouse_frame.rs); the move's arithmetic too (gfx mouse.rs). Every other C++ call (the
//! screen, the surfaces, the file system, the input devices) is a one-statement shim ([`MouseCpp`]); the surface
//! reads are `RTSGraphics.cpp`'s (graphics.rs).

use core::ffi::{c_char, c_long, c_ulong, c_void};
use core::ptr::{addr_of, read_volatile};
use core::sync::atomic::{AtomicI32, Ordering};

use ee_base::math::types::{Ge2DPoint, GeRectangle};
use ee_base::shim::ShimOut;

/// `RTSMouse`' plain members (`ee/RTSMouse.h`), in the class's order: a base of the class.
#[repr(C)]
#[allow(non_snake_case)]
pub struct EeRtsMouseFields {
    pub mSensitivity: f32,
    /// the desktop's height (unsigned: Update divides by it)
    pub mDesktopHeight: c_ulong,
    pub mDesktopWidth: c_ulong,
    pub mWheel: c_long,
    /// the pointer picture (volatile)
    pub mPointer: c_long,
    /// the overlay picture (= 8; volatile)
    pub mOverlay: c_long,
    /// the local player's shift mode (volatile)
    pub mShiftMode: bool,
    pub mBA2: bool,
    pub mIsFrozen: bool,
}

/// `GEVideoMode`.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct EeRtsVideoMode {
    pub width: c_ulong,
    pub height: c_ulong,
    pub refresh_rate: c_ulong,
    pub bit_count: c_ulong,
    pub pixel_format: c_ulong,
}

ee_base::cpp_shims! {
    trait MouseCpp;
    fn fields(mouse: usize) -> *mut EeRtsMouseFields = ee_rtsmcpp_fields;
    fn render_lock_slot(mouse: usize) -> *mut *mut c_void = ee_rtsmcpp_render_lock_slot;
    fn input_lock_slot(mouse: usize) -> *mut *mut c_void = ee_rtsmcpp_input_lock_slot;
    fn position(mouse: usize) -> *mut Ge2DPoint = ee_rtsmcpp_position;
    fn bounds(mouse: usize) -> *mut GeRectangle = ee_rtsmcpp_bounds;
    fn frame_call(mouse: usize, op: u32, surface: usize, origin: *const Ge2DPoint, flag: bool) -> c_long = ee_rtsmcpp_frame_call;
    fn slot(mouse: usize, index: c_ulong) -> *mut *mut c_void = ee_rtsmcpp_slot;
    fn display_size(width: ShimOut<i32>, height: ShimOut<i32>) = ee_rtsmcpp_display_size;
    fn debug_out_long(format: *const c_char, value: c_long) = ee_rtsmcpp_debug_out_long;
    fn screen_mode(screen: usize, out: *mut EeRtsVideoMode) = ee_rtsmcpp_screen_mode;
    fn screen_create_surface(screen: usize, description: *const c_void, slot: *mut *mut c_void) -> c_long = ee_rtsmcpp_screen_create_surface;
    fn pack_shift() -> c_long = ee_rtsmcpp_pack_shift;
    fn pack_drop() -> c_long = ee_rtsmcpp_pack_drop;
    fn text_new() -> usize = ee_rtsmcpp_text_new;
    fn text_delete(text: usize) = ee_rtsmcpp_text_delete;
    fn text_chars(text: usize) -> *const c_char = ee_rtsmcpp_text_chars;
    fn pack_name(file: *const c_char, out: usize) -> bool = ee_rtsmcpp_pack_name;
    fn find_text(screen: usize, name: usize, out: ShimOut<usize>) -> c_long = ee_rtsmcpp_find_text;
    fn find_file(screen: usize, name: *const c_char, out: ShimOut<usize>) -> c_long = ee_rtsmcpp_find_file;
    fn file_open_read(file: usize) -> c_long = ee_rtsmcpp_file_open_read;
    fn file_png_attributes(file: usize, width: ShimOut<c_ulong>, height: ShimOut<c_ulong>, depth: ShimOut<c_ulong>) -> c_long = ee_rtsmcpp_file_png_attributes;
    fn file_rewind(file: usize) -> c_long = ee_rtsmcpp_file_rewind;
    fn file_close(file: usize) -> c_long = ee_rtsmcpp_file_close;
    fn file_delete(file: usize) = ee_rtsmcpp_file_delete;
    fn surf_load_png(surface: usize, file: usize) -> c_long = ee_rtsmcpp_surf_load_png;
    fn surf_logical_shift(surface: usize) -> c_ulong = ee_rtsmcpp_surf_logical_shift;
    fn surf_set_logical_shift(surface: usize, shift: c_ulong) = ee_rtsmcpp_surf_set_logical_shift;
    fn surf_set_transparency(surface: usize) = ee_rtsmcpp_surf_set_transparency;
    fn surf_width(surface: usize) -> c_ulong = ee_rtsgcpp_surf_width;
    fn surf_height(surface: usize) -> c_ulong = ee_rtsgcpp_surf_height;
    fn surf_pixel_width(surface: usize) -> c_ulong = ee_rtsgcpp_surf_pixel_width;
    fn surf_pixel_height(surface: usize) -> c_ulong = ee_rtsgcpp_surf_pixel_height;
    fn surf_dereference(surface: usize) = ee_rtsgcpp_surf_dereference;
    fn surf_copy(from: usize, to: usize, source: *mut f32, destination: *mut f32) -> c_long = ee_rtsgcpp_surf_copy;
}

const OK: c_long = 0;
const NOT_FOUND: c_long = 3;
const FAILED: c_long = 9;
/// `kSurfaceOffscreen`.
const SURFACE_OFFSCREEN: c_ulong = 1;
/// The mouse axes (`kMouseXAxis`, `kMouseYAxis`, `kMouseZAxis`).
const AXIS_X: c_ulong = 0;
const AXIS_Y: c_ulong = 1;
const AXIS_Z: c_ulong = 2;

/// The ten pointer pictures (`sPointerFiles`).
const POINTER_FILES: [&core::ffi::CStr; 10] = [
    c"Textures\\Normal Pointer.png",
    c"Textures\\Attack Pointer.png",
    c"Textures\\Invalid Target Pointer.png",
    c"Textures\\Target Too Close Pointer.png",
    c"Textures\\Gather Pointer.png",
    c"Textures\\Build Pointer.png",
    c"Textures\\Blank Pointer.png",
    c"Textures\\No LOS.png",
    c"Textures\\Plus Pointer.png",
    c"Textures\\Blank Pointer.png",
];

/// The smallest moves seen (`sMinXValue`, `sMinYValue`: 100 at the start).
static MIN_X: AtomicI32 = AtomicI32::new(100);
static MIN_Y: AtomicI32 = AtomicI32::new(100);

/// The pointer's description: `kind`, the screen mode's format and depth, 32x32.
fn description(
    mode: &EeRtsVideoMode,
    volatility: c_ulong,
    transparent: bool,
) -> ee_render::ge::texobj::EeGeSurfaceDescription {
    ee_render::ge::texobj::EeGeSurfaceDescription {
        kind: SURFACE_OFFSCREEN,
        pixel_format: mode.pixel_format,
        surface_volatility: volatility,
        width: 0x20,
        height: 0x20,
        m14: 0,
        mip_level_count: 0,
        pixel_depth: mode.bit_count,
        is_3d_renderable: false,
        is_transparency_enabled: transparent,
        no_alpha: false,
    }
}

fn texture_pack_log() -> bool {
    std::env::var_os("EE_TEXTURE_PACK_LOG").is_some_and(|v| !v.is_empty())
}

/// `RTSMouse::RTSMouse` (after its frame): the locks, the desktop's size, the defaults.
///
/// # Safety
/// `mouse` is the C++ object under construction.
pub unsafe fn construct<C: MouseCpp>(c: &mut C, mouse: usize, verify: &mut dyn FnMut(bool, c_ulong, &'static [u8])) {
    let f = c.fields(mouse);
    unsafe {
        *c.render_lock_slot(mouse) = new_section();
        verify(true, 68, b"TSScheduler::CreateCriticalSection(this->mRenderLock) == keseOK");
        *c.input_lock_slot(mouse) = new_section();
        verify(true, 69, b"TSScheduler::CreateCriticalSection(this->mInputLock) == keseOK");
    }
    let (mut width, mut height) = (0i32, 0i32);
    c.display_size(ShimOut::of(&mut width), ShimOut::of(&mut height));
    unsafe {
        (*f).mSensitivity = 1.0;
        (*f).mDesktopHeight = height as c_ulong;
        (*f).mDesktopWidth = width as c_ulong;
        (*f).mWheel = 0;
        (*f).mPointer = 0;
        (*f).mOverlay = 8;
        (*f).mIsFrozen = false;
        (*f).mBA2 = false;
        (*f).mShiftMode = false;
    }
}

#[cfg(not(test))]
fn new_section() -> *mut c_void {
    ee_base::ts::ee_ts_section_new().cast()
}
#[cfg(test)]
fn new_section() -> *mut c_void {
    0x10 as *mut c_void
}

/// `LoadPackPointer` (port): the texture pack's picture for `file` when the pack has one (its file is 32x32 << the
/// pack's scale; held at its pixel size, halved for the pack's extra MIP drop, with 32x32 as its logical size), in
/// `*slot` (the old one dereferenced); an error, and the slot untouched, otherwise.
///
/// # Safety
/// `screen` is a live screen, `slot` a pointer slot.
pub unsafe fn load_pack_pointer<C: MouseCpp>(
    c: &mut C,
    file: &core::ffi::CStr,
    screen: usize,
    base: &ee_render::ge::texobj::EeGeSurfaceDescription,
    slot: *mut *mut c_void,
) -> c_long {
    let pack = c.text_new();
    let result = unsafe { load_pack_pointer_named(c, file, screen, base, slot, pack) };
    c.text_delete(pack);
    result
}

unsafe fn load_pack_pointer_named<C: MouseCpp>(
    c: &mut C,
    file: &core::ffi::CStr,
    screen: usize,
    base: &ee_render::ge::texobj::EeGeSurfaceDescription,
    slot: *mut *mut c_void,
    pack: usize,
) -> c_long {
    let shift = c.pack_shift();
    let drop = c.pack_drop();
    let mut found = 0usize;
    if shift <= 0
        || !c.pack_name(file.as_ptr(), pack)
        || c.find_text(screen, pack, ShimOut::of(&mut found)) != OK
        || found == 0
    {
        return NOT_FOUND;
    }
    let (mut width, mut height, mut depth) = (0 as c_ulong, 0 as c_ulong, 0 as c_ulong);
    let mut result = c.file_open_read(found);
    if result == OK {
        result =
            c.file_png_attributes(found, ShimOut::of(&mut width), ShimOut::of(&mut height), ShimOut::of(&mut depth));
    }
    if result == OK {
        result = c.file_rewind(found);
    }
    let size = base.width << (shift + drop);
    if result == OK && (width != size || height != size) {
        result = FAILED; // not a 32x32 pointer at the pack's scale
    }
    let mut d = *base;
    d.width = width;
    d.height = height;
    let mut full: *mut c_void = core::ptr::null_mut();
    if result == OK {
        result = c.screen_create_surface(screen, &d as *const _ as *const c_void, &mut full);
    }
    if result == OK {
        result = c.surf_load_png(full as usize, found);
    }
    c.file_close(found);
    c.file_delete(found);
    let mut surface = full;
    if result == OK && drop > 0 {
        d.width = width >> drop;
        d.height = height >> drop;
        surface = core::ptr::null_mut();
        result = c.screen_create_surface(screen, &d as *const _ as *const c_void, &mut surface);
        if result == OK {
            // GERectangle's storage order: bottom, left, right, top
            let mut source = [height as f32, 0.0, width as f32, 0.0];
            let mut destination = [d.height as f32, 0.0, d.width as f32, 0.0];
            result = c.surf_copy(full as usize, surface as usize, source.as_mut_ptr(), destination.as_mut_ptr());
        }
        c.surf_dereference(full as usize);
    }
    if result != OK {
        if texture_pack_log() {
            let name = pack_text(c, pack);
            eprintln!("texture pack: {name} not used ({width}x{height}, error {result})");
        }
        if !surface.is_null() {
            c.surf_dereference(surface as usize);
        }
        return result;
    }
    c.surf_set_logical_shift(surface as usize, shift as c_ulong);
    c.surf_set_transparency(surface as usize);
    unsafe {
        if !(*slot).is_null() {
            c.surf_dereference(*slot as usize);
        }
        *slot = surface;
    }
    if texture_pack_log() {
        let s = surface as usize;
        let name = pack_text(c, pack);
        eprintln!(
            "texture pack: {} logical {}x{} pixels {}x{} (pointer)",
            name,
            c.surf_width(s),
            c.surf_height(s),
            c.surf_pixel_width(s),
            c.surf_pixel_height(s)
        );
    }
    OK
}

/// The C++ string's text.
fn pack_text<C: MouseCpp>(c: &mut C, text: usize) -> String {
    let p = c.text_chars(text);
    if p.is_null() {
        return String::new();
    }
    // SAFETY: the live UString's characters
    unsafe { core::ffi::CStr::from_ptr(p) }.to_string_lossy().into_owned()
}

/// `RTSMouse::LoadPointer`: a 32x32 pointer picture (PNG, the texture pack's first) from the texture path into
/// `*slot` (created if needed), blue transparent; the first error of finding, loading and closing the file.
///
/// # Safety
/// `screen` is a live screen, `slot` a pointer slot.
pub unsafe fn load_pointer<C: MouseCpp>(
    c: &mut C,
    file: &core::ffi::CStr,
    screen: usize,
    slot: *mut *mut c_void,
) -> c_long {
    let mut mode = EeRtsVideoMode::default();
    c.screen_mode(screen, &mut mode);
    let d = description(&mode, 1, true);
    if unsafe { load_pack_pointer(c, file, screen, &d, slot) } == OK {
        return OK;
    }
    unsafe {
        if !(*slot).is_null() && c.surf_logical_shift(*slot as usize) != 0 {
            // was a pack pointer: back to 32x32 pixels
            c.surf_dereference(*slot as usize);
            *slot = core::ptr::null_mut();
        }
        if (*slot).is_null() {
            c.screen_create_surface(screen, &d as *const _ as *const c_void, slot);
        }
    }
    let mut found = 0usize;
    let result = c.find_file(screen, file.as_ptr(), ShimOut::of(&mut found));
    if found == 0 {
        // a missing pointer texture fails instead of being loaded through NULL
        return if result != OK { result } else { NOT_FOUND };
    }
    let surface = unsafe { *slot } as usize;
    let mut result = c.surf_load_png(surface, found);
    let close = c.file_close(found);
    if result == OK {
        result = close;
    }
    c.file_delete(found);
    c.surf_set_transparency(surface);
    result
}

/// `RTSMouse::Load`: the ten pointer pictures and the two 32x32 save-under surfaces on a screen; the first error.
///
/// # Safety
/// `mouse` is the live object, `screen` a live screen.
pub unsafe fn load<C: MouseCpp>(
    c: &mut C,
    mouse: usize,
    screen: usize,
    load_one: &mut dyn FnMut(&mut C, &core::ffi::CStr, *mut *mut c_void) -> c_long,
    verify: &mut dyn FnMut(bool, c_ulong, &'static [u8]),
) -> c_long {
    let mut mode = EeRtsVideoMode::default();
    c.screen_mode(screen, &mut mode);
    let mut result = OK;
    let mut i = 0;
    while i < 10 && result == OK {
        let slot = c.slot(mouse, i as c_ulong);
        result = load_one(c, POINTER_FILES[i], slot);
        i += 1;
    }
    if result == OK {
        let d = description(&mode, 0, false);
        for (index, line) in [(24, 349), (25, 356)] {
            let slot = c.slot(mouse, index);
            if unsafe { (*slot).is_null() } {
                result = c.screen_create_surface(screen, &d as *const _ as *const c_void, slot);
                verify(result == OK, line, b"theResult == keseOK");
            }
        }
    }
    c.frame_call(mouse, 7, 0, core::ptr::null(), false); // the next restore only clears the flag
    result
}

/// The mouse device's entities (the Input System is Rust: ee_base input).
pub trait MouseInput {
    /// `gISMouse->GetEntity(index, entity)`'s entity (0 for none).
    fn entity(&mut self, index: c_ulong) -> usize;
    fn normalized(&mut self, entity: usize) -> c_long;
    fn value(&mut self, entity: usize) -> c_long;
}

/// `RTSMouse::Update`: the pointer moved by the raw axes (scaled by bounds over desktop, sensitivity and the
/// smallest move seen: gfx mouse.rs) unless frozen; the wheel read (0 without one).
///
/// # Safety
/// `mouse` is the live object.
pub unsafe fn update<C: MouseCpp, I: MouseInput>(c: &mut C, i: &mut I, mouse: usize) {
    let f = c.fields(mouse);
    let x = i.entity(AXIS_X);
    let y = i.entity(AXIS_Y);
    let wheel = i.entity(AXIS_Z);
    if !unsafe { (*f).mIsFrozen } {
        let position = c.position(mouse);
        let bounds = c.bounds(mouse);
        let input = unsafe {
            ee_render::gfx::mouse::EeGfxMouseInput {
                position: *position,
                bounds: *bounds,
                desktop_width: (*f).mDesktopWidth as u32,
                desktop_height: (*f).mDesktopHeight as u32,
                sensitivity: (*f).mSensitivity,
                move_x: i.normalized(x) as i32,
                move_y: i.normalized(y) as i32,
            }
        };
        let (mut min_x, mut min_y) = (MIN_X.load(Ordering::Relaxed), MIN_Y.load(Ordering::Relaxed));
        let m = ee_render::gfx::mouse::update_move(&input, &mut min_x, &mut min_y);
        MIN_X.store(min_x, Ordering::Relaxed);
        MIN_Y.store(min_y, Ordering::Relaxed);
        if m.min_x_changed {
            c.debug_out_long(c"!MinXValue now %ld\n".as_ptr(), min_x as c_long);
        }
        if m.min_y_changed {
            c.debug_out_long(c"!MinYValue now %ld\n".as_ptr(), min_y as c_long);
        }
        if m.moved {
            unsafe { *position = m.position };
        }
    }
    unsafe { (*f).mWheel = if wheel != 0 { i.value(wheel) } else { 0 } };
}

/// `RTSMouse::BeginFrame`: the render lock entered (EndFrame leaves it), then the frame started at the position
/// (in shift mode the pointer with the overlay over it).
///
/// # Safety
/// `mouse` is the live object.
pub unsafe fn begin_frame<C: MouseCpp>(c: &mut C, mouse: usize, enter: &mut dyn FnMut(*mut c_void)) {
    let f = c.fields(mouse);
    enter(unsafe { *c.render_lock_slot(mouse) });
    let position = unsafe { *c.position(mouse) };
    let shift = unsafe { read_volatile(addr_of!((*f).mShiftMode)) };
    c.frame_call(mouse, 0, 0, &position, shift);
}

/// `RTSMouse::EndFrame`: the frame's pointer released, the render lock left.
///
/// # Safety
/// `mouse` is the live object.
pub unsafe fn end_frame<C: MouseCpp>(c: &mut C, mouse: usize, leave: &mut dyn FnMut(*mut c_void)) {
    c.frame_call(mouse, 4, 0, core::ptr::null(), false);
    leave(unsafe { *c.render_lock_slot(mouse) });
}

#[cfg(not(test))]
mod ffi {
    use super::*;
    use ee_base::shim::ShimFfi as Ffi;

    fn verify(ok: bool, line: c_ulong, expression: &'static [u8]) {
        if !ok {
            let mut e = expression.to_vec();
            e.push(0);
            // SAFETY: C strings
            unsafe {
                ee_base::error::ee_error_verify(
                    c"C:\\EEX\\RTS Engine\\Graphics\\RTSMouse.cpp".as_ptr(),
                    line,
                    e.as_ptr().cast(),
                )
            };
        }
    }

    /// `RTSMouse::RTSMouse` (after `mFrame`).
    ///
    /// # Safety
    /// `mouse` is the C++ object under construction.
    #[no_mangle]
    pub unsafe extern "C" fn ee_rts_mouse_construct(mouse: *mut c_void) {
        unsafe { construct(&mut Ffi, mouse as usize, &mut verify) }
    }

    /// `RTSMouse::~RTSMouse`: the locks.
    ///
    /// # Safety
    /// `mouse` is the C++ object being destroyed.
    #[no_mangle]
    pub unsafe extern "C" fn ee_rts_mouse_destruct(mouse: *mut c_void) {
        unsafe {
            ee_base::ts::ee_ts_section_free((*Ffi.input_lock_slot(mouse as usize)).cast());
            ee_base::ts::ee_ts_section_free((*Ffi.render_lock_slot(mouse as usize)).cast());
        }
    }

    /// `RTSMouse::LoadPointer`.
    ///
    /// # Safety
    /// `file` a C string, `screen` a live screen, `slot` a surface slot.
    #[no_mangle]
    pub unsafe extern "C" fn ee_rts_mouse_load_pointer(
        file: *const c_char,
        screen: *mut c_void,
        slot: *mut *mut c_void,
    ) -> c_long {
        unsafe { load_pointer(&mut Ffi, core::ffi::CStr::from_ptr(file), screen as usize, slot) }
    }

    /// `RTSMouse::Load`.
    ///
    /// # Safety
    /// `mouse` is the live object, `screen` a live screen.
    #[no_mangle]
    pub unsafe extern "C" fn ee_rts_mouse_load(mouse: *mut c_void, screen: *mut c_void) -> c_long {
        let m = mouse as usize;
        let s = screen as usize;
        let mut one =
            |c: &mut Ffi, file: &core::ffi::CStr, slot: *mut *mut c_void| unsafe { load_pointer(c, file, s, slot) };
        unsafe { load(&mut Ffi, m, s, &mut one, &mut verify) }
    }

    extern "C" {
        /// `gISMouse`.
        fn ee_rtsmcpp_is_mouse() -> *const c_void;
    }

    /// The Rust mouse device.
    struct Input;
    impl MouseInput for Input {
        fn entity(&mut self, index: c_ulong) -> usize {
            let mut e: *const ee_base::input::ffi::EeInputEntity = core::ptr::null();
            // SAFETY: the game's mouse
            unsafe { ee_base::input::ffi::ee_input_mouse_entity(ee_rtsmcpp_is_mouse().cast(), index, &mut e) };
            e as usize
        }
        fn normalized(&mut self, entity: usize) -> c_long {
            ee_base::input::ffi::ee_input_entity_normalized_value(entity as *const _)
        }
        fn value(&mut self, entity: usize) -> c_long {
            ee_base::input::ffi::ee_input_entity_value(entity as *const _)
        }
    }

    /// `RTSMouse::Update`.
    ///
    /// # Safety
    /// `mouse` is the live object.
    #[no_mangle]
    pub unsafe extern "C" fn ee_rts_mouse_update(mouse: *mut c_void) {
        unsafe { update(&mut Ffi, &mut Input, mouse as usize) }
    }

    /// `RTSMouse::BeginFrame`.
    ///
    /// # Safety
    /// `mouse` is the live object.
    #[no_mangle]
    pub unsafe extern "C" fn ee_rts_mouse_begin_frame(mouse: *mut c_void) {
        let mut enter = |s: *mut c_void| unsafe { ee_base::ts::ee_ts_section_enter(s.cast()) };
        unsafe { begin_frame(&mut Ffi, mouse as usize, &mut enter) }
    }

    /// `RTSMouse::EndFrame`.
    ///
    /// # Safety
    /// `mouse` is the live object.
    #[no_mangle]
    pub unsafe extern "C" fn ee_rts_mouse_end_frame(mouse: *mut c_void) {
        let mut leave = |s: *mut c_void| unsafe { ee_base::ts::ee_ts_section_leave(s.cast()) };
        unsafe { end_frame(&mut Ffi, mouse as usize, &mut leave) }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use ee_base::shim::ShimFake;

    fn record() -> Box<EeRtsMouseFields> {
        Box::new(EeRtsMouseFields {
            mSensitivity: 0.0,
            mDesktopHeight: 0,
            mDesktopWidth: 0,
            mWheel: 5,
            mPointer: 3,
            mOverlay: 0,
            mShiftMode: true,
            mBA2: true,
            mIsFrozen: true,
        })
    }

    #[test]
    fn construction_sets_the_defaults_and_the_desktop() {
        let mut f = record();
        let mut slots = [core::ptr::null_mut::<c_void>(); 2];
        let mut c = ShimFake::default();
        c.answer("fields", &[&mut *f as *mut _ as i64]);
        c.answer("render_lock_slot", &[&mut slots[0] as *mut _ as i64]);
        c.answer("input_lock_slot", &[&mut slots[1] as *mut _ as i64]);
        c.out("display_size", 1920).out("display_size", 1080);
        let mut lines = Vec::new();
        unsafe { construct(&mut c, 1, &mut |_, l, _| lines.push(l)) };
        assert_eq!(lines, [68, 69]);
        assert_eq!((f.mDesktopWidth, f.mDesktopHeight, f.mOverlay, f.mPointer), (1920, 1080, 8, 0));
        assert!(!f.mIsFrozen && !f.mShiftMode && !f.mBA2);
        assert_eq!(f.mSensitivity, 1.0);
    }

    struct FakeInput(Vec<usize>, Vec<String>);
    impl MouseInput for FakeInput {
        fn entity(&mut self, index: c_ulong) -> usize {
            self.1.push(format!("entity({index})"));
            self.0.get(index as usize).copied().unwrap_or(0)
        }
        fn normalized(&mut self, e: usize) -> c_long {
            self.1.push(format!("normalized({e})"));
            0
        }
        fn value(&mut self, e: usize) -> c_long {
            self.1.push(format!("value({e})"));
            -120
        }
    }

    #[test]
    fn a_frozen_mouse_reads_only_the_wheel() {
        let mut f = record();
        let mut c = ShimFake::default();
        c.answer("fields", &[&mut *f as *mut _ as i64]);
        let mut i = FakeInput(vec![1, 2, 3], vec![]);
        unsafe { update(&mut c, &mut i, 9) };
        assert_eq!(f.mWheel, -120);
        assert_eq!(i.1, ["entity(0)", "entity(1)", "entity(2)", "value(3)"]);
        // no wheel device: 0
        let mut c = ShimFake::default();
        c.answer("fields", &[&mut *f as *mut _ as i64]);
        let mut i = FakeInput(vec![1, 2], vec![]);
        unsafe { update(&mut c, &mut i, 9) };
        assert_eq!(f.mWheel, 0);
    }

    #[test]
    fn a_free_mouse_reads_its_axes() {
        let mut f = record();
        f.mIsFrozen = false;
        f.mDesktopWidth = 100;
        f.mDesktopHeight = 100;
        f.mSensitivity = 1.0;
        let mut position = Ge2DPoint { x: 10.0, y: 10.0 };
        let mut bounds = GeRectangle { bottom: 100.0, left: 0.0, right: 100.0, top: 0.0 };
        let mut c = ShimFake::default();
        c.answer("fields", &[&mut *f as *mut _ as i64]);
        c.answer("position", &[&mut position as *mut _ as i64]);
        c.answer("bounds", &[&mut bounds as *mut _ as i64]);
        let mut i = FakeInput(vec![1, 2, 0], vec![]);
        unsafe { update(&mut c, &mut i, 9) };
        assert_eq!(i.1, ["entity(0)", "entity(1)", "entity(2)", "normalized(1)", "normalized(2)"]);
        assert_eq!(f.mWheel, 0);
        assert_eq!((position.x, position.y), (10.0, 10.0), "no motion, no move");
    }

    #[test]
    fn load_stops_at_the_first_failed_pointer_and_clears_the_restore_flag() {
        let mut c = ShimFake::default();
        let mut slots = [core::ptr::null_mut::<c_void>(); 26];
        let base = slots.as_mut_ptr() as i64;
        c.answer("slot", &(0..26).map(|i| base + i * 8).collect::<Vec<_>>());
        let mut seen = Vec::new();
        let mut one = |_: &mut ShimFake, file: &core::ffi::CStr, _slot: *mut *mut c_void| {
            seen.push(file.to_str().unwrap().to_string());
            if seen.len() == 3 {
                5
            } else {
                0
            }
        };
        let r = unsafe { load(&mut c, 1, 2, &mut one, &mut |_, _, _| {}) };
        assert_eq!(r, 5);
        assert_eq!(
            seen,
            ["Textures\\Normal Pointer.png", "Textures\\Attack Pointer.png", "Textures\\Invalid Target Pointer.png"]
        );
        assert_eq!(c.count("screen_create_surface"), 0);
        assert!(c.calls().ends_with("frame_call(1, 7, 0, 0x0, false)"));
        // all ten: the two save-under surfaces are created
        let mut c = ShimFake::default();
        c.answer(
            "slot",
            &(0..12).map(|i| base + [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 24, 25][i as usize] * 8).collect::<Vec<_>>(),
        );
        let mut ok = |_: &mut ShimFake, _: &core::ffi::CStr, _: *mut *mut c_void| 0;
        let mut lines = Vec::new();
        let r = unsafe { load(&mut c, 1, 2, &mut ok, &mut |ok, l, _| lines.push((ok, l))) };
        assert_eq!(r, 0);
        assert_eq!(c.count("screen_create_surface"), 2);
        assert_eq!(lines, [(true, 349), (true, 356)]);
    }

    #[test]
    fn a_pointer_without_a_pack_loads_from_the_path() {
        let mut c = ShimFake::default();
        let mut slot: *mut c_void = core::ptr::null_mut();
        c.answer("pack_shift", &[0]);
        c.answer("screen_create_surface", &[0]);
        c.out("find_file", 0x44);
        c.answer("surf_load_png", &[0]);
        c.answer("file_close", &[6]);
        let r = unsafe { load_pointer(&mut c, c"Textures\\Normal Pointer.png", 2, &mut slot) };
        assert_eq!(r, 6, "the close's error counts once the load succeeded");
        let names: Vec<&str> = c.log.iter().map(|l| l.split('(').next().unwrap()).collect();
        assert_eq!(
            names,
            [
                "screen_mode",
                "text_new",
                "pack_shift",
                "pack_drop",
                "text_delete",
                "screen_create_surface",
                "find_file",
                "surf_load_png",
                "file_close",
                "file_delete",
                "surf_set_transparency"
            ]
        );
        // a missing file: not found, nothing loaded
        let mut c = ShimFake::default();
        let r = unsafe { load_pointer(&mut c, c"x", 2, &mut slot) };
        assert_eq!(r, NOT_FOUND);
        assert_eq!(c.count("surf_load_png"), 0);
    }
}
