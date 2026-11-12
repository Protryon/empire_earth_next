//! The damage glue (R5b.2, tenth round): `EEUnitClass::ApplyHit` (a shot's hit: the splash, the direct damage, the
//! target's reaction, the kill count, a hunted animal's food), `ApplyAreaDamage` with each victim's hit, and
//! `ComputeDamage` (the record of one hit's damage: its store part here, the tables, the stats and combat.yaml the
//! C++'s) over the world host ([`WorldHost`]: the unit store's fields through [`Stored`], the rest C++ reads and
//! operations made where the original made them).
//!
//! Determinism (RUST_PLAN 2.1): the food share is `long x float` rounded by `lrintf` (ApplyHit) or the x86 ceiling
//! (the splash), as the original; the formulas are lib.rs's; no random draw.

use crate::reads::{
    Stored, EE_COMBAT_H_COMPLEX, EE_COMBAT_H_OWNER, EE_COMBAT_H_PLAYER, EE_COMBAT_Q_CATEGORY, EE_COMBAT_Q_ENEMY,
    EE_COMBAT_Q_HIT_POINTS, EE_COMBAT_Q_PLAYER_ID, EE_COMBAT_Q_TARGETABLE, EE_COMBAT_Q_TILE_X, EE_COMBAT_Q_TILE_Y,
    EE_COMBAT_Q_UNIT_TYPE,
};
use crate::walks::{area_victims, AreaHost, EeCombatAreaIn, EeCombatHandle};
use crate::world::{EeCombatWorldHost, WorldHost, EE_COMBAT_Q_DISTANCE_SQUARED};
use crate::{
    ceil, damage, hit_factor, EeCombatDamageIn, EeCombatDamageOut, DAMAGE_DIRECT, DAMAGE_SPLASH, DAMAGE_TRAMPLE,
};
use ee_ge::um::lrintf;

// ---- the reads (world.rs's numbering goes on); the first three are the store's (never asked of the C++)

/// `EEUnit::mResourceAmount` (the store's).
pub const EE_COMBAT_Q_RESOURCE_AMOUNT: i32 = 68;
/// `mHerd != NULL` (the store's).
pub const EE_COMBAT_Q_HERD: i32 = 69;
/// `mCloaked`, the fanatic's timer (the store's).
pub const EE_COMBAT_Q_CLOAK_TIMER: i32 = 70;
/// `EEUnit::mResourceType`.
pub const EE_COMBAT_Q_RESOURCE_TYPE: i32 = 71;

// ---- the operations (world.rs's numbering goes on)

/// `ComputeDamage`'s C++ part into the record at `args[0]` (an `EeCombatDamageIn*`; `form` and `attack` set) for
/// `object` hitting `other` with ammunition `args[1]` and weapon column `args[2]`: the attack factor
/// (`gToHitTable.GetAttackFactor`) and `gWeaponToHitFactors` unless a direct hit without attack, the target's
/// armour of the weapon, combat.yaml's constants.
pub const EE_COMBAT_W_DAMAGE_IN: i32 = 40;
/// The class's `GetElevationFactor(object, other)` (other: the target's complex unit), as float bits.
pub const EE_COMBAT_W_ELEVATION: i32 = 41;
/// TEST ONLY (CBDM): the record at `args[0]` and its result at `args[1]` (an `EeCombatDamageOut*`).
pub const EE_COMBAT_W_DAMAGE_CAPTURE: i32 = 42;
/// ApplyHit's classes of `object` into `args`: the hit's (a capital ship's special weapon, else the launcher's or the
/// class's) attack and area effect as float bits (its player's upgrade's, else the class's), ammunition, weapon
/// column, category; the splash's (the launcher's or the class's) friendly factor bits and category; combat.yaml's
/// dead food share bits; the class has a shot effect.
pub const EE_COMBAT_W_HIT_CLASS: i32 = 43;
/// `object->GetEffect(3, e)`: false for none; else plays `e` on it (when not NULL) and answers true.
pub const EE_COMBAT_W_UNIT_EFFECT: i32 = 44;
/// `object->TakeHit(other, args[0], other's player id, 1)`.
pub const EE_COMBAT_W_TAKE_HIT: i32 = 45;
/// `object->mUnitClass->UnitDamaged(object->GetComplexUnit(), other, args[0])`.
pub const EE_COMBAT_W_UNIT_DAMAGED: i32 = 46;
/// `object->mExperience++`.
pub const EE_COMBAT_W_EXPERIENCE: i32 = 47;
/// `object->mResourceAmount = args[0]`.
pub const EE_COMBAT_W_SET_RESOURCE: i32 = 48;
/// The first unit node of tile (`args[0]`, `args[1]`), 0 for none or no tile.
pub const EE_COMBAT_W_TILE_UNITS: i32 = 49;
/// The node after node `object`.
pub const EE_COMBAT_W_NODE_NEXT: i32 = 50;
/// The complex unit of node `object`'s unit.
pub const EE_COMBAT_W_NODE_UNIT: i32 = 51;
/// TEST ONLY (CBAD, CBXA): `ApplyAreaDamage` of `object` with target `other`: `args[1]` victims at `args[0]` (a
/// `const u64*`), the tile `args[2..4]`, the radius bits `args[4]`, the friendly factor above 0 `args[5]`.
pub const EE_COMBAT_W_AREA_CAPTURE: i32 = 52;
/// TEST ONLY (CBXA): `ApplyHit` of `object` on `other` ends.
pub const EE_COMBAT_W_HIT_CAPTURE: i32 = 53;

const TYPE_VOLCANO_PROJECTILE: u32 = 0x16f;
const CATEGORY_LAND: u32 = 5;
const CATEGORY_LAND_CITIZEN: u32 = 6;
const RESOURCE_FOOD: u32 = 2;

fn read<H: WorldHost>(h: &mut H, code: i32, object: EeCombatHandle) -> u32 {
    h.read(code, object, 0)
}

fn op1<H: WorldHost>(h: &mut H, code: i32, object: EeCombatHandle, other: EeCombatHandle, arg: i64) -> u64 {
    let mut args = [0; 12];
    args[0] = arg;
    h.op(code, object, other, &mut args)
}

/// One hit's damage arguments (`ComputeDamage`'s after the units).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeCombatDamageCall {
    /// `DAMAGE_DIRECT`, `DAMAGE_SPLASH` or `DAMAGE_TRAMPLE`
    pub form: i32,
    pub attack: f32,
    /// the attack's ammunition kind (the to-hit table's column) and weapon (the armour's index)
    pub ammo: i32,
    pub weapon: i32,
    /// splash on the own player's units
    pub friendly_factor: f32,
    /// out: the damage when the call answers true
    pub amount: i32,
}

/// `EEUnitClass::ComputeDamage(form, unit, target, ...)`: the record (the C++'s tables, stats and rules; the players,
/// the herd from the store; the elevation and the fanatic only for a damaging hit, as the original read them), the
/// formula; the damage when the hit does any.
pub fn compute_damage<H: WorldHost>(
    h: &mut H,
    unit: EeCombatHandle,
    target: EeCombatHandle,
    call: &EeCombatDamageCall,
) -> Option<i32> {
    let mut i = EeCombatDamageIn { form: call.form, attack: call.attack, ..Default::default() };
    let mut args = [0; 12];
    args[0] = &mut i as *mut EeCombatDamageIn as i64;
    args[1] = i64::from(call.ammo);
    args[2] = i64::from(call.weapon);
    h.op(EE_COMBAT_W_DAMAGE_IN, unit, target, &mut args);
    i.elevation = 1.0;
    let (player, other) = (h.handle(EE_COMBAT_H_PLAYER, unit, 0), h.handle(EE_COMBAT_H_PLAYER, target, 0));
    i.same_player = i32::from(other == player);
    // `inUnit->mPlayer->mEnemies[inTarget->mPlayer->GetPlayerID()] == 0`
    let id = read(h, EE_COMBAT_Q_PLAYER_ID, other);
    i.allied = i32::from(h.read(EE_COMBAT_Q_ENEMY, player, id as i32) == 0);
    i.herd = i32::from(read(h, EE_COMBAT_Q_HERD, unit) != 0);
    i.friendly_factor = call.friendly_factor;
    if call.form != DAMAGE_TRAMPLE && hit_factor(&i) > 0.0 {
        let complex = h.handle(EE_COMBAT_H_COMPLEX, target, 0);
        i.elevation = f32::from_bits(h.op(EE_COMBAT_W_ELEVATION, unit, complex, &mut [0; 12]) as u32);
        let owner = h.handle(EE_COMBAT_H_OWNER, unit, 0);
        let fanatic = if owner != 0 { owner } else { unit };
        // `IsCloaked()`: `mCloaked > 0` (an unsigned long)
        i.fanatic = i32::from(call.form == DAMAGE_DIRECT && read(h, EE_COMBAT_Q_CLOAK_TIMER, fanatic) != 0);
    }
    let mut out: EeCombatDamageOut = damage(&i);
    let mut args = [0; 12];
    args[0] = &mut i as *mut EeCombatDamageIn as i64;
    args[1] = &mut out as *mut EeCombatDamageOut as i64;
    h.op(EE_COMBAT_W_DAMAGE_CAPTURE, unit, target, &mut args);
    (out.hit != 0).then_some(out.amount)
}

/// ApplyHit's classes (see [`EE_COMBAT_W_HIT_CLASS`]).
#[derive(Clone, Copy, Debug, Default, PartialEq)]
struct HitClass {
    attack: f32,
    radius: f32,
    ammo: i32,
    weapon: i32,
    category: u32,
    friendly_factor: f32,
    splash_category: u32,
    dead_food_share: f32,
    shot_effect: bool,
}

fn hit_class<H: WorldHost>(h: &mut H, unit: EeCombatHandle) -> HitClass {
    let mut a = [0; 12];
    h.op(EE_COMBAT_W_HIT_CLASS, unit, 0, &mut a);
    let real = |v: i64| f32::from_bits(v as u32);
    HitClass {
        attack: real(a[0]),
        radius: real(a[1]),
        ammo: a[2] as i32,
        weapon: a[3] as i32,
        category: a[4] as u32,
        friendly_factor: real(a[5]),
        splash_category: a[6] as u32,
        dead_food_share: real(a[7]),
        shot_effect: a[8] != 0,
    }
}

/// The volcano projectile's hit only plays the victim's effect 3 (when it has one).
fn volcano_effect<H: WorldHost>(h: &mut H, unit: EeCombatHandle, victim: EeCombatHandle) -> bool {
    read(h, EE_COMBAT_Q_UNIT_TYPE, unit) == TYPE_VOLCANO_PROJECTILE
        && victim != 0
        && h.op(EE_COMBAT_W_UNIT_EFFECT, victim, 0, &mut [0; 12]) != 0
}

/// A hunted animal just killed keeps `share` of its food (`round` the rounding): not after a citizen's attack, only
/// a land unit with food that had hit points before and has none now.
fn hunted_food<H: WorldHost>(
    h: &mut H,
    category: u32,
    victim: EeCombatHandle,
    before: u32,
    share: f32,
    round: fn(f32) -> i32,
) {
    if category == CATEGORY_LAND_CITIZEN
        || victim == 0
        || before == 0
        || read(h, EE_COMBAT_Q_HIT_POINTS, victim) != 0
        || read(h, EE_COMBAT_Q_CATEGORY, victim) != CATEGORY_LAND
        || read(h, EE_COMBAT_Q_RESOURCE_TYPE, victim) != RESOURCE_FOOD
    {
        return;
    }
    let amount = read(h, EE_COMBAT_Q_RESOURCE_AMOUNT, victim) as i32;
    let remaining = round(amount as f32 * share);
    if amount != remaining {
        op1(h, EE_COMBAT_W_SET_RESOURCE, victim, 0, i64::from(remaining));
    }
}

/// `ApplyAreaDamage`'s world: the walk's reads and each victim's hit.
struct AreaWorld<'a, H: WorldHost> {
    h: &'a mut H,
    unit: EeCombatHandle,
    class: HitClass,
    victims: Vec<u64>,
}

impl<H: WorldHost> AreaHost for AreaWorld<'_, H> {
    fn tile_first(&mut self, x: i32, y: i32) -> EeCombatHandle {
        let mut a = [0; 12];
        a[0] = i64::from(x);
        a[1] = i64::from(y);
        self.h.op(EE_COMBAT_W_TILE_UNITS, 0, 0, &mut a)
    }
    fn node_next(&mut self, node: EeCombatHandle) -> EeCombatHandle {
        self.h.op(EE_COMBAT_W_NODE_NEXT, node, 0, &mut [0; 12])
    }
    fn node_unit(&mut self, node: EeCombatHandle) -> EeCombatHandle {
        self.h.op(EE_COMBAT_W_NODE_UNIT, node, 0, &mut [0; 12])
    }
    fn distance_squared(&mut self, unit: EeCombatHandle) -> f32 {
        f32::from_bits(read(self.h, EE_COMBAT_Q_DISTANCE_SQUARED, unit))
    }
    fn targetable(&mut self, unit: EeCombatHandle) -> bool {
        self.h.read(EE_COMBAT_Q_TARGETABLE, unit, 1) != 0
    }
    fn same_player(&mut self, unit: EeCombatHandle) -> bool {
        self.h.handle(EE_COMBAT_H_PLAYER, unit, 0) == self.h.handle(EE_COMBAT_H_PLAYER, self.unit, 0)
    }
    /// One victim: the volcano's effect only, else the splash damage, the kill count and a hunted animal's food.
    fn hit(&mut self, victim: EeCombatHandle) {
        self.victims.push(victim);
        let (h, unit, c) = (&mut *self.h, self.unit, self.class);
        if volcano_effect(h, unit, victim) {
            return;
        }
        let call = EeCombatDamageCall {
            form: DAMAGE_SPLASH,
            attack: c.attack,
            ammo: c.ammo,
            weapon: c.weapon,
            friendly_factor: c.friendly_factor,
            amount: 0,
        };
        let Some(amount) = compute_damage(h, unit, victim, &call) else { return };
        let before = read(h, EE_COMBAT_Q_HIT_POINTS, victim);
        op1(h, EE_COMBAT_W_TAKE_HIT, victim, unit, i64::from(amount));
        if (read(h, EE_COMBAT_Q_HIT_POINTS, victim) as i32) <= 0 {
            h.op(EE_COMBAT_W_EXPERIENCE, unit, 0, &mut [0; 12]);
        }
        hunted_food(h, c.splash_category, victim, before, c.dead_food_share, ceil);
    }
}

/// `EEUnitClass::ApplyAreaDamage(unit, radius, ...)`: every victim of the walk (walks.rs `area_victims`) around the
/// unit's tile takes its splash hit; `target` (its complex unit) counts as hit already.
fn area_damage<H: WorldHost>(h: &mut H, unit: EeCombatHandle, class: HitClass, target: EeCombatHandle) {
    let (x, y) = (read(h, EE_COMBAT_Q_TILE_X, unit) as i32, read(h, EE_COMBAT_Q_TILE_Y, unit) as i32);
    let complex = if target != 0 { h.handle(EE_COMBAT_H_COMPLEX, target, 0) } else { 0 };
    let input = EeCombatAreaIn {
        tile_x: x,
        tile_y: y,
        radius: class.radius,
        friendly_factor: class.friendly_factor,
        target: complex,
    };
    let mut world = AreaWorld { h: &mut *h, unit, class, victims: Vec::new() };
    area_victims(&input, &mut world);
    let victims = world.victims;
    let mut a = [0; 12];
    a[0] = victims.as_ptr() as i64;
    a[1] = victims.len() as i64;
    a[2] = i64::from(x);
    a[3] = i64::from(y);
    a[4] = i64::from(class.radius.to_bits());
    a[5] = i64::from(class.friendly_factor > 0.0);
    h.op(EE_COMBAT_W_AREA_CAPTURE, unit, target, &mut a);
}

/// `EEUnitClass::ApplyHit(unit, target)` (the class: the host's): the volcano projectile only plays the target's
/// effect; else the splash (an area effect), then the direct damage, the target's reaction (`UnitDamaged`, unless
/// the class has a shot effect), the kill count and a hunted animal's food.
pub fn apply_hit<H: WorldHost>(h: &mut H, unit: EeCombatHandle, target: EeCombatHandle) {
    hit(h, unit, target);
    h.op(EE_COMBAT_W_HIT_CAPTURE, unit, target, &mut [0; 12]);
}

fn hit<H: WorldHost>(h: &mut H, unit: EeCombatHandle, target: EeCombatHandle) {
    let complex = if target != 0 { h.handle(EE_COMBAT_H_COMPLEX, target, 0) } else { 0 };
    if volcano_effect(h, unit, complex) {
        return;
    }
    let class = hit_class(h, unit);
    if class.radius != 0.0 {
        area_damage(h, unit, class, target);
    }
    if target == 0 {
        return;
    }
    let before = read(h, EE_COMBAT_Q_HIT_POINTS, target);
    let call = EeCombatDamageCall {
        form: DAMAGE_DIRECT,
        attack: class.attack,
        ammo: class.ammo,
        weapon: class.weapon,
        friendly_factor: 0.0,
        amount: 0,
    };
    let Some(amount) = compute_damage(h, unit, target, &call) else { return };
    op1(h, EE_COMBAT_W_TAKE_HIT, target, unit, i64::from(amount));
    if complex != 0 && !class.shot_effect {
        op1(h, EE_COMBAT_W_UNIT_DAMAGED, target, unit, i64::from(amount));
    }
    if (read(h, EE_COMBAT_Q_HIT_POINTS, target) as i32) <= 0 {
        h.op(EE_COMBAT_W_EXPERIENCE, unit, 0, &mut [0; 12]);
    }
    if amount > 0 {
        hunted_food(h, class.category, complex, before, class.dead_food_share, lrintf);
    }
}

// ---- the C ABI

/// `ComputeDamage` of `unit` on `target` over `host` (the trample's): true with `call.amount` set when it does damage.
///
/// # Safety
/// `call` must point to a writable record, `host` to a valid table whose callbacks may be called with its context.
#[no_mangle]
pub unsafe extern "C" fn ee_combat_compute_damage(
    call: *mut EeCombatDamageCall,
    unit: u64,
    target: u64,
    host: *const EeCombatWorldHost,
) -> bool {
    let mut world = unsafe { *host };
    let call = unsafe { &mut *call };
    match compute_damage(&mut Stored::new(&mut world), unit, target, call) {
        Some(amount) => {
            call.amount = amount;
            true
        }
        None => false,
    }
}

#[cfg(test)]
#[path = "damage_tests.rs"]
mod tests;
