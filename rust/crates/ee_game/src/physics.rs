//! The physics thread's own bodies (`EEPhysics`, world lane round 1): the tick, the input queue's drain, the
//! out-of-synch checksum and its dump, the unit swaps, the civilization and resource timers, the weather, the
//! messages, the autosave and the timing. The C++ class keeps its members, its vtable and its thread; each method is a
//! one-statement forward to a body here, and each C++ call a body makes is one operation ([`EePhysCpp`]) answered by
//! a one-statement case of `ee_physcpp` / `ee_physcpp_f` (`src/Empire Earth/Physics Thread/EEPhysics.cpp`).
//!
//! The saved block (`EeUnitsPhysicsSaved`, the record `EEPhysics` derives from) is read and written in place; a
//! read or write is never held across a call back into the C++. Every C++ call that writes a capture record (the
//! unit store check, the world view check, the executed message, the UI fixture's hook) is made in the C++'s place and
//! order, and so is every random draw (RUST_PLAN 2.1).

// the C++'s own casts (`long` and `unsigned long` are 32-bit here, 64-bit on LP64)
#![allow(
    clippy::unnecessary_cast,
    clippy::excessive_precision,
    clippy::collapsible_if,
    clippy::neg_cmp_op_on_partial_ord
)]

use std::ffi::{c_char, c_long, c_ulong, c_void};
use std::sync::atomic::{AtomicBool, AtomicU32, Ordering};
use std::sync::Mutex;

use ee_base::math::transform::RotationOrder;
use ee_base::math::types::{GeTransformation, GeVector};
use ee_base::math::um::lrintf;
use ee_base::time::{ee_time_get, ee_time_of_day, EeTimeOfDay};
use ee_sim::units::physics_save::EeUnitsPhysicsSaved;

/// The physics' calls (p: the `EEPhysics`, or the object a call names). See EEPhysics.cpp for each exact call.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EePhysCpp {
    // ---- the physics (p: the EEPhysics)
    /// `static_cast<EeUnitsPhysicsSaved*>(this)`
    Rec = 1,
    /// `mSpeed` (float)
    Speed,
    SetSpeed,
    TimerInterval,
    TicksPerSecond,
    TimerRunning,
    /// `mRandom.Generate()` (float)
    Random,
    RandomSeed,
    SetRandomSeed,
    InputMessages,
    SetInputMessages,
    SetLastChecksum,
    /// `mThread != NULL ? mThread->Post(a0, NULL, NULL, true) : 0`
    ThreadPost,
    RtsHandleMessage,
    UpdateUnits,
    /// the loaded game's fixups (EEAfterLoad, EEAfterLoadLast)
    RunAfterLoad,
    ServerLockEnter,
    ServerLockLeave,
    ServerQueueSize,
    ServerQueueFront,
    ServerQueuePopFront,
    SaveLockEnter,
    SaveLockLeave,
    LoadSaveCount,
    LoadSaveAt,
    LoadSaveSetNull,
    LoadSaveClear,
    LoadSavePush,
    SwapCount,
    SwapOld,
    SwapNew,
    SwapPush,
    SwapClear,
    // ---- a message (p: an NEMessage*)
    MsgType,
    MsgReference,
    MsgDereference,
    MsgForSpecificSlot,
    MsgOriginatingSlot,
    MsgSetOriginatingSlot,
    /// `static_cast<NEMRecord*>(p)->Get<bool>(a0)`
    RecBool,
    RecU8,
    RecU32,
    GameCast,
    ExecuteSelection,
    ExecuteGame,
    Execute,
    /// `static_cast<EEMRecord*>(p)->mRecord`
    RecordBytes,
    /// `ExecuteRemovePlayer` with the record's player, out of synch, exiting and booted
    RemovePlayer,
    NewChecksumResponse,
    NewLoadSaveResponse,
    NewReportStatistics,
    NewExitShiftMode,
    NewCivTimerExpired,
    // ---- the captures
    CaptureExecuted,
    SimCaptureActive,
    UnitStoreCheck,
    WorldViewCheck,
    TestHookSet,
    TestHookCall,
    OrderLog,
    GameLogOn,
    // ---- the communications
    IsServer,
    IsClient,
    /// `gCommunications != NULL && gCommunications->IsServer()`
    CommsIsServer,
    ClientHasLeft,
    CtsInPhysics,
    IncrementCts,
    SetAverageUpdate,
    SetAverageGap,
    ProcessLocalInput,
    LogChecksumResponse,
    LogResponse,
    IsPaused,
    CurrentCommsTick,
    PauseGame,
    /// `SendToAllAndWait(new EEMRecord(kMessageLoadOrSaveGame, {...}), 15000, 12)` with the name a0
    SendAutoSave,
    // ---- the world
    NumPlayers,
    Player,
    CurrentPlayer,
    PlayerBySlot,
    FlushQueuedPlacements,
    QueueBornDeactivations,
    UpdateActiveUnits,
    RemovePlayerFromGame,
    CheckGameOver,
    InCinematic,
    ScenarioResult,
    SetScenarioResult,
    ScenarioUpdate,
    ScenarioShowingDialog,
    ScenarioCanShowDialog,
    ScenarioShowDialog,
    ScenarioReplaceUnit,
    TeamsClear,
    CameraUnitA,
    CameraUnitB,
    CameraFollow,
    CameraClearA,
    CameraClearB,
    CameraUpdate,
    /// `gWorld.GetGameSpeed()` (float)
    GameSpeed,
    /// `gWorld.mGameSpeed` (float)
    WorldGameSpeed,
    /// (float)
    TimeOfDay,
    SetTimeOfDay,
    /// (float)
    WindYaw,
    SetWindYaw,
    SetWindDirection,
    EnvironmentTick,
    WorldWriteLock,
    WorldWriteUnlock,
    UnitsFirst,
    UnitNext,
    UnitChecksum,
    UnitDump,
    ContagionSwapUnit,
    ContagionDump,
    MapChecksum,
    MapDump,
    CampaignNameLength,
    CampaignChecksum,
    IsScenarioGame,
    Resources,
    CanBuyCivilization,
    CheckUnitList,
    SetGameOverFlag,
    // ---- a player (p: an EEPlayer*)
    PlayerObserver,
    PlayerSetGameResult,
    PlayerGameResult,
    PlayerAddPendingUnits,
    PlayerRemoveKilledUnits,
    /// `UnitsSwapped(((EEPhysics*)a0)->mUnitSwaps)`
    PlayerUnitsSwapped,
    PlayerCivChosen,
    PlayerShiftMode,
    PlayerAvailablePoints,
    PlayerSavedGroupLast,
    PlayerId,
    PlayerSlot,
    PlayerGetSlot,
    PlayerActiveGroup,
    PlayerRunningGroups,
    PlayerRunningGroup,
    PlayerAirTraffic,
    // ---- the groups (p: an EESelectedGroup*, an EEGroup* for the Group ones)
    SelectedCount,
    SelectedUnitId,
    GroupCount,
    GroupUnitId,
    // ---- a unit (p: an EEComplexUnit*)
    UnitPlayer,
    UnitIsCitizen,
    UnitReference,
    UnitDereference,
    UnitId,
    // ---- the other systems
    FreeCamera,
    PathManagerUpdate,
    PathManagerDump,
    AreaEffectsTick,
    EffectManagerUpdate,
    EffectReplaceUnit,
    CalamityUpdate,
    CalamityDump,
    CalamityChecksum,
    AiUpdate,
    WorldTableLong,
    HasJitPower,
    /// the just-in-time manufacturing chat line
    JitChat,
    CivBuilderBlink,
    IsMainGameForm,
    OptionsButtonBlink,
    CivTimerLimit,
    CivTimerSecondWarning,
    CivTimerFirstWarning,
    ShiftKeyUp,
    AutoSaves,
    AutoSaveMilliseconds,
    SetLastAutoSave,
    SavePreferences,
    /// `gSavedGameExtension` into the buffer a0 (room a1)
    SavedGameExtension,
    LoadOrSave,
    FileOpen,
    FilePuts,
    FileClose,
    AtcChecksum,
    AtcDump,
    CpChecksum,
    ContinentChecksum,
    StoreChecksumFlag1,
    StoreChecksumFlag2,
    /// the campaign progress's won scenario (EndGame)
    CampaignWin,
    AppTeamsClear,
    Srand,
    Rand,
    Verify,
}

use EePhysCpp as O;

/// cbindgen:ignore
#[cfg(not(test))]
mod sys {
    use super::EePhysCpp;
    extern "C" {
        // EEPhysics.cpp
        pub fn ee_physcpp(op: EePhysCpp, p: usize, a: *const i64, f: *const f32) -> i64;
        pub fn ee_physcpp_f(op: EePhysCpp, p: usize, a: *const i64, f: *const f32) -> f64;
    }
}

/// The tests run no C++: a call is a bug of the test.
#[cfg(test)]
mod sys {
    use super::EePhysCpp;
    pub unsafe fn ee_physcpp(op: EePhysCpp, _: usize, _: *const i64, _: *const f32) -> i64 {
        panic!("no C++ in the tests ({op:?})")
    }
    pub unsafe fn ee_physcpp_f(op: EePhysCpp, _: usize, _: *const i64, _: *const f32) -> f64 {
        panic!("no C++ in the tests ({op:?})")
    }
}

type P = usize;

fn hf(op: O, p: P, a: &[i64], f: &[f32]) -> i64 {
    let mut ai = [0i64; 6];
    ai[..a.len()].copy_from_slice(a);
    let mut fi = [0f32; 4];
    fi[..f.len()].copy_from_slice(f);
    // SAFETY: the host's operation over the C++ objects of the physics thread, on that thread
    unsafe { sys::ee_physcpp(op, p, ai.as_ptr(), fi.as_ptr()) }
}

fn h(op: O, p: P, a: &[i64]) -> i64 {
    hf(op, p, a, &[])
}

fn h0(op: O) -> i64 {
    hf(op, 0, &[], &[])
}

fn hp(op: O, p: P, a: &[i64]) -> P {
    h(op, p, a) as P
}

fn hd(op: O, p: P, a: &[i64]) -> f32 {
    let mut ai = [0i64; 6];
    ai[..a.len()].copy_from_slice(a);
    let fi = [0f32; 4];
    // SAFETY: as hf
    unsafe { sys::ee_physcpp_f(op, p, ai.as_ptr(), fi.as_ptr()) as f32 }
}

fn b(op: O, p: P, a: &[i64]) -> bool {
    h(op, p, a) != 0
}

/// A C string the C++ reads for the call.
fn cstr(text: &str) -> Vec<u8> {
    let mut bytes = text.as_bytes().to_vec();
    bytes.push(0);
    bytes
}

fn sa(bytes: &[u8]) -> i64 {
    bytes.as_ptr() as usize as i64
}

/// The physics' saved block. Each use reads or writes one field; none is held across a C++ call.
fn rec(p: P) -> *mut EeUnitsPhysicsSaved {
    hp(O::Rec, p, &[]) as *mut EeUnitsPhysicsSaved
}

macro_rules! get {
    ($r:expr, $f:ident) => {
        // SAFETY: the physics' live record (rec), read now
        unsafe { (*$r).$f }
    };
}

macro_rules! set {
    ($r:expr, $f:ident, $v:expr) => {{
        let value = $v;
        // SAFETY: the physics' live record (rec), written now
        unsafe { (*$r).$f = value }
    }};
}

fn time() -> u32 {
    ee_time_get()
}

fn time_of_day() -> EeTimeOfDay {
    let mut t = EeTimeOfDay::default();
    // SAFETY: a live record
    unsafe { ee_time_of_day(&mut t) };
    t
}

const MESSAGE_PHYSICS_TICK: i64 = 0x28;
const MESSAGE_REMOVE_PLAYER: i64 = 0x2b;
/// The selection messages (kMessageSelectUnit .. kMessageSelectTheseUnits).
const SELECTION_MESSAGES: [i64; 11] = [0x48, 0x49, 0x4a, 0x4b, 0x4e, 0x4f, 0x50, 0x76, 0x82, 0x84, 0x96];
const SLOT_SERVER: i64 = 0xfe;
const SLOT_NONE: i64 = 0xff;
/// kWorldDeathmatchJitManufacturingDelay
const WORLD_JIT_DELAY: i64 = 94;
/// the area effects' period (ms)
const AREA_EFFECT_INTERVAL: f32 = 2000.0;
/// the wind direction's random walk step
const WIND_DRIFT: f32 = 0.005;

// ---- the loaded game's seed (Load sets it, the first tick spends it: "load equals continue", R5a.2)
static LOADED_SEED: AtomicU32 = AtomicU32::new(0);
static LOADED_SEED_PENDING: AtomicBool = AtomicBool::new(false);

/// A loaded game's seed, put back at its first tick.
#[no_mangle]
pub extern "C" fn ee_physics_loaded_seed(seed: u32) {
    LOADED_SEED.store(seed, Ordering::Relaxed);
    LOADED_SEED_PENDING.store(true, Ordering::Relaxed);
}

/// `Init`: the clocks, tick counters and game flags reset; the queued server messages and load/save requests dropped.
fn init(p: P) {
    let r = rec(p);
    set!(r, mCurrentTick, 0);
    set!(r, mNumTicksDone, 0);
    set!(r, mF84, 0.0);
    set!(r, mTicksPerGameSecond, 0.0);
    set!(r, mGameTime, 0);
    set!(r, mTickSeconds, 0.0);
    set!(r, mTickRandom, 0);
    set!(r, mTimeOfDayStep, 0.0);
    set!(r, mTickTime, 0.0);
    set!(r, mGameClock, 0.0);
    set!(r, mTickStartTime, time() as c_long);
    set!(r, mPreviousTickStartTime, time() as c_long);
    set!(r, mAreaEffectsClock, 0.0);
    set!(r, mTickTimeMs, 0);
    set!(r, mShiftModeClock, 0.0);
    set!(r, mTicksToProcess, 0);
    set!(r, mTickGapSum, 0);
    set!(r, mTickGapCount, 0);
    set!(r, mTickWorkSum, 0);
    set!(r, mTickWorkCount, 0);
    set!(r, mAveragePhysicsUpdateTime, 0);
    set!(r, mAverageTimeBetweenPhysicsTicks, 0);
    set!(r, mLD4, 0);
    set!(r, mTimeOfDayRuns, false);
    set!(r, mGameStarted, false);
    set!(r, mBE2, false);
    set!(r, mCivTimerRuns, true);
    set!(r, mCivTimerFirstWarning, false);
    set!(r, mCivTimerSecondWarning, false);
    set!(r, mGameOver, false);
    set!(r, mBE7, false);
    set!(r, mResourceTimerPending, true);
    clear_server_messages(p);
    h(O::SaveLockEnter, p, &[]);
    let mut i = 0;
    while i < h(O::LoadSaveCount, p, &[]) {
        h(O::MsgDereference, hp(O::LoadSaveAt, p, &[i]), &[]);
        i += 1;
    }
    h(O::LoadSaveClear, p, &[]);
    h(O::SaveLockLeave, p, &[]);
    h(O::SetInputMessages, p, &[0]);
    hf(O::SetSpeed, p, &[], &[hd(O::Speed, p, &[])]);
}

/// `ClearServerMessages`: every queued server message dropped (under the queue's lock).
fn clear_server_messages(p: P) {
    h(O::ServerLockEnter, p, &[]);
    while h(O::ServerQueueSize, p, &[]) > 0 {
        let message = hp(O::ServerQueueFront, p, &[]);
        h(O::ServerQueuePopFront, p, &[]);
        h(O::MsgDereference, message, &[]);
    }
    h(O::ServerLockLeave, p, &[]);
}

/// `AutoSave`: with autosaves on, names the file "AutoSave <date - time>", records it in the preferences and sends
/// every player the save request.
fn auto_save(_p: P) {
    if h0(O::AutoSaves) as c_ulong > 0 {
        let t = time_of_day();
        let mut ext = [0u8; 64];
        h(O::SavedGameExtension, 0, &[ext.as_mut_ptr() as usize as i64, ext.len() as i64]);
        let ext_len = ext.iter().position(|&c| c == 0).unwrap_or(ext.len());
        let name = format!(
            "{} {:4}.{:02}.{:02} - {:02}h.{:02}m.{:02}s{}",
            "AutoSave",
            t.mYear as c_long,
            (t.mMonth as c_long).wrapping_add(1),
            t.mDay as c_long,
            t.mHour as c_long,
            t.mMinute as c_long,
            t.mSecond as c_long,
            String::from_utf8_lossy(&ext[..ext_len])
        );
        let name = cstr(&name);
        h(O::SetLastAutoSave, 0, &[sa(&name)]);
        h0(O::SavePreferences);
        h(O::SendAutoSave, 0, &[sa(&name)]);
    }
}

/// `FlushWorld`: the queued placements, every player's pending units, the born units' deactivations, the queued unit
/// swaps, then every player's killed units removed.
fn flush_world(p: P) {
    let players = h0(O::NumPlayers);
    h0(O::CheckUnitList);
    h0(O::FlushQueuedPlacements);
    for i in 0..players {
        let player = hp(O::Player, 0, &[i]);
        if player != 0 {
            h(O::PlayerAddPendingUnits, player, &[]);
        }
    }
    h0(O::QueueBornDeactivations);
    process_unit_swaps(p);
    for i in 0..players {
        let player = hp(O::Player, 0, &[i]);
        if player != 0 {
            h(O::PlayerRemoveKilledUnits, player, &[]);
        }
    }
}

/// `EndGame`: every non-observer player but player 0 wins or loses, a won campaign scenario is recorded, the in-game
/// options button blinks, the teams are cleared and a server pauses the game.
fn end_game(p: P, victory: bool) {
    h0(O::SetGameOverFlag);
    for i in 1..h0(O::NumPlayers) {
        let player = hp(O::Player, 0, &[i]);
        if !b(O::PlayerObserver, player, &[]) {
            h(O::RemovePlayerFromGame, 0, &[victory as i64, i]);
            h(O::PlayerSetGameResult, hp(O::Player, 0, &[i]), &[if victory { 1 } else { 2 }]);
        }
    }
    if h0(O::CampaignNameLength) > 0
        && h0(O::CampaignChecksum) as c_ulong > 0
        && h(O::PlayerGameResult, hp(O::CurrentPlayer, 0, &[]), &[]) == 1
    {
        h0(O::CampaignWin);
    }
    if h(O::PlayerGameResult, hp(O::CurrentPlayer, 0, &[]), &[]) != 0 {
        h(O::CivBuilderBlink, 0, &[0]);
        if h0(O::IsMainGameForm) != 0 {
            h0(O::OptionsButtonBlink);
        }
    }
    h0(O::TeamsClear);
    h0(O::AppTeamsClear);
    if h0(O::IsServer) != 0 {
        h0(O::PauseGame);
    }
    set!(rec(p), mGameOver, true);
}

/// One line of the out-of-synch dump.
fn put(file: P, line: &str) {
    let text = cstr(line);
    h(O::FilePuts, file, &[sa(&text)]);
}

/// `GetOutOfSynchChecksum`'s dump to "OOS <date>.log".
fn dump_out_of_synch(p: P, flags: c_ulong) {
    let t = time_of_day();
    let name = format!(
        "OOS {:4}.{:02}.{:02} - {:02}h.{:02}m.{:02}s.log",
        t.mYear as c_long,
        (t.mMonth as c_long).wrapping_add(1),
        t.mDay as c_long,
        t.mHour as c_long,
        t.mMinute as c_long,
        t.mSecond as c_long
    );
    let name = cstr(&name);
    let file = hp(O::FileOpen, 0, &[sa(&name)]);
    h(O::Verify, 0, &[(file != 0) as i64, 544]);
    for i in 0..h0(O::NumPlayers) {
        let player = hp(O::Player, 0, &[i]);
        put(file, "\n");
        put(
            file,
            &format!(
                "OUT OF SYNCH DUMP:  **************  Player Number: {}  *************\n",
                h(O::PlayerId, player, &[]) as c_long
            ),
        );
        let group = hp(O::PlayerActiveGroup, player, &[]);
        if group != 0 {
            put(file, "\n");
            let count = h(O::SelectedCount, group, &[]) as c_ulong;
            put(
                file,
                &format!(
                    "OUT OF SYNCH DUMP:  **************  Selected Group Count: {}  *************\n",
                    count as c_long
                ),
            );
            for j in 0..count {
                let id = h(O::SelectedUnitId, group, &[j as i64]) as c_ulong;
                put(
                    file,
                    &format!("OUT OF SYNCH DUMP: Unit #{} has an id of {}({:08x})\n", j as c_long, id as c_long, id),
                );
            }
        }
        put(file, "\n");
        let running = h(O::PlayerRunningGroups, player, &[]) as c_ulong;
        put(
            file,
            &format!("OUT OF SYNCH DUMP:  **************  Active Group Count: {}  *************\n", running as c_long),
        );
        for j in 0..running {
            let group = hp(O::PlayerRunningGroup, player, &[j as i64]);
            put(file, "\n");
            let count = h(O::GroupCount, group, &[]) as c_ulong;
            put(
                file,
                &format!("OUT OF SYNCH DUMP:  Active Group #{} has a unit count of {}\n", j as c_long, count as c_long),
            );
            for m in 0..count {
                let id = h(O::GroupUnitId, group, &[m as i64]) as c_ulong;
                put(
                    file,
                    &format!("OUT OF SYNCH DUMP: Unit #{} has an id of {}({:08x})\n", m as c_long, id as c_long, id),
                );
            }
        }
        for k in 0..4 {
            h(O::AtcDump, hp(O::PlayerAirTraffic, player, &[k]), &[file as i64]);
        }
    }
    h(O::PathManagerDump, 0, &[file as i64]);
    if flags & 4 != 0 {
        h(O::MapDump, 0, &[file as i64]);
    }
    if flags & 0x10 != 0 {
        h(O::CalamityDump, 0, &[file as i64]);
        h(O::ContagionDump, 0, &[file as i64]);
    }
    let mut unit = hp(O::UnitsFirst, 0, &[]);
    while unit != 0 {
        h(O::UnitDump, unit, &[file as i64, flags as i64]);
        unit = hp(O::UnitNext, unit, &[]);
    }
    let r = rec(p);
    put(file, &format!("OUT OF SYNCH DUMP: ******* Dumped on PT {} *******\n", get!(r, mNumTicksDone) as c_long));
    put(file, &format!("OUT OF SYNCH DUMP: ******* Current PT {} *******\n", get!(r, mCurrentTick) as c_long));
    put(
        file,
        &format!(
            "OUT OF SYNCH DUMP: ******* Continent Manager Checksum: {} *******\n",
            h0(O::ContinentChecksum) as c_long
        ),
    );
    put(file, &format!("OUT OF SYNCH DUMP: ******* Overall Checksum: {} *******\n", 0));
    put(
        file,
        &format!(
            "OUT OF SYNCH DUMP: ******* this->mRandomNumberGenerator.GetSeed(): {} *******\n",
            h(O::RandomSeed, p, &[]) as c_long
        ),
    );
    put(
        file,
        &format!(
            "OUT OF SYNCH DUMP: ******* Number of user-input messages processed: {} *******\n",
            h(O::InputMessages, p, &[]) as c_long
        ),
    );
    put(
        file,
        &format!(
            "OUT OF SYNCH DUMP: ******* Number of Messages in Server Queue: {} *******\n",
            h(O::ServerQueueSize, p, &[])
        ),
    );
    put(
        file,
        &format!(
            "OUT OF SYNCH DUMP: ******* Number of CT Physics Ticks To Process: {} *******\n",
            get!(r, mTicksToProcess) as c_long
        ),
    );
    h(O::FileClose, file, &[]);
}

/// `GetOutOfSynchChecksum`: the world's out-of-synch checksum (`flags` picks the parts: map, calamities, air traffic,
/// continents, selections); `dump` also writes everything to "OOS <date>.log". The units' flags 1 and 2 are the unit
/// store's sums (R5b.4); the sums wrap and commute as the C++'s `unsigned long`.
fn out_of_synch_checksum(p: P, dump: bool, flags: c_ulong) -> c_ulong {
    if dump {
        dump_out_of_synch(p, flags);
    }
    let mut sum: c_ulong = 0;
    let add = |sum: &mut c_ulong, v: i64| *sum = sum.wrapping_add(v as c_ulong);
    if flags & 1 != 0 {
        add(&mut sum, h0(O::StoreChecksumFlag1));
    }
    if flags & 2 != 0 {
        add(&mut sum, h0(O::StoreChecksumFlag2));
    }
    let unit_flags = flags & !3;
    if unit_flags & (0x20 | 8) != 0 {
        let mut unit = hp(O::UnitsFirst, 0, &[]);
        while unit != 0 {
            add(&mut sum, h(O::UnitChecksum, unit, &[unit_flags as i64]));
            unit = hp(O::UnitNext, unit, &[]);
        }
    }
    for i in 0..h0(O::NumPlayers) {
        let player = hp(O::Player, 0, &[i]);
        if flags & 0x200 != 0 {
            let group = hp(O::PlayerActiveGroup, player, &[]);
            if group != 0 {
                let count = h(O::SelectedCount, group, &[]) as c_ulong;
                for j in 0..count {
                    let id = h(O::SelectedUnitId, group, &[j as i64]) as c_ulong;
                    let player_id = h(O::PlayerId, player, &[]) as c_ulong;
                    sum = sum.wrapping_add(id.wrapping_mul(player_id));
                }
            }
        }
        if flags & 0x40 == 0 {
            continue;
        }
        for k in 0..4 {
            add(&mut sum, h(O::AtcChecksum, hp(O::PlayerAirTraffic, player, &[k]), &[]));
        }
    }
    let r = rec(p);
    let tick_sum = (get!(r, mCurrentTick) as c_ulong)
        .wrapping_add(h(O::RandomSeed, p, &[]) as c_ulong)
        .wrapping_add(h(O::InputMessages, p, &[]) as c_ulong);
    sum = sum.wrapping_add(tick_sum);
    add(&mut sum, h0(O::CpChecksum));
    if flags & 4 != 0 {
        add(&mut sum, h0(O::MapChecksum));
    }
    if flags & 0x100 != 0 {
        add(&mut sum, h0(O::ContinentChecksum));
    }
    if flags & 0x10 != 0 {
        add(&mut sum, h0(O::CalamityChecksum));
    }
    h(O::SetLastChecksum, p, &[sum as i64]);
    sum
}

/// `GetTimeString`: the game speed and the elapsed game time ("<speed> hh:mm:ss"; three digits of hours past 99).
fn time_string(p: P) -> String {
    let time = get!(rec(p), mGameClock);
    let hours: c_ulong =
        if time >= 3600000.0 { ee_base::math::um::floor_unsigned(time * 2.7777778e-07) as c_ulong } else { 0 };
    let mut minutes: c_ulong = 0;
    if time >= 60000.0 {
        minutes = ((time as c_ulong).wrapping_sub(hours.wrapping_mul(3600000))) / 60000;
    }
    let seconds =
        ((time as c_ulong).wrapping_sub(hours.wrapping_mul(3600000)).wrapping_sub(minutes.wrapping_mul(60000))) / 1000;
    let speed = hd(O::WorldGameSpeed, 0, &[]);
    if hours < 100 {
        format!("{:03.2} {:02}:{:02}:{:02}", speed, hours as c_long, minutes as c_long, seconds as c_long)
    } else {
        format!("{:03.2} {:03}:{:02}:{:02}", speed, hours as c_long, minutes as c_long, seconds as c_long)
    }
}

/// `GetUnitSwapKind`: the unit's role in the queued unit swaps (the earliest wins): old 1, new 3; another owner 2 /
/// 4; citizen to non-citizen 5 / 7, non-citizen to citizen 6 / 8; 0 when it is not swapped.
fn unit_swap_kind(p: P, unit: P) -> c_long {
    let mut result = 0;
    let mut i = h(O::SwapCount, p, &[]) - 1;
    while i >= 0 {
        let old = hp(O::SwapOld, p, &[i]);
        let new = hp(O::SwapNew, p, &[i]);
        let is_old = old == unit;
        if is_old || new == unit {
            if h(O::UnitPlayer, old, &[]) == h(O::UnitPlayer, new, &[]) {
                result = if is_old { 1 } else { 3 };
                if b(O::UnitIsCitizen, old, &[]) {
                    if !b(O::UnitIsCitizen, new, &[]) {
                        result = if is_old { 5 } else { 7 };
                    }
                } else if b(O::UnitIsCitizen, new, &[]) {
                    result = if is_old { 6 } else { 8 };
                }
            } else {
                result = if is_old { 2 } else { 4 };
            }
        }
        i -= 1;
    }
    result
}

/// `HandleMessage`: start (the tick clock restarts), the time of day's toggle and reset, the scenario dialog, re-init,
/// autosave, a load/save request (queued with no lock), 0x50c; the rest to the base.
fn handle_message(p: P, message: u32, parameter1: usize, parameter2: usize) {
    let r = rec(p);
    match message {
        0x404 => {
            let now = time() as c_long;
            set!(r, mTickStartTime, now);
            set!(r, mPreviousTickStartTime, now);
            h(O::RtsHandleMessage, p, &[0x404, parameter1 as i64, parameter2 as i64]);
        }
        0x502 => set!(r, mTimeOfDayRuns, !get!(r, mTimeOfDayRuns)),
        0x503 => {
            set!(r, mTimeOfDayRuns, false);
            h0(O::WorldWriteLock);
            hf(O::SetTimeOfDay, 0, &[], &[0.35]);
            h0(O::WorldWriteUnlock);
        }
        0x508 => {
            if h0(O::ScenarioCanShowDialog) != 0 {
                h0(O::ScenarioShowDialog);
            }
        }
        0x509 => init(p),
        0x50a => auto_save(p),
        0x50b => {
            h(O::LoadSavePush, p, &[parameter1 as i64]);
        }
        0x50c => {
            set!(r, mLD4, parameter1 as isize as c_long);
            set!(r, mBE7, true);
        }
        _ => {
            h(O::RtsHandleMessage, p, &[message as i64, parameter1 as i64, parameter2 as i64]);
        }
    }
}

/// `Idle`: with the free camera on, drains the input queue and flushes the world under the world's lock.
fn idle(p: P) {
    if h0(O::FreeCamera) == 0 {
        return;
    }
    h0(O::WorldWriteLock);
    drain_input(p);
    flush_world(p);
    h0(O::WorldWriteUnlock);
}

/// `Tick`: one physics tick (the game clock, the civilization and resource timers, the weather, the paths, the area
/// effects, the units, the effects, the calamities, the AI, the scenario and the game over), then the timing averages
/// and, on a server, the autosave clock.
fn tick(p: P) {
    if h0(O::SimCaptureActive) != 0 {
        // TEST ONLY: the unit store equals the objects; the world views' check
        h(O::UnitStoreCheck, 0, &[0]);
        h(O::WorldViewCheck, 0, &[get!(rec(p), mNumTicksDone) as u32 as i64]);
    }
    if LOADED_SEED_PENDING.swap(false, Ordering::Relaxed) {
        // a loaded game goes on from the saved seed
        h(O::SetRandomSeed, p, &[LOADED_SEED.load(Ordering::Relaxed) as i64]);
    }
    h(O::RunAfterLoad, p, &[]);
    report_statistics(p);
    let r = rec(p);
    if get!(r, mTicksToProcess) == 0 {
        drain_input(p);
        serve_load_save_requests(p);
    }
    if get!(r, mTicksToProcess) != 0 || h0(O::IsPaused) != 0 || get!(r, mGameOver) || h0(O::CameraUnitA) == 0 {
        hf(O::CameraUpdate, 0, &[], &[hd(O::Speed, p, &[])]);
    }
    if !(get!(r, mTicksToProcess) > 0) {
        return;
    }
    set!(r, mGameClock, get!(r, mTickTime) + get!(r, mGameClock));
    set!(r, mGameTime, get!(r, mGameClock) as c_long);
    set!(r, mTickRandom, lrintf(hd(O::Random, p, &[])) as c_long);
    set!(r, mPreviousTickStartTime, get!(r, mTickStartTime));
    set!(r, mTickStartTime, time() as c_long);
    set!(r, mTicksToProcess, get!(r, mTicksToProcess).wrapping_sub(1));
    if get!(r, mGameOver) {
        return;
    }
    h0(O::UpdateActiveUnits);
    if get!(r, mCivTimerRuns) {
        update_civ_timer(p);
    }
    if get!(r, mResourceTimerPending) {
        let resource_time = (h(O::WorldTableLong, 0, &[WORLD_JIT_DELAY]) as c_long).wrapping_mul(1000);
        if resource_time as f32 <= get!(r, mGameClock) {
            set!(r, mResourceTimerPending, false);
            if h0(O::Resources) == 4 && h0(O::HasJitPower) != 0 {
                h0(O::JitChat);
            }
        }
    }
    update_weather(p);
    h0(O::PathManagerUpdate);
    update_area_effects_clock(p);
    check_shift_mode(p);
    h(O::UpdateUnits, p, &[]);
    h0(O::EffectManagerUpdate);
    hf(O::CalamityUpdate, 0, &[], &[get!(r, mTickTime)]);
    h0(O::AiUpdate);
    let mut result = 0;
    if h0(O::InCinematic) == 0 {
        result = h0(O::ScenarioResult);
    }
    h0(O::ScenarioUpdate);
    let mut new_result = if h0(O::InCinematic) != 0 { 0 } else { h0(O::ScenarioResult) };
    if new_result == 0 {
        if h0(O::InCinematic) == 0 {
            new_result = h0(O::CheckGameOver);
            if new_result != 0 {
                h(O::SetScenarioResult, 0, &[new_result]);
                end_game(p, new_result == 1);
            }
        }
    } else if result == 0 {
        end_game(p, new_result == 1);
    }
    flush_world(p);
    let start = get!(r, mTickStartTime) as c_ulong;
    set!(r, mTickWorkSum, get!(r, mTickWorkSum).wrapping_add((time() as c_ulong).wrapping_sub(start)));
    set!(r, mTickWorkCount, get!(r, mTickWorkCount).wrapping_add(1));
    if get!(r, mTickWorkSum) >= 1000 {
        set!(r, mAveragePhysicsUpdateTime, (get!(r, mTickWorkSum) / get!(r, mTickWorkCount)) as c_long);
        set!(r, mTickWorkSum, 0);
        set!(r, mTickWorkCount, 0);
    }
    set!(
        r,
        mTickGapSum,
        get!(r, mTickGapSum).wrapping_add(start.wrapping_sub(get!(r, mPreviousTickStartTime) as c_ulong))
    );
    set!(r, mTickGapCount, get!(r, mTickGapCount).wrapping_add(1));
    if get!(r, mTickGapSum) >= 1000 {
        set!(r, mAverageTimeBetweenPhysicsTicks, (get!(r, mTickGapSum) / get!(r, mTickGapCount)) as c_long);
        set!(r, mTickGapSum, 0);
        set!(r, mTickGapCount, 0);
    }
    set!(r, mNumTicksDone, get!(r, mNumTicksDone).wrapping_add(1));
    if get!(r, mGameStarted) {
        set!(r, mGameStarted, false);
        if (h0(O::CampaignNameLength) > 0 && h0(O::CampaignChecksum) as c_ulong > 0) || h0(O::IsScenarioGame) != 0 {
            if h(O::PlayerAvailablePoints, hp(O::CurrentPlayer, 0, &[]), &[]) as c_ulong > 0 {
                h(O::CivBuilderBlink, 0, &[1]);
            }
        }
    }
    if h0(O::IsServer) == 0 || !(h0(O::AutoSaveMilliseconds) as c_ulong > 0) {
        return;
    }
    set!(r, mAutoSaveClock, get!(r, mAutoSaveClock).wrapping_add(h(O::TimerInterval, p, &[]) as c_long));
    if !(get!(r, mAutoSaveClock) as c_ulong > h0(O::AutoSaveMilliseconds) as c_ulong)
        || h0(O::InCinematic) != 0
        || h0(O::ScenarioShowingDialog) != 0
    {
        return;
    }
    set!(r, mAutoSaveClock, 0);
    h(O::ThreadPost, p, &[0x50a]);
    h0(O::CurrentCommsTick);
    h0(O::CurrentCommsTick);
}

/// `EEPhysicsLog`: the game log's human orders and selections (TEST ONLY; ee_sim units order_log.rs).
fn physics_log(message: P, time: c_long, select: u8) {
    let player =
        if h0(O::GameLogOn) != 0 { hp(O::PlayerBySlot, 0, &[h(O::MsgOriginatingSlot, message, &[])]) } else { 0 };
    let record = if player != 0 { hp(O::GameCast, message, &[]) } else { 0 };
    if player != 0 {
        let bytes = if record != 0 { h(O::RecordBytes, record, &[]) } else { 0 };
        h(
            O::OrderLog,
            0,
            &[
                player as i64,
                h(O::PlayerActiveGroup, player, &[]),
                h(O::MsgType, message, &[]),
                bytes,
                time as u32 as i64,
                select as i64,
            ],
        );
    }
}

/// `DrainInput`: runs the queued physics tick messages (one while the timer runs): the world checksummed or dumped
/// when asked, the owed ticks and the speed taken, the tick's player inputs executed; a stray message at the head is
/// dropped.
fn drain_input(p: P) {
    h(O::ServerLockEnter, p, &[]);
    while h(O::ServerQueueSize, p, &[]) > 0 {
        if get!(rec(p), mTicksToProcess) != 0 && h(O::TimerRunning, p, &[]) != 0 {
            break;
        }
        let message = hp(O::ServerQueueFront, p, &[]);
        if h(O::MsgType, message, &[]) != MESSAGE_PHYSICS_TICK {
            h(O::ServerQueuePopFront, p, &[]);
            h(O::MsgDereference, message, &[]);
            continue;
        }
        h(O::ServerQueuePopFront, p, &[]);
        if b(O::RecBool, message, &[2]) && (h0(O::IsServer) != 0 || (h0(O::IsClient) != 0 && h0(O::ClientHasLeft) == 0))
        {
            let dump = b(O::RecBool, message, &[3]);
            let checksum = out_of_synch_checksum(p, dump, 1);
            if !dump {
                let tick = h0(O::CtsInPhysics);
                let response = hp(O::NewChecksumResponse, 0, &[tick, checksum as i64]);
                h(O::MsgSetOriginatingSlot, response, &[h(O::PlayerSlot, hp(O::CurrentPlayer, 0, &[]), &[])]);
                if h0(O::IsClient) != 0 {
                    h(O::ProcessLocalInput, response, &[SLOT_NONE]);
                } else {
                    h(O::LogChecksumResponse, response, &[]);
                    h(O::MsgDereference, response, &[]);
                }
            }
        }
        h0(O::IncrementCts);
        let r = rec(p);
        h(O::SetAverageUpdate, 0, &[get!(r, mAveragePhysicsUpdateTime) as i64]);
        h(O::SetAverageGap, 0, &[get!(r, mAverageTimeBetweenPhysicsTicks) as i64]);
        let ticks = h(O::RecU8, message, &[1]) as c_ulong;
        set!(r, mCurrentTick, get!(r, mCurrentTick).wrapping_add(ticks));
        set!(r, mTicksToProcess, h(O::RecU8, message, &[1]) as c_ulong);
        hf(O::SetSpeed, p, &[], &[1.0f32 / h(O::RecU8, message, &[4]) as u8 as f32]);
        let mut count = h(O::RecU32, message, &[0]) as u32;
        h(O::MsgDereference, message, &[]);
        h0(O::CtsInPhysics);
        while count > 0 {
            let input = hp(O::ServerQueueFront, p, &[]);
            h(O::ServerQueuePopFront, p, &[]);
            if h(O::MsgForSpecificSlot, input, &[]) == SLOT_NONE {
                h(O::SetInputMessages, p, &[(h(O::InputMessages, p, &[]) as c_long).wrapping_add(1) as i64]);
            }
            // TEST ONLY: the simulation capture's NMEX record; the unit store before the message; the UI fixture
            h(O::CaptureExecuted, input, &[]);
            if h0(O::SimCaptureActive) != 0 {
                h(O::UnitStoreCheck, 0, &[h(O::MsgType, input, &[])]);
            }
            if h0(O::TestHookSet) != 0 {
                h(O::TestHookCall, input, &[0]);
            }
            let ty = h(O::MsgType, input, &[]);
            let game_time = get!(rec(p), mGameTime);
            if ty == MESSAGE_REMOVE_PLAYER {
                h(O::RemovePlayer, input, &[]);
            } else if SELECTION_MESSAGES.contains(&ty) {
                h(O::ExecuteSelection, input, &[]);
                physics_log(input, game_time, 1);
            } else {
                physics_log(input, game_time, 0);
                let game = hp(O::GameCast, input, &[]);
                if game != 0 {
                    h(O::ExecuteGame, game, &[]);
                } else {
                    h(O::Execute, input, &[]);
                }
            }
            if h0(O::TestHookSet) != 0 {
                h(O::TestHookCall, input, &[1]);
            }
            h(O::MsgDereference, input, &[]);
            count -= 1;
        }
    }
    h(O::ServerLockLeave, p, &[]);
}

/// The two statistics the current player last reported, and the report clock (`ReportStatistics`' statics).
struct Report {
    last: Option<u32>,
    period: u32,
    first: c_ulong,
    second: c_ulong,
}

static REPORT: Mutex<Report> = Mutex::new(Report { last: None, period: 0, first: 0, second: 0 });

/// `ReportStatistics`: every 2.5 .. 5 s (a random period drawn once), the current player's two statistics to the
/// others when they changed.
fn report_statistics(_p: P) {
    let (first, second) = {
        let mut report = REPORT.lock().unwrap_or_else(|e| e.into_inner());
        let last = match report.last {
            Some(last) => last,
            None => {
                let now = time();
                report.last = Some(now);
                // the first call also draws the period
                h(O::Srand, 0, &[time() as i64]);
                report.period = (h0(O::Rand) as u32 % 2500).wrapping_add(2500);
                now
            }
        };
        let now = time();
        if !(now.wrapping_sub(last) > report.period) {
            return;
        }
        report.last = Some(now);
        let player = hp(O::CurrentPlayer, 0, &[]);
        let first = h(O::PlayerSavedGroupLast, player, &[0x14]) as c_ulong;
        let second = h(O::PlayerSavedGroupLast, player, &[0x15]) as c_ulong;
        if first == report.first && second == report.second {
            return;
        }
        report.first = first;
        report.second = second;
        (first, second)
    };
    let message = hp(O::NewReportStatistics, 0, &[first as i64, second as i64]);
    h(O::ProcessLocalInput, message, &[SLOT_NONE]);
}

/// `QueueMessage`: a load/save request queued under the save-game lock, referenced.
fn queue_message(p: P, message: P) {
    h(O::SaveLockEnter, p, &[]);
    h(O::LoadSavePush, p, &[message as i64]);
    h(O::MsgReference, message, &[]);
    h(O::SaveLockLeave, p, &[]);
}

/// `SwapUnit`: the camera follows the new unit (or drops the old one); the contagions, the scenario and the effects
/// switch to it.
fn swap_unit(old: P, new: P) {
    let unit_a = h0(O::CameraUnitA) as P;
    if h0(O::CameraUnitB) as P == old && unit_a == old {
        h(O::CameraFollow, 0, &[new as i64]);
    } else {
        if unit_a == old {
            h0(O::CameraClearA);
        }
        if h0(O::CameraUnitB) as P == old {
            h0(O::CameraClearB);
        }
    }
    h(O::ContagionSwapUnit, 0, &[old as i64, new as i64]);
    h(O::ScenarioReplaceUnit, 0, &[h(O::UnitId, old, &[]), h(O::UnitId, new, &[])]);
    h(O::EffectReplaceUnit, 0, &[old as i64, new as i64]);
}

/// `ProcessUnitSwaps`: every player but player 0 told, each pair swapped and both units released, the queue cleared.
fn process_unit_swaps(p: P) {
    if h(O::SwapCount, p, &[]) <= 0 {
        return;
    }
    for i in 1..h0(O::NumPlayers) {
        h(O::PlayerUnitsSwapped, hp(O::Player, 0, &[i]), &[p as i64]);
    }
    let mut k = 0;
    while k < h(O::SwapCount, p, &[]) {
        let old = hp(O::SwapOld, p, &[k]);
        let new = hp(O::SwapNew, p, &[k]);
        swap_unit(old, new);
        h(O::UnitDereference, old, &[]);
        h(O::UnitDereference, new, &[]);
        k += 1;
    }
    h(O::SwapClear, p, &[]);
}

/// `UpdateTiming`: the ticks per game second and the tick's length (s and ms) from the tick rate and the game speed.
fn update_timing(p: P) {
    let r = rec(p);
    set!(r, mTicksPerGameSecond, h(O::TicksPerSecond, p, &[]) as c_ulong as f32 / hd(O::GameSpeed, 0, &[]));
    set!(r, mTickSeconds, 1.0f32 / get!(r, mTicksPerGameSecond));
    set!(r, mTickTime, get!(r, mTickSeconds) * 1000.0f32);
    set!(r, mTickTimeMs, lrintf(get!(r, mTickTime)) as c_long);
}

/// `QueueUnitSwap`: `old` replaced by `new` at the next flush, both referenced.
fn queue_unit_swap(p: P, old: P, new: P) {
    h(O::UnitReference, old, &[]);
    h(O::UnitReference, new, &[]);
    h(O::SwapPush, p, &[old as i64, new as i64]);
}

/// `UpdateAreaEffectsClock`: the area effects tick once per 2 s of game time.
fn update_area_effects_clock(p: P) {
    let r = rec(p);
    set!(r, mAreaEffectsClock, get!(r, mAreaEffectsClock) + get!(r, mTickTime));
    if get!(r, mAreaEffectsClock) >= AREA_EFFECT_INTERVAL {
        hf(O::AreaEffectsTick, 0, &[], &[AREA_EFFECT_INTERVAL]);
        set!(r, mAreaEffectsClock, get!(r, mAreaEffectsClock) - AREA_EFFECT_INTERVAL);
    }
}

/// `UpdateCivTimer`: on a server, the warnings at 240 s and 270 s and the expiry at 300 s, and the civilization
/// builder's button blinks for a player with no civilization; elsewhere (or with no civilization to buy) the timer
/// stops.
fn update_civ_timer(p: P) {
    let r = rec(p);
    let current = || hp(O::CurrentPlayer, 0, &[]);
    if h0(O::CommsIsServer) != 0 {
        if h0(O::CanBuyCivilization) != 0 {
            if get!(r, mTickWorkCount) == 0
                && !b(O::PlayerCivChosen, current(), &[])
                && !b(O::PlayerObserver, current(), &[])
            {
                h(O::CivBuilderBlink, 0, &[1]);
            }
            let game_time = get!(r, mGameTime) as c_ulong;
            let message = if game_time > h0(O::CivTimerLimit) as c_ulong {
                set!(r, mCivTimerRuns, false);
                hp(O::NewCivTimerExpired, 0, &[0])
            } else if game_time > h0(O::CivTimerSecondWarning) as c_ulong && !get!(r, mCivTimerSecondWarning) {
                set!(r, mCivTimerSecondWarning, true);
                hp(O::NewCivTimerExpired, 0, &[1])
            } else if game_time > h0(O::CivTimerFirstWarning) as c_ulong && !get!(r, mCivTimerFirstWarning) {
                set!(r, mCivTimerFirstWarning, true);
                hp(O::NewCivTimerExpired, 0, &[1])
            } else {
                return;
            };
            if message != 0 {
                h(O::ProcessLocalInput, message, &[SLOT_NONE]);
            }
        } else {
            set!(r, mCivTimerRuns, false);
        }
    } else {
        set!(r, mCivTimerRuns, false);
        if h0(O::CanBuyCivilization) != 0
            && !b(O::PlayerCivChosen, current(), &[])
            && !b(O::PlayerObserver, current(), &[])
        {
            h(O::CivBuilderBlink, 0, &[1]);
        }
    }
}

/// `ServeLoadSaveRequests`: between ticks every queued load/save request runs (under the save-game lock) and one
/// response carries the last result (logged on a server); with ticks owed it waits.
fn serve_load_save_requests(p: P) {
    h(O::SaveLockEnter, p, &[]);
    if h(O::LoadSaveCount, p, &[]) > 0 {
        if get!(rec(p), mTicksToProcess) == 0 {
            let mut result = 0;
            let mut i = 0;
            while i < h(O::LoadSaveCount, p, &[]) {
                result = h(O::LoadOrSave, hp(O::LoadSaveAt, p, &[i]), &[]);
                h(O::MsgDereference, hp(O::LoadSaveAt, p, &[i]), &[]);
                h(O::LoadSaveSetNull, p, &[i]);
                i += 1;
            }
            h(O::LoadSaveClear, p, &[]);
            let response = hp(O::NewLoadSaveResponse, 0, &[result]);
            h(O::MsgSetOriginatingSlot, response, &[h(O::PlayerGetSlot, hp(O::CurrentPlayer, 0, &[]), &[])]);
            if h0(O::IsServer) != 0 {
                h(O::LogResponse, response, &[]);
                h(O::MsgDereference, response, &[]);
            } else {
                h(O::ProcessLocalInput, response, &[SLOT_SERVER]);
            }
        } else {
            h0(O::CtsInPhysics);
            h0(O::CurrentCommsTick);
        }
    }
    h(O::SaveLockLeave, p, &[]);
}

/// `CheckShiftMode`: every 4 s of game time the current player leaves shift mode when the Shift key is up.
fn check_shift_mode(p: P) {
    let r = rec(p);
    set!(r, mShiftModeClock, get!(r, mShiftModeClock) + get!(r, mTickTime));
    if !(get!(r, mShiftModeClock) >= 4000.0) {
        return;
    }
    set!(r, mShiftModeClock, get!(r, mShiftModeClock) - 4000.0);
    if !b(O::PlayerShiftMode, hp(O::CurrentPlayer, 0, &[]), &[]) {
        return;
    }
    if h0(O::ShiftKeyUp) != 0 {
        let message = hp(O::NewExitShiftMode, 0, &[get!(r, mGameTime) as i64]);
        h(O::ProcessLocalInput, message, &[SLOT_NONE]);
    }
}

/// `UpdateWeather`: the day/night clock (when it runs) and the wind direction's random walk, then the environment.
fn update_weather(p: P) {
    let r = rec(p);
    if get!(r, mTimeOfDayRuns) {
        let mut t = get!(r, mTickSeconds) * 0.0016666667 + get!(r, mTimeOfDayStep);
        if t >= 0.0 {
            let mut time_of_day = t + hd(O::TimeOfDay, 0, &[]);
            if time_of_day >= 1.0 {
                time_of_day = 0.0;
            }
            hf(O::SetTimeOfDay, 0, &[], &[time_of_day]);
            t = 0.0;
        }
        set!(r, mTimeOfDayStep, t);
    }
    let angle = hd(O::WindYaw, 0, &[]);
    let elapsed = get!(r, mTickTime);
    let yaw = ((hd(O::Random, p, &[]) * 2.0 - 1.0) * elapsed) * WIND_DRIFT + angle;
    hf(O::SetWindYaw, 0, &[], &[yaw]);
    let yaw = hd(O::WindYaw, 0, &[]);
    let mut transformation = GeTransformation::IDENTITY;
    transformation.set_orientation(RotationOrder::Ypr, yaw, 0.0, 0.0);
    let wind = transformation.apply_vector(GeVector::new(0.0, 0.001, 0.0));
    hf(O::SetWindDirection, 0, &[], &[wind.x, wind.y, wind.z]);
    h(O::EnvironmentTick, 0, &[get!(r, mGameTime) as i64]);
}

// ---- the C++ class's forwards (EEPhysics.cpp)

fn this(p: *mut c_void) -> P {
    p as P
}

#[no_mangle]
pub extern "C" fn ee_physics_init(p: *mut c_void) {
    init(this(p))
}

#[no_mangle]
pub extern "C" fn ee_physics_clear_server_messages(p: *mut c_void) {
    clear_server_messages(this(p))
}

#[no_mangle]
pub extern "C" fn ee_physics_auto_save(p: *mut c_void) {
    auto_save(this(p))
}

#[no_mangle]
pub extern "C" fn ee_physics_flush_world(p: *mut c_void) {
    flush_world(this(p))
}

/// `RunOwedTicks`: every owed tick, draining the input queue for more whenever none are left.
#[no_mangle]
pub extern "C" fn ee_physics_run_owed_ticks(p: *mut c_void) {
    let p = this(p);
    if get!(rec(p), mTicksToProcess) == 0 {
        drain_input(p);
    }
    while get!(rec(p), mTicksToProcess) > 0 {
        tick(p);
        if get!(rec(p), mTicksToProcess) == 0 {
            drain_input(p);
        }
    }
}

#[no_mangle]
pub extern "C" fn ee_physics_end_game(p: *mut c_void, victory: bool) {
    end_game(this(p), victory)
}

#[no_mangle]
pub extern "C" fn ee_physics_out_of_synch_checksum(p: *mut c_void, dump: bool, flags: c_ulong) -> c_ulong {
    out_of_synch_checksum(this(p), dump, flags)
}

/// `GetTimeString` into `out` (room for `capacity` bytes, NUL-terminated).
///
/// # Safety
/// `out` has room for `capacity` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_physics_time_string(p: *mut c_void, out: *mut c_char, capacity: usize) {
    let text = time_string(this(p));
    let n = text.len().min(capacity.saturating_sub(1));
    // SAFETY: the caller's guarantee
    unsafe {
        std::ptr::copy_nonoverlapping(text.as_ptr(), out as *mut u8, n);
        *out.add(n) = 0;
    }
}

/// `GetMinutesAndSeconds`: a game time in ms (0: the current one) as whole minutes and the seconds past them.
///
/// # Safety
/// Both outputs are writable.
#[no_mangle]
pub unsafe extern "C" fn ee_physics_minutes_and_seconds(
    p: *mut c_void,
    minutes: *mut c_ulong,
    seconds: *mut c_ulong,
    time: c_ulong,
) {
    let time = if time == 0 { get!(rec(this(p)), mGameTime) as c_ulong } else { time };
    let m = time / 1000 / 60;
    // SAFETY: the caller's guarantee
    unsafe {
        *minutes = m;
        *seconds = time.wrapping_sub(m.wrapping_mul(60000)) / 1000;
    }
}

#[no_mangle]
pub extern "C" fn ee_physics_unit_swap_kind(p: *mut c_void, unit: *mut c_void) -> c_long {
    unit_swap_kind(this(p), unit as P)
}

#[no_mangle]
pub extern "C" fn ee_physics_handle_message(
    p: *mut c_void,
    message: u32,
    parameter1: *mut c_void,
    parameter2: *mut c_void,
) {
    handle_message(this(p), message, parameter1 as usize, parameter2 as usize)
}

#[no_mangle]
pub extern "C" fn ee_physics_idle(p: *mut c_void) {
    idle(this(p))
}

#[no_mangle]
pub extern "C" fn ee_physics_tick(p: *mut c_void) {
    tick(this(p))
}

#[no_mangle]
pub extern "C" fn ee_physics_queue_message(p: *mut c_void, message: *mut c_void) {
    queue_message(this(p), message as P)
}

#[no_mangle]
pub extern "C" fn ee_physics_swap_unit(old: *mut c_void, new: *mut c_void) {
    swap_unit(old as P, new as P)
}

#[no_mangle]
pub extern "C" fn ee_physics_process_unit_swaps(p: *mut c_void) {
    process_unit_swaps(this(p))
}

#[no_mangle]
pub extern "C" fn ee_physics_drain_input(p: *mut c_void) {
    drain_input(this(p))
}

#[no_mangle]
pub extern "C" fn ee_physics_report_statistics(p: *mut c_void) {
    report_statistics(this(p))
}

/// `PostAutoSave` (0x50a), `PostShowDialogMessage` (0x508), `PostInit` (0x509): the message posted to the physics
/// thread -> the post's result (0 with no thread).
#[no_mangle]
pub extern "C" fn ee_physics_post(p: *mut c_void, message: u32) -> c_long {
    h(O::ThreadPost, this(p), &[message as i64]) as c_long
}

#[no_mangle]
pub extern "C" fn ee_physics_update_timing(p: *mut c_void) {
    update_timing(this(p))
}

#[no_mangle]
pub extern "C" fn ee_physics_queue_unit_swap(p: *mut c_void, old: *mut c_void, new: *mut c_void) {
    queue_unit_swap(this(p), old as P, new as P)
}

#[no_mangle]
pub extern "C" fn ee_physics_update_area_effects_clock(p: *mut c_void) {
    update_area_effects_clock(this(p))
}

#[no_mangle]
pub extern "C" fn ee_physics_update_civ_timer(p: *mut c_void) {
    update_civ_timer(this(p))
}

#[no_mangle]
pub extern "C" fn ee_physics_serve_load_save_requests(p: *mut c_void) {
    serve_load_save_requests(this(p))
}

#[no_mangle]
pub extern "C" fn ee_physics_check_shift_mode(p: *mut c_void) {
    check_shift_mode(this(p))
}

#[no_mangle]
pub extern "C" fn ee_physics_update_weather(p: *mut c_void) {
    update_weather(this(p))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_selection_messages_are_the_elevens() {
        assert_eq!(SELECTION_MESSAGES.len(), 11);
        assert!(SELECTION_MESSAGES.contains(&0x48) && SELECTION_MESSAGES.contains(&0x96));
        assert!(!SELECTION_MESSAGES.contains(&MESSAGE_REMOVE_PLAYER));
    }

    #[test]
    fn a_loaded_seed_is_pending_until_spent() {
        ee_physics_loaded_seed(77);
        assert!(LOADED_SEED_PENDING.load(Ordering::Relaxed));
        assert_eq!(LOADED_SEED.load(Ordering::Relaxed), 77);
        LOADED_SEED_PENDING.store(false, Ordering::Relaxed);
    }

    #[test]
    fn the_time_string_formats_as_sprintf() {
        assert_eq!(format!("{:03.2}", 1.0f32), "1.00");
        assert_eq!(format!("{:03.2} {:02}:{:02}:{:02}", 1.5f32, 1, 2, 3), "1.50 01:02:03");
        assert_eq!(format!("{:4}", 7), "   7");
        assert_eq!(format!("{:08x}", 0x1234u32), "00001234");
    }

    #[test]
    fn the_wind_points_north_rotated_by_the_yaw() {
        let mut t = GeTransformation::IDENTITY;
        t.set_orientation(RotationOrder::Ypr, 0.0, 0.0, 0.0);
        let wind = t.apply_vector(GeVector::new(0.0, 0.001, 0.0));
        assert_eq!((wind.x, wind.y, wind.z), (0.0, 0.001, 0.0));
    }
}
