//! TEST ONLY: what EE-SIM's driver ([`super::sim_host`]) asks of the game's C++ objects beyond the host table: one
//! host operation per C++ call or member (EESim.cpp `ee_simcpp_sim`, `ee_simcpp_sim_text`, `ee_simcpp_sim_float`: a
//! one-statement case each). The pattern of [`super::sed_host`].

use crate::gamecpp as gc;
use std::ffi::{c_char, CStr, CString};

/// The host operations (`p` the object, `a` and `f` the integer and float arguments, `s` a text). A pointer is
/// answered as an integer.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeSimDriverSim {
    // the user interface
    EditorForm = 1,
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
    // the physics and the world
    PhysicsTickMs,
    PhysicsTickSeconds,
    PhysicsFlushPlacements,
    MapPresent,
    NumPlayers,
    PlayerSetCivilizationChosen,
    PlayerSetCivilizationLocked,
    PlayerSetStance,
    PlayerAllyCount,
    PlayerId,
    PlayerAddPendingUnits,
    NodeData,
    NodeCost,
    RequestSaveNew,
    SendToAllAndWait,
    DerefMessage,
    // the platform switches
    SetScenarioDialogs,
    // the units
    UnitHitPoints,
    UnitHasStats,
    UnitIsBuilding,
    UnitType,
    UnitId,
    UnitTileX,
    UnitTileY,
    ComplexOnMap,
    ComplexAttack,
    ComplexIsPriest,
    ComplexCategory,
    ComplexClass,
    ComplexLocation,
    ComplexSetLocation,
    ComplexPosition,
    ComplexSetGoal,
    ComplexVelocity,
    ComplexFlightTime,
    ComplexHasGoalState,
    ComplexPlayer,
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
    ClassCategory,
    ClassProjectile,
    ClassAcceleration,
    ClassCreateUnit,
    ClassLaunchProjectile,
    ClassStep,
    ClassWaterImpact,
    ClassGroundImpact,
    // the capture and its sweeps
    PhysicsRandom,
    GroupCommandSweep,
    MiniMapSnapshot,
    TradeFoodWood,
    TradeFoodWood2,
    TradeStoneIron,
    TradeStoneIron2,
    RegenerationInterval,
    ConversionFaithCost,
    DialogShowing,
    ShowDialogMessage,
    // the map's sight (the app sweep)
    WonderSightRadius,
    MapAddSight,
    MapRemoveSight,
    MapExploreUnseen,
    MapFogSeen,
    // the state dump (dump.rs)
    DmPhysLastOos,
    DmPhysSetLastOos,
    DmPhysOos,
    DmPhysTicksDone,
    DmPhysCurrentTick,
    DmPhysSeed,
    DmPhysInputs,
    DmPhysTickRandom,
    DmPhysTicksToProcess,
    DmWorldFirstUnit,
    DmUnitChecksum,
    DmComplexChecksum,
    DmActiveUnitId,
    DmPlPop,
    DmPlEog,
    DmPlEpoch,
    DmPlTownCount,
    DmPlTownAt,
    DmTownId,
    DmTownUnits,
    DmTownPatrol,
    DmTownHullCount,
    DmTownHullX,
    DmTownHullY,
    DmPlPatrolCount,
    DmPlPatrolAt,
    DmCpDump,
    DmPathDump,
    DmUnitTypeRaw,
    DmUnitPlayerId,
    DmComplexOf,
    DmCxComplex,
    DmCxUpdated,
    DmCxCommand,
    DmCxGroup,
    DmCxWpCount,
    DmCxPathDone,
    DmCxArrived,
    DmCxBlockingId,
    DmCxAnim,
    DmCxFrame,
    DmCxAnimTime,
    DmCxPaused,
    DmCxReload,
    DmCxQueues,
    DmCxQueueSize,
    DmCxGoal,
    DmGoalKind,
    DmGoalOos,
    DmGoalDump,
    DmCxContactHead,
    DmNodeNext,
    DmNodeUnitId,
    DmCxAttackerId,
    DmCxOwnerId,
    DmCxHolderId,
    DmCxAeCount,
    DmCxAffectingCount,
    DmCxAffectingUnitId,
    DmCxSize,
    DmUnitSize,
    DmCalCount,
    DmCalAt,
    DmCalClass,
    DmCalOos,
    DmCalTileX,
    DmCalTileY,
    DmCalInt1,
    DmCalInt2,
    DmCalTargetId,
    DmCalOriginId,
    DmCalTargetUnitId,
    DmCalCasterId,
    DmCalPlayer,
    DmCalBool1,
    DmCalBool2,
    DmCalInitialized,
    DmCalHasLocation,
    DmCalHasUnit,
    DmMapRegions,
    DmPhysClock,
    DmPlResource,
    DmUnitHp,
    DmCxLocX,
    DmCxLocY,
    DmCxLocZ,
    DmCxHeight,
    DmCxWpX,
    DmCxWpY,
    DmCxWpZ,
    DmCxWpDistance,
    DmCxWpStart,
    DmCxAeX,
    DmCxAeY,
    DmCalTargetX,
    DmCalTargetY,
    DmCalTargetZ,
    DmCalFloat1,
    DmCalFloat2,
    DmCalHeight,
    DmCalEpicenter,
    DmCalTimeAlive,
    DmCalDiscrete,
    DmCalElapsed,
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

/// A double (a C++ float or double, as the C++ widened it).
pub fn hdd(op: O, p: usize, a: &[i64]) -> f64 {
    let mut ai = [0i64; 6];
    ai[..a.len()].copy_from_slice(a);
    let fi = [0f32; 4];
    // SAFETY: as call
    unsafe { sys::ee_simcpp_sim_float(op, p, ai.as_ptr(), fi.as_ptr()) }
}

/// The user interface's copy lock held while it lives (`UILock`).
pub struct UiLock;

impl UiLock {
    pub fn hold() -> UiLock {
        gc::h(gc::O::UiLockForCopy, 0, &[]);
        UiLock
    }
}

impl Drop for UiLock {
    fn drop(&mut self) {
        gc::h(gc::O::UiUnlockForCopy, 0, &[]);
    }
}
