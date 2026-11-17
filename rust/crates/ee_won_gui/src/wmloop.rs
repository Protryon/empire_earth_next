//! WONGUI's WindowManager, the rest (R6 WON round 34): the window set, the event list and PostEvent, the timer
//! events, the broadcast set, ProcessEvents, DeliverEvent, CloseAllWindows, Add/RemoveWindow and the screen
//! resolution (the original's WindowManager.cpp), on the Rust manager of `wmobj.rs`.
//!
//! The event list is the one state other threads reach: PostEvent (a network thread's `QueueEvent`) and
//! PeekNextEvent lock it, as the original locked `mDataCrit`. It is a `Mutex` that is never held across a call
//! out: an event is held before the lock and given back after it, and RemoveWindow reads each event's window with
//! the lock released. Everything else is the GUI thread's, in a `RefCell` like the rest of the manager.
//!
//! The original's containers held `SmartPtr`s; their references are `Hold` / `Release` ops: a set's insert holds
//! a new entry, its erase gives it back (the original's temporary `SmartPtr` made from the raw argument, whose
//! AddRef / Release pair could not free anything, is not mirrored). The sets are walked as the original's
//! iterators walked a live `std::set`: the next entry after the one visited, so entries added or removed by the
//! calls out are seen or skipped as the original's tree would; the original's erasing the entry being visited
//! (undefined there) carries on from its key here.

use std::collections::BTreeSet;
use std::ops::Bound::{Excluded, Unbounded};
use std::sync::atomic::Ordering;
use std::sync::MutexGuard;

use crate::guictl::EeWonRect4;
use crate::guievent;
use crate::guihost::EeWonGuiOp as Op;
use crate::wmobj::{EeWonWm, EeWonWmComp, EeWonWmEvent, EeWonWmField as F, EeWonWmMouse, EeWonWmWin};

/// `WindowEventType` (`WONEventTypes.h`).
const EVENT_SIZE_CHANGE: isize = 1;
const EVENT_MOUSE_MOVE: isize = 2;
const EVENT_MOUSE_DOWN: isize = 3;
const EVENT_MOUSE_DOUBLE_CLICK: isize = 4;
const EVENT_MOUSE_DOWN_CHECK_DOUBLE: isize = 5;
const EVENT_MOUSE_UP: isize = 6;
const EVENT_MOUSE_WHEEL: isize = 7;
const EVENT_KEY_DOWN: isize = 8;
const EVENT_KEY_UP: isize = 9;
const EVENT_KEY_CHAR: isize = 10;
const EVENT_GOT_FOCUS: isize = 11;
const EVENT_LOST_FOCUS: isize = 12;
const EVENT_MOUSE_EXIT: isize = 13;
const EVENT_WINDOW_CLOSE: isize = 14;
/// `Window::WindowState_Open`.
const WINDOW_OPEN: isize = 0;

/// The GUI thread's part of the state.
#[derive(Debug, Default)]
pub(crate) struct Loop {
    /// `mWindowSet` (by address, as `SmartPtr<Window>::Comp` ordered it; each held)
    pub(crate) windows: BTreeSet<isize>,
    pub(crate) default_window: isize,
    /// `mWindowCount` (a `DWORD`)
    pub(crate) window_count: u32,
    /// `mTimerEvents`, `mRemoveTimerEvents` (each held)
    pub(crate) timers: BTreeSet<isize>,
    pub(crate) remove_timers: BTreeSet<isize>,
    pub(crate) calling_timers: bool,
    /// `mBroadcastSet` (each held)
    pub(crate) broadcast: BTreeSet<isize>,
    pub(crate) full_screen: bool,
    /// `mFullScreenWidth`, `mFullScreenHeight` (the original left them unset; nothing sets them)
    pub(crate) full_screen_size: (i32, i32),
    /// `mOriginalScreenWidth`, `mOriginalScreenHeight`
    pub(crate) original_size: (i32, i32),
    pub(crate) last_timer_tick: u32,
    pub(crate) last_input_tick: u32,
}

/// Which set `ee_won_wm_set_at` reads.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonWmSet {
    Windows = 0,
    Events,
    Timers,
    RemoveTimers,
    Broadcast,
}

/// The entry after `at` (the first with none) of a set.
fn next_in(set: &BTreeSet<isize>, at: Option<isize>) -> Option<isize> {
    match at {
        None => set.iter().next().copied(),
        Some(k) => set.range((Excluded(k), Unbounded)).next().copied(),
    }
}

impl EeWonWm {
    fn events_lock(&self) -> MutexGuard<'_, std::collections::VecDeque<isize>> {
        // a poisoned lock (a panic elsewhere) still guards a consistent list: every change is one statement
        self.events.lock().unwrap_or_else(|e| e.into_inner())
    }
    fn ev(&self, call: EeWonWmEvent, ev: isize, c: isize, d: isize) -> isize {
        self.op(Op::WmEvent, call as isize, ev, c, d)
    }
    fn ev_window(&self, ev: isize) -> isize {
        self.ev(EeWonWmEvent::Window, ev, 0, 0)
    }
    fn ev_mouse(&self, ev: isize) -> EeWonWmMouse {
        let mut m = EeWonWmMouse::default();
        self.ev(EeWonWmEvent::Mouse, ev, &mut m as *mut EeWonWmMouse as isize, 0);
        m
    }
    fn ev_set_mouse(&self, ev: isize, m: &EeWonWmMouse) {
        self.ev(EeWonWmEvent::SetMouse, ev, m as *const EeWonWmMouse as isize, 0);
    }
    fn hold_event(&self, ev: isize) {
        if ev != 0 {
            self.ev(EeWonWmEvent::Hold, ev, 0, 0);
        }
    }
    fn release_event(&self, ev: isize) {
        if ev != 0 {
            self.ev(EeWonWmEvent::Release, ev, 0, 0);
        }
    }
    fn f(&self, field: F) -> isize {
        self.field(field as i32)
    }

    /// The window set's entry after `at` (the first with `None`): the original's iterator over the live set.
    pub(crate) fn next_window(&self, at: Option<isize>) -> Option<isize> {
        next_in(&self.lp.borrow().windows, at)
    }

    pub(crate) fn loop_field(&self, f: i32) -> isize {
        let l = self.lp.borrow();
        let b = isize::from;
        match f {
            x if x == F::DefaultWindow as i32 => l.default_window,
            x if x == F::WindowCount as i32 => l.window_count as isize,
            x if x == F::FullScreen as i32 => b(l.full_screen),
            x if x == F::FullScreenWidth as i32 => l.full_screen_size.0 as isize,
            x if x == F::FullScreenHeight as i32 => l.full_screen_size.1 as isize,
            x if x == F::OriginalWidth as i32 => l.original_size.0 as isize,
            x if x == F::OriginalHeight as i32 => l.original_size.1 as isize,
            x if x == F::LastTimerTick as i32 => l.last_timer_tick as isize,
            x if x == F::LastInputTick as i32 => l.last_input_tick as isize,
            x if x == F::CallingTimers as i32 => b(l.calling_timers),
            x if x == F::WindowSetCount as i32 => l.windows.len() as isize,
            x if x == F::EventCount as i32 => {
                drop(l);
                self.events_lock().len() as isize
            }
            x if x == F::TimerCount as i32 => l.timers.len() as isize,
            x if x == F::RemoveTimerCount as i32 => l.remove_timers.len() as isize,
            x if x == F::BroadcastCount as i32 => l.broadcast.len() as isize,
            _ => 0,
        }
    }

    /// The settable fields: full screen and its size, the last timer tick (the constructor's).
    pub(crate) fn set_loop_field(&self, f: i32, v: isize) {
        let mut l = self.lp.borrow_mut();
        match f {
            x if x == F::FullScreen as i32 => l.full_screen = v != 0,
            x if x == F::FullScreenWidth as i32 => l.full_screen_size.0 = v as i32,
            x if x == F::FullScreenHeight as i32 => l.full_screen_size.1 = v as i32,
            x if x == F::LastTimerTick as i32 => l.last_timer_tick = v as u32,
            _ => {}
        }
    }

    /// A set's `i`-th entry (0 past the end; the goldens' trace).
    pub fn set_at(&self, which: u32, i: usize) -> isize {
        if which == EeWonWmSet::Events as u32 {
            return self.events_lock().get(i).copied().unwrap_or(0);
        }
        let l = self.lp.borrow();
        let set = match which {
            0 => &l.windows,
            2 => &l.timers,
            3 => &l.remove_timers,
            4 => &l.broadcast,
            _ => return 0,
        };
        set.iter().nth(i).copied().unwrap_or(0)
    }

    // ---- the screen resolution

    /// `SetScreenResolution`: the mode changed, the previous one remembered, only when both the width and the
    /// height differ from the current mode.
    pub fn set_screen_resolution(&self, width: i32, height: i32) {
        let (mut w, mut h) = (0i32, 0i32);
        self.op(Op::WmResolution, 0, &mut w as *mut i32 as isize, &mut h as *mut i32 as isize, 0);
        if w == width || h == height {
            return;
        }
        self.op(Op::WmResolution, 1, width as isize, height as isize, 0);
        self.lp.borrow_mut().original_size = (w, h);
    }

    /// `RestoreScreenResolution`: the open windows that ask for it minimized, then the remembered mode back.
    pub fn restore_screen_resolution(&self) {
        let mut at = None;
        while let Some(w) = self.next_window(at) {
            at = Some(w);
            if self.wn(EeWonWmWin::State, w, 0, 0) == WINDOW_OPEN
                && self.wn(EeWonWmWin::MinimizeOnRestore, w, 0, 0) != 0
            {
                self.wn(EeWonWmWin::Minimize, w, 1, 0);
            }
        }
        let (w, h) = self.lp.borrow().original_size;
        if w == 0 || h == 0 {
            return;
        }
        self.set_screen_resolution(w, h);
        self.lp.borrow_mut().original_size = (0, 0);
    }

    // ---- the window set

    /// `CloseAllWindows`: every window closed, the resulting events processed, the resolution restored when
    /// full screen. (The original held `mDataCrit` while closing; closing posts events on this thread only.)
    pub fn close_all_windows(&self) {
        let had = !self.lp.borrow().windows.is_empty();
        let mut at = None;
        while let Some(w) = self.next_window(at) {
            at = Some(w);
            self.wn(EeWonWmWin::Close, w, 0, 0);
        }
        if had {
            self.process_events();
        }
        if self.lp.borrow().full_screen {
            self.restore_screen_resolution();
        }
    }

    /// `AddWindow`: the window held in the set (the first one the default window); always true.
    pub fn add_window(&self, win: isize) -> bool {
        let inserted = {
            let mut l = self.lp.borrow_mut();
            if l.windows.is_empty() {
                l.default_window = win;
            }
            let inserted = l.windows.insert(win);
            if inserted {
                l.window_count = l.window_count.wrapping_add(1);
            }
            inserted
        };
        if inserted {
            self.hold_win(win);
        }
        true
    }

    /// `RemoveWindow`: the window given back, its queued events dropped, a new default window, its dialog ended
    /// and the saved popup window forgotten; false when it was not in the set.
    pub fn remove_window(&self, win: isize) -> bool {
        if !self.lp.borrow_mut().windows.remove(&win) {
            return false;
        }
        self.release_win(win);
        let empty_count = {
            let mut l = self.lp.borrow_mut();
            l.window_count = l.window_count.wrapping_sub(1);
            l.window_count == 0
        };
        if empty_count {
            self.set_field(F::TargetDepth as i32, 0);
        }
        {
            let mut l = self.lp.borrow_mut();
            l.default_window = l.windows.iter().next().copied().unwrap_or(0);
        }
        // the queued events of the window, in the list's order: each event's window read with the lock released,
        // a match taken out (its first entry still queued) and given back
        let queued: Vec<isize> = self.events_lock().iter().copied().collect();
        for ev in queued {
            if self.ev_window(ev) == win {
                let taken = {
                    let mut q = self.events_lock();
                    match q.iter().position(|&e| e == ev) {
                        Some(i) => q.remove(i).is_some(),
                        None => false,
                    }
                };
                if taken {
                    self.release_event(ev);
                }
            }
        }
        if win == self.f(F::ModalWindow) {
            self.end_dialog(-1, 0, 0);
        }
        if win == self.f(F::PopupWindowSave) {
            self.set_field(F::PopupWindowSave as i32, 0);
        }
        true
    }

    // ---- the events

    /// `PostEvent` (any thread): the event held and queued; the loop signalled when asked or when no message loop
    /// runs.
    pub fn post_event(&self, ev: isize, need_signal: bool) {
        self.hold_event(ev);
        let signal = {
            let mut q = self.events_lock();
            q.push_back(ev);
            need_signal || self.cur_depth.load(Ordering::SeqCst) <= 0
        };
        if signal {
            self.op(Op::WmSignal, 0, 0, 0, 0);
        }
    }

    /// `PeekNextEvent` (any thread): the next queued event, or 0.
    pub fn peek_next_event(&self) -> isize {
        self.events_lock().front().copied().unwrap_or(0)
    }

    /// `AddTimerEvent`: the event delivered on every timer step.
    pub fn add_timer_event(&self, ev: isize) {
        if self.lp.borrow_mut().timers.insert(ev) {
            self.hold_event(ev);
        }
    }

    /// `RemoveTimerEvent`: removed, or its removal deferred while the timers are delivered.
    pub fn remove_timer_event(&self, ev: isize) {
        let calling = self.lp.borrow().calling_timers;
        if calling {
            if self.lp.borrow_mut().remove_timers.insert(ev) {
                self.hold_event(ev);
            }
        } else if self.lp.borrow_mut().timers.remove(&ev) {
            self.release_event(ev);
        }
    }

    /// `RemoveAllTimerEvents`: every timer removed, or every removal deferred while the timers are delivered
    /// (the deferred set made a copy of the timers: its old entries given back, the timers held, in order).
    pub fn remove_all_timer_events(&self) {
        let calling = self.lp.borrow().calling_timers;
        if calling {
            let old = std::mem::take(&mut self.lp.borrow_mut().remove_timers);
            for ev in old {
                self.release_event(ev);
            }
            let timers = self.lp.borrow().timers.clone();
            for &ev in &timers {
                self.hold_event(ev);
            }
            self.lp.borrow_mut().remove_timers = timers;
        } else {
            let old = std::mem::take(&mut self.lp.borrow_mut().timers);
            for ev in old {
                self.release_event(ev);
            }
        }
    }

    // ---- the broadcasts

    /// `SubscribeToBroadcast`: the component held in the broadcast set, or taken out.
    pub fn subscribe_to_broadcast(&self, comp: isize, subscribe: bool) {
        if subscribe {
            if self.lp.borrow_mut().broadcast.insert(comp) {
                self.hold_comp(comp);
            }
        } else if self.lp.borrow_mut().broadcast.remove(&comp) {
            self.release_comp(comp);
        }
    }

    /// `ClearSubscriptions` (given back in address order).
    pub fn clear_subscriptions(&self) {
        let old = std::mem::take(&mut self.lp.borrow_mut().broadcast);
        for c in old {
            self.release_comp(c);
        }
    }

    /// `BroadcastEvent`: the event to every subscribed component.
    pub fn broadcast_event(&self, ev: isize) {
        let mut at = None;
        loop {
            let c = match next_in(&self.lp.borrow().broadcast, at) {
                Some(c) => c,
                None => break,
            };
            at = Some(c);
            self.cm(EeWonWmComp::HandleComponentEvent, c, ev, 0);
        }
    }

    // ---- the loop

    /// `ProcessEvents`: every 50 ms the popups' focus rule, the resolution restored after a focus loss, the windows'
    /// and the timer events' ticks (the deferred removals applied and, as in the original, never cleared); then
    /// the queued events delivered while the loop depth allows, and every window idled.
    pub fn process_events(&self) {
        let tick = self.op(Op::WmTick, 0, 0, 0, 0) as u32;
        let last = self.lp.borrow().last_timer_tick;
        if let Some(elapsed) = guievent::timer_step(tick, last) {
            self.lp.borrow_mut().last_timer_tick = tick;
            let focus = self.f(F::FocusWindow);
            let focus_is_popup = focus != 0 && self.wn(EeWonWmWin::IsPopup, focus, 0, 0) != 0;
            let step = guievent::popup_focus_step(
                self.f(F::PopupIsUp) != 0,
                focus == self.f(F::PopupOwnerWindow),
                self.f(F::PopupFlags) as i32,
                focus != 0,
                focus_is_popup,
                self.f(F::EndPopup) != 0,
            );
            match step {
                1 => {
                    self.set_field(F::EndPopup as i32, 0);
                    self.wn(EeWonWmWin::SetFocus, self.f(F::PopupOwnerWindow), 0, 0);
                }
                2 => self.end_popup(0),
                3 => self.set_field(F::EndPopup as i32, 1),
                _ => self.set_field(F::EndPopup as i32, 0),
            }
            if guievent::restore_due(self.f(F::LastLoseFocusTick) as u32, self.f(F::FocusWindow) != 0, tick) {
                self.set_field(F::LastLoseFocusTick as i32, 0);
                if self.lp.borrow().full_screen {
                    self.restore_screen_resolution();
                }
            }
            let mut at = None;
            while let Some(w) = self.next_window(at) {
                at = Some(w);
                self.wn(EeWonWmWin::TimerEvent, w, elapsed as isize, 0);
            }
            if !self.lp.borrow().timers.is_empty() {
                self.lp.borrow_mut().calling_timers = true;
                let mut at = None;
                loop {
                    let t = match next_in(&self.lp.borrow().timers, at) {
                        Some(t) => t,
                        None => break,
                    };
                    at = Some(t);
                    self.ev(EeWonWmEvent::Deliver, t, 0, 0);
                }
                self.lp.borrow_mut().calling_timers = false;
            }
            let mut at = None;
            loop {
                let r = match next_in(&self.lp.borrow().remove_timers, at) {
                    Some(r) => r,
                    None => break,
                };
                at = Some(r);
                if self.lp.borrow_mut().timers.remove(&r) {
                    self.release_event(r);
                }
            }
        }
        loop {
            let go = !self.events_lock().is_empty()
                && self.cur_depth.load(Ordering::SeqCst) <= self.f(F::TargetDepth) as i32;
            if !go {
                break;
            }
            // the list's reference becomes the local's (the original's copy and pop_front, AddRef then Release)
            let ev = match self.events_lock().pop_front() {
                Some(e) => e,
                None => break,
            };
            self.deliver_event(ev);
            self.release_event(ev);
        }
        let mut at = None;
        while let Some(w) = self.next_window(at) {
            at = Some(w);
            self.wn(EeWonWmWin::IdleUpdate, w, 0, 0);
        }
    }

    /// The C++ `FindMouseWindow(MouseEvent*)` / `CheckPopupClick(MouseEvent*)` over an event: its fields read, the
    /// window and place written back.
    fn find_mouse_on(&self, ev: isize) {
        let mut m = self.ev_mouse(ev);
        self.find_mouse_window(&mut m);
        self.ev_set_mouse(ev, &m);
    }

    /// `DeliverEvent`: one event dispatched with the popup, modal and full-screen rules; repeated moves and size
    /// changes merged. The event's window is read again after every call that may change it.
    pub fn deliver_event(&self, ev: isize) {
        let ty = self.ev(EeWonWmEvent::Type, ev, 0, 0);
        match ty {
            EVENT_MOUSE_DOUBLE_CLICK | EVENT_MOUSE_DOWN_CHECK_DOUBLE | EVENT_MOUSE_DOWN => {
                let mut eaten = false;
                if self.f(F::PopupIsUp) != 0 {
                    let mut m = self.ev_mouse(ev);
                    eaten = self.check_popup_click(&mut m);
                    self.ev_set_mouse(ev, &m);
                }
                if !eaten {
                    let w = self.ev_window(ev);
                    let focus = self.f(F::FocusWindow);
                    let popup = self.wn(EeWonWmWin::IsPopup, w, 0, 0) != 0;
                    if guievent::press_focuses(focus == w, popup, focus != w && !popup && self.not_modal_window(w)) {
                        self.wn(EeWonWmWin::SetFocus, w, 0, 0);
                    }
                    let button = self.ev(EeWonWmEvent::Button, ev, 0, 0) as i32;
                    if guievent::capture_changes(button, self.f(F::PopupIsUp) != 0, self.f(F::CaptureSupport) != 0) {
                        self.wn(EeWonWmWin::SetCapture, self.ev_window(ev), 1, 0);
                    }
                    let w = self.ev_window(ev);
                    if self.wn(EeWonWmWin::InClient, w, 0, 0) != 0 {
                        self.wn(EeWonWmWin::HandleMouse, w, ev, 0);
                    }
                }
            }
            EVENT_MOUSE_MOVE => {
                let next = self.peek_next_event();
                if next != 0
                    && self.ev(EeWonWmEvent::Type, next, 0, 0) == EVENT_MOUSE_MOVE
                    && self.ev_window(next) == self.ev_window(ev)
                {
                    return;
                }
                self.find_mouse_on(ev);
                if guievent::move_ends_popup(self.f(F::PopupIsUp) != 0, self.f(F::PopupFlags) as i32) {
                    self.end_popup(0);
                }
                self.wn(EeWonWmWin::HandleMouse, self.ev_window(ev), ev, 0);
            }
            EVENT_MOUSE_UP => {
                self.find_mouse_on(ev);
                let button = self.ev(EeWonWmEvent::Button, ev, 0, 0) as i32;
                if button == 0 {
                    self.set_field(F::MouseDownWindow as i32, 0);
                }
                if guievent::capture_changes(button, self.f(F::PopupIsUp) != 0, self.f(F::CaptureSupport) != 0) {
                    self.wn(EeWonWmWin::SetCapture, self.ev_window(ev), 0, 0);
                }
                self.wn(EeWonWmWin::HandleMouse, self.ev_window(ev), ev, 0);
            }
            EVENT_MOUSE_WHEEL => {
                self.find_mouse_on(ev);
                if guievent::wheel_to_popup(self.f(F::PopupIsUp) != 0, self.f(F::PopupFlags) as i32) {
                    // the topmost popup; none (popups flagged up mid-EndPopup with the list empty: the original
                    // read the back of an empty list) takes nothing
                    let popup = self.f(F::PopupBack);
                    let amount = self.ev(EeWonWmEvent::Button, ev, 0, 0);
                    if popup != 0 {
                        self.cm(EeWonWmComp::MouseWheel, popup, amount, 0);
                    }
                    return;
                }
                self.wn(EeWonWmWin::HandleMouse, self.ev_window(ev), ev, 0);
            }
            EVENT_KEY_DOWN | EVENT_KEY_UP | EVENT_KEY_CHAR => {
                let key = self.ev(EeWonWmEvent::Key, ev, 0, 0) as i32;
                let step = guievent::popup_key(self.f(F::PopupIsUp) != 0, ty as i32, self.f(F::PopupFlags) as i32, key);
                if step == 1 {
                    self.end_popup(0);
                } else {
                    if step == 2 {
                        let window = self.cm(EeWonWmComp::Root, self.f(F::PopupBack), 0, 0);
                        if window != self.ev_window(ev) {
                            self.ev(EeWonWmEvent::SetWindow, ev, window, 0);
                        }
                    }
                    self.wn(EeWonWmWin::HandleKey, self.ev_window(ev), ev, 0);
                }
            }
            EVENT_GOT_FOCUS => {
                let (full, size) = {
                    let l = self.lp.borrow();
                    (l.full_screen, l.full_screen_size)
                };
                if full {
                    self.set_screen_resolution(size.0, size.1);
                }
                let w = self.ev_window(ev);
                self.notify_got_focus(w);
                self.wn(EeWonWmWin::HandleInput, w, ev, 0);
                if self.not_modal_window(self.ev_window(ev)) {
                    self.wn(EeWonWmWin::SetFocus, self.f(F::ModalWindow), 0, 0);
                }
                return;
            }
            EVENT_LOST_FOCUS => {
                self.notify_lost_focus();
                self.wn(EeWonWmWin::HandleInput, self.ev_window(ev), ev, 0);
                return;
            }
            EVENT_SIZE_CHANGE => {
                let w = self.ev_window(ev);
                if w == self.f(F::PopupOwnerWindow) {
                    self.end_popup(0);
                } else {
                    let next = self.peek_next_event();
                    if next != 0
                        && self.ev(EeWonWmEvent::Type, next, 0, 0) == EVENT_SIZE_CHANGE
                        && self.ev_window(next) == w
                    {
                        let mut a = EeWonRect4::default();
                        let mut b = EeWonRect4::default();
                        self.ev(EeWonWmEvent::Size, ev, &mut a as *mut EeWonRect4 as isize, 0);
                        self.ev(EeWonWmEvent::Size, next, &mut b as *mut EeWonRect4 as isize, 0);
                        let m = guievent::size_merge([a.x, a.y, a.w, a.h], [b.x, b.y, b.w, b.h]);
                        let merged = EeWonRect4 { x: m[0], y: m[1], w: m[2], h: m[3] };
                        self.ev(EeWonWmEvent::SetSize, next, &merged as *const EeWonRect4 as isize, 0);
                        return;
                    }
                }
                self.wn(EeWonWmWin::HandleInput, self.ev_window(ev), ev, 0);
                return;
            }
            EVENT_MOUSE_EXIT => {
                self.wn(EeWonWmWin::HandleInput, self.ev_window(ev), ev, 0);
                return;
            }
            EVENT_WINDOW_CLOSE => {
                let w = self.ev_window(ev);
                let popup = self.wn(EeWonWmWin::IsPopup, w, 0, 0) != 0;
                if guievent::close_ends_popup(self.f(F::PopupIsUp) != 0, w == self.f(F::PopupOwnerWindow), popup) {
                    self.end_popup(0);
                }
                let w = self.ev_window(ev);
                if self.f(F::FocusWindow) == w {
                    self.notify_lost_focus();
                }
                if self.f(F::MouseDownWindow) == w {
                    self.set_field(F::MouseDownWindow as i32, 0);
                }
                self.wn(EeWonWmWin::PlatformClose, w, 0, 0);
                // the window's close event, held by a local until after RemoveWindow
                let w = self.ev_window(ev);
                let close = self.wn(EeWonWmWin::CloseEvent, w, 0, 0);
                self.hold_event(close);
                if close != 0 {
                    self.ev(EeWonWmEvent::SetWindow, close, w, 0);
                    self.ev(EeWonWmEvent::Deliver, close, 0, 0);
                }
                // RemoveWindow is virtual; no subclass overrides it
                self.remove_window(self.ev_window(ev));
                self.release_event(close);
                return;
            }
            _ => {
                self.ev(EeWonWmEvent::Deliver, ev, 0, 0);
                return;
            }
        }
        let tick = self.op(Op::WmTick, 0, 0, 0, 0) as u32;
        self.lp.borrow_mut().last_input_tick = tick;
    }

    /// The containers given back as the original's destructor did (the members in reverse order: the broadcast
    /// set, the deferred timer removals, the timer events, the event list, the window set).
    pub(crate) fn release_loop(&self) {
        self.clear_subscriptions();
        let old = std::mem::take(&mut self.lp.borrow_mut().remove_timers);
        for ev in old {
            self.release_event(ev);
        }
        let old = std::mem::take(&mut self.lp.borrow_mut().timers);
        for ev in old {
            self.release_event(ev);
        }
        let queued: Vec<isize> = std::mem::take(&mut *self.events_lock()).into_iter().collect();
        for ev in queued {
            self.release_event(ev);
        }
        let old = std::mem::take(&mut self.lp.borrow_mut().windows);
        for w in old {
            self.release_win(w);
        }
    }
}

/// A set's `i`-th entry (`which` an `EeWonWmSet`; 0 past the end; the goldens' trace).
///
/// # Safety
/// `wm` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_wm_set_at(wm: *const EeWonWm, which: u32, i: usize) -> isize {
    // SAFETY: live (the contract)
    unsafe { (*wm).set_at(which, i) }
}

#[cfg(test)]
#[path = "wmloop_tests.rs"]
mod tests;
