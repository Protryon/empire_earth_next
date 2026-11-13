//! WONGUI's child layouts as whole Rust objects (R6 WON round 31): the layout manager every container owns
//! (`ChildLayoutManager`: its instructions, held, and its layout points, held) and the eight instruction classes
//! (`ChildLayoutInstruction`, `CenterLayout`, `SameSizeLayout`, `ClearRightLayout`, `SpaceEvenlyLayout`,
//! `NoOverlapLayout`, `SizeAtLeastLayout`, `PercentLayout`) with their component lists, their numbers, the group and
//! the enabled flag, every method, `Container::AddChildLayout` and `CenterLayout::Center`.
//!
//! The C++ classes stay the types the lobby makes (`new CenterLayout(...)`, `->Add(...)`, `SmartPtr`s of them): each
//! is a reference-counted shell over one `EeWonLayout`, its methods one-line forwarders. The arithmetic was already
//! `guilayout`'s; this module adds the objects: what each reads, in which order it moves which component, and the
//! manager's list. Every component read and move is an op on the component given as an argument (the host's own
//! component is never read), so a layout object needs no live C++ object of its own except for its references.

use std::cell::{Cell, RefCell};
use std::ffi::c_void;

use crate::guictl::EeWonRect4;
use crate::guihost::{EeWonGuiHostFn, EeWonGuiOp as Op};
use crate::guilayout::{self, EeWonChildLayout, EeWonPercent, EeWonRect};

/// The instruction classes.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonLayoutKind {
    /// `ChildLayoutInstructionBase` itself: Execute does nothing, UsesComponent is false.
    Base = 0,
    /// `ChildLayoutInstruction`: refs child, reference; ints flags, x, y, width, height.
    Child,
    /// `CenterLayout`: ref the centre box (may be NULL); horz, vert; the list.
    Center,
    /// `SameSizeLayout`: horz, vert; the list.
    SameSize,
    /// `ClearRightLayout`: ref the component; int the pad; the list.
    ClearRight,
    /// `SpaceEvenlyLayout`: ref the space box; ints the left and right pads; horz (`mIsHorz`); the list.
    SpaceEvenly,
    /// `NoOverlapLayout`: refs the reference, the target; ints the type, the horizontal and vertical pads; the list.
    NoOverlap,
    /// `SizeAtLeastLayout`: ref the target; ints the horizontal and vertical pads; the list.
    SizeAtLeast,
    /// `PercentLayout`: refs the target and the param's left, top, right, bottom anchors; ints px, py, pw, ph, lp,
    /// tp, rp, bp.
    Percent,
}

/// A new instruction's numbers, as its C++ constructor received them (see `EeWonLayoutKind` for each class's).
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeWonLayoutInit {
    pub kind: EeWonLayoutKind,
    pub refs: [isize; 5],
    pub ints: [i32; 8],
    pub horz: bool,
    pub vert: bool,
}

/// An instruction's inward calls.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonLayoutCall {
    /// `Execute((Component*)a)` (the parent).
    Execute = 1,
    /// `UsesComponent((Component*)a)` (0 or 1).
    UsesComponent,
    /// `Add((Component*)a)`: appended to the list (the C++ returns its own `this`).
    Add,
    /// `mGroupId`.
    GroupId,
    /// `mEnabled` (0 or 1).
    Enabled,
}

/// A child layout instruction.
pub struct EeWonLayout {
    host: EeWonGuiHostFn,
    comp: *mut c_void,
    kind: EeWonLayoutKind,
    refs: [isize; 5],
    ints: [i32; 8],
    horz: bool,
    vert: bool,
    group: Cell<u8>,
    enabled: Cell<bool>,
    /// the component list (`std::list<Component*>`, borrowed components, appended only)
    list: RefCell<Vec<isize>>,
}

impl EeWonLayout {
    pub fn new(comp: *mut c_void, host: EeWonGuiHostFn, init: &EeWonLayoutInit) -> Self {
        EeWonLayout {
            host,
            comp,
            kind: init.kind,
            refs: init.refs,
            ints: init.ints,
            horz: init.horz,
            vert: init.vert,
            group: Cell::new(0),
            enabled: Cell::new(true),
            list: RefCell::new(Vec::new()),
        }
    }

    fn op(&self, op: Op, a: isize, b: isize, c: isize) -> isize {
        match self.host {
            // SAFETY: the host's contract (the component pointers are the C++ caller's)
            Some(h) => unsafe { h(self.comp, op, a, b, c, 0) },
            None => 0,
        }
    }
    fn left(&self, c: isize) -> i32 {
        self.op(Op::ChildLeft, c, 0, 0) as i32
    }
    fn top(&self, c: isize) -> i32 {
        self.op(Op::ChildTop, c, 0, 0) as i32
    }
    fn width(&self, c: isize) -> i32 {
        self.op(Op::ChildWidth, c, 0, 0) as i32
    }
    fn height(&self, c: isize) -> i32 {
        self.op(Op::ChildHeight, c, 0, 0) as i32
    }
    fn bounds(&self, c: isize) -> EeWonRect {
        EeWonRect::new(self.left(c), self.top(c), self.width(c), self.height(c))
    }
    fn set_pos(&self, c: isize, x: i32, y: i32) {
        self.op(Op::ChildSetPos, c, x as isize, y as isize);
    }
    fn set_size(&self, c: isize, w: i32, h: i32) {
        self.op(Op::ChildSetSize, c, w as isize, h as isize);
    }
    fn set_pos_size(&self, c: isize, r: EeWonRect) {
        let mut q = EeWonRect4 { x: r.x, y: r.y, w: r.width, h: r.height };
        self.op(Op::ChildSetPosSize, c, &mut q as *mut EeWonRect4 as isize, 0);
    }
    fn items(&self) -> Vec<isize> {
        self.list.borrow().clone()
    }

    pub fn kind(&self) -> EeWonLayoutKind {
        self.kind
    }
    pub fn group(&self) -> u8 {
        self.group.get()
    }
    pub fn set_group(&self, g: u8) {
        self.group.set(g);
    }
    pub fn enabled(&self) -> bool {
        self.enabled.get()
    }
    pub fn set_enabled(&self, e: bool) {
        self.enabled.set(e);
    }

    pub fn add(&self, c: isize) {
        self.list.borrow_mut().push(c);
    }

    /// `UsesComponent`: only `ChildLayoutInstruction` overrides the base's false.
    pub fn uses_component(&self, c: isize) -> bool {
        self.kind == EeWonLayoutKind::Child && (self.refs[0] == c || self.refs[1] == c)
    }

    /// `Execute(parent)`: the class's statements. Every read is made where the original made it, after the moves
    /// before it (a component listed twice reads its moved place).
    pub fn execute(&self, parent: isize) {
        type K = EeWonLayoutKind;
        match self.kind {
            K::Base => {}
            K::Child => self.execute_child(),
            K::Center => self.execute_center(parent),
            K::SameSize => self.execute_same_size(),
            K::ClearRight => self.execute_clear_right(),
            K::SpaceEvenly => self.execute_space_evenly(parent),
            K::NoOverlap => self.execute_no_overlap(parent),
            K::SizeAtLeast => self.execute_size_at_least(),
            K::Percent => self.execute_percent(parent),
        }
    }

    fn execute_child(&self) {
        let (child, reference) = (self.refs[0], self.refs[1]);
        let c = self.bounds(child);
        let r = self.bounds(reference);
        let l = EeWonChildLayout {
            flags: self.ints[0],
            x: self.ints[1],
            y: self.ints[2],
            width: self.ints[3],
            height: self.ints[4],
        };
        let ref_is_parent = reference == self.op(Op::LayParentOf, child, 0, 0);
        self.set_pos_size(child, guilayout::child_layout(c, r, ref_is_parent, l));
    }

    fn execute_center(&self, parent: isize) {
        let items = self.items();
        if items.is_empty() {
            return;
        }
        let bounds: Vec<EeWonRect> = items.iter().map(|&c| self.bounds(c)).collect();
        let center_box = self.refs[0];
        let is_box = center_box != 0 && center_box != parent;
        let center = self.bounds(if is_box { center_box } else { parent });
        let Some((dx, dy)) = guilayout::center_shift(&bounds, center, is_box, self.horz, self.vert) else {
            return;
        };
        for c in items {
            let (x, y) = (self.left(c), self.top(c));
            self.set_pos(c, x.wrapping_add(dx), y.wrapping_add(dy));
        }
    }

    fn execute_same_size(&self) {
        let items = self.items();
        let sizes: Vec<(i32, i32)> = items.iter().map(|&c| (self.width(c), self.height(c))).collect();
        let (w, h) = guilayout::same_size(&sizes, self.horz, self.vert);
        for c in items {
            if self.horz && self.vert {
                self.set_size(c, w, h);
            } else if self.horz {
                let ch = self.height(c);
                self.set_size(c, w, ch);
            } else if self.vert {
                let cw = self.width(c);
                self.set_size(c, cw, h);
            }
        }
    }

    fn execute_clear_right(&self) {
        let rights: Vec<i32> = self.items().iter().map(|&c| self.left(c).wrapping_add(self.width(c))).collect();
        let comp = self.refs[0];
        let x = guilayout::clear_right(&rights, self.ints[0]);
        let y = self.top(comp);
        self.set_pos(comp, x, y);
    }

    fn execute_space_evenly(&self, parent: isize) {
        let items = self.items();
        if items.is_empty() {
            return;
        }
        let extents: Vec<i32> =
            items.iter().map(|&c| if self.horz { self.width(c) } else { self.height(c) }).collect();
        let b = self.refs[0];
        let (pos, extent) =
            if self.horz { (self.left(b), self.width(b)) } else { (self.top(b), self.height(b)) };
        let positions = guilayout::space_evenly(&extents, pos, extent, b == parent, self.ints[0], self.ints[1]);
        for (c, p) in items.into_iter().zip(positions) {
            if self.horz {
                let y = self.top(c);
                self.set_pos(c, p, y);
            } else {
                let x = self.left(c);
                self.set_pos(c, x, p);
            }
        }
    }

    fn execute_no_overlap(&self, parent: isize) {
        let (reference, target) = (self.refs[0], self.refs[1]);
        let t = self.bounds(target);
        let r = self.bounds(reference);
        let Some((dx, dy)) =
            guilayout::no_overlap(t, r, reference == parent, self.ints[0], self.ints[1], self.ints[2])
        else {
            return;
        };
        let (x, y) = (self.left(target), self.top(target));
        self.set_pos(target, x.wrapping_add(dx), y.wrapping_add(dy));
        for c in self.items() {
            let (x, y) = (self.left(c), self.top(c));
            self.set_pos(c, x.wrapping_add(dx), y.wrapping_add(dy));
        }
    }

    fn execute_size_at_least(&self) {
        let sizes: Vec<(i32, i32)> = self.items().iter().map(|&c| (self.width(c), self.height(c))).collect();
        let (w, h) = guilayout::size_at_least_layout(&sizes, self.ints[0], self.ints[1]);
        self.op(Op::LaySetSizeAtLeast, self.refs[0], w as isize, h as isize);
    }

    /// `PercentLayout::Execute`: the param's `DoCalc(target, parent)`. An anchor is read only for an axis the layout
    /// sets, as the original read them.
    fn execute_percent(&self, parent: isize) {
        let target = self.refs[0];
        let (left, top, right, bottom) = (self.refs[1], self.refs[2], self.refs[3], self.refs[4]);
        let n = self.ints;
        let p = EeWonPercent { px: n[0], py: n[1], pw: n[2], ph: n[3], lp: n[4], tp: n[5], rp: n[6], bp: n[7] };
        let mut edges = [0i32; 4];
        if p.px >= 0 || p.pw >= 0 {
            edges[0] = if left == parent { 0 } else { self.left(left) };
            edges[1] = if right == parent {
                self.width(parent)
            } else {
                self.left(right).wrapping_add(self.width(right))
            };
        }
        if p.py >= 0 || p.ph >= 0 {
            edges[2] = if top == parent { 0 } else { self.top(top) };
            edges[3] = if bottom == parent {
                self.height(parent)
            } else {
                self.top(bottom).wrapping_add(self.height(bottom))
            };
        }
        let r = guilayout::percent_layout(self.bounds(target), edges, p);
        self.set_pos_size(target, r);
    }

    pub fn call(&self, call: EeWonLayoutCall, a: isize, _b: isize) -> isize {
        type C = EeWonLayoutCall;
        match call {
            C::Execute => self.execute(a),
            C::UsesComponent => return self.uses_component(a) as isize,
            C::Add => self.add(a),
            C::GroupId => return self.group.get() as isize,
            C::Enabled => return self.enabled.get() as isize,
        }
        0
    }
}

/// The manager's inward calls.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonLayoutMgrCall {
    /// `ExecuteInstructions((Component*)a)` (the container).
    Execute = 1,
    /// `AddInstruction(a)` (`a` the `ChildLayoutInstructionBase*`, `b` its `EeWonLayout*`).
    AddInstruction,
    /// `AddLayoutPoint((LayoutPoint*)a)` (as a `Component*`).
    AddLayoutPoint,
    /// `RemoveComponent((Component*)a)`.
    RemoveComponent,
    Clear,
    /// `EnableInstructions(a, b != 0)`.
    EnableInstructions,
    /// `Container::AddChildLayout(a, c)` on the container `d` (`a` the instruction, `b` its `EeWonLayout*`, `c` the
    /// group id).
    AddChildLayout,
    /// The number of instructions (a test read).
    Count,
}

/// A container's child layout manager: the instructions (held, each with its Rust object) and the layout points
/// (held), in the order they were added.
pub struct EeWonLayoutMgr {
    host: EeWonGuiHostFn,
    comp: *mut c_void,
    instructions: RefCell<Vec<(isize, *const EeWonLayout)>>,
    points: RefCell<Vec<isize>>,
}

impl EeWonLayoutMgr {
    pub fn new(comp: *mut c_void, host: EeWonGuiHostFn) -> Self {
        EeWonLayoutMgr { host, comp, instructions: RefCell::new(Vec::new()), points: RefCell::new(Vec::new()) }
    }

    fn op(&self, op: Op, a: isize) -> isize {
        match self.host {
            // SAFETY: the host's contract
            Some(h) => unsafe { h(self.comp, op, a, 0, 0, 0) },
            None => 0,
        }
    }

    fn at(&self, i: usize) -> Option<(isize, *const EeWonLayout)> {
        self.instructions.borrow().get(i).copied()
    }

    /// `ExecuteInstructions`: every enabled instruction in order. The list is read again after each call (an
    /// instruction appended meanwhile is run, as the original's `std::list` walk ran it).
    pub fn execute(&self, container: isize) {
        let mut i = 0;
        while let Some((_, lay)) = self.at(i) {
            // SAFETY: the list holds the instruction, and with it its Rust object
            let l = unsafe { &*lay };
            if l.enabled() {
                l.execute(container);
            }
            i += 1;
        }
    }

    pub fn add_instruction(&self, instr: isize, lay: *const EeWonLayout) {
        self.op(Op::LayHold, instr);
        self.instructions.borrow_mut().push((instr, lay));
    }

    pub fn add_layout_point(&self, point: isize) {
        self.op(Op::ChildHold, point);
        self.points.borrow_mut().push(point);
    }

    /// `RemoveComponent`: every instruction using the component erased (released) in order.
    pub fn remove_component(&self, c: isize) {
        let mut i = 0;
        while let Some((instr, lay)) = self.at(i) {
            // SAFETY: held by the list
            if unsafe { (*lay).uses_component(c) } {
                self.instructions.borrow_mut().remove(i);
                self.op(Op::LayRelease, instr);
            } else {
                i += 1;
            }
        }
    }

    /// `Clear`: the instructions, then the layout points, released front to back.
    pub fn clear(&self) {
        self.release_instructions();
        self.release_points();
    }

    fn release_instructions(&self) {
        let all = std::mem::take(&mut *self.instructions.borrow_mut());
        for (instr, _) in all {
            self.op(Op::LayRelease, instr);
        }
    }

    fn release_points(&self) {
        let all = std::mem::take(&mut *self.points.borrow_mut());
        for p in all {
            self.op(Op::ChildRelease, p);
        }
    }

    /// `EnableInstructions`: the group id is a byte compared with the int.
    pub fn enable_instructions(&self, group: i32, enabled: bool) {
        let all = self.instructions.borrow().clone();
        for (_, lay) in all {
            // SAFETY: held by the list
            let l = unsafe { &*lay };
            if i32::from(l.group()) == group {
                l.set_enabled(enabled);
            }
        }
    }

    /// `Container::AddChildLayout(instr, id)`: the group stored as a byte; group 0 enabled and run at once (before
    /// the manager holds it), any other disabled.
    pub fn add_child_layout(&self, instr: isize, lay: *const EeWonLayout, id: i32, container: isize) {
        // SAFETY: the caller's live instruction
        let l = unsafe { &*lay };
        l.set_group(id as u8);
        if id == 0 {
            l.set_enabled(true);
            l.execute(container);
        } else {
            l.set_enabled(false);
        }
        self.add_instruction(instr, lay);
    }

    /// The destructor: the members in reverse order, the layout points first, then the instructions.
    pub fn destroy(&self) {
        self.release_points();
        self.release_instructions();
    }

    pub fn call(&self, call: EeWonLayoutMgrCall, a: isize, b: isize, c: isize, d: isize) -> isize {
        type C = EeWonLayoutMgrCall;
        match call {
            C::Execute => self.execute(a),
            C::AddInstruction => self.add_instruction(a, b as *const EeWonLayout),
            C::AddLayoutPoint => self.add_layout_point(a),
            C::RemoveComponent => self.remove_component(a),
            C::Clear => self.clear(),
            C::EnableInstructions => self.enable_instructions(a as i32, b != 0),
            C::AddChildLayout => self.add_child_layout(a, b as *const EeWonLayout, c as i32, d),
            C::Count => return self.instructions.borrow().len() as isize,
        }
        0
    }
}

/// A new Rust instruction for the C++ shell `comp` (its calls out through `host`), from its constructor's numbers.
///
/// # Safety
/// `init` points at a live `EeWonLayoutInit`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_layout_new(
    comp: *mut c_void,
    host: EeWonGuiHostFn,
    init: *const EeWonLayoutInit,
) -> *mut EeWonLayout {
    // SAFETY: the contract
    let init = unsafe { &*init };
    Box::into_raw(Box::new(EeWonLayout::new(comp, host, init)))
}

/// Frees a Rust instruction (its list held no references).
///
/// # Safety
/// `lay` came from `ee_won_layout_new` (or is NULL) and is not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_won_layout_free(lay: *mut EeWonLayout) {
    if !lay.is_null() {
        // SAFETY: the contract
        drop(unsafe { Box::from_raw(lay) });
    }
}

/// One of an instruction's methods (`EeWonLayoutCall`).
///
/// # Safety
/// `lay` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_layout_call(lay: *const EeWonLayout, call: EeWonLayoutCall, a: isize, b: isize) -> isize {
    // SAFETY: live (the contract); a shared reference: the calls out may re-enter
    unsafe { (*lay).call(call, a, b) }
}

/// `CenterLayout::Center(parent, child, horz, vert)`: a temporary centre layout (centre box the parent, the child
/// listed) run on the parent.
#[no_mangle]
pub extern "C" fn ee_won_layout_center(host: EeWonGuiHostFn, parent: isize, child: isize, horz: bool, vert: bool) {
    let init = EeWonLayoutInit {
        kind: EeWonLayoutKind::Center,
        refs: [parent, 0, 0, 0, 0],
        ints: [0; 8],
        horz,
        vert,
    };
    let l = EeWonLayout::new(std::ptr::null_mut(), host, &init);
    l.add(child);
    l.execute(parent);
}

/// A new Rust layout manager for the C++ shell `comp`, its calls out through `host`.
#[no_mangle]
pub extern "C" fn ee_won_lmgr_new(comp: *mut c_void, host: EeWonGuiHostFn) -> *mut EeWonLayoutMgr {
    Box::into_raw(Box::new(EeWonLayoutMgr::new(comp, host)))
}

/// Frees a Rust layout manager: the layout points, then the instructions, released (the original's members).
///
/// # Safety
/// `mgr` came from `ee_won_lmgr_new` (or is NULL) and is not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lmgr_free(mgr: *mut EeWonLayoutMgr) {
    if !mgr.is_null() {
        // SAFETY: the contract
        let m = unsafe { Box::from_raw(mgr) };
        m.destroy();
    }
}

/// One of the manager's methods (`EeWonLayoutMgrCall`).
///
/// # Safety
/// `mgr` is live; an instruction passed with its live Rust object.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lmgr_call(
    mgr: *const EeWonLayoutMgr,
    call: EeWonLayoutMgrCall,
    a: isize,
    b: isize,
    c: isize,
    d: isize,
) -> isize {
    // SAFETY: live (the contract); a shared reference: the calls out may re-enter
    unsafe { (*mgr).call(call, a, b, c, d) }
}

#[cfg(test)]
#[path = "layoutobj_tests.rs"]
mod tests;
