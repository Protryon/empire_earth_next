//! The loading splash screen (Empire Earth.cpp's EEStartScreen, game core round 10): in full screen, a thread
//! ("StartScreenMainThread") shows the splash bitmap (resource 0x70) in a popup window centred on the desktop, pumps
//! its messages until told to quit, and the start-up waits until the window has painted once. Win32 only: elsewhere
//! there is no splash window (PORT.md step 4).

use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Arc;

use ee_base::random::float_to_long;

use super::{h, O};

/// The window's top-left corner: the desktop's centre less half the bitmap, each half `UMFloatToLong`'d (as the
/// C++'s `int * 0.5f`).
pub fn start_screen_origin(desktop_width: i32, desktop_height: i32, width: i32, height: i32) -> (i32, i32) {
    (
        float_to_long(desktop_width as f32 * 0.5) - float_to_long(width as f32 * 0.5),
        float_to_long(desktop_height as f32 * 0.5) - float_to_long(height as f32 * 0.5),
    )
}

/// The splash screen: the instance its resources come from, the quit flag its thread polls, the thread.
pub(crate) struct StartScreen {
    instance: usize,
    quit: Arc<AtomicBool>,
    thread: Option<std::thread::JoinHandle<()>>,
}

/// The window's first WM_PAINT sets it (`gStartScreenPainted`).
static PAINTED: AtomicBool = AtomicBool::new(false);

impl StartScreen {
    pub fn new(instance: usize) -> StartScreen {
        StartScreen { instance, quit: Arc::new(AtomicBool::new(false)), thread: None }
    }

    /// In full screen only: starts the splash thread and waits until it has painted (or ended: no bitmap, no
    /// window; the original waited forever then).
    pub fn start(&mut self) {
        if !cfg!(windows) || h(O::PrefFullScreen, 0, &[]) == 0 {
            return;
        }
        PAINTED.store(false, Ordering::SeqCst);
        let instance = self.instance;
        let quit = self.quit.clone();
        let thread = std::thread::spawn(move || win::thread_main(instance, &quit));
        while !PAINTED.load(Ordering::SeqCst) && !thread.is_finished() {
            std::thread::sleep(std::time::Duration::from_millis(1));
        }
        self.thread = Some(thread);
    }

    /// In full screen only: tells the thread to quit and waits for it to end.
    pub fn stop(&mut self) {
        if !cfg!(windows) || h(O::PrefFullScreen, 0, &[]) == 0 {
            return;
        }
        self.quit.store(true, Ordering::SeqCst);
        if let Some(thread) = self.thread.take() {
            let _ = thread.join();
        }
    }
}

#[cfg(windows)]
mod win {
    use std::sync::atomic::{AtomicBool, AtomicIsize, AtomicPtr, Ordering};

    use windows_sys_059::Win32::Foundation::{HINSTANCE, HWND, LPARAM, LRESULT, RECT, WPARAM};
    use windows_sys_059::Win32::Graphics::Gdi::{
        BitBlt, CreateCompatibleDC, DeleteObject, GetDC, GetObjectA, GetStockObject, LoadBitmapA, ReleaseDC,
        SelectObject, BITMAP, HBRUSH, NULL_BRUSH, SRCCOPY,
    };
    use windows_sys_059::Win32::UI::WindowsAndMessaging::{
        CreateWindowExA, DefWindowProcA, DispatchMessageA, GetDesktopWindow, GetWindowRect, LoadCursorA, PeekMessageA,
        PostQuitMessage, RegisterClassExA, ShowCursor, TranslateMessage, CS_HREDRAW, CS_VREDRAW, MSG, PM_REMOVE,
        WM_ERASEBKGND, WM_PAINT, WM_SETCURSOR, WNDCLASSEXA, WS_EX_TOOLWINDOW, WS_POPUP, WS_VISIBLE,
    };

    const CLASS_NAME: &std::ffi::CStr = c"Loading Game Window";

    // the window procedure's view of the thread's bitmap and memory DC (one splash at a time)
    static BITMAP_HANDLE: AtomicPtr<core::ffi::c_void> = AtomicPtr::new(std::ptr::null_mut());
    static MEMORY_DC: AtomicPtr<core::ffi::c_void> = AtomicPtr::new(std::ptr::null_mut());
    static SIZE: AtomicIsize = AtomicIsize::new(0);

    /// Hides the cursor, skips background erases and paints the splash bitmap (flagging it painted).
    unsafe extern "system" fn window_proc(window: HWND, message: u32, wparam: WPARAM, lparam: LPARAM) -> LRESULT {
        match message {
            WM_SETCURSOR => {
                ShowCursor(0);
                0
            }
            WM_ERASEBKGND => 0,
            WM_PAINT => {
                let dc = GetDC(window);
                let memory = MEMORY_DC.load(Ordering::SeqCst);
                let old = SelectObject(memory, BITMAP_HANDLE.load(Ordering::SeqCst));
                let size = SIZE.load(Ordering::SeqCst);
                BitBlt(dc, 0, 0, (size & 0xffff_ffff) as i32, (size >> 32) as i32, memory, 0, 0, SRCCOPY);
                ReleaseDC(window, dc);
                SelectObject(memory, old);
                super::PAINTED.store(true, Ordering::SeqCst);
                0
            }
            _ => DefWindowProcA(window, message, wparam, lparam),
        }
    }

    /// Loads the bitmap, shows it in a centred popup window and pumps messages until told to quit, then frees it.
    pub fn thread_main(instance: usize, quit: &AtomicBool) {
        let instance = instance as HINSTANCE;
        // SAFETY: Win32 calls on this thread's own window, bitmap and DCs
        unsafe {
            let bitmap = LoadBitmapA(instance, 0x70 as *const u8);
            if bitmap.is_null() {
                return;
            }
            let mut info: BITMAP = std::mem::zeroed();
            GetObjectA(bitmap, std::mem::size_of::<BITMAP>() as i32, &mut info as *mut BITMAP as *mut _);
            BITMAP_HANDLE.store(bitmap, Ordering::SeqCst);
            SIZE.store((info.bmWidth as u32 as isize) | ((info.bmHeight as isize) << 32), Ordering::SeqCst);
            let class = WNDCLASSEXA {
                cbSize: std::mem::size_of::<WNDCLASSEXA>() as u32,
                style: CS_HREDRAW | CS_VREDRAW,
                lpfnWndProc: Some(window_proc),
                cbClsExtra: 0,
                cbWndExtra: 0,
                hInstance: instance,
                hIcon: std::ptr::null_mut(),
                hCursor: LoadCursorA(instance, 0x68 as *const u8),
                hbrBackground: GetStockObject(NULL_BRUSH) as HBRUSH,
                lpszMenuName: std::ptr::null(),
                lpszClassName: CLASS_NAME.as_ptr() as *const u8,
                hIconSm: std::ptr::null_mut(),
            };
            RegisterClassExA(&class);
            let mut desktop: RECT = std::mem::zeroed();
            GetWindowRect(GetDesktopWindow(), &mut desktop);
            let (x, y) = super::start_screen_origin(
                desktop.right - desktop.left,
                desktop.bottom - desktop.top,
                info.bmWidth,
                info.bmHeight,
            );
            // the title: the original's never-written 4-byte buffer
            let window = CreateWindowExA(
                WS_EX_TOOLWINDOW,
                CLASS_NAME.as_ptr() as *const u8,
                c"".as_ptr() as *const u8,
                WS_POPUP | WS_VISIBLE,
                x,
                y,
                info.bmWidth,
                info.bmHeight,
                std::ptr::null_mut(),
                std::ptr::null_mut(),
                instance,
                std::ptr::null(),
            );
            if !window.is_null() {
                let dc = GetDC(window);
                let memory = CreateCompatibleDC(dc);
                MEMORY_DC.store(memory, Ordering::SeqCst);
                ReleaseDC(window, dc);
                loop {
                    let mut message: MSG = std::mem::zeroed();
                    let mut kind = 0;
                    if PeekMessageA(&mut message, std::ptr::null_mut(), 0, 0, PM_REMOVE) != 0 {
                        kind = message.message;
                        TranslateMessage(&message);
                        DispatchMessageA(&message);
                    }
                    super::super::startup::ee_gamecpp_startup_thread_fp();
                    if quit.load(Ordering::SeqCst) {
                        PostQuitMessage(0);
                    }
                    std::thread::sleep(std::time::Duration::from_millis(1));
                    if kind == 0x12 {
                        break; // WM_QUIT
                    }
                }
                ReleaseDC(window, memory);
            }
            DeleteObject(bitmap);
        }
    }
}

#[cfg(not(windows))]
mod win {
    use std::sync::atomic::AtomicBool;

    pub fn thread_main(_: usize, _: &AtomicBool) {}
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_window_is_centred_with_each_half_rounded_to_even() {
        assert_eq!(start_screen_origin(1920, 1080, 640, 480), (640, 300));
        // 1.5 -> 2 and 2.5 -> 2 (lrintf's ties to even)
        assert_eq!(start_screen_origin(3, 5, 0, 0), (2, 2));
        assert_eq!(start_screen_origin(100, 100, 5, 3), (48, 48));
    }
}
