//! The generic widget shells (R6 WONLobby round 20): one C++ shell class per WONGUI base (`WONShellComp :
//! Component`, `WONShellButton : Button`, `WONShellMsButton : MSButtonBase`, `WONShellInput : InputBox`, ...;
//! include/ee/wonapi/WONGUIShell.h), one Rust kind per deleted concrete widget class (MSButton, MSCheckbox,
//! MSArrowButton, MS3DFrame, ...: shell_ms.rs).
//!
//! The shell's virtuals ask the kind when its mask says the kind overrides them ([`ShellKind::overrides`], a bit per
//! [`EeWonShellVirt`]), else run the base class's; the kind calls the base's back through the host's `Base` op (the
//! shell's `BaseCall`: `Base::Paint(g)`). The kind's own state is Rust (`Cell`s: a base call re-enters), its own
//! methods the shell's ops ([`EeWonShellOp`]: `WONShell::Op(control, op, ...)` for the callers of a deleted class's
//! methods). The constructor's body is the kind's `init`, run after the mask is known, so a virtual the body calls
//! reaches the kind as the derived class's own would have; the destructor's body is `free`.
//!
//! The kind's calls out go through [`Sh`]: in the game the base classes' Rust objects answer the component's,
//! button's and MS button's calls (compobj.rs, the registry), as the C++ inline forwards did; the C++ host answers
//! the rest (the base call, a draw, the wrapped text). Without the game (the drivers) every call is a host op, so a
//! driver's scripted host traces them all.

use std::ffi::{c_char, c_void, CStr};

use crate::btnobj::{EeWonBtnCall, EeWonBtnField, EeWonButton};
use crate::compobj::EeWonCompCall;
use crate::guidraw::{EeWonDrawOp, EE_WON_DRAW_CALL};
use crate::msbtnobj::{EeWonMsBtnCall, EeWonMsBtnField, EeWonMsButton};
use crate::mspaint::{EeWonMsPaint, EeWonMsPaintKind, Plan, EE_WON_MSPAINT_CALL_TEXT};
use crate::registry::{self, EeWonGuiKind};

/// A deleted widget class: the kind a shell runs.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonShellKind {
    /// `MSButton` (`a` the text, a `GUIString*`, or 0 for the default constructor).
    MsButton = 1,
    /// `MSHeaderButton` (`a` the text).
    MsHeaderButton,
    /// `MSCheckbox`.
    MsCheckbox,
    /// `MSRadioButton`.
    MsRadioButton,
    /// `MSLabel` (`a` the text, or 0 for the default constructor).
    MsLabel,
    /// `MSTabButton` (`a` the text or 0 for "", `b` fill the whole tab, `c` the lower amount).
    MsTabButton,
    /// `MSArrowButton` (`a` the direction).
    MsArrowButton,
    /// `MaxSplitterButton` (`a` the direction).
    MaxSplitterButton,
    /// `MSCloseButton`.
    MsCloseButton,
    /// `MS3DFrame` (`a` the type, `b` raised).
    Ms3DFrame,
    /// `MSSeperator` (`a` the height).
    MsSeperator,
    /// `MSEtchedFrame`.
    MsEtchedFrame,
    /// `MSScrollThumb`.
    MsScrollThumb,
    /// `MSPopup`.
    MsPopup,
    /// `MSDlgBack`.
    MsDlgBack,
    /// `MSInputBox`.
    MsInputBox,
    /// `MSComboBox`.
    MsComboBox,
    /// `MSScrollbar` (`a` vertical).
    MsScrollbar,
    /// `MSScrollContainer` (`a` the scroll area, a `ScrollArea*`, or 0).
    MsScrollContainer,
    /// `MSListCtrl` (`a` the list, a `MultiListArea*`, or 0 for a new one).
    MsListCtrl,
    /// `MSHeaderControl` (`a` the list, a `MultiListArea*`, or 0).
    MsHeaderControl,
    /// `MSTabCtrl`.
    MsTabCtrl,
    /// `MSEtchedFrameGroup`.
    MsEtchedFrameGroup,
    /// `MSMessageBox` over `MSDialog` (`a` its texts, an `EeWonMsgBoxTexts*`, or 0 for the dialog alone).
    MsMessageBox,
}

/// The C++ shell class a kind's widget is (its base class).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonShellBase {
    /// `WONShellComp : Component`.
    Component = 1,
    /// `WONShellButton : Button`.
    Button,
    /// `WONShellMsButton : MSButtonBase`.
    MsButton,
    /// `WONShellInput : InputBox`.
    Input,
    /// `WONShellContainer : Container`.
    Container,
    /// `WONShellCombo : ComboBox`.
    Combo,
    /// `WONShellBar : Scrollbar` (made `Scrollbar(a != 0)`).
    Scrollbar,
    /// `WONShellScroller : ScrollbarScroller`.
    Scroller,
    /// `WONShellHeader : MultiListHeader` (made `MultiListHeader((MultiListArea*)a)`).
    Header,
    /// `WONShellTab : TabCtrl`.
    Tab,
    /// `WONShellDialog : Dialog`.
    Dialog,
}

/// A base class's widget a kind makes (the host's `NewBase`).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonShellBaseNew {
    InputBox = 1,
    ListArea,
    MultiListArea,
    Button,
    Label,
}

/// A combo box's part member (the host's `ComboPart`: `mButton`, `mEdit`, `mListArea`, `mListBox`).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonShellComboPart {
    Button = 1,
    Edit,
    ListArea,
    ListBox,
}

/// A virtual a shell overrides (its bit in the kind's mask). The arguments are the C++ ones as `isize` (a pointer
/// for a reference).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonShellVirt {
    /// `Virtual08()` (the size changed).
    SizeChanged = 0,
    /// `GetDesiredSize(*(int*)a, *(int*)b)`.
    DesiredSize,
    /// `Paint(*(Graphics*)a)`.
    Paint,
    /// `MouseDown(a, b, c)`.
    MouseDown,
    /// `MouseUp(a, b, c)`.
    MouseUp,
    /// `MouseMove(a, b)`.
    MouseMove,
    /// `MouseDrag(a, b)`.
    MouseDrag,
    /// `MouseEnter(a, b)`.
    MouseEnter,
    /// `MouseExit()`.
    MouseExit,
    /// `MouseWheel(a)` (0 or 1).
    MouseWheel,
    /// `KeyDown(a)` (0 or 1).
    KeyDown,
    /// `KeyChar(a)` (0 or 1).
    KeyChar,
    /// `KeyUp(a)` (0 or 1).
    KeyUp,
    /// `TimerEvent(a)` (0 or 1).
    TimerEvent,
    /// `GotFocus()`.
    GotFocus,
    /// `LostFocus()`.
    LostFocus,
    /// `AddedToParent()`.
    AddedToParent,
    /// `HandleComponentEvent((ComponentEvent*)a)`.
    HandleEvent,
    /// `SetEnabled(a != 0)`.
    SetEnabled,
    /// `SetVisible(a != 0)`.
    SetVisible,
}

/// The mask bit of `v`.
pub const fn bit(v: EeWonShellVirt) -> u64 {
    1u64 << (v as u32)
}

/// A deleted class's own method (`WONShell::Op(control, op, a, b)`): 0 for a control of another kind.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonShellOp {
    /// `MSButton::SetUseFrame2(a != 0)`.
    SetUseFrame2 = 1,
    /// `MSArrowButton::SetDirection(a)`.
    SetDirection,
    /// `MSTabButton::SetLowerAmount(a)`.
    SetLowerAmount,
    /// `MS3DFrame::SetFrameType(a)`.
    SetFrameType,
    /// `MS3DFrame::SetIsUp(a != 0)`.
    SetIsUp,
    /// `MSComboBox::SetDrawFrame`, `MSScrollContainer::SetDrawFrame(a != 0)`.
    SetDrawFrame,
    /// `MSScrollContainer::SetDraw3DFrame(a != 0)`.
    SetDraw3DFrame,
    /// `MSScrollbar`'s, `MSScrollContainer`'s, `MSTabCtrl`'s `SetTransparent(a != 0)`.
    SetTransparent,
    /// `MSListCtrl::SetHeaderHeight(a)`.
    SetHeaderHeight,
    /// `MSListCtrl::GetHeader()` (a `Component*`).
    GetHeader,
    /// `MSHeaderControl::SetColumn(a, *(const GUIString*)b)`.
    SetColumn,
    /// `MSTabCtrl::AddStandardTab(*(const GUIString*)a, (Container*)b)`.
    AddStandardTab,
    /// `MSEtchedFrameGroup::SetLabelText(*(const GUIString*)a)`.
    SetLabelText,
    /// `MSEtchedFrameGroup::AddContainedComponent((Component*)a)`.
    AddContainedComponent,
    /// `MSEtchedFrameGroup::GetLabel()` (a `Component*`).
    GetLabel,
    /// `MSMessageBox`'s `mButton1` (a `Component*`).
    GetButton1,
}

/// A call out of a kind (the C++ host's op).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonShellHostOp {
    /// the base class's virtual `a` (`EeWonShellVirt`) with `b`, `c`, `d` (the shell's `BaseCall`).
    Base = 1,
    /// `WONDrawPlain(*(Graphics*)a, *(const EeWonDrawOp*)b)`.
    Draw,
    /// Component's call `a` (`EeWonCompCall`) with `b`, `c`, `d` (the game: compobj.rs).
    Comp,
    /// Button's call `a` (`EeWonBtnCall`) with `b`, `c`, `d` (the game: the registered btnobj.rs object).
    Btn,
    /// MSButtonBase's call `a` (`EeWonMsBtnCall`) with `b`, `c`, `d` (the game: the registered msbtnobj.rs object).
    MsBtn,
    /// MSButtonBase's `mText->SetText(*(const GUIString*)a)`.
    WrapSetText,
    /// MSButtonBase's `mText->GetWidth()`.
    WrapWidth,
    /// `MultiListHeader::GetSortTypeForButton(this)`.
    SortType,
    /// a Rust widget object's call on another component (`a` it; `b` the object's kind << 16 | the call; `c`, `d`)
    /// (the game: the registered object).
    Call,
    /// `WONShellNew(a, b, c, d)`: a new widget of the kind `a` (a `Component*`, not yet held).
    New,
    /// a new widget of the base class `a` (`EeWonShellBaseNew`).
    NewBase,
    /// `((Component*)a)->AddRef()` when `b` > 0, else its `Release()`.
    Ref,
    /// the shell's own `Virtual08()` (through its vtable).
    SelfVirt,
    /// the skin layout host's op `a` with `b`, `c`, `d` (`ee_won_guicpp_skin_call`).
    SkinCall,
    /// `Fn2F6140((Component*)a, b, (Component*)c, d[0..5])`: the child's layout against a reference.
    Layout,
    /// a tab control's `mContainerLayout`.
    TabLayoutPoint,
    /// `((Label*)a)->SetText(*(const GUIString*)b)`.
    LabelSetText,
    /// `a`'s `SetFont(GetNamedFont("MSButton"))`, `b` its class (`EeWonShellBaseNew`: an input box or a list).
    NamedFont,
    /// a combo box's part `a` (`EeWonShellComboPart`) = `(Component*)b`.
    ComboPart,
    /// `((MSButtonBase*)a)->mText->GetWidth()`.
    WrapWidthOf,
    /// `((MSButtonBase*)a)->SetWrap(b != 0)`.
    SetWrap,
    /// `(new SameSizeLayout(a, true, true))->Add(b)->Add(c)`, executed on the shell's container.
    SameSize,
    /// `CenterLayout(this, a, true, false)` with `b` and `c` (when not 0) added, added to the shell's child layouts.
    CenterLayout,
}

/// The C++ host: `(shell, op, a, b, c, d) -> result`, `shell` the C++ `WONShell*` the kind belongs to.
pub type EeWonShellHostFn = Option<
    unsafe extern "C" fn(shell: *mut c_void, op: EeWonShellHostOp, a: isize, b: isize, c: isize, d: isize) -> isize,
>;

/// A kind's view of its shell: the widget (`Component*`), the C++ shell and its host.
pub struct Sh {
    comp: isize,
    me: *mut c_void,
    host: EeWonShellHostFn,
}

impl Sh {
    /// The widget, a `Component*`.
    pub fn comp(&self) -> isize {
        self.comp
    }
    /// One host op.
    pub fn host(&self, op: EeWonShellHostOp, a: isize, b: isize, c: isize, d: isize) -> isize {
        match self.host {
            // SAFETY: the host was given with this shell; the kinds hold no borrow across a call
            Some(h) => unsafe { h(self.me, op, a, b, c, d) },
            None => 0,
        }
    }
    /// The base class's virtual `v`.
    pub fn base(&self, v: EeWonShellVirt, a: isize, b: isize, c: isize) -> isize {
        self.host(EeWonShellHostOp::Base, v as isize, a, b, c)
    }
    /// Component's call `k` (the C++ `KCall`).
    pub fn comp_call(&self, k: EeWonCompCall, a: isize, b: isize, c: isize) -> isize {
        if crate::compobj::is_game() {
            return crate::compobj::Comp::new(crate::compobj::host()).call(self.comp, k, a, b, c);
        }
        self.host(EeWonShellHostOp::Comp, k as isize, a, b, c)
    }
    /// Button's call `k` (the C++ `BCall`).
    pub fn btn(&self, k: EeWonBtnCall, a: isize, b: isize, c: isize) -> isize {
        // SAFETY: the kind's type; the widget is live for its own call
        match unsafe { registry::get::<EeWonButton>(self.comp, EeWonGuiKind::Button) } {
            Some(o) => o.call(k, a, b, c),
            None => self.host(EeWonShellHostOp::Btn, k as isize, a, b, c),
        }
    }
    /// MSButtonBase's call `k` (the C++ `MCall`).
    pub fn msb(&self, k: EeWonMsBtnCall, a: isize, b: isize, c: isize) -> isize {
        // SAFETY: as `btn`
        match unsafe { registry::get::<EeWonMsButton>(self.comp, EeWonGuiKind::MsButton) } {
            Some(o) => o.call(k, a, b, c),
            None => self.host(EeWonShellHostOp::MsBtn, k as isize, a, b, c),
        }
    }
    /// A Rust widget object's call on another component `p` (the registry's `kind`): the object's in the game, else the
    /// host's `Call` op.
    fn call_on<T: CallObject>(&self, p: isize, kind: EeWonGuiKind, k: T::Call, a: isize, b: isize) -> isize {
        // SAFETY: the kind's type; the component is live for the call
        match unsafe { registry::get::<T>(p, kind) } {
            Some(o) => o.call_obj(k, a, b),
            None => self.host(EeWonShellHostOp::Call, p, ((kind as isize) << 16) | T::code(k), a, b),
        }
    }
    /// Component's call on `p` (`p->SetSize(..)`, ...).
    pub fn comp_on(&self, p: isize, k: EeWonCompCall, a: isize, b: isize) -> isize {
        if crate::compobj::is_game() {
            return crate::compobj::Comp::new(crate::compobj::host()).call(p, k, a, b, 0);
        }
        self.host(EeWonShellHostOp::Call, p, k as isize, a, b)
    }
    pub fn btn_on(&self, p: isize, k: EeWonBtnCall, a: isize, b: isize) -> isize {
        self.call_on::<EeWonButton>(p, EeWonGuiKind::Button, k, a, b)
    }
    pub fn list_on(&self, p: isize, k: crate::listobj::EeWonListCall, a: isize, b: isize) -> isize {
        self.call_on::<crate::listobj::EeWonList>(p, EeWonGuiKind::List, k, a, b)
    }
    pub fn ct_on(&self, p: isize, k: crate::ctobj::EeWonCtCall, a: isize, b: isize) -> isize {
        self.call_on::<crate::ctobj::EeWonContainer>(p, EeWonGuiKind::Container, k, a, b)
    }
    pub fn label_on(&self, p: isize, k: crate::labelobj::EeWonLabelCall, a: isize, b: isize) -> isize {
        self.call_on::<crate::labelobj::EeWonLabel>(p, EeWonGuiKind::Label, k, a, b)
    }
    pub fn scroller_on(&self, p: isize, k: crate::scrollerobj::EeWonScrollerCall, a: isize, b: isize) -> isize {
        self.call_on::<crate::scrollerobj::EeWonScroller>(p, EeWonGuiKind::Scroller, k, a, b)
    }
    pub fn bar_on(&self, p: isize, k: crate::barobj::EeWonBarCall, a: isize, b: isize) -> isize {
        self.call_on::<crate::barobj::EeWonScrollbar>(p, EeWonGuiKind::Bar, k, a, b)
    }
    pub fn mhead_on(&self, p: isize, k: crate::multiobj::EeWonMHeadCall, a: isize, b: isize) -> isize {
        self.call_on::<crate::multiobj::EeWonMHead>(p, EeWonGuiKind::MHead, k, a, b)
    }
    pub fn msb_on(&self, p: isize, k: EeWonMsBtnCall, a: isize, b: isize) -> isize {
        self.call_on::<EeWonMsButton>(p, EeWonGuiKind::MsButton, k, a, b)
    }
    pub fn dlg_on(&self, p: isize, k: crate::dialogobj::EeWonDlgCall, a: isize, b: isize) -> isize {
        self.call_on::<crate::dialogobj::EeWonDialog>(p, EeWonGuiKind::Dialog, k, a, b)
    }
    pub fn tab_on(&self, p: isize, k: crate::tabobj::EeWonTabCall, a: isize, b: isize) -> isize {
        self.call_on::<crate::tabobj::EeWonTabCtrl>(p, EeWonGuiKind::Tab, k, a, b)
    }
    /// Another shell's own method (`WONShell::Op(p, op, a, b)`).
    pub fn shell_on(&self, p: isize, op: EeWonShellOp, a: isize, b: isize) -> isize {
        self.call_on::<EeWonShell>(p, EeWonGuiKind::Shell, op, a, b)
    }
    /// The shell's own container's `Fn2F6140(child, flags, reference, r[0..5])`.
    pub fn layout(&self, child: isize, flags: i32, reference: isize, r: [i32; 5]) {
        self.host(EeWonShellHostOp::Layout, child, flags as isize, reference, r.as_ptr() as isize);
    }
    /// The skins' layout `kind` of the widget (`WONSkinLayout(this, kind, a, b, c, w, h)`): the kind answers its own
    /// fields and parts, the C++ skin host the rest.
    pub fn skin_layout(
        &self,
        kind: crate::skinlay::EeWonSkinLayKind,
        a: i32,
        b: isize,
        c: i32,
        wh: Option<(isize, isize)>,
    ) {
        let (mut w, mut h) = (0i32, 0i32);
        if let Some((pw, ph)) = wh {
            // SAFETY: the C++ passes two live ints
            unsafe {
                w = *(pw as *const i32);
                h = *(ph as *const i32);
            }
        }
        crate::skinlay::layout(kind, self.comp as *mut c_void, Some(shell_skin_host), a, b, c, &mut w, &mut h);
        if let Some((pw, ph)) = wh {
            // SAFETY: as above
            unsafe {
                *(pw as *mut i32) = w;
                *(ph as *mut i32) = h;
            }
        }
    }
    /// MSButtonBase's `MSet(f, v)`.
    pub fn msb_set(&self, f: EeWonMsBtnField, v: i32) {
        self.msb(EeWonMsBtnCall::Set, f as isize, v as isize, 0);
    }
    /// Component's `SetComponentFlags(f, on)`.
    pub fn set_flags(&self, f: i32, on: bool) {
        self.comp_call(EeWonCompCall::SetFlags, f as isize, on as isize, 0);
    }
    /// Component's `SetDesiredSize(w, h)`.
    pub fn set_desired_size(&self, w: i32, h: i32) {
        self.comp_call(EeWonCompCall::SetDesiredSize, w as isize, h as isize, 0);
    }
    /// Component's `SetSize(w, h)`.
    pub fn set_size(&self, w: i32, h: i32) {
        self.comp_call(EeWonCompCall::SetSize, w as isize, h as isize, 0);
    }
    /// Button's `SetButtonFlags(f, on)`.
    pub fn set_button_flags(&self, f: u32, on: bool) {
        self.btn(EeWonBtnCall::SetButtonFlags, f as isize, on as isize, 0);
    }
    /// Button's flags (`GetButtonFlags()`).
    pub fn button_flags(&self) -> u32 {
        self.btn(EeWonBtnCall::GetField, EeWonBtnField::Flags as isize, 0, 0) as u32
    }
    /// `IsPressed()`.
    pub fn is_pressed(&self) -> bool {
        self.button_flags() & 0x20 != 0
    }
    /// `IsChecked()`.
    pub fn is_checked(&self) -> bool {
        self.button_flags() & 0x04 != 0
    }
    /// The widget's width, height and disabled bit (compmem.rs's block).
    pub fn width(&self) -> i32 {
        crate::compmem::width(self.comp)
    }
    pub fn height(&self) -> i32 {
        crate::compmem::height(self.comp)
    }
    pub fn is_disabled(&self) -> bool {
        crate::compmem::is_disabled(self.comp)
    }
    /// One draw operation on the Graphics `g`.
    pub fn draw(&self, g: isize, o: &EeWonDrawOp) {
        self.host(EeWonShellHostOp::Draw, g, o as *const EeWonDrawOp as isize, 0, 0);
    }
    /// The Windows skin's paint `p` (`WONMsPaint(g, p, text)`): its plan's operations, the text call the MS button
    /// base's `DrawButtonText` when `text`.
    pub fn ms_paint(&self, g: isize, p: &EeWonMsPaint, text: bool) {
        let mut plan = Plan::default();
        plan.paint(p);
        for o in &plan.ops {
            if o.kind == EE_WON_DRAW_CALL {
                if text && o.a == EE_WON_MSPAINT_CALL_TEXT {
                    self.msb(EeWonMsBtnCall::DrawButtonText, g, o.b as isize, o.c as isize);
                }
            } else {
                self.draw(g, o);
            }
        }
    }
}

/// `WONMsPaintOf(kind, width, height, flags, a, b, c)`.
pub fn ms_paint_of(kind: EeWonMsPaintKind, w: i32, h: i32, flags: u32, a: i32, b: i32, c: i32) -> EeWonMsPaint {
    EeWonMsPaint { kind, flags, width: w, height: h, a, b, c, d: 0, e: 0 }
}

/// The length of a C++ `GUIString` at `p` (its data pointer first), read in place.
///
/// # Safety
/// `p` is a live `GUIString` (or NULL: 0).
pub unsafe fn gui_string_len(p: isize) -> usize {
    if p == 0 {
        return 0;
    }
    // SAFETY: a GUIString is its `GUIStringData*` (the contract)
    let data = unsafe { *(p as *const *const c_void) };
    // SAFETY: the data is a live GUIStringData or NULL
    unsafe { crate::guistrobj::data_units(data) }.len()
}

/// A deleted class as a Rust kind.
pub trait ShellKind {
    /// The class's name.
    fn name(&self) -> &'static CStr;
    /// The virtuals the class overrides (bits of [`EeWonShellVirt`]).
    fn overrides(&self) -> u64;
    /// The constructor's body (`args` the shell's constructor arguments).
    fn init(&self, _sh: &Sh, _args: [isize; 3]) {}
    /// An overridden virtual.
    fn virt(&self, sh: &Sh, v: EeWonShellVirt, a: isize, b: isize, c: isize) -> isize;
    /// One of the class's own methods.
    fn op(&self, _sh: &Sh, _op: EeWonShellOp, _a: isize, _b: isize) -> isize {
        0
    }
    /// The destructor's body.
    fn free(&self, _sh: &Sh) {}
    /// A skin layout's question `w` (a part or field `which` of the receiver) when the kind holds the answer.
    fn skin(&self, _sh: &Sh, _w: crate::skinlay::EeWonSkinLay, _which: isize) -> Option<isize> {
        None
    }
}

/// A Rust widget object a kind calls on another component (the registry's).
pub trait CallObject {
    type Call: Copy;
    fn call_obj(&self, k: Self::Call, a: isize, b: isize) -> isize;
    fn code(k: Self::Call) -> isize;
}
macro_rules! call_object {
    ($t:ty, $c:ty) => {
        impl CallObject for $t {
            type Call = $c;
            fn call_obj(&self, k: $c, a: isize, b: isize) -> isize {
                self.call(k, a, b, 0)
            }
            fn code(k: $c) -> isize {
                k as isize
            }
        }
    };
}
call_object!(EeWonButton, EeWonBtnCall);
call_object!(crate::listobj::EeWonList, crate::listobj::EeWonListCall);
call_object!(crate::ctobj::EeWonContainer, crate::ctobj::EeWonCtCall);
call_object!(crate::labelobj::EeWonLabel, crate::labelobj::EeWonLabelCall);
call_object!(crate::scrollerobj::EeWonScroller, crate::scrollerobj::EeWonScrollerCall);
call_object!(crate::barobj::EeWonScrollbar, crate::barobj::EeWonBarCall);
call_object!(crate::multiobj::EeWonMHead, crate::multiobj::EeWonMHeadCall);
call_object!(EeWonMsButton, EeWonMsBtnCall);
call_object!(crate::dialogobj::EeWonDialog, crate::dialogobj::EeWonDlgCall);
impl CallObject for crate::tabobj::EeWonTabCtrl {
    type Call = crate::tabobj::EeWonTabCall;
    fn call_obj(&self, k: crate::tabobj::EeWonTabCall, a: isize, b: isize) -> isize {
        self.call(k, a, b)
    }
    fn code(k: crate::tabobj::EeWonTabCall) -> isize {
        k as isize
    }
}
impl CallObject for EeWonShell {
    type Call = EeWonShellOp;
    fn call_obj(&self, k: EeWonShellOp, a: isize, b: isize) -> isize {
        self.op(k, a, b)
    }
    fn code(k: EeWonShellOp) -> isize {
        k as isize
    }
}

/// The skin layouts' host for a shell's widget: the kind's answers, else the C++ skin host (through the shell's).
unsafe extern "C" fn shell_skin_host(
    me: *mut c_void,
    op: crate::guihost::EeWonGuiOp,
    a: isize,
    b: isize,
    c: isize,
    d: isize,
) -> isize {
    use crate::skinlay::EeWonSkinLay as L;
    // SAFETY: the widget whose layout runs is live
    let Some(s) = (unsafe { registry::get::<EeWonShell>(me as isize, EeWonGuiKind::Shell) }) else {
        return 0;
    };
    if op != crate::guihost::EeWonGuiOp::SkinLay {
        return 0;
    }
    let w = match a {
        x if x == L::Part as isize => Some(L::Part),
        x if x == L::Field as isize => Some(L::Field),
        _ => None,
    };
    if let Some(w) = w {
        if let Some(r) = s.imp.skin(&s.sh, w, c) {
            return r;
        }
    }
    s.sh.host(EeWonShellHostOp::SkinCall, a, b, c, d)
}

/// A shell's Rust state: its view and its kind.
pub struct EeWonShell {
    sh: Sh,
    kind: EeWonShellKind,
    imp: Box<dyn ShellKind>,
}

impl EeWonShell {
    /// A shell over `imp` (another crate's kinds make theirs with this).
    pub fn new(
        kind: EeWonShellKind,
        imp: Box<dyn ShellKind>,
        comp: isize,
        me: *mut c_void,
        host: EeWonShellHostFn,
    ) -> Self {
        EeWonShell { sh: Sh { comp, me, host }, kind, imp }
    }
    pub fn sh(&self) -> &Sh {
        &self.sh
    }
    pub fn kind(&self) -> EeWonShellKind {
        self.kind
    }
    pub fn overrides(&self) -> u64 {
        self.imp.overrides()
    }
    pub fn init(&self, args: [isize; 3]) {
        self.imp.init(&self.sh, args)
    }
    pub fn virt(&self, v: EeWonShellVirt, a: isize, b: isize, c: isize) -> isize {
        self.imp.virt(&self.sh, v, a, b, c)
    }
    pub fn op(&self, op: EeWonShellOp, a: isize, b: isize) -> isize {
        self.imp.op(&self.sh, op, a, b)
    }
    pub fn name(&self) -> &'static CStr {
        self.imp.name()
    }
}

/// The C++ shell class of `kind`'s widget.
#[no_mangle]
pub extern "C" fn ee_won_shell_base(kind: EeWonShellKind) -> EeWonShellBase {
    crate::shell_ms::base(kind)
}

/// A new shell of `kind` for the widget `comp` (a `Component*`), its C++ shell `me` and host.
#[no_mangle]
pub extern "C" fn ee_won_shell_new(
    kind: EeWonShellKind,
    comp: *mut c_void,
    me: *mut c_void,
    host: EeWonShellHostFn,
) -> *mut EeWonShell {
    let made = Box::into_raw(Box::new(EeWonShell::new(kind, crate::shell_ms::make(kind), comp as isize, me, host)));
    registry::register(comp, EeWonGuiKind::Shell, made as *const c_void);
    made
}

/// # Safety
/// `s` came from `ee_won_shell_new` (or a kind crate's maker) and is live.
unsafe fn shell<'a>(s: *const EeWonShell) -> &'a EeWonShell {
    // SAFETY: the contract
    unsafe { &*s }
}

/// The kind's override mask.
///
/// # Safety
/// `s` is a live shell.
#[no_mangle]
pub unsafe extern "C" fn ee_won_shell_overrides(s: *const EeWonShell) -> u64 {
    // SAFETY: the contract
    unsafe { shell(s) }.overrides()
}

/// The constructor's body.
///
/// # Safety
/// `s` is a live shell; the arguments are what the kind's constructor takes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_shell_init(s: *const EeWonShell, a: isize, b: isize, c: isize) {
    // SAFETY: the contract
    unsafe { shell(s) }.init([a, b, c])
}

/// An overridden virtual.
///
/// # Safety
/// `s` is a live shell; the arguments are the virtual's.
#[no_mangle]
pub unsafe extern "C" fn ee_won_shell_virtual(
    s: *const EeWonShell,
    v: EeWonShellVirt,
    a: isize,
    b: isize,
    c: isize,
) -> isize {
    // SAFETY: the contract
    unsafe { shell(s) }.virt(v, a, b, c)
}

/// One of the kind's own methods.
///
/// # Safety
/// `s` is a live shell.
#[no_mangle]
pub unsafe extern "C" fn ee_won_shell_op(s: *const EeWonShell, op: EeWonShellOp, a: isize, b: isize) -> isize {
    // SAFETY: the contract
    unsafe { shell(s) }.op(op, a, b)
}

/// The shell's kind.
///
/// # Safety
/// `s` is a live shell.
#[no_mangle]
pub unsafe extern "C" fn ee_won_shell_kind(s: *const EeWonShell) -> EeWonShellKind {
    // SAFETY: the contract
    unsafe { shell(s) }.kind()
}

/// The deleted class's name (NUL-terminated, static).
///
/// # Safety
/// `s` is a live shell.
#[no_mangle]
pub unsafe extern "C" fn ee_won_shell_name(s: *const EeWonShell) -> *const c_char {
    // SAFETY: the contract
    unsafe { shell(s) }.name().as_ptr()
}

/// The destructor's body, then the shell goes.
///
/// # Safety
/// `s` came from `ee_won_shell_new` (or is NULL) and is not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_won_shell_free(s: *mut EeWonShell) {
    if s.is_null() {
        return;
    }
    // SAFETY: the contract
    let b = unsafe { Box::from_raw(s) };
    b.imp.free(&b.sh);
    registry::unregister(s as *const c_void);
}

#[cfg(test)]
#[path = "shell_tests.rs"]
pub(crate) mod tests;
