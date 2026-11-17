//! The area effects (`Civilization Builder/EEAreaEffect.cpp`, `EEAreaEffectTemplate.cpp`, `EEAECloaking.cpp`,
//! `EEAEEconomy.cpp`, `EEAEHealing.cpp`, `EEAEMoraleFromBuildings.cpp`, `EEAEMoraleFromMilitaryHeroes.cpp` and the
//! inline kinds of `EEAEManager.cpp`; 0x27aa0b..0x27c8b9) as one record keyed by the template's kind.
//!
//! The eight C++ subclasses (healing, morale, calamity and conversion immunity, morale from buildings, morale from
//! military heroes, economy from units, cloaking) differed only in their slots' bodies; every slot is here, by kind:
//! [`strength`], [`level`], [`modify`], [`is_immunity`], [`covers`], [`applied_to`], [`removed_from`],
//! [`mark_dirty`], [`recount`], [`tick`]; the template's player and area tests ([`affects_player`],
//! [`affects_player_reversed`], [`is_within`]), its constructor and `Init` ([`template_reset`], [`template_init`]).
//! Every read or write of the world (units, players, the continents, the tech trees, hit points, cloaking, the
//! buildings around an effect) is a call to the host ([`AreaEffectHost`]), which the C++ (`EEAreaEffect.cpp`)
//! answers in one switch, in the original's order.

// the records keep the C++ member names; c_long is 32 bits on Windows
#![allow(non_snake_case, clippy::unnecessary_cast)]

use crate::ftol;
use ee_render::ge::types::Ge2DPoint;
use ee_render::ge::um::lrintf;
use std::os::raw::{c_char, c_long, c_void};

/// `kAreaEffectHealing` (ee/EEAreaEffectKindIDs.h).
pub const EE_AREA_EFFECT_HEALING: c_long = 1;
/// `kAreaEffectMorale`.
pub const EE_AREA_EFFECT_MORALE: c_long = 2;
/// `kAreaEffectCalamityImmunity`.
pub const EE_AREA_EFFECT_CALAMITY_IMMUNITY: c_long = 3;
/// `kAreaEffectConversionImmunity`.
pub const EE_AREA_EFFECT_CONVERSION_IMMUNITY: c_long = 4;
/// `kAreaEffectMoraleFromBuildings`.
pub const EE_AREA_EFFECT_MORALE_FROM_BUILDINGS: c_long = 5;
/// `kAreaEffectMoraleFromMilitaryHeroes`.
pub const EE_AREA_EFFECT_MORALE_FROM_MILITARY_HEROES: c_long = 6;
/// `kAreaEffectEconomyFromUnits`.
pub const EE_AREA_EFFECT_ECONOMY_FROM_UNITS: c_long = 7;
/// `kAreaEffectCloaking`.
pub const EE_AREA_EFFECT_CLOAKING: c_long = 8;

/// `Modify`'s value kinds: damage taken (the morale kinds), a gather amount (the economy), the cloaking strength.
pub const EE_AREA_EFFECT_MODIFY_DAMAGE: c_long = 1;
pub const EE_AREA_EFFECT_MODIFY_GATHER: c_long = 2;
pub const EE_AREA_EFFECT_MODIFY_CLOAK: c_long = 3;

/// `Relation`'s answer flags: the two ids name the same player object, the first is allied with the second, the
/// first is at war with the second (`mEnemies[other] != 0`).
pub const EE_AREA_EFFECT_SAME: i32 = 1;
pub const EE_AREA_EFFECT_ALLIED: i32 = 2;
pub const EE_AREA_EFFECT_AT_WAR: i32 = 4;

/// One area-effect template record of the area-effect table (`EEAreaEffectData`, ee/EEAreaEffectTemplate.h: 188
/// bytes, the same layout).
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeAreaEffectData {
    pub mName: [c_char; 104],
    pub mID: i32,
    pub mKind: i32,
    pub mShape: i32,
    pub mGraphicEffect: i32,
    pub mUnitSet: i32,
    pub mMode: i32,
    pub mGraphicEffect18: i32,
    pub mMedia: i32,
    pub mMaxLevel: i32,
    pub mLevel: i32,
    pub mUnusedValue1: i32,
    pub mUnusedValue2: i32,
    pub mUnusedValue3: i32,
    pub mRadius: f32,
    pub mRed: f32,
    pub mGreen: f32,
    pub mBlue: f32,
    pub mAlpha: f32,
    pub mUnusedFloat: f32,
    pub mUnusedFlag1: bool,
    pub mUnusedFlag2: bool,
    pub mUnusedFlag3: bool,
    pub mUnusedFlag4: bool,
    pub mUnusedFlag5: bool,
    pub mPad: [u8; 3],
}

const _: () = assert!(std::mem::size_of::<EeAreaEffectData>() == 188);

impl Default for EeAreaEffectData {
    fn default() -> Self {
        // SAFETY: plain numbers, flags and bytes: all zero is the record `EEAEManager::Load` zeroes first
        unsafe { std::mem::zeroed() }
    }
}

/// A template's numbers (`EEAreaEffectTemplate`'s members under their C++ names; its graphic effects and media are
/// C++ members). The color (the original's `GEColor mColor`) is `mColorRed`, `mColorGreen`, `mColorBlue`.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeAreaEffectTemplateFields {
    /// The kind (`EE_AREA_EFFECT_*`): which effect `CreateAreaEffect` makes.
    pub mKind: c_long,
    /// The area's shape: 1 a square, 2 a circle.
    pub mShape: c_long,
    /// The players it affects: 1 the owner, 2 its allies, 3 its enemies, 4 everyone.
    pub mMode: c_long,
    /// The radius in tiles, scaled by the owner unit's area-effect scale (0 or less: everywhere).
    pub mRadius: f32,
    pub mColorAlpha: f32,
    pub mID: c_long,
    /// The unit set it covers (`EEUnitSetManager`; 4: no continent test).
    pub mUnitSet: c_long,
    /// The cloaking's strength; the morale from buildings' cap before the variant's bonus.
    pub mMaxLevel: c_long,
    /// The healing's hit points per second, the morale's level.
    pub mLevel: c_long,
    pub mL44: c_long,
    pub mL48: c_long,
    pub mL4C: c_long,
    pub mRed: f32,
    pub mGreen: f32,
    pub mBlue: f32,
    pub mAlpha: f32,
    pub mF60: f32,
    pub mB64: bool,
    pub mB65: bool,
    pub mB66: bool,
    pub mB67: bool,
    pub mB68: bool,
    pub mColorRed: f32,
    pub mColorGreen: f32,
    pub mColorBlue: f32,
}

/// An area effect's numbers (`EEAreaEffect` and its kinds' members; its location, template and owner unit are C++
/// members).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeAreaEffectFields {
    /// The template's kind when the effect was made: the original's class.
    pub mKind: c_long,
    /// The owner's player id.
    pub mPlayer: c_long,
    /// The morale from buildings' cap (the template's maximum level plus the game variant's morale bonus).
    pub mMax: c_long,
    /// The morale from buildings' count of buildings.
    pub mCount: c_long,
    /// The morale from buildings' recount flag.
    pub mDirty: bool,
}

/// The host calls. "Target" is the unit the slot was given (the covered, ticked, cloaked unit), "owner" the
/// effect's own unit.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeAreaEffectOp {
    /// Whether the target's unit type is in unit set `a` (`gUnitSetManager.Contains`).
    TargetInSet = 0,
    /// Whether player `a`'s tech tree node of the target's class technology is of kind 4.
    TargetTechKind4 = 1,
    /// Players `a` and `b` (ids into the world's table): value the `EE_AREA_EFFECT_SAME/ALLIED/AT_WAR` flags.
    Relation = 2,
    /// Whether the target is built.
    TargetBuilt = 3,
    /// Whether the target can be healed (`IsRepairable`).
    TargetRepairable = 4,
    /// The target's position (x, y of its physical model's location).
    TargetPosition = 5,
    /// The continent of tile (`a`, `b`) for the ground paths: value.
    Continent = 6,
    /// ok when the target is a space dock or turret carried by a complex unit: value that unit's planet.
    TargetCarrierPlanet = 7,
    /// The target's hit points: value.
    TargetHitPoints = 8,
    /// The target's maximum hit points: value.
    TargetMaxHitPoints = 9,
    /// Whether a cinematic runs (the free camera flag).
    InCinematic = 10,
    /// The target's `SetHitPoints(a)`.
    TargetSetHitPoints = 11,
    /// The target's `SetCloakableChain(true)` (the cloaking's `AppliedTo`).
    TargetCloak = 12,
    /// The cloaking's `RemovedFrom` on the target (the camouflage power's test, the uncloak, the mini map).
    TargetUncloak = 13,
    /// ok when the effect has an owner unit: x its area-effect scale.
    OwnerScale = 14,
    /// The owner's hit points: value.
    OwnerHitPoints = 15,
    /// The owner's leadership: x.
    OwnerLeadership = 16,
    /// The owner class's garrison bonus: x.
    OwnerGarrisonBonus = 17,
    /// The owner player's living, built morale buildings in rectangle (`a` left, `b` top, `c` right, `d` bottom):
    /// shape `e` 1 those whose footprint meets it, else those whose tile is within `x` (squared) of tile (`f`, `g`):
    /// value their count.
    CountBuildings = 18,
}

/// One host call.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct EeAreaEffectCall {
    pub op: EeAreaEffectOp,
    pub a: i32,
    pub b: i32,
    pub c: i32,
    pub d: i32,
    pub e: i32,
    pub f: i32,
    pub g: i32,
    pub x: f32,
}

impl EeAreaEffectCall {
    pub fn new(op: EeAreaEffectOp) -> Self {
        EeAreaEffectCall { op, a: 0, b: 0, c: 0, d: 0, e: 0, f: 0, g: 0, x: 0.0 }
    }
    fn ab(op: EeAreaEffectOp, a: i32, b: i32) -> Self {
        EeAreaEffectCall { a, b, ..Self::new(op) }
    }
}

/// The host's answer.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeAreaEffectReply {
    pub ok: bool,
    pub value: i32,
    pub x: f32,
    pub y: f32,
}

/// The world, as the C++ answers an area effect's calls (a test host in the tests).
pub trait AreaEffectHost {
    fn call(&mut self, call: &EeAreaEffectCall) -> EeAreaEffectReply;
}

fn ask<H: AreaEffectHost>(h: &mut H, op: EeAreaEffectOp) -> EeAreaEffectReply {
    h.call(&EeAreaEffectCall::new(op))
}

/// Whether `kind` is one `CreateAreaEffect` makes an effect for (else it returns NULL).
pub fn known_kind(kind: c_long) -> bool {
    (EE_AREA_EFFECT_HEALING..=EE_AREA_EFFECT_CLOAKING).contains(&kind)
}

/// The constructor of the kind's effect: the morale from buildings' cap and its first recount pending.
pub fn init(kind: c_long, template_max_level: c_long, morale_bonus: c_long) -> EeAreaEffectFields {
    let mut e = EeAreaEffectFields { mKind: kind, ..EeAreaEffectFields::default() };
    if kind == EE_AREA_EFFECT_MORALE_FROM_BUILDINGS {
        e.mMax = template_max_level.wrapping_add(morale_bonus);
        e.mDirty = true;
    }
    e
}

/// Whether two effects of these kinds count as one in a unit's list (`EEUnitAreaEffectList::AddAreaEffect`: the
/// same kind, or both of the three morale kinds).
pub fn same_group(kind: c_long, other: c_long) -> bool {
    let morale = |k: c_long| {
        k == EE_AREA_EFFECT_MORALE
            || k == EE_AREA_EFFECT_MORALE_FROM_BUILDINGS
            || k == EE_AREA_EFFECT_MORALE_FROM_MILITARY_HEROES
    };
    kind == other || (morale(kind) && morale(other))
}

/// The template's constructor 0x27c0f3 (its id is left as it was: 0 here; `Init` sets it).
pub fn template_reset() -> EeAreaEffectTemplateFields {
    EeAreaEffectTemplateFields {
        mKind: 0,
        mShape: 1,
        mMode: 2,
        mRadius: 1.0,
        mColorAlpha: 0.15,
        mID: 0,
        mUnitSet: -1,
        mMaxLevel: -1,
        mLevel: -1,
        mL44: -1,
        mL48: -1,
        mL4C: -1,
        mRed: -1.0,
        mGreen: -1.0,
        mBlue: -1.0,
        mAlpha: -1.0,
        mF60: -1.0,
        mB64: false,
        mB65: false,
        mB66: false,
        mB67: false,
        mB68: false,
        mColorRed: 1.0,
        mColorGreen: 1.0,
        mColorBlue: 1.0,
    }
}

/// `Init` 0x27c489's numbers from a record (the graphic effects and media are the C++'s).
pub fn template_init(t: &mut EeAreaEffectTemplateFields, d: &EeAreaEffectData) {
    t.mKind = d.mKind as c_long;
    t.mID = d.mID as c_long;
    t.mRadius = d.mRadius;
    t.mShape = d.mShape as c_long;
    t.mMode = d.mMode as c_long;
    t.mUnitSet = d.mUnitSet as c_long;
    t.mMaxLevel = d.mMaxLevel as c_long;
    t.mLevel = d.mLevel as c_long;
    t.mL44 = d.mUnusedValue1 as c_long;
    t.mL48 = d.mUnusedValue2 as c_long;
    t.mL4C = d.mUnusedValue3 as c_long;
    t.mRed = d.mRed;
    t.mGreen = d.mGreen;
    t.mBlue = d.mBlue;
    t.mAlpha = d.mAlpha;
    t.mF60 = d.mUnusedFloat;
    t.mB64 = d.mUnusedFlag1;
    t.mB65 = d.mUnusedFlag2;
    t.mB66 = d.mUnusedFlag3;
    t.mB67 = d.mUnusedFlag4;
    t.mB68 = d.mUnusedFlag5;
    t.mColorRed = t.mRed;
    t.mColorGreen = t.mGreen;
    t.mColorBlue = t.mBlue;
    t.mColorAlpha = t.mAlpha;
}

/// `AffectsPlayer` 0x27c1c5: player `other` counts for `player` under the template's mode (4 everyone, 1 itself, 2
/// its allies, 3 the players it is at war with).
pub fn affects_player<H: AreaEffectHost>(h: &mut H, t: &EeAreaEffectTemplateFields, player: i32, other: i32) -> bool {
    let r = h.call(&EeAreaEffectCall::ab(EeAreaEffectOp::Relation, player, other)).value;
    let allied = r & EE_AREA_EFFECT_ALLIED != 0;
    match t.mMode {
        4 => true,
        1 => player == other,
        2 => allied,
        3 => r & EE_AREA_EFFECT_SAME == 0 && r & EE_AREA_EFFECT_AT_WAR != 0,
        _ => false,
    }
}

/// `AffectsPlayerReversed` 0x27c233: `other` falls outside the mode (1 the others, 2 the enemies, 3 the friends:
/// itself or not at war).
pub fn affects_player_reversed<H: AreaEffectHost>(
    h: &mut H,
    t: &EeAreaEffectTemplateFields,
    player: i32,
    other: i32,
) -> bool {
    let r = h.call(&EeAreaEffectCall::ab(EeAreaEffectOp::Relation, player, other)).value;
    let friendly = r & EE_AREA_EFFECT_SAME != 0 || r & EE_AREA_EFFECT_AT_WAR == 0;
    match t.mMode {
        1 => player != other,
        2 => !friendly,
        3 => friendly,
        _ => false,
    }
}

/// `IsWithin` 0x27c334: `point` lies in the square or circle of `scale` times the radius around `center`, and on
/// `center`'s continent unless the unit set is 4 (a space dock or turret `target` carried by a unit also passes on
/// that unit's planet; `has_target`: the C++ passed a unit).
pub fn is_within<H: AreaEffectHost>(
    h: &mut H,
    t: &EeAreaEffectTemplateFields,
    center: Ge2DPoint,
    point: Ge2DPoint,
    scale: f32,
    has_target: bool,
) -> bool {
    let range = scale * t.mRadius;
    if t.mRadius <= 0.0 {
        return true;
    }
    if t.mShape == 1 {
        let dx = (center.x - point.x).abs();
        let dy = (center.y - point.y).abs();
        if dx > range || dy > range {
            return false;
        }
    } else if t.mShape != 2 || point.distance_to(center) > range {
        return false;
    }
    let mut result = true;
    if t.mUnitSet != 4 {
        let continent = h.call(&EeAreaEffectCall::ab(EeAreaEffectOp::Continent, ftol(point.x), ftol(point.y))).value;
        if continent != 0 {
            let other = h.call(&EeAreaEffectCall::ab(EeAreaEffectOp::Continent, ftol(center.x), ftol(center.y))).value;
            if continent != other {
                result = false;
                if has_target {
                    let r = ask(h, EeAreaEffectOp::TargetCarrierPlanet);
                    if r.ok {
                        result = continent == r.value;
                    }
                }
            }
        }
    }
    result
}

/// `GetScale` 0x27c0b7: the owner unit's area-effect scale, 1 without one.
pub fn scale<H: AreaEffectHost>(h: &mut H) -> f32 {
    let r = ask(h, EeAreaEffectOp::OwnerScale);
    if r.ok {
        r.x
    } else {
        1.0
    }
}

/// `GetRadius` 0x27c0a0: the template's radius times the scale.
pub fn radius<H: AreaEffectHost>(h: &mut H, t: &EeAreaEffectTemplateFields) -> f32 {
    let s = scale(h);
    t.mRadius * s
}

/// The economy's rate (`EEAEEconomyFromUnits::GetRate` 0x27ac9e): the garrison bonus as `min(max(0, b), 10)`.
fn economy_rate<H: AreaEffectHost>(h: &mut H) -> f32 {
    let b = ask(h, EeAreaEffectOp::OwnerGarrisonBonus).x;
    let low = if 0.0 > b { 0.0 } else { b };
    if low < 10.0 {
        low
    } else {
        10.0
    }
}

/// The morale kinds' level (slot +0x4c): the template's level, the counted buildings up to the cap, the hero's
/// leadership while it lives.
pub fn level<H: AreaEffectHost>(h: &mut H, e: &EeAreaEffectFields, t: &EeAreaEffectTemplateFields) -> c_long {
    match e.mKind {
        EE_AREA_EFFECT_MORALE_FROM_BUILDINGS => {
            if e.mCount < e.mMax {
                e.mCount
            } else {
                e.mMax
            }
        }
        EE_AREA_EFFECT_MORALE_FROM_MILITARY_HEROES => {
            if ask(h, EeAreaEffectOp::OwnerHitPoints).value > 0 {
                lrintf(ask(h, EeAreaEffectOp::OwnerLeadership).x) as c_long
            } else {
                0
            }
        }
        _ => t.mLevel,
    }
}

/// `GetStrength` (slot +0x18): which of two effects of a kind a unit keeps.
pub fn strength<H: AreaEffectHost>(h: &mut H, e: &EeAreaEffectFields, t: &EeAreaEffectTemplateFields) -> c_long {
    match e.mKind {
        EE_AREA_EFFECT_HEALING => t.mLevel,
        EE_AREA_EFFECT_MORALE | EE_AREA_EFFECT_MORALE_FROM_BUILDINGS | EE_AREA_EFFECT_MORALE_FROM_MILITARY_HEROES => {
            level(h, e, t)
        }
        EE_AREA_EFFECT_ECONOMY_FROM_UNITS => ftol(economy_rate(h) * 10.0) as c_long,
        EE_AREA_EFFECT_CLOAKING => t.mMaxLevel,
        _ => 0,
    }
}

/// `Modify` (slot +0x30): the morale kinds lessen damage taken by a tenth per level (0..10), the economy scales a
/// gather amount by its rate (0..10), the cloaking answers its strength; `None` when the kind leaves the value.
pub fn modify<H: AreaEffectHost>(
    h: &mut H,
    e: &EeAreaEffectFields,
    t: &EeAreaEffectTemplateFields,
    kind: c_long,
    value: c_long,
) -> Option<c_long> {
    match e.mKind {
        EE_AREA_EFFECT_MORALE | EE_AREA_EFFECT_MORALE_FROM_BUILDINGS | EE_AREA_EFFECT_MORALE_FROM_MILITARY_HEROES
            if kind == EE_AREA_EFFECT_MODIFY_DAMAGE =>
        {
            let l = level(h, e, t);
            let capped = if l > 10 { 10 } else { l };
            let l = if 0 > capped { 0 } else { capped };
            Some(value.wrapping_sub(lrintf(value as f32 * (l as f32 / 10.0)) as c_long))
        }
        EE_AREA_EFFECT_ECONOMY_FROM_UNITS if kind == EE_AREA_EFFECT_MODIFY_GATHER => {
            let rate = economy_rate(h);
            let capped = if rate > 10.0 { 10.0 } else { rate };
            let rate = if 0.0 > capped { 0.0 } else { capped };
            Some(lrintf(rate * value as f32) as c_long)
        }
        EE_AREA_EFFECT_CLOAKING if kind == EE_AREA_EFFECT_MODIFY_CLOAK => Some(strength(h, e, t)),
        _ => None,
    }
}

/// `IsImmunity` (slot +0x20): the two immunities.
pub fn is_immunity(e: &EeAreaEffectFields) -> bool {
    e.mKind == EE_AREA_EFFECT_CALAMITY_IMMUNITY || e.mKind == EE_AREA_EFFECT_CONVERSION_IMMUNITY
}

/// The target's position as the area tests take it.
fn target_position<H: AreaEffectHost>(h: &mut H) -> Ge2DPoint {
    let r = ask(h, EeAreaEffectOp::TargetPosition);
    Ge2DPoint { x: r.x, y: r.y }
}

/// `CoversUnit` 0x27c28e (the base's `Covers`): the target is in the unit set, its owner `owner` is affected, it is
/// built, and it lies in the area around `point`.
fn covers_unit<H: AreaEffectHost>(
    h: &mut H,
    t: &EeAreaEffectTemplateFields,
    player: i32,
    owner: i32,
    point: Ge2DPoint,
    scale: f32,
) -> bool {
    if !h.call(&EeAreaEffectCall::ab(EeAreaEffectOp::TargetInSet, t.mUnitSet as i32, 0)).ok
        || !affects_player(h, t, player, owner)
        || !ask(h, EeAreaEffectOp::TargetBuilt).ok
    {
        return false;
    }
    if t.mRadius > 0.0 {
        let center = target_position(h);
        is_within(h, t, center, point, scale, true)
    } else {
        true
    }
}

/// `Covers` (slot +0x08) for the target owned by player `owner`: the healing only repairable units, the cloaking
/// without the built test and not units whose technology is a kind-4 node of `player`'s tree.
pub fn covers<H: AreaEffectHost>(
    h: &mut H,
    e: &EeAreaEffectFields,
    t: &EeAreaEffectTemplateFields,
    player: i32,
    owner: i32,
    point: Ge2DPoint,
) -> bool {
    match e.mKind {
        EE_AREA_EFFECT_CLOAKING => {
            if !h.call(&EeAreaEffectCall::ab(EeAreaEffectOp::TargetInSet, t.mUnitSet as i32, 0)).ok {
                return false;
            }
            if h.call(&EeAreaEffectCall::ab(EeAreaEffectOp::TargetTechKind4, player, 0)).ok
                || !affects_player(h, t, player, owner)
            {
                return false;
            }
            if t.mRadius > 0.0 {
                let center = target_position(h);
                let s = scale(h);
                is_within(h, t, center, point, s, true)
            } else {
                true
            }
        }
        EE_AREA_EFFECT_HEALING => {
            if !ask(h, EeAreaEffectOp::TargetRepairable).ok {
                return false;
            }
            let s = scale(h);
            covers_unit(h, t, player, owner, point, s)
        }
        _ => {
            let s = scale(h);
            covers_unit(h, t, player, owner, point, s)
        }
    }
}

/// `AppliedTo` (slot +0x04): the cloaking makes the target cloakable.
pub fn applied_to<H: AreaEffectHost>(h: &mut H, e: &EeAreaEffectFields) {
    if e.mKind == EE_AREA_EFFECT_CLOAKING {
        ask(h, EeAreaEffectOp::TargetCloak);
    }
}

/// `RemovedFrom` (slot +0x38): the cloaking gives the target back its own cloakability.
pub fn removed_from<H: AreaEffectHost>(h: &mut H, e: &EeAreaEffectFields) {
    if e.mKind == EE_AREA_EFFECT_CLOAKING {
        ask(h, EeAreaEffectOp::TargetUncloak);
    }
}

/// `MarkDirty` (slot +0x3c): the morale from buildings recounts at its next `Recount`.
pub fn mark_dirty(e: &mut EeAreaEffectFields) {
    if e.mKind == EE_AREA_EFFECT_MORALE_FROM_BUILDINGS {
        e.mDirty = true;
    }
}

/// `Recount` 0x27b95b (slot +0x44; the morale from buildings): when flagged, the owner's buildings in the square or
/// circle around `location`, up to the cap.
pub fn recount<H: AreaEffectHost>(
    h: &mut H,
    e: &mut EeAreaEffectFields,
    t: &EeAreaEffectTemplateFields,
    location: Ge2DPoint,
) {
    if e.mKind != EE_AREA_EFFECT_MORALE_FROM_BUILDINGS {
        return;
    }
    let tile_x = ftol(location.x);
    let tile_y = ftol(location.y);
    let shape = t.mShape;
    let r = lrintf(radius(h, t));
    if !e.mDirty {
        return;
    }
    let dx = location.x - tile_x as f32;
    let dy = location.y - tile_y as f32;
    let (top, left, right, bottom) = if dx > 0.25 && dx < 0.75 && dy > 0.25 && dy < 0.75 {
        (tile_y.wrapping_sub(r), tile_x.wrapping_sub(r), r.wrapping_add(tile_x), tile_y.wrapping_add(r))
    } else {
        (
            tile_y.wrapping_sub(r),
            tile_x.wrapping_sub(r),
            r.wrapping_add(tile_x).wrapping_sub(1),
            tile_y.wrapping_add(r).wrapping_sub(1),
        )
    };
    let mut call = EeAreaEffectCall {
        a: left,
        b: top,
        c: right,
        d: bottom,
        e: shape as i32,
        ..EeAreaEffectCall::new(EeAreaEffectOp::CountBuildings)
    };
    if shape != 1 {
        let range = radius(h, t);
        call.f = tile_x;
        call.g = tile_y;
        call.x = range * range;
    }
    let mut count = h.call(&call).value as c_long;
    let limited = if e.mMax < count { e.mMax } else { count };
    if limited < 0 {
        count = 0;
    } else if e.mMax < count {
        count = e.mMax;
    }
    e.mDirty = false;
    e.mCount = count;
}

/// `Tick` (slot +0x48; the healing 0x27ad56): a living target gains the level per second of `elapsed` (in ms; at
/// least 1), up to its maximum; nothing during a cinematic.
pub fn tick<H: AreaEffectHost>(h: &mut H, e: &EeAreaEffectFields, t: &EeAreaEffectTemplateFields, elapsed: f32) {
    if e.mKind != EE_AREA_EFFECT_HEALING {
        return;
    }
    let hit_points = ask(h, EeAreaEffectOp::TargetHitPoints).value;
    if hit_points <= 0 || ask(h, EeAreaEffectOp::InCinematic).ok {
        return;
    }
    let mut heal = lrintf(t.mLevel as f32 * (elapsed * 0.001));
    if heal <= 1 {
        heal = 1;
    }
    let mut hit_points = hit_points.wrapping_add(heal);
    let max = ask(h, EeAreaEffectOp::TargetMaxHitPoints).value;
    if hit_points >= max {
        hit_points = max;
    }
    h.call(&EeAreaEffectCall::ab(EeAreaEffectOp::TargetSetHitPoints, hit_points, 0));
}

/// `Checksum` 0x27c073 (slot +0x14): the location's coordinates as their bit patterns, the player and the owner
/// unit's id (0 without one), summed as `long`.
pub fn checksum(e: &EeAreaEffectFields, location: Ge2DPoint, unit_id: c_long) -> c_long {
    let sum = (location.x.to_bits() as i32 as c_long).wrapping_add(location.y.to_bits() as i32 as c_long);
    e.mPlayer.wrapping_add(unit_id).wrapping_add(sum)
}

/// The C++ host of an area effect's calls (`EEAreaEffect.cpp`): `call` answers one [`EeAreaEffectCall`] on the C++
/// objects (the effect and the unit it was given).
#[repr(C)]
#[derive(Clone, Copy)]
pub struct EeAreaEffectHostFfi {
    pub ctx: *mut c_void,
    pub call: unsafe extern "C" fn(ctx: *mut c_void, call: *const EeAreaEffectCall, reply: *mut EeAreaEffectReply),
}

struct FfiHost(EeAreaEffectHostFfi);

impl AreaEffectHost for FfiHost {
    fn call(&mut self, call: &EeAreaEffectCall) -> EeAreaEffectReply {
        let mut reply = EeAreaEffectReply::default();
        // SAFETY: the C++ host's contract: valid pointers for the call's duration
        unsafe { (self.0.call)(self.0.ctx, call, &mut reply) };
        reply
    }
}

/// A copy of the host: every entry point works on copies, so no reference into C++ memory lives across a host call.
///
/// # Safety
/// `h` must be valid.
unsafe fn host(h: *const EeAreaEffectHostFfi) -> FfiHost {
    // SAFETY: the caller's contract
    FfiHost(unsafe { *h })
}

/// [`template_reset`] into `t`.
///
/// # Safety
/// `t` must be valid.
#[no_mangle]
pub unsafe extern "C" fn ee_area_effect_template_reset(t: *mut EeAreaEffectTemplateFields) {
    // SAFETY: the caller's contract
    unsafe { *t = template_reset() };
}

/// [`template_init`] of `t` from the record `d`.
///
/// # Safety
/// `t` and `d` must be valid.
#[no_mangle]
pub unsafe extern "C" fn ee_area_effect_template_init(t: *mut EeAreaEffectTemplateFields, d: *const EeAreaEffectData) {
    // SAFETY: the caller's contract
    unsafe { template_init(&mut *t, &*d) };
}

/// [`affects_player`] (or, `reversed`, [`affects_player_reversed`]).
///
/// # Safety
/// `t` and `h` must be valid.
#[no_mangle]
pub unsafe extern "C" fn ee_area_effect_affects_player(
    t: *const EeAreaEffectTemplateFields,
    h: *const EeAreaEffectHostFfi,
    player: i32,
    other: i32,
    reversed: bool,
) -> bool {
    // SAFETY: the caller's contract
    let (t, mut h) = unsafe { (*t, host(h)) };
    if reversed {
        affects_player_reversed(&mut h, &t, player, other)
    } else {
        affects_player(&mut h, &t, player, other)
    }
}

/// [`is_within`].
///
/// # Safety
/// `t` and `h` must be valid.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_area_effect_is_within(
    t: *const EeAreaEffectTemplateFields,
    h: *const EeAreaEffectHostFfi,
    center_x: f32,
    center_y: f32,
    point_x: f32,
    point_y: f32,
    scale: f32,
    has_target: bool,
) -> bool {
    // SAFETY: the caller's contract
    let (t, mut h) = unsafe { (*t, host(h)) };
    let center = Ge2DPoint { x: center_x, y: center_y };
    is_within(&mut h, &t, center, Ge2DPoint { x: point_x, y: point_y }, scale, has_target)
}

/// [`known_kind`].
#[no_mangle]
pub extern "C" fn ee_area_effect_known_kind(kind: c_long) -> bool {
    known_kind(kind)
}

/// [`init`] into `e`.
///
/// # Safety
/// `e` must be valid.
#[no_mangle]
pub unsafe extern "C" fn ee_area_effect_init(
    e: *mut EeAreaEffectFields,
    kind: c_long,
    template_max_level: c_long,
    morale_bonus: c_long,
) {
    // SAFETY: the caller's contract
    unsafe { *e = init(kind, template_max_level, morale_bonus) };
}

/// [`same_group`].
#[no_mangle]
pub extern "C" fn ee_area_effect_same_group(kind: c_long, other: c_long) -> bool {
    same_group(kind, other)
}

/// [`scale`].
///
/// # Safety
/// `h` must be valid.
#[no_mangle]
pub unsafe extern "C" fn ee_area_effect_scale(h: *const EeAreaEffectHostFfi) -> f32 {
    // SAFETY: the caller's contract
    scale(&mut unsafe { host(h) })
}

/// [`radius`].
///
/// # Safety
/// `t` and `h` must be valid.
#[no_mangle]
pub unsafe extern "C" fn ee_area_effect_radius(
    t: *const EeAreaEffectTemplateFields,
    h: *const EeAreaEffectHostFfi,
) -> f32 {
    // SAFETY: the caller's contract
    let (t, mut h) = unsafe { (*t, host(h)) };
    radius(&mut h, &t)
}

/// [`level`].
///
/// # Safety
/// `e`, `t` and `h` must be valid.
#[no_mangle]
pub unsafe extern "C" fn ee_area_effect_level(
    e: *const EeAreaEffectFields,
    t: *const EeAreaEffectTemplateFields,
    h: *const EeAreaEffectHostFfi,
) -> c_long {
    // SAFETY: the caller's contract
    let (e, t, mut h) = unsafe { (*e, *t, host(h)) };
    level(&mut h, &e, &t)
}

/// [`strength`].
///
/// # Safety
/// `e`, `t` and `h` must be valid.
#[no_mangle]
pub unsafe extern "C" fn ee_area_effect_strength(
    e: *const EeAreaEffectFields,
    t: *const EeAreaEffectTemplateFields,
    h: *const EeAreaEffectHostFfi,
) -> c_long {
    // SAFETY: the caller's contract
    let (e, t, mut h) = unsafe { (*e, *t, host(h)) };
    strength(&mut h, &e, &t)
}

/// [`modify`]: whether the kind modifies `value`, the result in `out` (left alone otherwise).
///
/// # Safety
/// `e`, `t`, `h` and `out` must be valid.
#[no_mangle]
pub unsafe extern "C" fn ee_area_effect_modify(
    e: *const EeAreaEffectFields,
    t: *const EeAreaEffectTemplateFields,
    h: *const EeAreaEffectHostFfi,
    kind: c_long,
    value: c_long,
    out: *mut c_long,
) -> bool {
    // SAFETY: the caller's contract
    let (e, t, mut h) = unsafe { (*e, *t, host(h)) };
    match modify(&mut h, &e, &t, kind, value) {
        Some(v) => {
            // SAFETY: as above
            unsafe { *out = v };
            true
        }
        None => false,
    }
}

/// [`is_immunity`].
///
/// # Safety
/// `e` must be valid.
#[no_mangle]
pub unsafe extern "C" fn ee_area_effect_is_immunity(e: *const EeAreaEffectFields) -> bool {
    // SAFETY: the caller's contract
    is_immunity(unsafe { &*e })
}

/// [`covers`].
///
/// # Safety
/// `e`, `t` and `h` must be valid.
#[no_mangle]
pub unsafe extern "C" fn ee_area_effect_covers(
    e: *const EeAreaEffectFields,
    t: *const EeAreaEffectTemplateFields,
    h: *const EeAreaEffectHostFfi,
    player: i32,
    owner: i32,
    point_x: f32,
    point_y: f32,
) -> bool {
    // SAFETY: the caller's contract
    let (e, t, mut h) = unsafe { (*e, *t, host(h)) };
    covers(&mut h, &e, &t, player, owner, Ge2DPoint { x: point_x, y: point_y })
}

/// [`applied_to`] (`removed`: [`removed_from`]).
///
/// # Safety
/// `e` and `h` must be valid.
#[no_mangle]
pub unsafe extern "C" fn ee_area_effect_applied(
    e: *const EeAreaEffectFields,
    h: *const EeAreaEffectHostFfi,
    removed: bool,
) {
    // SAFETY: the caller's contract
    let (e, mut h) = unsafe { (*e, host(h)) };
    if removed {
        removed_from(&mut h, &e);
    } else {
        applied_to(&mut h, &e);
    }
}

/// [`mark_dirty`].
///
/// # Safety
/// `e` must be valid.
#[no_mangle]
pub unsafe extern "C" fn ee_area_effect_mark_dirty(e: *mut EeAreaEffectFields) {
    // SAFETY: the caller's contract
    mark_dirty(unsafe { &mut *e });
}

/// [`recount`]; `e` in and out.
///
/// # Safety
/// `e`, `t` and `h` must be valid.
#[no_mangle]
pub unsafe extern "C" fn ee_area_effect_recount(
    e: *mut EeAreaEffectFields,
    t: *const EeAreaEffectTemplateFields,
    h: *const EeAreaEffectHostFfi,
    location_x: f32,
    location_y: f32,
) {
    // SAFETY: the caller's contract; a copy across the host's calls
    let (mut s, t, mut h) = unsafe { (*e, *t, host(h)) };
    recount(&mut h, &mut s, &t, Ge2DPoint { x: location_x, y: location_y });
    // SAFETY: as above
    unsafe { *e = s };
}

/// [`tick`].
///
/// # Safety
/// `e`, `t` and `h` must be valid.
#[no_mangle]
pub unsafe extern "C" fn ee_area_effect_tick(
    e: *const EeAreaEffectFields,
    t: *const EeAreaEffectTemplateFields,
    h: *const EeAreaEffectHostFfi,
    elapsed: f32,
) {
    // SAFETY: the caller's contract
    let (e, t, mut h) = unsafe { (*e, *t, host(h)) };
    tick(&mut h, &e, &t, elapsed);
}

/// [`checksum`].
///
/// # Safety
/// `e` must be valid.
#[no_mangle]
pub unsafe extern "C" fn ee_area_effect_checksum(
    e: *const EeAreaEffectFields,
    location_x: f32,
    location_y: f32,
    unit_id: c_long,
) -> c_long {
    // SAFETY: the caller's contract
    checksum(unsafe { &*e }, Ge2DPoint { x: location_x, y: location_y }, unit_id)
}

#[cfg(test)]
#[path = "area_effect_tests.rs"]
mod tests;
