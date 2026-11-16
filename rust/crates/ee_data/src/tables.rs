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

use std::ffi::c_long;

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
