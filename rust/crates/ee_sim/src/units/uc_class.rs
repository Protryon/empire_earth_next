//! `EEUnitClass`'s constructor and `CreateUnitClasses` over their C++ statements (the units' lane, round 8; were
//! EEUnitClass.cpp): the constructor's media, effects (each referenced as found), related classes and AI script
//! around unit_class_fields.rs's fill of the plain fields (a statement: its base pointer is the C++'s cast); the classes built from dbobjects by category, their related
//! classes linked and their unit-set flags set. Each C++ statement is one `EE_UNITS_UC_*` case of
//! `ee_units_uc_host` (EEUnitClass.cpp), in the original's order. The constructor's statements take the class
//! record as the host's unit.

// `c_long` is 32 bits on Windows and 64 elsewhere: the casts are needed there
#![allow(clippy::unnecessary_cast)]

use crate::units::kinds::EE_UNIT_CLASS_KINDS;
use crate::units::uc_body::*;
use crate::units::uc_create::*;
use crate::units::uc_media::*;
use crate::units::unit_class_fields::EeUnitsUnitClassFillIn;
use std::os::raw::c_long;

// ---- the statements (600..699)
/// `class->mPhysicsModel = ` model `a`
pub const EE_UNITS_UC_SET_PHYSICS_MODEL: i32 = 600;
/// the record's `mNumAttributes`
pub const EE_UNITS_UC_RECORD_ATTRIBUTES: i32 = 601;
/// `class->mResearchTechnologies.resize(b)`
pub const EE_UNITS_UC_RESEARCH_RESIZE: i32 = 602;
/// `class->mResearchTechnologies[b] = record->mAttributes[b]`
pub const EE_UNITS_UC_RESEARCH_SET: i32 = 603;
/// `class->mButton = EEButtonObject::Get(record->mButton)`
pub const EE_UNITS_UC_SET_BUTTON: i32 = 604;
/// `class->mModel = (EEModel*)EEMedia::Get(record->mGraphic)`
pub const EE_UNITS_UC_SET_MODEL: i32 = 605;
/// `class->mCreationSound = EESoundObject::Get(record->mCreationSound)`
pub const EE_UNITS_UC_SET_CREATION_SOUND: i32 = 606;
/// `class->mAcknowledgeSounds[b] = EESoundObject::Get(record's acknowledge sound b + 1)`
pub const EE_UNITS_UC_SET_ACKNOWLEDGE_SOUND: i32 = 607;
/// `class->mSelectionSounds[b] = EESoundObject::Get(record's selection sound b + 1)`
pub const EE_UNITS_UC_SET_SELECTION_SOUND: i32 = 608;
/// `class->mDeathSound = EESoundObject::Get(record->mDeathSound)`
pub const EE_UNITS_UC_SET_DEATH_SOUND: i32 = 609;
/// `record->mLineOfSight > kMaxLineOfSight`
pub const EE_UNITS_UC_LOS_TOO_FAR: i32 = 610;
/// `ESDebugOut(` the line of sight's warning `)`
pub const EE_UNITS_UC_LOS_WARNING: i32 = 611;
/// `class->mAIScriptName.Copy(record->mAIScriptName, ...)`
pub const EE_UNITS_UC_COPY_SCRIPT_NAME: i32 = 612;
/// `class->mName.Copy(record->mName, ...)`
pub const EE_UNITS_UC_COPY_NAME: i32 = 613;
/// `EEAnimal::Find(record->mUnitType)`
pub const EE_UNITS_UC_FIND_ANIMAL: i32 = 614;
/// animal `a`'s `mAnimalID`
pub const EE_UNITS_UC_ANIMAL_ID: i32 = 615;
/// `gTechTree.Get(record->mTechnology)`
pub const EE_UNITS_UC_RECORD_TECH_NODE: i32 = 616;
/// node `a`'s `mEpoch`
pub const EE_UNITS_UC_NODE_EPOCH: i32 = 617;
/// `class->GetNumResearchTechnologies()`
pub const EE_UNITS_UC_NUM_RESEARCH: i32 = 618;
/// `gTechTree.Get(class->mResearchTechnologies[b])`
pub const EE_UNITS_UC_RESEARCH_TECH_NODE: i32 = 619;
/// `class->mImprovements = ` the record's upgrade table
pub const EE_UNITS_UC_SET_IMPROVEMENTS: i32 = 620;
/// `EEGraphicEffect::Get(` the record's effect `b`)` (the constructor's order)
pub const EE_UNITS_UC_RECORD_EFFECT: i32 = 621;
/// effect `a`'s `Reference()`
pub const EE_UNITS_UC_EFFECT_REFERENCE: i32 = 622;
/// the class's effect `b` (the constructor's order) `=` effect `a`
pub const EE_UNITS_UC_SET_EFFECT: i32 = 623;
/// `class->mAIScriptName.GetLength()`
pub const EE_UNITS_UC_SCRIPT_NAME_LENGTH: i32 = 624;
/// `class->mAIScript = EEUnitAction::GetScript(class->mAIScriptName)`
pub const EE_UNITS_UC_SET_AI_SCRIPT: i32 = 625;
/// `class->mAIScript`
pub const EE_UNITS_UC_AI_SCRIPT: i32 = 626;
/// `ee_units_unit_class_fill(static_cast<EeUnitsUnitClassFields*>(class), record, the fill input at a)`
pub const EE_UNITS_UC_CLASS_FILL: i32 = 627;
/// `class->mBehavior = EEUnitBehavior::sTable[record->mBehavior]`
pub const EE_UNITS_UC_SET_CLASS_BEHAVIOR: i32 = 628;
/// `EEDbFindFile(gDataPath, kDbObjectsFile, the file = NULL)`
pub const EE_UNITS_UC_DB_FIND: i32 = 640;
/// `ESVERIFY_AT(b != 0, c)`
pub const EE_UNITS_UC_VERIFY: i32 = 641;
/// the file `!= NULL`
pub const EE_UNITS_UC_DB_FILE: i32 = 642;
/// `gDatabaseObjects.Load(the file)`
pub const EE_UNITS_UC_DB_LOAD: i32 = 643;
/// `delete the file`
pub const EE_UNITS_UC_DB_DELETE: i32 = 644;
/// `gDatabaseObjects.GetCount()`
pub const EE_UNITS_UC_DB_COUNT: i32 = 645;
/// `gDatabaseObjects.GetEntry(b)->mRecord`
pub const EE_UNITS_UC_DB_RECORD: i32 = 646;
/// record `a`'s `mIsEnabled`
pub const EE_UNITS_UC_RECORD_ENABLED: i32 = 647;
/// record `a`'s `mUnitType`
pub const EE_UNITS_UC_RECORD_TYPE: i32 = 648;
/// record `a`'s `mCategory`
pub const EE_UNITS_UC_RECORD_CATEGORY: i32 = 649;
/// `gUnitClassTable.assign(b, NULL)`
pub const EE_UNITS_UC_TABLE_ASSIGN: i32 = 650;
/// `new EEUnitClass(b >= 0, record a, b >= 0 ? gPhysicsModels[b] : NULL)`
pub const EE_UNITS_UC_NEW_CLASS: i32 = 651;
/// `EEUnitClass::RegisterUnitClass(class a)`
pub const EE_UNITS_UC_REGISTER_CLASS: i32 = 652;
/// record `a`'s related type `b` (projectile, scaffolding, death, death spawn, packed, remains)
pub const EE_UNITS_UC_RECORD_RELATED: i32 = 653;
/// class `a`'s related class `b` `= gUnitClassTable[c]`
pub const EE_UNITS_UC_SET_RELATED: i32 = 654;
/// class `a`'s related class `b`'s `Reference()`
pub const EE_UNITS_UC_REFERENCE_RELATED: i32 = 655;
/// class `a`'s `mIsWall = gUnitSetManager.Contains(a->mUnitType, 9)`
pub const EE_UNITS_UC_SET_IS_WALL: i32 = 656;
/// class `a`'s `mIsCyber` (set 0x38)
pub const EE_UNITS_UC_SET_IS_CYBER: i32 = 657;
/// class `a`'s `mIsHorse` (set 0x6e)
pub const EE_UNITS_UC_SET_IS_HORSE: i32 = 658;
/// class `a`'s `mIsHuman` (set 0x71)
pub const EE_UNITS_UC_SET_IS_HUMAN: i32 = 659;
/// class `a`'s `mIsMedic` (set 0x81)
pub const EE_UNITS_UC_SET_IS_MEDIC: i32 = 660;
/// class `a`'s `mIsFlare` (set 0x8a)
pub const EE_UNITS_UC_SET_IS_FLARE: i32 = 661;
/// class `a`'s `mIsBoobyTrap` (set 0x97)
pub const EE_UNITS_UC_SET_IS_BOOBY_TRAP: i32 = 662;
/// class `a`'s `mIsParatrooperBomber` (set 0x96)
pub const EE_UNITS_UC_SET_IS_PARATROOPER_BOMBER: i32 = 663;
/// class `a`'s `mSkipsLineOfFireCheck` (set 0x86)
pub const EE_UNITS_UC_SET_SKIPS_LINE_OF_FIRE: i32 = 664;
/// class `a`'s `mIsLandBuildingClass = a->IsLandBuilding()`
pub const EE_UNITS_UC_SET_LAND_BUILDING_CLASS: i32 = 665;
/// `gDatabaseObjects.Clear()`
pub const EE_UNITS_UC_DB_CLEAR: i32 = 666;
/// `std::sort(EEUnitClass::sUnitTypes, EEUnitClassNameLess())`
pub const EE_UNITS_UC_SORT_TYPES: i32 = 667;

// ---- the bodies
pub const EE_UNITS_UC_DO_CONSTRUCT: i32 = 100;
pub const EE_UNITS_UC_DO_CREATE_UNIT_CLASSES: i32 = 101;

/// The class's effects (the constructor's order)
const EFFECTS: i64 = 29;
/// The related classes (projectile, scaffolding, death, death spawn, packed, remains)
const RELATED: i64 = 6;
/// The unit-set flags' statements, in the original's order
const SET_FLAGS: [i32; 9] = [
    EE_UNITS_UC_SET_IS_WALL,
    EE_UNITS_UC_SET_IS_CYBER,
    EE_UNITS_UC_SET_IS_HORSE,
    EE_UNITS_UC_SET_IS_HUMAN,
    EE_UNITS_UC_SET_IS_MEDIC,
    EE_UNITS_UC_SET_IS_FLARE,
    EE_UNITS_UC_SET_IS_BOOBY_TRAP,
    EE_UNITS_UC_SET_IS_PARATROOPER_BOMBER,
    EE_UNITS_UC_SET_SKIPS_LINE_OF_FIRE,
];
const CATEGORIES: u64 = 0x23;
/// `keseOK`
const OK: i64 = 0;

/// Each category's physics model (an index into gPhysicsModels; -1: an ambient or resource class, which is not
/// complex and has none): the EEUC* constructors' arguments (the land 0, airplane 1, helicopter 2, naval 4,
/// submerged naval 5, projectile 6, missile 7, building 8, sea animal 9, air animal 10 and space 11 models).
const MODEL_BY_CATEGORY: [i8; 0x23] = [
    -1, -1, 8, 1, 4, 0, 0, 2, 0, 6, 5, 6, 7, 0, 4, 0, // ambient .. land 15
    0, 2, 4, 8, 7, 4, 4, 0, 0, 8, 2, 9, 10, 8, 8, 11, // land transport .. spaceship
    11, 11, 1, // space building, space transport, planet spaceship
];

/// `EEUnitClass::EEUnitClass(is complex, record, physics model)`: the class's media, effects, related classes and
/// AI script around the plain fields' fill, in the original's order.
pub fn construct<H: UcHost + ?Sized>(h: &mut H, class: u64, record: u64, is_complex: c_long, model: u64) {
    let mut s = S { h, class, unit: record };
    s.a(EE_UNITS_UC_SET_PHYSICS_MODEL, model);
    let n = s.s(EE_UNITS_UC_RECORD_ATTRIBUTES) as u32;
    s.b(EE_UNITS_UC_RESEARCH_RESIZE, i64::from(n));
    for i in 0..n {
        s.b(EE_UNITS_UC_RESEARCH_SET, i64::from(i));
    }
    s.b(EE_UNITS_UC_SET_MEDIA_LOADED, 0);
    s.s(EE_UNITS_UC_SET_BUTTON);
    s.s(EE_UNITS_UC_SET_MODEL);
    s.s(EE_UNITS_UC_SET_CREATION_SOUND);
    s.b(EE_UNITS_UC_SET_ACKNOWLEDGE_SOUND, 0);
    s.b(EE_UNITS_UC_SET_ACKNOWLEDGE_SOUND, 1);
    s.b(EE_UNITS_UC_SET_SELECTION_SOUND, 0);
    s.b(EE_UNITS_UC_SET_SELECTION_SOUND, 1);
    s.s(EE_UNITS_UC_SET_DEATH_SOUND);
    if s.s(EE_UNITS_UC_LOS_TOO_FAR) != 0 {
        s.s(EE_UNITS_UC_LOS_WARNING);
    }
    s.s(EE_UNITS_UC_COPY_SCRIPT_NAME);
    s.s(EE_UNITS_UC_COPY_NAME);
    let mut input = EeUnitsUnitClassFillIn { is_complex, ..Default::default() };
    let animal = s.s(EE_UNITS_UC_FIND_ANIMAL) as u64;
    input.has_animal = animal != 0;
    input.animal_id = if animal != 0 { s.a(EE_UNITS_UC_ANIMAL_ID, animal) as c_long } else { 0 };
    let node = s.s(EE_UNITS_UC_RECORD_TECH_NODE) as u64;
    input.has_tech_node = node != 0;
    input.tech_epoch = if node != 0 { s.a(EE_UNITS_UC_NODE_EPOCH, node) as c_long } else { 0 };
    let mut j: u32 = 0;
    while j < s.s(EE_UNITS_UC_NUM_RESEARCH) as u32 && !input.has_calamity_powers {
        let technology = s.b(EE_UNITS_UC_RESEARCH_TECH_NODE, i64::from(j)) as u64;
        input.has_calamity_powers = technology != 0 && s.a(EE_UNITS_UC_NODE_KIND, technology) as c_long == 5;
        j += 1;
    }
    s.s(EE_UNITS_UC_SET_IMPROVEMENTS);
    // each effect referenced as it is found
    for i in 0..EFFECTS {
        let effect = s.b(EE_UNITS_UC_RECORD_EFFECT, i) as u64;
        if effect != 0 {
            s.a(EE_UNITS_UC_EFFECT_REFERENCE, effect);
        }
        s.ab(EE_UNITS_UC_SET_EFFECT, effect, i);
    }
    for i in 0..RELATED {
        s.b(EE_UNITS_UC_CLEAR_RELATED, i);
    }
    if s.s(EE_UNITS_UC_SCRIPT_NAME_LENGTH) as c_long > 0 {
        s.s(EE_UNITS_UC_SET_AI_SCRIPT);
    } else {
        s.s(EE_UNITS_UC_CLEAR_AI_SCRIPT);
    }
    input.ai_script = s.s(EE_UNITS_UC_AI_SCRIPT) as u64;
    s.a(EE_UNITS_UC_CLASS_FILL, addr(&mut input));
    s.s(EE_UNITS_UC_SET_CLASS_BEHAVIOR);
}

/// `EEUnitClass::CreateUnitClasses()`: every unit class built from dbobjects by category (one class-table slot per
/// unit type), the related classes linked, the unit-set flags set, the unit types sorted by name.
pub fn create_unit_classes<H: UcHost + ?Sized>(h: &mut H) {
    let mut s = S { h, class: 0, unit: 0 };
    let result = s.s(EE_UNITS_UC_DB_FIND);
    s.bc(EE_UNITS_UC_VERIFY, i64::from(result == OK), 5710);
    let file = s.s(EE_UNITS_UC_DB_FILE);
    s.bc(EE_UNITS_UC_VERIFY, i64::from(file != 0), 5711);
    let result = s.s(EE_UNITS_UC_DB_LOAD);
    s.bc(EE_UNITS_UC_VERIFY, i64::from(result == OK), 5713);
    if s.s(EE_UNITS_UC_DB_FILE) != 0 {
        s.s(EE_UNITS_UC_DB_DELETE);
    }
    // one class slot per unit type: as many as dbobjects has records, or the highest unit type + 1
    let mut slots = s.s(EE_UNITS_UC_DB_COUNT) as u32;
    let mut r: u32 = 0;
    while r < s.s(EE_UNITS_UC_DB_COUNT) as u32 {
        let record = s.b(EE_UNITS_UC_DB_RECORD, i64::from(r)) as u64;
        let kind = s.a(EE_UNITS_UC_RECORD_TYPE, record) as c_long;
        if s.a(EE_UNITS_UC_RECORD_ENABLED, record) != 0 && kind >= 0 && kind as u32 >= slots {
            slots = (kind as u32).wrapping_add(1);
        }
        r += 1;
    }
    s.b(EE_UNITS_UC_TABLE_ASSIGN, i64::from(slots));
    let count = s.s(EE_UNITS_UC_DB_COUNT) as u32;
    for i in 0..count {
        let record = s.b(EE_UNITS_UC_DB_RECORD, i64::from(i)) as u64;
        if s.a(EE_UNITS_UC_RECORD_ENABLED, record) == 0 || s.a(EE_UNITS_UC_RECORD_TYPE, record) as c_long == -1 {
            continue;
        }
        // one EEUnitClass per record, of its category's kind, with that kind's physics model
        let category = s.a(EE_UNITS_UC_RECORD_CATEGORY, record) as u32 as u64;
        let mut new_class = 0u64;
        if category < CATEGORIES && EE_UNIT_CLASS_KINDS[category as usize] != 0 {
            let model = MODEL_BY_CATEGORY[category as usize];
            new_class = s.ab(EE_UNITS_UC_NEW_CLASS, record, i64::from(model)) as u64;
        }
        s.a(EE_UNITS_UC_REGISTER_CLASS, new_class);
        if new_class != 0 {
            s.a(EE_UNITS_UC_CLASS_DEREFERENCE, new_class);
        }
    }
    // each class's related classes, from its own record
    for i in 0..count {
        let record = s.b(EE_UNITS_UC_DB_RECORD, i64::from(i)) as u64;
        if s.a(EE_UNITS_UC_RECORD_ENABLED, record) == 0 || (s.a(EE_UNITS_UC_RECORD_TYPE, record) as c_long) < 0 {
            continue;
        }
        let kind = s.a(EE_UNITS_UC_RECORD_TYPE, record);
        let c = s.b(EE_UNITS_UC_CLASS_AT, kind) as u64;
        if c == 0 {
            continue;
        }
        for k in 0..RELATED {
            let related = s.ab(EE_UNITS_UC_RECORD_RELATED, record, k) as c_long;
            if related > 0 {
                s.abc(EE_UNITS_UC_SET_RELATED, c, k, i64::from(related));
                s.ab(EE_UNITS_UC_REFERENCE_RELATED, c, k);
            }
        }
        for op in SET_FLAGS {
            s.a(op, c);
        }
        s.a(EE_UNITS_UC_SET_LAND_BUILDING_CLASS, c);
    }
    s.s(EE_UNITS_UC_DB_CLEAR);
    s.s(EE_UNITS_UC_SORT_TYPES);
}

impl<H: UcHost + ?Sized> S<'_, H> {
    fn bc(&mut self, op: i32, b: i64, c: i64) -> i64 {
        self.h.uc(op, self.class, self.unit, 0, b, c, &mut [0.0; 4])
    }
}

#[cfg(not(test))]
mod game {
    use super::*;
    use crate::units::uc_body::game::ee_units_uc_host;
    use std::os::raw::c_void;

    struct GameK;

    impl UcHost for GameK {
        #[allow(clippy::too_many_arguments)]
        fn uc(&mut self, op: i32, class: u64, unit: u64, a: u64, b: i64, c: i64, f: &mut [f32; 4]) -> i64 {
            if let Some(v) = crate::units::tech_cases::uc(op, unit, a) {
                return v;
            }
            if let Some(v) =
                crate::units::uc_view::uc_view(crate::units::uc_view::Kind::Class, op, class, unit, a, b, c, f)
            {
                return v;
            }
            // SAFETY: the body runs inside the C++ member that handed over its live class (and record)
            unsafe { ee_units_uc_host(op, class as usize as *mut c_void, unit as usize as *mut c_void, a, b, c, f) }
        }
    }

    /// The class's constructor body (`EE_UNITS_UC_DO_CONSTRUCT`: `record` the `EEUnitClassRecord`, `b` is-complex,
    /// `model` the physics model) or `CreateUnitClasses` (`EE_UNITS_UC_DO_CREATE_UNIT_CLASSES`).
    ///
    /// # Safety
    /// `class` is the `EEUnitClass` under construction (or NULL), `record` its live record, on the main thread.
    #[no_mangle]
    pub unsafe extern "C" fn ee_units_uc_class(
        what: i32,
        class: *mut c_void,
        record: *mut c_void,
        b: i64,
        model: *mut c_void,
    ) {
        let (class, record) = (class as usize as u64, record as usize as u64);
        match what {
            EE_UNITS_UC_DO_CONSTRUCT => construct(&mut GameK, class, record, b as c_long, model as usize as u64),
            EE_UNITS_UC_DO_CREATE_UNIT_CLASSES => create_unit_classes(&mut GameK),
            _ => {}
        }
    }
}

#[cfg(test)]
#[path = "uc_class_tests.rs"]
mod tests;
