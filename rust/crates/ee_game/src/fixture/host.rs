//! What the fixture asks of the game (TEST ONLY): one C++ function (`EETestFixture.cpp`'s `EEFixtureHost`) taking an
//! operation and its arguments in an [`EeFixtureCall`]. The UI's controls, the input devices, the game's messages
//! and actions, the picker and the units' methods are C++ calls; the world's plain members are read here through
//! the world views (ee_sim::world).

use std::ffi::{c_char, CString};
use std::sync::OnceLock;

/// The operations (`EeFixtureCall::op`). The arguments and answers of each are documented on it: `h`/`h2` handles,
/// `i` and `f` numbers in and out, `s` a text in, `ids`/`n` ids in, `sink` texts and ids out.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeFixtureOp {
    /// `s` to stderr (fputs, fflush)
    Stderr = 1,
    /// exit(i0)
    Exit,
    /// PlatSimSetTimeFactor(i0, i1) -> 1 taken, 0 refused
    SetTimeFactor,
    /// the platform switch i0 (0 the seed, 1 no human defeat, 2 the port offset) set to i1
    SetSwitch,
    /// the debug output and the fault handler (EETestFixtureConfigure's), or with i0 = 1 the game's test hooks
    Install,
    /// the harness clock: i0/i1 its factor, i2 PlatTimeMilliseconds
    Clock,
    /// LockUIForCopyData (i0 = 1) or UnlockUIForCopyData (i0 = 0)
    UiLock,
    /// the form on top -> its handle (0: none); i0 its id, i1 its control count, i2 the control under the pointer
    /// (-1: none); the stacked forms' ids into the sink (the manager's first 64)
    TopForm,
    /// the window -> 1 (i0, i1 its size in the game's pixels), 0 none
    Window,
    /// control i0 of form h -> -1 none, else its kind (0 other, 1 combo, 2 check, 3 list); i1 whether it has a
    /// trigger event; a combo's or list's selection (i2, 32-bit), its line count (i3) and its first 64 lines into the
    /// sink; a check box's state (i2)
    Control,
    /// control i0 of form h: its trigger event fired (a radio button checked first) -> 1, 0 no trigger
    Click,
    /// control i0 of form h: a combo's line i1 chosen -> 1, 0 not a combo or no such line
    ComboSelect,
    /// control i0 of form h: a check box set to i1 -> 1, 0 not a check box
    CheckSet,
    /// control i0 of form h: a list's line i1 selected -> 1, 0 not a list or no such line
    ListSelect,
    /// the first control of form h whose button-up event is the screen event i0, clicked -> 1, 0 none
    ClickEvent,
    /// the screen event i0 on form h
    ScreenEvent,
    /// the chat log's test line -> 1, 0 no user interface
    Chat,
    /// the camera zoom set to f0
    Zoom,
    /// the pointer set to (f0, f1) under the input lock -> 1 and (f0, f1) where it is now, 0 no mouse
    MouseSet,
    /// the pointer put back at (f0, f1) under the input lock when it is elsewhere -> 1 put back, 0 there, -1 no mouse
    MouseFix,
    /// ForceUIUpdate
    UiUpdate,
    /// ISHandleEvent of a platform event: i0 its kind (EE_FIXTURE_KEY_DOWN ..), i1 its key code or button, f0 the wheel
    Input,
    /// the keyboard's Shift and Control -> 0 none, 1 Shift alone, 2 anything else
    Modifiers,
    /// the state bits: 1 gPhysics, 2 the map, 4 the world's lock, 8 gMouse, 16 gISKeyboard, 32 gUserInterface,
    /// 64 the WON lobby open, 128 gUIManager
    State,
    /// the game form's selection control (form h's kMainGameSelectUnits) -> its handle, 0 none
    SelectControl,
    /// selection control h under the UI lock: i0 the form's mouse captured, i1 dragging, (f0, f1) the drag's end
    SelectState,
    /// the game view's rectangle: f0 left, f1 top, f2 right, f3 bottom
    ViewRect,
    /// a game message sent as local input (a UI action): i0 0 EEMSelectTheseUnits(ids), 1 EEMKillUnit(0),
    /// 2 EEMPathTo(f0, f1), 3 EEMStopActiveGroup
    Post,
    /// a UI action that calls ee_fixture_mark when it runs
    Mark,
    /// EEACenterCamera on tile (i0, i1)
    CenterCamera,
    /// the visible status button of the form on top for button object i0 -> 1 and its centre (f0, f1), 0 none
    StatusButton,
    /// the world's lock: i0 0 read, 1 read unlock, 2 write, 3 write unlock -> 0 no lock
    WorldLock,
    /// the physics -> 1 and i0 its game time, i1 its ticks done, i2 game over, 0 none
    Physics,
    /// the communications -> 1 and i0 the CTs processed, i1 the last received, i2 paused, i3 multiplayer, 0 none
    Comms,
    /// the camera -> 1 with a map: f0..f2 its focus, f3, f4 its move's end, f5 its zoom, i0 its mode, i1 the map's
    /// size, i2 and i3 whether the focus and the end are points of the terrain mesh; 0 no map
    Camera,
    /// the local player's active group's unit ids into the sink -> 1, 0 no player or group
    ActiveGroup,
    /// gWorld.GetUnit(i0) -> its handle (0 none)
    UnitById,
    /// player h's GetUnit(i0) -> its handle (0 none)
    PlayerUnit,
    /// unit h's CanDelete()
    CanDelete,
    /// the picker's unit at (f0, f1) with resource preference i0 -> its id, -1 none
    Pick,
    /// EEPointUnitOrTile at (f0, f1) on a tile the local player sees -> 1 and its world point (f0, f1, f2), 0
    PointTile,
    /// unit h's DistanceSquared3D to (f0, f1, f2) -> f0
    Distance,
    /// unit h's CanGather(unit h2, i0, i1, i2, i3)
    CanGather,
    /// the continents let unit h reach tile (i0, i1)
    CanGetTo,
    /// EEIsSeenBy(unit h, player h2, true)
    SeenBy,
    /// unit h's class has the action kActionGetNextPatrolPoint
    PatrolAction,
    /// unit h's nearest town's patrol route: its point nearest the unit -> the point's id and h2 the route, -1 none
    NearestRoute,
    /// goal h -> its kind; i0 the class (0 gather, 1 location, 2 attack, 3 ground, 4 patrol, 5 build, 6 explore,
    /// 7 other), i1 its target's id (-1: none), h2 its target's handle or its patrol group, (f0, f1) its point
    Goal,
    /// unit h's last queued goal -> its handle (0: none)
    LastQueued,
    /// unit h's citizen build queue's units (handles) into the sink
    BuildQueue,
    /// unit h's hit points set to i0 (TEST ONLY, the world's write lock held)
    SetHitPoints,
    /// message h's value i0 as i32 (i1 0), bool (i1 1) -> the value; as float (i1 2) -> f0
    MessageValue,
    /// the calamity classes -> their count
    CalamityCount,
    /// calamity class row i0 -> 1, i1 its enum, i2 its target kind; 0 none
    CalamityClass,
    /// a calamity of row i0 cast by unit i1 of player i2: i3 0 at unit i4, 1 at (i4, i5), 2 global -> 1 cast
    CalamityCast,
    /// the time alive of the last living calamity of enum i0 -> it, -1 none
    CalamityAlive,
    /// the WON lobby's command `s` (seq i0) posted to its loop -> 1, 0 the lobby is not open
    LobbyPost,
    /// the WON lobby's last snapshot into the sink (a new one asked for) -> 1, 0 the lobby is not open
    LobbySnapshot,
}

/// One call's arguments and answers (see [`EeFixtureOp`]).
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeFixtureCall {
    pub op: EeFixtureOp,
    pub h: u64,
    pub h2: u64,
    pub i: [i64; 6],
    pub f: [f32; 6],
    pub s: *const c_char,
    pub ids: *const i64,
    pub n: usize,
    pub sink: *mut EeFixtureSink,
}

/// The texts and ids a call answers with (`ee_fixture_sink_text`, `ee_fixture_sink_id`).
#[derive(Debug, Default)]
pub struct EeFixtureSink {
    pub texts: Vec<Vec<u8>>,
    pub ids: Vec<i64>,
}

/// The host: one call -> its result.
pub type EeFixtureHostFn = unsafe extern "C" fn(*mut EeFixtureCall) -> i64;

static HOST: OnceLock<EeFixtureHostFn> = OnceLock::new();

/// The host (EETestFixtureConfigure registers it, first).
pub fn set_host(host: EeFixtureHostFn) {
    let _ = HOST.set(host);
}

/// A call's answer: its result, the call (its `i`, `f` and `h2` out) and the sink.
pub struct Answer {
    pub result: i64,
    pub call: EeFixtureCall,
    pub sink: EeFixtureSink,
}

impl Answer {
    pub fn i(&self, k: usize) -> i64 {
        self.call.i[k]
    }
    pub fn f(&self, k: usize) -> f32 {
        self.call.f[k]
    }
}

/// A call being built.
pub struct Call {
    call: EeFixtureCall,
    text: Option<CString>,
    ids: Vec<i64>,
}

impl Call {
    pub fn new(op: EeFixtureOp) -> Call {
        Call {
            call: EeFixtureCall {
                op,
                h: 0,
                h2: 0,
                i: [0; 6],
                f: [0.0; 6],
                s: std::ptr::null(),
                ids: std::ptr::null(),
                n: 0,
                sink: std::ptr::null_mut(),
            },
            text: None,
            ids: Vec::new(),
        }
    }
    pub fn h(mut self, h: u64) -> Call {
        self.call.h = h;
        self
    }
    pub fn h2(mut self, h: u64) -> Call {
        self.call.h2 = h;
        self
    }
    pub fn i(mut self, values: &[i64]) -> Call {
        self.call.i[..values.len()].copy_from_slice(values);
        self
    }
    pub fn f(mut self, values: &[f32]) -> Call {
        self.call.f[..values.len()].copy_from_slice(values);
        self
    }
    pub fn s(mut self, text: &str) -> Call {
        self.text = Some(CString::new(text.replace('\0', "")).unwrap_or_default());
        self
    }
    pub fn ids(mut self, ids: &[i64]) -> Call {
        self.ids = ids.to_vec();
        self
    }
    /// The call made (0 and nothing answered when no host is registered: a unit test).
    pub fn run(mut self) -> Answer {
        let mut sink = EeFixtureSink::default();
        self.call.s = self.text.as_ref().map_or(std::ptr::null(), |t| t.as_ptr());
        self.call.ids = self.ids.as_ptr();
        self.call.n = self.ids.len();
        self.call.sink = &mut sink;
        let result = match HOST.get() {
            // SAFETY: the host's contract: the call's pointers live through the call (the text, the ids, the sink)
            Some(host) => unsafe { host(&mut self.call) },
            None => 0,
        };
        self.call.s = std::ptr::null();
        self.call.ids = std::ptr::null();
        self.call.sink = std::ptr::null_mut();
        Answer { result, call: self.call, sink }
    }
    /// The call made, its result only.
    pub fn go(self) -> i64 {
        self.run().result
    }
}

/// A text line to stderr through the C library's stream (the C++ fixture's `fprintf(stderr, ..)` and `fflush`).
pub fn stderr(text: &str) {
    if HOST.get().is_none() {
        eprint!("{text}");
        return;
    }
    Call::new(EeFixtureOp::Stderr).s(text).go();
}

/// The world's read lock taken for the closure's time (none when the world has no lock: the closure is not run).
pub fn with_read_lock<T>(f: impl FnOnce() -> T) -> Option<T> {
    if Call::new(EeFixtureOp::WorldLock).i(&[0]).go() == 0 {
        return None;
    }
    let v = f();
    Call::new(EeFixtureOp::WorldLock).i(&[1]).go();
    Some(v)
}

/// The world's write lock taken for the closure's time.
pub fn with_write_lock<T>(f: impl FnOnce() -> T) -> Option<T> {
    if Call::new(EeFixtureOp::WorldLock).i(&[2]).go() == 0 {
        return None;
    }
    let v = f();
    Call::new(EeFixtureOp::WorldLock).i(&[3]).go();
    Some(v)
}

/// The UI's copy lock taken for the closure's time.
pub fn with_ui_lock<T>(f: impl FnOnce() -> T) -> T {
    Call::new(EeFixtureOp::UiLock).i(&[1]).go();
    let v = f();
    Call::new(EeFixtureOp::UiLock).i(&[0]).go();
    v
}

pub const EE_FIXTURE_STATE_PHYSICS: i64 = 1;
pub const EE_FIXTURE_STATE_MAP: i64 = 2;
pub const EE_FIXTURE_STATE_LOCK: i64 = 4;
pub const EE_FIXTURE_STATE_MOUSE: i64 = 8;
pub const EE_FIXTURE_STATE_KEYBOARD: i64 = 16;
pub const EE_FIXTURE_STATE_UI: i64 = 32;
pub const EE_FIXTURE_STATE_LOBBY: i64 = 64;
pub const EE_FIXTURE_STATE_UI_MANAGER: i64 = 128;

/// The state bits ([`EeFixtureOp::State`]).
pub fn state() -> i64 {
    Call::new(EeFixtureOp::State).go()
}

/// The platform's input events ([`EeFixtureOp::Input`]'s i0; the host maps them to platform/PlatWindow.h's
/// `PLAT_EVENT_KEY_DOWN`, `_KEY_UP`, `_MOUSE_DOWN`, `_MOUSE_UP`, `_MOUSE_WHEEL`).
pub const EE_FIXTURE_KEY_DOWN: i64 = 0;
pub const EE_FIXTURE_KEY_UP: i64 = 1;
pub const EE_FIXTURE_MOUSE_DOWN: i64 = 2;
pub const EE_FIXTURE_MOUSE_UP: i64 = 3;
pub const EE_FIXTURE_MOUSE_WHEEL: i64 = 4;

/// A platform event: a key's (its scan code) or a mouse button's (1 left, 3 right).
pub fn input(kind: i64, code: i64) {
    Call::new(EeFixtureOp::Input).i(&[kind, code]).go();
}

/// The pointer to (x, y) under the input lock -> where it is now (None: no mouse).
pub fn mouse_set(x: f32, y: f32) -> Option<(f32, f32)> {
    let a = Call::new(EeFixtureOp::MouseSet).f(&[x, y]).run();
    (a.result != 0).then(|| (a.f(0), a.f(1)))
}

pub fn ui_update() {
    Call::new(EeFixtureOp::UiUpdate).go();
}

pub fn sleep_ms(ms: u64) {
    std::thread::sleep(std::time::Duration::from_millis(ms));
}
