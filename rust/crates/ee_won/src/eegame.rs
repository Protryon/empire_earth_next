//! The Empire Earth lobby game as a Rust object (R6 WONLobby round 5): EELobbyGame (the sixteen game options and
//! their wire types, the eight player slots, the saved game's file transfer, the game room's messages 0x64..0x6a, the
//! join request and reply, the summary and details of the game list) and its slots, EELobbyPlayerSlot (the six slot
//! options, the slot's client, player and reserved name). Every method's statements are the originals' in order; the
//! state (options, slots, counts, the transfer, the details' player names) lives here.
//!
//! The C++ keeps the class EELobbyGame (a `LobbyGame`: its virtual hooks forward here, so the lobby's game code
//! did not change) and EELobbyPlayerSlot as a two-member view (this object and the slot's number) that callers keep
//! using. Every call on a buffer, a client, a player, the file transfer, the lobby or a global is one op of the host
//! given to `ee_won_eegame_new` (`EELobbyGameHost.cpp`), the original's statements with no decision between them.
//! A `SmartPtr` member is a pointer here with the original's reference traffic (`AddRef`/`Release` ops); a slot's
//! `GUIString` name is its `GUIStringData` pointer, 0 for the shared empty string (whose references nobody sees).
//!
//! A read past a message's end threw `ReadBufferException` in the original. A read op answers `isize::MIN` (the
//! C++ caught it at the op); the method stops there with the temporaries it held released, as the unwinding did,
//! and the inward call answers `isize::MIN` so the C++ shell throws again for the lobby's handler.
//!
//! Re-entry: the lobby's events (a slot's or an option's sync, the staging change) and the saved game's default
//! options come back into this object: the state is in `Cell`s behind a shared reference, re-read after every call
//! out, and nothing is borrowed across an op.

use std::cell::{Cell, RefCell};
use std::ffi::c_void;
use std::sync::atomic::{AtomicBool, AtomicU32, Ordering};

/// The host's ops. `a`..`d` as documented; a read answers `isize::MIN` when it threw.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonEeGameOp {
    /// `a->AddRef()` (a `SmartPtr`'s reference: a client, a player, the file transfer, a `GUIStringData`).
    AddRef = 1,
    /// `a->Release()`.
    Release,
    /// `((ReadBuffer*)a)->ReadByte()` (0..255).
    ReadByte,
    /// `ReadShort()`, sign-extended.
    ReadShort,
    /// `ReadBool()` (0 or 1).
    ReadBool,
    /// `ReadLong()`, sign-extended.
    ReadLong,
    /// `ReadWString(text, 2)` as a new `GUIStringData` with one reference (the caller's).
    ReadWString,
    /// `Available()`.
    Available,
    /// `((WriteBuffer*)a)->AppendByte(b)`.
    AppendByte,
    /// `AppendShort(b)`.
    AppendShort,
    /// `AppendBool(b != 0)`.
    AppendBool,
    /// `AppendLong(b)`.
    AppendLong,
    /// `AppendWString(GUIString(b), 2)` (`b` a `GUIStringData`, 0 the empty string).
    AppendWString,
    /// `length()`.
    Length,
    /// `SetByte(b, c)`.
    SetByte,
    /// `new WriteBuffer(0, true)` (a game room message).
    NewBuffer,
    /// `LobbyBroadcastToGame(a->ToByteBuffer(true).get())`.
    BroadcastToGame,
    /// `LobbySendToCaptain(a->ToByteBuffer(true).get())`.
    SendToCaptain,
    /// `LobbySendToClient(b, a->ToByteBuffer(true).get())`.
    SendToClient,
    /// `delete a` (the message buffer's end of scope).
    FreeBuffer,
    /// `this->GetIsHost()`.
    IsHost,
    /// `this->GetMaxPlayers()`.
    MaxPlayers,
    /// `this->SetMaxPlayers(a)`.
    SetMaxPlayers,
    /// `this->GetGameType()`.
    GameType,
    /// `this->GetClients().mPtr` (a `LobbyClientList`).
    Clients,
    /// `((LobbyClientList*)a)->FindClient(b)`.
    FindClient,
    /// `LobbyGetCurrentServer()`.
    CurrentServer,
    /// `((LobbyServer*)a)->GetClient(b)`.
    ServerClient,
    /// `new LobbyClientSetIterator(a)`.
    IterNew,
    /// `a->HasMore()`.
    IterHasMore,
    /// `a->Next(&client)`: the client.
    IterNext,
    /// `delete a` (the iterator's end of scope).
    IterFree,
    /// `((LobbyClient*)a)->GetName()`'s `GUIStringData` (no reference added).
    ClientName,
    /// `GetClientId()`.
    ClientId,
    /// `GetPlayer().mPtr` (an `EELobbyPlayer`).
    ClientPlayer,
    /// `IsCaptain(1)`.
    ClientIsCaptain,
    /// `((EELobbyPlayer*)a)->GetSlot()`.
    PlayerSlot,
    /// `GetTeam()` (its slot's team, through that slot's game).
    PlayerTeam,
    /// `a->mPlayerSlot = &this->mSlots[b]; a->mSlot = b` (EELobbyPlayerSlot::SetPlayer's two stores).
    PlayerAttach,
    /// `a->mPlayerSlot = NULL` (ClearPlayer's store).
    PlayerDetach,
    /// `GetMyLobbyPlayer()`.
    MyPlayer,
    /// `LobbyIAmCaptain()`.
    IAmCaptain,
    /// `LobbyNotifyStagingChange()`.
    NotifyStagingChange,
    /// `LobbyNetAdvertiseGame()`.
    AdvertiseGame,
    /// `LobbyResetReadyFlags()`.
    ResetReadyFlags,
    /// `LobbyBroadcastEvent(new EELobbySyncPlayerEvent(a), true)`.
    SyncPlayer,
    /// `LobbyBroadcastEvent(new EELobbySyncOptionEvent(a), true)`.
    SyncOption,
    /// `LobbyBroadcastEvent(new PlayerTeamChangedEvent(a, b != 0), true)`.
    TeamChanged,
    /// `LobbyBroadcastLobbyEvent(a, true)`.
    LobbyEvent,
    /// `LobbyKickClient(a, false)`.
    KickClient,
    /// `LobbyMessageBox(StartGameError, EveryoneOnSameTeam)`.
    SameTeamBox,
    /// `EELobbyHostGame()`.
    HostGame,
    /// `EELobbyCancelConnectDialog()`.
    CancelConnectDialog,
    /// `EELobbySetSlotConnected(a, b != 0)`.
    SetSlotConnected,
    /// `EELobbyIsConnectDialogRunning()`.
    ConnectDialogRunning,
    /// `EELobbyGetFileTransferPort()`.
    FileTransferPort,
    /// `EELobbyGetNumMapTypes()`.
    NumMapTypes,
    /// `EELobbySetDefaultGameOptions(this)`.
    SetDefaultOptions,
    /// `EELobbyGetVersion(version, *(uint32_t*)a)`: the version as a new `GUIStringData` with one reference.
    Version,
    /// `GUIString(a) == GUIString(b)` (0: the empty string).
    StringsEqual,
    /// `new EELobbyFileTransfer` (no reference: the `SmartPtr` assignment follows).
    TransferNew,
    /// `a->SetFileInfo(GUIString(b), c, d)`.
    TransferSetFileInfo,
    /// `a->SetPath(EELobbyFn2988E9() + a->mName)` (the saved games' directory and the file's name).
    TransferSetSavesPath,
    /// `a->Serve(b)`.
    TransferServe,
    /// `a->mName`'s `GUIStringData` (no reference).
    TransferName,
    /// `a->mSize`.
    TransferSize,
    /// `a->mChecksum`.
    TransferChecksum,
    /// `a->SetHost(this->GetAddress() with port b)`.
    TransferSetHost,
    /// `a->Stop()`.
    TransferStop,
    /// `a->CloseClients()`.
    TransferCloseClients,
    /// `a->IsFileHere()`.
    TransferIsFileHere,
    /// `a->Download()`.
    TransferDownload,
}

/// The host: `(game, op, a, b, c, d) -> result`.
pub type EeWonEeGameHostFn =
    Option<unsafe extern "C" fn(game: *mut c_void, op: EeWonEeGameOp, a: isize, b: isize, c: isize, d: isize) -> isize>;

/// The inward calls: EELobbyGame's methods and virtual hooks, then EELobbyPlayerSlot's (`a` the slot's number).
/// `a`..`d` are the method's arguments in order (pointers, numbers); a hook whose read threw answers `isize::MIN`.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonEeGameCall {
    GetNumFiles = 1,
    /// `GetFile(a)`: its `GUIStringData`, 0 the empty string.
    GetFile,
    /// `SetGameFile(GUIString(a), b, c, d != 0)`.
    SetGameFile,
    /// `GetGameFileName()`'s `GUIStringData` (0 without a file transfer).
    GetGameFileName,
    StopTransfer,
    /// `UpdateTransferProgress(a, b)`.
    UpdateTransferProgress,
    SetTransferPercent,
    StartTransfer,
    CanReadyHook,
    CanStartGameHook,
    TryStartGameHook,
    PostNetLeaveGameHook,
    NotifyStartGameHook,
    /// `HandleHostGameHook(a)`.
    HandleHostGameHook,
    /// `GetJoinGameRequestHook(*a)`.
    GetJoinGameRequestHook,
    /// `HandleJoinGameRequestHook(*a, b)`.
    HandleJoinGameRequestHook,
    /// `GetJoinGameReplyHook(a, *b)`.
    GetJoinGameReplyHook,
    HandleJoinGameReplyHook,
    /// `HandlePlayerLeftHook(a, b)`.
    HandlePlayerLeftHook,
    HandlePlayerJoinedHook,
    WriteSummaryHook,
    WriteDetailsHook,
    ReadSummaryHook,
    ReadDetailsHook,
    /// `HandleGameMessageHook(a, *b, c)`.
    HandleGameMessageHook,
    /// `GetStatusStringHook(a)`: 0 the empty string, 1 not in the saved game, 2 invalid version, 3 corrupt binaries.
    GetStatusStringHook,
    ResetSlots,
    CountPlayers,
    HasGameFile,
    GetOption,
    /// `SetOption(a, b)`.
    SetOption,
    /// `ChangeOption(a, b, c != 0)`.
    ChangeOption,
    SendGameState,
    /// `ChangePlayerOption(a, b, c)`.
    ChangePlayerOption,
    SendConnectStatus,
    SendCancelConnect,
    SendOption,
    /// `GetFreeColor(a, b)`.
    GetFreeColor,
    MakeColorsDistinct,
    GetMySlot,
    GetMyTeam,
    /// `GetTakenColors((bool*)a, b)` (16 entries).
    GetTakenColors,
    IsColorFree,
    /// a member: `a` 0 the file transfer, 1 the file is here, 2 human players, 3 computer players, 4 reserved slots.
    Field,
    /// EELobbyPlayerSlot::Reset (`a` the slot).
    SlotReset,
    /// `GetName()`'s `GUIStringData` (0 the empty string).
    SlotGetName,
    /// `SetState(b)`.
    SlotSetState,
    /// `GetOption(b)`.
    SlotGetOption,
    /// `SetOption(b, c)`.
    SlotSetOption,
    SlotClearPlayer,
    SlotSetOpen,
    SlotSetClosed,
    SlotSetComputer,
    /// `SetName(GUIString(b))`.
    SlotSetName,
    /// `SetPlayer(b, c)`.
    SlotSetPlayer,
    /// `mOptions[b]` (0 past the six).
    SlotRawOption,
    /// `mOptions[b] = c` (nothing past the six).
    SlotSetRawOption,
    /// `mClient.mPtr`.
    SlotClient,
    /// `mPlayer.mPtr`.
    SlotPlayer,
}

/// What a read that threw answers (`ReadBufferException` caught at the op).
const THREW: isize = isize::MIN;

struct Threw;
type R<T> = Result<T, Threw>;

/// Each option's wire type (0 bool, 1 byte, 2 short): EELobbyInitOptionTypes' table.
const OPTION_TYPES: [i32; 16] = [1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 1, 0, 0, 0, 0, 0];
/// Each option's limit (SetOption keeps a value below it); option 2's is the number of map types, read once.
const OPTION_LIMITS: [u32; 16] = [3, 6, 0, 5, 16, 16, 2, 3, 4, 1201, 7, 2, 2, 2, 2, 2];
/// HandleHostGameHook's options before the saved preferences.
const HOST_OPTIONS: [i32; 16] = [0, 1, 0, 0, 0, 0xe, 1, 1, 1, 0x1f4, 3, 0, 1, 0, 0, 1];

/// `sOptionTypesInitialized` and `sOptionLimits[2]`: set by the first game made, for the process.
static TYPES_INITIALIZED: AtomicBool = AtomicBool::new(false);
static MAP_TYPE_LIMIT: AtomicU32 = AtomicU32::new(0);

/// The wire type of an option; -1 past the table (the original read past its 16 entries; nothing is read or
/// written for such an option here).
fn option_type(option: i32) -> i32 {
    if (0..16).contains(&option) {
        OPTION_TYPES[option as usize]
    } else {
        -1
    }
}

fn option_limit(option: usize) -> u32 {
    if option == 2 {
        MAP_TYPE_LIMIT.load(Ordering::Relaxed)
    } else {
        OPTION_LIMITS[option]
    }
}

/// x86's `cvttsd2si`: the truncated value, `i32::MIN` for NaN and out of range (the original's `(int)` of a double).
fn trunc_i32(v: f64) -> i32 {
    if v.is_nan() || v >= 2147483648.0 || v <= -2147483649.0 {
        i32::MIN
    } else {
        v as i32
    }
}

/// The game options' `GetStatusStringHook` table: 0 the empty string, 1..3 the refusal texts.
pub fn status_string(status: i16) -> i32 {
    match status {
        10000 => 1,
        10001 => 2,
        10002 => 3,
        _ => 0,
    }
}

/// One inward call on the Rust game `handle` (an `EELobbyGame`'s `mRust`; 0 answers 0) from another Rust object:
/// the direct path that replaced the C++ forwarders (R6 WONLobby round 6; `EELobbyGame::GetOption` and the others
/// were `ee_won_eegame_call`). The reference lives for this call only: the game's calls out may re-enter.
pub fn game_call(handle: isize, call: EeWonEeGameCall, a: isize, b: isize, c: isize) -> isize {
    game_call4(handle, call, a, b, c, 0)
}

/// `game_call` with the fourth argument (SetGameFile's flag).
pub fn game_call4(handle: isize, call: EeWonEeGameCall, a: isize, b: isize, c: isize, d: isize) -> isize {
    let p = handle as *const EeWonEeGame;
    if p.is_null() {
        return 0;
    }
    // SAFETY: a live game's object (the caller's C++ game holds it); shared, as `ee_won_eegame_call`
    unsafe { (*p).call(call, a, b, c, d) }
}

/// EELobbyPlayer::IsTeammate: team 0 (no team) has no teammates.
pub fn is_teammate(mine: u8, theirs: u8) -> bool {
    mine != 0 && mine == theirs
}

/// One player slot (EELobbyPlayerSlot). Options: 0 state (0 open, 1 computer, 2 closed, 3 a client's, 4 reserved
/// by name), 1 team, 2 colour, 3 citizen, 4 observe, 5 file transfer percent.
struct Slot {
    index: u8,
    options: [Cell<u8>; 6],
    client: Cell<isize>,
    player: Cell<isize>,
    name: Cell<isize>,
}

impl Slot {
    fn new(index: u8) -> Self {
        let s =
            Slot { index, options: Default::default(), client: Cell::new(0), player: Cell::new(0), name: Cell::new(0) };
        s.reset();
        s
    }

    fn opt(&self, i: usize) -> u8 {
        self.options[i].get()
    }

    fn set_opt(&self, i: usize, v: u8) {
        self.options[i].set(v);
    }

    fn reset(&self) {
        self.set_opt(1, 0);
        self.set_opt(2, 0);
        self.set_opt(3, 5);
        self.set_opt(4, 0);
        self.set_opt(5, 100);
    }

    fn get_option(&self, option: i32) -> u8 {
        if (0..6).contains(&option) {
            self.opt(option as usize)
        } else {
            0
        }
    }
}

/// The lobby game's Rust half.
pub struct EeWonEeGame {
    game: *mut c_void,
    host: EeWonEeGameHostFn,
    /// the model's ops answered by its Rust objects (modelobj.rs; the game's), not the host
    model: Cell<bool>,
    /// `mFiles`: the details' player names (`GUIStringData`s, a reference each)
    files: RefCell<Vec<isize>>,
    /// `mFileTransfer`
    transfer: Cell<isize>,
    /// `mHasFile`
    has_file: Cell<bool>,
    num_human: Cell<u8>,
    num_computer: Cell<u8>,
    num_reserved: Cell<u8>,
    options: [Cell<i32>; 16],
    slots: [Slot; 8],
}

impl EeWonEeGame {
    /// EELobbyGame's constructor: the option types once for the process, every option 0, the slots numbered.
    pub fn new(game: *mut c_void, host: EeWonEeGameHostFn) -> Self {
        let g = Self {
            game,
            host,
            model: Cell::new(false),
            files: RefCell::new(Vec::new()),
            transfer: Cell::new(0),
            has_file: Cell::new(false),
            num_human: Cell::new(0),
            num_computer: Cell::new(0),
            num_reserved: Cell::new(0),
            options: Default::default(),
            slots: [0u8, 1, 2, 3, 4, 5, 6, 7].map(Slot::new),
        };
        if !TYPES_INITIALIZED.swap(true, Ordering::Relaxed) {
            MAP_TYPE_LIMIT.store(g.op(Op::NumMapTypes, 0, 0, 0, 0) as u32, Ordering::Relaxed);
        }
        g
    }

    fn op(&self, op: Op, a: isize, b: isize, c: isize, d: isize) -> isize {
        if self.model.get() {
            if let Some(r) = crate::modelobj::eegame_op(op, self.game as isize, a, b) {
                return r;
            }
        }
        match self.host {
            // SAFETY: the host is the C++ game's (or a test's), called with the game it was given
            Some(f) => unsafe { f(self.game, op, a, b, c, d) },
            None => 0,
        }
    }

    fn op1(&self, op: Op, a: isize) -> isize {
        self.op(op, a, 0, 0, 0)
    }

    fn ask(&self, op: Op) -> bool {
        self.op(op, 0, 0, 0, 0) != 0
    }

    fn read(&self, op: Op, buffer: isize) -> R<isize> {
        let r = self.op1(op, buffer);
        if r == THREW {
            Err(Threw)
        } else {
            Ok(r)
        }
    }

    fn read_byte(&self, buffer: isize) -> R<u8> {
        Ok(self.read(Op::ReadByte, buffer)? as u8)
    }

    fn append(&self, op: Op, buffer: isize, value: isize) {
        self.op(op, buffer, value, 0, 0);
    }

    fn release(&self, p: isize) {
        if p != 0 {
            self.op1(Op::Release, p);
        }
    }

    /// A `SmartPtr`'s assignment (`ConstSmartPtr::operator=`): nothing for the same pointer, else the old one
    /// released, the new one referenced.
    fn assign(&self, member: &Cell<isize>, new: isize) {
        let old = member.get();
        if old == new {
            return;
        }
        if old != 0 {
            self.op1(Op::Release, old);
        }
        member.set(new);
        if new != 0 {
            self.op1(Op::AddRef, new);
        }
    }

    fn opt(&self, i: usize) -> i32 {
        self.options[i].get()
    }

    fn slot(&self, slot: i32) -> Option<&Slot> {
        if (0..8).contains(&slot) {
            Some(&self.slots[slot as usize])
        } else {
            None
        }
    }

    // ---- EELobbyPlayerSlot

    fn slot_get_name(&self, s: &Slot) -> isize {
        let client = s.client.get();
        if client == 0 {
            if s.opt(0) == 4 {
                return s.name.get();
            }
            return 0;
        }
        self.op1(Op::ClientName, client)
    }

    fn slot_set_state(&self, s: &Slot, state: u8) {
        s.set_opt(0, state);
        if state != 3 {
            self.slot_clear_player(s);
        }
        if state != 4 {
            self.assign(&s.name, 0);
        }
    }

    /// SetOption: option 0 changes the state (an invalid state is ignored). The original wrote past the six
    /// options for a larger number (undefined); nothing is written here.
    fn slot_set_option(&self, s: &Slot, option: i32, value: u8) {
        if option == 0 {
            if value < 5 {
                self.slot_set_state(s, value);
            }
        } else if (1..6).contains(&option) {
            s.set_opt(option as usize, value);
        }
    }

    fn slot_clear_player(&self, s: &Slot) {
        let player = s.player.get();
        if player != 0 {
            self.op1(Op::PlayerDetach, player);
            self.assign(&s.player, 0);
        }
        self.assign(&s.client, 0);
    }

    fn slot_set_name(&self, s: &Slot, name: isize) {
        self.slot_set_state(s, 4);
        self.assign(&s.name, name);
    }

    fn slot_set_player(&self, s: &Slot, client: isize, player: isize) {
        self.slot_clear_player(s);
        if client != 0 && player != 0 {
            self.assign(&s.client, client);
            self.assign(&s.player, player);
            self.op(Op::PlayerAttach, s.player.get(), s.index as isize, 0, 0);
            self.slot_set_state(s, 3);
        }
    }

    fn slot_read_data(&self, s: &Slot, buffer: isize) -> R<()> {
        for i in 0..6 {
            s.set_opt(i, self.read_byte(buffer)?);
        }
        if s.opt(0) != 3 {
            self.slot_clear_player(s);
        }
        Ok(())
    }

    fn slot_write_data(&self, s: &Slot, buffer: isize) {
        for i in 0..6 {
            self.append(Op::AppendByte, buffer, s.opt(i) as isize);
        }
    }

    // ---- EELobbyGame

    fn get_file(&self, index: i32) -> isize {
        let files = self.files.borrow();
        if index >= 0 && (index as usize) < files.len() {
            files[index as usize]
        } else {
            0
        }
    }

    fn set_game_file(&self, name: isize, size: isize, checksum: isize, flag: bool) {
        if self.transfer.get() == 0 {
            let t = self.op1(Op::TransferNew, 0);
            self.assign(&self.transfer, t);
        }
        self.op(Op::TransferSetFileInfo, self.transfer.get(), name, size, checksum);
        self.op1(Op::TransferSetSavesPath, self.transfer.get());
        if self.ask(Op::IsHost) && flag {
            let port = self.op1(Op::FileTransferPort, 0);
            self.op(Op::TransferServe, self.transfer.get(), port, 0, 0);
        }
    }

    fn game_file_name(&self) -> isize {
        let t = self.transfer.get();
        if t != 0 {
            return self.op1(Op::TransferName, t);
        }
        0
    }

    fn write_game_file(&self, buffer: isize) {
        let name = self.game_file_name();
        self.append(Op::AppendWString, buffer, name);
        let size = self.op1(Op::TransferSize, self.transfer.get());
        self.append(Op::AppendLong, buffer, size);
        let checksum = self.op1(Op::TransferChecksum, self.transfer.get());
        self.append(Op::AppendLong, buffer, checksum);
        let port = self.op1(Op::FileTransferPort, 0);
        self.append(Op::AppendShort, buffer, port);
    }

    fn read_game_file(&self, buffer: isize) -> R<()> {
        let name = self.read(Op::ReadWString, buffer)?;
        let r = (|| {
            let size = self.read(Op::ReadLong, buffer)? as u32;
            let checksum = self.read(Op::ReadLong, buffer)? as u32;
            let port = self.read(Op::ReadShort, buffer)? as u16;
            self.set_game_file(name, size as isize, checksum as isize, false);
            self.op(Op::TransferSetHost, self.transfer.get(), port as isize, 0, 0);
            Ok(())
        })();
        // the name's wide string, at the end of its scope (or the unwinding)
        self.release(name);
        r
    }

    fn stop_transfer(&self) {
        let t = self.transfer.get();
        if t != 0 {
            self.op1(Op::TransferStop, t);
        }
    }

    fn update_transfer_progress(&self, done: i32, total: i32) {
        if done == total {
            self.has_file.set(true);
            self.set_transfer_percent(100);
            self.op1(Op::NotifyStagingChange, 0);
        } else {
            let percent = trunc_i32(done as f64 / total as f64 * 100.0);
            self.set_transfer_percent(percent as u8);
        }
    }

    fn set_transfer_percent(&self, percent: u8) {
        self.change_player_option(self.my_slot(), 5, percent);
    }

    fn start_transfer(&self) {
        if self.ask(Op::IsHost) {
            return;
        }
        self.op1(Op::TransferCloseClients, self.transfer.get());
        self.op1(Op::TransferSetSavesPath, self.transfer.get());
        let here = self.op1(Op::TransferIsFileHere, self.transfer.get()) != 0;
        self.has_file.set(here);
        self.op1(Op::NotifyStagingChange, 0);
        if !self.has_file.get() {
            self.set_transfer_percent(0);
            self.op1(Op::TransferDownload, self.transfer.get());
        } else {
            self.set_transfer_percent(100);
        }
    }

    fn can_ready(&self) -> bool {
        !(self.has_game_file() && !self.has_file.get())
    }

    fn can_start(&self) -> bool {
        let human = self.num_human.get() as i32;
        human > 0 && human + self.num_computer.get() as i32 > 1 && self.num_reserved.get() == 0
    }

    /// TryStartGameHook: refused (with a message) when every playing slot is on one team other than 0.
    fn try_start(&self) -> bool {
        let mut different = false;
        let mut team = 0xffu8;
        let mut i = 0;
        while i < 8 && !different {
            let s = &self.slots[i];
            let state = s.opt(0);
            if (state == 3 && s.opt(4) == 0) || state == 1 {
                if team == 0xff {
                    team = s.opt(1);
                } else {
                    different = team != s.opt(1);
                }
            }
            i += 1;
        }
        if !different && team != 0 {
            self.op1(Op::SameTeamBox, 0);
            return false;
        }
        self.stop_transfer();
        self.ask(Op::HostGame)
    }

    fn reset_slots(&self) {
        self.num_computer.set(0);
        self.num_reserved.set(0);
        self.op1(Op::SetMaxPlayers, 8);
        self.num_human.set(1);
        for s in &self.slots {
            self.slot_set_state(s, 0);
            s.set_opt(1, 0);
            s.set_opt(3, 5);
        }
    }

    fn count_players(&self, notify: bool) {
        if !self.ask(Op::IAmCaptain) {
            return;
        }
        let mut open = 8;
        self.num_human.set(0);
        self.num_computer.set(0);
        self.num_reserved.set(0);
        for s in &self.slots {
            let state = s.opt(0);
            if state == 1 || state == 2 {
                open -= 1;
            }
            if state == 3 && s.opt(4) == 0 {
                self.num_human.set(self.num_human.get().wrapping_add(1));
            }
            if state == 1 {
                self.num_computer.set(self.num_computer.get().wrapping_add(1));
            }
            if state == 4 && s.opt(4) == 0 {
                self.num_reserved.set(self.num_reserved.get().wrapping_add(1));
            }
        }
        if open != self.op1(Op::MaxPlayers, 0) {
            self.op1(Op::SetMaxPlayers, open);
            if notify {
                self.op1(Op::AdvertiseGame, 0);
            }
        }
        self.op1(Op::NotifyStagingChange, 0);
    }

    fn has_game_file(&self) -> bool {
        self.get_option(0) != 0
    }

    fn get_option(&self, option: i32) -> i32 {
        if (0..16).contains(&option) {
            self.opt(option as usize)
        } else {
            0
        }
    }

    fn write_option(&self, buffer: isize, option: i32) {
        match option_type(option) {
            2 => self.append(Op::AppendShort, buffer, self.opt(option as usize) as i16 as isize),
            1 => self.append(Op::AppendByte, buffer, self.opt(option as usize) as u8 as isize),
            0 => self.append(Op::AppendBool, buffer, (self.opt(option as usize) != 0) as isize),
            _ => {}
        }
    }

    fn read_option(&self, buffer: isize, option: i32) -> R<()> {
        let value: i32 = match option_type(option) {
            2 => self.read(Op::ReadShort, buffer)? as i16 as i32,
            1 => self.read_byte(buffer)? as i8 as i32,
            0 => (self.read(Op::ReadBool, buffer)? != 0) as i32,
            _ => return Ok(()),
        };
        self.set_option(option, value as u32);
        Ok(())
    }

    fn handle_host_game(&self, client: isize) {
        let t = self.op1(Op::TransferNew, 0);
        self.assign(&self.transfer, t);
        for (o, v) in self.options.iter().zip(HOST_OPTIONS) {
            o.set(v);
        }
        self.op1(Op::SetDefaultOptions, 0);
        self.reset_slots();
        let player = self.op1(Op::ClientPlayer, client);
        self.slot_set_player(&self.slots[0], client, player);
    }

    fn taken_colors(&self, except: i32) -> [bool; 16] {
        let mut taken = [false; 16];
        for (i, s) in self.slots.iter().enumerate() {
            if i as i32 != except {
                let state = s.opt(0);
                if (state == 3 || state == 4) && s.opt(4) == 0 && s.opt(2) < 16 {
                    taken[s.opt(2) as usize] = true;
                }
            }
        }
        taken
    }

    fn is_color_free(&self, color: u8) -> bool {
        let taken = self.taken_colors(-1);
        if color < 16 {
            return !taken[color as usize];
        }
        false
    }

    fn write_summary(&self, buffer: isize) {
        for option in [4, 0, 2] {
            let v = self.get_option(option);
            self.append(Op::AppendByte, buffer, v as u8 as isize);
        }
    }

    fn read_summary(&self, buffer: isize) -> R<bool> {
        for option in [4, 0, 2] {
            let v = self.read_byte(buffer)? as i8 as i32;
            self.set_option(option, v as u32);
        }
        Ok(true)
    }

    fn write_details(&self, buffer: isize) {
        for i in 0..16 {
            self.write_option(buffer, i);
        }
        let mut count = 0u8;
        let pos = self.op1(Op::Length, buffer);
        self.append(Op::AppendByte, buffer, 0);
        for s in &self.slots {
            let client = s.client.get();
            if client != 0 {
                count = count.wrapping_add(1);
                if self.op1(Op::GameType, 0) == 2 {
                    let id = self.op1(Op::ClientId, s.client.get());
                    self.append(Op::AppendShort, buffer, id);
                } else {
                    let name = self.op1(Op::ClientName, s.client.get());
                    self.append(Op::AppendWString, buffer, name);
                }
            }
        }
        self.op(Op::SetByte, buffer, pos, count as isize, 0);
    }

    fn read_details(&self, buffer: isize) -> R<bool> {
        let old = std::mem::take(&mut *self.files.borrow_mut());
        for name in old {
            self.release(name);
        }
        for i in 0..16 {
            self.read_option(buffer, i)?;
        }
        if self.op1(Op::Available, buffer) as i32 > 0 {
            let count = self.read_byte(buffer)?;
            let server = self.op1(Op::CurrentServer, 0);
            if server == 0 && self.op1(Op::GameType, 0) == 2 {
                return Ok(true);
            }
            for _ in 0..count {
                if self.op1(Op::GameType, 0) == 2 {
                    let id = self.read(Op::ReadShort, buffer)? as u16;
                    let client = self.op(Op::ServerClient, server, id as isize, 0, 0);
                    if client != 0 {
                        let name = self.op1(Op::ClientName, client);
                        if name != 0 {
                            self.op1(Op::AddRef, name);
                        }
                        self.files.borrow_mut().push(name);
                    }
                } else {
                    let name = self.read(Op::ReadWString, buffer)?;
                    self.files.borrow_mut().push(name);
                }
            }
        }
        Ok(true)
    }

    fn get_free_color(&self, except: i32, color: u8) -> u8 {
        let taken = self.taken_colors(except);
        if color < 16 && !taken[color as usize] {
            return color;
        }
        taken.iter().position(|&t| !t).map_or(0, |i| i as u8)
    }

    fn make_colors_distinct(&self) {
        for (i, s) in self.slots.iter().enumerate() {
            let state = s.opt(0);
            if (state == 3 || state == 4) && s.opt(4) == 0 {
                let c = self.get_free_color(i as i32, s.opt(2));
                s.set_opt(2, c);
            }
        }
    }

    fn join_request(&self, buffer: isize) {
        let mut checksum: u32 = 0;
        let version = self.op1(Op::Version, &mut checksum as *mut u32 as isize);
        self.append(Op::AppendWString, buffer, version);
        self.append(Op::AppendLong, buffer, checksum as i32 as isize);
        self.release(version);
    }

    /// HandleJoinGameRequestHook: 10001 another version, 10002 another checksum, 10000 not in the saved game, 2 no
    /// open slot, else 0 with the client seated.
    fn handle_join_request(&self, buffer: isize, client: isize) -> R<i16> {
        let mut checksum: u32 = 0;
        let version = self.op1(Op::Version, &mut checksum as *mut u32 as isize);
        let text = match self.read(Op::ReadWString, buffer) {
            Ok(t) => t,
            Err(e) => {
                self.release(version);
                return Err(e);
            }
        };
        let r = self.join_request_checked(buffer, client, version, text, checksum);
        // the wide string, then the version, at the end of their scope (or the unwinding)
        self.release(text);
        self.release(version);
        r
    }

    fn join_request_checked(&self, buffer: isize, client: isize, version: isize, text: isize, checksum: u32) -> R<i16> {
        if self.op(Op::StringsEqual, version, text, 0, 0) == 0 {
            return Ok(10001);
        }
        if self.read(Op::ReadLong, buffer)? as u32 != checksum {
            return Ok(10002);
        }
        let saved = self.has_game_file();
        let mut i = 0;
        while i < 8 {
            let s = &self.slots[i];
            if !saved {
                if s.opt(0) == 0 {
                    break;
                }
            } else if s.opt(0) == 4 {
                let name = self.op1(Op::ClientName, client);
                let slot_name = self.slot_get_name(s);
                if self.op(Op::StringsEqual, name, slot_name, 0, 0) != 0 {
                    break;
                }
            }
            i += 1;
        }
        if i >= 8 {
            return Ok(if saved { 10000 } else { 2 });
        }
        let player = self.op1(Op::ClientPlayer, client);
        self.slot_set_player(&self.slots[i], client, player);
        if !saved {
            self.slots[i].reset();
        }
        Ok(0)
    }

    fn join_reply(&self, buffer: isize) {
        for i in 0..16 {
            self.write_option(buffer, i);
        }
        for s in &self.slots {
            self.slot_write_data(s, buffer);
        }
        if self.has_game_file() {
            self.write_game_file(buffer);
        }
    }

    fn seat_client(&self, client: isize) -> i32 {
        if client != 0 {
            let player = self.op1(Op::ClientPlayer, client);
            if player != 0 {
                let slot = self.op1(Op::PlayerSlot, player) as u8;
                if slot < 8 {
                    self.slot_set_player(&self.slots[slot as usize], client, player);
                    return slot as i32;
                }
            }
        }
        -1
    }

    fn handle_join_reply(&self, buffer: isize) -> R<()> {
        let t = self.op1(Op::TransferNew, 0);
        self.assign(&self.transfer, t);
        for i in 0..16 {
            self.read_option(buffer, i)?;
        }
        for s in &self.slots {
            self.slot_read_data(s, buffer)?;
        }
        if self.has_game_file() {
            self.read_game_file(buffer)?;
        }
        let clients = self.op1(Op::Clients, 0);
        let iter = self.op1(Op::IterNew, clients);
        while self.op1(Op::IterHasMore, iter) != 0 {
            let client = self.op1(Op::IterNext, iter);
            self.seat_client(client);
        }
        self.op1(Op::IterFree, iter);
        if let Some(s) = self.slot(self.my_slot()) {
            let c = self.get_free_color(s.index as i32, 0);
            s.set_opt(2, c);
        }
        if self.has_game_file() {
            self.start_transfer();
        }
        Ok(())
    }

    fn handle_player_joined(&self, client: isize) {
        let slot = self.seat_client(client);
        if slot < 0 {
            return;
        }
        if let Some(s) = self.slot(slot) {
            if !self.has_game_file() {
                s.reset();
                let c = self.get_free_color(s.index as i32, 0);
                s.set_opt(2, c);
            }
        }
        self.count_players(false);
        self.op1(Op::SyncPlayer, slot as isize);
    }

    fn handle_player_left(&self, client: isize, player: isize) {
        let slot = self.op1(Op::PlayerSlot, player) as u8;
        if slot >= 8 || self.slots[slot as usize].client.get() != client {
            return;
        }
        let s = &self.slots[slot as usize];
        if self.has_game_file() {
            let name = self.op1(Op::ClientName, client);
            self.slot_set_name(s, name);
        } else {
            self.slot_set_state(s, 0);
            s.reset();
        }
        self.count_players(false);
        self.op1(Op::SyncPlayer, slot as isize);
        if self.ask(Op::IAmCaptain) {
            self.broadcast_connect_status(slot, false);
        }
    }

    fn game_message(&self, kind: u8, buffer: isize, from: isize) -> R<()> {
        match kind {
            0x64 => self.handle_option_change(buffer, from),
            0x65 => self.handle_player_option_change(buffer, from),
            0x66 => self.handle_player_option_request(buffer, from),
            0x67 => self.handle_game_state(buffer, from),
            0x68 => self.handle_connect_status(buffer, from, true),
            0x69 => self.handle_connect_status(buffer, from, false),
            0x6a => {
                if self.op1(Op::ClientIsCaptain, from) != 0 {
                    self.op1(Op::CancelConnectDialog, 0);
                }
                Ok(())
            }
            _ => Ok(()),
        }
    }

    fn handle_connect_status(&self, buffer: isize, from: isize, from_client: bool) -> R<()> {
        let slot = self.read_byte(buffer)?;
        let flag = self.read(Op::ReadBool, buffer)? != 0;
        if from_client {
            if let Some(s) = self.slot(slot as i32) {
                if s.client.get() == from {
                    self.broadcast_connect_status(slot, flag);
                }
            }
        } else if self.op1(Op::ClientIsCaptain, from) != 0 {
            self.op(Op::SetSlotConnected, slot as isize, flag as isize, 0, 0);
        }
        Ok(())
    }

    fn broadcast_connect_status(&self, slot: u8, connected: bool) {
        if !self.ask(Op::ConnectDialogRunning) {
            return;
        }
        let b = self.op1(Op::NewBuffer, 0);
        self.append(Op::AppendByte, b, 0x69);
        self.append(Op::AppendByte, b, slot as isize);
        self.append(Op::AppendBool, b, connected as isize);
        self.op1(Op::BroadcastToGame, b);
        self.op1(Op::FreeBuffer, b);
    }

    fn send_connect_status(&self, connected: bool) {
        let b = self.op1(Op::NewBuffer, 0);
        self.append(Op::AppendByte, b, 0x68);
        let slot = self.my_slot();
        self.append(Op::AppendByte, b, slot as u8 as isize);
        self.append(Op::AppendBool, b, connected as isize);
        self.op1(Op::SendToCaptain, b);
        self.op1(Op::FreeBuffer, b);
    }

    fn send_cancel_connect(&self) {
        let b = self.op1(Op::NewBuffer, 0);
        self.append(Op::AppendByte, b, 0x6a);
        self.op1(Op::BroadcastToGame, b);
        self.op1(Op::FreeBuffer, b);
    }

    /// Message 0x66 on the host: a player's own team, colour, observing or transfer percent; accepted changes go to
    /// everyone, a refusal back to the player.
    fn handle_player_option_request(&self, buffer: isize, from: isize) -> R<()> {
        if !self.ask(Op::IsHost) {
            return Ok(());
        }
        let player = self.op1(Op::ClientPlayer, from);
        if player == 0 {
            return Ok(());
        }
        let index = self.op1(Op::PlayerSlot, player) as u8 as i32;
        let s = match self.slot(index) {
            Some(s) if s.client.get() == from => s,
            _ => return Ok(()),
        };
        let what = self.read_byte(buffer)? as i32;
        let value = self.read_byte(buffer)?;
        if self.op1(Op::PlayerSlot, player) as u8 as i32 != index {
            return Ok(());
        }
        let ok = match what {
            1 | 4 | 5 => true,
            2 => self.is_color_free(value),
            _ => false,
        };
        let b = self.op1(Op::NewBuffer, 0);
        self.append(Op::AppendByte, b, 0x65);
        self.append(Op::AppendByte, b, index as u8 as isize);
        self.append(Op::AppendByte, b, what as u8 as isize);
        let v = if ok { value } else { s.get_option(what) };
        self.append(Op::AppendByte, b, v as isize);
        if ok {
            self.op1(Op::BroadcastToGame, b);
        } else {
            self.op(Op::SendToClient, b, from, 0, 0);
        }
        self.op1(Op::FreeBuffer, b);
        Ok(())
    }

    /// Message 0x65 from the captain: a slot's option, with the team changes that join or leave our team announced.
    fn handle_player_option_change(&self, buffer: isize, from: isize) -> R<()> {
        if self.op1(Op::ClientIsCaptain, from) == 0 {
            return Ok(());
        }
        let index = self.read_byte(buffer)? as i8 as i32;
        let what = self.read_byte(buffer)? as i32;
        let value = self.read_byte(buffer)?;
        if let Some(s) = self.slot(index) {
            if what == 1 && s.client.get() != 0 && index != self.my_slot() {
                let old = s.opt(1);
                let team = self.my_team() as u8;
                if team != 0 && old != value {
                    if old == team {
                        self.op(Op::TeamChanged, s.client.get(), 1, 0, 0);
                    } else if value == team {
                        self.op(Op::TeamChanged, s.client.get(), 0, 0, 0);
                    }
                }
            }
            if what == 4 && value == 0 {
                let c = self.get_free_color(s.index as i32, s.opt(2));
                s.set_opt(2, c);
            }
            self.slot_set_option(s, what, value);
            if what == 0 || what == 4 {
                self.count_players(true);
            }
        }
        self.op1(Op::SyncPlayer, index as isize);
        if what == 3 {
            self.op1(Op::ResetReadyFlags, 0);
        }
        Ok(())
    }

    fn handle_option_change(&self, buffer: isize, from: isize) -> R<()> {
        if self.op1(Op::ClientIsCaptain, from) == 0 {
            return Ok(());
        }
        let option = self.read_byte(buffer)? as i32;
        self.read_option(buffer, option)?;
        self.op1(Op::SyncOption, option as isize);
        self.op1(Op::ResetReadyFlags, 0);
        Ok(())
    }

    fn send_option(&self, option: i32) {
        let b = self.op1(Op::NewBuffer, 0);
        self.append(Op::AppendByte, b, 0x64);
        self.append(Op::AppendByte, b, option as u8 as isize);
        self.write_option(b, option);
        self.op1(Op::BroadcastToGame, b);
        self.op1(Op::FreeBuffer, b);
    }

    fn set_option(&self, option: i32, value: u32) {
        if (0..16).contains(&option) && value < option_limit(option as usize) {
            self.options[option as usize].set(value as i32);
        }
    }

    /// ChangeOption: the starting epoch kept at or before the ending one (unless it is 15), the game re-advertised
    /// when a listed option changed; an out-of-range value resyncs the controls.
    fn change_option(&self, option: i32, value: u32, flag: bool) {
        if !(0..16).contains(&option) {
            return;
        }
        let o = option as usize;
        if value >= option_limit(o) {
            self.op1(Op::SyncOption, option as isize);
            return;
        }
        if self.opt(o) as u32 == value {
            return;
        }
        self.options[o].set(value as i32);
        if flag {
            self.send_option(option);
        }
        if option == 4 {
            if (self.opt(5) as u32) < (self.opt(4) as u32) && self.opt(4) != 0xf {
                self.change_option(5, self.opt(4) as u32, flag);
            }
        } else if option == 5 {
            if (self.opt(5) as u32) < (self.opt(4) as u32) && self.opt(4) != 0xf {
                self.change_option(4, self.opt(5) as u32, flag);
            }
            return;
        } else if option != 2 {
            return;
        }
        self.op1(Op::AdvertiseGame, 0);
    }

    fn my_slot(&self) -> i32 {
        let player = self.op1(Op::MyPlayer, 0);
        if player != 0 {
            return self.op1(Op::PlayerSlot, player) as u8 as i32;
        }
        -1
    }

    fn my_team(&self) -> i32 {
        let player = self.op1(Op::MyPlayer, 0);
        if player != 0 {
            return self.op1(Op::PlayerTeam, player) as u8 as i32;
        }
        -1
    }

    /// Message 0x67 from the captain: the whole game state (options, slots with their clients, the saved game).
    fn handle_game_state(&self, buffer: isize, from: isize) -> R<()> {
        if self.op1(Op::ClientIsCaptain, from) == 0 {
            return Ok(());
        }
        for i in 0..16 {
            self.read_option(buffer, i)?;
        }
        for s in &self.slots {
            self.slot_read_data(s, buffer)?;
            if s.opt(0) == 3 {
                let id = self.read(Op::ReadShort, buffer)? as u16;
                let clients = self.op1(Op::Clients, 0);
                let client = self.op(Op::FindClient, clients, id as isize, 0, 0);
                if client != 0 && self.op1(Op::ClientPlayer, client) != 0 {
                    let player = self.op1(Op::ClientPlayer, client);
                    self.slot_set_player(s, client, player);
                } else {
                    self.slot_set_state(s, 0);
                }
            }
        }
        if self.has_game_file() {
            self.read_game_file(buffer)?;
            self.start_transfer();
            self.make_colors_distinct();
        }
        self.op1(Op::LobbyEvent, 100065);
        self.op1(Op::ResetReadyFlags, 0);
        if self.ask(Op::IsHost) {
            self.count_players(false);
            self.op1(Op::AdvertiseGame, 0);
        }
        Ok(())
    }

    fn send_game_state(&self) {
        let b = self.op1(Op::NewBuffer, 0);
        self.append(Op::AppendByte, b, 0x67);
        for i in 0..16 {
            self.write_option(b, i);
        }
        for s in &self.slots {
            self.slot_write_data(s, b);
            if s.opt(0) == 3 {
                let id = self.op1(Op::ClientId, s.client.get());
                self.append(Op::AppendShort, b, id);
            }
        }
        if self.has_game_file() {
            self.write_game_file(b);
        }
        self.op1(Op::BroadcastToGame, b);
        self.op1(Op::FreeBuffer, b);
    }

    /// ChangePlayerOption: the host sends a change to everyone, a player asks the host; a refused change resyncs the
    /// slot's controls; a state change by the host kicks the slot's client.
    fn change_player_option(&self, slot: i32, what: i32, value: u8) {
        let s = match self.slot(slot) {
            Some(s) => s,
            None => return,
        };
        if s.get_option(what) == value {
            return;
        }
        let mut to_host = false;
        let mut ok = true;
        match what {
            1 | 2 | 4 | 5 => {
                ok = slot == self.my_slot() || (self.ask(Op::IsHost) && s.opt(0) != 3);
                to_host = true;
            }
            _ => {
                if what == 0 {
                    if self.ask(Op::IsHost) && slot != 0 {
                        self.op(Op::KickClient, s.client.get(), 0, 0, 0);
                    } else {
                        ok = false;
                    }
                }
                // case 0 falls through to the host check
                if !self.ask(Op::IsHost) {
                    ok = false;
                }
            }
        }
        if what == 2 && ok {
            ok = self.is_color_free(value);
        }
        if !ok {
            self.op1(Op::SyncPlayer, slot as isize);
            return;
        }
        if self.ask(Op::IsHost) {
            to_host = false;
        }
        let b = self.op1(Op::NewBuffer, 0);
        if to_host {
            self.append(Op::AppendByte, b, 0x66);
            self.append(Op::AppendByte, b, what as u8 as isize);
            self.append(Op::AppendByte, b, value as isize);
            self.op1(Op::SendToCaptain, b);
        } else {
            self.append(Op::AppendByte, b, 0x65);
            self.append(Op::AppendByte, b, slot as u8 as isize);
            self.append(Op::AppendByte, b, what as u8 as isize);
            self.append(Op::AppendByte, b, value as isize);
            self.op1(Op::BroadcastToGame, b);
        }
        self.op1(Op::FreeBuffer, b);
    }

    fn hook<T: Into<isize>>(r: R<T>) -> isize {
        match r {
            Ok(v) => v.into(),
            Err(Threw) => THREW,
        }
    }

    /// One inward call.
    pub fn call(&self, call: EeWonEeGameCall, a: isize, b: isize, c: isize, d: isize) -> isize {
        use EeWonEeGameCall as C;
        let slot = || self.slot(a as i32);
        match call {
            C::GetNumFiles => return self.files.borrow().len() as isize,
            C::GetFile => return self.get_file(a as i32),
            C::SetGameFile => self.set_game_file(a, b as u32 as isize, c as u32 as isize, d != 0),
            C::GetGameFileName => return self.game_file_name(),
            C::StopTransfer => self.stop_transfer(),
            C::UpdateTransferProgress => self.update_transfer_progress(a as i32, b as i32),
            C::SetTransferPercent => self.set_transfer_percent(a as u8),
            C::StartTransfer => self.start_transfer(),
            C::CanReadyHook => return self.can_ready() as isize,
            C::CanStartGameHook => return self.can_start() as isize,
            C::TryStartGameHook => return self.try_start() as isize,
            C::PostNetLeaveGameHook | C::NotifyStartGameHook => self.stop_transfer(),
            C::HandleHostGameHook => self.handle_host_game(a),
            C::GetJoinGameRequestHook => self.join_request(a),
            C::HandleJoinGameRequestHook => return Self::hook(self.handle_join_request(a, b)),
            C::GetJoinGameReplyHook => self.join_reply(b),
            C::HandleJoinGameReplyHook => return Self::hook(self.handle_join_reply(a).map(|()| 0isize)),
            C::HandlePlayerLeftHook => self.handle_player_left(a, b),
            C::HandlePlayerJoinedHook => self.handle_player_joined(a),
            C::WriteSummaryHook => self.write_summary(a),
            C::WriteDetailsHook => self.write_details(a),
            C::ReadSummaryHook => return Self::hook(self.read_summary(a).map(|v| v as isize)),
            C::ReadDetailsHook => return Self::hook(self.read_details(a).map(|v| v as isize)),
            C::HandleGameMessageHook => return Self::hook(self.game_message(a as u8, b, c).map(|()| 0isize)),
            C::GetStatusStringHook => return status_string(a as i16) as isize,
            C::ResetSlots => self.reset_slots(),
            C::CountPlayers => self.count_players(a != 0),
            C::HasGameFile => return self.has_game_file() as isize,
            C::GetOption => return self.get_option(a as i32) as isize,
            C::SetOption => self.set_option(a as i32, b as u32),
            C::ChangeOption => self.change_option(a as i32, b as u32, c != 0),
            C::SendGameState => self.send_game_state(),
            C::ChangePlayerOption => self.change_player_option(a as i32, b as i32, c as u8),
            C::SendConnectStatus => self.send_connect_status(a != 0),
            C::SendCancelConnect => self.send_cancel_connect(),
            C::SendOption => self.send_option(a as i32),
            C::GetFreeColor => return self.get_free_color(a as i32, b as u8) as isize,
            C::MakeColorsDistinct => self.make_colors_distinct(),
            C::GetMySlot => return self.my_slot() as isize,
            C::GetMyTeam => return self.my_team() as isize,
            C::GetTakenColors => {
                let taken = self.taken_colors(b as i32);
                if a != 0 {
                    // SAFETY: the caller's 16 bools (the contract of the call)
                    let out = unsafe { std::slice::from_raw_parts_mut(a as *mut bool, 16) };
                    out.copy_from_slice(&taken);
                }
            }
            C::IsColorFree => return self.is_color_free(a as u8) as isize,
            C::Field => {
                return match a {
                    0 => self.transfer.get(),
                    1 => self.has_file.get() as isize,
                    2 => self.num_human.get() as isize,
                    3 => self.num_computer.get() as isize,
                    4 => self.num_reserved.get() as isize,
                    _ => 0,
                }
            }
            _ => {
                let Some(s) = slot() else { return 0 };
                match call {
                    C::SlotReset => s.reset(),
                    C::SlotGetName => return self.slot_get_name(s),
                    C::SlotSetState => self.slot_set_state(s, b as u8),
                    C::SlotGetOption => return s.get_option(b as i32) as isize,
                    C::SlotSetOption => self.slot_set_option(s, b as i32, c as u8),
                    C::SlotClearPlayer => self.slot_clear_player(s),
                    C::SlotSetOpen => self.slot_set_state(s, 0),
                    C::SlotSetClosed => self.slot_set_state(s, 2),
                    C::SlotSetComputer => self.slot_set_state(s, 1),
                    C::SlotSetName => self.slot_set_name(s, b),
                    C::SlotSetPlayer => self.slot_set_player(s, b, c),
                    C::SlotRawOption => return s.get_option(b as i32) as isize,
                    C::SlotSetRawOption => {
                        if (0..6).contains(&b) {
                            s.set_opt(b as usize, c as u8);
                        }
                    }
                    C::SlotClient => return s.client.get(),
                    C::SlotPlayer => return s.player.get(),
                    _ => {}
                }
            }
        }
        0
    }

    /// The destructor: each slot from the last (its player cleared, then its name), the file transfer, the names.
    fn release_all(&self) {
        for s in self.slots.iter().rev() {
            self.slot_clear_player(s);
            self.release(s.name.replace(0));
        }
        self.release(self.transfer.replace(0));
        let files = std::mem::take(&mut *self.files.borrow_mut());
        for name in files {
            self.release(name);
        }
    }
}

use EeWonEeGameOp as Op;

/// A new lobby game for the C++ `game` (EELobbyGame's constructor).
/// As `ee_won_eegame_new`, with the model's ops answered by its Rust objects (the game's).
#[no_mangle]
pub extern "C" fn ee_won_eegame_new_model(game: *mut c_void, host: EeWonEeGameHostFn) -> *mut EeWonEeGame {
    let g = EeWonEeGame::new(game, host);
    g.model.set(true);
    Box::into_raw(Box::new(g))
}

#[no_mangle]
pub extern "C" fn ee_won_eegame_new(game: *mut c_void, host: EeWonEeGameHostFn) -> *mut EeWonEeGame {
    Box::into_raw(Box::new(EeWonEeGame::new(game, host)))
}

/// Releases the slots' members, the transfer and the names (the destructor's order) and frees the object.
///
/// # Safety
/// `p` came from `ee_won_eegame_new` (or is NULL) and is not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_won_eegame_free(p: *mut EeWonEeGame) {
    if !p.is_null() {
        // SAFETY: the contract; the releases run through a shared reference before the box is dropped
        unsafe { (*p).release_all() };
        // SAFETY: the contract
        drop(unsafe { Box::from_raw(p) });
    }
}

/// One inward call (`EeWonEeGameCall`); `isize::MIN` when a hook's read threw.
///
/// # Safety
/// `p` is live; the call's arguments as documented.
#[no_mangle]
pub unsafe extern "C" fn ee_won_eegame_call(
    p: *const EeWonEeGame,
    call: EeWonEeGameCall,
    a: isize,
    b: isize,
    c: isize,
    d: isize,
) -> isize {
    // SAFETY: live (the contract); a shared reference: the calls out may re-enter
    unsafe { (*p).call(call, a, b, c, d) }
}

/// EELobbyPlayer::IsTeammate's rule: team 0 (no team) has no teammates.
#[no_mangle]
pub extern "C" fn ee_won_eegame_is_teammate(mine: u8, theirs: u8) -> bool {
    is_teammate(mine, theirs)
}

/// For tests: forget the option types read once (the next game reads the map types again).
#[cfg(test)]
pub(crate) fn reset_types() {
    TYPES_INITIALIZED.store(false, Ordering::Relaxed);
}

/// For the other objects' tests: a real game over a test's op function (the direct path's other end).
#[cfg(test)]
pub(crate) mod testgame {
    use super::*;
    use std::sync::{Mutex, MutexGuard};

    // The option types are read once per process (a static, as the original's): every test that makes a game holds
    // this lock.
    static TYPES: Mutex<()> = Mutex::new(());

    pub fn lock() -> MutexGuard<'static, ()> {
        TYPES.lock().unwrap_or_else(|e| e.into_inner())
    }

    /// A game's op function: the op and its four arguments.
    pub type GameOps = fn(EeWonEeGameOp, isize, isize, isize, isize) -> isize;

    thread_local! {
        static OPS: Cell<GameOps> = const { Cell::new(|_, _, _, _, _| 0) };
    }

    unsafe extern "C" fn host(_g: *mut c_void, op: EeWonEeGameOp, a: isize, b: isize, c: isize, d: isize) -> isize {
        // the map types (twelve), read by a new game and not shown, as the C++ goldens answer them
        if op == EeWonEeGameOp::NumMapTypes {
            return 12;
        }
        let f = OPS.with(|h| h.get());
        f(op, a, b, c, d)
    }

    /// A new game (C++ pointer 0x6000) whose ops go to `ops`; the caller holds `lock()`.
    pub fn new(ops: GameOps) -> *mut EeWonEeGame {
        OPS.with(|h| h.set(ops));
        reset_types();
        ee_won_eegame_new(0x6000 as *mut c_void, Some(host))
    }

    /// Sets the game's options and slots' raw options (`SetOption` and `SlotSetRawOption`, as a golden's set-up).
    pub fn setup(g: *mut EeWonEeGame, options: &[(isize, isize)], slots: &[(isize, isize, isize)]) {
        // SAFETY: a live game of `new`
        let g = unsafe { &*g };
        for &(o, v) in options {
            g.call(EeWonEeGameCall::SetOption, o, v, 0, 0);
        }
        for &(s, o, v) in slots {
            g.call(EeWonEeGameCall::SlotSetRawOption, s, o, v, 0);
        }
    }
}

#[cfg(test)]
#[path = "eegame_tests.rs"]
mod tests;
