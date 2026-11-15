//! The civilization manager and the civilization powers' table whole in Rust (the civilization builder lane, round
//! 2): `EECivilizationManager` (Civilization Builder/EECivilizationManager.cpp, 0x281458..0x2830ae) and
//! `EECivPowersManager` (EECivPowersManager.cpp, 0x2830d0..0x283b2e) without their simulation (`civ_apply.rs`).
//!
//! The tables are the game's one instance of each (`gCivilizationManager`, `gCivPowersManager`): the categories of
//! dbcivilization with their levels (the record's 60, or as many as civbuilder.yaml's category text gives) and ids,
//! the special powers' category the load appends (one level per attribute that applies a power), the premade
//! civilizations sorted by index with their eras in order, and dbcivpowers' records. They are written only by the
//! loads (the database's, before any game thread runs) and read under one lock; the records keep the C++ layouts
//! (`EECivilizationCategoryEntry`, `EECivilizationEntry`, `EECivPower`, now typedefs of these), so a pointer the
//! C++ takes (`GetCategory`, `GetPowerRecord`) names the record in place, as the original's vectors did.
//!
//! The rules (civbuilder.yaml's attributes and category texts, civpowers.yaml) are read from the game's
//! `EERules::mRust` ([`bind_rules`]), the same values the C++ snapshot held.

#![allow(non_snake_case)]

use ee_data::ffi::EeDataRules;
use ee_data::generated::rules_gen::{CivAttribute, CivPowerRule, K_CIV_STAT_POWER};
use ee_data::rules::Rules;
use std::os::raw::c_char;
use std::sync::atomic::{AtomicPtr, Ordering};
use std::sync::{OnceLock, RwLock, RwLockReadGuard, RwLockWriteGuard};

/// An upgrade id: (category << 16) | level (ee/EECivUpgrade.h).
pub type EeTechCivUpgrade = u32;
/// `kCivUpgradeLevelBits`, `kCivUpgradeLevelMask`
pub(crate) const LEVEL_BITS: u32 = 16;
pub(crate) const LEVEL_MASK: u32 = 0xffff;
/// `kCivPowerFlamingArrows`: the first power id (the powers' records are indexed by id - 0x26)
pub(crate) const POWER_FIRST: i32 = 0x26;
/// `IDS_CIV_BONUS_BASE`, `IDS_CIV_POWER_BASE` (ee/EEStringIDs.h)
pub(crate) const IDS_CIV_BONUS_BASE: i32 = 29716;
pub(crate) const IDS_CIV_POWER_BASE: i32 = 32780;
/// `kErrorOK`, `kErrorEndOfFile` (FSMemoryFile::Read past the end)
pub(crate) const CIV_OK: i32 = 0;
pub(crate) const CIV_END_OF_FILE: i32 = 0x12;

/// One level of a category: its value and bonus (`EECivilizationUpgradeValue`).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeTechCivLevel {
    pub mValue: f32,
    pub mBonus: f32,
}

/// A category row of dbcivilization (`EECivilizationCategoryEntry`, 600 bytes).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct EeTechCivCategory {
    pub mDatabaseID: i32,
    pub mIndex: i32,
    /// the category name (the game never reads it)
    pub mLabel: [c_char; 100],
    pub mNameID: i32,
    /// the extra cost per pick of the category
    pub mCost: i32,
    pub mUnitSet: i32,
    pub mLevels: [EeTechCivLevel; 60],
}

impl Default for EeTechCivCategory {
    fn default() -> Self {
        EeTechCivCategory {
            mDatabaseID: 0,
            mIndex: 0,
            mLabel: [0; 100],
            mNameID: 0,
            mCost: 0,
            mUnitSet: 0,
            mLevels: [EeTechCivLevel::default(); 60],
        }
    }
}

/// A premade civilization (`EECivilizationEntry`, 120 bytes).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct EeTechCivEntry {
    pub mUnusedDatabaseID: i32,
    pub mIndex: i32,
    /// the civilization's file (NUL-terminated)
    pub mFileName: [c_char; 100],
    /// the file's checksum: its name's characters, its legacy upgrade ids and their count
    pub mChecksum: i32,
    pub mNameID: i32,
    pub mEraNameID: i32,
}

impl Default for EeTechCivEntry {
    fn default() -> Self {
        EeTechCivEntry { mUnusedDatabaseID: 0, mIndex: 0, mFileName: [0; 100], mChecksum: 0, mNameID: 0, mEraNameID: 0 }
    }
}

/// A civilization power (`EECivPower`, 152 bytes).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct EeTechCivPower {
    pub mLabel: [c_char; 100],
    pub mUnusedDatabaseID: i32,
    pub mID: i32,
    /// the powers' category's level bonus
    pub mBonus: f32,
    pub mUnitSet: i32,
    pub mTechnology: i32,
    pub mMiningLimit: i32,
    pub mChance: i32,
    pub mUnused1: i32,
    pub mAmount: f32,
    pub mUnused2: i32,
    pub mUnused3: i32,
    pub mUnused4: i32,
    pub mUnitType: i32,
}

impl Default for EeTechCivPower {
    fn default() -> Self {
        EeTechCivPower {
            mLabel: [0; 100],
            mUnusedDatabaseID: 0,
            mID: 0,
            mBonus: 0.0,
            mUnitSet: 0,
            mTechnology: 0,
            mMiningLimit: 0,
            mChance: 0,
            mUnused1: 0,
            mAmount: 0.0,
            mUnused2: 0,
            mUnused3: 0,
            mUnused4: 0,
            mUnitType: 0,
        }
    }
}

const _: () = assert!(std::mem::size_of::<EeTechCivCategory>() == 600);
const _: () = assert!(std::mem::size_of::<EeTechCivEntry>() == 120);
const _: () = assert!(std::mem::size_of::<EeTechCivPower>() == 152);

/// The manager's and the powers' tables.
#[derive(Debug, Default)]
pub struct CivTables {
    pub categories: Vec<EeTechCivCategory>,
    /// each category's levels (by level - 1)
    pub levels: Vec<Vec<EeTechCivLevel>>,
    /// each category's id (dbcivilization's row id through civbuilder.yaml; empty without one)
    pub ids: Vec<Vec<u8>>,
    pub civilizations: Vec<EeTechCivEntry>,
    pub eras: Vec<i32>,
    /// the special powers' category (`sPowersCategory`, 0x965568)
    pub powers_category: u32,
    pub powers: Vec<EeTechCivPower>,
}

static TABLES: RwLock<CivTables> = RwLock::new(CivTables {
    categories: Vec::new(),
    levels: Vec::new(),
    ids: Vec::new(),
    civilizations: Vec::new(),
    eras: Vec::new(),
    powers_category: 0,
    powers: Vec::new(),
});

/// The tables, read.
pub fn tables() -> RwLockReadGuard<'static, CivTables> {
    TABLES.read().unwrap_or_else(|e| e.into_inner())
}

/// The tables, written (the loads).
pub fn tables_mut() -> RwLockWriteGuard<'static, CivTables> {
    TABLES.write().unwrap_or_else(|e| e.into_inner())
}

// ------------------------------------------------------------------------------------------------ the rules

static RULES: AtomicPtr<EeDataRules> = AtomicPtr::new(std::ptr::null_mut());

/// The game's rules (`EEGetRules()`'s Rust owner): the C++ binds them once, before the database loads.
///
/// # Safety
/// `rules` lives for the rest of the process (or is null) and is written only while no thread reads the civilization
/// tables (the database load).
pub unsafe fn bind_rules(rules: *const EeDataRules) {
    RULES.store(rules as *mut EeDataRules, Ordering::Release);
}

/// The rules: the bound ones, else the defaults (unit tests, a tool without the game).
pub fn rules() -> &'static Rules {
    let p = RULES.load(Ordering::Acquire);
    if p.is_null() {
        static DEFAULT: OnceLock<Rules> = OnceLock::new();
        return DEFAULT.get_or_init(Rules::default);
    }
    // SAFETY: bind_rules' contract
    unsafe { (*p).rules() }
}

/// `EECivBuilderRules::Find`: the first attribute of a level.
pub fn find_attribute(rules: &Rules, level: i32) -> Option<&CivAttribute> {
    rules.civ_builder.attributes.iter().find(|a| a.level == level)
}

/// `EECivPowerRules::Find`: a power's rule by its row (the power id - 0x26).
pub fn find_power_rule(rules: &Rules, row: i32) -> Option<&CivPowerRule> {
    rules.civ_powers.powers.iter().find(|p| p.power == row)
}

// ------------------------------------------------------------------------------------------------ the loads

/// A little-endian reader over a record stream, as `FSMemoryFile::Read` reads: a read past the end copies nothing,
/// stays where it was and is `kErrorEndOfFile`.
pub struct Reader<'a> {
    bytes: &'a [u8],
    at: usize,
}

impl<'a> Reader<'a> {
    pub fn new(bytes: &'a [u8]) -> Self {
        Reader { bytes, at: 0 }
    }

    /// `out.len()` bytes into `out` (left as it was past the end); the read's result.
    pub fn bytes(&mut self, out: &mut [u8]) -> i32 {
        if self.at + out.len() > self.bytes.len() {
            return CIV_END_OF_FILE;
        }
        out.copy_from_slice(&self.bytes[self.at..self.at + out.len()]);
        self.at += out.len();
        CIV_OK
    }

    /// A 4-byte word (`FSFile::Read(unsigned long&)`); the read's error past the end.
    pub fn u32(&mut self) -> Result<u32, i32> {
        let mut b = [0u8; 4];
        match self.bytes(&mut b) {
            CIV_OK => Ok(u32::from_le_bytes(b)),
            e => Err(e),
        }
    }

    /// A record of `T` (plain old data: every bit pattern valid), zeroed first.
    pub fn record<T: Copy + Default>(&mut self, out: &mut T) -> i32 {
        *out = T::default();
        // SAFETY: T is one of this module's repr(C) records of integers, floats and byte arrays (every bit pattern
        // is a value); the slice covers exactly its bytes
        let b = unsafe { std::slice::from_raw_parts_mut(out as *mut T as *mut u8, std::mem::size_of::<T>()) };
        self.bytes(b)
    }
}

impl CivTables {
    /// `AddCategory`: a category record, its 60 levels and an empty id.
    pub fn add_category(&mut self, entry: &EeTechCivCategory) {
        self.categories.push(*entry);
        self.levels.push(entry.mLevels.to_vec());
        self.ids.push(Vec::new());
    }

    /// `mCategoryCount`
    pub fn category_count(&self) -> u32 {
        self.categories.len() as u32
    }

    /// `EECivilizationManager::Load` over the table's bytes (a count, then 600-byte records): the categories with
    /// an index, their texts' ids and levels, then the special powers' category (`powers_cost`: civbuilder.yaml's
    /// powers_cost as the C++ resolves it, dbworld's value by default). The load's result.
    pub fn load(&mut self, bytes: &[u8], rules: &Rules, powers_cost: i32) -> i32 {
        let mut r = Reader::new(bytes);
        let mut last_database_id = 0i32;
        let mut last_index = 0i32;
        let count = match r.u32() {
            Ok(n) => n,
            Err(e) => return e,
        };
        let mut result = CIV_OK;
        let b = &rules.civ_builder;
        for i in 0..count {
            let mut entry = EeTechCivCategory::default();
            result = r.record(&mut entry);
            if entry.mIndex <= 0 {
                continue;
            }
            self.add_category(&entry);
            // the text's id and levels (any number of them: the record holds the first 60)
            if let Some(text) = b.categories.iter().find(|c| c.row == i as i32) {
                *self.ids.last_mut().unwrap() = text.id.clone();
                let levels = self.levels.last_mut().unwrap();
                if text.values.len() > levels.len() {
                    levels.resize(text.values.len(), EeTechCivLevel::default());
                }
                for (k, value) in text.values.iter().enumerate() {
                    levels[k] = EeTechCivLevel { mValue: *value, mBonus: text.bonuses.get(k).copied().unwrap_or(0.0) };
                }
            }
            last_database_id = entry.mDatabaseID;
            last_index = entry.mIndex;
        }
        // the civilization powers' category: one level per attribute that applies a power
        let mut powers = EeTechCivCategory {
            mDatabaseID: last_database_id.wrapping_add(1),
            mIndex: last_index.wrapping_add(1),
            mNameID: b.powers_name_id,
            mUnitSet: b.powers_unit_set,
            mCost: powers_cost,
            ..EeTechCivCategory::default()
        };
        for a in &b.attributes {
            for apply in &a.applies {
                if apply.stat != K_CIV_STAT_POWER
                    || a.level < 1
                    || a.level > 60
                    || apply.power as u32 as usize >= self.powers.len()
                {
                    continue;
                }
                let level = &mut powers.mLevels[(a.level - 1) as usize];
                level.mBonus = self.powers[apply.power as usize].mBonus;
                level.mValue = b.powers_level_value;
                break;
            }
        }
        self.powers_category = self.category_count();
        self.add_category(&powers);
        // powers past level 60
        let bonuses: Vec<(i32, f32)> = b
            .attributes
            .iter()
            .flat_map(|a| a.applies.iter().map(move |p| (a, p)))
            .filter(|(a, p)| {
                p.stat == K_CIV_STAT_POWER && a.level > 60 && (p.power as u32 as usize) < self.powers.len()
            })
            .map(|(a, p)| (a.level, self.powers[p.power as usize].mBonus))
            .collect();
        let levels = self.levels.last_mut().unwrap();
        for (level, bonus) in bonuses {
            if (levels.len() as i64) < level as i64 {
                levels.resize(level as usize, EeTechCivLevel::default());
            }
            levels[(level - 1) as usize] = EeTechCivLevel { mValue: b.powers_level_value, mBonus: bonus };
        }
        if !b.categories.is_empty() {
            *self.ids.last_mut().unwrap() = b.powers_id.clone();
        }
        result
    }

    /// `LoadPremadeCivilizations` over the table's bytes (a count, then 120-byte records): sorted by index, the
    /// distinct era name ids in order.
    pub fn load_premade(&mut self, bytes: &[u8]) -> i32 {
        self.eras.clear();
        self.civilizations.clear();
        let mut r = Reader::new(bytes);
        let mut civs = Vec::new();
        let result = match r.u32() {
            Ok(count) => {
                let mut result = CIV_OK;
                for _ in 0..count {
                    let mut entry = EeTechCivEntry::default();
                    result = r.record(&mut entry);
                    civs.push(entry);
                }
                result
            }
            Err(e) => e,
        };
        if civs.is_empty() {
            return result;
        }
        // std::sort by index (the premade indices are distinct: a stable sort gives its order)
        civs.sort_by_key(|c| c.mIndex);
        for c in civs {
            if !self.eras.contains(&c.mEraNameID) {
                self.eras.push(c.mEraNameID);
            }
            self.civilizations.push(c);
        }
        result
    }

    /// `EECivPowersManager::Load` over the table's bytes (a count, then 152-byte records, appended).
    pub fn load_powers(&mut self, bytes: &[u8]) -> i32 {
        let mut r = Reader::new(bytes);
        match r.u32() {
            Ok(count) => {
                let mut result = CIV_OK;
                for _ in 0..count {
                    let mut power = EeTechCivPower::default();
                    result = r.record(&mut power);
                    self.powers.push(power);
                }
                result
            }
            Err(e) => e,
        }
    }

    // -------------------------------------------------------------------------------------------- queries

    /// `GetCategory`'s record; None past the categories.
    pub fn category(&self, category: u32) -> Option<&EeTechCivCategory> {
        self.categories.get(category as usize)
    }

    /// `GetExtraCost`: the category's cost per pick (0 for an unknown category, where the original read past its
    /// table).
    pub fn extra_cost(&self, upgrade: EeTechCivUpgrade) -> i32 {
        self.category(upgrade >> LEVEL_BITS).map_or(0, |c| c.mCost)
    }

    /// `GetLevelCount`
    pub fn level_count(&self, category: u32) -> u32 {
        self.levels.get(category as usize).map_or(0, |l| l.len() as u32)
    }

    /// `GetCategoryLevel`: a known category's level (1-based; zero past its levels); None for an unknown category.
    pub fn category_level(&self, category: u32, index: u32) -> Option<EeTechCivLevel> {
        self.category(category)?;
        let levels = &self.levels[category as usize];
        Some(if index >= 1 && index as usize <= levels.len() {
            levels[index as usize - 1]
        } else {
            EeTechCivLevel::default()
        })
    }

    /// `LookupLevel`
    pub fn lookup_level(&self, upgrade: EeTechCivUpgrade) -> Option<EeTechCivLevel> {
        self.category_level(upgrade >> LEVEL_BITS, upgrade & LEVEL_MASK)
    }

    /// `GetUpgrades`: every level of every category, in order.
    pub fn upgrades(&self) -> Vec<EeTechCivUpgrade> {
        let mut out = Vec::new();
        for c in 0..self.category_count() {
            for i in 1..=self.level_count(c) {
                out.push(make_upgrade(c, i));
            }
        }
        out
    }

    /// `MakePowerUpgrade`
    pub fn power_upgrade(&self, index: u32) -> EeTechCivUpgrade {
        (index & LEVEL_MASK).wrapping_add(self.powers_category << LEVEL_BITS)
    }

    /// `GetCivilizationsOfType`
    pub fn civilizations_of_type(&self, era: i32) -> Vec<EeTechCivEntry> {
        self.civilizations.iter().filter(|c| c.mEraNameID == era).copied().collect()
    }

    /// `UpgradeKey`: `<category id>.<attribute id>`; None without ids.
    pub fn upgrade_key(&self, rules: &Rules, upgrade: EeTechCivUpgrade) -> Option<Vec<u8>> {
        let id = self.ids.get((upgrade >> LEVEL_BITS) as usize).filter(|id| !id.is_empty())?;
        let attribute = find_attribute(rules, (upgrade & LEVEL_MASK) as i32)?;
        let mut key = id.clone();
        key.push(b'.');
        key.extend_from_slice(&attribute.id);
        Some(key)
    }

    /// `FindUpgradeKey`: the upgrade of a `<category id>.<attribute id>` key.
    pub fn find_upgrade_key(&self, rules: &Rules, key: &[u8]) -> Option<EeTechCivUpgrade> {
        let dot = key.iter().position(|&b| b == b'.')?;
        let attribute = rules.civ_builder.attributes.iter().find(|a| a.id[..] == key[dot + 1..])?;
        let category = self.ids.iter().position(|id| id[..] == key[..dot])?;
        Some(((category as u32) << LEVEL_BITS) | (attribute.level as u32 & LEVEL_MASK))
    }

    /// `MatchesPremadeCivilization`: the premade civilization of a file (its name id, 0 when the file is none), and
    /// whether the checksum (the name's characters' first bytes, the legacy ids, their count) matches it.
    /// `name_characters`: the name's characters' first bytes as the C++ `UWideString::operator[]` reads them.
    pub fn matches_premade(
        &self,
        file: &[u8],
        name_characters: &[c_char],
        upgrades: &[EeTechCivUpgrade],
    ) -> (bool, i32) {
        let mut checksum = upgrades.len() as i32;
        for &c in name_characters {
            checksum = checksum.wrapping_add(c as i32);
        }
        for &u in upgrades {
            checksum = checksum.wrapping_add(to_legacy(u).1 as i32);
        }
        for c in &self.civilizations {
            if c_bytes(&c.mFileName) != file {
                continue;
            }
            return (c.mChecksum == checksum, c.mNameID);
        }
        (false, 0)
    }

    // -------------------------------------------------------------------------------------------- the powers

    /// `GetPower`: the record of a power id (powers start at 0x26); None when its slot is empty or holds another id.
    pub fn power(&self, id: i32) -> Option<&EeTechCivPower> {
        let index = (id as u32).wrapping_sub(POWER_FIRST as u32) as usize;
        self.powers.get(index).filter(|p| p.mID > 0 && p.mID == id)
    }
}

/// The bytes of a NUL-terminated fixed array.
pub fn c_bytes(field: &[c_char]) -> Vec<u8> {
    field.iter().take_while(|&&c| c != 0).map(|&c| c as u8).collect()
}

/// `MakeUpgrade`
pub fn make_upgrade(category: u32, index: u32) -> EeTechCivUpgrade {
    (category << LEVEL_BITS).wrapping_add(index & LEVEL_MASK)
}

/// `FromLegacy`: a (category << 6) | level id.
pub fn from_legacy(legacy: u32) -> EeTechCivUpgrade {
    ((legacy >> 6) << LEVEL_BITS) | (legacy & 0x3f)
}

/// `ToLegacy`: the legacy id and whether it fits.
pub fn to_legacy(upgrade: EeTechCivUpgrade) -> (bool, u32) {
    let category = upgrade >> LEVEL_BITS;
    let level = upgrade & LEVEL_MASK;
    (category < 0x400 && level < 0x40, (category << 6) | (level & 0x3f))
}

/// `LevelNameID`: the attribute's name, else the original's string of the level.
pub fn level_name_id(rules: &Rules, level: u32) -> i32 {
    match find_attribute(rules, level as i32) {
        Some(a) if a.name_id != 0 => a.name_id,
        _ if level <= 0x25 => (level as i32).wrapping_add(IDS_CIV_BONUS_BASE),
        _ => (level as i32).wrapping_add(IDS_CIV_POWER_BASE),
    }
}

/// `IsPowerLevel`: the level's attribute applies a power (without an attribute: the original's levels 0x26..).
pub fn is_power_level(rules: &Rules, level: u32) -> bool {
    match find_attribute(rules, level as i32) {
        None => level >= 0x26,
        Some(a) => a.applies.iter().any(|p| p.stat == K_CIV_STAT_POWER),
    }
}

/// `PowerLevel`: the level of the attribute that applies a power id (the id itself without one).
pub fn power_level(rules: &Rules, power_id: u32) -> u32 {
    for a in &rules.civ_builder.attributes {
        for p in &a.applies {
            if p.stat == K_CIV_STAT_POWER && p.power.wrapping_add(POWER_FIRST) as u32 == power_id {
                return a.level as u32;
            }
        }
    }
    power_id
}

/// The power ids a level's attribute applies (`HasPower`'s test, `ApplyUpgradeLevel`'s first power).
pub fn level_powers(rules: &Rules, level: u32) -> impl Iterator<Item = i32> + '_ {
    find_attribute(rules, level as i32)
        .into_iter()
        .flat_map(|a| a.applies.iter())
        .filter(|p| p.stat == K_CIV_STAT_POWER)
        .map(|p| p.power.wrapping_add(POWER_FIRST))
}

/// `HasPower`: a purchased upgrade's attribute applies the power.
pub fn has_power(rules: &Rules, purchased: &[EeTechCivUpgrade], id: i32) -> bool {
    purchased.iter().any(|&u| level_powers(rules, u & LEVEL_MASK).any(|p| p == id))
}

// ------------------------------------------------------------------------------------------------ the C ABI

/// Binds the game's rules ([`bind_rules`]).
///
/// # Safety
/// As [`bind_rules`].
#[no_mangle]
pub unsafe extern "C" fn ee_tech_civ_bind_rules(rules: *const EeDataRules) {
    // SAFETY: the caller's contract
    unsafe { bind_rules(rules) }
}

/// The bytes at `data` (none for a null pointer).
///
/// # Safety
/// `data` names `size` readable bytes or is null.
unsafe fn slice<'a, T>(data: *const T, size: usize) -> &'a [T] {
    if data.is_null() || size == 0 {
        &[]
    } else {
        // SAFETY: the caller's contract
        unsafe { std::slice::from_raw_parts(data, size) }
    }
}

/// `EECivilizationManager::Load` over the table's bytes ([`CivTables::load`]).
///
/// # Safety
/// `data` names `size` readable bytes or is null.
#[no_mangle]
pub unsafe extern "C" fn ee_tech_civ_load(data: *const u8, size: usize, powers_cost: i32) -> i32 {
    // SAFETY: the caller's contract
    let bytes = unsafe { slice(data, size) };
    tables_mut().load(bytes, rules(), powers_cost)
}

/// `LoadPremadeCivilizations` over the table's bytes.
///
/// # Safety
/// As [`ee_tech_civ_load`].
#[no_mangle]
pub unsafe extern "C" fn ee_tech_civ_load_premade(data: *const u8, size: usize) -> i32 {
    // SAFETY: the caller's contract
    let bytes = unsafe { slice(data, size) };
    tables_mut().load_premade(bytes)
}

/// `EECivPowersManager::Load` over the table's bytes.
///
/// # Safety
/// As [`ee_tech_civ_load`].
#[no_mangle]
pub unsafe extern "C" fn ee_tech_civ_load_powers(data: *const u8, size: usize) -> i32 {
    // SAFETY: the caller's contract
    let bytes = unsafe { slice(data, size) };
    tables_mut().load_powers(bytes)
}

/// `mCategoryCount`
#[no_mangle]
pub extern "C" fn ee_tech_civ_category_count() -> u32 {
    tables().category_count()
}

/// `GetCategory`: the record in place, null past the categories (written only by the loads).
#[no_mangle]
pub extern "C" fn ee_tech_civ_category(category: u32) -> *mut EeTechCivCategory {
    tables().category(category).map_or(std::ptr::null_mut(), |c| c as *const _ as *mut _)
}

/// A category's id (dbcivilization's row id through civbuilder.yaml; empty without one or past the categories)
/// through `out(ctx, bytes, length)`.
///
/// # Safety
/// `out` is callable with `ctx`.
#[no_mangle]
pub unsafe extern "C" fn ee_tech_civ_category_id(
    category: u32,
    ctx: *mut std::ffi::c_void,
    out: unsafe extern "C" fn(ctx: *mut std::ffi::c_void, bytes: *const u8, length: usize),
) {
    let id = tables().ids.get(category as usize).cloned().unwrap_or_default();
    // SAFETY: the caller's contract
    unsafe { out(ctx, id.as_ptr(), id.len()) };
}

/// `GetExtraCost`
#[no_mangle]
pub extern "C" fn ee_tech_civ_extra_cost(upgrade: EeTechCivUpgrade) -> i32 {
    tables().extra_cost(upgrade)
}

/// `CountInCategory`: the upgrades of `list` in `upgrade`'s category.
///
/// # Safety
/// `list` names `count` upgrades or is null.
#[no_mangle]
pub unsafe extern "C" fn ee_tech_civ_count_in_category(
    upgrade: EeTechCivUpgrade,
    list: *const EeTechCivUpgrade,
    count: usize,
) -> i32 {
    // SAFETY: the caller's contract
    let list = unsafe { slice(list, count) };
    list.iter().filter(|&&u| u >> LEVEL_BITS == upgrade >> LEVEL_BITS).count() as i32
}

/// `GetCategoryLevel`: false for an unknown category; the outputs may be null.
///
/// # Safety
/// `value` and `bonus` are writable or null.
#[no_mangle]
pub unsafe extern "C" fn ee_tech_civ_category_level(
    category: u32,
    index: u32,
    value: *mut f32,
    bonus: *mut f32,
) -> bool {
    let Some(level) = tables().category_level(category, index) else {
        return false;
    };
    // SAFETY: the caller's contract
    unsafe {
        if !value.is_null() {
            *value = level.mValue;
        }
        if !bonus.is_null() {
            *bonus = level.mBonus;
        }
    }
    true
}

/// `GetLevelCount`
#[no_mangle]
pub extern "C" fn ee_tech_civ_level_count(category: u32) -> u32 {
    tables().level_count(category)
}

/// `GetUpgrades`: every upgrade, through `push(ctx, upgrade)` in order.
///
/// # Safety
/// `push` is callable with `ctx`; it must not call back into the civilization tables' loads.
#[no_mangle]
pub unsafe extern "C" fn ee_tech_civ_upgrades(
    ctx: *mut std::ffi::c_void,
    push: unsafe extern "C" fn(ctx: *mut std::ffi::c_void, upgrade: EeTechCivUpgrade),
) {
    let upgrades = tables().upgrades();
    for u in upgrades {
        // SAFETY: the caller's contract
        unsafe { push(ctx, u) }
    }
}

/// `MakePowerUpgrade`
#[no_mangle]
pub extern "C" fn ee_tech_civ_power_upgrade(index: u32) -> EeTechCivUpgrade {
    tables().power_upgrade(index)
}

/// `LevelNameID`
#[no_mangle]
pub extern "C" fn ee_tech_civ_level_name_id(level: u32) -> i32 {
    level_name_id(rules(), level)
}

/// `IsPowerLevel`
#[no_mangle]
pub extern "C" fn ee_tech_civ_is_power_level(level: u32) -> bool {
    is_power_level(rules(), level)
}

/// `PowerLevel`
#[no_mangle]
pub extern "C" fn ee_tech_civ_power_level(power_id: u32) -> u32 {
    power_level(rules(), power_id)
}

/// `FromLegacy`
#[no_mangle]
pub extern "C" fn ee_tech_civ_from_legacy(legacy: u32) -> EeTechCivUpgrade {
    from_legacy(legacy)
}

/// `ToLegacy`
///
/// # Safety
/// `legacy` is writable.
#[no_mangle]
pub unsafe extern "C" fn ee_tech_civ_to_legacy(upgrade: EeTechCivUpgrade, legacy: *mut u32) -> bool {
    let (fits, value) = to_legacy(upgrade);
    // SAFETY: the caller's contract
    unsafe { *legacy = value };
    fits
}

/// `UpgradeKey`: the key's bytes through `out(ctx, bytes, length)`; false without one.
///
/// # Safety
/// `out` is callable with `ctx`.
#[no_mangle]
pub unsafe extern "C" fn ee_tech_civ_upgrade_key(
    upgrade: EeTechCivUpgrade,
    ctx: *mut std::ffi::c_void,
    out: unsafe extern "C" fn(ctx: *mut std::ffi::c_void, bytes: *const u8, length: usize),
) -> bool {
    let key = tables().upgrade_key(rules(), upgrade);
    match key {
        Some(key) => {
            // SAFETY: the caller's contract
            unsafe { out(ctx, key.as_ptr(), key.len()) };
            true
        }
        None => false,
    }
}

/// `FindUpgradeKey`
///
/// # Safety
/// `key` names `length` readable bytes or is null; `upgrade` is writable.
#[no_mangle]
pub unsafe extern "C" fn ee_tech_civ_find_upgrade_key(
    key: *const u8,
    length: usize,
    upgrade: *mut EeTechCivUpgrade,
) -> bool {
    // SAFETY: the caller's contract
    let key = unsafe { slice(key, length) };
    match tables().find_upgrade_key(rules(), key) {
        Some(u) => {
            // SAFETY: the caller's contract
            unsafe { *upgrade = u };
            true
        }
        None => false,
    }
}

/// The first bytes of a C string's first `characters` characters (`UWideString::operator[]` over `GetLength()`).
///
/// # Safety
/// `text` is a NUL-terminated string or null.
pub unsafe fn character_bytes(text: *const c_char, characters: u32) -> Vec<c_char> {
    if text.is_null() {
        return Vec::new();
    }
    (0..characters)
        // SAFETY: the caller's contract; PlatMbsNInc stops at the terminator
        .map(|i| unsafe { *ee_plat_core::string::PlatMbsNInc(text as *const u8, i as usize) } as c_char)
        .collect()
}

/// `MatchesPremadeCivilization` ([`CivTables::matches_premade`]): `file` the file's name (C string), `name` the
/// civilization's name (C string, `characters` its `GetLength()`).
///
/// # Safety
/// The strings are NUL-terminated or null; `upgrades` names `count` upgrades or is null; `name_id` is writable.
#[no_mangle]
pub unsafe extern "C" fn ee_tech_civ_matches_premade(
    file: *const c_char,
    name: *const c_char,
    characters: u32,
    upgrades: *const EeTechCivUpgrade,
    count: usize,
    name_id: *mut i32,
) -> bool {
    // SAFETY: the caller's contract
    let (file, characters, upgrades) = unsafe {
        let file = if file.is_null() { &[][..] } else { std::ffi::CStr::from_ptr(file).to_bytes() };
        (file, character_bytes(name, characters), slice(upgrades, count))
    };
    let (matches, id) = tables().matches_premade(file, &characters, upgrades);
    // SAFETY: the caller's contract
    unsafe { *name_id = id };
    matches
}

/// The number of premade civilizations (`GetNumCivilizations`).
#[no_mangle]
pub extern "C" fn ee_tech_civ_civilization_count() -> u32 {
    tables().civilizations.len() as u32
}

/// A premade civilization in place (sorted by index); null past them.
#[no_mangle]
pub extern "C" fn ee_tech_civ_civilization(index: u32) -> *const EeTechCivEntry {
    tables().civilizations.get(index as usize).map_or(std::ptr::null(), |c| c as *const _)
}

/// The number of eras (`mEraNameIDs`).
#[no_mangle]
pub extern "C" fn ee_tech_civ_era_count() -> u32 {
    tables().eras.len() as u32
}

/// An era's name id (0 past them).
#[no_mangle]
pub extern "C" fn ee_tech_civ_era(index: u32) -> i32 {
    tables().eras.get(index as usize).copied().unwrap_or(0)
}

/// The number of power records (`mPowers.size()`).
#[no_mangle]
pub extern "C" fn ee_tech_civ_power_count() -> u32 {
    tables().powers.len() as u32
}

/// `GetPower`: a copy of the record (zero when none) and whether it is the id's.
///
/// # Safety
/// `out` is writable.
#[no_mangle]
pub unsafe extern "C" fn ee_tech_civ_power(id: i32, out: *mut EeTechCivPower) -> bool {
    let t = tables();
    let found = t.power(id);
    // SAFETY: the caller's contract
    unsafe { *out = found.copied().unwrap_or_default() };
    found.is_some()
}

/// `GetPowerRecord`: the record of a power id in place (the original indexed without a check; null past the
/// records).
#[no_mangle]
pub extern "C" fn ee_tech_civ_power_record(id: i32) -> *mut EeTechCivPower {
    let t = tables();
    let index = (id as u32).wrapping_sub(POWER_FIRST as u32) as usize;
    t.powers.get(index).map_or(std::ptr::null_mut(), |p| p as *const _ as *mut _)
}

/// `HasPower` over the player's purchased upgrades.
///
/// # Safety
/// `purchased` names `count` upgrades or is null.
#[no_mangle]
pub unsafe extern "C" fn ee_tech_civ_has_power(purchased: *const EeTechCivUpgrade, count: usize, id: i32) -> bool {
    // SAFETY: the caller's contract
    has_power(rules(), unsafe { slice(purchased, count) }, id)
}
