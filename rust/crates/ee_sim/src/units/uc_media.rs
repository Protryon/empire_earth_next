//! `EEUnitClass`'s media and lifetime bodies over their C++ statements (the units' lane, round 7; were
//! EEUnitClass.cpp): `LoadClassMedia`/`UnloadClassMedia`, `LoadButtons`/`UnloadButtons`,
//! `BaseLoadMedia`/`BaseUnloadMedia` and the destructor's releases. As uc_create.rs: each C++ statement one
//! `EE_UNITS_UC_*` case of `ee_units_uc_host`, in the original's order. A related class (projectile, scaffolding,
//! death, death spawn, packed, remains) is named by its index in the host's member table.

use crate::units::uc_body::*;
use crate::units::uc_create::*;

// ---- the statements (from 520)
/// `class->mAcknowledgeSounds[b]`
pub const EE_UNITS_UC_ACKNOWLEDGE_SOUND: i32 = 520;
/// `class->mSelectionSounds[b]`
pub const EE_UNITS_UC_SELECTION_SOUND: i32 = 521;
/// `class->mDeathSound`
pub const EE_UNITS_UC_CLASS_DEATH_SOUND: i32 = 522;
/// `class->mCreationSound`
pub const EE_UNITS_UC_CLASS_CREATION_SOUND: i32 = 523;
/// `class->mModel`
pub const EE_UNITS_UC_CLASS_MODEL: i32 = 524;
/// `class->mButton`
pub const EE_UNITS_UC_CLASS_BUTTON: i32 = 525;
/// sound `a`'s `LoadMedia()`
pub const EE_UNITS_UC_SOUND_LOAD: i32 = 526;
/// sound `a`'s `UnloadMedia()`
pub const EE_UNITS_UC_SOUND_UNLOAD: i32 = 527;
/// model `a`'s `LoadMedia()`
pub const EE_UNITS_UC_MODEL_LOAD: i32 = 528;
/// model `a`'s `UnloadMedia()`
pub const EE_UNITS_UC_MODEL_UNLOAD: i32 = 529;
/// button `a`'s `Acquire()`
pub const EE_UNITS_UC_BUTTON_ACQUIRE: i32 = 530;
/// button `a`'s `Release()`
pub const EE_UNITS_UC_BUTTON_RELEASE: i32 = 531;
/// player `a`'s `mTechnologies`
pub const EE_UNITS_UC_TECH_TREE: i32 = 532;
/// `class->mResearchTechnologies.size()`
pub const EE_UNITS_UC_RESEARCH_COUNT: i32 = 533;
/// tech tree `a`'s `Get(class->mResearchTechnologies[b])`
pub const EE_UNITS_UC_RESEARCH_NODE: i32 = 534;
/// player `a`'s `IsHuman()`
pub const EE_UNITS_UC_PLAYER_IS_HUMAN: i32 = 535;
/// node `a`'s `mData->mKind`
pub const EE_UNITS_UC_NODE_KIND: i32 = 536;
/// node `a`'s `mData->mCalamity`
pub const EE_UNITS_UC_NODE_CALAMITY: i32 = 537;
/// `USingleton<EECalamityManager>::GetInstance()->LoadMedia(player a, (EECalamityEnum)b)`
pub const EE_UNITS_UC_CALAMITY_LOAD: i32 = 538;
/// `USingleton<EECalamityManager>::GetInstance()->UnloadMedia(player a, (EECalamityEnum)b)`
pub const EE_UNITS_UC_CALAMITY_UNLOAD: i32 = 539;
/// `class->mCommands[b]`
pub const EE_UNITS_UC_COMMAND: i32 = 540;
/// `EEButtonObject::Get(b)->Acquire()`
pub const EE_UNITS_UC_COMMAND_ACQUIRE: i32 = 541;
/// `EEButtonObject::Get(b)->Release()`
pub const EE_UNITS_UC_COMMAND_RELEASE: i32 = 542;
/// node `a`'s `mButton->Acquire()`
pub const EE_UNITS_UC_NODE_BUTTON_ACQUIRE: i32 = 543;
/// node `a`'s `mButton->Release()`
pub const EE_UNITS_UC_NODE_BUTTON_RELEASE: i32 = 544;
/// player `a`'s `SetUnitClassMedia(b, 0x7f)`
pub const EE_UNITS_UC_SET_CLASS_MEDIA: i32 = 545;
/// player `a`'s `UnloadUnitClassMedia(b)`
pub const EE_UNITS_UC_UNLOAD_CLASS_MEDIA: i32 = 546;
/// `EEGraphicEffect::Get(b)->Load(false)`
pub const EE_UNITS_UC_EFFECT_LOAD_ID: i32 = 547;
/// `EEGraphicEffect::Get(b)->Unload()`
pub const EE_UNITS_UC_EFFECT_UNLOAD_ID: i32 = 548;
/// the related class `b` (`class->*kRelated[b]`)
pub const EE_UNITS_UC_RELATED_CLASS: i32 = 549;
/// class `a`'s `mUnitType`
pub const EE_UNITS_UC_CLASS_TYPE_FIELD: i32 = 550;
/// `class->GetEffects()[b]`
pub const EE_UNITS_UC_EFFECT_AT: i32 = 551;
/// effect `a`'s `Load(false)`
pub const EE_UNITS_UC_EFFECT_LOAD: i32 = 552;
/// effect `a`'s `Unload()`
pub const EE_UNITS_UC_EFFECT_UNLOAD: i32 = 553;
/// effect `a`'s `Dereference()`
pub const EE_UNITS_UC_EFFECT_DEREFERENCE: i32 = 554;
/// `class->mIsMediaLoaded = b != 0`
pub const EE_UNITS_UC_SET_MEDIA_LOADED: i32 = 555;
/// `class->mAIScript = NULL`
pub const EE_UNITS_UC_CLEAR_AI_SCRIPT: i32 = 556;
/// class `a`'s `Dereference()`
pub const EE_UNITS_UC_CLASS_DEREFERENCE: i32 = 557;
/// `class->*kRelated[b] = NULL`
pub const EE_UNITS_UC_CLEAR_RELATED: i32 = 558;

// ---- the bodies (`ee_units_uc_call`'s `what`, from 60)
pub const EE_UNITS_UC_DO_LOAD_CLASS_MEDIA: i32 = 60;
pub const EE_UNITS_UC_DO_UNLOAD_CLASS_MEDIA: i32 = 61;
pub const EE_UNITS_UC_DO_LOAD_BUTTONS: i32 = 62;
pub const EE_UNITS_UC_DO_UNLOAD_BUTTONS: i32 = 63;
pub const EE_UNITS_UC_DO_BASE_LOAD_MEDIA: i32 = 64;
pub const EE_UNITS_UC_DO_BASE_UNLOAD_MEDIA: i32 = 65;
pub const EE_UNITS_UC_DO_DESTROY: i32 = 66;

const UNIT_TYPE_AIRPORT: i64 = 0x42;
const UNIT_TYPE_GRANARY: i64 = 0x34;
const UNIT_TYPE_GRANARY_IMPROVED: i64 = 0x2ba;
/// the airport's rally flag effects (`kGraphicEffectRallyFlagFighter`, `...Bomber`, `...AtomicBomber`,
/// `...Paratrooper`)
const RALLY_FLAG_EFFECTS: [i64; 4] = [0xd4, 0xd5, 0xd6, 0xf5];
/// the airport's runway class and the farm class (their media marked with it)
const RUNWAY_TYPE: i64 = 0xf0;
const FARM_TYPE: i64 = 0x2a;
/// the class's effects (`kUnitClassEffectCount`)
const EFFECT_COUNT: i64 = 29;
/// the related classes' indices in the host's table (projectile, scaffolding, death, death spawn, packed, remains)
const RELATED_MEDIA: [i64; 5] = [0, 1, 2, 3, 5];
const RELATED_ALL: i64 = 6;
/// the command buttons a human player acquires (2..0x32)
const COMMAND_COUNT: i64 = 9;

/// `LoadClassMedia()` (`load`) / `UnloadClassMedia()`: the class's acknowledge, selection, death and creation sounds,
/// its model and its button.
pub fn class_media<H: UcHost + ?Sized>(h: &mut H, class: u64, load: bool) {
    let mut s = S { h, class, unit: 0 };
    let sound_op = if load { EE_UNITS_UC_SOUND_LOAD } else { EE_UNITS_UC_SOUND_UNLOAD };
    for op in [EE_UNITS_UC_ACKNOWLEDGE_SOUND, EE_UNITS_UC_SELECTION_SOUND] {
        for i in 0..2 {
            let sound = s.b(op, i) as u64;
            if sound != 0 {
                s.a(sound_op, sound);
            }
        }
    }
    for op in [EE_UNITS_UC_CLASS_DEATH_SOUND, EE_UNITS_UC_CLASS_CREATION_SOUND] {
        let sound = s.s(op) as u64;
        if sound != 0 {
            s.a(sound_op, sound);
        }
    }
    let model = s.s(EE_UNITS_UC_CLASS_MODEL) as u64;
    if model != 0 {
        s.a(if load { EE_UNITS_UC_MODEL_LOAD } else { EE_UNITS_UC_MODEL_UNLOAD }, model);
    }
    let button = s.s(EE_UNITS_UC_CLASS_BUTTON) as u64;
    if button != 0 {
        s.a(if load { EE_UNITS_UC_BUTTON_ACQUIRE } else { EE_UNITS_UC_BUTTON_RELEASE }, button);
    }
}

/// `LoadButtons(player)` (`load`) / `UnloadButtons(player)`: a human player's buttons for the class's research and
/// commands, and its calamities' media.
pub fn buttons<H: UcHost + ?Sized>(h: &mut H, class: u64, player: u64, load: bool) {
    let mut s = S { h, class, unit: 0 };
    let tree = s.a(EE_UNITS_UC_TECH_TREE, player) as u64;
    if tree != 0 {
        let mut i = 0i64;
        while i < s.s(EE_UNITS_UC_RESEARCH_COUNT) as u32 as i64 {
            let node = s.ab(EE_UNITS_UC_RESEARCH_NODE, tree, i) as u64;
            i += 1;
            if node == 0 {
                continue;
            }
            if s.a(EE_UNITS_UC_PLAYER_IS_HUMAN, player) == 1 {
                s.a(if load { EE_UNITS_UC_NODE_BUTTON_ACQUIRE } else { EE_UNITS_UC_NODE_BUTTON_RELEASE }, node);
            }
            if s.a(EE_UNITS_UC_NODE_KIND, node) == 5 {
                let calamity = s.a(EE_UNITS_UC_NODE_CALAMITY, node);
                s.ab(if load { EE_UNITS_UC_CALAMITY_LOAD } else { EE_UNITS_UC_CALAMITY_UNLOAD }, player, calamity);
            }
        }
    }
    for j in 0..COMMAND_COUNT {
        let command = s.b(EE_UNITS_UC_COMMAND, j);
        if (2..0x32).contains(&command) && s.a(EE_UNITS_UC_PLAYER_IS_HUMAN, player) == 1 {
            s.b(if load { EE_UNITS_UC_COMMAND_ACQUIRE } else { EE_UNITS_UC_COMMAND_RELEASE }, command);
        }
    }
}

/// `BaseLoadMedia(player)` (`load`) / `BaseUnloadMedia(player)`: the class's graphic effects, and its related
/// classes (projectile, death, farms, runways ...) marked for loading or unloaded.
pub fn base_media<H: UcHost + ?Sized>(h: &mut H, class: u64, player: u64, load: bool) {
    let mut s = S { h, class, unit: 0 };
    let media = |s: &mut S<'_, H>, unit_type: i64| {
        if load {
            s.ab(EE_UNITS_UC_SET_CLASS_MEDIA, player, unit_type);
        } else {
            s.ab(EE_UNITS_UC_UNLOAD_CLASS_MEDIA, player, unit_type);
        }
    };
    let unit_type = s.s(crate::units::uc_produce::EE_UNITS_UC_THIS_UNIT_TYPE);
    if unit_type == UNIT_TYPE_AIRPORT {
        media(&mut s, RUNWAY_TYPE);
        for effect in RALLY_FLAG_EFFECTS {
            s.b(if load { EE_UNITS_UC_EFFECT_LOAD_ID } else { EE_UNITS_UC_EFFECT_UNLOAD_ID }, effect);
        }
    }
    if unit_type == UNIT_TYPE_GRANARY_IMPROVED || unit_type == UNIT_TYPE_GRANARY {
        media(&mut s, FARM_TYPE);
    }
    for k in RELATED_MEDIA {
        let related = s.b(EE_UNITS_UC_RELATED_CLASS, k) as u64;
        if related != 0 {
            let related_type = s.a(EE_UNITS_UC_CLASS_TYPE_FIELD, related);
            media(&mut s, related_type);
        }
    }
    for i in 0..EFFECT_COUNT {
        let effect = s.b(EE_UNITS_UC_EFFECT_AT, i) as u64;
        if effect != 0 {
            s.a(if load { EE_UNITS_UC_EFFECT_LOAD } else { EE_UNITS_UC_EFFECT_UNLOAD }, effect);
        }
    }
    s.b(EE_UNITS_UC_SET_MEDIA_LOADED, i64::from(load));
}

/// `~EEUnitClass()`'s releases: its graphic effects and its related classes.
pub fn destroy<H: UcHost + ?Sized>(h: &mut H, class: u64) {
    let mut s = S { h, class, unit: 0 };
    for i in 0..EFFECT_COUNT {
        let effect = s.b(EE_UNITS_UC_EFFECT_AT, i) as u64;
        if effect != 0 {
            s.a(EE_UNITS_UC_EFFECT_DEREFERENCE, effect);
        }
    }
    s.s(EE_UNITS_UC_CLEAR_AI_SCRIPT);
    for k in 0..RELATED_ALL {
        let related = s.b(EE_UNITS_UC_RELATED_CLASS, k) as u64;
        if related != 0 {
            s.a(EE_UNITS_UC_CLASS_DEREFERENCE, related);
            s.b(EE_UNITS_UC_CLEAR_RELATED, k);
        }
    }
}

/// Runs body `what` of `class` (`a` the player).
pub fn run<H: UcHost + ?Sized>(h: &mut H, what: i32, class: u64, _unit: u64, a: u64, _b: i64, _c: i64) -> Option<i64> {
    match what {
        EE_UNITS_UC_DO_LOAD_CLASS_MEDIA => class_media(h, class, true),
        EE_UNITS_UC_DO_UNLOAD_CLASS_MEDIA => class_media(h, class, false),
        EE_UNITS_UC_DO_LOAD_BUTTONS => buttons(h, class, a, true),
        EE_UNITS_UC_DO_UNLOAD_BUTTONS => buttons(h, class, a, false),
        EE_UNITS_UC_DO_BASE_LOAD_MEDIA => base_media(h, class, a, true),
        EE_UNITS_UC_DO_BASE_UNLOAD_MEDIA => base_media(h, class, a, false),
        EE_UNITS_UC_DO_DESTROY => destroy(h, class),
        _ => return None,
    }
    Some(0)
}

#[cfg(test)]
#[path = "uc_media_tests.rs"]
mod tests;
