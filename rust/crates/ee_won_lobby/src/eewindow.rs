//! The EE lobby's window in the game (R6 WONLobby round 29): EELobbyGlueHost.cpp's EELobbyOpen, EELobbyShow,
//! EELobbyHide, EELobbyClose and EELobbyShowGameScreen, their decisions and arithmetic here, each C++ statement one
//! of the glue host's ops (`EeWonEeGlueOp` from `SetOpen` on). The lobby's open and shown flags stay the C++
//! globals the renderer reads under the lobby lock; the back buffer's size at the last show (`sEELobbyWidth`,
//! `sEELobbyHeight`) is this module's. won_eewindow_test runs the deleted functions against these over one stand-in
//! world.

use super::{EeWonEeGlue, EeWonEeGlueOp as Op};
use std::sync::atomic::{AtomicIsize, Ordering};

/// `sEELobbyWidth`, `sEELobbyHeight`: the back buffer's size when the lobby was last shown over it (`long`).
static WIDTH: AtomicIsize = AtomicIsize::new(0);
static HEIGHT: AtomicIsize = AtomicIsize::new(0);

/// The drivers' reset of the remembered sizes.
pub(crate) fn reset() {
    WIDTH.store(0, Ordering::Relaxed);
    HEIGHT.store(0, Ordering::Relaxed);
}

impl EeWonEeGlue {
    /// EELobbyOpen: WONAPI core, window manager, the lobby window and the lobby screen in it; then shown and run
    /// until it is left.
    pub(crate) fn window_open(&self) {
        if self.op1(Op::IsLobbyOpen, 0) != 0 {
            return;
        }
        for op in [
            Op::SetOpenTrue,
            Op::CoreNew,
            Op::CoreSetPumpThread,
            Op::CoreStartup,
            Op::RootNew,
            Op::RootNoPopupWindows,
            Op::RootAddJpg,
            Op::RootAddPng,
            Op::RootSoundDecoder,
        ] {
            self.op1(op, 0);
        }
        if self.op1(Op::SecondarySurface, 0) != 0 {
            self.op1(Op::RootSetSurfaceSize, 0);
        }
        self.op1(Op::RootSetEventForward, 0);
        self.op1(Op::WindowNew, 0);
        self.op1(Op::WindowForwardKeys, 0);
        let style = if self.op1(Op::FullScreen, 0) != 0 { 0x22 } else { 2 };
        self.op1(Op::WindowSetStyle, style);
        self.op1(Op::WindowCreate, 0);
        self.op1(Op::GlueInit, 0);
        let screen = self.op1(Op::ScreenHold, 0);
        self.op1(Op::RootLayoutScreen, screen);
        self.op1(Op::RootAddScreen, screen);
        self.op1(Op::Show, 0);
        self.op1(Op::LobbyStart, 0);
        // (the screen's reference is the original's SmartPtr, given back when EELobbyOpen returned)
        self.run_loop();
        self.op1(Op::Release, screen);
    }

    /// EELobbyShow: the lobby in the game's place, the cursor where it was, the game's input suspended.
    pub(crate) fn window_show(&self) {
        if self.op1(Op::IsShown, 0) != 0 || self.op1(Op::WindowExists, 0) == 0 {
            return;
        }
        let window = self.op1(Op::GameOsWindow, 0);
        let mut cursor = [0.0f32; 2];
        let mouse = self.op1(Op::MouseExists, 0) != 0;
        if mouse {
            self.op1(Op::MousePosition, cursor.as_mut_ptr() as isize);
            self.op1(Op::MouseSetPointer, 6);
        }
        let secondary = self.op1(Op::SecondarySurface, 0);
        if secondary != 0 {
            WIDTH.store(self.op1(Op::SurfaceWidth, secondary), Ordering::Relaxed);
            HEIGHT.store(self.op1(Op::SurfaceHeight, secondary), Ordering::Relaxed);
            self.op1(Op::RootSetSurfaceSize, 0);
        }
        self.op1(Op::WindowShow, 1);
        self.op1(Op::LobbyLock, 0);
        self.op1(Op::SetShown, 1);
        if self.op1(Op::KeyboardExists, 0) != 0 {
            self.op1(Op::KeyboardSuspend, 0);
        }
        if self.op1(Op::MouseInputExists, 0) != 0 {
            self.op1(Op::MouseInputSuspend, 0);
        }
        self.op1(Op::LobbyUnlock, 0);
        self.op1(Op::TextInputStart, window);
        self.op1(Op::CursorShow, 1);
        let (w, h) = (WIDTH.load(Ordering::Relaxed), HEIGHT.load(Ordering::Relaxed));
        if self.op1(Op::MouseExists, 0) != 0 && window != 0 && w > 0 && h > 0 {
            // the game's pointer is in back buffer pixels, the window's in client pixels: `(int32_t)(x * width /
            // sEELobbyWidth)`, float arithmetic
            let mut size = [0i32; 2];
            self.op2(Op::ClientSize, window, size.as_mut_ptr() as isize);
            let x = (cursor[0] * size[0] as f32 / w as f32) as i32;
            let y = (cursor[1] * size[1] as f32 / h as f32) as i32;
            self.op(Op::MouseWarp, window, x as isize, y as isize, 0, 0, 0);
        }
        self.op1(Op::WindowInvalidateRoot, 0);
    }

    /// EELobbyHide: the game's window back, its input resumed and its pointer where the system pointer is.
    pub(crate) fn window_hide(&self) {
        if self.op1(Op::IsShown, 0) == 0 || self.op1(Op::WindowExists, 0) == 0 {
            return;
        }
        let window = self.op1(Op::GameOsWindow, 0);
        let mut pointer = [0i32; 2];
        let mut size = [0i32; 2];
        if window != 0 {
            self.op2(Op::MouseGetState, window, pointer.as_mut_ptr() as isize);
            self.op2(Op::ClientSize, window, size.as_mut_ptr() as isize);
        }
        self.op1(Op::LobbyLock, 0);
        self.op1(Op::SetShown, 0);
        self.op1(Op::LobbyUnlock, 0);
        self.op1(Op::RedrawAll, 0);
        if self.op1(Op::KeyboardExists, 0) != 0 {
            self.op1(Op::KeyboardResume, 0);
        }
        if self.op1(Op::MouseInputExists, 0) != 0 {
            self.op1(Op::MouseInputResume, 0);
        }
        self.op1(Op::CursorSetDefault, 0);
        self.op1(Op::CursorShow, 0);
        if self.op1(Op::MouseExists, 0) != 0 {
            let mut at = [0.0f32; 2];
            if size[0] > 0 && size[1] > 0 {
                // `(float)x * sEELobbyWidth / width`
                at[0] = pointer[0] as f32 * WIDTH.load(Ordering::Relaxed) as f32 / size[0] as f32;
                at[1] = pointer[1] as f32 * HEIGHT.load(Ordering::Relaxed) as f32 / size[1] as f32;
            }
            self.op1(Op::MouseSetPosition, at.as_ptr() as isize);
            self.op1(Op::MouseSetPointer, 0);
        }
        self.op1(Op::WindowShow, 0);
    }

    /// EELobbyClose: the lobby's windows and WONAPI core gone, the game's music volume back.
    pub(crate) fn window_close(&self) {
        if self.op1(Op::IsLobbyOpen, 0) == 0 {
            return;
        }
        self.op1(Op::SetOpenFalse, 0);
        self.op1(Op::Hide, 0);
        for op in [
            Op::RootCloseAll,
            Op::GlueLobbyShutdown,
            Op::CoreShutdown,
            Op::CoreDelete,
            Op::CoreClear,
            Op::WindowClear,
            Op::RootClear,
        ] {
            self.op1(op, 0);
        }
        // port: closing the lobby's windows shows the system pointer; the game draws its own
        self.op1(Op::CursorShow, 0);
        if self.op1(Op::HasUiManager, 0) != 0 {
            self.op1(Op::MusicVolumeRestore, 0);
        }
    }

    /// EELobbyShowGameScreen: the game form (30) pushed, hooked to return to the lobby once the game is done.
    pub(crate) fn window_show_game_screen(&self) {
        if self.op1(Op::HasUiManager, 0) == 0 {
            return;
        }
        let event = self.op1(Op::GameDoneEventNew, 0);
        self.op1(Op::GameFormSetOpenEvent, event);
        self.op1(Op::EventDereference, event);
        self.op1(Op::PushGameForm, 0);
    }
}

#[cfg(test)]
mod tests {
    use super::super::{EeWonEeGlue, EeWonEeGlueOp as Op};
    use std::cell::RefCell;
    use std::sync::atomic::Ordering;

    thread_local! {
        static LOG: RefCell<Vec<String>> = const { RefCell::new(Vec::new()) };
        static SHOWN: RefCell<bool> = const { RefCell::new(false) };
    }

    // a game with a window, a mouse at (100, 50), an 800 x 600 back buffer and a 1600 x 1200 client, the pointer at
    // (300, 150)
    unsafe extern "C" fn host(op: Op, a: isize, b: isize, c: isize, _: isize, _: isize, _: isize) -> isize {
        LOG.with(|l| l.borrow_mut().push(format!("{op:?} {a} {b} {c}")));
        match op {
            Op::IsShown => SHOWN.with(|s| *s.borrow()) as isize,
            Op::SetShown => {
                SHOWN.with(|s| *s.borrow_mut() = a != 0);
                0
            }
            Op::WindowExists | Op::MouseExists | Op::KeyboardExists | Op::MouseInputExists => 1,
            Op::GameOsWindow => 0x77,
            Op::SecondarySurface => 0x55,
            Op::SurfaceWidth => 800,
            Op::SurfaceHeight => 600,
            Op::MousePosition => {
                // SAFETY: the op's two floats
                unsafe { *(a as *mut [f32; 2]) = [100.0, 50.0] };
                0
            }
            Op::ClientSize => {
                // SAFETY: the op's two ints
                unsafe { *(b as *mut [i32; 2]) = [1600, 1200] };
                0
            }
            Op::MouseGetState => {
                // SAFETY: the op's two ints
                unsafe { *(b as *mut [i32; 2]) = [300, 150] };
                0
            }
            Op::MouseSetPosition => {
                // SAFETY: the op's two floats
                let p = unsafe { *(a as *const [f32; 2]) };
                LOG.with(|l| l.borrow_mut().push(format!("at {} {}", p[0], p[1])));
                0
            }
            _ => 0,
        }
    }

    #[test]
    fn show_and_hide_scale_the_pointer_between_the_back_buffer_and_the_window() {
        super::reset();
        let g = EeWonEeGlue::new();
        let f: unsafe extern "C" fn(Op, isize, isize, isize, isize, isize, isize) -> isize = host;
        g.host.store(f as usize, Ordering::Relaxed);
        g.window_show();
        let log = LOG.with(|l| std::mem::take(&mut *l.borrow_mut()));
        assert!(log.contains(&"MouseSetPointer 6 0 0".to_string()));
        assert!(log.contains(&"MouseWarp 119 200 100".to_string()), "{log:?}");
        assert_eq!(log.last().unwrap(), "WindowInvalidateRoot 0 0 0");
        // shown: a second show does nothing
        g.window_show();
        assert_eq!(LOG.with(|l| std::mem::take(&mut *l.borrow_mut())).len(), 1);
        g.window_hide();
        let log = LOG.with(|l| std::mem::take(&mut *l.borrow_mut()));
        assert!(log.contains(&"at 150 75".to_string()), "{log:?}");
        assert_eq!(log.last().unwrap(), "WindowShow 0 0 0");
        super::reset();
    }
}
