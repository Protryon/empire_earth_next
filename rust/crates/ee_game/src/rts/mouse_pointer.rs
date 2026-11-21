//! `RTSMousePointer`'s bodies (LLE round 8, from `src/RTS Engine/Graphics/RTSMousePointer.cpp`): the mouse-pointer
//! thread (the pointer drawn straight to the screen while the graphics thread does not draw it), its construction,
//! teardown and messages.
//!
//! The class stays C++ (`ee/RTSMousePointer.h`); its members are reached through one-statement shims of
//! `RTSMousePointer.cpp` ([`PointerCpp`]); the thread and the events are Rust objects (ee_base ts).

use core::ffi::{c_char, c_long, c_ulong, c_void};

use ee_base::math::types::Ge2DPoint;

ee_base::cpp_shims! {
    trait PointerCpp;
    fn thread_slot(pointer: usize) -> *mut *mut c_void = ee_rtsmpcpp_thread_slot;
    fn created_slot(pointer: usize) -> *mut *mut c_void = ee_rtsmpcpp_created_slot;
    fn done_slot(pointer: usize) -> *mut *mut c_void = ee_rtsmpcpp_done_slot;
    fn shown_slot(pointer: usize) -> *mut bool = ee_rtsmpcpp_shown_slot;
    fn quit_slot(pointer: usize) -> *mut bool = ee_rtsmpcpp_quit_slot;
    fn surface(pointer: usize) -> usize = ee_rtsmpcpp_surface;
    fn set_surface(pointer: usize, surface: usize) = ee_rtsmpcpp_set_surface;
    fn mouse(pointer: usize) -> usize = ee_rtsmpcpp_mouse;
    fn set_mouse(pointer: usize, mouse: usize) = ee_rtsmpcpp_set_mouse;
    fn thread_name() -> *const c_char = ee_rtsmpcpp_thread_name;
    fn mouse_input_enter() = ee_rtsgcpp_mouse_input_enter;
    fn mouse_input_leave() = ee_rtsgcpp_mouse_input_leave;
    fn mouse_refresh_xy() = ee_rtsgcpp_mouse_refresh_xy;
    fn mouse_update() = ee_rtsgcpp_mouse_update;
    fn mouse_position(mouse: usize, out: *mut Ge2DPoint) = ee_rtsmpcpp_mouse_position;
    fn full_screen(surface: usize) -> bool = ee_rtsmpcpp_full_screen;
    fn client_origin(pointer: usize, out: *mut Ge2DPoint) = ee_rtsmpcpp_client_origin;
    fn mouse_begin_frame(mouse: usize) = ee_rtsgcpp_mouse_begin_frame;
    fn mouse_restore_at(mouse: usize, surface: usize, origin: *const Ge2DPoint) = ee_rtsmpcpp_mouse_restore_at;
    fn mouse_save_at(mouse: usize, surface: usize, origin: *const Ge2DPoint) = ee_rtsmpcpp_mouse_save_at;
    fn mouse_draw_at(mouse: usize, surface: usize, origin: *const Ge2DPoint) = ee_rtsmpcpp_mouse_draw_at;
    fn mouse_end_frame(mouse: usize) = ee_rtsgcpp_mouse_end_frame;
}

/// The platform's calls (Thread System, floating-point control, sleep).
pub trait PointerPlat {
    fn event_new(&mut self, slot: *mut *mut c_void) -> c_long;
    fn event_free(&mut self, event: *mut c_void);
    fn event_set(&mut self, event: *mut c_void);
    fn event_wait(&mut self, event: *mut c_void);
    fn thread_new(&mut self, slot: *mut *mut c_void, parameter: usize) -> c_long;
    fn thread_free(&mut self, thread: *mut c_void);
    fn thread_set_priority(&mut self, thread: *mut c_void, priority: i32) -> c_long;
    fn thread_set_name(&mut self, thread: *mut c_void, name: *const c_char);
    fn thread_receive(
        &mut self,
        thread: *mut c_void,
        message: &mut c_ulong,
        first: &mut usize,
        second: &mut usize,
    ) -> c_long;
    fn fp_precision53(&mut self);
    fn fp_check53(&mut self);
    fn sleep(&mut self, milliseconds: u32);
    fn verify(&mut self, ok: bool, line: c_ulong, expression: &'static [u8]);
}

const OK: c_long = 0;
const MESSAGE_QUIT: c_ulong = 0x401;

/// `RTSMousePointer::RTSMousePointer` (after `mWindow`): the events, hidden with no surface, the thread (waited
/// for).
///
/// # Safety
/// `pointer` is the C++ object under construction.
pub unsafe fn construct<C: PointerCpp, P: PointerPlat>(
    c: &mut C,
    p: &mut P,
    pointer: usize,
    mouse: usize,
    priority: c_long,
) {
    let r = p.event_new(c.created_slot(pointer));
    p.verify(r == OK, 44, b"TSScheduler::CreateEvent(this->mThreadCreated) == keseOK");
    let r = p.event_new(c.done_slot(pointer));
    p.verify(r == OK, 45, b"TSScheduler::CreateEvent(this->mThreadDone) == keseOK");
    c.set_surface(pointer, 0);
    c.set_mouse(pointer, mouse);
    unsafe {
        core::ptr::write_volatile(c.shown_slot(pointer), false);
        *c.quit_slot(pointer) = false;
    }
    let r = p.thread_new(c.thread_slot(pointer), pointer);
    p.verify(r == OK, 54, b"TSScheduler::CreateThread(this->mThread, RTSMousePointer::Main, (void*)this) == keseOK");
    let thread = unsafe { *c.thread_slot(pointer) };
    let r = p.thread_set_priority(thread, priority as i32);
    p.verify(r == OK, 55, b"this->mThread->SetPriority(inThreadPriority) == keseOK");
    p.event_wait(unsafe { *c.created_slot(pointer) });
    p.thread_set_name(thread, c.thread_name());
}

/// `RTSMousePointer::~RTSMousePointer`: the events and the thread.
///
/// # Safety
/// `pointer` is the C++ object being destroyed.
pub unsafe fn destruct<C: PointerCpp, P: PointerPlat>(c: &mut C, p: &mut P, pointer: usize) {
    unsafe {
        p.event_free(*c.created_slot(pointer));
        p.event_free(*c.done_slot(pointer));
        p.thread_free(*c.thread_slot(pointer));
    }
}

/// `RTSMousePointer::Main`: while shown with a surface, the input mouse updated and, when the pointer moved, the
/// pointer redrawn (restore, save, draw: at the window's client origin unless full screen); every 25 ms.
///
/// # Safety
/// `pointer` is the live object.
pub unsafe fn main_loop<C: PointerCpp, P: PointerPlat>(c: &mut C, p: &mut P, pointer: usize) -> c_ulong {
    let mut last = Ge2DPoint { x: -1.0, y: -1.0 };
    let (mut message, mut first, mut second) = (0 as c_ulong, 0usize, 0usize);
    let _ = &mut second;
    unsafe { p.event_set(*c.created_slot(pointer)) };
    p.fp_precision53();
    let quit = |c: &mut C| unsafe { *c.quit_slot(pointer) };
    while !quit(c) {
        let r = p.thread_receive(unsafe { *c.thread_slot(pointer) }, &mut message, &mut first, &mut second);
        p.fp_check53();
        if r == OK {
            unsafe { handle_message(c, pointer, message, first) };
        }
        let shown = unsafe { core::ptr::read_volatile(c.shown_slot(pointer)) };
        if shown && !quit(c) && c.surface(pointer) != 0 {
            // the input mouse (RTSGetInputMouse, the game's gMouse) updated under its lock
            c.mouse_input_enter();
            c.mouse_refresh_xy();
            c.mouse_update();
            c.mouse_input_leave();
            let mouse = c.mouse(pointer);
            let mut position = Ge2DPoint { x: 0.0, y: 0.0 };
            c.mouse_position(mouse, &mut position);
            if position != last {
                c.mouse_begin_frame(mouse);
                last = position;
                let surface = c.surface(pointer);
                let mut origin = Ge2DPoint { x: 0.0, y: 0.0 };
                if !c.full_screen(surface) {
                    c.client_origin(pointer, &mut origin);
                }
                c.mouse_restore_at(mouse, surface, &origin);
                c.mouse_save_at(mouse, surface, &origin);
                c.mouse_draw_at(mouse, surface, &origin);
                c.mouse_end_frame(mouse);
            }
        }
        p.sleep(0x19);
    }
    unsafe { p.event_set(*c.done_slot(pointer)) };
    0
}

/// `RTSMousePointer::HandleMessage`: quit, hide (0x402), show (0x404), the surface to draw on (0x405).
///
/// # Safety
/// `pointer` is the live object.
pub unsafe fn handle_message<C: PointerCpp>(c: &mut C, pointer: usize, message: c_ulong, first: usize) {
    unsafe {
        match message {
            MESSAGE_QUIT => *c.quit_slot(pointer) = true,
            0x402 => core::ptr::write_volatile(c.shown_slot(pointer), false),
            0x404 => core::ptr::write_volatile(c.shown_slot(pointer), true),
            0x405 => c.set_surface(pointer, first),
            _ => {}
        }
    }
}

#[cfg(not(test))]
mod ffi {
    use super::*;
    use ee_base::shim::ShimFfi as Ffi;

    struct Plat;
    impl PointerPlat for Plat {
        fn event_new(&mut self, slot: *mut *mut c_void) -> c_long {
            // SAFETY: the C++ object's event slot
            unsafe { ee_base::ts::ee_ts_event_new(slot.cast(), true) }
        }
        fn event_free(&mut self, event: *mut c_void) {
            // SAFETY: an event the constructor made
            unsafe { ee_base::ts::ee_ts_event_free(event.cast()) }
        }
        fn event_set(&mut self, event: *mut c_void) {
            // SAFETY: a live event
            unsafe { ee_base::ts::ee_ts_event_set(event.cast()) };
        }
        fn event_wait(&mut self, event: *mut c_void) {
            // SAFETY: a live event
            unsafe { ee_base::ts::ee_ts_event_wait(event.cast(), u32::MAX) };
        }
        fn thread_new(&mut self, slot: *mut *mut c_void, parameter: usize) -> c_long {
            // SAFETY: the C++ object's thread slot
            unsafe {
                ee_base::ts::ee_ts_thread_new(
                    slot.cast(),
                    Some(ee_rts_mouse_pointer_main),
                    parameter as *mut c_void,
                    0,
                    false,
                )
            }
        }
        fn thread_free(&mut self, thread: *mut c_void) {
            // SAFETY: the constructor's thread
            unsafe { ee_base::ts::ee_ts_thread_free(thread.cast()) };
        }
        fn thread_set_priority(&mut self, thread: *mut c_void, priority: i32) -> c_long {
            // SAFETY: a live thread
            unsafe { ee_base::ts::ee_ts_thread_set_priority(thread.cast(), priority) }
        }
        fn thread_set_name(&mut self, thread: *mut c_void, name: *const c_char) {
            // SAFETY: a live thread, a C string
            unsafe { ee_base::ts::ee_ts_thread_set_name(thread.cast(), name) }
        }
        fn thread_receive(
            &mut self,
            thread: *mut c_void,
            message: &mut c_ulong,
            first: &mut usize,
            second: &mut usize,
        ) -> c_long {
            let (mut a, mut b) = (core::ptr::null_mut::<c_void>(), core::ptr::null_mut::<c_void>());
            // SAFETY: a live thread
            let r = unsafe { ee_base::ts::ee_ts_thread_receive(thread.cast(), message, &mut a, &mut b) };
            *first = a as usize;
            *second = b as usize;
            r
        }
        fn fp_precision53(&mut self) {
            ee_base::util::ee_util_fp_set_precision(0x10000);
        }
        fn fp_check53(&mut self) {
            if ee_base::util::ee_util_fp_precision() != 0x10000 {
                ee_base::util::ee_util_fp_set_precision(0x10000);
            }
            if ee_base::util::ee_util_fp_rounding() != 0 {
                ee_base::util::ee_util_fp_set_rounding(0);
            }
        }
        fn sleep(&mut self, milliseconds: u32) {
            ee_base::plat_core::time::PlatSleep(milliseconds);
        }
        fn verify(&mut self, ok: bool, line: c_ulong, expression: &'static [u8]) {
            if !ok {
                let mut e = expression.to_vec();
                e.push(0);
                // SAFETY: C strings
                unsafe {
                    ee_base::error::ee_error_verify(
                        c"C:\\EEX\\RTS Engine\\Graphics\\RTSMousePointer.cpp".as_ptr(),
                        line,
                        e.as_ptr().cast(),
                    )
                };
            }
        }
    }

    /// `RTSMousePointer::RTSMousePointer`.
    ///
    /// # Safety
    /// `pointer` is the C++ object under construction.
    #[no_mangle]
    pub unsafe extern "C" fn ee_rts_mouse_pointer_construct(
        pointer: *mut c_void,
        mouse: *mut c_void,
        priority: c_long,
    ) {
        unsafe { construct(&mut Ffi, &mut Plat, pointer as usize, mouse as usize, priority) }
    }

    /// `RTSMousePointer::~RTSMousePointer`.
    ///
    /// # Safety
    /// `pointer` is the C++ object being destroyed.
    #[no_mangle]
    pub unsafe extern "C" fn ee_rts_mouse_pointer_destruct(pointer: *mut c_void) {
        unsafe { destruct(&mut Ffi, &mut Plat, pointer as usize) }
    }

    /// `RTSMousePointer::Main` (the thread's function).
    ///
    /// # Safety
    /// `pointer` is the live object.
    #[no_mangle]
    pub unsafe extern "C" fn ee_rts_mouse_pointer_main(pointer: *mut c_void) -> c_ulong {
        unsafe { main_loop(&mut Ffi, &mut Plat, pointer as usize) }
    }

    /// `RTSMousePointer::HandleMessage`.
    ///
    /// # Safety
    /// `pointer` is the live object.
    #[no_mangle]
    pub unsafe extern "C" fn ee_rts_mouse_pointer_handle_message(
        pointer: *mut c_void,
        message: c_ulong,
        first: *mut c_void,
    ) {
        unsafe { handle_message(&mut Ffi, pointer as usize, message, first as usize) }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use ee_base::shim::ShimFake;

    #[test]
    fn messages_show_hide_quit_and_set_the_surface() {
        let (mut shown, mut quit) = (false, false);
        let mut c = ShimFake::default();
        c.answer("shown_slot", &[&mut shown as *mut _ as i64; 4]);
        c.answer("quit_slot", &[&mut quit as *mut _ as i64; 4]);
        unsafe {
            handle_message(&mut c, 1, 0x404, 0);
            assert!(shown);
            handle_message(&mut c, 1, 0x402, 0);
            assert!(!shown);
            handle_message(&mut c, 1, 0x405, 0x33);
            handle_message(&mut c, 1, 0x401, 0);
        }
        assert!(quit);
        assert!(c.calls().contains("set_surface(1, 51)"));
    }
}
