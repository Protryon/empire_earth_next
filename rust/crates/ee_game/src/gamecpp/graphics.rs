//! The graphics thread object's own bodies (was EEGraphics.cpp): the frame's start and end under the world lock of
//! the frame lock mode, the game's graphics messages, the surfaces' restore and the screenshot's file. Each C++ call
//! or member is a case of EEGraphics.cpp's `EEGraphicsOp` ([`super::EeGameCpp`]); the methods forward here.
//!
//! Only a game with a window runs these (the tour, gates.sh): EE-SIM draws nothing.

#![allow(clippy::unnecessary_cast, clippy::if_same_then_else)]

use std::ffi::{c_long, c_ulong, c_void, CString};

use super::rast::rast;
use super::{h, hp, pa, Owned, O, P};

/// `kErrorAlreadyExists`.
const GFX_ALREADY_EXISTS: i64 = 2;

/// The frame lock mode (`mLockMode`).
fn lock_mode(g: P) -> c_long {
    // SAFETY: the address of the graphics object's long member
    unsafe { *(hp(O::GfxLockMode, g, &[]) as *const c_long) }
}

/// EEGraphics::FrameStarting: the whole UI redrawn when needed, then the frame's data copied under the world lock
/// (the read lock and the UI's for modes 0 and 1, the write lock for 2 and 3; 1 and 3 keep it until FrameDone).
#[no_mangle]
pub extern "C" fn ee_gamecpp_gfx_frame_starting(g: *mut c_void) {
    let g = g as P;
    if h(O::UiManagerPresent, 0, &[]) != 0 && h(O::UiCurrentFormPresent, 0, &[]) != 0 {
        if h(O::AntiAliasing, 0, &[]) != 0 {
            h(O::UiRedrawAll, 0, &[]);
        } else if !rast().dirty_rectangle_support() {
            h(O::UiRedrawAll, 0, &[]);
        }
    }
    let mode = lock_mode(g);
    if mode == 0 || mode == 1 {
        h(O::UiLockForCopy, 0, &[]);
        h(O::WorldReadLock, 0, &[]);
    } else {
        h(O::WorldWriteLock, 0, &[]);
    }
    h(O::SceneFlags6668Clear, 0, &[]);
    super::ro_views::ee_gamecpp_sn_take();
    h(O::RtsGfxFrameStarting, g, &[]);
    let mode = lock_mode(g);
    if mode == 0 {
        h(O::WorldReadUnlock, 0, &[]);
        h(O::UiUnlockForCopy, 0, &[]);
    } else if mode == 2 {
        h(O::WorldWriteUnlock, 0, &[]);
    }
}

/// EEGraphics::FrameDone: the frame finished and the lock kept since FrameStarting released (modes 1 and 3).
#[no_mangle]
pub extern "C" fn ee_gamecpp_gfx_frame_done(g: *mut c_void) {
    let g = g as P;
    h(O::RtsGfxFrameDone, g, &[]);
    let mode = lock_mode(g);
    if mode == 1 {
        h(O::WorldReadUnlock, 0, &[]);
        h(O::UiUnlockForCopy, 0, &[]);
    } else if mode == 3 {
        h(O::WorldWriteUnlock, 0, &[]);
    }
}

/// EEGraphics::HandleMessage: the game's graphics messages (the fades, the letterbox, the scene's start and
/// release, the UI's redraw, a screenshot, the lock mode, the queue-done signal); the others RTSGraphics'.
#[no_mangle]
pub extern "C" fn ee_gamecpp_gfx_handle_message(g: *mut c_void, message: u32, p1: *mut c_void, p2: *mut c_void) {
    let (g, p1, p2) = (g as P, p1 as P, p2 as P);
    match message {
        0x506 => {
            super::render_object::ee_gamecpp_ro_light_fade(false, p1 as c_ulong);
        }
        0x507 => {
            super::render_object::ee_gamecpp_ro_light_fade(true, p1 as c_ulong);
        }
        0x508 => {
            super::render_object::ee_gamecpp_ro_letterbox(false, p1 as c_ulong);
        }
        0x509 => {
            super::render_object::ee_gamecpp_ro_letterbox(true, p1 as c_ulong);
        }
        0x503 => {
            super::render_object::ee_gamecpp_ro_initialise_scene();
        }
        0x504 => {
            super::render_object::ee_gamecpp_ro_release_scene();
        }
        0x50b => {
            if h(O::UiManagerPresent, 0, &[]) != 0 {
                h(O::UiLockForCopy, 0, &[]);
                h(O::UiRedrawAll, 0, &[]);
                h(O::UiUnlockForCopy, 0, &[]);
            }
        }
        0x505 => ee_gamecpp_gfx_screenshot(g as *mut c_void, p1 as *mut c_void),
        0x502 => {
            // SAFETY: the graphics object's member
            unsafe { *(hp(O::GfxLockMode, g, &[]) as *mut c_long) = p1 as c_long };
        }
        0x50a => {
            h(O::GfxQueueDoneSet, g, &[]);
        }
        0x501 => {}
        _ => {
            h(O::RtsGfxHandleMessage, g, &[message as i64, pa(p1), pa(p2)]);
        }
    }
}

/// EEGraphics::RestoreSurfaces: the lost surfaces restored, then the mouse pointer hidden, the UI redrawn (two more
/// redraws posted), the cursor reloaded and the pointer shown again if it was; the restore's result.
#[no_mangle]
pub extern "C" fn ee_gamecpp_gfx_restore_surfaces(g: *mut c_void) -> c_long {
    let g = g as P;
    let was_drawing = h(O::MousePointerShown, 0, &[]) != 0;
    let result = h(O::RtsGfxRestoreSurfaces, g, &[]) as c_long;
    if result == 0 {
        h(O::MousePointerPost, 0, &[0x402]);
        while h(O::MousePointerShown, 0, &[]) != 0 {
            ee_base::plat_core::time::PlatSleep(1);
        }
        while h(O::MousePointerShown, 0, &[]) != 0 {
            ee_base::plat_core::time::PlatSleep(1);
        }
        h(O::WorldWriteLock, 0, &[]);
        h(O::UiRedrawAll, 0, &[]);
        h(O::GfxPostRedrawUI, g, &[]);
        h(O::GfxPostRedrawUI, g, &[]);
        h(O::MouseBeginFrame, 0, &[]);
        h(O::MouseLoadScreenIndex, 0, &[]);
        h(O::MouseEndFrame, 0, &[]);
        h(O::WorldWriteUnlock, 0, &[]);
        if was_drawing {
            h(O::MousePointerPost, 0, &[0x404]);
        }
    }
    result
}

/// EEGraphics::SaveScreenShot: the capture rendered into the first free file EE###.bmp (or .jpg), or
/// EEPostGame-M-D-Y-###.jpg for the post-game map, then the capture signalled done.
#[no_mangle]
pub extern "C" fn ee_gamecpp_gfx_screenshot(g: *mut c_void, capture: *mut c_void) {
    let (g, capture) = (g as P, capture as P);
    let file = Owned::new(O::FileSpecNew, O::FileSpecDelete);
    let mut now = [0 as c_ulong; 6];
    // SAFETY: a TMTime is six unsigned longs
    unsafe { ee_base::time::ee_time_of_day(now.as_mut_ptr().cast()) };
    let post_game = h(O::CaptureIsPostGame, capture, &[]) != 0;
    let jpg = h(O::TakeJpgScreenshots, 0, &[]) != 0 || post_game;
    let extension = if jpg { "jpg" } else { "bmp" };
    let mut index: c_long = 0;
    loop {
        let name = if post_game {
            // TMTime: the month (0-based), the day, the year, ...
            format!("EEPostGame-{}-{}-{}-{:03}.{}", now[0] + 1, now[1], now[2], index, extension)
        } else {
            format!("EE{:03}.{}", index, extension)
        };
        let name = CString::new(name).unwrap_or_default();
        h(O::FileSpecSetFileC, file.0, &[name.as_ptr() as usize as i64]);
        index += 1;
        if h(O::FileSpecCreate, file.0, &[]) != GFX_ALREADY_EXISTS {
            break;
        }
    }
    super::render_to::ee_gamecpp_gfx_render_to(g as *mut c_void, capture as *mut c_void);
    let jpg = h(O::TakeJpgScreenshots, 0, &[]) != 0 || h(O::CaptureIsPostGame, capture, &[]) != 0;
    // kImageFileJPG, kImageFileBMP
    h(O::CaptureSave, capture, &[pa(file.0), if jpg { 2 } else { 1 }]);
    h(O::CaptureDoneSet, capture, &[]);
}
