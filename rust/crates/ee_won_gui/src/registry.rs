//! The widget registry (R6 WONLobby round 19): each C++ widget's Rust objects by the widget's address.
//!
//! Every Rust widget object is made for one C++ component (`ee_won_*_new(component, host)`), and the C++ class's
//! methods are one-line forwards to it (`ee_won_input_call(this->mRust, ...)`). The registry records the pair when
//! the object is made and forgets it when the object is freed, so a Rust caller that holds only the C++ pointer (the
//! lobby's dialogs and controls hold their controls by it) can make the forward itself instead of asking a C++ host
//! to make it. One widget has several objects (a `ListArea` is a container, a scroll pane and a list): the key is the
//! address and the object's kind.
//!
//! The maps are only looked up, never iterated, so their order is not behaviour; the lock is held for the look-up
//! alone, never across a call into an object.

use std::collections::BTreeMap;
use std::ffi::c_void;
use std::sync::Mutex;

/// A Rust widget object's kind (the module that made it).
#[repr(u8)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord)]
pub enum EeWonGuiKind {
    /// inputobj.rs `EeWonInput`
    Input = 1,
    /// labelobj.rs `EeWonLabel`
    Label,
    /// btnobj.rs `EeWonButton`
    Button,
    /// msbtnobj.rs `EeWonMsButton`
    MsButton,
    /// comboobj.rs `EeWonComboBox`
    Combo,
    /// listobj.rs `EeWonList`
    List,
    /// multiobj.rs `EeWonMulti`
    Multi,
    /// treeobj.rs `EeWonTree`
    Tree,
    /// ctobj.rs `EeWonContainer`
    Container,
    /// dialogobj.rs `EeWonDialog`
    Dialog,
    /// textobj.rs `EeWonText`
    Text,
    /// menuobj.rs `EeWonMenu`
    Menu,
    /// wonbtnobj.rs `EeWonWonButton`
    WonButton,
    /// paneobj.rs `EeWonScrollPane`
    Pane,
    /// treeobj.rs `EeWonTNode` (a tree row, by its `TreeItem`)
    TreeNode,
    /// scrollerobj.rs `EeWonScroller`
    Scroller,
    /// barobj.rs `EeWonScrollbar`
    Bar,
    /// multiobj.rs `EeWonMHead`
    MHead,
    /// tabobj.rs `EeWonTabCtrl`
    Tab,
    /// shell.rs `EeWonShell` (a generic widget shell's kind)
    Shell,
    /// splitobj.rs `EeWonSplitter`
    Splitter,
}

struct Maps {
    by_widget: BTreeMap<(usize, EeWonGuiKind), usize>,
    by_object: BTreeMap<usize, (usize, EeWonGuiKind)>,
}

static MAPS: Mutex<Maps> = Mutex::new(Maps { by_widget: BTreeMap::new(), by_object: BTreeMap::new() });

fn maps() -> std::sync::MutexGuard<'static, Maps> {
    MAPS.lock().unwrap_or_else(|e| e.into_inner())
}

/// Records `object` as `widget`'s object of `kind` (a later one of the same widget and kind replaces it).
pub fn register(widget: *const c_void, kind: EeWonGuiKind, object: *const c_void) {
    if widget.is_null() || object.is_null() {
        return;
    }
    let mut m = maps();
    let key = (widget as usize, kind);
    if let Some(old) = m.by_widget.insert(key, object as usize) {
        m.by_object.remove(&old);
    }
    m.by_object.insert(object as usize, key);
}

/// Forgets `object` (its widget's entry too, if it is still this object's).
pub fn unregister(object: *const c_void) {
    let mut m = maps();
    if let Some(key) = m.by_object.remove(&(object as usize)) {
        if m.by_widget.get(&key) == Some(&(object as usize)) {
            m.by_widget.remove(&key);
        }
    }
}

/// `widget`'s object of `kind`, or NULL.
pub fn find(widget: isize, kind: EeWonGuiKind) -> *const c_void {
    maps().by_widget.get(&(widget as usize, kind)).map_or(std::ptr::null(), |&o| o as *const c_void)
}

/// `widget`'s object of `kind` as its type.
///
/// # Safety
/// `T` is the type `kind` names; the reference is used only while the object lives (the widget is held).
pub unsafe fn get<'a, T>(widget: isize, kind: EeWonGuiKind) -> Option<&'a T> {
    let p = find(widget, kind) as *const T;
    // SAFETY: the contract
    unsafe { p.as_ref() }
}

/// The C++ base class whose inline virtual a C++ host's `Base*` op called (`t->Container::MouseDown(...)`).
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonGuiBase {
    /// Component's virtuals: Component's non-virtual call through the component host (its `KCall`).
    Component,
    /// Container's: its Rust container (`CCall`); the ones it inherits are Component's.
    Container,
    /// ScrollArea's: its size change and position adjust are its scroll pane's (`ACall`), the rest Container's.
    ScrollArea,
}

/// In the game, a Rust object's `Base*` op on its widget `comp` answered here: the call the C++ host's case made
/// through the base class's inline virtual, by construction the same Rust call. `None`: not a base op, not the
/// game (the drivers' hosts answer their ops), or no object.
pub fn base_op(
    comp: isize,
    base: EeWonGuiBase,
    op: crate::guihost::EeWonGuiOp,
    a: isize,
    b: isize,
    c: isize,
) -> Option<isize> {
    use crate::compobj::{host, Comp, EeWonCompCall as K};
    use crate::ctobj::{EeWonContainer, EeWonCtCall as C};
    use crate::guihost::EeWonGuiOp as O;
    use crate::paneobj::{EeWonPaneCall as P, EeWonScrollPane};
    if !matches!(
        op,
        O::BaseSizeChanged
            | O::BaseAdjustPosition
            | O::BaseNotifyRootVisibility
            | O::BaseMouseDown
            | O::BaseMouseUp
            | O::BaseMouseExit
            | O::BaseMouseMove
            | O::BaseMouseEnter
            | O::BaseMouseDrag
            | O::BaseKeyDown
            | O::BaseKeyChar
            | O::BaseGotFocus
            | O::BaseLostFocus
            | O::BasePaint
            | O::BaseHandleEvent
            | O::BaseAddedToParent
            | O::BaseSetEnabled
            | O::BaseMouseWheel
    ) || !crate::compobj::is_game()
    {
        return None;
    }
    let (ai, bi, ci) = (a as i32 as isize, b as i32 as isize, c as i32 as isize);
    let k = |k: K, x: isize| Comp::new(host()).call(comp, k, x, 0, 0);
    // a void call's answer
    let z = |_: isize| 0isize;
    if base == EeWonGuiBase::ScrollArea {
        let pane = |call: P, x: isize| -> Option<isize> {
            // SAFETY: the kind's type; the widget is live for its own op
            let p: &EeWonScrollPane = unsafe { get(comp, EeWonGuiKind::Pane) }?;
            Some(p.call(call, x, 0, 0))
        };
        match op {
            O::BaseSizeChanged => return pane(P::SizeChanged, 0).map(|_| 0),
            O::BaseAdjustPosition => return pane(P::AdjustPosition, (a != 0) as isize).map(|_| 0),
            _ => {}
        }
    }
    if base == EeWonGuiBase::Component {
        return Some(match op {
            O::BaseSizeChanged => z(k(K::SizeChanged, 0)),
            O::BaseGotFocus => z(k(K::GotFocus, 0)),
            O::BaseLostFocus => z(k(K::LostFocus, 0)),
            O::BaseMouseExit => z(k(K::MouseExit, 0)),
            O::BasePaint => z(k(K::Paint, a)),
            O::BaseSetEnabled => z(k(K::SetEnabled, (a != 0) as isize)),
            O::BaseHandleEvent => z(k(K::HandleEvent, a)),
            _ => return None,
        });
    }
    // Container's (ScrollArea inherits these); HandleComponentEvent is Component's for both
    if op == O::BaseHandleEvent {
        return Some(z(k(K::HandleEvent, a)));
    }
    // SAFETY: as above
    let ct: &EeWonContainer = unsafe { get(comp, EeWonGuiKind::Container) }?;
    let r = match op {
        O::BaseSizeChanged => z(ct.call(C::SizeChanged, 0, 0, 0)),
        O::BaseMouseDown => z(ct.call(C::MouseDown, ai, bi, ci)),
        O::BaseMouseUp => z(ct.call(C::MouseUp, ai, bi, ci)),
        O::BaseMouseDrag => z(ct.call(C::MouseDrag, ai, bi, 0)),
        O::BaseMouseMove => z(ct.call(C::MouseMove, ai, bi, 0)),
        O::BaseMouseEnter => z(ct.call(C::MouseEnter, ai, bi, 0)),
        O::BaseMouseExit => z(ct.call(C::MouseExit, 0, 0, 0)),
        O::BaseAddedToParent => z(ct.call(C::AddedToParent, 0, 0, 0)),
        O::BaseNotifyRootVisibility => z(ct.call(C::NotifyRootVisibility, (a != 0) as isize, 0, 0)),
        O::BaseMouseWheel => (ct.call(C::MouseWheel, ai, 0, 0) != 0) as isize,
        O::BaseKeyDown => (ct.call(C::KeyDown, ai, 0, 0) != 0) as isize,
        O::BaseKeyChar => (ct.call(C::KeyChar, ai, 0, 0) != 0) as isize,
        _ => return None,
    };
    Some(r)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn register_find_unregister() {
        let (w, o1, o2) = (0x7000_0010usize, 0x7100_0020usize, 0x7100_0040usize);
        register(w as *const c_void, EeWonGuiKind::Input, o1 as *const c_void);
        register(w as *const c_void, EeWonGuiKind::Label, o2 as *const c_void);
        assert_eq!(find(w as isize, EeWonGuiKind::Input) as usize, o1);
        assert_eq!(find(w as isize, EeWonGuiKind::Label) as usize, o2);
        assert!(find(w as isize, EeWonGuiKind::List).is_null());
        unregister(o1 as *const c_void);
        assert!(find(w as isize, EeWonGuiKind::Input).is_null());
        assert_eq!(find(w as isize, EeWonGuiKind::Label) as usize, o2);
        unregister(o2 as *const c_void);
        assert!(find(w as isize, EeWonGuiKind::Label).is_null());
    }

    #[test]
    fn a_replaced_object_does_not_drop_the_new_one() {
        let (w, o1, o2) = (0x7200_0010usize, 0x7300_0020usize, 0x7300_0040usize);
        register(w as *const c_void, EeWonGuiKind::Combo, o1 as *const c_void);
        register(w as *const c_void, EeWonGuiKind::Combo, o2 as *const c_void);
        unregister(o1 as *const c_void);
        assert_eq!(find(w as isize, EeWonGuiKind::Combo) as usize, o2);
        unregister(o2 as *const c_void);
        assert!(find(w as isize, EeWonGuiKind::Combo).is_null());
        // NULLs are never recorded
        register(std::ptr::null(), EeWonGuiKind::Combo, o1 as *const c_void);
        assert!(find(0, EeWonGuiKind::Combo).is_null());
    }

    /// The C++ hosts' `Base*` cases as they were (GuiHost.cpp, ComboBox.cpp, MSButtonBase.cpp, WONButton.cpp at
    /// c9d7de90c): each base's op and the call its inline virtual made, against `base_op` in game mode.
    #[test]
    fn base_ops_are_the_base_classes_calls() {
        use crate::compobj::{ee_won_comp_host_world, EeWonCompCall as K, FORCE_GAME};
        use crate::ctobj::{ee_won_ct_free, ee_won_ct_new};
        use crate::guihost::EeWonGuiOp as O;
        use crate::paneobj::{ee_won_pane_free, ee_won_pane_new};
        use std::cell::RefCell;
        thread_local! {
            static LOG: RefCell<Vec<(i32, isize, isize, isize)>> = const { RefCell::new(Vec::new()) };
        }
        unsafe extern "C" fn rec(
            _: *mut c_void,
            op: crate::guihost::EeWonGuiOp,
            a: isize,
            b: isize,
            c: isize,
            _: isize,
        ) -> isize {
            LOG.with(|l| l.borrow_mut().push((op as i32, a, b, c)));
            0
        }
        let take = || LOG.with(|l| std::mem::take(&mut *l.borrow_mut()));
        // the component host: Component's KCall arrives as its op with the call kind first
        unsafe extern "C" fn comp_host(
            _: *mut c_void,
            op: crate::guihost::EeWonGuiOp,
            a: isize,
            b: isize,
            _c: isize,
            _: isize,
        ) -> isize {
            // the component's own address is the same argument for both twins
            let (p1, p2) = PAIR.with(|p| p.get());
            let b = if b == p1 || b == p2 { 1 } else { b };
            LOG.with(|l| l.borrow_mut().push((-(op as i32) - 1, a, b, 0)));
            0
        }
        thread_local! {
            static PAIR: std::cell::Cell<(isize, isize)> = const { std::cell::Cell::new((0, 0)) };
        }
        // a world: blocks the component calls read (compmem), the hosts recording
        let blocks = vec![0u64; 512];
        let w = blocks.as_ptr() as isize;
        // the twin the direct calls are made on (its members compared with the answered one's)
        let twin = vec![0u64; 64];
        let w2 = twin.as_ptr() as isize;
        PAIR.with(|p| p.set((w, w2)));
        let members = |c: isize| unsafe { std::slice::from_raw_parts(c as *const u64, 32).to_vec() };
        // without the game, nothing is answered
        assert!(base_op(w, EeWonGuiBase::Container, O::BaseMouseDown, 1, 2, 3).is_none());
        FORCE_GAME.with(|f| f.set(true));
        ee_won_comp_host_world(Some(comp_host));
        // Component::X(): one component call each (the trace of a direct KCall is the expectation)
        let direct = |k: K, x: isize| {
            crate::compobj::Comp::new(crate::compobj::host()).call(w2, k, x, 0, 0);
        };
        let comp_cases: [(O, K, isize); 7] = [
            (O::BaseSizeChanged, K::SizeChanged, 0),
            (O::BaseGotFocus, K::GotFocus, 0),
            (O::BaseLostFocus, K::LostFocus, 0),
            (O::BaseMouseExit, K::MouseExit, 0),
            (O::BasePaint, K::Paint, 0x40),
            (O::BaseSetEnabled, K::SetEnabled, 1),
            (O::BaseHandleEvent, K::HandleEvent, 0x50),
        ];
        for (op, k, x) in comp_cases {
            assert_eq!(base_op(w, EeWonGuiBase::Component, op, x, 0, 0), Some(0), "{op:?}");
            let got = take();
            direct(k, x);
            assert_eq!(got, take(), "{op:?}");
            assert_eq!(members(w), members(w2), "{op:?}");
        }
        // a component op Component's base never had
        assert!(base_op(w, EeWonGuiBase::Component, O::BaseMouseDown, 1, 2, 3).is_none());
        // Container::X() and ScrollArea's: the container's and the pane's calls
        let ct = ee_won_ct_new(w as *mut c_void, Some(rec), false);
        let pane = ee_won_pane_new(w as *mut c_void, Some(rec));
        let ct2 = ee_won_ct_new((w + 2048) as *mut c_void, Some(rec), false);
        let pane2 = ee_won_pane_new((w + 2048) as *mut c_void, Some(rec));
        use crate::ctobj::EeWonCtCall as C;
        use crate::paneobj::EeWonPaneCall as P;
        // (AddedToParent is Component's own in Rust since round 38, which reads the parent the stand-in world has not:
        // the LAN game and the tour run it)
        let ct_cases: [(O, C, [isize; 3]); 11] = [
            (O::BaseSizeChanged, C::SizeChanged, [0, 0, 0]),
            (O::BaseMouseDown, C::MouseDown, [5, 6, 1]),
            (O::BaseMouseUp, C::MouseUp, [5, 6, 0]),
            (O::BaseMouseDrag, C::MouseDrag, [7, 8, 0]),
            (O::BaseMouseMove, C::MouseMove, [7, 9, 0]),
            (O::BaseMouseEnter, C::MouseEnter, [3, 4, 0]),
            (O::BaseMouseExit, C::MouseExit, [0, 0, 0]),
            (O::BaseNotifyRootVisibility, C::NotifyRootVisibility, [1, 0, 0]),
            (O::BaseMouseWheel, C::MouseWheel, [-3, 0, 0]),
            (O::BaseKeyDown, C::KeyDown, [13, 0, 0]),
            (O::BaseKeyChar, C::KeyChar, [65, 0, 0]),
        ];
        for base in [EeWonGuiBase::Container, EeWonGuiBase::ScrollArea] {
            for (op, call, [x, y, z]) in ct_cases {
                if base == EeWonGuiBase::ScrollArea && op == O::BaseSizeChanged {
                    continue;
                }
                let r = base_op(w, base, op, x, y, z).expect("answered");
                let got = take();
                let r2 = unsafe { (*ct2).call(call, x, y, z) };
                let want: Vec<_> = take();
                assert_eq!(got, want, "{op:?}");
                let bool_op = matches!(op, O::BaseMouseWheel | O::BaseKeyDown | O::BaseKeyChar);
                assert_eq!(r, if bool_op { (r2 != 0) as isize } else { 0 }, "{op:?}");
            }
            // HandleComponentEvent is Component's for both
            assert_eq!(base_op(w, base, O::BaseHandleEvent, 0x60, 0, 0), Some(0));
            let got = take();
            direct(K::HandleEvent, 0x60);
            assert_eq!(got, take());
        }
        // ScrollArea's own: the pane's size change and position adjust
        // (AdjustPosition needs a scroller the stand-in world has not: the LAN game and the tour run it)
        assert_eq!(base_op(w, EeWonGuiBase::ScrollArea, O::BaseSizeChanged, 0, 0, 0), Some(0));
        let got = take();
        unsafe { (*pane2).call(P::SizeChanged, 0, 0, 0) };
        assert_eq!(got, take());
        unsafe {
            ee_won_pane_free(pane);
            ee_won_pane_free(pane2);
            ee_won_ct_free(ct);
            ee_won_ct_free(ct2);
        }
        ee_won_comp_host_world(None);
        FORCE_GAME.with(|f| f.set(false));
    }
}
