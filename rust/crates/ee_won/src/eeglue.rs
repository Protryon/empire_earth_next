//! The Empire Earth lobby's glue to the game (R6 WONLobby round 6; the original EELobbyInit.cpp, EELobbyMisc.cpp
//! and the decisions of EELobbyInterface.cpp): the lobby's set-up for Empire Earth (the server list's epoch and map
//! type columns, the factories, the resources, the game options, the player colours), the server list's game
//! details, the file and connect dialogs' accessors and the connect wait, the saved games in the game room (header
//! checks, the room set up from the saved options and players), the game options copied to the room and from it
//! (the game's `EEGameOptions` and its players), the network adapter, the game screen's return to the lobby and the
//! lobby's run loop.
//!
//! The C++ keeps what the game owns: its windows and the lobby's window (`EELobbyOpen`, `Show`, `Hide`, `Close`,
//! the frames for the renderer), its network objects (`EELobbyHostGame`, `EELobbyJoinGame`,
//! `EELobbyStartHostedGame`), the classes the lobby's factories make, the lobby lock, and one function per entry
//! point that calls here. Every call on a game object, a lobby object, a string or a global is one op of the host
//! given to `ee_won_eeglue_set_host` (`WONLobby/EELobbyGlueHost.cpp`), the original's statements with no decision
//! between them. The lobby game, the connect dialog and the colour palette are Rust objects called directly.
//!
//! A text the lobby draws or compares is a `(EeWonEeGlueText, value)` pair the host makes a `GUIString` of: one of
//! the EE lobby's resource strings (`eeresource.rs`'s row), a number, a random map type's name, a `GUIStringData`.

use std::sync::atomic::{AtomicBool, AtomicIsize, AtomicUsize, Ordering};

use crate::eegame::{game_call, game_call4, EeWonEeGameCall as GC};
use crate::eeresource::EE_RESOURCES;
use crate::lobbydlg::{EeWonLobbyDlg, EeWonLobbyDlgCall as DC};

/// A text's kind (the `value` it goes with).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonEeGlueText {
    /// the empty string
    Empty = 0,
    /// `EELobbyResource_GetString(value)`
    Resource = 1,
    /// `sprintf("%ld", value)`
    Number = 2,
    /// `EELobbyToGUIString(EERMGetGeneratorDisplayName(value))`
    MapType = 3,
    /// the `GUIStringData*` `value` (0 the empty string)
    Data = 4,
}

/// A field of `EEGameOptions` or of an `EEGameOptionsPlayer` the glue reads or writes.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonEeGlueField {
    LoadType = 0,
    MapSize,
    Resources,
    EpochStart,
    EpochEnd,
    Variant,
    Difficulty,
    GameSpeed,
    UnitLimit,
    WondersForVictory,
    RevealMap,
    AllowCustomCivs,
    LockTeams,
    LockSpeed,
    Victory,
    /// read: `EERMFindGenerator(mMapType, index)`'s index (0 when not found)
    MapTypeIndex,
    // a player's
    Type,
    LoadedType,
    Slot,
    Color,
    Team,
    StartingCitizens,
    IsLocal,
    IsReady,
    B36,
    /// read: `mName == ""` (0 or 1)
    NameEmpty,
}

/// The host's ops (`a`..`f` as documented).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonEeGlueOp {
    /// `GetMyLobbyGame()`.
    MyGame = 1,
    /// `((EELobbyGame*)a)->mRust`.
    GameRust,
    /// `((LobbyGame*)a)->GetIsHost()` (0 or 1).
    GameIsHost,
    /// `((ServerDetailsItem*)a)->mGame.get()`.
    ItemGame,
    /// `((LobbyRustDialog*)a)->mLobbyDlg`.
    DialogRust,
    /// `((RefCount*)a)->AddRef()`.
    AddRef,
    /// `((RefCount*)a)->Release()`.
    Release,
    // ---- drawing and texts (a text: kind `b`, value `c`)
    /// `((Graphics*)a)->SetColor(b)`.
    SetColor,
    /// `((Graphics*)a)->DrawString(text, d, e)`.
    DrawText,
    /// `g->mFont->GetStringWidth(text, 0, text.length())` of the Graphics `a`.
    TextWidth,
    /// `((Graphics*)a)->mFont->GetHeight()`.
    FontHeight,
    /// `((MultiListArea*)a)->mFont->GetHeight()`.
    ListFontHeight,
    /// `text.findNoCase(*(const GUIString*)d, 0)`.
    TextFind,
    /// `textA.CompareNoCase(textB)` (text A: kind `a`, value `b`; text B: kind `c`, value `d`).
    TextCompareNoCase,
    /// `GUIStringData* a`'s length (`mLength`); `b` non-NULL: its units copied to the `uint32_t*` `b`.
    StringUnits,
    // ---- EELobbyInit
    /// The column `a` (0 "Epoch", 1 "MapType") of `ServerListCtrl::GetColumnInfo`: its paint, compare and filter
    /// functions set to the host's.
    SetColumnHandlers,
    /// The game, player, server details item and config factories set.
    SetFactories,
    /// `EELobbyResource_Init(NULL)`, the five component allocators added to it, `EELobbyAddResources` and
    /// `LobbyAddComponentLoader` (the component getter).
    AddResources,
    /// `LobbyAddGameOption(a ? "MapType" : "Epoch", text, true)`.
    AddGameOption,
    /// `EERMGetNumberOfGenerators()`.
    NumMapTypes,
    /// `EEGetPlayerColor(a)`'s red, green and blue into the `float[3]` `b`.
    PlayerColor,
    /// `LobbySetProductName(L"EEAOC", NULL)`.
    SetProductName,
    /// `((ComboBox*)a)->Clear(true)`.
    ComboClear,
    /// `((ComboBox*)a)->AddString(EELobbyToGUIString(EERMGetGeneratorDisplayName(b)))`.
    ComboAddMapType,
    /// `SafeGetComponent(a, ptr, b ? "EELobbyConnectDialog" : "EELobbyFileDialog", __FILE__, 329 or 328)` into a
    /// local `SmartPtr`: the dialog with a reference added.
    FindDialog,
    /// `EELobbyClose()`.
    LobbyClose,
    /// `EELobbyResource_Destroy()`.
    ResourceDestroy,
    /// `GetLobbyConfig()`.
    Config,
    /// `((EELobbyConfig*)a)->mFileTransferPort`.
    ConfigPort,
    /// `LobbySetGameInProgress(a)`.
    SetGameInProgress,
    /// `LobbyLeaveGame(a != 0)`.
    LeaveGame,
    /// `LobbyIAmCaptain()` (0 or 1).
    IAmCaptain,
    /// `GetLobbyWindow()`.
    LobbyWindow,
    /// `((WindowManager*)a)->PostEvent(new UserEvent(send connect status, b), true)`.
    PostSendConnectStatus,
    // ---- EELobbyMisc: saved games
    /// A saved game's record (an `EEPSaveGameHeader`, an `EEGameOptions`, an `FSFileSpec` set to the file `a`).
    SaveNew,
    /// `spec.Open(1) == keseOK` (0 or 1).
    SaveOpen,
    /// `spec.GetLength(length)` (from 0).
    SaveLength,
    /// `spec.Close()`.
    SaveClose,
    /// The header's save versions set and `LoadFromFile(spec) == keseOK` (0 or 1).
    SaveLoadHeader,
    /// `header.mChecksum`.
    SaveChecksum,
    /// `options = header.mGameOptions`; the options' address.
    SaveOptions,
    /// The record freed.
    SaveFree,
    /// `LobbyMessageBox(gEELobbyInterface_LoadGameError, gEELobbyInterface_ErrorReadingFile)`.
    BoxReadError,
    /// `LobbyMessageBox(gEELobbyInterface_LoadGameError, LobbyFormatString(gEELobbyInterface_PlayerNotInGame,
    /// GUIString(a)))`.
    BoxNotInGame,
    /// `((const EEGameOptions*)a)->` field `b` (an `EeWonEeGlueField`).
    OptionsField,
    /// `((const EEGameOptions*)a)->mPlayers[b].` field `c`.
    OptionsPlayer,
    /// `EELobbyToGUIString(((const EEGameOptions*)a)->mPlayers[b].mName)`'s data with a reference added.
    OptionsPlayerName,
    /// `WONFile(a).GetFileName()`'s data with a reference added.
    FileName,
    /// `WONFile(a).GetSize()`.
    FileSize,
    /// `((LobbyClient*)a)->GetPlayer().mPtr`.
    ClientPlayer,
    /// `&gGamePreferences.GetSettings().mGameOptions`.
    DefaultOptions,
    // ---- EELobbyInterface
    /// `gGameOptions.mLock->WriteLock(); gGameOptions = gLobbyGameOptions; mIsMultiplayer = true; mCheatCodes =
    /// false`.
    OptionsBegin,
    /// `gGameOptions.` field `a` `= b`.
    OptionSet,
    /// `gGameOptions.mMapType = EELobbyToWideString(GUIString(a))`.
    OptionMapTypeName,
    /// `gGameOptions.mMapType = EERMGetGeneratorName(a)`.
    OptionMapTypeGenerator,
    /// `gGameOptions.mLock->WriteUnlock()`.
    OptionsEnd,
    /// The player being made: a new `EEGameOptionsPlayer`.
    PlayerNew,
    /// Its field `a` `= b`.
    PlayerSet,
    /// Its `mType`.
    PlayerType,
    /// Its `mName = EELobbyToWideString(GUIString(a))`.
    PlayerSetName,
    /// `gGameOptions.mPlayers[a] = ` it.
    PlayerCommit,
    /// `*(EEGameOptionsPlayer*)a = ` it.
    PlayerCopyTo,
    /// `WONAPI::IPAddr::GetLocalHost()`.
    LocalHost,
    /// `LobbyGetNetAddresses(list, false)` into a new vector.
    AddressesGet,
    /// Its size.
    AddressesCount,
    /// Its entry `b`'s `mHandle`.
    AddressHandle,
    /// The vector freed.
    AddressesFree,
    /// `gNetworkAdapter = a`.
    SetNetworkAdapter,
    /// `((UIForm*)a)->GetControl(b)->SetDisplayMode(0)`.
    FormHide,
    /// `EEIsLobbyOpen()` (0 or 1).
    IsLobbyOpen,
    /// `EELobbyIsShown()` (0 or 1).
    IsShown,
    /// `EELobbyPostReturnFromGame()`.
    PostReturnFromGame,
    /// The UI manager's copy lock and the world's write lock released (`EELobbyActionLocksReleased`).
    LocksRelease,
    /// ... and taken back.
    LocksRetake,
    /// `LobbyRun()`.
    LobbyRun,
    /// `EELobbyPostAction(EELobbyStartHostedGame)`.
    PostStartHostedGame,
    /// `EELobbyShowGameScreen()`.
    ShowGameScreen,
    /// `LobbySuspend()`.
    LobbySuspend,
    /// `EELobbyHide()`.
    Hide,
    /// `EELobbyJoinGame()`.
    JoinGame,
    /// `EELobbyRunSierraUpdate()`.
    SierraUpdate,
    /// `delete gCommunications; gCommunications = NULL`.
    DeleteCommunications,
    /// `EELobbyShow(true)`.
    Show,
    /// `gUIManager->GetFormByID(30)`.
    GameForm,
    /// `gUIManager->GetCurrentForm()`.
    CurrentForm,
    /// `gUIManager->Pop()`.
    PopForm,
    /// `LobbyResume()`.
    LobbyResume,
    /// `LobbyMessageBox(RoutingLogic_GameDissolvedTitle_String, RoutingLogic_GameDissolvedDetails_String)`.
    BoxDissolved,
}

/// The inward calls (`a`.. as documented).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonEeGlueCall {
    /// `EELobbyInit()`.
    Init = 1,
    /// `EELobbyServerDetailsItem::PaintDetailsHook(*(Graphics*)b)` of the item `a`.
    PaintDetails,
    /// `GetDetailsHeightHook((MultiListArea*)a)`.
    DetailsHeight,
    /// The epoch column's paint of the item `b` into the Graphics `a`.
    EpochPaint,
    /// The epoch column's order of the items `a` and `b`.
    EpochCompare,
    /// The epoch column's filter: whether the item `a` stays for the filter text `*(const GUIString*)b` (0 or 1).
    EpochKeep,
    MapTypePaint,
    MapTypeCompare,
    MapTypeKeep,
    /// The component getter of the config `a` (the file and connect dialogs).
    GetComponents,
    /// `EELobbyShutdown()`.
    Shutdown,
    /// `EELobbyGetFileTransferPort()`.
    FileTransferPort,
    /// `EELobbyGetFileDialog()`.
    FileDialog,
    /// `EELobbyGetConnectDialog()`.
    ConnectDialog,
    /// `EELobbyRunConnectDialog()` (0 or 1).
    RunConnectDialog,
    /// `EELobbySetSlotConnected(a, b != 0)`.
    SetSlotConnected,
    /// `EELobbyCancelConnectDialog()`.
    CancelConnectDialog,
    /// `EELobbyPostConnectStatus(a != 0)`.
    PostConnectStatus,
    /// The posted send of the connect status `a`.
    SendConnectStatus,
    /// `EELobbyIsConnectDialogRunning()` (0 or 1).
    IsConnectDialogRunning,
    /// `EELobbyGetSavedGameInfo((const char*)a, *(uint32_t*)b, *(uint32_t*)c)` (0 or 1).
    SavedGameInfo,
    /// `EELobbyLoadSavedGame((const char*)a)` (0 or 1).
    LoadSavedGame,
    /// `EELobbyFillMapTypeCombo((ComboBox*)a)`: its op is the host's; the loop is here.
    FillMapTypeCombo,
    /// `EELobbySetDefaultGameOptions((EELobbyGame*)a)`.
    SetDefaultGameOptions,
    /// `EELobbyFillGameOptions(*(EEGameOptionsPlayer*)a)`.
    FillGameOptions,
    /// `EELobbySyncNetworkAdapter()`.
    SyncNetworkAdapter,
    /// `EELobbyGameDoneEvent::ProcessEvent((UIForm*)a)` with its count at the `int32_t*` `b`.
    GameDone,
    /// `EELobbyRunLoop()`.
    RunLoop,
    /// `EELobbyReturnFromGame()`.
    ReturnFromGame,
    /// `EESetQuitFlag()`.
    SetQuitFlag,
    /// a member: 0 the file dialog, 1 the connect dialog, 2 the closing flag, 3 the game-done flag.
    Field,
    /// The state forgotten without releases (the goldens' fresh start; the game never calls it).
    Reset,
}

/// The host: `(op, a, b, c, d, e, f) -> result`.
pub type EeWonEeGlueHostFn = Option<
    unsafe extern "C" fn(op: EeWonEeGlueOp, a: isize, b: isize, c: isize, d: isize, e: isize, f: isize) -> isize,
>;

/// The glue's state (the original's globals; the lobby runs on one thread, re-entered by its calls out).
pub struct EeWonEeGlue {
    host: AtomicUsize,
    /// `gEELobbyFileDialog`, `gEELobbyConnectDialog` (held `SmartPtr`s)
    file_dialog: AtomicIsize,
    connect_dialog: AtomicIsize,
    /// `gEELobbyClosing` (the run loop is running), `gEELobbyGameDone` (the lobby game dissolved while a game ran)
    closing: AtomicBool,
    game_done: AtomicBool,
}

impl EeWonEeGlue {
    pub const fn new() -> Self {
        Self {
            host: AtomicUsize::new(0),
            file_dialog: AtomicIsize::new(0),
            connect_dialog: AtomicIsize::new(0),
            closing: AtomicBool::new(false),
            game_done: AtomicBool::new(false),
        }
    }
}

impl Default for EeWonEeGlue {
    fn default() -> Self {
        Self::new()
    }
}

static GLUE: EeWonEeGlue = EeWonEeGlue::new();

use EeWonEeGlueField as F;
use EeWonEeGlueOp as Op;
use EeWonEeGlueText as T;

/// A resource string's row by its global's name (the table's order is the original's index table's).
pub fn res(name: &str) -> isize {
    EE_RESOURCES.iter().position(|r| r.name.to_bytes() == name.as_bytes()).expect("an EE lobby string") as isize
}

/// x86's `cvttss2si` of a float: the truncated value, `i32::MIN` for NaN and out of range.
fn trunc_f32(v: f32) -> i32 {
    if v.is_nan() || v >= 2147483648.0 || v < -2147483648.0 {
        i32::MIN
    } else {
        v as i32
    }
}

/// One player colour as 0x00rrggbb from its channels (EELobbyGetPlayerColors' rule).
pub fn pack_color(rgb: [f32; 3]) -> u32 {
    let c = |v: f32| trunc_f32(v * 255.0f32).clamp(0, 255) as u32;
    (c(rgb[0]) << 8 | c(rgb[1])) << 8 | c(rgb[2])
}

/// The game details' four columns: their counts, gaps and colours.
const DETAIL_COUNTS: [usize; 4] = [8, 8, 7, 7];
const DETAIL_GAPS: [isize; 4] = [10, 50, 10, 50];
const DETAIL_COLORS: [isize; 4] = [0x8000000f, 0x8000000e, 0x8000000f, 0x8000000e];
/// The game room form's controls hidden once a game is done.
const GAME_FORM_HIDDEN: [isize; 18] = [11, 5, 3, 4, 6, 16, 23, 26, 10, 9, 13, 22, 2, 1, 20, 27, 0, 7];

impl EeWonEeGlue {
    #[allow(clippy::too_many_arguments)]
    fn op(&self, op: Op, a: isize, b: isize, c: isize, d: isize, e: isize, f: isize) -> isize {
        let h = self.host.load(Ordering::Relaxed);
        if h == 0 {
            return 0;
        }
        // SAFETY: the host set by `ee_won_eeglue_set_host` (a function of that type)
        let h: unsafe extern "C" fn(Op, isize, isize, isize, isize, isize, isize) -> isize =
            unsafe { std::mem::transmute(h) };
        // SAFETY: the C++ host's ops
        unsafe { h(op, a, b, c, d, e, f) }
    }

    fn op1(&self, op: Op, a: isize) -> isize {
        self.op(op, a, 0, 0, 0, 0, 0)
    }

    fn op2(&self, op: Op, a: isize, b: isize) -> isize {
        self.op(op, a, b, 0, 0, 0, 0)
    }

    /// A call on the C++ game's Rust object.
    fn game(&self, game: isize, call: GC, a: isize, b: isize) -> isize {
        game_call(self.op1(Op::GameRust, game), call, a, b, 0)
    }

    fn option(&self, game: isize, option: isize) -> i32 {
        self.game(game, GC::GetOption, option, 0) as i32
    }

    /// A call on a C++ lobby dialog's Rust object (`mLobbyDlg`).
    fn dialog(&self, dialog: isize, call: DC, a: isize, b: isize) -> isize {
        let p = self.op1(Op::DialogRust, dialog) as *const EeWonLobbyDlg;
        if p.is_null() {
            return 0;
        }
        // SAFETY: the C++ dialog's live Rust object, for this call only (it may re-enter)
        unsafe { (*p).call(call, a, b, 0) }
    }

    /// A `GUIStringData`'s units (`StringUnits`), for the name comparisons (`GUIStringData::Compare`).
    fn units(&self, data: isize) -> Vec<u32> {
        if data == 0 {
            return Vec::new();
        }
        let n = self.op2(Op::StringUnits, data, 0).max(0) as usize;
        let mut v = vec![0u32; n];
        if n != 0 {
            self.op2(Op::StringUnits, data, v.as_mut_ptr() as isize);
        }
        v
    }

    /// A held dialog replaced by `new` (which comes with a reference: the look-up's, or 0): the old one released
    /// (the same one again gives the look-up's reference back, as its `SmartPtr` assignment took none).
    fn assign(&self, member: &AtomicIsize, new: isize) {
        let old = member.swap(new, Ordering::Relaxed);
        if old != 0 {
            self.op1(Op::Release, old);
        }
    }

    // ---- EELobbyServerDetailsItem

    /// GetOptionText(option, first, count): the resource string `first + value` below `count`, else empty.
    fn option_text(&self, game: isize, option: isize, first: isize, count: u32) -> (T, isize) {
        let v = self.option(game, option) as u32;
        if v < count {
            (T::Resource, v as isize + first)
        } else {
            (T::Empty, 0)
        }
    }

    /// GetEpochText(start): option 15 is the sixteenth epoch's name.
    fn epoch_text(&self, game: isize, start: bool) -> (T, isize) {
        let option = if start { 4 } else { 5 };
        if self.option(game, option) == 15 {
            return (T::Resource, res("gEELobbyGameOptionsCtrl_Epoch16"));
        }
        self.option_text(game, option, 0, 16)
    }

    fn on_off_text(&self, game: isize, option: isize) -> (T, isize) {
        if self.option(game, option) != 0 {
            (T::Resource, res("gEELobbyGameOptionsCtrl_OptionOn"))
        } else {
            (T::Resource, res("gEELobbyGameOptionsCtrl_OptionOff"))
        }
    }

    /// EELobbyGetMapTypeName(index): a random map type's name, empty outside the generators.
    fn map_type_name(&self, index: i32) -> (T, isize) {
        if index >= 0 && (index as u32 as isize) < self.op1(Op::NumMapTypes, 0) {
            (T::MapType, index as isize)
        } else {
            (T::Empty, 0)
        }
    }

    /// EELobbyGetMapTypeText(game): the random map type's name, or the second game type's for the others.
    fn map_type_text(&self, game: isize) -> (T, isize) {
        if self.option(game, 0) == 0 {
            let i = self.option(game, 2);
            return self.map_type_name(i);
        }
        (T::Resource, res("gEELobbyGameOptionsCtrl_GameType2"))
    }

    fn draw(&self, g: isize, t: (T, isize), x: isize, y: isize) {
        self.op(Op::DrawText, g, t.0 as isize, t.1, x, y, 0);
    }

    fn width(&self, g: isize, t: (T, isize)) -> isize {
        self.op(Op::TextWidth, g, t.0 as isize, t.1, 0, 0, 0) as i32 as isize
    }

    /// PaintDetailsHook: two label/value columns of the game's options, then the players' names.
    fn paint_details(&self, item: isize, g: isize) {
        let game = self.op1(Op::ItemGame, item);
        let r = |n: &str| (T::Resource, res(n));
        let labels1 = [
            r("gEELobbyGameOptionsCtrl_GameType"),
            r("gEELobbyGameOptionsCtrl_MapSize"),
            r("gEELobbyGameOptionsCtrl_MapType"),
            r("gEELobbyGameOptionsCtrl_Resources"),
            r("gEELobbyGameOptionsCtrl_StartingEpoch"),
            r("gEELobbyGameOptionsCtrl_EndingEpoch"),
            r("gEELobbyGameOptionsCtrl_GameUnitLimit"),
            r("gEELobbyGameOptionsCtrl_GameVariant"),
        ];
        let values1 = [
            self.option_text(game, 0, 0x10, 3),
            self.option_text(game, 1, 0x13, 6),
            self.map_type_name(self.option(game, 2)),
            self.option_text(game, 3, 0x19, 5),
            self.epoch_text(game, true),
            self.epoch_text(game, false),
            (T::Number, self.option(game, 9) as isize),
            self.option_text(game, 6, 0x1e, 2),
        ];
        let labels2 = [
            r("gEELobbyGameOptionsCtrl_DifficultyLevel"),
            r("gEELobbyGameOptionsCtrl_GameSpeed"),
            r("gEELobbyGameOptionsCtrl_RevealMap"),
            r("gEELobbyGameOptionsCtrl_UseCustomCivs"),
            r("gEELobbyGameOptionsCtrl_LockTeams"),
            r("gEELobbyGameOptionsCtrl_LockSpeed"),
            r("gEELobbyGameOptionsCtrl_NumberOfWonders"),
        ];
        let values2 = [
            self.option_text(game, 7, 0x20, 3),
            self.option_text(game, 8, 0x23, 4),
            self.on_off_text(game, 0xb),
            self.on_off_text(game, 0xc),
            self.on_off_text(game, 0xd),
            self.on_off_text(game, 0xe),
            // GetWondersText
            if self.option(game, 10) == 0 {
                r("gEELobbyGameOptionsCtrl_WondersOff")
            } else {
                (T::Number, self.option(game, 10) as isize)
            },
        ];
        let columns: [&[(T, isize)]; 4] = [&labels1, &values1, &labels2, &values2];
        let mut x: isize = 0;
        for i in 0..4 {
            let mut y: isize = 0;
            let mut width: isize = 0;
            self.op2(Op::SetColor, g, DETAIL_COLORS[i]);
            for &t in &columns[i][..DETAIL_COUNTS[i]] {
                let w = self.width(g, t);
                if w > width {
                    width = w;
                }
                self.draw(g, t, x, y);
                y += self.op1(Op::FontHeight, g) as i32 as isize;
            }
            x += DETAIL_GAPS[i] + width;
        }
        let mut y: isize = 0;
        let players = r("gEELobbyInterface_Players");
        self.op2(Op::SetColor, g, 0x8000000f);
        self.draw(g, players, x, 0);
        x += self.width(g, players) + 10;
        self.op2(Op::SetColor, g, 0x8000000e);
        let n = self.game(game, GC::GetNumFiles, 0, 0) as i32;
        for i in 0..n {
            let name = self.game(game, GC::GetFile, i as isize, 0);
            self.draw(g, (T::Data, name), x, y);
            y += self.op1(Op::FontHeight, g) as i32 as isize;
        }
    }

    fn compare_epochs(&self, a: isize, b: isize) -> isize {
        let ga = self.op1(Op::ItemGame, a);
        let gb = self.op1(Op::ItemGame, b);
        let vb = self.option(gb, 4);
        self.option(ga, 4).wrapping_sub(vb) as isize
    }

    fn keep(&self, t: (T, isize), filter: isize) -> bool {
        self.op(Op::TextFind, 0, t.0 as isize, t.1, filter, 0, 0) as i32 >= 0
    }

    // ---- EELobbyInit

    fn init(&self) {
        self.op1(Op::SetColumnHandlers, 0);
        self.op1(Op::SetColumnHandlers, 1);
        self.op1(Op::SetFactories, 0);
        self.op1(Op::AddResources, 0);
        for i in 0..16 {
            self.op(Op::AddGameOption, 0, T::Resource as isize, i, 0, 0, 0);
        }
        // EELobbyAddMapTypeOptions
        let n = self.op1(Op::NumMapTypes, 0) as i32;
        for i in 0..n {
            self.op(Op::AddGameOption, 1, T::MapType as isize, i as isize, 0, 0, 0);
        }
        // EELobbyGetPlayerColors(gEELobbyColors, 16): the palette
        let mut colors = [0u32; 16];
        for (i, c) in colors.iter_mut().enumerate() {
            let mut rgb = [0f32; 3];
            self.op2(Op::PlayerColor, i as isize, rgb.as_mut_ptr() as isize);
            *c = pack_color(rgb);
        }
        crate::eecolor::set_palette(&colors);
        self.op(Op::AddGameOption, 1, T::Resource as isize, res("gEELobbyGameOptionsCtrl_GameType2"), 0, 0, 0);
        self.op1(Op::SetProductName, 0);
    }

    fn get_components(&self, config: isize) {
        let file = self.op2(Op::FindDialog, config, 0);
        self.assign(&self.file_dialog, file);
        let connect = self.op2(Op::FindDialog, config, 1);
        self.assign(&self.connect_dialog, connect);
    }

    /// EELobbyShutdown: the lobby closed, its strings emptied, the dialogs let go.
    fn shutdown(&self) {
        self.op1(Op::LobbyClose, 0);
        self.op1(Op::ResourceDestroy, 0);
        self.assign(&self.file_dialog, 0);
        self.assign(&self.connect_dialog, 0);
    }

    fn file_transfer_port(&self) -> isize {
        let config = self.op1(Op::Config, 0);
        if config == 0 {
            return 8889;
        }
        self.op1(Op::ConfigPort, config) as u16 as isize
    }

    /// EELobbyRunConnectDialog: the wait for every player; on failure the host cancels the launch, a client
    /// reports it and leaves (unless the dialog was ended for it).
    fn run_connect_dialog(&self) -> bool {
        // SmartPtr<EELobbyConnectDialog> theDialog = EELobbyGetConnectDialog()
        let dialog = self.connect_dialog.load(Ordering::Relaxed);
        if dialog == 0 {
            return false;
        }
        self.op1(Op::AddRef, dialog);
        let result = self.run_connect(dialog);
        self.op1(Op::Release, dialog);
        result
    }

    fn run_connect(&self, dialog: isize) -> bool {
        let game = self.op1(Op::MyGame, 0);
        if game == 0 {
            return false;
        }
        let host = self.op1(Op::GameIsHost, game) != 0;
        let result = self.dialog(dialog, DC::ConnectRun, game, 0) != 0;
        let game = self.op1(Op::MyGame, 0);
        if game == 0 {
            return false;
        }
        if !result {
            if host {
                self.op1(Op::SetGameInProgress, 0);
                self.game(game, GC::SendCancelConnect, 0, 0);
            } else if self.dialog(dialog, DC::Field, 8, 0) == 0 {
                self.game(game, GC::SendConnectStatus, 0, 0);
                self.op1(Op::LeaveGame, 0);
            }
        }
        result
    }

    // ---- EELobbyMisc

    /// EELobbyLoadSavedGame: the room set up from the saved game, refused (with a box) when the file cannot be read
    /// or a seated player was not in it.
    fn load_saved_game(&self, file: isize) -> bool {
        let s = self.op1(Op::SaveNew, file);
        let ok = self.load_saved_game_of(s, file);
        self.op1(Op::SaveFree, s);
        ok
    }

    fn load_saved_game_of(&self, s: isize, file: isize) -> bool {
        if self.op1(Op::SaveOpen, s) == 0 {
            self.op1(Op::BoxReadError, 0);
            return false;
        }
        self.op1(Op::SaveClose, s);
        if self.op1(Op::SaveLoadHeader, s) == 0 {
            self.op1(Op::BoxReadError, 0);
            return false;
        }
        let options = self.op1(Op::SaveOptions, s);
        let game = self.op1(Op::MyGame, 0);
        if game == 0 {
            return false;
        }
        // std::set<GUIString> of the saved humans' and observers' names
        let mut names: Vec<Vec<u32>> = Vec::new();
        for i in 0..8 {
            let t = self.op(Op::OptionsPlayer, options, i, F::Type as isize, 0, 0, 0);
            if t == 1 || t == 2 {
                let data = self.op2(Op::OptionsPlayerName, options, i);
                let name = self.units(data);
                self.op1(Op::Release, data);
                if !names.iter().any(|n| crate::guistring::compare(n, &name) == 0) {
                    names.push(name);
                }
            }
        }
        for i in 0..8 {
            if self.game(game, GC::SlotRawOption, i, 0) == 3 {
                let name = self.units(self.game(game, GC::SlotGetName, i, 0));
                if !names.iter().any(|n| crate::guistring::compare(n, &name) == 0) {
                    let data = self.game(game, GC::SlotGetName, i, 0);
                    self.op1(Op::BoxNotInGame, data);
                    return false;
                }
            }
        }
        let checksum = self.op1(Op::SaveChecksum, s);
        self.setup_saved_game(options, file, checksum);
        true
    }

    /// EELobbySetupSavedGame: the file, the options, the seated players matched to their saved entries by name, the
    /// other saved players spread over the free slots.
    fn setup_saved_game(&self, options: isize, file: isize, checksum: isize) {
        let game = self.op1(Op::MyGame, 0);
        if game == 0 {
            return;
        }
        self.game(game, GC::SetOption, 0, 1);
        let name = self.op1(Op::FileName, file);
        let size = self.op1(Op::FileSize, file);
        game_call4(self.op1(Op::GameRust, game), GC::SetGameFile, name, size, checksum, 1);
        self.op1(Op::Release, name);
        self.set_game_options(options, game);
        let mut free_options: std::collections::BTreeSet<isize> = (0..8).collect();
        let mut free_slots: std::collections::BTreeSet<isize> = (0..8).collect();
        // std::map<GUIString, long>: a later equal name overwrites; a missing one reads (and inserts) 0
        let mut names: Vec<(Vec<u32>, isize)> = Vec::new();
        for i in 0..8 {
            let t = self.op(Op::OptionsPlayer, options, i, F::Type as isize, 0, 0, 0);
            if t == 1 || t == 2 {
                let data = self.op2(Op::OptionsPlayerName, options, i);
                let name = self.units(data);
                self.op1(Op::Release, data);
                match names.iter_mut().find(|(n, _)| crate::guistring::compare(n, &name) == 0) {
                    Some(e) => e.1 = i,
                    None => names.push((name, i)),
                }
            }
        }
        for i in 0..8 {
            if self.game(game, GC::SlotRawOption, i, 0) == 3 {
                let name = self.units(self.game(game, GC::SlotGetName, i, 0));
                let index = match names.iter().find(|(n, _)| crate::guistring::compare(n, &name) == 0) {
                    Some(e) => e.1,
                    None => {
                        names.push((name, 0));
                        0
                    }
                };
                self.set_slot(game, i, options, index);
                free_options.remove(&index);
                free_slots.remove(&i);
            }
        }
        while let (Some(&slot), Some(&option)) = (free_slots.first(), free_options.first()) {
            self.set_slot(game, slot, options, option);
            free_slots.remove(&slot);
            free_options.remove(&option);
        }
    }

    /// EELobbySetSlot: a slot from a saved player (computer, human, observer, open, closed), then its colour
    /// (humans), team and citizens (all but observers).
    fn set_slot(&self, game: isize, slot: isize, options: isize, player: isize) {
        let field = |f: F| self.op(Op::OptionsPlayer, options, player, f as isize, 0, 0, 0);
        let set_raw = |o: isize, v: isize| game_call(self.op1(Op::GameRust, game), GC::SlotSetRawOption, slot, o, v);
        let t = field(F::Type);
        match t {
            0 => {
                self.game(game, GC::SlotSetComputer, slot, 0);
            }
            1 | 2 => {
                let client = self.game(game, GC::SlotClient, slot, 0);
                if client != 0 && self.op1(Op::ClientPlayer, client) != 0 {
                    set_raw(4, (t == 2) as isize);
                    let client = self.game(game, GC::SlotClient, slot, 0);
                    let player = self.op1(Op::ClientPlayer, self.game(game, GC::SlotClient, slot, 0));
                    game_call(self.op1(Op::GameRust, game), GC::SlotSetPlayer, slot, client, player);
                } else if field(F::NameEmpty) != 0 {
                    self.game(game, GC::SlotSetClosed, slot, 0);
                } else {
                    let name = self.op2(Op::OptionsPlayerName, options, player);
                    self.game(game, GC::SlotSetName, slot, name);
                    self.op1(Op::Release, name);
                    set_raw(4, (t == 2) as isize);
                }
            }
            3 => {
                self.game(game, GC::SlotSetOpen, slot, 0);
                return;
            }
            4 => {
                self.game(game, GC::SlotSetClosed, slot, 0);
                return;
            }
            _ => {}
        }
        if field(F::Type) == 1 {
            set_raw(2, field(F::Color) as u8 as isize);
        }
        if field(F::Type) != 2 {
            set_raw(1, field(F::Team) as u8 as isize);
            set_raw(3, field(F::StartingCitizens) as u8 as isize);
        }
    }

    /// EELobbySetGameOptions: the game's options (map, size, resources, epochs, variant, difficulty, speed, limits,
    /// flags) to the room.
    fn set_game_options(&self, options: isize, game: isize) {
        let field = |f: F| self.op2(Op::OptionsField, options, f as isize);
        let set = |o: isize, v: isize| {
            self.game(game, GC::SetOption, o, v as u32 as isize);
        };
        set(2, field(F::MapTypeIndex));
        set(1, field(F::MapSize));
        set(3, field(F::Resources));
        set(4, field(F::EpochStart));
        set(5, field(F::EpochEnd));
        set(6, field(F::Variant).wrapping_sub(1));
        set(7, field(F::Difficulty));
        let speed = field(F::GameSpeed) as i32;
        if (0..=3).contains(&speed) {
            set(8, speed as isize);
        }
        set(9, field(F::UnitLimit));
        set(10, field(F::WondersForVictory));
        set(11, (field(F::RevealMap) != 0) as isize);
        set(12, (field(F::AllowCustomCivs) != 0) as isize);
        set(13, (field(F::LockTeams) != 0) as isize);
        set(14, (field(F::LockSpeed) != 0) as isize);
        set(15, (field(F::Victory) != 0) as isize);
    }

    // ---- EELobbyInterface

    /// EELobbyFillGameOptions: the game's options from the lobby defaults and the room's settings, then the players.
    fn fill_game_options(&self, my_player: isize) {
        let game = self.op1(Op::MyGame, 0);
        if game == 0 {
            return;
        }
        self.op1(Op::OptionsBegin, 0);
        let set = |f: F, v: isize| {
            self.op2(Op::OptionSet, f as isize, v);
        };
        match self.option(game, 0) {
            0 => set(F::LoadType, 0),
            1 => set(F::LoadType, 2),
            2 => set(F::LoadType, 1),
            _ => {}
        }
        let size = self.option(game, 1);
        if (0..=5).contains(&size) {
            set(F::MapSize, size as isize);
        }
        if self.game(game, GC::HasGameFile, 0, 0) != 0 {
            let name = self.game(game, GC::GetGameFileName, 0, 0);
            self.op1(Op::OptionMapTypeName, name);
        } else {
            self.op1(Op::OptionMapTypeGenerator, self.option(game, 2) as isize);
        }
        set(F::Resources, self.option(game, 3) as isize);
        set(F::EpochStart, self.option(game, 4) as isize);
        set(F::EpochEnd, self.option(game, 5) as isize);
        set(F::Variant, self.option(game, 6).wrapping_add(1) as isize);
        set(F::Difficulty, self.option(game, 7) as isize);
        let speed = self.option(game, 8);
        if (0..=3).contains(&speed) {
            set(F::GameSpeed, speed as isize);
        }
        set(F::UnitLimit, self.option(game, 9) as isize);
        set(F::WondersForVictory, self.option(game, 10) as isize);
        set(F::RevealMap, (self.option(game, 11) != 0) as isize);
        set(F::AllowCustomCivs, (self.option(game, 12) != 0) as isize);
        set(F::LockTeams, (self.option(game, 13) != 0) as isize);
        set(F::LockSpeed, (self.option(game, 14) != 0) as isize);
        set(F::Victory, (self.option(game, 15) != 0) as isize);
        self.fill_players(my_player);
        self.op1(Op::OptionsEnd, 0);
    }

    /// EELobbyFillPlayers: the sixteen game players from the room's slots; the local one copied out.
    fn fill_players(&self, my_player: isize) {
        let game = self.op1(Op::MyGame, 0);
        if game == 0 {
            return;
        }
        let random = self.game(game, GC::HasGameFile, 0, 0) != 0;
        for i in 0..16 {
            self.op1(Op::PlayerNew, 0);
            // GetPlayerSlot: NULL past the eight slots
            self.slot_to_player(game, if i < 8 { Some(i) } else { None });
            self.op2(Op::PlayerSet, F::Slot as isize, i);
            let local = self.game(game, GC::GetMySlot, 0, 0) as i32 == i as i32;
            self.op2(Op::PlayerSet, F::IsLocal as isize, local as isize);
            if local {
                self.op1(Op::PlayerCopyTo, my_player);
            }
            if random {
                let t = self.op1(Op::PlayerType, 0);
                self.op2(Op::PlayerSet, F::LoadedType as isize, t);
            }
            self.op1(Op::PlayerCommit, i);
        }
    }

    /// EELobbySlotToPlayer: a slot as a game player (computer, human or observer; open, closed and none type 4).
    fn slot_to_player(&self, game: isize, slot: Option<isize>) {
        let set = |f: F, v: isize| {
            self.op2(Op::PlayerSet, f as isize, v);
        };
        let Some(s) = slot else {
            set(F::Type, 4);
            return;
        };
        let opt = |o: isize| self.game(game, GC::SlotRawOption, s, o);
        match opt(0) {
            0 | 2 => {
                set(F::Type, 4);
                return;
            }
            1 => set(F::Type, 0),
            3 => set(F::Type, if opt(4) != 0 { 2 } else { 1 }),
            _ => {}
        }
        if opt(0) == 3 {
            self.op1(Op::PlayerSetName, self.game(game, GC::SlotGetName, s, 0));
        }
        set(F::IsReady, 0);
        set(F::B36, 0);
        if self.op1(Op::PlayerType, 0) == 1 {
            set(F::Color, opt(2));
        }
        if self.op1(Op::PlayerType, 0) != 2 {
            set(F::Team, opt(1));
            set(F::StartingCitizens, opt(3));
        }
    }

    /// EELobbySyncNetworkAdapter: the adapter is the local host's address's one-based place (0 not found).
    fn sync_network_adapter(&self) {
        let mut index = 0isize;
        let host = self.op1(Op::LocalHost, 0) as i32;
        if host != 0 {
            let list = self.op1(Op::AddressesGet, 0);
            let n = self.op1(Op::AddressesCount, list);
            for k in 0..n {
                if self.op2(Op::AddressHandle, list, k) as i32 == host {
                    index = k + 1;
                    break;
                }
            }
            self.op1(Op::AddressesFree, list);
        }
        self.op1(Op::SetNetworkAdapter, index);
    }

    /// EELobbyGameDoneEvent::ProcessEvent: the game form's controls hidden; the second time, back to the lobby.
    fn game_done(&self, form: isize, count: *mut i32) {
        for id in GAME_FORM_HIDDEN {
            self.op2(Op::FormHide, form, id);
        }
        // SAFETY: the event's count (the call's contract)
        let c = unsafe {
            *count = (*count).wrapping_add(1);
            *count
        };
        if c == 2 && self.op1(Op::IsLobbyOpen, 0) != 0 {
            self.op1(Op::PostReturnFromGame, 0);
        }
    }

    /// EELobbyRunLoop: the lobby runs until a hosted or joined game connects (the game screen shows) or it is left.
    fn run_loop(&self) {
        if self.closing.load(Ordering::Relaxed) {
            return;
        }
        self.closing.store(true, Ordering::Relaxed);
        self.op1(Op::LocksRelease, 0);
        loop {
            let result = self.op1(Op::LobbyRun, 0) as i32;
            if result == 2 {
                if self.run_connect_dialog() {
                    self.op1(Op::PostStartHostedGame, 0);
                    self.op1(Op::ShowGameScreen, 0);
                    self.op1(Op::LobbySuspend, 0);
                    self.op1(Op::Hide, 0);
                    break;
                }
            } else if result == 1 {
                self.op1(Op::JoinGame, 0);
                if self.run_connect_dialog() {
                    self.op1(Op::ShowGameScreen, 0);
                    self.op1(Op::LobbySuspend, 0);
                    self.op1(Op::Hide, 0);
                    break;
                }
            } else {
                self.shutdown();
                if result == 3 {
                    self.op1(Op::SierraUpdate, 0);
                }
                break;
            }
            self.op1(Op::DeleteCommunications, 0);
        }
        self.game_done.store(false, Ordering::Relaxed);
        self.closing.store(false, Ordering::Relaxed);
        self.op1(Op::LocksRetake, 0);
    }

    /// EELobbyReturnFromGame: the open lobby back after a game (a dissolved game reported), run again.
    fn return_from_game(&self) {
        if self.op1(Op::IsLobbyOpen, 0) == 0 {
            return;
        }
        self.op1(Op::Show, 0);
        let form = self.op1(Op::GameForm, 0);
        if self.op1(Op::CurrentForm, 0) == form {
            self.op1(Op::PopForm, 0);
        }
        self.op1(Op::LobbyResume, 0);
        if self.game_done.load(Ordering::Relaxed) {
            self.op1(Op::BoxDissolved, 0);
        }
        self.run_loop();
    }

    /// One inward call.
    pub fn call(&self, call: EeWonEeGlueCall, a: isize, b: isize, c: isize) -> isize {
        use EeWonEeGlueCall as C;
        match call {
            C::Init => self.init(),
            C::PaintDetails => self.paint_details(a, b),
            C::DetailsHeight => return (self.op1(Op::ListFontHeight, a) as i32).wrapping_mul(9) as isize,
            C::EpochPaint => {
                let game = self.op1(Op::ItemGame, b);
                let t = self.epoch_text(game, true);
                self.draw(a, t, 0, 0);
            }
            C::EpochCompare => return self.compare_epochs(a, b),
            C::EpochKeep => {
                let game = self.op1(Op::ItemGame, a);
                let t = self.epoch_text(game, true);
                return self.keep(t, b) as isize;
            }
            C::MapTypePaint => {
                let game = self.op1(Op::ItemGame, b);
                let t = self.map_type_text(game);
                self.draw(a, t, 0, 0);
            }
            C::MapTypeCompare => {
                let ta = self.map_type_text(self.op1(Op::ItemGame, a));
                let tb = self.map_type_text(self.op1(Op::ItemGame, b));
                return self.op(Op::TextCompareNoCase, ta.0 as isize, ta.1, tb.0 as isize, tb.1, 0, 0) as i32 as isize;
            }
            C::MapTypeKeep => {
                let t = self.map_type_text(self.op1(Op::ItemGame, a));
                return self.keep(t, b) as isize;
            }
            C::GetComponents => self.get_components(a),
            C::Shutdown => self.shutdown(),
            C::FileTransferPort => return self.file_transfer_port(),
            C::FileDialog => return self.file_dialog.load(Ordering::Relaxed),
            C::ConnectDialog => return self.connect_dialog.load(Ordering::Relaxed),
            C::RunConnectDialog => return self.run_connect_dialog() as isize,
            C::SetSlotConnected => {
                let d = self.connect_dialog.load(Ordering::Relaxed);
                if d != 0 {
                    self.dialog(d, DC::ConnectSetConnected, a as u8 as isize, (b != 0) as isize);
                }
            }
            C::CancelConnectDialog => {
                let d = self.connect_dialog.load(Ordering::Relaxed);
                if d != 0 && self.op1(Op::IAmCaptain, 0) == 0 {
                    self.dialog(d, DC::ConnectEnd, 0, 0);
                }
            }
            C::PostConnectStatus => {
                let w = self.op1(Op::LobbyWindow, 0);
                if w != 0 {
                    self.op2(Op::PostSendConnectStatus, w, (a != 0) as isize);
                }
            }
            C::SendConnectStatus => {
                let game = self.op1(Op::MyGame, 0);
                if game != 0 {
                    self.game(game, GC::SendConnectStatus, (a != 0) as isize, 0);
                }
            }
            C::IsConnectDialogRunning => {
                let d = self.connect_dialog.load(Ordering::Relaxed);
                return if d != 0 { (self.dialog(d, DC::Field, 7, 0) != 0) as isize } else { 0 };
            }
            C::SavedGameInfo => {
                let s = self.op1(Op::SaveNew, a);
                let ok = self.saved_game_info(s, b as *mut u32, c as *mut u32);
                self.op1(Op::SaveFree, s);
                return ok as isize;
            }
            C::LoadSavedGame => return self.load_saved_game(a) as isize,
            C::FillMapTypeCombo => {
                // EELobbyFillMapTypeCombo: cleared, then every random map type's name
                self.op1(Op::ComboClear, a);
                let n = self.op1(Op::NumMapTypes, 0) as i32;
                for i in 0..n {
                    self.op2(Op::ComboAddMapType, a, i as isize);
                }
            }
            C::SetDefaultGameOptions => {
                let options = self.op1(Op::DefaultOptions, 0);
                self.set_game_options(options, a);
            }
            C::FillGameOptions => self.fill_game_options(a),
            C::SyncNetworkAdapter => self.sync_network_adapter(),
            C::GameDone => self.game_done(a, b as *mut i32),
            C::RunLoop => self.run_loop(),
            C::ReturnFromGame => self.return_from_game(),
            C::SetQuitFlag => {
                if self.op1(Op::IsShown, 0) == 0 {
                    self.game_done.store(true, Ordering::Relaxed);
                }
            }
            C::Field => {
                return match a {
                    0 => self.file_dialog.load(Ordering::Relaxed),
                    1 => self.connect_dialog.load(Ordering::Relaxed),
                    2 => self.closing.load(Ordering::Relaxed) as isize,
                    3 => self.game_done.load(Ordering::Relaxed) as isize,
                    _ => 0,
                }
            }
            C::Reset => {
                self.file_dialog.store(0, Ordering::Relaxed);
                self.connect_dialog.store(0, Ordering::Relaxed);
                self.closing.store(false, Ordering::Relaxed);
                self.game_done.store(false, Ordering::Relaxed);
            }
        }
        0
    }

    /// EELobbyGetSavedGameInfo: the file's length and its header's checksum, each written where the original
    /// wrote it; false when the file or its header cannot be read.
    fn saved_game_info(&self, s: isize, length: *mut u32, checksum: *mut u32) -> bool {
        if self.op1(Op::SaveOpen, s) == 0 {
            return false;
        }
        let l = self.op1(Op::SaveLength, s) as u32;
        // SAFETY: the caller's outputs (the call's contract)
        unsafe { *length = l };
        self.op1(Op::SaveClose, s);
        if self.op1(Op::SaveLoadHeader, s) == 0 {
            return false;
        }
        let c = self.op1(Op::SaveChecksum, s) as u32;
        // SAFETY: as above
        unsafe { *checksum = c };
        true
    }
}

/// The lobby's timeouts (EELobbyTimeouts.cpp: one global at 0x9659d8, 0x5c bytes, that no code read; its
/// constructor's defaults, its names inferred): three counts and times, a flag set to 1; its two vectors, its name and
/// its other fields started empty or 0.
// no code read the original global either
#[allow(dead_code)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub(crate) struct EeLobbyTimeouts {
    /// +0x00
    pub l00: i32,
    /// +0x04
    pub l04: i32,
    /// +0x08
    pub l08: i32,
    /// +0x1c
    pub s1c: i16,
    /// +0x30
    pub l30: i32,
    /// +0x44
    pub b44: bool,
    /// +0x58
    pub l58: i32,
}

/// EELobbyTimeouts' constructor's values (0x69d022).
#[allow(dead_code)]
pub(crate) const TIMEOUTS: EeLobbyTimeouts =
    EeLobbyTimeouts { l00: 3, l04: 2000, l08: 45000, s1c: 1, l30: 0, b44: false, l58: 0 };

/// Sets the glue's host (the C++ calls it once, before any other call).
#[no_mangle]
pub extern "C" fn ee_won_eeglue_set_host(host: EeWonEeGlueHostFn) {
    GLUE.host.store(host.map_or(0, |f| f as usize), Ordering::Relaxed);
}

/// One inward call (`EeWonEeGlueCall`).
///
/// # Safety
/// The call's pointer arguments as documented.
#[no_mangle]
pub unsafe extern "C" fn ee_won_eeglue_call(call: EeWonEeGlueCall, a: isize, b: isize, c: isize) -> isize {
    GLUE.call(call, a, b, c)
}

#[cfg(test)]
#[path = "eeglue_tests.rs"]
mod tests;
