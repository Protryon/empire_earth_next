//! WONGUI's TabBar and TabCtrl as whole Rust objects (R6 WON round 27): the lobby's tab control (the root screen's
//! LAN, Internet, direct connect, game, help and news tabs) and its bar of tab buttons. The original's TabCtrl.cpp:
//! the bar's buttons (held by reference), its selected tab, its overlap; the control's padding, offsets, bar height
//! and overlap, scroll arrows (held), placement flags, pages and selected page, with every method of both classes.
//!
//! The C++ `TabBar` and `TabCtrl` stay the types the configs, skins and the root screen use (their vtables the
//! overrides of the container's virtuals); their methods are one-line forwarders. The control's four containers (the
//! bottom bar, the top bar and their two containers) and the page area's layout point stay C++ members made by its
//! constructor, which hands them to `ee_won_tab_init`; the Rust object borrows them. Each Rust object holds its
//! container's Rust object (made by the base's constructor, freed after it) and calls it for every `Container::X` the
//! original made; a container part's own `Container::X` goes to that part's Rust container (`CtContainerOf`). Every
//! virtual the original called on a component (the pages', the buttons', its own RemoveChild and Virtual08) goes
//! through the C++ object, so the subclasses' overrides run. The bar calls its control's Rust object directly.

use std::cell::{Cell, RefCell};
use std::ffi::c_void;

use crate::ctobj::{EeWonContainer, EeWonCtField};
use crate::guictl::{self, EeWonRect4};
use crate::guihost::{EeWonCtVirt as V, EeWonGuiEvent, EeWonGuiHostFn, EeWonGuiOp as Op};

const FLAG_INVISIBLE: isize = 0x1;
const FLAG_WANT_FOCUS: isize = 0x8;
const FLAG_HAS_FOCUS: isize = 0x10;
const FLAG_DISABLED: isize = 0x80;
const FLAG_GRAB_BG: isize = 0x100;
const FLAG_ALWAYS_ON_TOP: isize = 0x1000;
const FLAG_CLIP: isize = 0x2000;
const BUTTON_REPEAT: isize = 0x2;
const BUTTON_ACTIVATE_ON_PRESS: isize = 0x40;
const EVENT_BUTTON_PRESSED: isize = 15;

/// The calls out every tab object makes, over its C++ component and its container's Rust object.
struct EeWonTabOut {
    comp: *mut c_void,
    host: EeWonGuiHostFn,
    ct: *const EeWonContainer,
}

impl EeWonTabOut {
    fn op(&self, op: Op, a: isize, b: isize, c: isize, d: isize) -> isize {
        match self.host {
            // SAFETY: the host was given with this object's component; no borrow of a state is live here
            Some(h) => unsafe { h(self.comp, op, a, b, c, d) },
            None => 0,
        }
    }
    fn ct(&self) -> &EeWonContainer {
        // SAFETY: the base's Rust object outlives this one (made before it, freed after it)
        unsafe { &*self.ct }
    }
    fn me(&self) -> isize {
        self.comp as isize
    }
    fn bits(&self, c: isize) -> isize {
        self.op(Op::CtBits, c, 0, 0, 0)
    }
    fn left(&self, c: isize) -> i32 {
        self.op(Op::ChildLeft, c, 0, 0, 0) as i32
    }
    fn top(&self, c: isize) -> i32 {
        self.op(Op::ChildTop, c, 0, 0, 0) as i32
    }
    fn width(&self, c: isize) -> i32 {
        self.op(Op::ChildWidth, c, 0, 0, 0) as i32
    }
    fn height(&self, c: isize) -> i32 {
        self.op(Op::ChildHeight, c, 0, 0, 0) as i32
    }
    fn set_pos(&self, c: isize, x: i32, y: i32) {
        self.op(Op::ChildSetPos, c, x as isize, y as isize, 0);
    }
    fn set_size(&self, c: isize, w: i32, h: i32) {
        self.op(Op::ChildSetSize, c, w as isize, h as isize, 0);
    }
    /// A container part's Rust container (its `Container::X` calls).
    fn ct_of(&self, c: isize) -> Option<&EeWonContainer> {
        let p = self.op(Op::CtContainerOf, c, 0, 0, 0);
        // SAFETY: a live container's Rust object (the part is held while this object lives)
        (p != 0).then(|| unsafe { &*(p as *const EeWonContainer) })
    }
    /// `c->X(...)`, a component's virtual.
    fn child(&self, c: isize, v: V, a: isize, b: isize) -> isize {
        self.op(Op::CtChild, c, v as isize, a, b)
    }
}

// ---- TabBar

/// The inward calls of the C++ `TabBar` shell.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonTabBarCall {
    /// `HandleComponentEvent(a)` (a `ComponentEvent*`).
    HandleComponentEvent = 1,
}

#[derive(Debug)]
struct BarState {
    tabs: Vec<isize>,
    cur: isize,
    overlap: i32,
}

/// The Rust TabBar.
pub struct EeWonTabBar {
    o: EeWonTabOut,
    ctrl: Cell<*const EeWonTabCtrl>,
    st: RefCell<BarState>,
}

impl EeWonTabBar {
    /// The constructor's state: no tab, none selected, a 4-pixel overlap, for the control `ctrl`.
    pub fn new(comp: *mut c_void, host: EeWonGuiHostFn, ct: *const EeWonContainer, ctrl: *const EeWonTabCtrl) -> Self {
        EeWonTabBar {
            o: EeWonTabOut { comp, host, ct },
            ctrl: Cell::new(ctrl),
            st: RefCell::new(BarState { tabs: Vec::new(), cur: 0, overlap: 4 }),
        }
    }

    fn ctrl(&self) -> Option<&EeWonTabCtrl> {
        let p = self.ctrl.get();
        // SAFETY: the control's Rust object outlives its bar's use (it clears this pointer when it goes)
        (!p.is_null()).then(|| unsafe { &*p })
    }
    fn count(&self) -> usize {
        self.st.borrow().tabs.len()
    }
    fn tab(&self, i: usize) -> isize {
        self.st.borrow().tabs[i]
    }
    pub fn cur_tab(&self) -> isize {
        self.st.borrow().cur
    }
    pub fn overlap(&self) -> i32 {
        self.st.borrow().overlap
    }
    pub fn set_overlap(&self, v: i32) {
        self.st.borrow_mut().overlap = v;
    }
    fn in_range(&self, id: i32) -> bool {
        id >= 0 && (id as usize) < self.count()
    }

    pub fn cur_tab_id(&self) -> i32 {
        let cur = self.cur_tab();
        if cur == 0 {
            -1
        } else {
            self.o.op(Op::ChildControlId, cur, 0, 0, 0) as i32
        }
    }

    pub fn position_tab(&self, tab: isize) {
        let id = self.o.op(Op::ChildControlId, tab, 0, 0, 0) as i32;
        let n = self.count();
        let prev_right = if id > 0 && id as usize <= n {
            let prev = self.tab(id as usize - 1);
            self.o.op(Op::ChildRight, prev, 0, 0, 0) as i32
        } else {
            0
        };
        let x = guictl::tab_x(id, n, prev_right, self.overlap());
        self.o.set_pos(tab, x, 0);
    }

    pub fn add_tab(&self, tab: isize) {
        let o = &self.o;
        o.op(Op::ButtonSetFlags, tab, BUTTON_ACTIVATE_ON_PRESS, 1, 0);
        o.op(Op::ChildSetControlId, tab, self.count() as isize, 0, 0);
        o.op(Op::ChildHold, tab, 0, 0, 0);
        self.st.borrow_mut().tabs.push(tab);
        self.position_tab(tab);
        let me = o.me();
        let w = o.left(tab).wrapping_add(o.width(tab));
        o.set_size(me, w, o.height(me));
        o.ct().add_child_ex(tab, 0);
    }

    pub fn set_cur_tab(&self, id: i32, request_focus: bool) {
        if !self.in_range(id) {
            return;
        }
        let old = self.cur_tab_id();
        let o = &self.o;
        if old == id {
            let cur = self.cur_tab();
            if request_focus && cur != 0 {
                o.child(cur, V::RequestFocus, 0, 0);
            }
            return;
        }
        let tab = self.tab(id as usize);
        if o.bits(tab) & FLAG_DISABLED != 0 {
            return;
        }
        if self.cur_tab() != 0 {
            o.op(Op::ButtonSetCheck, self.cur_tab(), 0, 0, 0);
            // the selected tab read again after the uncheck (as the original)
            let cur = self.cur_tab();
            if o.op(Op::CtParentOf, cur, 0, 0, 0) != o.me() {
                let at = (self.count() as i32).wrapping_sub(old).wrapping_sub(1);
                o.ct().add_child_ex(self.cur_tab(), at);
                self.position_tab(self.cur_tab());
            }
        }
        self.st.borrow_mut().cur = tab;
        o.op(Op::ButtonSetCheck, tab, 1, 0, 0);
        if let Some(c) = self.ctrl() {
            c.set_user_has_interacted(true);
            c.notify_tab_change(request_focus);
        }
    }

    /// `name` is the C++ `const GUIString*` the buttons' texts are compared with.
    pub fn set_cur_tab_by_name(&self, name: isize, request_focus: bool) {
        let mut i = 0;
        while i < self.count() {
            if self.o.op(Op::TabTextIs, self.tab(i), name, 0, 0) != 0 {
                self.set_cur_tab(i as i32, request_focus);
                return;
            }
            i += 1;
        }
    }

    pub fn enable_tab(&self, id: i32, enabled: bool) {
        if !self.in_range(id) {
            return;
        }
        let old = self.cur_tab_id();
        if old == id && !enabled {
            self.set_cur_tab(guictl::tab_neighbour(old), true);
        }
        let tab = self.tab(id as usize);
        if tab != 0 {
            self.o.op(Op::ChildSetEnabled, tab, enabled as isize, 0, 0);
        }
    }

    pub fn disabled(&self, id: i32) -> bool {
        if !self.in_range(id) {
            return true;
        }
        let tab = self.tab(id as usize);
        tab == 0 || self.o.bits(tab) & FLAG_DISABLED != 0
    }

    pub fn handle_component_event(&self, event: isize) {
        let o = &self.o;
        if o.op(Op::EventType, event, 0, 0, 0) == EVENT_BUTTON_PRESSED {
            let id = o.op(Op::EventControlId, event, 0, 0, 0) as i32;
            if self.in_range(id) && o.op(Op::EventComponent, event, 0, 0, 0) == self.tab(id as usize) {
                self.set_cur_tab(id, true);
                return;
            }
        }
        o.op(Op::ComponentHandleEvent, event, 0, 0, 0);
    }

    pub fn remove_tab(&self, id: i32) {
        if !self.in_range(id) {
            return;
        }
        let _ = self.cur_tab_id();
        let o = &self.o;
        let gone = self.tab(id as usize);
        o.op(Op::CtVirt, V::RemoveChild as isize, gone, 0, 0);
        self.st.borrow_mut().tabs.remove(id as usize);
        o.op(Op::ChildRelease, gone, 0, 0, 0);
        for i in 0..self.count() {
            o.op(Op::ChildSetControlId, self.tab(i), i as isize, 0, 0);
        }
        self.st.borrow_mut().cur = 0;
        let mut i = 0;
        while i < self.count() {
            let b = self.tab(i);
            if o.op(Op::CtParentOf, b, 0, 0, 0) != o.me() {
                o.ct().add_child_ex(b, (self.count() - i - 1) as i32);
            }
            self.position_tab(b);
            i += 1;
        }
    }

    /// The destructor: the buttons given back in the vector's order.
    pub fn destroy(&self) {
        let tabs = std::mem::take(&mut self.st.borrow_mut().tabs);
        for t in tabs {
            self.o.op(Op::ChildRelease, t, 0, 0, 0);
        }
        self.ctrl.set(std::ptr::null());
    }

    pub fn call(&self, call: EeWonTabBarCall, a: isize) -> isize {
        match call {
            EeWonTabBarCall::HandleComponentEvent => self.handle_component_event(a),
        }
        0
    }
}

// ---- TabCtrl

/// The inward calls of the C++ `TabCtrl` shell.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonTabCall {
    /// `Virtual08` (SizeChanged).
    SizeChanged = 1,
    /// `GetDesiredSize(*a, *b)` (`a`, `b` point at i32s).
    DesiredSize,
    /// `KeyDown(a)` (0 or 1).
    KeyDown,
    AddedToParent,
    /// `HandleComponentEvent(a)` (a `ComponentEvent*`).
    HandleComponentEvent,
    /// `TabFocus(a != 0, b != 0)` (0 or 1).
    TabFocus,
    /// `CopyAttributes(a)` (`a` the other control's `EeWonTabCtrl*`).
    CopyAttributes,
    /// `SetTabBarOnRight(a != 0)`.
    SetTabBarOnRight,
    /// `SetArrowsOnLeft(a != 0)`.
    SetArrowsOnLeft,
    /// `SetTabArrows(a, b)` (two `Button*` as `Component*`).
    SetTabArrows,
    /// `SetCurTab(a, b != 0)`.
    SetCurTab,
    /// `SetCurTabByName(*a, b != 0)` (`a` a `const GUIString*`).
    SetCurTabByName,
    /// `GetTabIdForContainer(a)` (a `Container*` as a `Component*`).
    GetTabIdForContainer,
    /// `EnableTab(a, b != 0)`.
    EnableTab,
    /// `AddTab(a, b)` (a `Button*` and a `Container*` as `Component*`s, `b` may be NULL).
    AddTab,
    /// `RemoveTab(a)`.
    RemoveTab,
    /// the field `a` (`EeWonTabField`).
    GetField,
    /// the field `a` = `b`.
    SetField,
}

/// The control's fields its configs and the root screen read and set.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonTabField {
    /// the bottom bar's selected tab id (read only).
    CurTabId = 1,
    TabBarHeight,
    /// the bottom bar's overlap between tabs.
    TabOverlap,
    TabBarOverlap,
    BottomTabTopOffset,
    BottomTabLeftOffset,
    UserHasInteracted,
}

/// The constructor's parts (`ee_won_tab_init`): the bottom bar's Rust object, the four containers and the layout
/// point, as `Component*`s.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeWonTabParts {
    pub bar: *const EeWonTabBar,
    pub bottom_bar: isize,
    pub top_bar: isize,
    pub bottom_bar_container: isize,
    pub top_bar_container: isize,
    pub layout: isize,
}

#[derive(Debug)]
struct CtrlState {
    /// left, right, top, bottom
    pads: [i32; 4],
    bottom_left_offset: i32,
    bottom_top_offset: i32,
    bar_height: i32,
    bar_overlap: i32,
    left: isize,
    right: isize,
    arrows_on_left: bool,
    on_right: bool,
    on_bottom: bool,
    user_has_interacted: bool,
    pages: Vec<isize>,
    cur: isize,
    bar: *const EeWonTabBar,
    bottom_bar: isize,
    top_bar: isize,
    bottom_ct: isize,
    top_ct: isize,
    layout: isize,
}

/// The Rust TabCtrl.
pub struct EeWonTabCtrl {
    o: EeWonTabOut,
    st: RefCell<CtrlState>,
}

impl EeWonTabCtrl {
    /// The constructor's state: 5-pixel padding, the bottom bar's offsets 2 and 3, no bar height yet, a 1-pixel bar
    /// overlap, no arrows, no page; the user has not interacted (the original left that flag unset).
    pub fn new(comp: *mut c_void, host: EeWonGuiHostFn, ct: *const EeWonContainer) -> Self {
        EeWonTabCtrl {
            o: EeWonTabOut { comp, host, ct },
            st: RefCell::new(CtrlState {
                pads: [5; 4],
                bottom_left_offset: 2,
                bottom_top_offset: 3,
                bar_height: 0,
                bar_overlap: 1,
                left: 0,
                right: 0,
                arrows_on_left: false,
                on_right: false,
                on_bottom: false,
                user_has_interacted: false,
                pages: Vec::new(),
                cur: 0,
                bar: std::ptr::null(),
                bottom_bar: 0,
                top_bar: 0,
                bottom_ct: 0,
                top_ct: 0,
                layout: 0,
            }),
        }
    }

    fn bar(&self) -> &EeWonTabBar {
        let p = self.st.borrow().bar;
        // SAFETY: the bottom bar is a member the C++ control holds while its Rust object lives (`init` set it)
        unsafe { &*p }
    }
    fn part(&self, f: impl Fn(&CtrlState) -> isize) -> isize {
        f(&self.st.borrow())
    }
    fn bottom_bar(&self) -> isize {
        self.part(|s| s.bottom_bar)
    }
    fn top_bar(&self) -> isize {
        self.part(|s| s.top_bar)
    }
    fn bottom_ct(&self) -> isize {
        self.part(|s| s.bottom_ct)
    }
    fn top_ct(&self) -> isize {
        self.part(|s| s.top_ct)
    }
    fn layout(&self) -> isize {
        self.part(|s| s.layout)
    }
    fn arrows(&self) -> (isize, isize) {
        let s = self.st.borrow();
        (s.left, s.right)
    }
    fn cur(&self) -> isize {
        self.st.borrow().cur
    }
    fn set_user_has_interacted(&self, v: bool) {
        self.st.borrow_mut().user_has_interacted = v;
    }
    fn top_bar_used_keys(&self) {
        if let Some(t) = self.o.ct_of(self.top_bar()) {
            t.set_field(EeWonCtField::HasUsedDialogKeys as i32, 1);
        }
    }

    /// The constructor's statements after its members were made: the bars' and containers' flags, the bars in their
    /// containers, the containers and the layout point in the control.
    pub fn init(&self, parts: &EeWonTabParts) {
        {
            let mut s = self.st.borrow_mut();
            s.bar = parts.bar;
            s.bottom_bar = parts.bottom_bar;
            s.top_bar = parts.top_bar;
            s.bottom_ct = parts.bottom_bar_container;
            s.top_ct = parts.top_bar_container;
            s.layout = parts.layout;
        }
        let o = &self.o;
        let (top_bar, top_ct, bottom_ct) = (parts.top_bar, parts.top_bar_container, parts.bottom_bar_container);
        if let Some(t) = o.ct_of(top_bar) {
            t.set_field(EeWonCtField::OnlyChildInput as i32, 1);
        }
        if let Some(t) = o.ct_of(top_ct) {
            t.set_field(EeWonCtField::OnlyChildInput as i32, 1);
        }
        o.op(Op::ChildSetFlags, top_ct, FLAG_GRAB_BG, 1, 0);
        o.op(Op::ChildSetFlags, bottom_ct, FLAG_GRAB_BG, 1, 0);
        o.op(Op::ChildSetFlags, bottom_ct, FLAG_WANT_FOCUS, 0, 0);
        o.op(Op::ChildSetFlags, bottom_ct, FLAG_CLIP, 1, 0);
        o.op(Op::ChildSetFlags, top_ct, FLAG_ALWAYS_ON_TOP | FLAG_CLIP, 1, 0);
        if let Some(t) = o.ct_of(bottom_ct) {
            t.add_child(parts.bottom_bar);
        }
        if let Some(t) = o.ct_of(top_ct) {
            t.add_child(top_bar);
        }
        o.ct().add_child(bottom_ct);
        o.op(Op::TabAddLayoutPoint, parts.layout, 0, 0, 0);
        o.ct().add_child(top_ct);
    }

    pub fn copy_attributes(&self, from: &EeWonTabCtrl) {
        // the values read first: a control may copy itself
        let (lo, to, bh, ol, or, ob, bo) = {
            let f = from.st.borrow();
            (
                f.bottom_left_offset,
                f.bottom_top_offset,
                f.bar_height,
                f.arrows_on_left,
                f.on_right,
                f.on_bottom,
                f.bar_overlap,
            )
        };
        let overlap = from.bar().overlap();
        {
            let mut s = self.st.borrow_mut();
            s.bottom_left_offset = lo;
            s.bottom_top_offset = to;
            s.bar_height = bh;
            s.arrows_on_left = ol;
            s.on_right = or;
            s.on_bottom = ob;
            s.bar_overlap = bo;
        }
        self.bar().set_overlap(overlap);
    }

    pub fn desired_size(&self) -> (i32, i32) {
        let (w, h) = self.o.ct().desired_size();
        let s = self.st.borrow();
        let [l, r, t, b] = s.pads;
        (w.wrapping_add(r.wrapping_add(l)), h.wrapping_add(s.bar_height.wrapping_add(b).wrapping_add(t)))
    }

    pub fn set_tab_arrows(&self, left: isize, right: isize) {
        let o = &self.o;
        // the SmartPtr assignments: the old one released, then the new one held
        for (k, new) in [(0, left), (1, right)] {
            let old = {
                let s = self.st.borrow();
                if k == 0 {
                    s.left
                } else {
                    s.right
                }
            };
            if old == new {
                continue;
            }
            if old != 0 {
                o.op(Op::ChildRelease, old, 0, 0, 0);
            }
            {
                let mut s = self.st.borrow_mut();
                if k == 0 {
                    s.left = new;
                } else {
                    s.right = new;
                }
            }
            if new != 0 {
                o.op(Op::ChildHold, new, 0, 0, 0);
            }
        }
        let (l, r) = self.arrows();
        o.op(Op::ChildSetFlags, l, FLAG_ALWAYS_ON_TOP, 1, 0);
        o.op(Op::ChildSetFlags, r, FLAG_ALWAYS_ON_TOP, 1, 0);
        o.op(Op::ButtonSetFlags, l, BUTTON_REPEAT, 1, 0);
        o.op(Op::ButtonSetFlags, r, BUTTON_REPEAT, 1, 0);
        let (l, r) = self.arrows();
        o.ct().add_child(l);
        o.ct().add_child(r);
    }

    pub fn notify_tab_change(&self, request_focus: bool) {
        let o = &self.o;
        let button = self.bar().cur_tab();
        if button == 0 {
            return;
        }
        if let Some(t) = o.ct_of(self.top_bar()) {
            t.add_child(button);
        }
        let id = self.bar().cur_tab_id();
        let selected = {
            let s = self.st.borrow();
            if id < 0 || id as usize >= s.pages.len() {
                return;
            }
            s.pages[id as usize]
        };
        if selected == self.cur() {
            return;
        }
        let old = self.cur();
        if old != 0 {
            o.op(Op::ChildSetVisible, old, 0, 0, 0);
        }
        self.st.borrow_mut().cur = selected;
        self.position_tab_container(selected);
        // the page read again (as the original); a NULL page made the original crash here
        if self.cur() != 0 {
            o.op(Op::ChildSetVisible, self.cur(), 1, 0, 0);
            o.ct().bring_to_top(self.cur());
            if request_focus {
                o.child(self.cur(), V::RequestFocus, 0, 0);
            }
        }
        self.keep_tab_button_in_view();
        o.op(Op::Invalidate, 0, 0, 0, 0);
        o.op(Op::FireEvent, EeWonGuiEvent::TabCtrlTabSelected as isize, 0, 0, 0);
    }

    pub fn keep_tab_button_in_view(&self) {
        let o = &self.o;
        let button = self.bar().cur_tab();
        if button == 0 {
            return;
        }
        let left = o.left(self.bottom_bar()).wrapping_add(o.left(button));
        if let Some(a) = guictl::tab_keep_in_view(left, o.width(button), o.width(self.top_ct())) {
            self.adjust_tab_bar_scroll(a);
        }
    }

    pub fn position_tab_container(&self, c: isize) {
        if c == 0 {
            return;
        }
        let o = &self.o;
        let pads = self.st.borrow().pads;
        let layout = self.layout();
        let r = guictl::tab_page_rect(pads, o.top(layout), o.height(layout), o.width(o.me()));
        o.set_pos(c, r.x, r.y);
        o.set_size(c, r.w, r.h);
    }

    pub fn added_to_parent(&self) {
        self.o.ct().added_to_parent();
        if self.cur() == 0 {
            self.bar().set_cur_tab(0, false);
        }
        self.set_user_has_interacted(false);
    }

    pub fn calc_container_layout(&self) {
        let o = &self.o;
        let top_ct = self.top_ct();
        let (on_bottom, overlap) = {
            let s = self.st.borrow();
            (s.on_bottom, s.bar_overlap)
        };
        let bar_top = o.top(top_ct);
        let (top, height) =
            guictl::tab_area(on_bottom, o.height(o.me()), bar_top, bar_top.wrapping_add(o.height(top_ct)), overlap);
        let mut r = EeWonRect4 { x: 0, y: top, w: o.width(o.me()), h: height };
        o.op(Op::ChildSetPosSize, self.layout(), &mut r as *mut EeWonRect4 as isize, 0, 0);
    }

    pub fn position_arrows(&self) {
        let o = &self.o;
        let (l, r) = self.arrows();
        if l == 0 || r == 0 {
            return;
        }
        let (on_left, on_bottom) = {
            let s = self.st.borrow();
            (s.arrows_on_left, s.on_bottom)
        };
        let layout = self.layout();
        let area_top = o.top(layout);
        let (x, y) = guictl::tab_arrows(
            on_left,
            on_bottom,
            o.width(o.me()),
            o.width(r),
            o.width(l),
            o.height(l),
            area_top,
            area_top.wrapping_add(o.height(layout)),
        );
        o.set_pos(l, x, y);
        let (l, r) = self.arrows();
        o.set_pos(r, o.op(Op::ChildRight, l, 0, 0, 0) as i32, y);
    }

    pub fn vert_position_tab_bar(&self) {
        let o = &self.o;
        let (bar, top_bar, bottom_ct, top_ct) = (self.bottom_bar(), self.top_bar(), self.bottom_ct(), self.top_ct());
        let h = {
            let s = self.st.borrow();
            guictl::tab_bottom_height(s.bar_height, s.bar_overlap, s.bottom_top_offset)
        };
        o.set_size(bar, o.width(bar), h);
        o.set_size(bottom_ct, o.width(bottom_ct), o.height(bar));
        let bh = self.st.borrow().bar_height;
        o.set_size(top_bar, o.width(top_bar), bh);
        let bh = self.st.borrow().bar_height;
        o.set_size(top_ct, o.width(top_ct), bh);
        let (on_bottom, offset) = {
            let s = self.st.borrow();
            (s.on_bottom, s.bottom_top_offset)
        };
        let (y, bottom_y) = guictl::tab_vert(on_bottom, o.height(o.me()), o.height(bottom_ct), offset);
        o.set_pos(top_ct, o.left(top_ct), y);
        o.set_pos(bottom_ct, o.left(bottom_ct), bottom_y);
    }

    pub fn horz_position_tab_bar(&self) {
        let o = &self.o;
        let need = self.need_scroll_buttons();
        let (l, r) = self.arrows();
        let (on_left, on_right) = {
            let s = self.st.borrow();
            (s.arrows_on_left, s.on_right)
        };
        // the arrows are read only where the original read them
        let visible = |c: isize| o.bits(c) & FLAG_INVISIBLE == 0;
        let right_arrow = (need && on_left && r != 0 && visible(r)).then(|| o.op(Op::ChildRight, r, 0, 0, 0) as i32);
        let left_arrow = (need && !on_left && l != 0 && visible(l)).then(|| o.left(l));
        let offset = self.st.borrow().bottom_left_offset;
        let (left, right) = guictl::tab_horz(
            need,
            on_left,
            right_arrow,
            left_arrow,
            on_right,
            o.width(o.me()),
            o.width(self.bottom_bar()),
            offset,
        );
        let (top_ct, bottom_ct) = (self.top_ct(), self.bottom_ct());
        o.set_pos(top_ct, left, o.top(top_ct));
        o.set_size(top_ct, right.wrapping_sub(left), o.height(top_ct));
        let offset = self.st.borrow().bottom_left_offset;
        o.set_pos(bottom_ct, offset.wrapping_add(left), o.top(bottom_ct));
        let offset = self.st.borrow().bottom_left_offset;
        o.set_size(bottom_ct, right.wrapping_sub(offset).wrapping_sub(left), o.height(bottom_ct));
    }

    pub fn size_changed(&self) {
        let o = &self.o;
        let (l, r) = self.arrows();
        if l != 0 && r != 0 {
            let need = self.need_scroll_buttons();
            o.op(Op::ChildSetVisible, l, need as isize, 0, 0);
            let (_, r) = self.arrows();
            o.op(Op::ChildSetVisible, r, need as isize, 0, 0);
        }
        self.vert_position_tab_bar();
        self.calc_container_layout();
        self.position_arrows();
        self.horz_position_tab_bar();
        self.position_tab_container(self.cur());
        self.adjust_tab_bar_scroll(0);
        if !self.st.borrow().user_has_interacted {
            self.keep_tab_button_in_view();
        }
        o.ct().size_changed();
    }

    pub fn need_scroll_buttons(&self) -> bool {
        self.o.width(self.bottom_bar()) > self.o.width(self.o.me())
    }

    pub fn adjust_tab_bar_scroll(&self, amount: i32) {
        let o = &self.o;
        let bar = self.bottom_bar();
        let old_x = o.left(bar);
        let (x, arrows) = guictl::tab_scroll(old_x, amount, o.width(bar), o.width(self.bottom_ct()));
        if let Some((el, er)) = arrows {
            let (l, r) = self.arrows();
            if l != 0 && r != 0 {
                o.op(Op::ChildSetEnabled, l, el as isize, 0, 0);
                let (_, r) = self.arrows();
                o.op(Op::ChildSetEnabled, r, er as isize, 0, 0);
            }
        }
        if x == old_x {
            return;
        }
        o.set_pos(bar, x, o.top(bar));
        let top_bar = self.top_bar();
        o.set_pos(top_bar, x, o.top(top_bar));
        o.op(Op::ChildInvalidate, self.bottom_ct(), 0, 0, 0);
        o.op(Op::ChildInvalidate, self.top_ct(), 0, 0, 0);
    }

    pub fn handle_component_event(&self, event: isize) {
        let o = &self.o;
        if o.op(Op::EventType, event, 0, 0, 0) == EVENT_BUTTON_PRESSED {
            let comp = o.op(Op::EventComponent, event, 0, 0, 0);
            let (l, r) = self.arrows();
            let d = if comp == l {
                10
            } else if comp == r {
                -10
            } else {
                0
            };
            if d != 0 {
                self.set_user_has_interacted(true);
                self.adjust_tab_bar_scroll(d);
                return;
            }
        }
        o.op(Op::ComponentHandleEvent, event, 0, 0, 0);
    }

    pub fn tab_focus(&self, forward: bool, init: bool) -> bool {
        let o = &self.o;
        let button = self.bar().cur_tab();
        let current = self.cur();
        if button != 0 && current != 0 {
            let tab = |c: isize, f: bool, i: bool| o.child(c, V::TabFocus, f as isize, i as isize) != 0;
            if init {
                if !forward && tab(current, false, true) {
                    return true;
                }
                let result = tab(button, forward, true);
                if result {
                    self.top_bar_used_keys();
                }
                return result;
            }
            if o.bits(button) & FLAG_HAS_FOCUS != 0 {
                return forward && tab(current, true, true);
            }
            if tab(current, forward, false) {
                return true;
            }
            if forward {
                return false;
            }
            if tab(button, false, true) {
                self.top_bar_used_keys();
                return true;
            }
            return false;
        }
        o.ct().tab_focus(forward, init)
    }

    pub fn key_down(&self, key: i32) -> bool {
        let o = &self.o;
        let bar = self.bar();
        let button = bar.cur_tab();
        if button != 0 && o.bits(button) & FLAG_HAS_FOCUS != 0 {
            let count = self.st.borrow().pages.len();
            let Some(target) = guictl::tab_key_target(key, bar.cur_tab_id(), count, |i| bar.disabled(i)) else {
                return o.ct().key_down(key);
            };
            bar.o.ct().set_field(EeWonCtField::HasUsedDialogKeys as i32, 1);
            self.top_bar_used_keys();
            self.set_user_has_interacted(true);
            bar.set_cur_tab(target, false);
            let c = bar.cur_tab();
            if c != 0 {
                o.child(c, V::RequestFocus, 0, 0);
            }
            return true;
        }
        o.ct().key_down(key)
    }

    pub fn tab_id_for_container(&self, c: isize) -> i32 {
        self.st.borrow().pages.iter().position(|&p| p == c).map_or(-1, |i| i as i32)
    }

    pub fn add_tab(&self, tab: isize, c: isize) {
        let o = &self.o;
        let h = guictl::tab_bar_height(self.st.borrow().bar_height, o.height(tab));
        self.st.borrow_mut().bar_height = h;
        o.set_size(tab, o.width(tab), self.st.borrow().bar_height);
        o.op(Op::ChildSetFlags, tab, FLAG_GRAB_BG, 0, 0);
        self.st.borrow_mut().pages.push(c);
        let old_need = self.need_scroll_buttons();
        self.bar().add_tab(tab);
        if c != 0 {
            o.op(Op::ChildSetVisible, c, 0, 0, 0);
            o.op(Op::ChildSetFlags, c, FLAG_CLIP, 1, 0);
            o.ct().add_child(c);
        }
        if self.need_scroll_buttons() != old_need {
            o.op(Op::CtVirt, V::SizeChanged as isize, 0, 0, 0);
        }
    }

    pub fn remove_tab(&self, id: i32) {
        if id < 0 || id as usize >= self.st.borrow().pages.len() {
            return;
        }
        let o = &self.o;
        let cur_id = self.bar().cur_tab_id();
        o.child(self.top_bar(), V::RemoveChild, self.bar().cur_tab(), 0);
        self.st.borrow_mut().cur = 0;
        let page = self.st.borrow().pages[id as usize];
        o.op(Op::CtVirt, V::RemoveChild as isize, page, 0, 0);
        self.st.borrow_mut().pages.remove(id as usize);
        self.bar().remove_tab(id);
        self.bar().set_cur_tab(guictl::tab_after_remove(id, cur_id), true);
    }

    pub fn field(&self, f: i32) -> isize {
        type F = EeWonTabField;
        if f == F::CurTabId as i32 {
            return self.bar().cur_tab_id() as isize;
        }
        if f == F::TabOverlap as i32 {
            return self.bar().overlap() as isize;
        }
        let s = self.st.borrow();
        (match f {
            x if x == F::TabBarHeight as i32 => s.bar_height,
            x if x == F::TabBarOverlap as i32 => s.bar_overlap,
            x if x == F::BottomTabTopOffset as i32 => s.bottom_top_offset,
            x if x == F::BottomTabLeftOffset as i32 => s.bottom_left_offset,
            x if x == F::UserHasInteracted as i32 => s.user_has_interacted as i32,
            _ => 0,
        }) as isize
    }

    pub fn set_field(&self, f: i32, v: isize) {
        type F = EeWonTabField;
        if f == F::TabOverlap as i32 {
            self.bar().set_overlap(v as i32);
            return;
        }
        let mut s = self.st.borrow_mut();
        let v = v as i32;
        match f {
            x if x == F::TabBarHeight as i32 => s.bar_height = v,
            x if x == F::TabBarOverlap as i32 => s.bar_overlap = v,
            x if x == F::BottomTabTopOffset as i32 => s.bottom_top_offset = v,
            x if x == F::BottomTabLeftOffset as i32 => s.bottom_left_offset = v,
            x if x == F::UserHasInteracted as i32 => s.user_has_interacted = v != 0,
            _ => {}
        }
    }

    /// The destructor's statements: the arrows given back (the right one first: the members' reverse order); the
    /// bar no longer reaches this object.
    pub fn destroy(&self) {
        let (l, r) = {
            let mut s = self.st.borrow_mut();
            (std::mem::take(&mut s.left), std::mem::take(&mut s.right))
        };
        if r != 0 {
            self.o.op(Op::ChildRelease, r, 0, 0, 0);
        }
        if l != 0 {
            self.o.op(Op::ChildRelease, l, 0, 0, 0);
        }
        let bar = self.st.borrow().bar;
        if !bar.is_null() {
            // SAFETY: the bar is still held by the C++ control's member (its members go after this body)
            unsafe { (*bar).ctrl.set(std::ptr::null()) };
        }
    }

    pub fn call(&self, call: EeWonTabCall, a: isize, b: isize) -> isize {
        type C = EeWonTabCall;
        let (ai, bb) = (a as i32, b != 0);
        match call {
            C::SizeChanged => self.size_changed(),
            C::DesiredSize => {
                let (w, h) = self.desired_size();
                // SAFETY: the C++ passes two writable ints
                unsafe {
                    *(a as *mut i32) = w;
                    *(b as *mut i32) = h;
                }
            }
            C::KeyDown => return self.key_down(ai) as isize,
            C::AddedToParent => self.added_to_parent(),
            C::HandleComponentEvent => self.handle_component_event(a),
            C::TabFocus => return self.tab_focus(a != 0, bb) as isize,
            C::CopyAttributes => {
                if a != 0 {
                    // SAFETY: the other control's live Rust object (the C++ passes its member)
                    self.copy_attributes(unsafe { &*(a as *const EeWonTabCtrl) });
                }
            }
            C::SetTabBarOnRight => self.st.borrow_mut().on_right = a != 0,
            C::SetArrowsOnLeft => self.st.borrow_mut().arrows_on_left = a != 0,
            C::SetTabArrows => self.set_tab_arrows(a, b),
            C::SetCurTab => self.bar().set_cur_tab(ai, bb),
            C::SetCurTabByName => self.bar().set_cur_tab_by_name(a, bb),
            C::GetTabIdForContainer => return self.tab_id_for_container(a) as isize,
            C::EnableTab => self.bar().enable_tab(ai, bb),
            C::AddTab => self.add_tab(a, b),
            C::RemoveTab => self.remove_tab(ai),
            C::GetField => return self.field(ai),
            C::SetField => self.set_field(ai, b),
        }
        0
    }
}

/// A new Rust TabBar for the C++ `component` over its container's Rust object `ct`, the bar of the control `ctrl`
/// (NULL for none), its calls out through `host`.
///
/// # Safety
/// `ct` is the component's live container object and outlives the bar's; `ctrl` (when not NULL) is live until it
/// is freed (it then leaves the bar).
#[no_mangle]
pub unsafe extern "C" fn ee_won_tabbar_new(
    component: *mut c_void,
    host: EeWonGuiHostFn,
    ct: *const EeWonContainer,
    ctrl: *const EeWonTabCtrl,
) -> *mut EeWonTabBar {
    Box::into_raw(Box::new(EeWonTabBar::new(component, host, ct, ctrl)))
}

/// The destructor's statements (the buttons given back), then the object freed.
///
/// # Safety
/// `bar` came from `ee_won_tabbar_new` (or is NULL) and is not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_won_tabbar_free(bar: *mut EeWonTabBar) {
    if !bar.is_null() {
        // SAFETY: the contract
        unsafe {
            (*bar).destroy();
            drop(Box::from_raw(bar));
        }
    }
}

/// One of TabBar's virtuals (`EeWonTabBarCall`).
///
/// # Safety
/// `bar` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_tabbar_call(bar: *const EeWonTabBar, call: EeWonTabBarCall, a: isize) -> isize {
    // SAFETY: live (the contract); a shared reference: the calls out re-enter through these functions
    unsafe { (*bar).call(call, a) }
}

/// A new Rust TabCtrl for the C++ `component` over its container's Rust object `ct`, its calls out through `host`.
///
/// # Safety
/// `ct` is the component's live container object and outlives the control's.
#[no_mangle]
pub unsafe extern "C" fn ee_won_tab_new(
    component: *mut c_void,
    host: EeWonGuiHostFn,
    ct: *const EeWonContainer,
) -> *mut EeWonTabCtrl {
    Box::into_raw(Box::new(EeWonTabCtrl::new(component, host, ct)))
}

/// The constructor's statements over its made members (`parts`).
///
/// # Safety
/// `tab` is live; `parts` is readable, its bar the live bottom bar's Rust object, held by the control's member.
#[no_mangle]
pub unsafe extern "C" fn ee_won_tab_init(tab: *const EeWonTabCtrl, parts: *const EeWonTabParts) {
    // SAFETY: the contract
    unsafe { (*tab).init(&*parts) }
}

/// The destructor's statements (the arrows given back), then the object freed.
///
/// # Safety
/// `tab` came from `ee_won_tab_new` (or is NULL) and is not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_won_tab_free(tab: *mut EeWonTabCtrl) {
    if !tab.is_null() {
        // SAFETY: the contract
        unsafe {
            (*tab).destroy();
            drop(Box::from_raw(tab));
        }
    }
}

/// One of TabCtrl's methods (`EeWonTabCall`).
///
/// # Safety
/// `tab` is live; the call's arguments as documented.
#[no_mangle]
pub unsafe extern "C" fn ee_won_tab_call(tab: *const EeWonTabCtrl, call: EeWonTabCall, a: isize, b: isize) -> isize {
    // SAFETY: live (the contract); a shared reference: the calls out re-enter through these functions
    unsafe { (*tab).call(call, a, b) }
}

#[cfg(test)]
#[path = "tabobj_tests.rs"]
mod tests;
