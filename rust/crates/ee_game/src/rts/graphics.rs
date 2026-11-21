//! `RTSGraphics`' bodies (LLE round 8, from `src/RTS Engine/Graphics/RTSGraphics.cpp`): the graphics thread's
//! construction and teardown, one pass of its loop, its messages, the frame (`Render`), the WON lobby's frame
//! (`RenderLobby`), the render objects' passes, the pause and resume handshakes and the surface chain.
//!
//! The class stays C++ (`ee/RTSGraphics.h`: EEGraphics derives from it); its plain members are the record
//! [`EeRtsGraphicsFields`], a base of the class. The thread and the events are Rust objects (ee_base ts) reached
//! through their slots; every other call (the rasterizer, the surfaces, the mouse, the UI manager, the render
//! objects, the lobby's frame, the virtuals) is a one-statement shim of `RTSGraphics.cpp` ([`GraphicsCpp`]).
//!
//! The loop's pass is Rust but its `Sleep(1)` stays in the C++ `RTSGraphics::Main`: the legacy harness's time shim
//! caps the frame rate at that call by its return address (tools/harness/timeshim.py `RENDER_MAIN`).

use core::ffi::{c_char, c_int, c_long, c_ulong, c_void};
use core::ptr::{addr_of, addr_of_mut, read_volatile, write_volatile};

use ee_base::shim::ShimOut;

/// `RTSGraphics`' plain members (`ee/RTSGraphics.h`), in the class's order: a base of the class.
#[repr(C)]
#[allow(non_snake_case)]
pub struct EeRtsGraphicsFields {
    /// the rasterizer's screen
    pub mScreen: c_ulong,
    /// message 0x40e: the overdraw display mode
    pub mShowOverdraw: bool,
    pub mL50: c_long,
    /// message 0x408 (clearing it redraws the UI)
    pub mB54: bool,
    /// (= true; volatile)
    pub mIsPaused: bool,
    /// PostResume waits for it to clear (volatile)
    pub mIsPausePending: bool,
    /// PostPause waits for it to clear (volatile)
    pub mIsResumePending: bool,
    /// message 0x401: the thread ends
    pub mIsQuitting: bool,
    pub mIsWindowed: bool,
    /// message 0x40a
    pub mIsSlowPlayback: bool,
    /// RenderLobby: the lobby frame packed into the frame surface
    pub mLobbySerial: u32,
    /// RenderLobby: the back buffer's size last given to the lobby
    pub mLobbySecondaryWidth: c_ulong,
    pub mLobbySecondaryHeight: c_ulong,
    /// RenderLobby: `TMGetTime` of the last ShowScene
    pub mLobbyShownTime: c_ulong,
}

ee_base::cpp_shims! {
    trait GraphicsCpp;
    fn fields(graphics: usize) -> *mut EeRtsGraphicsFields = ee_rtsgcpp_fields;
    fn thread_slot(graphics: usize) -> *mut *mut c_void = ee_rtsgcpp_thread_slot;
    fn render_now_slot(graphics: usize) -> *mut *mut c_void = ee_rtsgcpp_render_now_slot;
    fn created_slot(graphics: usize) -> *mut *mut c_void = ee_rtsgcpp_created_slot;
    fn done_slot(graphics: usize) -> *mut *mut c_void = ee_rtsgcpp_done_slot;
    fn rasterizer(graphics: usize) -> usize = ee_rtsgcpp_rasterizer;
    fn set_rasterizer(graphics: usize, rasterizer: usize) = ee_rtsgcpp_set_rasterizer;
    fn surface(graphics: usize) -> usize = ee_rtsgcpp_surface;
    fn set_surface_field(graphics: usize, surface: usize) = ee_rtsgcpp_set_surface_field;
    fn u38(graphics: usize) -> usize = ee_rtsgcpp_u38;
    fn set_u38(graphics: usize, surface: usize) = ee_rtsgcpp_set_u38;
    fn mouse(graphics: usize) -> usize = ee_rtsgcpp_mouse;
    fn set_mouse(graphics: usize, mouse: usize) = ee_rtsgcpp_set_mouse;
    fn ui_manager(graphics: usize) -> usize = ee_rtsgcpp_ui_manager;
    fn set_ui_manager(graphics: usize, manager: usize) = ee_rtsgcpp_set_ui_manager;
    fn lobby_frame(graphics: usize) -> usize = ee_rtsgcpp_lobby_frame;
    fn lobby_frame_slot(graphics: usize) -> *mut *mut c_void = ee_rtsgcpp_lobby_frame_slot;
    fn thread_name() -> *const c_char = ee_rtsgcpp_thread_name;
    fn handle_message(graphics: usize, message: c_ulong, first: usize, second: usize) = ee_rtsgcpp_handle_message;
    fn render(graphics: usize, draw_mouse: bool, show_scene: bool) = ee_rtsgcpp_render;
    fn restore_surfaces(graphics: usize) -> c_long = ee_rtsgcpp_restore_surfaces;
    fn frame_starting(graphics: usize) = ee_rtsgcpp_frame_starting;
    fn scene_starting(graphics: usize) = ee_rtsgcpp_scene_starting;
    fn render_scene(graphics: usize) = ee_rtsgcpp_render_scene;
    fn frame_done(graphics: usize) = ee_rtsgcpp_frame_done;
    fn objects_size(graphics: usize) -> usize = ee_rtsgcpp_objects_size;
    fn object_at(graphics: usize, index: usize) -> usize = ee_rtsgcpp_object_at;
    fn objects_push(graphics: usize, object: usize) = ee_rtsgcpp_objects_push;
    fn objects_pop(graphics: usize) = ee_rtsgcpp_objects_pop;
    fn objects_clear(graphics: usize) = ee_rtsgcpp_objects_clear;
    fn object_frame_starting(object: usize) = ee_rtsgcpp_object_frame_starting;
    fn object_scene_starting(object: usize) = ee_rtsgcpp_object_scene_starting;
    fn object_render_scene(object: usize) = ee_rtsgcpp_object_render_scene;
    fn object_frame_done(object: usize) = ee_rtsgcpp_object_frame_done;
    fn sim_mode() -> bool = ee_rtsgcpp_sim_mode;
    fn lobby_shown() -> bool = ee_rtsgcpp_lobby_shown;
    fn lobby_lock() = ee_rtsgcpp_lobby_lock;
    fn lobby_unlock() = ee_rtsgcpp_lobby_unlock;
    fn mouse_input_enter() = ee_rtsgcpp_mouse_input_enter;
    fn mouse_input_leave() = ee_rtsgcpp_mouse_input_leave;
    fn mouse_refresh_xy() = ee_rtsgcpp_mouse_refresh_xy;
    fn mouse_update() = ee_rtsgcpp_mouse_update;
    fn ui_current_form(manager: usize) -> usize = ee_rtsgcpp_ui_current_form;
    fn ui_redraw_all(manager: usize) = ee_rtsgcpp_ui_redraw_all;
    fn raster_secondary(rasterizer: usize, screen: c_ulong) -> usize = ee_rtsgcpp_raster_secondary;
    fn raster_vsync(rasterizer: usize, on: bool, screen: c_ulong) = ee_rtsgcpp_raster_vsync;
    fn raster_overdraw(rasterizer: usize, on: bool, screen: c_ulong) = ee_rtsgcpp_raster_overdraw;
    fn raster_no_texture(rasterizer: usize, on: bool, screen: c_ulong) = ee_rtsgcpp_raster_no_texture;
    fn raster_clear_view(rasterizer: usize, screen: c_ulong) = ee_rtsgcpp_raster_clear_view;
    fn raster_clear_rect(rasterizer: usize, right: f32, bottom: f32, screen: c_ulong) = ee_rtsgcpp_raster_clear_rect;
    fn raster_begin(rasterizer: usize, surface: usize, screen: c_ulong) -> c_long = ee_rtsgcpp_raster_begin;
    fn raster_end(rasterizer: usize, screen: c_ulong) -> c_long = ee_rtsgcpp_raster_end;
    fn raster_show(rasterizer: usize, screen: c_ulong) -> c_long = ee_rtsgcpp_raster_show;
    fn raster_alpha_ref_zero(rasterizer: usize, screen: c_ulong) = ee_rtsgcpp_raster_alpha_ref_zero;
    fn raster_create_surface(rasterizer: usize, screen: c_ulong, description: *const c_void, slot: *mut *mut c_void) -> c_long = ee_rtsgcpp_raster_create_surface;
    fn surf_attached(surface: usize, index: c_ulong) -> usize = ee_rtsgcpp_surf_attached;
    fn surf_attached_count(surface: usize) -> c_ulong = ee_rtsgcpp_surf_attached_count;
    fn surf_type(surface: usize) -> c_ulong = ee_rtsgcpp_surf_type;
    fn surf_text_unlock(surface: usize) = ee_rtsgcpp_surf_text_unlock;
    fn surf_width(surface: usize) -> c_ulong = ee_rtsgcpp_surf_width;
    fn surf_height(surface: usize) -> c_ulong = ee_rtsgcpp_surf_height;
    fn surf_pixel_width(surface: usize) -> c_ulong = ee_rtsgcpp_surf_pixel_width;
    fn surf_pixel_height(surface: usize) -> c_ulong = ee_rtsgcpp_surf_pixel_height;
    fn surf_render_scale(surface: usize) -> f32 = ee_rtsgcpp_surf_render_scale;
    fn surf_pixel_format(surface: usize) -> c_ulong = ee_rtsgcpp_surf_pixel_format;
    fn surf_pixel_depth(surface: usize) -> c_ulong = ee_rtsgcpp_surf_pixel_depth;
    fn surf_set_logical_size(surface: usize, width: c_ulong, height: c_ulong) = ee_rtsgcpp_surf_set_logical_size;
    fn surf_lock(surface: usize) -> c_long = ee_rtsgcpp_surf_lock;
    fn surf_unlock(surface: usize) = ee_rtsgcpp_surf_unlock;
    fn surf_memory(surface: usize) -> *mut u8 = ee_rtsgcpp_surf_memory;
    fn surf_bytes_per_row(surface: usize) -> c_ulong = ee_rtsgcpp_surf_bytes_per_row;
    fn surf_dereference(surface: usize) = ee_rtsgcpp_surf_dereference;
    fn surf_copy(from: usize, to: usize, source: *mut f32, destination: *mut f32) -> c_long = ee_rtsgcpp_surf_copy;
    fn pack_row(source: *const u8, row: *mut u8, width: c_ulong, format: c_ulong) = ee_rtsgcpp_pack_row;
    fn lobby_surface_size(width: ShimOut<c_int>, height: ShimOut<c_int>) = ee_rtsgcpp_lobby_surface_size;
    fn lobby_set_presentation(scale: f32, left: f32, top: f32, width: f32, height: f32) = ee_rtsgcpp_lobby_set_presentation;
    fn lobby_lock_frame(frame: *mut EeRtsLobbyFrame) -> bool = ee_rtsgcpp_lobby_lock_frame;
    fn lobby_unlock_frame() = ee_rtsgcpp_lobby_unlock_frame;
    fn mouse_begin_frame(mouse: usize) = ee_rtsgcpp_mouse_begin_frame;
    fn mouse_save_under(mouse: usize, surface: usize) = ee_rtsgcpp_mouse_save_under;
    fn mouse_draw(mouse: usize, surface: usize) = ee_rtsgcpp_mouse_draw;
    fn mouse_restore_under(mouse: usize, surface: usize) = ee_rtsgcpp_mouse_restore_under;
    fn mouse_end_frame(mouse: usize) = ee_rtsgcpp_mouse_end_frame;
}

/// `EELobbyLockFrame`'s outputs.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeRtsLobbyFrame {
    pub pixels: *const u32,
    pub width: c_int,
    pub height: c_int,
    pub pitch: c_int,
    pub logical_width: c_int,
    pub logical_height: c_int,
    pub serial: u32,
}

impl Default for EeRtsLobbyFrame {
    fn default() -> Self {
        EeRtsLobbyFrame {
            pixels: core::ptr::null(),
            width: 0,
            height: 0,
            pitch: 0,
            logical_width: 0,
            logical_height: 0,
            serial: 0,
        }
    }
}

/// The Rust objects' calls (the Thread System, the time, the floating-point control), a trait for the tests.
pub trait GraphicsPlat {
    fn event_new(&mut self, slot: *mut *mut c_void, manual: bool) -> c_long;
    fn event_free(&mut self, event: *mut c_void);
    fn event_set(&mut self, event: *mut c_void);
    fn event_wait(&mut self, event: *mut c_void);
    fn thread_new(&mut self, slot: *mut *mut c_void, parameter: usize) -> c_long;
    fn thread_free(&mut self, thread: *mut c_void);
    fn thread_set_priority(&mut self, thread: *mut c_void, priority: i32) -> c_long;
    fn thread_set_name(&mut self, thread: *mut c_void, name: *const c_char);
    fn thread_post(&mut self, thread: *mut c_void, message: c_ulong, first: usize, second: usize) -> c_long;
    fn thread_receive(&mut self, thread: *mut c_void, message: &mut c_ulong, first: &mut usize, second: &mut usize)
        -> c_long;
    fn fp_precision24(&mut self);
    fn fp_check24(&mut self);
    fn time(&mut self) -> c_ulong;
    fn sleep(&mut self, milliseconds: u32);
    fn verify(&mut self, ok: bool, line: c_ulong, expression: &'static [u8]);
}

const OK: c_long = 0;
/// `kErrorFailed`: the surfaces were lost.
const FAILED: c_long = 9;
/// `kSurfaceZBuffer`.
const SURFACE_Z_BUFFER: c_ulong = 3;
/// `kSurfaceSystemMemory`.
const SURFACE_SYSTEM_MEMORY: c_ulong = 4;
const MESSAGE_QUIT: c_ulong = 0x401;

/// The surface the mouse restores (`mU38`): the last of the chain before a z-buffer.
fn surface_chain<C: GraphicsCpp>(c: &mut C, graphics: usize, first: usize) {
    let mut s = first;
    while s != 0 {
        c.set_u38(graphics, s);
        if c.surf_attached_count(s) == 0 {
            break;
        }
        let next = c.surf_attached(s, 0);
        if c.surf_type(next) == SURFACE_Z_BUFFER {
            break;
        }
        s = c.surf_attached(s, 0);
    }
}

/// `RTSGraphics::RTSGraphics`: the events, the members, the surface chain, the thread (waited for).
///
/// # Safety
/// `graphics` is the C++ object under construction.
#[allow(clippy::too_many_arguments)]
pub unsafe fn construct<C: GraphicsCpp, P: GraphicsPlat>(
    c: &mut C,
    p: &mut P,
    graphics: usize,
    rasterizer: usize,
    screen: c_ulong,
    mouse: usize,
    ui_manager: usize,
    full_screen: bool,
    priority: c_long,
) {
    let f = c.fields(graphics);
    let r = p.event_new(c.render_now_slot(graphics), false);
    p.verify(r == OK, 72, b"TSScheduler::CreateEvent(this->mRenderNow, false) == keseOK");
    let r = p.event_new(c.created_slot(graphics), true);
    p.verify(r == OK, 73, b"TSScheduler::CreateEvent(this->mThreadCreated) == keseOK");
    let r = p.event_new(c.done_slot(graphics), true);
    p.verify(r == OK, 74, b"TSScheduler::CreateEvent(this->mThreadDone) == keseOK");
    c.set_rasterizer(graphics, rasterizer);
    let surface = c.raster_secondary(rasterizer, screen);
    c.set_surface_field(graphics, surface);
    c.set_mouse(graphics, mouse);
    c.set_ui_manager(graphics, ui_manager);
    unsafe {
        (*f).mScreen = screen;
        (*f).mL50 = 0;
        (*f).mB54 = false;
        (*f).mIsPaused = true;
        (*f).mIsQuitting = false;
        (*f).mIsPausePending = false;
        (*f).mIsResumePending = false;
        (*f).mIsWindowed = !full_screen;
        (*f).mIsSlowPlayback = false;
        (*f).mShowOverdraw = false;
        (*f).mLobbySerial = 0;
        (*f).mLobbySecondaryWidth = 0;
        (*f).mLobbySecondaryHeight = 0;
        (*f).mLobbyShownTime = 0;
        *c.lobby_frame_slot(graphics) = core::ptr::null_mut();
    }
    surface_chain(c, graphics, surface);
    let r = p.thread_new(c.thread_slot(graphics), graphics);
    p.verify(r == OK, 111, b"TSScheduler::CreateThread(this->mThread, RTSGraphics::Main, (void*)this) == keseOK");
    let thread = unsafe { *c.thread_slot(graphics) };
    let r = p.thread_set_priority(thread, priority as i32);
    p.verify(r == OK, 112, b"this->mThread->SetPriority(inThreadPriority) == keseOK");
    p.event_wait(unsafe { *c.created_slot(graphics) });
    p.thread_set_name(thread, c.thread_name());
}

/// `RTSGraphics::~RTSGraphics`: the lobby frame dereferenced, the events and the thread destroyed.
///
/// # Safety
/// `graphics` is the C++ object being destroyed.
pub unsafe fn destruct<C: GraphicsCpp, P: GraphicsPlat>(c: &mut C, p: &mut P, graphics: usize) {
    let frame = c.lobby_frame(graphics);
    if frame != 0 {
        c.surf_dereference(frame);
        unsafe { *c.lobby_frame_slot(graphics) = core::ptr::null_mut() };
    }
    unsafe {
        p.event_free(*c.render_now_slot(graphics));
        p.event_free(*c.created_slot(graphics));
        p.event_free(*c.done_slot(graphics));
        p.thread_free(*c.thread_slot(graphics));
    }
}

/// `RTSGraphics::Main`'s start: the creation event set, single precision.
///
/// # Safety
/// `graphics` is the live object.
pub unsafe fn main_start<C: GraphicsCpp, P: GraphicsPlat>(c: &mut C, p: &mut P, graphics: usize) {
    unsafe { p.event_set(*c.created_slot(graphics)) };
    p.fp_precision24();
}

/// One pass of `RTSGraphics::Main`'s loop (its `Sleep(1)` is the C++'s): false when the thread is to end (the done
/// event set). A message is handled; then, unless paused, quitting or the headless simulation, the mouse is updated
/// and a frame rendered, or the lobby's frame while the lobby is shown.
///
/// # Safety
/// `graphics` is the live object.
pub unsafe fn main_pass<C: GraphicsCpp, P: GraphicsPlat>(c: &mut C, p: &mut P, graphics: usize) -> bool {
    let f = c.fields(graphics);
    let quitting = || unsafe { read_volatile(addr_of!((*f).mIsQuitting)) };
    if quitting() {
        unsafe { p.event_set(*c.done_slot(graphics)) };
        return false;
    }
    let (mut message, mut first, mut second) = (0 as c_ulong, 0usize, 0usize);
    let r = p.thread_receive(unsafe { *c.thread_slot(graphics) }, &mut message, &mut first, &mut second);
    p.fp_check24();
    if r == OK {
        c.handle_message(graphics, message, first, second);
    }
    let paused = unsafe { read_volatile(addr_of!((*f).mIsPaused)) };
    if !c.sim_mode() && !c.lobby_shown() && !paused && !quitting() {
        c.mouse_input_enter();
        c.mouse_refresh_xy();
        c.mouse_update();
        c.mouse_input_leave();
        c.render(graphics, true, true);
    } else if !c.sim_mode() && c.lobby_shown() && !quitting() {
        unsafe { render_lobby(c, p, graphics) };
    }
    true
}

/// `RTSGraphics::HandleMessage`: quit, vsync, pause and resume, the render objects, the rasterizer and the UI
/// manager, the flags, the debug display modes (the original's 0x40c and 0x40d started and stopped a movie). The bool
/// messages take the parameter's low bit pattern as the C++'s `(bool)` cast did (any non-zero is true).
///
/// # Safety
/// `graphics` is the live object.
pub unsafe fn handle_message<C: GraphicsCpp>(c: &mut C, graphics: usize, message: c_ulong, first: usize, second: usize) {
    let f = c.fields(graphics);
    let on = first != 0;
    unsafe {
        match message {
            MESSAGE_QUIT => (*f).mIsQuitting = true,
            0x40b => {
                let r = c.rasterizer(graphics);
                c.raster_vsync(r, on, (*f).mScreen)
            }
            0x402 => {
                write_volatile(addr_of_mut!((*f).mIsPausePending), false);
                write_volatile(addr_of_mut!((*f).mIsPaused), true);
            }
            0x403 => {
                write_volatile(addr_of_mut!((*f).mIsResumePending), false);
                write_volatile(addr_of_mut!((*f).mIsPaused), false);
            }
            0x404 => c.objects_push(graphics, first),
            0x405 => c.objects_pop(graphics),
            0x406 => c.objects_clear(graphics),
            0x407 => {
                c.set_rasterizer(graphics, first);
                (*f).mScreen = second as c_ulong;
            }
            0x409 => c.set_ui_manager(graphics, first),
            0x408 => {
                (*f).mB54 = on;
                let m = c.ui_manager(graphics);
                if !(*f).mB54 && m != 0 && c.ui_current_form(m) != 0 {
                    c.ui_redraw_all(m);
                }
            }
            0x40a => (*f).mIsSlowPlayback = on,
            0x40e => {
                let r = c.rasterizer(graphics);
                c.raster_overdraw(r, on, (*f).mScreen);
                (*f).mShowOverdraw = on;
            }
            0x40f => {
                let r = c.rasterizer(graphics);
                c.raster_no_texture(r, on, (*f).mScreen)
            }
            _ => {}
        }
    }
}

/// `RTSGraphics::Render`: every render object's scene, then the mouse pointer and the flip as asked; lost surfaces
/// restored on failure.
///
/// # Safety
/// `graphics` is the live object.
pub unsafe fn render<C: GraphicsCpp>(c: &mut C, graphics: usize, draw_mouse: bool, show_scene: bool) {
    let f = c.fields(graphics);
    let screen = unsafe { (*f).mScreen };
    let rasterizer = c.rasterizer(graphics);
    c.frame_starting(graphics);
    c.scene_starting(graphics);
    if unsafe { (*f).mShowOverdraw } {
        c.raster_clear_view(rasterizer, screen);
    }
    c.lobby_lock();
    let mut result = 5;
    if !c.lobby_shown() {
        let surface = c.surface(graphics);
        result = c.raster_begin(rasterizer, surface, screen);
        if result == OK {
            c.render_scene(graphics);
            result = c.raster_end(rasterizer, screen);
        }
    }
    c.lobby_unlock();
    c.frame_done(graphics);
    let surface = c.surface(graphics);
    c.surf_text_unlock(surface);
    if result == OK {
        let mouse = c.mouse(graphics);
        if draw_mouse {
            c.mouse_begin_frame(mouse);
            let surface = c.surface(graphics);
            c.mouse_save_under(mouse, surface);
            let surface = c.surface(graphics);
            c.mouse_draw(mouse, surface);
        }
        if show_scene {
            c.lobby_lock();
            if !c.lobby_shown() {
                c.raster_show(rasterizer, screen);
            }
            c.lobby_unlock();
        }
        if draw_mouse {
            // port: this frame's own save-under goes back (the windowed restore) in full screen too: GX presents a
            // copy of the back buffer and keeps it
            let u38 = c.u38(graphics);
            c.mouse_restore_under(mouse, u38);
            c.mouse_end_frame(mouse);
        }
    } else if result == FAILED {
        c.restore_surfaces(graphics);
    }
}

/// The render objects' pass `which` (0 FrameStarting, 1 SceneStarting, 2 RenderScene, 3 FrameDone), each object's
/// slot in order; RenderScene first resets the alpha test reference.
///
/// # Safety
/// `graphics` is the live object.
pub unsafe fn objects_pass<C: GraphicsCpp>(c: &mut C, graphics: usize, which: u32) {
    if which == 2 {
        let f = c.fields(graphics);
        let r = c.rasterizer(graphics);
        c.raster_alpha_ref_zero(r, unsafe { (*f).mScreen });
    }
    let mut i = 0;
    while i < c.objects_size(graphics) {
        let o = c.object_at(graphics, i);
        match which {
            0 => c.object_frame_starting(o),
            1 => c.object_scene_starting(o),
            2 => c.object_render_scene(o),
            _ => c.object_frame_done(o),
        }
        i += 1;
    }
}

/// `RTSGraphics::RenderLobby` (port): the WON lobby's frame packed into a system-memory surface in the back buffer's
/// format, copied letterboxed (the largest 4:3 rectangle of whole back-buffer pixels, centred) to the back buffer and
/// shown; an unchanged frame is shown again every 100 ms. The lobby's pixel scale is the letterbox's height in
/// pixels over the lobby's logical height, from 1 to 4. Lock order: the lobby lock, then the lobby's frame lock.
///
/// # Safety
/// `graphics` is the live object.
pub unsafe fn render_lobby<C: GraphicsCpp, P: GraphicsPlat>(c: &mut C, p: &mut P, graphics: usize) {
    let f = c.fields(graphics);
    let screen = unsafe { (*f).mScreen };
    let rasterizer = c.rasterizer(graphics);
    let target = c.raster_secondary(rasterizer, screen);
    if target == 0 {
        return;
    }
    let mut changed = false;
    c.lobby_lock();
    unsafe {
        if c.surf_width(target) != (*f).mLobbySecondaryWidth || c.surf_height(target) != (*f).mLobbySecondaryHeight {
            (*f).mLobbySecondaryWidth = c.surf_width(target);
            (*f).mLobbySecondaryHeight = c.surf_height(target);
            changed = true;
        }
    }
    let scale = if c.surf_render_scale(target) > 0.0 { c.surf_render_scale(target) } else { 1.0 };
    let target_width = c.surf_width(target) as c_long;
    let target_height = c.surf_height(target) as c_long;
    let (mut lobby_width, mut lobby_height) = (0 as c_int, 0 as c_int);
    c.lobby_surface_size(ShimOut::of(&mut lobby_width), ShimOut::of(&mut lobby_height));
    let units = (target_width / 4).min(target_height / 3).max(1);
    let (box_width, box_height) = (units * 4, units * 3);
    let (box_left, box_top) = ((target_width - box_width) / 2, (target_height - box_height) / 2);
    let pixel_scale = (box_height as f32 * scale / lobby_height as f32).min(4.0).max(1.0);
    c.lobby_set_presentation(
        pixel_scale,
        box_left as f32 / target_width as f32,
        box_top as f32 / target_height as f32,
        box_width as f32 / target_width as f32,
        box_height as f32 / target_height as f32,
    );
    let mut frame = EeRtsLobbyFrame::default();
    if c.lobby_lock_frame(&mut frame) {
        let mut lobby = c.lobby_frame(graphics);
        if lobby != 0
            && (c.surf_pixel_width(lobby) != frame.width as c_ulong
                || c.surf_pixel_height(lobby) != frame.height as c_ulong
                || c.surf_width(lobby) != frame.logical_width as c_ulong
                || c.surf_height(lobby) != frame.logical_height as c_ulong
                || c.surf_pixel_format(lobby) != c.surf_pixel_format(target))
        {
            c.surf_dereference(lobby);
            unsafe { *c.lobby_frame_slot(graphics) = core::ptr::null_mut() };
            lobby = 0;
        }
        if lobby == 0 && frame.width > 0 && frame.height > 0 && frame.logical_width > 0 && frame.logical_height > 0 {
            let description = ee_render::ge::texobj::EeGeSurfaceDescription {
                kind: SURFACE_SYSTEM_MEMORY,
                pixel_format: c.surf_pixel_format(target),
                surface_volatility: 0,
                width: frame.width as c_ulong,
                height: frame.height as c_ulong,
                m14: 0,
                mip_level_count: 0,
                pixel_depth: c.surf_pixel_depth(target),
                is_3d_renderable: false,
                is_transparency_enabled: false,
                no_alpha: false,
            };
            let slot = c.lobby_frame_slot(graphics);
            let d = &description as *const _ as *const c_void;
            if c.raster_create_surface(rasterizer, screen, d, slot) != OK {
                unsafe { *slot = core::ptr::null_mut() };
            } else {
                let made = c.lobby_frame(graphics);
                c.surf_set_logical_size(made, frame.logical_width as c_ulong, frame.logical_height as c_ulong);
            }
            lobby = c.lobby_frame(graphics);
            changed = true;
        }
        if lobby != 0 && (changed || frame.serial != unsafe { (*f).mLobbySerial }) && c.surf_lock(lobby) == OK {
            let mut row = c.surf_memory(lobby);
            let format = c.surf_pixel_format(lobby);
            let source = frame.pixels as *const u8;
            for y in 0..frame.height {
                c.pack_row(source.wrapping_add(y as usize * frame.pitch as usize), row, frame.width as c_ulong, format);
                row = row.wrapping_add(c.surf_bytes_per_row(lobby) as usize);
            }
            c.surf_unlock(lobby);
            unsafe { (*f).mLobbySerial = frame.serial };
            changed = true;
        }
        c.lobby_unlock_frame();
    }
    let time = p.time();
    let lobby = c.lobby_frame(graphics);
    if lobby != 0 && (changed || time.wrapping_sub(unsafe { (*f).mLobbyShownTime }) >= 100) {
        // GERectangle's storage order: bottom, left, right, top
        let mut source = [c.surf_height(lobby) as f32, 0.0, c.surf_width(lobby) as f32, 0.0];
        let mut destination =
            [(box_top + box_height) as f32, box_left as f32, (box_left + box_width) as f32, box_top as f32];
        if box_left > 0 || box_top > 0 {
            c.raster_clear_rect(rasterizer, target_width as f32, target_height as f32, screen);
        }
        let mut r = c.surf_copy(lobby, target, source.as_mut_ptr(), destination.as_mut_ptr());
        if r == OK {
            r = c.raster_show(rasterizer, screen);
        }
        if r == FAILED {
            c.restore_surfaces(graphics);
        }
        unsafe { (*f).mLobbyShownTime = time };
    }
    c.lobby_unlock();
}

/// `RTSGraphics::PostPause`: once a pending resume is handled, the pause pending and posted (0x402).
///
/// # Safety
/// `graphics` is the live object.
pub unsafe fn post_pause<C: GraphicsCpp, P: GraphicsPlat>(c: &mut C, p: &mut P, graphics: usize) -> c_long {
    let f = c.fields(graphics);
    unsafe {
        while read_volatile(addr_of!((*f).mIsResumePending)) {
            p.sleep(1);
        }
        write_volatile(addr_of_mut!((*f).mIsPausePending), true);
        p.thread_post(*c.thread_slot(graphics), 0x402, 0, 0)
    }
}

/// `RTSGraphics::Shutdown`: the quit posted, then waited for.
///
/// # Safety
/// `graphics` is the live object.
pub unsafe fn shutdown<C: GraphicsCpp, P: GraphicsPlat>(c: &mut C, p: &mut P, graphics: usize) {
    unsafe {
        p.thread_post(*c.thread_slot(graphics), MESSAGE_QUIT, 0, 0);
        p.event_wait(*c.done_slot(graphics));
    }
}

/// `RTSGraphics::PostResume`: once a pending pause is handled, the resume pending and posted (0x403).
///
/// # Safety
/// `graphics` is the live object.
pub unsafe fn post_resume<C: GraphicsCpp, P: GraphicsPlat>(c: &mut C, p: &mut P, graphics: usize) {
    let f = c.fields(graphics);
    unsafe {
        while read_volatile(addr_of!((*f).mIsPausePending)) {
            p.sleep(1);
        }
        write_volatile(addr_of_mut!((*f).mIsResumePending), true);
        p.thread_post(*c.thread_slot(graphics), 0x403, 0, 0);
    }
}

/// `RTSGraphics::SetSurface`: the surface drawn on and the mouse's restore surface (the chain's last before the
/// z-buffer).
///
/// # Safety
/// `graphics` is the live object.
pub unsafe fn set_surface<C: GraphicsCpp>(c: &mut C, graphics: usize, surface: usize) {
    c.set_surface_field(graphics, surface);
    surface_chain(c, graphics, surface);
}

/// The Rust objects' calls.
#[cfg(not(test))]
pub struct Plat;

#[cfg(not(test))]
impl GraphicsPlat for Plat {
    fn event_new(&mut self, slot: *mut *mut c_void, manual: bool) -> c_long {
        // SAFETY: the C++ object's event slot
        unsafe { ee_base::ts::ee_ts_event_new(slot.cast(), manual) }
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
        // SAFETY: the C++ object's thread slot; the thread runs the C++ RTSGraphics::Main (its Sleep, see above)
        unsafe { ee_base::ts::ee_ts_thread_new(slot.cast(), Some(ee_rtsgcpp_main), parameter as *mut c_void, 0, false) }
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
    fn thread_post(&mut self, thread: *mut c_void, message: c_ulong, first: usize, second: usize) -> c_long {
        // SAFETY: a live thread
        unsafe { ee_base::ts::ee_ts_thread_post(thread.cast(), message, first as *mut c_void, second as *mut c_void, true) }
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
    fn fp_precision24(&mut self) {
        ee_base::util::ee_util_fp_set_precision(FP_PRECISION_24);
    }
    fn fp_check24(&mut self) {
        if ee_base::util::ee_util_fp_precision() != FP_PRECISION_24 {
            ee_base::util::ee_util_fp_set_precision(FP_PRECISION_24);
        }
        if ee_base::util::ee_util_fp_rounding() != 0 {
            ee_base::util::ee_util_fp_set_rounding(0);
        }
    }
    fn time(&mut self) -> c_ulong {
        ee_base::time::ee_time_get() as c_ulong
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
                ee_base::error::ee_error_verify(c"C:\\EEX\\RTS Engine\\Graphics\\RTSGraphics.cpp".as_ptr(), line, e.as_ptr().cast())
            };
        }
    }
}

/// `kFPPrecision24`'s `_PC_24`.
#[cfg(not(test))]
const FP_PRECISION_24: c_ulong = 0x20000;

#[cfg(not(test))]
extern "C" {
    /// `RTSGraphics::Main` (the C++ loop over [`ee_rts_graphics_pass`] and `Sleep(1)`).
    fn ee_rtsgcpp_main(graphics: *mut c_void) -> c_ulong;
}

#[cfg(not(test))]
use ee_base::shim::ShimFfi as Ffi;

/// `RTSGraphics::RTSGraphics`.
///
/// # Safety
/// `graphics` is the C++ object under construction.
#[cfg(not(test))]
#[no_mangle]
pub unsafe extern "C" fn ee_rts_graphics_construct(
    graphics: *mut c_void,
    rasterizer: *mut c_void,
    screen: c_ulong,
    mouse: *mut c_void,
    ui_manager: *mut c_void,
    full_screen: bool,
    priority: c_long,
) {
    unsafe {
        construct(
            &mut Ffi,
            &mut Plat,
            graphics as usize,
            rasterizer as usize,
            screen,
            mouse as usize,
            ui_manager as usize,
            full_screen,
            priority,
        )
    }
}

/// `RTSGraphics::~RTSGraphics`.
///
/// # Safety
/// `graphics` is the C++ object being destroyed.
#[cfg(not(test))]
#[no_mangle]
pub unsafe extern "C" fn ee_rts_graphics_destruct(graphics: *mut c_void) {
    unsafe { destruct(&mut Ffi, &mut Plat, graphics as usize) }
}

/// `RTSGraphics::Main`'s start.
///
/// # Safety
/// `graphics` is the live object.
#[cfg(not(test))]
#[no_mangle]
pub unsafe extern "C" fn ee_rts_graphics_start(graphics: *mut c_void) {
    unsafe { main_start(&mut Ffi, &mut Plat, graphics as usize) }
}

/// One pass of `RTSGraphics::Main`'s loop; false at its end.
///
/// # Safety
/// `graphics` is the live object.
#[cfg(not(test))]
#[no_mangle]
pub unsafe extern "C" fn ee_rts_graphics_pass(graphics: *mut c_void) -> bool {
    unsafe { main_pass(&mut Ffi, &mut Plat, graphics as usize) }
}

/// `RTSGraphics::HandleMessage`.
///
/// # Safety
/// `graphics` is the live object.
#[cfg(not(test))]
#[no_mangle]
pub unsafe extern "C" fn ee_rts_graphics_handle_message(
    graphics: *mut c_void,
    message: c_ulong,
    first: *mut c_void,
    second: *mut c_void,
) {
    unsafe { handle_message(&mut Ffi, graphics as usize, message, first as usize, second as usize) }
}

/// `RTSGraphics::Render`.
///
/// # Safety
/// `graphics` is the live object.
#[cfg(not(test))]
#[no_mangle]
pub unsafe extern "C" fn ee_rts_graphics_render(graphics: *mut c_void, draw_mouse: bool, show_scene: bool) {
    unsafe { render(&mut Ffi, graphics as usize, draw_mouse, show_scene) }
}

/// The render objects' passes (`FrameStarting` 0, `SceneStarting` 1, `RenderScene` 2, `FrameDone` 3).
///
/// # Safety
/// `graphics` is the live object.
#[cfg(not(test))]
#[no_mangle]
pub unsafe extern "C" fn ee_rts_graphics_objects_pass(graphics: *mut c_void, which: u32) {
    unsafe { objects_pass(&mut Ffi, graphics as usize, which) }
}

/// `RTSGraphics::RenderLobby`.
///
/// # Safety
/// `graphics` is the live object.
#[cfg(not(test))]
#[no_mangle]
pub unsafe extern "C" fn ee_rts_graphics_render_lobby(graphics: *mut c_void) {
    unsafe { render_lobby(&mut Ffi, &mut Plat, graphics as usize) }
}

/// `RTSGraphics::PostPause`.
///
/// # Safety
/// `graphics` is the live object.
#[cfg(not(test))]
#[no_mangle]
pub unsafe extern "C" fn ee_rts_graphics_post_pause(graphics: *mut c_void) -> c_long {
    unsafe { post_pause(&mut Ffi, &mut Plat, graphics as usize) }
}

/// `RTSGraphics::Shutdown`.
///
/// # Safety
/// `graphics` is the live object.
#[cfg(not(test))]
#[no_mangle]
pub unsafe extern "C" fn ee_rts_graphics_shutdown(graphics: *mut c_void) {
    unsafe { shutdown(&mut Ffi, &mut Plat, graphics as usize) }
}

/// `RTSGraphics::PostResume`.
///
/// # Safety
/// `graphics` is the live object.
#[cfg(not(test))]
#[no_mangle]
pub unsafe extern "C" fn ee_rts_graphics_post_resume(graphics: *mut c_void) {
    unsafe { post_resume(&mut Ffi, &mut Plat, graphics as usize) }
}

/// `RTSGraphics::SetSurface`.
///
/// # Safety
/// `graphics` is the live object.
#[cfg(not(test))]
#[no_mangle]
pub unsafe extern "C" fn ee_rts_graphics_set_surface(graphics: *mut c_void, surface: *mut c_void) {
    unsafe { set_surface(&mut Ffi, graphics as usize, surface as usize) }
}

#[cfg(test)]
mod tests {
    use super::*;
    use ee_base::shim::ShimFake;

    fn record() -> Box<EeRtsGraphicsFields> {
        Box::new(EeRtsGraphicsFields {
            mScreen: 2,
            mShowOverdraw: false,
            mL50: 0,
            mB54: false,
            mIsPaused: true,
            mIsPausePending: false,
            mIsResumePending: false,
            mIsQuitting: false,
            mIsWindowed: false,
            mIsSlowPlayback: false,
            mLobbySerial: 0,
            mLobbySecondaryWidth: 0,
            mLobbySecondaryHeight: 0,
            mLobbyShownTime: 0,
        })
    }

    fn fake(f: &mut EeRtsGraphicsFields) -> ShimFake {
        let mut c = ShimFake::default();
        c.answer("fields", &[f as *mut _ as i64; 64]);
        c.answer("rasterizer", &[0x70; 64]);
        c.answer("surface", &[0x80; 64]);
        c.answer("mouse", &[0x90; 64]);
        c
    }

    #[test]
    fn the_surface_chain_stops_before_the_z_buffer() {
        let mut f = record();
        let mut c = fake(&mut f);
        // 0x10 -> 0x11 (attached: 0x12, a z-buffer)
        c.answer("surf_attached_count", &[1, 1]);
        c.answer("surf_attached", &[0x11, 0x11, 0x12]);
        c.answer("surf_type", &[0, 3]);
        unsafe { set_surface(&mut c, 1, 0x10) };
        let sets: Vec<_> = c.log.iter().filter(|l| l.starts_with("set_u38")).cloned().collect();
        assert_eq!(sets, ["set_u38(1, 16)", "set_u38(1, 17)"]);
    }

    #[test]
    fn messages_set_the_flags_and_call_the_rasterizer() {
        let mut f = record();
        let mut c = fake(&mut f);
        c.answer("ui_manager", &[0x60]);
        c.answer("ui_current_form", &[0x61]);
        unsafe {
            handle_message(&mut c, 1, 0x403, 0, 0);
            handle_message(&mut c, 1, 0x40e, 7, 0);
            handle_message(&mut c, 1, 0x407, 0x71, 5);
            handle_message(&mut c, 1, 0x408, 0, 0);
            handle_message(&mut c, 1, 0x401, 0, 0);
        }
        assert!(!f.mIsPaused);
        assert!(f.mShowOverdraw);
        assert_eq!(f.mScreen, 5);
        assert!(f.mIsQuitting);
        let calls = c.calls();
        assert!(calls.contains("raster_overdraw(112, true, 2)"));
        assert!(calls.contains("set_rasterizer(1, 113)"));
        assert!(calls.contains("ui_redraw_all(96)"));
    }

    #[test]
    fn a_frame_draws_the_scene_the_mouse_and_shows() {
        let mut f = record();
        let mut c = fake(&mut f);
        c.answer("u38", &[0x81]);
        unsafe { render(&mut c, 1, true, true) };
        let order: Vec<&str> = c.log.iter().map(|l| l.split('(').next().unwrap()).filter(|n| *n != "fields" && *n != "rasterizer" && *n != "surface" && *n != "mouse").collect();
        assert_eq!(
            order,
            [
                "frame_starting", "scene_starting", "lobby_lock", "lobby_shown", "raster_begin", "render_scene",
                "raster_end", "lobby_unlock", "frame_done", "surf_text_unlock", "mouse_begin_frame", "mouse_save_under",
                "mouse_draw", "lobby_lock", "lobby_shown", "raster_show", "lobby_unlock", "u38", "mouse_restore_under",
                "mouse_end_frame"
            ]
        );
        // a lost surface: restored, nothing drawn
        let mut c = fake(&mut f);
        c.answer("raster_begin", &[9]);
        unsafe { render(&mut c, 1, true, true) };
        assert_eq!(c.count("restore_surfaces"), 1);
        assert_eq!(c.count("mouse_draw"), 0);
    }

    #[test]
    fn the_object_passes_call_each_object_in_order() {
        let mut f = record();
        let mut c = fake(&mut f);
        c.answer("objects_size", &[2, 2, 2]);
        c.answer("object_at", &[5, 6]);
        unsafe { objects_pass(&mut c, 1, 2) };
        let calls = c.calls();
        assert!(calls.starts_with("fields(1)\nrasterizer(1)\nraster_alpha_ref_zero(112, 2)"));
        assert!(calls.contains("object_render_scene(5)") && calls.contains("object_render_scene(6)"));
    }
}
