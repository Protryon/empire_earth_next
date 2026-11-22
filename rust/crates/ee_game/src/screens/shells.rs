//! The generic C++ shells' kinds (UI cluster round 3): the game's control classes whose C++ classes are gone.
//!
//! The toolkit's C++ calls a control's virtuals (`MouseLButtonUp`, `ButtonUp`, `ChangeSelection`, ...); a game class
//! was a C++ subclass for its overrides. Now one C++ shell per toolkit base (`ee/EEUIShell.h`: `EEUIShellButton`,
//! `EEUIShellRadio`, `EEUIShellCheck`, `EEUIShellCombo`, `EEUIShellList`, `EEUIShellEdit`, `EEUIShellSlider`) holds a
//! [`Shell`]: the kind (`EE_SCREENS_SK_*`, one per deleted class) with its state and its overrides. A shell's virtual
//! asks the kind when the kind's mask ([`Kind::overrides`]) has its bit (`EE_SCREENS_SV_*`), else runs the base's;
//! the kind calls the base's back (`EE_SCREENS_SH_BASE + v`). Everything else the kind needs from C++ (the toolkit's
//! C++ objects, the game's) is one op of the shells' host, `ee_screenscpp_shell` (`EEUIShell.cpp`), each a step of
//! the deleted class's code. A C++ caller of a deleted class's own method calls the kind's op ([`Kind::op`]).
//!
//! A host call can come back into the same shell (a base `ButtonUp` fires an event that reads the control): the
//! kinds are used through `&self` only, their state is `Cell`s.

use core::cell::Cell;
use core::ffi::{c_void, CStr};

/// A C++ control (`UIControl*`).
pub type Cpp = *mut c_void;

// ---- the shells' virtuals (the bit numbers of a kind's override mask)

/// `CopyData()`.
pub const EE_SCREENS_SV_COPY_DATA: u32 = 0;
/// `DrawPolygons()`.
pub const EE_SCREENS_SV_DRAW_POLYGONS: u32 = 1;
/// `DrawTextA()`.
pub const EE_SCREENS_SV_DRAW_TEXT: u32 = 2;
/// `MouseLButtonDown()`.
pub const EE_SCREENS_SV_MOUSE_L_DOWN: u32 = 3;
/// `MouseLButtonUp()`.
pub const EE_SCREENS_SV_MOUSE_L_UP: u32 = 4;
/// `MouseLButtonDoublePressed()`.
pub const EE_SCREENS_SV_MOUSE_L_DOUBLE: u32 = 5;
/// `MouseRButtonDown()`.
pub const EE_SCREENS_SV_MOUSE_R_DOWN: u32 = 6;
/// `MouseRButtonUp()`.
pub const EE_SCREENS_SV_MOUSE_R_UP: u32 = 7;
/// `UpdateKeyboard()`.
pub const EE_SCREENS_SV_UPDATE_KEYBOARD: u32 = 8;
/// `TabbedTo()`.
pub const EE_SCREENS_SV_TABBED_TO: u32 = 9;
/// `TabbedFrom()`.
pub const EE_SCREENS_SV_TABBED_FROM: u32 = 10;
/// `MouseMove(f[0], f[1])`.
pub const EE_SCREENS_SV_MOUSE_MOVE: u32 = 11;
/// `SetDirty(a)`.
pub const EE_SCREENS_SV_SET_DIRTY: u32 = 12;
/// `GetTabable()`.
pub const EE_SCREENS_SV_GET_TABABLE: u32 = 13;
/// `GetHelpString(*a)` (a `UWideString*`).
pub const EE_SCREENS_SV_HELP_STRING: u32 = 14;
/// A button's `ButtonDown()`.
pub const EE_SCREENS_SV_BUTTON_DOWN: u32 = 15;
/// A button's (or a list box's) `ButtonUp()`.
pub const EE_SCREENS_SV_BUTTON_UP: u32 = 16;
/// A button's `MouseOver()`.
pub const EE_SCREENS_SV_MOUSE_OVER: u32 = 17;
/// A radio button's `Check()`.
pub const EE_SCREENS_SV_CHECK: u32 = 18;
/// A radio button's `UnCheck()`.
pub const EE_SCREENS_SV_UNCHECK: u32 = 19;
/// A combo box's `ChangeSelection()`.
pub const EE_SCREENS_SV_CHANGE_SELECTION: u32 = 20;
/// A list box's `ChangeSelectionIndex(a, b & 1, b & 2)`.
pub const EE_SCREENS_SV_CHANGE_SELECTION_INDEX: u32 = 21;
/// A slider's `ChangeRange(a)`.
pub const EE_SCREENS_SV_CHANGE_RANGE: u32 = 22;
/// An edit box's `ProcessEnterKey()`.
pub const EE_SCREENS_SV_PROCESS_ENTER: u32 = 23;
/// A form's (`EEUIShellForm`): `Activate()`, `Deactivate()`, `DrawBackground(f[0..4])`, `RelayoutForm(f[0..4])`,
/// `SetHelpTextPopupLocation()`, `UpdateMouse()`, `UpdateTranslatedKey((const char*)a, b)`, `Close()`, `Open()`,
/// `SetRectangle(f[0..4])` (and the common `CopyData`, `DrawPolygons`, `DrawTextA`, `UpdateKeyboard`); rectangles as
/// left, top, right, bottom.
pub const EE_SCREENS_SV_ACTIVATE: u32 = 24;
pub const EE_SCREENS_SV_DEACTIVATE: u32 = 25;
pub const EE_SCREENS_SV_DRAW_BACKGROUND: u32 = 26;
pub const EE_SCREENS_SV_RELAYOUT: u32 = 27;
pub const EE_SCREENS_SV_HELP_POPUP: u32 = 28;
pub const EE_SCREENS_SV_UPDATE_MOUSE: u32 = 29;
pub const EE_SCREENS_SV_TRANSLATED_KEY: u32 = 30;
pub const EE_SCREENS_SV_CLOSE: u32 = 31;
pub const EE_SCREENS_SV_OPEN: u32 = 32;
pub const EE_SCREENS_SV_SET_RECTANGLE: u32 = 33;

const fn bit(v: u32) -> u64 {
    1 << v
}

// ---- the kinds (one per deleted C++ class; `EEUIShell::KindName` keeps the class's name)

/// `EEUIBColor` (a button; args: the player colour).
pub const EE_SCREENS_SK_COLOR: u32 = 1;
/// `EESEPSSetPlayers` (a button; args: the font).
pub const EE_SCREENS_SK_SET_PLAYERS: u32 = 2;
/// `EEUIFSERBBrush` (a radio button; args: the brush group, the brush size).
pub const EE_SCREENS_SK_BRUSH: u32 = 3;
/// `EESETSCComboBox` (a combo box).
pub const EE_SCREENS_SK_CONDITION_COMBO: u32 = 4;
/// `EESETSEComboBox` (a combo box).
pub const EE_SCREENS_SK_EFFECT_COMBO: u32 = 5;
/// `EESETSEScenarioComboBox` (a combo box; args: the screen, the mode).
pub const EE_SCREENS_SK_SCENARIO_COMBO: u32 = 6;
/// `EESETSTriggerUIComboBox` (a combo box; args: the jump's kind).
pub const EE_SCREENS_SK_TRIGGER_COMBO: u32 = 7;
/// `EESETSAComboBox` (a combo box).
pub const EE_SCREENS_SK_OBJECT_COMBO: u32 = 8;
/// `EESETSUICBPlayer` (a combo box).
pub const EE_SCREENS_SK_PLAYER_COMBO: u32 = 9;
/// `EEUIFMGErrorLog` (a string list box).
pub const EE_SCREENS_SK_ERROR_LOG: u32 = 10;
/// `EEUIFSEESElevation` (a slider).
pub const EE_SCREENS_SK_ELEVATION_SLIDER: u32 = 11;
/// `EESETSAEditBox` (an edit box).
pub const EE_SCREENS_SK_OBJECT_EDIT: u32 = 12;
/// `EESETSOUnitList` (a string list box).
pub const EE_SCREENS_SK_UNIT_LIST: u32 = 13;

// ---- the kinds' ops (`EEUIShell::Op`: a deleted class's own methods)

/// `EEUIBColor::GetColor()`.
pub const EE_SCREENS_SKOP_COLOR: u32 = 1;
/// `EEUIBColor::SetColor(a)`.
pub const EE_SCREENS_SKOP_SET_COLOR: u32 = 2;
/// `EEUIFSERBBrush::mBrushSize`.
pub const EE_SCREENS_SKOP_BRUSH_SIZE: u32 = 3;
/// `EESETSEScenarioComboBox::mMode = a`.
pub const EE_SCREENS_SKOP_SET_MODE: u32 = 4;
/// `EESETSEScenarioComboBox::mMode`.
pub const EE_SCREENS_SKOP_MODE: u32 = 9;
/// `EESETSUICBPlayer::GetSelectedPlayer()`.
pub const EE_SCREENS_SKOP_SELECTED_PLAYER: u32 = 5;
/// `EESETSUICBPlayer::SetSelectedPlayer(a)`.
pub const EE_SCREENS_SKOP_SET_SELECTED_PLAYER: u32 = 6;
/// `EESETSUICBPlayer::SetFlags(a)` (then its list).
pub const EE_SCREENS_SKOP_SET_FLAGS: u32 = 7;
/// `EESETSUICBPlayer::SetPlayerCount(a)`.
pub const EE_SCREENS_SKOP_SET_PLAYER_COUNT: u32 = 8;

// ---- the host's ops (`ee_screenscpp_shell(control, op, a, b, f)`)

/// `EE_SCREENS_SH_BASE + v`: the base class's virtual `v` (`EE_SCREENS_SV_*`) on the shell.
pub const EE_SCREENS_SH_BASE: u32 = 0x100;
/// `EE_SCREENS_SH_FORM_BASE + v`: a form shell's `UIForm` virtual `v` (the host's object is the `UIForm*`).
pub const EE_SCREENS_SH_FORM_BASE: u32 = 0x200;
/// A form shell's own virtual `a` (through its vtable).
pub const EE_SCREENS_SH_FORM_SELF: u32 = 0xff;
/// `SetBgOpacity(f[0])`.
pub const EE_SCREENS_SH_BG_OPACITY: u32 = 1;
/// `SetBgColor(GEColor(f[0], f[1], f[2]))`.
pub const EE_SCREENS_SH_BG_COLOR: u32 = 2;
/// `SetBgColor(EEGetPlayerColor(a))`.
pub const EE_SCREENS_SH_BG_PLAYER_COLOR: u32 = 3;
/// The text (`a` 0: the control's `GetGEText()`, 1: its label's) coloured `f[0..3]` and transparent.
pub const EE_SCREENS_SH_TEXT_FG: u32 = 4;
/// A list box's `GetSelectedBackground()->SetColor(f[0..3])`.
pub const EE_SCREENS_SH_SELECTED_COLOR: u32 = 5;
/// A list box's `GetSelectedBackground()->SetOpacity(f[0])`.
pub const EE_SCREENS_SH_SELECTED_OPACITY: u32 = 6;
/// A button's `SetText(resource string a, (GEFont*)b)`.
pub const EE_SCREENS_SH_BUTTON_TEXT: u32 = 7;
/// A combo box's `GetSelectionIndex()`.
pub const EE_SCREENS_SH_SELECTION_INDEX: u32 = 8;
/// A combo box's `SetCurrentSelection(a)`.
pub const EE_SCREENS_SH_SET_SELECTION: u32 = 9;
/// A combo box's `DeleteAllEntries()`.
pub const EE_SCREENS_SH_DELETE_ALL: u32 = 10;
/// A combo box's `AppendLine(resource string a)`.
pub const EE_SCREENS_SH_APPEND_STRING: u32 = 11;
/// A combo box's lines "resource string a" printed with 1..=b (`sprintf`), appended.
pub const EE_SCREENS_SH_APPEND_NUMBERED: u32 = 12;
/// A list box's `GetScrollBarCaptured()`.
pub const EE_SCREENS_SH_SCROLL_CAPTURED: u32 = 13;
/// `ReleaseCapturedMouse()`.
pub const EE_SCREENS_SH_RELEASE_MOUSE: u32 = 14;
/// `FireTriggerEvent(a)`.
pub const EE_SCREENS_SH_FIRE: u32 = 15;
/// A slider's `GetCurrentRangeValue()`.
pub const EE_SCREENS_SH_SLIDER_VALUE: u32 = 16;
/// A slider's `GetMaxRange()`.
pub const EE_SCREENS_SH_SLIDER_MAX: u32 = 17;
/// The form's label of id `a`: `SetText((long)b)`.
pub const EE_SCREENS_SH_LABEL_NUMBER: u32 = 18;
/// `GetId()`.
pub const EE_SCREENS_SH_ID: u32 = 19;
/// The trigger screen's sub-screen `a`: `RefreshControls()`.
pub const EE_SCREENS_SH_TRIGGER_REFRESH: u32 = 0x40;
/// The trigger screen's `RefreshScreen()`.
pub const EE_SCREENS_SH_TRIGGER_REFRESH_SCREEN: u32 = 0x41;
/// `EESETriggerJump(a, b)` (the jump to the entry of kind `a`, line `b`).
pub const EE_SCREENS_SH_TRIGGER_JUMP: u32 = 0x42;
/// The object screen's `UpdateSelection()`.
pub const EE_SCREENS_SH_OBJECT_UPDATE_SELECTION: u32 = 0x43;
/// The object screen's `Store()`.
pub const EE_SCREENS_SH_OBJECT_STORE: u32 = 0x44;
/// The player tab's `CopyFirstPlayerToAll()`.
pub const EE_SCREENS_SH_COPY_FIRST_TO_ALL: u32 = 0x45;
/// The brush group `a`'s `mBrushSize = b`.
pub const EE_SCREENS_SH_BRUSH_SIZE: u32 = 0x46;

/// `kControlTriggerSelectionChanged` (lle/UIControl.h).
const TRIGGER_SELECTION_CHANGED: i64 = 0x3eb;
/// `kSETriggerScreenCondition`, `kSETriggerScreenEffect`, `kSETriggerScreenArea` (editor_ids.rs).
const SCREEN_CONDITION: i64 = 1;
const SCREEN_EFFECT: i64 = 2;
const SCREEN_AREA: i64 = 4;
/// `kSEElevationStrengthLabel` (editor_ids.rs).
const ELEVATION_STRENGTH_LABEL: i64 = 0x9f;

/// The shells' calls out: one op of `ee_screenscpp_shell` on the control `ctl`.
pub trait ShellHost {
    fn call(&self, ctl: Cpp, op: u32, a: i64, b: i64, f: &mut [f32; 4]) -> i64;
}

/// What a kind's method sees: its control and the host.
pub struct Ctx<'h> {
    pub ctl: Cpp,
    pub host: &'h dyn ShellHost,
}

impl Ctx<'_> {
    pub fn call(&self, op: u32, a: i64, b: i64) -> i64 {
        let mut f = [0.0f32; 4];
        self.host.call(self.ctl, op, a, b, &mut f)
    }
    pub fn call_f(&self, op: u32, a: i64, f: [f32; 4]) -> i64 {
        let mut f = f;
        self.host.call(self.ctl, op, a, 0, &mut f)
    }
    /// The base class's virtual `v`.
    pub fn base(&self, v: u32, a: i64, b: i64, f: &mut [f32; 4]) -> i64 {
        self.host.call(self.ctl, EE_SCREENS_SH_BASE + v, a, b, f)
    }
    pub fn base0(&self, v: u32) -> i64 {
        self.base(v, 0, 0, &mut [0.0; 4])
    }
    /// A form shell's `UIForm` virtual `v`.
    pub fn form_base(&self, v: u32, a: i64, b: i64, f: &mut [f32; 4]) -> i64 {
        self.host.call(self.ctl, EE_SCREENS_SH_FORM_BASE + v, a, b, f)
    }
    /// A form shell's own virtual `v` (a kind's override runs).
    pub fn form_self(&self, v: u32) -> i64 {
        self.call(EE_SCREENS_SH_FORM_SELF, i64::from(v), 0)
    }
}

/// One deleted class: its name, the virtuals it overrides, its constructor's body, its overrides and its methods.
pub trait Kind {
    /// The class's name, as the gates print it.
    fn name(&self) -> &'static CStr;
    /// The overridden virtuals (bits of `EE_SCREENS_SV_*`).
    fn overrides(&self) -> u64 {
        0
    }
    /// The constructor's body (after the base's constructor).
    fn init(&self, _c: &Ctx) {}
    /// An overridden virtual `v`.
    fn virt(&self, c: &Ctx, v: u32, a: i64, b: i64, f: &mut [f32; 4]) -> i64 {
        c.base(v, a, b, f)
    }
    /// The destructor's body (before the base's destructor).
    fn free(&self, _c: &Ctx) {}
    /// One of the class's own methods (`EE_SCREENS_SKOP_*`).
    fn op(&self, _c: &Ctx, _op: u32, _a: i64, _b: i64, _f: &mut [f32; 4]) -> i64 {
        0
    }
}

/// A shell's Rust half (opaque to C++): its control and its kind.
pub struct EeScreensShell {
    pub ctl: Cpp,
    pub kind: Box<dyn Kind>,
}

fn arg(args: &[i64], i: usize) -> i64 {
    args.get(i).copied().unwrap_or(0)
}

/// The kind `kind` made with the constructor's arguments `args`; `None` for an unknown kind.
pub fn make_kind(kind: u32, args: &[i64]) -> Option<Box<dyn Kind>> {
    Some(match kind {
        EE_SCREENS_SK_COLOR => Box::new(Color { color: Cell::new(arg(args, 0) as i32) }),
        EE_SCREENS_SK_SET_PLAYERS => Box::new(SetPlayers { font: arg(args, 0) }),
        EE_SCREENS_SK_BRUSH => Box::new(Brush { group: arg(args, 0), size: arg(args, 1) as i32 }),
        EE_SCREENS_SK_CONDITION_COMBO => {
            Box::new(RefreshCombo { name: c"EESETSCComboBox", screen: SCREEN_CONDITION, refresh_screen: false })
        }
        EE_SCREENS_SK_EFFECT_COMBO => {
            Box::new(RefreshCombo { name: c"EESETSEComboBox", screen: SCREEN_EFFECT, refresh_screen: false })
        }
        EE_SCREENS_SK_SCENARIO_COMBO => {
            Box::new(ScenarioCombo { screen: arg(args, 0) as i32, mode: Cell::new(arg(args, 1) as u32) })
        }
        EE_SCREENS_SK_TRIGGER_COMBO => Box::new(TriggerCombo { kind: arg(args, 0) as u32 }),
        EE_SCREENS_SK_OBJECT_COMBO => Box::new(ObjectCombo),
        EE_SCREENS_SK_PLAYER_COMBO => Box::new(PlayerCombo { flags: Cell::new(0), count: Cell::new(0) }),
        EE_SCREENS_SK_ERROR_LOG => Box::new(ErrorLog),
        EE_SCREENS_SK_ELEVATION_SLIDER => Box::new(ElevationSlider),
        EE_SCREENS_SK_OBJECT_EDIT => Box::new(ObjectEdit),
        EE_SCREENS_SK_UNIT_LIST => Box::new(UnitList),
        _ => return crate::screens::shells_editor::make(kind, args),
    })
}

// ---- the kinds

/// `EEUIBColor`: the player colour button (a 25x25 square of the colour; left click the next of the 16, right
/// click the previous).
struct Color {
    /// `mColor` (a signed `long`).
    color: Cell<i32>,
}

impl Color {
    /// `SetColor`: wraps one below 0 (0xffffffff) to 15 and 16 or more to 0.
    fn set(&self, c: &Ctx, color: u32) {
        let mut m = color as i32;
        if m as u32 == u32::MAX {
            m = 15;
        } else if m >= 16 {
            m = 0;
        }
        self.color.set(m);
        c.call(EE_SCREENS_SH_BG_PLAYER_COLOR, i64::from(m), 0);
    }
}

impl Kind for Color {
    fn name(&self) -> &'static CStr {
        c"EEUIBColor"
    }
    fn overrides(&self) -> u64 {
        bit(EE_SCREENS_SV_MOUSE_L_UP) | bit(EE_SCREENS_SV_MOUSE_R_DOWN) | bit(EE_SCREENS_SV_MOUSE_R_UP)
    }
    fn init(&self, c: &Ctx) {
        c.call(EE_SCREENS_SH_BG_PLAYER_COLOR, i64::from(self.color.get()), 0);
        c.call_f(EE_SCREENS_SH_BG_OPACITY, 0, [1.0, 0.0, 0.0, 0.0]);
    }
    fn virt(&self, c: &Ctx, v: u32, a: i64, b: i64, f: &mut [f32; 4]) -> i64 {
        match v {
            EE_SCREENS_SV_MOUSE_L_UP => {
                self.set(c, (self.color.get() as u32).wrapping_add(1));
                c.base0(EE_SCREENS_SV_MOUSE_L_UP)
            }
            // a right press is a left one
            EE_SCREENS_SV_MOUSE_R_DOWN => c.base0(EE_SCREENS_SV_MOUSE_L_DOWN),
            EE_SCREENS_SV_MOUSE_R_UP => {
                self.set(c, (self.color.get() as u32).wrapping_sub(1));
                c.base0(EE_SCREENS_SV_MOUSE_L_UP);
                0
            }
            _ => c.base(v, a, b, f),
        }
    }
    fn op(&self, c: &Ctx, op: u32, a: i64, _b: i64, _f: &mut [f32; 4]) -> i64 {
        match op {
            EE_SCREENS_SKOP_COLOR => i64::from(self.color.get()),
            EE_SCREENS_SKOP_SET_COLOR => {
                self.set(c, a as u32);
                0
            }
            _ => 0,
        }
    }
}

/// `EESEPSSetPlayers`: the player tab's 'Set All Players' button.
struct SetPlayers {
    font: i64,
}

/// `IDS_SET_ALL_PLAYERS` (ee/EEStringIDs.h).
const IDS_SET_ALL_PLAYERS: i64 = 20_522;

impl Kind for SetPlayers {
    fn name(&self) -> &'static CStr {
        c"EESEPSSetPlayers"
    }
    fn overrides(&self) -> u64 {
        bit(EE_SCREENS_SV_BUTTON_UP)
    }
    fn init(&self, c: &Ctx) {
        c.call(EE_SCREENS_SH_BUTTON_TEXT, IDS_SET_ALL_PLAYERS, self.font);
        c.call_f(EE_SCREENS_SH_TEXT_FG, 0, [1.0, 1.0, 1.0, 0.0]);
    }
    fn virt(&self, c: &Ctx, v: u32, a: i64, b: i64, f: &mut [f32; 4]) -> i64 {
        if v == EE_SCREENS_SV_BUTTON_UP {
            c.call(EE_SCREENS_SH_COPY_FIRST_TO_ALL, 0, 0);
        }
        c.base(v, a, b, f)
    }
}

/// `EEUIFSERBBrush`: a brush size radio button of the paint tabs' brush group.
struct Brush {
    group: i64,
    /// `mBrushSize`.
    size: i32,
}

impl Kind for Brush {
    fn name(&self) -> &'static CStr {
        c"EEUIFSERBBrush"
    }
    fn overrides(&self) -> u64 {
        bit(EE_SCREENS_SV_CHECK)
    }
    fn init(&self, c: &Ctx) {
        c.call_f(EE_SCREENS_SH_BG_OPACITY, 0, [0.4, 0.0, 0.0, 0.0]);
    }
    fn virt(&self, c: &Ctx, v: u32, a: i64, b: i64, f: &mut [f32; 4]) -> i64 {
        if v == EE_SCREENS_SV_CHECK {
            c.call(EE_SCREENS_SH_BRUSH_SIZE, self.group, i64::from(self.size));
        }
        c.base(v, a, b, f)
    }
    fn op(&self, _c: &Ctx, op: u32, _a: i64, _b: i64, _f: &mut [f32; 4]) -> i64 {
        if op == EE_SCREENS_SKOP_BRUSH_SIZE {
            i64::from(self.size)
        } else {
            0
        }
    }
}

/// `EESETSCComboBox`, `EESETSEComboBox`: a trigger sub-screen's combo box; a changed selection refreshes the screen.
struct RefreshCombo {
    name: &'static CStr,
    screen: i64,
    refresh_screen: bool,
}

impl Kind for RefreshCombo {
    fn name(&self) -> &'static CStr {
        self.name
    }
    fn overrides(&self) -> u64 {
        bit(EE_SCREENS_SV_CHANGE_SELECTION)
    }
    fn virt(&self, c: &Ctx, v: u32, a: i64, b: i64, f: &mut [f32; 4]) -> i64 {
        let r = c.base(v, a, b, f);
        if v == EE_SCREENS_SV_CHANGE_SELECTION {
            if self.refresh_screen {
                c.call(EE_SCREENS_SH_TRIGGER_REFRESH_SCREEN, 0, 0);
            } else {
                c.call(EE_SCREENS_SH_TRIGGER_REFRESH, self.screen, 0);
            }
        }
        r
    }
}

/// `EESETSEScenarioComboBox`: a trigger screen's list of the scenario's entries (triggers, conditions, effects,
/// objects, areas); a changed selection refreshes its screen, a right click jumps to the entry.
struct ScenarioCombo {
    /// `mScreen` (a signed `long`).
    screen: i32,
    /// `mMode`.
    mode: Cell<u32>,
}

impl Kind for ScenarioCombo {
    fn name(&self) -> &'static CStr {
        c"EESETSEScenarioComboBox"
    }
    fn overrides(&self) -> u64 {
        bit(EE_SCREENS_SV_CHANGE_SELECTION) | bit(EE_SCREENS_SV_MOUSE_R_UP)
    }
    fn virt(&self, c: &Ctx, v: u32, a: i64, b: i64, f: &mut [f32; 4]) -> i64 {
        match v {
            EE_SCREENS_SV_CHANGE_SELECTION => {
                c.base(v, a, b, f);
                if (0..=SCREEN_AREA as i32).contains(&self.screen) {
                    c.call(EE_SCREENS_SH_TRIGGER_REFRESH, i64::from(self.screen), 0);
                }
                0
            }
            EE_SCREENS_SV_MOUSE_R_UP => {
                let selection = c.call(EE_SCREENS_SH_SELECTION_INDEX, 0, 0);
                c.call(EE_SCREENS_SH_TRIGGER_JUMP, i64::from(self.mode.get()), selection);
                0
            }
            _ => c.base(v, a, b, f),
        }
    }
    fn op(&self, _c: &Ctx, op: u32, a: i64, _b: i64, _f: &mut [f32; 4]) -> i64 {
        match op {
            EE_SCREENS_SKOP_SET_MODE => self.mode.set(a as u32),
            EE_SCREENS_SKOP_MODE => return i64::from(self.mode.get()),
            _ => {}
        }
        0
    }
}

/// `EESETSTriggerUIComboBox`: the trigger screen's condition and effect lists; a right click jumps to the entry.
struct TriggerCombo {
    /// `mKind`.
    kind: u32,
}

impl Kind for TriggerCombo {
    fn name(&self) -> &'static CStr {
        c"EESETSTriggerUIComboBox"
    }
    fn overrides(&self) -> u64 {
        bit(EE_SCREENS_SV_MOUSE_R_UP)
    }
    fn virt(&self, c: &Ctx, v: u32, a: i64, b: i64, f: &mut [f32; 4]) -> i64 {
        if v != EE_SCREENS_SV_MOUSE_R_UP {
            return c.base(v, a, b, f);
        }
        let selection = c.call(EE_SCREENS_SH_SELECTION_INDEX, 0, 0);
        c.call(EE_SCREENS_SH_TRIGGER_JUMP, i64::from(self.kind), selection);
        0
    }
}

/// `EESETSAComboBox`: an object page combo box; a changed selection updates the page's generated name.
struct ObjectCombo;

impl Kind for ObjectCombo {
    fn name(&self) -> &'static CStr {
        c"EESETSAComboBox"
    }
    fn overrides(&self) -> u64 {
        bit(EE_SCREENS_SV_CHANGE_SELECTION)
    }
    fn virt(&self, c: &Ctx, v: u32, a: i64, b: i64, f: &mut [f32; 4]) -> i64 {
        let r = c.base(v, a, b, f);
        if v == EE_SCREENS_SV_CHANGE_SELECTION {
            c.call(EE_SCREENS_SH_OBJECT_UPDATE_SELECTION, 0, 0);
        }
        r
    }
}

/// `EESETSUICBPlayer`: a trigger screen's player list (the world, the "any" and "all" entries its flags choose, then
/// the players).
struct PlayerCombo {
    /// `mFlags`.
    flags: Cell<u32>,
    /// `mPlayerCount`.
    count: Cell<u32>,
}

/// The player list's entries by flag (`kPlayerListWorld` .. `kPlayerListAnyExcludeWorld`, editor_ids.rs)
/// and their strings (`IDS_TRIGGER_PLAYER_WORLD`, `IDS_ANY_HUMAN`, `IDS_ANY_COMPUTER`, `IDS_ALL_HUMAN`,
/// `IDS_ALL_COMPUTER`, `IDS_ANY_PLAYER`, `IDS_ANY_EXCLUDE_WORLD`).
const PLAYER_LIST_ENTRIES: [(u32, i64); 7] =
    [(0x1, 29_050), (0x2, 29_051), (0x4, 29_052), (0x8, 29_053), (0x10, 29_054), (0x20, 29_055), (0x40, 29_056)];
/// `kPlayerListPlayers`, `IDS_TRIGGER_PLAYER` ("Player %d").
const PLAYER_LIST_PLAYERS: u32 = 0x80;
const IDS_TRIGGER_PLAYER: i64 = 29_057;

impl PlayerCombo {
    /// `GetSelectedPlayer`: the player of selection line `selection`.
    fn player_of(&self, selection: u32) -> u32 {
        let flags = self.flags.get();
        let mut index = 0u32;
        let mut player = 0u32;
        if selection != u32::MAX {
            let mut b = 1u32;
            while b <= 0x80 {
                if flags & b != 0 {
                    if b == 0x80 && selection < self.count.get().wrapping_add(index) {
                        player = player.wrapping_add(selection.wrapping_sub(index));
                        break;
                    }
                    if selection == index {
                        break;
                    }
                    index += 1;
                }
                player += 1;
                b <<= 1;
            }
        }
        player
    }
    /// `SetSelectedPlayer`: the line of player `player`.
    fn line_of(&self, player: u32) -> u32 {
        let flags = self.flags.get();
        let mut selection = 0u32;
        let mut p = 0u32;
        let mut b = 1u32;
        while b <= 0x80 {
            if p >= player {
                break;
            }
            if flags & b != 0 {
                if b == 0x80 && selection < self.count.get().wrapping_add(p) {
                    selection = selection.wrapping_sub(7).wrapping_add(player);
                    break;
                }
                if selection == player {
                    break;
                }
                selection += 1;
            }
            p += 1;
            b <<= 1;
        }
        selection
    }
    /// `FillList`.
    fn fill(&self, c: &Ctx) {
        let flags = self.flags.get();
        c.call(EE_SCREENS_SH_DELETE_ALL, 0, 0);
        for (flag, string) in PLAYER_LIST_ENTRIES {
            if flags & flag != 0 {
                c.call(EE_SCREENS_SH_APPEND_STRING, string, 0);
            }
        }
        if flags & PLAYER_LIST_PLAYERS != 0 {
            c.call(EE_SCREENS_SH_APPEND_NUMBERED, IDS_TRIGGER_PLAYER, i64::from(self.count.get()));
        }
    }
}

impl Kind for PlayerCombo {
    fn name(&self) -> &'static CStr {
        c"EESETSUICBPlayer"
    }
    fn overrides(&self) -> u64 {
        bit(EE_SCREENS_SV_CHANGE_SELECTION)
    }
    fn virt(&self, c: &Ctx, v: u32, a: i64, b: i64, f: &mut [f32; 4]) -> i64 {
        let r = c.base(v, a, b, f);
        if v == EE_SCREENS_SV_CHANGE_SELECTION {
            c.call(EE_SCREENS_SH_TRIGGER_REFRESH_SCREEN, 0, 0);
        }
        r
    }
    fn op(&self, c: &Ctx, op: u32, a: i64, _b: i64, _f: &mut [f32; 4]) -> i64 {
        match op {
            EE_SCREENS_SKOP_SELECTED_PLAYER => {
                i64::from(self.player_of(c.call(EE_SCREENS_SH_SELECTION_INDEX, 0, 0) as u32))
            }
            EE_SCREENS_SKOP_SET_SELECTED_PLAYER => {
                c.call(EE_SCREENS_SH_SET_SELECTION, i64::from(self.line_of(a as u32)), 0);
                0
            }
            EE_SCREENS_SKOP_SET_FLAGS => {
                self.flags.set(a as u32);
                self.fill(c);
                0
            }
            EE_SCREENS_SKOP_SET_PLAYER_COUNT => {
                self.count.set(a as u32);
                0
            }
            _ => 0,
        }
    }
}

/// `EEUIFMGErrorLog`: the in-game form's error log (no selection highlight; a click keeps no mouse capture unless the
/// scroll bar holds it).
struct ErrorLog;

impl Kind for ErrorLog {
    fn name(&self) -> &'static CStr {
        c"EEUIFMGErrorLog"
    }
    fn overrides(&self) -> u64 {
        bit(EE_SCREENS_SV_MOUSE_L_DOWN)
    }
    fn init(&self, c: &Ctx) {
        c.call_f(EE_SCREENS_SH_TEXT_FG, 0, [1.0, 1.0, 1.0, 0.0]);
        c.call_f(EE_SCREENS_SH_SELECTED_OPACITY, 0, [0.0; 4]);
    }
    fn virt(&self, c: &Ctx, v: u32, a: i64, b: i64, f: &mut [f32; 4]) -> i64 {
        let r = c.base(v, a, b, f);
        if v == EE_SCREENS_SV_MOUSE_L_DOWN && c.call(EE_SCREENS_SH_SCROLL_CAPTURED, 0, 0) == 0 {
            c.call(EE_SCREENS_SH_RELEASE_MOUSE, 0, 0);
        }
        r
    }
}

/// `EEUIFSEESElevation`: the elevation tab's slider; a move shows the level (max - value - 24, as a `char`) in the
/// strength label first.
struct ElevationSlider;

impl Kind for ElevationSlider {
    fn name(&self) -> &'static CStr {
        c"EEUIFSEESElevation"
    }
    fn overrides(&self) -> u64 {
        bit(EE_SCREENS_SV_CHANGE_RANGE)
    }
    fn virt(&self, c: &Ctx, v: u32, a: i64, b: i64, f: &mut [f32; 4]) -> i64 {
        if v == EE_SCREENS_SV_CHANGE_RANGE && c.call(EE_SCREENS_SH_SLIDER_VALUE, 0, 0) as u32 != a as u32 {
            let max = c.call(EE_SCREENS_SH_SLIDER_MAX, 0, 0) as i32;
            let level = max.wrapping_sub(a as i32).wrapping_sub(24) as i8;
            c.call(EE_SCREENS_SH_LABEL_NUMBER, ELEVATION_STRENGTH_LABEL, i64::from(level));
        }
        c.base(v, a, b, f)
    }
}

/// `EESETSAEditBox`: the object page's count box; Enter stores the page and updates its name.
struct ObjectEdit;

impl Kind for ObjectEdit {
    fn name(&self) -> &'static CStr {
        c"EESETSAEditBox"
    }
    fn overrides(&self) -> u64 {
        bit(EE_SCREENS_SV_PROCESS_ENTER)
    }
    fn virt(&self, c: &Ctx, v: u32, a: i64, b: i64, f: &mut [f32; 4]) -> i64 {
        let r = c.base(v, a, b, f);
        if v == EE_SCREENS_SV_PROCESS_ENTER {
            c.call(EE_SCREENS_SH_OBJECT_STORE, 0, 0);
            c.call(EE_SCREENS_SH_OBJECT_UPDATE_SELECTION, 0, 0);
        }
        r
    }
}

/// `EESETSOUnitList`: the object page's unit list; a selection always fires its event (the base's never does).
struct UnitList;

impl Kind for UnitList {
    fn name(&self) -> &'static CStr {
        c"EESETSOUnitList"
    }
    fn overrides(&self) -> u64 {
        bit(EE_SCREENS_SV_CHANGE_SELECTION_INDEX)
    }
    fn virt(&self, c: &Ctx, v: u32, a: i64, b: i64, f: &mut [f32; 4]) -> i64 {
        if v != EE_SCREENS_SV_CHANGE_SELECTION_INDEX {
            return c.base(v, a, b, f);
        }
        c.base(v, a, b & 2, f);
        c.call(EE_SCREENS_SH_FIRE, TRIGGER_SELECTION_CHANGED, 0);
        0
    }
}

#[cfg(test)]
#[path = "shells_tests.rs"]
mod tests;
