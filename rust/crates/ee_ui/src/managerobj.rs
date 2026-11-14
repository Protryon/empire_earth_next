//! `UIManager` as a whole Rust object (R6 ui round 17): every member of the manager (the event factories, the form
//! stack, the forms, the visible and copied forms, the queued events, the fonts, the music, the rasterizer, sound
//! manager and string table, the UI texture and the 21 control bitmaps cut from it, the IME candidate window, the two
//! locks, the counts and flags) and every method of `UIManager.cpp` and the header's but the database's
//! (`LoadDatabase`, `CreateBackgrounds`, `RemoveDatabase`: the tables' statics, C++) and the mouse buttons' statics.
//!
//! The C++ `UIManager` stays the type the game holds and `EEUIManager` derives from: its methods are one-line
//! forwarders to [`ee_ui_manager_call`], its constructor and destructor make and free this object, and every call out
//! is an [`EeUiOp`] through its host (`UIManager::HostOp`, UIManager.cpp). The forms, events, bitmaps, texture,
//! locks, sound instances and the candidate window stay C++ objects held by address. The manager's virtuals that
//! `EEUIManager` overrides and that the manager calls itself (`UpdateKeyboard`, `UpdateMouse`, from `UpdateUI`) go
//! through the C++ object (`MgrVirtUpdateKeyboard`, `MgrVirtUpdateMouse`).
//!
//! The algorithms are rounds 4 and 9's: the form stack, the dirty and clip rectangles and the event queue
//! (`manager.rs`), the music and the IME (`manager_media.rs`); this object is their host.
//!
//! **Threads.** The UI thread (the updates, the stack, the music) and the graphics thread (`CopyData` under
//! `LockUIForCopyData`, the draws under the draw lock) both reach the manager, as they reached the C++ members, under
//! the original's two locks. The state is one `UnsafeCell` reached only inside [`EeUiManager::with`] closures that
//! make no call out (`formobj.rs`' rule): a re-entrant call (an event's `ProcessEvent` pushing a form) never meets a
//! live borrow, and no borrow flag can be raced into a panic.

use core::cell::UnsafeCell;
use core::ffi::{c_char, c_long, c_ulong, c_void};
use std::collections::VecDeque;

use crate::manager::{self, EventHost, Form, StackHost, Trigger};
use crate::manager_media::{self, ImeCall, ImeHost, Instance, MusicHost};
use crate::obj::{buf_rect, read_buf, rect_buf, write_buf, EeUiHostFn, EeUiOp as Op, Host};
use crate::rect::{UiPoint, UiRect};

/// The inward calls (the C++ shim's methods). Rectangles and points travel in `f`.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeUiManagerCall {
    /// `AddDirtyRectangle(f, form a)`.
    AddDirtyRectangle = 1,
    /// `AddEventToTrigger(event a, form b, entity c)`.
    AddEventToTrigger = 2,
    /// `AddForm(a)` (winspool.h maps the name AddForm).
    AppendForm = 3,
    /// `AddIMEResultString(*(const UWideString*)a)`.
    AddImeResult = 4,
    /// `ChangeIMECandidateWindow(*(const UIIMECandidateList*)a)`: its style `b`, its count `c` -> OK.
    ChangeCandidateWindow = 5,
    /// `ChangeIMECompositionString`: `a` the host's record of its three arguments -> the form's result.
    ChangeComposition = 6,
    CloseCandidateWindow = 7,
    CloseComposition = 8,
    /// -> the last copy's result.
    CopyData = 9,
    /// `CreateBitmapForScaledControl(bitmap a, f)` -> the new bitmap.
    ScaledBitmap = 10,
    /// `CreateControlEvent(form a, id b)` -> the event or NULL.
    CreateControlEvent = 11,
    /// `CreateFormEvent(id a)` -> the event or NULL.
    CreateFormEvent = 12,
    /// `CreateIMECandidateWindow(font a)`.
    CreateCandidateWindow = 13,
    DrawPolygons = 14,
    DrawText = 15,
    /// The control bitmap `a` ([`Bitmap`]).
    Bitmap = 16,
    CandidateWindow = 17,
    CurrentForm = 18,
    DrawCandidateWindow = 19,
    /// `GetFont(a)`.
    Font = 20,
    /// `GetFormByID(a)` -> the form or NULL.
    FormById = 21,
    /// The stack's size.
    FormCount = 22,
    /// `GetStackedForm(a)`.
    StackedForm = 23,
    NumberOfSurfaces = 24,
    Rasterizer = 25,
    Screen = 26,
    SoundManager = 27,
    StringTable = 28,
    TextureBitDepth = 29,
    VisibleFormCount = 30,
    IsFormOnStack = 31,
    IsFormVisible = 32,
    /// `LockUIForCopyData` (`a` 0) / `UnlockUIForCopyData` (1).
    LockForCopy = 33,
    OpenCandidateWindow = 34,
    OpenComposition = 35,
    Pop = 36,
    PopTo = 37,
    ProcessAllEventsForForm = 38,
    Push = 39,
    RedrawAll = 40,
    /// `RegisterControlEventCallback(a)`.
    RegisterControlCallback = 41,
    /// `RegisterFormEventCallback(a)`.
    RegisterFormCallback = 42,
    RelayoutAllForms = 43,
    /// `SetFont(a, font b)`.
    SetFont = 44,
    SwapForm = 45,
    /// `SetMusicVolume(f[0])`.
    SetMusicVolume = 46,
    SetNumberOfFonts = 47,
    /// `StartMusic`: `a` the name's NUL-terminated bytes.
    StartMusic = 48,
    StopMusic = 49,
    /// `UpdateUI(f[0], f[1])` -> the mouse's result.
    UpdateUi = 50,
    /// `UpdateTranslatedKey(a, repeat b)` -> the form's result.
    UpdateTranslatedKey = 51,
    /// The virtual `UpdateKeyboard()`'s base body.
    UpdateKeyboard = 52,
    /// The virtual `UpdateMouse(f[0], f[1])`'s base body.
    UpdateMouse = 53,
    RemoveAndStopSoundInstance = 54,
    RemoveSoundInstance = 55,
    SetSoundInstance = 56,
    ResetClippedForms = 57,
    ChangeRasterizer = 58,
    ForceUiUpdate = 59,
    /// The constructor's work after `ee_ui_manager_new` (the C++ constructor's, once `mRust` is set: the host's ops
    /// read the manager through its forwarders).
    Init = 60,
}

/// The control bitmaps (the header's member order; [`EeUiManagerCall::Bitmap`]'s index).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub(crate) enum Bitmap {
    AlphaSortUp = 0,
    AlphaSortDown = 1,
    SbDownNormal = 2,
    SbDownPressed = 3,
    SbLeftNormal = 4,
    SbLeftPressed = 5,
    SbRightNormal = 6,
    SbRightPressed = 7,
    SbUpNormal = 8,
    SbUpPressed = 9,
    DragBar = 10,
    CheckOff = 11,
    CheckDownOff = 12,
    CheckOn = 13,
    CheckDownOn = 14,
    CheckNot = 15,
    CheckDownNot = 16,
    RadioOff = 17,
    RadioDownOff = 18,
    RadioOn = 19,
    RadioDownOn = 20,
}

const BITMAPS: usize = 21;

/// The constructor's bitmaps in its order, each the texture's pixel square from (x0, y0) to (x1, y1).
const CUTS: [(Bitmap, [f32; 4]); BITMAPS] = [
    (Bitmap::DragBar, [80.0, 40.0, 100.0, 60.0]),
    (Bitmap::CheckOn, [0.0, 0.0, 20.0, 20.0]),
    (Bitmap::CheckDownOn, [20.0, 0.0, 40.0, 20.0]),
    (Bitmap::CheckNot, [0.0, 60.0, 20.0, 80.0]),
    (Bitmap::CheckDownNot, [20.0, 60.0, 40.0, 80.0]),
    (Bitmap::AlphaSortUp, [40.0, 60.0, 60.0, 80.0]),
    (Bitmap::AlphaSortDown, [60.0, 60.0, 80.0, 80.0]),
    (Bitmap::CheckOff, [40.0, 0.0, 60.0, 20.0]),
    (Bitmap::CheckDownOff, [60.0, 0.0, 80.0, 20.0]),
    (Bitmap::RadioOn, [80.0, 0.0, 100.0, 20.0]),
    (Bitmap::RadioDownOn, [100.0, 0.0, 120.0, 20.0]),
    (Bitmap::RadioOff, [0.0, 20.0, 20.0, 40.0]),
    (Bitmap::RadioDownOff, [20.0, 20.0, 40.0, 40.0]),
    (Bitmap::SbUpNormal, [40.0, 20.0, 60.0, 40.0]),
    (Bitmap::SbUpPressed, [60.0, 20.0, 80.0, 40.0]),
    (Bitmap::SbDownPressed, [80.0, 20.0, 100.0, 40.0]),
    (Bitmap::SbDownNormal, [100.0, 20.0, 120.0, 40.0]),
    (Bitmap::SbRightNormal, [0.0, 40.0, 20.0, 60.0]),
    (Bitmap::SbRightPressed, [20.0, 40.0, 40.0, 60.0]),
    (Bitmap::SbLeftPressed, [40.0, 40.0, 60.0, 60.0]),
    (Bitmap::SbLeftNormal, [60.0, 40.0, 80.0, 60.0]),
];

/// The destructor's release order.
const RELEASES: [Bitmap; BITMAPS] = [
    Bitmap::SbDownNormal,
    Bitmap::SbDownPressed,
    Bitmap::SbLeftNormal,
    Bitmap::SbLeftPressed,
    Bitmap::SbRightNormal,
    Bitmap::SbRightPressed,
    Bitmap::SbUpNormal,
    Bitmap::SbUpPressed,
    Bitmap::DragBar,
    Bitmap::CheckOff,
    Bitmap::CheckDownOff,
    Bitmap::CheckNot,
    Bitmap::CheckDownNot,
    Bitmap::CheckOn,
    Bitmap::CheckDownOn,
    Bitmap::RadioOff,
    Bitmap::RadioDownOff,
    Bitmap::RadioOn,
    Bitmap::RadioDownOn,
    Bitmap::AlphaSortUp,
    Bitmap::AlphaSortDown,
];

/// `kBlendSrcAlpha`, `kBlendInvSrcAlpha`.
const BLEND_SRC_ALPHA: isize = 4;
const BLEND_INV_SRC_ALPHA: isize = 5;

// MgrFormCall's calls
const F_OPEN: isize = 0;
const F_CLOSE: isize = 1;
const F_ACTIVATE: isize = 2;
const F_DEACTIVATE: isize = 3;
const F_CLEAR_CLIPS: isize = 4;
const F_ALL_DIRTY: isize = 5;
const F_COPY: isize = 6;
const F_DRAW_POLYGONS: isize = 7;
const F_DRAW_TEXT: isize = 8;
const F_UPDATE_KEYBOARD: isize = 9;
const F_UPDATE_MOUSE: isize = 10;
const F_DELETE: isize = 11;
// MgrFormRectCall's
const R_ADD_DIRTY: isize = 0;
const R_REDRAW: isize = 1;
const R_ADD_CLIP: isize = 2;
const R_RELAYOUT: isize = 3;
const R_MOUSE_LOCATION: isize = 4;
// MgrCandidateCall's
const C_DRAW_POLYGONS: isize = 0;
const C_DRAW_TEXT: isize = 1;
const C_CLOSE: isize = 2;
const C_COPY: isize = 3;
const C_DELETE: isize = 4;

/// The manager's members.
struct State {
    lock: isize,
    draw_lock: isize,
    form_callbacks: Vec<isize>,
    control_callbacks: Vec<isize>,
    stack: Vec<isize>,
    forms: Vec<isize>,
    visible: Vec<isize>,
    events: VecDeque<Trigger>,
    copied: Vec<isize>,
    fonts: Vec<isize>,
    /// The track's lower-cased name (bytes, no NUL).
    music_name: Vec<u8>,
    rasterizer: isize,
    sound: isize,
    texture: isize,
    bitmaps: [isize; BITMAPS],
    string_table: isize,
    candidate: isize,
    music_id: c_long,
    music_instance: isize,
    music_volume: f32,
    surfaces: c_ulong,
    screen: c_ulong,
    depth: c_ulong,
    candidate_open: bool,
    draw_candidate: bool,
    stop_music: bool,
    start_music: bool,
    force_update: bool,
}

/// The Rust UIManager.
pub struct EeUiManager {
    host: Host,
    s: UnsafeCell<State>,
}

fn at(v: &[isize], i: usize) -> isize {
    v.get(i).copied().unwrap_or(0)
}

fn bits2(a: f32, b: f32) -> isize {
    (a.to_bits() as u64 | (b.to_bits() as u64) << 32) as isize
}

/// The constructor's arguments.
#[derive(Clone, Copy, Debug)]
pub(crate) struct Config {
    pub(crate) rasterizer: isize,
    pub sound: isize,
    pub surfaces: c_ulong,
    pub depth: c_ulong,
    pub string_table: isize,
    pub screen: c_ulong,
}

impl EeUiManager {
    /// The constructor: the two locks, the UI texture and the bitmaps cut from it, no music, no candidate window.
    #[cfg(test)]
    pub(crate) fn new(host: Host, c: Config) -> Self {
        let m = Self::blank(host, c);
        m.init();
        m
    }

    /// The members before the constructor's work ([`EeUiManagerCall::Init`]).
    pub(crate) fn blank(host: Host, c: Config) -> Self {
        let s = State {
            lock: 0,
            draw_lock: 0,
            form_callbacks: Vec::new(),
            control_callbacks: Vec::new(),
            stack: Vec::new(),
            forms: Vec::new(),
            visible: Vec::new(),
            events: VecDeque::new(),
            copied: Vec::new(),
            fonts: Vec::new(),
            music_name: Vec::new(),
            rasterizer: c.rasterizer,
            sound: c.sound,
            texture: 0,
            bitmaps: [0; BITMAPS],
            string_table: c.string_table,
            candidate: 0,
            music_id: -1,
            music_instance: 0,
            music_volume: 1.0,
            surfaces: c.surfaces,
            screen: c.screen,
            depth: c.depth,
            candidate_open: false,
            draw_candidate: false,
            stop_music: false,
            start_music: false,
            force_update: false,
        };
        EeUiManager { host, s: UnsafeCell::new(s) }
    }

    /// The state, for a closure that makes no call out.
    fn with<R>(&self, f: impl FnOnce(&mut State) -> R) -> R {
        // SAFETY: every borrow ends inside the closure, which never calls out, so no other borrow of this thread is
        // alive; the other thread's accesses are the original's member accesses under its locks (module notes)
        f(unsafe { &mut *self.s.get() })
    }

    fn op(&self, op: Op, a: isize, b: isize, c: isize, d: isize) -> isize {
        self.host.op(op, a, b, c, d)
    }

    fn op_f(&self, op: Op, a: isize, b: isize, c: isize, f: [f32; 4]) -> isize {
        let mut f = f;
        self.host.op_f(op, a, b, c, 0, &mut f)
    }

    fn enter(&self, draw: bool) {
        let l = self.with(|s| if draw { s.draw_lock } else { s.lock });
        self.op(Op::FormLockEnter, l, 0, 0, 0);
    }

    fn leave(&self, draw: bool) {
        let l = self.with(|s| if draw { s.draw_lock } else { s.lock });
        self.op(Op::FormLockLeave, l, 0, 0, 0);
    }

    fn top(&self) -> isize {
        self.with(|s| s.stack.last().copied().unwrap_or(0))
    }

    fn init(&self) {
        let lock = self.op(Op::MgrLockNew, 81, 0, 0, 0);
        self.with(|s| s.lock = lock);
        let draw_lock = self.op(Op::MgrLockNew, 82, 0, 0, 0);
        self.with(|s| s.draw_lock = draw_lock);
        let texture = self.op(Op::MgrTextureLoad, 0, 0, 0, 0);
        self.with(|s| s.texture = texture);
        let size = self.host.get_point(Op::MgrTextureSize, texture);
        let (w, h) = (size.x, size.y);
        for (which, [x0, y0, x1, y1]) in CUTS {
            let bitmap = self.op(Op::MgrBitmapNew, texture, 0, 0, 0);
            self.with(|s| s.bitmaps[which as usize] = bitmap);
            let (u0, v0, u1, v1) = (x0 / w, y0 / h, x1 / w, y1 / h);
            // CreateBitmap: the opacity and coordinates, then the scale (top-right u - top-left u, bottom-left v -
            // top-left v)
            self.op_f(Op::MgrBitmapCoords, bitmap, 0, 0, [u0, v0, u1, v1]);
            self.op_f(Op::BitmapSetScale, bitmap, 0, 0, [u1 - u0, v1 - v0, 0.0, 0.0]);
        }
    }

    /// The destructor's work, before the object is freed: the music stopped, the locks, texture and bitmaps
    /// released, the forms and hot keys deleted, the candidate window deleted (the C++ then deletes the database).
    fn release(&self) {
        manager_media::remove_and_stop_sound_instance(&mut Media { m: self, args: 0 });
        let (lock, draw_lock, texture, bitmaps) = self.with(|s| (s.lock, s.draw_lock, s.texture, s.bitmaps));
        self.op(Op::MgrLockDelete, lock, 0, 0, 0);
        self.op(Op::MgrLockDelete, draw_lock, 0, 0, 0);
        self.op(Op::MgrTextureRelease, texture, 0, 0, 0);
        for b in RELEASES {
            self.op(Op::BitmapRelease, bitmaps[b as usize], 0, 0, 0);
        }
        let mut i = 0;
        while let Some(form) = self.with(|s| s.forms.get(i).copied()) {
            self.op(Op::MgrFormCall, form, F_DELETE, 0, 0);
            self.with(|s| {
                if let Some(slot) = s.forms.get_mut(i) {
                    *slot = 0;
                }
            });
            i += 1;
        }
        self.op(Op::MgrHotKeysDelete, 0, 0, 0, 0);
        self.with(|s| {
            s.forms.clear();
            s.visible.clear();
        });
        let candidate = self.with(|s| s.candidate);
        self.op(Op::MgrCandidateCall, candidate, C_DELETE, 0, 0);
    }

    /// `Push`, `Pop`, `PopTo` and `SwapForm`: the body inside both locks -> OK.
    fn locked(&self, body: impl FnOnce(&mut Stack)) -> isize {
        self.enter(false);
        self.enter(true);
        body(&mut Stack(self));
        self.leave(true);
        self.leave(false);
        0
    }

    fn copy_data(&self) -> isize {
        let mut result = 0;
        self.with(|s| s.copied.clear());
        let mut i = 0;
        while let Some(form) = self.with(|s| s.visible.get(i).copied()) {
            result = self.op(Op::MgrFormCall, form, F_COPY, 0, 0);
            // the original pushed mVisibleForms[i] read again after the copy
            self.with(|s| {
                let again = at(&s.visible, i);
                s.copied.push(again)
            });
            i += 1;
        }
        manager_media::copy_candidate_window(&mut Media { m: self, args: 0 }, result as c_long) as isize
    }

    fn draw_polygons(&self) -> isize {
        let blending = self.op(Op::MgrBlending, 0, 0, 0, 0);
        let (source, destination) = (blending as u64 & 0xffff_ffff, blending as u64 >> 32);
        self.op(Op::MgrSetBlending, BLEND_SRC_ALPHA, BLEND_INV_SRC_ALPHA, 0, 0);
        self.enter(true);
        let mut result = 0;
        let mut i = 0;
        while let Some(form) = self.with(|s| s.copied.get(i).copied()) {
            result = self.op(Op::MgrFormCall, form, F_DRAW_POLYGONS, 0, 0);
            // the original read mCopiedForms[i] again for each step
            let form = self.with(|s| at(&s.copied, i));
            if self.op(Op::MgrFormUseGdi, form, 0, 0, 0) == 0 {
                let form = self.with(|s| at(&s.copied, i));
                result = self.op(Op::MgrFormCall, form, F_DRAW_TEXT, 0, 0);
            }
            i += 1;
        }
        let (draw, candidate) = self.with(|s| (s.draw_candidate, s.candidate));
        if draw {
            result = self.op(Op::MgrCandidateCall, candidate, C_DRAW_POLYGONS, 0, 0);
        }
        self.leave(true);
        self.op(Op::MgrSetBlending, source as isize, destination as isize, 0, 0);
        result
    }

    fn draw_text(&self) -> isize {
        let mut result = 0;
        self.enter(true);
        let mut i = 0;
        while let Some(form) = self.with(|s| s.copied.get(i).copied()) {
            if self.op(Op::MgrFormUseGdi, form, 0, 0, 0) != 0 {
                let form = self.with(|s| at(&s.copied, i));
                result = self.op(Op::MgrFormCall, form, F_DRAW_TEXT, 0, 0);
            }
            i += 1;
        }
        let (draw, candidate) = self.with(|s| (s.draw_candidate, s.candidate));
        if draw {
            result = self.op(Op::MgrCandidateCall, candidate, C_DRAW_TEXT, 0, 0);
        }
        self.leave(true);
        result
    }

    fn scaled_bitmap(&self, bitmap: isize, r: UiRect) -> isize {
        let made = self.op(Op::MgrScaledBitmapNew, bitmap, 0, 0, 0);
        let scale = self.host.get_point(Op::BitmapScale, bitmap);
        self.op_f(Op::BitmapSetScale, made, 0, 0, [r.width() * scale.x * 0.05, r.height() * scale.y * 0.05, 0.0, 0.0]);
        made
    }

    /// `CreateFormEvent` (`form` 0) / `CreateControlEvent(form)`: the factories in turn until one makes the event.
    fn create_event(&self, control: bool, form: isize, id: isize) -> isize {
        let mut event = 0;
        let mut i = 0;
        while event == 0 {
            let Some(cb) =
                self.with(|s| if control { &s.control_callbacks } else { &s.form_callbacks }.get(i).copied())
            else {
                break;
            };
            event = self.op(Op::MgrCallback, cb, control as isize, id, form);
            i += 1;
        }
        event
    }

    /// The base `UpdateKeyboard`: the top form's keyboard pass when the keyboard changed, a control holds it, or an
    /// update is forced.
    fn update_keyboard(&self) -> isize {
        let mut result = 0;
        self.enter(false);
        let top = self.top();
        if top != 0
            && (self.op(Op::MgrKeyboardShouldUpdate, 0, 0, 0, 0) != 0
                || self.op(Op::MgrFormKeyboard, top, 0, 0, 0) != 0
                || self.with(|s| s.force_update))
        {
            self.op(Op::MgrHotKeyPoll, 0, 0, 0, 0);
            result = self.op(Op::MgrFormCall, top, F_UPDATE_KEYBOARD, 0, 0);
        }
        self.leave(false);
        result
    }

    /// The base `UpdateMouse`: the top form's mouse pass when the mouse changed, it is captured, or an update is
    /// forced.
    fn update_mouse(&self, p: UiPoint) -> isize {
        let mut result = 0;
        self.enter(false);
        let top = self.top();
        if top != 0
            && (self.op(Op::MgrMouseShouldUpdate, 0, 0, 0, 0) != 0
                || self.op(Op::MgrFormMouse, top, 0, 0, 0) != 0
                || self.with(|s| s.force_update))
        {
            self.op_f(Op::MgrFormRectCall, top, R_MOUSE_LOCATION, 0, [p.x, p.y, 0.0, 0.0]);
            result = self.op(Op::MgrFormCall, top, F_UPDATE_MOUSE, 0, 0);
        }
        self.leave(false);
        result
    }

    fn update_translated_key(&self, key: isize, repeat: isize) -> isize {
        let mut result = 0;
        self.enter(false);
        let top = self.top();
        if top != 0 && (repeat as i16) < 0x10 {
            result = self.op(Op::MgrTranslatedKey, top, key, repeat, 0);
        }
        self.leave(false);
        result
    }

    /// `UpdateUI`: the keyboard, the mouse (both virtual), then the queued events and the music under the lock.
    fn update_ui(&self, p: UiPoint) -> isize {
        self.op(Op::MgrVirtUpdateKeyboard, 0, 0, 0, 0);
        let result = self.op_f(Op::MgrVirtUpdateMouse, 0, 0, 0, [p.x, p.y, 0.0, 0.0]);
        self.with(|s| s.force_update = false);
        self.enter(false);
        manager::process_all_events(&mut Events(self));
        manager_media::update_music(&mut Media { m: self, args: 0 });
        self.leave(false);
        result
    }

    fn bitmap(&self, i: isize) -> isize {
        self.with(|s| if (0..BITMAPS as isize).contains(&i) { s.bitmaps[i as usize] } else { 0 })
    }

    /// One inward call ([`EeUiManagerCall`]).
    ///
    /// # Safety
    /// `f` is NULL or four live floats; the pointers a call takes (names, the IME arguments) are live.
    pub unsafe fn call(&self, call: EeUiManagerCall, a: isize, b: isize, c: isize, f: *mut f32) -> isize {
        use EeUiManagerCall as C;
        // SAFETY: the contract
        let fl = unsafe { read_buf(f) };
        let point = UiPoint { x: fl[0], y: fl[1] };
        match call {
            C::AddDirtyRectangle => manager::add_dirty_rectangle(&mut Stack(self), buf_rect(&fl), a as Form),
            C::AddEventToTrigger => {
                self.op(Op::FormEventCall, a, 2, 0, 0);
                let t = Trigger { event: a as *mut c_void, form: b as *mut c_void, entity: c as *mut c_void };
                self.with(|s| s.events.push_back(t));
            }
            C::AppendForm => self.with(|s| s.forms.push(a)),
            C::AddImeResult => return self.ime(ImeCall::AddResult, a),
            C::ChangeCandidateWindow => {
                return manager_media::change_candidate_window(
                    &mut Media { m: self, args: a },
                    b as c_ulong,
                    c as c_ulong,
                ) as isize
            }
            C::ChangeComposition => return self.ime(ImeCall::ChangeComposition, a),
            C::CloseCandidateWindow => {
                return manager_media::close_candidate_window(&mut Media { m: self, args: 0 }) as isize
            }
            C::CloseComposition => return self.ime(ImeCall::CloseComposition, 0),
            C::CopyData => return self.copy_data(),
            C::ScaledBitmap => return self.scaled_bitmap(a, buf_rect(&fl)),
            C::CreateControlEvent => return self.create_event(true, a, b),
            C::CreateFormEvent => return self.create_event(false, 0, a),
            C::CreateCandidateWindow => {
                let w = self.op(Op::MgrCandidateNew, a, 0, 0, 0);
                self.with(|s| s.candidate = w);
            }
            C::DrawPolygons => return self.draw_polygons(),
            C::DrawText => return self.draw_text(),
            C::Bitmap => return self.bitmap(a),
            C::CandidateWindow => return self.with(|s| s.candidate),
            C::CurrentForm => return self.top(),
            C::DrawCandidateWindow => return self.with(|s| s.draw_candidate) as isize,
            C::Font => return self.with(|s| at(&s.fonts, a as usize)),
            C::FormById => {
                return manager::form_by_id(&Stack(self), a as c_ulong).unwrap_or(core::ptr::null_mut()) as isize
            }
            C::FormCount => return self.with(|s| s.stack.len() as isize),
            C::StackedForm => return self.with(|s| at(&s.stack, a as usize)),
            C::NumberOfSurfaces => return self.with(|s| s.surfaces as isize),
            C::Rasterizer => return self.with(|s| s.rasterizer),
            C::Screen => return self.with(|s| s.screen as isize),
            C::SoundManager => return self.with(|s| s.sound),
            C::StringTable => return self.with(|s| s.string_table),
            C::TextureBitDepth => return self.with(|s| s.depth as isize),
            C::VisibleFormCount => return self.with(|s| s.visible.len() as isize),
            C::IsFormOnStack => return self.with(|s| s.stack.contains(&a)) as isize,
            C::IsFormVisible => return self.with(|s| s.visible.contains(&a)) as isize,
            C::LockForCopy => {
                if a == 0 {
                    self.enter(false)
                } else {
                    self.leave(false)
                }
            }
            C::OpenCandidateWindow => {
                return manager_media::open_candidate_window(&mut Media { m: self, args: 0 }) as isize
            }
            C::OpenComposition => return self.ime(ImeCall::OpenComposition, 0),
            C::Pop => return self.locked(|h| manager::pop(h)),
            C::PopTo => return self.locked(|h| manager::pop_to(h, a as Form)),
            C::ProcessAllEventsForForm => manager::process_all_events_for_form(&mut Events(self), a as *mut c_void),
            C::Push => return self.locked(|h| manager::push(h, a as Form)),
            C::RedrawAll => {
                self.enter(false);
                manager::redraw_all(&mut Stack(self));
                self.leave(false);
            }
            C::RegisterControlCallback => self.with(|s| s.control_callbacks.push(a)),
            C::RegisterFormCallback => self.with(|s| s.form_callbacks.push(a)),
            C::RelayoutAllForms => {
                let size = self.op(Op::MgrWindowSize, 0, 0, 0, 0) as u64;
                manager::relayout_all_forms(&mut Stack(self), (size & 0xffff_ffff) as c_ulong, (size >> 32) as c_ulong);
            }
            C::SetFont => self.with(|s| {
                if let Some(slot) = s.fonts.get_mut(a as usize) {
                    *slot = b;
                }
            }),
            C::SwapForm => return self.locked(|h| manager::swap_form(h, a as Form)),
            C::SetMusicVolume => manager_media::set_music_volume(&mut Media { m: self, args: 0 }, fl[0]),
            C::SetNumberOfFonts => self.with(|s| s.fonts.resize(a as usize, 0)),
            C::StartMusic => {
                // SAFETY: the contract (a NUL-terminated name)
                let name = unsafe { core::ffi::CStr::from_ptr(a as *const c_char) }.to_bytes().to_vec();
                manager_media::start_music(&mut Media { m: self, args: 0 }, &name);
            }
            C::StopMusic => manager_media::stop_music(&mut Media { m: self, args: 0 }),
            C::UpdateUi => return self.update_ui(point),
            C::UpdateTranslatedKey => return self.update_translated_key(a, b),
            C::UpdateKeyboard => return self.update_keyboard(),
            C::UpdateMouse => return self.update_mouse(point),
            C::RemoveAndStopSoundInstance => {
                manager_media::remove_and_stop_sound_instance(&mut Media { m: self, args: 0 })
            }
            C::RemoveSoundInstance => manager_media::remove_sound_instance(&mut Media { m: self, args: 0 }),
            C::SetSoundInstance => manager_media::set_sound_instance(&mut Media { m: self, args: 0 }, a as Instance),
            C::ResetClippedForms => manager::reset_clipped_forms(&mut Stack(self)),
            C::ChangeRasterizer => self.with(|s| s.rasterizer = a),
            C::ForceUiUpdate => self.with(|s| s.force_update = true),
            C::Init => self.init(),
        }
        // SAFETY: the contract
        unsafe { write_buf(f, fl) };
        0
    }

    fn ime(&self, call: ImeCall, args: isize) -> isize {
        manager_media::ime_form_call(&mut Media { m: self, args }, call) as isize
    }
}

/// The form stack's host over the object.
struct Stack<'a>(&'a EeUiManager);

impl StackHost for Stack<'_> {
    fn stack_len(&self) -> usize {
        self.0.with(|s| s.stack.len())
    }
    fn stack(&self, i: usize) -> Form {
        self.0.with(|s| at(&s.stack, i)) as Form
    }
    fn stack_push(&mut self, form: Form) {
        self.0.with(|s| s.stack.push(form as isize))
    }
    fn stack_pop(&mut self) {
        self.0.with(|s| s.stack.pop());
    }
    fn visible_len(&self) -> usize {
        self.0.with(|s| s.visible.len())
    }
    fn visible(&self, i: usize) -> Form {
        self.0.with(|s| at(&s.visible, i)) as Form
    }
    fn visible_insert(&mut self, i: usize, form: Form) {
        self.0.with(|s| {
            let i = i.min(s.visible.len());
            s.visible.insert(i, form as isize)
        })
    }
    fn visible_erase(&mut self, i: usize) {
        self.0.with(|s| {
            if i < s.visible.len() {
                s.visible.remove(i);
            }
        })
    }
    fn visible_push(&mut self, form: Form) {
        self.0.with(|s| s.visible.push(form as isize))
    }
    fn visible_pop(&mut self) {
        self.0.with(|s| s.visible.pop());
    }
    fn forms_len(&self) -> usize {
        self.0.with(|s| s.forms.len())
    }
    fn forms(&self, i: usize) -> Form {
        self.0.with(|s| at(&s.forms, i)) as Form
    }
    fn copied_clear(&mut self) {
        self.0.with(|s| s.copied.clear())
    }
    fn rect(&self, form: Form) -> UiRect {
        self.0.host.get_rect(Op::MgrFormRect, form as isize)
    }
    fn form_id(&self, form: Form) -> c_ulong {
        self.0.op(Op::MgrFormId, form as isize, 0, 0, 0) as c_ulong
    }
    fn open(&mut self, form: Form) {
        self.0.op(Op::MgrFormCall, form as isize, F_OPEN, 0, 0);
    }
    fn close(&mut self, form: Form) {
        self.0.op(Op::MgrFormCall, form as isize, F_CLOSE, 0, 0);
    }
    fn activate(&mut self, form: Form) {
        self.0.op(Op::MgrFormCall, form as isize, F_ACTIVATE, 0, 0);
    }
    fn deactivate(&mut self, form: Form) {
        self.0.op(Op::MgrFormCall, form as isize, F_DEACTIVATE, 0, 0);
    }
    fn add_dirty(&mut self, form: Form, rect: UiRect) {
        self.0.op_f(Op::MgrFormRectCall, form as isize, R_ADD_DIRTY, 0, rect_buf(rect));
    }
    fn redraw_intersect(&mut self, form: Form, rect: UiRect) {
        self.0.op_f(Op::MgrFormRectCall, form as isize, R_REDRAW, 0, rect_buf(rect));
    }
    fn clear_clips(&mut self, form: Form) {
        self.0.op(Op::MgrFormCall, form as isize, F_CLEAR_CLIPS, 0, 0);
    }
    fn add_clip(&mut self, form: Form, rect: UiRect) {
        self.0.op_f(Op::MgrFormRectCall, form as isize, R_ADD_CLIP, 0, rect_buf(rect));
    }
    fn relayout(&mut self, form: Form, rect: UiRect) {
        self.0.op_f(Op::MgrFormRectCall, form as isize, R_RELAYOUT, 0, rect_buf(rect));
    }
    fn set_all_dirty(&mut self, form: Form) {
        self.0.op(Op::MgrFormCall, form as isize, F_ALL_DIRTY, 0, 0);
    }
}

/// The event queue's host over the object.
struct Events<'a>(&'a EeUiManager);

impl EventHost for Events<'_> {
    fn take_front(&mut self) -> Option<Trigger> {
        self.0.with(|s| s.events.pop_front())
    }
    fn process(&mut self, t: Trigger) {
        self.0.op(Op::MgrEventProcess, t.event as isize, t.form as isize, t.entity as isize, 0);
        self.0.op(Op::FormEventCall, t.event as isize, 1, 0, 0);
    }
    fn push_back(&mut self, t: Trigger) {
        self.0.with(|s| s.events.push_back(t))
    }
}

/// The music's and the IME's host over the object; `args` the C++ call's own arguments (the IME strings, the
/// candidate list), handed back to the host untouched.
struct Media<'a> {
    m: &'a EeUiManager,
    args: isize,
}

impl MusicHost for Media<'_> {
    fn has_sound_manager(&self) -> bool {
        self.m.with(|s| s.sound != 0)
    }
    fn volume(&self) -> f32 {
        self.m.with(|s| s.music_volume)
    }
    fn set_volume(&mut self, v: f32) {
        self.m.with(|s| s.music_volume = v)
    }
    fn file_name(&self) -> Vec<u8> {
        self.m.with(|s| s.music_name.clone())
    }
    fn set_file_name(&mut self, name: &[u8]) {
        self.m.with(|s| s.music_name = name.to_vec())
    }
    fn lower(&self, name: &[u8]) -> Vec<u8> {
        // PlatMbsLwr on a copy (UWideString's bytes)
        let mut v = name.to_vec();
        ee_plat_core::utf8::map_in_place(&mut v, ee_plat_core::utf8::fold);
        v
    }
    fn music_id(&self) -> c_long {
        self.m.with(|s| s.music_id)
    }
    fn set_music_id(&mut self, id: c_long) {
        self.m.with(|s| s.music_id = id)
    }
    fn start_flag(&self) -> bool {
        self.m.with(|s| s.start_music)
    }
    fn set_start_flag(&mut self, v: bool) {
        self.m.with(|s| s.start_music = v)
    }
    fn stop_flag(&self) -> bool {
        self.m.with(|s| s.stop_music)
    }
    fn set_stop_flag(&mut self, v: bool) {
        self.m.with(|s| s.stop_music = v)
    }
    fn instance(&self) -> Instance {
        self.m.with(|s| s.music_instance) as Instance
    }
    fn set_instance_field(&mut self, i: Instance) {
        self.m.with(|s| s.music_instance = i as isize)
    }
    fn reference(&mut self, i: Instance) {
        self.m.op(Op::MgrInstanceRef, i as isize, 0, 0, 0);
    }
    fn dereference(&mut self, i: Instance) {
        self.m.op(Op::MgrInstanceRef, i as isize, 1, 0, 0);
    }
    fn stop(&mut self, i: Instance) {
        self.m.op(Op::MgrSoundStop, i as isize, 0, 0, 0);
    }
    fn unload(&mut self, id: c_long) {
        self.m.op(Op::MgrSoundUnload, id as isize, 0, 0, 0);
    }
    fn load(&mut self, name: &[u8]) -> c_long {
        let mut z = name.to_vec();
        z.push(0);
        self.m.op(Op::MgrSoundLoad, z.as_ptr() as isize, 0, 0, 0) as c_long
    }
    fn play(&mut self, id: c_long, volume: f32) -> Instance {
        self.m.op_f(Op::MgrSoundPlay, id as isize, 0, 0, [volume, 0.0, 0.0, 0.0]) as Instance
    }
    fn set_parameters(&mut self, i: Instance, volume: f32) {
        self.m.op_f(Op::MgrSoundParameters, i as isize, 0, 0, [volume, 0.0, 0.0, 0.0]);
    }
    fn lock(&mut self) {
        self.m.enter(false)
    }
    fn unlock(&mut self) {
        self.m.leave(false)
    }
}

impl ImeHost for Media<'_> {
    fn lock(&mut self) {
        self.m.enter(false)
    }
    fn unlock(&mut self) {
        self.m.leave(false)
    }
    fn has_form(&self) -> bool {
        self.m.top() != 0
    }
    fn form_call(&mut self, call: ImeCall) -> c_long {
        let top = self.m.top();
        self.m.op(Op::MgrFormIme, top, call as isize, self.args, 0) as c_long
    }
    fn captured_ime(&mut self, _current: bool) -> bool {
        // GetCurrentForm() and the stack's back are the same form (the back of an empty stack: none)
        let top = self.m.top();
        if top == 0 {
            return false;
        }
        let control = self.m.op(Op::MgrFormKeyboard, top, 0, 0, 0);
        control != 0 && self.m.op(Op::MgrCtlIme, control, 0, 0, 0) != 0
    }
    fn captured_rect(&self, _current: bool) -> UiRect {
        let control = self.m.op(Op::MgrFormKeyboard, self.m.top(), 0, 0, 0);
        self.m.host.get_rect(Op::PartRect, control)
    }
    fn window(&mut self) -> Option<(f32, f32, c_ulong, c_ulong)> {
        let mut f = [0.0f32; 4];
        if self.m.host.op_f(Op::MgrWindow, 0, 0, 0, 0, &mut f) == 0 {
            return None;
        }
        let size = self.m.op(Op::MgrWindowSize, 0, 0, 0, 0) as u64;
        Some((f[0], f[1], (size & 0xffff_ffff) as c_ulong, (size >> 32) as c_ulong))
    }
    fn candidate_open(&self) -> bool {
        self.m.with(|s| s.candidate_open)
    }
    fn set_candidate_open(&mut self, v: bool) {
        self.m.with(|s| s.candidate_open = v)
    }
    fn set_draw_candidate(&mut self, v: bool) {
        self.m.with(|s| s.draw_candidate = v)
    }
    fn candidate_set_list(&mut self, left: f32) {
        let w = self.m.with(|s| s.candidate);
        self.m.op_f(Op::MgrCandidateSetList, w, self.args, 0, [left, 0.0, 0.0, 0.0]);
    }
    fn candidate_open_window(&mut self, screen: UiRect, control: UiRect) {
        let w = self.m.with(|s| s.candidate);
        self.m.op_f(
            Op::MgrCandidateOpen,
            w,
            bits2(control.bottom, control.left),
            bits2(control.right, control.top),
            rect_buf(screen),
        );
    }
    fn candidate_close(&mut self) {
        let w = self.m.with(|s| s.candidate);
        self.m.op(Op::MgrCandidateCall, w, C_CLOSE, 0, 0);
    }
    fn candidate_copy(&mut self) -> c_long {
        let w = self.m.with(|s| s.candidate);
        self.m.op(Op::MgrCandidateCall, w, C_COPY, 0, 0) as c_long
    }
}

// ---- the C ABI

/// A new Rust manager for the C++ `manager`: `UIManager(rasterizer, sound, surfaces, depth, string table, screen)`'s
/// members; the C++ constructor then sets `mRust` and calls [`EeUiManagerCall::Init`].
#[no_mangle]
pub extern "C" fn ee_ui_manager_new(
    manager: *mut c_void,
    host: EeUiHostFn,
    rasterizer: *mut c_void,
    sound: *mut c_void,
    surfaces: c_ulong,
    depth: c_ulong,
    string_table: *mut c_void,
    screen: c_ulong,
) -> *mut EeUiManager {
    let c = Config {
        rasterizer: rasterizer as isize,
        sound: sound as isize,
        surfaces,
        depth,
        string_table: string_table as isize,
        screen,
    };
    Box::into_raw(Box::new(EeUiManager::blank(Host { obj: manager, f: host }, c)))
}

/// The destructor's work (the C++ then deletes the database tables); the object freed.
///
/// # Safety
/// `manager` came from `ee_ui_manager_new` (or is NULL) and is not used after this returns (it is during: the forms'
/// destructors may call the manager).
#[no_mangle]
pub unsafe extern "C" fn ee_ui_manager_free(manager: *mut EeUiManager) {
    if manager.is_null() {
        return;
    }
    // SAFETY: the contract
    unsafe { (*manager).release() };
    // SAFETY: the contract
    drop(unsafe { Box::from_raw(manager) });
}

/// One of UIManager's methods ([`EeUiManagerCall`]).
///
/// # Safety
/// `manager` is live; `f` is NULL or four live floats; see [`EeUiManager::call`].
#[no_mangle]
pub unsafe extern "C" fn ee_ui_manager_call(
    manager: *const EeUiManager,
    call: EeUiManagerCall,
    a: isize,
    b: isize,
    c: isize,
    f: *mut f32,
) -> isize {
    // SAFETY: live (the contract); a shared reference: the calls out re-enter through this function
    unsafe { (*manager).call(call, a, b, c, f) }
}

/// `UIManager::GetMouseWheelScrollAmount`'s step from the wheel's normalized value.
#[no_mangle]
pub extern "C" fn ee_ui_manager_wheel_scroll_amount(value: c_long) -> c_long {
    manager::wheel_scroll_amount(value)
}

#[cfg(test)]
#[path = "managerobj_tests.rs"]
mod tests;
