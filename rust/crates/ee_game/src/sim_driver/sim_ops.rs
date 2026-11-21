//! TEST ONLY: what EE-SIM's driver ([`super::sim_host`]) asks of the game's C++ objects beyond the host table: one
//! host operation per C++ call or member (EESim.cpp `ee_simcpp_sim`, `ee_simcpp_sim_text`, `ee_simcpp_sim_float`: a
//! one-statement case each). The pattern of [`super::sed_host`].

use std::ffi::{c_char, CStr, CString};

/// The host operations (`p` the object, `a` and `f` the integer and float arguments, `s` a text). A pointer is
/// answered as an integer.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeSimDriverSim {
    // the user interface
    UiPresent = 1,
    UiLock,
    UiUnlock,
    CurrentForm,
    EditorForm,
    FormId,
    FormShellOf,
    ShellKindName,
    FormTypeName,
    FormControlCount,
    FormControl,
    ControlTriggerEvent,
    ControlFire,
    ComboOf,
    ComboList,
    ComboSelection,
    ComboSetSelection,
    ListOf,
    ListLineCount,
    ListData,
    ListSelection,
    ListChangeSelection,
    ScreenEventOf,
    ScreenEventCode,
    SpLoadGameFileList,
    LoadScenarioFileList,
    // the physics and the world
    PhysicsPresent,
    PhysicsTimerRunning,
    PhysicsGameOver,
    PhysicsGameTime,
    PhysicsTicksDone,
    PhysicsTickMs,
    PhysicsTickSeconds,
    PhysicsFlushPlacements,
    MapPresent,
    CurrentPlayer,
    NumPlayers,
    GetPlayer,
    PlayerSetCivilizationChosen,
    PlayerSetCivilizationLocked,
    PlayerSetStance,
    PlayerFirstUnit,
    PlayerAllyCount,
    PlayerAllyAt,
    PlayerFlareReceived,
    PlayerTechnologies,
    PlayerId,
    PlayerAddPendingUnits,
    TreeGet,
    NodeData,
    NodeCost,
    CastLocationCalamity,
    VolcanoProjectileType,
    RequestSaveNew,
    SendToAllAndWait,
    DerefMessage,
    // the platform switches
    SetSimMode,
    SetSeedSet,
    SetSeed,
    SetNoHumanDefeat,
    SetScenarioDialogs,
    SetPortOffset,
    AnyPort,
    ClearNoHumanDefeatHits,
    NoHumanDefeatHits,
    // the rules and the engines
    RulesAdd,
    RulesError,
    RulesProblemsLoad,
    RulesProblemCount,
    RulesProblem,
    EngineId,
    // the units
    UnitNext,
    UnitComplex,
    UnitHitPoints,
    UnitHasStats,
    UnitIsBuilding,
    UnitType,
    UnitId,
    UnitTileX,
    UnitTileY,
    UnitPlayer,
    ComplexOf,
    ComplexOnMap,
    ComplexTileX,
    ComplexTileY,
    ComplexAttack,
    ComplexIsPriest,
    ComplexCategory,
    ComplexClass,
    ComplexLocation,
    ComplexSetLocation,
    ComplexSetLauncherClass,
    ComplexSetOwner,
    ComplexPosition,
    ComplexSetGoal,
    ComplexVelocity,
    ComplexSetVelocity,
    ComplexFlightTime,
    ComplexSetFlightTime,
    ComplexHasGoalState,
    ComplexPlayer,
    ComplexHitPoints,
    UnitGetComplex,
    UnitTileXRaw,
    IgnoredSave,
    IgnoredRestore,
    IgnoredFree,
    SearchAlongList,
    NewAttackUnit,
    NewAttackGround,
    CanHeal,
    UpgradeProjectile,
    // the unit classes
    ClassCount,
    ClassAt,
    ClassCategory,
    ClassProjectile,
    ClassAcceleration,
    ClassCreateUnit,
    ClassLaunchProjectile,
    ClassStep,
    ClassWaterImpact,
    ClassGroundImpact,
    MissileCategory,
    TorpedoCategory,
    // the capture and its sweeps
    CaptureActive,
    CaptureOnly,
    CaptureOnlyList,
    CaptureExclusive,
    CaptureHold,
    CaptureClose,
    CombatFlush,
    FormationSweep,
    TechSweep,
    CalamitySweep,
    GroupCommandSweep,
    MessageSweep,
    CastCalamities,
    MiniMapPresent,
    MiniMapSnapshot,
    SightSnapshot,
    CheatSweep,
    StormSweep,
    EffectSweep,
    StatisticsSweep,
    TradeFoodWood,
    TradeFoodWood2,
    TradeStoneIron,
    TradeStoneIron2,
    RegenerationInterval,
    ConversionFaithCost,
    DialogShowing,
    ShowDialogMessage,
    // the map's sight (the app sweep)
    MapSize,
    WonderSightRadius,
    MapAddSight,
    MapRemoveSight,
    MapExploreUnseen,
    MapFogSeen,
    MapComputeChecksum,
    WorldNumPlayers,
    WorldPlayersAt,
}

use EeSimDriverSim as O;

/// cbindgen:ignore
mod sys {
    use std::ffi::c_char;

    use super::EeSimDriverSim;

    extern "C" {
        // EESim.cpp
        pub fn ee_simcpp_sim(op: EeSimDriverSim, p: usize, a: *const i64, f: *const f32, s: *const c_char) -> i64;
        pub fn ee_simcpp_sim_text(op: EeSimDriverSim, p: usize, a: i64) -> *const c_char;
        pub fn ee_simcpp_sim_float(op: EeSimDriverSim, p: usize, a: *const i64, f: *const f32) -> f64;
    }
}

fn call(op: O, p: usize, a: &[i64], f: &[f32], s: Option<&str>) -> i64 {
    let mut ai = [0i64; 6];
    ai[..a.len()].copy_from_slice(a);
    let mut fi = [0f32; 4];
    fi[..f.len()].copy_from_slice(f);
    let text = s.map(|s| CString::new(s.replace('\0', "")).expect("no NUL"));
    // SAFETY: the host's operation over the C++ objects the caller holds (the locks the C++ took)
    unsafe {
        sys::ee_simcpp_sim(op, p, ai.as_ptr(), fi.as_ptr(), text.as_ref().map_or(std::ptr::null(), |t| t.as_ptr()))
    }
}

/// An operation with integer arguments.
pub fn h(op: O, p: usize, a: &[i64]) -> i64 {
    call(op, p, a, &[], None)
}

/// An operation with integer and float arguments.
pub fn hf(op: O, p: usize, a: &[i64], f: &[f32]) -> i64 {
    call(op, p, a, f, None)
}

/// An operation with a text.
pub fn hs(op: O, p: usize, s: &str) -> i64 {
    call(op, p, &[], &[], Some(s))
}

/// A text the object holds (NULL: empty).
pub fn ht(op: O, p: usize, a: i64) -> String {
    // SAFETY: as call; the text is copied at once
    unsafe {
        let t = sys::ee_simcpp_sim_text(op, p, a);
        if t.is_null() {
            String::new()
        } else {
            CStr::from_ptr(t as *const c_char).to_string_lossy().into_owned()
        }
    }
}

/// A float (a C++ float, widened).
pub fn hd(op: O, p: usize, a: &[i64], f: &[f32]) -> f32 {
    let mut ai = [0i64; 6];
    ai[..a.len()].copy_from_slice(a);
    let mut fi = [0f32; 4];
    fi[..f.len()].copy_from_slice(f);
    // SAFETY: as call
    unsafe { sys::ee_simcpp_sim_float(op, p, ai.as_ptr(), fi.as_ptr()) as f32 }
}

/// The user interface's copy lock held while it lives (`UILock`).
pub struct UiLock;

impl UiLock {
    pub fn hold() -> UiLock {
        h(O::UiLock, 0, &[]);
        UiLock
    }
}

impl Drop for UiLock {
    fn drop(&mut self) {
        h(O::UiUnlock, 0, &[]);
    }
}
