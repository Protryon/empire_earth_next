//! `include/platform/PlatWindow.h`: the C exports, each a thin use of SDL with its decisions in `ee_plat_ui`.

use std::ffi::{c_char, c_int, c_void, CStr};
use std::ptr::{null, null_mut};
#[cfg(windows)]
use std::sync::atomic::AtomicUsize;
use std::sync::atomic::{AtomicI32, AtomicPtr, AtomicU32, Ordering};
use std::sync::Mutex;

use ee_plat_ui::event::*;
use ee_plat_ui::font::{c_bytes, c_terminated};
use ee_plat_ui::keys;
use ee_plat_ui::window::*;

use crate::ffi::*;

const INIT_VIDEO: u32 = 0x20;
const PIXELFORMAT_ARGB8888: u32 = 0x1636_2004;
const HINT_MINIMIZE_ON_FOCUS_LOSS: &CStr = c"SDL_VIDEO_MINIMIZE_ON_FOCUS_LOSS";
const HINT_IME_IMPLEMENTED_UI: &CStr = c"SDL_IME_IMPLEMENTED_UI";
const WINDOW_PROPERTY: &CStr = c"ee.platwindow";
const PROP_TITLE: &CStr = c"SDL.window.create.title";
const PROP_X: &CStr = c"SDL.window.create.x";
const PROP_Y: &CStr = c"SDL.window.create.y";
const PROP_WIDTH: &CStr = c"SDL.window.create.width";
const PROP_HEIGHT: &CStr = c"SDL.window.create.height";
const PROP_FLAGS: &CStr = c"SDL.window.create.flags";

/// `PlatWindow`: opaque to C++.
pub struct PlatWindow {
    window: *mut SdlWindow,
    #[cfg_attr(not(target_os = "macos"), allow(dead_code))]
    metal_view: *mut c_void,
    fullscreen: bool, // asked to be full screen (SDL's flag follows asynchronously on macOS)
    fixed_size: bool, // a windowed game window: its size is the player's, not the game mode's
}

// PlatWindowLastError's text. C++ used a std::string; a Mutex for the same unsynchronised use.
static LAST_ERROR: Mutex<Vec<u8>> = Mutex::new(Vec::new());
static WAKE_EVENT: AtomicU32 = AtomicU32::new(0);
#[cfg(windows)]
static ACTIVATE_EVENT: AtomicU32 = AtomicU32::new(0);
// the game icon's pixels (PlatWindowSetIcon, off Windows)
static ICON_PIXELS: AtomicPtr<c_void> = AtomicPtr::new(null_mut());
static ICON_SIZE: AtomicI32 = AtomicI32::new(0);

fn set_last_error(text: &[u8]) {
    let mut e = LAST_ERROR.lock().unwrap_or_else(|p| p.into_inner());
    e.clear();
    e.extend_from_slice(text);
    e.push(0);
}

unsafe fn set_last_error_sdl() {
    set_last_error(CStr::from_ptr(SDL_GetError()).to_bytes());
}

/// The game's text for SDL: both UTF-8 (decision D6), so a terminated copy with any stray byte made U+FFFD.
unsafe fn utf8_c(text: *const c_char) -> Vec<u8> {
    c_terminated(&ee_plat_core::utf8::lossy(c_bytes(text)))
}

unsafe fn window_of(id: u32) -> *mut c_void {
    let w = SDL_GetWindowFromID(id);
    if w.is_null() {
        return null_mut();
    }
    SDL_GetPointerProperty(SDL_GetWindowProperties(w), WINDOW_PROPERTY.as_ptr(), null_mut())
}

struct SdlHost;

impl Host for SdlHost {
    fn window_of(&self, id: u32) -> *mut c_void {
        unsafe { window_of(id) }
    }
    fn mod_state(&self) -> u16 {
        unsafe { SDL_GetModState() }
    }
    fn time_ms(&self) -> u32 {
        unsafe { PlatTimeMilliseconds() }
    }
    fn wake_event(&self) -> u32 {
        WAKE_EVENT.load(Ordering::SeqCst)
    }
    fn activate_event(&self) -> u32 {
        #[cfg(windows)]
        {
            ACTIVATE_EVENT.load(Ordering::SeqCst)
        }
        #[cfg(not(windows))]
        0
    }
    fn focus_from_activation(&self) -> bool {
        cfg!(windows)
    }
    #[cfg(target_os = "macos")]
    fn window_event(&self, sdl_event_type: u32, window: *mut c_void) {
        // a window meant to be full screen that lost it (the game started while another application was in front, or
        // the system took it out) goes back to full screen when it is the active window again
        unsafe {
            let w = window as *mut PlatWindow;
            if sdl_event_type == SDL_EVENT_WINDOW_FOCUS_GAINED
                && !w.is_null()
                && (*w).fullscreen
                && SDL_GetWindowFlags((*w).window) & SDL_WINDOW_FULLSCREEN == 0
            {
                SDL_SetWindowFullscreen((*w).window, true);
            }
        }
    }
}

unsafe extern "C" fn show_message(title: *const c_char, text: *const c_char, flags: u32) {
    let (t, x) = (utf8_c(title), utf8_c(text));
    SDL_ShowSimpleMessageBox(message_box_flags(flags), t.as_ptr().cast(), x.as_ptr().cast(), null_mut());
}

// ---- Win32: every window is subclassed (after SDL's window procedure is in place) for two things SDL does not give:
// the native message hook sees the sent and posted messages of the window (MM_MCINOTIFY, the lobby's), and the
// activation is reported as Windows sends it (WM_ACTIVATEAPP, and WM_ACTIVATE while not minimised: the original
// window procedure's two cases) instead of SDL's keyboard-focus events, which need the window to be the foreground
// one (not true under a debugger or over RDP, where the game still got its activation messages).

#[cfg(windows)]
mod win32 {
    use super::*;

    extern "system" {
        pub fn GetPropA(window: *mut c_void, name: *const c_char) -> *mut c_void;
        pub fn SetPropA(window: *mut c_void, name: *const c_char, value: *mut c_void) -> i32;
        pub fn GetWindowLongPtrA(window: *mut c_void, index: c_int) -> isize;
        pub fn SetWindowLongPtrA(window: *mut c_void, index: c_int, value: isize) -> isize;
        pub fn IsIconic(window: *mut c_void) -> i32;
        pub fn CallWindowProcA(prev: *mut c_void, window: *mut c_void, msg: u32, wparam: usize, lparam: isize)
            -> isize;
        pub fn DefWindowProcA(window: *mut c_void, msg: u32, wparam: usize, lparam: isize) -> isize;
    }

    const NEXT_PROC: &CStr = c"ee.platwindow.next";
    const WINDOW_ID: &CStr = c"ee.platwindow.id";
    const WM_ACTIVATE: u32 = 0x0006;
    const WM_ACTIVATEAPP: u32 = 0x001C;
    const WA_INACTIVE: u32 = 0;
    const GWLP_WNDPROC: c_int = -4;
    const PROP_HWND: &CStr = c"SDL.window.win32.hwnd";

    pub static NATIVE_HOOK: AtomicUsize = AtomicUsize::new(0);
    type Hook = unsafe extern "C" fn(*mut c_void, u32, u64, i64) -> c_int;

    unsafe extern "system" fn subclass_proc(window: *mut c_void, msg: u32, wparam: usize, lparam: isize) -> isize {
        let next = GetPropA(window, NEXT_PROC.as_ptr());
        let hook = NATIVE_HOOK.load(Ordering::SeqCst);
        if hook != 0 {
            let h: Hook = std::mem::transmute::<usize, Hook>(hook);
            if h(window, msg, wparam as u64, lparam as i64) != 0 {
                return 0;
            }
        }
        let activate = msg == WM_ACTIVATE && (wparam & 0xFFFF) as u32 != WA_INACTIVE && IsIconic(window) == 0;
        let event_type = ACTIVATE_EVENT.load(Ordering::SeqCst);
        if (msg == WM_ACTIVATEAPP || activate) && event_type != 0 {
            let mut e = SdlEvent::zeroed();
            e.event_type = event_type;
            e.user.window_id = GetPropA(window, WINDOW_ID.as_ptr()) as usize as u32;
            e.user.code = if msg == WM_ACTIVATEAPP { i32::from(wparam != 0) } else { 1 };
            SDL_PushEvent(&mut e);
        }
        if next.is_null() {
            DefWindowProcA(window, msg, wparam, lparam)
        } else {
            CallWindowProcA(next, window, msg, wparam, lparam)
        }
    }

    pub unsafe fn subclass(window: *mut SdlWindow) {
        let hwnd = SDL_GetPointerProperty(SDL_GetWindowProperties(window), PROP_HWND.as_ptr(), null_mut());
        if hwnd.is_null() {
            return;
        }
        SetPropA(hwnd, WINDOW_ID.as_ptr(), SDL_GetWindowID(window) as usize as *mut c_void);
        SetPropA(hwnd, NEXT_PROC.as_ptr(), GetWindowLongPtrA(hwnd, GWLP_WNDPROC) as *mut c_void);
        SetWindowLongPtrA(hwnd, GWLP_WNDPROC, subclass_proc as *const () as isize);
    }
}

#[no_mangle]
pub extern "C" fn PlatWindowSetNativeMessageHook(
    hook: Option<unsafe extern "C" fn(*mut c_void, u32, u64, i64) -> c_int>,
) {
    #[cfg(windows)]
    win32::NATIVE_HOOK.store(hook.map_or(0, |h| h as usize), Ordering::SeqCst);
    #[cfg(not(windows))]
    let _ = hook;
}

/// # Safety
/// `name` is NUL-terminated.
#[no_mangle]
pub unsafe extern "C" fn PlatWindowSetAppName(name: *const c_char, icon_resource: u32) {
    #[cfg(windows)]
    {
        if icon_resource != 0 {
            let number = format!("{icon_resource}\0");
            SDL_SetHint(c"SDL_WINDOWS_INTRESOURCE_ICON".as_ptr(), number.as_ptr().cast());
            SDL_SetHint(c"SDL_WINDOWS_INTRESOURCE_ICON_SMALL".as_ptr(), number.as_ptr().cast());
        }
        // the window class name (SDL registers it; CS_HREDRAW | CS_VREDRAW | CS_OWNDC as SDL's default and the game's)
        SDL_RegisterApp(name, 0, null_mut());
    }
    #[cfg(not(windows))]
    let _ = icon_resource;
    SDL_SetAppMetadata(name, null(), null());
}

#[no_mangle]
pub unsafe extern "C" fn PlatWindowSystemStart() -> c_int {
    // the game minimizes itself when it loses the focus (EEUserInterface); SDL must not do it a second time
    SDL_SetHint(HINT_MINIMIZE_ON_FOCUS_LOSS.as_ptr(), c"0".as_ptr());
    // the game shows the input method's composition in its edit box (PLAT_EVENT_TEXT_EDITING); the system draws the
    // candidate list
    SDL_SetHint(HINT_IME_IMPLEMENTED_UI.as_ptr(), c"composition".as_ptr());
    if !SDL_InitSubSystem(INIT_VIDEO) {
        set_last_error_sdl();
        return 0;
    }
    #[cfg(windows)]
    if ACTIVATE_EVENT.load(Ordering::SeqCst) == 0 {
        ACTIVATE_EVENT.store(SDL_RegisterEvents(1), Ordering::SeqCst);
    }
    if WAKE_EVENT.load(Ordering::SeqCst) == 0 {
        WAKE_EVENT.store(SDL_RegisterEvents(1), Ordering::SeqCst);
    }
    PlatMessageSetHandler(Some(show_message));
    1
}

#[no_mangle]
pub unsafe extern "C" fn PlatWindowSystemStop() {
    PlatMessageSetHandler(None);
    SDL_QuitSubSystem(INIT_VIDEO);
}

#[no_mangle]
pub extern "C" fn PlatWindowLastError() -> *const c_char {
    let e = LAST_ERROR.lock().unwrap_or_else(|p| p.into_inner());
    if e.is_empty() {
        c"".as_ptr()
    } else {
        e.as_ptr().cast() // valid until the next error, as C++'s c_str()
    }
}

/// # Safety
/// `title` is NULL or NUL-terminated.
#[no_mangle]
pub unsafe extern "C" fn PlatWindowCreate(
    title: *const c_char,
    width: i32,
    height: i32,
    flags: u32,
) -> *mut PlatWindow {
    PlatWindowCreateAt(title, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, flags)
}

/// # Safety
/// `title` is NULL or NUL-terminated.
#[no_mangle]
pub unsafe extern "C" fn PlatWindowCreateAt(
    title: *const c_char,
    x: i32,
    y: i32,
    width: i32,
    height: i32,
    flags: u32,
) -> *mut PlatWindow {
    #[cfg(target_os = "macos")]
    {
        // full screen without a Space of its own: the Spaces transition is animated and asynchronous, and the game's
        // start-up (the Metal view, the first mode changes) took the new window straight back out of it
        SDL_SetHint(c"SDL_VIDEO_MAC_FULLSCREEN_SPACES".as_ptr(), c"0".as_ptr());
    }
    let sdl_flags = sdl_window_flags(flags, cfg!(target_os = "macos"));
    // a full screen game window stays up when another window or application takes the focus
    SDL_SetHint(HINT_MINIMIZE_ON_FOCUS_LOSS.as_ptr(), c"0".as_ptr());
    let props = SDL_CreateProperties();
    let utf8_title = utf8_c(title);
    SDL_SetStringProperty(props, PROP_TITLE.as_ptr(), utf8_title.as_ptr().cast());
    SDL_SetNumberProperty(props, PROP_X.as_ptr(), x as i64);
    SDL_SetNumberProperty(props, PROP_Y.as_ptr(), y as i64);
    SDL_SetNumberProperty(props, PROP_WIDTH.as_ptr(), width as i64);
    SDL_SetNumberProperty(props, PROP_HEIGHT.as_ptr(), height as i64);
    SDL_SetNumberProperty(props, PROP_FLAGS.as_ptr(), sdl_flags as i64);
    let window = SDL_CreateWindowWithProperties(props);
    SDL_DestroyProperties(props);
    if window.is_null() {
        set_last_error_sdl();
        return null_mut();
    }
    let result = Box::into_raw(Box::new(PlatWindow {
        window,
        metal_view: null_mut(),
        fullscreen: flags & PLAT_WINDOW_FULLSCREEN != 0,
        fixed_size: false,
    }));
    #[cfg(target_os = "macos")]
    if (*result).fullscreen {
        // full screen at creation is asynchronous on macOS and can be refused (another application in front, the
        // previous instance's full screen still closing): wait for it, and ask once more
        SDL_SyncWindow(window);
        if SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN == 0 {
            SDL_SetWindowFullscreen(window, true);
            SDL_SyncWindow(window);
        }
    }
    SDL_SetPointerProperty(SDL_GetWindowProperties(window), WINDOW_PROPERTY.as_ptr(), result.cast());
    #[cfg(not(windows))]
    {
        // the game's icon (Windows: the exe's resource): macOS shows it in the Dock, X11/Wayland on the window
        let icon = ICON_PIXELS.load(Ordering::SeqCst);
        let size = ICON_SIZE.load(Ordering::SeqCst);
        if !icon.is_null() && size > 0 {
            let surface = SDL_CreateSurfaceFrom(size, size, PIXELFORMAT_ARGB8888, icon, size * 4);
            if !surface.is_null() {
                SDL_SetWindowIcon(window, surface);
                SDL_DestroySurface(surface);
            }
        }
    }
    #[cfg(windows)]
    win32::subclass(window);
    result
}

/// The game icon (`inSize` x `inSize` BGRA pixels, kept in place): the build's generated `PlatWindowIcon.c` registers
/// it at start-up, off Windows, before any window exists.
///
/// # Safety
/// `pixels` stays valid for the life of the process.
#[no_mangle]
pub unsafe extern "C" fn PlatWindowSetIcon(pixels: *const c_void, size: c_int) {
    ICON_PIXELS.store(pixels as *mut c_void, Ordering::SeqCst);
    ICON_SIZE.store(size, Ordering::SeqCst);
}

/// # Safety
/// `window` is NULL or a live window.
#[no_mangle]
pub unsafe extern "C" fn PlatWindowDestroy(window: *mut PlatWindow) {
    if window.is_null() {
        return;
    }
    #[cfg(target_os = "macos")]
    if !(*window).metal_view.is_null() {
        SDL_Metal_DestroyView((*window).metal_view);
    }
    SDL_DestroyWindow((*window).window);
    drop(Box::from_raw(window));
}

#[no_mangle]
pub unsafe extern "C" fn PlatWindowSetTitle(window: *mut PlatWindow, title: *const c_char) {
    let t = utf8_c(title);
    SDL_SetWindowTitle((*window).window, t.as_ptr().cast());
}

#[no_mangle]
pub unsafe extern "C" fn PlatWindowShow(window: *mut PlatWindow, show: c_int) {
    if show != 0 {
        SDL_ShowWindow((*window).window);
    } else {
        SDL_HideWindow((*window).window);
    }
}

#[no_mangle]
pub unsafe extern "C" fn PlatWindowSetFullscreen(window: *mut PlatWindow, fullscreen: c_int) -> c_int {
    SDL_SetWindowFullscreenMode((*window).window, null()); // borderless desktop full screen, no mode change
    (*window).fullscreen = fullscreen != 0;
    c_int::from(SDL_SetWindowFullscreen((*window).window, fullscreen != 0))
}

#[no_mangle]
pub unsafe extern "C" fn PlatWindowSetFixedSize(window: *mut PlatWindow, fixed: c_int) {
    (*window).fixed_size = fixed != 0;
}

#[no_mangle]
pub unsafe extern "C" fn PlatWindowSetSize(window: *mut PlatWindow, width: i32, height: i32) {
    // a full screen window keeps the display's size: the game's mode is its render size, which GX scales to the
    // window (macOS left full screen for a titled window of the mode's size at every mode change)
    if !set_size_applies((*window).fullscreen, (*window).fixed_size, SDL_GetWindowFlags((*window).window)) {
        return;
    }
    SDL_SetWindowSize((*window).window, width, height);
}

#[no_mangle]
pub unsafe extern "C" fn PlatWindowGetPosition(window: *mut PlatWindow, out_x: *mut i32, out_y: *mut i32) {
    let (mut x, mut y) = (0, 0);
    SDL_GetWindowPosition((*window).window, &mut x, &mut y); // the client area's origin (SDL3 excludes the frame)
    *out_x = x;
    *out_y = y;
}

#[no_mangle]
pub unsafe extern "C" fn PlatWindowSetPosition(window: *mut PlatWindow, x: i32, y: i32) {
    SDL_SetWindowPosition((*window).window, x, y);
}

#[no_mangle]
pub unsafe extern "C" fn PlatWindowMinimize(window: *mut PlatWindow) {
    SDL_MinimizeWindow((*window).window);
}

#[no_mangle]
pub unsafe extern "C" fn PlatWindowRaise(window: *mut PlatWindow) {
    SDL_RaiseWindow((*window).window);
}

#[no_mangle]
pub unsafe extern "C" fn PlatWindowGetClientSize(window: *mut PlatWindow, out_width: *mut i32, out_height: *mut i32) {
    let (mut w, mut h) = (0, 0);
    SDL_GetWindowSizeInPixels((*window).window, &mut w, &mut h);
    *out_width = w;
    *out_height = h;
}

unsafe fn desktop_mode(window: *mut PlatWindow) -> *const SdlDisplayMode {
    let display = if window.is_null() { SDL_GetPrimaryDisplay() } else { SDL_GetDisplayForWindow((*window).window) };
    SDL_GetDesktopDisplayMode(display)
}

#[no_mangle]
pub unsafe extern "C" fn PlatWindowGetDisplaySize(
    window: *mut PlatWindow,
    out_width: *mut i32,
    out_height: *mut i32,
) -> c_int {
    let mode = desktop_mode(window);
    if mode.is_null() {
        return 0;
    }
    let (w, h) = display_pixels((*mode).w, (*mode).h, (*mode).pixel_density);
    *out_width = w;
    *out_height = h;
    1
}

#[no_mangle]
pub unsafe extern "C" fn PlatWindowGetDisplayLogicalSize(
    window: *mut PlatWindow,
    out_width: *mut i32,
    out_height: *mut i32,
) -> c_int {
    let mode = desktop_mode(window);
    if mode.is_null() {
        return 0;
    }
    *out_width = (*mode).w;
    *out_height = (*mode).h;
    1
}

const NATIVE_NONE: i32 = 0;
#[cfg(windows)]
const NATIVE_WIN32: i32 = 1;
#[cfg(all(unix, not(target_os = "macos")))]
const NATIVE_X11: i32 = 2;
#[cfg(all(unix, not(target_os = "macos")))]
const NATIVE_WAYLAND: i32 = 3;
#[cfg(target_os = "macos")]
const NATIVE_METAL: i32 = 4;

#[no_mangle]
pub unsafe extern "C" fn PlatWindowGetNative(window: *mut PlatWindow, out: *mut PlatNativeWindow) -> c_int {
    *out = PlatNativeWindow { kind: NATIVE_NONE, display: null_mut(), window: null_mut() };
    let props = SDL_GetWindowProperties((*window).window);
    #[cfg(windows)]
    {
        (*out).kind = NATIVE_WIN32;
        (*out).display = SDL_GetPointerProperty(props, c"SDL.window.win32.instance".as_ptr(), null_mut());
        (*out).window = SDL_GetPointerProperty(props, c"SDL.window.win32.hwnd".as_ptr(), null_mut());
    }
    #[cfg(target_os = "macos")]
    {
        let _ = props;
        if (*window).metal_view.is_null() {
            (*window).metal_view = SDL_Metal_CreateView((*window).window);
        }
        (*out).kind = NATIVE_METAL;
        (*out).window =
            if (*window).metal_view.is_null() { null_mut() } else { SDL_Metal_GetLayer((*window).metal_view) };
    }
    #[cfg(all(unix, not(target_os = "macos")))]
    {
        let driver = SDL_GetCurrentVideoDriver();
        if !driver.is_null() {
            let name = CStr::from_ptr(driver).to_bytes();
            if name == b"wayland" {
                (*out).kind = NATIVE_WAYLAND;
                (*out).display = SDL_GetPointerProperty(props, c"SDL.window.wayland.display".as_ptr(), null_mut());
                (*out).window = SDL_GetPointerProperty(props, c"SDL.window.wayland.surface".as_ptr(), null_mut());
            } else if name == b"x11" {
                (*out).kind = NATIVE_X11;
                (*out).display = SDL_GetPointerProperty(props, c"SDL.window.x11.display".as_ptr(), null_mut());
                (*out).window =
                    SDL_GetNumberProperty(props, c"SDL.window.x11.window".as_ptr(), 0) as usize as *mut c_void;
            }
        }
    }
    if (*out).kind == NATIVE_NONE || (*out).window.is_null() {
        set_last_error(b"no native window handle for this video driver");
        return 0;
    }
    1
}

#[no_mangle]
pub unsafe extern "C" fn PlatWindowPollEvent(out: *mut PlatInputEvent) -> c_int {
    let mut e = SdlEvent::zeroed();
    while SDL_PollEvent(&mut e) {
        if translate(&e, &SdlHost, &mut *out) {
            return 1;
        }
    }
    0
}

#[no_mangle]
pub unsafe extern "C" fn PlatWindowWaitEvent(out: *mut PlatInputEvent, timeout_ms: u32) -> c_int {
    let start = PlatTimeMilliseconds();
    loop {
        let mut e = SdlEvent::zeroed();
        let left = match wait_timeout(timeout_ms, start, PlatTimeMilliseconds()) {
            None => return PlatWindowPollEvent(out),
            Some(left) => left,
        };
        if !SDL_WaitEventTimeout(&mut e, left) {
            return 0;
        }
        if translate(&e, &SdlHost, &mut *out) {
            return 1;
        }
    }
}

unsafe fn keyboard_state() -> *const bool {
    SDL_GetKeyboardState(null_mut())
}

#[no_mangle]
pub unsafe extern "C" fn PlatScanCodeIsDown(scan_code: u32) -> c_int {
    let state = keyboard_state();
    keys::scan_code_is_down(scan_code, &|s| *state.add(s as usize))
}

#[no_mangle]
pub unsafe extern "C" fn PlatKeyIsDown(virtual_key: u32) -> c_int {
    let state = keyboard_state();
    let buttons = SDL_GetMouseState(null_mut(), null_mut());
    keys::key_is_down(virtual_key, &|s| *state.add(s as usize), buttons, &|k| SDL_GetScancodeFromKey(k, null_mut()))
}

#[no_mangle]
pub unsafe extern "C" fn PlatKeyIsToggled(virtual_key: u32) -> c_int {
    keys::key_is_toggled(virtual_key, SDL_GetModState())
}

#[no_mangle]
pub unsafe extern "C" fn PlatModifiers() -> u32 {
    keys::modifiers_of(SDL_GetModState())
}

#[no_mangle]
pub unsafe extern "C" fn PlatMouseGetState(window: *mut PlatWindow, out_x: *mut i32, out_y: *mut i32) -> u32 {
    let (mut x, mut y) = (0.0f32, 0.0f32);
    let buttons = SDL_GetGlobalMouseState(&mut x, &mut y);
    if !window.is_null() {
        let (mut wx, mut wy) = (0, 0);
        SDL_GetWindowPosition((*window).window, &mut wx, &mut wy);
        let scale = SDL_GetWindowPixelDensity((*window).window);
        let (cx, cy) = global_to_client(x, y, wx, wy, scale);
        x = cx;
        y = cy;
    }
    if !out_x.is_null() {
        *out_x = x as i32;
    }
    if !out_y.is_null() {
        *out_y = y as i32;
    }
    mouse_buttons(buttons)
}

#[no_mangle]
pub unsafe extern "C" fn PlatMouseSetRelative(window: *mut PlatWindow, relative: c_int) -> c_int {
    c_int::from(SDL_SetWindowRelativeMouseMode((*window).window, relative != 0))
}

#[no_mangle]
pub unsafe extern "C" fn PlatMouseWarp(window: *mut PlatWindow, x: i32, y: i32) {
    let (wx, wy) = warp_target(x, y, SDL_GetWindowPixelDensity((*window).window));
    SDL_WarpMouseInWindow((*window).window, wx, wy);
}

#[no_mangle]
pub unsafe extern "C" fn PlatCursorShow(show: c_int) {
    if show != 0 {
        SDL_ShowCursor();
    } else {
        SDL_HideCursor();
    }
}

#[no_mangle]
pub unsafe extern "C" fn PlatMouseConfine(window: *mut PlatWindow, confine: c_int) {
    SDL_SetWindowMouseGrab((*window).window, confine != 0);
}

#[no_mangle]
pub unsafe extern "C" fn PlatClipboardSetText(text: *const c_char) -> c_int {
    let t = utf8_c(text);
    c_int::from(SDL_SetClipboardText(t.as_ptr().cast()))
}

#[no_mangle]
pub unsafe extern "C" fn PlatClipboardGetText() -> *mut c_char {
    if !SDL_HasClipboardText() {
        return null_mut();
    }
    let text = SDL_GetClipboardText();
    if text.is_null() {
        return null_mut();
    }
    // SDL's clipboard text is UTF-8, as the game's (decision D6): a copy the caller frees
    let bytes = c_terminated(CStr::from_ptr(text).to_bytes());
    let result = malloc(bytes.len()) as *mut u8;
    if !result.is_null() {
        std::ptr::copy_nonoverlapping(bytes.as_ptr(), result, bytes.len());
    }
    SDL_free(text.cast());
    result.cast()
}

#[no_mangle]
pub unsafe extern "C" fn PlatTextInputStart(window: *mut PlatWindow, x: i32, y: i32, width: i32, height: i32) {
    let area = SdlRect { x, y, w: width, h: height };
    SDL_SetTextInputArea((*window).window, &area, 0);
    SDL_StartTextInput((*window).window);
}

#[no_mangle]
pub unsafe extern "C" fn PlatTextInputStop(window: *mut PlatWindow) {
    SDL_StopTextInput((*window).window);
}

#[no_mangle]
pub unsafe extern "C" fn PlatTextInputCancel(window: *mut PlatWindow) {
    if !window.is_null() {
        SDL_ClearComposition((*window).window);
    }
}

#[no_mangle]
pub unsafe extern "C" fn PlatWindowWake() {
    let event_type = WAKE_EVENT.load(Ordering::SeqCst);
    if event_type == 0 {
        return;
    }
    let mut e = SdlEvent::zeroed();
    e.event_type = event_type;
    SDL_PushEvent(&mut e); // thread-safe
}

// the window system's thread only, as the C++'s plain table
static CURSORS: [AtomicPtr<SdlCursor>; 6] = [const { AtomicPtr::new(null_mut()) }; 6];

#[no_mangle]
pub unsafe extern "C" fn PlatCursorSetSystem(cursor: u32) {
    let i = system_cursor_index(cursor);
    let mut c = CURSORS[i].load(Ordering::Relaxed);
    if c.is_null() {
        c = SDL_CreateSystemCursor(SYSTEM_CURSORS[i]);
        CURSORS[i].store(c, Ordering::Relaxed);
    }
    if !c.is_null() {
        SDL_SetCursor(c);
    }
}

#[no_mangle]
pub unsafe extern "C" fn PlatCursorCreate(
    bgra: *const u32,
    width: c_int,
    height: c_int,
    hot_x: c_int,
    hot_y: c_int,
) -> *mut c_void {
    if bgra.is_null() || width <= 0 || height <= 0 {
        return null_mut();
    }
    // ARGB8888 is a packed format: each pixel one 0xAARRGGBB value, as the platform interface has them
    let surface = SDL_CreateSurfaceFrom(width, height, PIXELFORMAT_ARGB8888, bgra as *mut c_void, width * 4);
    if surface.is_null() {
        set_last_error_sdl();
        return null_mut();
    }
    let cursor = SDL_CreateColorCursor(surface, hot_x, hot_y); // copies the pixels
    SDL_DestroySurface(surface);
    if cursor.is_null() {
        set_last_error_sdl();
    }
    cursor.cast()
}

#[no_mangle]
pub unsafe extern "C" fn PlatCursorSet(cursor: *mut c_void) {
    SDL_SetCursor(if cursor.is_null() { SDL_GetDefaultCursor() } else { cursor as *mut SdlCursor });
}

#[no_mangle]
pub unsafe extern "C" fn PlatCursorDestroy(cursor: *mut c_void) {
    if !cursor.is_null() {
        SDL_DestroyCursor(cursor as *mut SdlCursor);
    }
}

#[no_mangle]
pub unsafe extern "C" fn PlatOpenURL(url: *const c_char) -> c_int {
    if url.is_null() {
        set_last_error(b"no URL");
        return 0;
    }
    if !SDL_OpenURL(url) {
        set_last_error_sdl();
        return 0;
    }
    1
}
