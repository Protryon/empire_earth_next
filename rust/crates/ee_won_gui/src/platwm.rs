//! WONGUI's window manager on the platform layer as a whole Rust object (R6 WON round 40): the C++
//! `PlatWindowManager` (WONLobbyPlatform.h), the GDI backend's MSWindowManager and MSWindow's window procedure. The
//! lobby's windows are rectangles of one lobby surface the game shows in its own window; this object keeps:
//!
//!  - the surface's size and pixel scale (set from any thread: applied at once on the UI thread, otherwise at the
//!    next pass of the event loop), where the game shows it (the pointer is mapped back through it), the
//!    composited frame with its serial and lock (the game copies it out from its render thread), the wake count;
//!  - the windows in z order, the keyboard focus, the mouse capture, a framed window dragged by its caption, whether
//!    the game window has the system's focus, the pointer, the game window, the dirty flag, the event loop's last
//!    pass, the caption font;
//!  - the event translation (mouse to the capture window or the topmost window under the pointer, a press on a frame
//!    closing the popups and dragging by the caption; keys to the focus window with the control characters
//!    TranslateMessage made; text as character events; focus, close and window events), the drawing order, the
//!    hit test, the focus rules when a window goes, and the compositor (each framed window's classic frame and
//!    caption, then every visible window's offscreen image, into the frame).
//!
//! The C++ class keeps its base (WindowManager) and the platform event loop's wait; its methods are forwarders.
//! Every call on a window or the platform is one op of the host (`EeWonPlatWmOp`); a window's own fields are read
//! with `Info`. No borrow is held across a host call: a window's methods call back into this object (a window
//! taking the focus raises itself and moves the focus).

use std::cell::{Cell, RefCell};
use std::ffi::{c_char, c_void};
use std::sync::atomic::{AtomicI32, AtomicU32, Ordering};
use std::sync::{Condvar, Mutex};

use ee_base::plat_core::string::{PlatUtf8ToWide, WChar};
use ee_base::plat_ui::font::{
    PlatFont, PlatFontCreate, PlatFontDestroy, PlatFontDraw, PlatFontGetMetrics, PlatFontMetrics, PlatFontRect,
    PlatFontScaled, PlatFontTarget,
};

use crate::platfont::to_utf8;
use crate::platgfx::pixels;
use crate::surface::{EeWonSurface, EeWonSurfaceField as F};

/// The classic Windows colours (the "Windows Classic" scheme of the time) the colour scheme takes in place of
/// GetSysColor's, as 0xRRGGBB, in StandardColor order of `GetSystemColorScheme` (3DDarkShadow, 3DFace, 3DHilight,
/// 3DShadow, Scrollbar, ButtonText, GrayText, Hilight, HilightText, ToolTipBack, ToolTipText, MenuBack, MenuText,
/// Back, Text: the window frame's, as MSWindowManager did).
pub const CLASSIC: [u32; 15] = [
    0x404040, 0xD4D0C8, 0xFFFFFF, 0x808080, 0xD4D0C8, 0x000000, 0x808080, 0x0A246A, 0xFFFFFF, 0xFFFFE1, 0x000000,
    0xD4D0C8, 0x000000, 0xFFFFFF, 0x000000,
];
const DARK_SHADOW: u32 = 0x404040;
const FACE: u32 = 0xD4D0C8;
const HILIGHT: u32 = 0xFFFFFF;
const SHADOW: u32 = 0x808080;
const CAPTION_ACTIVE: u32 = 0x0A246A;
const CAPTION_INACTIVE: u32 = 0x808080;
/// The classic dialog frame: a 3-pixel border and an 18-pixel caption.
pub const FRAME_BORDER: i32 = 3;
pub const FRAME_CAPTION: i32 = 18;
/// The surface before SetSurfaceSize.
const DEFAULT_WIDTH: i32 = 800;
const DEFAULT_HEIGHT: i32 = 600;

// PlatInputEventType, PLAT_MOD_*, the VK_* the key translation reads
const EV_QUIT: u32 = 1;
const EV_CLOSE: u32 = 2;
const EV_RESIZED: u32 = 3;
const EV_FOCUS_GAINED: u32 = 4;
const EV_FOCUS_LOST: u32 = 5;
const EV_MINIMIZED: u32 = 6;
const EV_RESTORED: u32 = 7;
const EV_EXPOSED: u32 = 8;
const EV_KEY_DOWN: u32 = 9;
const EV_KEY_UP: u32 = 10;
const EV_TEXT: u32 = 11;
const EV_MOUSE_MOVE: u32 = 13;
const EV_MOUSE_DOWN: u32 = 14;
const EV_MOUSE_UP: u32 = 15;
const EV_MOUSE_WHEEL: u32 = 16;
const MOD_SHIFT: u32 = 0x1;
const MOD_CONTROL: u32 = 0x2;
const MOD_ALT: u32 = 0x4;
const VK_BACK: u32 = 0x08;
const VK_TAB: u32 = 0x09;
const VK_RETURN: u32 = 0x0D;
const VK_MENU: u32 = 0x12;
const VK_ESCAPE: u32 = 0x1B;
const VK_F10: u32 = 0x79;

/// `PlatInputEvent` (platform/PlatWindow.h): its header and its union's bytes.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct EeWonPlatEvent {
    pub kind: u32,
    pub time: u32,
    pub window: *mut c_void,
    /// The union: mKey (scan code, virtual key, modifiers, repeat), mMouse (x, y, dx, dy, button, clicks, wheel,
    /// modifiers), mText (64 bytes, cursor, length), mSize.
    pub data: [u32; 18],
}

impl EeWonPlatEvent {
    fn u(&self, i: usize) -> u32 {
        self.data[i]
    }
    fn i(&self, i: usize) -> i32 {
        self.data[i] as i32
    }
    /// mText with a terminator after its 64 bytes.
    fn text(&self) -> [u8; 65] {
        let mut t = [0u8; 65];
        for (k, w) in self.data[..16].iter().enumerate() {
            t[k * 4..k * 4 + 4].copy_from_slice(&w.to_ne_bytes());
        }
        t
    }
}

/// A window's fields the manager reads (`Info`).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeWonPlatWinInfo {
    /// The window rectangle in surface pixels (frame included).
    pub x: i32,
    pub y: i32,
    pub width: i32,
    pub height: i32,
    pub visible: bool,
    pub top_most: bool,
    pub registered: bool,
    /// `IsPopup()`.
    pub popup: bool,
    /// `FillsSurface()` (no parent).
    pub fills: bool,
    /// `HasFrame()`.
    pub frame: bool,
    /// `GetWindowState() == WindowState_Open`.
    pub open: bool,
    pub minimize_on_alt_tab: bool,
    pub forward_keys: bool,
}

/// `PlatGUIWindow::GetClientRect`: inside the frame and caption of a framed window (never negative), else the
/// window rectangle.
pub fn client_rect(i: &EeWonPlatWinInfo) -> [i32; 4] {
    if i.frame {
        [
            i.x + FRAME_BORDER,
            i.y + FRAME_BORDER + FRAME_CAPTION,
            (i.width - 2 * FRAME_BORDER).max(0),
            (i.height - 2 * FRAME_BORDER - FRAME_CAPTION).max(0),
        ]
    } else {
        [i.x, i.y, i.width, i.height]
    }
}

/// The host's calls (the receiver the C++ `PlatWindowManager*`; a window is a `PlatGUIWindow*`).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonPlatWmOp {
    /// `PlatCurrentThreadId() == WindowManager::GetThreadId()` (0 or 1).
    IsUiThread = 1,
    /// the window `a`'s fields into the `EeWonPlatWinInfo` at `b`.
    Info,
    /// `a->mRegistered = b != 0`.
    SetRegistered,
    /// `a->FollowSurface()`.
    FollowSurface,
    /// `a->FollowPixelScale()`.
    FollowPixelScale,
    /// `a->QueueEvent(new InputEvent(t))`, `t` GotFocus (b 0), LostFocus (1), MouseExit (2).
    QueueInput,
    /// `a->QueueEvent(new KeyEvent(t, c, d))`, `t` KeyDown (b 0), KeyUp (1), KeyChar (2).
    QueueKey,
    /// `a->QueueMouse(t, p[0], p[1], p[2], p[3])` with `p` the four ints at `c`, `t` MouseMove (b 0), MouseDown (1),
    /// MouseDoubleClick (2), MouseUp (3), MouseWheel (4).
    QueueMouse,
    /// `dynamic_cast<PlatGUIWindow*>(a->GetParent())`.
    Parent,
    /// the window `a` held (a `SmartPtr<Window>`'s AddRef) / let go.
    Hold,
    Release,
    /// `a->Close()`.
    Close,
    /// `a->ClosePopup(NULL)`.
    ClosePopup,
    /// `NotModalWindow(a)` (0 or 1).
    NotModal,
    /// `a->PlatformSetFocus()`.
    SetFocus,
    /// `a->SetPosSize(b, c, -1, -1)`.
    SetPosSize,
    /// `a->OnPaint()`.
    Paint,
    /// the window `a`'s title: its wide characters (the count into the int at `b`), valid until the next call.
    Title,
    /// the window `a`'s offscreen image's surface (`dynamic_cast<PlatNativeImage*>`), null when it has none.
    Offscreen,
    /// `PlatModifiers()`.
    Modifiers,
    /// `PlatWindowGetClientSize((PlatWindow*)a, &p[0], &p[1])` with `p` the two ints at `b`.
    ClientSize,
    /// `PlatWindowMinimize((PlatWindow*)a)`.
    Minimize,
    /// `PlatWindowWake()`.
    Wake,
}

/// The host: `(manager, op, a, b, c, d) -> result`.
pub type EeWonPlatWmHost =
    Option<unsafe extern "C" fn(me: *mut c_void, op: EeWonPlatWmOp, a: isize, b: isize, c: isize, d: isize) -> isize>;

/// What the frame lock guards (the frame, its sizes and serial, the pending size and scale, the present rectangle).
#[derive(Default)]
struct Shared {
    frame: Vec<u32>,
    width: i32,
    height: i32,
    logical_width: i32,
    logical_height: i32,
    serial: u32,
    pending_width: i32,
    pending_height: i32,
    pending_scale: f32,
    present: [f32; 4],
}

/// The frame lock: held from LockFrame to UnlockFrame (across calls), so a flag under a mutex with a condition.
struct FrameLock {
    held: Mutex<bool>,
    free: Condvar,
    data: std::cell::UnsafeCell<Shared>,
}

impl FrameLock {
    fn lock(&self) {
        let mut h = self.held.lock().unwrap_or_else(|e| e.into_inner());
        while *h {
            h = self.free.wait(h).unwrap_or_else(|e| e.into_inner());
        }
        *h = true;
    }
    fn unlock(&self) {
        let mut h = self.held.lock().unwrap_or_else(|e| e.into_inner());
        *h = false;
        self.free.notify_one();
    }
    /// The data, the lock held by the caller.
    #[allow(clippy::mut_from_ref)]
    unsafe fn data(&self) -> &mut Shared {
        // SAFETY: the caller holds the lock
        unsafe { &mut *self.data.get() }
    }
    fn with<R>(&self, f: impl FnOnce(&mut Shared) -> R) -> R {
        self.lock();
        // SAFETY: held
        let r = f(unsafe { self.data() });
        self.unlock();
        r
    }
}

/// The Rust PlatWindowManager.
pub struct EeWonPlatWm {
    me: *mut c_void,
    host: EeWonPlatWmHost,
    shared: FrameLock,
    /// The pixel scale (written on the UI thread under the frame lock; read anywhere): f32 bits.
    scale: AtomicU32,
    wake: AtomicI32,
    surface_w: Cell<i32>,
    surface_h: Cell<i32>,
    /// bottom to top
    z: RefCell<Vec<*mut c_void>>,
    focus: Cell<*mut c_void>,
    capture: Cell<*mut c_void>,
    drag: Cell<*mut c_void>,
    drag_x: Cell<i32>,
    drag_y: Cell<i32>,
    active: Cell<bool>,
    dirty: Cell<bool>,
    mouse_x: Cell<i32>,
    mouse_y: Cell<i32>,
    have_mouse: Cell<bool>,
    plat_window: Cell<*mut c_void>,
    last_tick: Cell<u32>,
    caption_font: Cell<*mut PlatFont>,
}

// SAFETY: the cells are touched on the UI thread only; other threads use the frame lock's data, the atomics and
// the host's thread-safe ops (IsUiThread, Wake), as the C++ class did.
unsafe impl Sync for EeWonPlatWm {}
// SAFETY: as Sync
unsafe impl Send for EeWonPlatWm {}

fn tick() -> u32 {
    ee_base::plat_core::time::PlatTimeMilliseconds64() as u32
}

/// The WONAPI key modifiers of PLAT_MOD_* (shift 1, alt 2, control 4).
pub fn key_mod(m: u32) -> i32 {
    i32::from(m & MOD_SHIFT != 0) | (i32::from(m & MOD_ALT != 0) << 1) | (i32::from(m & MOD_CONTROL != 0) << 2)
}

/// The windows in drawing order, bottom to top: those that fill the surface, the others, the topmost ones, popups.
pub fn drawing_order(z: &[(*mut c_void, EeWonPlatWinInfo)]) -> Vec<(*mut c_void, EeWonPlatWinInfo)> {
    let layer = |i: &EeWonPlatWinInfo| {
        if i.popup {
            3
        } else if i.top_most {
            2
        } else if i.fills {
            0
        } else {
            1
        }
    };
    let mut out = Vec::with_capacity(z.len());
    for pass in 0..4 {
        out.extend(z.iter().filter(|(_, i)| layer(i) == pass).copied());
    }
    out
}

/// A logical rectangle's frame pixels filled (`FrameFill`).
#[allow(clippy::too_many_arguments)]
pub fn frame_fill(frame: &mut [u32], fw: i32, fh: i32, k: f32, x: i32, y: i32, w: i32, h: i32, color: u32) {
    if w <= 0 || h <= 0 {
        return;
    }
    let (l, t) = (pixels(x, k), pixels(y, k));
    let (r, b) = (pixels(x + w, k), pixels(y + h, k));
    let (x1, y1, x2, y2) = (l.max(0), t.max(0), r.min(fw), b.min(fh));
    for j in y1..y2 {
        for i in x1..x2 {
            frame[(j * fw + i) as usize] = 0xFF00_0000 | color;
        }
    }
}

impl EeWonPlatWm {
    pub fn new(me: *mut c_void, host: EeWonPlatWmHost) -> Self {
        EeWonPlatWm {
            me,
            host,
            shared: FrameLock { held: Mutex::new(false), free: Condvar::new(), data: Default::default() },
            scale: AtomicU32::new(1.0f32.to_bits()),
            wake: AtomicI32::new(0),
            surface_w: Cell::new(DEFAULT_WIDTH),
            surface_h: Cell::new(DEFAULT_HEIGHT),
            z: RefCell::new(Vec::new()),
            focus: Cell::new(std::ptr::null_mut()),
            capture: Cell::new(std::ptr::null_mut()),
            drag: Cell::new(std::ptr::null_mut()),
            drag_x: Cell::new(0),
            drag_y: Cell::new(0),
            active: Cell::new(true),
            dirty: Cell::new(true),
            mouse_x: Cell::new(0),
            mouse_y: Cell::new(0),
            have_mouse: Cell::new(false),
            plat_window: Cell::new(std::ptr::null_mut()),
            last_tick: Cell::new(tick()),
            caption_font: Cell::new(std::ptr::null_mut()),
        }
    }

    fn call(&self, op: EeWonPlatWmOp, a: isize, b: isize, c: isize, d: isize) -> isize {
        match self.host {
            // SAFETY: the host's contract
            Some(f) => unsafe { f(self.me, op, a, b, c, d) },
            None => 0,
        }
    }
    fn w(&self, op: EeWonPlatWmOp, w: *mut c_void, b: isize) -> isize {
        self.call(op, w as isize, b, 0, 0)
    }
    pub fn info(&self, w: *mut c_void) -> EeWonPlatWinInfo {
        let mut i = EeWonPlatWinInfo::default();
        if !w.is_null() {
            self.w(EeWonPlatWmOp::Info, w, &mut i as *mut _ as isize);
        }
        i
    }
    fn z_at(&self, i: usize) -> Option<*mut c_void> {
        self.z.borrow().get(i).copied()
    }
    fn z_len(&self) -> usize {
        self.z.borrow().len()
    }

    pub fn pixel_scale(&self) -> f32 {
        f32::from_bits(self.scale.load(Ordering::Relaxed))
    }
    pub fn surface_size(&self) -> (i32, i32) {
        (self.surface_w.get(), self.surface_h.get())
    }
    pub fn set_dirty(&self) {
        self.dirty.set(true);
    }

    // ---- the game interface

    pub fn set_surface_size(&self, w: i32, h: i32) {
        if self.call(EeWonPlatWmOp::IsUiThread, 0, 0, 0, 0) != 0 {
            self.apply_surface_size(w, h);
            return;
        }
        self.shared.with(|s| {
            s.pending_width = w;
            s.pending_height = h;
        });
        self.signal();
    }

    fn apply_surface_size(&self, w: i32, h: i32) {
        if w <= 0 || h <= 0 || (w == self.surface_w.get() && h == self.surface_h.get()) {
            return;
        }
        self.surface_w.set(w);
        self.surface_h.set(h);
        let mut i = 0;
        while let Some(win) = self.z_at(i) {
            self.w(EeWonPlatWmOp::FollowSurface, win, 0);
            i += 1;
        }
        self.dirty.set(true);
    }

    /// The pixel scale, at least 1 (a smaller lobby is shown scaled down) and at most 4.
    pub fn set_pixel_scale(&self, scale: f32) {
        let mut k = scale;
        if k.is_nan() || k < 1.0 {
            k = 1.0;
        }
        if k > 4.0 {
            k = 4.0;
        }
        if self.call(EeWonPlatWmOp::IsUiThread, 0, 0, 0, 0) != 0 {
            self.apply_pixel_scale(k);
            return;
        }
        // (a scale the manager has or is about to take wakes nothing: the game gives it with every frame)
        let same = self.shared.with(|s| {
            let same = if s.pending_scale > 0.0 { s.pending_scale == k } else { self.pixel_scale() == k };
            if !same {
                s.pending_scale = k;
            }
            same
        });
        if !same {
            self.signal();
        }
    }

    pub fn set_present_rect(&self, l: f32, t: f32, w: f32, h: f32) {
        self.shared.with(|s| s.present = [l, t, w, h]);
    }

    fn apply_pixel_scale(&self, k: f32) {
        if k == self.pixel_scale() {
            return;
        }
        self.shared.with(|_| self.scale.store(k.to_bits(), Ordering::Relaxed));
        let mut i = 0;
        while let Some(win) = self.z_at(i) {
            self.w(EeWonPlatWmOp::FollowPixelScale, win, 0);
            i += 1;
        }
        self.dirty.set(true);
    }

    /// LockFrame: false (and the lock let go) when nothing was composited yet; else the lock is held until
    /// `unlock_frame` and `out` gets the pixels, width, height, pitch, logical width and height and serial.
    pub fn lock_frame(&self, out: &mut (*const u32, i32, i32, i32, i32, i32, u32)) -> bool {
        self.shared.lock();
        // SAFETY: held
        let s = unsafe { self.shared.data() };
        if s.serial == 0 || s.frame.is_empty() {
            self.shared.unlock();
            return false;
        }
        *out = (s.frame.as_ptr(), s.width, s.height, s.width * 4, s.logical_width, s.logical_height, s.serial);
        true
    }
    pub fn unlock_frame(&self) {
        self.shared.unlock();
    }

    /// Signal (any thread): one platform wake until the loop runs again.
    pub fn signal(&self) {
        if self.wake.fetch_add(1, Ordering::SeqCst) + 1 == 1 {
            self.call(EeWonPlatWmOp::Wake, 0, 0, 0, 0);
        }
    }

    /// GetNextEvent's start: the wakes counted again, the pending size and scale applied; the wait's timeout (at
    /// most until 50 ms after the last pass, the original's window timer).
    pub fn begin_pass(&self) -> u32 {
        self.wake.store(0, Ordering::SeqCst);
        let (w, h, k) = self.shared.with(|s| {
            let v = (s.pending_width, s.pending_height, s.pending_scale);
            s.pending_width = 0;
            s.pending_height = 0;
            s.pending_scale = 0.0;
            v
        });
        if w > 0 {
            self.apply_surface_size(w, h);
        }
        if k > 0.0 {
            self.apply_pixel_scale(k);
        }
        let elapsed = tick().wrapping_sub(self.last_tick.get());
        50u32.saturating_sub(elapsed)
    }
    /// GetNextEvent after the wait: the pass's time.
    pub fn end_wait(&self) {
        self.last_tick.set(tick());
    }

    // ---- windows

    pub fn add(&self, w: *mut c_void) {
        {
            let mut z = self.z.borrow_mut();
            if !z.contains(&w) {
                z.push(w);
            }
        }
        self.w(EeWonPlatWmOp::SetRegistered, w, 1);
        self.dirty.set(true);
    }

    /// Takes a closed window off the surface; the focus goes to its parent or the topmost window left (Windows
    /// activates another window when the active one is destroyed).
    pub fn remove(&self, w: *mut c_void) {
        self.z.borrow_mut().retain(|&x| x != w);
        self.w(EeWonPlatWmOp::SetRegistered, w, 0);
        if self.capture.get() == w {
            self.capture.set(std::ptr::null_mut());
        }
        if self.drag.get() == w {
            self.drag.set(std::ptr::null_mut());
        }
        if self.focus.get() == w {
            self.focus.set(std::ptr::null_mut());
            let mut next = self.w(EeWonPlatWmOp::Parent, w, 0) as *mut c_void;
            let ni = self.info(next);
            if next.is_null() || !ni.registered || !ni.visible {
                next = std::ptr::null_mut();
                let mut i = self.z_len();
                while i > 0 {
                    i -= 1;
                    let Some(c) = self.z_at(i) else {
                        continue;
                    };
                    let ci = self.info(c);
                    if ci.visible && !ci.popup {
                        next = c;
                        break;
                    }
                }
            }
            self.set_focus(next);
        }
        self.dirty.set(true);
    }

    /// Raises a window above the other windows of its kind.
    pub fn bring_to_front(&self, w: *mut c_void) {
        let mut z = self.z.borrow_mut();
        let Some(p) = z.iter().position(|&x| x == w) else {
            return;
        };
        if p + 1 == z.len() {
            return;
        }
        z.remove(p);
        z.push(w);
        drop(z);
        self.dirty.set(true);
    }

    /// Moves the keyboard focus (WM_KILLFOCUS to the old window, WM_SETFOCUS to the new one).
    pub fn set_focus(&self, w: *mut c_void) {
        if w == self.focus.get() {
            return;
        }
        let old = self.focus.get();
        self.focus.set(w);
        if !old.is_null() && self.info(old).open && self.active.get() {
            self.call(EeWonPlatWmOp::QueueInput, old as isize, 1, 0, 0);
        }
        if !w.is_null() && self.active.get() {
            self.call(EeWonPlatWmOp::QueueInput, w as isize, 0, 0, 0);
        }
        self.dirty.set(true);
    }

    /// The destructor's: every window unregistered (the base destructor closes them after this part is gone).
    pub fn unregister_all(&self) {
        let z = std::mem::take(&mut *self.z.borrow_mut());
        for w in z {
            self.w(EeWonPlatWmOp::SetRegistered, w, 0);
        }
    }

    pub fn focus(&self) -> *mut c_void {
        self.focus.get()
    }
    pub fn capture(&self) -> *mut c_void {
        self.capture.get()
    }
    pub fn set_capture(&self, w: *mut c_void) {
        self.capture.set(w);
    }
    pub fn mouse(&self) -> (i32, i32, bool) {
        (self.mouse_x.get(), self.mouse_y.get(), self.have_mouse.get())
    }
    pub fn plat_window(&self) -> *mut c_void {
        self.plat_window.get()
    }

    fn infos(&self) -> Vec<(*mut c_void, EeWonPlatWinInfo)> {
        let z = self.z.borrow().clone();
        z.into_iter().map(|w| (w, self.info(w))).collect()
    }

    /// The topmost visible window whose window rectangle holds the surface point (WindowFromPoint).
    pub fn window_from_point(&self, x: i32, y: i32) -> *mut c_void {
        let order = drawing_order(&self.infos());
        for (w, i) in order.iter().rev() {
            if i.visible && x >= i.x && y >= i.y && x < i.x + i.width && y < i.y + i.height {
                return *w;
            }
        }
        std::ptr::null_mut()
    }

    // ---- events

    /// The pointer from the game window's client pixels to the surface's: through the rectangle the game gave, else
    /// the surface scaled uniformly to fit the window, centred (RTSGraphics::RenderLobby).
    fn scale_to_surface(&self, e: &EeWonPlatEvent, x: &mut i32, y: &mut i32) {
        let win = if !e.window.is_null() { e.window } else { self.plat_window.get() };
        let mut size = [0i32; 2];
        if !win.is_null() {
            self.call(EeWonPlatWmOp::ClientSize, win as isize, size.as_mut_ptr() as isize, 0, 0);
        }
        let (cw, ch) = (size[0], size[1]);
        let (sw, sh) = (self.surface_w.get(), self.surface_h.get());
        if cw <= 0 || ch <= 0 || sw <= 0 || sh <= 0 {
            return;
        }
        let r = self.shared.with(|s| s.present);
        if r[2] > 0.0 && r[3] > 0.0 {
            // the original's float expression, its int operands converted
            *x = ((*x as f32 - r[0] * cw as f32) / (r[2] * cw as f32) * sw as f32).floor() as i32;
            *y = ((*y as f32 - r[1] * ch as f32) / (r[3] * ch as f32) * sh as f32).floor() as i32;
            return;
        }
        let scale = (cw as f64 / sw as f64).min(ch as f64 / sh as f64);
        let left = (cw as f64 - sw as f64 * scale) / 2.0;
        let top = (ch as f64 - sh as f64 * scale) / 2.0;
        *x = ((*x as f64 - left) / scale).floor() as i32;
        *y = ((*y as f64 - top) / scale).floor() as i32;
    }

    /// HandlePlatEvent: true when the event goes on to the game's forward function.
    pub fn handle(&self, e: &EeWonPlatEvent) -> bool {
        if !e.window.is_null() {
            self.plat_window.set(e.window);
        }
        match e.kind {
            EV_MOUSE_MOVE | EV_MOUSE_DOWN | EV_MOUSE_UP | EV_MOUSE_WHEEL => {
                self.handle_mouse(e);
                false
            }
            EV_KEY_DOWN | EV_KEY_UP => self.handle_key(e),
            EV_TEXT => {
                // WM_CHAR: one character event per character of the committed text (UTF-8, decision D6; a character
                // past U+FFFF is two events on Windows, its UTF-16 units, as WM_CHAR delivered them)
                let target = self.focus.get();
                if target.is_null() {
                    return false;
                }
                let m = key_mod(self.call(EeWonPlatWmOp::Modifiers, 0, 0, 0, 0) as u32);
                let text = e.text();
                let mut wide = [0 as WChar; 64];
                // SAFETY: a terminated buffer (PLAT_TEXT_TERMINATED) into 64 units
                let n = unsafe { PlatUtf8ToWide(text.as_ptr() as *const c_char, u32::MAX, wide.as_mut_ptr(), 64) };
                for &c in &wide[..(n as usize).min(64)] {
                    self.call(EeWonPlatWmOp::QueueKey, target as isize, 2, c as isize, m as isize);
                }
                false
            }
            EV_FOCUS_GAINED => {
                self.active.set(true);
                let f = self.focus.get();
                if !f.is_null() {
                    self.call(EeWonPlatWmOp::QueueInput, f as isize, 0, 0, 0);
                } else {
                    let mut i = self.z_len();
                    while i > 0 {
                        i -= 1;
                        let Some(c) = self.z_at(i) else {
                            continue;
                        };
                        let ci = self.info(c);
                        if ci.visible && !ci.popup {
                            self.set_focus(c);
                            break;
                        }
                    }
                }
                self.dirty.set(true);
                true
            }
            EV_FOCUS_LOST => {
                let f = self.focus.get();
                if !f.is_null() {
                    self.call(EeWonPlatWmOp::QueueInput, f as isize, 1, 0, 0);
                }
                self.active.set(false);
                self.dirty.set(true);
                true
            }
            EV_QUIT | EV_CLOSE => {
                // WM_CLOSE to the windows that fill the surface (the lobby's main window)
                let held: Vec<*mut c_void> =
                    self.infos().into_iter().filter(|(_, i)| i.fills).map(|(w, _)| w).collect();
                for &w in &held {
                    self.w(EeWonPlatWmOp::Hold, w, 0);
                }
                for &w in &held {
                    self.w(EeWonPlatWmOp::Close, w, 0);
                }
                for &w in &held {
                    self.w(EeWonPlatWmOp::Release, w, 0);
                }
                true
            }
            EV_EXPOSED | EV_RESIZED | EV_MINIMIZED | EV_RESTORED => {
                self.dirty.set(true);
                true
            }
            _ => false,
        }
    }

    fn queue_mouse(&self, w: *mut c_void, kind: isize, p: [i32; 4]) {
        self.call(EeWonPlatWmOp::QueueMouse, w as isize, kind, p.as_ptr() as isize, 0);
    }

    /// Mouse input: to the capture window, else to the topmost window under the pointer; a press on a frame closes
    /// the popups (WM_NCLBUTTONDOWN) and, on a caption, drags the window unless another window is modal.
    fn handle_mouse(&self, e: &EeWonPlatEvent) {
        let (mut x, mut y) = (e.i(0), e.i(1));
        self.scale_to_surface(e, &mut x, &mut y);
        self.mouse_x.set(x);
        self.mouse_y.set(y);
        self.have_mouse.set(true);
        let m = key_mod(e.u(7));
        let mut button = -1;
        if e.kind == EV_MOUSE_DOWN || e.kind == EV_MOUSE_UP {
            let b = e.u(4);
            if (1..=3).contains(&b) {
                button = b as i32 - 1;
            } else {
                return;
            }
        }
        let drag = self.drag.get();
        if !drag.is_null() {
            if e.kind == EV_MOUSE_MOVE {
                self.call(
                    EeWonPlatWmOp::SetPosSize,
                    drag as isize,
                    (x - self.drag_x.get()) as isize,
                    (y - self.drag_y.get()) as isize,
                    0,
                );
            } else if e.kind == EV_MOUSE_UP && button == 0 {
                self.drag.set(std::ptr::null_mut());
            }
            return;
        }
        let capture = self.capture.get();
        let target = if !capture.is_null() { capture } else { self.window_from_point(x, y) };
        if target.is_null() {
            return;
        }
        if capture.is_null() {
            let c = client_rect(&self.info(target));
            if !(x >= c[0] && y >= c[1] && x < c[0] + c[2] && y < c[1] + c[3]) {
                if e.kind == EV_MOUSE_DOWN {
                    self.w(EeWonPlatWmOp::Hold, target, 0);
                    self.w(EeWonPlatWmOp::ClosePopup, target, 0);
                    if self.w(EeWonPlatWmOp::NotModal, target, 0) == 0 && self.info(target).open {
                        self.w(EeWonPlatWmOp::SetFocus, target, 0);
                        let ti = self.info(target);
                        if button == 0 && y < c[1] && y >= ti.y + FRAME_BORDER {
                            self.drag.set(target);
                            self.drag_x.set(x - ti.x);
                            self.drag_y.set(y - ti.y);
                        }
                    }
                    self.w(EeWonPlatWmOp::Release, target, 0);
                }
                return;
            }
        }
        match e.kind {
            EV_MOUSE_MOVE => self.queue_mouse(target, 0, [x, y, m, 0]),
            EV_MOUSE_DOWN => {
                // CS_DBLCLKS: the second press of a double click is a double-click event
                let clicks = e.u(5);
                let double = clicks >= 2 && clicks.is_multiple_of(2);
                self.queue_mouse(target, if double { 2 } else { 1 }, [x, y, m, button]);
            }
            EV_MOUSE_UP => self.queue_mouse(target, 3, [x, y, m, button]),
            EV_MOUSE_WHEEL => {
                let amount = (f32::from_bits(e.u(6)) * 120.0).round() as i64 as i32;
                if amount != 0 {
                    self.queue_mouse(target, 4, [x, y, m, amount as i16 as i32]);
                }
            }
            _ => {}
        }
    }

    /// Keys to the focus window (WM_KEYDOWN / WM_KEYUP with the virtual key), with the control characters
    /// TranslateMessage made of them (Backspace, Tab, Enter, Escape, Ctrl+letter); with Alt held they were system
    /// keys left to DefWindowProc (Alt+Tab minimizes the game for a window that asks it). True when the focus
    /// window forwards its keys to the game.
    fn handle_key(&self, e: &EeWonPlatEvent) -> bool {
        let target = self.focus.get();
        let (key, mods) = (e.u(1), e.u(2));
        let down = e.kind == EV_KEY_DOWN;
        let system = mods & MOD_ALT != 0 || key == VK_MENU || key == VK_F10;
        if !target.is_null() && key != 0 {
            let m = key_mod(mods) as isize;
            if system {
                let pw = self.plat_window.get();
                if down && key == VK_TAB && self.info(target).minimize_on_alt_tab && !pw.is_null() {
                    self.call(EeWonPlatWmOp::Minimize, pw as isize, 0, 0, 0);
                }
            } else {
                self.call(EeWonPlatWmOp::QueueKey, target as isize, if down { 0 } else { 1 }, key as isize, m);
                let mut ch = 0;
                if down {
                    ch = match key {
                        VK_BACK => 8,
                        VK_TAB => 9,
                        VK_RETURN => 13,
                        VK_ESCAPE => 27,
                        k if mods & MOD_CONTROL != 0 && (b'A' as u32..=b'Z' as u32).contains(&k) => {
                            (k - b'A' as u32 + 1) as isize
                        }
                        _ => 0,
                    };
                }
                if ch != 0 {
                    self.call(EeWonPlatWmOp::QueueKey, target as isize, 2, ch, m);
                }
            }
        }
        !target.is_null() && self.info(target).forward_keys
    }

    // ---- painting and compositing

    fn caption_font(&self) -> *mut PlatFont {
        if self.caption_font.get().is_null() {
            // SAFETY: a NUL-terminated face
            self.caption_font.set(unsafe { PlatFontCreate(c"Tahoma".as_ptr(), -11, 1, 0, 0, 0) });
        }
        self.caption_font.get()
    }

    /// Paints every window whose root is invalid, then composites when anything changed.
    pub fn paint_and_composite(&self) {
        let z = self.z.borrow().clone();
        for &w in &z {
            self.w(EeWonPlatWmOp::Hold, w, 0);
        }
        for &w in &z {
            let i = self.info(w);
            if i.registered && i.visible && i.open {
                self.w(EeWonPlatWmOp::Paint, w, 0);
            }
        }
        for &w in &z {
            self.w(EeWonPlatWmOp::Release, w, 0);
        }
        if self.dirty.get() {
            self.composite();
        }
    }

    /// Composites the visible windows into the frame (under the frame lock) and bumps its serial.
    pub fn composite(&self) {
        let order = drawing_order(&self.infos());
        let k = self.pixel_scale();
        let (sw, sh) = (self.surface_w.get(), self.surface_h.get());
        // what the windows hold, read before the lock (a host call never runs under it)
        struct Win {
            info: EeWonPlatWinInfo,
            active: bool,
            title: Vec<u8>,
            image: *mut EeWonSurface,
        }
        let wins: Vec<Win> = order
            .iter()
            .filter(|(_, i)| i.visible)
            .map(|&(w, info)| {
                let title = if info.frame {
                    let mut n = 0i32;
                    let p = self.w(EeWonPlatWmOp::Title, w, &mut n as *mut i32 as isize) as *const WChar;
                    let units: &[WChar] = if p.is_null() || n <= 0 {
                        &[]
                    } else {
                        // SAFETY: the host's n units
                        unsafe { std::slice::from_raw_parts(p, n as usize) }
                    };
                    // the original narrowed the count to an unsigned short
                    let units = &units[..units.len().min(u16::MAX as usize)];
                    to_utf8(units)
                } else {
                    Vec::new()
                };
                Win {
                    info,
                    active: w == self.focus.get() && self.active.get(),
                    title,
                    image: self.w(EeWonPlatWmOp::Offscreen, w, 0) as *mut EeWonSurface,
                }
            })
            .collect();
        let font = if wins.iter().any(|w| w.info.frame) {
            // SAFETY: our font (or null)
            unsafe { PlatFontScaled(self.caption_font(), k) }
        } else {
            std::ptr::null()
        };
        self.shared.lock();
        // SAFETY: held
        let s = unsafe { self.shared.data() };
        let (fw, fh) = (pixels(sw, k), pixels(sh, k));
        s.width = fw;
        s.height = fh;
        s.logical_width = sw;
        s.logical_height = sh;
        s.frame.clear();
        s.frame.resize((fw.max(0) as usize) * (fh.max(0) as usize), 0xFF00_0000);
        for win in &wins {
            let i = &win.info;
            if i.frame {
                let (x, y, ww, hh) = (i.x, i.y, i.width, i.height);
                let f = &mut s.frame;
                frame_fill(f, fw, fh, k, x, y, ww, hh, FACE);
                frame_fill(f, fw, fh, k, x, y, ww, 1, FACE);
                frame_fill(f, fw, fh, k, x, y, 1, hh, FACE);
                frame_fill(f, fw, fh, k, x, y + hh - 1, ww, 1, DARK_SHADOW);
                frame_fill(f, fw, fh, k, x + ww - 1, y, 1, hh, DARK_SHADOW);
                frame_fill(f, fw, fh, k, x + 1, y + 1, ww - 2, 1, HILIGHT);
                frame_fill(f, fw, fh, k, x + 1, y + 1, 1, hh - 2, HILIGHT);
                frame_fill(f, fw, fh, k, x + 1, y + hh - 2, ww - 2, 1, SHADOW);
                frame_fill(f, fw, fh, k, x + ww - 2, y + 1, 1, hh - 2, SHADOW);
                let (cx, cy, cw) = (x + FRAME_BORDER, y + FRAME_BORDER, ww - 2 * FRAME_BORDER);
                frame_fill(
                    f,
                    fw,
                    fh,
                    k,
                    cx,
                    cy,
                    cw,
                    FRAME_CAPTION,
                    if win.active { CAPTION_ACTIVE } else { CAPTION_INACTIVE },
                );
                if !font.is_null() && cw > 4 {
                    let mut m = PlatFontMetrics::default();
                    // SAFETY: a live font
                    unsafe { PlatFontGetMetrics(font, &mut m) };
                    let target = PlatFontTarget {
                        pixels: f.as_mut_ptr() as *mut c_void,
                        pitch: fw * 4,
                        width: fw,
                        height: fh,
                        flags: 0,
                        scale: 0.0,
                    };
                    let (top, bottom) = (pixels(cy, k), pixels(cy + FRAME_CAPTION, k));
                    let clip = PlatFontRect {
                        left: pixels(cx, k).max(0),
                        top: top.max(0),
                        right: pixels(cx + cw - 2, k).min(fw),
                        bottom: bottom.min(fh),
                    };
                    if clip.right > clip.left && clip.bottom > clip.top {
                        // SAFETY: a live font, the title's bytes, the frame as the target
                        unsafe {
                            PlatFontDraw(
                                font,
                                win.title.as_ptr() as *const c_char,
                                win.title.len() as u32,
                                pixels(cx + 3, k),
                                top + (bottom - top - m.height) / 2,
                                0xFFFFFF,
                                0,
                                0,
                                &clip,
                                &target,
                            )
                        };
                    }
                }
            }
            if win.image.is_null() {
                continue;
            }
            // SAFETY: the window's live offscreen surface (the host's answer; no host call since)
            let img = unsafe { &mut *win.image };
            let c = client_rect(i);
            // in frame pixels (an image still at another scale, until it repaints, shows with its pixels as they are)
            let (cx, cy) = (pixels(c[0], k), pixels(c[1], k));
            let iw = img.field(F::Width).min(pixels(c[0] + c[2], k) - cx);
            let ih = img.field(F::Height).min(pixels(c[1] + c[3], k) - cy);
            let pitch = img.field(F::PixelsPerRow);
            let Some(px) = img.words_mut() else {
                continue;
            };
            let (x1, y1) = (cx.max(0), cy.max(0));
            let (x2, y2) = ((cx + iw).min(fw), (cy + ih).min(fh));
            for j in y1..y2 {
                for i2 in x1..x2 {
                    let from = ((j - cy) * pitch + (i2 - cx)) as usize;
                    s.frame[(j * fw + i2) as usize] = px.get(from).copied().unwrap_or(0) | 0xFF00_0000;
                }
            }
        }
        s.serial = s.serial.wrapping_add(1);
        if s.serial == 0 {
            s.serial = 1;
        }
        self.shared.unlock();
        self.dirty.set(false);
    }
}

impl Drop for EeWonPlatWm {
    fn drop(&mut self) {
        let f = self.caption_font.get();
        if !f.is_null() {
            // SAFETY: ours
            unsafe { PlatFontDestroy(f) };
        }
    }
}

// ---- the C entry points (the C++ class's forwarders)

/// The manager's calls (`ee_won_plat_wm`; `a`, `b` their arguments, a window as a `PlatGUIWindow*`).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonPlatWmCall {
    /// `SetSurfaceSize(a, b)`.
    SetSurfaceSize = 1,
    /// `Signal()`.
    Signal,
    /// GetNextEvent's start: the timeout.
    BeginPass,
    /// GetNextEvent after the wait.
    EndWait,
    /// `AddPlatWindow(a)`.
    Add,
    /// `RemovePlatWindow(a)`.
    Remove,
    /// `BringToFront(a)`.
    BringToFront,
    /// `SetFocusWindow(a)`.
    SetFocus,
    /// `SetCaptureWindow(a)`.
    SetCapture,
    /// `GetCaptureWindow()`.
    Capture,
    /// `GetFocusWindow()`.
    Focus,
    /// `WindowFromPoint(a, b)`.
    WindowFromPoint,
    /// `SetDirty()`.
    SetDirty,
    /// `GetSurfaceWidth()`.
    SurfaceWidth,
    /// `GetSurfaceHeight()`.
    SurfaceHeight,
    /// `GetPlatWindow()`.
    PlatWindow,
    /// `PaintAndComposite()`.
    PaintAndComposite,
    /// `UnlockFrame()`.
    UnlockFrame,
    /// the classic scheme's colour `a` (0..14), -1 past it.
    Classic,
    /// the destructor's: every window in the z order unregistered, the order emptied.
    UnregisterAll,
}

/// A new manager for the C++ `me` and its host.
#[no_mangle]
pub extern "C" fn ee_won_plat_wm_new(me: *mut c_void, host: EeWonPlatWmHost) -> *mut EeWonPlatWm {
    Box::into_raw(Box::new(EeWonPlatWm::new(me, host)))
}

/// The manager's destruction.
///
/// # Safety
/// `m` is null or from `ee_won_plat_wm_new`, not freed before.
#[no_mangle]
pub unsafe extern "C" fn ee_won_plat_wm_free(m: *mut EeWonPlatWm) {
    if !m.is_null() {
        // SAFETY: the contract
        drop(unsafe { Box::from_raw(m) });
    }
}

/// A call (`EeWonPlatWmCall`).
///
/// # Safety
/// `m` is a live manager; a window argument is a live window (or null where the call takes one).
#[no_mangle]
pub unsafe extern "C" fn ee_won_plat_wm(m: *const EeWonPlatWm, call: EeWonPlatWmCall, a: isize, b: isize) -> isize {
    use EeWonPlatWmCall as C;
    if call == C::Classic {
        return usize::try_from(a).ok().and_then(|i| CLASSIC.get(i)).map_or(-1, |&c| c as isize);
    }
    // SAFETY: the contract
    let m = unsafe { &*m };
    let p = a as *mut c_void;
    match call {
        C::SetSurfaceSize => m.set_surface_size(a as i32, b as i32),
        C::Signal => m.signal(),
        C::BeginPass => return m.begin_pass() as isize,
        C::EndWait => m.end_wait(),
        C::Add => m.add(p),
        C::Remove => m.remove(p),
        C::BringToFront => m.bring_to_front(p),
        C::SetFocus => m.set_focus(p),
        C::SetCapture => m.set_capture(p),
        C::Capture => return m.capture() as isize,
        C::Focus => return m.focus() as isize,
        C::WindowFromPoint => return m.window_from_point(a as i32, b as i32) as isize,
        C::SetDirty => m.set_dirty(),
        C::SurfaceWidth => return m.surface_size().0 as isize,
        C::SurfaceHeight => return m.surface_size().1 as isize,
        C::PlatWindow => return m.plat_window() as isize,
        C::PaintAndComposite => m.paint_and_composite(),
        C::UnlockFrame => m.unlock_frame(),
        C::UnregisterAll => m.unregister_all(),
        C::Classic => {}
    }
    0
}

/// `SetPixelScale(scale)`.
///
/// # Safety
/// `m` is a live manager.
#[no_mangle]
pub unsafe extern "C" fn ee_won_plat_wm_set_scale(m: *const EeWonPlatWm, scale: f32) {
    // SAFETY: the contract
    unsafe { (*m).set_pixel_scale(scale) }
}

/// `GetPixelScale()`.
///
/// # Safety
/// `m` is a live manager.
#[no_mangle]
pub unsafe extern "C" fn ee_won_plat_wm_scale(m: *const EeWonPlatWm) -> f32 {
    // SAFETY: the contract
    unsafe { (*m).pixel_scale() }
}

/// `SetPresentRect(l, t, w, h)`.
///
/// # Safety
/// `m` is a live manager.
#[no_mangle]
pub unsafe extern "C" fn ee_won_plat_wm_present(m: *const EeWonPlatWm, l: f32, t: f32, w: f32, h: f32) {
    // SAFETY: the contract
    unsafe { (*m).set_present_rect(l, t, w, h) }
}

/// `LockFrame(...)`: `out` gets the pixels' address and then width, height, pitch, logical width, logical height,
/// serial in `ints` (six).
///
/// # Safety
/// `m` is a live manager; `pixels` writable, `ints` six writable ints.
#[no_mangle]
pub unsafe extern "C" fn ee_won_plat_wm_lock_frame(
    m: *const EeWonPlatWm,
    pixels: *mut *const u32,
    ints: *mut u32,
) -> bool {
    let mut o = (std::ptr::null(), 0, 0, 0, 0, 0, 0u32);
    // SAFETY: the contract
    if !unsafe { (*m).lock_frame(&mut o) } {
        return false;
    }
    // SAFETY: the contract
    unsafe {
        *pixels = o.0;
        for (k, v) in [o.1 as u32, o.2 as u32, o.3 as u32, o.4 as u32, o.5 as u32, o.6].into_iter().enumerate() {
            *ints.add(k) = v;
        }
    }
    true
}

/// HandlePlatEvent's translation: true when the event goes on to the game's forward function.
///
/// # Safety
/// `m` is a live manager; `e` a `PlatInputEvent`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_plat_wm_event(m: *const EeWonPlatWm, e: *const EeWonPlatEvent) -> bool {
    // SAFETY: the contract (the event copied: the handlers call out)
    let ev = unsafe { *e };
    // SAFETY: the contract
    unsafe { (*m).handle(&ev) }
}

/// `GetMousePosition(x, y)`.
///
/// # Safety
/// `m` is a live manager; `x`, `y` writable.
#[no_mangle]
pub unsafe extern "C" fn ee_won_plat_wm_mouse(m: *const EeWonPlatWm, x: *mut i32, y: *mut i32) -> bool {
    // SAFETY: the contract
    let (mx, my, have) = unsafe { (*m).mouse() };
    // SAFETY: the contract
    unsafe {
        *x = mx;
        *y = my;
    }
    have
}

/// `PlatGUIWindow::GetClientRect` of the window's fields at `i` into the four ints at `out`.
///
/// # Safety
/// `i` is readable, `out` four writable ints.
#[no_mangle]
pub unsafe extern "C" fn ee_won_plat_client_rect(i: *const EeWonPlatWinInfo, out: *mut i32) {
    // SAFETY: the contract
    let r = client_rect(unsafe { &*i });
    // SAFETY: the contract
    unsafe { std::ptr::copy_nonoverlapping(r.as_ptr(), out, 4) };
}

#[cfg(test)]
#[path = "platwm_tests.rs"]
mod tests;
