//! The table setters that call a widget's Rust object (R6 WONLobby round 27).
//!
//! A setter of the table reader (cfgtab.rs: `table << 8 | n`) was one case of the C++ host's apply
//! (include/ee/wonapi/WONCfgTable.inl): a call of the configured component's method under the config kind's type,
//! over the frame's values (`EE_C(Label)->SetTextColor(i[0])`). Where that method is an inline forward to the
//! component's Rust object (`Label::SetTextColor` is `ee_won_label_call(mRust, SetTextColor, c, 0, 0)`), the setter is
//! here: the component's object of the forward's kind is the widget registry's (registry.rs), and the call is the
//! forward's, with the arguments converted as the C++ expressions and parameter types converted them. The setters
//! on C++ members (fonts held by a wrapped text, backgrounds, selection colours, images held by the C++ class), the
//! virtual ones and the out-of-line ones stay the host's.
//!
//! Each setter is described twice: the method the original statement called and its arguments as the statement's
//! expressions had them (what the table golden, won_cfgtab_test, traces), and the forward's calls on the Rust objects
//! (what the forward golden, won_cfgset_test, compares with the game's inline forwards). A trace hook installed by a
//! driver receives the description instead of the calls being made.
//!
//! A shell op on a component that is not a shell does nothing, as `WONShell::Op`'s did; a forward whose object the
//! registry does not hold (no C++ widget of that type has that address) is not made.

use std::ffi::c_void;
use std::sync::atomic::{AtomicPtr, AtomicUsize, Ordering};

use crate::barobj::EeWonBarCall;
use crate::btnobj::EeWonBtnCall;
use crate::cfgtab::EeWonCfgTabHost;
use crate::comboobj::{EeWonComboCall, EeWonComboField};
use crate::dialogobj::{EeWonDlgCall, EeWonDlgField};
use crate::guiconf::*;
use crate::inputobj::EeWonInputCall;
use crate::labelobj::EeWonLabelCall;
use crate::listobj::EeWonListCall;
use crate::msbtnobj::{EeWonMsBtnCall, EeWonMsBtnField};
use crate::multiobj::{EeWonMHeadCall, EeWonMultiCall};
use crate::paneobj::EeWonPaneCall;
use crate::registry::{self, EeWonGuiKind};
use crate::scrollerobj::EeWonScrollerCall;
use crate::shell::EeWonShellOp;
use crate::splitobj::EeWonSplitCall;
use crate::tabobj::{EeWonTabCall, EeWonTabField};
use crate::textobj::EeWonTextCall;
use crate::treeobj::EeWonTreeCall;
use crate::wonbtnobj::{EeWonWbCall, EeWonWbField};

/// A value of the host's frame (`EeWonCfgTabHost::arg`).
#[repr(i32)]
#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum EeWonCfgArg {
    /// `i[slot]`.
    Int = 0,
    /// The ints' count (`ni`).
    IntCount = 1,
    /// `&s[slot]` (a `GUIString*`).
    StrPtr = 2,
    /// `f[slot].get()` (a `Font*`).
    Font = 3,
    /// The fonts' count (`nf`).
    FontCount = 4,
    /// `m[slot].get()` (an `Image*`).
    Image = 5,
    /// `c[slot]` (a `Component*`).
    Comp = 6,
    /// The configured component (`mConfig->mComponent.get()`).
    Component = 7,
    /// `s[slot].mData->mText` (its units).
    StrUnits = 8,
    /// `s[slot].length()`.
    StrLen = 9,
    /// The components' count (`nc`).
    CompCount = 10,
}

/// The Rust function a forward called (its object the registry's of the component).
#[repr(i32)]
#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum EeWonCfgFun {
    /// `ee_won_btn_call(mBtn, op, a, b, c)`
    Btn = 0,
    /// `ee_won_msbtn_call(mMsb, op, a, b, c)`
    MsBtn,
    /// `ee_won_shell_op(mShell, op, a, b)` (WONShell::Op)
    Shell,
    /// `ee_won_multi_call(mMulti, op, a, b, c)`
    Multi,
    /// `ee_won_mhead_call(mHead, op, a, b, c)`
    MHead,
    /// `ee_won_bar_call(mRust, op, a, b, c)`
    Bar,
    /// `ee_won_tab_call(mTab, op, a, b)`
    Tab,
    /// `ee_won_combo_call(mCombo, op, a, b, c)`
    Combo,
    /// `ee_won_dlg_call(mDlg, op, a, b, c)`
    Dlg,
    /// `ee_won_label_call(mRust, op, a, b, 0)`
    Label,
    /// `ee_won_list_call(mRust, op, a, b, c)`
    List,
    /// `ee_won_input_call(mRust, op, a, b, c)`
    Input,
    /// `ee_won_text_call(mRust, op, a, b, c)`
    Text,
    /// `ee_won_tree_call(mTree, op, a, b, c)`
    Tree,
    /// `ee_won_wonbtn_call(mWb, op, a, b, c, d)`
    WonBtn,
    /// `ee_won_wonbtn_set_state(mWb, a, b, c, d, e != 0)` (op 0)
    WonBtnState,
    /// `ee_won_split_call(mRust, op, a, b, c)`
    Split,
    /// `ee_won_pane_call(mPane, op, a, b, c)`
    Pane,
    /// `ee_won_scroller_call(mRust, op, a, b, c)`
    Scroller,
}

impl EeWonCfgFun {
    fn kind(self) -> EeWonGuiKind {
        use EeWonCfgFun as F;
        use EeWonGuiKind as K;
        match self {
            F::Btn => K::Button,
            F::MsBtn => K::MsButton,
            F::Shell => K::Shell,
            F::Multi => K::Multi,
            F::MHead => K::MHead,
            F::Bar => K::Bar,
            F::Tab => K::Tab,
            F::Combo => K::Combo,
            F::Dlg => K::Dialog,
            F::Label => K::Label,
            F::List => K::List,
            F::Input => K::Input,
            F::Text => K::Text,
            F::Tree => K::Tree,
            F::WonBtn | F::WonBtnState => K::WonButton,
            F::Split => K::Splitter,
            F::Pane => K::Pane,
            F::Scroller => K::Scroller,
        }
    }
}

/// How the table golden shows a method argument (the stand-in world's `P` of the expression's type).
#[repr(u8)]
#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum EeWonCfgDisp {
    /// A number (an int, a bool as 0 or 1, an enum, a narrowed or widened int).
    Num = 0,
    /// A component (`Component*`).
    Comp,
    /// A string (`const GUIString*`).
    Str,
    /// An image (`Image*`).
    Image,
    /// A font (`Font*`).
    Font,
}

/// One forward call: `fun(object, op, args...)`.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeWonCfgSetCall {
    pub fun: EeWonCfgFun,
    pub op: u32,
    pub args: [isize; 5],
}

/// A setter as the original statement called it and as its forward's calls.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeWonCfgSetTrace {
    /// The method's name (`len` bytes).
    pub name: *const u8,
    pub name_len: usize,
    pub nargs: i32,
    pub disp: [EeWonCfgDisp; 6],
    pub args: [isize; 6],
    pub ncalls: i32,
    pub calls: [EeWonCfgSetCall; 4],
}

/// A driver's trace hook: the setter `setter` on `component`, described (nothing is called).
pub type EeWonCfgSetTraceFn =
    extern "C" fn(ctx: *mut c_void, component: *mut c_void, setter: i32, t: *const EeWonCfgSetTrace);

static TRACE: AtomicUsize = AtomicUsize::new(0);
static TRACE_CTX: AtomicPtr<c_void> = AtomicPtr::new(std::ptr::null_mut());

/// Installs a trace hook (NULL removes it), for the drivers.
#[no_mangle]
pub extern "C" fn ee_won_cfg_set_trace(
    f: Option<extern "C" fn(ctx: *mut c_void, component: *mut c_void, setter: i32, t: *const EeWonCfgSetTrace)>,
    ctx: *mut c_void,
) {
    TRACE_CTX.store(ctx, Ordering::SeqCst);
    TRACE.store(f.map_or(0, |f| f as usize), Ordering::SeqCst);
}

struct Desc {
    name: &'static str,
    args: Vec<(EeWonCfgDisp, isize)>,
    calls: Vec<EeWonCfgSetCall>,
}

impl Desc {
    fn new(name: &'static str) -> Self {
        Desc { name, args: Vec::new(), calls: Vec::new() }
    }
    fn n(mut self, v: isize) -> Self {
        self.args.push((EeWonCfgDisp::Num, v));
        self
    }
    fn d(mut self, d: EeWonCfgDisp, v: isize) -> Self {
        self.args.push((d, v));
        self
    }
    fn call(mut self, fun: EeWonCfgFun, op: u32, args: &[isize]) -> Self {
        let mut a = [0isize; 5];
        a[..args.len()].copy_from_slice(args);
        self.calls.push(EeWonCfgSetCall { fun, op, args: a });
        self
    }
}

/// The frame's values, as the setters' expressions read them.
struct Frame<'a>(&'a EeWonCfgTabHost);

impl Frame<'_> {
    fn arg(&self, what: EeWonCfgArg, slot: i32) -> isize {
        (self.0.arg)(self.0.ctx, what, slot)
    }
    /// `i[n]` (an int)
    fn i(&self, n: i32) -> i32 {
        self.arg(EeWonCfgArg::Int, n) as i32
    }
    /// `i[n]!=0`
    fn b(&self, n: i32) -> isize {
        isize::from(self.i(n) != 0)
    }
    fn s(&self, n: i32) -> isize {
        self.arg(EeWonCfgArg::StrPtr, n)
    }
    fn c(&self, n: i32) -> isize {
        self.arg(EeWonCfgArg::Comp, n)
    }
    fn m(&self, n: i32) -> isize {
        self.arg(EeWonCfgArg::Image, n)
    }
    fn f(&self, n: i32) -> isize {
        self.arg(EeWonCfgArg::Font, n)
    }
    fn ni(&self) -> i32 {
        self.arg(EeWonCfgArg::IntCount, 0) as i32
    }
    fn nf(&self) -> i32 {
        self.arg(EeWonCfgArg::FontCount, 0) as i32
    }
}

use EeWonCfgDisp as D;
use EeWonCfgFun as Fu;

/// `(unsigned long)v` as an `intptr_t`: zero-extended where long is 32 bits (Windows), sign-extended where it is 64
fn ulong(v: i32) -> isize {
    v as std::ffi::c_ulong as isize
}

fn shell(name: &'static str, op: EeWonShellOp, a: isize, b: isize, shown: &[(EeWonCfgDisp, isize)]) -> Desc {
    let mut d = Desc::new(name).call(Fu::Shell, op as u32, &[a, b]);
    d.args.extend_from_slice(shown);
    d
}

/// The setter `setter`'s description over the frame, or None when the setter is the host's.
fn describe(setter: i32, f: &Frame<'_>, cols: &mut [isize; 2]) -> Option<Desc> {
    let (t, n) = (setter >> 8, setter & 0xff);
    let i = |k: i32| f.i(k) as isize;
    let b = |k: i32| f.b(k);
    let d = match (t, n) {
        // Button
        (EE_WON_CFG_KEYS_BUTTONCONFIG_VIRTUAL24_ININSTRUCTION, 0) => Desc::new("SetButtonFlags").n(i(1)).n(b(0)).call(
            Fu::Btn,
            EeWonBtnCall::SetButtonFlags as u32,
            &[ulong(f.i(1)), b(0)],
        ),
        (EE_WON_CFG_KEYS_BUTTONCONFIG_VIRTUAL24_ININSTRUCTION, 2) => {
            Desc::new("SetCheck").n(b(0)).n(0).call(Fu::Btn, EeWonBtnCall::SetCheck as u32, &[b(0), 0])
        }
        // MSButtonBase
        (EE_WON_CFG_KEYS_MSBUTTONCONFIG_VIRTUAL24_KEY, 1) => Desc::new("SetTextColor").n(i(0)).call(
            Fu::MsBtn,
            EeWonMsBtnCall::Set as u32,
            &[EeWonMsBtnField::TextColor as isize, i(0)],
        ),
        (EE_WON_CFG_KEYS_MSBUTTONCONFIG_VIRTUAL24_KEY, 2) => Desc::new("SetVertAlign").n(i(0)).call(
            Fu::MsBtn,
            EeWonMsBtnCall::Set as u32,
            &[EeWonMsBtnField::VAlign as isize, i(0)],
        ),
        (EE_WON_CFG_KEYS_MSBUTTONCONFIG_VIRTUAL24_KEY, 3) => {
            Desc::new("SetHorzAlign").n(i(0)).call(Fu::MsBtn, EeWonMsBtnCall::SetHorzAlign as u32, &[i(0)])
        }
        (EE_WON_CFG_KEYS_MSBUTTONCONFIG_VIRTUAL24_KEY, 4) => {
            Desc::new("SetTransparent").n(b(0)).call(Fu::MsBtn, EeWonMsBtnCall::SetTransparent as u32, &[b(0)])
        }
        (EE_WON_CFG_KEYS_MSBUTTONCONFIG_VIRTUAL24_KEY, 5) => Desc::new("SetAdjustHeightToFitText").n(b(0)).call(
            Fu::MsBtn,
            EeWonMsBtnCall::SetAdjustHeightToFitText as u32,
            &[b(0)],
        ),
        (EE_WON_CFG_KEYS_MSBUTTONCONFIG_VIRTUAL24_KEY, 7) => Desc::new("IncrementHorzTextPad").n(i(0)).n(i(1)).call(
            Fu::MsBtn,
            EeWonMsBtnCall::IncrementHorzTextPad as u32,
            &[i(0), i(1)],
        ),
        (EE_WON_CFG_KEYS_MSBUTTONCONFIG_VIRTUAL24_KEY, 8) => Desc::new("IncrementVertTextPad").n(i(0)).n(i(1)).call(
            Fu::MsBtn,
            EeWonMsBtnCall::IncrementVertTextPad as u32,
            &[i(0), i(1)],
        ),
        // the shell kinds' own methods (WONShell::Op)
        (EE_WON_CFG_KEYS_MSARROWBUTTONCONFIG_VIRTUAL24_KEY, 0) => {
            shell("SetDirection", EeWonShellOp::SetDirection, i(0), 0, &[])
        }
        (EE_WON_CFG_KEYS_MSTABBUTTONCONFIG_VIRTUAL24_KEY, 0) => {
            shell("SetLowerAmount", EeWonShellOp::SetLowerAmount, i(0), 0, &[])
        }
        (EE_WON_CFG_KEYS_MS3DFRAMECONFIG_VIRTUAL24_KEY, 0) => {
            shell("SetFrameType", EeWonShellOp::SetFrameType, i(0), 0, &[])
        }
        (EE_WON_CFG_KEYS_MS3DFRAMECONFIG_VIRTUAL24_KEY, 1) => shell("SetIsUp", EeWonShellOp::SetIsUp, b(0), 0, &[]),
        (EE_WON_CFG_KEYS_MSCOMBOBOXCONFIG_VIRTUAL24_KEY, 0) => {
            shell("SetDrawFrame", EeWonShellOp::SetDrawFrame, b(0), 0, &[])
        }
        (EE_WON_CFG_KEYS_MSSCROLLBARCONFIG_VIRTUAL24_KEY, 0)
        | (EE_WON_CFG_KEYS_MSTABCTRLCONFIG_VIRTUAL24_KEY, 0)
        | (EE_WON_CFG_KEYS_MSSCROLLCONTAINERCONFIG_VIRTUAL24_KEY, 2)
        | (EE_WON_CFG_KEYS_WONTABCTRLCONFIG_VIRTUAL24_KEY, 0) => {
            shell("SetTransparent", EeWonShellOp::SetTransparent, b(0), 0, &[])
        }
        (EE_WON_CFG_KEYS_MSETCHEDFRAMEGROUPCONFIG_VIRTUAL24_KEY, 0) => {
            shell("SetLabelText", EeWonShellOp::SetLabelText, f.s(0), 0, &[])
        }
        (EE_WON_CFG_KEYS_MSETCHEDFRAMEGROUPCONFIG_VIRTUAL24_KEY, 1) => {
            shell("AddContainedComponent", EeWonShellOp::AddContainedComponent, f.c(0), 0, &[])
        }
        (EE_WON_CFG_KEYS_MSTABCTRLCONFIG_VIRTUAL24_KEY, 1) | (EE_WON_CFG_KEYS_WONTABCTRLCONFIG_VIRTUAL24_KEY, 1) => {
            shell("AddStandardTab", EeWonShellOp::AddStandardTab, f.s(0), f.c(0), &[])
        }
        (EE_WON_CFG_KEYS_MSSCROLLCONTAINERCONFIG_VIRTUAL24_KEY, 0) => {
            shell("SetDrawFrame", EeWonShellOp::SetDrawFrame, b(0), 0, &[])
        }
        (EE_WON_CFG_KEYS_MSSCROLLCONTAINERCONFIG_VIRTUAL24_KEY, 1) => {
            shell("SetDraw3DFrame", EeWonShellOp::SetDraw3DFrame, b(0), 0, &[])
        }
        (EE_WON_CFG_KEYS_MSHEADERCONTROLCONFIG_VIRTUAL24_KEY, 0) => {
            shell("SetColumn", EeWonShellOp::SetColumn, i(0), f.s(0), &[])
        }
        (EE_WON_CFG_KEYS_MSLISTCTRLCONFIG_VIRTUAL24_KEY, 0) => {
            shell("SetHeaderHeight", EeWonShellOp::SetHeaderHeight, i(0), 0, &[])
        }
        (EE_WON_CFG_KEYS_WONSIMPLEBUTTONCONFIG_VIRTUAL24_THEINSTRUCTION, 0) => {
            shell("SetScaleImage", EeWonShellOp::SetScaleImage, b(0), 0, &[])
        }
        (EE_WON_CFG_KEYS_WONSIMPLEBUTTONCONFIG_VIRTUAL24_THEINSTRUCTION, 2) => {
            shell("SetImage", EeWonShellOp::SetImage, i(0), f.m(0), &[])
        }
        (EE_WON_CFG_KEYS_WONSIMPLEBUTTONCONFIG_VIRTUAL24_THEINSTRUCTION, 3) => {
            shell("SetCheckMouseButtonDown", EeWonShellOp::SetCheckMouseButtonDown, b(0), 0, &[])
        }
        (EE_WON_CFG_KEYS_WONHEADERCONTROLCONFIG_VIRTUAL24_KEY, 0) => {
            *cols = [f.s(0), 0];
            shell("SetColumn", EeWonShellOp::WonSetColumn, i(0), cols.as_ptr() as isize, &[])
        }
        (EE_WON_CFG_KEYS_WONLISTCTRLCONFIG_VIRTUAL24_KEY, 0) => {
            shell("mHeaderHeight", EeWonShellOp::SetHeaderHeightField, i(0), 0, &[])
        }
        (EE_WON_CFG_KEYS_WONLISTCTRLCONFIG_VIRTUAL24_KEY, 4) => {
            *cols = [f.s(0), i(1)];
            shell("SetStandarColumnHeader", EeWonShellOp::SetStandardColumnHeader, i(0), cols.as_ptr() as isize, &[])
        }
        (EE_WON_CFG_KEYS_WONLISTCTRLCONFIG_VIRTUAL24_KEY, 5) => {
            *cols = [f.c(0), i(1)];
            shell("SetColumnHeader", EeWonShellOp::SetColumnHeader, i(0), cols.as_ptr() as isize, &[])
        }
        (EE_WON_CFG_KEYS_WONFRAMECOMPONENTCONFIG_VIRTUAL24_KEY, k @ 0..=3) => {
            shell("SetFrameBorder", EeWonShellOp::SetFrameBorder, k as isize, i(k), &[])
        }
        (EE_WON_CFG_KEYS_WONFRAMECOMPONENTCONFIG_VIRTUAL24_KEY, 4) => {
            shell("SetFrameImage", EeWonShellOp::SetFrameImage, f.m(0), 0, &[])
        }
        (EE_WON_CFG_KEYS_WONCOMBOBOXCONFIG_VIRTUAL24_KEY, 0) => {
            shell("mDrawFrame", EeWonShellOp::SetDrawFrameField, b(0), 0, &[])
        }
        (EE_WON_CFG_KEYS_WONCOMBOBOXCONFIG_VIRTUAL24_KEY, 1) => {
            shell("mButtonInFrame", EeWonShellOp::SetButtonInFrame, b(0), 0, &[])
        }
        (EE_WON_CFG_KEYS_WONSCROLLCONTAINERCONFIG_VIRTUAL24_KEY, 1) => {
            shell("mTransparent", EeWonShellOp::SetTransparentField, b(0), 0, &[])
        }
        (EE_WON_CFG_KEYS_WONSCROLLCONTAINERCONFIG_VIRTUAL24_KEY, 3) => {
            shell("SetCorner", EeWonShellOp::SetCorner, f.m(0), 0, &[])
        }
        // MultiListArea and MultiListHeader
        (EE_WON_CFG_KEYS_MULTILISTAREACONFIG_VIRTUAL24_INSTRUCTION, 0) => Desc::new("SetSortColumn")
            .n(i(0))
            .n(i(1))
            .call(Fu::Multi, EeWonMultiCall::SetSortColumn as u32, &[i(0), i(1)]),
        (EE_WON_CFG_KEYS_MULTILISTAREACONFIG_VIRTUAL24_INSTRUCTION, 1) => Desc::new("SetColumnWidth")
            .n(i(0))
            .n(i(1))
            .n(0)
            .call(Fu::Multi, EeWonMultiCall::SetColumnWidth as u32, &[i(0), i(1), 0]),
        (EE_WON_CFG_KEYS_MULTILISTAREACONFIG_VIRTUAL24_INSTRUCTION, 2) => {
            Desc::new("SetNumColumns").n(i(0)).call(Fu::Multi, EeWonMultiCall::SetNumColumns as u32, &[i(0)])
        }
        (EE_WON_CFG_KEYS_MULTILISTAREACONFIG_VIRTUAL24_INSTRUCTION, 3) => Desc::new("SetDynamicSizeColumn")
            .n(i(0))
            .call(Fu::Multi, EeWonMultiCall::SetDynamicSizeColumn as u32, &[i(0)]),
        (EE_WON_CFG_KEYS_MULTILISTAREACONFIG_VIRTUAL24_INSTRUCTION, 5) => {
            Desc::new("SetAutoGrowColumns").n(b(0)).call(Fu::Multi, EeWonMultiCall::SetAutoGrowColumns as u32, &[b(0)])
        }
        (EE_WON_CFG_KEYS_MULTILISTAREACONFIG_VIRTUAL24_INSTRUCTION, 6) => Desc::new("SetColumnHAlign")
            .n(i(0))
            .n(i(1))
            .call(Fu::Multi, EeWonMultiCall::SetColumnHAlign as u32, &[i(0), i(1)]),
        (EE_WON_CFG_KEYS_MULTILISTHEADERCONFIG_VIRTUAL24_INSTRUCTION, 0) => Desc::new("SetColumnItem")
            .n(i(0))
            .d(D::Comp, f.c(0))
            .call(Fu::MHead, EeWonMHeadCall::SetColumnItem as u32, &[i(0), f.c(0)]),
        (EE_WON_CFG_KEYS_MULTILISTHEADERCONFIG_VIRTUAL24_INSTRUCTION, 1) => Desc::new("SetMultiListArea")
            .d(D::Comp, f.c(0))
            .call(Fu::MHead, EeWonMHeadCall::SetMultiListArea as u32, &[f.c(0)]),
        // Scrollbar
        (EE_WON_CFG_KEYS_SCROLLBARCONFIG_VIRTUAL24_INSTRUCTION, 0) => {
            Desc::new("SetVertical").n(b(0)).call(Fu::Bar, EeWonBarCall::SetVertical as u32, &[b(0)])
        }
        (EE_WON_CFG_KEYS_SCROLLBARCONFIG_VIRTUAL24_INSTRUCTION, 1) => {
            Desc::new("SetResizeThumb").n(b(0)).call(Fu::Bar, EeWonBarCall::SetResizeThumb as u32, &[b(0)])
        }
        (EE_WON_CFG_KEYS_SCROLLBARCONFIG_VIRTUAL24_INSTRUCTION, 2) => {
            let v = (f.i(0) as u8) as isize;
            Desc::new("SetMinThumbSize").n(v).call(Fu::Bar, EeWonBarCall::SetMinThumbSize as u32, &[v])
        }
        // TabCtrl
        (EE_WON_CFG_KEYS_TABCTRLCONFIG_VIRTUAL24_KEY, 0) => Desc::new("AddTab")
            .d(D::Comp, f.c(0))
            .d(D::Comp, f.c(1))
            .call(Fu::Tab, EeWonTabCall::AddTab as u32, &[f.c(0), f.c(1)]),
        (EE_WON_CFG_KEYS_TABCTRLCONFIG_VIRTUAL24_KEY, 1) => {
            Desc::new("SetTabBarOnRight").n(b(0)).call(Fu::Tab, EeWonTabCall::SetTabBarOnRight as u32, &[b(0)])
        }
        (EE_WON_CFG_KEYS_TABCTRLCONFIG_VIRTUAL24_KEY, 2) => {
            Desc::new("SetArrowsOnLeft").n(b(0)).call(Fu::Tab, EeWonTabCall::SetArrowsOnLeft as u32, &[b(0)])
        }
        (EE_WON_CFG_KEYS_TABCTRLCONFIG_VIRTUAL24_KEY, k @ 3..=7) => {
            let (name, field) = match k {
                3 => ("SetTabBarHeight", EeWonTabField::TabBarHeight),
                4 => ("SetTabOverlap", EeWonTabField::TabOverlap),
                5 => ("SetTabBarOverlap", EeWonTabField::TabBarOverlap),
                6 => ("SetBottomTabTopOffset", EeWonTabField::BottomTabTopOffset),
                _ => ("SetBottomTabLeftOffset", EeWonTabField::BottomTabLeftOffset),
            };
            Desc::new(name).n(i(0)).call(Fu::Tab, EeWonTabCall::SetField as u32, &[field as isize, i(0)])
        }
        // ComboBox
        (EE_WON_CFG_KEYS_COMBOBOXCONFIG_VIRTUAL24_KEY, 0) => {
            Desc::new("SetListBoxWidth").n(i(0)).call(Fu::Combo, EeWonComboCall::SetListBoxWidth as u32, &[i(0)])
        }
        (EE_WON_CFG_KEYS_COMBOBOXCONFIG_VIRTUAL24_KEY, 1) => {
            Desc::new("SetListBoxHeight").n(i(0)).call(Fu::Combo, EeWonComboCall::SetListBoxHeight as u32, &[i(0)])
        }
        (EE_WON_CFG_KEYS_COMBOBOXCONFIG_VIRTUAL24_KEY, 2) => Desc::new("SetListBoxDimensions").n(i(0)).n(i(1)).call(
            Fu::Combo,
            EeWonComboCall::SetListBoxDimensions as u32,
            &[i(0), i(1)],
        ),
        (EE_WON_CFG_KEYS_COMBOBOXCONFIG_VIRTUAL24_KEY, 3) => {
            Desc::new("SetText").d(D::Str, f.s(0)).n(1).call(Fu::Combo, EeWonComboCall::SetText as u32, &[f.s(0), 1])
        }
        (EE_WON_CFG_KEYS_COMBOBOXCONFIG_VIRTUAL24_KEY, 4) => Desc::new("AddString").d(D::Str, f.s(0)).n(-1).call(
            Fu::Combo,
            EeWonComboCall::AddString as u32,
            &[f.s(0), -1],
        ),
        (EE_WON_CFG_KEYS_COMBOBOXCONFIG_VIRTUAL24_KEY, 5) => {
            Desc::new("SetSelItem").n(i(0)).n(1).call(Fu::Combo, EeWonComboCall::SetSelItem as u32, &[i(0), 1])
        }
        (EE_WON_CFG_KEYS_COMBOBOXCONFIG_VIRTUAL24_KEY, 6) => {
            Desc::new("SetNoType").n(b(0)).call(Fu::Combo, EeWonComboCall::SetNoType as u32, &[b(0)])
        }
        (EE_WON_CFG_KEYS_COMBOBOXCONFIG_VIRTUAL24_KEY, 8) => Desc::new("SetShowButtonWhenDisabled").n(b(0)).call(
            Fu::Combo,
            EeWonComboCall::SetField as u32,
            &[EeWonComboField::ShowButtonWhenDisabled as isize, b(0)],
        ),
        // Dialog
        (EE_WON_CFG_KEYS_DIALOGCONFIG_VIRTUAL24_INSTRUCTION, 0) => Desc::new("SetAllowDrag").n(b(0)).call(
            Fu::Dlg,
            EeWonDlgCall::SetField as u32,
            &[EeWonDlgField::AllowDrag as isize, b(0)],
        ),
        (EE_WON_CFG_KEYS_DIALOGCONFIG_VIRTUAL24_INSTRUCTION, 1) => Desc::new("SetKeepInRoot").n(b(0)).call(
            Fu::Dlg,
            EeWonDlgCall::SetField as u32,
            &[EeWonDlgField::KeepInRoot as isize, b(0)],
        ),
        (EE_WON_CFG_KEYS_DIALOGCONFIG_VIRTUAL24_INSTRUCTION, 2) => {
            Desc::new("SetEndOnEscape").n(i(0)).call(Fu::Dlg, EeWonDlgCall::SetEndOnEscape as u32, &[i(0)])
        }
        (EE_WON_CFG_KEYS_DIALOGCONFIG_VIRTUAL24_INSTRUCTION, 3) => {
            Desc::new("SetDontEndOnEscape").call(Fu::Dlg, EeWonDlgCall::SetDontEndOnEscape as u32, &[])
        }
        // Label
        (EE_WON_CFG_KEYS_LABELCONFIG_VIRTUAL24_INSTRUCTION, 2) => {
            Desc::new("SetTextColor").n(i(0)).call(Fu::Label, EeWonLabelCall::SetTextColor as u32, &[i(0)])
        }
        (EE_WON_CFG_KEYS_LABELCONFIG_VIRTUAL24_INSTRUCTION, 4) => {
            Desc::new("SetVertAlign").n(i(0)).call(Fu::Label, EeWonLabelCall::SetVertAlign as u32, &[i(0)])
        }
        (EE_WON_CFG_KEYS_LABELCONFIG_VIRTUAL24_INSTRUCTION, 5) => {
            Desc::new("SetHorzAlign").n(i(0)).call(Fu::Label, EeWonLabelCall::SetHorzAlign as u32, &[i(0)])
        }
        (EE_WON_CFG_KEYS_LABELCONFIG_VIRTUAL24_INSTRUCTION, 6) => {
            Desc::new("SetWrap").n(b(0)).call(Fu::Label, EeWonLabelCall::SetWrap as u32, &[b(0)])
        }
        (EE_WON_CFG_KEYS_LABELCONFIG_VIRTUAL24_INSTRUCTION, 7) => Desc::new("SetAdjustHeightToFitText").n(b(0)).call(
            Fu::Label,
            EeWonLabelCall::SetAdjustHeightToFitText as u32,
            &[b(0)],
        ),
        // ListArea
        (EE_WON_CFG_KEYS_LISTAREACONFIG_VIRTUAL24_ININSTRUCTION, 0) => Desc::new("SetListFlags")
            .n(ulong(f.i(1)))
            .n(b(0))
            .call(Fu::List, EeWonListCall::SetListFlags as u32, &[i(1), b(0)]),
        (EE_WON_CFG_KEYS_LISTAREACONFIG_VIRTUAL24_ININSTRUCTION, 5) => {
            Desc::new("SetSortType").n(i(0)).call(Fu::List, EeWonListCall::SetSortType as u32, &[i(0)])
        }
        // InputBox
        (EE_WON_CFG_KEYS_INPUTBOXCONFIG_VIRTUAL24_ININSTRUCTION, 0) => {
            Desc::new("SetText").d(D::Str, f.s(0)).n(1).call(
                Fu::Input,
                EeWonInputCall::SetText as u32,
                &[f.arg(EeWonCfgArg::StrUnits, 0), f.arg(EeWonCfgArg::StrLen, 0), 1],
            )
        }
        (EE_WON_CFG_KEYS_INPUTBOXCONFIG_VIRTUAL24_ININSTRUCTION, 1) => {
            // SetPasswordChar(char): the int narrowed to the platform's char, then widened back
            let ch = (f.i(0) as std::ffi::c_char) as isize;
            Desc::new("SetPasswordChar").n(i(0)).call(Fu::Input, EeWonInputCall::SetPasswordChar as u32, &[ch])
        }
        (EE_WON_CFG_KEYS_INPUTBOXCONFIG_VIRTUAL24_ININSTRUCTION, 2) => {
            Desc::new("SetAllowSelection").n(b(0)).call(Fu::Input, EeWonInputCall::SetAllowSelection as u32, &[b(0)])
        }
        (EE_WON_CFG_KEYS_INPUTBOXCONFIG_VIRTUAL24_ININSTRUCTION, 3) => {
            Desc::new("SetMaxChars").n(i(0)).call(Fu::Input, EeWonInputCall::SetMaxChars as u32, &[i(0)])
        }
        (EE_WON_CFG_KEYS_INPUTBOXCONFIG_VIRTUAL24_ININSTRUCTION, 4) => {
            Desc::new("SetCharWidth").n(i(0)).call(Fu::Input, EeWonInputCall::SetCharWidth as u32, &[i(0)])
        }
        (EE_WON_CFG_KEYS_INPUTBOXCONFIG_VIRTUAL24_ININSTRUCTION, 11) => {
            Desc::new("SetSelectOnFocus").n(b(0)).call(Fu::Input, EeWonInputCall::SetSelectOnFocus as u32, &[b(0)])
        }
        (EE_WON_CFG_KEYS_INPUTBOXCONFIG_VIRTUAL24_ININSTRUCTION, 12) => {
            Desc::new("SetMaxInputHistory").n(i(0)).call(Fu::Input, EeWonInputCall::SetMaxInputHistory as u32, &[i(0)])
        }
        (EE_WON_CFG_KEYS_INPUTBOXCONFIG_VIRTUAL24_ININSTRUCTION, 14) => {
            let off = if f.ni() > 1 { i(1) } else { i(0) };
            Desc::new("SetCursorBlinkRate").n(i(0)).n(off).call(
                Fu::Input,
                EeWonInputCall::SetCursorBlinkRate as u32,
                &[i(0), off],
            )
        }
        // TextArea
        (EE_WON_CFG_KEYS_TEXTAREACONFIG_VIRTUAL24_KEY, 0) => {
            Desc::new("SetTextFlags").n(i(1)).n(b(0)).call(Fu::Text, EeWonTextCall::SetTextFlags as u32, &[i(1), b(0)])
        }
        (EE_WON_CFG_KEYS_TEXTAREACONFIG_VIRTUAL24_KEY, 7) => {
            Desc::new("SetMaxChars").n(i(0)).call(Fu::Text, EeWonTextCall::SetMaxChars as u32, &[i(0)])
        }
        (EE_WON_CFG_KEYS_TEXTAREACONFIG_VIRTUAL24_KEY, 8) => {
            Desc::new("SetMaxLines").n(i(0)).call(Fu::Text, EeWonTextCall::SetMaxLines as u32, &[i(0)])
        }
        (EE_WON_CFG_KEYS_TEXTAREACONFIG_VIRTUAL24_KEY, 9) => {
            Desc::new("SetWrapIndentSize").n(i(0)).call(Fu::Text, EeWonTextCall::SetWrapIndentSize as u32, &[i(0)])
        }
        (EE_WON_CFG_KEYS_TEXTAREACONFIG_VIRTUAL24_KEY, 10) => {
            Desc::new("SetLineAlignment").n(i(0)).call(Fu::Text, EeWonTextCall::SetLineAlignment as u32, &[i(0)])
        }
        // TreeArea
        (EE_WON_CFG_KEYS_TREEAREACONFIG_VIRTUAL24_INSTRUCTION, 0) => {
            Desc::new("SetSortType").n(i(0)).call(Fu::Tree, EeWonTreeCall::SetSortType as u32, &[i(0)])
        }
        // WONButton
        (EE_WON_CFG_KEYS_WONBUTTONCONFIG_VIRTUAL24_THEINSTRUCTION, n) => return wonbutton(n, f),
        // Splitter
        (EE_WON_CFG_KEYS_SPLITTERCONFIG_VIRTUAL24_THEINSTRUCTION, n) => return splitter(n, f),
        // ScrollArea, Scroller, ScrollbarScroller
        (EE_WON_CFG_KEYS_SCROLLAREACONFIG_VIRTUAL24_INSTRUCTION, 1) => {
            // SetBorderPadding's four ints: the forward passes them as a local array, made when the call is
            let p = [i(0), i(1), i(2), i(3)];
            Desc::new("SetBorderPadding").n(p[0]).n(p[1]).n(p[2]).n(p[3]).call(
                Fu::Pane,
                EeWonPaneCall::SetBorderPadding as u32,
                &p,
            )
        }
        (EE_WON_CFG_KEYS_SCROLLERCONFIG_VIRTUAL24_INSTRUCTION, 0) => Desc::new("SetScrollArea")
            .d(D::Comp, f.c(0))
            .call(Fu::Scroller, EeWonScrollerCall::SetScrollArea as u32, &[f.c(0)]),
        (EE_WON_CFG_KEYS_SCROLLBARSCROLLERCONFIG_VIRTUAL24_INSTRUCTION, 0) => Desc::new("SetScrollbarConditions")
            .n(i(0))
            .n(i(1))
            .call(Fu::Scroller, EeWonScrollerCall::SetScrollbarConditions as u32, &[i(0), i(1)]),
        _ => return None,
    };
    Some(d)
}

fn wonbutton(n: i32, f: &Frame<'_>) -> Option<Desc> {
    let i = |k: i32| f.i(k) as isize;
    let b = |k: i32| f.b(k);
    let set = |d: Desc, fld: EeWonWbField, v: isize| d.call(Fu::WonBtn, EeWonWbCall::Set as u32, &[fld as isize, v]);
    let call = |d: Desc, c: EeWonWbCall, a: isize| d.call(Fu::WonBtn, c as u32, &[a]);
    use EeWonWbField as W;
    Some(match n {
        0 => call(Desc::new("SetFont").d(D::Font, f.f(0)), EeWonWbCall::SetFont, f.f(0)),
        2 => call(Desc::new("SetTextColor").n(i(0)), EeWonWbCall::SetTextColor, i(0)),
        3 => set(Desc::new("SetDefaultTextColor").n(i(0)), W::DefaultTextColor, i(0)),
        // SetVertAlign stores the low byte (its forward's (unsigned char))
        4 => set(Desc::new("SetVertAlign").n(i(0)), W::VAlign, (f.i(0) as u8) as isize),
        5 => call(Desc::new("SetHorzAlign").n(i(0)), EeWonWbCall::SetHorzAlign, i(0)),
        6 => call(Desc::new("SetTransparent").n(b(0)), EeWonWbCall::SetTransparent, b(0)),
        7 => call(Desc::new("SetAdjustHeightToFitText").n(b(0)), EeWonWbCall::SetAdjustHeightToFitText, b(0)),
        8 => call(Desc::new("SetWrapText").n(b(0)), EeWonWbCall::SetWrapText, b(0)),
        9 => set(Desc::new("SetDrawGrayText").n(b(0)), W::DrawGrayText, b(0)),
        10 | 11 => {
            let (name, l, t, r, bt) = if n == 10 {
                ("SetTextPadding", W::LeftTextPad, W::TopTextPad, W::RightTextPad, W::BottomTextPad)
            } else {
                ("SetFocusPadding", W::LeftFocusPad, W::TopFocusPad, W::RightFocusPad, W::BottomFocusPad)
            };
            let d = Desc::new(name).n(i(0)).n(i(1)).n(i(2)).n(i(3));
            set(set(set(set(d, l, i(0)), t, i(1)), r, i(2)), bt, i(3))
        }
        15 => {
            let color = if f.ni() > 1 { i(1) } else { -1 };
            let font = if f.nf() > 0 { f.f(0) } else { 0 };
            let state = i(0) & 0xff;
            let checked = isize::from((f.i(0) & 0x100) != 0);
            Desc::new("SetButtonState").n(state).d(D::Image, f.m(0)).n(color).d(D::Font, font).n(checked).call(
                Fu::WonBtnState,
                0,
                &[state, f.m(0), color, font, checked],
            )
        }
        16 => set(Desc::new("SetScaleImage").n(b(0)), W::ScaleImage, b(0)),
        17 => set(Desc::new("SetFocusStyle").n(b(0)), W::FocusStyle, b(0)),
        _ => return None,
    })
}

fn splitter(n: i32, f: &Frame<'_>) -> Option<Desc> {
    let i = |k: i32| f.i(k) as isize;
    let call = |d: Desc, c: EeWonSplitCall, a: &[isize]| d.call(Fu::Split, c as u32, a);
    use EeWonSplitCall as S;
    Some(match n {
        0 => call(Desc::new("AddQuadrant").d(D::Comp, f.c(0)).n(i(0)), S::AddQuadrant, &[f.c(0), i(0)]),
        1 => call(Desc::new("AddSplitter").d(D::Comp, f.c(0)).n(i(0)), S::AddSplitter, &[f.c(0), i(0)]),
        2 => {
            let size = if f.ni() > 1 { i(1) } else { 10 };
            call(Desc::new("AddInvisibleSplitter").n(i(0)).n(size), S::AddInvisibleSplitter, &[i(0), size])
        }
        3 => {
            let nc = f.arg(EeWonCfgArg::CompCount, 0) as i32;
            let drag = if nc > 1 { f.c(1) } else { 0 };
            call(
                Desc::new("AddComplexSplitter").d(D::Comp, f.c(0)).n(i(0)).d(D::Comp, drag),
                S::AddComplexSplitter,
                &[f.c(0), i(0), drag],
            )
        }
        4 => call(Desc::new("SetMinLeftRight").n(i(0)).n(i(1)), S::SetMinLeftRight, &[i(0), i(1)]),
        5 => call(Desc::new("SetMinTopBottom").n(i(0)).n(i(1)), S::SetMinTopBottom, &[i(0), i(1)]),
        6 => call(Desc::new("SetPercentages").n(i(0)).n(i(1)), S::SetPercentages, &[i(0), i(1)]),
        7 => call(Desc::new("MaxQuadrant").n(i(0)), S::MaxQuadrant, &[i(0)]),
        8 => call(Desc::new("UnMaxQuadrant"), S::UnMaxQuadrant, &[]),
        9 => call(Desc::new("SetSplitterPos").n(i(0)).n(i(1)), S::SetSplitterPos, &[i(0), i(1), 1]),
        10 => call(Desc::new("RegisterMaxButton").d(D::Comp, f.c(0)).n(i(0)), S::RegisterMaxButton, &[f.c(0), i(0)]),
        _ => return None,
    })
}

/// Makes one forward call on `component`'s object (none when the registry holds no object of its kind).
fn exec(component: isize, c: &EeWonCfgSetCall) {
    let obj = registry::find(component, c.fun.kind());
    if obj.is_null() {
        return;
    }
    let a = c.args;
    // SAFETY: the object is the live one of the component's kind (the registry's); the op is the forward's, a
    // valid value of the call's enum (made from one in `describe`)
    unsafe {
        use std::mem::transmute as tr;
        match c.fun {
            Fu::Btn => {
                crate::btnobj::ee_won_btn_call(obj.cast(), tr::<u32, EeWonBtnCall>(c.op), a[0], a[1], a[2]);
            }
            Fu::MsBtn => {
                crate::msbtnobj::ee_won_msbtn_call(obj.cast(), tr::<u32, EeWonMsBtnCall>(c.op), a[0], a[1], a[2]);
            }
            Fu::Shell => {
                crate::shell::ee_won_shell_op(obj.cast(), tr::<u32, EeWonShellOp>(c.op), a[0], a[1]);
            }
            Fu::Multi => {
                crate::multiobj::ee_won_multi_call(obj.cast(), tr::<u32, EeWonMultiCall>(c.op), a[0], a[1], a[2]);
            }
            Fu::MHead => {
                crate::multiobj::ee_won_mhead_call(obj.cast(), tr::<u32, EeWonMHeadCall>(c.op), a[0], a[1], a[2]);
            }
            Fu::Bar => {
                crate::barobj::ee_won_bar_call(obj.cast(), tr::<u32, EeWonBarCall>(c.op), a[0], a[1], a[2]);
            }
            Fu::Tab => {
                crate::tabobj::ee_won_tab_call(obj.cast(), tr::<u32, EeWonTabCall>(c.op), a[0], a[1]);
            }
            Fu::Combo => {
                crate::comboobj::ee_won_combo_call(obj.cast(), tr::<u32, EeWonComboCall>(c.op), a[0], a[1], a[2]);
            }
            Fu::Dlg => {
                crate::dialogobj::ee_won_dlg_call(obj.cast(), tr::<u32, EeWonDlgCall>(c.op), a[0], a[1], a[2]);
            }
            Fu::Label => {
                crate::labelobj::ee_won_label_call(obj.cast(), tr::<u32, EeWonLabelCall>(c.op), a[0], a[1], 0);
            }
            Fu::List => {
                crate::listobj::ee_won_list_call(obj.cast(), tr::<u32, EeWonListCall>(c.op), a[0], a[1], a[2]);
            }
            Fu::Input => {
                crate::inputobj::ee_won_input_call(obj.cast(), tr::<u32, EeWonInputCall>(c.op), a[0], a[1], a[2]);
            }
            Fu::Text => {
                crate::textobj::ee_won_text_call(obj.cast(), tr::<u32, EeWonTextCall>(c.op), a[0], a[1], a[2]);
            }
            Fu::Tree => {
                crate::treeobj::ee_won_tree_call(obj.cast(), tr::<u32, EeWonTreeCall>(c.op), a[0], a[1], a[2]);
            }
            Fu::WonBtn => {
                crate::wonbtnobj::ee_won_wonbtn_call(obj.cast(), tr::<u32, EeWonWbCall>(c.op), a[0], a[1], a[2], a[3]);
            }
            Fu::WonBtnState => crate::wonbtnobj::ee_won_wonbtn_set_state(
                obj.cast(),
                a[0] as i32,
                a[1] as *mut c_void,
                a[2] as i32,
                a[3] as *mut c_void,
                a[4] != 0,
            ),
            Fu::Split => {
                crate::splitobj::ee_won_split_call(obj.cast(), tr::<u32, EeWonSplitCall>(c.op), a[0], a[1], a[2]);
            }
            Fu::Pane => {
                let p = [a[0] as i32, a[1] as i32, a[2] as i32, a[3] as i32];
                crate::paneobj::ee_won_pane_call(obj.cast(), tr::<u32, EeWonPaneCall>(c.op), p.as_ptr() as isize, 0, 0);
            }
            Fu::Scroller => {
                crate::scrollerobj::ee_won_scroller_call(
                    obj.cast(),
                    tr::<u32, EeWonScrollerCall>(c.op),
                    a[0],
                    a[1],
                    a[2],
                );
            }
        }
    }
}

/// The setter `setter` over the host's frame when it is one of these (true), else false (the host's).
pub fn apply(host: &EeWonCfgTabHost, setter: i32) -> bool {
    let f = Frame(host);
    let mut cols = [0isize; 2];
    let Some(d) = describe(setter, &f, &mut cols) else {
        return false;
    };
    let component = f.arg(EeWonCfgArg::Component, 0);
    let hook = TRACE.load(Ordering::SeqCst);
    if hook != 0 {
        let mut t = EeWonCfgSetTrace {
            name: d.name.as_ptr(),
            name_len: d.name.len(),
            nargs: d.args.len() as i32,
            disp: [EeWonCfgDisp::Num; 6],
            args: [0; 6],
            ncalls: d.calls.len() as i32,
            calls: [EeWonCfgSetCall { fun: Fu::Btn, op: 0, args: [0; 5] }; 4],
        };
        for (k, (disp, v)) in d.args.iter().enumerate() {
            t.disp[k] = *disp;
            t.args[k] = *v;
        }
        t.calls[..d.calls.len()].copy_from_slice(&d.calls);
        // SAFETY: the hook was installed as an EeWonCfgSetTraceFn
        let hook: EeWonCfgSetTraceFn = unsafe { std::mem::transmute::<usize, EeWonCfgSetTraceFn>(hook) };
        hook(TRACE_CTX.load(Ordering::SeqCst), component as *mut c_void, setter, &t);
        return true;
    }
    for c in &d.calls {
        exec(component, c);
    }
    true
}

#[cfg(test)]
#[path = "cfgset_tests.rs"]
mod tests;
