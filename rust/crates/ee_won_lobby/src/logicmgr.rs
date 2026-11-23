//! LobbyLogicMgr whole in Rust (R6 WON lobby and network round 35; LobbyLogicMgr.cpp, 0x766b30..): the lobby's list
//! of logic objects (the account, dialog, staging, init, LAN, routing and ping logics, in the order WONLobbyPrv's Init
//! adds them), which it initialises, kills and passes the control, component and timer events to. The C++ class is
//! the member WONLobbyPrv holds (`mLogicMgr`) with this object; its methods forward here.
//!
//! The list holds a reference to each logic (the original's `std::list<SmartPtr<LobbyLogic> >`), released front to
//! back when the manager goes. Every call on a logic or an event is one host op (`LobbyLogicMgr.cpp`'s host). A
//! logic's virtual may throw (InitComponent's config errors): each runs under WonGuard and answers
//! `EE_WON_LOBBY_THROWN`; the dispatch then stops, as the original's loop was left by the exception, and the C++
//! entry rethrows it.
//!
//! A dispatch re-reads the list's length at each step and never borrows it across a call (a logic added during a
//! dispatch is visited, as the original's list iterator reached an appended node).
//!
//! Reach: every LAN game (the logics' init, the button and menu events, the timer, the kill at the lobby's end).

use std::cell::RefCell;
use std::ffi::c_void;

use crate::lobbyprv::EE_WON_LOBBY_THROWN;

/// The host's ops: each one C++ statement.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonLogicMgrOp {
    /// `((LobbyLogic*)a)->AddRef()`.
    AddRef = 1,
    /// `((LobbyLogic*)a)->Release()`.
    Release,
    /// `return WON_GUARDED_V(((LobbyLogic*)a)->InitComponent((ComponentConfig*)b))`.
    InitComponent,
    /// `return WON_GUARDED_V(((LobbyLogic*)a)->Kill())`.
    Kill,
    /// `return WON_GUARDED_V(((LobbyLogic*)a)->HandleControlEvent((int)b))`.
    HandleControlEvent,
    /// `return WON_GUARDED_V(((LobbyLogic*)a)->HandleComponentEvent((ComponentEvent*)b))`.
    HandleComponentEvent,
    /// `return WON_GUARDED_V(((LobbyLogic*)a)->Reset())` (the timer tick: vtable slot 0xc).
    Reset,
    /// `((ListItemClickedEvent*)a)->mItem.get()`.
    ClickItem,
    /// `((ListItemClickedEvent*)a)->mButton`.
    ClickButton,
    /// `dynamic_cast<MenuItem*>((ListItem*)a) != NULL` (0 or 1).
    IsMenuItem,
    /// `dynamic_cast<MenuItem*>((ListItem*)a)->mMenuId`.
    MenuId,
    /// `((ComponentEvent*)a)->GetControlId()`.
    ControlId,
}

/// The host: `(op, a, b) -> result`.
pub type EeWonLogicMgrHostFn = Option<unsafe extern "C" fn(op: EeWonLogicMgrOp, a: isize, b: isize) -> isize>;

/// The inward calls (the C++ class's methods).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonLogicMgrCall {
    /// `AddLogic(a)`.
    AddLogic = 1,
    /// `Init(a)`.
    Init,
    /// `Kill()`.
    Kill,
    /// `DispatchControlEvent(a)`.
    DispatchControlEvent,
    /// `DispatchComponentEvent(a)`.
    DispatchComponentEvent,
    /// `DispatchTimerEvent()`.
    DispatchTimerEvent,
    /// `HandleComponentEvent(a)`, `b` its `mType`.
    HandleComponentEvent,
}

// WONGUI's event types (ee/wonapi/WONEventTypes.h's places, as evmap.rs reads them)
const BUTTON_PRESSED: isize = 15;
const LIST_ITEM_UP_CLICKED: isize = 24;

/// The manager.
pub struct EeWonLogicMgr {
    host: EeWonLogicMgrHostFn,
    list: RefCell<Vec<isize>>,
}

impl EeWonLogicMgr {
    fn op(&self, op: EeWonLogicMgrOp, a: isize, b: isize) -> isize {
        match self.host {
            // SAFETY: the host's contract (each op on a live logic or event)
            Some(f) => unsafe { f(op, a, b) },
            None => 0,
        }
    }

    /// The op on every logic, front to back; stops at a thrown exception (answered).
    fn each(&self, op: EeWonLogicMgrOp, b: isize) -> isize {
        let mut i = 0;
        loop {
            let logic = match self.list.borrow().get(i) {
                Some(&l) => l,
                None => return 0,
            };
            if self.op(op, logic, b) == EE_WON_LOBBY_THROWN {
                return EE_WON_LOBBY_THROWN;
            }
            i += 1;
        }
    }

    fn call(&self, call: EeWonLogicMgrCall, a: isize, b: isize) -> isize {
        use EeWonLogicMgrCall as C;
        use EeWonLogicMgrOp as O;
        match call {
            C::AddLogic => {
                // `push_back(SmartPtr<LobbyLogic>(a))`
                if a != 0 {
                    self.op(O::AddRef, a, 0);
                }
                self.list.borrow_mut().push(a);
                0
            }
            C::Init => self.each(O::InitComponent, a),
            C::Kill => self.each(O::Kill, 0),
            C::DispatchControlEvent => self.each(O::HandleControlEvent, a),
            C::DispatchComponentEvent => self.each(O::HandleComponentEvent, a),
            C::DispatchTimerEvent => self.each(O::Reset, 0),
            C::HandleComponentEvent => match b {
                // a left click on a menu item: its menu id as a control event
                LIST_ITEM_UP_CLICKED => {
                    let item = self.op(O::ClickItem, a, 0);
                    if item == 0 || self.op(O::ClickButton, a, 0) != 0 {
                        return 0;
                    }
                    if self.op(O::IsMenuItem, item, 0) != 0 {
                        let id = self.op(O::MenuId, item, 0);
                        return self.each(O::HandleControlEvent, id);
                    }
                    0
                }
                BUTTON_PRESSED => {
                    let id = self.op(O::ControlId, a, 0);
                    self.each(O::HandleControlEvent, id)
                }
                _ => self.each(O::HandleComponentEvent, a),
            },
        }
    }
}

impl Drop for EeWonLogicMgr {
    fn drop(&mut self) {
        // the list's nodes, front to back, each releasing its logic
        let list = std::mem::take(&mut *self.list.borrow_mut());
        for logic in list {
            if logic != 0 {
                self.op(EeWonLogicMgrOp::Release, logic, 0);
            }
        }
    }
}

/// A new, empty manager (the C++ constructor's).
#[no_mangle]
pub extern "C" fn ee_won_logicmgr_new(host: EeWonLogicMgrHostFn) -> *mut EeWonLogicMgr {
    Box::into_raw(Box::new(EeWonLogicMgr { host, list: RefCell::new(Vec::new()) }))
}

/// Frees a manager, releasing its logics.
///
/// # Safety
/// `m` is from `ee_won_logicmgr_new` (or NULL), not used after.
#[no_mangle]
pub unsafe extern "C" fn ee_won_logicmgr_free(m: *mut EeWonLogicMgr) {
    if !m.is_null() {
        // SAFETY: the contract
        drop(unsafe { Box::from_raw(m) });
    }
}

/// A call on a manager: 0, or `EE_WON_LOBBY_THROWN` when a logic's call threw (the C++ entry rethrows it).
///
/// # Safety
/// `m` is a live manager; the arguments as the call documents.
#[no_mangle]
pub unsafe extern "C" fn ee_won_logicmgr_call(
    m: *mut EeWonLogicMgr,
    call: EeWonLogicMgrCall,
    a: isize,
    b: isize,
) -> isize {
    if m.is_null() {
        return 0;
    }
    // SAFETY: the contract (shared: a logic's call may re-enter the manager)
    unsafe { (*m).call(call, a, b) }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::cell::RefCell;

    thread_local! {
        static LOG: RefCell<Vec<String>> = const { RefCell::new(Vec::new()) };
        static THROW_AT: RefCell<isize> = const { RefCell::new(0) };
        static MGR: RefCell<isize> = const { RefCell::new(0) };
    }

    unsafe extern "C" fn host(op: EeWonLogicMgrOp, a: isize, b: isize) -> isize {
        LOG.with(|l| l.borrow_mut().push(format!("{op:?} {a} {b}")));
        match op {
            EeWonLogicMgrOp::ClickItem => a + 1000,
            EeWonLogicMgrOp::ClickButton => (a == 2) as isize,
            EeWonLogicMgrOp::IsMenuItem => (a != 1003) as isize,
            EeWonLogicMgrOp::MenuId => 77,
            EeWonLogicMgrOp::ControlId => 55,
            EeWonLogicMgrOp::HandleControlEvent if a == 30 && b == 99 => {
                // a logic added while the dispatch runs is visited too
                let m = MGR.with(|m| *m.borrow()) as *mut EeWonLogicMgr;
                unsafe { ee_won_logicmgr_call(m, EeWonLogicMgrCall::AddLogic, 40, 0) };
                0
            }
            _ if THROW_AT.with(|t| *t.borrow()) == a && op != EeWonLogicMgrOp::AddRef => EE_WON_LOBBY_THROWN,
            _ => 0,
        }
    }

    fn log() -> Vec<String> {
        LOG.with(|l| std::mem::take(&mut *l.borrow_mut()))
    }

    fn make(logics: &[isize]) -> *mut EeWonLogicMgr {
        let m = ee_won_logicmgr_new(Some(host));
        MGR.with(|g| *g.borrow_mut() = m as isize);
        for &l in logics {
            unsafe { ee_won_logicmgr_call(m, EeWonLogicMgrCall::AddLogic, l, 0) };
        }
        log();
        m
    }

    #[test]
    fn dispatches_in_order_and_releases_front_to_back() {
        THROW_AT.with(|t| *t.borrow_mut() = 0);
        let m = make(&[10, 20, 30]);
        unsafe { ee_won_logicmgr_call(m, EeWonLogicMgrCall::Init, 5, 0) };
        assert_eq!(log(), vec!["InitComponent 10 5", "InitComponent 20 5", "InitComponent 30 5"]);
        unsafe { ee_won_logicmgr_call(m, EeWonLogicMgrCall::DispatchTimerEvent, 0, 0) };
        assert_eq!(log(), vec!["Reset 10 0", "Reset 20 0", "Reset 30 0"]);
        unsafe { ee_won_logicmgr_call(m, EeWonLogicMgrCall::DispatchControlEvent, 99, 0) };
        assert_eq!(log().last().unwrap(), "HandleControlEvent 40 99");
        unsafe { ee_won_logicmgr_free(m) };
        assert_eq!(log(), vec!["Release 10 0", "Release 20 0", "Release 30 0", "Release 40 0"]);
    }

    #[test]
    fn a_thrown_call_stops_the_dispatch() {
        THROW_AT.with(|t| *t.borrow_mut() = 20);
        let m = make(&[10, 20, 30]);
        let r = unsafe { ee_won_logicmgr_call(m, EeWonLogicMgrCall::Init, 5, 0) };
        assert_eq!(r, EE_WON_LOBBY_THROWN);
        assert_eq!(log(), vec!["InitComponent 10 5", "InitComponent 20 5"]);
        THROW_AT.with(|t| *t.borrow_mut() = 0);
        unsafe { ee_won_logicmgr_free(m) };
    }

    #[test]
    fn clicks_and_buttons_become_control_events() {
        THROW_AT.with(|t| *t.borrow_mut() = 0);
        let m = make(&[10]);
        // a menu item clicked with the left button
        unsafe { ee_won_logicmgr_call(m, EeWonLogicMgrCall::HandleComponentEvent, 1, LIST_ITEM_UP_CLICKED) };
        assert_eq!(
            log(),
            vec!["ClickItem 1 0", "ClickButton 1 0", "IsMenuItem 1001 0", "MenuId 1001 0", "HandleControlEvent 10 77"]
        );
        // another button: nothing
        unsafe { ee_won_logicmgr_call(m, EeWonLogicMgrCall::HandleComponentEvent, 2, LIST_ITEM_UP_CLICKED) };
        assert_eq!(log(), vec!["ClickItem 2 0", "ClickButton 2 0"]);
        // not a menu item: nothing
        unsafe { ee_won_logicmgr_call(m, EeWonLogicMgrCall::HandleComponentEvent, 3, LIST_ITEM_UP_CLICKED) };
        assert_eq!(log(), vec!["ClickItem 3 0", "ClickButton 3 0", "IsMenuItem 1003 0"]);
        unsafe { ee_won_logicmgr_call(m, EeWonLogicMgrCall::HandleComponentEvent, 4, BUTTON_PRESSED) };
        assert_eq!(log(), vec!["ControlId 4 0", "HandleControlEvent 10 55"]);
        unsafe { ee_won_logicmgr_call(m, EeWonLogicMgrCall::HandleComponentEvent, 4, 3) };
        assert_eq!(log(), vec!["HandleComponentEvent 10 4"]);
        unsafe { ee_won_logicmgr_free(m) };
    }
}
