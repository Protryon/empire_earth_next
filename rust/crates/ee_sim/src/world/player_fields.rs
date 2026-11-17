//! The player's plain members as Rust records the C++ classes derive from: `RTSPlayer`'s
//! ([`EeUnitsRtsPlayerFields`], R5b.4 round 29: `class RTSPlayer : public EeUnitsRtsPlayerFields`), `EEPlayer`'s
//! saved numbers ([`EeUnitsPlayerSaved`]) and its other members ([`EeUnitsPlayerFields`], the player lane's round 1:
//! `class EEPlayer : public RTSPlayer, public EeUnitsPlayerSaved, public EeUnitsPlayerFields`, ee/EEPlayer.h), with
//! the two Rust objects the last one points to ([`EeUnitsQueued`], [`EeUnitsPlayerWonders`]). The fields keep their
//! C++ names, types and order. The loads, saves and bodies over them are ee_sim::units' (rts_player_save.rs,
//! player_save.rs, player.rs, player_units.rs).

#![allow(non_snake_case)]

use std::collections::BTreeMap;
use std::os::raw::{c_long, c_ulong};

/// `RTSPlayer`'s plain members (ee/EEPlayer.h), in the class's order.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeUnitsRtsPlayerFields {
    /// the next unit id's low 24 bits (saved)
    pub mNextUniqueID: c_ulong,
    /// the player's index (saved)
    pub mPlayerID: c_ulong,
    /// its units (saved)
    pub mUnitCount: c_ulong,
    /// the lobby slot (0xff none; not saved)
    pub mSlot: u8,
    /// sees the whole map (saved; cleared for 0.207 and older)
    pub mSeesAll: bool,
    /// keeps a visibility map (saved)
    pub mSaveVisMap: bool,
    /// an observer (saved)
    pub mObserverMode: bool,
}

/// `EEPlayer`'s saved numbers (their C++ names and types).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeUnitsPlayerSaved {
    /// the scenario's player variables (zeroed together; saved)
    pub mScenarioVariables: [c_ulong; 10],
    /// non-zero: at war with that player (calamity targets; the store's diplomacy table)
    pub mEnemies: [c_ulong; 17],
    /// the tributes given and received by resource (two rows of 5)
    pub mTributes: [c_ulong; 10],
    /// 0..3 (the chat form's radio buttons 3, 2, 4, 5)
    pub mChatType: c_ulong,
    pub mCombatState: c_ulong,
    pub mGameResult: c_ulong,
    pub mFreeUnitCounter: c_ulong,
    pub mFreeUnitInterval: c_ulong,
    /// indexed by a resource (2..6 used)
    pub mResourceCaps: [c_long; 7],
    pub mPopLimit: c_long,
    pub mPopulationCount: c_long,
    pub mEndOfGameCount: c_long,
    /// written, never read back (a load reads and drops the word)
    pub mNumCapitols: c_long,
    /// indexed by a resource 2..6 as `[resource - 2]`
    pub mResourceTrade: [c_long; 5],
    pub mInsuranceRefunds: [c_long; 5],
    pub mNumVitalBuildings: c_long,
    pub mHighGroundBonus: f32,
    pub mHealTimer: f32,
    pub mSendChatMessage: [bool; 17],
    pub mSharedLOS: bool,
    pub mDisconnected: bool,
    pub mCivilizationLocked: bool,
    pub mCivilizationChosen: bool,
    pub mCivUpgradesPurchased: bool,
}

/// The queued improvements: per unit type the running timer (`first`) and the improvements in order.
#[derive(Clone, Debug, Default, PartialEq)]
pub struct EeUnitsQueued {
    pub map: BTreeMap<c_ulong, (f32, Vec<c_ulong>)>,
}

impl EeUnitsQueued {
    /// `operator[]`: the entry, made (0, empty) if absent.
    pub fn entry(&mut self, key: c_ulong) -> &mut (f32, Vec<c_ulong>) {
        self.map.entry(key).or_insert((0.0, Vec::new()))
    }
}

/// `EEPlayer`'s other members (their C++ names and types).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct EeUnitsPlayerFields {
    /// `kNoUnitClass` (0xffffffff): none; a unit class table index
    pub mHackUnitClass: c_ulong,
    pub mConversionFactor: f32,
    /// the tribute fee (0.4)
    pub mTributeFee: f32,
    pub mMaxStartingCitizens: c_long,
    /// the damage taken from each player
    pub mDamageFrom: [c_long; 17],
    pub mNumAttacks: c_long,
    pub mLastAcknowledgementTime: c_long,
    pub mLastAcknowledgedGroup: c_long,
    pub mHeroLimit: c_long,
    pub mMiningLimit: c_long,
    pub mLAF0: c_long,
    pub mShareBonus: c_long,
    pub mPopulationLimitBonus: c_long,
    pub mNumHeroes: c_long,
    pub mNumHeroesInProduction: c_long,
    pub mNumTeleporters: c_long,
    pub mAreaConversion: bool,
    pub mBB42: bool,
    pub mBB44: bool,
    pub mInstantBuild: bool,
    pub mHealsUnits: bool,
    pub mBehindFirewall: bool,
    pub mEarthquakeShaken: bool,
    pub mShiftMode: bool,
    pub mJITManufacturing: bool,
    /// the queued improvements (made by the constructors, freed by the destructor)
    pub mQueued: *mut EeUnitsQueued,
    /// the wonders' types and powers (player_units.rs; made by the constructors, freed by the destructor)
    pub mWonderState: *mut EeUnitsPlayerWonders,
}

impl Default for EeUnitsPlayerFields {
    /// zeros and no queue (`Reset` sets the C++ values)
    fn default() -> Self {
        EeUnitsPlayerFields {
            mHackUnitClass: 0,
            mConversionFactor: 0.0,
            mTributeFee: 0.0,
            mMaxStartingCitizens: 0,
            mDamageFrom: [0; 17],
            mNumAttacks: 0,
            mLastAcknowledgementTime: 0,
            mLastAcknowledgedGroup: 0,
            mHeroLimit: 0,
            mMiningLimit: 0,
            mLAF0: 0,
            mShareBonus: 0,
            mPopulationLimitBonus: 0,
            mNumHeroes: 0,
            mNumHeroesInProduction: 0,
            mNumTeleporters: 0,
            mAreaConversion: false,
            mBB42: false,
            mBB44: false,
            mInstantBuild: false,
            mHealsUnits: false,
            mBehindFirewall: false,
            mEarthquakeShaken: false,
            mShiftMode: false,
            mJITManufacturing: false,
            mQueued: std::ptr::null_mut(),
            mWonderState: std::ptr::null_mut(),
        }
    }
}

/// The wonders' Rust state: the count of the player's wonders per unit type (`mWonderTypeCounts`) and the powers
/// they grant, in the order granted (`mWonderPowers`: a power once per wonder type that grants it).
#[derive(Clone, Debug, Default, PartialEq)]
pub struct EeUnitsPlayerWonders {
    pub counts: BTreeMap<c_long, c_long>,
    pub powers: Vec<c_long>,
}
