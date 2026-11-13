//! WONGUI's ComboBox as a whole Rust object (R6 WON round 28): the lobby's drop-down lists (the game options, the
//! player options, the lobby options' LAN name and tab choice, the server list's filters). The original's
//! ComboBox.cpp: whether the list is open, no-typing, whether the button shows while disabled, the list box's
//! configured size, with every method.
//!
//! The C++ `ComboBox` stays the type its skins (MSComboBox, WONComboBox), configs and the lobby use (its vtable their
//! overrides); its methods are one-line forwarders. Its parts (the input box, the drop button, the list box and its
//! list area) and the undo text stay C++ members the skins make and read; the Rust object reaches them through the
//! combo ops (`ComboPart` and the list, input box and popup ops). It holds the container's Rust object (made by the
//! base's constructor, freed after it) and calls it for every `Container::X` the original made; every virtual the
//! original called (the list area's KeyDown and MouseWheel, the edit box's RequestFocus, its own SetFocusChild) goes
//! through the C++ object.

use std::cell::RefCell;
use std::ffi::c_void;

use crate::ctobj::{EeWonContainer, EeWonCtField};
use crate::guictl;
use crate::guihost::{EeWonCtVirt as V, EeWonGuiEvent, EeWonGuiHostFn, EeWonGuiOp as Op};

const FLAG_WANT_FOCUS: isize = 0x8;
const FLAG_DISABLED: isize = 0x80;
const BUTTON_ACTIVATE_ON_PRESS: isize = 0x40;
const LIST_POPUP: isize = 0x1000;
const EVENT_INPUT_RETURN: isize = 20;
const EVENT_INPUT_TEXT_CHANGED: isize = 21;
const EVENT_LIST_ITEM_CLICKED: isize = 23;
const EVENT_END_POPUP: isize = 30;

/// The combo box's parts (`ComboPart`'s argument).
const PART_EDIT: isize = 0;
const PART_BUTTON: isize = 1;
const PART_LIST_BOX: isize = 2;
const PART_LIST_AREA: isize = 3;

/// The inward calls (the C++ shell's virtuals and methods).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonComboCall {
    /// `Virtual08` (SizeChanged).
    SizeChanged = 1,
    /// `MouseDown(a, b, c)`.
    MouseDown,
    /// `MouseWheel(a)` (0 or 1).
    MouseWheel,
    GotFocus,
    LostFocus,
    /// `KeyDown(a)` (0 or 1).
    KeyDown,
    AddedToParent,
    /// `HandleComponentEvent(a)` (a `ComponentEvent*`).
    HandleComponentEvent,
    /// `SetEnabled(a != 0)`.
    SetEnabled,
    /// `AddString(*a, b)` (`a` a `const GUIString*`): the item, or 0.
    AddString,
    /// `InsertItem(a, b)` (a `TextListItem*`).
    InsertItem,
    /// `DeleteItem(a)`.
    DeleteItem,
    /// `UpdateItem(a)` (a `TextListItem*`).
    UpdateItem,
    /// `Clear(a != 0)`.
    Clear,
    /// `GetSelItem()`.
    GetSelItem,
    /// `SetSelItem(a, b != 0)`.
    SetSelItem,
    /// `SetListBox(a)` (a `Scroller*` as a `Component*`).
    SetListBox,
    AddControls,
    CalcListBoxSize,
    /// `SetListBoxDimensions(a, b)`.
    SetListBoxDimensions,
    /// `SetListBoxWidth(a)`.
    SetListBoxWidth,
    /// `SetListBoxHeight(a)`.
    SetListBoxHeight,
    /// `SetInputBoxToListSelection(a != 0, b != 0)`.
    SetInputBoxToListSelection,
    /// `SetText(*a, b != 0)` (`a` a `const GUIString*`).
    SetText,
    /// `SetTextAndSel(*a, b != 0)`.
    SetTextAndSel,
    ShowList,
    /// `SetNoType(a != 0)`.
    SetNoType,
    /// the field `a` (`EeWonComboField`).
    GetField,
    /// the field `a` = `b`.
    SetField,
}

/// The fields a config sets and the tests read.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonComboField {
    ListBoxVisible = 1,
    NoType,
    ShowButtonWhenDisabled,
    ListBoxWidth,
    ListBoxHeight,
}

#[derive(Debug)]
struct State {
    visible: bool,
    no_type: bool,
    show_button_when_disabled: bool,
    width: i32,
    height: i32,
}

/// The Rust ComboBox.
pub struct EeWonComboBox {
    comp: *mut c_void,
    host: EeWonGuiHostFn,
    ct: *const EeWonContainer,
    st: RefCell<State>,
}

impl EeWonComboBox {
    /// The constructor's state: the list closed, typing allowed, the button shown while disabled, a 200 x 150 list
    /// box.
    pub fn new(comp: *mut c_void, host: EeWonGuiHostFn, ct: *const EeWonContainer) -> Self {
        EeWonComboBox {
            comp,
            host,
            ct,
            st: RefCell::new(State {
                visible: false,
                no_type: false,
                show_button_when_disabled: true,
                width: 200,
                height: 150,
            }),
        }
    }

    fn op(&self, op: Op, a: isize, b: isize, c: isize, d: isize) -> isize {
        match self.host {
            // SAFETY: the host was given with this object's component; no borrow of the state is live here
            Some(h) => unsafe { h(self.comp, op, a, b, c, d) },
            None => 0,
        }
    }
    fn op1(&self, op: Op, a: isize) -> isize {
        self.op(op, a, 0, 0, 0)
    }
    fn ct(&self) -> &EeWonContainer {
        // SAFETY: the base's Rust object outlives this one (made before it, freed after it)
        unsafe { &*self.ct }
    }
    fn me(&self) -> isize {
        self.comp as isize
    }
    fn part(&self, k: isize) -> isize {
        self.op1(Op::ComboPart, k)
    }
    fn visible(&self) -> bool {
        self.st.borrow().visible
    }
    fn set_visible(&self, v: bool) {
        self.st.borrow_mut().visible = v;
    }
    fn close_list(&self) {
        self.set_visible(false);
        self.op1(Op::ClosePopup, self.part(PART_LIST_BOX));
    }
    fn click_button(&self) {
        self.op1(Op::DlgClick, self.part(PART_BUTTON));
    }
    /// The first item whose text is `text` (0: the input box's), as the original's `ComboMatchOf`.
    fn first_match(&self, text: isize) -> Option<usize> {
        let count = self.op1(Op::ComboItemCount, 0) as usize;
        guictl::first_match(count, |i| {
            let item = self.op1(Op::ComboItemAt, i as isize);
            item != 0 && self.op(Op::ComboItemTextIs, item, text, 0, 0) != 0
        })
    }

    pub fn add_string(&self, text: isize, pos: i32) -> isize {
        if self.part(PART_LIST_AREA) != 0 {
            let item = self.op(Op::ComboInsertString, text, pos as isize, 0, 0);
            self.calc_list_box_size();
            return item;
        }
        0
    }

    pub fn insert_item(&self, item: isize, pos: i32) {
        if self.part(PART_LIST_AREA) != 0 {
            self.op(Op::ComboInsertItem, item, pos as isize, 0, 0);
            self.calc_list_box_size();
        } else {
            self.op1(Op::ComboDropItem, item);
        }
    }

    pub fn delete_item(&self, pos: i32) {
        if self.part(PART_LIST_AREA) != 0 {
            self.op1(Op::ComboDeleteItem, pos as isize);
            self.calc_list_box_size();
        }
    }

    pub fn update_item(&self, item: isize) {
        if self.part(PART_LIST_AREA) != 0 {
            self.op1(Op::ComboRefreshItem, item);
        }
    }

    pub fn clear(&self, clear_input: bool) {
        self.op1(Op::ComboListClear, 0);
        if clear_input {
            self.op1(Op::ComboEditClear, 1);
        }
        self.calc_list_box_size();
    }

    pub fn sel_item(&self) -> i32 {
        let item = self.op1(Op::ComboSelItem, 0);
        if item == 0 {
            -1
        } else {
            self.op1(Op::ComboItemPos, item) as i32
        }
    }

    pub fn set_sel_item(&self, pos: i32, notify: bool) {
        if self.part(PART_LIST_AREA) == 0 {
            return;
        }
        self.op(Op::ComboSetSel, pos as isize, 1, 0, 0);
        if self.op1(Op::ComboSelItem, 0) == 0 {
            self.op1(Op::ComboEditClear, notify as isize);
        } else {
            self.set_input_box_to_list_selection(false, notify);
        }
    }

    pub fn set_list_box(&self, list_box: isize) {
        self.op1(Op::ComboSetListBox, list_box);
        // the original read the new box's scroll area before testing it for NULL (a NULL box crashed)
        if self.part(PART_LIST_AREA) != 0
            && list_box != 0
            && self.op1(Op::ComboBoxArea, list_box) != self.part(PART_LIST_AREA)
        {
            self.op1(Op::ComboSetScroller, 0);
        }
    }

    pub fn added_to_parent(&self) {
        if self.op1(Op::CtParentOf, self.part(PART_EDIT)) != self.me() {
            self.add_controls();
        }
        self.ct().added_to_parent();
    }

    pub fn add_controls(&self) {
        self.ct().add_child(self.part(PART_EDIT));
        self.ct().add_child(self.part(PART_BUTTON));
        self.op(Op::ComboSetListFlags, LIST_POPUP, 1, 0, 0);
        self.op(Op::ChildSetFlags, self.part(PART_LIST_AREA), FLAG_WANT_FOCUS, 0, 0);
        self.op1(Op::ListenTo, self.part(PART_LIST_BOX));
        self.op(Op::ButtonSetFlags, self.part(PART_BUTTON), BUTTON_ACTIVATE_ON_PRESS, 1, 0);
        self.op(Op::ChildSetFlags, self.part(PART_BUTTON), FLAG_WANT_FOCUS, 0, 0);
    }

    pub fn calc_list_box_size(&self) {
        if self.visible() {
            return;
        }
        let (mw, mh) = {
            let s = self.st.borrow();
            (s.width, s.height)
        };
        let list_box = self.part(PART_LIST_BOX);
        let box_width = self.op1(Op::ChildWidth, list_box) as i32;
        let area_width = self.op1(Op::ChildWidth, self.part(PART_LIST_AREA)) as i32;
        let metric = |k: isize| self.op1(Op::ComboListMetric, k) as i32;
        let (right, left, total_w, total_h) = (metric(0), metric(1), metric(2), metric(3));
        let combo_width = self.op1(Op::ChildWidth, self.me()) as i32;
        let (w, h) = guictl::combo_list_size(mw, mh, box_width, area_width, right, left, total_w, combo_width, total_h);
        self.op(Op::ChildSetSize, self.part(PART_LIST_BOX), w as isize, h as isize, 0);
    }

    pub fn size_changed(&self) {
        self.ct().size_changed();
        self.calc_list_box_size();
    }

    pub fn set_list_box_dimensions(&self, w: Option<i32>, h: Option<i32>) {
        {
            let mut s = self.st.borrow_mut();
            if let Some(w) = w {
                s.width = w;
            }
            if let Some(h) = h {
                s.height = h;
            }
        }
        self.calc_list_box_size();
    }

    pub fn got_focus(&self) {
        self.ct().got_focus();
        if self.ct().field(EeWonCtField::FocusChild as i32) == 0 {
            self.op(Op::CtChild, self.part(PART_EDIT), V::RequestFocus as isize, 0, 0);
        }
    }

    pub fn set_input_box_to_list_selection(&self, active: bool, fire: bool) {
        let item = self.op1(Op::ComboSelItem, 0);
        if item == 0 {
            return;
        }
        self.op1(Op::ComboEditItemText, item);
        let item_width = self.op1(Op::ComboItemWidth, item) as i32;
        let edit_width = self.op1(Op::ChildWidth, self.part(PART_EDIT)) as i32;
        if guictl::combo_select_to_end(item_width, edit_width) {
            let len = self.op1(Op::ComboItemTextLength, item);
            self.op(Op::ComboEditSelect, len, 0, 0, 0);
        } else {
            self.op(Op::ComboEditSelect, 0, -1, 0, 0);
        }
        if fire {
            self.op(Op::ComboDeliverSel, item, active as isize, 0, 0);
        }
    }

    pub fn set_text(&self, text: isize, notify: bool) {
        self.op(Op::ComboEditSetText, text, notify as isize, 0, 0);
        self.op1(Op::Invalidate, 0);
    }

    pub fn set_text_and_sel(&self, text: isize, notify: bool) {
        if self.part(PART_LIST_AREA) != 0 {
            if let Some(i) = self.first_match(text) {
                self.set_sel_item(i as i32, notify);
                return;
            }
        }
        self.set_text(text, notify);
    }

    pub fn show_list(&self) {
        if self.visible() {
            return;
        }
        self.calc_list_box_size();
        let selected = self.op1(Op::ComboSelItem, 0);
        if selected != 0 {
            if self.op(Op::ComboItemTextIs, selected, 0, 0, 0) == 0 {
                match self.first_match(0) {
                    Some(i) => {
                        self.op(Op::ComboSetSel, i as isize, 0, 0, 0);
                        let item = self.op1(Op::ComboItemAt, i as isize);
                        self.op1(Op::ComboEnsureVisible, item);
                    }
                    None => {
                        self.op(Op::ComboSetSel, -1, 0, 0, 0);
                    }
                }
            } else {
                self.op1(Op::ComboEnsureVisible, selected);
            }
        }
        self.op1(Op::ComboOpenPopup, 0);
        self.set_visible(true);
    }

    pub fn handle_component_event(&self, event: isize) {
        if self.op1(Op::EventComponent, event) == self.part(PART_BUTTON) {
            if !self.visible() {
                self.op1(Op::ComboUndoSave, 0);
                self.show_list();
                return;
            }
            self.close_list();
            return;
        }
        let kind = self.op1(Op::EventType, event);
        if kind == EVENT_LIST_ITEM_CLICKED {
            if self.op1(Op::ComboEventButton, event) != 0 {
                return;
            }
            self.set_input_box_to_list_selection(true, true);
            self.close_list();
            return;
        }
        if kind == EVENT_END_POPUP {
            self.set_visible(false);
            return;
        }
        if kind == EVENT_INPUT_TEXT_CHANGED || kind == EVENT_INPUT_RETURN {
            // the input box's events fired again as the combo box's own
            let e =
                if kind == EVENT_INPUT_RETURN { EeWonGuiEvent::InputReturn } else { EeWonGuiEvent::InputTextChanged };
            self.op(Op::FireEvent, e as isize, 1, 0, 0);
            return;
        }
        self.op1(Op::ComponentHandleEvent, event);
    }

    pub fn mouse_down(&self, x: i32, y: i32, button: i32) {
        self.ct().mouse_down(x, y, button);
        if button == 0 {
            let mouse = self.ct().field(EeWonCtField::MouseChild as i32);
            if mouse == self.part(PART_EDIT) && self.st.borrow().no_type {
                self.click_button();
            }
            self.op1(Op::RequestFocus, 0);
        }
    }

    pub fn mouse_wheel(&self, amount: i32) -> bool {
        self.visible()
            && self.op(Op::CtChild, self.part(PART_LIST_AREA), V::MouseWheel as isize, amount as isize, 0) != 0
    }

    pub fn key_down(&self, key: i32) -> bool {
        match guictl::combo_key(key, self.visible()) {
            1 => {
                self.op1(Op::ComboUndoSave, 0);
                self.set_input_box_to_list_selection(false, true);
                self.show_list();
                true
            }
            2 => {
                self.op(Op::CtChild, self.part(PART_LIST_AREA), V::KeyDown as isize, key as isize, 0);
                self.set_input_box_to_list_selection(false, true);
                true
            }
            3 => {
                self.set_input_box_to_list_selection(true, true);
                self.click_button();
                true
            }
            4 => {
                self.op1(Op::ComboEditUndo, 0);
                self.click_button();
                true
            }
            _ => self.ct().key_down(key),
        }
    }

    pub fn set_no_type(&self, no_type: bool) {
        if no_type == self.st.borrow().no_type {
            return;
        }
        self.st.borrow_mut().no_type = no_type;
        self.op(Op::ChildSetFlags, self.part(PART_EDIT), FLAG_WANT_FOCUS, (!no_type) as isize, 0);
        self.op1(Op::ComboEditAllowSelection, (!no_type) as isize);
        if no_type && self.ct().field(EeWonCtField::FocusChild as i32) == self.part(PART_EDIT) {
            self.op(Op::CtVirt, V::SetFocusChild as isize, 0, 0, 0);
        }
    }

    pub fn set_enabled(&self, enabled: bool) {
        let disabled = self.op1(Op::CtBits, self.me()) & FLAG_DISABLED != 0;
        if enabled == !disabled {
            return;
        }
        self.op1(Op::BaseSetEnabled, enabled as isize);
        self.op(Op::ChildSetEnabled, self.part(PART_EDIT), enabled as isize, 0, 0);
        self.op(Op::ChildSetEnabled, self.part(PART_BUTTON), enabled as isize, 0, 0);
        if !enabled && self.visible() {
            self.close_list();
        }
        if !self.st.borrow().show_button_when_disabled {
            self.op(Op::ChildSetVisible, self.part(PART_BUTTON), enabled as isize, 0, 0);
            self.op1(Op::ComboRelayout, 0);
        }
    }

    pub fn field(&self, f: i32) -> isize {
        type F = EeWonComboField;
        let s = self.st.borrow();
        match f {
            x if x == F::ListBoxVisible as i32 => s.visible as isize,
            x if x == F::NoType as i32 => s.no_type as isize,
            x if x == F::ShowButtonWhenDisabled as i32 => s.show_button_when_disabled as isize,
            x if x == F::ListBoxWidth as i32 => s.width as isize,
            x if x == F::ListBoxHeight as i32 => s.height as isize,
            _ => 0,
        }
    }

    pub fn set_field(&self, f: i32, v: isize) {
        if f == EeWonComboField::ShowButtonWhenDisabled as i32 {
            self.st.borrow_mut().show_button_when_disabled = v != 0;
        }
    }

    /// The destructor's statement: the combo box leaves its list box's listeners.
    pub fn destroy(&self) {
        if self.part(PART_LIST_BOX) != 0 {
            self.op1(Op::ComboUnlisten, 0);
        }
    }

    pub fn call(&self, call: EeWonComboCall, a: isize, b: isize, c: isize) -> isize {
        type C = EeWonComboCall;
        let (ai, bi, ci) = (a as i32, b as i32, c as i32);
        match call {
            C::SizeChanged => self.size_changed(),
            C::MouseDown => self.mouse_down(ai, bi, ci),
            C::MouseWheel => return self.mouse_wheel(ai) as isize,
            C::GotFocus => self.got_focus(),
            C::LostFocus => self.ct().lost_focus(),
            C::KeyDown => return self.key_down(ai) as isize,
            C::AddedToParent => self.added_to_parent(),
            C::HandleComponentEvent => self.handle_component_event(a),
            C::SetEnabled => self.set_enabled(a != 0),
            C::AddString => return self.add_string(a, bi),
            C::InsertItem => self.insert_item(a, bi),
            C::DeleteItem => self.delete_item(ai),
            C::UpdateItem => self.update_item(a),
            C::Clear => self.clear(a != 0),
            C::GetSelItem => return self.sel_item() as isize,
            C::SetSelItem => self.set_sel_item(ai, b != 0),
            C::SetListBox => self.set_list_box(a),
            C::AddControls => self.add_controls(),
            C::CalcListBoxSize => self.calc_list_box_size(),
            C::SetListBoxDimensions => self.set_list_box_dimensions(Some(ai), Some(bi)),
            C::SetListBoxWidth => self.set_list_box_dimensions(Some(ai), None),
            C::SetListBoxHeight => self.set_list_box_dimensions(None, Some(ai)),
            C::SetInputBoxToListSelection => self.set_input_box_to_list_selection(a != 0, b != 0),
            C::SetText => self.set_text(a, b != 0),
            C::SetTextAndSel => self.set_text_and_sel(a, b != 0),
            C::ShowList => self.show_list(),
            C::SetNoType => self.set_no_type(a != 0),
            C::GetField => return self.field(ai),
            C::SetField => self.set_field(ai, b),
        }
        0
    }
}

/// A new Rust ComboBox for the C++ `component` over its container's Rust object `ct`, its calls out through `host`.
///
/// # Safety
/// `ct` is the component's live container object and outlives the combo box's.
#[no_mangle]
pub unsafe extern "C" fn ee_won_combo_new(
    component: *mut c_void,
    host: EeWonGuiHostFn,
    ct: *const EeWonContainer,
) -> *mut EeWonComboBox {
    Box::into_raw(Box::new(EeWonComboBox::new(component, host, ct)))
}

/// The destructor's statement (the list box's listener dropped), then the object freed.
///
/// # Safety
/// `combo` came from `ee_won_combo_new` (or is NULL) and is not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_won_combo_free(combo: *mut EeWonComboBox) {
    if !combo.is_null() {
        // SAFETY: the contract
        unsafe {
            (*combo).destroy();
            drop(Box::from_raw(combo));
        }
    }
}

/// One of ComboBox's methods (`EeWonComboCall`).
///
/// # Safety
/// `combo` is live; the call's arguments as documented.
#[no_mangle]
pub unsafe extern "C" fn ee_won_combo_call(
    combo: *const EeWonComboBox,
    call: EeWonComboCall,
    a: isize,
    b: isize,
    c: isize,
) -> isize {
    // SAFETY: live (the contract); a shared reference: the calls out re-enter through this function
    unsafe { (*combo).call(call, a, b, c) }
}

#[cfg(test)]
#[path = "comboobj_tests.rs"]
mod tests;
