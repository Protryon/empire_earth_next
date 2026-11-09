//! SDL events and `PlatInputEvent`: the `repr(C)` layouts (SDL3 3.4's and platform/PlatWindow.h's; the golden layout
//! table checks them against the C compilers' `offsetof`) and the translation, with the SDL calls behind [`Host`].

use std::ffi::{c_char, c_void, CStr};

use crate::cp1252::to_cp1252;
use crate::keys::{find_scan_code, modifiers_of, virtual_key_of};

pub const SDL_EVENT_QUIT: u32 = 0x100;
pub const SDL_EVENT_WINDOW_EXPOSED: u32 = 0x204;
pub const SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED: u32 = 0x207;
pub const SDL_EVENT_WINDOW_MINIMIZED: u32 = 0x209;
pub const SDL_EVENT_WINDOW_RESTORED: u32 = 0x20B;
pub const SDL_EVENT_WINDOW_FOCUS_GAINED: u32 = 0x20E;
pub const SDL_EVENT_WINDOW_FOCUS_LOST: u32 = 0x20F;
pub const SDL_EVENT_WINDOW_CLOSE_REQUESTED: u32 = 0x210;
pub const SDL_EVENT_KEY_DOWN: u32 = 0x300;
pub const SDL_EVENT_KEY_UP: u32 = 0x301;
pub const SDL_EVENT_TEXT_EDITING: u32 = 0x302;
pub const SDL_EVENT_TEXT_INPUT: u32 = 0x303;
pub const SDL_EVENT_MOUSE_MOTION: u32 = 0x400;
pub const SDL_EVENT_MOUSE_BUTTON_DOWN: u32 = 0x401;
pub const SDL_EVENT_MOUSE_BUTTON_UP: u32 = 0x402;
pub const SDL_EVENT_MOUSE_WHEEL: u32 = 0x403;
pub const SDL_MOUSEWHEEL_FLIPPED: i32 = 1;

#[repr(C)]
#[derive(Clone, Copy)]
pub struct SdlWindowEvent {
    pub event_type: u32,
    pub reserved: u32,
    pub timestamp: u64,
    pub window_id: u32,
    pub data1: i32,
    pub data2: i32,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct SdlKeyboardEvent {
    pub event_type: u32,
    pub reserved: u32,
    pub timestamp: u64,
    pub window_id: u32,
    pub which: u32,
    pub scancode: i32,
    pub key: u32,
    pub modifiers: u16,
    pub raw: u16,
    pub down: bool,
    pub repeat: bool,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct SdlTextEditingEvent {
    pub event_type: u32,
    pub reserved: u32,
    pub timestamp: u64,
    pub window_id: u32,
    pub text: *const c_char,
    pub start: i32,
    pub length: i32,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct SdlTextInputEvent {
    pub event_type: u32,
    pub reserved: u32,
    pub timestamp: u64,
    pub window_id: u32,
    pub text: *const c_char,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct SdlMouseMotionEvent {
    pub event_type: u32,
    pub reserved: u32,
    pub timestamp: u64,
    pub window_id: u32,
    pub which: u32,
    pub state: u32,
    pub x: f32,
    pub y: f32,
    pub xrel: f32,
    pub yrel: f32,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct SdlMouseButtonEvent {
    pub event_type: u32,
    pub reserved: u32,
    pub timestamp: u64,
    pub window_id: u32,
    pub which: u32,
    pub button: u8,
    pub down: bool,
    pub clicks: u8,
    pub padding: u8,
    pub x: f32,
    pub y: f32,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct SdlMouseWheelEvent {
    pub event_type: u32,
    pub reserved: u32,
    pub timestamp: u64,
    pub window_id: u32,
    pub which: u32,
    pub x: f32,
    pub y: f32,
    pub direction: i32,
    pub mouse_x: f32,
    pub mouse_y: f32,
    pub integer_x: i32,
    pub integer_y: i32,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct SdlUserEvent {
    pub event_type: u32,
    pub reserved: u32,
    pub timestamp: u64,
    pub window_id: u32,
    pub code: i32,
    pub data1: *mut c_void,
    pub data2: *mut c_void,
}

/// `SDL_Event`
#[repr(C)]
#[derive(Clone, Copy)]
pub union SdlEvent {
    pub event_type: u32,
    pub window: SdlWindowEvent,
    pub key: SdlKeyboardEvent,
    pub edit: SdlTextEditingEvent,
    pub text: SdlTextInputEvent,
    pub motion: SdlMouseMotionEvent,
    pub button: SdlMouseButtonEvent,
    pub wheel: SdlMouseWheelEvent,
    pub user: SdlUserEvent,
    pub padding: [u8; 128],
}

impl SdlEvent {
    /// An all-zero event (`SDL_zero`).
    pub fn zeroed() -> Self {
        SdlEvent { padding: [0; 128] }
    }
}

/// `SDL_Rect`
#[repr(C)]
#[derive(Clone, Copy)]
pub struct SdlRect {
    pub x: i32,
    pub y: i32,
    pub w: i32,
    pub h: i32,
}

/// `SDL_DisplayMode`
#[repr(C)]
#[derive(Clone, Copy)]
pub struct SdlDisplayMode {
    pub display_id: u32,
    pub format: u32,
    pub w: i32,
    pub h: i32,
    pub pixel_density: f32,
    pub refresh_rate: f32,
    pub refresh_rate_numerator: i32,
    pub refresh_rate_denominator: i32,
    pub internal: *mut c_void,
}

pub const PLAT_EVENT_NONE: u32 = 0;
pub const PLAT_EVENT_QUIT: u32 = 1;
pub const PLAT_EVENT_CLOSE: u32 = 2;
pub const PLAT_EVENT_RESIZED: u32 = 3;
pub const PLAT_EVENT_FOCUS_GAINED: u32 = 4;
pub const PLAT_EVENT_FOCUS_LOST: u32 = 5;
pub const PLAT_EVENT_MINIMIZED: u32 = 6;
pub const PLAT_EVENT_RESTORED: u32 = 7;
pub const PLAT_EVENT_EXPOSED: u32 = 8;
pub const PLAT_EVENT_KEY_DOWN: u32 = 9;
pub const PLAT_EVENT_KEY_UP: u32 = 10;
pub const PLAT_EVENT_TEXT: u32 = 11;
pub const PLAT_EVENT_TEXT_EDITING: u32 = 12;
pub const PLAT_EVENT_MOUSE_MOVE: u32 = 13;
pub const PLAT_EVENT_MOUSE_DOWN: u32 = 14;
pub const PLAT_EVENT_MOUSE_UP: u32 = 15;
pub const PLAT_EVENT_MOUSE_WHEEL: u32 = 16;
pub const PLAT_EVENT_WAKE: u32 = 17;

#[repr(C)]
#[derive(Clone, Copy)]
pub struct PlatKeyData {
    pub scan_code: u32,
    pub virtual_key: u32,
    pub modifiers: u32,
    pub repeat: i32,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct PlatMouseData {
    pub x: i32,
    pub y: i32,
    pub delta_x: i32,
    pub delta_y: i32,
    pub button: u32,
    pub clicks: u32,
    pub wheel: f32,
    pub modifiers: u32,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct PlatTextData {
    pub text: [u8; 64],
    pub cursor: i32,
    pub length: i32,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct PlatSizeData {
    pub width: i32,
    pub height: i32,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub union PlatEventData {
    pub key: PlatKeyData,
    pub mouse: PlatMouseData,
    pub text: PlatTextData,
    pub size: PlatSizeData,
}

/// `PlatInputEvent`
#[repr(C)]
#[derive(Clone, Copy)]
pub struct PlatInputEvent {
    pub event_type: u32,
    pub time: u32,
    pub window: *mut c_void,
    pub data: PlatEventData,
}

impl PlatInputEvent {
    /// The C's `memset(out, 0, sizeof(*out))`.
    pub fn zeroed() -> Self {
        // SAFETY: all fields are integers, floats, a pointer, or their arrays; zero is valid for each
        unsafe { std::mem::zeroed() }
    }
}

/// What translation needs from SDL (the real one is `ee_plat_sdl`, compiled out of `cargo test`).
pub trait Host {
    /// `WindowOf`: the PlatWindow of an SDL window id, NULL when none.
    fn window_of(&self, id: u32) -> *mut c_void;
    fn mod_state(&self) -> u16;
    fn time_ms(&self) -> u32;
    /// The registered event numbers, 0 before they are registered.
    fn wake_event(&self) -> u32;
    fn activate_event(&self) -> u32;
    /// Windows: focus comes from the subclass's activation messages, SDL's focus events are dropped.
    fn focus_from_activation(&self) -> bool;
    /// Called for a window event once its window is known (macOS re-enters full screen on focus here).
    fn window_event(&self, _sdl_event_type: u32, _window: *mut c_void) {}
}

/// The C++ `Translate`: `false` for an event the game does not see.
///
/// # Safety
/// `e` holds a valid SDL event of its `event_type` (a text event's pointer is live and terminated).
pub unsafe fn translate(e: &SdlEvent, host: &impl Host, out: &mut PlatInputEvent) -> bool {
    *out = PlatInputEvent::zeroed();
    out.time = host.time_ms();
    let t = e.event_type;
    match t {
        SDL_EVENT_QUIT => {
            out.event_type = PLAT_EVENT_QUIT;
            true
        }
        SDL_EVENT_WINDOW_FOCUS_GAINED | SDL_EVENT_WINDOW_FOCUS_LOST if host.focus_from_activation() => false,
        SDL_EVENT_WINDOW_CLOSE_REQUESTED
        | SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED
        | SDL_EVENT_WINDOW_FOCUS_GAINED
        | SDL_EVENT_WINDOW_FOCUS_LOST
        | SDL_EVENT_WINDOW_MINIMIZED
        | SDL_EVENT_WINDOW_RESTORED
        | SDL_EVENT_WINDOW_EXPOSED => {
            out.window = host.window_of(e.window.window_id);
            host.window_event(t, out.window);
            out.event_type = match t {
                SDL_EVENT_WINDOW_CLOSE_REQUESTED => PLAT_EVENT_CLOSE,
                SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED => PLAT_EVENT_RESIZED,
                SDL_EVENT_WINDOW_FOCUS_GAINED => PLAT_EVENT_FOCUS_GAINED,
                SDL_EVENT_WINDOW_FOCUS_LOST => PLAT_EVENT_FOCUS_LOST,
                SDL_EVENT_WINDOW_MINIMIZED => PLAT_EVENT_MINIMIZED,
                SDL_EVENT_WINDOW_RESTORED => PLAT_EVENT_RESTORED,
                _ => PLAT_EVENT_EXPOSED,
            };
            out.data.size = PlatSizeData { width: e.window.data1, height: e.window.data2 };
            true
        }
        SDL_EVENT_KEY_DOWN | SDL_EVENT_KEY_UP => {
            out.event_type = if t == SDL_EVENT_KEY_DOWN { PLAT_EVENT_KEY_DOWN } else { PLAT_EVENT_KEY_UP };
            out.window = host.window_of(e.key.window_id);
            let row = find_scan_code(e.key.scancode);
            out.data.key = PlatKeyData {
                scan_code: row.map_or(0, |r| r.dik as u32),
                virtual_key: virtual_key_of(e.key.scancode, e.key.key),
                modifiers: modifiers_of(e.key.modifiers),
                repeat: i32::from(e.key.repeat),
            };
            true
        }
        SDL_EVENT_TEXT_INPUT => {
            out.event_type = PLAT_EVENT_TEXT;
            out.window = host.window_of(e.text.window_id);
            let bytes = CStr::from_ptr(e.text.text).to_bytes();
            to_cp1252(bytes, &mut out.data.text.text);
            true
        }
        SDL_EVENT_TEXT_EDITING => {
            out.event_type = PLAT_EVENT_TEXT_EDITING;
            out.window = host.window_of(e.edit.window_id);
            let bytes = CStr::from_ptr(e.edit.text).to_bytes();
            to_cp1252(bytes, &mut out.data.text.text);
            out.data.text.cursor = e.edit.start;
            out.data.text.length = e.edit.length;
            true
        }
        SDL_EVENT_MOUSE_MOTION => {
            out.event_type = PLAT_EVENT_MOUSE_MOVE;
            out.window = host.window_of(e.motion.window_id);
            out.data.mouse.x = e.motion.x as i32;
            out.data.mouse.y = e.motion.y as i32;
            out.data.mouse.delta_x = e.motion.xrel as i32;
            out.data.mouse.delta_y = e.motion.yrel as i32;
            out.data.mouse.modifiers = modifiers_of(host.mod_state());
            true
        }
        SDL_EVENT_MOUSE_BUTTON_DOWN | SDL_EVENT_MOUSE_BUTTON_UP => {
            out.event_type = if t == SDL_EVENT_MOUSE_BUTTON_DOWN { PLAT_EVENT_MOUSE_DOWN } else { PLAT_EVENT_MOUSE_UP };
            out.window = host.window_of(e.button.window_id);
            out.data.mouse.x = e.button.x as i32;
            out.data.mouse.y = e.button.y as i32;
            out.data.mouse.button = e.button.button as u32;
            out.data.mouse.clicks = e.button.clicks as u32;
            out.data.mouse.modifiers = modifiers_of(host.mod_state());
            true
        }
        SDL_EVENT_MOUSE_WHEEL => {
            out.event_type = PLAT_EVENT_MOUSE_WHEEL;
            out.window = host.window_of(e.wheel.window_id);
            out.data.mouse.wheel = if e.wheel.direction == SDL_MOUSEWHEEL_FLIPPED { -e.wheel.y } else { e.wheel.y };
            out.data.mouse.x = e.wheel.mouse_x as i32;
            out.data.mouse.y = e.wheel.mouse_y as i32;
            out.data.mouse.modifiers = modifiers_of(host.mod_state());
            true
        }
        _ => {
            if host.wake_event() != 0 && t == host.wake_event() {
                out.event_type = PLAT_EVENT_WAKE;
                return true;
            }
            if host.activate_event() != 0 && t == host.activate_event() {
                out.event_type = if e.user.code != 0 { PLAT_EVENT_FOCUS_GAINED } else { PLAT_EVENT_FOCUS_LOST };
                out.window = host.window_of(e.user.window_id);
                return true;
            }
            false
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::cell::{Cell, RefCell};
    use std::ffi::CString;
    use std::mem::{offset_of, size_of};

    const GOLDEN: &str = include_str!("golden_window.txt");

    struct Fake {
        modifiers: Cell<u16>,
        win32: bool,
        events: RefCell<Vec<(u32, usize)>>,
    }

    impl Host for Fake {
        fn window_of(&self, id: u32) -> *mut c_void {
            id as usize as *mut c_void
        }
        fn mod_state(&self) -> u16 {
            self.modifiers.get()
        }
        fn time_ms(&self) -> u32 {
            777
        }
        fn wake_event(&self) -> u32 {
            0x8001
        }
        fn activate_event(&self) -> u32 {
            if self.win32 {
                0x8002
            } else {
                0
            }
        }
        fn focus_from_activation(&self) -> bool {
            self.win32
        }
        fn window_event(&self, t: u32, w: *mut c_void) {
            self.events.borrow_mut().push((t, w as usize));
        }
    }

    fn fake(win32: bool) -> Fake {
        Fake { modifiers: Cell::new(0), win32, events: RefCell::new(Vec::new()) }
    }

    fn unhex(s: &str) -> Vec<u8> {
        let h = s.strip_prefix('x').unwrap_or(s);
        (0..h.len()).step_by(2).map(|i| u8::from_str_radix(&h[i..i + 2], 16).unwrap()).collect()
    }

    fn text_of(data: &[u8]) -> Vec<u8> {
        data.iter().take_while(|&&b| b != 0).copied().collect()
    }

    #[test]
    fn sdl_event_layout_matches_the_c_compilers() {
        let offset = |name: &str| -> usize {
            GOLDEN
                .lines()
                .filter_map(|l| l.strip_prefix("ol "))
                .find_map(|l| l.strip_prefix(name).map(|r| r.trim().parse().unwrap()))
                .unwrap_or_else(|| panic!("no {name}"))
        };
        assert_eq!(size_of::<SdlEvent>(), offset("sizeof_SDL_Event "));
        assert_eq!(0, offset("type "));
        assert_eq!(offset_of!(SdlKeyboardEvent, window_id), offset("key.windowID "));
        assert_eq!(offset_of!(SdlKeyboardEvent, scancode), offset("key.scancode "));
        assert_eq!(offset_of!(SdlKeyboardEvent, key), offset("key.key "));
        assert_eq!(offset_of!(SdlKeyboardEvent, modifiers), offset("key.mod "));
        assert_eq!(offset_of!(SdlKeyboardEvent, repeat), offset("key.repeat "));
        assert_eq!(offset_of!(SdlTextInputEvent, window_id), offset("text.windowID "));
        assert_eq!(offset_of!(SdlTextInputEvent, text), offset("text.text "));
        assert_eq!(offset_of!(SdlTextEditingEvent, window_id), offset("edit.windowID "));
        assert_eq!(offset_of!(SdlTextEditingEvent, text), offset("edit.text "));
        assert_eq!(offset_of!(SdlTextEditingEvent, start), offset("edit.start "));
        assert_eq!(offset_of!(SdlTextEditingEvent, length), offset("edit.length "));
        assert_eq!(offset_of!(SdlMouseMotionEvent, window_id), offset("motion.windowID "));
        assert_eq!(offset_of!(SdlMouseMotionEvent, x), offset("motion.x "));
        assert_eq!(offset_of!(SdlMouseMotionEvent, y), offset("motion.y "));
        assert_eq!(offset_of!(SdlMouseMotionEvent, xrel), offset("motion.xrel "));
        assert_eq!(offset_of!(SdlMouseMotionEvent, yrel), offset("motion.yrel "));
        assert_eq!(offset_of!(SdlMouseButtonEvent, window_id), offset("button.windowID "));
        assert_eq!(offset_of!(SdlMouseButtonEvent, button), offset("button.button "));
        assert_eq!(offset_of!(SdlMouseButtonEvent, clicks), offset("button.clicks "));
        assert_eq!(offset_of!(SdlMouseButtonEvent, x), offset("button.x "));
        assert_eq!(offset_of!(SdlMouseButtonEvent, y), offset("button.y "));
        assert_eq!(offset_of!(SdlMouseWheelEvent, window_id), offset("wheel.windowID "));
        assert_eq!(offset_of!(SdlMouseWheelEvent, y), offset("wheel.y "));
        assert_eq!(offset_of!(SdlMouseWheelEvent, direction), offset("wheel.direction "));
        assert_eq!(offset_of!(SdlMouseWheelEvent, mouse_x), offset("wheel.mouse_x "));
        assert_eq!(offset_of!(SdlMouseWheelEvent, mouse_y), offset("wheel.mouse_y "));
        assert_eq!(offset_of!(SdlWindowEvent, window_id), offset("window.windowID "));
        assert_eq!(offset_of!(SdlWindowEvent, data1), offset("window.data1 "));
        assert_eq!(offset_of!(SdlWindowEvent, data2), offset("window.data2 "));
        assert_eq!(offset_of!(SdlUserEvent, window_id), offset("user.windowID "));
        assert_eq!(offset_of!(SdlUserEvent, code), offset("user.code "));
        assert_eq!(size_of::<SdlDisplayMode>(), offset("sizeof_SDL_DisplayMode "));
        assert_eq!(offset_of!(SdlDisplayMode, w), offset("DisplayMode.w "));
        assert_eq!(offset_of!(SdlDisplayMode, h), offset("DisplayMode.h "));
        assert_eq!(offset_of!(SdlDisplayMode, pixel_density), offset("DisplayMode.pixel_density "));
        assert_eq!(size_of::<SdlRect>(), offset("sizeof_SDL_Rect "));
    }

    #[test]
    fn plat_event_layout_matches_the_c_header() {
        let offset = |name: &str| -> usize {
            GOLDEN
                .lines()
                .filter_map(|l| l.strip_prefix("pl ").or_else(|| l.strip_prefix("ol ")))
                .find_map(|l| l.strip_prefix(name).map(|r| r.trim().parse().unwrap()))
                .unwrap_or_else(|| panic!("no {name}"))
        };
        assert_eq!(size_of::<PlatInputEvent>(), offset("sizeof_PlatInputEvent "));
        assert_eq!(offset_of!(PlatInputEvent, event_type), offset("mType "));
        assert_eq!(offset_of!(PlatInputEvent, time), offset("mTime "));
        assert_eq!(offset_of!(PlatInputEvent, window), offset("mWindow "));
        let data = offset_of!(PlatInputEvent, data);
        assert_eq!(data + offset_of!(PlatKeyData, scan_code), offset("mKey.mScanCode "));
        assert_eq!(data + offset_of!(PlatKeyData, virtual_key), offset("mKey.mVirtualKey "));
        assert_eq!(data + offset_of!(PlatKeyData, modifiers), offset("mKey.mModifiers "));
        assert_eq!(data + offset_of!(PlatKeyData, repeat), offset("mKey.mRepeat "));
        assert_eq!(data + offset_of!(PlatMouseData, x), offset("mMouse.mX "));
        assert_eq!(data + offset_of!(PlatMouseData, y), offset("mMouse.mY "));
        assert_eq!(data + offset_of!(PlatMouseData, delta_x), offset("mMouse.mDeltaX "));
        assert_eq!(data + offset_of!(PlatMouseData, delta_y), offset("mMouse.mDeltaY "));
        assert_eq!(data + offset_of!(PlatMouseData, button), offset("mMouse.mButton "));
        assert_eq!(data + offset_of!(PlatMouseData, clicks), offset("mMouse.mClicks "));
        assert_eq!(data + offset_of!(PlatMouseData, wheel), offset("mMouse.mWheel "));
        assert_eq!(data + offset_of!(PlatMouseData, modifiers), offset("mMouse.mModifiers "));
        assert_eq!(data + offset_of!(PlatTextData, text), offset("mText.mText "));
        assert_eq!(data + offset_of!(PlatTextData, cursor), offset("mText.mCursor "));
        assert_eq!(data + offset_of!(PlatTextData, length), offset("mText.mLength "));
        assert_eq!(data + offset_of!(PlatSizeData, width), offset("mSize.mWidth "));
        assert_eq!(data + offset_of!(PlatSizeData, height), offset("mSize.mHeight "));
    }

    #[test]
    fn translation_matches_the_cpp_on_stub_sdl() {
        let mut compared = 0;
        for line in GOLDEN.lines().filter(|l| l.starts_with("ev ")) {
            let p: Vec<&str> = line.split_whitespace().collect();
            // ev TYPE w WIN args A B C D X Y XR YR mod M text TEXT => R ...
            let event_type = u32::from_str_radix(p[1], 16).unwrap();
            let win: u32 = p[3].parse().unwrap();
            let a: i32 = p[5].parse().unwrap();
            let b: i32 = p[6].parse().unwrap();
            let c: i32 = p[7].parse().unwrap();
            let (x, y, xr, yr): (f32, f32, f32, f32) =
                (p[9].parse().unwrap(), p[10].parse().unwrap(), p[11].parse().unwrap(), p[12].parse().unwrap());
            let m: u16 = p[14].parse().unwrap();
            let text = if p[16] == "-" { None } else { Some(CString::new(unhex(p[16])).unwrap()) };
            let mut e = SdlEvent::zeroed();
            e.event_type = event_type;
            let tp = text.as_ref().map_or(std::ptr::null(), |t| t.as_ptr());
            {
                match event_type {
                    SDL_EVENT_WINDOW_CLOSE_REQUESTED
                    | SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED
                    | SDL_EVENT_WINDOW_FOCUS_GAINED
                    | SDL_EVENT_WINDOW_FOCUS_LOST
                    | SDL_EVENT_WINDOW_MINIMIZED
                    | SDL_EVENT_WINDOW_RESTORED
                    | SDL_EVENT_WINDOW_EXPOSED => {
                        e.window.window_id = win;
                        e.window.data1 = a;
                        e.window.data2 = b;
                    }
                    SDL_EVENT_KEY_DOWN | SDL_EVENT_KEY_UP => {
                        e.key.window_id = win;
                        e.key.scancode = a;
                        e.key.key = b as u32;
                        e.key.repeat = c != 0;
                        e.key.modifiers = m;
                    }
                    SDL_EVENT_TEXT_INPUT => {
                        e.text.window_id = win;
                        e.text.text = tp;
                    }
                    SDL_EVENT_TEXT_EDITING => {
                        e.edit.window_id = win;
                        e.edit.text = tp;
                        e.edit.start = a;
                        e.edit.length = b;
                    }
                    SDL_EVENT_MOUSE_MOTION => {
                        e.motion.window_id = win;
                        e.motion.x = x;
                        e.motion.y = y;
                        e.motion.xrel = xr;
                        e.motion.yrel = yr;
                    }
                    SDL_EVENT_MOUSE_BUTTON_DOWN | SDL_EVENT_MOUSE_BUTTON_UP => {
                        e.button.window_id = win;
                        e.button.button = a as u8;
                        e.button.clicks = b as u8;
                        e.button.x = x;
                        e.button.y = y;
                    }
                    SDL_EVENT_MOUSE_WHEEL => {
                        e.wheel.window_id = win;
                        e.wheel.direction = a;
                        e.wheel.y = y;
                        e.wheel.mouse_x = xr;
                        e.wheel.mouse_y = yr;
                    }
                    _ => {
                        e.user.window_id = win;
                        e.user.code = a;
                    }
                }
            }
            let host = fake(true);
            host.modifiers.set(m);
            let mut out = PlatInputEvent::zeroed();
            let r = unsafe { translate(&e, &host, &mut out) };
            let expected_r: i32 = p[18].parse().unwrap();
            assert_eq!(i32::from(r), expected_r, "{line}");
            if r {
                let num = |i: usize| -> i64 { p[i].parse().unwrap() };
                assert_eq!(p[19], "t", "{line}");
                assert_eq!(out.event_type as i64, num(20), "type {line}");
                assert_eq!(out.time as i64, num(21), "time {line}");
                assert_eq!(out.window as i64, num(23), "win {line}");
                unsafe {
                    assert_eq!(out.data.key.scan_code as i64, num(25), "{line}");
                    assert_eq!(out.data.key.virtual_key as i64, num(26), "{line}");
                    assert_eq!(out.data.key.modifiers as i64, num(27), "{line}");
                    assert_eq!(out.data.key.repeat as i64, num(28), "{line}");
                    assert_eq!(out.data.mouse.x as i64, num(30), "{line}");
                    assert_eq!(out.data.mouse.y as i64, num(31), "{line}");
                    assert_eq!(out.data.mouse.delta_x as i64, num(32), "{line}");
                    assert_eq!(out.data.mouse.delta_y as i64, num(33), "{line}");
                    assert_eq!(out.data.mouse.button as i64, num(34), "{line}");
                    assert_eq!(out.data.mouse.clicks as i64, num(35), "{line}");
                    let wheel: f32 = p[36].parse().unwrap();
                    assert_eq!(out.data.mouse.wheel, wheel, "{line}");
                    assert_eq!(out.data.mouse.modifiers as i64, num(37), "{line}");
                    assert_eq!(text_of(&out.data.text.text), unhex(p[39]), "{line}");
                    assert_eq!(out.data.text.cursor as i64, num(40), "{line}");
                    assert_eq!(out.data.text.length as i64, num(41), "{line}");
                    assert_eq!(out.data.size.width as i64, num(43), "{line}");
                    assert_eq!(out.data.size.height as i64, num(44), "{line}");
                }
            }
            compared += 1;
        }
        assert!(compared >= 20);
    }

    // POSIX: the focus events go straight through, no activation event exists; the golden run is the Windows branch.
    #[test]
    fn posix_focus_events_pass_through_and_call_the_hook() {
        let host = fake(false);
        let mut out = PlatInputEvent::zeroed();
        let mut e = SdlEvent::zeroed();
        e.window = SdlWindowEvent {
            event_type: SDL_EVENT_WINDOW_FOCUS_GAINED,
            reserved: 0,
            timestamp: 0,
            window_id: 3,
            data1: 0,
            data2: 0,
        };
        assert!(unsafe { translate(&e, &host, &mut out) });
        assert_eq!(out.event_type, PLAT_EVENT_FOCUS_GAINED);
        assert_eq!(out.window as usize, 3);
        e.window.event_type = SDL_EVENT_WINDOW_FOCUS_LOST;
        assert!(unsafe { translate(&e, &host, &mut out) });
        assert_eq!(out.event_type, PLAT_EVENT_FOCUS_LOST);
        assert_eq!(host.events.borrow().len(), 2);
        assert_eq!(host.events.borrow()[0], (SDL_EVENT_WINDOW_FOCUS_GAINED, 3));
        // the activation user event does nothing on POSIX (activate_event is 0)
        let mut u = SdlEvent::zeroed();
        u.event_type = 0x8002;
        assert!(!unsafe { translate(&u, &host, &mut out) });
    }

    #[test]
    fn unknown_events_are_refused_and_leave_the_output_zeroed() {
        let host = fake(true);
        let mut out = PlatInputEvent::zeroed();
        out.time = 1234;
        let mut e = SdlEvent::zeroed();
        e.event_type = 0x700;
        assert!(!unsafe { translate(&e, &host, &mut out) });
        assert_eq!(out.event_type, 0);
        assert_eq!(out.time, 777); // the C sets mTime before it looks at the event
    }

    #[test]
    fn the_wake_event_needs_registration() {
        let host = fake(false);
        struct NoWake;
        impl Host for NoWake {
            fn window_of(&self, _: u32) -> *mut c_void {
                std::ptr::null_mut()
            }
            fn mod_state(&self) -> u16 {
                0
            }
            fn time_ms(&self) -> u32 {
                0
            }
            fn wake_event(&self) -> u32 {
                0
            }
            fn activate_event(&self) -> u32 {
                0
            }
            fn focus_from_activation(&self) -> bool {
                false
            }
        }
        let mut out = PlatInputEvent::zeroed();
        let mut e = SdlEvent::zeroed();
        e.event_type = 0x8001;
        assert!(unsafe { translate(&e, &host, &mut out) });
        assert_eq!(out.event_type, PLAT_EVENT_WAKE);
        e.event_type = 0; // zero is not "the wake event" when it is unregistered
        assert!(!unsafe { translate(&e, &NoWake, &mut out) });
        assert_eq!(out.event_type, 0);
    }
}
