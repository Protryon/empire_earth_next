//! Hand-written imports of the SDL3 functions the window layer calls (each signature read from SDL 3.4's headers).
//! Resolved by the program's link line (`SDL3-static.lib` / `libSDL3.a`).

use std::ffi::{c_char, c_int, c_void};

use ee_plat_ui::event::{SdlDisplayMode, SdlEvent, SdlRect};

pub enum SdlWindow {}
pub enum SdlSurface {}
pub enum SdlCursor {}

extern "C" {
    pub fn SDL_SetHint(name: *const c_char, value: *const c_char) -> bool;
    pub fn SDL_InitSubSystem(flags: u32) -> bool;
    pub fn SDL_QuitSubSystem(flags: u32);
    pub fn SDL_GetError() -> *const c_char;
    pub fn SDL_RegisterEvents(count: c_int) -> u32;
    #[cfg(windows)]
    pub fn SDL_RegisterApp(name: *const c_char, style: u32, hinst: *mut c_void) -> bool;
    pub fn SDL_SetAppMetadata(name: *const c_char, version: *const c_char, identifier: *const c_char) -> bool;
    pub fn SDL_CreateProperties() -> u32;
    pub fn SDL_DestroyProperties(props: u32);
    pub fn SDL_SetStringProperty(props: u32, name: *const c_char, value: *const c_char) -> bool;
    pub fn SDL_SetNumberProperty(props: u32, name: *const c_char, value: i64) -> bool;
    pub fn SDL_CreateWindowWithProperties(props: u32) -> *mut SdlWindow;
    #[cfg(target_os = "macos")]
    pub fn SDL_SyncWindow(window: *mut SdlWindow) -> bool;
    pub fn SDL_GetWindowFlags(window: *mut SdlWindow) -> u64;
    pub fn SDL_SetWindowFullscreen(window: *mut SdlWindow, fullscreen: bool) -> bool;
    pub fn SDL_SetWindowFullscreenMode(window: *mut SdlWindow, mode: *const SdlDisplayMode) -> bool;
    pub fn SDL_GetWindowProperties(window: *mut SdlWindow) -> u32;
    pub fn SDL_SetPointerProperty(props: u32, name: *const c_char, value: *mut c_void) -> bool;
    pub fn SDL_GetPointerProperty(props: u32, name: *const c_char, default_value: *mut c_void) -> *mut c_void;
    #[cfg(all(unix, not(target_os = "macos")))]
    pub fn SDL_GetNumberProperty(props: u32, name: *const c_char, default_value: i64) -> i64;
    pub fn SDL_CreateSurfaceFrom(
        width: c_int,
        height: c_int,
        format: u32,
        pixels: *mut c_void,
        pitch: c_int,
    ) -> *mut SdlSurface;
    #[cfg(not(windows))]
    pub fn SDL_SetWindowIcon(window: *mut SdlWindow, icon: *mut SdlSurface) -> bool;
    pub fn SDL_DestroySurface(surface: *mut SdlSurface);
    #[cfg(target_os = "macos")]
    pub fn SDL_Metal_CreateView(window: *mut SdlWindow) -> *mut c_void;
    #[cfg(target_os = "macos")]
    pub fn SDL_Metal_GetLayer(view: *mut c_void) -> *mut c_void;
    #[cfg(target_os = "macos")]
    pub fn SDL_Metal_DestroyView(view: *mut c_void);
    pub fn SDL_DestroyWindow(window: *mut SdlWindow);
    pub fn SDL_SetWindowTitle(window: *mut SdlWindow, title: *const c_char) -> bool;
    pub fn SDL_ShowWindow(window: *mut SdlWindow) -> bool;
    pub fn SDL_HideWindow(window: *mut SdlWindow) -> bool;
    pub fn SDL_SetWindowSize(window: *mut SdlWindow, w: c_int, h: c_int) -> bool;
    pub fn SDL_GetWindowPosition(window: *mut SdlWindow, x: *mut c_int, y: *mut c_int) -> bool;
    pub fn SDL_SetWindowPosition(window: *mut SdlWindow, x: c_int, y: c_int) -> bool;
    pub fn SDL_MinimizeWindow(window: *mut SdlWindow) -> bool;
    pub fn SDL_RaiseWindow(window: *mut SdlWindow) -> bool;
    pub fn SDL_GetWindowSizeInPixels(window: *mut SdlWindow, w: *mut c_int, h: *mut c_int) -> bool;
    pub fn SDL_GetDisplayForWindow(window: *mut SdlWindow) -> u32;
    pub fn SDL_GetPrimaryDisplay() -> u32;
    pub fn SDL_GetDesktopDisplayMode(display: u32) -> *const SdlDisplayMode;
    #[cfg(all(unix, not(target_os = "macos")))]
    pub fn SDL_GetCurrentVideoDriver() -> *const c_char;
    pub fn SDL_PollEvent(event: *mut SdlEvent) -> bool;
    pub fn SDL_WaitEventTimeout(event: *mut SdlEvent, timeout_ms: i32) -> bool;
    pub fn SDL_PushEvent(event: *mut SdlEvent) -> bool;
    pub fn SDL_GetKeyboardState(numkeys: *mut c_int) -> *const bool;
    pub fn SDL_GetMouseState(x: *mut f32, y: *mut f32) -> u32;
    pub fn SDL_GetGlobalMouseState(x: *mut f32, y: *mut f32) -> u32;
    pub fn SDL_GetModState() -> u16;
    pub fn SDL_GetScancodeFromKey(key: u32, modstate: *mut u16) -> c_int;
    pub fn SDL_SetWindowRelativeMouseMode(window: *mut SdlWindow, enabled: bool) -> bool;
    pub fn SDL_GetWindowPixelDensity(window: *mut SdlWindow) -> f32;
    pub fn SDL_WarpMouseInWindow(window: *mut SdlWindow, x: f32, y: f32);
    pub fn SDL_ShowCursor() -> bool;
    pub fn SDL_HideCursor() -> bool;
    pub fn SDL_SetWindowMouseGrab(window: *mut SdlWindow, grabbed: bool) -> bool;
    pub fn SDL_SetClipboardText(text: *const c_char) -> bool;
    pub fn SDL_HasClipboardText() -> bool;
    pub fn SDL_GetClipboardText() -> *mut c_char;
    pub fn SDL_free(mem: *mut c_void);
    pub fn SDL_SetTextInputArea(window: *mut SdlWindow, rect: *const SdlRect, cursor: c_int) -> bool;
    pub fn SDL_StartTextInput(window: *mut SdlWindow) -> bool;
    pub fn SDL_StopTextInput(window: *mut SdlWindow) -> bool;
    pub fn SDL_ClearComposition(window: *mut SdlWindow) -> bool;
    pub fn SDL_GetWindowFromID(id: u32) -> *mut SdlWindow;
    #[cfg(windows)]
    pub fn SDL_GetWindowID(window: *mut SdlWindow) -> u32;
    pub fn SDL_CreateSystemCursor(id: c_int) -> *mut SdlCursor;
    pub fn SDL_SetCursor(cursor: *mut SdlCursor) -> bool;
    pub fn SDL_GetDefaultCursor() -> *mut SdlCursor;
    pub fn SDL_CreateColorCursor(surface: *mut SdlSurface, hot_x: c_int, hot_y: c_int) -> *mut SdlCursor;
    pub fn SDL_DestroyCursor(cursor: *mut SdlCursor);
    pub fn SDL_OpenURL(url: *const c_char) -> bool;
    pub fn SDL_ShowSimpleMessageBox(
        flags: u32,
        title: *const c_char,
        message: *const c_char,
        window: *mut SdlWindow,
    ) -> bool;

    // the C runtime: PlatClipboardGetText's result is free()d by the caller
    pub fn malloc(size: usize) -> *mut c_void;
    // the rest of the platform layer (ee_plat_core)
    pub fn PlatTimeMilliseconds() -> u32;
    pub fn PlatMessageSetHandler(handler: Option<unsafe extern "C" fn(*const c_char, *const c_char, u32)>);
}
