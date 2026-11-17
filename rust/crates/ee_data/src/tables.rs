//! The db table classes' records (`src/Empire Earth/Database`): each on-disk record as the text compiler writes it,
//! and its copy into the class's plain members (the C++ class's base, `EeData*Fields`, named as the members).
//!
//! The record structs are the record layouts' source: `tools/eedata` reads them (overlay.py `RUST_STRUCTS`) into
//! specs.json, so their field names are the record members' (the text's column names) and a one-byte flag is a `u8`
//! (any byte the file holds; the C++'s `bool` copies took it as nonzero).
//!
//! The C++ keeps the objects, their vectors and media (`EEMedia`); a loader reads each record and hands its bytes to
//! the `ee_data_*_from_record` copy here.
#![allow(non_snake_case)]

use std::ffi::{c_char, c_long, c_ulong, c_void};

/// `kHeightEpsilon`: a percentage's factor.
const PERCENT: f32 = 0.01;

/// Reads a record from the bytes the C++ read (`size_of::<T>()` of them, any alignment).
///
/// # Safety
/// `bytes` points at `size_of::<T>()` readable bytes.
unsafe fn record<T: Copy>(bytes: *const u8) -> T {
    // SAFETY: the caller's guarantee; every record is plain integers, floats and bytes
    unsafe { std::ptr::read_unaligned(bytes as *const T) }
}

// ---- EEAnimal (db\dbanimals)

/// One record of dbanimals (`EEAnimalRecord`, 272 bytes).
#[repr(C)]
#[derive(Clone, Copy)]
pub struct DbAnimal {
    /// The row name (the game never reads it; the text's label).
    pub mLabel: [u8; 100],
    /// The database row id (not read).
    pub mUnusedDatabaseID: i32,
    pub mAnimalID: i32,
    pub mUnitType: i32,
    pub mMinHerdSize: i32,
    pub mMaxHerdSize: i32,
    pub mBreeds: u8,
    /// Seconds (the class keeps milliseconds), as every interval below.
    pub mMinBirthInterval: i32,
    pub mMaxBirthInterval: i32,
    pub mHasLeader: u8,
    pub mUnusedFlag1: u8,
    /// Four values the class lists when positive (nothing reads the list).
    pub mUnusedListValue1: i32,
    pub mUnusedListValue2: i32,
    pub mUnusedListValue3: i32,
    pub mUnusedListValue4: i32,
    pub mUnusedIsPredator: u8,
    pub mUnusedFactor: f32,
    pub mMinHuntInterval: i32,
    pub mMaxHuntInterval: i32,
    pub mMinRoamMargin: i32,
    pub mMaxRoamMargin: i32,
    pub mMinGrazeTime: i32,
    pub mMaxGrazeTime: i32,
    pub mUnusedIsFlightless: u8,
    pub mIsPredator: u8,
    pub mUnusedFightsBack: u8,
    pub mHasParentalVengeance: u8,
    pub mHasGroupAttack: u8,
    pub mHasGroupFlee: u8,
    pub mNewbornGrowth: f32,
    pub mGrowthStep: f32,
    pub mGrowInterval: i32,
    pub mHerdRadius: i32,
    pub mMinRestInterval: i32,
    pub mMaxRestInterval: i32,
    pub mUnusedPredatorFleeChance: i32,
    pub mMinFollowInterval: i32,
    pub mMaxFollowInterval: i32,
    /// The media table index of the animal's graphics (negative: none).
    pub mMedia: i32,
    /// Up to three sounds' media indices (negative: none).
    pub mSound1: i32,
    pub mSound2: i32,
    pub mSound3: i32,
    pub _gapF4: [u8; 0x1c],
}

const _: () = assert!(core::mem::size_of::<DbAnimal>() == 272);

/// `EEAnimal`'s plain members, in the class's order (`include/ee/EEAnimal.h`: the class's base).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeDataAnimalFields {
    pub mF28: f32,
    pub mL2C: c_long,
    pub mNewbornGrowth: f32,
    pub mGrowthStep: f32,
    pub mUnitType: c_long,
    pub mAnimalID: c_long,
    pub mMinHerdSize: c_long,
    pub mMaxHerdSize: c_long,
    pub mMinBirthInterval: c_long,
    pub mMaxBirthInterval: c_long,
    pub mMinHuntInterval: c_long,
    pub mMaxHuntInterval: c_long,
    pub mMinTerritoryMargin: c_long,
    pub mMaxTerritoryMargin: c_long,
    pub mMinGrazeTime: c_long,
    pub mMaxGrazeTime: c_long,
    pub mHerdRadius: c_long,
    pub mGrowInterval: c_long,
    pub mMinRestInterval: c_long,
    pub mMaxRestInterval: c_long,
    pub mMinFollowInterval: c_long,
    pub mMaxFollowInterval: c_long,
    pub mHasParentalVengeance: bool,
    pub mHasGroupAttack: bool,
    pub mHasGroupFlee: bool,
    pub mB83: bool,
    pub mB84: bool,
    pub mBreeds: bool,
    pub mIsPredator: bool,
    pub mHasLeader: bool,
    pub mB88: bool,
    pub mB89: bool,
}

/// The media an animal record names, for the C++ to load (`EEMedia::Get`): negative for none.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeDataAnimalMedia {
    pub media: i32,
    pub sounds: [i32; 3],
    /// The list values (`mL04`: the positive ones go in, in order).
    pub list: [i32; 4],
}

/// Seconds to the class's milliseconds (the C++'s `int` product, wrapping).
fn ms(seconds: i32) -> c_long {
    seconds.wrapping_mul(1000) as c_long
}

/// `EEAnimal::Load`'s copy of a record.
pub fn animal_fields(r: &DbAnimal) -> (EeDataAnimalFields, EeDataAnimalMedia) {
    let fields = EeDataAnimalFields {
        mF28: r.mUnusedFactor,
        mL2C: r.mUnusedPredatorFleeChance as c_long,
        mNewbornGrowth: r.mNewbornGrowth,
        mGrowthStep: r.mGrowthStep,
        mUnitType: r.mUnitType as c_long,
        mAnimalID: r.mAnimalID as c_long,
        mMinHerdSize: r.mMinHerdSize as c_long,
        mMaxHerdSize: r.mMaxHerdSize as c_long,
        mMinBirthInterval: ms(r.mMinBirthInterval),
        mMaxBirthInterval: ms(r.mMaxBirthInterval),
        mMinHuntInterval: ms(r.mMinHuntInterval),
        mMaxHuntInterval: ms(r.mMaxHuntInterval),
        mMinTerritoryMargin: r.mMinRoamMargin as c_long,
        mMaxTerritoryMargin: r.mMaxRoamMargin as c_long,
        mMinGrazeTime: ms(r.mMinGrazeTime),
        mMaxGrazeTime: ms(r.mMaxGrazeTime),
        mHerdRadius: r.mHerdRadius as c_long,
        mGrowInterval: ms(r.mGrowInterval),
        mMinRestInterval: ms(r.mMinRestInterval),
        mMaxRestInterval: ms(r.mMaxRestInterval),
        mMinFollowInterval: ms(r.mMinFollowInterval),
        mMaxFollowInterval: ms(r.mMaxFollowInterval),
        mHasParentalVengeance: r.mHasParentalVengeance != 0,
        mHasGroupAttack: r.mHasGroupAttack != 0,
        mHasGroupFlee: r.mHasGroupFlee != 0,
        mB83: r.mUnusedFightsBack != 0,
        mB84: r.mUnusedIsFlightless != 0,
        mBreeds: r.mBreeds != 0,
        mIsPredator: r.mIsPredator != 0,
        mHasLeader: r.mHasLeader != 0,
        mB88: r.mUnusedFlag1 != 0,
        mB89: r.mUnusedIsPredator != 0,
    };
    let media = EeDataAnimalMedia {
        media: r.mMedia,
        sounds: [r.mSound1, r.mSound2, r.mSound3],
        list: [r.mUnusedListValue1, r.mUnusedListValue2, r.mUnusedListValue3, r.mUnusedListValue4],
    };
    (fields, media)
}

/// The size of a dbanimals record.
#[no_mangle]
pub extern "C" fn ee_data_animal_record_size() -> usize {
    core::mem::size_of::<DbAnimal>()
}

/// `EEAnimal::Load`'s copy of one record into the class's members and the media it names.
///
/// # Safety
/// `bytes` holds `ee_data_animal_record_size()` bytes; `fields` and `media` are writable.
#[no_mangle]
pub unsafe extern "C" fn ee_data_animal_from_record(
    bytes: *const u8,
    fields: *mut EeDataAnimalFields,
    media: *mut EeDataAnimalMedia,
) {
    // SAFETY: the caller's guarantee
    unsafe {
        let (f, m) = animal_fields(&record::<DbAnimal>(bytes));
        *fields = f;
        *media = m;
    }
}

// ---- EEUnitBehavior and EEToHitTable (db\dbunitbehavior, db\dbweapontohit)

/// One record of dbunitbehavior (`EEUnitBehaviorRecord`, 124 bytes).
#[repr(C)]
#[derive(Clone, Copy)]
pub struct DbUnitBehavior {
    /// The row name (the game never reads it; the text's label).
    pub mLabel: [u8; 100],
    /// The database row id (not read).
    pub mUnusedDatabaseID: i32,
    /// Negative: no behaviour in this row.
    pub mIndex: i32,
    pub mCanMoveFreely: u8,
    pub mCanPursuePastInitialContactLOS: u8,
    pub mCanTargetEnemies: u8,
    pub mReturnsToInitialContactLocation: u8,
    pub mCanFlee: u8,
    pub mMeleeCanMoveFreely: u8,
    pub mRangedCanMoveFreely: u8,
    pub _gap73: [u8; 0x09],
}

const _: () = assert!(core::mem::size_of::<DbUnitBehavior>() == 124);

/// `EEUnitBehavior`'s members, in the class's order (`include/ee/EEUnitBehavior.h`: the class's base).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeDataUnitBehaviorFields {
    pub mCanMoveFreely: bool,
    pub mMeleeCanMoveFreely: bool,
    pub mRangedCanMoveFreely: bool,
    pub mCanPursuePastInitialContactLOS: bool,
    pub mCanTargetEnemies: bool,
    pub mReturnsToInitialContactLocation: bool,
    pub mCanFlee: bool,
}

/// `EEUnitBehavior::Load`'s copy of a record: `None` for a row with a negative index (the table keeps NULL there).
pub fn unit_behavior_fields(r: &DbUnitBehavior) -> Option<EeDataUnitBehaviorFields> {
    (r.mIndex >= 0).then_some(EeDataUnitBehaviorFields {
        mCanMoveFreely: r.mCanMoveFreely != 0,
        mMeleeCanMoveFreely: r.mMeleeCanMoveFreely != 0,
        mRangedCanMoveFreely: r.mRangedCanMoveFreely != 0,
        mCanPursuePastInitialContactLOS: r.mCanPursuePastInitialContactLOS != 0,
        mCanTargetEnemies: r.mCanTargetEnemies != 0,
        mReturnsToInitialContactLocation: r.mReturnsToInitialContactLocation != 0,
        mCanFlee: r.mCanFlee != 0,
    })
}

/// The size of a dbunitbehavior record.
#[no_mangle]
pub extern "C" fn ee_data_unit_behavior_record_size() -> usize {
    core::mem::size_of::<DbUnitBehavior>()
}

/// `EEUnitBehavior::Load`'s copy of one record; false (and `fields` untouched) for a row without a behaviour.
///
/// # Safety
/// `bytes` holds `ee_data_unit_behavior_record_size()` bytes; `fields` is writable.
#[no_mangle]
pub unsafe extern "C" fn ee_data_unit_behavior_from_record(
    bytes: *const u8,
    fields: *mut EeDataUnitBehaviorFields,
) -> bool {
    // SAFETY: the caller's guarantee
    match unit_behavior_fields(&unsafe { record::<DbUnitBehavior>(bytes) }) {
        Some(f) => {
            // SAFETY: the caller's guarantee
            unsafe { *fields = f };
            true
        }
        None => false,
    }
}

/// One record of dbweapontohit (`EEToHitRecord`, 132 bytes): one weapon kind's row of the to-hit table.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct DbToHit {
    /// The row name (the game never reads it; the text's label).
    pub mLabel: [u8; 100],
    /// The database row id (not read).
    pub mUnusedDatabaseID: i32,
    /// The 0-based row index (not read).
    pub mUnusedIndex: i32,
    /// Hundredths, per target kind.
    pub mPercents: [i32; 6],
}

const _: () = assert!(core::mem::size_of::<DbToHit>() == 132);

/// `EEToHitTable::Load`'s row: each percentage times `kHeightEpsilon`.
pub fn to_hit_factors(r: &DbToHit) -> [f32; 6] {
    r.mPercents.map(|p| p as f32 * PERCENT)
}

/// The size of a dbweapontohit record.
#[no_mangle]
pub extern "C" fn ee_data_to_hit_record_size() -> usize {
    core::mem::size_of::<DbToHit>()
}

/// `EEToHitTable::Load`'s row of six factors from one record.
///
/// # Safety
/// `bytes` holds `ee_data_to_hit_record_size()` bytes; `out` has room for six floats.
#[no_mangle]
pub unsafe extern "C" fn ee_data_to_hit_from_record(bytes: *const u8, out: *mut f32) {
    // SAFETY: the caller's guarantee
    unsafe {
        let row = to_hit_factors(&record::<DbToHit>(bytes));
        std::ptr::copy_nonoverlapping(row.as_ptr(), out, 6);
    }
}

// ---- EEStartingResources (db\dbstartingresources)

/// One record of dbstartingresources (`EEStartingResourcesRecord`, 132 bytes).
#[repr(C)]
#[derive(Clone, Copy)]
pub struct DbStartingResources {
    /// The row name (the game never reads it; the text's label).
    pub mLabel: [u8; 100],
    /// The database row id (not read).
    pub mUnusedDatabaseID: i32,
    /// Negative: the row is skipped.
    pub mIndex: i32,
    pub mNameID: i32,
    pub mFood: i32,
    pub mWood: i32,
    pub mStone: i32,
    pub mGold: i32,
    pub mIron: i32,
}

const _: () = assert!(core::mem::size_of::<DbStartingResources>() == 132);

/// `EEStartingResources`' members (`include/ee/EEStartingResources.h`: the class's base).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeDataStartingResourcesFields {
    /// Food, wood, stone, gold, iron.
    pub mAmounts: [c_long; 5],
    /// The preset's name string id.
    pub mNameID: c_long,
}

/// `EEStartingResources::Load`'s copy of a record: `None` for a skipped row.
pub fn starting_resources_fields(r: &DbStartingResources) -> Option<EeDataStartingResourcesFields> {
    (r.mIndex >= 0).then_some(EeDataStartingResourcesFields {
        mAmounts: [r.mFood, r.mWood, r.mStone, r.mGold, r.mIron].map(|a| a as c_long),
        mNameID: r.mNameID as c_long,
    })
}

/// The size of a dbstartingresources record.
#[no_mangle]
pub extern "C" fn ee_data_starting_resources_record_size() -> usize {
    core::mem::size_of::<DbStartingResources>()
}

/// `EEStartingResources::Load`'s copy of one record; false (and `fields` untouched) for a skipped row.
///
/// # Safety
/// `bytes` holds `ee_data_starting_resources_record_size()` bytes; `fields` is writable.
#[no_mangle]
pub unsafe extern "C" fn ee_data_starting_resources_from_record(
    bytes: *const u8,
    fields: *mut EeDataStartingResourcesFields,
) -> bool {
    // SAFETY: the caller's guarantee
    match starting_resources_fields(&unsafe { record::<DbStartingResources>(bytes) }) {
        Some(f) => {
            // SAFETY: the caller's guarantee
            unsafe { *fields = f };
            true
        }
        None => false,
    }
}

// ---- EEUCUpgradeTable (db\dbupgrade)

/// One record of dbupgrade (`EEUCUpgradeRecord`, 240 bytes): a unit class's upgrade steps and costs.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct DbUpgrade {
    /// The row name (the game never reads it; the text's label).
    pub mLabel: [u8; 100],
    pub mAttackStep: f32,
    pub mHitPointsStep: f32,
    pub mSpeedStep: f32,
    pub mRangeStep: f32,
    pub mShockArmorStep: f32,
    pub mArrowArmorStep: f32,
    pub mPierceArmorStep: f32,
    pub mGunArmorStep: f32,
    pub mLaserArmorStep: f32,
    pub mMissileArmorStep: f32,
    pub mFlightTimeStep: f32,
    pub mLeadershipStep: f32,
    pub mAreaEffectStep: f32,
    pub mTransportStep: f32,
    pub mAttackCost: f32,
    pub mHitPointsCost: f32,
    pub mSpeedCost: f32,
    pub mRangeCost: f32,
    pub mShockArmorCost: f32,
    pub mArrowArmorCost: f32,
    pub mPierceArmorCost: f32,
    pub mGunArmorCost: f32,
    pub mLaserArmorCost: f32,
    pub mMissileArmorCost: f32,
    pub mFlightTimeCost: f32,
    pub mLeadershipCost: f32,
    pub mRepairCost: f32,
    pub mAreaEffectCost: f32,
    pub mTransportCost: f32,
    pub mMaxLevel: i32,
    pub mUnused1: i32,
    /// Negative: no entry.
    pub mIndex: i32,
    pub mUnused2: i32,
    pub mUnused3: i32,
    pub mHasRepairStep: u8,
}

const _: () = assert!(core::mem::size_of::<DbUpgrade>() == 240);

/// `EEUCUpgradeFactor`: one upgraded value.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeDataUpgradeFactor {
    pub mScale: f32,
    pub mOffset: f32,
}

/// `EEUCUpgradeTable`'s members (`include/ee/EEUCUpgradeTable.h`: the class's base).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeDataUpgradeFields {
    pub mFactors: [EeDataUpgradeFactor; 16],
    pub mMaxLevel: c_long,
}

/// The constructor's entry: every factor of scale 1 and offset 0, max level 0.
pub fn upgrade_default() -> EeDataUpgradeFields {
    EeDataUpgradeFields { mFactors: [EeDataUpgradeFactor { mScale: 1.0, mOffset: 0.0 }; 16], mMaxLevel: 0 }
}

/// `EEUCUpgradeTable::Load`'s copy: each factor's cost as its scale and its step as its offset, in the class's factor
/// order; the repair factor (12) takes 1 or 0 by the record's flag as its offset, and factor 13 keeps the default (the
/// original stored element 12 twice). `None` for a row with a negative index.
pub fn upgrade_fields(r: &DbUpgrade) -> Option<EeDataUpgradeFields> {
    if r.mIndex < 0 {
        return None;
    }
    let mut f = upgrade_default();
    f.mMaxLevel = r.mMaxLevel as c_long;
    let pairs = [
        (0, r.mAreaEffectCost, r.mAreaEffectStep),
        (1, r.mAttackCost, r.mAttackStep),
        (2, r.mArrowArmorCost, r.mArrowArmorStep),
        (3, r.mGunArmorCost, r.mGunArmorStep),
        (4, r.mLaserArmorCost, r.mLaserArmorStep),
        (5, r.mMissileArmorCost, r.mMissileArmorStep),
        (6, r.mPierceArmorCost, r.mPierceArmorStep),
        (7, r.mShockArmorCost, r.mShockArmorStep),
        (8, r.mFlightTimeCost, r.mFlightTimeStep),
        (9, r.mHitPointsCost, r.mHitPointsStep),
        (10, r.mLeadershipCost, r.mLeadershipStep),
        (11, r.mRangeCost, r.mRangeStep),
        (14, r.mSpeedCost, r.mSpeedStep),
        (15, r.mTransportCost, r.mTransportStep),
    ];
    for (i, scale, offset) in pairs {
        f.mFactors[i] = EeDataUpgradeFactor { mScale: scale, mOffset: offset };
    }
    // `mHasRepairStep == true`: the byte compared with 1
    f.mFactors[12] =
        EeDataUpgradeFactor { mScale: r.mRepairCost, mOffset: if r.mHasRepairStep == 1 { 1.0 } else { 0.0 } };
    Some(f)
}

/// The size of a dbupgrade record.
#[no_mangle]
pub extern "C" fn ee_data_upgrade_record_size() -> usize {
    core::mem::size_of::<DbUpgrade>()
}

/// The constructor's entry (see [`upgrade_default`]).
///
/// # Safety
/// `fields` is writable.
#[no_mangle]
pub unsafe extern "C" fn ee_data_upgrade_default(fields: *mut EeDataUpgradeFields) {
    // SAFETY: the caller's guarantee
    unsafe { *fields = upgrade_default() };
}

/// `EEUCUpgradeTable::Load`'s copy of one record; false (and `fields` untouched) for a row without an entry.
///
/// # Safety
/// `bytes` holds `ee_data_upgrade_record_size()` bytes; `fields` is writable.
#[no_mangle]
pub unsafe extern "C" fn ee_data_upgrade_from_record(bytes: *const u8, fields: *mut EeDataUpgradeFields) -> bool {
    // SAFETY: the caller's guarantee
    match upgrade_fields(&unsafe { record::<DbUpgrade>(bytes) }) {
        Some(f) => {
            // SAFETY: the caller's guarantee
            unsafe { *fields = f };
            true
        }
        None => false,
    }
}

// ---- EEColorTable (db\dbcolortable)

/// One record of dbcolortable (`EEColorTableRecord`, 168 bytes): a player's colours.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct DbColor {
    /// The row name (the game never reads it; the text's label).
    pub mLabel: [u8; 100],
    /// The database row id (not read).
    pub mUnusedDatabaseID: i32,
    /// The 0-based row index (not read).
    pub mUnusedIndex: i32,
    pub mColorRed: f32,
    pub mColorGreen: f32,
    pub mColorBlue: f32,
    pub mColor2Red: f32,
    pub mColor2Green: f32,
    pub mColor2Blue: f32,
    pub mColor4Red: f32,
    pub mColor4Green: f32,
    pub mColor4Blue: f32,
    pub _gap90: [u8; 0x18],
}

const _: () = assert!(core::mem::size_of::<DbColor>() == 168);

/// `EEColorTable`'s four colours (red, green, blue): `mColor`, `mColor2`, `mDarkColor`, `mColor4`.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeDataPlayerColors {
    pub colors: [[f32; 3]; 4],
}

/// The share of the second colour the dark shade keeps.
const DARK_SHADE: f32 = 0.6;

/// `EEColorTable::Load`'s copy: the main, second and fourth colours, and the second at 60% as the dark shade.
pub fn player_colors(r: &DbColor) -> EeDataPlayerColors {
    let second = [r.mColor2Red, r.mColor2Green, r.mColor2Blue];
    EeDataPlayerColors {
        colors: [
            [r.mColorRed, r.mColorGreen, r.mColorBlue],
            second,
            second.map(|c| c * DARK_SHADE),
            [r.mColor4Red, r.mColor4Green, r.mColor4Blue],
        ],
    }
}

/// The size of a dbcolortable record.
#[no_mangle]
pub extern "C" fn ee_data_color_record_size() -> usize {
    core::mem::size_of::<DbColor>()
}

/// `EEColorTable::Load`'s copy of one record.
///
/// # Safety
/// `bytes` holds `ee_data_color_record_size()` bytes; `out` is writable.
#[no_mangle]
pub unsafe extern "C" fn ee_data_color_from_record(bytes: *const u8, out: *mut EeDataPlayerColors) {
    // SAFETY: the caller's guarantee
    unsafe { *out = player_colors(&record::<DbColor>(bytes)) };
}

// ---- EEAIUnitTargetingTable (db\dbaiunittargeting)

/// One record of dbaiunittargeting (`EEAIUnitTargetingRecord`, 160 bytes): one row of the AI's targeting weights.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct DbAiTargeting {
    /// The row name (the game never reads it; the text's label).
    pub mLabel: [u8; 100],
    /// The database row id (not read).
    pub mUnusedDatabaseID: i32,
    /// The 0-based row index (not read).
    pub mUnusedIndex: i32,
    pub mAttackerBonus: i32,
    pub mUnusedWeight: i32,
    pub mInRangeBonus: i32,
    pub mFasterTargetBonus: i32,
    pub mStrongerTargetBonus: i32,
    pub mPreferredUnitType: i32,
    pub mToHitWeight: i32,
    pub mProximityWeight: i32,
    pub mImmobileTargetBonus: i32,
    pub mUnused1: i32,
    pub mUnused2: i32,
    pub mUnused3: i32,
    pub mMaxValue: i32,
}

const _: () = assert!(core::mem::size_of::<DbAiTargeting>() == 160);

/// `EEAIUnitTargetingTable::Load`'s row of ten values, in the table's order.
pub fn ai_targeting_row(r: &DbAiTargeting) -> [c_long; 10] {
    [
        r.mAttackerBonus,
        r.mUnusedWeight,
        r.mInRangeBonus,
        r.mFasterTargetBonus,
        r.mStrongerTargetBonus,
        r.mPreferredUnitType,
        r.mToHitWeight,
        r.mProximityWeight,
        r.mImmobileTargetBonus,
        r.mMaxValue,
    ]
    .map(|v| v as c_long)
}

/// The size of a dbaiunittargeting record.
#[no_mangle]
pub extern "C" fn ee_data_ai_targeting_record_size() -> usize {
    core::mem::size_of::<DbAiTargeting>()
}

/// `EEAIUnitTargetingTable::Load`'s row from one record.
///
/// # Safety
/// `bytes` holds `ee_data_ai_targeting_record_size()` bytes; `out` has room for ten `long`s.
#[no_mangle]
pub unsafe extern "C" fn ee_data_ai_targeting_from_record(bytes: *const u8, out: *mut c_long) {
    // SAFETY: the caller's guarantee
    unsafe {
        let row = ai_targeting_row(&record::<DbAiTargeting>(bytes));
        std::ptr::copy_nonoverlapping(row.as_ptr(), out, 10);
    }
}

// ---- EETerrainTypeTable (db\dbterraintype)

/// One record of dbterraintype (`EETerrainTypeRecord`, 128 bytes).
#[repr(C)]
#[derive(Clone, Copy)]
pub struct DbTerrainType {
    /// The row name (the game never reads it; the text's label).
    pub mLabel: [u8; 100],
    /// The database row id (not read).
    pub mUnusedDatabaseID: i32,
    pub mID: i32,
    pub mFirstColor: i32,
    pub mLastColor: i32,
    pub mNameID: i32,
    pub mUnusedPaletteIndex: i32,
    pub mIsInEditor: u8,
}

const _: () = assert!(core::mem::size_of::<DbTerrainType>() == 128);

/// `EETerrainTypeTable`'s members from `mID` (`include/ee/EEDatabaseTables.h`: the class's base).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeDataTerrainTypeFields {
    /// The map's colour set of the type (EEMap sets it; not from the record).
    pub mColorSetID: c_long,
    pub mID: c_long,
    pub mFirstColor: c_long,
    pub mLastColor: c_long,
    pub mNameID: c_long,
    pub mL18: c_long,
    pub mIsInEditor: bool,
}

/// `EETerrainTypeTable::Load`'s copy of a record (the unused palette index is kept as `mL18`).
pub fn terrain_type_fields(r: &DbTerrainType) -> EeDataTerrainTypeFields {
    EeDataTerrainTypeFields {
        mColorSetID: 0,
        mID: r.mID as c_long,
        mFirstColor: r.mFirstColor as c_long,
        mLastColor: r.mLastColor as c_long,
        mNameID: r.mNameID as c_long,
        mL18: r.mUnusedPaletteIndex as c_long,
        mIsInEditor: r.mIsInEditor != 0,
    }
}

/// The size of a dbterraintype record.
#[no_mangle]
pub extern "C" fn ee_data_terrain_type_record_size() -> usize {
    core::mem::size_of::<DbTerrainType>()
}

/// `EETerrainTypeTable::Load`'s copy of one record.
///
/// # Safety
/// `bytes` holds `ee_data_terrain_type_record_size()` bytes; `fields` is writable.
#[no_mangle]
pub unsafe extern "C" fn ee_data_terrain_type_from_record(bytes: *const u8, fields: *mut EeDataTerrainTypeFields) {
    // SAFETY: the caller's guarantee
    unsafe { *fields = terrain_type_fields(&record::<DbTerrainType>(bytes)) };
}

#[test]
fn a_terrain_type_keeps_its_palette_index_as_l18() {
    // SAFETY: plain integers and bytes
    let mut r: DbTerrainType = unsafe { std::mem::zeroed() };
    (r.mID, r.mFirstColor, r.mLastColor, r.mNameID, r.mUnusedPaletteIndex, r.mIsInEditor) = (3, 10, 20, 900, 7, 1);
    let f = terrain_type_fields(&r);
    assert_eq!((f.mID, f.mFirstColor, f.mLastColor, f.mNameID, f.mL18, f.mIsInEditor), (3, 10, 20, 900, 7, true));
    assert_eq!(core::mem::offset_of!(DbTerrainType, mIsInEditor), 0x7c);
}

// ---- EEGameVariant (db\dbgamevariant)

/// One record of dbgamevariant (`EEGameVariantData`, 276 bytes): ten (factor, unit set) pairs and the variant's
/// other factors.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct DbGameVariant {
    /// The row name (the game never reads it; the text's label).
    pub mLabel: [u8; 100],
    /// The database row id (not read).
    pub mUnusedDatabaseID: i32,
    /// Negative: the row is skipped.
    pub mIndex: i32,
    pub mFactor0: f32,
    pub mSet0: i32,
    pub mFactor1: f32,
    pub mSet1: i32,
    pub mFactor2: f32,
    pub mSet2: i32,
    pub mFactor3: f32,
    pub mSet3: i32,
    pub mFactor4: f32,
    pub mSet4: i32,
    pub mFactor5: f32,
    pub mSet5: i32,
    pub mFactor6: f32,
    pub mSet6: i32,
    pub mFactor7: f32,
    pub mSet7: i32,
    pub mFactor8: f32,
    pub mSet8: i32,
    pub mFactor9: f32,
    pub mSet9: i32,
    pub mDepositFactor: f32,
    pub mEpochCostFactor: f32,
    pub mWorkTimeFactor: f32,
    pub mMoraleBonus: i32,
    /// The name's string id.
    pub mNameID: i32,
    pub mFirstEpochCostFactor: f32,
    pub mSecondEpochCostFactor: f32,
    pub mCostSetFactor: f32,
    pub mCostSet: i32,
    pub mEarthquakeDamageFactor: f32,
    /// Zero in every record; not copied (`mUnused1` .. `mUnused10`).
    pub mUnused1: i32,
    pub mUnused2: i32,
    pub mUnused3: i32,
    pub mUnused4: i32,
    pub mUnused5: i32,
    pub mUnused6: i32,
    pub mUnused7: i32,
    pub mUnused8: i32,
    pub mUnused9: i32,
    pub mUnused10: i32,
    pub mSelectable: u8,
    pub mUnused11: i32,
}

const _: () = assert!(core::mem::size_of::<DbGameVariant>() == 276);

/// `EEGameVariant`'s members (`include/ee/EEGameVariant.h`: the class's base).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeDataGameVariantFields {
    /// The (set, factor) pairs' factors.
    pub mFactors: [f32; 10],
    pub mDepositFactor: f32,
    pub mEpochCostFactor: f32,
    pub mWorkTimeFactor: f32,
    pub mFirstEpochCostFactor: f32,
    pub mSecondEpochCostFactor: f32,
    pub mCostSetFactor: f32,
    pub mEarthquakeDamageFactor: f32,
    /// The pairs' unit sets (`gUnitSetManager` ids; > 0: used).
    pub mSets: [c_long; 10],
    pub mIndex: c_long,
    pub mMoraleBonus: c_long,
    pub mNameID: c_long,
    pub mCostSet: c_long,
    pub mSelectable: bool,
}

/// `EEGameVariant::Load`'s copy of a record: `None` for a skipped row.
pub fn game_variant_fields(r: &DbGameVariant) -> Option<EeDataGameVariantFields> {
    (r.mIndex >= 0).then_some(EeDataGameVariantFields {
        mFactors: [
            r.mFactor0, r.mFactor1, r.mFactor2, r.mFactor3, r.mFactor4, r.mFactor5, r.mFactor6, r.mFactor7, r.mFactor8,
            r.mFactor9,
        ],
        mDepositFactor: r.mDepositFactor,
        mEpochCostFactor: r.mEpochCostFactor,
        mWorkTimeFactor: r.mWorkTimeFactor,
        mFirstEpochCostFactor: r.mFirstEpochCostFactor,
        mSecondEpochCostFactor: r.mSecondEpochCostFactor,
        mCostSetFactor: r.mCostSetFactor,
        mEarthquakeDamageFactor: r.mEarthquakeDamageFactor,
        mSets: [r.mSet0, r.mSet1, r.mSet2, r.mSet3, r.mSet4, r.mSet5, r.mSet6, r.mSet7, r.mSet8, r.mSet9]
            .map(|s| s as c_long),
        mIndex: r.mIndex as c_long,
        mMoraleBonus: r.mMoraleBonus as c_long,
        mNameID: r.mNameID as c_long,
        mCostSet: r.mCostSet as c_long,
        mSelectable: r.mSelectable != 0,
    })
}

/// The size of a dbgamevariant record.
#[no_mangle]
pub extern "C" fn ee_data_game_variant_record_size() -> usize {
    core::mem::size_of::<DbGameVariant>()
}

/// `EEGameVariant::Load`'s copy of one record; false (and `fields` untouched) for a skipped row.
///
/// # Safety
/// `bytes` holds `ee_data_game_variant_record_size()` bytes; `fields` is writable.
#[no_mangle]
pub unsafe extern "C" fn ee_data_game_variant_from_record(
    bytes: *const u8,
    fields: *mut EeDataGameVariantFields,
) -> bool {
    // SAFETY: the caller's guarantee
    match game_variant_fields(&unsafe { record::<DbGameVariant>(bytes) }) {
        Some(f) => {
            // SAFETY: the caller's guarantee
            unsafe { *fields = f };
            true
        }
        None => false,
    }
}

#[test]
fn a_game_variant_splits_its_pairs() {
    // SAFETY: plain integers, floats and bytes
    let mut r: DbGameVariant = unsafe { std::mem::zeroed() };
    (r.mIndex, r.mFactor0, r.mSet0, r.mFactor9, r.mSet9, r.mCostSet, r.mSelectable) = (1, 0.5, 3, 2.0, 7, 12, 1);
    r.mEarthquakeDamageFactor = 0.25;
    let f = game_variant_fields(&r).unwrap();
    assert_eq!((f.mFactors[0], f.mFactors[9], f.mSets[0], f.mSets[9]), (0.5, 2.0, 3, 7));
    assert_eq!((f.mIndex, f.mCostSet, f.mSelectable, f.mEarthquakeDamageFactor), (1, 12, true, 0.25));
    r.mIndex = -1;
    assert_eq!(game_variant_fields(&r), None);
    assert_eq!(core::mem::offset_of!(DbGameVariant, mSelectable), 0x10c);
}

// ---- Names in records

/// A text field of a record as the C++ copies it into a `UString`: its bytes up to the first NUL (or the whole
/// field), pointing into the record the C++ read.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeDataName {
    pub text: *const c_char,
    pub length: usize,
}

/// The name in `field` (at `offset` in the record at `bytes`): up to its first NUL, or all of it.
fn name_at(bytes: *const u8, offset: usize, field: &[u8]) -> EeDataName {
    let length = field.iter().position(|&b| b == 0).unwrap_or(field.len());
    // SAFETY: `offset` is the field's offset in the record `bytes` points at
    EeDataName { text: unsafe { bytes.add(offset) } as *const c_char, length }
}

// ---- EETerrainTable and EECliffTerrainTable (db\dbterrain, db\dbcliffterrain)

/// One record of dbterrain after its lead string, the texture file name (`EETerrainRecord`, 204 bytes).
#[repr(C)]
#[derive(Clone, Copy)]
pub struct DbTerrain {
    /// The terrain's name (the class's `mName`: the random map scripts name a terrain by it, spaces removed).
    pub mName: [u8; 0x68],
    /// Negative: an unused row (`EETerrainTable::Get` returns NULL).
    pub mID: i32,
    pub mTextureID: i32,
    /// The name's string id.
    pub mNameID: i32,
    pub mTextureTileCoverage: i32,
    pub mTerrainType: i32,
    /// The texture grid: `mColumns` x `mRows` textures, the file name numbered `_1` .. `_n`.
    pub mColumns: i32,
    pub mRows: i32,
    pub mIsWater: u8,
    pub mHasAlpha: u8,
    pub mIsInEditor: u8,
    pub mColorType: i32,
    /// Three ranges of ambient unit classes, [first, last] (both nonzero: used).
    pub mFirst: [u32; 3],
    pub mLast: [u32; 3],
    pub mAmbientUnitChance: i32,
    pub mWaterAnimationPeriod: i32,
    pub mWaterDepth: i32,
    /// The colour, 0..255 a component.
    pub mRed: i32,
    pub mGreen: i32,
    pub mBlue: i32,
    pub _gapBC: [u8; 0x0f],
    pub mIsAmbientsOnly: u8,
}

const _: () = assert!(core::mem::size_of::<DbTerrain>() == 204);

/// `EETerrainTable`'s members from the record (`include/ee/EEDatabaseTables.h`: the class's base).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeDataTerrainFields {
    pub mWaterAnimationPeriod: c_long,
    pub mTextureID: c_long,
    pub mID: c_long,
    pub mNameID: c_long,
    pub mTextureTileCoverage: c_long,
    pub mTerrainType: c_long,
    pub mColumns: c_long,
    pub mRows: c_long,
    /// -1 in an empty terrain.
    pub mColorType: c_long,
    pub mAmbientUnitChance: c_long,
    pub mWaterDepth: c_long,
    /// `mColumns * mRows`.
    pub mNumTextures: c_long,
    pub mIsWater: bool,
    pub mIsInEditor: bool,
    pub mHasAlpha: bool,
    pub mIsAmbientsOnly: bool,
}

/// What a terrain record gives the C++ besides its plain members.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeDataTerrainExtra {
    pub name: EeDataName,
    /// The colour, a component each in 0..1.
    pub color: [f32; 3],
    /// The ambient unit class ranges ([first, last], used when both are nonzero).
    pub first: [u32; 3],
    pub last: [u32; 3],
}

/// The constructor's members: all zero, colour type -1.
pub fn terrain_default() -> EeDataTerrainFields {
    EeDataTerrainFields { mColorType: -1, ..Default::default() }
}

/// `EETerrainTable::Load`'s copy of a record's plain members.
pub fn terrain_fields(r: &DbTerrain) -> EeDataTerrainFields {
    EeDataTerrainFields {
        mWaterAnimationPeriod: r.mWaterAnimationPeriod as c_long,
        mTextureID: r.mTextureID as c_long,
        mID: r.mID as c_long,
        mNameID: r.mNameID as c_long,
        mTextureTileCoverage: r.mTextureTileCoverage as c_long,
        mTerrainType: r.mTerrainType as c_long,
        mColumns: r.mColumns as c_long,
        mRows: r.mRows as c_long,
        mColorType: r.mColorType as c_long,
        mAmbientUnitChance: r.mAmbientUnitChance as c_long,
        mWaterDepth: r.mWaterDepth as c_long,
        mNumTextures: (r.mColumns as c_long).wrapping_mul(r.mRows as c_long),
        mIsWater: r.mIsWater != 0,
        mIsInEditor: r.mIsInEditor != 0,
        mHasAlpha: r.mHasAlpha != 0,
        mIsAmbientsOnly: r.mIsAmbientsOnly != 0,
    }
}

/// The size of a dbterrain record (after its lead string).
#[no_mangle]
pub extern "C" fn ee_data_terrain_record_size() -> usize {
    core::mem::size_of::<DbTerrain>()
}

/// The terrain constructor's members.
///
/// # Safety
/// `fields` is writable.
#[no_mangle]
pub unsafe extern "C" fn ee_data_terrain_default(fields: *mut EeDataTerrainFields) {
    // SAFETY: the caller's guarantee
    unsafe { *fields = terrain_default() };
}

/// `EETerrainTable::Load`'s copy of one record: the plain members, and the name (pointing into `bytes`), colour and
/// unit class ranges.
///
/// # Safety
/// `bytes` holds `ee_data_terrain_record_size()` bytes and outlives the name's use; `fields` and `extra` are
/// writable.
#[no_mangle]
pub unsafe extern "C" fn ee_data_terrain_from_record(
    bytes: *const u8,
    fields: *mut EeDataTerrainFields,
    extra: *mut EeDataTerrainExtra,
) {
    // SAFETY: the caller's guarantee
    let r = unsafe { record::<DbTerrain>(bytes) };
    let component = |c: i32| c as f32 / 255.0;
    // SAFETY: the caller's guarantee
    unsafe {
        *fields = terrain_fields(&r);
        *extra = EeDataTerrainExtra {
            name: name_at(bytes, core::mem::offset_of!(DbTerrain, mName), &r.mName),
            color: [component(r.mRed), component(r.mGreen), component(r.mBlue)],
            first: r.mFirst,
            last: r.mLast,
        };
    }
}

/// A terrain texture's file: `name` numbered `_n` before its extension (the last `.`), or after it without one.
pub fn terrain_texture_file(name: &[u8], n: c_long) -> Vec<u8> {
    let number = format!("_{n}");
    match name.iter().rposition(|&b| b == b'.') {
        Some(dot) => [&name[..dot], number.as_bytes(), &name[dot..]].concat(),
        None => [name, number.as_bytes()].concat(),
    }
}

/// [`terrain_texture_file`] of the NUL-terminated `name` into `out` (NUL-terminated, cut to `capacity - 1`
/// bytes); returns the file name's full length.
///
/// # Safety
/// `name` is NUL-terminated; `out` has room for `capacity` bytes (`capacity` > 0).
#[no_mangle]
pub unsafe extern "C" fn ee_data_terrain_texture_file(
    name: *const c_char,
    n: c_long,
    out: *mut c_char,
    capacity: usize,
) -> usize {
    // SAFETY: the caller's guarantee
    let name = unsafe { std::ffi::CStr::from_ptr(name) }.to_bytes();
    let file = terrain_texture_file(name, n);
    let kept = file.len().min(capacity - 1);
    // SAFETY: the caller's guarantee: room for `capacity` bytes
    unsafe {
        std::ptr::copy_nonoverlapping(file.as_ptr(), out as *mut u8, kept);
        *out.add(kept) = 0;
    }
    file.len()
}

/// One record of dbcliffterrain (`EECliffTerrainRecord`, 124 bytes).
#[repr(C)]
#[derive(Clone, Copy)]
pub struct DbCliffTerrain {
    pub mName: [u8; 0x68],
    /// Negative: no cliff terrain in this row (the table keeps NULL).
    pub mIndex: i32,
    /// The terrains of the cliff's faces, first to last.
    pub mFirstTerrain: i32,
    pub mLastTerrain: i32,
    pub mUnusedValue: i32,
    pub mUnusedFlag: u8,
}

const _: () = assert!(core::mem::size_of::<DbCliffTerrain>() == 124);

/// `EECliffTerrainTable`'s plain members (`include/ee/EEDatabaseTables.h`: the class's base).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeDataCliffTerrainFields {
    pub mFirstTerrain: c_long,
    pub mLastTerrain: c_long,
    pub mL1C: c_long,
    pub mB20: bool,
}

/// `EECliffTerrainTable::Load`'s copy of a record: `None` for a row with a negative index.
pub fn cliff_terrain_fields(r: &DbCliffTerrain) -> Option<EeDataCliffTerrainFields> {
    (r.mIndex >= 0).then_some(EeDataCliffTerrainFields {
        mFirstTerrain: r.mFirstTerrain as c_long,
        mLastTerrain: r.mLastTerrain as c_long,
        mL1C: r.mUnusedValue as c_long,
        mB20: r.mUnusedFlag != 0,
    })
}

/// The size of a dbcliffterrain record.
#[no_mangle]
pub extern "C" fn ee_data_cliff_terrain_record_size() -> usize {
    core::mem::size_of::<DbCliffTerrain>()
}

/// `EECliffTerrainTable::Load`'s copy of one record; false (and the outputs untouched) for a row without one.
///
/// # Safety
/// `bytes` holds `ee_data_cliff_terrain_record_size()` bytes and outlives the name's use; `fields` and `name` are
/// writable.
#[no_mangle]
pub unsafe extern "C" fn ee_data_cliff_terrain_from_record(
    bytes: *const u8,
    fields: *mut EeDataCliffTerrainFields,
    name: *mut EeDataName,
) -> bool {
    // SAFETY: the caller's guarantee
    let r = unsafe { record::<DbCliffTerrain>(bytes) };
    match cliff_terrain_fields(&r) {
        Some(f) => {
            // SAFETY: the caller's guarantee
            unsafe {
                *fields = f;
                *name = name_at(bytes, core::mem::offset_of!(DbCliffTerrain, mName), &r.mName);
            }
            true
        }
        None => false,
    }
}

// ---- EEButtonObject (db\dbbuttons)

/// One record of dbbuttons (`EEButtonRecord`, 224 bytes): a button image's texture and its cell.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct DbButton {
    /// The row name (the game never reads it; the text's label).
    pub mLabel: [u8; 100],
    /// The texture file (the class's `mName`).
    pub mFileName: [u8; 0x68],
    /// Below 50: the texture has alpha.
    pub mID: i32,
    /// The cell in the texture's grid of 41-texel cells.
    pub mRow: i32,
    pub mColumn: i32,
    pub mPosition: i32,
    pub mHotKey: i32,
}

const _: () = assert!(core::mem::size_of::<DbButton>() == 224);

/// `EEButtonObject`'s members from the record (`include/ee/EEButtonObject.h`: the class's base).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeDataButtonFields {
    pub mPosition: c_long,
    pub mHotKeyIndex: c_long,
    pub mColumn: c_long,
    pub mRow: c_long,
    pub mID: c_long,
}

/// `EEButtonObject::Load`'s copy of a record.
pub fn button_fields(r: &DbButton) -> EeDataButtonFields {
    EeDataButtonFields {
        mPosition: r.mPosition as c_long,
        mHotKeyIndex: r.mHotKey as c_long,
        mColumn: r.mColumn as c_long,
        mRow: r.mRow as c_long,
        mID: r.mID as c_long,
    }
}

/// The size of a dbbuttons record.
#[no_mangle]
pub extern "C" fn ee_data_button_record_size() -> usize {
    core::mem::size_of::<DbButton>()
}

/// `EEButtonObject::Load`'s copy of one record; returns the texture file's name (pointing into `bytes`).
///
/// # Safety
/// `bytes` holds `ee_data_button_record_size()` bytes and outlives the name's use; `fields` is writable.
#[no_mangle]
pub unsafe extern "C" fn ee_data_button_from_record(bytes: *const u8, fields: *mut EeDataButtonFields) -> EeDataName {
    // SAFETY: the caller's guarantee
    let r = unsafe { record::<DbButton>(bytes) };
    // SAFETY: the caller's guarantee
    unsafe { *fields = button_fields(&r) };
    name_at(bytes, core::mem::offset_of!(DbButton, mFileName), &r.mFileName)
}

/// A button texture's sizes, as `EEButtonObject`'s bitmap setters read them.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeDataButtonTexture {
    /// `GETexture::GetWidth`, `GetHeight` (the logical size) and `GetNumMipLevelsToDrop`.
    pub width: c_ulong,
    pub height: c_ulong,
    pub drop: c_ulong,
    /// The surface's `GetHeight`.
    pub surface_height: c_ulong,
}

/// A button's cell on a bitmap: the texture coordinates of its corners and the bitmap's scale.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeDataButtonCell {
    pub left: f32,
    pub top: f32,
    pub right: f32,
    pub bottom: f32,
    pub scale_x: f32,
    pub scale_y: f32,
}

/// `EEButtonObject::LoadTexture`'s cell offset: the cell's 41-texel step over the texture's full width (height).
#[allow(clippy::unnecessary_cast)] // `c_ulong` is u64 on LP64: the shift counts are u32
pub fn button_cell_offset(column: c_long, row: c_long, t: &EeDataButtonTexture) -> [f32; 2] {
    [
        column.wrapping_mul(41) as f32 / t.width.wrapping_shl(t.drop as u32) as f32,
        row.wrapping_mul(41) as f32 / t.height.wrapping_shl(t.drop as u32) as f32,
    ]
}

/// `EEButtonObject::SetFaceBitmap` (`inset`: one texel in) and `SetPressedBitmap`: a 40-texel cell on a texture
/// 256 or more wide at full size, else the whole texture less a texel; the bitmap scaled to the rectangle.
#[allow(clippy::unnecessary_cast)] // `c_ulong` is u64 on LP64: the shift counts are u32
pub fn button_cell(t: &EeDataButtonTexture, cell: [f32; 2], inset: bool, rect: [f32; 2]) -> EeDataButtonCell {
    let width = t.width.wrapping_shl(t.drop as u32) as f32;
    let (u, v) = if width >= 256.0 {
        (40.0 / width, 40.0 / t.surface_height.wrapping_shl(t.drop as u32) as f32)
    } else {
        (t.width.wrapping_sub(1) as f32 / t.width as f32, t.height.wrapping_sub(1) as f32 / t.height as f32)
    };
    let (left, top) = if inset {
        let texel = 1.0 / t.width as f32;
        (texel + cell[0], texel + cell[1])
    } else {
        (cell[0], cell[1])
    };
    EeDataButtonCell {
        left,
        top,
        right: left + u,
        bottom: top + v,
        scale_x: (rect[0] + 1.0) / t.width as f32,
        scale_y: (rect[1] + 1.0) / t.surface_height as f32,
    }
}

/// [`button_cell_offset`] for the C++.
///
/// # Safety
/// `texture` is readable; `out` has room for two floats.
#[no_mangle]
pub unsafe extern "C" fn ee_data_button_cell_offset(
    column: c_long,
    row: c_long,
    texture: *const EeDataButtonTexture,
    out: *mut f32,
) {
    // SAFETY: the caller's guarantee
    unsafe {
        let o = button_cell_offset(column, row, &*texture);
        std::ptr::copy_nonoverlapping(o.as_ptr(), out, 2);
    }
}

/// [`button_cell`] for the C++: the cell offset (`mCellU`, `mCellV`), the inset flag and the rectangle's size.
///
/// # Safety
/// `texture` is readable.
#[no_mangle]
pub unsafe extern "C" fn ee_data_button_cell(
    texture: *const EeDataButtonTexture,
    cell_u: f32,
    cell_v: f32,
    inset: bool,
    width: f32,
    height: f32,
) -> EeDataButtonCell {
    // SAFETY: the caller's guarantee
    button_cell(unsafe { &*texture }, [cell_u, cell_v], inset, [width, height])
}

// ---- the damage tables (db\dbfamily)

/// The damage columns of a dbfamily row (`EE_COMBAT_DAMAGE_COLUMNS`).
const DAMAGE_COLUMNS: usize = 0x44;

/// One record of dbfamily (`EEDamageTableRecord`, 380 bytes): a unit family's damage percentages by attack kind.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct DbDamageTable {
    /// The row name (the game never reads it; the text's label).
    pub mLabel: [u8; 100],
    /// The database row id (not read).
    pub mUnusedDatabaseID: i32,
    /// The 0-based row index (not read).
    pub mUnusedIndex: i32,
    /// Percent, scaled by `kHeightEpsilon` into the damage table's row.
    pub mDamage: [i32; 0x44],
}

const _: () = assert!(core::mem::size_of::<DbDamageTable>() == 380);

/// The size of a dbfamily record.
#[no_mangle]
pub extern "C" fn ee_data_damage_table_record_size() -> usize {
    core::mem::size_of::<DbDamageTable>()
}

/// `EETerrainTypeTable::LoadDamageTables`' row of percentages from one record.
///
/// # Safety
/// `bytes` holds `ee_data_damage_table_record_size()` bytes; `out` has room for 0x44 `int32_t`s.
#[no_mangle]
pub unsafe extern "C" fn ee_data_damage_table_from_record(bytes: *const u8, out: *mut i32) {
    // SAFETY: the caller's guarantee
    unsafe {
        let r = record::<DbDamageTable>(bytes);
        std::ptr::copy_nonoverlapping(r.mDamage.as_ptr(), out, DAMAGE_COLUMNS);
    }
}

// ---- EESoundObject (db\dbsounds)

/// One record of dbsounds after its lead string, the sample's file (`EESoundRecord`, 132 bytes).
#[repr(C)]
#[derive(Clone, Copy)]
pub struct DbSound {
    /// The row name (the game never reads it; the text's label).
    pub mLabel: [u8; 100],
    /// The database row id (not read).
    pub mUnusedDatabaseID: i32,
    /// The 0-based row index (not read).
    pub mUnusedIndex: i32,
    pub mMaxInstances: i32,
    pub mCategory: i32,
    pub mUnusedPriority: i32,
    pub mLooping: u8,
    /// Percent (the class keeps a factor).
    pub mVolume: i32,
    pub _gap80: [u8; 4],
}

const _: () = assert!(core::mem::size_of::<DbSound>() == 132);

/// `EESoundObject`'s members from the record (`include/ee/EESoundObject.h`: the class's base).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeDataSoundFields {
    pub mVolume: f32,
    pub mMaxInstances: c_long,
    pub mCategory: c_long,
    pub mL24: c_long,
    pub mLooping: bool,
}

/// `EESoundObject::LoadTable`'s copy of a record (the volume's percent times 0.01).
pub fn sound_fields(r: &DbSound) -> EeDataSoundFields {
    EeDataSoundFields {
        mVolume: r.mVolume as f32 * PERCENT,
        mMaxInstances: r.mMaxInstances as c_long,
        mCategory: r.mCategory as c_long,
        mL24: r.mUnusedPriority as c_long,
        mLooping: r.mLooping != 0,
    }
}

/// The size of a dbsounds record (after its lead string).
#[no_mangle]
pub extern "C" fn ee_data_sound_record_size() -> usize {
    core::mem::size_of::<DbSound>()
}

/// `EESoundObject::LoadTable`'s copy of one record.
///
/// # Safety
/// `bytes` holds `ee_data_sound_record_size()` bytes; `fields` is writable.
#[no_mangle]
pub unsafe extern "C" fn ee_data_sound_from_record(bytes: *const u8, fields: *mut EeDataSoundFields) {
    // SAFETY: the caller's guarantee
    unsafe { *fields = sound_fields(&record::<DbSound>(bytes)) };
}

// ---- EEGraphicEffect (db\dbgfxeffects)

/// One record of dbgfxeffects (`EEGraphicEffectRecord`, 232 bytes).
#[repr(C)]
#[derive(Clone, Copy)]
pub struct DbGraphicEffect {
    /// The row name (the game never reads it; the text's label).
    pub mLabel: [u8; 100],
    /// The database row id (not read).
    pub mUnusedDatabaseID: i32,
    /// Above 0: a real record (the table keeps NULL for the others).
    pub mID: i32,
    pub mKind: i32,
    pub mTagPointKind: i32,
    /// The media table index of the effect's graphic (none: a random one of its kind).
    pub mMediaIndex: i32,
    pub mStartSize: f32,
    pub mHeight: f32,
    pub mAlpha: f32,
    pub mTime1: i32,
    pub mTime2: i32,
    pub mDuration: i32,
    pub mIntParam1: i32,
    pub mIntParam2: i32,
    pub mIntParam3: i32,
    /// Three follow-on effects, by table index (out of the table: none).
    pub mLinkedEffectIndex1: i32,
    pub mLinkedEffectIndex2: i32,
    pub mLinkedEffectIndex3: i32,
    pub mNameStringId: i32,
    pub mParam1: f32,
    pub mParam2: f32,
    pub mParam3: f32,
    pub mParam4: f32,
    pub mParam5: f32,
    pub mParam6: f32,
    pub mParam7: f32,
    pub mParam8: f32,
    pub mParam9: f32,
    pub mParam10: f32,
    pub mParam11: f32,
    pub mEmitPositionSpread: f32,
    pub mFlag1: u8,
    pub mFlag2: u8,
    pub mIsScenarioListed: u8,
    pub mIsEmitFlat: u8,
    pub mEndsWithUnitOrDuration: u8,
    pub mDetailLevel: i32,
}

const _: () = assert!(core::mem::size_of::<DbGraphicEffect>() == 232);

/// `EEGraphicEffect`'s members from the record (`include/ee/EEGraphicEffect.h`: the class's third base).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeDataGraphicEffectFields {
    pub mSize: f32,
    pub mStartSize: f32,
    pub mAlpha: f32,
    pub mParam1: f32,
    pub mParam2: f32,
    pub mParam3: f32,
    pub mParam4: f32,
    pub mParam5: f32,
    pub mParam6: f32,
    pub mParam7: f32,
    pub mParam8: f32,
    pub mParam9: f32,
    pub mParam10: f32,
    pub mParam11: f32,
    pub mEmitPositionSpread: f32,
    pub mKind: c_long,
    pub mTagPointKind: c_long,
    pub mID: c_ulong,
    pub mIntParam1: c_long,
    pub mIntParam2: c_long,
    pub mIntParam3: c_long,
    pub mTime1: c_ulong,
    pub mTime2: c_long,
    pub mDuration: c_ulong,
    pub mNameStringId: c_long,
    pub mDetailLevel: c_long,
    pub mFlag1: bool,
    pub mFlag2: bool,
    pub mEndsWithUnitOrDuration: bool,
    pub mIsScenarioListed: bool,
    pub mIsEmitFlat: bool,
}

/// The table indices a graphic effect record names, for the C++ to look up.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeDataGraphicEffectLinks {
    /// `EEMedia::Get`'s index (the record's, sign-extended: a negative one is past the table).
    pub media: c_long,
    /// The three follow-on effects' indices into the effect table.
    pub linked: [c_ulong; 3],
}

/// `EEGraphicEffect`'s constructor's copy of a record (the record's height is the class's size); `None` for a row
/// whose id is not above 0.
pub fn graphic_effect_fields(r: &DbGraphicEffect) -> Option<(EeDataGraphicEffectFields, EeDataGraphicEffectLinks)> {
    if r.mID <= 0 {
        return None;
    }
    let fields = EeDataGraphicEffectFields {
        mSize: r.mHeight,
        mStartSize: r.mStartSize,
        mAlpha: r.mAlpha,
        mParam1: r.mParam1,
        mParam2: r.mParam2,
        mParam3: r.mParam3,
        mParam4: r.mParam4,
        mParam5: r.mParam5,
        mParam6: r.mParam6,
        mParam7: r.mParam7,
        mParam8: r.mParam8,
        mParam9: r.mParam9,
        mParam10: r.mParam10,
        mParam11: r.mParam11,
        mEmitPositionSpread: r.mEmitPositionSpread,
        mKind: r.mKind as c_long,
        mTagPointKind: r.mTagPointKind as c_long,
        mID: r.mID as c_long as c_ulong,
        mIntParam1: r.mIntParam1 as c_long,
        mIntParam2: r.mIntParam2 as c_long,
        mIntParam3: r.mIntParam3 as c_long,
        mTime1: r.mTime1 as c_long as c_ulong,
        mTime2: r.mTime2 as c_long,
        mDuration: r.mDuration as c_long as c_ulong,
        mNameStringId: r.mNameStringId as c_long,
        mDetailLevel: r.mDetailLevel as c_long,
        mFlag1: r.mFlag1 != 0,
        mFlag2: r.mFlag2 != 0,
        mEndsWithUnitOrDuration: r.mEndsWithUnitOrDuration != 0,
        mIsScenarioListed: r.mIsScenarioListed != 0,
        mIsEmitFlat: r.mIsEmitFlat != 0,
    };
    // the original kept each index in the pointer member (sign-extended) and read it back unsigned
    let index = |i: i32| i as c_long as c_ulong;
    let links = EeDataGraphicEffectLinks {
        media: r.mMediaIndex as c_long,
        linked: [index(r.mLinkedEffectIndex1), index(r.mLinkedEffectIndex2), index(r.mLinkedEffectIndex3)],
    };
    Some((fields, links))
}

/// The size of a dbgfxeffects record.
#[no_mangle]
pub extern "C" fn ee_data_graphic_effect_record_size() -> usize {
    core::mem::size_of::<DbGraphicEffect>()
}

/// `EEGraphicEffect`'s copy of one record; false (and the outputs untouched) for a row that is not an effect.
///
/// # Safety
/// `bytes` holds `ee_data_graphic_effect_record_size()` bytes; `fields` and `links` are writable.
#[no_mangle]
pub unsafe extern "C" fn ee_data_graphic_effect_from_record(
    bytes: *const u8,
    fields: *mut EeDataGraphicEffectFields,
    links: *mut EeDataGraphicEffectLinks,
) -> bool {
    // SAFETY: the caller's guarantee
    match graphic_effect_fields(&unsafe { record::<DbGraphicEffect>(bytes) }) {
        Some((f, l)) => {
            // SAFETY: the caller's guarantee
            unsafe {
                *fields = f;
                *links = l;
            }
            true
        }
        None => false,
    }
}

// ---- EEMusicObject (db\dbmusic)

/// A one-byte flag in a record's array (specgen reads `[DbFlag; N]` as `bool[N]`; any byte, nonzero is set).
pub type DbFlag = u8;

/// One record of dbmusic (`EEMusicRecord`, 196 bytes; also what `Data\Music\New Music.txt` fills).
#[repr(C)]
#[derive(Clone, Copy)]
pub struct DbMusic {
    /// The database row id (not read).
    pub mUnusedDatabaseID: i32,
    /// The 0-based row index (not read).
    pub mUnusedIndex: i32,
    /// The row name (the game never reads it; the text's label).
    pub mLabel: [u8; 100],
    /// A plain name: the song appends ".sgt".
    pub mFileName: [u8; 0x34],
    pub mReverbGain: f32,
    pub mReverbHighFreqRTRatio: f32,
    pub mReverbMix: f32,
    pub mReverbTime: f32,
    pub _gapB0: [u8; 5],
    /// The epochs the song plays in (prehistoric .. nano; the space age plays the nano age's).
    pub mInEpoch: [DbFlag; 14],
    pub _gapC3: [u8; 1],
}

const _: () = assert!(core::mem::size_of::<DbMusic>() == 196);

/// `EEMusicObject`'s members from the record (`include/ee/EEMusicObject.h`: the class's third base).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeDataMusicFields {
    pub mReverbGain: f32,
    pub mReverbHighFreqRTRatio: f32,
    pub mReverbMix: f32,
    pub mReverbTime: f32,
    /// By epoch; the space age (14) copies the nano age's flag.
    pub mInEpoch: [bool; 15],
}

/// `EEMusicObject`'s constructor's copy of a record.
pub fn music_fields(r: &DbMusic) -> EeDataMusicFields {
    let mut in_epoch = [false; 15];
    for (k, flag) in r.mInEpoch.iter().enumerate() {
        in_epoch[k] = *flag != 0;
    }
    in_epoch[14] = in_epoch[13];
    EeDataMusicFields {
        mReverbGain: r.mReverbGain,
        mReverbHighFreqRTRatio: r.mReverbHighFreqRTRatio,
        mReverbMix: r.mReverbMix,
        mReverbTime: r.mReverbTime,
        mInEpoch: in_epoch,
    }
}

/// The size of a dbmusic record.
#[no_mangle]
pub extern "C" fn ee_data_music_record_size() -> usize {
    core::mem::size_of::<DbMusic>()
}

/// `EEMusicObject`'s copy of one record (the file name points into `bytes`; the C++ appends ".sgt").
///
/// # Safety
/// `bytes` holds `ee_data_music_record_size()` bytes and outlives the name's use; `fields` is writable.
#[no_mangle]
pub unsafe extern "C" fn ee_data_music_from_record(bytes: *const u8, fields: *mut EeDataMusicFields) -> EeDataName {
    // SAFETY: the caller's guarantee
    let r = unsafe { record::<DbMusic>(bytes) };
    // SAFETY: the caller's guarantee
    unsafe { *fields = music_fields(&r) };
    name_at(bytes, core::mem::offset_of!(DbMusic, mFileName), &r.mFileName)
}

/// `EEMusicObject::ReadNewMusic`'s record: the file name (cut to the field, NUL-terminated as `sprintf` left it
/// when it fits), the four reverb values and every epoch on.
pub fn new_music_record(file: &[u8], reverb: [f32; 4]) -> DbMusic {
    // SAFETY: plain integers, floats and bytes
    let mut r: DbMusic = unsafe { std::mem::zeroed() };
    let kept = file.len().min(r.mFileName.len() - 1);
    r.mFileName[..kept].copy_from_slice(&file[..kept]);
    (r.mReverbGain, r.mReverbHighFreqRTRatio, r.mReverbMix, r.mReverbTime) =
        (reverb[0], reverb[1], reverb[2], reverb[3]);
    r.mInEpoch = [1; 14];
    r
}

/// The flags of a record for `EERules::SetMusicFromFlags` (14, prehistoric .. nano).
///
/// # Safety
/// `bytes` holds `ee_data_music_record_size()` bytes; `out` has room for 14 bools.
#[no_mangle]
pub unsafe extern "C" fn ee_data_music_epoch_flags(bytes: *const u8, out: *mut bool) {
    // SAFETY: the caller's guarantee
    unsafe {
        let r = record::<DbMusic>(bytes);
        for (k, flag) in r.mInEpoch.iter().enumerate() {
            *out.add(k) = *flag != 0;
        }
    }
}

/// [`new_music_record`] into `out` from the NUL-terminated file name and the four reverb values.
///
/// # Safety
/// `file` is NUL-terminated, `reverb` holds four floats; `out` has room for `ee_data_music_record_size()` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_data_new_music_record(file: *const c_char, reverb: *const f32, out: *mut u8) {
    // SAFETY: the caller's guarantee
    unsafe {
        let file = std::ffi::CStr::from_ptr(file).to_bytes();
        let r = new_music_record(file, [*reverb, *reverb.add(1), *reverb.add(2), *reverb.add(3)]);
        std::ptr::write_unaligned(out as *mut DbMusic, r);
    }
}

// ---- EEGraphicObject (db\dbgraphics)

/// One record of dbgraphics after its 32 lead strings (`EEGraphicObjectRecord`, 716 bytes). The 14 animation states
/// are stored in the order 0, 13, 7, 8, 4, 5, 12, 11, 9, 10, 6, 3, 1, 2 (states 4 and 5 without sounds); each state's
/// closing flag shares a dword with the next state's first flag.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct DbGraphicObject {
    /// The row name (the game never reads it; the text's label).
    pub mLabel: [u8; 100],
    /// The database row id (not read).
    pub mUnusedDatabaseID: i32,
    pub mMediaIndex: i32,
    pub mIsDrawn: u8,
    pub mUnusedBuildingValue1: i32,
    pub mUnusedBuildingValue2: i32,
    pub mSharesFrames0: u8,
    pub mFirstFrame0: u32,
    pub mLastFrame0: u32,
    pub mFrameTime0: u32,
    pub mLoops0: u8,
    /// Not read: the loader sets the hold time of state 0 to 300000.
    pub mUnusedHoldTime0: u32,
    pub mSound1Frame0: u32,
    pub mSound2Frame0: u32,
    pub mSound1Index0: u32,
    pub mSound2Index0: u32,
    pub mIsSpeedScaled0: u8,
    pub mSharesFrames13: u8,
    pub mFirstFrame13: u32,
    pub mLastFrame13: u32,
    pub mFrameTime13: u32,
    pub mLoops13: u8,
    pub mHoldTime13: u32,
    pub mSound1Frame13: u32,
    pub mSound2Frame13: u32,
    pub mSound1Index13: u32,
    pub mSound2Index13: u32,
    pub mIsSpeedScaled13: u8,
    pub mSharesFrames7: u8,
    pub mFirstFrame7: u32,
    pub mLastFrame7: u32,
    pub mFrameTime7: u32,
    pub mLoops7: u8,
    pub mHoldTime7: u32,
    pub mSound1Frame7: u32,
    pub mSound2Frame7: u32,
    pub mSound1Index7: u32,
    pub mSound2Index7: u32,
    pub mIsSpeedScaled7: u8,
    pub mSharesFrames8: u8,
    pub mFirstFrame8: u32,
    pub mLastFrame8: u32,
    pub mFrameTime8: u32,
    pub mLoops8: u8,
    pub mHoldTime8: u32,
    pub mSound1Frame8: u32,
    pub mSound2Frame8: u32,
    pub mSound1Index8: u32,
    pub mSound2Index8: u32,
    pub mIsSpeedScaled8: u8,
    pub mSharesFrames4: u8,
    pub mFirstFrame4: u32,
    pub mLastFrame4: u32,
    pub mFrameTime4: u32,
    pub mLoops4: u8,
    pub mHoldTime4: u32,
    pub mIsSpeedScaled4: u8,
    pub mSharesFrames5: u8,
    pub mFirstFrame5: u32,
    pub mLastFrame5: u32,
    pub mFrameTime5: u32,
    pub mLoops5: u8,
    pub mHoldTime5: u32,
    pub mIsSpeedScaled5: u8,
    pub mSharesFrames12: u8,
    pub mFirstFrame12: u32,
    pub mLastFrame12: u32,
    pub mFrameTime12: u32,
    pub mLoops12: u8,
    pub mHoldTime12: u32,
    pub mSound1Frame12: u32,
    pub mSound2Frame12: u32,
    pub mSound1Index12: u32,
    pub mSound2Index12: u32,
    pub mIsSpeedScaled12: u8,
    pub mSharesFrames11: u8,
    pub mFirstFrame11: u32,
    pub mLastFrame11: u32,
    pub mFrameTime11: u32,
    pub mLoops11: u8,
    pub mHoldTime11: u32,
    pub mSound1Frame11: u32,
    pub mSound2Frame11: u32,
    pub mSound1Index11: u32,
    pub mSound2Index11: u32,
    pub mIsSpeedScaled11: u8,
    pub mSharesFrames9: u8,
    pub mFirstFrame9: u32,
    pub mLastFrame9: u32,
    pub mFrameTime9: u32,
    pub mLoops9: u8,
    pub mHoldTime9: u32,
    pub mSound1Frame9: u32,
    pub mSound2Frame9: u32,
    pub mSound1Index9: u32,
    pub mSound2Index9: u32,
    pub mIsSpeedScaled9: u8,
    pub mSharesFrames10: u8,
    pub mFirstFrame10: u32,
    pub mLastFrame10: u32,
    pub mFrameTime10: u32,
    pub mLoops10: u8,
    pub mHoldTime10: u32,
    pub mSound1Frame10: u32,
    pub mSound2Frame10: u32,
    pub mSound1Index10: u32,
    pub mSound2Index10: u32,
    pub mIsSpeedScaled10: u8,
    pub mSharesFrames6: u8,
    pub mFirstFrame6: u32,
    pub mLastFrame6: u32,
    pub mFrameTime6: u32,
    pub mLoops6: u8,
    pub mHoldTime6: u32,
    pub mSound1Frame6: u32,
    pub mSound2Frame6: u32,
    pub mSound1Index6: u32,
    pub mSound2Index6: u32,
    pub mIsSpeedScaled6: u8,
    pub mSharesFrames3: u8,
    pub mFirstFrame3: u32,
    pub mLastFrame3: u32,
    pub mFrameTime3: u32,
    pub mLoops3: u8,
    pub mHoldTime3: u32,
    pub mSound1Frame3: u32,
    pub mSound2Frame3: u32,
    pub mSound1Index3: u32,
    pub mSound2Index3: u32,
    pub mIsSpeedScaled3: u8,
    pub mSharesFrames1: u8,
    pub mFirstFrame1: u32,
    pub mLastFrame1: u32,
    pub mFrameTime1: u32,
    pub mLoops1: u8,
    pub mHoldTime1: u32,
    pub mSound1Frame1: u32,
    pub mSound2Frame1: u32,
    pub mSound1Index1: u32,
    pub mSound2Index1: u32,
    pub mIsSpeedScaled1: u8,
    pub mSharesFrames2: u8,
    pub mFirstFrame2: u32,
    pub mLastFrame2: u32,
    pub mFrameTime2: u32,
    pub mLoops2: u8,
    pub mHoldTime2: u32,
    pub mSound1Frame2: u32,
    pub mSound2Frame2: u32,
    pub mSound1Index2: u32,
    pub mSound2Index2: u32,
    pub mIsSpeedScaled2: u8,
    pub mUnusedIsBuilding: i32,
    pub mUnusedIsSprite: u8,
    pub _gap291: [u8; 0x7],
    pub mUnusedDummyValue: i32,
    pub mUnusedBaseGraphic: i32,
    pub mAlphaReference: i32,
    pub mUnusedBuildingIndex: i32,
    pub _gap2A8: [u8; 0x4],
    pub mKind: i32,
    pub mHasAlpha: u8,
    pub mOpacity: f32,
    pub _gap2B8: [u8; 0x4],
    pub mMipMapBias: f32,
    pub mNumMipLevelsToDrop: i32,
    pub mMaxMipLevelsToDrop: i32,
    pub mCreateMipMaps: u8,
}

const _: () = assert!(core::mem::size_of::<DbGraphicObject>() == 716);

/// The animation states of a graphic.
pub const EE_DATA_GRAPHIC_STATES: usize = 14;

/// `EESoundObject::Get`'s index for no sound (past any table).
pub const EE_DATA_NO_SOUND: c_ulong = c_ulong::MAX;

/// `EEGraphicObject`'s members from the record (`include/ee/EEGraphicObject.h`: the class's second base).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct EeDataGraphicObjectFields {
    /// The graphic-effect kind it is filed under (below 0x48; -1 in an empty object).
    pub mKind: c_ulong,
    pub mMipMapBias: f32,
    pub mOpacity: f32,
    /// The alpha-test reference: the record's percent times 0.01.
    pub mAlphaReference: f32,
    /// -1 in an empty object; 0: the loader drops the object.
    pub mMediaIndex: c_long,
    pub mL318: c_long,
    pub mL31C: c_long,
    pub mL320: c_long,
    pub mL2FC: c_long,
    pub mL4B0: c_long,
    pub mL4B4: c_long,
    pub mNumMipLevelsToDrop: c_long,
    pub mMaxMipLevelsToDrop: c_ulong,
    /// By state: the hold time (state 0's is 300000).
    pub mHoldTimes: [c_ulong; EE_DATA_GRAPHIC_STATES],
    pub mStartFrames: [c_ulong; EE_DATA_GRAPHIC_STATES],
    pub mLastFrames: [c_ulong; EE_DATA_GRAPHIC_STATES],
    pub mFrameTimes: [c_ulong; EE_DATA_GRAPHIC_STATES],
    /// The frames the sounds start at (the record's; the C++ sets -1 where the sound is missing).
    pub mSound1Frames: [c_ulong; EE_DATA_GRAPHIC_STATES],
    pub mSound2Frames: [c_ulong; EE_DATA_GRAPHIC_STATES],
    /// `mLastFrames - mStartFrames + 1`.
    pub mFrameCounts: [c_ulong; EE_DATA_GRAPHIC_STATES],
    pub mIsSpeedScaled: [bool; EE_DATA_GRAPHIC_STATES],
    pub mLoops: [bool; EE_DATA_GRAPHIC_STATES],
    pub mSharesFrames: [bool; EE_DATA_GRAPHIC_STATES],
    pub mB4CE: bool,
    pub mIsDrawn: bool,
    pub mHasAlpha: bool,
    pub mCreateMipMaps: bool,
}

/// The sounds a graphic record names by state, for `EESoundObject::Get` ([`EE_DATA_NO_SOUND`]: none).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct EeDataGraphicObjectSounds {
    pub sound1: [c_ulong; EE_DATA_GRAPHIC_STATES],
    pub sound2: [c_ulong; EE_DATA_GRAPHIC_STATES],
}

/// The constructor's members: no media index or kind, drawn, opacity 0.01, mip maps on, at most two levels dropped.
pub fn graphic_object_default() -> EeDataGraphicObjectFields {
    EeDataGraphicObjectFields {
        mKind: c_ulong::MAX,
        mMipMapBias: 0.0,
        mOpacity: 0.01,
        mAlphaReference: 0.0,
        mMediaIndex: -1,
        mL318: 0,
        mL31C: 0,
        mL320: 0,
        mL2FC: 0,
        mL4B0: 0,
        mL4B4: 0,
        mNumMipLevelsToDrop: 0,
        mMaxMipLevelsToDrop: 2,
        mHoldTimes: [0; EE_DATA_GRAPHIC_STATES],
        mStartFrames: [0; EE_DATA_GRAPHIC_STATES],
        mLastFrames: [0; EE_DATA_GRAPHIC_STATES],
        mFrameTimes: [0; EE_DATA_GRAPHIC_STATES],
        mSound1Frames: [0; EE_DATA_GRAPHIC_STATES],
        mSound2Frames: [0; EE_DATA_GRAPHIC_STATES],
        mFrameCounts: [0; EE_DATA_GRAPHIC_STATES],
        mIsSpeedScaled: [false; EE_DATA_GRAPHIC_STATES],
        mLoops: [false; EE_DATA_GRAPHIC_STATES],
        mSharesFrames: [false; EE_DATA_GRAPHIC_STATES],
        mB4CE: false,
        mIsDrawn: true,
        mHasAlpha: false,
        mCreateMipMaps: true,
    }
}

/// One animation state's block of a graphic record.
struct GraphicState {
    shares: u8,
    first: u32,
    last: u32,
    time: u32,
    loops: u8,
    hold: u32,
    /// (sound 1's frame, sound 2's frame, sound 1, sound 2); states 4 and 5 have none.
    sounds: Option<[u32; 4]>,
    speed: u8,
}

/// The record's state blocks, by state.
fn graphic_states(r: &DbGraphicObject) -> [GraphicState; EE_DATA_GRAPHIC_STATES] {
    #[rustfmt::skip]
    macro_rules! state {
        ($s:ident $f:ident $l:ident $t:ident $o:ident $h:ident $sp:ident) => {
            GraphicState { shares: r.$s, first: r.$f, last: r.$l, time: r.$t, loops: r.$o, hold: r.$h, sounds: None,
                           speed: r.$sp }
        };
        ($s:ident $f:ident $l:ident $t:ident $o:ident $h:ident $sp:ident $f1:ident $f2:ident $i1:ident $i2:ident) => {
            GraphicState { sounds: Some([r.$f1, r.$f2, r.$i1, r.$i2]), ..state!($s $f $l $t $o $h $sp) }
        };
    }
    #[rustfmt::skip]
    let states = [
        GraphicState { hold: 300000, ..state!(mSharesFrames0 mFirstFrame0 mLastFrame0 mFrameTime0 mLoops0 mUnusedHoldTime0
            mIsSpeedScaled0 mSound1Frame0 mSound2Frame0 mSound1Index0 mSound2Index0) },
        state!(mSharesFrames1 mFirstFrame1 mLastFrame1 mFrameTime1 mLoops1 mHoldTime1 mIsSpeedScaled1
            mSound1Frame1 mSound2Frame1 mSound1Index1 mSound2Index1),
        state!(mSharesFrames2 mFirstFrame2 mLastFrame2 mFrameTime2 mLoops2 mHoldTime2 mIsSpeedScaled2
            mSound1Frame2 mSound2Frame2 mSound1Index2 mSound2Index2),
        state!(mSharesFrames3 mFirstFrame3 mLastFrame3 mFrameTime3 mLoops3 mHoldTime3 mIsSpeedScaled3
            mSound1Frame3 mSound2Frame3 mSound1Index3 mSound2Index3),
        state!(mSharesFrames4 mFirstFrame4 mLastFrame4 mFrameTime4 mLoops4 mHoldTime4 mIsSpeedScaled4),
        state!(mSharesFrames5 mFirstFrame5 mLastFrame5 mFrameTime5 mLoops5 mHoldTime5 mIsSpeedScaled5),
        state!(mSharesFrames6 mFirstFrame6 mLastFrame6 mFrameTime6 mLoops6 mHoldTime6 mIsSpeedScaled6
            mSound1Frame6 mSound2Frame6 mSound1Index6 mSound2Index6),
        state!(mSharesFrames7 mFirstFrame7 mLastFrame7 mFrameTime7 mLoops7 mHoldTime7 mIsSpeedScaled7
            mSound1Frame7 mSound2Frame7 mSound1Index7 mSound2Index7),
        state!(mSharesFrames8 mFirstFrame8 mLastFrame8 mFrameTime8 mLoops8 mHoldTime8 mIsSpeedScaled8
            mSound1Frame8 mSound2Frame8 mSound1Index8 mSound2Index8),
        state!(mSharesFrames9 mFirstFrame9 mLastFrame9 mFrameTime9 mLoops9 mHoldTime9 mIsSpeedScaled9
            mSound1Frame9 mSound2Frame9 mSound1Index9 mSound2Index9),
        state!(mSharesFrames10 mFirstFrame10 mLastFrame10 mFrameTime10 mLoops10 mHoldTime10 mIsSpeedScaled10
            mSound1Frame10 mSound2Frame10 mSound1Index10 mSound2Index10),
        state!(mSharesFrames11 mFirstFrame11 mLastFrame11 mFrameTime11 mLoops11 mHoldTime11 mIsSpeedScaled11
            mSound1Frame11 mSound2Frame11 mSound1Index11 mSound2Index11),
        state!(mSharesFrames12 mFirstFrame12 mLastFrame12 mFrameTime12 mLoops12 mHoldTime12 mIsSpeedScaled12
            mSound1Frame12 mSound2Frame12 mSound1Index12 mSound2Index12),
        state!(mSharesFrames13 mFirstFrame13 mLastFrame13 mFrameTime13 mLoops13 mHoldTime13 mIsSpeedScaled13
            mSound1Frame13 mSound2Frame13 mSound1Index13 mSound2Index13),
    ];
    states
}

/// The record loader's copy (`EELoadGraphicObjectRecord`): the record's fields over `fields` (an object's
/// constructor values) and the sounds each state names.
pub fn graphic_object_fields(r: &DbGraphicObject, fields: &mut EeDataGraphicObjectFields) -> EeDataGraphicObjectSounds {
    let mut sounds = EeDataGraphicObjectSounds {
        sound1: [EE_DATA_NO_SOUND; EE_DATA_GRAPHIC_STATES],
        sound2: [EE_DATA_NO_SOUND; EE_DATA_GRAPHIC_STATES],
    };
    let f = fields;
    f.mMediaIndex = r.mMediaIndex as c_long;
    f.mKind = r.mKind as c_ulong;
    f.mIsDrawn = r.mIsDrawn != 0;
    f.mL318 = r.mUnusedBuildingValue1 as c_long;
    f.mL31C = r.mUnusedBuildingValue2 as c_long;
    for (i, s) in graphic_states(r).iter().enumerate() {
        f.mStartFrames[i] = s.first as c_ulong;
        f.mLastFrames[i] = s.last as c_ulong;
        f.mFrameTimes[i] = s.time as c_ulong;
        f.mLoops[i] = s.loops != 0;
        f.mSharesFrames[i] = s.shares != 0;
        f.mHoldTimes[i] = s.hold as c_ulong;
        match s.sounds {
            Some([f1, f2, i1, i2]) => {
                f.mSound1Frames[i] = f1 as c_ulong;
                f.mSound2Frames[i] = f2 as c_ulong;
                sounds.sound1[i] = i1 as c_ulong;
                sounds.sound2[i] = i2 as c_ulong;
            }
            None => {
                f.mSound1Frames[i] = c_ulong::MAX;
                f.mSound2Frames[i] = c_ulong::MAX;
            }
        }
        f.mIsSpeedScaled[i] = s.speed != 0;
    }
    f.mL4B4 = r.mUnusedIsBuilding as c_long;
    f.mB4CE = r.mUnusedIsSprite != 0;
    f.mL4B0 = r.mUnusedDummyValue as c_long;
    f.mL2FC = r.mUnusedBaseGraphic as c_long;
    f.mL320 = r.mUnusedBuildingIndex as c_long;
    f.mHasAlpha = r.mHasAlpha != 0;
    f.mMipMapBias = r.mMipMapBias;
    f.mOpacity = r.mOpacity;
    f.mAlphaReference = r.mAlphaReference as f32 * PERCENT;
    f.mNumMipLevelsToDrop = r.mNumMipLevelsToDrop as c_long;
    f.mCreateMipMaps = r.mCreateMipMaps != 0;
    f.mMaxMipLevelsToDrop = r.mMaxMipLevelsToDrop as c_ulong;
    for i in 0..EE_DATA_GRAPHIC_STATES {
        f.mFrameCounts[i] = f.mLastFrames[i].wrapping_sub(f.mStartFrames[i]).wrapping_add(1);
    }
    sounds
}

/// The size of a dbgraphics record (after its lead strings).
#[no_mangle]
pub extern "C" fn ee_data_graphic_object_record_size() -> usize {
    core::mem::size_of::<DbGraphicObject>()
}

/// The graphic object constructor's members.
///
/// # Safety
/// `fields` is writable.
#[no_mangle]
pub unsafe extern "C" fn ee_data_graphic_object_default(fields: *mut EeDataGraphicObjectFields) {
    // SAFETY: the caller's guarantee
    unsafe { *fields = graphic_object_default() };
}

/// The record loader's copy of one record over the object's members; returns the sounds the states name.
///
/// # Safety
/// `bytes` holds `ee_data_graphic_object_record_size()` bytes; `fields` is readable and writable.
#[no_mangle]
pub unsafe extern "C" fn ee_data_graphic_object_from_record(
    bytes: *const u8,
    fields: *mut EeDataGraphicObjectFields,
) -> EeDataGraphicObjectSounds {
    // SAFETY: the caller's guarantee
    unsafe { graphic_object_fields(&record::<DbGraphicObject>(bytes), &mut *fields) }
}

/// The tag-point kind names, one per graphic-effect kind (0x48; the last two empty: never matched).
#[rustfmt::skip]
const TAG_POINT_KIND_NAMES: [&str; 0x48] = [
    "damage_trail", "2ddebris", "invalid", "exhaust_trail", "2dexplosion", "2dfire", "impact", "impact",
    "invalid", "weapon_mount", "invalid", "damage_trail", "water_splash", "light", "invalid", "invalid",
    "invalid", "invalid", "blast_wave", "invalid", "invalid", "invalid", "winter_breath", "invalid",
    "2dexplosion", "3ddebris", "invalid", "exhaust_trail", "3dexplosion", "3dfire", "weapon_mount", "weapon_mount",
    "terrain_deformation", "weapon_mount", "water_ripple", "invalid", "invalid", "invalid", "invalid", "invalid",
    "invalid", "invalid", "invalid", "invalid", "invalid", "invalid", "invalid", "invalid",
    "invalid", "invalid", "invalid", "invalid", "invalid", "invalid", "invalid", "end_water_ripple",
    "end_2dfire", "invalid", "invalid", "invalid", "invalid", "invalid", "invalid", "invalid",
    "invalid", "invalid", "invalid", "invalid", "invalid", "invalid", "", "",
];

/// The point kinds a tag point named `name` belongs to (`EEGraphicObject::CollectTagPoints`: each kind whose name
/// the tag point's starts with), in kind order.
pub fn tag_point_kinds(name: &[u8]) -> Vec<c_long> {
    (0..TAG_POINT_KIND_NAMES.len())
        .filter(|&k| {
            let kind = TAG_POINT_KIND_NAMES[k].as_bytes();
            !kind.is_empty() && name.starts_with(kind)
        })
        .map(|k| k as c_long)
        .collect()
}

/// [`tag_point_kinds`] of a NUL-terminated name into `out` (room for 0x48); returns the count.
///
/// # Safety
/// `name` is NUL-terminated; `out` has room for 0x48 `long`s.
#[no_mangle]
pub unsafe extern "C" fn ee_data_tag_point_kinds(name: *const c_char, out: *mut c_long) -> usize {
    // SAFETY: the caller's guarantee
    let kinds = tag_point_kinds(unsafe { std::ffi::CStr::from_ptr(name) }.to_bytes());
    // SAFETY: at most 0x48 kinds
    unsafe { std::ptr::copy_nonoverlapping(kinds.as_ptr(), out, kinds.len()) };
    kinds.len()
}

/// The model slot of the shared model in the frame maps (the parts' models take 0 .. 13 in first-seen order).
pub const EE_DATA_GRAPHIC_SHARED_MODEL: c_long = 14;

/// One animated part as `EEGraphicObject::LoadMedia` reads it for the texture-frame maps.
#[derive(Clone, Copy, Debug)]
pub struct GraphicFramePart<'a> {
    pub frame_count: c_ulong,
    pub shares: bool,
    pub start: c_ulong,
    pub last: c_ulong,
    /// The part's model name (`None` or empty: the shared model).
    pub model: Option<&'a [u8]>,
}

/// `EEGraphicObject::LoadMedia`'s texture-frame maps: for each part of more than four frames that shares its
/// frames, the model's slot (a part's model names numbered in first-seen order, [`EE_DATA_GRAPHIC_SHARED_MODEL`] without
/// one) and the entries for it, in the order the original inserted them (a map keeps the first value of a frame):
/// every second frame -1; with an even count the last such frame instead gives its previous frame 1. Returns each
/// part's slot (-1: not mapped) and calls `insert(slot, frame, value)`.
pub fn graphic_frame_maps(
    parts: &[GraphicFramePart<'_>],
    mut insert: impl FnMut(c_long, c_ulong, c_long),
) -> Vec<c_long> {
    let mut names: Vec<&[u8]> = Vec::new();
    let mut slots = vec![-1; parts.len()];
    for (i, p) in parts.iter().enumerate() {
        if !(p.frame_count > 4 && p.shares) {
            continue;
        }
        let slot = match p.model.filter(|m| !m.is_empty()) {
            Some(m) => match names.iter().position(|n| *n == m) {
                Some(k) => k as c_long,
                None => {
                    names.push(m);
                    (names.len() - 1) as c_long
                }
            },
            None => EE_DATA_GRAPHIC_SHARED_MODEL,
        };
        slots[i] = slot;
        let even = p.frame_count % 2 == 0;
        let mut frame = p.start;
        while frame <= p.last {
            if frame.wrapping_sub(p.start) % 2 == 1 {
                if even && frame == p.last {
                    insert(slot, frame.wrapping_sub(1), 1);
                } else {
                    insert(slot, frame, -1);
                }
            }
            if frame == p.last {
                break;
            }
            frame += 1;
        }
    }
    slots
}

/// One part for [`ee_data_graphic_frame_maps`].
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeDataGraphicFramePart {
    pub frame_count: c_ulong,
    pub start: c_ulong,
    pub last: c_ulong,
    /// NUL-terminated, or NULL.
    pub model: *const c_char,
    pub shares: bool,
}

/// [`graphic_frame_maps`] over `count` parts: `insert(context, slot, frame, value)` for each entry, each part's slot
/// into `slots`.
///
/// # Safety
/// `parts` holds `count` parts (their models NUL-terminated or NULL); `slots` has room for `count`.
#[no_mangle]
pub unsafe extern "C" fn ee_data_graphic_frame_maps(
    parts: *const EeDataGraphicFramePart,
    count: usize,
    slots: *mut c_long,
    insert: extern "C" fn(context: *mut c_void, slot: c_long, frame: c_ulong, value: c_long),
    context: *mut c_void,
) {
    // SAFETY: the caller's guarantee
    let parts = unsafe { std::slice::from_raw_parts(parts, count) };
    let parts: Vec<GraphicFramePart<'_>> = parts
        .iter()
        .map(|p| GraphicFramePart {
            frame_count: p.frame_count,
            shares: p.shares,
            start: p.start,
            last: p.last,
            // SAFETY: the caller's guarantee
            model: (!p.model.is_null()).then(|| unsafe { std::ffi::CStr::from_ptr(p.model) }.to_bytes()),
        })
        .collect();
    let out = graphic_frame_maps(&parts, |slot, frame, value| insert(context, slot, frame, value));
    // SAFETY: room for `count`
    unsafe { std::ptr::copy_nonoverlapping(out.as_ptr(), slots, count) };
}

// ---- the tables the game does not read (db\dbrandommap, db\dbterraingraytextures)

/// One record of dbrandommap (`EERandomMapRecord`, 132 bytes; docs/DATA_FORMATS.md 3.38): a terrain band of a random
/// map's terrain group. The game reads no field (the text keeps the retail values; overlay.UNREAD_TABLES).
#[repr(C)]
#[derive(Clone, Copy)]
pub struct DbRandomMap {
    /// 4..71, unique.
    pub mID: i32,
    /// 1..10: two or three rows per group.
    pub mGroup: i32,
    /// The terrain's name ("Grass Tufts", "Snow", "StoneWork").
    pub mName: [u8; 100],
    /// -10, -2, 0.
    pub mElevationLow: i32,
    /// -4, -3, -1, 20.
    pub mElevationHigh: i32,
    /// Four equal dbterrain ids (2..71).
    pub mTerrain: [i32; 4],
}

const _: () = assert!(core::mem::size_of::<DbRandomMap>() == 132);

/// One record of dbterraingraytextures after its lead string, the texture file (`EETerrainGrayTextureRecord`, 120
/// bytes; docs/DATA_FORMATS.md 3.39): a terrain's grey texture. No code opens the table (overlay.UNREAD_TABLES).
#[repr(C)]
#[derive(Clone, Copy)]
pub struct DbTerrainGrayTexture {
    /// "Grass Rough (x)" ... "Space".
    pub mName: [u8; 100],
    /// 2..60.
    pub mID: i32,
    /// -1 (row 0) .. 58.
    pub mIndex: i32,
    /// Red, green, blue (0..255), like dbterrain's colour.
    pub mColor: [i32; 3],
}

const _: () = assert!(core::mem::size_of::<DbTerrainGrayTexture>() == 120);

// ---- the tables as Rust lists

/// The db tables whose entries are only their records' members, held here (`ee/EEDbRecords.h` `EEDbList`: the
/// classes' `sTable` reads them as the vectors they were).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeDataList {
    /// `EEStartingResources::sTable` (rows with a negative index skipped).
    StartingResources = 0,
    /// `gGameVariants` (rows with a negative index skipped).
    GameVariants = 1,
    /// `EEUnitBehavior::sTable` (a row with a negative index is NULL).
    UnitBehaviors = 2,
    /// `EEUCUpgradeTable::sTable` (likewise).
    Upgrades = 3,
    /// `EETerrainTypeTable::sTable` (every row).
    TerrainTypes = 4,
}

/// One entry of a list (boxed: its address is the C++'s object pointer until the list is cleared).
enum Entry {
    StartingResources(EeDataStartingResourcesFields),
    GameVariant(EeDataGameVariantFields),
    UnitBehavior(EeDataUnitBehaviorFields),
    Upgrade(EeDataUpgradeFields),
    TerrainType(EeDataTerrainTypeFields),
}

impl Entry {
    fn fields(&mut self) -> *mut c_void {
        match self {
            Entry::StartingResources(f) => f as *mut _ as *mut c_void,
            Entry::GameVariant(f) => f as *mut _ as *mut c_void,
            Entry::UnitBehavior(f) => f as *mut _ as *mut c_void,
            Entry::Upgrade(f) => f as *mut _ as *mut c_void,
            Entry::TerrainType(f) => f as *mut _ as *mut c_void,
        }
    }
}

const LISTS: usize = 5;
type Lists = [Vec<Option<Box<Entry>>>; LISTS];
static TABLES: std::sync::Mutex<Lists> =
    std::sync::Mutex::new([Vec::new(), Vec::new(), Vec::new(), Vec::new(), Vec::new()]);

fn tables() -> std::sync::MutexGuard<'static, Lists> {
    TABLES.lock().unwrap_or_else(|e| e.into_inner())
}

/// A record's entry for `list` (`None`: a skipped or NULL row), by the tables' copies above.
///
/// # Safety
/// `bytes` holds a whole record of the list's table.
unsafe fn entry_of(list: EeDataList, bytes: *const u8) -> Option<Entry> {
    // SAFETY: the caller's guarantee
    unsafe {
        match list {
            EeDataList::StartingResources => starting_resources_fields(&record(bytes)).map(Entry::StartingResources),
            EeDataList::GameVariants => game_variant_fields(&record(bytes)).map(Entry::GameVariant),
            EeDataList::UnitBehaviors => unit_behavior_fields(&record(bytes)).map(Entry::UnitBehavior),
            EeDataList::Upgrades => upgrade_fields(&record(bytes)).map(Entry::Upgrade),
            EeDataList::TerrainTypes => Some(Entry::TerrainType(terrain_type_fields(&record(bytes)))),
        }
    }
}

/// Whether a list keeps a slot (NULL) for a row without an entry, rather than skipping it.
fn slotted(list: EeDataList) -> bool {
    matches!(list, EeDataList::UnitBehaviors | EeDataList::Upgrades | EeDataList::TerrainTypes)
}

/// The size of a record of `list`'s table.
#[no_mangle]
pub extern "C" fn ee_data_list_record_size(list: EeDataList) -> usize {
    match list {
        EeDataList::StartingResources => core::mem::size_of::<DbStartingResources>(),
        EeDataList::GameVariants => core::mem::size_of::<DbGameVariant>(),
        EeDataList::UnitBehaviors => core::mem::size_of::<DbUnitBehavior>(),
        EeDataList::Upgrades => core::mem::size_of::<DbUpgrade>(),
        EeDataList::TerrainTypes => core::mem::size_of::<DbTerrainType>(),
    }
}

/// A load's start: a slotted list gets `count` slots (as `resize(count, NULL)` did: the first ones stay).
#[no_mangle]
pub extern "C" fn ee_data_list_begin(list: EeDataList, count: usize) {
    if slotted(list) {
        let mut t = tables();
        t[list as usize].resize_with(count, || None);
    }
}

/// Row `index`'s record: a slotted list's slot gets its entry, another list appends it (a skipped row adds nothing).
///
/// # Safety
/// `bytes` holds `ee_data_list_record_size(list)` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_data_list_load(list: EeDataList, index: usize, bytes: *const u8) {
    // SAFETY: the caller's guarantee
    let entry = unsafe { entry_of(list, bytes) }.map(Box::new);
    let mut t = tables();
    let v = &mut t[list as usize];
    if slotted(list) {
        if index >= v.len() {
            v.resize_with(index + 1, || None);
        }
        v[index] = entry;
    } else if entry.is_some() {
        v.push(entry);
    }
}

/// The entry at `index` (its members: the class's base), NULL for a NULL slot or past the end.
#[no_mangle]
pub extern "C" fn ee_data_list_get(list: EeDataList, index: usize) -> *mut c_void {
    let mut t = tables();
    match t[list as usize].get_mut(index) {
        Some(Some(e)) => e.fields(),
        _ => std::ptr::null_mut(),
    }
}

/// The list's length.
#[no_mangle]
pub extern "C" fn ee_data_list_count(list: EeDataList) -> usize {
    tables()[list as usize].len()
}

/// Empties the list (its entries' pointers end).
#[no_mangle]
pub extern "C" fn ee_data_list_clear(list: EeDataList) {
    tables()[list as usize].clear();
}

#[cfg(test)]
mod list_tests {
    use super::*;

    fn bytes_of<T: Copy>(r: &T) -> Vec<u8> {
        // SAFETY: a record is `size_of::<T>()` plain bytes
        unsafe { std::slice::from_raw_parts(r as *const T as *const u8, std::mem::size_of::<T>()) }.to_vec()
    }

    #[test]
    fn skipped_rows_and_null_slots() {
        // the lists are process statics: this test is the only one using them
        ee_data_list_clear(EeDataList::StartingResources);
        // SAFETY: plain integers and bytes
        let mut r: DbStartingResources = unsafe { std::mem::zeroed() };
        for (i, index) in [0, -1, 2].into_iter().enumerate() {
            r.mIndex = index;
            r.mFood = i as i32 * 10;
            // SAFETY: a whole record
            unsafe { ee_data_list_load(EeDataList::StartingResources, i, bytes_of(&r).as_ptr()) };
        }
        assert_eq!(ee_data_list_count(EeDataList::StartingResources), 2);
        let second = ee_data_list_get(EeDataList::StartingResources, 1) as *const EeDataStartingResourcesFields;
        // SAFETY: a live entry
        assert_eq!(unsafe { (*second).mAmounts[0] }, 20);
        assert!(ee_data_list_get(EeDataList::StartingResources, 2).is_null());

        ee_data_list_clear(EeDataList::UnitBehaviors);
        ee_data_list_begin(EeDataList::UnitBehaviors, 3);
        // SAFETY: plain integers and bytes
        let mut b: DbUnitBehavior = unsafe { std::mem::zeroed() };
        b.mIndex = 0;
        b.mCanFlee = 1;
        // SAFETY: a whole record
        unsafe { ee_data_list_load(EeDataList::UnitBehaviors, 2, bytes_of(&b).as_ptr()) };
        b.mIndex = -1;
        // SAFETY: a whole record
        unsafe { ee_data_list_load(EeDataList::UnitBehaviors, 0, bytes_of(&b).as_ptr()) };
        assert_eq!(ee_data_list_count(EeDataList::UnitBehaviors), 3);
        assert!(ee_data_list_get(EeDataList::UnitBehaviors, 0).is_null());
        assert!(ee_data_list_get(EeDataList::UnitBehaviors, 1).is_null());
        let p = ee_data_list_get(EeDataList::UnitBehaviors, 2) as *const EeDataUnitBehaviorFields;
        // SAFETY: a live entry; its address is stable
        assert!(unsafe { (*p).mCanFlee });
        assert_eq!(ee_data_list_get(EeDataList::UnitBehaviors, 2) as *const EeDataUnitBehaviorFields, p);
        ee_data_list_clear(EeDataList::UnitBehaviors);
        assert_eq!(ee_data_list_count(EeDataList::UnitBehaviors), 0);
        assert_eq!(ee_data_list_record_size(EeDataList::Upgrades), 240);
    }
}

// ---- the songs by epoch (EEMusicObject::sSongs, sShuffled)

/// Each epoch's songs and what is left of its shuffled order (the C++'s song objects, as addresses).
static SONGS: std::sync::Mutex<(Vec<Vec<usize>>, Vec<Vec<usize>>)> = std::sync::Mutex::new((Vec::new(), Vec::new()));

fn songs() -> std::sync::MutexGuard<'static, (Vec<Vec<usize>>, Vec<Vec<usize>>)> {
    SONGS.lock().unwrap_or_else(|e| e.into_inner())
}

/// `GetRandom`'s pick: the epoch's shuffled order, refilled when empty by drawing each next song from the songs not
/// yet drawn (`draw(0, left - 1)`, the music's generator), then taken from its end; `None` for an epoch without songs.
pub fn pick_song(songs: &[usize], shuffled: &mut Vec<usize>, mut draw: impl FnMut(c_long, c_long) -> c_long) -> Option<usize> {
    if shuffled.is_empty() && !songs.is_empty() {
        let mut left = songs.to_vec();
        while !left.is_empty() {
            let index = draw(0, left.len() as c_long - 1) as usize;
            shuffled.push(left.remove(index));
        }
    }
    shuffled.pop()
}

/// `LoadTable`'s start: `epochs` empty song lists.
#[no_mangle]
pub extern "C" fn ee_data_songs_reset(epochs: usize) {
    let mut s = songs();
    s.0 = vec![Vec::new(); epochs];
    s.1 = vec![Vec::new(); epochs];
}

/// Adds a song to an epoch's list (the C++ takes its reference).
#[no_mangle]
pub extern "C" fn ee_data_songs_add(epoch: usize, song: *mut c_void) {
    if let Some(list) = songs().0.get_mut(epoch) {
        list.push(song as usize);
    }
}

/// `GetRandom`: the epoch's next song (see [`pick_song`]); NULL for an epoch out of range or without songs.
#[no_mangle]
pub extern "C" fn ee_data_songs_random(
    epoch: c_long,
    draw: extern "C" fn(context: *mut c_void, minimum: c_long, maximum: c_long) -> c_long,
    context: *mut c_void,
) -> *mut c_void {
    let mut s = songs();
    let (all, shuffled) = &mut *s;
    if epoch < 0 || epoch as usize >= all.len() {
        return std::ptr::null_mut();
    }
    let e = epoch as usize;
    pick_song(&all[e], &mut shuffled[e], |a, b| draw(context, a, b)).map_or(std::ptr::null_mut(), |p| p as *mut c_void)
}

/// `ClearAll`: `release` for each song of each epoch (in order), then every list empty (the epochs stay).
#[no_mangle]
pub extern "C" fn ee_data_songs_clear(release: extern "C" fn(song: *mut c_void)) {
    let lists: Vec<Vec<usize>> = {
        let mut s = songs();
        let lists = s.0.iter_mut().map(std::mem::take).collect();
        s.1.iter_mut().for_each(Vec::clear);
        lists
    };
    for song in lists.into_iter().flatten() {
        release(song as *mut c_void);
    }
}

#[cfg(test)]
mod song_tests {
    use super::*;

    #[test]
    fn the_shuffle_draws_from_the_songs_left_and_plays_from_the_end() {
        let mut shuffled = Vec::new();
        let mut draws = Vec::new();
        // always the last song left: the order reversed, then played from the end: the original order
        let mut pick = |shuffled: &mut Vec<usize>| {
            pick_song(&[10, 20, 30], shuffled, |a, b| {
                draws.push((a, b));
                b
            })
        };
        assert_eq!(pick(&mut shuffled), Some(10));
        assert_eq!(pick(&mut shuffled), Some(20));
        assert_eq!(pick(&mut shuffled), Some(30));
        assert_eq!(draws, vec![(0, 2), (0, 1), (0, 0)]);
        assert_eq!(pick_song(&[], &mut shuffled, |_, _| 0), None);
        // always the first: played back to front
        let mut s = Vec::new();
        assert_eq!(pick_song(&[1, 2, 3], &mut s, |a, _| a), Some(3));
        assert_eq!(s, vec![1, 2]);
    }
}

#[cfg(test)]
mod tests_more {
    use super::*;

    fn zeroed<T: Copy>() -> T {
        // SAFETY: the records are plain integers, floats and bytes
        unsafe { std::mem::zeroed() }
    }

    #[test]
    fn upgrades_pair_cost_and_step_and_leave_factor_13() {
        let mut r: DbUpgrade = zeroed();
        r.mIndex = 0;
        r.mAttackCost = 2.0;
        r.mAttackStep = 0.5;
        r.mTransportCost = 7.0;
        r.mRepairCost = 3.0;
        r.mHasRepairStep = 1;
        r.mMaxLevel = 4;
        let f = upgrade_fields(&r).unwrap();
        assert_eq!(f.mFactors[1], EeDataUpgradeFactor { mScale: 2.0, mOffset: 0.5 });
        assert_eq!(f.mFactors[15].mScale, 7.0);
        assert_eq!(f.mFactors[12], EeDataUpgradeFactor { mScale: 3.0, mOffset: 1.0 });
        assert_eq!(f.mFactors[13], EeDataUpgradeFactor { mScale: 1.0, mOffset: 0.0 });
        assert_eq!(f.mMaxLevel, 4);
        r.mHasRepairStep = 2; // not `true`: the C++ compared the byte with 1
        assert_eq!(upgrade_fields(&r).unwrap().mFactors[12].mOffset, 0.0);
        r.mIndex = -1;
        assert_eq!(upgrade_fields(&r), None);
        assert_eq!(core::mem::offset_of!(DbUpgrade, mHasRepairStep), 0xec);
    }

    #[test]
    fn the_dark_colour_is_the_second_at_sixty_percent() {
        let mut r: DbColor = zeroed();
        (r.mColorRed, r.mColor2Red, r.mColor2Blue, r.mColor4Green) = (0.1, 0.5, 1.0, 0.25);
        let c = player_colors(&r);
        assert_eq!(c.colors[0], [0.1, 0.0, 0.0]);
        assert_eq!(c.colors[2], [0.5 * 0.6, 0.0, 0.6]);
        assert_eq!(c.colors[3], [0.0, 0.25, 0.0]);
        assert_eq!(core::mem::offset_of!(DbColor, mColor4Blue), 0x8c);
    }

    #[test]
    fn targeting_rows_skip_the_unused_values() {
        let mut r: DbAiTargeting = zeroed();
        (r.mAttackerBonus, r.mImmobileTargetBonus, r.mUnused1, r.mMaxValue) = (1, 9, 77, 10);
        let mut out = [0 as c_long; 10];
        // SAFETY: a whole record, room for ten
        unsafe { ee_data_ai_targeting_from_record(&r as *const DbAiTargeting as *const u8, out.as_mut_ptr()) };
        assert_eq!(out, [1, 0, 0, 0, 0, 0, 0, 0, 9, 10]);
        assert_eq!(core::mem::offset_of!(DbAiTargeting, mMaxValue), 0x9c);
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn zeroed<T: Copy>() -> T {
        // SAFETY: the records are plain integers, floats and bytes: all bits zero is a value of each
        unsafe { std::mem::zeroed() }
    }

    fn bytes_of<T: Copy>(r: &T) -> Vec<u8> {
        // SAFETY: a record is `size_of::<T>()` plain bytes
        unsafe { std::slice::from_raw_parts(r as *const T as *const u8, std::mem::size_of::<T>()) }.to_vec()
    }

    #[test]
    fn the_records_sit_at_the_offsets_of_the_file() {
        assert_eq!(core::mem::offset_of!(DbAnimal, mAnimalID), 0x68);
        assert_eq!(core::mem::offset_of!(DbAnimal, mMinBirthInterval), 0x7c);
        assert_eq!(core::mem::offset_of!(DbAnimal, mUnusedListValue1), 0x88);
        assert_eq!(core::mem::offset_of!(DbAnimal, mUnusedFactor), 0x9c);
        assert_eq!(core::mem::offset_of!(DbAnimal, mNewbornGrowth), 0xc0);
        assert_eq!(core::mem::offset_of!(DbAnimal, mSound3), 0xf0);
        assert_eq!(core::mem::offset_of!(DbUnitBehavior, mRangedCanMoveFreely), 0x72);
        assert_eq!(core::mem::offset_of!(DbToHit, mPercents), 0x6c);
        assert_eq!(core::mem::offset_of!(DbStartingResources, mIron), 0x80);
    }

    #[test]
    fn an_animal_keeps_milliseconds_and_names_its_media() {
        let mut r: DbAnimal = zeroed();
        r.mUnitType = 77;
        r.mMinBirthInterval = 30;
        r.mMaxFollowInterval = 3_000_000; // past the int range in milliseconds: wraps as the C++'s product
        r.mMinRoamMargin = 4;
        r.mBreeds = 2;
        r.mUnusedIsFlightless = 1;
        r.mUnusedFactor = 1.5;
        r.mMedia = 12;
        r.mSound1 = -1;
        r.mSound2 = 5;
        r.mSound3 = 6;
        r.mUnusedListValue3 = 9;
        let mut f = EeDataAnimalFields::default();
        let mut m = EeDataAnimalMedia::default();
        // SAFETY: a whole record, writable outputs
        unsafe { ee_data_animal_from_record(bytes_of(&r).as_ptr(), &mut f, &mut m) };
        assert_eq!(f.mUnitType, 77);
        assert_eq!(f.mMinBirthInterval, 30_000);
        assert_eq!(f.mMaxFollowInterval, 3_000_000i32.wrapping_mul(1000) as c_long);
        assert_eq!(f.mMinTerritoryMargin, 4);
        assert!(f.mBreeds && f.mB84 && !f.mIsPredator);
        assert_eq!(f.mF28, 1.5);
        assert_eq!(m, EeDataAnimalMedia { media: 12, sounds: [-1, 5, 6], list: [0, 0, 9, 0] });
        assert_eq!(ee_data_animal_record_size(), 272);
    }

    #[test]
    fn a_behaviour_row_with_a_negative_index_is_none() {
        let mut r: DbUnitBehavior = zeroed();
        r.mIndex = -1;
        r.mCanFlee = 1;
        assert_eq!(unit_behavior_fields(&r), None);
        r.mIndex = 0;
        r.mMeleeCanMoveFreely = 1;
        let f = unit_behavior_fields(&r).unwrap();
        assert!(f.mCanFlee && f.mMeleeCanMoveFreely && !f.mCanMoveFreely);
        let mut out = EeDataUnitBehaviorFields::default();
        // SAFETY: a whole record
        assert!(unsafe { ee_data_unit_behavior_from_record(bytes_of(&r).as_ptr(), &mut out) });
        assert_eq!(out, f);
    }

    #[test]
    fn to_hit_rows_are_fractions() {
        let mut r: DbToHit = zeroed();
        r.mPercents = [100, 50, 0, 25, 75, 33];
        let mut out = [0.0f32; 6];
        // SAFETY: a whole record, room for six
        unsafe { ee_data_to_hit_from_record(bytes_of(&r).as_ptr(), out.as_mut_ptr()) };
        assert_eq!(out, [100.0 * 0.01, 50.0 * 0.01, 0.0, 25.0 * 0.01, 75.0 * 0.01, 33.0 * 0.01]);
        assert_eq!(out[1].to_bits(), (50.0f32 * 0.01f32).to_bits());
    }

    #[test]
    fn starting_resources_put_the_amounts_in_resource_order() {
        let mut r: DbStartingResources = zeroed();
        r.mIndex = 2;
        (r.mFood, r.mWood, r.mStone, r.mGold, r.mIron, r.mNameID) = (1, 2, 3, 4, 5, 900);
        let f = starting_resources_fields(&r).unwrap();
        assert_eq!(f.mAmounts, [1, 2, 3, 4, 5]);
        assert_eq!(f.mNameID, 900);
        r.mIndex = -3;
        assert_eq!(starting_resources_fields(&r), None);
    }
}

#[cfg(test)]
mod tests_round3 {
    use super::*;

    fn zeroed<T: Copy>() -> T {
        // SAFETY: the records are plain integers, floats and bytes
        unsafe { std::mem::zeroed() }
    }

    fn bytes_of<T: Copy>(r: &T) -> Vec<u8> {
        // SAFETY: a record is `size_of::<T>()` plain bytes
        unsafe { std::slice::from_raw_parts(r as *const T as *const u8, std::mem::size_of::<T>()) }.to_vec()
    }

    fn name_of(n: &EeDataName) -> Vec<u8> {
        // SAFETY: the name points into a live record
        unsafe { std::slice::from_raw_parts(n.text as *const u8, n.length) }.to_vec()
    }

    #[test]
    fn the_records_sit_at_the_offsets_of_the_file() {
        use core::mem::offset_of;
        assert_eq!(offset_of!(DbTerrain, mIsWater), 0x84);
        assert_eq!(offset_of!(DbTerrain, mColorType), 0x88);
        assert_eq!(offset_of!(DbTerrain, mLast), 0x98);
        assert_eq!(offset_of!(DbTerrain, mBlue), 0xb8);
        assert_eq!(offset_of!(DbTerrain, mIsAmbientsOnly), 0xcb);
        assert_eq!(offset_of!(DbCliffTerrain, mUnusedFlag), 0x78);
        assert_eq!(offset_of!(DbButton, mID), 0xcc);
        assert_eq!(offset_of!(DbButton, mHotKey), 0xdc);
        assert_eq!(offset_of!(DbDamageTable, mDamage), 0x6c);
        assert_eq!(offset_of!(DbSound, mVolume), 0x7c);
        assert_eq!(offset_of!(DbGraphicEffect, mEmitPositionSpread), 0xd8);
        assert_eq!(offset_of!(DbGraphicEffect, mDetailLevel), 0xe4);
        assert_eq!(offset_of!(DbMusic, mReverbGain), 0xa0);
        assert_eq!(offset_of!(DbMusic, mInEpoch), 0xb5);
        assert_eq!(offset_of!(DbGraphicObject, mMediaIndex), 0x68);
        assert_eq!(offset_of!(DbGraphicObject, mSharesFrames13), 0xa1);
        assert_eq!(offset_of!(DbGraphicObject, mFirstFrame13), 0xa4);
        assert_eq!(offset_of!(DbGraphicObject, mIsSpeedScaled4), 0x130);
        assert_eq!(offset_of!(DbGraphicObject, mIsSpeedScaled2), 0x288);
        assert_eq!(offset_of!(DbGraphicObject, mKind), 0x2ac);
        assert_eq!(offset_of!(DbGraphicObject, mCreateMipMaps), 0x2c8);
    }

    #[test]
    fn a_terrain_record_gives_its_members_name_colour_and_ranges() {
        let mut r: DbTerrain = zeroed();
        r.mName[..5].copy_from_slice(b"Grass");
        (r.mID, r.mColumns, r.mRows, r.mColorType, r.mIsWater, r.mIsAmbientsOnly) = (7, 3, 2, 4, 1, 2);
        (r.mRed, r.mGreen, r.mBlue) = (255, 0, 51);
        r.mFirst = [10, 0, 5];
        r.mLast = [12, 9, 5];
        let bytes = bytes_of(&r);
        let mut f = EeDataTerrainFields::default();
        // SAFETY: plain data: the name is overwritten below
        let mut e: EeDataTerrainExtra = unsafe { std::mem::zeroed() };
        // SAFETY: a whole record, writable outputs
        unsafe { ee_data_terrain_from_record(bytes.as_ptr(), &mut f, &mut e) };
        assert_eq!((f.mID, f.mColumns, f.mRows, f.mNumTextures, f.mColorType), (7, 3, 2, 6, 4));
        assert!(f.mIsWater && f.mIsAmbientsOnly && !f.mHasAlpha);
        assert_eq!(name_of(&e.name), b"Grass");
        assert_eq!(e.color, [1.0, 0.0, 51.0 / 255.0]);
        assert_eq!((e.first, e.last), ([10, 0, 5], [12, 9, 5]));
        assert_eq!(terrain_default().mColorType, -1);
        // a name filling the field has no NUL
        r.mName = [b'x'; 0x68];
        let bytes = bytes_of(&r);
        // SAFETY: as above
        unsafe { ee_data_terrain_from_record(bytes.as_ptr(), &mut f, &mut e) };
        assert_eq!(e.name.length, 0x68);
    }

    #[test]
    fn terrain_textures_are_numbered_before_the_extension() {
        assert_eq!(terrain_texture_file(b"textures\\grass.png", 3), b"textures\\grass_3.png");
        assert_eq!(terrain_texture_file(b"a.b.c", 1), b"a.b_1.c");
        assert_eq!(terrain_texture_file(b"grass", 12), b"grass_12");
        let mut out = [0 as c_char; 8];
        // SAFETY: a NUL-terminated name, room for 8
        let n = unsafe { ee_data_terrain_texture_file(c"grass.png".as_ptr(), 1, out.as_mut_ptr(), out.len()) };
        assert_eq!(n, 11);
        // SAFETY: NUL-terminated by the call
        assert_eq!(unsafe { std::ffi::CStr::from_ptr(out.as_ptr()) }.to_bytes(), b"grass_1");
    }

    #[test]
    fn a_cliff_row_with_a_negative_index_is_empty() {
        let mut r: DbCliffTerrain = zeroed();
        r.mName[..4].copy_from_slice(b"Rock");
        (r.mIndex, r.mFirstTerrain, r.mLastTerrain, r.mUnusedValue, r.mUnusedFlag) = (0, 40, 44, 9, 1);
        let bytes = bytes_of(&r);
        let mut f = EeDataCliffTerrainFields::default();
        let mut n = EeDataName { text: std::ptr::null(), length: 0 };
        // SAFETY: a whole record, writable outputs
        assert!(unsafe { ee_data_cliff_terrain_from_record(bytes.as_ptr(), &mut f, &mut n) });
        assert_eq!(f, EeDataCliffTerrainFields { mFirstTerrain: 40, mLastTerrain: 44, mL1C: 9, mB20: true });
        assert_eq!(name_of(&n), b"Rock");
        r.mIndex = -1;
        assert_eq!(cliff_terrain_fields(&r), None);
    }

    #[test]
    fn a_button_record_names_its_texture() {
        let mut r: DbButton = zeroed();
        r.mFileName[..9].copy_from_slice(b"btn01.png");
        (r.mID, r.mRow, r.mColumn, r.mPosition, r.mHotKey) = (3, 2, 5, 7, 11);
        let bytes = bytes_of(&r);
        let mut f = EeDataButtonFields::default();
        // SAFETY: a whole record
        let n = unsafe { ee_data_button_from_record(bytes.as_ptr(), &mut f) };
        assert_eq!(name_of(&n), b"btn01.png");
        assert_eq!(f, EeDataButtonFields { mPosition: 7, mHotKeyIndex: 11, mColumn: 5, mRow: 2, mID: 3 });
    }

    #[test]
    fn button_cells_follow_the_texture_size() {
        let t = EeDataButtonTexture { width: 256, height: 128, drop: 1, surface_height: 128 };
        assert_eq!(button_cell_offset(2, 1, &t), [82.0 / 512.0, 41.0 / 256.0]);
        let c = button_cell(&t, [0.25, 0.5], true, [39.0, 19.0]);
        let texel = 1.0f32 / 256.0;
        assert_eq!((c.left, c.top), (texel + 0.25, texel + 0.5));
        assert_eq!((c.right, c.bottom), (texel + 0.25 + 40.0 / 512.0, texel + 0.5 + 40.0 / 256.0));
        assert_eq!((c.scale_x, c.scale_y), (40.0 / 256.0, 20.0 / 128.0));
        let small = EeDataButtonTexture { width: 64, height: 32, drop: 0, surface_height: 32 };
        let p = button_cell(&small, [0.0, 0.0], false, [63.0, 31.0]);
        assert_eq!((p.left, p.top, p.right, p.bottom), (0.0, 0.0, 63.0 / 64.0, 31.0 / 32.0));
    }

    #[test]
    fn damage_rows_copy_every_column() {
        let mut r: DbDamageTable = zeroed();
        for (k, d) in r.mDamage.iter_mut().enumerate() {
            *d = k as i32 * 3;
        }
        let mut out = [0i32; DAMAGE_COLUMNS];
        // SAFETY: a whole record, room for the columns
        unsafe { ee_data_damage_table_from_record(bytes_of(&r).as_ptr(), out.as_mut_ptr()) };
        assert_eq!(out, r.mDamage);
    }

    #[test]
    fn a_sound_keeps_its_volume_as_a_factor() {
        let mut r: DbSound = zeroed();
        (r.mMaxInstances, r.mCategory, r.mUnusedPriority, r.mLooping, r.mVolume) = (4, 2, 9, 3, 80);
        let f = sound_fields(&r);
        assert_eq!(
            f,
            EeDataSoundFields { mVolume: 80.0 * 0.01, mMaxInstances: 4, mCategory: 2, mL24: 9, mLooping: true }
        );
    }

    #[test]
    fn a_graphic_effect_names_its_media_and_links() {
        let mut r: DbGraphicEffect = zeroed();
        (r.mID, r.mKind, r.mMediaIndex, r.mHeight, r.mStartSize) = (5, 0x23, -1, 2.5, 1.5);
        (r.mLinkedEffectIndex1, r.mLinkedEffectIndex2, r.mLinkedEffectIndex3) = (3, -1, 0);
        (r.mIsScenarioListed, r.mEndsWithUnitOrDuration, r.mDetailLevel) = (1, 1, 2);
        let (f, l) = graphic_effect_fields(&r).unwrap();
        assert_eq!((f.mID, f.mKind, f.mSize, f.mStartSize, f.mDetailLevel), (5, 0x23, 2.5, 1.5, 2));
        assert!(f.mIsScenarioListed && f.mEndsWithUnitOrDuration && !f.mIsEmitFlat && !f.mFlag1);
        assert_eq!(l.media, -1);
        assert_eq!(l.linked, [3, c_ulong::MAX, 0]);
        r.mID = 0;
        assert_eq!(graphic_effect_fields(&r), None);
    }

    #[test]
    fn a_song_plays_the_nano_songs_in_space() {
        let mut r: DbMusic = zeroed();
        r.mFileName[..4].copy_from_slice(b"song");
        r.mInEpoch[13] = 7;
        r.mInEpoch[0] = 1;
        r.mReverbMix = 0.5;
        let bytes = bytes_of(&r);
        let mut f = EeDataMusicFields::default();
        // SAFETY: a whole record
        let n = unsafe { ee_data_music_from_record(bytes.as_ptr(), &mut f) };
        assert_eq!(name_of(&n), b"song");
        assert!(f.mInEpoch[0] && !f.mInEpoch[1] && f.mInEpoch[13] && f.mInEpoch[14]);
        assert_eq!(f.mReverbMix, 0.5);
        let mut flags = [false; 14];
        // SAFETY: a whole record, room for 14
        unsafe { ee_data_music_epoch_flags(bytes.as_ptr(), flags.as_mut_ptr()) };
        assert_eq!(flags.iter().filter(|f| **f).count(), 2);
        let m = new_music_record(&[b'n'; 80], [1.0, 2.0, 3.0, 4.0]);
        assert_eq!(&m.mFileName[..0x33], &[b'n'; 0x33][..]);
        assert_eq!(m.mFileName[0x33], 0);
        assert_eq!((m.mReverbGain, m.mReverbTime, m.mInEpoch), (1.0, 4.0, [1; 14]));
    }

    #[test]
    fn a_graphic_record_fills_its_states_by_the_stored_order() {
        let mut r: DbGraphicObject = zeroed();
        (r.mMediaIndex, r.mKind, r.mIsDrawn, r.mAlphaReference, r.mMaxMipLevelsToDrop) = (12, 5, 0, 50, 3);
        (r.mFirstFrame13, r.mLastFrame13, r.mHoldTime13, r.mSound1Index13, r.mSound1Frame13) = (10, 19, 7, 4, 2);
        (r.mFirstFrame0, r.mLastFrame0, r.mUnusedHoldTime0) = (0, 4, 99);
        (r.mFirstFrame4, r.mLastFrame4, r.mHoldTime4, r.mIsSpeedScaled4) = (5, 3, 8, 1);
        r.mSharesFrames2 = 1;
        let mut f = graphic_object_default();
        let s = graphic_object_fields(&r, &mut f);
        assert_eq!((f.mMediaIndex, f.mKind, f.mIsDrawn, f.mMaxMipLevelsToDrop), (12, 5, false, 3));
        assert_eq!(f.mAlphaReference, 50.0 * 0.01);
        assert_eq!((f.mStartFrames[13], f.mLastFrames[13], f.mFrameCounts[13], f.mHoldTimes[13]), (10, 19, 10, 7));
        assert_eq!((s.sound1[13], f.mSound1Frames[13], s.sound2[13]), (4, 2, 0));
        assert_eq!((f.mHoldTimes[0], f.mFrameCounts[0]), (300000, 5));
        // states 4 and 5 have no sounds; a range backwards wraps its count
        assert_eq!((s.sound1[4], s.sound2[5], f.mSound1Frames[4]), (EE_DATA_NO_SOUND, EE_DATA_NO_SOUND, c_ulong::MAX));
        assert_eq!(f.mFrameCounts[4], (3 as c_ulong).wrapping_sub(5).wrapping_add(1));
        assert!(f.mIsSpeedScaled[4] && f.mSharesFrames[2] && !f.mSharesFrames[1]);
        let d = graphic_object_default();
        assert_eq!(
            (d.mMediaIndex, d.mKind, d.mOpacity, d.mIsDrawn, d.mCreateMipMaps),
            (-1, c_ulong::MAX, 0.01, true, true)
        );
    }

    #[test]
    fn tag_points_match_every_kind_they_start_with() {
        assert_eq!(tag_point_kinds(b"impact01"), vec![6, 7]);
        assert_eq!(tag_point_kinds(b"weapon_mount_2"), vec![9, 30, 31, 33]);
        assert_eq!(tag_point_kinds(b"attack"), Vec::<c_long>::new());
        assert_eq!(tag_point_kinds(b"end_2dfire"), vec![56]);
        assert_eq!(tag_point_kinds(b""), Vec::<c_long>::new());
    }

    /// The original loops of `EEGraphicObject::LoadMedia` (the C++ before this round), over a map that keeps the
    /// first value of a key.
    fn frame_maps_by_the_original(parts: &[GraphicFramePart<'_>]) -> Vec<std::collections::BTreeMap<c_ulong, c_long>> {
        let mut maps = vec![std::collections::BTreeMap::new(); 15];
        let mut names: Vec<(Vec<u8>, usize)> = Vec::new();
        let mut next = 0;
        for p in parts {
            if !(p.frame_count > 4 && p.shares) {
                continue;
            }
            let index = match p.model {
                Some(m) if !m.is_empty() => match names.iter().find(|(n, _)| n.as_slice() == m) {
                    Some((_, k)) => *k,
                    None => {
                        names.push((m.to_vec(), next));
                        next += 1;
                        next - 1
                    }
                },
                _ => 14,
            };
            let map = &mut maps[index];
            let mut frame = p.start;
            while frame <= p.last {
                if (frame - p.start) % 2 == 1 {
                    if p.frame_count % 2 != 0 || frame != p.last {
                        map.entry(frame).or_insert(-1);
                    } else {
                        map.entry(frame - 1).or_insert(1);
                    }
                }
                frame += 1;
            }
        }
        maps
    }

    #[test]
    fn frame_maps_equal_the_original_loops() {
        let models: [&[u8]; 4] = [b"a.cem", b"b.cem", b"", b"a.cem"];
        let mut seed = 12345u32;
        let mut next = |n: u32| {
            seed = seed.wrapping_mul(1103515245).wrapping_add(12345);
            (seed >> 16) % n
        };
        for _ in 0..2000 {
            let parts: Vec<GraphicFramePart<'_>> = (0..14)
                .map(|_| {
                    let start = next(20) as c_ulong;
                    let last = start + next(12) as c_ulong;
                    GraphicFramePart {
                        frame_count: last - start + 1,
                        shares: next(3) != 0,
                        start,
                        last,
                        model: match next(5) {
                            4 => None,
                            k => Some(models[k as usize]),
                        },
                    }
                })
                .collect();
            let mut maps = vec![std::collections::BTreeMap::new(); 15];
            let slots = graphic_frame_maps(&parts, |slot, frame, value| {
                maps[slot as usize].entry(frame).or_insert(value);
            });
            assert_eq!(maps, frame_maps_by_the_original(&parts));
            for (p, s) in parts.iter().zip(&slots) {
                assert_eq!(*s >= 0, p.frame_count > 4 && p.shares);
            }
        }
    }
}
