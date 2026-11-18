//! `UIForm` as a whole Rust object (R6 ui round 16): every member of the form (its controls, groups, backgrounds,
//! hot keys, database records, events, the dirty-rectangle lists, the captures, the popup, the help text, the
//! layout and the counts) and every method of `UIForm.cpp` and the header's.
//!
//! The C++ `UIForm` stays the type the manager and the game's forms hold and derive from: its methods are one-line
//! forwarders to [`ee_ui_form_call`], its constructors and destructor make and free this object, and every call out
//! is an [`EeUiOp`] through its host (`UIForm::HostOp`, UIForm.cpp). The controls, groups, backgrounds, hot keys and
//! events stay C++ objects held by address; their virtuals (a control's `CopyData`, `TabbedTo`, `UpdateKeyboard`, a
//! background's `DrawBackground`, a hot key's `FireEvents`, an event's `ProcessEvent`) run through ops. The form's own
//! virtuals that a game form overrides and that the form calls itself (`SetHelpTextPopupLocation`,
//! `DrawBackground(rect)`) go through the C++ object (`FormVirtHelpLocation`, `FormVirtDrawBackground`).
//!
//! The algorithms are rounds 1-9's: the focus walk and hit test (`form.rs`), the dirty rectangles and the open,
//! close, copy and popup sequences (`form_state.rs`), the mouse pass (`mouse.rs`), the keyboard pass (`hotkey.rs`),
//! the clipped text (`draw.rs`) and the tiles (`background.rs`); this object is their host.
//!
//! **Threads.** The UI thread (the mouse and keyboard passes, the setters) and the graphics thread (`CopyData`, the
//! draws) both reach the form, as they reached the C++ members, with no lock but the draw-background lock the
//! original had. The state is one `UnsafeCell` reached only inside [`EeUiForm::with`] closures that make no call out,
//! so a re-entrant call on the same thread never meets a live borrow, and no borrow flag can be raced into a panic
//! (a `RefCell` would): two threads meet only as the original's two threads met its members.

use core::cell::UnsafeCell;
use core::ffi::{c_char, c_long, c_ulong, c_void};
use std::collections::VecDeque;

use crate::background::{self, FormTileHost};
use crate::draw::{self, Clipping, TextDraw};
use crate::form::{self, FormHost, HitHost};
use crate::form_state::{self, intersects, DirtyRect, FormEvent, Handle, LifeHost, Lists, HIDDEN, SHOWN};
use crate::hotkey::{self, HotKey, KeyboardHost};
use crate::layout::Layout;
use crate::mouse::{self, Control, EntityQuery, FormMouseEvent, FormMouseHost, MouseEntity};
use crate::obj::{buf_rect, read_buf, rect_buf, write_buf, EeUiHostFn, EeUiOp as Op, Host};
use crate::rect::{UiPoint, UiRect};

/// `kControlTriggerMouseOver`, `kControlTriggerMouseOut`.
const TRIGGER_MOUSE_OVER: isize = 0x3f2;
const TRIGGER_MOUSE_OUT: isize = 0x3f3;
/// `kToolTipShadowOpacity`.
const SHADOW_OPACITY: f32 = 0.4;
/// `kKeyNone`: a modifier code at or above it is no modifier.
const KEY_NONE: c_ulong = 0x7fff;

/// The form's events (the setters' index): 0..12 are `UIFormEventType`'s numbers (the database's form events).
pub(crate) const EV_OPEN: usize = 0;
pub(crate) const EV_CLOSE: usize = 1;
pub(crate) const EV_MOVE: usize = 11;
pub(crate) const EV_WHEEL: usize = 12;
pub(crate) const EV_UPDATE: usize = 13;
pub(crate) const EV_ACTIVATE: usize = 14;
pub(crate) const EV_DEACTIVATE: usize = 15;
pub(crate) const EV_KEYBOARD: usize = 16;
const EVENTS: usize = 17;

/// The inward calls (the C++ shim's methods). Rectangles and points travel in `f`.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeUiFormCall {
    Activate = 1,
    Deactivate = 2,
    Close = 3,
    Open = 4,
    /// -> its result.
    CopyData = 5,
    DrawPolygons = 6,
    DrawText = 7,
    /// The virtual `DrawBackground(f)`'s base body -> its result.
    DrawBackground = 8,
    RelayoutForm = 9,
    /// The virtual `SetHelpTextPopupLocation()`'s base body.
    SetHelpTextPopupLocation = 10,
    UpdateKeyboard = 11,
    UpdateMouse = 12,
    /// The virtual `SetRectangle(f)`'s base body.
    SetRectangle = 13,
    AddBackground = 14,
    AddClipRectangle = 15,
    /// `AddControl(a, group b)`.
    AddControl = 16,
    /// `AddDatabaseControl({a, b})`.
    AddDatabaseControl = 17,
    ReserveControlIndex = 18,
    AddDrawControl = 19,
    /// `AddEvent({id a, type b})`.
    AddEvent = 20,
    /// `AddHotKey(*(const UIHotKey*)a, group b)`.
    AddHotKey = 21,
    /// The built hot key's just-pressed (`a`) and pressed (`b`) events, before `AddHotKeyBuild`.
    AddHotKeyEvents = 22,
    /// Its just-unpressed (`a`) and double-pressed (`b`) events.
    AddHotKeyEvents2 = 23,
    /// `AddHotKey(key, m1, m2, m3, the four events, group, always)`: the key and modifiers 16 bits each in `a` (low to
    /// high, each at most 0xffff), the group in `b`'s low 32 bits, always-fire above.
    AddHotKeyBuild = 24,
    ClearAllClipRectangles = 25,
    /// `DrawBackground(bitmap a, f, location)`: the location's x and y float bits in `b` (low, high) -> the draw's.
    DrawBitmap = 26,
    /// Stages rectangle `a` (0..2) for `DrawTiles`.
    StageRect = 27,
    /// `DrawBackground(bitmap a, staged 0, staged 1, staged 2)` -> OK.
    DrawTiles = 28,
    /// `DrawClippedText(f, text a, control b)` -> its result.
    DrawClippedText = 29,
    /// `GetAvailableIndex` -> the index, or -1 for false.
    AvailableIndex = 30,
    BackgroundById = 31,
    ControlById = 32,
    EntityGroupById = 33,
    ShouldFadeIn = 34,
    /// `GetToolTipShadowRectangle(f) -> f`.
    ToolTipShadow = 35,
    LoadBackgrounds = 36,
    /// `LockDrawBackground` (`a` 1) / `UnlockDrawBackground` (0).
    LockDraw = 37,
    RedrawIntersectControls = 38,
    RemoveAllHotKeys = 39,
    RemoveAllHotKeysForAGroup = 40,
    RemoveBackground = 41,
    /// `RemoveControl(a, group b)`.
    RemoveControl = 42,
    /// `RemoveHotKey(key, m1, m2, m3, group b)`: `a` packed as `AddHotKeyBuild`'s.
    RemoveHotKeyKeys = 43,
    /// `RemoveHotKey(index a, group b)`.
    RemoveHotKeyAt = 44,
    SetAllControlsDirty = 45,
    /// One of the seventeen event setters: event index `a` (`EV_*`), the event `b`.
    SetEvent = 46,
    /// The private `SetFormEvent(type a, event b)`.
    SetFormEvent = 47,
    SetFormManager = 48,
    SetHelpTextLabel = 49,
    SetHelpTextTimeDelay = 50,
    /// `SetHotKeyGroupEnabled(a, b != 0)`.
    SetHotKeyGroupEnabled = 51,
    /// `SetLayout`: the four types 16 bits each in `a` (left, right, top, bottom), the offsets in `f` (same order).
    SetLayout = 52,
    /// `SetMusicFileName`: `a` the name's NUL-terminated bytes.
    SetMusicFileName = 53,
    SetPopup = 54,
    SetTabbing = 55,
    /// -> 1 when moved.
    SetTabStop = 56,
    SetUseToolTips = 57,
    ShiftTab = 58,
    Tab = 59,
    UnloadBackgrounds = 60,
    UpdateHelpString = 61,
    UpdateMouseLocation = 62,
    SetKeyboardCaptured = 63,
    SetMouseCaptured = 64,
    SetMouseOverControl = 65,
    UpdateHelpText = 66,
    /// `CheckedRadioButton(a)` -> the index.
    CheckedRadioButton = 67,
    AddCopyDirtyRectangle = 68,
    AddDirtyRectangle = 69,
    Control = 70,
    ControlCount = 71,
    ControlGroupCount = 72,
    DesignedHeight = 73,
    DesignedWidth = 74,
    EntityGroup = 75,
    /// -> 0, `f[0]` the opacity.
    FadeOpacity = 76,
    /// The event of index `a`.
    Event = 77,
    FormId = 78,
    FormManager = 79,
    HelpTextLabel = 80,
    HowLongOpen = 81,
    KeyboardCaptured = 82,
    MouseCaptured = 83,
    UseExclusiveCoords = 84,
    Manager = 85,
    /// -> the address of the mouse location (`GE2DPoint`'s layout).
    MouseLocation = 86,
    MouseOverControl = 87,
    /// -> the address of the music file name's NUL-terminated bytes.
    MusicFileName = 88,
    /// -> the address of the rectangle (`GERectangle`'s layout).
    Rectangle = 89,
    TabbingOn = 90,
    TabStop = 91,
    UseGdiForText = 92,
    DrawToolTipShadow = 93,
    SetDrawToolTipShadow = 94,
}

/// `UIDbDataForms`, the form record the database constructor reads (268 bytes on every target).
#[repr(C)]
#[derive(Clone, Copy)]
pub struct DbForm {
    label: [u8; 0x64],
    music: [u8; 0x64],
    unused_id: i32,
    form_id: i32,
    control_count: u32,
    group_count: u32,
    left: f32,
    right: f32,
    bottom: f32,
    top: f32,
    designed_width: u32,
    designed_height: u32,
    help_delay: u32,
    unused: u32,
    left_type: u32,
    right_type: u32,
    top_type: u32,
    bottom_type: u32,
    tabbing: bool,
    tool_tips: bool,
    exclusive: bool,
}

const _: () = assert!(core::mem::size_of::<DbForm>() == 268);

/// The form's members.
struct State {
    draw_shadow: bool,
    event_data: Vec<(c_ulong, c_ulong)>,
    hot_keys: Vec<Vec<isize>>,
    hot_key_list: Vec<isize>,
    backgrounds: Vec<isize>,
    db: Vec<(c_ulong, isize)>,
    dirty: VecDeque<DirtyRect>,
    copies: Vec<UiRect>,
    clips: Vec<UiRect>,
    form_manager: isize,
    layout: Layout,
    copied_popup_rect: UiRect,
    rect: UiRect,
    /// NUL-terminated.
    music: Vec<u8>,
    mouse: UiPoint,
    groups: Vec<isize>,
    controls: Vec<isize>,
    draws: Vec<isize>,
    manager: isize,
    keyboard: isize,
    mouse_captured: isize,
    popup: isize,
    copied_popup: isize,
    mouse_over: isize,
    help_label: isize,
    events: [isize; EVENTS],
    wheel_event: isize,
    help_poll: isize,
    lock: isize,
    control_count: c_long,
    group_count: c_long,
    tab_stop: c_ulong,
    popup_draw_count: c_ulong,
    form_id: c_ulong,
    designed_width: c_ulong,
    designed_height: c_ulong,
    help_delay: c_ulong,
    open_time: c_ulong,
    fade: f32,
    use_tool_tips: bool,
    tabbing_on: bool,
    use_gdi: bool,
    exclusive: bool,
    /// `AddHotKey`'s four events between `AddHotKeyEvents` and `AddHotKeyBuild`.
    pending_events: [isize; 4],
    /// `DrawTiles`' three rectangles.
    staged: [UiRect; 3],
    /// The mouse pass's six input entities.
    mouse_entities: [isize; 6],
}

/// The Rust UIForm.
pub struct EeUiForm {
    host: Host,
    s: UnsafeCell<State>,
}

fn at(v: &[isize], i: c_long) -> isize {
    if i < 0 {
        return 0;
    }
    v.get(i as usize).copied().unwrap_or(0)
}

impl EeUiForm {
    fn blank(manager: isize) -> State {
        State {
            draw_shadow: false,
            event_data: Vec::new(),
            hot_keys: Vec::new(),
            hot_key_list: Vec::new(),
            backgrounds: Vec::new(),
            db: Vec::new(),
            dirty: VecDeque::new(),
            copies: Vec::new(),
            clips: Vec::new(),
            form_manager: 0,
            layout: Layout::default(),
            copied_popup_rect: UiRect::default(),
            rect: UiRect::default(),
            music: vec![0],
            mouse: UiPoint::default(),
            groups: Vec::new(),
            controls: Vec::new(),
            draws: Vec::new(),
            manager,
            keyboard: 0,
            mouse_captured: 0,
            popup: 0,
            copied_popup: 0,
            mouse_over: 0,
            help_label: 0,
            events: [0; EVENTS],
            wheel_event: 0,
            help_poll: 0,
            lock: 0,
            control_count: 0,
            group_count: 0,
            tab_stop: 0,
            popup_draw_count: 0,
            form_id: 0,
            designed_width: 0,
            designed_height: 0,
            help_delay: 0,
            open_time: 0,
            fade: 0.0,
            use_tool_tips: false,
            tabbing_on: false,
            use_gdi: false,
            exclusive: false,
            pending_events: [0; 4],
            staged: [UiRect::default(); 3],
            mouse_entities: [0; 6],
        }
    }

    /// The constructor `UIForm(controls, groups, manager, id, gdi)`: tool tips and tabbing off, then `Init`.
    pub fn new(host: Host, controls: c_ulong, groups: c_ulong, manager: isize, id: c_ulong, gdi: bool) -> Self {
        let mut s = Self::blank(manager);
        s.control_count = controls as c_long;
        s.group_count = groups as c_long;
        s.form_id = id;
        s.use_gdi = gdi;
        let f = EeUiForm { host, s: UnsafeCell::new(s) };
        f.init();
        f
    }

    /// The constructor from the database record (`music`: its file name, lower-cased, NUL-terminated): the counts,
    /// tool tips and help delay, `Init`, the layout and designed size, then `SetTabbing`.
    pub fn new_db(host: Host, manager: isize, d: &DbForm, music: &[u8], gdi: bool) -> Self {
        let mut s = Self::blank(manager);
        s.control_count = d.control_count as c_long;
        s.group_count = d.group_count as c_long;
        s.form_id = d.form_id as c_ulong;
        s.use_tool_tips = d.tool_tips;
        s.draw_shadow = d.tool_tips;
        s.help_delay = d.help_delay as c_ulong;
        s.music = music.to_vec();
        if s.music.last() != Some(&0) {
            s.music.push(0);
        }
        let f = EeUiForm { host, s: UnsafeCell::new(s) };
        f.init();
        f.with(|s| {
            s.layout.left_type = d.left_type as c_ulong;
            s.layout.left_offset = d.left;
            s.layout.top_type = d.top_type as c_ulong;
            s.layout.top_offset = d.top;
            s.layout.right_type = d.right_type as c_ulong;
            s.layout.right_offset = d.right;
            s.layout.bottom_type = d.bottom_type as c_ulong;
            s.layout.bottom_offset = d.bottom;
            s.designed_height = d.designed_height as c_ulong;
            s.designed_width = d.designed_width as c_ulong;
            s.use_gdi = gdi;
            s.exclusive = d.exclusive;
        });
        f.set_tabbing(d.tabbing);
        f
    }

    /// The state, for a closure that makes no call out.
    fn with<R>(&self, f: impl FnOnce(&mut State) -> R) -> R {
        // SAFETY: every borrow ends inside the closure, which never calls out, so no other borrow of this thread is
        // alive; the other thread's accesses are the original's unguarded member accesses (see the module's notes)
        f(unsafe { &mut *self.s.get() })
    }

    fn op(&self, op: Op, a: isize, b: isize) -> isize {
        self.host.op(op, a, b, 0, 0)
    }

    fn op_rect(&self, op: Op, a: isize, r: UiRect) -> isize {
        self.host.op_rect(op, a, 0, r)
    }

    fn ctl_rect(&self, c: isize) -> UiRect {
        self.host.get_rect(Op::PartRect, c)
    }

    fn control(&self, i: c_long) -> isize {
        self.with(|s| at(&s.controls, i))
    }

    // ---- construction and destruction

    /// `Init`: no events or captures, the wheel-scroll event, the input keys, the help-text poll, the control slots,
    /// the groups and the draw-background lock.
    fn init(&self) {
        let wheel = self.op(Op::FormWheelEventNew, 0, 0);
        self.with(|s| s.wheel_event = wheel);
        self.set_event(EV_WHEEL, wheel);
        if self.op(Op::FormKeysReady, 0, 0) == 0 {
            self.op(Op::FormKeysFetch, 0, 0);
        }
        let delay = self.with(|s| {
            if s.help_delay == 0 {
                s.help_delay = 1;
            }
            s.help_delay
        });
        let poll = self.op(Op::PollCreate, delay as isize, 0);
        let groups = self.with(|s| {
            s.help_poll = poll;
            let n = s.control_count.max(0) as usize;
            s.controls = vec![0; n];
            s.draws = vec![0; n];
            s.groups = vec![0; s.group_count.max(0) as usize];
            s.group_count
        });
        let mut i = groups - 1;
        while i >= 0 {
            let g = self.op(Op::FormGroupNew, 0, 0);
            self.with(|s| s.groups[i as usize] = g);
            i -= 1;
        }
        let lock = self.op(Op::FormLockNew, 0, 0);
        self.with(|s| s.lock = lock);
    }

    /// The destructor: the events released, the hot keys, groups, backgrounds and database controls deleted, the poll,
    /// the form manager and the lock.
    pub fn release(&self) {
        for ev in [
            EV_CLOSE,
            EV_OPEN,
            EV_KEYBOARD,
            EV_UPDATE,
            4,
            2,
            3,
            7,
            5,
            6,
            EV_MOVE,
            10,
            8,
            9,
            EV_WHEEL,
            EV_ACTIVATE,
            EV_DEACTIVATE,
            EV_OPEN,
            EV_CLOSE,
        ] {
            self.set_event(ev, 0);
        }
        let wheel = self.with(|s| s.wheel_event);
        self.op(Op::FormEventCall, wheel, 1);
        self.remove_all_hot_keys();
        let groups = self.with(|s| s.group_count);
        let mut i = groups - 1;
        while i >= 0 {
            let g = self.with(|s| core::mem::take(&mut s.groups[i as usize]));
            self.op(Op::FormGroupDelete, g, 0);
            i -= 1;
        }
        let mut i = 0;
        while let Some(b) = self.with(|s| s.backgrounds.get(i).copied()) {
            self.op(Op::FormBgDelete, b, 0);
            self.with(|s| s.backgrounds[i] = 0);
            i += 1;
        }
        let mut i = self.with(|s| s.db.len()) as isize - 1;
        while i >= 0 {
            let index = self.with(|s| s.db[i as usize].0);
            let table = self.op(Op::FormTable, index as isize, 0);
            if table != 0 {
                let at_form = self.op(Op::FormTableFormIndex, table, 0) as c_long;
                if at_form < self.with(|s| s.control_count) {
                    let c = self.control(at_form);
                    if c != 0 {
                        self.op(Op::DeletePart, c, 0);
                    }
                }
            }
            i -= 1;
        }
        let poll = self.with(|s| s.help_poll);
        if poll != 0 {
            self.op(Op::PollDelete, poll, 0);
        }
        let fm = self.with(|s| s.form_manager);
        if fm != 0 {
            self.op(Op::FormManagerDelete, fm, 0);
        }
        let lock = self.with(|s| s.lock);
        self.op(Op::FormLockDelete, lock, 0);
    }

    // ---- events

    /// One of the event setters: the old event uninitialized and released, the new one referenced and initialized.
    fn set_event(&self, i: usize, e: isize) {
        let old = self.with(|s| s.events[i]);
        if old != 0 {
            self.op(Op::FormEventCall, old, 0);
            self.op(Op::FormEventCall, old, 1);
        }
        self.with(|s| s.events[i] = e);
        let e = self.with(|s| s.events[i]);
        if e != 0 {
            self.op(Op::FormEventCall, e, 2);
            self.op(Op::FormEventCall, e, 3);
        }
    }

    /// The private `SetFormEvent(type, event)`: types 0..12 set that event; others are ignored.
    fn set_form_event(&self, kind: c_ulong, e: isize) {
        if kind <= EV_WHEEL as c_ulong {
            self.set_event(kind as usize, e);
        }
    }

    /// The event's `ProcessEvent(this, NULL)` when the form has it.
    fn fire(&self, i: usize) -> Option<isize> {
        let e = self.with(|s| s.events[i]);
        (e != 0).then(|| self.op(Op::FormEventProcess, e, 0))
    }

    // ---- the virtuals' base bodies

    pub fn activate(&self) {
        let music = self.with(|s| s.music.clone());
        if music.len() > 1 {
            if &music[..music.len() - 1] == b"none" {
                self.op(Op::FormMusicStop, 0, 0);
            } else {
                self.op(Op::FormMusicStart, music.as_ptr() as isize, 0);
            }
        }
        let stop = self.with(|s| s.tab_stop);
        self.set_tab_stop(stop);
        self.fire(EV_ACTIVATE);
    }

    pub fn deactivate(&self) {
        self.fire(EV_DEACTIVATE);
        self.set_popup(0);
        let label = self.with(|s| s.help_label);
        if label != 0 {
            self.op(Op::FormCtlSetDisplayMode, label, HIDDEN as isize);
        }
        let poll = self.with(|s| s.help_poll);
        self.op(Op::PollStop, poll, 0);
        let c = self.with(|s| s.mouse_captured);
        if c != 0 {
            self.op(Op::FormCtlReleaseMouse, c, 0);
            self.with(|s| s.mouse_captured = 0);
        }
        let c = self.with(|s| s.keyboard);
        if c != 0 {
            self.op(Op::FormCtlReleaseKeyboard, c, 0);
            self.with(|s| s.keyboard = 0);
        }
        self.with(|s| s.mouse_over = 0);
    }

    /// `DrawBackground(rect)`: every visible background meeting it redraws it -> the last result.
    pub fn draw_background(&self, r: UiRect) -> isize {
        let mut result = 0;
        let mut i = 0;
        while let Some(b) = self.with(|s| s.backgrounds.get(i).copied()) {
            if self.op(Op::FormBgVisible, b, 0) != 0 && intersects(&r, &self.host.get_rect(Op::FormBgRect, b)) {
                result = self.op_rect(Op::FormBgDraw, b, r);
            }
            i += 1;
        }
        result
    }

    pub fn draw_polygons(&self) -> isize {
        let mut result = 0;
        let lock = self.with(|s| s.lock);
        self.op(Op::FormLockEnter, lock, 0);
        let mut i = 0;
        while let Some(r) = self.with(|s| s.copies.get(i).copied()) {
            self.op_rect(Op::FormVirtDrawBackground, 0, r);
            i += 1;
        }
        self.with(|s| s.copies.clear());
        self.op(Op::FormLockLeave, lock, 0);
        let count = self.with(|s| s.control_count);
        for i in 0..count.max(0) {
            let d = self.with(|s| at(&s.draws, i));
            if d != 0 {
                result = self.op(Op::PartDrawPolygons, d, 0);
            }
        }
        let popup = self.with(|s| s.copied_popup);
        if popup != 0 {
            let (shadow, label) = self.with(|s| (s.draw_shadow, s.help_label));
            if shadow && label == popup {
                let r = form_state::tool_tip_shadow(&self.ctl_rect(popup));
                let rh = self.op(Op::RhombusCreate, 0, 0);
                self.op(Op::FormRhombusLabelColor, rh, popup);
                crate::draw2d::rhombus_set_opacity(rh, SHADOW_OPACITY, || {
                    self.host.op_f(Op::RhombusSetOpacity, rh, 0, 0, 0, &mut [SHADOW_OPACITY, 0.0, 0.0, 0.0])
                });
                crate::draw2d::rhombus_set_rect(rh, r, || self.op_rect(Op::RhombusSetRect, rh, r));
                crate::draw2d::rhombus(rh, || self.op(Op::FormRhombusDraw, rh, 0));
                self.op(Op::RhombusDelete, rh, 0);
            }
            result = self.op(Op::PartDrawPolygons, popup, 0);
        }
        result
    }

    pub fn draw_text(&self) -> isize {
        let mut result = 0;
        let count = self.with(|s| s.control_count);
        let draw_all = |result: &mut isize| {
            let mut i = count - 1;
            while i >= 0 {
                let d = self.with(|s| at(&s.draws, i));
                if d != 0 {
                    *result = self.op(Op::PartDrawText, d, 0);
                    self.with(|s| s.draws[i as usize] = 0);
                }
                i -= 1;
            }
        };
        if self.with(|s| s.copied_popup) != 0 {
            draw_all(&mut result);
            let popup = self.with(|s| s.copied_popup);
            if popup != 0 {
                result = self.op(Op::PartDrawText, popup, 0);
                self.with(|s| {
                    s.popup_draw_count = s.popup_draw_count.wrapping_sub(1);
                    s.copied_popup = 0;
                });
            }
        } else {
            draw_all(&mut result);
        }
        result
    }

    /// `DrawBackground(bitmap, rect, location)`: the part of the bitmap's texture under the rectangle (one pixel wider
    /// and taller) drawn at the location.
    pub fn draw_bitmap(&self, bitmap: isize, rect: UiRect, location: UiPoint) -> isize {
        let size = self.host.get_point(Op::FormBitmapTexSize, bitmap);
        let (w, h) = (size.x, size.y);
        let mut r = rect;
        r.right += 1.0;
        r.bottom += 1.0;
        let (u0, v0) = (r.left / w, r.top / h);
        let (u1, v1) = (r.right / w, r.bottom / h);
        crate::draw2d::bitmap_set_location(bitmap, location.x, location.y, || {
            self.host.op_point(Op::BitmapSetLocation, bitmap, location)
        });
        let (sx, sy) = (r.width() / w, r.height() / h);
        crate::draw2d::bitmap_set_scale(bitmap, sx, sy, || {
            self.host.op_f(Op::BitmapSetScale, bitmap, 0, 0, 0, &mut [sx, sy, 0.0, 0.0])
        });
        self.host.op_f(Op::FormBitmapSetTexCoords, bitmap, 0, 0, 0, &mut [u0, v0, u1, v1]);
        crate::draw2d::bitmap(bitmap, || self.op(Op::FormBitmapDraw, bitmap, 0))
    }

    pub fn draw_clipped_text(&self, r: UiRect, text: isize, control: isize) -> isize {
        let (clips, popup) = self.with(|s| (s.clips.clone(), s.copied_popup));
        let clipping = if !clips.is_empty() {
            Clipping::Rectangles(clips)
        } else if popup != 0 {
            let is_control = control != 0 && {
                let i = self.op(Op::FormCtlIndex, control, 0) as c_ulong;
                popup == self.control(i as c_long)
            };
            let rect = self.ctl_rect(popup);
            let draw_rect = self.host.get_rect(Op::PartCopiedDrawRect, popup);
            Clipping::Popup { is_control, rect, draw_rect }
        } else {
            let shown = self.op(Op::FormCandidateShown, 0, 0) != 0;
            Clipping::Plain { candidate: shown.then(|| self.host.get_rect(Op::FormCandidateRect, 0)) }
        };
        draw::draw_clipped_text(&mut TextOut { f: self, text }, r, &clipping) as isize
    }

    pub fn relayout(&self, parent: UiRect) {
        self.with(|s| s.rect = s.layout.calculate(&parent, false));
        let mut i = 0;
        while let Some(b) = self.with(|s| s.backgrounds.get(i).copied()) {
            self.op(Op::FormBgRelayout, b, 0);
            i += 1;
        }
        let mut i = 0;
        while let Some((index, c)) = self.with(|s| s.db.get(i).copied()) {
            let table = self.op(Op::FormTable, index as isize, 0);
            if table != 0 {
                self.op(Op::FormTableRelayout, table, c);
            }
            i += 1;
        }
    }

    pub fn set_help_text_popup_location(&self) {
        let label = self.with(|s| s.help_label);
        let lr = self.ctl_rect(label);
        let (mouse, rect) = self.with(|s| (s.mouse, s.rect));
        let p = form_state::help_popup_location(mouse, lr, rect);
        self.host.op_point(Op::PartSetLocation, label, p);
    }

    // ---- controls and groups

    pub fn add_control(&self, c: isize, group: c_ulong) {
        let i = self.op(Op::FormCtlIndex, c, 0) as c_ulong as usize;
        let g = self.with(|s| {
            if let Some(slot) = s.controls.get_mut(i) {
                if *slot == 0 {
                    *slot = c;
                }
            }
            s.groups.get(group as usize).copied().unwrap_or(0)
        });
        self.op(Op::FormGroupAdd, g, c);
    }

    pub fn remove_control(&self, c: isize, group: c_ulong) {
        let i = self.op(Op::FormCtlIndex, c, 0) as c_ulong as usize;
        let g = self.with(|s| {
            if let Some(slot) = s.controls.get_mut(i) {
                *slot = 0;
            }
            s.groups.get(group as usize).copied().unwrap_or(0)
        });
        self.op(Op::FormGroupRemove, g, c);
    }

    /// `GetEntityGroupById`: the group holding the button-type entity with the id (the groups from the last); the
    /// last group looked at (group 0) when none does.
    pub fn entity_group_by_id(&self, id: c_ulong) -> isize {
        let mut found = false;
        let mut group = 0;
        let mut i = self.with(|s| s.group_count) - 1;
        while i >= 0 && !found {
            group = self.with(|s| at(&s.groups, i));
            let mut j = self.op(Op::FormGroupCount, group, 0) as c_ulong as c_long - 1;
            while j >= 0 {
                let e = self.op(Op::FormGroupEntity, group, j as isize);
                if self.op(Op::FormCtlEntityType, e, 0) as c_ulong == form_state::ENTITY_CONTROL
                    && self.op(Op::FormCtlId, e, 0) as c_ulong == id
                {
                    found = true;
                    break;
                }
                j -= 1;
            }
            if found {
                break;
            }
            i -= 1;
        }
        group
    }

    fn available_index(&self) -> Option<c_ulong> {
        self.with(|s| {
            let mut i = s.control_count - 1;
            while i >= 0 {
                if at(&s.controls, i) == 0 {
                    return (i + 1 != s.control_count).then_some((i + 1) as c_ulong);
                }
                i -= 1;
            }
            None
        })
    }

    fn control_by_id(&self, id: c_ulong) -> isize {
        let mut i = self.with(|s| s.control_count) - 1;
        while i >= 0 {
            let c = self.control(i);
            if c != 0 && self.op(Op::FormCtlId, c, 0) as c_ulong == id {
                return c;
            }
            i -= 1;
        }
        0
    }

    fn reserve_control_index(&self, index: c_ulong) {
        self.with(|s| {
            if (index as c_long) < s.control_count {
                return;
            }
            let n = index as usize + 1;
            s.controls.resize(n, 0);
            s.draws.resize(n, 0);
            s.control_count = n as c_long;
        });
    }

    // ---- backgrounds

    pub fn add_background(&self, b: isize) {
        let priority = self.op(Op::FormBgPriority, b, 0) as c_ulong;
        let mut i = 0;
        while let Some(x) = self.with(|s| s.backgrounds.get(i).copied()) {
            if priority <= self.op(Op::FormBgPriority, x, 0) as c_ulong {
                break;
            }
            i += 1;
        }
        self.with(|s| s.backgrounds.insert(i, b));
    }

    fn background_by_id(&self, id: c_ulong) -> isize {
        let mut i = 0;
        while let Some(b) = self.with(|s| s.backgrounds.get(i).copied()) {
            if self.op(Op::FormBgId, b, 0) as c_ulong == id {
                return b;
            }
            i += 1;
        }
        0
    }

    fn remove_background(&self, b: isize) {
        let id = self.op(Op::FormBgId, b, 0) as c_ulong;
        let mut i = 0;
        while let Some(x) = self.with(|s| s.backgrounds.get(i).copied()) {
            if id == self.op(Op::FormBgId, x, 0) as c_ulong {
                self.with(|s| s.backgrounds.remove(i));
                self.op(Op::FormBgDelete, b, 0);
                break;
            }
            i += 1;
        }
    }

    fn each_background(&self, op: Op) {
        let mut i = 0;
        while let Some(b) = self.with(|s| s.backgrounds.get(i).copied()) {
            self.op(op, b, 0);
            i += 1;
        }
    }

    // ---- hot keys

    /// The hot keys ordered: more keys first, always-fire keys last (a stable sort over `UIHotKeySorter`'s order).
    fn sort_keys(&self, keys: &mut [isize]) {
        let rank: Vec<(bool, c_ulong)> = keys
            .iter()
            .map(|&k| (self.op(Op::FormHotKeyAlways, k, 0) != 0, self.op(Op::FormHotKeyKeys, k, 0) as c_ulong))
            .collect();
        let mut order: Vec<usize> = (0..keys.len()).collect();
        order.sort_by(|&x, &y| {
            if hotkey::before(rank[x].0, rank[x].1, rank[y].0, rank[y].1) {
                core::cmp::Ordering::Less
            } else if hotkey::before(rank[y].0, rank[y].1, rank[x].0, rank[x].1) {
                core::cmp::Ordering::Greater
            } else {
                core::cmp::Ordering::Equal
            }
        });
        let sorted: Vec<isize> = order.iter().map(|&i| keys[i]).collect();
        keys.copy_from_slice(&sorted);
    }

    pub fn add_hot_key(&self, source: isize, group: c_ulong) {
        let k = self.op(Op::FormHotKeyCopy, source, 0);
        let mut keys = self.with(|s| {
            if s.hot_keys.len() <= group as usize {
                s.hot_keys.resize(group as usize + 1, Vec::new());
            }
            s.hot_keys[group as usize].push(k);
            s.hot_keys[group as usize].clone()
        });
        self.sort_keys(&mut keys);
        let mut list = self.with(|s| {
            s.hot_keys[group as usize] = keys;
            s.hot_key_list.push(k);
            s.hot_key_list.clone()
        });
        self.sort_keys(&mut list);
        self.with(|s| s.hot_key_list = list);
    }

    fn add_hot_key_build(&self, packed: isize, group: c_ulong, always: bool) {
        let code = |n: u32| ((packed as u64 >> n) & 0xffff) as c_ulong;
        let k = self.op(Op::FormHotKeyTemp, code(0) as isize, 0);
        for slot in 0..3 {
            let m = code(16 * (slot + 1));
            if m < KEY_NONE {
                self.host.op(Op::FormHotKeySetModifier, k, m as isize, slot as isize, 0);
            }
        }
        let events = self.with(|s| core::mem::take(&mut s.pending_events));
        // the original's order: just pressed, pressed, just unpressed, double pressed
        for (which, e) in events.iter().enumerate() {
            if *e != 0 {
                self.host.op(Op::FormHotKeySetEvent, k, which as isize, *e, 0);
                self.op(Op::FormEventCall, *e, 1);
            }
        }
        self.op(Op::FormHotKeySetAlways, k, always as isize);
        self.add_hot_key(k, group);
        self.op(Op::FormHotKeyDelete, k, 0);
    }

    /// Every occurrence of `k` out of the form's list.
    fn unlist_hot_key(&self, k: isize) {
        self.with(|s| s.hot_key_list.retain(|&x| x != k));
    }

    pub fn remove_all_hot_keys(&self) {
        let mut i = 0;
        while let Some(n) = self.with(|s| s.hot_keys.get(i).map(|g| g.len())) {
            for j in 0..n {
                let k = self.with(|s| s.hot_keys[i].get(j).copied().unwrap_or(0));
                if k != 0 {
                    self.op(Op::FormHotKeyDelete, k, 0);
                }
            }
            self.with(|s| s.hot_keys[i].clear());
            i += 1;
        }
        self.with(|s| {
            s.hot_keys.clear();
            s.hot_key_list.clear();
        });
    }

    fn remove_all_hot_keys_for_group(&self, group: c_ulong) {
        let g = group as usize;
        if g >= self.with(|s| s.hot_keys.len()) {
            return;
        }
        while let Some(k) = self.with(|s| s.hot_keys[g].first().copied()) {
            self.unlist_hot_key(k);
            self.op(Op::FormHotKeyDelete, k, 0);
            self.with(|s| s.hot_keys[g].remove(0));
        }
        self.with(|s| s.hot_keys[g].clear());
    }

    fn remove_hot_key_keys(&self, packed: isize, group: c_ulong) {
        let code = |n: u32| ((packed as u64 >> n) & 0xffff) as isize;
        let key = self.op(Op::FormKeyEntity, code(0), 0);
        let mods = [
            self.op(Op::FormKeyEntity, code(16), 0),
            self.op(Op::FormKeyEntity, code(32), 0),
            self.op(Op::FormKeyEntity, code(48), 0),
        ];
        let g = group as usize;
        if g >= self.with(|s| s.hot_keys.len()) {
            return;
        }
        let mut j = 0;
        while let Some(k) = self.with(|s| s.hot_keys[g].get(j).copied()) {
            if self.op(Op::FormHotKeyKey, k, 0) != key
                || self.op(Op::FormHotKeyMod0, k, 0) != mods[0]
                || self.op(Op::FormHotKeyMod1, k, 0) != mods[1]
                || self.op(Op::FormHotKeyMod2, k, 0) != mods[2]
            {
                j += 1;
                continue;
            }
            self.unlist_hot_key(k);
            self.op(Op::FormHotKeyDelete, k, 0);
            self.with(|s| s.hot_keys[g].remove(j));
            return;
        }
    }

    fn remove_hot_key_at(&self, index: c_ulong, group: c_ulong) {
        let k = self.with(|s| {
            let keys = s.hot_keys.get_mut(group as usize)?;
            if (index as usize) < keys.len() {
                Some(keys.remove(index as usize))
            } else {
                None
            }
        });
        let Some(k) = k else { return };
        self.unlist_hot_key(k);
        self.op(Op::FormHotKeyDelete, k, 0);
    }

    fn set_hot_key_group_enabled(&self, group: c_ulong, enabled: bool) {
        let keys = self.with(|s| s.hot_keys.get(group as usize).cloned().unwrap_or_default());
        for k in keys {
            self.op(Op::FormHotKeySetEnabled, k, enabled as isize);
        }
    }

    // ---- focus, popup and help text

    pub fn set_tabbing(&self, tabbing: bool) {
        let stop = self.with(|s| {
            s.tabbing_on = tabbing;
            s.tab_stop
        });
        let c = self.control(stop as c_long);
        if c != 0 {
            self.op(Op::FormCtlTabbedFrom, c, 0);
        }
        if tabbing {
            let count = self.with(|s| s.control_count) as c_ulong;
            let mut i: c_ulong = 0;
            while i < count {
                let c = self.control(i as c_long);
                if c != 0 && self.op(Op::FormCtlTabable, c, 0) != 0 {
                    self.with(|s| s.tab_stop = i);
                    self.op(Op::FormCtlTabbedTo, c, 0);
                    break;
                }
                i += 1;
            }
        }
    }

    pub fn set_tab_stop(&self, stop: c_ulong) -> bool {
        let cur = self.control(self.with(|s| s.tab_stop) as c_long);
        if cur != 0 {
            self.op(Op::FormCtlTabbedFrom, cur, 0);
        }
        let c = self.control(stop as c_long);
        if c != 0 && self.op(Op::FormCtlTabable, c, 0) != 0 {
            self.op(Op::FormCtlTabbedTo, c, 0);
            self.with(|s| s.tab_stop = stop);
            return true;
        }
        let cur = self.control(self.with(|s| s.tab_stop) as c_long);
        if cur != 0 {
            self.op(Op::FormCtlTabbedTo, cur, 0);
        }
        false
    }

    pub fn set_popup(&self, c: isize) {
        form_state::set_popup(&mut Life(self), c as Handle);
    }

    fn set_keyboard_captured(&self, c: isize) {
        self.with(|s| s.keyboard = c);
        self.fire(EV_KEYBOARD);
    }

    pub fn set_mouse_over_control(&self, c: isize) {
        let old = self.with(|s| s.mouse_over);
        if old != 0 {
            self.op(Op::FormCtlFireTrigger, old, TRIGGER_MOUSE_OUT);
        }
        self.with(|s| s.mouse_over = c);
        let now = self.with(|s| s.mouse_over);
        if now != 0 {
            self.op(Op::FormCtlFireTrigger, now, TRIGGER_MOUSE_OVER);
        }
        let label = self.with(|s| s.help_label);
        if label == 0 {
            return;
        }
        let poll = self.with(|s| s.help_poll);
        self.op(Op::PollStop, poll, 0);
        if self.with(|s| s.use_tool_tips && s.popup == s.help_label) {
            self.set_popup(0);
        }
        let label = self.with(|s| s.help_label);
        self.op(Op::FormCtlSetDisplayMode, label, HIDDEN as isize);
        let over = self.with(|s| s.mouse_over);
        if over != 0 && self.op(Op::FormHelpGet, over, 0) != 0 {
            let poll = self.with(|s| s.help_poll);
            self.op(Op::PollStart, poll, 0);
            let label = self.with(|s| s.help_label);
            self.op(Op::FormHelpSetText, label, 0);
        }
    }

    pub fn update_help_string(&self) {
        let (label, over) = self.with(|s| (s.help_label, s.mouse_over));
        if label == 0 || over == 0 {
            return;
        }
        self.op(Op::FormHelpGet, over, 0);
        let fitted_tip =
            self.with(|s| s.use_tool_tips && s.popup == s.help_label) && self.op(Op::FormLabelFit, label, 0) != 0;
        if fitted_tip {
            self.op(Op::FormCtlSetDisplayMode, label, HIDDEN as isize);
            self.set_popup(0);
            let label = self.with(|s| s.help_label);
            self.op(Op::FormHelpSetText, label, 0);
            self.update_help_text(true);
        } else {
            let label = self.with(|s| s.help_label);
            self.op(Op::FormHelpSetText, label, 0);
        }
    }

    pub fn update_help_text(&self, force: bool) {
        let poll = self.with(|s| s.help_poll);
        if !(force || self.op(Op::PollRunning, poll, 0) != 0 && self.op(Op::PollPoll, poll, 0) as c_long > 0) {
            return;
        }
        if self.with(|s| s.use_tool_tips) {
            if self.with(|s| s.popup) == 0 {
                self.op(Op::FormVirtHelpLocation, 0, 0);
                let poll = self.with(|s| s.help_poll);
                self.op(Op::PollStop, poll, 0);
                let label = self.with(|s| s.help_label);
                self.op(Op::FormCtlSetDisplayMode, label, SHOWN as isize);
                self.set_popup(label);
            }
        } else {
            let poll = self.with(|s| s.help_poll);
            self.op(Op::PollStop, poll, 0);
            let label = self.with(|s| s.help_label);
            self.op(Op::FormCtlSetDisplayMode, label, SHOWN as isize);
        }
    }

    pub fn update_mouse_location(&self, p: UiPoint) {
        let mut found = 0;
        let captured = self.with(|s| {
            s.mouse = p;
            s.mouse_captured
        });
        if captured != 0 && self.ctl_rect(captured).contains(p) {
            found = captured;
        } else {
            if let Some((g, j)) = form::hit(&Life(self), p) {
                let group = self.with(|s| at(&s.groups, g));
                found = self.op(Op::FormGroupEntity, group, j as isize);
            }
            let (tips, label, popup, mouse) = self.with(|s| (s.use_tool_tips, s.help_label, s.popup, s.mouse));
            if tips && label != 0 && popup == label && self.ctl_rect(label).contains(mouse) {
                let poll = self.with(|s| s.help_poll);
                self.op(Op::PollStop, poll, 0);
                let label = self.with(|s| s.help_label);
                self.op(Op::FormCtlSetDisplayMode, label, HIDDEN as isize);
                self.set_popup(0);
            }
        }
        if self.with(|s| s.mouse_over) != found {
            self.set_mouse_over_control(found);
        }
    }

    /// The address of a field of the state (the getters answering a `const&`): stable while the object lives.
    fn field_addr(&self, f: impl FnOnce(&State) -> *const u8) -> isize {
        // SAFETY: only an address is taken; the boxed object does not move
        f(unsafe { &*self.s.get() }) as isize
    }

    /// One inward call; `f` points at four floats (or is NULL for the calls without).
    ///
    /// # Safety
    /// `f` is NULL or four live floats; the calls taking an address (`AddHotKey`'s hot key, `SetMusicFileName`'s
    /// bytes) get a live one.
    pub unsafe fn call(&self, call: EeUiFormCall, a: isize, b: isize, f: *mut f32) -> isize {
        type C = EeUiFormCall;
        // SAFETY: the contract
        let buf = unsafe { read_buf(f) };
        let rect = buf_rect(&buf);
        match call {
            C::Activate => self.activate(),
            C::Deactivate => self.deactivate(),
            C::Close => form_state::close(&mut Life(self)),
            C::Open => form_state::open(&mut Life(self)),
            C::CopyData => return form_state::copy_data(&mut Life(self)) as isize,
            C::DrawPolygons => return self.draw_polygons(),
            C::DrawText => return self.draw_text(),
            C::DrawBackground => return self.draw_background(rect),
            C::RelayoutForm => self.relayout(rect),
            C::SetHelpTextPopupLocation => self.set_help_text_popup_location(),
            C::UpdateKeyboard => return hotkey::update_keyboard(&mut Life(self)) as isize,
            C::UpdateMouse => return mouse::form_update_mouse(&mut Life(self)) as isize,
            C::SetRectangle => self.with(|s| s.rect = rect),
            C::AddBackground => self.add_background(a),
            C::AddClipRectangle => self.with(|s| s.clips.push(rect)),
            C::AddControl => self.add_control(a, b as c_ulong),
            C::AddDatabaseControl => self.with(|s| s.db.push((a as c_ulong, b))),
            C::ReserveControlIndex => self.reserve_control_index(a as c_ulong),
            C::AddDrawControl => self.with(|s| {
                let c = at(&s.controls, a as c_ulong as c_long);
                if let Some(d) = s.draws.get_mut(a as c_ulong as usize) {
                    *d = c;
                }
            }),
            C::AddEvent => self.with(|s| s.event_data.push((a as c_ulong, b as c_ulong))),
            C::AddHotKey => self.add_hot_key(a, b as c_ulong),
            C::AddHotKeyEvents => self.with(|s| {
                s.pending_events[0] = a;
                s.pending_events[1] = b;
            }),
            C::AddHotKeyEvents2 => self.with(|s| {
                s.pending_events[2] = a;
                s.pending_events[3] = b;
            }),
            C::AddHotKeyBuild => self.add_hot_key_build(a, b as u32 as c_ulong, (b as u64 >> 32) != 0),
            C::ClearAllClipRectangles => self.with(|s| s.clips.clear()),
            C::DrawBitmap => {
                let p = UiPoint { x: f32::from_bits(b as u64 as u32), y: f32::from_bits((b as u64 >> 32) as u32) };
                return self.draw_bitmap(a, rect, p);
            }
            C::StageRect => self.with(|s| s.staged[(a as usize).min(2)] = rect),
            C::DrawTiles => {
                let [r, d, t] = self.with(|s| s.staged);
                background::form_tiles(&mut Tiles { f: self, bitmap: a }, r, d, t);
            }
            C::DrawClippedText => return self.draw_clipped_text(rect, a, b),
            C::AvailableIndex => return self.available_index().map_or(-1, |i| i as isize),
            C::BackgroundById => return self.background_by_id(a as c_ulong),
            C::ControlById => return self.control_by_id(a as c_ulong),
            C::EntityGroupById => return self.entity_group_by_id(a as c_ulong),
            C::ShouldFadeIn => return 0,
            C::ToolTipShadow => {
                // SAFETY: the contract
                unsafe { write_buf(f, rect_buf(form_state::tool_tip_shadow(&rect))) };
            }
            C::LoadBackgrounds => self.each_background(Op::FormBgLoad),
            C::LockDraw => {
                let lock = self.with(|s| s.lock);
                self.op(if a != 0 { Op::FormLockEnter } else { Op::FormLockLeave }, lock, 0);
            }
            C::RedrawIntersectControls => form_state::redraw_intersect_controls(&mut Life(self), rect),
            C::RemoveAllHotKeys => self.remove_all_hot_keys(),
            C::RemoveAllHotKeysForAGroup => self.remove_all_hot_keys_for_group(a as c_ulong),
            C::RemoveBackground => self.remove_background(a),
            C::RemoveControl => self.remove_control(a, b as c_ulong),
            C::RemoveHotKeyKeys => self.remove_hot_key_keys(a, b as c_ulong),
            C::RemoveHotKeyAt => self.remove_hot_key_at(a as c_ulong, b as c_ulong),
            C::SetAllControlsDirty => form_state::set_all_controls_dirty(&mut Life(self)),
            C::SetEvent => {
                if (a as usize) < EVENTS {
                    self.set_event(a as usize, b);
                }
            }
            C::SetFormEvent => self.set_form_event(a as c_ulong, b),
            C::SetFormManager => {
                let old = self.with(|s| s.form_manager);
                if old != 0 {
                    self.op(Op::FormManagerDelete, old, 0);
                }
                self.with(|s| s.form_manager = a);
            }
            C::SetHelpTextLabel => self.with(|s| s.help_label = a),
            C::SetHelpTextTimeDelay => {
                let poll = self.with(|s| s.help_poll);
                self.op(Op::PollSetDelay, poll, a as c_ulong as isize);
            }
            C::SetHotKeyGroupEnabled => self.set_hot_key_group_enabled(a as c_ulong, b != 0),
            C::SetLayout => self.with(|s| {
                let t = |n: u32| ((a as u64 >> n) & 0xffff) as c_ulong;
                s.layout = Layout {
                    left_offset: buf[0],
                    right_offset: buf[1],
                    top_offset: buf[2],
                    bottom_offset: buf[3],
                    left_type: t(0),
                    right_type: t(16),
                    top_type: t(32),
                    bottom_type: t(48),
                };
            }),
            C::SetMusicFileName => {
                // SAFETY: the contract
                let name = unsafe { core::ffi::CStr::from_ptr(a as *const c_char) }.to_bytes_with_nul().to_vec();
                self.with(|s| s.music = name);
            }
            C::SetPopup => self.set_popup(a),
            C::SetTabbing => self.set_tabbing(a != 0),
            C::SetTabStop => return self.set_tab_stop(a as c_ulong) as isize,
            C::SetUseToolTips => self.with(|s| s.use_tool_tips = a != 0),
            C::ShiftTab => {
                let (on, stop) = self.with(|s| (s.tabbing_on, s.tab_stop));
                form::shift_tab(&mut Life(self), on, stop);
            }
            C::Tab => {
                let (on, stop) = self.with(|s| (s.tabbing_on, s.tab_stop));
                form::tab(&mut Life(self), on, stop);
            }
            C::UnloadBackgrounds => self.each_background(Op::FormBgUnload),
            C::UpdateHelpString => self.update_help_string(),
            C::UpdateMouseLocation => self.update_mouse_location(UiPoint { x: buf[0], y: buf[1] }),
            C::SetKeyboardCaptured => self.set_keyboard_captured(a),
            C::SetMouseCaptured => self.with(|s| s.mouse_captured = a),
            C::SetMouseOverControl => self.set_mouse_over_control(a),
            C::UpdateHelpText => self.update_help_text(a != 0),
            C::CheckedRadioButton => {
                let mut i = a as c_long;
                form::checked_radio_button(&Life(self), &mut i);
                return i as isize;
            }
            C::AddCopyDirtyRectangle => form_state::add_copy_dirty_rectangle(&mut Life(self), rect),
            C::AddDirtyRectangle => {
                let popup = self.with(|s| s.popup);
                if popup != 0 && intersects(&rect, &self.ctl_rect(popup)) {
                    self.op(Op::FormCtlSetDirty, popup, 1);
                }
                let n = self.op(Op::FormSurfaces, 0, 0) as c_ulong;
                form_state::add_dirty_rectangle(&mut Life(self), rect, n);
            }
            C::Control => return self.control(a as c_ulong as c_long),
            C::ControlCount => return self.with(|s| s.control_count) as isize,
            C::ControlGroupCount => return self.with(|s| s.group_count) as isize,
            C::DesignedHeight => return self.with(|s| s.designed_height) as isize,
            C::DesignedWidth => return self.with(|s| s.designed_width) as isize,
            C::EntityGroup => return self.with(|s| at(&s.groups, a as c_ulong as c_long)),
            C::FadeOpacity => {
                let v = self.with(|s| s.fade);
                // SAFETY: the contract
                unsafe { write_buf(f, [v, 0.0, 0.0, 0.0]) };
            }
            C::Event => return self.with(|s| s.events.get(a as usize).copied().unwrap_or(0)),
            C::FormId => return self.with(|s| s.form_id) as isize,
            C::FormManager => return self.with(|s| s.form_manager),
            C::HelpTextLabel => return self.with(|s| s.help_label),
            C::HowLongOpen => {
                let now = self.op(Op::FormNow, 0, 0) as c_ulong;
                return now.wrapping_sub(self.with(|s| s.open_time)) as isize;
            }
            C::KeyboardCaptured => return self.with(|s| s.keyboard),
            C::MouseCaptured => return self.with(|s| s.mouse_captured),
            C::UseExclusiveCoords => return self.with(|s| s.exclusive) as isize,
            C::Manager => return self.with(|s| s.manager),
            C::MouseLocation => return self.field_addr(|s| &s.mouse as *const UiPoint as *const u8),
            C::MouseOverControl => return self.with(|s| s.mouse_over),
            C::MusicFileName => return self.field_addr(|s| s.music.as_ptr()),
            C::Rectangle => return self.field_addr(|s| &s.rect as *const UiRect as *const u8),
            C::TabbingOn => return self.with(|s| s.tabbing_on) as isize,
            C::TabStop => return self.with(|s| s.tab_stop) as isize,
            C::UseGdiForText => return self.with(|s| s.use_gdi) as isize,
            C::DrawToolTipShadow => return self.with(|s| s.draw_shadow) as isize,
            C::SetDrawToolTipShadow => self.with(|s| s.draw_shadow = a != 0),
        }
        0
    }
}

/// The text draws of `DrawClippedText`: each piece through the rasterizer.
struct TextOut<'a> {
    f: &'a EeUiForm,
    text: isize,
}

impl TextDraw for TextOut<'_> {
    fn draw(&mut self, r: UiRect) -> c_long {
        crate::draw2d::text(self.text, r, || self.f.op_rect(Op::FormTextDraw, self.text, r))
    }
}

/// The tiles of `DrawBackground(bitmap, rect, draw, tile)`: each piece through `DrawBackground(bitmap, piece, at)`.
struct Tiles<'a> {
    f: &'a EeUiForm,
    bitmap: isize,
}

impl FormTileHost for Tiles<'_> {
    fn draw(&mut self, piece: UiRect, location: (f32, f32)) {
        self.f.draw_bitmap(self.bitmap, piece, UiPoint { x: location.0, y: location.1 });
    }
}

/// The form as the algorithms of rounds 1-9 ask about it.
struct Life<'a>(&'a EeUiForm);

impl Life<'_> {
    fn group(&self, g: c_long) -> isize {
        self.0.with(|s| at(&s.groups, g))
    }
    fn entity_of(&self, g: c_long, j: c_ulong) -> isize {
        self.0.op(Op::FormGroupEntity, self.group(g), j as isize)
    }
    fn ctl(&self, op: Op, c: Handle) -> isize {
        self.0.op(op, c as isize, 0)
    }
}

impl Lists for Life<'_> {
    fn copy_len(&self) -> usize {
        self.0.with(|s| s.copies.len())
    }
    fn copy_at(&self, i: usize) -> UiRect {
        self.0.with(|s| s.copies[i])
    }
    fn copy_erase(&mut self, i: usize) {
        self.0.with(|s| s.copies.remove(i));
    }
    fn copy_push(&mut self, r: UiRect) {
        self.0.with(|s| s.copies.push(r));
    }
    fn dirty_len(&self) -> usize {
        self.0.with(|s| s.dirty.len())
    }
    fn dirty_take_front(&mut self) -> DirtyRect {
        self.0.with(|s| s.dirty.pop_front().unwrap_or_default())
    }
    fn dirty_push_back(&mut self, d: DirtyRect) {
        self.0.with(|s| s.dirty.push_back(d));
    }
}

impl LifeHost for Life<'_> {
    fn surfaces(&self) -> c_ulong {
        self.0.op(Op::FormSurfaces, 0, 0) as c_ulong
    }
    fn rect(&self) -> UiRect {
        self.0.with(|s| s.rect)
    }
    fn layout(&mut self, screen: UiRect) {
        self.0.with(|s| s.rect = s.layout.calculate(&screen, false));
    }
    fn window_size(&self) -> (c_ulong, c_ulong) {
        (self.0.op(Op::FormWindowWidth, 0, 0) as c_ulong, self.0.op(Op::FormWindowHeight, 0, 0) as c_ulong)
    }
    fn now(&self) -> c_ulong {
        self.0.op(Op::FormNow, 0, 0) as c_ulong
    }
    fn open_time(&self) -> c_ulong {
        self.0.with(|s| s.open_time)
    }
    fn set_open_time(&mut self, t: c_ulong) {
        self.0.with(|s| s.open_time = t);
    }
    fn should_fade_in(&mut self) -> bool {
        false
    }
    fn set_fade_opacity(&mut self, v: f32) {
        self.0.with(|s| s.fade = v);
    }
    fn process_event(&mut self, which: FormEvent) -> Option<c_long> {
        let i = match which {
            FormEvent::Update => EV_UPDATE,
            FormEvent::Open => EV_OPEN,
            FormEvent::Close => EV_CLOSE,
        };
        self.0.fire(i).map(|r| r as c_long)
    }
    fn manager_add_dirty(&mut self, r: UiRect) {
        self.0.op_rect(Op::FormManagerAddDirty, 0, r);
    }
    fn manager_process_events(&mut self) {
        self.0.op(Op::FormManagerProcessEvents, 0, 0);
    }
    fn update_help_text(&mut self, force: bool) {
        self.0.update_help_text(force);
    }
    fn reset_tabbing(&mut self) {
        let on = self.0.with(|s| s.tabbing_on);
        self.0.set_tabbing(on);
    }
    fn control_count(&self) -> c_long {
        self.0.with(|s| s.control_count)
    }
    fn control(&self, i: c_long) -> Handle {
        self.0.control(i) as Handle
    }
    fn set_draw_control(&mut self, i: c_long, c: Handle) {
        self.0.with(|s| {
            if i >= 0 {
                if let Some(d) = s.draws.get_mut(i as usize) {
                    *d = c as isize;
                }
            }
        });
    }
    fn group_count(&self) -> c_long {
        self.0.with(|s| s.group_count)
    }
    fn group_enabled(&self, g: c_long) -> bool {
        self.0.op(Op::FormGroupEnabled, self.group(g), 0) != 0
    }
    fn entity_count(&self, g: c_long) -> c_ulong {
        self.0.op(Op::FormGroupCount, self.group(g), 0) as c_ulong
    }
    fn entity(&self, g: c_long, j: c_ulong) -> Handle {
        self.entity_of(g, j) as Handle
    }
    fn popup(&self) -> Handle {
        self.0.with(|s| s.popup) as Handle
    }
    fn set_popup_field(&mut self, c: Handle) {
        self.0.with(|s| s.popup = c as isize);
    }
    fn set_copied_popup(&mut self, c: Handle) {
        self.0.with(|s| s.copied_popup = c as isize);
    }
    fn set_copied_popup_rect(&mut self, r: UiRect) {
        self.0.with(|s| s.copied_popup_rect = r);
    }
    fn popup_draw_count(&self) -> c_ulong {
        self.0.with(|s| s.popup_draw_count)
    }
    fn set_popup_draw_count(&mut self, n: c_ulong) {
        self.0.with(|s| s.popup_draw_count = n);
    }
    fn help_label(&self) -> Handle {
        self.0.with(|s| s.help_label) as Handle
    }
    fn set_help_label(&mut self, c: Handle) {
        self.0.with(|s| s.help_label = c as isize);
    }
    fn mouse_over(&self) -> Handle {
        self.0.with(|s| s.mouse_over) as Handle
    }
    fn draw_shadow(&self) -> bool {
        self.0.with(|s| s.draw_shadow)
    }
    fn background_count(&self) -> usize {
        self.0.with(|s| s.backgrounds.len())
    }
    fn background_open(&mut self, i: usize) {
        let b = self.0.with(|s| s.backgrounds[i]);
        self.0.op(Op::FormBgOpen, b, 0);
    }
    fn background_close(&mut self, i: usize) {
        let b = self.0.with(|s| s.backgrounds[i]);
        self.0.op(Op::FormBgClose, b, 0);
    }
    fn db_count(&self) -> usize {
        self.0.with(|s| s.db.len())
    }
    fn db_table(&self, i: usize) -> Handle {
        let index = self.0.with(|s| s.db[i].0);
        self.0.op(Op::FormTable, index as isize, 0) as Handle
    }
    fn db_control(&self, i: usize) -> Handle {
        self.0.with(|s| s.db[i].1) as Handle
    }
    fn set_db_control(&mut self, i: usize, c: Handle) {
        self.0.with(|s| s.db[i].1 = c as isize);
    }
    fn table_create_control(&mut self, t: Handle) -> Handle {
        self.ctl(Op::FormTableCreateControl, t) as Handle
    }
    fn table_group(&self, t: Handle) -> c_ulong {
        self.ctl(Op::FormTableGroup, t) as c_ulong
    }
    fn table_visible(&self, t: Handle) -> bool {
        self.ctl(Op::FormTableVisible, t) != 0
    }
    fn table_control_type(&self, t: Handle) -> c_ulong {
        self.ctl(Op::FormTableType, t) as c_ulong
    }
    fn table_string_id(&self, t: Handle) -> c_ulong {
        self.ctl(Op::FormTableStringId, t) as c_ulong
    }
    fn table_create_event(&mut self, t: Handle) {
        self.ctl(Op::FormTableCreateEvent, t);
    }
    fn add_control(&mut self, c: Handle, group: c_ulong) {
        self.0.add_control(c as isize, group);
    }
    fn remove_control(&mut self, c: Handle, group: c_ulong) {
        self.0.remove_control(c as isize, group);
    }
    fn delete_control(&mut self, c: Handle) {
        if !c.is_null() {
            self.ctl(Op::DeletePart, c);
        }
    }
    fn event_data_count(&self) -> usize {
        self.0.with(|s| s.event_data.len())
    }
    fn event_data(&self, i: usize) -> (c_ulong, c_ulong) {
        self.0.with(|s| s.event_data[i])
    }
    fn create_form_event(&mut self, id: c_ulong) -> Handle {
        self.0.op(Op::FormCreateFormEvent, id as isize, 0) as Handle
    }
    fn set_form_event(&mut self, kind: c_ulong, e: Handle) {
        self.0.set_form_event(kind, e as isize);
        self.0.op(Op::FormEventCall, e as isize, 1);
    }
    fn ctl_entity_type(&self, c: Handle) -> c_ulong {
        self.ctl(Op::FormCtlEntityType, c) as c_ulong
    }
    fn ctl_mode(&self, c: Handle) -> c_ulong {
        self.ctl(Op::FormCtlDisplayMode, c) as c_ulong
    }
    fn ctl_dirty(&self, c: Handle) -> bool {
        self.ctl(Op::FormCtlDirty, c) != 0
    }
    fn ctl_rect(&self, c: Handle) -> UiRect {
        self.0.ctl_rect(c as isize)
    }
    fn ctl_draw_count(&self, c: Handle) -> c_ulong {
        self.ctl(Op::FormCtlDrawCount, c) as c_ulong
    }
    fn ctl_index(&self, c: Handle) -> c_ulong {
        self.ctl(Op::FormCtlIndex, c) as c_ulong
    }
    fn ctl_copy_data(&mut self, c: Handle) -> c_long {
        self.ctl(Op::PartCopyData, c) as c_long
    }
    fn ctl_set_dirty(&mut self, c: Handle, dirty: bool) {
        self.0.op(Op::FormCtlSetDirty, c as isize, dirty as isize);
    }
    fn ctl_set_display_mode(&mut self, c: Handle, mode: c_ulong) {
        self.0.op(Op::FormCtlSetDisplayMode, c as isize, mode as isize);
    }
    fn ctl_hide(&mut self, c: Handle) {
        self.ctl(Op::FormCtlHide, c);
    }
    fn ctl_set_string_id(&mut self, c: Handle, id: c_ulong) {
        self.0.op(Op::FormCtlSetStringId, c as isize, id as isize);
    }
}

impl FormHost for Life<'_> {
    fn count(&self) -> c_long {
        self.0.with(|s| s.control_count)
    }
    fn exists(&self, i: c_long) -> bool {
        self.0.control(i) != 0
    }
    fn kind(&self, i: c_long) -> c_ulong {
        self.0.op(Op::FormCtlType, self.0.control(i), 0) as c_ulong
    }
    fn checked(&self, i: c_long) -> bool {
        self.0.op(Op::FormCtlChecked, self.0.control(i), 0) != 0
    }
    fn shown_enabled(&self, i: c_long) -> bool {
        let c = self.0.control(i);
        self.0.op(Op::FormCtlDisplayMode, c, 0) as c_ulong == SHOWN && {
            let id = self.0.op(Op::FormCtlId, c, 0) as c_ulong;
            let g = self.0.entity_group_by_id(id);
            self.0.op(Op::FormGroupEnabled, g, 0) != 0
        }
    }
    fn tabable(&self, i: c_long) -> bool {
        self.0.op(Op::FormCtlTabable, self.0.control(i), 0) != 0
    }
    fn set_tab_stop(&mut self, i: c_ulong) -> bool {
        self.0.set_tab_stop(i)
    }
}

impl HitHost for Life<'_> {
    fn group_count(&self) -> c_long {
        self.0.with(|s| s.group_count)
    }
    fn group_enabled(&self, g: c_long) -> bool {
        self.0.op(Op::FormGroupEnabled, self.group(g), 0) != 0
    }
    fn entity_count(&self, g: c_long) -> c_ulong {
        self.0.op(Op::FormGroupCount, self.group(g), 0) as c_ulong
    }
    fn entity_hidden(&self, g: c_long, j: c_ulong) -> bool {
        self.0.op(Op::FormCtlDisplayMode, self.entity_of(g, j), 0) as c_ulong == HIDDEN
    }
    fn entity_rect(&self, g: c_long, j: c_ulong) -> UiRect {
        self.0.ctl_rect(self.entity_of(g, j))
    }
}

impl KeyboardHost for Life<'_> {
    fn captured(&self) -> bool {
        self.0.with(|s| s.keyboard) != 0
    }
    fn captured_ime_enabled(&mut self) -> bool {
        let c = self.0.with(|s| s.keyboard);
        self.0.op(Op::FormCtlImeEnabled, c, 0) != 0
    }
    fn groups(&self) -> usize {
        self.0.with(|s| s.hot_keys.len())
    }
    fn hot_keys(&self, g: usize) -> usize {
        self.0.with(|s| s.hot_keys[g].len())
    }
    fn hot_key(&self, g: usize, j: usize) -> HotKey {
        self.0.with(|s| s.hot_keys[g][j]) as HotKey
    }
    fn always_fire(&self, k: HotKey) -> bool {
        self.0.op(Op::FormHotKeyAlways, k as isize, 0) != 0
    }
    fn fire(&mut self, k: HotKey) -> bool {
        self.0.op(Op::FormHotKeyFire, k as isize, 0) != 0
    }
    fn tabbing(&self) -> bool {
        self.0.with(|s| s.tabbing_on)
    }
    fn tab_just_pressed(&mut self) -> bool {
        self.0.op(Op::FormTabJustPressed, 0, 0) != 0
    }
    fn shift_value(&mut self) -> c_long {
        self.0.op(Op::FormShiftValue, 0, 0) as c_long
    }
    fn shift_tab(&mut self) {
        let (on, stop) = self.0.with(|s| (s.tabbing_on, s.tab_stop));
        form::shift_tab(self, on, stop);
    }
    fn tab(&mut self) {
        let (on, stop) = self.0.with(|s| (s.tabbing_on, s.tab_stop));
        form::tab(self, on, stop);
    }
    fn captured_update(&mut self) -> c_long {
        let c = self.0.with(|s| s.keyboard);
        self.0.op(Op::FormCtlUpdateKeyboard, c, 0) as c_long
    }
    fn tab_stop_update(&mut self) -> c_long {
        let c = self.0.with(|s| at(&s.controls, s.tab_stop as c_long));
        self.0.op(Op::FormCtlUpdateKeyboard, c, 0) as c_long
    }
}

impl FormMouseHost for Life<'_> {
    fn captured(&self) -> Option<Control> {
        let c = self.0.with(|s| s.mouse_captured);
        (c != 0).then_some(c as Control)
    }
    fn mouse_over(&self) -> Option<Control> {
        let c = self.0.with(|s| s.mouse_over);
        (c != 0).then_some(c as Control)
    }
    fn control_update(&mut self, c: Control) -> c_long {
        let p = self.0.with(|s| s.mouse);
        self.0.host.op_point(Op::FormCtlUpdateMouse, c as isize, p) as c_long
    }
    fn wheel(&self) -> bool {
        self.0.op(Op::FormMouseWheel, 0, 0) != 0
    }
    fn wheel_amount(&mut self) -> c_long {
        self.0.op(Op::FormWheelAmount, 0, 0) as c_long
    }
    fn wheel_turned(&mut self, c: Control, amount: c_long) -> c_long {
        self.0.op(Op::FormCtlWheelTurned, c as isize, amount as isize) as c_long
    }
    fn entity(&mut self, e: MouseEntity) -> bool {
        let p = self.0.op(Op::FormMouseEntity, e as isize, 0);
        self.0.with(|s| s.mouse_entities[e as usize] = p);
        p != 0
    }
    fn ask(&mut self, e: MouseEntity, q: EntityQuery) -> bool {
        let p = self.0.with(|s| s.mouse_entities[e as usize]);
        self.0.op(Op::FormMouseAsk, p, q as isize) != 0
    }
    fn has_event(&self, ev: FormMouseEvent) -> bool {
        self.0.with(|s| s.events[mouse_event(ev)]) != 0
    }
    fn process(&mut self, ev: FormMouseEvent) -> c_long {
        let e = self.0.with(|s| s.events[mouse_event(ev)]);
        self.0.op(Op::FormEventProcess, e, 0) as c_long
    }
}

/// The mouse pass's event as the setters' index.
fn mouse_event(ev: FormMouseEvent) -> usize {
    match ev {
        FormMouseEvent::Move => EV_MOVE,
        FormMouseEvent::WheelMove => EV_WHEEL,
        other => other as usize,
    }
}

// ---- the C ABI

/// A new Rust form for the C++ `form`: `UIForm(controls, groups, manager, id, gdi)`.
#[no_mangle]
pub extern "C" fn ee_ui_form_new(
    form: *mut c_void,
    host: EeUiHostFn,
    controls: c_ulong,
    groups: c_ulong,
    manager: *mut c_void,
    id: c_ulong,
    gdi: bool,
) -> *mut EeUiForm {
    crate::direct::made(
        crate::direct::Kind::Form,
        form,
        Box::into_raw(Box::new(EeUiForm::new(
            Host { obj: form, f: host },
            controls,
            groups,
            manager as isize,
            id,
            gdi,
        ))),
    )
}

/// A new Rust form from the database record `data` (a `UIDbDataForms*`) and its lower-cased music file name.
///
/// # Safety
/// `data` is a live record; `music` NUL-terminated bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_form_new_db(
    form: *mut c_void,
    host: EeUiHostFn,
    manager: *mut c_void,
    data: *const c_void,
    music: *const c_char,
    gdi: bool,
) -> *mut EeUiForm {
    // SAFETY: the contract
    let d = unsafe { &*(data as *const DbForm) };
    // SAFETY: the contract
    let music = unsafe { core::ffi::CStr::from_ptr(music) }.to_bytes_with_nul();
    crate::direct::made(
        crate::direct::Kind::Form,
        form,
        Box::into_raw(Box::new(EeUiForm::new_db(Host { obj: form, f: host }, manager as isize, d, music, gdi))),
    )
}

/// The destructor: the events, hot keys, groups, backgrounds, database controls, poll, form manager and lock
/// released; the object freed.
///
/// # Safety
/// `form` came from `ee_ui_form_new`/`_new_db` (or is NULL) and is not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_form_free(form: *mut EeUiForm) {
    crate::direct::forget(crate::direct::Kind::Form, form as *const core::ffi::c_void);
    if form.is_null() {
        return;
    }
    // SAFETY: the contract
    unsafe { (*form).release() };
    // SAFETY: the contract
    drop(unsafe { Box::from_raw(form) });
}

/// One of UIForm's methods ([`EeUiFormCall`]).
///
/// # Safety
/// `form` is live; `f` is NULL or four live floats; see [`EeUiForm::call`].
#[no_mangle]
pub unsafe extern "C" fn ee_ui_form_call(
    form: *const EeUiForm,
    call: EeUiFormCall,
    a: isize,
    b: isize,
    f: *mut f32,
) -> isize {
    // SAFETY: live (the contract); a shared reference: the calls out re-enter through this function
    unsafe { (*form).call(call, a, b, f) }
}

#[cfg(test)]
#[path = "formobj_tests.rs"]
mod tests;
