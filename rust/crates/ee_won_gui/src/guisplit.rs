//! The component hosts' runs of C++ statements composed in Rust (R6 WON round 32): an op a Rust component object
//! sends that GuiHost.cpp answered with several statements (a segment's measure, the input box's filter and paint
//! inputs, the draw operations' slots, the splitter's handles, the list's new viewports and rows) is answered here,
//! each statement one one-statement case of the same C++ host. The game hands its components these hosts
//! (`ee_won_gui_text_game` ..) in place of GuiHost.cpp's own; the hosts GuiHost.cpp registers at static
//! initialisation (`ee_won_gui_split_register`) answer every other op. The drivers' worlds hand their objects their
//! own scripted hosts, so the components' goldens see the ops at the old grain.
//!
//! Also here: `MultiListHeader::GetSortTypeForButton`, Rust over one-statement ops of the header's host.

use crate::guictl::EeWonRect4;
use crate::guidraw::{EeWonDrawOp, EeWonInputPaint, EeWonSelColor, EE_WON_DRAW_CALL};
use crate::guihost::{EeWonGuiHostFn, EeWonGuiOp, EeWonGuiOp as Op};
use crate::guistrobj::{EeWonGstr, GText};
use crate::winobj::EeWonWinCall as C;
use crate::wmcache::EeWonWmRefKind;
use crate::wmobj::{EeWonWmEvent, EeWonWmMouse, EeWonWmWin};
use std::ffi::c_void;
use std::sync::atomic::{AtomicUsize, Ordering};

/// The C++ host a game host answers the rest with (`ee_won_gui_split_register`).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonGuiSplitHost {
    /// `ee_won_guicpp_text_call` (the text area).
    Text = 0,
    /// `ee_won_guicpp_input_call` (the input box).
    Input,
    /// `ee_won_guicpp_label_call` (the label).
    Label,
    /// `ee_won_guicpp_split_call` (the splitter).
    Split,
    /// `ee_won_guicpp_list_call` (the list area, the multi-column list, the tree, the popup menu).
    List,
    /// `ee_won_guicpp_wm_call` (the window manager).
    Wm,
}

const HOST_COUNT: usize = 6;
static HOSTS: [AtomicUsize; HOST_COUNT] = [const { AtomicUsize::new(0) }; HOST_COUNT];

/// The calls out: `(op, a, b, c, d) -> result` on the component the op came for.
pub(crate) type Out<'a> = &'a dyn Fn(Op, isize, isize, isize, isize) -> isize;

/// Registers the C++ host behind a game host (GuiHost.cpp, at static initialisation).
#[no_mangle]
pub extern "C" fn ee_won_gui_split_register(which: EeWonGuiSplitHost, host: EeWonGuiHostFn) {
    let f = host.map_or(0, |f| f as usize);
    HOSTS[which as usize].store(f, Ordering::Relaxed);
}

/// # Safety
/// `comp` is the C++ component of the host `which`.
unsafe fn game(which: EeWonGuiSplitHost, comp: *mut c_void, op: Op, a: isize, b: isize, c: isize, d: isize) -> isize {
    let f = HOSTS[which as usize].load(Ordering::Relaxed);
    if f == 0 {
        return 0;
    }
    // SAFETY: a value ee_won_gui_split_register stored from an `EeWonGuiHostFn`
    let host = unsafe {
        std::mem::transmute::<usize, unsafe extern "C" fn(*mut c_void, Op, isize, isize, isize, isize) -> isize>(f)
    };
    // SAFETY: the C++ host of this component
    let out = |op: Op, a: isize, b: isize, c: isize, d: isize| {
        if which == EeWonGuiSplitHost::Wm {
            if let Some(r) = crate::gamewin::wm_ref_op(comp as isize, op, a, b, c) {
                return r;
            }
        }
        // (the component services guisplit's own statements ask for: Component's and the children's, as every
        // Rust object's own calls; the manager is no component)
        if which != EeWonGuiSplitHost::Wm {
            if let Some(r) = crate::gamewin::common_op(comp as isize, op, a, b, c) {
                return r;
            }
        }
        if matches!(which, EeWonGuiSplitHost::List | EeWonGuiSplitHost::Text) {
            if let Some(r) = crate::gamewin::list_host_op(comp as isize, op, a, b, c, d) {
                return r;
            }
        }
        // SAFETY: as above
        unsafe { host(comp, op, a, b, c, d) }
    };
    let answer = match which {
        EeWonGuiSplitHost::Text => text_op(op, a, b, c, &out),
        EeWonGuiSplitHost::Input => input_op(op, a, b, c, &out),
        EeWonGuiSplitHost::Label => label_op(op, a, b, &out),
        EeWonGuiSplitHost::Split => split_op(op, a, &out),
        EeWonGuiSplitHost::List => list_op(op, [a, b, c, d], &out),
        EeWonGuiSplitHost::Wm => wm_op(op, a, b, c, &out),
    };
    match answer {
        Some(r) => r,
        None => out(op, a, b, c, d),
    }
}

/// The text area's host in the game.
///
/// # Safety
/// As the C++ host's: `comp` a `TextArea*`, the op's arguments as `EeWonGuiOp` says.
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_text_game(
    comp: *mut c_void,
    op: EeWonGuiOp,
    a: isize,
    b: isize,
    c: isize,
    d: isize,
) -> isize {
    // SAFETY: the caller's
    unsafe { game(EeWonGuiSplitHost::Text, comp, op, a, b, c, d) }
}

/// The input box's host in the game.
///
/// # Safety
/// As the C++ host's: `comp` an `InputBox*`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_input_game(
    comp: *mut c_void,
    op: EeWonGuiOp,
    a: isize,
    b: isize,
    c: isize,
    d: isize,
) -> isize {
    // SAFETY: the caller's
    unsafe { game(EeWonGuiSplitHost::Input, comp, op, a, b, c, d) }
}

/// The label's host in the game.
///
/// # Safety
/// As the C++ host's: `comp` a `Label*`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_label_game(
    comp: *mut c_void,
    op: EeWonGuiOp,
    a: isize,
    b: isize,
    c: isize,
    d: isize,
) -> isize {
    // SAFETY: the caller's
    unsafe { game(EeWonGuiSplitHost::Label, comp, op, a, b, c, d) }
}

/// The splitter's host in the game.
///
/// # Safety
/// As the C++ host's: `comp` a `Splitter*`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_split_game(
    comp: *mut c_void,
    op: EeWonGuiOp,
    a: isize,
    b: isize,
    c: isize,
    d: isize,
) -> isize {
    // SAFETY: the caller's
    unsafe { game(EeWonGuiSplitHost::Split, comp, op, a, b, c, d) }
}

/// The list area's host in the game (the multi-column list's, the tree's and the menu's too).
///
/// # Safety
/// As the C++ host's: `comp` a `ListArea*`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_list_game(
    comp: *mut c_void,
    op: EeWonGuiOp,
    a: isize,
    b: isize,
    c: isize,
    d: isize,
) -> isize {
    // SAFETY: the caller's
    unsafe { game(EeWonGuiSplitHost::List, comp, op, a, b, c, d) }
}

/// The window manager's host in the game.
///
/// # Safety
/// As the C++ host's: `comp` a `WindowManager*`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_wm_game(
    comp: *mut c_void,
    op: EeWonGuiOp,
    a: isize,
    b: isize,
    c: isize,
    d: isize,
) -> isize {
    // SAFETY: the caller's
    unsafe { game(EeWonGuiSplitHost::Wm, comp, op, a, b, c, d) }
}

fn text_op(op: Op, a: isize, b: isize, c: isize, out: Out) -> Option<isize> {
    match op {
        Op::SegMeasure => {
            let m = [
                out(Op::SegFontStringWidth, a, 0, 0, 0),
                out(Op::SegFontHeight, a, 0, 0, 0),
                out(Op::SegFontAscent, a, 0, 0, 0),
            ];
            // SAFETY: `b` points at three i32s (SegMeasure's contract)
            let dst = unsafe { std::slice::from_raw_parts_mut(b as *mut i32, 3) };
            for (o, v) in dst.iter_mut().zip(m) {
                *o = v as i32;
            }
            Some(0)
        }
        // a segment's font and text in the slots; without a segment the plain draw (ComponentOp's)
        Op::Draw if c != 0 => Some(out(Op::TextDrawSeg, a, b, c, 0)),
        // the link's parameter kept, then its event delivered (R6 WON round 33)
        Op::LinkActivated => {
            out(Op::TextSetLinkParam, a, 0, 0, 0);
            out(Op::TextDeliverLinkEvent, a, 0, 0, 0);
            Some(0)
        }
        _ => None,
    }
}

/// `n` u32 units at `p` (none for NULL).
///
/// # Safety
/// `p` holds `n` units.
unsafe fn units(p: isize, n: isize) -> Vec<u32> {
    if p == 0 || n <= 0 {
        return Vec::new();
    }
    // SAFETY: the contract
    unsafe { std::slice::from_raw_parts(p as *const u32, n as usize) }.to_vec()
}

/// The units into a take sink (a `Vec<u32>`; untouched for NULL or no units, as ee_won_gui_sink_units).
fn sink(s: isize, v: Vec<u32>) {
    if s != 0 && !v.is_empty() {
        // SAFETY: a take sink (the ops' contract)
        unsafe { (*(s as *mut Vec<u32>)).extend(v) };
    }
}

fn input_op(op: Op, a: isize, b: isize, c: isize, out: Out) -> Option<isize> {
    match op {
        // the clipboard's text into the sink `a` (the original's local GUIString, Rust's: R6 WON GUI round 39)
        Op::ClipboardTake => {
            // SAFETY: the host's new text
            let t = unsafe { GText::adopt(out(Op::ClipboardText, 0, 0, 0, 0) as *mut EeWonGstr) };
            sink(a, t.units());
            Some(0)
        }
        // the filter over the typed text (`b` units at `a`; the original's local GUIString), 2 without one
        Op::Filter => {
            if out(Op::InputHasFilter, 0, 0, 0, 0) == 0 {
                return Some(2);
            }
            // SAFETY: `b` units (the op's contract)
            let mut t = GText::new(&unsafe { units(a, b) });
            let r = out(Op::InputFilterText, t.arg(), 0, 0, 0);
            sink(c, t.units());
            Some(if r != 0 { 1 } else { 0 })
        }
        // the clipboard's and the box's texts from units (the original's GUIStrings)
        Op::ClipboardSet => {
            // SAFETY: `b` units (the op's contract)
            let mut t = GText::new(&unsafe { units(a, b) });
            Some(out(Op::ClipboardSetText, t.arg(), 0, 0, 0))
        }
        Op::TextStore => {
            // SAFETY: `c` units (the op's contract)
            let mut t = GText::new(&unsafe { units(b, c) });
            Some(out(Op::TextStoreText, a, t.arg(), 0, 0))
        }
        Op::PaintInputs => {
            // SAFETY: `a` is the box's paint record (PaintInputs' contract)
            let p = unsafe { &mut *(a as *mut EeWonInputPaint) };
            p.disabled_look =
                out(Op::InputDisabled, 0, 0, 0, 0) != 0 && out(Op::InputUseDisabledColors, 0, 0, 0, 0) != 0;
            p.has_icon = out(Op::InputHasIcon, 0, 0, 0, 0) != 0;
            p.has_focus = out(Op::InputHasFocus, 0, 0, 0, 0) != 0;
            p.text_color = out(Op::InputTextColor, 0, 0, 0, 0) as i32;
            p.disabled_text_color = out(Op::InputDisabledTextColor, 0, 0, 0, 0) as i32;
            p.icon_height = if p.has_icon { out(Op::InputIconHeight, 0, 0, 0, 0) as i32 } else { 0 };
            p.font_height = out(Op::FontHeight, 0, 0, 0, 0) as i32;
            let mut sel = EeWonSelColor { background: 0, foreground: 0, mode: 0 };
            out(Op::InputSelColor, &mut sel as *mut EeWonSelColor as isize, 0, 0, 0);
            p.sel = sel;
            Some(0)
        }
        // the icon, the font and the text in the slots; the call paints the (disabled) background
        Op::Draw => {
            // SAFETY: `b` is the draw operation (Draw's contract)
            let o = unsafe { &*(b as *const EeWonDrawOp) };
            if o.kind == EE_WON_DRAW_CALL {
                let back = if o.b != 0 { Op::InputPaintDisabledBack } else { Op::InputPaintBack };
                out(back, a, b, 0, 0);
            } else {
                out(Op::InputDrawSlots, a, b, 0, 0);
            }
            Some(0)
        }
        _ => None,
    }
}

fn label_op(op: Op, a: isize, b: isize, out: Out) -> Option<isize> {
    match op {
        Op::Draw => Some(out(Op::LabelDrawWrapped, a, b, 0, 0)),
        _ => None,
    }
}

fn split_op(op: Op, a: isize, out: Out) -> Option<isize> {
    match op {
        // a drag handle laid over the container and added (the original's SmartPtr: held, given back last)
        Op::NewDragComponent => {
            let comp = out(Op::NewComponent, 0, 0, 0, 0);
            out(Op::ChildHold, comp, 0, 0, 0);
            out(Op::ChildLayoutDrag, a, comp, 0, 0);
            out(Op::ChildAddEx, a, comp, 0, 0);
            out(Op::ChildRelease, comp, 0, 0, 0);
            Some(comp)
        }
        // a square component the creator holds (given back with ChildRelease)
        Op::NewSizedComponent => {
            let comp = out(Op::NewComponent, 0, 0, 0, 0);
            out(Op::ChildHold, comp, 0, 0, 0);
            out(Op::ChildSetSize, comp, a, a, 0);
            Some(comp)
        }
        _ => None,
    }
}

fn list_op(op: Op, v: [isize; 4], out: Out) -> Option<isize> {
    let [a, b, c, d] = v;
    match op {
        // ---- R6 WON round 33
        // 1 when the pointer is outside the list (in its parent's coordinates), 0 without a window or a parent
        Op::PointerOutside => {
            let window = out(Op::ListRoot, 0, 0, 0, 0);
            if window == 0 {
                return Some(0);
            }
            let mut at = [out(Op::WinMouseX, window, 0, 0, 0) as i32, 0];
            at[1] = out(Op::WinMouseY, window, 0, 0, 0) as i32;
            let parent = out(Op::ListParent, 0, 0, 0, 0);
            if parent == 0 {
                return Some(0);
            }
            out(Op::ParentRootToChild, parent, at.as_mut_ptr() as isize, 0, 0);
            Some(if out(Op::ListContains, at[0] as isize, at[1] as isize, 0, 0) != 0 { 0 } else { 1 })
        }
        // the item tooltip made on first use, then shown for the item `a`
        Op::TipShow => {
            if out(Op::TipExists, 0, 0, 0, 0) == 0 {
                out(Op::TipNew, 0, 0, 0, 0);
            }
            out(Op::TipDo, a, 0, 0, 0);
            Some(0)
        }
        Op::TipHide => {
            if out(Op::TipExists, 0, 0, 0, 0) != 0 {
                out(Op::TipDo, 0, 0, 0, 0);
            }
            Some(0)
        }
        // a submenu's popup queued on the window: the menu `a` at the point `b` (two ints) avoiding the rectangle
        // `c`, with the flags `d`
        Op::QueueSubMenu => {
            // SAFETY: `b` points at two ints (QueueSubMenu's contract)
            let at = unsafe { std::slice::from_raw_parts(b as *const i32, 2) };
            out(Op::ParamsMake, a, d, at[0] as isize, at[1] as isize);
            out(Op::ParamsAvoid, c, 0, 0, 0);
            out(Op::QueueDoPopup, 0, 0, 0, 0);
            Some(0)
        }
        // a mouse up of the button `c` at the list's point (a, b) queued on its window, in the root's coordinates
        Op::QueueMouseUp => {
            let window = out(Op::ListRoot, 0, 0, 0, 0);
            let mut at = [a as i32, b as i32];
            out(Op::ListLocalToRoot, at.as_mut_ptr() as isize, 0, 0, 0);
            out(Op::WinQueueMouseUp, window, at[0] as isize, at[1] as isize, c);
            Some(0)
        }
        // the item tooltip's end, when there is one
        Op::TipEnd => {
            if out(Op::TipExists, 0, 0, 0, 0) != 0 {
                out(Op::TipEndCall, 0, 0, 0, 0);
            }
            Some(0)
        }
        // a column's viewport in the list's (the column's SmartPtr, given back with ChildRelease)
        Op::NewColumnViewport => {
            let v = out(Op::NewViewport, 0, 0, 0, 0);
            out(Op::ChildHold, v, 0, 0, 0);
            out(Op::ViewportAddChild, v, 0, 0, 0);
            Some(v)
        }
        // a tree row the creator holds (given back with ItemRelease)
        Op::NewTreeItem => {
            let n = out(Op::NewTreeItemRaw, 0, 0, 0, 0);
            out(Op::ItemHold, n, 0, 0, 0);
            Some(n)
        }
        _ => None,
    }
}

fn wm_op(op: Op, a: isize, b: isize, c: isize, out: Out) -> Option<isize> {
    // an event's record (the C++ `WindowEventBase::mRec`, R6 WON GUI rounds 37 and 38), read or edited at once
    let rec_at = |e: isize| out(Op::WmInputRec, e, 0, 0, 0) as *mut crate::wmobj::EeWonWmEventRec;
    // SAFETY: the event `e` is a live event (the case's contract), its record a plain value
    let rec = |e: isize| unsafe { (*rec_at(e)).ev };
    // SAFETY: as `rec`; `f` makes no call out
    let rec_edit = |e: isize, f: &dyn Fn(&mut crate::winobj::EeWonWinEvent)| unsafe { f(&mut (*rec_at(e)).ev) };
    // an input event's handler run on its game window's Rust window (the original's `Window::Handle*Event`)
    let handle = |window: isize, handler: C, e: isize| {
        let r = rec_at(e);
        let p = if r.is_null() { None } else { crate::gamewin::parts(window) }?;
        // SAFETY: the live record, copied; the window's live Rust window (the bind's contract)
        unsafe {
            let ev = (*r).ev;
            (*p.win).call(handler, &ev as *const crate::winobj::EeWonWinEvent as isize, 0);
        }
        Some(0)
    };
    const MOUSE: isize = EeWonWmEvent::Mouse as isize;
    const SET_MOUSE: isize = EeWonWmEvent::SetMouse as isize;
    const SIZE: isize = EeWonWmEvent::Size as isize;
    const SET_SIZE: isize = EeWonWmEvent::SetSize as isize;
    match op {
        // ---- R6 WON GUI round 38: an event's type and window are its record's; an input event's delivery and the
        // manager's handler calls run the window's handler here (any other event's `Deliver` is the C++ virtual)
        Op::WmEvent if a == EeWonWmEvent::Type as isize => Some(rec(b).kind as isize),
        // SAFETY: the live event's record
        Op::WmEvent if a == EeWonWmEvent::Window as isize => Some(unsafe { (*rec_at(b)).window }),
        Op::WmEvent if a == EeWonWmEvent::SetWindow as isize => {
            // SAFETY: the live event's record
            unsafe { (*rec_at(b)).window = c };
            Some(0)
        }
        Op::WmEvent if a == EeWonWmEvent::Deliver as isize => {
            let r = rec_at(b);
            // SAFETY: the live event's record
            let (window, handler) = if r.is_null() { (0, 0) } else { unsafe { ((*r).window, (*r).handler) } };
            let r = match handler {
                x if x == C::HandleMouseEvent as u32 => handle(window, C::HandleMouseEvent, b),
                x if x == C::HandleKeyEvent as u32 => handle(window, C::HandleKeyEvent, b),
                x if x == C::HandleInputEvent as u32 => handle(window, C::HandleInputEvent, b),
                crate::wmobj::EE_WON_WM_HANDLER_COMPONENT => crate::gamewin::deliver_component_event(b),
                crate::wmobj::EE_WON_WM_HANDLER_POPUP => crate::gamewin::deliver_popup(b),
                _ => None,
            };
            // (a Rust-made event has no C++ Deliver: one whose window went is dropped)
            // SAFETY: the live event
            r.or_else(|| unsafe { crate::wmobj::is_rust_event(b) }.then_some(0))
        }
        Op::WmWin if a == EeWonWmWin::HandleMouse as isize => handle(b, C::HandleMouseEvent, c),
        Op::WmWin if a == EeWonWmWin::HandleKey as isize => handle(b, C::HandleKeyEvent, c),
        Op::WmWin if a == EeWonWmWin::HandleInput as isize => handle(b, C::HandleInputEvent, c),
        // ---- R6 WON round 33
        // a size event's place and size read into the record `c`, or set from it
        Op::WmEvent if a == SIZE => {
            let e = rec(b);
            // SAFETY: `c` is the record (the op's contract)
            unsafe { *(c as *mut EeWonRect4) = EeWonRect4 { x: e.x, y: e.y, w: e.width, h: e.height } };
            Some(0)
        }
        Op::WmEvent if a == SET_SIZE => {
            // SAFETY: `c` is the record (the op's contract)
            let r = unsafe { *(c as *const EeWonRect4) };
            rec_edit(b, &|e| (e.x, e.y, e.width, e.height) = (r.x, r.y, r.w, r.h));
            Some(0)
        }
        // a mouse event's button, a key event's key (pure reads of the record)
        Op::WmEvent if a == EeWonWmEvent::Button as isize => Some(rec(b).button as isize),
        Op::WmEvent if a == EeWonWmEvent::Key as isize => Some(rec(b).key as isize),
        // DoPopup's popup window: made, held for the Rust, set up as a one-component popup of `a` and created at
        // the rectangle `b`
        Op::WmNewPopupWindow => {
            let w = out(Op::WmNativeWindowNew, 0, 0, 0, 0);
            out(Op::WmWin, EeWonWmWin::Hold as isize, w, 0, 0);
            out(Op::WinSetOneComponent, w, 0, 0, 0);
            out(Op::WinSetStyle, w, 0x20, 0, 0);
            out(Op::WinSetParent, w, a, 0, 0);
            out(Op::WmWin, EeWonWmWin::SetIsPopup as isize, w, 1, 0);
            out(Op::WinCreate, w, b, 0, 0);
            Some(w)
        }
        // the default window's 8-point Tahoma with one reference for the caller (the local SmartPtr: the host's)
        Op::WmDefaultWindowFont => {
            out(Op::WmFontKeep, 0, 0, 0, 0);
            let f = out(Op::WmHostFont, 0, 0, 0, 0);
            if f != 0 {
                out(Op::WmFontHold, f, 0, 0, 0);
            }
            out(Op::WmHostFontClear, 0, 0, 0, 0);
            Some(f)
        }
        // the screen's resolution read (`a` 0: into the ints at `b`, `c`) or set (whether it was)
        Op::WmResolution if a == 0 => {
            out(Op::WmScreenResGet, b, c, 0, 0);
            Some(0)
        }
        Op::WmResolution => Some(out(Op::WmScreenResSet, b, c, 0, 0)),
        // a mouse event's window, point and button read, or its window and point set (`b` the event, `c` the record)
        Op::WmEvent if a == MOUSE => {
            // SAFETY: the live event's record
            let window = unsafe { (*rec_at(b)).window };
            let e = rec(b);
            // SAFETY: `c` is the record (the op's contract)
            unsafe { *(c as *mut EeWonWmMouse) = EeWonWmMouse { window, x: e.x, y: e.y, button: e.button } };
            Some(0)
        }
        Op::WmEvent if a == SET_MOUSE => {
            // SAFETY: `c` is the record (the op's contract)
            let m = unsafe { *(c as *const EeWonWmMouse) };
            // SAFETY: the live event's record
            unsafe { (*rec_at(b)).window = m.window };
            rec_edit(b, &|e| (e.x, e.y) = (m.x, m.y));
            Some(0)
        }
        // a font's, raw image's or image's reference taken or given back (a native image's is the C++ host's)
        Op::WmRef => {
            let (hold, release) = match a {
                x if x == EeWonWmRefKind::Font as isize => (Op::WmFontHold, Op::WmFontRelease),
                x if x == EeWonWmRefKind::RawImage as isize => (Op::WmRawHold, Op::WmRawRelease),
                x if x == EeWonWmRefKind::Image as isize => (Op::WmImageHold, Op::WmImageRelease),
                _ => return None,
            };
            out(if c != 0 { hold } else { release }, b, 0, 0, 0);
            Some(0)
        }
        // the decoder's picture with one reference for the caller (the original's SmartPtr: the host's)
        Op::WmDecode => {
            out(Op::WmDecodeKeep, a, b, c, 0);
            let image = out(Op::WmHostImage, 0, 0, 0, 0);
            if image != 0 {
                out(Op::WmRef, EeWonWmRefKind::NativeImage as isize, image, 1, 0);
            }
            out(Op::WmHostImageClear, 0, 0, 0, 0);
            Some(image)
        }
        Op::WmDecodeRaw => {
            out(Op::WmDecodeRawKeep, a, b, 0, 0);
            let image = out(Op::WmHostRaw, 0, 0, 0, 0);
            if image != 0 {
                out(Op::WmRawHold, image, 0, 0, 0);
            }
            out(Op::WmHostRawClear, 0, 0, 0, 0);
            Some(image)
        }
        // a delay-load image of the picture's size (0 when the decoder cannot read its attributes)
        Op::WmDelayImage => {
            if out(Op::WmImageAttributes, a, b, 0, 0) == 0 {
                return Some(0);
            }
            let image = out(Op::WmNewDelayImage, a, b, 0, 0);
            out(Op::WmImageHold, image, 0, 0, 0);
            Some(image)
        }
        _ => None,
    }
}

/// `MultiListHeader::GetSortTypeForButton`: the sort arrow of the header the button is in (`ListSortType_None`, 2,
/// without one or without arrows). `host` is the header's host; its ops here read only their arguments.
///
/// # Safety
/// `button` is a `Component*`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_sort_type_for_button(host: EeWonGuiHostFn, button: *mut c_void) -> i32 {
    let Some(host) = host else { return 2 };
    // SAFETY: the caller's host; the ops take their component as `a`
    let out = |op: Op, a: isize, b: isize, _c: isize, _d: isize| unsafe { host(std::ptr::null_mut(), op, a, b, 0, 0) };
    sort_type_for_button(button as isize, &out)
}

pub(crate) fn sort_type_for_button(mut button: isize, out: Out) -> i32 {
    const NONE: i32 = 2;
    loop {
        let parent = out(Op::ParentOf, button, 0, 0, 0);
        if parent == 0 {
            return NONE;
        }
        let header = out(Op::AsHeader, parent, 0, 0, 0);
        if header == 0 {
            button = parent;
            continue;
        }
        if out(Op::HeaderDrawsArrow, header, 0, 0, 0) != 0 {
            return out(Op::HeaderSortTypeOf, header, button, 0, 0) as i32;
        }
        return NONE;
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::cell::RefCell;

    fn run(f: impl Fn(Out) -> Option<isize>, answer: impl Fn(Op, isize) -> isize) -> (Option<isize>, Vec<String>) {
        let log = RefCell::new(Vec::new());
        let out = |op: Op, a: isize, b: isize, c: isize, d: isize| {
            log.borrow_mut().push(format!("{op:?} {a} {b} {c} {d}"));
            answer(op, a)
        };
        let r = f(&out);
        (r, log.into_inner())
    }

    #[test]
    fn the_measure_the_draws_and_the_list() {
        let mut m = [0i32; 3];
        let p = m.as_mut_ptr() as isize;
        let (r, log) = run(
            |o| text_op(Op::SegMeasure, 9, p, 0, o),
            |op, _| match op {
                Op::SegFontStringWidth => 40,
                Op::SegFontHeight => 12,
                _ => 10,
            },
        );
        assert_eq!((r, m), (Some(0), [40, 12, 10]));
        assert_eq!(log, ["SegFontStringWidth 9 0 0 0", "SegFontHeight 9 0 0 0", "SegFontAscent 9 0 0 0"]);
        // a plain draw is the C++ host's (ComponentOp's)
        assert_eq!(run(|o| text_op(Op::Draw, 1, 2, 0, o), |_, _| 0).0, None);
        assert_eq!(run(|o| text_op(Op::Draw, 1, 2, 3, o), |_, _| 0).1, ["TextDrawSeg 1 2 3 0"]);
        let (_, log) = run(|o| list_op(Op::TipEnd, [0; 4], o), |_, _| 0);
        assert_eq!(log, ["TipExists 0 0 0 0"]);
        let (_, log) = run(|o| list_op(Op::TipEnd, [0; 4], o), |_, _| 1);
        assert_eq!(log, ["TipExists 0 0 0 0", "TipEndCall 0 0 0 0"]);
        let (r, log) = run(|o| list_op(Op::NewColumnViewport, [0; 4], o), |_, _| 77);
        assert_eq!(r, Some(77));
        assert_eq!(log, ["NewViewport 0 0 0 0", "ChildHold 77 0 0 0", "ViewportAddChild 77 0 0 0"]);
        let (r, log) = run(|o| list_op(Op::NewTreeItem, [0; 4], o), |_, _| 5);
        assert_eq!(r, Some(5));
        assert_eq!(log, ["NewTreeItemRaw 0 0 0 0", "ItemHold 5 0 0 0"]);
    }

    #[test]
    fn the_filter_and_the_handles() {
        // no filter: 2 and nothing else; a filter's answer and the text taken back
        let (r, log) = run(|o| input_op(Op::Filter, 7, 3, 8, o), |_, _| 0);
        assert_eq!((r, log), (Some(2), vec!["InputHasFilter 0 0 0 0".to_string()]));
        // the typed text a Rust text the filter reads (and may replace) in place, taken back into the sink
        let typed = [u32::from(b'a'), u32::from(b'b')];
        let mut taken: Vec<u32> = Vec::new();
        let tp = &mut taken as *mut Vec<u32> as isize;
        let (r, log) = run(
            |o| input_op(Op::Filter, typed.as_ptr() as isize, 2, tp, o),
            |op, a| {
                if op == Op::InputFilterText {
                    // SAFETY: the GUIString slot the op hands over
                    assert_eq!(unsafe { crate::guistrobj::gstring_units(a as *const c_void) }, typed);
                }
                5
            },
        );
        assert_eq!(r, Some(1));
        assert_eq!(taken, typed);
        assert_eq!(log.len(), 2);
        assert_eq!(log[0], "InputHasFilter 0 0 0 0");
        assert!(log[1].starts_with("InputFilterText "));
        let (r, log) = run(|o| split_op(Op::NewDragComponent, 4, o), |_, _| 9);
        assert_eq!(r, Some(9));
        assert_eq!(
            log,
            [
                "NewComponent 0 0 0 0",
                "ChildHold 9 0 0 0",
                "ChildLayoutDrag 4 9 0 0",
                "ChildAddEx 4 9 0 0",
                "ChildRelease 9 0 0 0"
            ]
        );
        let (_, log) = run(|o| split_op(Op::NewSizedComponent, 12, o), |_, _| 9);
        assert_eq!(log, ["NewComponent 0 0 0 0", "ChildHold 9 0 0 0", "ChildSetSize 9 12 12 0"]);
    }

    #[test]
    fn the_input_paint_and_draw() {
        // SAFETY: plain data
        let mut p: EeWonInputPaint = unsafe { std::mem::zeroed() };
        let pa = &mut p as *mut EeWonInputPaint as isize;
        // enabled, an icon of 16, focused
        let (_, log) = run(
            |o| input_op(Op::PaintInputs, pa, 0, 0, o),
            |op, _| match op {
                Op::InputDisabled => 0,
                Op::InputIconHeight => 16,
                Op::FontHeight => 13,
                Op::InputTextColor => 7,
                _ => 1,
            },
        );
        assert!(!p.disabled_look && p.has_icon && p.has_focus);
        assert_eq!((p.icon_height, p.font_height, p.text_color, p.disabled_text_color), (16, 13, 7, 1));
        // the disabled test stops at IsDisabled
        assert!(!log.iter().any(|l| l.starts_with("InputUseDisabledColors")));
        // no icon: no height read
        let (_, log) = run(|o| input_op(Op::PaintInputs, pa, 0, 0, o), |op, _| (op != Op::InputHasIcon) as isize);
        assert!(p.disabled_look && !p.has_icon && p.icon_height == 0);
        assert!(!log.iter().any(|l| l.starts_with("InputIconHeight")));
        // SAFETY: plain data
        let mut o: EeWonDrawOp = unsafe { std::mem::zeroed() };
        o.kind = EE_WON_DRAW_CALL;
        o.b = 1;
        let ob = &o as *const EeWonDrawOp as isize;
        assert_eq!(
            run(|f| input_op(Op::Draw, 3, ob, 0, f), |_, _| 0).1,
            [format!("InputPaintDisabledBack 3 {ob} 0 0")]
        );
        o.b = 0;
        let ob = &o as *const EeWonDrawOp as isize;
        assert_eq!(run(|f| input_op(Op::Draw, 3, ob, 0, f), |_, _| 0).1, [format!("InputPaintBack 3 {ob} 0 0")]);
        o.kind = 0;
        let ob = &o as *const EeWonDrawOp as isize;
        assert_eq!(run(|f| input_op(Op::Draw, 3, ob, 0, f), |_, _| 0).1, [format!("InputDrawSlots 3 {ob} 0 0")]);
        assert_eq!(run(|f| label_op(Op::Draw, 3, ob, f), |_, _| 0).1, [format!("LabelDrawWrapped 3 {ob} 0 0")]);
    }

    #[test]
    fn the_manager_ops() {
        // the mouse record read and set over the event's record (WmInputRec, R6 WON GUI rounds 37 and 38)
        let mut whole = crate::wmobj::EeWonWmEventRec {
            ev: crate::winobj::EeWonWinEvent { kind: 9, x: 11, y: 12, button: 2, key: 65, ..Default::default() },
            window: 70,
            handler: 0,
        };
        let ep = &mut whole as *mut crate::wmobj::EeWonWmEventRec as isize;
        // SAFETY: the test's own record
        let e = unsafe { &mut (*(ep as *mut crate::wmobj::EeWonWmEventRec)).ev };
        let mut m = EeWonWmMouse { window: 0, x: 0, y: 0, button: 0 };
        let mp = &mut m as *mut EeWonWmMouse as isize;
        let rec = move |op: Op, _: isize| match op {
            Op::WmInputRec => ep,
            _ => 0,
        };
        let (_, log) = run(|o| wm_op(Op::WmEvent, EeWonWmEvent::Mouse as isize, 6, mp, o), rec);
        assert_eq!((m.window, m.x, m.y, m.button), (70, 11, 12, 2));
        assert_eq!(log, ["WmInputRec 6 0 0 0", "WmInputRec 6 0 0 0"]);
        // SAFETY: the test's own record
        unsafe { (*(mp as *mut EeWonWmMouse)).x = 21 };
        // SAFETY: as above
        unsafe { (*(mp as *mut EeWonWmMouse)).y = 22 };
        // SAFETY: as above
        unsafe { (*(mp as *mut EeWonWmMouse)).window = 71 };
        let (_, log) = run(|o| wm_op(Op::WmEvent, EeWonWmEvent::SetMouse as isize, 6, mp, o), rec);
        assert_eq!(log, ["WmInputRec 6 0 0 0", "WmInputRec 6 0 0 0"]);
        // the type and the window are the record's
        assert_eq!(run(|o| wm_op(Op::WmEvent, EeWonWmEvent::Type as isize, 6, 0, o), rec).0, Some(9));
        assert_eq!(run(|o| wm_op(Op::WmEvent, EeWonWmEvent::Window as isize, 6, 0, o), rec).0, Some(71));
        run(|o| wm_op(Op::WmEvent, EeWonWmEvent::SetWindow as isize, 6, 72, o), rec);
        assert_eq!(run(|o| wm_op(Op::WmEvent, EeWonWmEvent::Window as isize, 6, 0, o), rec).0, Some(72));
        // an input event for a window the game did not bind (a driver's) is the host's to deliver
        assert_eq!(run(|o| wm_op(Op::WmWin, EeWonWmWin::HandleKey as isize, 72, 6, o), rec).0, None);
        assert_eq!((e.x, e.y), (21, 22));
        // the button and the key are the record's
        assert_eq!(run(|o| wm_op(Op::WmEvent, EeWonWmEvent::Button as isize, 6, 0, o), rec).0, Some(2));
        assert_eq!(run(|o| wm_op(Op::WmEvent, EeWonWmEvent::Key as isize, 6, 0, o), rec).0, Some(65));
        // the size record
        let mut r = EeWonRect4::default();
        let rp = &mut r as *mut EeWonRect4 as isize;
        run(|o| wm_op(Op::WmEvent, EeWonWmEvent::Size as isize, 6, rp, o), rec);
        assert_eq!((r.x, r.y, r.w, r.h), (21, 22, 0, 0));
        // SAFETY: the test's own record
        unsafe { *(rp as *mut EeWonRect4) = EeWonRect4 { x: 1, y: 2, w: 3, h: 4 } };
        run(|o| wm_op(Op::WmEvent, EeWonWmEvent::SetSize as isize, 6, rp, o), rec);
        assert_eq!((e.x, e.y, e.width, e.height), (1, 2, 3, 4));
        // other events are the C++ host's
        assert_eq!(run(|o| wm_op(Op::WmEvent, EeWonWmEvent::Deliver as isize, 6, mp, o), rec).0, None);
        assert_eq!(run(|o| wm_op(Op::WmEvent, EeWonWmEvent::Deliver as isize, 6, mp, o), |_, _| 0).0, None);
        // references
        assert_eq!(
            run(|o| wm_op(Op::WmRef, EeWonWmRefKind::Font as isize, 5, 1, o), |_, _| 0).1,
            ["WmFontHold 5 0 0 0"]
        );
        assert_eq!(
            run(|o| wm_op(Op::WmRef, EeWonWmRefKind::Image as isize, 5, 0, o), |_, _| 0).1,
            ["WmImageRelease 5 0 0 0"]
        );
        assert_eq!(run(|o| wm_op(Op::WmRef, EeWonWmRefKind::NativeImage as isize, 5, 0, o), |_, _| 0).0, None);
        // the decodes: held only when found
        let (r, log) = run(|o| wm_op(Op::WmDecode, 1, 2, 3, o), |op, _| (op == Op::WmHostImage) as isize * 40);
        assert_eq!(r, Some(40));
        assert_eq!(
            log,
            [
                "WmDecodeKeep 1 2 3 0".to_string(),
                "WmHostImage 0 0 0 0".into(),
                format!("WmRef {} 40 1 0", EeWonWmRefKind::NativeImage as isize),
                "WmHostImageClear 0 0 0 0".into()
            ]
        );
        let (r, log) = run(|o| wm_op(Op::WmDecodeRaw, 2, 3, 0, o), |_, _| 0);
        assert_eq!((r, log.len()), (Some(0), 3));
        let (r, log) = run(|o| wm_op(Op::WmDelayImage, 2, 3, 0, o), |_, _| 0);
        assert_eq!((r, log.len()), (Some(0), 1));
        let (r, log) = run(|o| wm_op(Op::WmDelayImage, 2, 3, 0, o), |_, _| 8);
        assert_eq!(r, Some(8));
        assert_eq!(log, ["WmImageAttributes 2 3 0 0", "WmNewDelayImage 2 3 0 0", "WmImageHold 8 0 0 0"]);
    }

    #[test]
    fn the_sort_type_walks_up_to_the_header() {
        // button 1 in 2 in header 3 (arrows on, type 1)
        let parent = |a| match a {
            1 => 2,
            2 => 3,
            _ => 0,
        };
        let (r, log) = run(
            |o| Some(sort_type_for_button(1, o) as isize),
            |op, a| match op {
                Op::ParentOf => parent(a),
                Op::AsHeader => (a == 3) as isize * 3,
                Op::HeaderDrawsArrow => 1,
                Op::HeaderSortTypeOf => 1,
                _ => 0,
            },
        );
        assert_eq!(r, Some(1));
        assert_eq!(log.last().unwrap(), "HeaderSortTypeOf 3 2 0 0");
        // no header, or no arrows
        assert_eq!(run(|o| Some(sort_type_for_button(1, o) as isize), |_, _| 0).0, Some(2));
        let (r, _) = run(
            |o| Some(sort_type_for_button(1, o) as isize),
            |op, a| match op {
                Op::ParentOf => parent(a),
                Op::AsHeader => a,
                _ => 0,
            },
        );
        assert_eq!(r, Some(2));
    }

    #[test]
    fn the_round_33_compositions_stop_where_the_original_returned() {
        // no window: nothing else read
        assert_eq!(
            run(|o| list_op(Op::PointerOutside, [0; 4], o), |_, _| 0),
            (Some(0), vec!["ListRoot 0 0 0 0".into()])
        );
        // a window, no parent
        let (r, log) = run(|o| list_op(Op::PointerOutside, [0; 4], o), |op, _| if op == Op::ListRoot { 9 } else { 0 });
        assert_eq!((r, log.len()), (Some(0), 4));
        // inside: 0; the tooltip made once
        let (r, log) = run(|o| list_op(Op::PointerOutside, [0; 4], o), |_, _| 3);
        assert_eq!((r, log.last().unwrap().as_str()), (Some(0), "ListContains 3 3 0 0"));
        assert_eq!(
            run(|o| list_op(Op::TipShow, [5, 0, 0, 0], o), |_, _| 0).1,
            ["TipExists 0 0 0 0", "TipNew 0 0 0 0", "TipDo 5 0 0 0"]
        );
        assert_eq!(run(|o| list_op(Op::TipHide, [0; 4], o), |_, _| 0).1, ["TipExists 0 0 0 0"]);
        // the popup window: made, held, set up, created
        let (r, log) =
            run(|o| wm_op(Op::WmNewPopupWindow, 4, 8, 0, o), |op, _| if op == Op::WmNativeWindowNew { 70 } else { 0 });
        assert_eq!((r, log.len(), log[6].as_str()), (Some(70), 7, "WinCreate 70 8 0 0"));
        // the font held only when there is one
        assert_eq!(run(|o| wm_op(Op::WmDefaultWindowFont, 0, 0, 0, o), |_, _| 0).1.len(), 3);
        assert_eq!(
            run(|o| wm_op(Op::WmResolution, 1, 800, 600, o), |_, _| 1),
            (Some(1), vec!["WmScreenResSet 800 600 0 0".into()])
        );
    }
}
