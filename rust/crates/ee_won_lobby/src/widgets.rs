//! The lobby hosts' widget ops answered in Rust (R6 WONLobby round 19).
//!
//! A dialog's or a control's host op on one of its WONGUI controls was a one-line call of the C++ widget's method
//! (`anInput->Clear(b != 0)`), and that method a one-line forward to the widget's Rust object
//! (`ee_won_input_call(this->mRust, ...)`). With the widget registry (ee_won_gui registry.rs) a game object makes the
//! forward itself: the same Rust call with the same arguments, by construction. Only non-virtual methods are
//! answered here: a virtual call (SetEnabled, SetVisible, Invalidate, ...) still goes through the C++ vtable, since a
//! subclass may override it. Reads of the component's own members are the members' block (compmem.rs), and
//! Component's non-virtual calls go through the registered component host (compobj.rs `host()`).
//!
//! An answer needs the control's object in the registry; without one (never in the game: every control the lobby
//! holds was made by its C++ constructor, which registered it) the op goes to the C++ host as before.

use ee_won_gui::btnobj::{EeWonBtnCall, EeWonBtnField, EeWonButton};
use ee_won_gui::comboobj::{EeWonComboBox, EeWonComboCall};
use ee_won_gui::compmem as m;
use ee_won_gui::compobj::{host, Comp, EeWonCompCall as K};
use ee_won_gui::ctobj::{EeWonContainer, EeWonCtCall};
use ee_won_gui::dialogobj::{EeWonDialog, EeWonDlgCall, EeWonDlgField};
use ee_won_gui::guihost::EeWonGuiField;
use ee_won_gui::inputobj::EeWonInput;
use ee_won_gui::listobj::{EeWonList, EeWonListCall as L};
use ee_won_gui::multiobj::{EeWonMulti, EeWonMultiCall};
use ee_won_gui::paneobj::{EeWonPaneCall, EeWonScrollPane};
use ee_won_gui::registry::{get, EeWonGuiKind as Kind};
use ee_won_gui::treeobj::{EeWonTNode, EeWonTNodeCall, EeWonTree, EeWonTreeCall};

use crate::lobbyctl::EeWonLobbyCtlOp as COp;
use crate::lobbydlg::EeWonLobbyDlgOp as DOp;
use crate::lobbypopup::EeWonLobbyPopupOp as POp;

fn input<'a>(a: isize) -> Option<&'a EeWonInput> {
    // SAFETY: the kind's type; the caller holds the control for the call
    unsafe { get(a, Kind::Input) }
}
fn button<'a>(a: isize) -> Option<&'a EeWonButton> {
    // SAFETY: as `input`
    unsafe { get(a, Kind::Button) }
}
fn combo<'a>(a: isize) -> Option<&'a EeWonComboBox> {
    // SAFETY: as `input`
    unsafe { get(a, Kind::Combo) }
}
/// `((ComboBox*)combo)->InsertItem(item, pos)` on the combo box's Rust object (None for another component).
pub(crate) fn combo_insert(combo_box: isize, item: isize, pos: isize) -> Option<()> {
    combo(combo_box)?.call(EeWonComboCall::InsertItem, item, pos, 0);
    Some(())
}
fn list<'a>(a: isize) -> Option<&'a EeWonList> {
    // SAFETY: as `input`
    unsafe { get(a, Kind::List) }
}
fn multi<'a>(a: isize) -> Option<&'a EeWonMulti> {
    // SAFETY: as `input`
    unsafe { get(a, Kind::Multi) }
}
fn pane<'a>(a: isize) -> Option<&'a EeWonScrollPane> {
    // SAFETY: as `input`
    unsafe { get(a, Kind::Pane) }
}
fn tree<'a>(a: isize) -> Option<&'a EeWonTree> {
    // SAFETY: as `input`
    unsafe { get(a, Kind::Tree) }
}
fn node<'a>(a: isize) -> Option<&'a EeWonTNode> {
    // SAFETY: as `input`
    unsafe { get(a, Kind::TreeNode) }
}
fn dialog_obj<'a>(a: isize) -> Option<&'a EeWonDialog> {
    // SAFETY: as `input`
    unsafe { get(a, Kind::Dialog) }
}
fn container<'a>(a: isize) -> Option<&'a EeWonContainer> {
    // SAFETY: as `input`
    unsafe { get(a, Kind::Container) }
}

/// A void call's answer (the call made, 0).
fn zero(_: isize) -> isize {
    0
}

/// Component's non-virtual call `k` on `c` (the C++ inline `KCall`).
fn comp(c: isize, k: K, x: isize, y: isize) -> isize {
    Comp::new(host()).call(c, k, x, y, 0)
}

/// `sprintf(aBuf, "%d", n)` as a text's units.
fn number_units(n: i32) -> Vec<u32> {
    n.to_string().bytes().map(u32::from).collect()
}

/// A C++ `GUIStringData`'s units (the hosts' `Txt(b)`), read in place.
fn text_units(data: isize) -> Vec<u32> {
    // SAFETY: the op's text is a live GUIStringData (or NULL)
    unsafe { ee_won_gui::guistrobj::data_units(data as *const std::ffi::c_void) }
}

/// `ListArea`'s non-virtual forwards, shared by the dialog and control hosts: the list call and its arguments.
fn list_call(a: isize, call: L, x: isize, y: isize) -> Option<isize> {
    Some(list(a)?.call(call, x, y, 0))
}

/// The dialog host's widget ops (lobbydlg.rs's game mode); `None`: not one, or no object.
pub(crate) fn dlg_op(dialog: isize, op: DOp, a: isize, b: isize, c: isize) -> Option<isize> {
    let bi = b as i32 as isize;
    match op {
        // the dialog itself: `aDialog->Dialog::HandleComponentEvent(e)`, `AddChild(c)`, the drag flag, its roots
        DOp::BaseEvent => Some(zero(dialog_obj(dialog)?.call(EeWonDlgCall::HandleComponentEvent, a, 0, 0))),
        DOp::AddChild => Some(zero(container(dialog)?.call(EeWonCtCall::AddChild, a, 0, 0))),
        DOp::IsDragging => Some(
            (dialog_obj(dialog)?.call(EeWonDlgCall::GetField, EeWonDlgField::Dragging as isize, 0, 0) != 0) as isize,
        ),
        DOp::SetDragging => Some(zero(dialog_obj(dialog)?.call(
            EeWonDlgCall::SetField,
            EeWonDlgField::Dragging as isize,
            (b != 0) as isize,
            0,
        ))),
        DOp::Root => Some(comp(dialog, K::Root, 0, 0)),
        DOp::RootWindow => Some(comp(dialog, K::RootWindow, 0, 0)),
        // InputBox
        DOp::InputClear | DOp::InputSetEmpty => {
            input(a)?.set_text(&[], b != 0);
            Some(0)
        }
        DOp::InputTextLength => Some(input(a)?.real_text().len() as isize),
        DOp::InputShownLength => Some(input(a)?.shown_len() as isize),
        DOp::InputToInt => {
            Some(ee_won_gui::guistring::to_int(&input(a)?.real_text(), 0, &ee_won_net::chat::CrtChat) as isize)
        }
        DOp::InputSetNumber => {
            input(a)?.set_text(&number_units(b as i32), c != 0);
            Some(0)
        }
        DOp::InputSelectRange => {
            input(a)?.select_range(b as i32, c as i32);
            Some(0)
        }
        // Button and the component members
        DOp::ButtonIsDisabled => Some(m::is_disabled(a) as isize),
        DOp::ButtonIsChecked => {
            Some(((button(a)?.call(EeWonBtnCall::GetField, EeWonBtnField::Flags as isize, 0, 0) & 4) != 0) as isize)
        }
        DOp::ButtonSetCheck => {
            Some(zero(button(a)?.call(EeWonBtnCall::SetCheck, (b != 0) as isize, (c != 0) as isize, 0)))
        }
        DOp::ButtonSetControlId => {
            m::set_control_id(a, b as i32);
            Some(0)
        }
        // ComboBox
        DOp::ComboGetSel => Some(combo(a)?.call(EeWonComboCall::GetSelItem, 0, 0, 0) as i32 as isize),
        DOp::ComboSetSel => Some(zero(combo(a)?.call(EeWonComboCall::SetSelItem, bi, (c != 0) as isize, 0))),
        // ListArea
        DOp::ListBeginUpdate => list_call(a, L::BeginUpdate, 0, 0).map(|_| 0),
        DOp::ListEndUpdate => list_call(a, L::EndUpdate, 0, 0).map(|_| 0),
        DOp::ListClear => list_call(a, L::Clear, (b != 0) as isize, 0).map(|_| 0),
        DOp::ListRewind => list_call(a, L::RewindSelections, 0, 0).map(|_| 0),
        DOp::ListHasMore => list_call(a, L::HasMoreSelections, 0, 0).map(|r| (r != 0) as isize),
        DOp::ListNextSel => list_call(a, L::GetNextSelection, 0, 0),
        DOp::ListSetSel => list_call(a, L::SetSelItemAt, bi, (c != 0) as isize).map(|_| 0),
        DOp::ListNumItems => list_call(a, L::NumItems, 0, 0).map(|r| r as i32 as isize),
        DOp::ListGetSel => list_call(a, L::SelItem, 0, 0),
        DOp::ListGetItem => list_call(a, L::GetItem, bi, 0),
        DOp::ListRefresh => list_call(a, L::RefreshItem, b, 0).map(|_| 0),
        DOp::ListRemove => list_call(a, L::RemoveItem, b, 0).map(|_| 0),
        _ => None,
    }
}

/// The control hosts' widget ops (lobbyctl.rs's game mode; `ctl` the C++ control the op is for); `None`: not one,
/// or no object.
pub(crate) fn ctl_op(ctl: isize, op: COp, a: isize, b: isize, c: isize) -> Option<isize> {
    let bi = b as i32 as isize;
    let on_ctl = matches!(
        op,
        COp::IsRootVisible
            | COp::SubscribeBroadcast
            | COp::RequestTimer
            | COp::FireEvent
            | COp::ClearTimerFlag
            | COp::BaseTimer
            | COp::BaseEvent
            | COp::BasePaint
            | COp::DialogBaseEvent
    );
    if on_ctl && ctl == 0 {
        return None; // a row's op has no control
    }
    match op {
        // the component members and Component's non-virtual calls
        COp::SetControlId => {
            m::set_control_id(a, b as i32);
            Some(0)
        }
        COp::ControlId => Some(m::control_id(a) as isize),
        COp::IsDisabled => Some(m::is_disabled(a) as isize),
        COp::RequestFocus => Some(zero(comp(a, K::RequestFocus, 0, 0))),
        COp::IsRootVisible => Some((comp(ctl, K::IsRootVisible, 0, 0) != 0) as isize),
        COp::SubscribeBroadcast => Some(zero(comp(ctl, K::Subscribe, (a != 0) as isize, 0))),
        COp::RequestTimer => Some(zero(comp(ctl, K::RequestTimer, (a != 0) as isize, 0))),
        COp::FireEvent => Some(zero(comp(ctl, K::FireEvent, a as i32 as isize, (b as i32 != 0) as isize))),
        COp::ClearTimerFlag => {
            m::set_timer(ctl, m::timer(ctl) & !2);
            Some(0)
        }
        COp::BaseEvent => Some(zero(comp(ctl, K::HandleEvent, a, 0))),
        COp::BasePaint => Some(zero(container(ctl)?.call(EeWonCtCall::Paint, a, 0, 0))),
        COp::DialogBaseEvent => Some(zero(dialog_obj(ctl)?.call(EeWonDlgCall::HandleComponentEvent, a, 0, 0))),
        COp::BaseTimer => Some((container(ctl)?.call(EeWonCtCall::TimerEvent, a as i32 as isize, 0, 0) != 0) as isize),
        // Button
        COp::ButtonIsChecked => {
            Some(((button(a)?.call(EeWonBtnCall::GetField, EeWonBtnField::Flags as isize, 0, 0) & 4) != 0) as isize)
        }
        COp::ButtonSetCheck => {
            Some(zero(button(a)?.call(EeWonBtnCall::SetCheck, (b != 0) as isize, (c != 0) as isize, 0)))
        }
        // InputBox
        COp::InputClear => {
            input(a)?.set_text(&[], b != 0);
            Some(0)
        }
        COp::InputSetText => {
            input(a)?.set_text(&text_units(b), c != 0);
            Some(0)
        }
        COp::InputAddHistory => {
            input(a)?.add_input_history(&text_units(b), true);
            Some(0)
        }
        COp::InputSelectRange => {
            input(a)?.select_range(b as i32, c as i32);
            Some(0)
        }
        // ComboBox
        COp::ComboGetSel => Some(combo(a)?.call(EeWonComboCall::GetSelItem, 0, 0, 0) as i32 as isize),
        COp::ComboSetSel => Some(zero(combo(a)?.call(EeWonComboCall::SetSelItem, bi, (c != 0) as isize, 0))),
        COp::ComboClear => Some(zero(combo(a)?.call(EeWonComboCall::Clear, (b != 0) as isize, 0, 0))),
        COp::ComboDeleteItem => Some(zero(combo(a)?.call(EeWonComboCall::DeleteItem, bi, 0, 0))),
        // ListArea
        COp::ListGetSel => list_call(a, L::SelItem, 0, 0),
        COp::ListNumItems => list_call(a, L::NumItems, 0, 0).map(|r| r as i32 as isize),
        COp::ListItem => list_call(a, L::GetItem, bi, 0),
        COp::ListRefresh => list_call(a, L::RefreshItem, b, 0).map(|_| 0),
        COp::ListInsert | COp::ListInsert2 => list_call(a, L::InsertItem, b, -1).map(|_| 0),
        COp::ListRemove => list_call(a, L::RemoveItem, b, 0).map(|_| 0),
        COp::ListClear => list_call(a, L::Clear, (b != 0) as isize, 0).map(|_| 0),
        COp::ListBeginUpdate => list_call(a, L::BeginUpdate, 0, 0).map(|_| 0),
        COp::ListEndUpdate => list_call(a, L::EndUpdate, 0, 0).map(|_| 0),
        COp::ListEnsureVisible => list_call(a, L::EnsureVisible, b, 0).map(|_| 0),
        COp::ListItemPos => list_call(a, L::GetItemPos, b, 0).map(|r| r as i32 as isize),
        COp::ListSetSel => list_call(a, L::SetSelItemAt, bi, (c != 0) as isize).map(|_| 0),
        COp::ListItemAtY => list_call(a, L::GetItemPosAtYPos, bi, 0).map(|r| r as i32 as isize),
        COp::ListPaintWidth => list_call(a, L::PaintWidth, 0, 0).map(|r| r as i32 as isize),
        COp::ListFlagSet => list_call(a, L::ListFlags, 0, 0).map(|r| ((r & bi) != 0) as isize),
        // ScrollArea
        COp::ListVertOffset => {
            Some(pane(a)?.call(EeWonPaneCall::GetField, EeWonGuiField::VertOffset as isize, 0, 0) as i32 as isize)
        }
        COp::ListSetVertOffset => Some(zero(pane(a)?.call(EeWonPaneCall::SetVertOffset, bi, 0, 0))),
        COp::ListViewHeight => {
            Some(pane(a)?.call(EeWonPaneCall::GetField, EeWonGuiField::ScrollHeight as isize, 0, 0) as i32 as isize)
        }
        COp::ListTotalWidth => {
            Some(pane(a)?.call(EeWonPaneCall::GetField, EeWonGuiField::TotalWidth as isize, 0, 0) as i32 as isize)
        }
        // MultiListArea
        COp::ColumnWidth => Some(multi(a)?.call(EeWonMultiCall::ColumnWidth, bi, 0, 0) as i32 as isize),
        COp::ListPaintColumn => Some(multi(a)?.call(EeWonMultiCall::PaintContext, 0, 0, 0) as i32 as isize),
        COp::ListSortColumn => Some(multi(a)?.call(EeWonMultiCall::SortColumn, 0, 0, 0) as i32 as isize),
        COp::ListSortColumnPrev => Some(multi(a)?.call(EeWonMultiCall::SortColumnPrev, 0, 0, 0) as i32 as isize),
        // TreeArea and its rows
        COp::TreeInsertChild => Some(tree(a)?.call(EeWonTreeCall::InsertTreeItem, b, c, -1)),
        COp::TreeInsertServer => Some(tree(a)?.call(EeWonTreeCall::InsertTreeItem, 0, b, c as i32 as isize)),
        COp::TreeInsertLoading => Some(tree(a)?.call(EeWonTreeCall::InsertTreeItem, b, c, 0)),
        COp::TreeOpen => Some(zero(tree(a)?.call(EeWonTreeCall::OpenItem, b, (c != 0) as isize, 0))),
        COp::TreeRemoveAll => Some(zero(tree(a)?.call(EeWonTreeCall::RemoveAllItems, b, 0, 0))),
        COp::TreeRemove => Some(zero(tree(a)?.call(EeWonTreeCall::RemoveTreeItem, b, 0, 0))),
        COp::TreeClear => Some(zero(tree(a)?.call(EeWonTreeCall::RemoveAllItems, 0, 0, 0))),
        COp::TreeWrapped => Some(node(a)?.call(None, EeWonTNodeCall::Item, 0, 0, 0)),
        COp::TreeChildCount => Some(node(a)?.call(None, EeWonTNodeCall::NumChildren, 0, 0, 0) as i32 as isize),
        COp::TreeChild => Some(node(a)?.call(None, EeWonTNodeCall::GetChild, bi, 0, 0)),
        COp::TreeParent => Some(node(a)?.call(None, EeWonTNodeCall::Parent, 0, 0, 0)),
        COp::TreeIndex => Some(node(a)?.call(None, EeWonTNodeCall::Index, 0, 0, 0) as i32 as isize),
        COp::TreeIsOpen => Some((node(a)?.call(None, EeWonTNodeCall::IsOpen, 0, 0, 0) != 0) as isize),
        COp::TreeContainsPoint => Some(
            (node(a)?.call(None, EeWonTNodeCall::ContainsPoint, b as i32 as isize, (b >> 32) as i32 as isize, 0) != 0)
                as isize,
        ),
        _ => None,
    }
}

/// The options popups' widget ops (lobbypopup.rs's game mode; `popup` the C++ popup); `None`: not one, or no object.
pub(crate) fn popup_op(popup: isize, op: POp, a: isize, b: isize) -> Option<isize> {
    match op {
        // `aPopup->AddChild(a)`, `aPopup->Container::HandleComponentEvent(e)` (Component's), `aPopup->SetSize(a, b)`
        POp::AddChild => Some(zero(container(popup)?.call(EeWonCtCall::AddChild, a, 0, 0))),
        POp::BaseEvent => Some(zero(comp(popup, K::HandleEvent, a, 0))),
        POp::SetSize => Some(zero(comp(popup, K::SetSize, a as i32 as isize, b as i32 as isize))),
        POp::Width => Some(m::width(a) as isize),
        POp::Height => Some(m::height(a) as isize),
        // `aMenu->Clear(false)`: ListArea's
        POp::MenuClear => list_call(a, L::Clear, 0, 0).map(|_| 0),
        _ => None,
    }
}

#[cfg(test)]
#[path = "widgets_tests.rs"]
mod tests;
