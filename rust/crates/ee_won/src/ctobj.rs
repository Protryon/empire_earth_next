//! WONGUI's Container as a whole Rust object (R6 WON round 26): the base of every lobby screen, dialog, list, scroll
//! area and control group. The original's Container.cpp, ContainerChildLifecycle.cpp, ContainerInteraction.cpp,
//! ContainerLifetime.cpp, ContainerOrdering.cpp, ContainerSetup.cpp, ContainerTimerCursor.cpp and `MouseAt` with
//! their state: the child list (each child held by reference, as the original's `SmartPtr` list), the mouse and
//! focus children (borrowed), the timer children (held, ordered by address as the original's `std::set`), the next
//! focus id, the current cursor (held), the wheel, tab, dialog-key, child-input and removal flags and the modal count.
//!
//! The C++ `Container` stays the type every component holds (its vtable the subclasses' overrides, the child layout
//! manager a C++ object it points at); its virtuals and methods are one-line forwarders to `ee_won_ct_call`. Every
//! virtual the original called on itself (FindChildAt, SetCursor, Invalidate, TabFocus, RemoveChild, MouseMove) goes
//! through the C++ object, so the subclasses' overrides run, and so does every call on a child, the parent and the
//! `Component` base. A child's place in the list was its `mChildListItr`; the list is a linked list of nodes here
//! (an erased node keeps its links, as the original's iteration around a removal), found by the child's address.
//! `guievent` and `guifocus` keep the decisions they had before.
//!
//! A ScrollAreaViewport is a container with its scroll area (`SetViewportArea`): its ChildInvalidated repaints the
//! area when the child's bounds, moved up through nested viewports, are in view (ScrollAreaViewport.cpp).

use std::cell::RefCell;
use std::collections::BTreeSet;
use std::ffi::c_void;

use crate::guictl::EeWonRect4;
use crate::guidraw::{EeWonDrawOp, EE_WON_DRAW_POP_CLIP, EE_WON_DRAW_PUSH_CLIP, EE_WON_DRAW_TRANSLATE};
use crate::guievent;
use crate::guifocus;
use crate::guihost::{EeWonCtVirt as V, EeWonGuiHostFn, EeWonGuiOp as Op};

const FLAG_INVISIBLE: isize = 0x1;
const FLAG_HAS_FOCUS: isize = 0x10;
const FLAG_NO_PARENT_CURSOR: isize = 0x40;
const FLAG_DISABLED: isize = 0x80;
const FLAG_GRAB_BG: isize = 0x100;
const FLAG_ALWAYS_ON_TOP: isize = 0x1000;
const FLAG_CLIP: isize = 0x2000;
const FLAG_MODAL_OK: isize = 0x8000;

/// The inward calls (the C++ shell's virtuals and methods; `a`, `b`, `c` as each says).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonCtCall {
    /// `Virtual08` (SizeChanged).
    SizeChanged = 1,
    /// `GetDesiredSize(*a, *b)` (`a`, `b` point at i32s).
    DesiredSize,
    /// `PrePaint(*a)` (`a` the Graphics).
    PrePaint,
    /// `Paint(*a)`.
    Paint,
    /// `EnableControlsById(*a, b != 0)` (`a` the `ControlIdSet`).
    EnableControlsById,
    NotifyRootVisibility,
    /// `HitTest(a, b)` (0 or 1).
    HitTest,
    /// `FindChildAt(a, b)` (the child or 0).
    FindChildAt,
    UpdateMouseChild,
    MouseAt,
    MouseEnter,
    MouseExit,
    MouseMove,
    MouseDrag,
    /// `MouseUp(a, b, c)`.
    MouseUp,
    /// `MouseDown(a, b, c)`.
    MouseDown,
    /// `MouseWheel(a)` (0 or 1).
    MouseWheel,
    MouseHover,
    /// `AddChild(a)`.
    AddChild,
    /// `AddChildEx(a, b)`.
    AddChildEx,
    HandleBroadcastEvent,
    /// `FindChildById(a)` (the component or 0).
    FindChildById,
    EnterModal,
    LeaveModal,
    RemoveChild,
    RemoveAllChildren,
    BringToTop,
    SetFocusChild,
    ChildStateChanged,
    /// `ChildInvalidated(a)` (0 or 1).
    ChildInvalidated,
    MarkInvalid,
    ClearGrabBG,
    /// `TabFocus(a != 0, b != 0)` (0 or 1).
    TabFocus,
    KeyDown,
    KeyUp,
    KeyChar,
    CanClose,
    GotFocus,
    LostFocus,
    AddedToParent,
    AddTimerChild,
    /// `TimerEvent(a)` (0 or 1).
    TimerEvent,
    /// `SetCursor(a)` (a `Cursor*`).
    SetCursor,
    SetFocusToLastFocusChild,
    /// `EnsureOnTopOf(a, b)` (bottom, top).
    EnsureOnTopOf,
    /// `GetNextFocusId()`.
    NextFocusId,
    /// `NextChild(a, b != 0, c != 0)` (Component::GetNextSibling's walk; the child or 0).
    NextChild,
    /// `if (mFocusChild) { mFocusChild->LostFocus(); mFocusChild = NULL; }`.
    DropFocusChild,
    /// the field `a` (`EeWonCtField`).
    GetField,
    /// the field `a` = `b`.
    SetField,
    /// a viewport's scroll area `a` (the ScrollAreaViewport constructor).
    SetViewportArea,
    /// ScrollAreaViewport::InvalidateIfRectVisible(*a) (`a` a `WONRectangle`, moved up as the original's).
    VpInvalidateIfRectVisible,
    /// ScrollAreaViewport::ChildInvalidated(a) (0).
    VpChildInvalidated,
}

/// The container's fields its users read and set.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonCtField {
    MouseChild = 1,
    FocusChild,
    CurrentCursor,
    MouseWheelGoesToFocusChild,
    IsolateTabFocus,
    HasUsedDialogKeys,
    OnlyChildInput,
    ModalCount,
    NextFocusId,
    FrontChild,
    BackChild,
    NumChildren,
    /// a viewport's scroll area (0 for a plain container).
    ViewportArea,
}

const HEAD: u32 = 0;

#[derive(Debug, Clone, Copy)]
struct Node {
    comp: isize,
    prev: u32,
    next: u32,
}

/// The child list: a circular doubly linked list of nodes with a head (the original's `std::list`), so iteration
/// around a removal or an insertion made by a callee goes on as the original's did.
#[derive(Debug)]
struct List {
    nodes: Vec<Node>,
    free: Vec<u32>,
    len: usize,
}

impl List {
    fn new() -> Self {
        List { nodes: vec![Node { comp: 0, prev: HEAD, next: HEAD }], free: Vec::new(), len: 0 }
    }
    fn begin(&self) -> u32 {
        self.nodes[HEAD as usize].next
    }
    fn next(&self, i: u32) -> u32 {
        self.nodes[i as usize].next
    }
    fn prev(&self, i: u32) -> u32 {
        self.nodes[i as usize].prev
    }
    fn comp(&self, i: u32) -> isize {
        self.nodes[i as usize].comp
    }
    fn front(&self) -> isize {
        self.comp(self.begin())
    }
    fn back(&self) -> isize {
        self.comp(self.prev(HEAD))
    }
    /// A new node holding `comp` before `before`; its index.
    fn insert(&mut self, before: u32, comp: isize) -> u32 {
        let prev = self.prev(before);
        let n = Node { comp, prev, next: before };
        let i = match self.free.pop() {
            Some(i) => {
                self.nodes[i as usize] = n;
                i
            }
            None => {
                self.nodes.push(n);
                (self.nodes.len() - 1) as u32
            }
        };
        self.nodes[prev as usize].next = i;
        self.nodes[before as usize].prev = i;
        self.len += 1;
        i
    }
    /// Unlinks the node `i` (it keeps its own links).
    fn erase(&mut self, i: u32) {
        let Node { prev, next, .. } = self.nodes[i as usize];
        self.nodes[prev as usize].next = next;
        self.nodes[next as usize].prev = prev;
        self.free.push(i);
        self.len -= 1;
    }
    fn find(&self, comp: isize) -> Option<u32> {
        let mut i = self.begin();
        while i != HEAD {
            if self.comp(i) == comp {
                return Some(i);
            }
            i = self.next(i);
        }
        None
    }
    fn all(&self) -> Vec<isize> {
        let mut v = Vec::with_capacity(self.len);
        let mut i = self.begin();
        while i != HEAD {
            v.push(self.comp(i));
            i = self.next(i);
        }
        v
    }
    fn clear(&mut self) {
        *self = List::new();
    }
}

#[derive(Debug)]
struct State {
    list: List,
    mouse: isize,
    focus: isize,
    /// the timer children by address (`SmartPtr<Component>::Comp`: the raw pointers, unsigned)
    timers: BTreeSet<usize>,
    next_focus_id: u8,
    cursor: isize,
    wheel_to_focus: bool,
    isolate_tab: bool,
    used_dialog_keys: bool,
    only_child_input: bool,
    removing_all: bool,
    modal: i32,
    /// a viewport's scroll area (borrowed), 0 for a plain container
    viewport_area: isize,
}

/// The Rust Container.
pub struct EeWonContainer {
    comp: *mut c_void,
    host: EeWonGuiHostFn,
    st: RefCell<State>,
}

impl EeWonContainer {
    /// The constructor's state: no child, no mouse or focus child, no cursor, no modal state, the wheel to the mouse
    /// child unless `wheel_to_focus` (the class default).
    pub fn new(comp: *mut c_void, host: EeWonGuiHostFn, wheel_to_focus: bool) -> Self {
        EeWonContainer {
            comp,
            host,
            st: RefCell::new(State {
                list: List::new(),
                mouse: 0,
                focus: 0,
                timers: BTreeSet::new(),
                next_focus_id: 0,
                cursor: 0,
                wheel_to_focus,
                isolate_tab: false,
                used_dialog_keys: false,
                only_child_input: false,
                removing_all: false,
                modal: 0,
                viewport_area: 0,
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
    fn me(&self) -> isize {
        self.comp as isize
    }
    // ---- the calls out
    fn base(&self, v: V, a: isize, b: isize) -> isize {
        self.op(Op::CtBase, v as isize, a, b, 0)
    }
    fn child(&self, c: isize, v: V, a: isize, b: isize) -> isize {
        self.op(Op::CtChild, c, v as isize, a, b)
    }
    fn parent_call(&self, v: V, a: isize) -> isize {
        self.op(Op::CtParentCall, v as isize, a, 0, 0)
    }
    fn virt(&self, v: V, a: isize, b: isize) -> isize {
        self.op(Op::CtVirt, v as isize, a, b, 0)
    }
    fn hold(&self, c: isize) {
        self.op(Op::ChildHold, c, 0, 0, 0);
    }
    fn release(&self, c: isize) {
        self.op(Op::ChildRelease, c, 0, 0, 0);
    }
    fn draw(&self, g: isize, kind: i32, args: [i32; 4]) {
        let o = EeWonDrawOp { kind, a: args[0], b: args[1], c: args[2], d: args[3], ..EeWonDrawOp::default() };
        self.op(Op::Draw, g, &o as *const EeWonDrawOp as isize, 0, 0);
    }
    // ---- the pure reads
    fn bits(&self, c: isize) -> isize {
        self.op(Op::CtBits, c, 0, 0, 0)
    }
    fn flag(&self, c: isize, f: isize) -> bool {
        self.bits(c) & f != 0
    }
    fn visible(&self, c: isize) -> bool {
        !self.flag(c, FLAG_INVISIBLE)
    }
    fn disabled(&self, c: isize) -> bool {
        self.flag(c, FLAG_DISABLED)
    }
    fn parent_of(&self, c: isize) -> isize {
        self.op(Op::CtParentOf, c, 0, 0, 0)
    }
    fn left(&self, c: isize) -> i32 {
        self.op(Op::ChildLeft, c, 0, 0, 0) as i32
    }
    fn top(&self, c: isize) -> i32 {
        self.op(Op::ChildTop, c, 0, 0, 0) as i32
    }
    fn contains(&self, c: isize, x: i32, y: i32) -> bool {
        self.op(Op::CtContains, c, x as isize, y as isize, 0) != 0
    }
    fn timer_flags(&self, c: isize) -> i32 {
        self.op(Op::CtTimerFlags, c, 0, 0, 0) as i32
    }
    fn invalid_state(&self, c: isize) -> i32 {
        self.op(Op::CtInvalidState, c, 0, 0, 0) as i32
    }
    fn mouse_pos(&self) -> (i32, i32) {
        let (mut x, mut y) = (0i32, 0i32);
        self.op(Op::GetMousePos, &mut x as *mut i32 as isize, &mut y as *mut i32 as isize, 0, 0);
        (x, y)
    }
    fn has_focus(&self) -> bool {
        self.flag(self.me(), FLAG_HAS_FOCUS)
    }
    // ---- the state
    fn mouse(&self) -> isize {
        self.st.borrow().mouse
    }
    fn focus(&self) -> isize {
        self.st.borrow().focus
    }
    fn modal(&self) -> i32 {
        self.st.borrow().modal
    }
    fn children(&self) -> Vec<isize> {
        self.st.borrow().list.all()
    }
    fn is_empty(&self) -> bool {
        self.st.borrow().list.len == 0
    }

    /// `for (i = begin; i != end; ++i) f((*i).get())`: the next node is read after the call, as the original's `++i`.
    fn each(&self, mut f: impl FnMut(isize)) {
        let mut i = self.st.borrow().list.begin();
        while i != HEAD {
            let c = self.st.borrow().list.comp(i);
            f(c);
            i = self.st.borrow().list.next(i);
        }
    }

    // ---- Container.cpp

    pub fn size_changed(&self) {
        self.base(V::SizeChanged, 0, 0);
        self.op(Op::CtLayout, 0, 0, 0, 0);
    }

    pub fn desired_size(&self) -> (i32, i32) {
        let rects: Vec<[i32; 4]> = self
            .children()
            .into_iter()
            .map(|c| {
                [
                    self.left(c),
                    self.top(c),
                    self.op(Op::ChildWidth, c, 0, 0, 0) as i32,
                    self.op(Op::ChildHeight, c, 0, 0, 0) as i32,
                ]
            })
            .collect();
        guievent::desired_size(&rects)
    }

    pub fn pre_paint(&self, g: isize) {
        if !self.is_empty() {
            let mut i = HEAD;
            loop {
                let c = {
                    let s = self.st.borrow();
                    i = s.list.prev(i);
                    s.list.comp(i)
                };
                if self.child(c, V::NeedsPaint, 0, 0) != 0 {
                    self.draw(g, EE_WON_DRAW_TRANSLATE, [self.left(c), self.top(c), 0, 0]);
                    self.child(c, V::PrePaint, g, 0);
                    self.draw(
                        g,
                        EE_WON_DRAW_TRANSLATE,
                        [self.left(c).wrapping_neg(), self.top(c).wrapping_neg(), 0, 0],
                    );
                }
                if i == self.st.borrow().list.begin() {
                    break;
                }
            }
        }
        self.base(V::PrePaint, g, 0);
    }

    pub fn enable_controls_by_id(&self, ids: isize, enabled: bool) {
        self.base(V::EnableControlsById, ids, enabled as isize);
        self.each(|c| {
            self.child(c, V::EnableControlsById, ids, enabled as isize);
        });
    }

    pub fn notify_root_visibility(&self, visible: bool) {
        self.base(V::NotifyRootVisibility, visible as isize, 0);
        self.st.borrow_mut().used_dialog_keys = false;
        self.each(|c| {
            if self.visible(c) {
                self.child(c, V::NotifyRootVisibility, visible as isize, 0);
            }
        });
    }

    /// The child hit at the point from the top (the original's loop from the end).
    fn top_hit(&self, x: i32, y: i32) -> Option<isize> {
        let mut i = HEAD;
        loop {
            let c = {
                let s = self.st.borrow();
                i = s.list.prev(i);
                s.list.comp(i)
            };
            if self.contains(c, x, y) {
                let (cx, cy) = (x.wrapping_sub(self.left(c)), y.wrapping_sub(self.top(c)));
                if self.child(c, V::HitTest, cx as isize, cy as isize) != 0 {
                    return Some(c);
                }
            }
            if i == self.st.borrow().list.begin() {
                return None;
            }
        }
    }

    pub fn hit_test(&self, x: i32, y: i32) -> bool {
        if self.base(V::HitTest, x as isize, y as isize) == 0 {
            return false;
        }
        if self.st.borrow().only_child_input {
            // the original has no empty-list guard (it read the list's head as a child): no child, no hit
            if self.is_empty() {
                return false;
            }
            return self.top_hit(x, y).is_some();
        }
        true
    }

    pub fn find_child_at(&self, x: i32, y: i32) -> isize {
        if self.is_empty() {
            return 0;
        }
        match self.top_hit(x, y) {
            Some(c) => {
                let bits = self.bits(c);
                if guifocus::hit_child(bits & FLAG_DISABLED != 0, self.modal() > 0, bits as u32) {
                    c
                } else {
                    0
                }
            }
            None => 0,
        }
    }

    pub fn update_mouse_child(&self, x: i32, y: i32) {
        let c = self.virt(V::FindChildAt, x as isize, y as isize);
        let old = self.mouse();
        let steps = guievent::mouse_child(c == old, old != 0, c != 0);
        if c == old {
            return;
        }
        if steps & 1 != 0 {
            self.child(old, V::MouseExit, 0, 0);
            self.st.borrow_mut().mouse = c;
            let cursor = self.op(Op::CtDefaultCursor, 0, 0, 0, 0);
            self.virt(V::SetCursor, cursor, 0);
        } else {
            self.st.borrow_mut().mouse = c;
        }
        if steps & 2 != 0 {
            let (cx, cy) = (x.wrapping_sub(self.left(c)), y.wrapping_sub(self.top(c)));
            self.child(c, V::MouseEnter, cx as isize, cy as isize);
        }
    }

    pub fn mouse_enter(&self, x: i32, y: i32) {
        self.base(V::MouseEnter, x as isize, y as isize);
        self.update_mouse_child(x, y);
    }

    pub fn mouse_exit(&self) {
        self.base(V::MouseExit, 0, 0);
        let m = self.mouse();
        if m != 0 {
            self.child(m, V::MouseExit, 0, 0);
            self.st.borrow_mut().mouse = 0;
        }
        self.set_cursor_field(0);
    }

    fn to_child(&self, c: isize, x: i32, y: i32) -> (isize, isize) {
        (x.wrapping_sub(self.left(c)) as isize, y.wrapping_sub(self.top(c)) as isize)
    }

    pub fn mouse_move(&self, x: i32, y: i32) {
        self.update_mouse_child(x, y);
        let c = self.mouse();
        if c != 0 {
            let (cx, cy) = self.to_child(c, x, y);
            self.child(c, V::MouseMove, cx, cy);
        }
    }

    pub fn mouse_drag(&self, x: i32, y: i32) {
        let c = self.mouse();
        if c != 0 {
            let (cx, cy) = self.to_child(c, x, y);
            self.child(c, V::MouseDrag, cx, cy);
        }
    }

    pub fn mouse_up(&self, x: i32, y: i32, button: i32) {
        let old = self.mouse();
        self.update_mouse_child(x, y);
        let c = self.mouse();
        if guievent::release_to_child(c != 0, c == old) {
            let (cx, cy) = self.to_child(c, x, y);
            self.op(Op::CtChildMouseUp, c, cx, cy, button as isize);
        }
    }

    pub fn mouse_down(&self, x: i32, y: i32, button: i32) {
        self.update_mouse_child(x, y);
        let c = self.mouse();
        if c != 0 {
            let (cx, cy) = self.to_child(c, x, y);
            self.op(Op::CtChildMouseDown, c, cx, cy, button as isize);
        }
    }

    /// `FindTop(i)`: the node at or below `i` under the always-on-top children.
    fn find_top(&self, i: u32) -> u32 {
        let mut nodes = Vec::new();
        {
            let s = self.st.borrow();
            let mut j = s.list.begin();
            while j != i {
                nodes.push(j);
                j = s.list.next(j);
            }
        }
        let on_top: Vec<bool> = nodes
            .iter()
            .map(|&n| {
                let c = self.st.borrow().list.comp(n);
                self.flag(c, FLAG_ALWAYS_ON_TOP)
            })
            .collect();
        let place = guievent::find_top(&on_top, on_top.len());
        if place < nodes.len() {
            nodes[place]
        } else {
            i
        }
    }

    /// The new node for `c` (held) at FindTop(`from`).
    fn insert_child(&self, from: u32, c: isize) {
        self.hold(c);
        let at = self.find_top(from);
        self.st.borrow_mut().list.insert(at, c);
    }

    fn detach(&self, c: isize) {
        if self.parent_of(c) != 0 {
            self.op(Op::CtDetach, c, 0, 0, 0);
        }
        self.op(Op::CtSetParent, c, 1, 0, 0);
    }

    pub fn add_child(&self, c: isize) {
        self.detach(c);
        self.insert_child(HEAD, c);
        self.child(c, V::AddedToParent, 0, 0);
    }

    pub fn add_child_ex(&self, c: isize, position: i32) {
        self.detach(c);
        let i = {
            let s = self.st.borrow();
            let mut i = s.list.begin();
            let mut n = 0;
            while n < position && i != HEAD {
                i = s.list.next(i);
                n += 1;
            }
            i
        };
        self.insert_child(i, c);
        self.child(c, V::AddedToParent, 0, 0);
    }

    pub fn handle_broadcast_event(&self, event: isize) {
        self.base(V::HandleBroadcastEvent, event, 0);
        self.each(|c| {
            self.child(c, V::HandleBroadcastEvent, event, 0);
        });
    }

    /// `FindChildById`: level by level through the containers of the tree, this one first.
    pub fn find_child_by_id(&self, id: i32) -> isize {
        let mut pending = vec![self.me()];
        let mut k = 0;
        while k < pending.len() {
            let c = pending[k];
            k += 1;
            if self.op(Op::ChildControlId, c, 0, 0, 0) as i32 == id {
                return c;
            }
            let ct = self.op(Op::CtContainerOf, c, 0, 0, 0) as *const EeWonContainer;
            if ct.is_null() {
                continue;
            }
            // SAFETY: the host answers a live container's Rust object; the borrow ends here
            pending.extend(unsafe { (*ct).children() });
        }
        0
    }

    // ---- ContainerChildLifecycle.cpp

    pub fn enter_modal(&self) {
        self.st.borrow_mut().modal += 1;
        let m = self.mouse();
        if m != 0 && !self.flag(m, FLAG_MODAL_OK) {
            self.mouse_at();
        }
        let f = self.focus();
        if f != 0 && !self.flag(f, FLAG_MODAL_OK) {
            self.set_focus_to_last_focus_child();
        }
    }

    pub fn leave_modal(&self) {
        if self.modal() <= 0 {
            return;
        }
        self.st.borrow_mut().modal = 0;
        self.mouse_at();
        if self.focus() == 0 {
            self.set_focus_to_last_focus_child();
        }
    }

    pub fn remove_child(&self, c: isize) {
        if self.parent_of(c) != self.me() {
            return;
        }
        let removing_all = self.st.borrow().removing_all;
        if !removing_all && self.flag(c, FLAG_GRAB_BG) {
            self.op(Op::ChildInvalidate, c, 0, 0, 0);
            if self.op(Op::CtRoot, 0, 0, 0, 0) != 0 {
                let grabber = self.op(Op::CtGrabBG, c, 0, 0, 0);
                self.op(Op::CtAddBG, grabber, 0, 0, 0);
            }
        } else {
            self.op(Op::Invalidate, 0, 0, 0, 0);
        }
        self.hold(c);
        // a child whose list a reset cleared is not in it (the original erased a dead iterator)
        let node = self.st.borrow().list.find(c);
        if let Some(n) = node {
            self.st.borrow_mut().list.erase(n);
            self.release(c);
        }
        if !removing_all {
            self.op(Op::CtLayout, 1, c, 0, 0);
        }
        if c == self.focus() {
            self.set_focus_to_last_focus_child();
        }
        if c == self.mouse() {
            self.mouse_at();
        }
        self.op(Op::CtSetParent, c, 0, 0, 0);
        self.release(c);
    }

    pub fn remove_all_children(&self) {
        self.st.borrow_mut().removing_all = true;
        loop {
            let front = {
                let s = self.st.borrow();
                if s.list.len == 0 {
                    break;
                }
                s.list.front()
            };
            self.virt(V::RemoveChild, front, 0);
        }
        self.op(Op::CtLayout, 2, 0, 0, 0);
        self.st.borrow_mut().removing_all = false;
        self.virt(V::SetCursor, 0, 0);
    }

    pub fn bring_to_top(&self, c: isize) {
        if self.parent_of(c) != self.me() {
            return;
        }
        self.hold(c);
        let grabber = self.op(Op::CtGrabBG, c, 0, 0, 0);
        let mut need_invalidate = true;
        if grabber != 0 {
            self.op(Op::ChildInvalidate, c, 0, 0, 0);
            self.op(Op::CtAddBG, grabber, 0, 0, 0);
            need_invalidate = false;
        }
        let node = self.st.borrow().list.find(c);
        if let Some(n) = node {
            self.st.borrow_mut().list.erase(n);
            self.release(c);
        }
        self.insert_child(HEAD, c);
        if need_invalidate {
            self.op(Op::Invalidate, 0, 0, 0, 0);
        }
        self.release(c);
    }

    pub fn set_focus_child(&self, c: isize) {
        let me = self.me();
        if !self.visible(me) || self.disabled(me) {
            return;
        }
        if self.focus() == c && self.has_focus() {
            return;
        }
        if self.modal() > 0 && c != 0 && !self.flag(c, FLAG_MODAL_OK) {
            return;
        }
        let f = self.focus();
        if f != 0 && self.has_focus() {
            self.child(f, V::LostFocus, 0, 0);
        }
        self.st.borrow_mut().focus = c;
        let id = self.next_focus_id();
        // the original wrote the id before its NULL test (a NULL child crashed there)
        if c == 0 {
            return;
        }
        self.op(Op::CtSetFocusId, c, id as isize, 0, 0);
        let parent = self.parent_of(me);
        if !self.has_focus() && parent != 0 {
            self.parent_call(V::SetFocusChild, 0);
        } else {
            self.op(Op::ChildSetFlags, me, FLAG_HAS_FOCUS, 1, 0);
            if parent == 0 && self.op(Op::CtRoot, 0, 0, 0, 0) == me && self.op(Op::CtRootFocused, 0, 0, 0, 0) == 0 {
                return;
            }
            self.child(c, V::GotFocus, 0, 0);
        }
    }

    pub fn child_state_changed(&self, c: isize) {
        let bad = self.disabled(c) || !self.visible(c);
        if self.focus() == c && bad {
            self.child(c, V::LostFocus, 0, 0);
            self.st.borrow_mut().focus = 0;
        }
        if self.op(Op::CtRoot, 0, 0, 0, 0) == 0 {
            return;
        }
        let (x, y) = self.mouse_pos();
        if self.contains(c, x, y) || (self.mouse() == c && bad) {
            self.update_mouse_child(x, y);
        }
    }

    // ---- ContainerInteraction.cpp

    pub fn paint(&self, g: isize) {
        let scheme = self.op(Op::CtScheme, g, 0, 0, 0);
        self.base(V::Paint, g, 0);
        self.each(|c| {
            if self.invalid_state(c) == 0 || !self.visible(c) {
                return;
            }
            self.draw(g, EE_WON_DRAW_TRANSLATE, [self.left(c), self.top(c), 0, 0]);
            if self.flag(c, FLAG_CLIP) {
                let (w, h) = (self.op(Op::ChildWidth, c, 0, 0, 0) as i32, self.op(Op::ChildHeight, c, 0, 0, 0) as i32);
                self.draw(g, EE_WON_DRAW_PUSH_CLIP, [0, 0, w, h]);
            }
            let own = self.op(Op::CtOwnScheme, c, 0, 0, 0);
            self.op(Op::CtSetScheme, g, if own != 0 { own } else { scheme }, 0, 0);
            self.child(c, V::Paint, g, 0);
            if self.flag(c, FLAG_CLIP) {
                self.draw(g, EE_WON_DRAW_POP_CLIP, [0; 4]);
            }
            self.draw(g, EE_WON_DRAW_TRANSLATE, [self.left(c).wrapping_neg(), self.top(c).wrapping_neg(), 0, 0]);
        });
    }

    pub fn child_invalidated(&self, changed: isize) -> bool {
        let me = self.me();
        if self.invalid_state(me) == 2 {
            return false;
        }
        self.op(Op::CtSetInvalidState, me, 1, 0, 0);
        let mut higher = false;
        let mut i = HEAD;
        while i != self.st.borrow().list.begin() {
            let c = {
                let s = self.st.borrow();
                i = s.list.prev(i);
                s.list.comp(i)
            };
            if c == changed {
                break;
            }
            if self.op(Op::CtIntersects, c, changed, 0, 0) != 0 && self.visible(c) {
                self.op(Op::ChildInvalidate, c, 0, 0, 0);
                higher = true;
            }
        }
        if self.parent_of(me) != 0 {
            self.parent_call(V::ChildInvalidated, 0);
        }
        higher
    }

    pub fn mark_invalid(&self) {
        let me = self.me();
        if self.invalid_state(me) == 2 {
            return;
        }
        self.base(V::MarkInvalid, 0, 0);
        self.op(Op::CtSetInvalidState, me, 2, 0, 0);
        self.each(|c| {
            self.child(c, V::MarkInvalid, 0, 0);
        });
    }

    pub fn clear_grab_bg(&self) {
        self.base(V::ClearGrabBG, 0, 0);
        self.each(|c| {
            self.child(c, V::ClearGrabBG, 0, 0);
        });
    }

    fn sibling(&self, c: isize, forward: bool, wrap: bool) -> isize {
        self.op(Op::CtNextSibling, c, forward as isize, wrap as isize, 0)
    }

    pub fn tab_focus(&self, forward: bool, init: bool) -> bool {
        let me = self.me();
        let (isolate, has_children) = {
            let s = self.st.borrow();
            (s.isolate_tab, s.list.len != 0)
        };
        let want = self.op(Op::CtWantFocus, 0, 0, 0, 0) != 0;
        match guifocus::tab_focus_start(want, init, isolate, self.bits(me) as u32, has_children) {
            0 => return false,
            1 => {
                self.st.borrow_mut().used_dialog_keys = true;
                self.op(Op::RequestFocus, 0, 0, 0, 0);
                return true;
            }
            _ => {}
        }
        let wrap = isolate;
        let mut c = if init { 0 } else { self.focus() };
        if c == 0 {
            let s = self.st.borrow();
            c = if forward { s.list.front() } else { s.list.back() };
        } else {
            c = self.sibling(c, forward, wrap);
        }
        let mut count: u64 = 0;
        while c != 0 {
            if self.child(c, V::TabFocus, forward as isize, 1) != 0 {
                self.st.borrow_mut().used_dialog_keys = true;
                return true;
            }
            c = self.sibling(c, forward, wrap);
            let len = self.st.borrow().list.len;
            let done = guifocus::tab_focus_done(count, len);
            count = count.wrapping_add(1);
            if done {
                break;
            }
        }
        false
    }

    pub fn key_down(&self, key: i32) -> bool {
        let f = self.focus();
        if f != 0 && self.child(f, V::KeyDown, key as isize, 0) != 0 {
            return true;
        }
        if key != 9 {
            return false;
        }
        if self.is_empty() {
            return false;
        }
        let forward = self.op(Op::KeyMod, 0, 0, 0, 0) & 1 == 0;
        self.virt(V::TabFocus, forward as isize, 0) != 0
    }

    pub fn can_close(&self) -> bool {
        let mut i = self.st.borrow().list.begin();
        while i != HEAD {
            let c = self.st.borrow().list.comp(i);
            if self.child(c, V::CanClose, 0, 0) == 0 {
                return false;
            }
            i = self.st.borrow().list.next(i);
        }
        true
    }

    pub fn got_focus(&self) {
        self.base(V::GotFocus, 0, 0);
        let f = self.focus();
        if f != 0 {
            self.child(f, V::GotFocus, 0, 0);
        }
    }

    pub fn lost_focus(&self) {
        self.base(V::LostFocus, 0, 0);
        let f = self.focus();
        if f != 0 {
            self.child(f, V::LostFocus, 0, 0);
        }
    }

    pub fn key_char(&self, key: i32) -> bool {
        let f = self.focus();
        f != 0 && self.child(f, V::KeyChar, key as isize, 0) != 0
    }

    pub fn key_up(&self, key: i32) -> bool {
        let f = self.focus();
        if f != 0 {
            self.child(f, V::KeyUp, key as isize, 0);
            return true;
        }
        false
    }

    pub fn added_to_parent(&self) {
        self.base(V::AddedToParent, 0, 0);
        self.st.borrow_mut().used_dialog_keys = false;
    }

    pub fn mouse_wheel(&self, amount: i32) -> bool {
        let c = {
            let s = self.st.borrow();
            if s.wheel_to_focus {
                s.focus
            } else {
                s.mouse
            }
        };
        c != 0 && self.child(c, V::MouseWheel, amount as isize, 0) != 0
    }

    pub fn mouse_hover(&self, elapsed: u32) {
        self.base(V::MouseHover, elapsed as isize, 0);
        let m = self.mouse();
        if m != 0 {
            self.child(m, V::MouseHover, elapsed as isize, 0);
        }
    }

    /// `MouseAt` (ControlSetupCompletion.cpp): the current mouse point sent again as a move, when the mouse is in
    /// the window.
    pub fn mouse_at(&self) {
        if self.op(Op::CtMouseInClient, 0, 0, 0, 0) == 0 {
            return;
        }
        let (x, y) = self.mouse_pos();
        self.virt(V::MouseMove, x as isize, y as isize);
    }

    // ---- ContainerTimerCursor.cpp

    pub fn add_timer_child(&self, c: isize) {
        if self.timer_flags(c) == 0 {
            return;
        }
        if self.st.borrow_mut().timers.insert(c as usize) {
            self.hold(c);
        }
        let me = self.me();
        let flags = self.timer_flags(me);
        let request_parent = flags == 0;
        self.op(Op::CtSetTimerFlags, (flags | 1) as isize, 0, 0, 0);
        if request_parent && self.parent_of(me) != 0 {
            self.parent_call(V::AddTimerChild, 0);
        }
    }

    pub fn timer_event(&self, elapsed: i32) -> bool {
        let me = self.me();
        let mut child_needs = false;
        let mut cur = self.st.borrow().timers.iter().next().copied();
        while let Some(k) = cur {
            let c = k as isize;
            if self.timer_flags(c) != 0
                && self.parent_of(c) == me
                && self.child(c, V::TimerEvent, elapsed as isize, 0) != 0
            {
                child_needs = true;
                cur = self.st.borrow().timers.range(k + 1..).next().copied();
            } else {
                self.child(c, V::RequestTimer, 0, 0);
                let removed = {
                    let mut s = self.st.borrow_mut();
                    cur = s.timers.range(k + 1..).next().copied();
                    s.timers.remove(&k)
                };
                if removed {
                    self.release(c);
                }
            }
        }
        if !child_needs {
            let flags = self.timer_flags(me);
            self.op(Op::CtSetTimerFlags, (flags & !1) as isize, 0, 0, 0);
        }
        self.timer_flags(me) != 0
    }

    /// `mCurrentCursor = cursor` (the `SmartPtr` assignment: the old one released, then the new one held).
    fn set_cursor_field(&self, cursor: isize) {
        let old = self.st.borrow().cursor;
        if old == cursor {
            return;
        }
        if old != 0 {
            self.op(Op::CtCursorRelease, old, 0, 0, 0);
        }
        self.st.borrow_mut().cursor = cursor;
        if cursor != 0 {
            self.op(Op::CtCursorHold, cursor, 0, 0, 0);
        }
    }

    pub fn set_cursor(&self, cursor: isize) {
        let m = self.mouse();
        if m != 0 && self.flag(m, FLAG_NO_PARENT_CURSOR) {
            let fallback = self.op(Op::CtDefaultCursor, 0, 0, 0, 0);
            if self.st.borrow().cursor != fallback {
                self.parent_call(V::SetCursor, fallback);
            }
            return;
        }
        let cursor = if cursor == 0 { self.op(Op::CtDefaultCursor, 0, 0, 0, 0) } else { cursor };
        if cursor != self.st.borrow().cursor {
            self.set_cursor_field(cursor);
            if self.parent_of(self.me()) != 0 {
                self.parent_call(V::SetCursor, cursor);
            }
        }
    }

    // ---- ContainerOrdering.cpp

    pub fn set_focus_to_last_focus_child(&self) {
        let mut current = self.st.borrow().next_focus_id;
        let mut skip = false;
        let f = self.focus();
        if f != 0 {
            current = self.op(Op::CtFocusId, f, 0, 0, 0) as u8;
            self.child(f, V::LostFocus, 0, 0);
            self.st.borrow_mut().focus = 0;
            skip = true;
        }
        let children = self.children();
        let ids: Vec<u8> = children.iter().map(|&c| self.op(Op::CtFocusId, c, 0, 0, 0) as u8).collect();
        if let Some(p) = guifocus::last_focus_child(current, skip, &ids) {
            self.child(children[p], V::RequestFocus, 0, 0);
        }
    }

    pub fn ensure_on_top_of(&self, bottom: isize, top: isize) {
        let me = self.me();
        if self.parent_of(bottom) != me || self.parent_of(top) != me {
            return;
        }
        let (nb, nt) = {
            let s = self.st.borrow();
            (s.list.find(bottom), s.list.find(top))
        };
        let (Some(nb), Some(nt)) = (nb, nt) else {
            return;
        };
        {
            let s = self.st.borrow();
            let mut i = nb;
            while i != nt && i != HEAD {
                i = s.list.next(i);
            }
            if i != HEAD {
                return;
            }
        }
        self.hold(bottom);
        self.st.borrow_mut().list.erase(nb);
        self.release(bottom);
        self.hold(bottom);
        self.st.borrow_mut().list.insert(nt, bottom);
        self.release(bottom);
    }

    pub fn next_focus_id(&self) -> u8 {
        let mut s = self.st.borrow_mut();
        s.next_focus_id = guifocus::next_focus_id(s.next_focus_id);
        s.next_focus_id
    }

    /// Component::GetNextSibling's walk over this list.
    pub fn next_child(&self, c: isize, forward: bool, wrap: bool) -> isize {
        let s = self.st.borrow();
        let Some(n) = s.list.find(c) else {
            return 0;
        };
        if forward {
            let i = s.list.next(n);
            if i == HEAD {
                return if wrap { s.list.front() } else { 0 };
            }
            s.list.comp(i)
        } else {
            if n == s.list.begin() {
                return if wrap { s.list.back() } else { 0 };
            }
            s.list.comp(s.list.prev(n))
        }
    }

    // ---- ScrollAreaViewport.cpp

    /// `InvalidateIfRectVisible`: the area repainted when the rectangle is in its view, else the rectangle moved by
    /// this viewport's place and given to the parent viewport.
    pub fn vp_invalidate_if_rect_visible(&self, r: &mut EeWonRect4) {
        let me = self.me();
        let area = self.st.borrow().viewport_area;
        let parent = self.parent_of(me);
        if parent == area {
            if self.op(Op::VpAreaRectVisible, area, r as *mut EeWonRect4 as isize, 0, 0) != 0 {
                self.op(Op::ChildInvalidate, area, 0, 0, 0);
            }
        } else if parent != 0 {
            r.x = r.x.wrapping_add(self.left(me));
            r.y = r.y.wrapping_add(self.top(me));
            let ct = self.op(Op::CtContainerOf, parent, 0, 0, 0) as *const EeWonContainer;
            if !ct.is_null() {
                // SAFETY: the host answers a live container's Rust object (the parent viewport); no borrow is held
                unsafe { (*ct).vp_invalidate_if_rect_visible(r) };
            }
        }
    }

    /// `ChildInvalidated`: the child's bounds through InvalidateIfRectVisible; false.
    pub fn vp_child_invalidated(&self, c: isize) -> bool {
        let mut r = EeWonRect4 {
            x: self.left(c),
            y: self.top(c),
            w: self.op(Op::ChildWidth, c, 0, 0, 0) as i32,
            h: self.op(Op::ChildHeight, c, 0, 0, 0) as i32,
        };
        self.vp_invalidate_if_rect_visible(&mut r);
        false
    }

    pub fn drop_focus_child(&self) {
        let f = self.focus();
        if f != 0 {
            self.child(f, V::LostFocus, 0, 0);
            self.st.borrow_mut().focus = 0;
        }
    }

    pub fn field(&self, f: i32) -> isize {
        type F = EeWonCtField;
        let s = self.st.borrow();
        match f {
            x if x == F::MouseChild as i32 => s.mouse,
            x if x == F::FocusChild as i32 => s.focus,
            x if x == F::CurrentCursor as i32 => s.cursor,
            x if x == F::MouseWheelGoesToFocusChild as i32 => s.wheel_to_focus as isize,
            x if x == F::IsolateTabFocus as i32 => s.isolate_tab as isize,
            x if x == F::HasUsedDialogKeys as i32 => s.used_dialog_keys as isize,
            x if x == F::OnlyChildInput as i32 => s.only_child_input as isize,
            x if x == F::ModalCount as i32 => s.modal as isize,
            x if x == F::NextFocusId as i32 => s.next_focus_id as isize,
            x if x == F::FrontChild as i32 => s.list.front(),
            x if x == F::BackChild as i32 => s.list.back(),
            x if x == F::NumChildren as i32 => s.list.len as isize,
            x if x == F::ViewportArea as i32 => s.viewport_area,
            _ => 0,
        }
    }

    pub fn set_field(&self, f: i32, v: isize) {
        type F = EeWonCtField;
        let mut s = self.st.borrow_mut();
        match f {
            x if x == F::MouseWheelGoesToFocusChild as i32 => s.wheel_to_focus = v != 0,
            x if x == F::IsolateTabFocus as i32 => s.isolate_tab = v != 0,
            x if x == F::HasUsedDialogKeys as i32 => s.used_dialog_keys = v != 0,
            x if x == F::OnlyChildInput as i32 => s.only_child_input = v != 0,
            _ => {}
        }
    }

    /// The destructor: every child's parent cleared (front to back), then, as the original's members went, the
    /// cursor released, the child layout destroyed, the timer children released (in address order: the original's
    /// tree went in its own order) and the children released front to back.
    pub fn destroy(&self) {
        for c in self.children() {
            self.op(Op::CtSetParent, c, 0, 0, 0);
        }
        self.set_cursor_field(0);
        self.op(Op::CtLayout, 3, 0, 0, 0);
        let (timers, children) = {
            let mut s = self.st.borrow_mut();
            let t: Vec<usize> = std::mem::take(&mut s.timers).into_iter().collect();
            let c = s.list.all();
            s.list.clear();
            (t, c)
        };
        for t in timers {
            self.release(t as isize);
        }
        for c in children {
            self.release(c);
        }
    }

    pub fn call(&self, call: EeWonCtCall, a: isize, b: isize, c: isize) -> isize {
        type C = EeWonCtCall;
        let (ai, bi, ci) = (a as i32, b as i32, c as i32);
        match call {
            C::SizeChanged => self.size_changed(),
            C::DesiredSize => {
                let (w, h) = self.desired_size();
                // SAFETY: the C++ passes two writable i32s
                unsafe {
                    *(a as *mut i32) = w;
                    *(b as *mut i32) = h;
                }
            }
            C::PrePaint => self.pre_paint(a),
            C::Paint => self.paint(a),
            C::EnableControlsById => self.enable_controls_by_id(a, b != 0),
            C::NotifyRootVisibility => self.notify_root_visibility(a != 0),
            C::HitTest => return self.hit_test(ai, bi) as isize,
            C::FindChildAt => return self.find_child_at(ai, bi),
            C::UpdateMouseChild => self.update_mouse_child(ai, bi),
            C::MouseAt => self.mouse_at(),
            C::MouseEnter => self.mouse_enter(ai, bi),
            C::MouseExit => self.mouse_exit(),
            C::MouseMove => self.mouse_move(ai, bi),
            C::MouseDrag => self.mouse_drag(ai, bi),
            C::MouseUp => self.mouse_up(ai, bi, ci),
            C::MouseDown => self.mouse_down(ai, bi, ci),
            C::MouseWheel => return self.mouse_wheel(ai) as isize,
            C::MouseHover => self.mouse_hover(a as u32),
            C::AddChild => self.add_child(a),
            C::AddChildEx => self.add_child_ex(a, bi),
            C::HandleBroadcastEvent => self.handle_broadcast_event(a),
            C::FindChildById => return self.find_child_by_id(ai),
            C::EnterModal => self.enter_modal(),
            C::LeaveModal => self.leave_modal(),
            C::RemoveChild => self.remove_child(a),
            C::RemoveAllChildren => self.remove_all_children(),
            C::BringToTop => self.bring_to_top(a),
            C::SetFocusChild => self.set_focus_child(a),
            C::ChildStateChanged => self.child_state_changed(a),
            C::ChildInvalidated => return self.child_invalidated(a) as isize,
            C::MarkInvalid => self.mark_invalid(),
            C::ClearGrabBG => self.clear_grab_bg(),
            C::TabFocus => return self.tab_focus(a != 0, b != 0) as isize,
            C::KeyDown => return self.key_down(ai) as isize,
            C::KeyUp => return self.key_up(ai) as isize,
            C::KeyChar => return self.key_char(ai) as isize,
            C::CanClose => return self.can_close() as isize,
            C::GotFocus => self.got_focus(),
            C::LostFocus => self.lost_focus(),
            C::AddedToParent => self.added_to_parent(),
            C::AddTimerChild => self.add_timer_child(a),
            C::TimerEvent => return self.timer_event(ai) as isize,
            C::SetCursor => self.set_cursor(a),
            C::SetFocusToLastFocusChild => self.set_focus_to_last_focus_child(),
            C::EnsureOnTopOf => self.ensure_on_top_of(a, b),
            C::NextFocusId => return self.next_focus_id() as isize,
            C::NextChild => return self.next_child(a, b != 0, c != 0),
            C::DropFocusChild => self.drop_focus_child(),
            C::GetField => return self.field(ai),
            C::SetField => self.set_field(ai, b),
            C::SetViewportArea => self.st.borrow_mut().viewport_area = a,
            C::VpInvalidateIfRectVisible => {
                // SAFETY: the C++ passes a WONRectangle (x, y, width, height), written back
                let r = unsafe { &mut *(a as *mut EeWonRect4) };
                self.vp_invalidate_if_rect_visible(r);
            }
            C::VpChildInvalidated => return self.vp_child_invalidated(a) as isize,
        }
        0
    }
}

/// A new Rust Container for the C++ `component`, its calls out through `host`; the wheel goes to the focus child
/// when `wheel_to_focus` (the class default).
#[no_mangle]
pub extern "C" fn ee_won_ct_new(
    component: *mut c_void,
    host: EeWonGuiHostFn,
    wheel_to_focus: bool,
) -> *mut EeWonContainer {
    Box::into_raw(Box::new(EeWonContainer::new(component, host, wheel_to_focus)))
}

/// The destructor's statements (children told, cursor, layout, timer children and children released), then the
/// object freed.
///
/// # Safety
/// `ct` came from `ee_won_ct_new` (or is NULL) and is not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_won_ct_free(ct: *mut EeWonContainer) {
    if !ct.is_null() {
        // SAFETY: the contract
        unsafe {
            (*ct).destroy();
            drop(Box::from_raw(ct));
        }
    }
}

/// One of Container's methods (`EeWonCtCall`).
///
/// # Safety
/// `ct` is live; the call's arguments as documented.
#[no_mangle]
pub unsafe extern "C" fn ee_won_ct_call(
    ct: *const EeWonContainer,
    call: EeWonCtCall,
    a: isize,
    b: isize,
    c: isize,
) -> isize {
    // SAFETY: live (the contract); a shared reference: the calls out re-enter through this function
    unsafe { (*ct).call(call, a, b, c) }
}

#[cfg(test)]
#[path = "ctobj_tests.rs"]
pub(crate) mod tests;
