//! The GUI hosts' one-statement forwards answered in the game (R6 WON GUI round 41): a C++ case that called a
//! widget's non-virtual method, itself a one-line forward to the widget's Rust object (`ee_won_*_call`), is that
//! call here, the object found by the widget's address in the registry; a case that wrote a list's or text area's
//! own font, colour or sort function writes the member in place (the offsets GuiHost.cpp registers). None in a
//! driver's world (its hosts answer every op), for a widget without the object and for the other ops.

use std::ffi::c_void;
use std::sync::Mutex;

use crate::guihost::EeWonGuiOp as Op;
use crate::registry::{get, EeWonGuiKind as Kind};

/// The areas' members leafpaint.rs's record lacks: where a `ListArea` keeps its sort function, a `TextArea` its fonts
/// and colours and a `ScrollArea` its viewport (a SmartPtr) (GuiHost.cpp; the list's font and colour are
/// leafpaint.rs's).
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeWonAreaAt {
    pub list_sort_func: usize,
    pub text_font: usize,
    pub text_link_font: usize,
    pub text_text_color: usize,
    pub text_link_color: usize,
    pub pane_viewport: usize,
    /// `Label::mBackground`, `InputBox::mDisabledBackground` (Backgrounds: their colour is leafpaint.rs's
    /// `bg_color` in them), `TabCtrl::mContainerLayout` (a SmartPtr)
    pub label_background: usize,
    pub input_disabled_background: usize,
    pub tab_container_layout: usize,
}

/// `comp`'s member at the registered offset `f` picks (None before the registration or for no component).
pub(crate) fn member(comp: isize, f: impl Fn(&EeWonAreaAt) -> usize) -> Option<usize> {
    let at = (*AREA_AT.lock().ok()?).filter(|_| comp != 0)?;
    Some(comp as usize + f(&at))
}

static AREA_AT: Mutex<Option<EeWonAreaAt>> = Mutex::new(None);

/// Registers the list and text areas' member offsets (GuiHost.cpp at static initialisation).
///
/// # Safety
/// `at` points at the offsets.
#[no_mangle]
pub unsafe extern "C" fn ee_won_area_members_register(at: *const EeWonAreaAt) {
    if let Ok(mut s) = AREA_AT.lock() {
        // SAFETY: the contract
        *s = Some(unsafe { *at });
    }
}

/// The font the manager names `name` (raw, not held; the default font for an unknown name).
pub(crate) fn named_font(name: &str) -> Option<isize> {
    crate::gamewin::wm(crate::wmobj::EeWonWmCall::GetNamedFont, name.as_ptr() as isize, name.len() as isize)
}

/// `slot` (a `SmartPtr<Font>` member) set to `font`: the new font held, the old one given back.
///
/// # Safety
/// `slot` is a live object's SmartPtr member.
pub(crate) unsafe fn set_font(slot: *mut isize, font: isize) {
    if font != 0 {
        crate::gamewin::rf(font, true);
    }
    // SAFETY: the contract
    let old = unsafe { std::ptr::replace(slot, font) };
    if old != 0 {
        crate::gamewin::rf(old, false);
    }
}

/// A list area's, text area's, label's or input box's (`which`) constructor statements over its own members, in
/// place, and the list's selection background.
pub(crate) fn area_op(which: crate::guisplit::EeWonGuiSplitHost, me: isize, op: Op, a: isize) -> Option<isize> {
    use crate::guisplit::EeWonGuiSplitHost as W;
    if !crate::compobj::is_game() || me == 0 {
        return None;
    }
    let text = which == W::Text;
    match (which, op) {
        // `mBackground.SetColor(-1)`, `mDisabledBackground.SetColor(0x80000001)`: the Background's colour
        (W::Label, Op::CtorBackgroundNone) | (W::Input, Op::CtorDisabledBackground) => {
            let leaf = crate::leafpaint::leaf_at()?;
            let (bg, color) = if which == W::Label {
                (member(me, |at| at.label_background)?, -1)
            } else {
                (member(me, |at| at.input_disabled_background)?, 0x80000001u32 as i32)
            };
            // SAFETY: the game's live widget's Background member, its colour at leafpaint.rs's offset
            unsafe { *((bg + leaf.bg_color) as *mut i32) = color };
            return Some(0);
        }
        (W::Input, Op::CtorFontNamed) => return crate::inputobj::game_set_font(me, named_font("InputBox")?),
        (W::List, Op::SelBackground) => {
            // `GetSelColor().GetBackgroundColor(g)`: the selection colour's background (an unsigned long)
            let leaf = crate::leafpaint::leaf_at()?;
            // SAFETY: the game's live list, its SelectionColor at leafpaint.rs's offset
            return Some(unsafe { (*((me as usize + leaf.list_sel) as *const crate::guidraw::EeWonSelColor)).background } as isize);
        }
        (W::List | W::Text, _) => {}
        _ => return None,
    }
    if op == Op::CtorPads {
        // `SetPads(a, a, a, a)`: the scroll pane's four fields
        use crate::guihost::EeWonGuiField as Fd;
        // SAFETY: the area's Rust pane lives as long as the area
        let p = unsafe { get::<crate::paneobj::EeWonScrollPane>(me, Kind::Pane) }?;
        for f in [Fd::LeftPad, Fd::TopPad, Fd::RightPad, Fd::BottomPad] {
            p.call(crate::paneobj::EeWonPaneCall::SetField, f as isize, a as i32 as isize, 0);
        }
        return Some(0);
    }
    let at = (*AREA_AT.lock().ok()?)?;
    let m = |off: usize| (me as usize + off) as *mut isize;
    // SAFETY (the writes below): the game's live area `me` (the host's), each member at its registered offset
    let int = |off: usize, v: u32| unsafe { *(m(off) as *mut i32) = v as i32 };
    if text {
        match op {
            Op::CtorTextColor => int(at.text_text_color, 0x8000000e),
            Op::CtorLinkColor => int(at.text_link_color, 0x0000ff),
            // SAFETY: as above
            Op::CtorFontNamed => unsafe { set_font(m(at.text_font), named_font("TextArea")?) },
            // SAFETY: as above
            Op::CtorLinkFontNamed => unsafe { set_font(m(at.text_link_font), named_font("TextAreaLink")?) },
            Op::CtorLinkFontIsDefault => {
                let d = crate::gamewin::wm(crate::wmobj::EeWonWmCall::GetDefaultFont, 0, 0)?;
                // SAFETY: as above
                return Some(isize::from(unsafe { *m(at.text_link_font) } == d));
            }
            Op::CtorReset => {
                // SAFETY: the area's Rust area lives as long as the area
                let t = unsafe { get::<crate::textobj::EeWonText>(me, Kind::Text) }?;
                t.call(crate::textobj::EeWonTextCall::Reset, 0, 0, 0);
            }
            _ => return None,
        }
        return Some(0);
    }
    let leaf = crate::leafpaint::leaf_at()?;
    match op {
        Op::CtorTextColor => int(leaf.list_text_color, 0x8000000e),
        Op::CtorMenuTextColor => int(leaf.list_text_color, 0x8000000c),
        // SAFETY: as above
        Op::CtorFontNamed => unsafe { set_font(m(leaf.list_font), named_font("ListArea")?) },
        // SAFETY: as above
        Op::CtorMenuFont => unsafe { set_font(m(leaf.list_font), named_font("Menu")?) },
        // SAFETY: as above
        Op::CtorNoSortFunc => unsafe { *m(at.list_sort_func) = 0 },
        _ => return None,
    }
    Some(0)
}

/// A list's `SetFont(GetNamedFont(name))` (the shells' MSButton font on a list).
pub(crate) fn list_set_named_font(list: isize, name: &str) -> Option<isize> {
    if !crate::compobj::is_game() || list == 0 {
        return None;
    }
    let leaf = crate::leafpaint::leaf_at()?;
    // SAFETY: the game's live list `list`, its font member at the registered offset
    unsafe { set_font((list as usize + leaf.list_font) as *mut isize, named_font(name)?) };
    Some(0)
}

/// a void call's answer (the C++ case's `return 0`)
fn z(_: isize) -> isize {
    0
}

/// The forwards (`comp` the asking widget, the op's `a`, `b`, `c`).
pub(crate) fn fwd_op(comp: isize, op: Op, a: isize, b: isize, c: isize) -> Option<isize> {
    use crate::btnobj::{EeWonBtnCall as B, EeWonBtnField as BF, EeWonButton};
    use crate::ctobj::{EeWonContainer, EeWonCtCall as T, EeWonCtField as TF};
    use crate::listobj::{EeWonList, EeWonListCall as L};
    use crate::menuobj::{EeWonMenu, EeWonMenuCall as M};
    use crate::multiobj::{EeWonMHead, EeWonMHeadCall as H, EeWonMulti, EeWonMultiCall as U};
    use crate::paneobj::{EeWonPaneCall as P, EeWonScrollPane};
    use crate::scrollerobj::{EeWonScroller, EeWonScrollerCall as S};
    if !crate::compobj::is_game() {
        return None;
    }
    // SAFETY (each look-up): the game's live widgets (the ops' contracts) hold their Rust objects; the calls re-enter
    // through the objects' entries with shared references
    let ct = |w: isize| unsafe { get::<EeWonContainer>(w, Kind::Container) };
    // SAFETY: as above
    let btn = |w: isize| unsafe { get::<EeWonButton>(w, Kind::Button) };
    // SAFETY: as above
    let pane = |w: isize| unsafe { get::<EeWonScrollPane>(w, Kind::Pane) };
    // SAFETY: as above
    let scroller = |w: isize| unsafe { get::<EeWonScroller>(w, Kind::Scroller) };
    // SAFETY: as above
    let list = |w: isize| unsafe { get::<EeWonList>(w, Kind::List) };
    // SAFETY: as above
    let menu = |w: isize| unsafe { get::<EeWonMenu>(w, Kind::Menu) };
    // SAFETY: as above
    let head = |w: isize| unsafe { get::<EeWonMHead>(w, Kind::MHead) };
    // SAFETY: as above
    let multi = |w: isize| unsafe { get::<EeWonMulti>(w, Kind::Multi) };
    // SAFETY: Component's entry on the game's live widget
    let kc = |w: isize, k: crate::compobj::EeWonCompCall, p: isize| unsafe {
        crate::compobj::ee_won_comp_call(w as *mut c_void, crate::compobj::host(), k, p, 0, 0)
    };
    let on = |x: isize| isize::from(x != 0);
    let viewport = |w: isize| {
        let at = (*AREA_AT.lock().ok()?).filter(|_| w != 0)?;
        // SAFETY: the game's live scroll area `w`, its viewport SmartPtr at the registered offset
        Some(unsafe { *((w as usize + at.pane_viewport) as *const isize) })
    };
    let window = |w: isize, f: crate::winobj::EeWonWinField| {
        crate::gamewin::parts(w).map(|p| crate::gamewin::field(&p, f) as i32 as isize)
    };
    Some(match op {
        // a container's (the scroller's) children and focus
        Op::HasFocusChild => on(ct(comp)?.call(T::GetField, TF::FocusChild as isize, 0, 0)),
        Op::FocusChildLost => z(ct(comp)?.call(T::DropFocusChild, 0, 0, 0)),
        Op::AddChild => z(ct(comp)?.call(T::AddChild, a, 0, 0)),
        Op::ChildAddEx => z(ct(a)?.call(T::AddChildEx, b, 0, 0)),
        Op::CursorIs => {
            isize::from(ct(comp)?.call(T::GetField, TF::CurrentCursor as isize, 0, 0) == crate::guihost::ee_won_gui_cursor(a))
        }
        Op::ChildSetDefaultCursor => {
            kc(a, crate::compobj::EeWonCompCall::SetDefaultCursor, crate::guihost::ee_won_gui_cursor(b));
            0
        }
        Op::RootKeyMod => window(kc(comp, crate::compobj::EeWonCompCall::Root, 0), crate::winobj::EeWonWinField::KeyMod)?,
        Op::CheckDoubleClick => on(kc(comp, crate::compobj::EeWonCompCall::DoubleClick, a)),
        Op::WinMouseX => window(a, crate::winobj::EeWonWinField::MouseX)?,
        Op::WinMouseY => window(a, crate::winobj::EeWonWinField::MouseY)?,
        // the buttons (the splitter's, the bar's)
        Op::ButtonSetFlags => z(btn(a)?.call(B::SetButtonFlags, b, on(c), 0)),
        Op::ButtonSetCheck => z(btn(a)?.call(B::SetCheck, on(b), 0, 0)),
        Op::ButtonIsChecked => on(btn(a)?.call(B::GetField, BF::Flags as isize, 0, 0) & 4),
        // the scroll areas and scrollers
        Op::BarOf => crate::registry::find(a, Kind::Bar) as isize,
        Op::AreaGet => pane(a)?.call(P::GetField, b, 0, 0) as i32 as isize,
        Op::AreaSet => z(pane(a)?.call(P::SetField, b, c as i32 as isize, 0)),
        Op::AreaSetScroller => z(pane(a)?.call(P::SetScroller, comp, 0, 0)),
        Op::ScrollerSetArea => z(scroller(a)?.call(S::SetScrollArea, comp, 0, 0)),
        Op::ScrollerSetScrollbar => z(scroller(a)?.call(S::SetScrollbar, b, 0, 0)),
        // the multi-column list's header
        Op::AsHeader => {
            if a != 0 && !crate::registry::find(a, Kind::MHead).is_null() {
                a
            } else {
                0
            }
        }
        Op::HeaderDrawsArrow => on(head(a)?.call(H::DrawSortArrow, 0, 0, 0)),
        Op::HeaderSortTypeOf => {
            let h = head(a)?;
            let column = h.call(H::ColumnIndex, b, 0, 0) as i32 as isize;
            h.call(H::SortType, column, 0, 0)
        }
        Op::HeaderNotify => z(head(a)?.call(H::NotifyColumnChange, 0, 0, 0)),
        Op::HeaderSetSortColumn => z(head(a)?.call(H::SetSortColumn, b as i32 as isize, c as i32 as isize, 0)),
        Op::CtorHeaderArea => z(head(comp)?.call(H::SetMultiListArea, a, 0, 0)),
        Op::MultiOf => crate::registry::find(a, Kind::Multi) as isize,
        Op::CtorMultiColumns => z(multi(comp)?.call(U::SetNumColumns, a as i32 as isize, 0, 0)),
        // a scroll area's viewport (`GetViewport()`; the list's view's AddChild, the pane constructor's)
        Op::GetViewport => viewport(comp)?,
        Op::ViewportAddChild => z(ct(viewport(comp)?)?.call(T::AddChild, a, 0, 0)),
        Op::CtorAddViewport => z(ct(comp)?.call(T::AddChild, viewport(comp)?, 0, 0)),
        // a menu item's submenu and disabled flag (leafpaint.rs's offsets)
        Op::MenuItemSub => {
            let leaf = crate::leafpaint::leaf_at()?;
            // SAFETY: the game's live menu item `a` (the op's contract), its SmartPtr member
            unsafe { *((a as usize + leaf.sub_menu) as *const isize) }
        }
        Op::MenuItemDisabled => {
            let leaf = crate::leafpaint::leaf_at()?;
            // SAFETY: as above, its bool member
            isize::from(unsafe { *((a as usize + leaf.disabled) as *const u8) } != 0)
        }
        // a multi-column row's cells (MultiListItem's GetItem and SetItem: the row's calls with no list)
        Op::RowCell => {
            // SAFETY: as above
            let r = unsafe { get::<crate::multiobj::EeWonMRow>(a, Kind::MRow) }?;
            r.call(None, crate::multiobj::EeWonMRowCall::GetItem, b as i32 as isize, 0, 0)
        }
        Op::RowSetCell => {
            // SAFETY: as above
            let r = unsafe { get::<crate::multiobj::EeWonMRow>(a, Kind::MRow) }?;
            z(r.call(None, crate::multiobj::EeWonMRowCall::SetItem, b, c as i32 as isize, 0))
        }
        // `GetRoot()->ClosePopup(a)`: the one manager's EndPopup
        Op::ClosePopup => z(crate::gamewin::wm(crate::wmobj::EeWonWmCall::EndPopup, a, 0)?),
        // the tree's rows
        Op::NodeOf => crate::registry::find(a, Kind::TreeNode) as isize,
        // the popup menus
        Op::MenuSelItem => list(a)?.call(L::SelItem, 0, 0, 0),
        Op::MenuSetSelAt => z(list(a)?.call(L::SetSelItemAt, b as i32 as isize, 1, 0)),
        Op::MenuReset => z(menu(a)?.call(M::Reset, 0, 0, 0)),
        Op::MenuTryKeyDown => on(menu(a)?.call(M::TryKeyDown, b as i32 as isize, 0, 0)),
        Op::CtorMenuListFlags => z(list(comp)?.call(L::SetListFlags, 0x806, 1, 0)),
        Op::CtorMenuBorderPadding => {
            let p: [i32; 4] = [4, 4, 4, 4];
            z(pane(comp)?.call(P::SetBorderPadding, p.as_ptr() as isize, 0, 0))
        }
        _ => return None,
    })
}
