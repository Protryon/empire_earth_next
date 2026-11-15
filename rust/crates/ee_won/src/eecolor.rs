//! The game room's colour button and its colour chooser popup as Rust objects (R6 WONLobby round 6; the original
//! EELobbyColorButton.cpp): EELobbyColorButton (a `Button` showing one of the sixteen player colours; a click opens
//! the chooser below it, or closes it) and EELobbyColorChooser (a `Component`: the 4 x 4 swatches of the colours no
//! other player uses, the one under the mouse framed, a click choosing it). The sixteen colours (the original's
//! `gEELobbyColors`, filled from the game's player colours by the lobby's set-up) are this module's palette.
//!
//! The C++ keeps both classes (their virtuals forward here: the resource factory, the player rows' look-ups and the
//! popup machinery did not change). Every call on a component, a graphics context, the root window or the lobby is
//! one op of the host given to `ee_won_eecolor_new` (`WONLobby/EELobbyGlueHost.cpp`), the original's statements with
//! no decision between them. The chooser's taken colours come from the game's Rust object directly. The state is in
//! `Cell`s behind a shared reference: closing a popup and the button's event come back into these objects.

use std::cell::Cell;
use std::ffi::c_void;
use std::sync::atomic::{AtomicU32, Ordering};

use crate::eegame::{game_call, EeWonEeGameCall as GC};

/// The two objects.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonEeColorKind {
    Button = 0,
    Chooser = 1,
}

/// The host's ops (`a`..`f` as documented; `self` the C++ object the Rust object belongs to).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonEeColorOp {
    /// `((Component*)a)->Invalidate()`.
    Invalidate = 1,
    /// `((Component*)a)->Width()`.
    Width,
    /// `((Component*)a)->Height()`.
    Height,
    /// `((Button*)a)->IsDisabled()` (0 or 1).
    IsDisabled,
    /// `((Button*)a)->IsPressed()` (0 or 1).
    IsPressed,
    /// `((Component*)a)->Component::Paint(*(Graphics*)b)`.
    BasePaint,
    /// `((Graphics*)a)->SetColor(b)` (0x00rrggbb).
    SetColor,
    /// `((Graphics*)a)->SetSysColor(StandardColor_MenuBack)`.
    SetMenuBackColor,
    /// `((Graphics*)a)->FillRect(b, c, d, e)`.
    FillRect,
    /// `((Graphics*)a)->DrawRect(b, c, d, e)`.
    DrawRect,
    /// `LobbyDraw3DFrame(*(Graphics*)a, b, c, d, e, f != 0)`.
    Draw3DFrame,
    /// `LobbyDrawFrame(*(Graphics*)a, b, c, d, e, f != 0)`.
    DrawFrame,
    /// `new EELobbyColorChooser` with a reference added (the button's `SmartPtr`).
    ChooserNew,
    /// `((EELobbyColorChooser*)a)->mRust`: the chooser's Rust object.
    ChooserRust,
    /// `((Component*)a)->SetDesiredSize(b, c)`.
    SetDesiredSize,
    /// `((Component*)a)->AddListener(new ComponentListenerComp((Component*)b))`.
    AddListener,
    /// `((Component*)a)->mParent`.
    Parent,
    /// `((Component*)a)->GetRoot()->ClosePopup((Component*)b)`.
    ClosePopup,
    /// `((Component*)a)->LocalToRoot(*(int*)b, *(int*)c)`.
    LocalToRoot,
    /// `PopupParams p((Component*)b, 0x23e7, (Component*)a, c, d); p.mAvoidRect.height = a->Height();
    /// p.mAvoidRect.y = d - a->Height(); p.mInputComponent = a; a->GetRoot()->OpenPopup(p)`.
    OpenPopup,
    /// `((Component*)a)->FireEvent(100064, false)` (the player rows' colour change).
    FireColorChanged,
    /// `((Button*)a)->Button::HandleComponentEvent((ComponentEvent*)b)`.
    BaseEvent,
    /// `((Component*)a)->Component::AddedToParent()`.
    BaseAddedToParent,
    /// `((Component*)a)->Component::MouseExit()`.
    BaseMouseExit,
    /// `GetMyLobbyGame()`.
    MyGame,
    /// `((EELobbyGame*)a)->mRust`: the game's Rust object.
    GameRust,
    /// `((RefCount*)a)->Release()`.
    Release,
}

/// The inward calls (the classes' methods and virtuals).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonEeColorCall {
    /// Button: `SetColor(a)` (0 or 1: whether it changed).
    SetColor = 1,
    /// Button: `mColor`.
    GetColor,
    /// Both: `Paint(*(Graphics*)a)`.
    Paint,
    /// Button: `HandleActivate()` (0).
    Activate,
    /// Button: `HandleComponentEvent(a)`, `b` 1 for an end of popup.
    Event,
    /// Chooser: `AddedToParent()`.
    AddedToParent,
    /// Chooser: `MouseMove(a, b)`.
    MouseMove,
    /// Chooser: `MouseDown(a, b, c)`.
    MouseDown,
    /// Chooser: `MouseExit()`.
    MouseExit,
    /// Chooser: a member: 0 `mHover`, 1 `mSelected`, 2..17 `mTaken[a - 2]`; Button: 0 `mChooser`.
    Field,
    /// Chooser: `mSelected = a`.
    SetSelected,
}

/// The host: `(object, op, a, b, c, d, e, f) -> result`.
pub type EeWonEeColorHostFn = Option<
    unsafe extern "C" fn(
        obj: *mut c_void,
        op: EeWonEeColorOp,
        a: isize,
        b: isize,
        c: isize,
        d: isize,
        e: isize,
        f: isize,
    ) -> isize,
>;

/// The chooser's swatches: 4 x 4 of 20 pixels, 28 apart, 8 from the edge; its size.
const SWATCH: i32 = 20;
const PITCH: i32 = 28;
const MARGIN: i32 = 8;
pub const CHOOSER_SIZE: i32 = 120;

/// The original `gEELobbyColors` (0x00rrggbb), replaced by the game's player colours at the lobby's set-up.
static PALETTE: [AtomicU32; 16] = {
    const fn c(v: u32) -> AtomicU32 {
        AtomicU32::new(v)
    }
    [
        c(0x0000ff),
        c(0xff0000),
        c(0xffff00),
        c(0xff00ff),
        c(0xff8800),
        c(0x008800),
        c(0x00ffff),
        c(0x880000),
        c(0x888888),
        c(0x000088),
        c(0xbbbbbb),
        c(0xdddddd),
        c(0xffff88),
        c(0x88ff88),
        c(0x8800ff),
        c(0xff8888),
    ]
};

/// Colour `i` of the palette (0..15).
pub fn palette(i: usize) -> u32 {
    PALETTE[i & 15].load(Ordering::Relaxed)
}

/// Sets the palette's first `n` colours (the lobby's set-up, `EELobbyGetPlayerColors(gEELobbyColors, 16)`).
pub fn set_palette(colors: &[u32]) {
    for (slot, &c) in PALETTE.iter().zip(colors) {
        slot.store(c, Ordering::Relaxed);
    }
}

/// The swatch under a point of the chooser, -1 in the gaps or outside (MouseMove's rule).
pub fn swatch_at(x: i32, y: i32) -> i32 {
    let mut outside = false;
    let mut x = x.wrapping_sub(MARGIN);
    if x < 0 || x % PITCH >= SWATCH {
        outside = true;
    } else {
        x /= PITCH;
    }
    let mut y = y.wrapping_sub(MARGIN);
    if y < 0 || y % PITCH >= SWATCH {
        outside = true;
    } else {
        y /= PITCH;
    }
    if outside {
        -1
    } else {
        x.wrapping_add(y.wrapping_mul(4))
    }
}

/// A colour button or a colour chooser.
pub struct EeWonEeColor {
    obj: *mut c_void,
    host: EeWonEeColorHostFn,
    kind: EeWonEeColorKind,
    /// the button's `mColor` (0..15)
    color: Cell<i32>,
    /// the button's `mChooser` (a held `SmartPtr`)
    chooser: Cell<isize>,
    /// the chooser's `mHover` and `mSelected` (-1 none)
    hover: Cell<i32>,
    selected: Cell<i32>,
    /// the chooser's `mTaken` (another player's colours)
    taken: [Cell<bool>; 16],
}

use EeWonEeColorOp as Op;

impl EeWonEeColor {
    pub fn new(obj: *mut c_void, kind: EeWonEeColorKind, host: EeWonEeColorHostFn) -> Self {
        Self {
            obj,
            host,
            kind,
            color: Cell::new(0),
            chooser: Cell::new(0),
            hover: Cell::new(-1),
            selected: Cell::new(-1),
            taken: Default::default(),
        }
    }

    #[allow(clippy::too_many_arguments)]
    fn op(&self, op: Op, a: isize, b: isize, c: isize, d: isize, e: isize, f: isize) -> isize {
        match self.host {
            // SAFETY: the C++ object's host (or a test's), called with the object it was given for
            Some(h) => unsafe { h(self.obj, op, a, b, c, d, e, f) },
            None => 0,
        }
    }

    fn op1(&self, op: Op, a: isize) -> isize {
        self.op(op, a, 0, 0, 0, 0, 0)
    }

    fn me(&self) -> isize {
        self.obj as isize
    }

    /// The chooser's Rust object (`mChooser`'s), for its selection.
    fn chooser_rust(&self) -> *const EeWonEeColor {
        let c = self.chooser.get();
        if c == 0 {
            return std::ptr::null();
        }
        self.op1(Op::ChooserRust, c) as *const EeWonEeColor
    }

    // ---- EELobbyColorButton

    fn set_color(&self, color: i32) -> bool {
        if self.color.get() != color && (0..16).contains(&color) {
            self.color.set(color);
            self.op1(Op::Invalidate, self.me());
            return true;
        }
        false
    }

    fn button_paint(&self, g: isize) {
        self.op(Op::BasePaint, self.me(), g, 0, 0, 0, 0);
        self.op(Op::SetColor, g, palette(self.color.get() as usize) as isize, 0, 0, 0, 0);
        let w = self.op1(Op::Width, self.me());
        let h = self.op1(Op::Height, self.me());
        self.op(Op::FillRect, g, 0, 0, w, h, 0);
        if self.op1(Op::IsDisabled, self.me()) == 0 {
            let w = self.op1(Op::Width, self.me());
            let h = self.op1(Op::Height, self.me());
            let pressed = self.op1(Op::IsPressed, self.me());
            self.op(Op::Draw3DFrame, g, 0, 0, w, h, pressed);
        }
    }

    /// HandleActivate: the chooser made once; open, it closes; else it opens centred below the button.
    fn activate(&self) {
        if self.chooser.get() == 0 {
            let c = self.op1(Op::ChooserNew, 0);
            let old = self.chooser.replace(c);
            if old != 0 {
                self.op1(Op::Release, old);
            }
            self.op(Op::SetDesiredSize, c, 0, 0, 0, 0, 0);
            self.op(Op::AddListener, c, self.me(), 0, 0, 0, 0);
        } else if self.op1(Op::Parent, self.chooser.get()) != 0 {
            self.op(Op::ClosePopup, self.me(), self.chooser.get(), 0, 0, 0, 0);
            return;
        }
        let rust = self.chooser_rust();
        if !rust.is_null() {
            // SAFETY: the held chooser's live Rust object (a field write)
            unsafe { (*rust).selected.set(-1) };
        }
        let w = self.op1(Op::Width, self.me());
        let cw = self.op1(Op::Width, self.chooser.get());
        let mut x = ((w as i32).wrapping_sub(cw as i32)) / 2;
        let mut y = self.op1(Op::Height, self.me()) as i32;
        self.op(Op::LocalToRoot, self.me(), &mut x as *mut i32 as isize, &mut y as *mut i32 as isize, 0, 0, 0);
        self.op(Op::OpenPopup, self.me(), self.chooser.get(), x as isize, y as isize, 0, 0);
    }

    fn button_event(&self, event: isize, end_popup: bool) {
        if end_popup {
            let rust = self.chooser_rust();
            // SAFETY: the held chooser's live Rust object (a field read)
            let selected = if rust.is_null() { -1 } else { unsafe { (*rust).selected.get() } };
            if self.set_color(selected) {
                self.op1(Op::FireColorChanged, self.me());
            }
        } else {
            self.op(Op::BaseEvent, self.me(), event, 0, 0, 0, 0);
        }
    }

    // ---- EELobbyColorChooser

    fn added_to_parent(&self) {
        self.op1(Op::BaseAddedToParent, self.me());
        self.hover.set(-1);
        let game = self.op1(Op::MyGame, 0);
        if game != 0 {
            let mut taken = [false; 16];
            let handle = self.op1(Op::GameRust, game);
            game_call(handle, GC::GetTakenColors, taken.as_mut_ptr() as isize, -1, 0);
            for (t, v) in self.taken.iter().zip(taken) {
                t.set(v);
            }
        }
    }

    fn mouse_move(&self, x: i32, y: i32) {
        let hover = swatch_at(x, y);
        if hover != self.hover.get() {
            self.hover.set(hover);
            self.op1(Op::Invalidate, self.me());
        }
    }

    fn mouse_down(&self) {
        let h = self.hover.get();
        if (0..16).contains(&h) && !self.taken[h as usize].get() {
            self.selected.set(h);
            self.op(Op::ClosePopup, self.me(), self.me(), 0, 0, 0, 0);
        }
    }

    fn mouse_exit(&self) {
        self.op1(Op::BaseMouseExit, self.me());
        if self.hover.get() != -1 {
            self.hover.set(-1);
            self.op1(Op::Invalidate, self.me());
        }
    }

    fn chooser_paint(&self, g: isize) {
        self.op(Op::BasePaint, self.me(), g, 0, 0, 0, 0);
        self.op1(Op::SetMenuBackColor, g);
        let w = self.op1(Op::Width, self.me());
        let h = self.op1(Op::Height, self.me());
        self.op(Op::FillRect, g, 0, 0, w, h, 0);
        let w = self.op1(Op::Width, self.me());
        let h = self.op1(Op::Height, self.me());
        self.op(Op::DrawFrame, g, 0, 0, w, h, 0);
        let s = SWATCH as isize;
        for i in 0..16 {
            if self.taken[i].get() {
                continue;
            }
            let x = (MARGIN + PITCH * (i as i32 % 4)) as isize;
            let y = (MARGIN + PITCH * (i as i32 / 4)) as isize;
            self.op(Op::SetColor, g, palette(i) as isize, 0, 0, 0, 0);
            self.op(Op::FillRect, g, x, y, s, s, 0);
            self.op(Op::SetColor, g, 0x888888, 0, 0, 0, 0);
            self.op(Op::DrawRect, g, x, y, s, s, 0);
            if i as i32 == self.hover.get() {
                self.op(Op::Draw3DFrame, g, x, y, s, s, 0);
            }
        }
    }

    /// One inward call.
    pub fn call(&self, call: EeWonEeColorCall, a: isize, b: isize, _c: isize) -> isize {
        use EeWonEeColorCall as C;
        use EeWonEeColorKind as K;
        match (self.kind, call) {
            (K::Button, C::SetColor) => return self.set_color(a as i32) as isize,
            (K::Button, C::GetColor) => return self.color.get() as isize,
            (K::Button, C::Paint) => self.button_paint(a),
            (K::Button, C::Activate) => self.activate(),
            (K::Button, C::Event) => self.button_event(a, b != 0),
            (K::Button, C::Field) => return if a == 0 { self.chooser.get() } else { 0 },
            (K::Chooser, C::Paint) => self.chooser_paint(a),
            (K::Chooser, C::AddedToParent) => self.added_to_parent(),
            (K::Chooser, C::MouseMove) => self.mouse_move(a as i32, b as i32),
            (K::Chooser, C::MouseDown) => self.mouse_down(),
            (K::Chooser, C::MouseExit) => self.mouse_exit(),
            (K::Chooser, C::SetSelected) => self.selected.set(a as i32),
            (K::Chooser, C::Field) => {
                return match a {
                    0 => self.hover.get() as isize,
                    1 => self.selected.get() as isize,
                    2..=17 => self.taken[a as usize - 2].get() as isize,
                    _ => 0,
                }
            }
            _ => {}
        }
        0
    }

    /// The destructor: the button's chooser released (its `SmartPtr`).
    fn release_all(&self) {
        let c = self.chooser.replace(0);
        if c != 0 {
            self.op1(Op::Release, c);
        }
    }
}

/// A new colour button or chooser for the C++ object `obj`.
#[no_mangle]
pub extern "C" fn ee_won_eecolor_new(
    obj: *mut c_void,
    kind: EeWonEeColorKind,
    host: EeWonEeColorHostFn,
) -> *mut EeWonEeColor {
    Box::into_raw(Box::new(EeWonEeColor::new(obj, kind, host)))
}

/// Releases the button's chooser and frees the object.
///
/// # Safety
/// `p` came from `ee_won_eecolor_new` (or is NULL) and is not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_won_eecolor_free(p: *mut EeWonEeColor) {
    if !p.is_null() {
        // SAFETY: the contract; the release runs through a shared reference before the box is dropped
        unsafe { (*p).release_all() };
        // SAFETY: the contract
        drop(unsafe { Box::from_raw(p) });
    }
}

/// One inward call (`EeWonEeColorCall`).
///
/// # Safety
/// `p` is live; a paint's `a` is a `Graphics*`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_eecolor_call(
    p: *const EeWonEeColor,
    call: EeWonEeColorCall,
    a: isize,
    b: isize,
    c: isize,
) -> isize {
    // SAFETY: live (the contract); a shared reference: the calls out may re-enter
    unsafe { (*p).call(call, a, b, c) }
}

/// The palette's colour `i` (0..15).
#[no_mangle]
pub extern "C" fn ee_won_eecolor_palette(i: u32) -> u32 {
    palette(i as usize)
}

/// Sets the palette from `n` colours (0x00rrggbb; at most 16 are read).
///
/// # Safety
/// `colors` holds `n` values.
#[no_mangle]
pub unsafe extern "C" fn ee_won_eecolor_set_palette(colors: *const u32, n: u32) {
    if colors.is_null() {
        return;
    }
    // SAFETY: the contract
    let colors = unsafe { std::slice::from_raw_parts(colors, (n as usize).min(16)) };
    set_palette(colors);
}

#[cfg(test)]
#[path = "eecolor_tests.rs"]
mod tests;
