//! DialogLogic whole in Rust (R6 WONLobby round 2): the lobby's modal boxes (status waits, errors, OK/Cancel and
//! Yes/No questions) and the dialog stack they run on, inline in the lobby window or in popup windows.
//!
//! The C++ class stays the type its callers hold (`gLobbyData->mDialogLogic`, a `LobbyLogic` in the logic manager)
//! with one member, this object; its methods forward here. Every call on another object (a window, a component,
//! an async operation, the status and OK/Cancel dialogs, the lobby's globals) is one op of the host given to
//! `ee_won_dlglogic_new` (`DialogLogicHost` in `WONLobby/DialogLogic.cpp`).
//!
//! What the original held by `SmartPtr` (the two dialogs, the status operation, each stack entry's window and
//! component) is held here by reference: an op hands a pointer with a reference added and `Release*` ends it, in
//! the order the original's assignments and destructors did (an assignment releases the old pointer, then adds a
//! reference to the new one, and does nothing when they are equal).
//!
//! Re-entry: `RunDialog` runs a modal loop in which any lobby code runs, this object's calls included (a nested
//! dialog, the status operation's completion, a reset); `RunOp` may complete at once. So the state is in `Cell`s and
//! one `RefCell` stack that is never borrowed across an op, and DoDialog finds its own entry again by id after the
//! loop (the original kept a reference into its `std::list`, which a nested ClearDialogs never erases: the entry
//! is modal then).

use std::cell::{Cell, RefCell};
use std::ffi::c_void;

/// `WS_Success`.
const WS_SUCCESS: i32 = 0;
/// `WS_Pending`.
const WS_PENDING: i32 = 100_000;

/// The ops (`a`..`d` as documented).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonDlgLogicOp {
    /// `SafeGetComponent(a, aPtr, "OkCancelDialog", "DialogLogic.cpp", 48)` into a `SmartPtr<OkCancelDialog>`: the
    /// dialog with a reference added, or NULL.
    FindOkCancel = 1,
    /// `SafeGetComponent(a, aPtr, "StatusDialog", "DialogLogic.cpp", 49)`: the dialog with a reference, or NULL.
    FindStatus,
    /// `WindowManager::GetDefaultWindowManager()`: -1 when NULL, else its `GetPopupsInOwnWindow()` (0 or 1).
    WmPopups,
    /// `a->Release()` of a component (a dialog, a container).
    ReleaseComponent,
    /// `a->AddRef()` of a component.
    AddRefComponent,
    /// `a->Release()` of a window.
    ReleaseWindow,
    /// `a->AddRef()` of a window.
    AddRefWindow,
    /// `a->Release()` of an async operation.
    ReleaseOp,
    /// `a->AddRef()` of an async operation.
    AddRefOp,
    /// `((AsyncOp*)a)->Kill(true)`.
    OpKill,
    /// `((AsyncOp*)a)->ForceFinish(WS_Killed)`.
    OpForceFinishKilled,
    /// `((AsyncOp*)a)->GetStatus()`.
    OpGetStatus,
    /// `this->RunOp(a, new LogicCompletion<DialogLogic>(&DialogLogic::StatusOpComplete, this), b)`.
    RunOp,
    /// `GetLobbyWindow()->EndDialog(-1, NULL, a)` (the status dialog).
    EndDialog,
    /// `GetLobbyRoot()` (not held).
    LobbyRoot,
    /// `LobbyGetRootWindow()` (not held).
    RootWindow,
    /// `a->mParent` (a component).
    Parent,
    /// `((Container*)a)->RemoveChild(b)`.
    RemoveChild,
    /// `((Window*)a)->Close()`.
    WindowClose,
    /// `a->Width()` (a component).
    Width,
    /// `a->Height()`.
    Height,
    /// `a->GetBounds()` into `*(int(*)[4])b`: x, y, width, height.
    Bounds,
    /// `CreateNativeWindow()` into a `SmartPtr<Window>`, then its `SetOneComponentWindow(true)`: the window with a
    /// reference added.
    NewWindow,
    /// `a->SetParent(b); a->SetStyle(0x22); a->Create(WONRectangle(0, 0, 100, 100));
    /// LobbyAddLogicListener(a->mRoot); a->mRoot->SetColorScheme(LobbyGetColorScheme())`.
    SetUpPopup,
    /// `((Window*)a)->SetClientSize(b, c)`.
    SetClientSize,
    /// `((Window*)a)->mRoot.get()` (not held).
    WindowRoot,
    /// `((Window*)a)->Center(true)`.
    WindowCenter,
    /// `CenterLayout::Center((Container*)a, b, true, true)`.
    CenterLayout,
    /// `((Container*)a)->AddChild(b)`.
    AddChild,
    /// `((Window*)a)->Show(true)`.
    WindowShow,
    /// `((Window*)a)->RunDialog(b)`: the dialog's result.
    RunDialog,
    /// `LobbyIsSuspended()` (0 or 1).
    LobbySuspended,
    /// `((StatusDialog*)a)->SetText(*(GUIString*)b, *(GUIString*)c, d != 0)`.
    StatusSetText,
    /// `((StatusDialog*)a)->mStartTick = WONTickCount()`.
    StatusSetStartTick,
    /// `((StatusDialog*)a)->SetStatus(GetWONStatusString(b), false)`.
    StatusSetStatus,
    /// `LobbyGetLobbySoundEffects()` (0 or 1).
    SoundEffects,
    /// `LobbyPlaySound(LobbyGlobal_Error_Sound.get())`.
    PlayErrorSound,
    /// `((OkCancelDialog*)a)->SetText(*(GUIString*)b, *(GUIString*)c, OK or Yes, Cancel or No)`: Yes/No when
    /// `d != 0`.
    OkCancelSetText,
    /// `a->GetRootWindow()` (a component; not held).
    RootWindowOf,
    /// `((Container*)a)->InvalidateRect(WONRectangle(*(int(*)[4])b))`.
    InvalidateRect,
}

/// The inward calls (the C++ class's methods).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonDlgLogicCall {
    /// `InitComponent(a)`.
    Init = 1,
    /// `KillHook()`.
    KillHook,
    /// `Reset()` (= ClearDialogs).
    Reset,
    /// `StatusOpComplete(a)`.
    StatusOpComplete,
    /// `ClearDialogs()`.
    ClearDialogs,
    /// `DoDialog(a)`: the dialog's result.
    DoDialog,
    /// `RunOpWithStatus(a, title b, status c, mode d, timeout e)`.
    RunOpWithStatus,
    /// `ShowStatus(title a, status b, cancel c != 0)`.
    ShowStatus,
    /// `ShowError(title a, text b)`.
    ShowError,
    /// `UpdateStatus(a)`.
    UpdateStatus,
    /// `ShowOkCancel(title a, text b, sound c != 0)`.
    ShowOkCancel,
    /// `ShowYesNo(title a, text b, sound c != 0)`.
    ShowYesNo,
    /// a field: 0 `mStatusCancelled`, 1 `mPopupWindows`, 2 `mStatusVisible`, 3 `mStatusMode`, 4 `mStatusOp`,
    /// 5 the OK/Cancel dialog, 6 the status dialog, 7 the stack's depth.
    Field,
}

/// The host: `(logic, op, a, b, c, d) -> result`; `logic` is the C++ DialogLogic.
pub type EeWonDlgLogicHostFn = Option<
    unsafe extern "C" fn(logic: *mut c_void, op: EeWonDlgLogicOp, a: isize, b: isize, c: isize, d: isize) -> isize,
>;

/// One entry of the dialog stack (the original's `DialogInfo`).
#[derive(Clone, Copy, Debug, Default)]
struct Entry {
    id: u64,
    window: isize,
    component: isize,
    rect: [i32; 4],
    in_modal: bool,
}

use EeWonDlgLogicOp as Op;

/// DialogLogic's Rust half.
pub struct EeWonDlgLogic {
    logic: *mut c_void,
    host: EeWonDlgLogicHostFn,
    stack: RefCell<Vec<Entry>>,
    next_id: Cell<u64>,
    popup_windows: Cell<bool>,
    status_cancelled: Cell<bool>,
    ok_cancel: Cell<isize>,
    status: Cell<isize>,
    /// unset by the original's constructor (InitComponent sets it): false here
    status_visible: Cell<bool>,
    status_op: Cell<isize>,
    /// unset by the original's constructor: 0 here
    status_mode: Cell<i32>,
}

impl EeWonDlgLogic {
    pub fn new(logic: *mut c_void, host: EeWonDlgLogicHostFn) -> Self {
        Self {
            logic,
            host,
            stack: RefCell::new(Vec::new()),
            next_id: Cell::new(1),
            popup_windows: Cell::new(false),
            status_cancelled: Cell::new(false),
            ok_cancel: Cell::new(0),
            status: Cell::new(0),
            status_visible: Cell::new(false),
            status_op: Cell::new(0),
            status_mode: Cell::new(0),
        }
    }

    fn op(&self, op: Op, a: isize, b: isize, c: isize, d: isize) -> isize {
        match self.host {
            // SAFETY: the host is the C++ logic's (or a test's), called with the logic it was given for
            Some(f) => unsafe { f(self.logic, op, a, b, c, d) },
            None => 0,
        }
    }

    fn op1(&self, op: Op, a: isize) -> isize {
        self.op(op, a, 0, 0, 0)
    }

    /// A `SmartPtr` assignment: the old pointer released, then a reference added to the new one; nothing when equal.
    fn assign(&self, cell: &Cell<isize>, new: isize, add: Op, release: Op) {
        let old = cell.get();
        if old == new {
            return;
        }
        if old != 0 {
            self.op1(release, old);
        }
        cell.set(new);
        if new != 0 {
            self.op1(add, new);
        }
    }

    /// `SafeGetComponent` into a member: the found dialog (a reference added by the op) replaces the old one.
    fn find(&self, cell: &Cell<isize>, op: Op, config: isize) {
        let found = self.op1(op, config);
        let old = cell.replace(found);
        if old != 0 {
            self.op1(Op::ReleaseComponent, old);
        }
    }

    fn init(&self, config: isize) {
        self.status_visible.set(false);
        self.status_mode.set(0);
        self.assign(&self.status_op, 0, Op::AddRefOp, Op::ReleaseOp);
        self.find(&self.ok_cancel, Op::FindOkCancel, config);
        self.find(&self.status, Op::FindStatus, config);
        let popups = self.op(Op::WmPopups, 0, 0, 0, 0);
        if popups >= 0 {
            self.popup_windows.set(popups != 0);
        }
    }

    fn kill_hook(&self) {
        let op = self.status_op.get();
        if op != 0 {
            self.op1(Op::OpKill, op);
            self.assign(&self.status_op, 0, Op::AddRefOp, Op::ReleaseOp);
        }
        self.clear_dialogs();
    }

    fn status_op_complete(&self, op: isize) {
        if op != self.status_op.get() {
            return;
        }
        let mode = self.status_mode.get();
        let done = mode == 2 || (mode == 1 && self.op1(Op::OpGetStatus, op) as i32 == WS_SUCCESS);
        if done {
            self.op1(Op::EndDialog, self.status.get());
        } else {
            let status = self.op1(Op::OpGetStatus, op);
            self.update_status(status as i32);
        }
    }

    fn back(&self) -> Option<Entry> {
        self.stack.borrow().last().copied()
    }

    fn clear_dialogs(&self) {
        if self.stack.borrow().is_empty() || self.op(Op::LobbyRoot, 0, 0, 0, 0) == 0 {
            return;
        }
        let mut changed = false;
        let mut i = self.stack.borrow().len();
        while i > 0 {
            i -= 1;
            let Some(e) = self.stack.borrow().get(i).copied() else { break };
            if e.in_modal {
                break;
            }
            let parent = self.op1(Op::Parent, e.component);
            if parent != 0 {
                self.op(Op::RemoveChild, parent, e.component, 0, 0);
            }
            if self.popup_windows.get() {
                self.op1(Op::WindowClose, e.window);
            }
            changed = true;
        }
        if !changed {
            return;
        }
        // every entry not running is erased (its SmartPtrs destroyed: the component, then the window)
        let gone: Vec<Entry> = {
            let mut stack = self.stack.borrow_mut();
            let gone = stack.iter().filter(|e| !e.in_modal).copied().collect();
            stack.retain(|e| e.in_modal);
            gone
        };
        for e in gone {
            if e.component != 0 {
                self.op1(Op::ReleaseComponent, e.component);
            }
            if e.window != 0 {
                self.op1(Op::ReleaseWindow, e.window);
            }
        }
    }

    /// `CheckDialog`: whether `dialog` is the top entry, and whether its size changed since it was shown.
    fn check_dialog(&self, dialog: isize) -> (bool, bool) {
        let Some(e) = self.back() else { return (false, true) };
        if e.component != dialog {
            return (false, true);
        }
        let same = self.op1(Op::Width, dialog) as i32 == e.rect[2] && self.op1(Op::Height, dialog) as i32 == e.rect[3];
        (true, !same)
    }

    /// `GetDialogWindow`: the window held (a reference added), or 0.
    fn get_dialog_window(&self, reuse: bool) -> isize {
        if !self.popup_windows.get() {
            let w = self.op(Op::RootWindow, 0, 0, 0, 0);
            if w != 0 {
                self.op1(Op::AddRefWindow, w);
            }
            return w;
        }
        if reuse {
            if let Some(e) = self.back() {
                if e.window != 0 {
                    self.op1(Op::AddRefWindow, e.window);
                }
                return e.window;
            }
        }
        let w = self.op(Op::NewWindow, 0, 0, 0, 0);
        let parent = match self.back() {
            Some(e) => e.window,
            None => self.op(Op::RootWindow, 0, 0, 0, 0),
        };
        self.op(Op::SetUpPopup, w, parent, 0, 0);
        w
    }

    fn do_dialog(&self, dialog: isize) -> i32 {
        let (reuse, size_changed) = self.check_dialog(dialog);
        if !reuse {
            self.clear_dialogs();
        }
        let window = self.get_dialog_window(reuse);
        if window == 0 {
            return -1;
        }
        let popup = self.popup_windows.get();
        let root = if popup {
            let w = self.op1(Op::Width, dialog);
            let h = self.op1(Op::Height, dialog);
            self.op(Op::SetClientSize, window, w, h, 0);
            self.op1(Op::WindowRoot, window)
        } else {
            self.op(Op::LobbyRoot, 0, 0, 0, 0)
        };
        if root != 0 {
            self.op1(Op::AddRefComponent, root);
        }
        if root == 0 {
            self.op1(Op::ReleaseWindow, window);
            return -1;
        }
        if size_changed {
            if popup {
                self.op1(Op::WindowCenter, window);
            } else {
                self.op(Op::CenterLayout, root, dialog, 0, 0);
            }
        }
        if !reuse {
            self.op(Op::AddChild, root, dialog, 0, 0);
            let id = self.next_id.get();
            self.next_id.set(id + 1);
            self.stack.borrow_mut().push(Entry { id, ..Entry::default() });
        }
        let id = self.back().map(|e| e.id).unwrap_or(0);
        self.set_entry_window(id, window);
        self.set_entry_component(id, dialog);
        let mut rect = [0i32; 4];
        self.op(Op::Bounds, dialog, rect.as_mut_ptr() as isize, 0, 0);
        self.with_entry(id, |e| {
            e.in_modal = true;
            e.rect = rect;
        });
        if popup {
            self.op1(Op::WindowShow, window);
        }
        let result = self.op(Op::RunDialog, window, dialog, 0, 0) as i32;
        self.with_entry(id, |e| e.in_modal = false);
        self.op1(Op::ReleaseComponent, root);
        self.op1(Op::ReleaseWindow, window);
        result
    }

    fn with_entry(&self, id: u64, f: impl FnOnce(&mut Entry)) {
        if let Some(e) = self.stack.borrow_mut().iter_mut().find(|e| e.id == id) {
            f(e);
        }
    }

    fn entry_field(&self, id: u64, window: bool) -> Option<isize> {
        self.stack.borrow().iter().find(|e| e.id == id).map(|e| if window { e.window } else { e.component })
    }

    /// `anInfo.mWindow = aWindow.get()`: the SmartPtr assignment on the entry.
    fn set_entry_window(&self, id: u64, window: isize) {
        let Some(old) = self.entry_field(id, true) else { return };
        if old == window {
            return;
        }
        if old != 0 {
            self.op1(Op::ReleaseWindow, old);
        }
        self.with_entry(id, |e| e.window = window);
        if window != 0 {
            self.op1(Op::AddRefWindow, window);
        }
    }

    /// `anInfo.mComponent = inDialog`.
    fn set_entry_component(&self, id: u64, component: isize) {
        let Some(old) = self.entry_field(id, false) else { return };
        if old == component {
            return;
        }
        if old != 0 {
            self.op1(Op::ReleaseComponent, old);
        }
        self.with_entry(id, |e| e.component = component);
        if component != 0 {
            self.op1(Op::AddRefComponent, component);
        }
    }

    fn run_op_with_status(&self, op: isize, title: isize, status: isize, mode: i32, timeout: isize) {
        if self.status_visible.get() {
            self.op1(Op::OpForceFinishKilled, op);
            return;
        }
        self.assign(&self.status_op, op, Op::AddRefOp, Op::ReleaseOp);
        self.status_cancelled.set(false);
        self.op(Op::RunOp, self.status_op.get(), timeout, 0, 0);
        self.status_mode.set(mode);
        self.show_status(title, status, true);
        let held = self.status_op.get();
        // the original read the operation again here; a KillHook in the dialog's loop had cleared it (a NULL read
        // in the original): nothing to cancel then
        if held != 0 && self.op1(Op::OpGetStatus, held) as i32 == WS_PENDING {
            self.status_cancelled.set(true);
            self.op1(Op::OpKill, held);
        }
        self.assign(&self.status_op, 0, Op::AddRefOp, Op::ReleaseOp);
    }

    fn show_status(&self, title: isize, status: isize, cancel: bool) {
        if self.status_visible.get() || self.op(Op::LobbySuspended, 0, 0, 0, 0) != 0 {
            return;
        }
        self.status_visible.set(true);
        let dialog = self.status.get();
        self.op(Op::StatusSetText, dialog, title, status, cancel as isize);
        self.do_dialog(dialog);
        self.op1(Op::StatusSetStartTick, self.status.get());
        self.status_visible.set(false);
    }

    fn play_error_sound(&self) {
        if self.op(Op::SoundEffects, 0, 0, 0, 0) != 0 {
            self.op(Op::PlayErrorSound, 0, 0, 0, 0);
        }
    }

    fn show_error(&self, title: isize, text: isize) {
        if self.status_visible.get() {
            return;
        }
        self.play_error_sound();
        self.show_status(title, text, false);
    }

    fn update_status(&self, status: i32) {
        let dialog = self.status.get();
        let mut old = [0i32; 4];
        self.op(Op::Bounds, dialog, old.as_mut_ptr() as isize, 0, 0);
        self.op(Op::StatusSetStatus, dialog, status as isize, 0, 0);
        if status != WS_SUCCESS {
            self.play_error_sound();
        }
        if old[2] == self.op1(Op::Width, dialog) as i32 && old[3] == self.op1(Op::Height, dialog) as i32 {
            return;
        }
        if self.popup_windows.get() {
            let window = self.op1(Op::RootWindowOf, dialog);
            if window == 0 {
                return;
            }
            let w = self.op1(Op::Width, dialog);
            let h = self.op1(Op::Height, dialog);
            self.op(Op::SetClientSize, window, w, h, 0);
            self.op1(Op::WindowCenter, window);
        } else {
            let root = self.op(Op::LobbyRoot, 0, 0, 0, 0);
            if root != 0 {
                self.op(Op::CenterLayout, root, dialog, 0, 0);
                let mut rect = [0i32; 4];
                self.op(Op::Bounds, dialog, rect.as_mut_ptr() as isize, 0, 0);
                self.op(Op::InvalidateRect, root, rect.as_ptr() as isize, 0, 0);
            }
        }
    }

    fn show_ok_cancel(&self, title: isize, text: isize, sound: bool, yes_no: bool) -> i32 {
        self.op(Op::OkCancelSetText, self.ok_cancel.get(), title, text, yes_no as isize);
        if sound {
            self.play_error_sound();
        }
        self.do_dialog(self.ok_cancel.get())
    }

    /// One inward call.
    pub fn call(&self, call: EeWonDlgLogicCall, a: isize, b: isize, c: isize, d: isize, e: isize) -> isize {
        use EeWonDlgLogicCall as C;
        match call {
            C::Init => self.init(a),
            C::KillHook => self.kill_hook(),
            C::Reset | C::ClearDialogs => self.clear_dialogs(),
            C::StatusOpComplete => self.status_op_complete(a),
            C::DoDialog => return self.do_dialog(a) as isize,
            C::RunOpWithStatus => self.run_op_with_status(a, b, c, d as i32, e),
            C::ShowStatus => self.show_status(a, b, c != 0),
            C::ShowError => self.show_error(a, b),
            C::UpdateStatus => self.update_status(a as i32),
            C::ShowOkCancel => return self.show_ok_cancel(a, b, c != 0, false) as isize,
            C::ShowYesNo => return self.show_ok_cancel(a, b, c != 0, true) as isize,
            C::Field => {
                return match a {
                    0 => self.status_cancelled.get() as isize,
                    1 => self.popup_windows.get() as isize,
                    2 => self.status_visible.get() as isize,
                    3 => self.status_mode.get() as isize,
                    4 => self.status_op.get(),
                    5 => self.ok_cancel.get(),
                    6 => self.status.get(),
                    7 => self.stack.borrow().len() as isize,
                    _ => 0,
                }
            }
        }
        0
    }

    /// The destructor: the members in reverse order (the status operation, the two dialogs, then the stack's
    /// entries in order, each its component then its window).
    fn release_all(&self) {
        let op = self.status_op.replace(0);
        if op != 0 {
            self.op1(Op::ReleaseOp, op);
        }
        for cell in [&self.status, &self.ok_cancel] {
            let c = cell.replace(0);
            if c != 0 {
                self.op1(Op::ReleaseComponent, c);
            }
        }
        let entries = std::mem::take(&mut *self.stack.borrow_mut());
        for e in entries {
            if e.component != 0 {
                self.op1(Op::ReleaseComponent, e.component);
            }
            if e.window != 0 {
                self.op1(Op::ReleaseWindow, e.window);
            }
        }
    }
}

/// A new DialogLogic object for the C++ `logic`.
#[no_mangle]
pub extern "C" fn ee_won_dlglogic_new(logic: *mut c_void, host: EeWonDlgLogicHostFn) -> *mut EeWonDlgLogic {
    Box::into_raw(Box::new(EeWonDlgLogic::new(logic, host)))
}

/// Releases what the object holds and frees it.
///
/// # Safety
/// `obj` came from `ee_won_dlglogic_new` (or is NULL) and is not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_won_dlglogic_free(obj: *mut EeWonDlgLogic) {
    if !obj.is_null() {
        // SAFETY: the contract; the releases run through a shared reference before the box is dropped
        unsafe { (*obj).release_all() };
        // SAFETY: the contract
        drop(unsafe { Box::from_raw(obj) });
    }
}

/// One inward call (`EeWonDlgLogicCall`).
///
/// # Safety
/// `obj` is live; the arguments as documented.
#[no_mangle]
pub unsafe extern "C" fn ee_won_dlglogic_call(
    obj: *const EeWonDlgLogic,
    call: EeWonDlgLogicCall,
    a: isize,
    b: isize,
    c: isize,
    d: isize,
    e: isize,
) -> isize {
    // SAFETY: live (the contract); a shared reference: the calls out re-enter through this function
    unsafe { (*obj).call(call, a, b, c, d, e) }
}

#[cfg(test)]
#[path = "dlglogic_tests.rs"]
mod tests;
