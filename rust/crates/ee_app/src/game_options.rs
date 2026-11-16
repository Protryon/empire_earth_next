//! The game's options (`EEGameOptions`, `EEGameOptionsPlayer`; ee/EEGameOptions.h) as Rust records the C++ classes
//! derive from, and their bodies: the constructors' defaults, the validating copy (`operator=`), the lobby's
//! defaults (`SetDefaults`) and the saved game's `Load` and `Save`.
//!
//! The C++ classes keep what is not plain data: the player's name, the four option strings, the read/write lock and
//! the sixteen slots as an array. The bodies reach them through the host by operation (the strings' loads and saves,
//! the map type's check), and the file through `EESavePrimitive`'s operations (save_io.rs); the rules'
//! limits arrive as one [`EeAppOptionsLimits`] record the C++ fills from gameoptions.yaml and the tables.

#![allow(non_snake_case)]

use crate::save_io::SaveHost;
use std::os::raw::{c_long, c_ulong};

/// `EEGameOptionsPlayer`'s plain members (its name stays a C++ `UWideString`), in the class's order.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeAppOptionsPlayerFields {
    /// 0/1 human, 2 computer, 3 closed
    pub mType: c_ulong,
    pub mLoadedType: c_long,
    /// the lobby's ping meter
    pub mPing: f32,
    /// the slot index
    pub mSlot: c_ulong,
    pub mColor: c_ulong,
    pub mTeam: c_ulong,
    pub mStartingCitizens: c_ulong,
    /// a transfer in progress when > 0
    pub mTransferID: c_ulong,
    pub mIsLocal: bool,
    pub mIsReady: bool,
    pub mB36: bool,
    /// a computer slot's engine choice (ee/EECPEngine.h: 0 classic, 1 random, 2 + k the manifest's entry k)
    pub mEngine: c_ulong,
}

/// `EEGameOptions`' plain members (the slots, strings and lock stay C++), in the class's order.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeAppOptionsFields {
    pub mGameSpeed: c_ulong,
    /// 0 new game, 1 scenario, 2 saved game ... (< `load_types`)
    pub mLoadType: c_ulong,
    pub mMapSize: c_ulong,
    pub mEpochStart: c_long,
    pub mEpochEnd: c_long,
    pub mVariant: c_ulong,
    pub mResources: c_ulong,
    pub mUnitLimit: c_ulong,
    pub mWondersForVictory: c_ulong,
    pub mDifficulty: c_ulong,
    pub mCampaignChecksum: c_ulong,
    pub mVictory: bool,
    pub mLockTeams: bool,
    pub mLockSpeed: bool,
    pub mRevealMap: bool,
    pub mCheatCodes: bool,
    pub mRecordGames: bool,
    pub mAllowCustomCivs: bool,
    pub mB407: bool,
    pub mIsMultiplayer: bool,
    pub mIsScenarioGame: bool,
}

/// The limits and defaults the bodies check against (gameoptions.yaml's lists and numbers, the tables' sizes).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeAppOptionsLimits {
    /// `kLoadTypeCount`
    pub load_types: c_ulong,
    /// `EEGetNumberOfResourceLevels()`
    pub resource_levels: c_ulong,
    /// `EEGetNumberOfGameVariants()`
    pub variants: c_ulong,
    /// `EEEpochCount()`
    pub epochs: c_long,
    /// the map sizes' count and default
    pub map_sizes: c_ulong,
    pub map_size_default: c_ulong,
    pub difficulties: c_ulong,
    pub speeds: c_ulong,
    pub speed_default: c_ulong,
    pub wonders_default: c_ulong,
    pub wonders_maximum: c_ulong,
    pub unit_limit_minimum: c_ulong,
    pub unit_limit_maximum: c_ulong,
    pub unit_limit_default: c_ulong,
    /// `EECPEngineChoiceCount()`
    pub engine_choices: c_ulong,
}

// ---- the host's operations beyond `EESavePrimitive`'s (`index` the slot or the string)
/// the slot's name `UWideString::Load` / `Save` (index: the slot)
pub const EE_APP_OPTIONS_NAME_LOAD: i32 = 200;
pub const EE_APP_OPTIONS_NAME_SAVE: i32 = 201;
/// the option string `index` (0 campaign, 1 scenario, 2 map name) `Load` / `Save`
pub const EE_APP_OPTIONS_STRING_LOAD: i32 = 202;
pub const EE_APP_OPTIONS_STRING_SAVE: i32 = 203;
/// 1 when variant `index` is a known game variant (below the count and not NULL)
pub const EE_APP_OPTIONS_VARIANT_KNOWN: i32 = 204;

/// The option strings by index (`EE_APP_OPTIONS_STRING_*`).
pub const EE_APP_OPTIONS_CAMPAIGN: u32 = 0;
pub const EE_APP_OPTIONS_SCENARIO: u32 = 1;
pub const EE_APP_OPTIONS_MAP_NAME: u32 = 2;

/// The sixteen slots.
pub const EE_APP_OPTIONS_SLOTS: u32 = 16;

const OK: i32 = 0;
const CHOICE_CLASSIC: c_ulong = 0;
const CHOICE_RANDOM: c_ulong = 1;
const CHOICE_FIRST_AI: c_ulong = 2;

/// The slot's constructor (`EEGameOptionsPlayer()`): empty and closed.
pub fn player_default() -> EeAppOptionsPlayerFields {
    EeAppOptionsPlayerFields {
        mType: 3,
        mLoadedType: 3,
        mPing: 0.0,
        mSlot: 0,
        mColor: 0,
        mTeam: 0,
        mStartingCitizens: 0,
        mTransferID: 0,
        mIsLocal: false,
        mIsReady: false,
        mB36: true,
        mEngine: CHOICE_CLASSIC,
    }
}

/// The options' constructor (`EEGameOptions()`): random epochs, the rules' map size, wonders and speed, unit limit 50.
pub fn options_default(l: &EeAppOptionsLimits) -> EeAppOptionsFields {
    EeAppOptionsFields {
        mGameSpeed: l.speed_default,
        mLoadType: 0,
        mMapSize: l.map_size_default,
        mEpochStart: l.epochs,
        mEpochEnd: l.epochs.wrapping_sub(1),
        mVariant: 0,
        mResources: 0,
        mUnitLimit: 50,
        mWondersForVictory: l.wonders_default,
        mDifficulty: 0,
        mCampaignChecksum: 0,
        mVictory: true,
        mLockTeams: false,
        mLockSpeed: false,
        mRevealMap: false,
        mCheatCodes: false,
        mRecordGames: false,
        mAllowCustomCivs: false,
        mB407: true,
        mIsMultiplayer: false,
        // untouched by the original constructor (the C++ member's indeterminate value); false here
        mIsScenarioGame: false,
    }
}

/// `operator=`'s plain part: each out-of-range setting replaced by its default, the rest copied (`mB407` is not
/// copied, as the original). The map type's choice is the C++'s (a string).
pub fn assign(dst: &mut EeAppOptionsFields, src: &EeAppOptionsFields, l: &EeAppOptionsLimits) {
    dst.mLoadType = if src.mLoadType < l.load_types { src.mLoadType } else { 0 };
    dst.mResources = if src.mResources < l.resource_levels { src.mResources } else { 0 };
    dst.mVariant = if src.mVariant < l.variants { src.mVariant } else { 0 };
    dst.mEpochStart = if src.mEpochStart <= l.epochs { src.mEpochStart } else { 0 };
    dst.mEpochEnd = if src.mEpochEnd < l.epochs && (src.mEpochStart == l.epochs || src.mEpochEnd >= src.mEpochStart) {
        src.mEpochEnd
    } else {
        dst.mEpochStart
    };
    dst.mMapSize = if src.mMapSize < l.map_sizes { src.mMapSize } else { l.map_size_default };
    dst.mDifficulty = if src.mDifficulty < l.difficulties { src.mDifficulty } else { 0 };
    dst.mGameSpeed = if src.mGameSpeed < l.speeds { src.mGameSpeed } else { l.speed_default };
    dst.mWondersForVictory = if src.mWondersForVictory <= l.wonders_maximum { src.mWondersForVictory } else { 0 };
    dst.mUnitLimit = if src.mUnitLimit <= l.unit_limit_maximum { src.mUnitLimit } else { l.unit_limit_minimum };
    dst.mCampaignChecksum = src.mCampaignChecksum;
    dst.mVictory = src.mVictory;
    dst.mLockTeams = src.mLockTeams;
    dst.mLockSpeed = src.mLockSpeed;
    dst.mRevealMap = src.mRevealMap;
    dst.mCheatCodes = src.mCheatCodes;
    dst.mRecordGames = src.mRecordGames;
    dst.mAllowCustomCivs = src.mAllowCustomCivs;
    dst.mIsMultiplayer = src.mIsMultiplayer;
    dst.mIsScenarioGame = src.mIsScenarioGame;
}

/// `SetDefaults`' plain part after its copy of the lobby's options: all resources, random epochs, cheats on, victory
/// off, the preferred difficulty when it is a known one.
pub fn set_defaults(o: &mut EeAppOptionsFields, l: &EeAppOptionsLimits, preferred_difficulty: c_ulong) {
    o.mLoadType = 0;
    o.mResources = l.resource_levels.wrapping_sub(1);
    o.mVariant = 0;
    o.mEpochStart = l.epochs;
    o.mEpochEnd = l.epochs.wrapping_sub(1);
    o.mMapSize = l.map_size_default;
    o.mUnitLimit = l.unit_limit_default;
    o.mWondersForVictory = l.wonders_default;
    o.mGameSpeed = l.speed_default;
    o.mVictory = false;
    o.mLockTeams = false;
    o.mLockSpeed = false;
    o.mRevealMap = false;
    o.mCheatCodes = true;
    o.mRecordGames = false;
    o.mAllowCustomCivs = false;
    o.mIsMultiplayer = false;
    o.mB407 = true;
    o.mIsScenarioGame = false;
    o.mDifficulty = 0;
    if preferred_difficulty < l.difficulties {
        o.mDifficulty = preferred_difficulty;
    }
}

/// `EEGameOptionsPlayer::Load`: the slot's fields in file order, the engine choice from 0.251 (renumbered in 0.253),
/// the loaded type remembered; the first read error.
pub fn load_player<H: SaveHost + ?Sized>(
    h: &mut H,
    p: &mut EeAppOptionsPlayerFields,
    slot: u32,
    major: u32,
    minor: u32,
    l: &EeAppOptionsLimits,
) -> i32 {
    let mut r = h.read_ulong(&mut p.mType);
    if r == OK {
        r = h.op_at(EE_APP_OPTIONS_NAME_LOAD, slot);
    }
    if r == OK {
        r = h.read_float(&mut p.mPing);
    }
    if r == OK {
        r = h.read_ulong(&mut p.mSlot);
    }
    if r == OK {
        r = h.read_ulong(&mut p.mColor);
    }
    if r == OK {
        r = h.read_ulong(&mut p.mTeam);
    }
    if r == OK {
        r = h.read_ulong(&mut p.mStartingCitizens);
    }
    if r == OK {
        r = h.read_bool(&mut p.mIsLocal);
    }
    if r == OK {
        r = h.read_bool(&mut p.mIsReady);
    }
    if r == OK {
        r = h.read_bool(&mut p.mB36);
    }
    p.mEngine = CHOICE_CLASSIC;
    if r == OK && (major > 0 || minor >= 251) {
        r = h.read_ulong(&mut p.mEngine);
        if major == 0 && minor < 253 {
            p.mEngine = match p.mEngine {
                1 => CHOICE_FIRST_AI,
                2 => CHOICE_RANDOM,
                _ => CHOICE_CLASSIC,
            };
        }
        if p.mEngine >= l.engine_choices {
            p.mEngine = CHOICE_CLASSIC;
        }
    }
    p.mLoadedType = p.mType as c_long;
    r
}

/// `EEGameOptionsPlayer::Save`; the first write error.
pub fn save_player<H: SaveHost + ?Sized>(h: &mut H, p: &EeAppOptionsPlayerFields, slot: u32) -> i32 {
    let mut r = h.write_ulong(p.mType);
    if r == OK {
        r = h.op_at(EE_APP_OPTIONS_NAME_SAVE, slot);
    }
    let steps: [&dyn Fn(&mut H) -> i32; 9] = [
        &|h| h.write_float(p.mPing),
        &|h| h.write_ulong(p.mSlot),
        &|h| h.write_ulong(p.mColor),
        &|h| h.write_ulong(p.mTeam),
        &|h| h.write_ulong(p.mStartingCitizens),
        &|h| h.write_bool(p.mIsLocal),
        &|h| h.write_bool(p.mIsReady),
        &|h| h.write_bool(p.mB36),
        &|h| h.write_ulong(p.mEngine),
    ];
    for step in steps {
        if r == OK {
            r = step(h);
        }
    }
    r
}

/// `EEGameOptions::Load`: the sixteen slots, then the options in file order (saves before 0.200 lack the map name);
/// a resource level out of range is the first, an unknown variant the first; the first read error.
pub fn load<H: SaveHost + ?Sized>(
    h: &mut H,
    o: &mut EeAppOptionsFields,
    players: &mut [EeAppOptionsPlayerFields],
    major: u32,
    minor: u32,
    l: &EeAppOptionsLimits,
) -> i32 {
    let mut r = OK;
    for (i, p) in players.iter_mut().enumerate() {
        if r != OK {
            break;
        }
        r = load_player(h, p, i as u32, major, minor, l);
    }
    if r == OK {
        r = h.read_ulong(&mut o.mGameSpeed);
    }
    if r == OK {
        r = h.read_ulong(&mut o.mLoadType);
    }
    if r == OK {
        r = h.read_ulong(&mut o.mVariant);
    }
    if r == OK {
        r = h.read_ulong(&mut o.mMapSize);
    }
    if r == OK {
        r = h.read_long(&mut o.mEpochStart);
    }
    if r == OK {
        r = h.read_long(&mut o.mEpochEnd);
    }
    if r == OK {
        r = h.read_ulong(&mut o.mResources);
        if o.mResources >= l.resource_levels {
            o.mResources = 0;
        }
    }
    if r == OK {
        r = h.read_ulong(&mut o.mUnitLimit);
    }
    if r == OK {
        r = h.read_ulong(&mut o.mWondersForVictory);
    }
    if r == OK {
        r = h.read_ulong(&mut o.mDifficulty);
    }
    for v in [
        &mut o.mVictory,
        &mut o.mLockTeams,
        &mut o.mLockSpeed,
        &mut o.mRevealMap,
        &mut o.mCheatCodes,
        &mut o.mRecordGames,
        &mut o.mAllowCustomCivs,
        &mut o.mIsMultiplayer,
    ] {
        if r == OK {
            r = h.read_bool(v);
        }
    }
    if r == OK {
        r = h.op_at(EE_APP_OPTIONS_STRING_LOAD, EE_APP_OPTIONS_CAMPAIGN);
    }
    if r == OK {
        r = h.op_at(EE_APP_OPTIONS_STRING_LOAD, EE_APP_OPTIONS_SCENARIO);
    }
    if r == OK {
        r = h.read_ulong(&mut o.mCampaignChecksum);
    }
    if r == OK {
        r = h.read_bool(&mut o.mIsScenarioGame);
    }
    if r == OK && !(major == 0 && minor < 200) {
        r = h.op_at(EE_APP_OPTIONS_STRING_LOAD, EE_APP_OPTIONS_MAP_NAME);
    }
    #[allow(clippy::unnecessary_cast)] // c_ulong is u64 on LP64
    let variant = o.mVariant as u32;
    if o.mVariant >= l.variants || h.op_at(EE_APP_OPTIONS_VARIANT_KNOWN, variant) == 0 {
        o.mVariant = 0;
    }
    r
}

/// `EEGameOptions::Save`: the sixteen slots, then the options; the first write error.
pub fn save<H: SaveHost + ?Sized>(h: &mut H, o: &EeAppOptionsFields, players: &[EeAppOptionsPlayerFields]) -> i32 {
    let mut r = OK;
    for (i, p) in players.iter().enumerate() {
        if r != OK {
            break;
        }
        r = save_player(h, p, i as u32);
    }
    let steps: [&dyn Fn(&mut H) -> i32; 23] = [
        &|h| h.write_ulong(o.mGameSpeed),
        &|h| h.write_ulong(o.mLoadType),
        &|h| h.write_ulong(o.mVariant),
        &|h| h.write_ulong(o.mMapSize),
        &|h| h.write_long(o.mEpochStart),
        &|h| h.write_long(o.mEpochEnd),
        &|h| h.write_ulong(o.mResources),
        &|h| h.write_ulong(o.mUnitLimit),
        &|h| h.write_ulong(o.mWondersForVictory),
        &|h| h.write_ulong(o.mDifficulty),
        &|h| h.write_bool(o.mVictory),
        &|h| h.write_bool(o.mLockTeams),
        &|h| h.write_bool(o.mLockSpeed),
        &|h| h.write_bool(o.mRevealMap),
        &|h| h.write_bool(o.mCheatCodes),
        &|h| h.write_bool(o.mRecordGames),
        &|h| h.write_bool(o.mAllowCustomCivs),
        &|h| h.write_bool(o.mIsMultiplayer),
        &|h| h.op_at(EE_APP_OPTIONS_STRING_SAVE, EE_APP_OPTIONS_CAMPAIGN),
        &|h| h.op_at(EE_APP_OPTIONS_STRING_SAVE, EE_APP_OPTIONS_SCENARIO),
        &|h| h.write_ulong(o.mCampaignChecksum),
        &|h| h.write_bool(o.mIsScenarioGame),
        &|h| h.op_at(EE_APP_OPTIONS_STRING_SAVE, EE_APP_OPTIONS_MAP_NAME),
    ];
    for step in steps {
        if r == OK {
            r = step(h);
        }
    }
    r
}
