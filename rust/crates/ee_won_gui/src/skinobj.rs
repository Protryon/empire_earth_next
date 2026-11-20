//! The WON skin (R6 WONLobby round 23): `WONSkin` and `WONMSSkin` whole in Rust. The skin holds the template widgets
//! (the WON skin's kinds, shell_won.rs, and C++ `WONButton`s) the configs' `WONSkin` group sets and the resource
//! table's factories duplicate, and the two named button type maps (case-insensitive, `GUIStringLessNoCase`); the
//! Windows look's constructor (`WONMSSkin`) builds every template from drawn `MSImage`s. The statements are those of
//! WONSkinDefaultGetters.cpp, SkinSelectionAdapters.cpp, WONSkinComposition.cpp, WONSkinListComposition.cpp,
//! WONSkinCopies.cpp, WONSkinConfig.cpp and WONMSSkinComposition.cpp, in their order.
//!
//! The game has one skin (the original's file-static `gSkin`), made by the first `WONSkin` group's PreParse and never
//! freed (a template tab control and the header controls held it: it was never freed either). Its calls out are the
//! shell host's ops with no shell (`Sh` over no widget).

use std::cell::{Cell, RefCell};
use std::ffi::c_void;
use std::sync::atomic::{AtomicPtr, Ordering};

use crate::barobj::{EeWonBarCall as Bar, EeWonScrollbar};
use crate::btnobj::EeWonBtnCall as Bc;
use crate::comboobj::EeWonComboCall as Co;
use crate::compobj::EeWonCompCall as Cc;
use crate::guistring::compare_no_case;
use ee_won_net::chat::CrtChat;
use crate::multiobj::EeWonMHeadCall as Mh;
use crate::registry::EeWonGuiKind;
use crate::scrollerobj::EeWonScrollerCall as Sc;
use crate::shell::{
    EeWonShellBaseNew as N, EeWonShellComboPart as Cp, EeWonShellHostFn, EeWonShellHostOp as H, EeWonShellKind as K,
    EeWonShellOp as O, Sh,
};
use crate::shell_won::{hold_comp, EeWonFrameIn};
use crate::tabobj::EeWonTabCall as Tb;
use crate::wonbtnobj::EeWonWbField as Wf;

/// A template slot of the skin (`WONSkin`'s members in declaration order).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonSkinSlot {
    PushButton = 0,
    Checkbox,
    RadioButton,
    HeaderButton,
    TabButton,
    CloseButton,
    ComboButton,
    ScrollThumb0,
    ScrollThumb1,
    ScrollGutter0,
    ScrollGutter1,
    Arrow0,
    Arrow1,
    Arrow2,
    Arrow3,
    Scrollbar0,
    Scrollbar1,
    InputBox,
    ComboBox,
    ScrollContainer,
    ListCtrl,
    TabCtrl,
    PopupBack,
    DialogBack,
}
const SLOTS: usize = EeWonSkinSlot::DialogBack as usize + 1;

/// A getter of the skin (`ee_won_skin_get`; `a` its argument).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonSkinGet {
    PushButton = 1,
    Checkbox,
    RadioButton,
    TabButton,
    HeaderButton,
    CloseButton,
    InputBox,
    PopupBack,
    DialogBack,
    /// `GetArrowButton(a)` (a `Direction`).
    ArrowButton,
    /// `GetScrollThumb(a != 0)`.
    ScrollThumb,
    /// `GetScrollGutter(a != 0)`.
    ScrollGutter,
    /// `GetScrollbar(a != 0)`.
    Scrollbar,
    ComboBox,
    ScrollContainer,
    ListCtrl,
    TabCtrl,
}

/// `Direction_Left`, `_Right`, `_Up`, `_Down`.
const DIR_LEFT: isize = 0;
const DIR_RIGHT: isize = 1;
const DIR_UP: isize = 2;
const DIR_DOWN: isize = 3;

/// `WONMSSkin::MSImageType_*` and `MSImageFlag_*`.
mod ms {
    pub(super) const FRAME3: isize = 2;
    pub(super) const LEFT_ARROW: isize = 3;
    pub(super) const DOWN_ARROW: isize = 6;
    pub(super) const PUSH_BUTTON: isize = 7;
    pub(super) const CHECKBOX: isize = 8;
    pub(super) const RADIO_BUTTON: isize = 9;
    pub(super) const SCROLL_THUMB: isize = 10;
    pub(super) const SCROLL_GUTTER: isize = 11;
    pub(super) const SCROLL_CORNER: isize = 12;
    pub(super) const LIST_HEADER: isize = 13;
    pub(super) const TAB_BUTTON: isize = 14;
    pub(super) const TAB_CTRL_FRAME: isize = 15;
    pub(super) const COMBO_POPUP_FRAME: isize = 16;
    pub(super) const CLOSE_BUTTON: isize = 17;
    pub(super) const DOWN: isize = 1;
    pub(super) const DEFAULT: isize = 2;
    pub(super) const DISABLED: isize = 4;
    pub(super) const CHECKED: isize = 8;
}

/// A type map's entries in their order (`std::map<GUIString, SmartPtr<..>, GUIStringLessNoCase>`).
type TypeMap = RefCell<Vec<(Vec<u32>, Cell<isize>)>>;

/// The skin.
pub struct EeWonSkin {
    sh: Sh,
    slots: [Cell<isize>; SLOTS],
    button_types: TypeMap,
    simple_types: TypeMap,
}

impl EeWonSkin {
    fn slot(&self, s: EeWonSkinSlot) -> isize {
        self.slots[s as usize].get()
    }
    fn slot_at(&self, first: EeWonSkinSlot, i: isize) -> isize {
        self.slots[first as usize + i as usize].get()
    }
    /// A template's `SmartPtr` assignment.
    pub fn set(&self, s: usize, c: isize) {
        if s < SLOTS {
            hold_comp(&self.sh, &self.slots[s], c);
        }
    }
    /// `map[key] = c` (the entry made empty first when new).
    pub fn set_type(&self, simple: bool, key: Vec<u32>, c: isize) {
        let map = if simple { &self.simple_types } else { &self.button_types };
        let at = {
            let mut m = map.borrow_mut();
            match m.binary_search_by(|(k, _)| compare_no_case(k, &key, &CrtChat).cmp(&0)) {
                Ok(i) => i,
                Err(i) => {
                    m.insert(i, (key, Cell::new(0)));
                    i
                }
            }
        };
        let cell = Cell::new(map.borrow()[at].1.get());
        hold_comp(&self.sh, &cell, c);
        map.borrow()[at].1.set(cell.get());
    }
    fn find_type(&self, simple: bool, key: &[u32]) -> Option<isize> {
        let map = if simple { &self.simple_types } else { &self.button_types };
        let m = map.borrow();
        m.binary_search_by(|(k, _)| compare_no_case(k, key, &CrtChat).cmp(&0)).ok().map(|i| m[i].1.get())
    }
    /// `CopyButtonType(type, copyTo)` / `CopySimpleButtonType`: the named type's attributes onto the button; false
    /// when there is none.
    pub fn copy_type(&self, simple: bool, key: &[u32], to: isize) -> bool {
        let Some(from) = self.find_type(simple, key) else { return false };
        if simple {
            self.sh.host(H::BtnCopy, to, from, 0, 0);
        } else {
            self.sh.wb_copy(to, from);
        }
        true
    }
    /// `WONSkinConfig::HandleButtonClickSound`: the sound on the template buttons and every named button type.
    pub fn click_sound(&self, sound: isize) {
        use EeWonSkinSlot as S;
        for s in [S::PushButton, S::Checkbox, S::RadioButton, S::HeaderButton, S::TabButton, S::CloseButton, S::ComboButton]
        {
            let c = self.slot(s);
            if c != 0 {
                self.sh.host(H::ClickSound, c, sound, 0, 0);
            }
        }
        let all: Vec<isize> = self.button_types.borrow().iter().map(|(_, c)| c.get()).collect();
        for c in all {
            if c != 0 {
                self.sh.host(H::ClickSound, c, sound, 0, 0);
            }
        }
    }

    // ---- the getters (each a new widget, not held)
    pub fn get_push_button(&self) -> isize {
        self.sh.wb_duplicate(self.slot(EeWonSkinSlot::PushButton))
    }
    pub fn get_checkbox(&self) -> isize {
        self.sh.wb_duplicate(self.slot(EeWonSkinSlot::Checkbox))
    }
    pub fn get_radio_button(&self) -> isize {
        self.sh.wb_duplicate(self.slot(EeWonSkinSlot::RadioButton))
    }
    /// (`_sh` the caller's view: the calls are the same host's)
    pub fn get_tab_button(&self, _sh: &Sh) -> isize {
        self.sh.wb_duplicate(self.slot(EeWonSkinSlot::TabButton))
    }
    pub fn get_header_button(&self, _sh: &Sh) -> isize {
        self.sh.shell_on(self.slot(EeWonSkinSlot::HeaderButton), O::Duplicate, 0, 0)
    }
    pub fn get_close_button(&self) -> isize {
        self.sh.shell_on(self.slot(EeWonSkinSlot::CloseButton), O::Duplicate, 0, 0)
    }
    pub fn get_input_box(&self) -> isize {
        self.sh.shell_on(self.slot(EeWonSkinSlot::InputBox), O::Duplicate, 0, 0)
    }
    /// `new WONFrameComponent(back->GetFrame())`.
    fn get_back(&self, s: EeWonSkinSlot) -> isize {
        let mut f = EeWonFrameIn::default();
        self.sh.shell_on(self.slot(s), O::GetFrame, &mut f as *mut EeWonFrameIn as isize, 0);
        self.sh.host(H::New, K::WonFrameComponent as isize, &f as *const EeWonFrameIn as isize, 0, 0)
    }
    /// `GetArrowButton(direction)`: a copy with its desired size cleared.
    pub fn get_arrow_button(&self, dir: isize) -> isize {
        let b = self.sh.shell_on(self.slot_at(EeWonSkinSlot::Arrow0, dir & 3), O::Duplicate, 0, 0);
        self.sh.comp_on(b, Cc::SetDesiredSize, 0, 0);
        b
    }
    pub fn get_scroll_thumb(&self, vertical: bool) -> isize {
        self.sh.shell_on(self.slot_at(EeWonSkinSlot::ScrollThumb0, vertical as isize), O::Duplicate, 0, 0)
    }
    pub fn get_scroll_gutter(&self, vertical: bool) -> isize {
        self.sh.shell_on(self.slot_at(EeWonSkinSlot::ScrollGutter0, vertical as isize), O::Duplicate, 0, 0)
    }
    /// `GetScrollbar(vertical)`: from the template with its thumb, gutter and arrows, one arrow wide or high.
    pub fn get_scrollbar(&self, vertical: bool) -> isize {
        let sh = &self.sh;
        let bar = sh.host(H::New, K::WonScrollbar as isize, vertical as isize, 0, 0);
        let from = self.slot_at(EeWonSkinSlot::Scrollbar0, vertical as isize);
        sh.bar_on(bar, Bar::CopyAttributes, sh.obj_of::<EeWonScrollbar>(from, EeWonGuiKind::Bar), 0);
        sh.bar_on(bar, Bar::SetPart, 3, self.get_scroll_thumb(vertical));
        sh.bar_on(bar, Bar::SetPart, 2, self.get_scroll_gutter(vertical));
        if vertical {
            sh.bar_on(bar, Bar::SetPart, 0, self.get_arrow_button(DIR_UP));
            sh.bar_on(bar, Bar::SetPart, 1, self.get_arrow_button(DIR_DOWN));
            let up = sh.bar_on(bar, Bar::GetPart, 0, 0);
            sh.comp_on(bar, Cc::SetSize, crate::compmem::width(up) as isize, crate::compmem::height(bar) as isize);
        } else {
            sh.bar_on(bar, Bar::SetPart, 0, self.get_arrow_button(DIR_LEFT));
            sh.bar_on(bar, Bar::SetPart, 1, self.get_arrow_button(DIR_RIGHT));
            let up = sh.bar_on(bar, Bar::GetPart, 0, 0);
            sh.comp_on(bar, Cc::SetSize, crate::compmem::width(bar) as isize, crate::compmem::height(up) as isize);
        }
        bar
    }
    /// `SetFrame(WONFrame(image, border))` on `c`: the temporary frame holds the image over the call.
    fn set_frame_of(&self, c: isize, image: isize, border: i32) {
        let sh = &self.sh;
        sh.host(H::RefObj, image, 1, 0, 0);
        let f = EeWonFrameIn { left: border, top: border, right: border, bottom: border, image };
        sh.shell_on(c, O::SetFrame, &f as *const EeWonFrameIn as isize, 0);
        sh.host(H::RefObj, image, -1, 0, 0);
    }
    fn ms_image(&self, t: isize, f: isize, w: isize, h: isize) -> isize {
        self.sh.host(H::NewMsImage, t, f, w, h)
    }
    /// `GetComboBox()`: from the template with its arrow button and a framed popup list.
    pub fn get_combo_box(&self) -> isize {
        let sh = &self.sh;
        let combo = sh.shell_on(self.slot(EeWonSkinSlot::ComboBox), O::Duplicate, 0, 0);
        let arrow = sh.shell_on(self.slot(EeWonSkinSlot::ComboButton), O::Duplicate, 0, 0);
        sh.comp_on(arrow, Cc::SetDesiredSize, 0, 0);
        sh.btn_on(arrow, Bc::SetButtonFlags, 2, 0);
        let list = self.get_scroll_container();
        sh.scroller_on(list, Sc::SetScrollbarConditions, 0, 2); // Disable, Conditional
        let img = self.ms_image(ms::COMBO_POPUP_FRAME, 0, 0, 0);
        self.set_frame_of(list, img, 0);
        sh.host(H::ComboPartOn, combo, Cp::Button as isize, arrow, 0);
        sh.combo_on(combo, Co::SetListBox, list, 0);
        combo
    }
    /// `GetScrollContainer()`: from the template with skinned scrollbars.
    pub fn get_scroll_container(&self) -> isize {
        let sh = &self.sh;
        let c = sh.shell_on(self.slot(EeWonSkinSlot::ScrollContainer), O::DuplicateScroll, 0, 0);
        sh.scroller_on(c, Sc::SetScrollbar, self.get_scrollbar(true), 0);
        sh.scroller_on(c, Sc::SetScrollbar, self.get_scrollbar(false), 0);
        c
    }
    /// `GetTabCtrl()`: from the template, bound to the skin, with skinned arrows.
    pub fn get_tab_ctrl(&self) -> isize {
        let sh = &self.sh;
        let tab = sh.shell_on(self.slot(EeWonSkinSlot::TabCtrl), O::Duplicate, 0, 0);
        sh.shell_on(tab, O::SetSkin, self as *const EeWonSkin as isize, 0);
        let left = self.get_arrow_button(DIR_LEFT);
        let right = self.get_arrow_button(DIR_RIGHT);
        sh.tab_on(tab, Tb::SetTabArrows, left, right);
        tab
    }
    /// `GetListCtrl()`: from the template with a header, a multi-column list and scrollbars.
    pub fn get_list_ctrl(&self) -> isize {
        let sh = &self.sh;
        let list = sh.shell_on(self.slot(EeWonSkinSlot::ListCtrl), O::Duplicate, 0, 0);
        sh.shell_on(list, O::CopyScrollAttributes, self.slot(EeWonSkinSlot::ScrollContainer), 0);
        let header = sh.host(H::New, K::WonHeaderControl as isize, 0, self as *const EeWonSkin as isize, 0);
        sh.shell_on(list, O::SetHeader, header, 0);
        let area = sh.host(H::NewBase, N::MultiListArea as isize, 0, 0, 0);
        sh.scroller_on(list, Sc::SetScrollArea, area, 0);
        sh.scroller_on(list, Sc::SetScrollbar, self.get_scrollbar(true), 0);
        sh.scroller_on(list, Sc::SetScrollbar, self.get_scrollbar(false), 0);
        let header = sh.shell_on(list, O::GetHeader, 0, 0);
        sh.mhead_on(header, Mh::SetMultiListArea, area, 0);
        list
    }
    /// One getter.
    pub fn get(&self, g: EeWonSkinGet, a: isize) -> isize {
        use EeWonSkinGet as G;
        match g {
            G::PushButton => self.get_push_button(),
            G::Checkbox => self.get_checkbox(),
            G::RadioButton => self.get_radio_button(),
            G::TabButton => self.get_tab_button(&self.sh),
            G::HeaderButton => self.get_header_button(&self.sh),
            G::CloseButton => self.get_close_button(),
            G::InputBox => self.get_input_box(),
            G::PopupBack => self.get_back(EeWonSkinSlot::PopupBack),
            G::DialogBack => self.get_back(EeWonSkinSlot::DialogBack),
            G::ArrowButton => self.get_arrow_button(a),
            G::ScrollThumb => self.get_scroll_thumb(a != 0),
            G::ScrollGutter => self.get_scroll_gutter(a != 0),
            G::Scrollbar => self.get_scrollbar(a != 0),
            G::ComboBox => self.get_combo_box(),
            G::ScrollContainer => self.get_scroll_container(),
            G::ListCtrl => self.get_list_ctrl(),
            G::TabCtrl => self.get_tab_ctrl(),
        }
    }

    /// `WONMSSkin()`: the Windows look's templates drawn as MS images.
    fn ms_skin(&self) {
        use EeWonSkinSlot as S;
        let sh = &self.sh;
        let font = sh.host(H::GetNamedFont, c"MSButton".as_ptr() as isize, 0, 0, 0);
        let new_button = |s: S| {
            let b = sh.host(H::NewBase, N::WonButton as isize, 0, 0, 0);
            self.set(s as usize, b);
            self.slot(s)
        };
        let new_kind = |s: S, k: K| {
            let b = sh.host(H::New, k as isize, 0, 0, 0);
            self.set(s as usize, b);
            self.slot(s)
        };
        let img = |t: isize, f: isize, w: isize, h: isize| self.ms_image(t, f, w, h);
        let pads = |b: isize, l: i32, t: i32, r: i32, bo: i32| {
            sh.wb_set(b, Wf::LeftTextPad, l);
            sh.wb_set(b, Wf::TopTextPad, t);
            sh.wb_set(b, Wf::RightTextPad, r);
            sh.wb_set(b, Wf::BottomTextPad, bo);
        };
        let push = |b: isize, x: i32, y: i32| {
            sh.wb_set(b, Wf::PushDX, x);
            sh.wb_set(b, Wf::PushDY, y);
        };

        // Push Button
        let b = new_button(S::PushButton);
        sh.btn_on(b, Bc::SetButtonFlags, 0x0001, 0);
        sh.btn_on(b, Bc::SetButtonFlags, 0x0400, 1);
        sh.wb_set(b, Wf::DrawGrayText, 1);
        sh.wb_state(b, 0, img(ms::PUSH_BUTTON, 0, 0, 0), -1, font, false);
        sh.wb_state(b, 1, img(ms::PUSH_BUTTON, ms::DOWN, 0, 0), -1, 0, false);
        sh.wb_state(b, 4, img(ms::PUSH_BUTTON, ms::DEFAULT, 0, 0), -1, 0, false);

        // Checkbox and RadioButton
        for (s, t) in [(S::Checkbox, ms::CHECKBOX), (S::RadioButton, ms::RADIO_BUTTON)] {
            let b = new_button(s);
            if s == S::RadioButton {
                sh.btn_on(b, Bc::SetButtonFlags, 0x0008, 1);
            }
            push(b, 0, 0);
            sh.wb_set(b, Wf::HasCheck, 1);
            sh.wb_set(b, Wf::ScaleImage, 0);
            pads(b, 13 + 4, 2, 2, 2);
            sh.wb_state(b, 0, img(t, 0, 13, 13), -1, font, false);
            sh.wb_state(b, 1, img(t, ms::DOWN, 13, 13), -1, 0, false);
            sh.wb_state(b, 0, img(t, ms::CHECKED, 13, 13), -1, 0, true);
            sh.wb_state(b, 1, img(t, ms::CHECKED | ms::DOWN, 13, 13), -1, 0, true);
        }

        // HeaderButton
        let b = new_kind(S::HeaderButton, K::WonHeaderButton);
        sh.wb_set(b, Wf::ScaleImage, 1);
        sh.wb_state(b, 0, img(ms::LIST_HEADER, 0, 0, 0), -1, font, false);
        sh.wb_state(b, 1, img(ms::LIST_HEADER, ms::DOWN, 0, 0), -1, font, false);

        // Tab Button (the checked image replaces the normal one: both go to state 0 without the checked flag)
        let b = new_button(S::TabButton);
        sh.wb_set(b, Wf::ScaleImage, 1);
        sh.wb_set(b, Wf::HasCheck, 1);
        push(b, 0, 0);
        sh.wb_state(b, 0, img(ms::TAB_BUTTON, 0, 0, 0), -1, font, false);
        sh.wb_state(b, 0, img(ms::TAB_BUTTON, ms::CHECKED, 0, 0), -1, font, false);

        // Close Button
        let b = new_kind(S::CloseButton, K::WonSimpleButton);
        sh.shell_on(b, O::SetImage, 0, img(ms::CLOSE_BUTTON, 0, 16, 14));
        sh.shell_on(b, O::SetImage, 1, img(ms::CLOSE_BUTTON, ms::DOWN, 16, 14));

        // Arrow Buttons
        for i in 0..4isize {
            let s = [S::Arrow0, S::Arrow1, S::Arrow2, S::Arrow3][i as usize];
            let b = new_kind(s, K::WonSimpleButton);
            sh.shell_on(b, O::SetImage, 0, img(ms::LEFT_ARROW + i, 0, 15, 15));
            sh.shell_on(b, O::SetImage, 1, img(ms::LEFT_ARROW + i, ms::DOWN, 15, 15));
            sh.shell_on(b, O::SetImage, 2, img(ms::LEFT_ARROW + i, ms::DISABLED, 15, 15));
        }

        // Scrollbar, ScrollThumb, and ScrollGutter
        for i in 0..2usize {
            let b = sh.host(H::New, K::WonScrollbar as isize, 1, 0, 0);
            self.set(S::Scrollbar0 as usize + i, b);

            let t = sh.host(H::New, K::WonScrollThumb as isize, 0, 0, 0);
            self.set(S::ScrollThumb0 as usize + i, t);
            let t = self.slots[S::ScrollThumb0 as usize + i].get();
            sh.shell_on(t, O::SetImage, 0, img(ms::SCROLL_THUMB, 0, 0, 0));
            sh.shell_on(t, O::SetScaleImage, 1, 0);

            let g = sh.host(H::New, K::WonSimpleButton as isize, 0, 0, 0);
            self.set(S::ScrollGutter0 as usize + i, g);
            let g = self.slots[S::ScrollGutter0 as usize + i].get();
            sh.shell_on(g, O::SetScaleImage, 1, 0);
            sh.shell_on(g, O::SetImage, 0, img(ms::SCROLL_GUTTER, 0, 0, 0));
        }

        // ComboBox Button
        let b = new_kind(S::ComboButton, K::WonSimpleButton);
        sh.shell_on(b, O::SetScaleImage, 1, 0);
        sh.shell_on(b, O::SetImage, 0, img(ms::DOWN_ARROW, 0, 16, 18));
        sh.shell_on(b, O::SetImage, 1, img(ms::DOWN_ARROW, ms::DOWN, 16, 18));
        sh.shell_on(b, O::SetImage, 2, img(ms::DOWN_ARROW, ms::DISABLED, 16, 18));

        let b = new_kind(S::PopupBack, K::WonFrameComponent);
        sh.shell_on(b, O::SetFrameImage, img(ms::SCROLL_THUMB, 0, 0, 0), 0);
        let b = new_kind(S::DialogBack, K::WonFrameComponent);
        sh.shell_on(b, O::SetFrameImage, img(ms::SCROLL_THUMB, 0, 0, 0), 0);

        let input = new_kind(S::InputBox, K::WonInputBox);
        let combo = new_kind(S::ComboBox, K::WonComboBox);
        let scroll = new_kind(S::ScrollContainer, K::WonScrollContainer);
        let list = new_kind(S::ListCtrl, K::WonListCtrl);
        let tab = new_kind(S::TabCtrl, K::WonTabCtrl);
        self.set_frame_of(tab, img(ms::TAB_CTRL_FRAME, 0, 0, 0), 2);

        self.set_frame_of(input, img(ms::FRAME3, ms::DOWN, 0, 0), 2);
        self.set_frame_of(combo, img(ms::FRAME3, ms::DOWN, 0, 0), 2);
        self.set_frame_of(scroll, img(ms::FRAME3, ms::DOWN, 0, 0), 2);
        sh.shell_on(scroll, O::SetCorner, img(ms::SCROLL_CORNER, 0, 0, 0), 0);
        sh.shell_on(list, O::CopyScrollAttributes, scroll, 0);

        if font != 0 {
            sh.host(H::RefObj, font, -1, 0, 0);
        }
    }

    /// `~WONSkin`: the members in reverse order (the type maps, then the templates).
    fn release_all(&self) {
        for map in [&self.simple_types, &self.button_types] {
            let all: Vec<isize> = map.borrow().iter().rev().map(|(_, c)| c.get()).collect();
            map.borrow_mut().clear();
            for c in all {
                if c != 0 {
                    self.sh.comp_ref(c, -1);
                }
            }
        }
        for s in self.slots.iter().rev() {
            hold_comp(&self.sh, s, 0);
        }
    }
}

/// The units of a C++ `GUIString` at `p`.
///
/// # Safety
/// `p` is a live `GUIString` (or NULL).
unsafe fn gui_units(p: *const c_void) -> Vec<u32> {
    if p.is_null() {
        return Vec::new();
    }
    // SAFETY: a GUIString is its data pointer (the contract)
    unsafe { crate::guistrobj::data_units(*(p as *const *const c_void)) }
}

/// A new Windows-look skin (`new WONMSSkin`) over the shell host `host`.
#[no_mangle]
pub extern "C" fn ee_won_skin_new(host: EeWonShellHostFn) -> *mut EeWonSkin {
    let s = Box::new(EeWonSkin {
        sh: Sh::detached(host),
        slots: Default::default(),
        button_types: RefCell::new(Vec::new()),
        simple_types: RefCell::new(Vec::new()),
    });
    s.ms_skin();
    Box::into_raw(s)
}

/// The skin goes (its references released): the drivers' (the game's skin is never freed).
///
/// # Safety
/// `s` came from `ee_won_skin_new` (or is NULL) and is not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_won_skin_free(s: *mut EeWonSkin) {
    if s.is_null() {
        return;
    }
    // SAFETY: the contract
    let b = unsafe { Box::from_raw(s) };
    b.release_all();
}

static GLOBAL: AtomicPtr<EeWonSkin> = AtomicPtr::new(std::ptr::null_mut());

/// The game's skin, made (`gSkin = new WONMSSkin`) when there is none (`WONSkinConfig::PreParse`).
#[no_mangle]
pub extern "C" fn ee_won_skin_ensure(host: EeWonShellHostFn) -> *mut EeWonSkin {
    let p = GLOBAL.load(Ordering::Acquire);
    if !p.is_null() {
        return p;
    }
    let made = ee_won_skin_new(host);
    GLOBAL.store(made, Ordering::Release);
    made
}

/// The game's skin (NULL before the first `WONSkin` group).
#[no_mangle]
pub extern "C" fn ee_won_skin_global() -> *mut EeWonSkin {
    GLOBAL.load(Ordering::Acquire)
}

/// # Safety
/// `s` is a live skin.
unsafe fn skin<'a>(s: *const EeWonSkin) -> &'a EeWonSkin {
    // SAFETY: the contract
    unsafe { &*s }
}

/// One of the skin's getters: a new widget (a `Component*`, not held).
///
/// # Safety
/// `s` is a live skin.
#[no_mangle]
pub unsafe extern "C" fn ee_won_skin_get(s: *const EeWonSkin, g: EeWonSkinGet, a: isize) -> *mut c_void {
    // SAFETY: the contract
    unsafe { skin(s) }.get(g, a) as *mut c_void
}

/// The template slot `slot` (an `EeWonSkinSlot`, `+ i` for the arrays) = `c` (held).
///
/// # Safety
/// `s` is a live skin; `c` a live component or NULL.
#[no_mangle]
pub unsafe extern "C" fn ee_won_skin_set(s: *const EeWonSkin, slot: EeWonSkinSlot, i: i32, c: *mut c_void) {
    // SAFETY: the contract
    unsafe { skin(s) }.set(slot as usize + i.max(0) as usize, c as isize)
}

/// The template slot's widget (borrowed).
///
/// # Safety
/// `s` is a live skin.
#[no_mangle]
pub unsafe extern "C" fn ee_won_skin_slot(s: *const EeWonSkin, slot: EeWonSkinSlot, i: i32) -> *mut c_void {
    // SAFETY: the contract
    let k = slot as usize + i.max(0) as usize;
    let sk = unsafe { skin(s) };
    (if k < SLOTS { sk.slots[k].get() } else { 0 }) as *mut c_void
}

/// `mButtonTypeMap[*key] = c` (`simple`: `mSimpleButtonTypeMap`).
///
/// # Safety
/// `s` is a live skin; `key` a live `GUIString`; `c` a live component or NULL.
#[no_mangle]
pub unsafe extern "C" fn ee_won_skin_set_type(s: *const EeWonSkin, simple: bool, key: *const c_void, c: *mut c_void) {
    // SAFETY: the contract
    unsafe { skin(s).set_type(simple, gui_units(key), c as isize) }
}

/// `CopyButtonType(*key, to)` (`simple`: `CopySimpleButtonType`).
///
/// # Safety
/// As `ee_won_skin_set_type`; `to` a live button.
#[no_mangle]
pub unsafe extern "C" fn ee_won_skin_copy_type(
    s: *const EeWonSkin,
    simple: bool,
    key: *const c_void,
    to: *mut c_void,
) -> bool {
    // SAFETY: the contract
    unsafe { skin(s).copy_type(simple, &gui_units(key), to as isize) }
}

/// `HandleButtonClickSound` with the sound read.
///
/// # Safety
/// `s` is a live skin; `sound` a live `Sound*` or NULL.
#[no_mangle]
pub unsafe extern "C" fn ee_won_skin_click_sound(s: *const EeWonSkin, sound: *mut c_void) {
    // SAFETY: the contract
    unsafe { skin(s) }.click_sound(sound as isize)
}

#[cfg(test)]
#[path = "skinobj_tests.rs"]
mod tests;
