//! The game's global objects' storage, the references the original's code made to their members and the save
//! version, stored by Rust under the C++ symbols (were EEGlobals.cpp's `EE_GLOBAL_STORAGE` lines, reference
//! definitions and constants; game core lane, round 13, fork A).
//!
//! - **Objects** (gWorld, gGamePreferences, the tables, the managers, the strings, the unit class vectors): zeroed
//!   bytes aligned to 16 under the object's symbol. C++ constructs each in place (`EE_CONSTRUCT_GLOBAL` in Empire
//!   Earth.cpp, EEUnitClass.cpp and EEScreens.cpp: ee/EEGlobalStorage.h) and reads it through the headers' `extern`
//!   declarations. A storage is [`EE_GAME_STORAGE_FACTOR`] times its `EE_GAME_SIZE_*` (the Windows `sizeof`): the
//!   LP64 builds' `long`s and standard containers are larger. `EE_CONSTRUCT_GLOBAL` static_asserts that the type fits
//!   (size and alignment), on every platform, so a class that outgrows its storage stops the build.
//! - **References** (`long& gNumPlayers`-style globals, which C++ stores as a pointer): a pointer to the member, the
//!   object's storage plus the member's offset. The offsets come from `#[repr(C)]` mirrors of the members' path
//!   (the classes' plain members are already Rust's `#[repr(C)]` structs: EeAppSettingsFields, EeAppOptionsFields,
//!   EeAppOptionsPlayerFields, EeUString, EeUWideString, EeAStarCmRecord), so they hold on LP64 too; the test
//!   `offsets_match_the_build_layout` pins every one against the Windows build's record layouts
//!   (build/clang/layout.json, written by tools/harness/layout_gen.py at each build). gWorld's and gGameState's
//!   members past their standard containers stay C++ references (Empire Earth.cpp): their LP64 offsets depend on the
//!   standard library (libstdc++'s std::map is 48 bytes, libc++'s 24).
//!
//! Rust never reads or writes these: they are storage. Symbols: MSVC's mangled names on Windows (the link map), the
//! plain (Itanium) names elsewhere.
#![allow(non_upper_case_globals, non_snake_case)]

use core::ffi::{c_ulong, c_void};
use core::mem::offset_of;
use core::ptr::addr_of_mut;

use ee_base::ustring::{EeUString, EeUWideString};
use ee_sim::astar::cm::EeAStarCmRecord;

use crate::app::game_options::{EeAppOptionsFields, EeAppOptionsPlayerFields};
use crate::app::settings::EeAppSettingsFields;

/// A storage holds this many times its `EE_GAME_SIZE_*` bytes (ee/EEGlobalStorage.h's static_assert reads it).
pub const EE_GAME_STORAGE_FACTOR: usize = 2;

// The objects' sizes on Windows (`sizeof`: build/clang/layout.json; UString 24, UWideString 32, std::vector 24);
// each storage is EE_GAME_STORAGE_FACTOR times its size.
pub const EE_GAME_SIZE_gU530BC8: usize = 32;
pub const EE_GAME_SIZE_gSavedGameExtension: usize = 24;
pub const EE_GAME_SIZE_gScenarioExtension: usize = 24;
pub const EE_GAME_SIZE_gU530B88: usize = 24;
pub const EE_GAME_SIZE_gCampaignExtension: usize = 24;
pub const EE_GAME_SIZE_gViewport: usize = 648;
pub const EE_GAME_SIZE_gGamePreferences: usize = 2096;
pub const EE_GAME_SIZE_gWorld: usize = 3712;
pub const EE_GAME_SIZE_gEvent: usize = 24;
pub const EE_GAME_SIZE_gDatabaseObjects: usize = 16;
pub const EE_GAME_SIZE_gTechTree: usize = 88;
pub const EE_GAME_SIZE_gToHitTable: usize = 8;
pub const EE_GAME_SIZE_gAIUnitTargetingTable: usize = 8;
pub const EE_GAME_SIZE_gPathManager: usize = 24;
pub const EE_GAME_SIZE_gContinentManager: usize = 144;
pub const EE_GAME_SIZE_gWorldTable: usize = 32;
pub const EE_GAME_SIZE_gTerrainTable: usize = 184;
pub const EE_GAME_SIZE_gTerrainColorTable: usize = 24;
pub const EE_GAME_SIZE_gTerrainTypeTable: usize = 28;
pub const EE_GAME_SIZE_gGameState: usize = 1688;
pub const EE_GAME_SIZE_gComputerPlayerBehaviorTable: usize = 8;
pub const EE_GAME_SIZE_gUnitSetManager: usize = 32;
pub const EE_GAME_SIZE_gAEManager: usize = 64;
pub const EE_GAME_SIZE_gAmbientSoundManager: usize = 128;
pub const EE_GAME_SIZE_gCivilizationManager: usize = 8;
pub const EE_GAME_SIZE_gDoppelgangerManager: usize = 168;
pub const EE_GAME_SIZE_gCivPowersManager: usize = 8;
pub const EE_GAME_SIZE_gCampaignAOCRoman: usize = 32;
pub const EE_GAME_SIZE_gCampaignAOCPacific: usize = 32;
pub const EE_GAME_SIZE_gCampaignAOCAsian: usize = 32;
pub const EE_GAME_SIZE_gCampaignEEDemo: usize = 32;
pub const EE_GAME_SIZE_gCampaignEELearning: usize = 32;
pub const EE_GAME_SIZE_gU52A280: usize = 32;
pub const EE_GAME_SIZE_gPhysicsThreadName: usize = 24;
pub const EE_GAME_SIZE_gGraphicsThreadName: usize = 24;
pub const EE_GAME_SIZE_gDataPreLoadingThreadName: usize = 24;
pub const EE_GAME_SIZE_gMousePointerThreadName: usize = 24;
pub const EE_GAME_SIZE_gScenarioEditorThreadName: usize = 24;
pub const EE_GAME_SIZE_gMultiplayerString: usize = 24;
pub const EE_GAME_SIZE_gUnitClassTable: usize = 24;
pub const EE_GAME_SIZE_EEUnitClassUnitTypes: usize = 24;
pub const EE_GAME_SIZE_gGameVariants: usize = 1;

/// An object's storage: zeroed bytes, aligned to 16 (a compiler assumes that alignment for a global object of 16
/// bytes or more: clang's movaps on gWorld's members).
#[repr(C, align(16))]
pub struct Storage<const N: usize>(pub [u8; N]);

/// A reference global's pointer (C++ stores `T& gX = ...;` as a `T*`).
#[repr(transparent)]
pub struct EeRef(pub *mut u8);

// SAFETY: the pointer is a constant address; Rust never dereferences it.
unsafe impl Sync for EeRef {}

macro_rules! ee_objects {
    ($($name:ident: $size:expr => $msvc:literal, $plain:literal;)*) => {
        $(
            #[cfg_attr(windows, export_name = $msvc)]
            #[cfg_attr(not(windows), export_name = $plain)]
            pub static mut $name: Storage<{ $size * EE_GAME_STORAGE_FACTOR }> =
                Storage([0; $size * EE_GAME_STORAGE_FACTOR]);
        )*
        /// The objects' names, Windows sizes and symbols, for the tests.
        #[cfg(test)]
        pub(crate) const OBJECTS: &[(&str, usize, &str)] = &[$((stringify!($name), $size, $msvc),)*];
    };
}

ee_objects! {
    gU530BC8: EE_GAME_SIZE_gU530BC8 => "?gU530BC8@@3VUWideString@@A", "gU530BC8";
    gSavedGameExtension: EE_GAME_SIZE_gSavedGameExtension => "?gSavedGameExtension@@3VUString@@A", "gSavedGameExtension";
    gScenarioExtension: EE_GAME_SIZE_gScenarioExtension => "?gScenarioExtension@@3VUString@@A", "gScenarioExtension";
    gU530B88: EE_GAME_SIZE_gU530B88 => "?gU530B88@@3VUString@@A", "gU530B88";
    gCampaignExtension: EE_GAME_SIZE_gCampaignExtension => "?gCampaignExtension@@3VUString@@A", "gCampaignExtension";
    gViewport: EE_GAME_SIZE_gViewport => "?gViewport@@3VGEViewport@@A", "gViewport";
    gGamePreferences: EE_GAME_SIZE_gGamePreferences => "?gGamePreferences@@3VEEGamePreferences@@A", "gGamePreferences";
    gWorld: EE_GAME_SIZE_gWorld => "?gWorld@@3VEEWorld@@A", "gWorld";
    gEvent: EE_GAME_SIZE_gEvent => "?gEvent@@3VEEEvent@@A", "gEvent";
    gDatabaseObjects: EE_GAME_SIZE_gDatabaseObjects => "?gDatabaseObjects@@3VEEDatabase@@A", "gDatabaseObjects";
    gTechTree: EE_GAME_SIZE_gTechTree => "?gTechTree@@3VEETechTree@@A", "gTechTree";
    gToHitTable: EE_GAME_SIZE_gToHitTable => "?gToHitTable@@3VEEToHitTable@@A", "gToHitTable";
    gAIUnitTargetingTable: EE_GAME_SIZE_gAIUnitTargetingTable
        => "?gAIUnitTargetingTable@@3VEEAIUnitTargetingTable@@A", "gAIUnitTargetingTable";
    gPathManager: EE_GAME_SIZE_gPathManager => "?gPathManager@@3VEEPathManager@@A", "gPathManager";
    gContinentManager: EE_GAME_SIZE_gContinentManager
        => "?gContinentManager@@3VEEAIContinentManager@@A", "gContinentManager";
    gWorldTable: EE_GAME_SIZE_gWorldTable => "?gWorldTable@@3VEEWorldTable@@A", "gWorldTable";
    gTerrainTable: EE_GAME_SIZE_gTerrainTable => "?gTerrainTable@@3VEETerrainTable@@A", "gTerrainTable";
    gTerrainColorTable: EE_GAME_SIZE_gTerrainColorTable
        => "?gTerrainColorTable@@3VEETerrainColorTable@@A", "gTerrainColorTable";
    gTerrainTypeTable: EE_GAME_SIZE_gTerrainTypeTable
        => "?gTerrainTypeTable@@3VEETerrainTypeTable@@A", "gTerrainTypeTable";
    gGameState: EE_GAME_SIZE_gGameState => "?gGameState@@3VEEGameState@@A", "gGameState";
    gComputerPlayerBehaviorTable: EE_GAME_SIZE_gComputerPlayerBehaviorTable
        => "?gComputerPlayerBehaviorTable@@3VEEComputerPlayerBehaviorTable@@A", "gComputerPlayerBehaviorTable";
    gUnitSetManager: EE_GAME_SIZE_gUnitSetManager => "?gUnitSetManager@@3VEEUnitSetManager@@A", "gUnitSetManager";
    gAEManager: EE_GAME_SIZE_gAEManager => "?gAEManager@@3VEEAEManager@@A", "gAEManager";
    gAmbientSoundManager: EE_GAME_SIZE_gAmbientSoundManager
        => "?gAmbientSoundManager@@3VEEAmbientSoundManager@@A", "gAmbientSoundManager";
    gCivilizationManager: EE_GAME_SIZE_gCivilizationManager
        => "?gCivilizationManager@@3VEECivilizationManager@@A", "gCivilizationManager";
    gDoppelgangerManager: EE_GAME_SIZE_gDoppelgangerManager
        => "?gDoppelgangerManager@@3VEEDoppelgangerManager@@A", "gDoppelgangerManager";
    gCivPowersManager: EE_GAME_SIZE_gCivPowersManager
        => "?gCivPowersManager@@3VEECivPowersManager@@A", "gCivPowersManager";
    gCampaignAOCRoman: EE_GAME_SIZE_gCampaignAOCRoman => "?gCampaignAOCRoman@@3VUWideString@@A", "gCampaignAOCRoman";
    gCampaignAOCPacific: EE_GAME_SIZE_gCampaignAOCPacific => "?gCampaignAOCPacific@@3VUWideString@@A", "gCampaignAOCPacific";
    gCampaignAOCAsian: EE_GAME_SIZE_gCampaignAOCAsian => "?gCampaignAOCAsian@@3VUWideString@@A", "gCampaignAOCAsian";
    gCampaignEEDemo: EE_GAME_SIZE_gCampaignEEDemo => "?gCampaignEEDemo@@3VUWideString@@A", "gCampaignEEDemo";
    gCampaignEELearning: EE_GAME_SIZE_gCampaignEELearning => "?gCampaignEELearning@@3VUWideString@@A", "gCampaignEELearning";
    gU52A280: EE_GAME_SIZE_gU52A280 => "?gU52A280@@3VUWideString@@A", "gU52A280";
    gPhysicsThreadName: EE_GAME_SIZE_gPhysicsThreadName => "?gPhysicsThreadName@@3VUString@@A", "gPhysicsThreadName";
    gGraphicsThreadName: EE_GAME_SIZE_gGraphicsThreadName => "?gGraphicsThreadName@@3VUString@@A", "gGraphicsThreadName";
    gDataPreLoadingThreadName: EE_GAME_SIZE_gDataPreLoadingThreadName
        => "?gDataPreLoadingThreadName@@3VUString@@A", "gDataPreLoadingThreadName";
    gMousePointerThreadName: EE_GAME_SIZE_gMousePointerThreadName
        => "?gMousePointerThreadName@@3VUString@@A", "gMousePointerThreadName";
    gScenarioEditorThreadName: EE_GAME_SIZE_gScenarioEditorThreadName
        => "?gScenarioEditorThreadName@@3VUString@@A", "gScenarioEditorThreadName";
    // 0x562fd8 the WON password the UI thread keeps (constructed in UI Events/EEScreens.cpp)
    gMultiplayerString: EE_GAME_SIZE_gMultiplayerString => "?gMultiplayerString@@3VUString@@A", "gMultiplayerString";
    // the unit classes by type and the registered unit types (constructed in EEUnitClass.cpp; ee/EEUnit.h)
    gUnitClassTable: EE_GAME_SIZE_gUnitClassTable
        => "?gUnitClassTable@@3V?$vector@PEAVEEUnitClass@@V?$allocator@PEAVEEUnitClass@@@std@@@std@@A",
           "gUnitClassTable";
    EEUnitClassUnitTypes: EE_GAME_SIZE_EEUnitClassUnitTypes
        => "?sUnitTypes@EEUnitClass@@2V?$vector@KV?$allocator@K@std@@@std@@A", "_ZN11EEUnitClass10sUnitTypesE";
    // the game variants: ee_data's list, an empty class (ee/EEDbList.h), never constructed
    gGameVariants: EE_GAME_SIZE_gGameVariants => "?gGameVariants@@3U?$EEDbList@VEEGameVariant@@$00@@A", "gGameVariants";
}

/// The save-game version (.rdata 0x4420ac/0x4420b0; the original wrote 0/250). The port's minor versions:
/// 251 the computer slots' engine (EEGameOptionsPlayer, EEPPlayerHelper; docs/AI_LUA_PLAN.md S0); 252 the next
/// engine's state and its script's memory after each computer player (docs/AI_LUA_PLAN.md 3.2); 253 the next engine's
/// AI id after each computer player's engine, and the lobby choices' new numbering (ee/EECPEngine.h); 254 the running
/// game's own values (ee/EESaveGameFiles.h EESaveHasLiveState, docs/RUST_PLAN.md R5a.2); 255 the UTF-8 text
/// (EESaveTextIsUtf8); 256 more of the running state (the AI's schedule, timers, targets and path requests, the path
/// manager, the area effects, contacts and tile order: EESaveHasRunningState); 257 the units' order in each map region
/// (EESaveHasRegionOrder); 258 the computer players' experiment timers (EESaveHasExperimentTimers); 259 their count of
/// units in production (EESaveHasProductionCount); 260 the towns' ids, order and counters (EESaveHasTowns); 261 the
/// computer players' gatherer lists as saved (EESaveHasGathererOrder); 262 the dead attackers the units still hold
/// (EESaveHasGhosts); 263 the continent manager as it is, with the herds' and towns' serials
/// (EESaveHasContinentRegions); 264 the attack sub groups' positions (EESaveHasSubGroupPosition); 265 the computer
/// players' claim ranges and danger zones (EESaveHasResourceState); 266 the order of the units their unit managers
/// track (EESaveHasUnitManagerOrder); 267 their repair and site timers (EESaveHasCivilizationTimers); 268 the towns'
/// areas (EESaveHasTownAreas); 269 the computer players' manager timers (EESaveHasManagerTimers); 270 the order of each
/// player's unit list (EESaveHasPlayerUnitOrder); 271 the computer players' intelligence files
/// (EESaveHasIntelligence); 272 the ghosts' resources and the gather goals' dead resources (EESaveHasGhostResources);
/// 273 the contagion manager's last helper id (EESaveHasContagionCounter); 274 the next engine's site walkers
/// (EECPNextState::mExplore: its state block's version 4); 275 the attack goals' dying targets and the goals' units out
/// of the world (EESaveHasGoalGhosts); 276 the units' tile searches in progress (EESaveHasPathSearches); 277 the wall
/// manager's path request (EESaveHasWallRequest); 278 the graph searches in progress (EESaveHasGraphSearches); 279 the
/// players' active groups (EESaveHasActiveGroups); 280 the per-unit Lua interface's state after the next engine's
/// (ee/EECPNextUnits.h, EESaveHasUnitsLuaState); 281 the computer players without the C++ managers' timers and
/// intelligence files, main's next engine state block (EESaveHasLuaOnlyComputers); 282 the players' tech nodes that
/// are not allowed (EESaveHasTechAllowed), the location goals' group path state; 283 every map tile's terrain type
/// (EESaveHasTileTypes).
#[cfg_attr(windows, export_name = "?kSaveGameMajorVersion@@3KB")]
#[cfg_attr(not(windows), no_mangle)]
pub static kSaveGameMajorVersion: c_ulong = 0;
/// See [`kSaveGameMajorVersion`].
#[cfg_attr(windows, export_name = "?kSaveGameMinorVersion@@3KB")]
#[cfg_attr(not(windows), no_mangle)]
pub static kSaveGameMinorVersion: c_ulong = 283;

// ---- the members' paths: #[repr(C)] mirrors of the C++ classes, for the offsets only (never instantiated)

/// `UString`: its vtable pointer (the virtual destructor), then the Rust state.
#[repr(C)]
struct UStringM {
    vptr: *const c_void,
    state: EeUString,
}

/// `UWideString`: its vtable pointer, then the Rust state.
#[repr(C)]
struct UWideStringM {
    vptr: *const c_void,
    state: EeUWideString,
}

/// `EEGameOptionsPlayer` (ee/EEGameOptions.h): its plain members' base, then the name.
#[repr(C)]
struct GameOptionsPlayerM {
    base: EeAppOptionsPlayerFields,
    mName: UWideStringM,
}

/// `EEGameOptions`: the plain members' base, the 16 slots, the four names, the lock.
#[repr(C)]
struct GameOptionsM {
    base: EeAppOptionsFields,
    mPlayers: [GameOptionsPlayerM; 16],
    mCampaignName: UWideStringM,
    mScenarioName: UWideStringM,
    mMapType: UWideStringM,
    mMapName: UWideStringM,
    mLock: *mut c_void,
}

/// `EEGameSettings` (ee/EEGamePreferences.h): the plain members' base, the options, the strings.
#[repr(C)]
struct GameSettingsM {
    base: EeAppSettingsFields,
    mGameOptions: GameOptionsM,
    mHostGameName: UWideStringM,
    mLoadGameName: UWideStringM,
    mLastQuickSave: UWideStringM,
    mLastAutoSave: UWideStringM,
    mPlayerName: UWideStringM,
    mSierraUserID: UWideStringM,
    mSierraGameName: UWideStringM,
    mRasterizerName: UStringM,
    mInstalledFromVolume: UStringM,
    mInstalledFromDirectory: UStringM,
    mLastGameFile: UStringM,
    mLastScenarioFile: UStringM,
    mWonNetPassword: UStringM,
}

/// `EEGamePreferences`: the settings.
#[repr(C)]
struct GamePreferencesM {
    mSettings: GameSettingsM,
}

/// `EEPathManager`'s first members: the vtable pointer, the Rust manager, the region size.
#[repr(C)]
struct PathManagerM {
    vptr: *const c_void,
    mRust: *mut c_void,
    mLC8: core::ffi::c_long,
}

/// `EEAIContinentManager`: the vtable pointer, the Rust record.
#[repr(C)]
struct ContinentManagerM {
    vptr: *const c_void,
    mRecord: EeAStarCmRecord,
}

/// `EEGameState`'s first members: the vtable pointer, the lock, the options.
#[repr(C)]
struct GameStateM {
    vptr: *const c_void,
    mLock: *mut c_void,
    mGameOptions: GameOptionsM,
}

/// A member of the settings: its offset in `EEGamePreferences`.
macro_rules! pref {
    ($($f:tt)+) => { offset_of!(GamePreferencesM, mSettings.$($f)+) };
}

macro_rules! ee_refs {
    ($($name:ident = $object:ident + $offset:expr, $path:literal => $msvc:literal;)*) => {
        $(
            #[cfg_attr(windows, export_name = $msvc)]
            #[cfg_attr(not(windows), no_mangle)]
            pub static $name: EeRef = EeRef(unsafe { addr_of_mut!($object).cast::<u8>().add($offset) });
        )*
        /// The references: name, object, offset, the member's path in build/clang/layout.json and the symbol.
        #[cfg(test)]
        pub(crate) const REFS: &[(&str, &str, usize, &str, &str)] =
            &[$((stringify!($name), stringify!($object), $offset, $path, $msvc),)*];
    };
}

// data the original placed inside gGamePreferences, gPathManager, gContinentManager and gGameState (it addressed
// them as globals); the original's offsets are in the C++ headers' comments
ee_refs! {
    gLobbyGameOptions = gGamePreferences + pref!(mGameOptions), "mSettings.mGameOptions"
        => "?gLobbyGameOptions@@3AEAVEEGameOptions@@EA";
    gDifficultyLevel2 = gGamePreferences + pref!(mGameOptions.base.mDifficulty), "mSettings.mGameOptions.mDifficulty"
        => "?gDifficultyLevel2@@3AEAJEA";
    gPlayerName = gGamePreferences + pref!(mPlayerName), "mSettings.mPlayerName"
        => "?gPlayerName@@3AEAVUWideString@@EA";
    gWonPlayerName = gGamePreferences + pref!(mSierraUserID), "mSettings.mSierraUserID"
        => "?gWonPlayerName@@3AEAVUWideString@@EA";
    gHostedGameName = gGamePreferences + pref!(mSierraGameName), "mSettings.mSierraGameName"
        => "?gHostedGameName@@3AEAVUWideString@@EA";
    gLoadSavedGameName = gGamePreferences + pref!(mLastGameFile), "mSettings.mLastGameFile"
        => "?gLoadSavedGameName@@3AEAVUString@@EA";
    gLoadScenarioName = gGamePreferences + pref!(mLastScenarioFile), "mSettings.mLastScenarioFile"
        => "?gLoadScenarioName@@3AEAVUString@@EA";
    gSideScrollSpeed = gGamePreferences + pref!(base.mSideScrollSpeed), "mSettings.mSideScrollSpeed"
        => "?gSideScrollSpeed@@3AEAMEA";
    gMasterVolume = gGamePreferences + pref!(base.mSoundVolume), "mSettings.mSoundVolume"
        => "?gMasterVolume@@3AEAMEA";
    gZoomLock = gGamePreferences + pref!(base.mZoomLock), "mSettings.mZoomLock" => "?gZoomLock@@3AEAMEA";
    gDifficultyLevel = gGamePreferences + pref!(base.mComputerPlayerDifficultyLevel),
        "mSettings.mComputerPlayerDifficultyLevel" => "?gDifficultyLevel@@3AEAJEA";
    gGameWindowHeight = gGamePreferences + pref!(base.mGameWindowHeight), "mSettings.mGameWindowHeight"
        => "?gGameWindowHeight@@3AEAKEA";
    gGameWindowWidth = gGamePreferences + pref!(base.mGameWindowWidth), "mSettings.mGameWindowWidth"
        => "?gGameWindowWidth@@3AEAKEA";
    gTextureMipLevelsToDrop = gGamePreferences + pref!(base.mMIPLevelsToDrop), "mSettings.mMIPLevelsToDrop"
        => "?gTextureMipLevelsToDrop@@3AEAKEA";
    gModelDetail = gGamePreferences + pref!(base.mModelDetail), "mSettings.mModelDetail" => "?gModelDetail@@3AEAJEA";
    gEffectDetail = gGamePreferences + pref!(base.mEffectDetail), "mSettings.mEffectDetail"
        => "?gEffectDetail@@3AEAKEA";
    gNetworkAdapter = gGamePreferences + pref!(base.mNetworkAdapter), "mSettings.mNetworkAdapter"
        => "?gNetworkAdapter@@3AEAKEA";
    gScreenIndex = gGamePreferences + pref!(base.mScreenID), "mSettings.mScreenID" => "?gScreenIndex@@3AEAKEA";
    gTerrainMIPLevelsToDrop = gGamePreferences + pref!(base.mTerrainMIPLevelsToDrop),
        "mSettings.mTerrainMIPLevelsToDrop" => "?gTerrainMIPLevelsToDrop@@3AEAKEA";
    gTexturePixelDepth = gGamePreferences + pref!(base.mTextureBitDepth), "mSettings.mTextureBitDepth"
        => "?gTexturePixelDepth@@3AEAKEA";
    gAutoSaveCount = gGamePreferences + pref!(base.mNumberOfAutoSaves), "mSettings.mNumberOfAutoSaves"
        => "?gAutoSaveCount@@3AEAJEA";
    gHiddenUnitsMode = gGamePreferences + pref!(base.mL5BC), "mSettings.mL5BC" => "?gHiddenUnitsMode@@3AEAJEA";
    gZoomEnabled = gGamePreferences + pref!(base.mZoomEnabled), "mSettings.mZoomEnabled"
        => "?gZoomEnabled@@3AEA_NEA";
    gInterpolateFrames = gGamePreferences + pref!(base.mAnimationSmoothing), "mSettings.mAnimationSmoothing"
        => "?gInterpolateFrames@@3AEA_NEA";
    gFarRearClipping = gGamePreferences + pref!(base.mB5C6), "mSettings.mB5C6" => "?gFarRearClipping@@3AEA_NEA";
    gDrawClouds = gGamePreferences + pref!(base.mDisplayClouds), "mSettings.mDisplayClouds"
        => "?gDrawClouds@@3AEA_NEA";
    gDrawGroundShadows = gGamePreferences + pref!(base.mDisplayShadows), "mSettings.mDisplayShadows"
        => "?gDrawGroundShadows@@3AEA_NEA";
    gAmbientEffectsEnabled = gGamePreferences + pref!(base.mDisplayBattlefieldAmbients),
        "mSettings.mDisplayBattlefieldAmbients" => "?gAmbientEffectsEnabled@@3AEA_NEA";
    gTrilinearFilter = gGamePreferences + pref!(base.mTrilinearFilter), "mSettings.mTrilinearFilter"
        => "?gTrilinearFilter@@3AEA_NEA";
    gAmbientSoundEnabled = gGamePreferences + pref!(base.mAmbientSounds), "mSettings.mAmbientSounds"
        => "?gAmbientSoundEnabled@@3AEA_NEA";
    gDrawBlood = gGamePreferences + pref!(base.mShowBlood), "mSettings.mShowBlood" => "?gDrawBlood@@3AEA_NEA";
    gMusicFollowsBattle = gGamePreferences + pref!(base.mMusicEnabled), "mSettings.mMusicEnabled"
        => "?gMusicFollowsBattle@@3AEA_NEA";
    gOrthographicView = gGamePreferences + pref!(base.mB5D6), "mSettings.mB5D6" => "?gOrthographicView@@3AEA_NEA";
    gUnitLighting = gGamePreferences + pref!(base.mUnitLighting), "mSettings.mUnitLighting"
        => "?gUnitLighting@@3AEA_NEA";
    gDisplayWeatherEffects = gGamePreferences + pref!(base.mDisplayWeatherEffects), "mSettings.mDisplayWeatherEffects"
        => "?gDisplayWeatherEffects@@3AEA_NEA";
    gTakeJPGScreenshots = gGamePreferences + pref!(base.mTakeJPGScreenshots), "mSettings.mTakeJPGScreenshots"
        => "?gTakeJPGScreenshots@@3AEA_NEA";
    gAntiAliasing = gGamePreferences + pref!(base.mAntiAliasing), "mSettings.mAntiAliasing"
        => "?gAntiAliasing@@3AEA_NEA";
    gEEAIRegionSize = gPathManager + offset_of!(PathManagerM, mLC8), "mLC8" => "?gEEAIRegionSize@@3AEAJEA";
    gContinentManagerChecksum = gContinentManager + offset_of!(ContinentManagerM, mRecord.checksum),
        "mRecord.checksum" => "?gContinentManagerChecksum@@3AEAKEA";
    gDCGameSpeed = gContinentManager + offset_of!(ContinentManagerM, mRecord.num_water_tiles),
        "mRecord.num_water_tiles" => "?gDCGameSpeed@@3AEAJEA";
    gNumSpaceTiles = gContinentManager + offset_of!(ContinentManagerM, mRecord.num_space_tiles),
        "mRecord.num_space_tiles" => "?gNumSpaceTiles@@3AEAJEA";
    gMapWidth = gContinentManager + offset_of!(ContinentManagerM, mRecord.width), "mRecord.width"
        => "?gMapWidth@@3AEAKEA";
    gMapHeight = gContinentManager + offset_of!(ContinentManagerM, mRecord.height), "mRecord.height"
        => "?gMapHeight@@3AEAKEA";
    gContinentManagerClearing = gContinentManager + offset_of!(ContinentManagerM, mRecord.clearing),
        "mRecord.clearing" => "?gContinentManagerClearing@@3AEA_NEA";
    gGameStateLoadType = gGameState + offset_of!(GameStateM, mGameOptions.base.mLoadType), "mGameOptions.mLoadType"
        => "?gGameStateLoadType@@3AEAJEA";
    gGameStateIsMultiplayer = gGameState + offset_of!(GameStateM, mGameOptions.base.mIsMultiplayer),
        "mGameOptions.mIsMultiplayer" => "?gGameStateIsMultiplayer@@3AEA_NEA";
}

// ---- the members' addresses, for Rust's direct use (the settings' and options' plain members, strings, lock)

/// `&gGamePreferences`.
pub fn prefs() -> usize {
    addr_of_mut!(gGamePreferences) as usize
}

/// `gGamePreferences.mSettings`' plain members (its base, EeAppSettingsFields).
pub fn settings() -> *mut EeAppSettingsFields {
    (prefs() + PREFS_SETTINGS) as *mut EeAppSettingsFields
}

/// `EEGamePreferences::mSettings`.
pub const PREFS_SETTINGS: usize = offset_of!(GamePreferencesM, mSettings);
// EEGameSettings' members (the plain ones are its base, at 0)
pub const SET_OPTIONS: usize = offset_of!(GameSettingsM, mGameOptions);
pub const SET_HOST_GAME_NAME: usize = offset_of!(GameSettingsM, mHostGameName);
pub const SET_LAST_QUICK_SAVE: usize = offset_of!(GameSettingsM, mLastQuickSave);
pub const SET_LAST_AUTO_SAVE: usize = offset_of!(GameSettingsM, mLastAutoSave);
pub const SET_PLAYER_NAME: usize = offset_of!(GameSettingsM, mPlayerName);
pub const SET_SIERRA_USER_ID: usize = offset_of!(GameSettingsM, mSierraUserID);
pub const SET_SIERRA_GAME_NAME: usize = offset_of!(GameSettingsM, mSierraGameName);
pub const SET_RASTERIZER_NAME: usize = offset_of!(GameSettingsM, mRasterizerName);
pub const SET_INSTALLED_VOLUME: usize = offset_of!(GameSettingsM, mInstalledFromVolume);
pub const SET_INSTALLED_DIRECTORY: usize = offset_of!(GameSettingsM, mInstalledFromDirectory);
pub const SET_LAST_GAME_FILE: usize = offset_of!(GameSettingsM, mLastGameFile);
pub const SET_LAST_SCENARIO_FILE: usize = offset_of!(GameSettingsM, mLastScenarioFile);
pub const SET_WON_NET_PASSWORD: usize = offset_of!(GameSettingsM, mWonNetPassword);
/// the last game options' map type
pub const SET_MAP_TYPE: usize = offset_of!(GameSettingsM, mGameOptions) + offset_of!(GameOptionsM, mMapType);
// EEGameOptions' members (the plain ones are its base, at 0)
pub const OPT_CAMPAIGN_NAME: usize = offset_of!(GameOptionsM, mCampaignName);
pub const OPT_SCENARIO_NAME: usize = offset_of!(GameOptionsM, mScenarioName);
pub const OPT_MAP_TYPE: usize = offset_of!(GameOptionsM, mMapType);
pub const OPT_MAP_NAME: usize = offset_of!(GameOptionsM, mMapName);
pub const OPT_LOCK: usize = offset_of!(GameOptionsM, mLock);

/// `&o->mPlayers[i]` (its plain members, the slot's base).
pub fn opt_slot(o: usize, i: u32) -> usize {
    o + offset_of!(GameOptionsM, mPlayers) + i as usize * core::mem::size_of::<GameOptionsPlayerM>()
}

/// `&o->mPlayers[i].mName`.
pub fn opt_slot_name(o: usize, i: u32) -> usize {
    opt_slot(o, i) + offset_of!(GameOptionsPlayerM, mName)
}

/// The C++ class each storage holds, as build/clang/layout.json names it (the tests).
#[cfg(test)]
fn class_of(object: &str) -> &'static str {
    match object {
        "gGamePreferences" => "EEGamePreferences",
        "gPathManager" => "EEPathManager",
        "gContinentManager" => "EEAIContinentManager",
        "gGameState" => "EEGameState",
        "gViewport" => "GEViewport",
        "gWorld" => "EEWorld",
        "gEvent" => "EEEvent",
        "gDatabaseObjects" => "EEDatabase",
        "gTechTree" => "EETechTree",
        "gToHitTable" => "EEToHitTable",
        "gAIUnitTargetingTable" => "EEAIUnitTargetingTable",
        "gWorldTable" => "EEWorldTable",
        "gTerrainTable" => "EETerrainTable",
        "gTerrainColorTable" => "EETerrainColorTable",
        "gTerrainTypeTable" => "EETerrainTypeTable",
        "gComputerPlayerBehaviorTable" => "EEComputerPlayerBehaviorTable",
        "gUnitSetManager" => "EEUnitSetManager",
        "gAEManager" => "EEAEManager",
        "gAmbientSoundManager" => "EEAmbientSoundManager",
        "gCivilizationManager" => "EECivilizationManager",
        "gDoppelgangerManager" => "EEDoppelgangerManager",
        "gCivPowersManager" => "EECivPowersManager",
        _ => "",
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn symbols_and_counts() {
        assert_eq!(OBJECTS.len(), 42);
        assert_eq!(REFS.len(), 46);
        for (name, _, msvc) in OBJECTS {
            let plain = name.strip_prefix("EEUnitClassUnitTypes").map(|_| "sUnitTypes").unwrap_or(name);
            assert!(msvc.starts_with(&format!("?{plain}@")), "{name}: {msvc}");
        }
        for (name, _, _, _, msvc) in REFS {
            assert!(msvc.starts_with(&format!("?{name}@@3AEA")), "{name}: {msvc}");
        }
        let mut names: Vec<_> = OBJECTS.iter().map(|o| o.0).chain(REFS.iter().map(|r| r.0)).collect();
        names.sort_unstable();
        names.dedup();
        assert_eq!(names.len(), OBJECTS.len() + REFS.len());
    }

    #[test]
    fn references_point_into_their_objects() {
        let base = addr_of_mut!(gGamePreferences) as usize;
        assert_eq!(gPlayerName.0 as usize - base, pref!(mPlayerName));
        let world = addr_of_mut!(gContinentManager) as usize;
        assert_eq!(gMapWidth.0 as usize - world, offset_of!(ContinentManagerM, mRecord.width));
        for (name, _, offset, _, _) in REFS {
            assert!(*offset < EE_GAME_SIZE_gGamePreferences, "{name}");
        }
        assert_eq!(core::mem::align_of::<Storage<8>>(), 16);
        assert_eq!(kSaveGameMinorVersion, 283);
    }

    type Layout = std::collections::HashMap<String, (u64, std::collections::HashMap<String, u64>)>;

    /// layout.json (`{"class": {"fields": {"path": offset, ...}, "size": n}, ...}`, one key per line) -> class ->
    /// (size, fields).
    fn parse_layout(text: &str) -> Layout {
        let mut layout = Layout::new();
        let mut class: Option<String> = None;
        let mut fields = std::collections::HashMap::new();
        let key = |line: &str| {
            line.strip_prefix('"').and_then(|l| l.rsplit_once("\": ")).map(|(k, v)| (k.to_string(), v.to_string()))
        };
        for line in text.lines() {
            let Some((k, v)) = key(line.trim()) else { continue };
            if v == "{" && k != "fields" {
                class = Some(k);
                fields = std::collections::HashMap::new();
            } else if k == "size" {
                if let Some(c) = class.take() {
                    layout.insert(c, (v.trim_end_matches(',').parse().unwrap_or(0), std::mem::take(&mut fields)));
                }
            } else if let Ok(n) = v.trim_end_matches(',').parse() {
                fields.insert(k, n);
            }
        }
        layout
    }

    /// The Windows build's record layouts (tools/harness/layout_gen.py: build/clang/layout.json), when a build left
    /// them: every reference's offset, and every object's size within its storage.
    #[test]
    fn offsets_match_the_build_layout() {
        let path = std::path::Path::new(env!("CARGO_MANIFEST_DIR")).join("../../../build/clang/layout.json");
        let Ok(text) = std::fs::read_to_string(&path) else {
            eprintln!("{}: no build layout, not checked", path.display());
            return;
        };
        let layout = parse_layout(&text);
        let field = |class: &str, path: &str| -> Option<u64> {
            let fields = &layout.get(class)?.1;
            // (an empty member prefixed its successors' names with "(empty)." until layout_gen.py's depth fix, LLE/UI
            // round 2)
            fields.get(path).or_else(|| fields.get(&format!("(empty).{path}"))).copied()
        };
        let size = |class: &str| layout.get(class).map(|r| r.0).unwrap_or_else(|| panic!("{class}: no record"));
        if !cfg!(windows) {
            return; // the layouts are the Windows build's
        }
        for (name, object, offset, path, _) in REFS {
            let class = class_of(object);
            assert_eq!(field(class, path), Some(*offset as u64), "{name}: {class}.{path}");
        }
        for (name, bytes, _) in OBJECTS {
            let class = class_of(name);
            // (EE_CONSTRUCT_GLOBAL's static_assert checks every size; the dump sees most of the classes)
            let Some(&(real, _)) = layout.get(class) else { continue };
            assert!(real as usize <= bytes * EE_GAME_STORAGE_FACTOR, "{name}: {class} is {real} bytes");
        }
        for (class, bytes) in [("UString", EE_GAME_SIZE_gSavedGameExtension), ("UWideString", EE_GAME_SIZE_gU52A280)] {
            assert_eq!(size(class), bytes as u64, "{class}");
        }
        assert_eq!(size("EEGameOptions"), core::mem::size_of::<GameOptionsM>() as u64);
        assert_eq!(field("EEGameOptions", "mPlayers"), Some((opt_slot(0, 0)) as u64));
        assert_eq!(opt_slot_name(0, 1) - opt_slot(0, 1), 40, "EEGameOptionsPlayer.mName");
        assert_eq!(field("EEGameOptions", "mLock"), Some(OPT_LOCK as u64));
        assert_eq!(field("EEGameSettings", "mWonNetPassword"), Some(SET_WON_NET_PASSWORD as u64));
        assert_eq!(field("EEGameSettings", "mGameOptions.mMapType"), Some(SET_MAP_TYPE as u64));
        assert_eq!(field("EEGamePreferences", "mSettings"), Some(PREFS_SETTINGS as u64));
        assert_eq!(size("EEGameSettings"), core::mem::size_of::<GameSettingsM>() as u64);
    }
}
