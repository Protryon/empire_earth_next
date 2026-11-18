//! `UIControlsTable` as a whole Rust object (R6 ui round 18): one control record of the UI database (its fields,
//! layout, texture rectangles, colours, file and sound names, form events and the font it holds while it makes a
//! control), the static array of every table (`sUIControlsArray`), and every method of `UIControlsTable.cpp` but
//! the three that only call GE objects (`CreateBitmap`, `SetupGEText`, `SetupStringArray`, kept in C++ and reached
//! through ops; the original's fourth, `CreateMovie`, left with the movies): `Create` (the database file), `AddControlLike`, `MoveControlsHorizontally`,
//! `DeleteAll`, `CreateControl` and its factories (`CreateBitmap`, `CreateButton`, ..., `CreateTextLabel`: which
//! control class a record makes, its constructor's arguments and the setters after it), `CreateControlEvent`,
//! `RelayoutControl` and `SetupFont`.
//!
//! The C++ `UIControlsTable` stays the type the game's user-defined controls read (`GetFont`, `GetFormIndex`,
//! `GetLayout`, ...): its getters read [`EeUiControlsTableData`] in place (the object is boxed and never moves), its
//! methods are forwarders, and every call out is an [`EeUiOp`] through its host (`UIControlsTable::HostOp`). The
//! controls' constructors are one op over a record ([`EeUiTableMake`]); their setters one op with a selector
//! ([`EeUiTableSet`]).
//!
//! The arithmetic is round 4's (`controls_table.rs`); this object is its host. Tables are made and read on the UI
//! thread only (the database's load, the forms' `Open` and relayout); the state is one `UnsafeCell` reached only
//! inside closures that make no call out (`formobj.rs`' rule), the array a `Mutex` never held across a call out.

use core::cell::UnsafeCell;
use core::ffi::{c_char, c_long, c_ulong, c_void};
use std::sync::Mutex;

use crate::button::UiColor;
use crate::controls_table::{self as ct, DbControl, TableFields, TableKey, TablesHost};
use crate::layout::Layout;
use crate::obj::{read_buf, rect_buf, EeUiHostFn, EeUiOp as Op, Host};
use crate::rect::UiRect;

/// The database's control types (`UIDbControlType`).
mod ty {
    use core::ffi::c_ulong;
    pub(crate) const BITMAP: c_ulong = 0;
    pub(crate) const BUTTON: c_ulong = 1;
    pub(crate) const CHECK_BOX: c_ulong = 2;
    pub(crate) const EDITABLE_COMBO_BOX: c_ulong = 3;
    pub(crate) const COMBO_BOX: c_ulong = 4;
    pub(crate) const NUMBER_MULTI_LINE_EDIT_BOX: c_ulong = 5;
    pub(crate) const NUMBER_EDIT_BOX: c_ulong = 6;
    pub(crate) const MULTI_LINE_EDIT_BOX: c_ulong = 7;
    pub(crate) const EDIT_BOX: c_ulong = 8;
    pub(crate) const LIST_BOX: c_ulong = 9;
    pub(crate) const AUTO_SELECT_LIST_BOX: c_ulong = 10;
    pub(crate) const RADIO_BUTTON: c_ulong = 11;
    pub(crate) const RADIO_TAB: c_ulong = 12;
    pub(crate) const STATUS_BAR: c_ulong = 13;
    pub(crate) const HORIZONTAL_SCROLL_BAR: c_ulong = 14;
    pub(crate) const TEXT_LABEL: c_ulong = 15;
    pub(crate) const MULTI_LINE_TEXT_LABEL: c_ulong = 16;
    pub(crate) const HORIZONTAL_SLIDER: c_ulong = 0x11;
    pub(crate) const VERTICAL_SLIDER: c_ulong = 0x12;
    pub(crate) const VERTICAL_SCROLL_BAR: c_ulong = 0x13;
    pub(crate) const NUMBER_TEXT_LABEL: c_ulong = 0x14;
    pub(crate) const NUMBER_MULTI_LINE_TEXT_LABEL: c_ulong = 0x15;
    pub(crate) const FILE_LIST_BOX: c_ulong = 0x16;
    pub(crate) const HELP_TEXT_LABEL: c_ulong = 0x18;
    pub(crate) const MULTI_LINE_HELP_TEXT_LABEL: c_ulong = 0x19;
    pub(crate) const SCROLLING_TEXT_LABEL: c_ulong = 0x1a;
    pub(crate) const BLINKING_BUTTON: c_ulong = 0x1b;
    pub(crate) const SCALABLE_BUTTON: c_ulong = 0x1c;
    pub(crate) const MOVIE: c_ulong = 0x1d;
    pub(crate) const LINE_GRAPH: c_ulong = 0x1e;
    pub(crate) const DUAL_BUTTON: c_ulong = 0x1f;
    pub(crate) const FILE_COMBO_BOX: c_ulong = 0x20;
    pub(crate) const STATUS_BUTTON: c_ulong = 0x3e8;
}

/// `kErrorOK`, `kErrorNotFound`.
const ERROR_OK: c_long = 0;
const ERROR_NOT_FOUND: c_long = 3;

/// The record's size on disk (`UIDbControl`).
const RECORD_SIZE: usize = core::mem::size_of::<DbControl>();

/// The table's data as the C++ getters read it.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct EeUiControlsTableData {
    pub fields: TableFields,
    pub layout: Layout,
    pub normal_rect: UiRect,
    pub pressed_rect: UiRect,
    pub focused_rect: UiRect,
    pub background: UiColor,
    pub text_foreground: UiColor,
    pub selected_list_box: UiColor,
    pub focused_line: UiColor,
    /// `mFont`: set by `SetupFont`, released after each `CreateControl` and `RelayoutControl`.
    pub font: *mut c_void,
    /// The normal, pressed and focused file names and the button down and up sounds, NUL-terminated (owned by the
    /// object).
    pub names: [*const c_char; 5],
}

/// Which name [`EeUiControlsTableData::names`] holds where.
const NORMAL: usize = 0;
const PRESSED: usize = 1;
const FOCUSED: usize = 2;
const DOWN_SOUND: usize = 3;
const UP_SOUND: usize = 4;

/// The control classes a table makes ([`EeUiTableMake::kind`]).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeUiTableKind {
    Bitmap = 0,
    Button = 1,
    BlinkingButton = 2,
    ScalableButton = 3,
    DualButton = 4,
    CheckBox = 5,
    ComboBox = 6,
    FileComboBox = 7,
    EditBox = 8,
    FileListBox = 9,
    LineGraph = 10,
    ListBox = 11,
    RadioButton = 12,
    ScrollBar = 13,
    ScrollingLabel = 14,
    Slider = 15,
    StatusBar = 16,
    TextLabel = 17,
}

/// A control's constructor's arguments ([`Op::TblMake`]); each kind reads the fields its constructor takes.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeUiTableMake {
    pub kind: EeUiTableKind,
    pub id: c_ulong,
    pub form: isize,
    pub index: c_ulong,
    pub border: c_ulong,
    pub font: isize,
    /// A `UWideString` ([`Op::TblStringNew`]).
    pub text: isize,
    /// A `std::vector<UWideString>` ([`Op::TblStringsNew`]).
    pub strings: isize,
    pub normal: isize,
    pub focused: isize,
    pub pressed: isize,
    /// The combo box's text box type, the scroll bar's and slider's style, the label's type.
    pub style: c_long,
    pub max_chars: c_long,
    pub blink_rate: c_long,
    /// The combo boxes' list height.
    pub list_height: c_long,
    /// The edit box's numbers only, the list box's auto-select, the radio button's push button (a tab), the check
    /// box's bitmap scale.
    pub flag_a: bool,
    /// The edit box's multi-line, the radio button's start check.
    pub flag_b: bool,
    /// The edit box's start check (`mStartChecked`), the radio button's bitmap scale.
    pub flag_c: bool,
    pub rect: UiRect,
    pub normal_rect: UiRect,
    pub focused_rect: UiRect,
    pub pressed_rect: UiRect,
    /// The scroll bar's length, the file combo box's width.
    pub length: f32,
}

/// The setters a table calls on a control it made ([`Op::TblSet`]'s `b`).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeUiTableSet {
    /// `UIButton::SetText(string c, font d)`.
    ButtonText = 0,
    ButtonFocusedColor = 1,
    /// `SetButtonDownSound` (the bytes at `c`).
    ButtonDownSound = 2,
    ButtonUpSound = 3,
    /// `SetupGEText` of the control's text: `c` 0 the button's, 1 the check box's label's, 2 the combo box's, 3 the
    /// edit box's, 4 the list box's, 5 the radio button's label's, 6 the text label's, 7 the status bar's.
    SetupText = 4,
    CheckSetChecked = 5,
    ComboFocusedRect = 6,
    ComboSelectedBg = 7,
    ComboSelection = 8,
    ComboTextBoxOpacity = 9,
    ComboTextBoxColor = 10,
    ComboListAlwaysDirty = 11,
    EditNumber = 12,
    EditText = 13,
    EditSelectedBg = 14,
    ListFocusedRect = 15,
    ListSelectedBg = 16,
    ListScrollOpacity = 17,
    ListSelection = 18,
    /// `AddValidExtension` of each of the strings `c`.
    FileListExtensions = 19,
    GraphFont = 20,
    GraphFontColor = 21,
    RadioUpUnchecked = 22,
    /// `SetDownUncheckedBitmap`, `SetDownCheckedBitmap`, `SetUpCheckedBitmap` (bitmap `c`).
    RadioPressed = 23,
    ScrollRange = 24,
    ScrollValue = 25,
    SliderRange = 26,
    SliderValue = 27,
    SliderPage = 28,
    StatusRange = 29,
    StatusValue = 30,
    StatusColor = 31,
    StatusOpacity = 32,
    StatusText = 33,
    StatusFocusedColor = 34,
    LabelNumber = 35,
    LabelAlign = 36,
    LabelScrollOpacity = 37,
    LabelFit = 38,
    BgColor = 39,
    BgOpacity = 40,
    AlwaysDirty = 41,
    SetRectangle = 42,
    ChangeFont = 43,
    /// `SetTriggerEvent(c, event d)`.
    SetTrigger = 44,
}

/// [`EeUiTableSet::SetupText`]'s classes.
const TEXT_BUTTON: isize = 0;
const TEXT_CHECK: isize = 1;
const TEXT_COMBO: isize = 2;
const TEXT_EDIT: isize = 3;
const TEXT_LIST: isize = 4;
const TEXT_RADIO: isize = 5;
const TEXT_LABEL: isize = 6;
const TEXT_STATUS: isize = 7;

/// The inward calls (the C++ shim's methods).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeUiTableCall {
    /// `CreateControl(manager a)` -> the control.
    CreateControl = 1,
    /// `CreateControlEvent(form a)`.
    CreateControlEvent = 2,
    /// `RelayoutControl(control a, manager b)`.
    RelayoutControl = 3,
    /// `SetupFont(manager a, f, b != 0)`.
    SetupFont = 4,
    /// `AddEvent({a, b})`.
    AddEvent = 5,
}

/// `AddControlLike`'s arguments.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeUiTableLike {
    pub template_id: c_long,
    pub id: c_long,
    pub form_index: c_long,
    pub group: c_ulong,
    pub left: f32,
    pub right: f32,
    pub top: f32,
    pub bottom: f32,
    pub string_table_begin: c_long,
    pub string_table_end: c_long,
    pub help_string_id: c_ulong,
}

struct State {
    host: Host,
    data: EeUiControlsTableData,
    names: [Vec<u8>; 5],
    /// `mEventData`: (event id, event type).
    events: Vec<(c_ulong, c_ulong)>,
}

/// The Rust UIControlsTable.
pub struct EeUiControlsTable {
    s: UnsafeCell<State>,
}

/// The static array (`sUIControlsArray`): the Rust tables by address, 0 for a record that is no control.
static TABLES: Mutex<Vec<usize>> = Mutex::new(Vec::new());

fn tables() -> std::sync::MutexGuard<'static, Vec<usize>> {
    TABLES.lock().unwrap_or_else(|e| e.into_inner())
}

/// A name's bytes up to its NUL (the record's 100-byte field, `strlen`), NUL-terminated.
fn name(field: &[u8]) -> Vec<u8> {
    let n = field.iter().position(|&b| b == 0).unwrap_or(field.len());
    let mut v = field[..n].to_vec();
    v.push(0);
    v
}

fn color_buf(c: UiColor) -> [f32; 4] {
    [c.red, c.green, c.blue, 0.0]
}

impl EeUiControlsTable {
    /// A table from its parts (the names NUL-terminated).
    fn make(host: Host, t: ct::Table, names: [Vec<u8>; 5], events: Vec<(c_ulong, c_ulong)>) -> Box<Self> {
        let data = EeUiControlsTableData {
            fields: t.fields,
            layout: t.layout,
            normal_rect: t.normal_rect,
            pressed_rect: t.pressed_rect,
            focused_rect: t.focused_rect,
            background: t.background,
            text_foreground: t.text_foreground,
            selected_list_box: t.selected_list_box,
            focused_line: t.focused_line,
            font: core::ptr::null_mut(),
            names: [core::ptr::null(); 5],
        };
        let b = Box::new(EeUiControlsTable { s: UnsafeCell::new(State { host, data, names, events }) });
        b.with(|s| {
            for i in 0..5 {
                s.data.names[i] = s.names[i].as_ptr() as *const c_char;
            }
        });
        b
    }

    /// The default constructor: an empty bitmap description (no font, files or layout).
    pub(crate) fn blank(host: Host) -> Box<Self> {
        let zero = UiRect::new(0.0, 0.0, 0.0, 0.0);
        let black = UiColor { red: 0.0, green: 0.0, blue: 0.0 };
        let t = ct::Table {
            fields: ct::init(),
            layout: Layout::default(),
            background: black,
            text_foreground: black,
            selected_list_box: black,
            focused_line: black,
            normal_rect: zero,
            pressed_rect: zero,
            focused_rect: zero,
        };
        Self::make(host, t, core::array::from_fn(|_| vec![0]), Vec::new())
    }

    /// The record constructor.
    fn from_record(host: Host, r: &DbControl) -> Box<Self> {
        let names = [
            name(&r.normal_file_name),
            name(&r.pressed_file_name),
            name(&r.focused_file_name),
            name(&r.button_down_sound),
            name(&r.button_up_sound),
        ];
        Self::make(host, ct::from_record(r), names, Vec::new())
    }

    /// The state, for a closure that makes no call out.
    fn with<R>(&self, f: impl FnOnce(&mut State) -> R) -> R {
        // SAFETY: every borrow ends inside the closure, which never calls out; tables are reached from the UI thread
        // only (module notes)
        f(unsafe { &mut *self.s.get() })
    }

    fn data(&self) -> EeUiControlsTableData {
        self.with(|s| s.data)
    }

    fn fields(&self) -> TableFields {
        self.with(|s| s.data.fields)
    }

    fn has_name(&self, i: usize) -> bool {
        self.with(|s| s.names[i].len() > 1)
    }

    fn name_ptr(&self, i: usize) -> isize {
        self.with(|s| s.names[i].as_ptr() as isize)
    }

    fn font(&self) -> isize {
        self.with(|s| s.data.font as isize)
    }

    fn set_font(&self, font: isize) {
        self.with(|s| s.data.font = font as *mut c_void);
    }

    fn op(&self, op: Op, a: isize, b: isize, c: isize, d: isize) -> isize {
        let h = self.with(|s| s.host);
        h.op(op, a, b, c, d)
    }

    fn op_f(&self, op: Op, a: isize, b: isize, c: isize, d: isize, f: [f32; 4]) -> isize {
        let h = self.with(|s| s.host);
        let mut f = f;
        h.op_f(op, a, b, c, d, &mut f)
    }

    fn set(&self, ctl: isize, which: EeUiTableSet, c: isize, d: isize) {
        self.op(Op::TblSet, ctl, which as isize, c, d);
    }

    fn set_f(&self, ctl: isize, which: EeUiTableSet, f: [f32; 4]) {
        self.op_f(Op::TblSet, ctl, which as isize, 0, 0, f);
    }

    fn set_color(&self, ctl: isize, which: EeUiTableSet, c: UiColor) {
        self.set_f(ctl, which, color_buf(c));
    }

    fn set_float(&self, ctl: isize, which: EeUiTableSet, v: f32) {
        self.set_f(ctl, which, [v, 0.0, 0.0, 0.0]);
    }

    /// The manager's `GetFormByID(mFormID)`.
    fn form(&self, m: isize) -> isize {
        self.op(Op::TblFormById, self.fields().form_id as isize, 0, 0, m)
    }

    /// The manager's window's size.
    fn window(&self, m: isize) -> (c_ulong, c_ulong) {
        (self.op(Op::TblWindowWidth, 0, 0, 0, m) as c_ulong, self.op(Op::TblWindowHeight, 0, 0, 0, m) as c_ulong)
    }

    fn string(&self, id: c_long, m: isize) -> isize {
        self.op(Op::TblStringNew, id as isize, 0, 0, m)
    }

    fn bitmap(&self, which: isize, r: UiRect, m: isize) -> isize {
        self.op_f(Op::TblBitmapNew, which, 0, 0, m, rect_buf(r))
    }

    fn release_bitmap(&self, b: isize) {
        self.op(Op::TblBitmapRelease, b, 0, 0, 0);
    }

    /// A constructor's record with the arguments every control takes.
    fn make_args(&self, kind: EeUiTableKind, form: isize, r: UiRect) -> EeUiTableMake {
        let f = self.fields();
        let zero = UiRect::new(0.0, 0.0, 0.0, 0.0);
        EeUiTableMake {
            kind,
            id: f.id as c_ulong,
            form,
            index: f.form_index as c_ulong,
            border: f.border_thickness as c_ulong,
            font: self.font(),
            text: 0,
            strings: 0,
            normal: 0,
            focused: 0,
            pressed: 0,
            style: 0,
            max_chars: f.max_chars,
            blink_rate: f.blink_rate,
            list_height: 0,
            flag_a: false,
            flag_b: false,
            flag_c: false,
            rect: r,
            normal_rect: zero,
            focused_rect: zero,
            pressed_rect: zero,
            length: 0.0,
        }
    }

    fn construct(&self, args: &EeUiTableMake) -> isize {
        self.op(Op::TblMake, args as *const EeUiTableMake as isize, 0, 0, 0)
    }

    fn sounds(&self, ctl: isize) {
        if self.has_name(DOWN_SOUND) {
            self.set(ctl, EeUiTableSet::ButtonDownSound, self.name_ptr(DOWN_SOUND), 0);
        }
        // Fixed (kept): the up sound used to be the down sound
        if self.has_name(UP_SOUND) {
            self.set(ctl, EeUiTableSet::ButtonUpSound, self.name_ptr(UP_SOUND), 0);
        }
    }

    // ---- the factories (CreateControl's switch)

    fn create_bitmap(&self, m: isize, r: UiRect) -> isize {
        let form = self.form(m);
        let normal = if self.has_name(NORMAL) { self.bitmap(0, r, m) } else { 0 };
        let mut a = self.make_args(EeUiTableKind::Bitmap, form, r);
        a.normal = normal;
        let ctl = self.construct(&a);
        if normal != 0 {
            self.release_bitmap(normal);
        }
        ctl
    }

    fn create_button(&self, m: isize, r: UiRect) -> isize {
        let form = self.form(m);
        let normal = if self.has_name(NORMAL) { self.bitmap(0, r, m) } else { 0 };
        let pressed = if self.has_name(PRESSED) { self.bitmap(1, r, m) } else { 0 };
        let focused = if self.has_name(FOCUSED) { self.bitmap(2, r, m) } else { 0 };
        let d = self.data();
        let kind = match d.fields.control_type {
            ty::BUTTON => EeUiTableKind::Button,
            ty::BLINKING_BUTTON => EeUiTableKind::BlinkingButton,
            ty::SCALABLE_BUTTON => EeUiTableKind::ScalableButton,
            _ => EeUiTableKind::DualButton,
        };
        let mut a = self.make_args(kind, form, r);
        a.normal = normal;
        a.focused = focused;
        a.pressed = pressed;
        a.normal_rect = d.normal_rect;
        a.focused_rect = d.focused_rect;
        a.pressed_rect = d.pressed_rect;
        let ctl = self.construct(&a);
        for b in [normal, pressed, focused] {
            if b != 0 {
                self.release_bitmap(b);
            }
        }
        let font = self.font();
        if d.fields.string_table_begin > 0 || font != 0 {
            let id = if d.fields.string_table_begin > 0 { d.fields.string_table_begin } else { 0 };
            let text = self.string(id, m);
            self.set(ctl, EeUiTableSet::ButtonText, text, font);
            self.set_color(ctl, EeUiTableSet::ButtonFocusedColor, d.focused_line);
            self.set(ctl, EeUiTableSet::SetupText, TEXT_BUTTON, 0);
            self.sounds(ctl);
            self.op(Op::TblStringDelete, text, 0, 0, 0);
        } else {
            self.sounds(ctl);
        }
        ctl
    }

    fn create_check_box(&self, m: isize, r: UiRect) -> isize {
        let form = self.form(m);
        let f = self.fields();
        let text = self.string(if f.string_table_begin > 0 { f.string_table_begin } else { 0 }, m);
        let mut a = self.make_args(EeUiTableKind::CheckBox, form, r);
        a.text = text;
        a.flag_a = f.scale_to_control;
        let ctl = self.construct(&a);
        self.set(ctl, EeUiTableSet::SetupText, TEXT_CHECK, 0);
        if f.start_checked {
            self.set(ctl, EeUiTableSet::CheckSetChecked, 1, 0);
        }
        self.sounds(ctl);
        self.op(Op::TblStringDelete, text, 0, 0, 0);
        ctl
    }

    /// The combo boxes' setters after the constructor.
    fn combo_setup(&self, ctl: isize) {
        let d = self.data();
        self.set(ctl, EeUiTableSet::SetupText, TEXT_COMBO, 0);
        self.set_color(ctl, EeUiTableSet::ComboFocusedRect, d.focused_line);
        self.set_color(ctl, EeUiTableSet::ComboSelectedBg, d.selected_list_box);
        if d.fields.start_current_selection > 0 {
            self.set(ctl, EeUiTableSet::ComboSelection, d.fields.start_current_selection as isize, 0);
        }
        self.set_float(ctl, EeUiTableSet::ComboTextBoxOpacity, d.fields.text_box_opacity);
        self.set_color(ctl, EeUiTableSet::ComboTextBoxColor, d.background);
        self.set(ctl, EeUiTableSet::ComboListAlwaysDirty, d.fields.list_box_always_dirty as isize, 0);
    }

    fn create_combo_box(&self, m: isize, r: UiRect) -> isize {
        let form = self.form(m);
        let strings = self.op(Op::TblStringsNew, 0, 0, 0, m);
        let f = self.fields();
        let font_height =
            if f.scale_to_control { self.op(Op::TblFontHeight, self.font(), 0, 0, 0) as c_long } else { 0 };
        let (list, height) = ct::combo_list(&r, f.scale_to_control, font_height, f.border_thickness);
        let mut a = self.make_args(EeUiTableKind::ComboBox, form, list);
        a.style = if f.control_type == ty::EDITABLE_COMBO_BOX { 0 } else { 1 };
        a.strings = strings;
        a.list_height = height;
        let ctl = self.construct(&a);
        self.combo_setup(ctl);
        self.op(Op::TblStringsDelete, strings, 0, 0, 0);
        ctl
    }

    fn create_file_combo_box(&self, m: isize, r: UiRect) -> isize {
        let form = self.form(m);
        let strings = self.op(Op::TblStringsNew, 0, 0, 0, m);
        let (width, height) = ct::file_combo_size(&r);
        let mut a = self.make_args(EeUiTableKind::FileComboBox, form, r);
        a.style = 1;
        a.strings = strings;
        a.length = width;
        a.list_height = height;
        let ctl = self.construct(&a);
        self.combo_setup(ctl);
        self.op(Op::TblStringsDelete, strings, 0, 0, 0);
        ctl
    }

    fn create_edit_box(&self, m: isize, r: UiRect) -> isize {
        let form = self.form(m);
        let d = self.data();
        let t = d.fields.control_type;
        let numbers = t == ty::NUMBER_MULTI_LINE_EDIT_BOX || t == ty::NUMBER_EDIT_BOX;
        let mut a = self.make_args(EeUiTableKind::EditBox, form, r);
        a.flag_a = numbers;
        a.flag_b = t == ty::NUMBER_MULTI_LINE_EDIT_BOX || t == ty::MULTI_LINE_EDIT_BOX;
        a.flag_c = d.fields.start_checked;
        let ctl = self.construct(&a);
        self.set(ctl, EeUiTableSet::SetupText, TEXT_EDIT, 0);
        let mut text = 0;
        if numbers {
            self.set(ctl, EeUiTableSet::EditNumber, d.fields.string_table_begin as isize, 0);
        } else if d.fields.string_table_begin > 0 {
            text = self.string(d.fields.string_table_begin, m);
            self.set(ctl, EeUiTableSet::EditText, text, 0);
        }
        self.set_color(ctl, EeUiTableSet::EditSelectedBg, d.selected_list_box);
        if text != 0 {
            self.op(Op::TblStringDelete, text, 0, 0, 0);
        }
        ctl
    }

    /// The list boxes' colours and scroll bar after the constructor.
    fn list_setup(&self, ctl: isize) {
        let d = self.data();
        self.set(ctl, EeUiTableSet::SetupText, TEXT_LIST, 0);
        self.set_color(ctl, EeUiTableSet::ListFocusedRect, d.focused_line);
        self.set_color(ctl, EeUiTableSet::ListSelectedBg, d.selected_list_box);
        self.set_float(ctl, EeUiTableSet::ListScrollOpacity, d.fields.scroll_bar_bg_opacity);
    }

    fn list_selection(&self, ctl: isize) {
        let sel = self.fields().start_current_selection;
        if sel > 0 {
            self.set(ctl, EeUiTableSet::ListSelection, sel as isize, 0);
        }
    }

    fn create_file_list_box(&self, m: isize, r: UiRect) -> isize {
        let form = self.form(m);
        let ctl = self.construct(&self.make_args(EeUiTableKind::FileListBox, form, r));
        self.list_setup(ctl);
        let strings = self.op(Op::TblStringsNew, 0, 0, 0, m);
        self.set(ctl, EeUiTableSet::FileListExtensions, strings, 0);
        self.list_selection(ctl);
        self.op(Op::TblStringsDelete, strings, 0, 0, 0);
        ctl
    }

    fn create_graph(&self, m: isize, r: UiRect) -> isize {
        let form = self.form(m);
        let ctl = self.construct(&self.make_args(EeUiTableKind::LineGraph, form, r));
        self.set(ctl, EeUiTableSet::GraphFont, self.font(), 0);
        self.set_color(ctl, EeUiTableSet::GraphFontColor, self.data().text_foreground);
        ctl
    }

    fn create_list_box(&self, m: isize, r: UiRect) -> isize {
        let form = self.form(m);
        let strings = self.op(Op::TblStringsNew, 0, 0, 0, m);
        let mut a = self.make_args(EeUiTableKind::ListBox, form, r);
        a.strings = strings;
        a.flag_a = self.fields().control_type == ty::AUTO_SELECT_LIST_BOX;
        let ctl = self.construct(&a);
        self.list_setup(ctl);
        self.list_selection(ctl);
        self.op(Op::TblStringsDelete, strings, 0, 0, 0);
        ctl
    }

    fn create_radio_button(&self, m: isize, r: UiRect) -> isize {
        let form = self.form(m);
        let f = self.fields();
        let text = self.string(if f.string_table_begin > 0 { f.string_table_begin } else { 0 }, m);
        let mut a = self.make_args(EeUiTableKind::RadioButton, form, r);
        a.text = text;
        a.flag_a = f.control_type == ty::RADIO_TAB;
        a.flag_b = f.start_checked;
        a.flag_c = f.scale_to_control;
        let ctl = self.construct(&a);
        if self.op(Op::TblStringLength, text, 0, 0, 0) > 0 {
            self.set(ctl, EeUiTableSet::SetupText, TEXT_RADIO, 0);
        }
        self.sounds(ctl);
        if self.has_name(NORMAL) {
            let b = self.bitmap(0, r, m);
            self.set(ctl, EeUiTableSet::RadioUpUnchecked, b, 0);
            self.release_bitmap(b);
        }
        if self.has_name(PRESSED) {
            let b = self.bitmap(1, r, m);
            self.set(ctl, EeUiTableSet::RadioPressed, b, 0);
            self.release_bitmap(b);
        }
        self.op(Op::TblStringDelete, text, 0, 0, 0);
        ctl
    }

    fn create_scroll_bar(&self, m: isize, r: UiRect) -> isize {
        let form = self.form(m);
        let f = self.fields();
        let vertical = f.control_type == ty::VERTICAL_SCROLL_BAR;
        let mut a = self.make_args(EeUiTableKind::ScrollBar, form, r);
        a.length = ct::scroll_bar_length(&r, vertical);
        a.style = vertical as c_long;
        let ctl = self.construct(&a);
        self.set(ctl, EeUiTableSet::ScrollRange, f.start_range as isize, f.end_range as isize);
        self.set(ctl, EeUiTableSet::ScrollValue, f.current_range as isize, 0);
        ctl
    }

    /// The string a label starts with: the string table's, but for the number types (they show the number).
    fn label_text(&self, m: isize) -> isize {
        let f = self.fields();
        let t = f.control_type;
        let id = if t != ty::NUMBER_TEXT_LABEL && t != ty::NUMBER_MULTI_LINE_TEXT_LABEL && f.string_table_begin > 0 {
            f.string_table_begin
        } else {
            0
        };
        self.string(id, m)
    }

    fn create_scrolling_label(&self, m: isize, r: UiRect) -> isize {
        let form = self.form(m);
        let text = self.label_text(m);
        let mut a = self.make_args(EeUiTableKind::ScrollingLabel, form, r);
        a.text = text;
        let ctl = self.construct(&a);
        self.set(ctl, EeUiTableSet::SetupText, TEXT_LABEL, 0);
        self.op(Op::TblStringDelete, text, 0, 0, 0);
        ctl
    }

    fn create_slider(&self, m: isize, r: UiRect) -> isize {
        let form = self.form(m);
        let f = self.fields();
        let mut a = self.make_args(EeUiTableKind::Slider, form, r);
        a.style = (f.control_type == ty::VERTICAL_SLIDER) as c_long;
        let ctl = self.construct(&a);
        self.set(ctl, EeUiTableSet::SliderRange, f.start_range as isize, f.end_range as isize);
        self.set(ctl, EeUiTableSet::SliderValue, f.current_range as isize, 0);
        let page = ct::slider_page_size(f.start_range, f.end_range, f.page_size);
        self.set(ctl, EeUiTableSet::SliderPage, page as isize, 0);
        ctl
    }

    fn create_status_bar(&self, m: isize, r: UiRect) -> isize {
        let form = self.form(m);
        let d = self.data();
        let f = d.fields;
        let ctl = self.construct(&self.make_args(EeUiTableKind::StatusBar, form, r));
        self.set(ctl, EeUiTableSet::StatusRange, f.start_range as isize, f.end_range as isize);
        self.set(ctl, EeUiTableSet::StatusValue, f.current_range as isize, 0);
        self.set_color(ctl, EeUiTableSet::StatusColor, d.text_foreground);
        self.set_float(ctl, EeUiTableSet::StatusOpacity, f.text_box_opacity);
        if f.string_table_begin > 0 {
            let text = self.string(f.string_table_begin, m);
            self.set(ctl, EeUiTableSet::StatusText, text, self.font());
            self.set_color(ctl, EeUiTableSet::StatusFocusedColor, d.focused_line);
            self.set(ctl, EeUiTableSet::SetupText, TEXT_STATUS, 0);
            self.op(Op::TblStringDelete, text, 0, 0, 0);
        }
        ctl
    }

    fn create_text_label(&self, m: isize, r: UiRect) -> isize {
        let form = self.form(m);
        let text = self.label_text(m);
        let f = self.fields();
        let t = f.control_type;
        let mut a = self.make_args(EeUiTableKind::TextLabel, form, r);
        a.text = text;
        a.style =
            matches!(t, ty::MULTI_LINE_TEXT_LABEL | ty::NUMBER_MULTI_LINE_TEXT_LABEL | ty::MULTI_LINE_HELP_TEXT_LABEL)
                as c_long;
        let ctl = self.construct(&a);
        self.set(ctl, EeUiTableSet::SetupText, TEXT_LABEL, 0);
        if t != ty::TEXT_LABEL && t != ty::MULTI_LINE_TEXT_LABEL {
            self.set(ctl, EeUiTableSet::LabelNumber, f.string_table_begin as isize, 0);
        }
        self.set(ctl, EeUiTableSet::LabelAlign, f.text_alignment as isize, 0);
        self.set_float(ctl, EeUiTableSet::LabelScrollOpacity, f.scroll_bar_bg_opacity);
        self.set(ctl, EeUiTableSet::LabelFit, f.fit_rect_to_text as isize, 0);
        self.op(Op::TblStringDelete, text, 0, 0, 0);
        ctl
    }

    /// The control's rectangle on the window (`CalculateRectangle` against the window less a pixel, unless the form
    /// uses exclusive coordinates) and whether the form does.
    fn layout_rect(&self, m: isize) -> (UiRect, bool) {
        let (w, h) = self.window(m);
        let form = self.form(m);
        let exclusive = self.op(Op::TblFormExclusive, form, 0, 0, 0) != 0;
        let parent = ct::parent_rect(w, h, exclusive);
        (self.with(|s| s.data.layout).calculate(&parent, exclusive), exclusive)
    }

    fn release_font(&self) {
        let font = self.font();
        if font != 0 {
            self.op(Op::TblFontRelease, font, 0, 0, 0);
            self.set_font(0);
        }
    }

    /// `CreateControl`: lays the control out on the window, picks its font, makes it by type (the user types through
    /// the manager) and sets its background.
    fn create_control(&self, m: isize) -> isize {
        let (r, exclusive) = self.layout_rect(m);
        self.setup_font(m, ct::font_rect(&r, exclusive), false);
        let ctl = match self.fields().control_type {
            ty::BITMAP => self.create_bitmap(m, r),
            ty::BUTTON | ty::BLINKING_BUTTON | ty::SCALABLE_BUTTON | ty::DUAL_BUTTON => self.create_button(m, r),
            ty::CHECK_BOX => self.create_check_box(m, r),
            ty::EDITABLE_COMBO_BOX | ty::COMBO_BOX => self.create_combo_box(m, r),
            ty::LINE_GRAPH => self.create_graph(m, r),
            ty::NUMBER_MULTI_LINE_EDIT_BOX | ty::NUMBER_EDIT_BOX | ty::MULTI_LINE_EDIT_BOX | ty::EDIT_BOX => {
                self.create_edit_box(m, r)
            }
            ty::LIST_BOX | ty::AUTO_SELECT_LIST_BOX => self.create_list_box(m, r),
            ty::RADIO_BUTTON | ty::RADIO_TAB => self.create_radio_button(m, r),
            ty::STATUS_BAR => self.create_status_bar(m, r),
            ty::HORIZONTAL_SCROLL_BAR | ty::VERTICAL_SCROLL_BAR => self.create_scroll_bar(m, r),
            ty::TEXT_LABEL
            | ty::MULTI_LINE_TEXT_LABEL
            | ty::NUMBER_TEXT_LABEL
            | ty::NUMBER_MULTI_LINE_TEXT_LABEL
            | ty::HELP_TEXT_LABEL
            | ty::MULTI_LINE_HELP_TEXT_LABEL => self.create_text_label(m, r),
            ty::HORIZONTAL_SLIDER | ty::VERTICAL_SLIDER => self.create_slider(m, r),
            ty::FILE_LIST_BOX => self.create_file_list_box(m, r),
            ty::FILE_COMBO_BOX => self.create_file_combo_box(m, r),
            ty::SCROLLING_TEXT_LABEL => self.create_scrolling_label(m, r),
            // no control: the original made a UIMovie playing the record's file; movies left the game (2026-11-17)
            ty::MOVIE => 0,
            t if t >= ty::STATUS_BUTTON => self.op(Op::TblUserDefined, 0, 0, 0, m),
            _ => 0,
        };
        if ctl != 0 {
            let d = self.data();
            self.set_color(ctl, EeUiTableSet::BgColor, d.background);
            self.set_float(ctl, EeUiTableSet::BgOpacity, d.fields.background_opacity);
            self.set(ctl, EeUiTableSet::AlwaysDirty, d.fields.always_dirty as isize, 0);
        }
        self.release_font();
        ctl
    }

    /// `CreateControlEvent`: the record's events on the form's control, each made by the form's manager.
    fn create_control_event(&self, form: isize) {
        let ctl = self.op(Op::TblFormControl, form, self.fields().form_index as isize, 0, 0);
        let mut i = 0;
        while let Some((id, kind)) = self.with(|s| s.events.get(i).copied()) {
            let event = self.op(Op::TblEventNew, form, id as isize, 0, 0);
            if event != 0 {
                self.set(ctl, EeUiTableSet::SetTrigger, kind as isize, event);
                self.op(Op::TblEventRelease, event, 0, 0, 0);
            }
            i += 1;
        }
    }

    /// `RelayoutControl`: the control's rectangle and font for the current window.
    fn relayout_control(&self, ctl: isize, m: isize) {
        if ctl == 0 {
            return;
        }
        let (r, _) = self.layout_rect(m);
        self.op_f(Op::TblSet, ctl, EeUiTableSet::SetRectangle as isize, 0, 0, rect_buf(r));
        self.setup_font(m, r, false);
        self.set(ctl, EeUiTableSet::ChangeFont, self.font(), 0);
        self.release_font();
    }

    /// `SetupFont`: the font table's font, or its closest size to the control's (or the window's) height when the
    /// record scales it; a font bitmap made for it unless the form draws text with GDI.
    fn setup_font(&self, m: isize, r: UiRect, scale_to_window: bool) {
        let form = self.form(m);
        let f = self.fields();
        let old = self.font();
        if old != 0 {
            self.op(Op::TblFontRelease, old, 0, 0, 0);
        }
        let index = f.font_table_index as isize;
        let font = if f.font_table_index > 0 {
            if f.scale_to_control {
                let size = if !ct::font_scales_to_window(f.control_type, scale_to_window) {
                    ct::control_font_size(r.height(), f.border_thickness)
                } else {
                    let (_, window_height) = self.window(m);
                    let table_font = self.op(Op::TblFontGet, index, 0, 0, 0);
                    let height = self.op(Op::TblFontHeight, table_font, 0, 0, 0) as c_long;
                    let designed = self.op(Op::TblFormDesignedHeight, form, 0, 0, 0) as c_ulong;
                    let size = ct::window_font_size(height, window_height, designed);
                    self.op(Op::TblFontRelease, table_font, 0, 0, 0);
                    size
                };
                self.op(Op::TblFontClosest, index, size as isize, 0, 0)
            } else {
                self.op(Op::TblFontGet, index, 0, 0, 0)
            }
        } else {
            0
        };
        self.set_font(font);
        if self.op(Op::TblFormGdi, form, 0, 0, 0) == 0 && font != 0 && self.op(Op::TblFontHasBitmap, font, 0, 0, 0) == 0
        {
            self.op(Op::TblFontBitmapNew, font, 0, 0, m);
        }
    }

    /// One of the methods ([`EeUiTableCall`]).
    ///
    /// # Safety
    /// `f` is NULL or four live floats.
    pub unsafe fn call(&self, call: EeUiTableCall, a: isize, b: isize, f: *mut f32) -> isize {
        match call {
            EeUiTableCall::CreateControl => self.create_control(a),
            EeUiTableCall::CreateControlEvent => {
                self.create_control_event(a);
                0
            }
            EeUiTableCall::RelayoutControl => {
                self.relayout_control(a, b);
                0
            }
            EeUiTableCall::SetupFont => {
                // SAFETY: the contract
                let r = crate::obj::buf_rect(&unsafe { read_buf(f) });
                self.setup_font(a, r, b != 0);
                0
            }
            EeUiTableCall::AddEvent => {
                self.with(|s| s.events.push((a as c_ulong, b as c_ulong)));
                0
            }
        }
    }

    /// The destructor's work: the font it still holds released.
    fn release(&self) {
        let font = self.font();
        if font != 0 {
            self.op(Op::TblFontRelease, font, 0, 0, 0);
        }
    }

    /// Makes the C++ table of a Rust table made here and gives it to the array's caller.
    fn wrap(t: Box<Self>) -> usize {
        let p = Box::into_raw(t);
        // SAFETY: just made; the C++ table owns it from here (its destructor frees it)
        let t = unsafe { &*p };
        let cpp = t.op(Op::TblWrap, p as isize, 0, 0, 0);
        t.with(|s| s.host.obj = cpp as *mut c_void);
        p as usize
    }

    fn cpp(&self) -> isize {
        self.with(|s| s.host.obj as isize)
    }
}

/// The static array's view for round 4's searches.
struct Array<'a>(&'a [usize]);

fn at(p: usize) -> &'static EeUiControlsTable {
    // SAFETY: the array holds live tables only (DeleteAll empties it before freeing them)
    unsafe { &*(p as *const EeUiControlsTable) }
}

impl TablesHost for Array<'_> {
    fn len(&self) -> usize {
        self.0.len()
    }
    fn key(&self, i: usize) -> Option<TableKey> {
        let p = self.0[i];
        (p != 0).then(|| {
            let f = at(p).fields();
            TableKey { id: f.id, form_id: f.form_id, form_index: f.form_index }
        })
    }
    fn layout(&self, i: usize) -> Layout {
        at(self.0[i]).with(|s| s.data.layout)
    }
    fn set_layout(&mut self, i: usize, layout: &Layout) {
        at(self.0[i]).with(|s| s.data.layout = *layout);
    }
}

/// `UIControlsTable::Create`: every record of the database file; a record with an id is a table, registered with its
/// form -> the last read's result.
fn create(file: isize, m: isize, host: EeUiHostFn) -> c_long {
    let h = Host { obj: core::ptr::null_mut(), f: host };
    h.op(Op::TblFileOpen, file, 0, 0, 0);
    let packed = h.op(Op::TblFileReadCount, file, 0, 0, 0) as u64;
    let mut result = packed as u32 as i32 as c_long;
    if result == ERROR_OK {
        let count = (packed >> 32) as usize;
        {
            let mut t = tables();
            t.resize(count, 0);
        }
        // the record is read into the same buffer each time (a failed read leaves the last one)
        let mut record = [0u8; RECORD_SIZE];
        for i in 0..count {
            result = h.op(Op::TblFileRead, file, record.as_mut_ptr() as isize, RECORD_SIZE as isize, 0) as c_long;
            // SAFETY: DbControl is plain data of RECORD_SIZE bytes (any bytes are a record)
            let r: DbControl = unsafe { core::ptr::read_unaligned(record.as_ptr() as *const DbControl) };
            let mut p = 0;
            if ct::record_is_control(r.id) {
                p = EeUiControlsTable::wrap(EeUiControlsTable::from_record(h, &r));
                let form = h.op(Op::TblFormById, r.form_id as isize, 0, 0, m);
                h.op(Op::TblAddDatabaseControl, form, i as isize, 0, 0);
            }
            let mut t = tables();
            if i < t.len() {
                t[i] = p;
            }
        }
    }
    h.op(Op::TblFileClose, file, 0, 0, 0);
    result
}

/// `AddControlLike`: a copy of the template's table (not its events or font) with the new control's own fields,
/// registered with the template's form.
fn add_control_like(m: isize, l: &EeUiTableLike) -> c_long {
    let template = {
        let t = tables();
        let Some(i) = ct::find_template(&Array(&t), l.template_id, l.id, l.form_index) else {
            return ERROR_NOT_FOUND;
        };
        t[i]
    };
    let src = at(template);
    let (host, mut data, names) = src.with(|s| (s.host, s.data, s.names.clone()));
    ct::like(
        &mut data.fields,
        &mut data.layout,
        l.id,
        l.form_index,
        l.group,
        l.left,
        l.right,
        l.top,
        l.bottom,
        l.string_table_begin,
        l.string_table_end,
        l.help_string_id,
    );
    let t = ct::Table {
        fields: data.fields,
        layout: data.layout,
        background: data.background,
        text_foreground: data.text_foreground,
        selected_list_box: data.selected_list_box,
        focused_line: data.focused_line,
        normal_rect: data.normal_rect,
        pressed_rect: data.pressed_rect,
        focused_rect: data.focused_rect,
    };
    let p = EeUiControlsTable::wrap(EeUiControlsTable::make(host, t, names, Vec::new()));
    let index = {
        let mut t = tables();
        t.push(p);
        t.len() - 1
    };
    let form = at(p).form(m);
    at(p).op(Op::TblReserveIndex, form, l.form_index as isize, 0, 0);
    at(p).op(Op::TblAddDatabaseControl, form, index as isize, 0, 0);
    ERROR_OK
}

/// `DeleteAll`: every table deleted (in order), the array emptied.
fn delete_all() {
    let all = core::mem::take(&mut *tables());
    for p in all {
        if p != 0 {
            // the host copied out first: the C++ destructor frees the table during the op
            let h = at(p).with(|s| s.host);
            h.op(Op::TblDelete, h.obj as isize, 0, 0, 0);
        }
    }
}

/// The static calls ([`ee_ui_controls_tables`]).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeUiTablesCall {
    /// `Create(file a, manager b)` -> the last read's result.
    Create = 1,
    /// `AddControlLike(manager a, the arguments at b)` -> `kErrorOK` or `kErrorNotFound`.
    AddControlLike = 2,
    /// `MoveControlsHorizontally(form a, first and last packed in b (low and high 32 bits), f[0], f[1])` -> how many
    /// moved.
    MoveControlsHorizontally = 3,
    /// `DeleteAll()`.
    DeleteAll = 4,
    /// `GetUIControlsTable(a)` -> the C++ table (NULL past the array or for a record that is no control).
    At = 5,
}

/// `UIControlsTable`'s statics ([`EeUiTablesCall`]); `host` is the C++ host for the tables `Create` makes.
///
/// # Safety
/// `f` is NULL or four live floats; `b` is `AddControlLike`'s arguments for that call; the host answers the ops.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_controls_tables(
    call: EeUiTablesCall,
    a: isize,
    b: isize,
    host: EeUiHostFn,
    f: *mut f32,
) -> isize {
    match call {
        EeUiTablesCall::Create => create(a, b, host) as isize,
        // SAFETY: the contract
        EeUiTablesCall::AddControlLike => add_control_like(a, unsafe { &*(b as *const EeUiTableLike) }) as isize,
        EeUiTablesCall::MoveControlsHorizontally => {
            // SAFETY: the contract
            let v = unsafe { read_buf(f) };
            let (first, last) = (b as u64 as u32 as i32 as c_long, (b as u64 >> 32) as u32 as i32 as c_long);
            let t = tables();
            ct::move_horizontally(&mut Array(&t), a as c_long, first, last, v[0], v[1]) as isize
        }
        EeUiTablesCall::DeleteAll => {
            delete_all();
            0
        }
        EeUiTablesCall::At => {
            let p = tables().get(a as usize).copied().unwrap_or(0);
            if p == 0 {
                0
            } else {
                at(p).cpp()
            }
        }
    }
}

/// The default constructor's table for the C++ table `obj`.
///
/// # Safety
/// `obj` is the C++ table the host answers for.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_controls_table_new(obj: *mut c_void, host: EeUiHostFn) -> *mut EeUiControlsTable {
    Box::into_raw(EeUiControlsTable::blank(Host { obj, f: host }))
}

/// Frees a table (the C++ destructor): the font it holds released first.
///
/// # Safety
/// `table` came from this module (or is NULL) and is not used after this returns.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_controls_table_free(table: *mut EeUiControlsTable) {
    if table.is_null() {
        return;
    }
    // SAFETY: the contract
    unsafe { (*table).release() };
    // SAFETY: the contract
    drop(unsafe { Box::from_raw(table) });
}

/// The table's data, read in place by the C++ getters.
///
/// # Safety
/// `table` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_controls_table_data(table: *const EeUiControlsTable) -> *const EeUiControlsTableData {
    // SAFETY: the contract; the object is boxed and never moves
    unsafe { &(*(*table).s.get()).data }
}

/// One of the table's methods ([`EeUiTableCall`]).
///
/// # Safety
/// `table` is live; `f` is NULL or four live floats.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_controls_table_call(
    table: *const EeUiControlsTable,
    call: EeUiTableCall,
    a: isize,
    b: isize,
    f: *mut f32,
) -> isize {
    // SAFETY: the contract; a shared reference: the calls out re-enter through the getters
    unsafe { (*table).call(call, a, b, f) }
}

/// `SetupStringArray`'s count: the strings `begin..=end` (none unless `begin` is positive and `end` not before it).
#[no_mangle]
pub extern "C" fn ee_ui_controls_table_string_count(begin: c_long, end: c_long) -> c_ulong {
    ct::string_count(begin, end)
}

/// The drivers' digest of a constructor's record (FNV-1a over its fields as 32-bit words, in declaration order).
#[cfg(test)]
#[allow(clippy::unnecessary_cast)] // c_ulong is u64 on LP64 targets
pub(crate) fn make_digest(a: &EeUiTableMake) -> isize {
    let words: [u32; 30] = [
        a.kind as u32,
        a.id as u32,
        a.form as u32,
        a.index as u32,
        a.border as u32,
        a.font as u32,
        a.text as u32,
        a.strings as u32,
        a.normal as u32,
        a.focused as u32,
        a.pressed as u32,
        a.style as u32,
        a.max_chars as u32,
        a.blink_rate as u32,
        a.list_height as u32,
        a.flag_a as u32 | (a.flag_b as u32) << 1 | (a.flag_c as u32) << 2,
        a.rect.bottom.to_bits(),
        a.rect.left.to_bits(),
        a.rect.right.to_bits(),
        a.rect.top.to_bits(),
        a.normal_rect.left.to_bits() ^ a.normal_rect.top.to_bits().rotate_left(8),
        a.normal_rect.right.to_bits() ^ a.normal_rect.bottom.to_bits().rotate_left(8),
        a.focused_rect.left.to_bits() ^ a.focused_rect.top.to_bits().rotate_left(8),
        a.focused_rect.right.to_bits() ^ a.focused_rect.bottom.to_bits().rotate_left(8),
        a.pressed_rect.left.to_bits() ^ a.pressed_rect.top.to_bits().rotate_left(8),
        a.pressed_rect.right.to_bits() ^ a.pressed_rect.bottom.to_bits().rotate_left(8),
        a.length.to_bits(),
        0,
        0,
        0,
    ];
    let mut h: u32 = 2166136261;
    for w in words {
        for b in w.to_le_bytes() {
            h = (h ^ b as u32).wrapping_mul(16777619);
        }
    }
    h as isize
}

#[cfg(test)]
#[path = "tableobj_tests.rs"]
mod tests;
