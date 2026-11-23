//! The lobby's root screen (RootScreen.cpp, R6 WON round 32): the Internet and LAN screens' tab control (lobby or
//! LAN and direct connect, game, help, news) and the lower bar's layout, whole in Rust. The C++ class keeps its
//! members (the held controls, the tab ids, the flags) and its vtable; every method is one forward to
//! [`ee_won_rootscreen_call`], and each C++ statement the bodies ran is one op of the screen's host (`RootHost` in
//! RootScreen.cpp), in the original's order. The function-local statics (the banner's height, the SWAT resize's
//! flag and old height) are this module's.
//!
//! Reach: the LAN screen is a RootScreen (the LAN game and the tour lay it out, join and leave its game tab); the
//! Internet screen's branches and the SWAT resize (no layout defines SWATUJDEF) run only in the unit tests.

use std::ffi::c_void;
use std::sync::atomic::{AtomicBool, AtomicI32, Ordering};

/// The host: `(screen, op, a, b, c) -> result`.
pub type EeWonRootScreenHostFn =
    Option<unsafe extern "C" fn(screen: *mut c_void, op: EeWonRootScreenOp, a: isize, b: isize, c: isize) -> isize>;

/// The screen's scalar members (`FieldGet` / `FieldSet`).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonRootScreenField {
    /// `mIsLan` (0 or 1).
    IsLan = 0,
    /// `mHideLowerControls` (0 or 1).
    HideLowerControls,
    /// `mIsSwat` (0 or 1).
    IsSwat,
    /// `mLastJoinTab`.
    LastJoinTab,
    /// `mGameTab`.
    GameTab,
    /// `mDirectConnectTab`.
    DirectConnectTab,
    /// `mLanTab`.
    LanTab,
    /// `mLobbyTab`.
    LobbyTab,
    /// `mRoomSpecFlags`.
    RoomSpecFlags,
}

/// The lower bar's held controls (`Part*` ops).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonRootScreenPart {
    /// `mLowerLayoutPoint`.
    LowerLayoutPoint = 0,
    /// `mLowerDivider`.
    LowerDivider,
    /// `mSierraLogo`.
    SierraLogo,
    /// `mLowerBanner`.
    LowerBanner,
    /// `mLowerBannerFrame`.
    LowerBannerFrame,
    /// `mBackButton`.
    BackButton,
}

/// The host's ops: each one C++ statement.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonRootScreenOp {
    /// the member `a` (`EeWonRootScreenField`).
    FieldGet = 1,
    /// the member `a` = `b`.
    FieldSet,
    /// `mTabCtrl->GetCurTabId()`.
    TabCurId,
    /// `mTabCtrl->EnableTab(a, b != 0)`.
    TabEnable,
    /// `mTabCtrl->SetCurTab(a, true)`.
    TabSetCur,
    /// `mTabCtrl->SetCurTabByName(LobbyGetTabSelect(), true)`.
    TabSetCurByPref,
    /// `mTabCtrl->GetTabIdForContainer(sRootTab.get())` (the tab the last `Find*Tab` found).
    TabIdOfFound,
    /// `sRootTab = NULL` (InitComponent's local going out of scope).
    TabFoundClear,
    /// the part `a` (`EeWonRootScreenPart`) is not NULL.
    PartExists,
    /// `part->SetVisible(b != 0)`.
    PartSetVisible,
    /// `part->Height()`.
    PartHeight,
    /// `part->Width()`.
    PartWidth,
    /// `part->Left()`.
    PartLeft,
    /// `part->SetPos(b, c)`.
    PartSetPos,
    /// `part->SetSize(b, c)`.
    PartSetSize,
    /// `mLowerBannerFrame->mThickness`.
    FrameThickness,
    /// `Container::Virtual08()`.
    BaseSizeChanged,
    /// `this->Height()`.
    Height,
    /// `this->Relayout()`.
    Relayout,
    /// `Component::HandleComponentEvent((ComponentEvent*)a)`.
    BaseEvent,
    /// `((ComponentEvent*)a)->GetControlId()`.
    EventControlId,
    /// `((ComponentEvent*)a)->mComponent == mTabCtrl.get()`.
    EventFromTabs,
    /// `GetMyLobbyGame()`.
    MyGame,
    /// `((LobbyGame*)a)->MatchesRoomSpec(b)`.
    GameMatches,
    /// `LobbyGetCrossPromotion()`.
    CrossPromotion,
    /// `LobbySaveUserPrefs(false)`.
    SaveUserPrefs,
    /// `mHelpTab->ResetBrowser()`.
    HelpReset,
    /// `mNewsTab.get() != NULL`.
    NewsExists,
    /// `mNewsTab->ResetBrowser()`.
    NewsReset,
    /// `((ComponentInit*)a)->mType`.
    InitType,
    /// `static_cast<ComponentConfigInit*>(a)->mConfig`.
    InitConfig,
    /// `((ComponentConfig*)a)->Fn386860("SWATUJDEF") != NULL`.
    ConfigHasSwat,
    /// `SafeGetComponent(a, mTabCtrl, "TabCtrl", __FILE__, 79)`.
    FindTabCtrl,
    /// `SafeGetComponent(a, mLowerLayoutPoint, "LowerLayoutPoint", __FILE__, 80)`.
    FindLowerLayoutPoint,
    /// `SafeGetShell(a, mLowerDivider, MsSeperator, "LowerDivider", __FILE__, 81)`.
    FindLowerDivider,
    /// `SafeGetComponent(a, mSierraLogo, "SierraLogo", __FILE__, 82)`.
    FindSierraLogo,
    /// `SafeGetComponent(a, mLowerBanner, "LowerBanner", __FILE__, 83)`.
    FindLowerBanner,
    /// `SafeGetComponent(a, mLowerBannerFrame, "LowerBannerFrame", __FILE__, 84)`.
    FindLowerBannerFrame,
    /// `SafeGetComponent(a, mBackButton, "BackButton", __FILE__, 85)`.
    FindBackButton,
    /// `SafeGetComponent(a, mHelpTab, "HelpTab", __FILE__, 86)`.
    FindHelpTab,
    /// `SafeGetComponent(a, sRootTab, "GameTab", __FILE__, 89)`.
    FindGameTab,
    /// `SafeGetComponent(a, mNewsTab, "NewsTab", __FILE__, 95)`.
    FindNewsTab,
    /// `SafeGetComponent(a, sRootTab, "LobbyTab", __FILE__, 96)`.
    FindLobbyTab,
    /// `SafeGetComponent(a, sRootTab, "LanTab", __FILE__, 101)`.
    FindLanTab,
    /// `SafeGetComponent(a, sRootTab, "DirectConnectTab", __FILE__, 102)`.
    FindDirectConnectTab,
    /// `this->BroadcastEventDown(SetRoomSpecFlagsEvent(a))`.
    BroadcastRoomSpec,
    /// `this->SubscribeToBroadcast(1)`.
    Subscribe,
    /// `GetRoot()->GetRect(sRootWindowRect, false)`.
    WindowRectGet,
    /// `GetRoot()->GetRect(sRootClientRect, true)`.
    ClientRectGet,
    /// `sRootWindowRect.height`.
    WindowHeight,
    /// `sRootClientRect.height`.
    ClientHeight,
    /// `GetRoot()->SetPosSize(sRootWindowRect.x, sRootWindowRect.y, sRootWindowRect.width, a)`.
    WindowSetHeight,
}

/// The calls in (`ee_won_rootscreen_call`).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonRootScreenCall {
    /// The constructor's body (`a`: is the LAN screen).
    Construct = 1,
    /// `InitComponent(*(ComponentInit*)a)`.
    InitComponent,
    /// `Virtual08()` (SizeChanged).
    SizeChanged,
    /// `HandleComponentEvent((ComponentEvent*)a)`, its type `b`.
    Event,
    /// `OnDirectConnectTab()`.
    OnDirectConnectTab,
    /// `OnLanTab()`.
    OnLanTab,
    /// `EnterGameTab(a != 0)`.
    EnterGameTab,
    /// `ShowLobbyTab()`.
    ShowLobbyTab,
    /// `HandleJoinedGame()`.
    JoinedGame,
    /// `HandleLeftGame()`.
    LeftGame,
    /// `HandleEnteredScreen()`.
    EnteredScreen,
    /// `HandleTabCtrlTabSelected((ComponentEvent*)a)`.
    TabSelected,
}

use EeWonRootScreenField as F;
use EeWonRootScreenOp as Op;
use EeWonRootScreenPart as P;

/// `Virtual08`'s `static int aBannerHeight` (read from the layout on the first call that shows the bar).
static BANNER_HEIGHT: AtomicI32 = AtomicI32::new(0);
static BANNER_HEIGHT_SET: AtomicBool = AtomicBool::new(false);
/// `HandleTabCtrlTabSelected`'s `static bool aResized` and `static int aOldHeight`.
static RESIZED: AtomicBool = AtomicBool::new(false);
static OLD_HEIGHT: AtomicI32 = AtomicI32::new(0);

/// The lobby events the screen handles (WONLobbyEvents.h's ids).
const EVENT_ENTERED_SCREEN: i32 = 100014;
const EVENT_JOINED_GAME: i32 = 100029;
const EVENT_LEFT_GAME: i32 = 100032;

struct Screen<'a> {
    host: &'a dyn Fn(Op, isize, isize, isize) -> isize,
}

impl Screen<'_> {
    fn op(&self, op: Op, a: isize, b: isize, c: isize) -> isize {
        (self.host)(op, a, b, c)
    }
    fn on(&self, op: Op) -> isize {
        self.op(op, 0, 0, 0)
    }
    fn get(&self, f: F) -> i32 {
        self.op(Op::FieldGet, f as isize, 0, 0) as i32
    }
    fn set(&self, f: F, v: i32) {
        self.op(Op::FieldSet, f as isize, v as isize, 0);
    }
    fn part(&self, op: Op, p: P) -> i32 {
        self.op(op, p as isize, 0, 0) as i32
    }
    fn visible(&self, p: P, v: bool) {
        self.op(Op::PartSetVisible, p as isize, v as isize, 0);
    }
    fn cur_tab(&self) -> i32 {
        self.on(Op::TabCurId) as i32
    }

    fn construct(&self, lan: bool) {
        self.set(F::IsLan, lan as i32);
        self.set(F::RoomSpecFlags, if lan { 8 } else { 4 });
        // `mLobbyTab = mLanTab = mDirectConnectTab = mLastJoinTab = -1` (assigned right to left)
        for f in [F::LastJoinTab, F::DirectConnectTab, F::LanTab, F::LobbyTab] {
            self.set(f, -1);
        }
        self.set(F::HideLowerControls, 0);
        self.set(F::IsSwat, 0);
    }

    fn enter_game_tab(&self, enter: bool) {
        let game = self.get(F::GameTab) as isize;
        if enter {
            self.op(Op::TabEnable, game, 1, 0);
            if self.cur_tab() as isize != game {
                self.set(F::LastJoinTab, self.cur_tab());
                self.op(Op::TabSetCur, game, 0, 0);
            }
        } else {
            if self.cur_tab() as isize == game {
                self.op(Op::TabSetCur, self.get(F::LastJoinTab) as isize, 0, 0);
            }
            self.op(Op::TabEnable, game, 0, 0);
        }
    }

    fn joined_game(&self) {
        let game = self.on(Op::MyGame);
        if game != 0 && self.op(Op::GameMatches, game, self.get(F::RoomSpecFlags) as isize, 0) != 0 {
            self.enter_game_tab(true);
        }
    }

    fn size_changed(&self) {
        if self.part(Op::PartExists, P::LowerLayoutPoint) != 0 {
            let place = |y: i32| {
                let x = self.part(Op::PartLeft, P::LowerLayoutPoint);
                self.op(Op::PartSetPos, P::LowerLayoutPoint as isize, x as isize, y as isize);
            };
            if self.get(F::HideLowerControls) != 0 {
                for p in [P::LowerDivider, P::SierraLogo, P::LowerBanner, P::LowerBannerFrame, P::BackButton] {
                    self.visible(p, false);
                }
                place(self.on(Op::Height) as i32);
                self.on(Op::BaseSizeChanged);
                return;
            }
            if !BANNER_HEIGHT_SET.load(Ordering::Relaxed) {
                BANNER_HEIGHT.store(self.part(Op::PartHeight, P::LowerBanner), Ordering::Relaxed);
                BANNER_HEIGHT_SET.store(true, Ordering::Relaxed);
            }
            let banner = BANNER_HEIGHT.load(Ordering::Relaxed);
            self.visible(P::LowerDivider, true);
            self.visible(P::SierraLogo, true);
            self.visible(P::BackButton, true);
            let mut lower = self.part(Op::PartHeight, P::LowerDivider);
            let size = |h: i32| {
                let w = self.part(Op::PartWidth, P::LowerBanner);
                self.op(Op::PartSetSize, P::LowerBanner as isize, w as isize, h as isize);
            };
            if self.on(Op::CrossPromotion) != 0 {
                self.visible(P::LowerBannerFrame, true);
                self.visible(P::LowerBanner, true);
                size(banner);
                let thickness = self.on(Op::FrameThickness) as i32;
                lower = lower.wrapping_add(banner.wrapping_add(thickness.wrapping_mul(2)));
            } else {
                self.visible(P::LowerBannerFrame, false);
                self.visible(P::LowerBanner, false);
                let mut h = self.part(Op::PartHeight, P::BackButton);
                if self.part(Op::PartHeight, P::SierraLogo) > h {
                    h = self.part(Op::PartHeight, P::SierraLogo);
                }
                lower = lower.wrapping_add(h);
                size(h);
            }
            let height = self.on(Op::Height) as i32;
            let divider = self.part(Op::PartHeight, P::LowerDivider);
            place(height.wrapping_sub(divider.wrapping_sub(2) / 2).wrapping_sub(lower));
        }
        self.on(Op::BaseSizeChanged);
    }

    fn event(&self, event: isize, ty: i32) {
        use ee_won_gui::guihost::{ee_won_gui_event_type, EeWonGuiEvent as E};
        match ty {
            t if t == ee_won_gui_event_type(E::TabCtrlTabSelected) => self.tab_selected(event),
            t if t == ee_won_gui_event_type(E::ButtonPressed) => {
                if self.op(Op::EventControlId, event, 0, 0) == 3 {
                    self.on(Op::Relayout);
                    return;
                }
            }
            EVENT_ENTERED_SCREEN => return self.entered_screen(),
            EVENT_JOINED_GAME => return self.joined_game(),
            EVENT_LEFT_GAME => return self.enter_game_tab(false),
            _ => {}
        }
        self.op(Op::BaseEvent, event, 0, 0);
    }

    fn entered_screen(&self) {
        self.on(Op::HelpReset);
        if self.on(Op::NewsExists) != 0 {
            self.on(Op::NewsReset);
        }
        self.on(Op::TabSetCurByPref);
    }

    fn init(&self, init: isize) {
        if self.op(Op::InitType, init, 0, 0) != 2 {
            return;
        }
        let config = self.op(Op::InitConfig, init, 0, 0);
        let find = |op: Op| self.op(op, config, 0, 0);
        self.set(F::IsSwat, (find(Op::ConfigHasSwat) != 0) as i32);
        for op in [
            Op::FindTabCtrl,
            Op::FindLowerLayoutPoint,
            Op::FindLowerDivider,
            Op::FindSierraLogo,
            Op::FindLowerBanner,
            Op::FindLowerBannerFrame,
            Op::FindBackButton,
            Op::FindHelpTab,
        ] {
            find(op);
        }
        let tab = |op: Op, f: F| {
            find(op);
            self.set(f, self.on(Op::TabIdOfFound) as i32);
        };
        tab(Op::FindGameTab, F::GameTab);
        self.op(Op::TabEnable, self.get(F::GameTab) as isize, 0, 0);
        if self.get(F::IsLan) == 0 {
            find(Op::FindNewsTab);
            tab(Op::FindLobbyTab, F::LobbyTab);
            self.op(Op::BroadcastRoomSpec, 4, 0, 0);
        } else {
            tab(Op::FindLanTab, F::LanTab);
            tab(Op::FindDirectConnectTab, F::DirectConnectTab);
            self.op(Op::BroadcastRoomSpec, 8, 0, 0);
        }
        self.op(Op::Subscribe, 0, 0, 0);
        self.on(Op::TabFoundClear);
    }

    fn tab_selected(&self, event: isize) {
        if self.op(Op::EventFromTabs, event, 0, 0) == 0 {
            return;
        }
        if self.get(F::IsSwat) != 0 {
            self.on(Op::WindowRectGet);
            self.on(Op::ClientRectGet);
            let window = self.on(Op::WindowHeight) as i32;
            let client = self.on(Op::ClientHeight) as i32;
            if self.cur_tab() == 2 {
                self.set(F::HideLowerControls, 1);
                OLD_HEIGHT.store(window, Ordering::Relaxed);
                RESIZED.store(true, Ordering::Relaxed);
                self.op(Op::WindowSetHeight, window.wrapping_sub(client).wrapping_add(150) as isize, 0, 0);
            } else if RESIZED.load(Ordering::Relaxed) {
                self.set(F::HideLowerControls, 0);
                RESIZED.store(false, Ordering::Relaxed);
                self.op(Op::WindowSetHeight, OLD_HEIGHT.load(Ordering::Relaxed) as isize, 0, 0);
            }
            self.on(Op::Relayout);
        }
        self.on(Op::SaveUserPrefs);
    }

    fn call(&self, call: EeWonRootScreenCall, a: isize, b: isize) -> isize {
        type C = EeWonRootScreenCall;
        match call {
            C::Construct => self.construct(a != 0),
            C::InitComponent => self.init(a),
            C::SizeChanged => self.size_changed(),
            C::Event => self.event(a, b as i32),
            C::OnDirectConnectTab => {
                return (self.get(F::IsLan) != 0 && self.cur_tab() == self.get(F::DirectConnectTab)) as isize
            }
            C::OnLanTab => return (self.get(F::IsLan) != 0 && self.cur_tab() == self.get(F::LanTab)) as isize,
            C::EnterGameTab => self.enter_game_tab(a != 0),
            C::ShowLobbyTab => {
                self.op(Op::TabSetCur, self.get(F::LobbyTab) as isize, 0, 0);
            }
            C::JoinedGame => self.joined_game(),
            C::LeftGame => self.enter_game_tab(false),
            C::EnteredScreen => self.entered_screen(),
            C::TabSelected => self.tab_selected(a),
        }
        0
    }
}

/// One call on the root screen `screen` with its host `host`.
///
/// # Safety
/// `host` answers the ops for `screen`; `a` as the call documents.
#[no_mangle]
pub unsafe extern "C" fn ee_won_rootscreen_call(
    screen: *mut c_void,
    host: EeWonRootScreenHostFn,
    call: EeWonRootScreenCall,
    a: isize,
    b: isize,
) -> isize {
    let out = |op: Op, a: isize, b: isize, c: isize| match host {
        // SAFETY: the contract
        Some(f) => unsafe { f(screen, op, a, b, c) },
        None => 0,
    };
    Screen { host: &out }.call(call, a, b)
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::cell::RefCell;
    use std::collections::BTreeMap;

    /// A screen of fields and parts, logging the effects.
    #[derive(Default)]
    struct World {
        fields: RefCell<BTreeMap<u32, isize>>,
        cur: RefCell<isize>,
        log: RefCell<Vec<String>>,
        cross: bool,
        no_point: bool,
        init_type: isize,
        swat: bool,
    }

    impl World {
        fn op(&self, op: Op, a: isize, b: isize, c: isize) -> isize {
            match op {
                Op::FieldGet => return *self.fields.borrow().get(&(a as u32)).unwrap_or(&0),
                Op::FieldSet => {
                    self.fields.borrow_mut().insert(a as u32, b);
                    return 0;
                }
                Op::TabCurId => return *self.cur.borrow(),
                Op::TabSetCur => *self.cur.borrow_mut() = a,
                Op::PartExists => return (!self.no_point) as isize,
                Op::PartHeight => return [0, 6, 30, 40, 0, 20][a as usize],
                Op::PartWidth => return 500,
                Op::PartLeft => return 3,
                Op::FrameThickness => return 2,
                Op::Height => return 768,
                Op::CrossPromotion => return self.cross as isize,
                Op::InitType => return self.init_type,
                Op::InitConfig => return 0x99,
                Op::ConfigHasSwat => return self.swat as isize,
                Op::TabIdOfFound => return 10 + self.log.borrow().len() as isize,
                Op::MyGame => return 0x55,
                Op::GameMatches => return (b == 8) as isize,
                Op::EventFromTabs | Op::NewsExists => return 1,
                Op::EventControlId => return a,
                Op::WindowHeight => return 600,
                Op::ClientHeight => return 570,
                _ => {}
            }
            self.log.borrow_mut().push(format!("{op:?} {a} {b} {c}"));
            0
        }
        fn call(&self, call: EeWonRootScreenCall, a: isize, b: isize) -> isize {
            let host = |op: Op, a: isize, b: isize, c: isize| self.op(op, a, b, c);
            Screen { host: &host }.call(call, a, b)
        }
        fn field(&self, f: F) -> isize {
            *self.fields.borrow().get(&(f as u32)).unwrap_or(&0)
        }
        fn take(&self) -> Vec<String> {
            std::mem::take(&mut *self.log.borrow_mut())
        }
    }

    #[test]
    fn construct_and_the_lan_tabs() {
        let w = World { init_type: 2, ..Default::default() };
        w.call(EeWonRootScreenCall::Construct, 1, 0);
        assert_eq!((w.field(F::IsLan), w.field(F::RoomSpecFlags), w.field(F::LobbyTab)), (1, 8, -1));
        w.call(EeWonRootScreenCall::InitComponent, 0x77, 0);
        let log = w.take();
        assert_eq!(log[0], "FindTabCtrl 153 0 0");
        assert!(log.contains(&"BroadcastRoomSpec 8 0 0".to_string()));
        assert!(!log.iter().any(|l| l.starts_with("FindNewsTab")));
        assert_eq!(log.last().unwrap(), "TabFoundClear 0 0 0");
        assert_ne!(w.field(F::LanTab), -1);
        // the game tab: entered, then left back to the tab joined from
        w.fields.borrow_mut().insert(F::GameTab as u32, 5);
        *w.cur.borrow_mut() = 1;
        w.call(EeWonRootScreenCall::Event, 0, EVENT_JOINED_GAME as isize);
        assert_eq!((*w.cur.borrow(), w.field(F::LastJoinTab)), (5, 1));
        w.call(EeWonRootScreenCall::Event, 0, EVENT_LEFT_GAME as isize);
        assert_eq!(*w.cur.borrow(), 1);
        assert_eq!(w.take().last().unwrap(), "TabEnable 5 0 0");
        w.fields.borrow_mut().insert(F::DirectConnectTab as u32, 1);
        assert_eq!(w.call(EeWonRootScreenCall::OnDirectConnectTab, 0, 0), 1);
        assert_eq!(w.call(EeWonRootScreenCall::OnLanTab, 0, 0), 0);
        // not a config init: nothing
        let w = World { init_type: 1, ..Default::default() };
        w.call(EeWonRootScreenCall::InitComponent, 0x77, 0);
        assert!(w.take().is_empty());
    }

    #[test]
    fn the_internet_screen_finds_its_news_and_lobby_tabs() {
        let w = World { init_type: 2, ..Default::default() };
        w.call(EeWonRootScreenCall::Construct, 0, 0);
        w.call(EeWonRootScreenCall::InitComponent, 0x77, 0);
        let log = w.take();
        assert!(log.contains(&"FindNewsTab 153 0 0".to_string()));
        assert!(log.contains(&"BroadcastRoomSpec 4 0 0".to_string()));
        w.call(EeWonRootScreenCall::Event, 0, EVENT_JOINED_GAME as isize);
        assert!(w.take().is_empty(), "the LAN game does not match the Internet screen's spec");
    }

    #[test]
    fn the_lower_bar_layouts() {
        let w = World::default();
        w.call(EeWonRootScreenCall::SizeChanged, 0, 0);
        let log = w.take();
        // logo 30 over the back button 20: the banner sized to it, the point at 768 - (6-2)/2 - (6+30)
        assert!(log.contains(&"PartSetSize 3 500 30".to_string()));
        assert_eq!(log[log.len() - 2], "PartSetPos 0 3 730");
        assert_eq!(log.last().unwrap(), "BaseSizeChanged 0 0 0");
        let w = World { cross: true, ..Default::default() };
        w.call(EeWonRootScreenCall::SizeChanged, 0, 0);
        let log = w.take();
        // the banner's height latched at its first layout (40): 6 + 40 + 2 * 2
        assert!(log.contains(&"PartSetSize 3 500 40".to_string()));
        assert!(log.contains(&format!("PartSetPos 0 3 {}", 768 - 2 - 50)));
        let w = World::default();
        w.fields.borrow_mut().insert(F::HideLowerControls as u32, 1);
        w.call(EeWonRootScreenCall::SizeChanged, 0, 0);
        let log = w.take();
        assert_eq!(log.len(), 7);
        assert_eq!(log[5], "PartSetPos 0 3 768");
        let w = World { no_point: true, ..Default::default() };
        w.call(EeWonRootScreenCall::SizeChanged, 0, 0);
        assert_eq!(w.take(), vec!["BaseSizeChanged 0 0 0"]);
    }

    #[test]
    fn events_tabs_and_the_swat_resize() {
        use ee_won_gui::guihost::{ee_won_gui_event_type, EeWonGuiEvent as E};
        let w = World::default();
        let pressed = ee_won_gui_event_type(E::ButtonPressed) as isize;
        w.call(EeWonRootScreenCall::Event, 3, pressed);
        assert_eq!(w.take(), vec!["Relayout 0 0 0"]);
        w.call(EeWonRootScreenCall::Event, 4, pressed);
        assert_eq!(w.take(), vec!["BaseEvent 4 0 0"]);
        w.call(EeWonRootScreenCall::Event, 0, EVENT_ENTERED_SCREEN as isize);
        assert_eq!(w.take(), vec!["HelpReset 0 0 0", "NewsReset 0 0 0", "TabSetCurByPref 0 0 0"]);
        let selected = ee_won_gui_event_type(E::TabCtrlTabSelected) as isize;
        w.call(EeWonRootScreenCall::Event, 8, selected);
        assert_eq!(w.take(), vec!["SaveUserPrefs 0 0 0", "BaseEvent 8 0 0"]);
        w.fields.borrow_mut().insert(F::IsSwat as u32, 1);
        *w.cur.borrow_mut() = 2;
        w.call(EeWonRootScreenCall::TabSelected, 8, 0);
        assert_eq!(w.take()[2], "WindowSetHeight 180 0 0");
        assert_eq!(w.field(F::HideLowerControls), 1);
        *w.cur.borrow_mut() = 1;
        w.call(EeWonRootScreenCall::TabSelected, 8, 0);
        assert_eq!(w.take()[2], "WindowSetHeight 600 0 0");
        assert_eq!(w.field(F::HideLowerControls), 0);
        w.call(EeWonRootScreenCall::TabSelected, 8, 0);
        assert_eq!(
            w.take(),
            vec!["WindowRectGet 0 0 0", "ClientRectGet 0 0 0", "Relayout 0 0 0", "SaveUserPrefs 0 0 0"]
        );
    }
}
