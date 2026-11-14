//! The UI thread's message switch and the game window's event handler (`EEUserInterface::HandleMessage`, the
//! 0x401..0x443 thread messages, and `EEUserInterface::HandleEvent`; `User Interface Thread/EEUserInterface.cpp`).
//!
//! Both are decisions over host calls: every call into the UI manager, the forms, the graphics thread, the world's
//! lock and the thread object is one host op ([`UiHost`], `EE_SCREENS_UI_OP_*` for the messages and
//! `EE_SCREENS_UI_EV_*` for the window events), asked in the original's order, as often as the original asked it.
//! Plain reads with no side effect (a global's NULL test, the current form, a form by id) are ops too, asked where
//! the original read them. The window events' state (the original's statics: minimised, the saved volumes, the
//! Control and AltGr keys, an open composition) is [`EventState`].

use core::sync::atomic::{AtomicBool, AtomicU32, Ordering::Relaxed};

/// The host: one op with up to three arguments (pointers and values as `i64`) and its answer.
pub trait UiHost {
    fn op(&mut self, op: u32, a: i64, b: i64, c: i64) -> i64;
}

// ---- the thread messages' ops (`ee_screenscpp_ui_message`)
/// `this->PauseGraphics()`.
pub const EE_SCREENS_UI_OP_PAUSE: u32 = 1;
/// `this->ResumeGraphics()`.
pub const EE_SCREENS_UI_OP_RESUME: u32 = 2;
/// A lock: a = 0 `gUIManager->LockUIForCopyData()`, 1 the world's read lock, 2 its write lock.
pub const EE_SCREENS_UI_OP_LOCK: u32 = 3;
/// The matching unlock (a as for `LOCK`).
pub const EE_SCREENS_UI_OP_UNLOCK: u32 = 4;
/// `gUIManager != NULL`.
pub const EE_SCREENS_UI_OP_HAS_MANAGER: u32 = 5;
/// `gUIManager->mMainGameForm` (a pointer, 0 when none).
pub const EE_SCREENS_UI_OP_MAIN_FORM: u32 = 6;
/// `gUIManager->mScenarioEditorForm` (a pointer, 0 when none).
pub const EE_SCREENS_UI_OP_EDITOR_FORM: u32 = 7;
/// `gUIManager->GetCurrentForm()`.
pub const EE_SCREENS_UI_OP_CURRENT_FORM: u32 = 8;
/// `gUIManager->GetFormByID(a)`.
pub const EE_SCREENS_UI_OP_FORM_BY_ID: u32 = 9;
/// `gUIManager->SwapForm(a)`.
pub const EE_SCREENS_UI_OP_SWAP: u32 = 10;
/// `gUIManager->Push(a)`.
pub const EE_SCREENS_UI_OP_PUSH: u32 = 11;
/// `gUIManager->Pop()`.
pub const EE_SCREENS_UI_OP_POP: u32 = 12;
/// `gUIManager->PopTo(a)`.
pub const EE_SCREENS_UI_OP_POP_TO: u32 = 13;
/// `gUIManager->IsFormOnStack(a)`.
pub const EE_SCREENS_UI_OP_ON_STACK: u32 = 14;
/// `gUIManager->IsFormVisible(a)`.
pub const EE_SCREENS_UI_OP_VISIBLE: u32 = 15;
/// `gUIManager->RedrawAll()`.
pub const EE_SCREENS_UI_OP_REDRAW: u32 = 16;
/// Form a's control b (a `UIStatusBar`): `SetCurrentRangeValue(c)`.
pub const EE_SCREENS_UI_OP_STATUS_VALUE: u32 = 17;
/// `this->ResizeGameScreen(gGameWindowWidth, gGameWindowHeight, true, true)`.
pub const EE_SCREENS_UI_OP_RESIZE_GAME: u32 = 18;
/// `this->ResizeGameScreen(EEMenuWidth(), EEMenuHeight(), true, false)`.
pub const EE_SCREENS_UI_OP_RESIZE_MENU: u32 = 19;
/// `gGameScreenActive = true` (under the write lock).
pub const EE_SCREENS_UI_OP_SCREEN_ACTIVE: u32 = 20;
/// `gDataPreLoading->PostLoadGame(false, false)`.
pub const EE_SCREENS_UI_OP_POST_LOAD_GAME: u32 = 21;
/// `this->ShowEpochName()`.
pub const EE_SCREENS_UI_OP_SHOW_EPOCH_NAME: u32 = 22;
/// The main game form's `UpdateBackgroundMedia(true)`.
pub const EE_SCREENS_UI_OP_BACKGROUND_MEDIA: u32 = 23;
/// `this->mIsUpdating = a`.
pub const EE_SCREENS_UI_OP_SET_UPDATING: u32 = 24;
/// `this->mIsQuitting = true`.
pub const EE_SCREENS_UI_OP_SET_QUITTING: u32 = 25;
/// `this->LoadSavedGame(a)`.
pub const EE_SCREENS_UI_OP_LOAD_SAVED_GAME: u32 = 26;
/// The posted event a: `ProcessEvent(NULL, b)`, then its `Dereference`.
pub const EE_SCREENS_UI_OP_PROCESS_EVENT: u32 = 27;
/// `gGraphics->PostPause() == kErrorOK`.
pub const EE_SCREENS_UI_OP_GFX_POST_PAUSE: u32 = 28;
/// `gGraphics->mIsPaused`.
pub const EE_SCREENS_UI_OP_GFX_PAUSED: u32 = 29;
/// `PlatSleep(1)`.
pub const EE_SCREENS_UI_OP_SLEEP: u32 = 30;
/// `gGraphics->PostResume()`.
pub const EE_SCREENS_UI_OP_GFX_POST_RESUME: u32 = 31;
/// The quick load action a: `mIsLoad = true`, `mGameState = &gGameState`, `gUserInterface->PostAction`, its
/// `Dereference`.
pub const EE_SCREENS_UI_OP_QUICK_LOAD: u32 = 32;
/// `this->ShowLoadingText(a)`.
pub const EE_SCREENS_UI_OP_LOADING_TEXT: u32 = 33;
/// `this->SaveAndTestScenario()`.
pub const EE_SCREENS_UI_OP_SAVE_AND_TEST: u32 = 34;
/// `EEResetGraphicsValues()`.
pub const EE_SCREENS_UI_OP_RESET_GFX_VALUES: u32 = 35;
/// The screen capture a: its event made, `gGraphics->PostScreenShot`, the wait, the event destroyed, its surface
/// released when it has a width, the capture deleted.
pub const EE_SCREENS_UI_OP_SCREEN_CAPTURE: u32 = 36;
/// `EEPostGraphicsAll()`.
pub const EE_SCREENS_UI_OP_POST_GFX_ALL: u32 = 37;
/// `this->CloseScenarioEditor()`.
pub const EE_SCREENS_UI_OP_CLOSE_EDITOR: u32 = 38;
/// `this->EnterScenarioEditor(a != 0)`.
pub const EE_SCREENS_UI_OP_ENTER_EDITOR: u32 = 39;
/// `this->CancelGame()`.
pub const EE_SCREENS_UI_OP_CANCEL_GAME: u32 = 40;
/// `this->StartLoadingFromEditor()`.
pub const EE_SCREENS_UI_OP_LOAD_FROM_EDITOR: u32 = 41;
/// `this->TestScenario()`.
pub const EE_SCREENS_UI_OP_TEST_SCENARIO: u32 = 42;
/// The main game form's `mRefreshGroup = true`.
pub const EE_SCREENS_UI_OP_REFRESH_GROUP: u32 = 43;
/// The error dialog's callback data a: whether its `mCallback` is set.
pub const EE_SCREENS_UI_OP_ERROR_HAS_CALLBACK: u32 = 44;
/// The error dialog's callback data a: `mCallback(mData)`.
pub const EE_SCREENS_UI_OP_ERROR_CALLBACK: u32 = 45;
/// `delete` the form a.
pub const EE_SCREENS_UI_OP_DELETE_FORM: u32 = 46;
/// `delete` the error dialog's callback data a.
pub const EE_SCREENS_UI_OP_DELETE_ERROR_DATA: u32 = 47;
/// The chat log entry a shown on the main game form (`DisplayChat`).
pub const EE_SCREENS_UI_OP_CHAT_SHOW: u32 = 48;
/// The chat log entry a's `mType`.
pub const EE_SCREENS_UI_OP_CHAT_TYPE: u32 = 49;
/// `EEUIECLEFormManager::AddEntry` of the chat log entry a.
pub const EE_SCREENS_UI_OP_CHAT_LOG: u32 = 50;
/// `delete` the chat log entry a.
pub const EE_SCREENS_UI_OP_DELETE_CHAT: u32 = 51;
/// `gGraphics->mIsMoviePlaying`.
pub const EE_SCREENS_UI_OP_MOVIE_PLAYING: u32 = 52;
/// `gGraphics->PlayMovie(*a, b) == kErrorOK`.
pub const EE_SCREENS_UI_OP_PLAY_MOVIE: u32 = 53;
/// `gMouse->mPointer = a`.
pub const EE_SCREENS_UI_OP_SET_POINTER: u32 = 54;
/// `gGraphics->PostStopMovie()`.
pub const EE_SCREENS_UI_OP_STOP_MOVIE: u32 = 55;
/// `gUserInterface->PostPlayMovie(*a, b)`.
pub const EE_SCREENS_UI_OP_REPOST_MOVIE: u32 = 56;
/// The reference counter a's `Dereference()`.
pub const EE_SCREENS_UI_OP_DEREFERENCE: u32 = 57;
/// `delete` the file spec a.
pub const EE_SCREENS_UI_OP_DELETE_SPEC: u32 = 58;
/// The main game form's `ShowInformation` of the dialog message a.
pub const EE_SCREENS_UI_OP_SHOW_INFORMATION: u32 = 59;
/// The dialog message a's text into the chat log (a new entry of type 1, `AddEntry`, deleted).
pub const EE_SCREENS_UI_OP_DIALOG_LOG: u32 = 60;
/// `delete` the dialog message a.
pub const EE_SCREENS_UI_OP_DELETE_DIALOG: u32 = 61;
/// The host has gone: `EESetQuitFlag`, the boot action posted, the client disconnected, deleted and made anew.
pub const EE_SCREENS_UI_OP_HOST_GONE: u32 = 62;
/// The main game form's `UpdateIconBar()`, then `this->mIconBarUpdatePosted = false`.
pub const EE_SCREENS_UI_OP_UPDATE_ICON_BAR: u32 = 63;
/// `this->ChangeRasterizer(a, b)`.
pub const EE_SCREENS_UI_OP_CHANGE_RASTERIZER: u32 = 64;
/// The main game form's `PauseInformation()` (a != 0) or `ResumeInformation()`.
pub const EE_SCREENS_UI_OP_INFORMATION: u32 = 65;
/// `this->ShowCivilizationBuilder(a, the low byte of b as a bool, false)` (message 0x420), or with c != 0
/// `ShowCivilizationBuilder(0, false, true)` (0x41f).
pub const EE_SCREENS_UI_OP_CIV_BUILDER: u32 = 66;
/// `gUserInterface->PostEvent(a, NULL)`.
pub const EE_SCREENS_UI_OP_POST_EVENT: u32 = 67;
/// The main game form's `HideMessages()`.
pub const EE_SCREENS_UI_OP_HIDE_MESSAGES: u32 = 68;
/// The scenario editor form's `SelectTab((unsigned char)a, b)`.
pub const EE_SCREENS_UI_OP_SELECT_TAB: u32 = 69;
/// `this->RestartGame()`.
pub const EE_SCREENS_UI_OP_RESTART: u32 = 70;
/// The main game form's control a (an `EEUIBSavedGroup`): `Refresh()`.
pub const EE_SCREENS_UI_OP_SAVED_GROUP: u32 = 71;
/// `this->OpenSelectServer()`.
pub const EE_SCREENS_UI_OP_SELECT_SERVER: u32 = 72;
/// `this->ShowMessageOfTheDay(*a, *b)`.
pub const EE_SCREENS_UI_OP_MOTD: u32 = 73;
/// `delete` the wide string a.
pub const EE_SCREENS_UI_OP_DELETE_WSTRING: u32 = 74;
/// The lobby's updates: a = 0 `ee_screens_lobby_update_player(b, c)`, 1 `..._update_game_options()`,
/// 2 `..._show_transfer_progress(b, c)`, 3 `..._update_form()`.
pub const EE_SCREENS_UI_OP_LOBBY: u32 = 75;
/// `this->ShowLagStatus(a, b != 0)`.
pub const EE_SCREENS_UI_OP_LAG: u32 = 76;
/// `this->mMessageQueueDone->Set()`.
pub const EE_SCREENS_UI_OP_QUEUE_DONE: u32 = 77;

const SCREENS_UI_LOCK_UI: i64 = 0;
const SCREENS_UI_LOCK_READ: i64 = 1;
const SCREENS_UI_LOCK_WRITE: i64 = 2;

// the forms by id (`ee/EEUIManager.h`)
const SCREENS_UI_FORM_SINGLE_PLAYER_LOAD: i64 = 13;
const SCREENS_UI_FORM_SCENARIO_INTRO: i64 = 19;
const SCREENS_UI_FORM_SPLASH: i64 = 23;
const SCREENS_UI_FORM_LOBBY: i64 = 26;
/// The splash form's load status bar (`kSplashLoadStatus`).
const SCREENS_UI_SPLASH_LOAD_STATUS: i64 = 2;

struct Ops<'a, H: UiHost>(&'a mut H);

impl<H: UiHost> Ops<'_, H> {
    fn o(&mut self, op: u32) -> i64 {
        self.0.op(op, 0, 0, 0)
    }
    fn a(&mut self, op: u32, a: i64) -> i64 {
        self.0.op(op, a, 0, 0)
    }
    fn ab(&mut self, op: u32, a: i64, b: i64) -> i64 {
        self.0.op(op, a, b, 0)
    }
    fn lock(&mut self, kind: i64) {
        self.a(EE_SCREENS_UI_OP_LOCK, kind);
    }
    fn unlock(&mut self, kind: i64) {
        self.a(EE_SCREENS_UI_OP_UNLOCK, kind);
    }
    /// A global is set (`EE_SCREENS_UI_GLOBAL_*`).
    fn has(&mut self, global: i64) -> bool {
        self.a(EE_SCREENS_UI_EV_HAS, global) != 0
    }
    fn form(&mut self, id: i64) -> i64 {
        self.a(EE_SCREENS_UI_OP_FORM_BY_ID, id)
    }
    /// `gUIManager != NULL && gUIManager->mMainGameForm != NULL`.
    fn has_main(&mut self) -> bool {
        self.o(EE_SCREENS_UI_OP_HAS_MANAGER) != 0 && self.o(EE_SCREENS_UI_OP_MAIN_FORM) != 0
    }
    /// The graphics thread paused: false when `PostPause` fails, else after waiting for its flag.
    fn pause_graphics_thread(&mut self) -> bool {
        if self.o(EE_SCREENS_UI_OP_GFX_POST_PAUSE) == 0 {
            return false;
        }
        while self.o(EE_SCREENS_UI_OP_GFX_PAUSED) == 0 {
            self.o(EE_SCREENS_UI_OP_SLEEP);
        }
        true
    }
    /// Paused graphics, the UI's copy lock and the world's read lock around `f`.
    fn paused_ui_read(&mut self, f: impl FnOnce(&mut Self)) {
        self.o(EE_SCREENS_UI_OP_PAUSE);
        self.lock(SCREENS_UI_LOCK_UI);
        self.lock(SCREENS_UI_LOCK_READ);
        f(self);
        self.unlock(SCREENS_UI_LOCK_READ);
        self.unlock(SCREENS_UI_LOCK_UI);
        self.o(EE_SCREENS_UI_OP_RESUME);
    }
    /// The splash form's status bar `control` set to `value` when the splash form is visible (0x443).
    fn splash_status(&mut self, control: i64, value: i64) {
        self.lock(SCREENS_UI_LOCK_UI);
        let splash = self.form(SCREENS_UI_FORM_SPLASH);
        if self.a(EE_SCREENS_UI_OP_VISIBLE, splash) != 0 {
            let splash = self.form(SCREENS_UI_FORM_SPLASH);
            self.0.op(EE_SCREENS_UI_OP_STATUS_VALUE, splash, control, value);
        }
        self.unlock(SCREENS_UI_LOCK_UI);
    }
}

/// `EEUserInterface::HandleMessage(message, p1, p2)`: one UI thread message.
pub fn handle_message(host: &mut impl UiHost, message: u32, p1: i64, p2: i64) {
    let mut h = Ops(host);
    match message {
        0x407 => {
            h.o(EE_SCREENS_UI_OP_PAUSE);
            h.o(EE_SCREENS_UI_OP_RESIZE_GAME);
            let current = h.o(EE_SCREENS_UI_OP_CURRENT_FORM);
            let swap = current == h.form(SCREENS_UI_FORM_LOBBY)
                || current == h.form(SCREENS_UI_FORM_SINGLE_PLAYER_LOAD)
                || current == h.form(SCREENS_UI_FORM_SCENARIO_INTRO);
            let splash = h.form(SCREENS_UI_FORM_SPLASH);
            h.a(if swap { EE_SCREENS_UI_OP_SWAP } else { EE_SCREENS_UI_OP_PUSH }, splash);
            h.lock(SCREENS_UI_LOCK_WRITE);
            h.o(EE_SCREENS_UI_OP_SCREEN_ACTIVE);
            h.unlock(SCREENS_UI_LOCK_WRITE);
            h.o(EE_SCREENS_UI_OP_POST_LOAD_GAME);
            // gUIManager->FnE123D(): an empty body
            h.o(EE_SCREENS_UI_OP_RESUME);
        }
        0x404 => {
            h.o(EE_SCREENS_UI_OP_SHOW_EPOCH_NAME);
        }
        0x403 => {
            h.lock(SCREENS_UI_LOCK_UI);
            h.lock(SCREENS_UI_LOCK_READ);
            h.o(EE_SCREENS_UI_OP_BACKGROUND_MEDIA);
            h.unlock(SCREENS_UI_LOCK_READ);
            h.unlock(SCREENS_UI_LOCK_UI);
        }
        0x402 => {
            h.a(EE_SCREENS_UI_OP_SET_UPDATING, p1);
        }
        // WM_QUIT, kThreadMessageQuit
        0x12 | 0x401 => {
            h.o(EE_SCREENS_UI_OP_SET_QUITTING);
        }
        0x408 => {
            h.a(EE_SCREENS_UI_OP_LOAD_SAVED_GAME, p1);
        }
        0x40e => {
            h.o(EE_SCREENS_UI_OP_PAUSE);
            h.lock(SCREENS_UI_LOCK_READ);
            h.ab(EE_SCREENS_UI_OP_PROCESS_EVENT, p1, p2);
            h.unlock(SCREENS_UI_LOCK_READ);
            h.o(EE_SCREENS_UI_OP_RESUME);
        }
        0x40d => h.paused_ui_read(|h| {
            h.a(EE_SCREENS_UI_OP_SWAP, p1);
        }),
        0x40c => {
            if h.pause_graphics_thread() {
                h.a(EE_SCREENS_UI_OP_QUICK_LOAD, p1);
            }
        }
        0x40b => {
            h.a(EE_SCREENS_UI_OP_LOADING_TEXT, p1);
        }
        0x40a => {
            h.lock(SCREENS_UI_LOCK_UI);
            if h.o(EE_SCREENS_UI_OP_HAS_MANAGER) != 0 {
                let current = h.o(EE_SCREENS_UI_OP_CURRENT_FORM);
                if current == h.form(SCREENS_UI_FORM_SPLASH) {
                    let splash = h.form(SCREENS_UI_FORM_SPLASH);
                    h.0.op(EE_SCREENS_UI_OP_STATUS_VALUE, splash, SCREENS_UI_SPLASH_LOAD_STATUS, p1);
                }
            }
            h.unlock(SCREENS_UI_LOCK_UI);
        }
        0x40f => {
            h.o(EE_SCREENS_UI_OP_SAVE_AND_TEST);
        }
        0x41b => {
            let keep_cursor = p2 > 0;
            if !keep_cursor {
                h.o(EE_SCREENS_UI_OP_RESET_GFX_VALUES);
            }
            h.a(EE_SCREENS_UI_OP_SCREEN_CAPTURE, p1);
            if !keep_cursor {
                h.o(EE_SCREENS_UI_OP_POST_GFX_ALL);
            }
            h.o(EE_SCREENS_UI_OP_REDRAW);
        }
        0x410 => {
            h.o(EE_SCREENS_UI_OP_CLOSE_EDITOR);
        }
        0x411 => {
            h.a(EE_SCREENS_UI_OP_ENTER_EDITOR, (p1 > 0) as i64);
        }
        0x412 => {
            h.o(EE_SCREENS_UI_OP_CANCEL_GAME);
        }
        0x413 => {
            h.o(EE_SCREENS_UI_OP_LOAD_FROM_EDITOR);
        }
        0x415 => {
            h.o(EE_SCREENS_UI_OP_TEST_SCENARIO);
        }
        0x416 => {
            if h.o(EE_SCREENS_UI_OP_MAIN_FORM) != 0 {
                h.o(EE_SCREENS_UI_OP_REFRESH_GROUP);
            }
        }
        0x419 => h.paused_ui_read(|h| {
            if p1 == 0 {
                h.o(EE_SCREENS_UI_OP_POP);
            } else {
                h.a(EE_SCREENS_UI_OP_POP_TO, p1);
            }
        }),
        0x414 => h.paused_ui_read(|h| {
            h.a(EE_SCREENS_UI_OP_PUSH, p1);
        }),
        0x417 => {
            h.o(EE_SCREENS_UI_OP_PAUSE);
            h.lock(SCREENS_UI_LOCK_UI);
            if h.a(EE_SCREENS_UI_OP_ON_STACK, p1) == 0 {
                if h.a(EE_SCREENS_UI_OP_ERROR_HAS_CALLBACK, p2) != 0 {
                    h.lock(SCREENS_UI_LOCK_READ);
                    h.a(EE_SCREENS_UI_OP_ERROR_CALLBACK, p2);
                    h.unlock(SCREENS_UI_LOCK_READ);
                }
                if p1 != 0 {
                    h.a(EE_SCREENS_UI_OP_DELETE_FORM, p1);
                }
                h.a(EE_SCREENS_UI_OP_DELETE_ERROR_DATA, p2);
            }
            h.unlock(SCREENS_UI_LOCK_UI);
            h.o(EE_SCREENS_UI_OP_RESUME);
        }
        0x418 => {
            if !h.has_main() {
                return;
            }
            h.lock(SCREENS_UI_LOCK_UI);
            h.a(EE_SCREENS_UI_OP_CHAT_SHOW, p1);
            h.unlock(SCREENS_UI_LOCK_UI);
            if h.a(EE_SCREENS_UI_OP_CHAT_TYPE, p1) != 2 {
                h.a(EE_SCREENS_UI_OP_CHAT_LOG, p1);
            }
            if p1 != 0 {
                h.a(EE_SCREENS_UI_OP_DELETE_CHAT, p1);
            }
        }
        0x41c => {
            if h.o(EE_SCREENS_UI_OP_MOVIE_PLAYING) == 0 {
                if h.ab(EE_SCREENS_UI_OP_PLAY_MOVIE, p1, p2) != 0 {
                    h.a(EE_SCREENS_UI_OP_SET_POINTER, 6);
                }
                while h.o(EE_SCREENS_UI_OP_MOVIE_PLAYING) == 0 {
                    h.o(EE_SCREENS_UI_OP_SLEEP);
                }
            } else {
                h.o(EE_SCREENS_UI_OP_STOP_MOVIE);
                while h.o(EE_SCREENS_UI_OP_MOVIE_PLAYING) != 0 {}
                h.o(EE_SCREENS_UI_OP_SLEEP);
                h.ab(EE_SCREENS_UI_OP_REPOST_MOVIE, p1, p2);
            }
            if p2 != 0 {
                h.a(EE_SCREENS_UI_OP_DEREFERENCE, p2);
            }
            if p1 != 0 {
                h.a(EE_SCREENS_UI_OP_DELETE_SPEC, p1);
            }
        }
        0x422 => {
            if h.has_main() {
                h.lock(SCREENS_UI_LOCK_UI);
                h.a(EE_SCREENS_UI_OP_SHOW_INFORMATION, p1);
                h.unlock(SCREENS_UI_LOCK_UI);
            }
            h.a(EE_SCREENS_UI_OP_DIALOG_LOG, p1);
            if p1 != 0 {
                h.a(EE_SCREENS_UI_OP_DELETE_DIALOG, p1);
            }
        }
        0x429 => {
            h.o(EE_SCREENS_UI_OP_PAUSE);
            h.o(EE_SCREENS_UI_OP_RESIZE_GAME);
            h.lock(SCREENS_UI_LOCK_READ);
            h.o(EE_SCREENS_UI_OP_POP);
            h.unlock(SCREENS_UI_LOCK_READ);
            h.o(EE_SCREENS_UI_OP_RESUME);
        }
        0x430 => {
            h.o(EE_SCREENS_UI_OP_PAUSE);
            h.o(EE_SCREENS_UI_OP_RESIZE_MENU);
            if p2 <= 0 {
                h.o(EE_SCREENS_UI_OP_POP);
            } else {
                h.o(EE_SCREENS_UI_OP_HOST_GONE);
            }
            h.o(EE_SCREENS_UI_OP_RESUME);
        }
        0x424 => {
            if !h.has_main() {
                return;
            }
            h.lock(SCREENS_UI_LOCK_UI);
            h.lock(SCREENS_UI_LOCK_READ);
            h.o(EE_SCREENS_UI_OP_UPDATE_ICON_BAR);
            h.unlock(SCREENS_UI_LOCK_READ);
            h.unlock(SCREENS_UI_LOCK_UI);
        }
        0x41e => {
            if !h.pause_graphics_thread() {
                return;
            }
            h.ab(EE_SCREENS_UI_OP_CHANGE_RASTERIZER, p1, p2);
            h.o(EE_SCREENS_UI_OP_GFX_POST_RESUME);
            while h.o(EE_SCREENS_UI_OP_GFX_PAUSED) != 0 {
                h.o(EE_SCREENS_UI_OP_SLEEP);
            }
            h.o(EE_SCREENS_UI_OP_REDRAW);
        }
        0x427 => {
            if !h.has_main() {
                return;
            }
            h.lock(SCREENS_UI_LOCK_UI);
            // `(unsigned long)inParameter1 > 0`: the low 32 bits
            h.a(EE_SCREENS_UI_OP_INFORMATION, (p1 as u32 != 0) as i64);
            h.unlock(SCREENS_UI_LOCK_UI);
        }
        0x420 => {
            h.0.op(EE_SCREENS_UI_OP_CIV_BUILDER, p1, p2, 0);
        }
        0x41d => {
            h.o(EE_SCREENS_UI_OP_REDRAW);
            h.a(EE_SCREENS_UI_OP_SET_POINTER, 0);
            if p1 != 0 {
                h.a(EE_SCREENS_UI_OP_POST_EVENT, p1);
                h.a(EE_SCREENS_UI_OP_DEREFERENCE, p1);
            }
        }
        0x432 => {
            if h.o(EE_SCREENS_UI_OP_MAIN_FORM) != 0 {
                h.o(EE_SCREENS_UI_OP_HIDE_MESSAGES);
            }
        }
        0x421 => {
            if h.o(EE_SCREENS_UI_OP_EDITOR_FORM) == 0 {
                return;
            }
            h.o(EE_SCREENS_UI_OP_PAUSE);
            h.lock(SCREENS_UI_LOCK_WRITE);
            h.ab(EE_SCREENS_UI_OP_SELECT_TAB, p1, p2);
            h.unlock(SCREENS_UI_LOCK_WRITE);
            h.o(EE_SCREENS_UI_OP_RESUME);
        }
        0x426 => {
            // the main game form's FnE123D (an empty body) when it is the current form
            h.lock(SCREENS_UI_LOCK_UI);
            h.unlock(SCREENS_UI_LOCK_UI);
        }
        0x428 => {
            h.o(EE_SCREENS_UI_OP_RESTART);
        }
        0x435 => {
            if !h.has_main() {
                return;
            }
            h.lock(SCREENS_UI_LOCK_UI);
            h.lock(SCREENS_UI_LOCK_READ);
            let main = h.o(EE_SCREENS_UI_OP_MAIN_FORM);
            if h.a(EE_SCREENS_UI_OP_VISIBLE, main) != 0 {
                h.a(EE_SCREENS_UI_OP_SAVED_GROUP, p1.wrapping_add(0xd3));
            }
            h.unlock(SCREENS_UI_LOCK_READ);
            h.unlock(SCREENS_UI_LOCK_UI);
        }
        0x441 => {
            h.o(EE_SCREENS_UI_OP_SELECT_SERVER);
        }
        0x440 => {
            h.ab(EE_SCREENS_UI_OP_MOTD, p1, p2);
            if p1 != 0 {
                h.a(EE_SCREENS_UI_OP_DELETE_WSTRING, p1);
            }
            if p2 != 0 {
                h.a(EE_SCREENS_UI_OP_DELETE_WSTRING, p2);
            }
        }
        0x41f => {
            h.0.op(EE_SCREENS_UI_OP_CIV_BUILDER, 0, 0, 1);
        }
        0x436..=0x439 => {
            let (kind, a, b) = match message {
                0x436 => (0, p1, p2),
                0x437 => (1, 0, 0),
                0x439 => (2, p1 as u8 as i64, p2 as u8 as i64),
                _ => (3, 0, 0),
            };
            h.lock(SCREENS_UI_LOCK_UI);
            h.0.op(EE_SCREENS_UI_OP_LOBBY, kind, a, b);
            h.unlock(SCREENS_UI_LOCK_UI);
        }
        // 0x442 falls into 0x443, which falls into 0x431
        0x442 => {
            h.ab(EE_SCREENS_UI_OP_LAG, p1 as u16 as i64, (p2 as u32 != 0) as i64);
            h.splash_status(p1.wrapping_add(0x14), p2);
            h.o(EE_SCREENS_UI_OP_QUEUE_DONE);
        }
        0x443 => {
            h.splash_status(p1.wrapping_add(0x14), p2);
            h.o(EE_SCREENS_UI_OP_QUEUE_DONE);
        }
        0x431 => {
            h.o(EE_SCREENS_UI_OP_QUEUE_DONE);
        }
        _ => {}
    }
}

// ---- the window events' ops (`ee_screenscpp_ui_event`)
/// Whether a global is set: a = one of `EE_SCREENS_UI_GLOBAL_*`.
pub const EE_SCREENS_UI_EV_HAS: u32 = 1;
/// `EELobbyIsShown()`.
pub const EE_SCREENS_UI_EV_LOBBY_SHOWN: u32 = 2;
/// `EEIsLobbyOpen()`.
pub const EE_SCREENS_UI_EV_LOBBY_OPEN: u32 = 3;
/// `EELobbyShutdown()`.
pub const EE_SCREENS_UI_EV_LOBBY_SHUTDOWN: u32 = 4;
/// `gUserInterface->mThread->Post(0x12, NULL, NULL, false)` (WM_QUIT).
pub const EE_SCREENS_UI_EV_POST_QUIT: u32 = 5;
/// The conductor's volume (f32 bits; 0.0 without a conductor).
pub const EE_SCREENS_UI_EV_MUSIC_VOLUME: u32 = 6;
/// The conductor's `SetVolume` (a: f32 bits).
pub const EE_SCREENS_UI_EV_SET_MUSIC_VOLUME: u32 = 7;
/// The sound manager's master volume (f32 bits; 0.0 without a sound manager).
pub const EE_SCREENS_UI_EV_MASTER_VOLUME: u32 = 8;
/// The sound manager's `SetMasterVolume` (a: f32 bits).
pub const EE_SCREENS_UI_EV_SET_MASTER_VOLUME: u32 = 9;
/// `SPIMSetMuted(a != 0)`.
pub const EE_SCREENS_UI_EV_MUTE: u32 = 10;
/// `gISKeyboard->Resume()` (a != 0) or `Suspend()`.
pub const EE_SCREENS_UI_EV_KEYBOARD: u32 = 11;
/// `gISMouse->Resume()` (a != 0) or `Suspend()`.
pub const EE_SCREENS_UI_EV_MOUSE: u32 = 12;
/// `gMouse->SetPosition` at the game window's centre.
pub const EE_SCREENS_UI_EV_CENTER_MOUSE: u32 = 13;
/// `gMousePointer->mThread->Post(a, NULL, NULL, true)`.
pub const EE_SCREENS_UI_EV_POINTER_POST: u32 = 14;
/// `gMousePointer->mIsShown`.
pub const EE_SCREENS_UI_EV_POINTER_SHOWN: u32 = 15;
/// `PlatSleep(1)`.
pub const EE_SCREENS_UI_EV_SLEEP: u32 = 16;
/// The "Mute in Background" setting.
pub const EE_SCREENS_UI_EV_MUTE_IN_BACKGROUND: u32 = 17;
/// `gUserInterface->mIsQuitting`.
pub const EE_SCREENS_UI_EV_QUITTING: u32 = 18;
/// The full screen setting.
pub const EE_SCREENS_UI_EV_FULL_SCREEN: u32 = 19;
/// `PlatWindowMinimize` of the game window.
pub const EE_SCREENS_UI_EV_MINIMIZE: u32 = 20;
/// `gUserInterface->OpenIMECompositionString()`.
pub const EE_SCREENS_UI_EV_IME_OPEN: u32 = 21;
/// `gUserInterface->UpdateIMECompositionString(a, b, c)` (a: the text).
pub const EE_SCREENS_UI_EV_IME_UPDATE: u32 = 22;
/// `gUserInterface->AddIMEResultString(a)`.
pub const EE_SCREENS_UI_EV_IME_RESULT: u32 = 23;
/// `gUserInterface->CloseIMECompositionString()`.
pub const EE_SCREENS_UI_EV_IME_CLOSE: u32 = 24;
/// `gUIManager->UpdateTranslatedKey(a, 1)` (a: one character's bytes, zero-padded to 8).
pub const EE_SCREENS_UI_EV_TRANSLATED_KEY: u32 = 25;

/// `gUserInterface` (for `EE_SCREENS_UI_EV_HAS`).
pub const EE_SCREENS_UI_GLOBAL_USER_INTERFACE: i64 = 0;
/// `gUIManager`.
pub const EE_SCREENS_UI_GLOBAL_MANAGER: i64 = 1;
/// `gConductor`.
pub const EE_SCREENS_UI_GLOBAL_CONDUCTOR: i64 = 2;
/// `gSoundManager`.
pub const EE_SCREENS_UI_GLOBAL_SOUND: i64 = 3;
/// `gISKeyboard`.
pub const EE_SCREENS_UI_GLOBAL_KEYBOARD: i64 = 4;
/// `gISMouse`.
pub const EE_SCREENS_UI_GLOBAL_MOUSE: i64 = 5;
/// `gGameWindow != NULL && gMouse != NULL`.
pub const EE_SCREENS_UI_GLOBAL_WINDOW_AND_MOUSE: i64 = 6;
/// `gMousePointer`.
pub const EE_SCREENS_UI_GLOBAL_POINTER: i64 = 7;
/// `gGameWindow`.
pub const EE_SCREENS_UI_GLOBAL_WINDOW: i64 = 8;

// platform/PlatWindow.h's PlatInputEventType
const SCREENS_UI_EVENT_QUIT: u32 = 1;
const SCREENS_UI_EVENT_CLOSE: u32 = 2;
const SCREENS_UI_EVENT_RESIZED: u32 = 3;
const SCREENS_UI_EVENT_FOCUS_GAINED: u32 = 4;
const SCREENS_UI_EVENT_FOCUS_LOST: u32 = 5;
const SCREENS_UI_EVENT_MINIMIZED: u32 = 6;
const SCREENS_UI_EVENT_RESTORED: u32 = 7;
const SCREENS_UI_EVENT_KEY_DOWN: u32 = 9;
const SCREENS_UI_EVENT_KEY_UP: u32 = 10;
const SCREENS_UI_EVENT_TEXT: u32 = 11;
const SCREENS_UI_EVENT_TEXT_EDITING: u32 = 12;

/// The window events' state (the original's statics), each read and written where the original did: a host call
/// may come back here (no lock is held across one).
#[derive(Default)]
pub struct EventState {
    /// The volumes were read (the original's function statics' first-call initialisation).
    pub started: AtomicBool,
    /// An input method composition is open (`sComposing`).
    pub composing: AtomicBool,
    /// The master and music volumes saved while minimised (f32 bits).
    pub master_volume: AtomicU32,
    pub music_volume: AtomicU32,
    /// `sMinimized`, `sControlDown`, `sAltDown`.
    pub minimized: AtomicBool,
    pub control_down: AtomicBool,
    pub alt_down: AtomicBool,
}

impl EventState {
    pub const fn new() -> Self {
        EventState {
            started: AtomicBool::new(false),
            composing: AtomicBool::new(false),
            master_volume: AtomicU32::new(0),
            music_volume: AtomicU32::new(0),
            minimized: AtomicBool::new(false),
            control_down: AtomicBool::new(false),
            alt_down: AtomicBool::new(false),
        }
    }
}

/// One window event (`PlatInputEvent`'s type, its key's virtual key, its text's bytes up to the terminator, the
/// composition's cursor and length).
pub struct WindowEvent<'a> {
    pub kind: u32,
    pub virtual_key: u32,
    /// The text's address, handed back to the host's IME ops.
    pub text_address: i64,
    pub text: &'a [u8],
    pub cursor: i32,
    pub length: i32,
}

/// `PlatUtf8Decode`'s byte count of the character at the start of `text` (0 at its end).
pub type Utf8Length = fn(&[u8]) -> usize;

/// [`Utf8Length`] by `PlatUtf8Decode` itself: `text` must lie in a zero-terminated string (the event's).
pub fn plat_utf8_length(text: &[u8]) -> usize {
    let mut n = 0u32;
    // the decoder stops at the terminator after the slice
    unsafe { ee_plat_core::string::PlatUtf8Decode(text.as_ptr().cast(), &mut n) };
    n as usize
}

fn is_control_key(key: u32) -> bool {
    // VK_CONTROL, VK_LCONTROL, VK_RCONTROL
    key == 0x11 || key == 0xa2 || key == 0xa3
}

fn is_alt_key(key: u32) -> bool {
    // VK_MENU, VK_RMENU
    key == 0x12 || key == 0xa5
}

/// `EEUserInterface::HandleEvent`: one game window event.
pub fn handle_event(host: &mut impl UiHost, st: &EventState, ev: &WindowEvent, utf8_length: Utf8Length) {
    let mut h = Ops(host);
    if !st.started.load(Relaxed) {
        let master = h.o(EE_SCREENS_UI_EV_MASTER_VOLUME) as u32;
        st.master_volume.store(master, Relaxed);
        let music = h.o(EE_SCREENS_UI_EV_MUSIC_VOLUME) as u32;
        st.music_volume.store(music, Relaxed);
        st.started.store(true, Relaxed);
    }
    // while the lobby is shown it owns the event loop and passes these on (EELobbyForwardEvent)
    if h.o(EE_SCREENS_UI_EV_LOBBY_SHOWN) != 0
        && !matches!(
            ev.kind,
            SCREENS_UI_EVENT_MINIMIZED
                | SCREENS_UI_EVENT_RESTORED
                | SCREENS_UI_EVENT_RESIZED
                | SCREENS_UI_EVENT_QUIT
                | SCREENS_UI_EVENT_CLOSE
        )
    {
        return;
    }
    match ev.kind {
        SCREENS_UI_EVENT_QUIT | SCREENS_UI_EVENT_CLOSE => {
            // a shown lobby closes its own window (the event came through it); shut a hidden one down here
            if h.o(EE_SCREENS_UI_EV_LOBBY_OPEN) != 0 && h.o(EE_SCREENS_UI_EV_LOBBY_SHOWN) == 0 {
                h.o(EE_SCREENS_UI_EV_LOBBY_SHUTDOWN);
            }
            if h.has(EE_SCREENS_UI_GLOBAL_USER_INTERFACE) {
                h.o(EE_SCREENS_UI_EV_POST_QUIT);
            }
        }
        SCREENS_UI_EVENT_MINIMIZED => {
            if !st.minimized.load(Relaxed) {
                if h.has(EE_SCREENS_UI_GLOBAL_CONDUCTOR) {
                    let music = h.o(EE_SCREENS_UI_EV_MUSIC_VOLUME) as u32;
                    st.music_volume.store(music, Relaxed);
                    h.a(EE_SCREENS_UI_EV_SET_MUSIC_VOLUME, 0);
                }
                if h.has(EE_SCREENS_UI_GLOBAL_SOUND) {
                    let master = h.o(EE_SCREENS_UI_EV_MASTER_VOLUME) as u32;
                    st.master_volume.store(master, Relaxed);
                    h.a(EE_SCREENS_UI_EV_SET_MASTER_VOLUME, 0);
                }
                st.minimized.store(true, Relaxed);
            }
        }
        SCREENS_UI_EVENT_RESTORED => {
            if st.minimized.load(Relaxed) {
                if h.has(EE_SCREENS_UI_GLOBAL_CONDUCTOR) {
                    let music = st.music_volume.load(Relaxed) as i64;
                    h.a(EE_SCREENS_UI_EV_SET_MUSIC_VOLUME, music);
                }
                if h.has(EE_SCREENS_UI_GLOBAL_SOUND) {
                    let master = st.master_volume.load(Relaxed) as i64;
                    h.a(EE_SCREENS_UI_EV_SET_MASTER_VOLUME, master);
                }
                st.minimized.store(false, Relaxed);
            }
        }
        SCREENS_UI_EVENT_FOCUS_GAINED => {
            h.a(EE_SCREENS_UI_EV_MUTE, 0);
            if h.has(EE_SCREENS_UI_GLOBAL_KEYBOARD) {
                h.a(EE_SCREENS_UI_EV_KEYBOARD, 1);
            }
            if h.has(EE_SCREENS_UI_GLOBAL_MOUSE) {
                h.a(EE_SCREENS_UI_EV_MOUSE, 1);
            }
            if h.has(EE_SCREENS_UI_GLOBAL_WINDOW_AND_MOUSE) {
                h.o(EE_SCREENS_UI_EV_CENTER_MOUSE);
            }
            if h.has(EE_SCREENS_UI_GLOBAL_POINTER) {
                h.a(EE_SCREENS_UI_EV_POINTER_POST, 0x404);
            }
        }
        SCREENS_UI_EVENT_FOCUS_LOST => {
            if h.o(EE_SCREENS_UI_EV_MUTE_IN_BACKGROUND) != 0 {
                h.a(EE_SCREENS_UI_EV_MUTE, 1);
            }
            // a windowed game stays on screen in the background (the original's full screen minimised)
            if h.has(EE_SCREENS_UI_GLOBAL_USER_INTERFACE)
                && h.o(EE_SCREENS_UI_EV_QUITTING) == 0
                && h.has(EE_SCREENS_UI_GLOBAL_WINDOW)
                && h.o(EE_SCREENS_UI_EV_FULL_SCREEN) != 0
            {
                h.o(EE_SCREENS_UI_EV_MINIMIZE);
            }
            if h.has(EE_SCREENS_UI_GLOBAL_KEYBOARD) {
                h.a(EE_SCREENS_UI_EV_KEYBOARD, 0);
            }
            if h.has(EE_SCREENS_UI_GLOBAL_MOUSE) {
                h.a(EE_SCREENS_UI_EV_MOUSE, 0);
            }
            if h.has(EE_SCREENS_UI_GLOBAL_WINDOW_AND_MOUSE) {
                h.o(EE_SCREENS_UI_EV_CENTER_MOUSE);
            }
            if h.has(EE_SCREENS_UI_GLOBAL_POINTER) {
                h.a(EE_SCREENS_UI_EV_POINTER_POST, 0x402);
                while h.o(EE_SCREENS_UI_EV_POINTER_SHOWN) != 0 {
                    h.o(EE_SCREENS_UI_EV_SLEEP);
                }
            }
        }
        // Control, and AltGr (Control + Alt: the one Alt WM_KEYDOWN sees)
        SCREENS_UI_EVENT_KEY_DOWN => {
            if !st.control_down.load(Relaxed) {
                st.control_down.store(is_control_key(ev.virtual_key), Relaxed);
            }
            if !st.alt_down.load(Relaxed) {
                st.alt_down.store(is_alt_key(ev.virtual_key), Relaxed);
            }
        }
        SCREENS_UI_EVENT_KEY_UP => {
            if st.control_down.load(Relaxed) {
                st.control_down.store(!is_control_key(ev.virtual_key), Relaxed);
            }
            if st.alt_down.load(Relaxed) {
                st.alt_down.store(!is_alt_key(ev.virtual_key), Relaxed);
            }
        }
        SCREENS_UI_EVENT_TEXT_EDITING => {
            if !h.has(EE_SCREENS_UI_GLOBAL_USER_INTERFACE) || !h.has(EE_SCREENS_UI_GLOBAL_MANAGER) {
                return;
            }
            if !ev.text.is_empty() {
                if !st.composing.load(Relaxed) {
                    h.o(EE_SCREENS_UI_EV_IME_OPEN);
                    st.composing.store(true, Relaxed);
                }
                h.0.op(EE_SCREENS_UI_EV_IME_UPDATE, ev.text_address, ev.cursor as i64, ev.length as i64);
            } else if st.composing.load(Relaxed) {
                h.o(EE_SCREENS_UI_EV_IME_CLOSE);
                st.composing.store(false, Relaxed);
            }
        }
        // WM_CHAR, one per character; a composition's result (GCS_RESULTSTR)
        SCREENS_UI_EVENT_TEXT => {
            if st.composing.load(Relaxed)
                && h.has(EE_SCREENS_UI_GLOBAL_USER_INTERFACE)
                && h.has(EE_SCREENS_UI_GLOBAL_MANAGER)
            {
                h.a(EE_SCREENS_UI_EV_IME_RESULT, ev.text_address);
                h.o(EE_SCREENS_UI_EV_IME_CLOSE);
                st.composing.store(false, Relaxed);
                return;
            }
            if !st.control_down.load(Relaxed) || st.alt_down.load(Relaxed) {
                let mut at = 0;
                while at < ev.text.len() {
                    let n = utf8_length(&ev.text[at..]).min(ev.text.len() - at);
                    // WM_CHAR's wParam: one character's bytes
                    let mut key = [0u8; 8];
                    key[..n.min(8)].copy_from_slice(&ev.text[at..at + n.min(8)]);
                    if !matches!(key[0], b'\t' | 0x1b | 8 | b'\r') {
                        h.a(EE_SCREENS_UI_EV_TRANSLATED_KEY, key.as_ptr() as i64);
                    }
                    if n == 0 {
                        break;
                    }
                    at += n;
                }
            }
        }
        _ => {}
    }
}

#[cfg(test)]
mod tests;
