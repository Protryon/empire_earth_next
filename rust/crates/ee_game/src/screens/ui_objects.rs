//! The UI thread's two objects as Rust records (the UI cluster lane, round 2): the game's UI manager's own members
//! (`EEUIManager : UIManager, EeScreensUiManagerFields`, ee/EEUIManager.h: the main game and scenario editor forms,
//! the editor flags) and the UI thread object whole (`EEUserInterface : EeScreensUserInterfaceFields`,
//! ee/EEUserInterface.h: its thread, its message-done event, the seed, the flags; the class has no other member and
//! no vtable). The members keep their C++ names, types and order, so the C++ that reads them by name
//! (`gUIManager->mMainGameForm`, `gUserInterface->mRandomSeed`) is unchanged.
//!
//! Each record is registered by its constructor (one manager and one UI object live at a time: `gUIManager`,
//! `gUserInterface`); the hosts' ops on them are answered here, below the hosts' traits (`ui_manager_ffi.rs`,
//! `ui_game_ffi.rs`, `ui_thread_ffi.rs`, `menus_ffi.rs`). The UI object's thread messages are posted here
//! (`EEUserInterface::Post*`, each the original's `mThread->Post(id, p1, p2, true)`).

#![allow(non_snake_case)]

use core::ffi::c_void;
use std::os::raw::{c_long, c_ulong};
use std::sync::atomic::{AtomicPtr, AtomicU32, Ordering};

/// `EEUIManager`'s own members (ee/EEUIManager.h), in the class's order (the original's +0x12c..+0x13d).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct EeScreensUiManagerFields {
    /// `EEUIFMainGame*`: `GetFormByID(11)` in the constructor
    pub mMainGameForm: *mut c_void,
    /// `EEUIFScenarioEditor*`: made by `CreateScenarioEditor`, deleted by `DeleteScenarioEditor`
    pub mScenarioEditorForm: *mut c_void,
    pub mU134: c_ulong,
    pub mU138: c_ulong,
    /// set by `StartScenarioEditor`, cleared by the main menu's events
    pub mInScenarioEditor: bool,
    pub mB13D: bool,
}

impl Default for EeScreensUiManagerFields {
    fn default() -> Self {
        Self {
            mMainGameForm: core::ptr::null_mut(),
            mScenarioEditorForm: core::ptr::null_mut(),
            mU134: 0,
            mU138: 0,
            mInScenarioEditor: false,
            mB13D: false,
        }
    }
}

/// `EEUserInterface`'s members (ee/EEUserInterface.h), in the class's order (the original's +0x40..+0x5f).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct EeScreensUserInterfaceFields {
    /// `TSThread*`: the constructing thread; every post goes to it
    pub mThread: *mut c_void,
    /// `TSEvent*`: set by the thread message 0x?? (`EE_SCREENS_UI_OP_QUEUE_DONE`)
    pub mMessageQueueDone: *mut c_void,
    /// `TMGetTime()` at construction; the game's start and saved games set it
    pub mRandomSeed: c_ulong,
    /// the load percentage's player (`EEMLoadPercent`)
    pub mLocalPlayer: c_long,
    pub mB05C: bool,
    /// a byte: the message handler truncates a long into it
    pub mIsUpdating: u8,
    /// `PostMessage424Once`'s guard (the handler clears it)
    pub mIconBarUpdatePosted: bool,
    /// set by the handler; the loop exits on it
    pub mIsQuitting: bool,
}

impl Default for EeScreensUserInterfaceFields {
    fn default() -> Self {
        Self {
            mThread: core::ptr::null_mut(),
            mMessageQueueDone: core::ptr::null_mut(),
            mRandomSeed: 0,
            mLocalPlayer: 0,
            mB05C: false,
            mIsUpdating: 0,
            mIconBarUpdatePosted: false,
            mIsQuitting: false,
        }
    }
}

static MANAGER: AtomicPtr<EeScreensUiManagerFields> = AtomicPtr::new(core::ptr::null_mut());
#[cfg_attr(test, allow(dead_code))]
static UI: AtomicPtr<EeScreensUserInterfaceFields> = AtomicPtr::new(core::ptr::null_mut());

/// `gUserInterface`'s record registered (its constructor) and forgotten (its destructor).
#[cfg_attr(test, allow(dead_code))]
pub(crate) fn register_ui(rec: *mut EeScreensUserInterfaceFields) {
    UI.store(rec, Ordering::Release);
}

#[cfg_attr(test, allow(dead_code))]
pub(crate) fn forget_ui(rec: *mut EeScreensUserInterfaceFields) {
    let _ = UI.compare_exchange(rec, core::ptr::null_mut(), Ordering::AcqRel, Ordering::Acquire);
}

/// The live manager's record (None before the manager is made and after it is destroyed).
pub(crate) fn manager() -> Option<*mut EeScreensUiManagerFields> {
    let p = MANAGER.load(Ordering::Acquire);
    (!p.is_null()).then_some(p)
}

/// The live UI object's record (`gUserInterface`).
#[cfg_attr(test, allow(dead_code))]
pub(crate) fn ui() -> Option<*mut EeScreensUserInterfaceFields> {
    let p = UI.load(Ordering::Acquire);
    (!p.is_null()).then_some(p)
}

/// `gUIManager->mMainGameForm` (NULL without a manager).
pub(crate) fn main_game_form() -> *mut c_void {
    // SAFETY: the registered live manager's record (a plain read)
    manager().map_or(core::ptr::null_mut(), |m| unsafe { (*m).mMainGameForm })
}

/// `gUIManager->mScenarioEditorForm` (NULL without a manager).
#[cfg_attr(test, allow(dead_code))]
pub(crate) fn editor_form() -> *mut c_void {
    // SAFETY: as above
    manager().map_or(core::ptr::null_mut(), |m| unsafe { (*m).mScenarioEditorForm })
}

/// `gUIManager->mInScenarioEditor` (false without a manager).
#[cfg_attr(test, allow(dead_code))]
pub(crate) fn in_scenario_editor() -> bool {
    // SAFETY: as above
    manager().is_some_and(|m| unsafe { (*m).mInScenarioEditor })
}

/// `EEUIManager`'s constructor: its members zeroed (the constructor's own stores come in `EE_SCREENS_UM_OP_FORMS`)
/// and the record registered.
///
/// # Safety
/// `rec` is the new manager's record; it lives until `ee_screens_ui_manager_fields_drop`.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_ui_manager_fields(rec: *mut EeScreensUiManagerFields) {
    *rec = EeScreensUiManagerFields::default();
    MANAGER.store(rec, Ordering::Release);
}

/// `EEUIManager`'s destructor: the record forgotten (the main game form's pointer cleared, as the original's).
///
/// # Safety
/// `rec` is the manager's live record.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_ui_manager_fields_drop(rec: *mut EeScreensUiManagerFields) {
    (*rec).mMainGameForm = core::ptr::null_mut();
    let _ = MANAGER.compare_exchange(rec, core::ptr::null_mut(), Ordering::AcqRel, Ordering::Acquire);
}

/// The constructor's stores (`EE_SCREENS_UM_OP_FORMS`): the main game form, the editor flags.
pub(crate) fn set_forms(rec: &mut EeScreensUiManagerFields, main_game: *mut c_void) {
    rec.mMainGameForm = main_game;
    rec.mInScenarioEditor = false;
    rec.mScenarioEditorForm = core::ptr::null_mut();
    rec.mB13D = false;
}

/// The thread messages' ids the posts use (the UI thread's, `ui_thread.rs`' switch).
pub const EE_SCREENS_UI_POST_START_LOADING: u32 = 0x407;
pub const EE_SCREENS_UI_POST_START_LOADING_EDITOR: u32 = 0x413;
pub const EE_SCREENS_UI_POST_ICON_BAR: u32 = 0x424;
pub const EE_SCREENS_UI_POST_LOAD_PERCENT: u32 = 0x40a;

/// The constructor's stores (0x276631): the thread, the flags, the seed; the event is the caller's
/// (`TSScheduler::CreateEvent`).
pub(crate) fn construct(rec: &mut EeScreensUserInterfaceFields, thread: *mut c_void, time: u32) {
    *rec = EeScreensUserInterfaceFields {
        mThread: thread,
        mIsUpdating: 1,
        mRandomSeed: time as c_ulong,
        ..Default::default()
    };
}

/// `PostStartLoading`'s message: 0x407, 0x413 from the scenario editor.
pub(crate) fn start_loading_message(in_editor: bool) -> u32 {
    if in_editor {
        EE_SCREENS_UI_POST_START_LOADING_EDITOR
    } else {
        EE_SCREENS_UI_POST_START_LOADING
    }
}

/// `PostMessage424Once`: whether to post (the guard set when it does).
pub(crate) fn icon_bar_once(rec: &mut EeScreensUserInterfaceFields) -> bool {
    if rec.mIconBarUpdatePosted {
        return false;
    }
    rec.mIconBarUpdatePosted = true;
    true
}

/// `SetLoadProgress`'s last percentage (0x565370).
static LAST_LOAD_PERCENT: AtomicU32 = AtomicU32::new(0);

/// `SetLoadProgress(fraction)`: the percentage (the fraction clamped to [0, 1], times 100, truncated) and whether
/// the others are told (a new 20 percent step), the last percentage updated.
pub(crate) fn load_progress(fraction: f32) -> (u32, bool) {
    let percent = (fraction.clamp(0.0, 1.0) * 100.0) as u32;
    let last = LAST_LOAD_PERCENT.swap(percent, Ordering::Relaxed);
    (percent, last / 20 < percent / 20)
}

#[cfg(test)]
pub(crate) fn reset_load_progress() {
    LAST_LOAD_PERCENT.store(0, Ordering::Relaxed);
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn records_keep_the_classes_layouts() {
        let p = core::mem::size_of::<*mut c_void>();
        let l = core::mem::size_of::<c_long>();
        assert_eq!(core::mem::offset_of!(EeScreensUiManagerFields, mScenarioEditorForm), p);
        assert_eq!(core::mem::offset_of!(EeScreensUiManagerFields, mInScenarioEditor), 2 * p + 2 * l);
        assert_eq!(core::mem::offset_of!(EeScreensUiManagerFields, mB13D), 2 * p + 2 * l + 1);
        assert_eq!(core::mem::offset_of!(EeScreensUserInterfaceFields, mRandomSeed), 2 * p);
        assert_eq!(core::mem::offset_of!(EeScreensUserInterfaceFields, mIsQuitting), 2 * p + 2 * l + 3);
    }

    #[test]
    fn construct_sets_the_thread_flags_and_seed() {
        let mut r = EeScreensUserInterfaceFields { mIsQuitting: true, mLocalPlayer: 5, ..Default::default() };
        let t = 0x1000 as *mut c_void;
        construct(&mut r, t, 1234);
        assert_eq!(r.mThread, t);
        assert_eq!(r.mRandomSeed, 1234);
        assert_eq!(r.mIsUpdating, 1);
        assert!(!r.mIsQuitting && !r.mB05C && !r.mIconBarUpdatePosted);
        assert_eq!(r.mLocalPlayer, 0);
        assert!(r.mMessageQueueDone.is_null());
    }

    #[test]
    fn manager_forms_and_registry() {
        let mut m = EeScreensUiManagerFields { mInScenarioEditor: true, mB13D: true, mU138: 7, ..Default::default() };
        set_forms(&mut m, 0x20 as *mut c_void);
        assert_eq!(m.mMainGameForm, 0x20 as *mut c_void);
        assert!(!m.mInScenarioEditor && !m.mB13D && m.mScenarioEditorForm.is_null());
        assert_eq!(m.mU138, 7);
        assert!(main_game_form().is_null() || manager().is_some());
    }

    #[test]
    fn posts_decisions() {
        assert_eq!(start_loading_message(false), 0x407);
        assert_eq!(start_loading_message(true), 0x413);
        let mut r = EeScreensUserInterfaceFields::default();
        assert!(icon_bar_once(&mut r));
        assert!(!icon_bar_once(&mut r));
        r.mIconBarUpdatePosted = false;
        assert!(icon_bar_once(&mut r));
    }

    #[test]
    fn load_progress_clamps_and_steps() {
        reset_load_progress();
        assert_eq!(load_progress(-1.0), (0, false));
        assert_eq!(load_progress(0.19), (19, false));
        assert_eq!(load_progress(0.2), (20, true));
        assert_eq!(load_progress(0.39), (39, false));
        assert_eq!(load_progress(2.0), (100, true));
        assert_eq!(load_progress(0.5), (50, false));
        assert_eq!(load_progress(0.61), (61, true));
    }
}
