//! The combat and unit formulas (docs/RUST_PLAN.md R5b.2), ported from the C++ unit methods that call them
//! (`include/ee/EECombatFormulas.h` lists each caller): the damage of a hit (`EEUnitClass::ComputeDamage`), the
//! attack factor's difficulty scale (`EEToHitTable::GetAttackFactor`), the elevation factor, morale, a hit's hit
//! points, regeneration and healing, conversion, the ballistic pitch (`EEGetBallisticPitch`), the projectile's arc
//! (`EEPMProjectile::Step0`), the range and reach tests and the damage table rows.
//!
//! The C++ gathers every input from the world into a record (`EeCombat*In`) and acts on the result; nothing here
//! reads the world, draws a random number or keeps state. Every operation is the C++'s, in `f32` and in its order
//! (RUST_PLAN 2.1): `lrintf` for `UMFloatToLong`, `(long)` casts as x86 truncation, `long` arithmetic wrapping, the
//! square roots through `f64`, the engine's table trigonometry from `ee_ge`.

// `!(a > b)` is the C++'s NaN-aware test, kept as written
#![allow(clippy::neg_cmp_op_on_partial_ord)]

pub mod capture;
pub mod damage_table;
pub mod decide;
pub mod ffi;
pub mod flight;
pub mod geometry;
pub mod impact;
pub mod launch;
pub mod pursuer;
pub mod screen;
pub mod search;
pub mod search_ffi;
#[cfg(test)]
mod search_tests;
#[cfg(test)]
mod tests;
pub mod walks;
pub mod walks_ffi;
#[cfg(test)]
mod walks_tests;

use ee_ge::um::{lrintf, sqrt, um_arccos, um_arctan, um_cos, HALF_PI};

/// `kDamageDirect`, `kDamageSplash`, `kDamageTrample` (`ee/EERules.h`).
pub const DAMAGE_DIRECT: i32 = 0;
pub const DAMAGE_SPLASH: i32 = 1;
pub const DAMAGE_TRAMPLE: i32 = 2;

/// The columns of a damage table row (the attack kinds).
pub const DAMAGE_COLUMNS: usize = 68;

/// `kUUContactMargin`, `kUUWorkReach`, `kUUGatherReach` (0x449b7c..0x449b84).
pub const CONTACT_MARGIN: f32 = 1.42;
pub const WORK_REACH: f32 = 1.5;
pub const GATHER_REACH: f32 = 1.42;

/// `k447ADC`: 1 / sqrt(6), the elevation thresholds' scale.
pub const ELEVATION_SCALE: f32 = 0.40824828;

/// `EEToHitTable::GetAttackFactor`'s inputs.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeCombatAttackFactorIn {
    /// `gDamageTable[the target class's family][the attack kind]`
    pub factor: f32,
    /// the target class `CanBeAttacked`
    pub attackable: i32,
    /// both players exist
    pub players_present: i32,
    /// `IsHuman` and not player 0
    pub attacker_human: i32,
    pub target_human: i32,
    /// the game's difficulty has a gameoptions.yaml rule
    pub has_rule: i32,
    /// that rule's scales (0 without one)
    pub scale_human_target: f32,
    pub scale_human_attacker: f32,
}

/// `EEUnitClass::ComputeDamage`'s inputs.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeCombatDamageIn {
    /// `DAMAGE_DIRECT`, `DAMAGE_SPLASH` or `DAMAGE_TRAMPLE`
    pub form: i32,
    pub attack: f32,
    /// `EEToHitTable::GetAttackFactor` (0, unread, for a direct hit without attack)
    pub attack_factor: f32,
    /// `gWeaponToHitFactors[the target class's weapon][the attack's weapon]`
    pub weapon_factor: f32,
    /// the target's armour of the attack's weapon
    pub armor: f32,
    /// `GetElevationFactor` (1 unless the C++ filled it: not a trample, a positive hit factor)
    pub elevation: f32,
    /// a direct hit by a cloaked attacker (or owner)
    pub fanatic: i32,
    pub same_player: i32,
    /// the attacker's player is not an enemy of the target's
    pub allied: i32,
    /// the attacker is a herd animal
    pub herd: i32,
    /// splash on the own player's units
    pub friendly_factor: f32,
    /// combat.yaml
    pub minimum_damage: i32,
    pub fanatic_factor: f32,
    pub elevation_step: f32,
    pub ally_factor: f32,
    pub herd_ally_factor: f32,
    pub trample_damage: i32,
    pub trample_threshold: f32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeCombatDamageOut {
    /// the hit does damage
    pub hit: i32,
    pub amount: i32,
}

/// `EEUnitClass::GetElevationFactor`'s inputs.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeCombatElevationIn {
    /// a combat.yaml elevation attacker against an elevation target
    pub applies: i32,
    /// the attacker's height (its launch height for a projectile)
    pub own_z: f32,
    pub target_z: f32,
    /// dbworld high_ground_height, low_ground_height (scaled by `ELEVATION_SCALE`)
    pub high_height: f32,
    pub low_height: f32,
    /// the player's high ground bonus
    pub high_bonus: f32,
    /// dbworld high_ground_attack_factor, low_ground_attack_factor
    pub high_factor: f32,
    pub low_factor: f32,
}

/// `EEComplexUnit::ApplyMorale`'s inputs.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeCombatMoraleIn {
    pub value: i32,
    /// the unit's positive and negative morale counts
    pub positive: i32,
    pub negative: i32,
    /// combat.yaml
    pub positive_share: f32,
    pub negative_share: f32,
}

/// `EEUnit::TakeHit`'s hit points.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeCombatHitPointsIn {
    pub hit_points: i32,
    pub damage: i32,
    pub max_hit_points: i32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeCombatHitPointsOut {
    /// at least 0
    pub hit_points: i32,
    /// the hit crossed a tenth of the maximum
    pub badly_hurt: i32,
}

/// `EEUnit::TakeHit`: whether the hit crosses the class's damage effect threshold.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeCombatDamageEffectIn {
    pub old_hit_points: i32,
    pub hit_points: i32,
    pub max_hit_points: i32,
    /// the effect's second parameter (a share of the maximum)
    pub threshold: f32,
}

/// `EEUnitClass::Regenerate`'s inputs.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeCombatRegenerateIn {
    pub timer: i32,
    pub tick_ms: i32,
    pub hit_points: i32,
    pub regeneration: i32,
    pub max_hit_points: i32,
    /// combat.yaml regeneration_interval
    pub interval: i32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeCombatRegenerateOut {
    pub timer: i32,
    pub hit_points: i32,
    /// the hit points change (a living unit)
    pub set: i32,
}

/// `EEUnitClass::HealGarrison`'s timer.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeCombatTimerIn {
    pub timer: i32,
    pub tick_ms: i32,
    pub interval: i32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeCombatTimerOut {
    pub timer: i32,
    pub due: i32,
}

/// `EEUnitClass::HealGarrison` per unit, `EEUGHealUnit::Work`.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeCombatHealIn {
    pub hit_points: i32,
    pub healing: i32,
    pub max_hit_points: i32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeCombatHealOut {
    /// at most the maximum
    pub hit_points: i32,
    /// a living unit
    pub set: i32,
}

/// `EEUGHealUnit::Work`'s clock.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeCombatHealStepIn {
    pub elapsed: u32,
    pub tick_ms: i32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeCombatHealStepOut {
    pub elapsed: u32,
    pub due: i32,
}

/// `EEGetConversionCost`'s inputs.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeCombatConversionCostIn {
    /// dbworld conversion_faith_cost
    pub cost: i32,
    /// the unit's maximum power
    pub power: i32,
}

/// `EEUCBasicPriest::UpdateConversion`'s (and `EEUCBuildingPriest`'s) inputs.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeCombatConversionIn {
    pub work_timer: i32,
    pub tick_seconds: f32,
    /// the target's conversion resistance
    pub resistance: f32,
    /// the adaptation power lowers a town centre's full resistance (the C++ fills it at resistance 1 only)
    pub adaptation: i32,
    /// the priest's conversion rate
    pub rate: f32,
    /// the target player's conversion factor
    pub factor: f32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeCombatConversionOut {
    /// the work timer after this tick (no attempt)
    pub timer: i32,
    /// a second has gone: one attempt
    pub due: i32,
    /// the attempt converts when the draw is at most this
    pub threshold: f32,
    /// the work timer after a failed attempt
    pub miss_timer: i32,
}

/// `EEGetBallisticPitch`'s inputs.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeCombatBallisticIn {
    pub from_x: f32,
    pub from_y: f32,
    pub from_z: f32,
    pub to_x: f32,
    pub to_y: f32,
    pub to_z: f32,
    /// the launch speed
    pub speed: f32,
    /// the projectile class's gravity
    pub acceleration: f32,
    /// the high arc (the class shoots high, or fires over walls high in animation 8)
    pub high: i32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeCombatBallisticOut {
    pub reached: i32,
    pub angle: f32,
}

/// `EEPMProjectile::Step0`'s arc.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeCombatArcIn {
    pub flight_time_ms: i32,
    pub gravity: f32,
    pub pitch: f32,
    /// flies straight at `max_velocity`, no gravity
    pub meteor: i32,
    pub max_velocity: f32,
    /// the launcher class's launch speed (else the projectile's own)
    pub launcher_speed: f32,
    pub own_speed: f32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeCombatArcOut {
    /// the move from the launch point in the yaw's frame (x is 0)
    pub move_y: f32,
    pub move_z: f32,
    /// the nose along the arc
    pub pitch: f32,
}

/// `EEIsInRangeToHit`'s distance test.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeCombatRangeIn {
    pub range: f32,
    /// a crusader's range
    pub heal_range: f32,
    pub crusader: i32,
    /// `EEGetHitDistanceSquared`
    pub distance_squared: f32,
    /// the target is a land building: half its inner sphere counts
    pub building: i32,
    pub inner_sphere: f32,
}

/// `EECanReach`, `EEIsInWorkReach`, `EEIsInGatherReach`.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeCombatReachIn {
    pub distance_squared: f32,
    pub radius: f32,
    pub other_radius: f32,
    /// `EECanReach`'s scale (0 for the work and gather tests)
    pub scale: f32,
    /// work: the target is a wall; gather: the target is a tree
    pub flag_a: i32,
    /// work: a robotic farm; gather: the unit is a naval citizen
    pub flag_b: i32,
}

/// One row of the damage tables (`EETerrainTypeTable::LoadDamageTables`).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct EeCombatDamageRowIn {
    pub percent: [i32; DAMAGE_COLUMNS],
    /// percent to fraction (`kHeightEpsilon`)
    pub scale: f32,
    /// dbworld difficulty_damage_scale F, F2
    pub twin1: f32,
    pub twin2: f32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct EeCombatDamageRowOut {
    pub base: [f32; DAMAGE_COLUMNS],
    pub scaled1: [f32; DAMAGE_COLUMNS],
    pub scaled2: [f32; DAMAGE_COLUMNS],
}

// every record is 4-byte fields without padding: the capture's layout
const _: () = assert!(std::mem::size_of::<EeCombatAttackFactorIn>() == 32);
const _: () = assert!(std::mem::size_of::<EeCombatDamageIn>() == 72);
const _: () = assert!(std::mem::size_of::<EeCombatElevationIn>() == 32);
const _: () = assert!(std::mem::size_of::<EeCombatMoraleIn>() == 20);
const _: () = assert!(std::mem::size_of::<EeCombatConversionIn>() == 24);
const _: () = assert!(std::mem::size_of::<EeCombatBallisticIn>() == 36);
const _: () = assert!(std::mem::size_of::<EeCombatArcIn>() == 28);
const _: () = assert!(std::mem::size_of::<EeCombatRangeIn>() == 24);
const _: () = assert!(std::mem::size_of::<EeCombatReachIn>() == 24);
const _: () = assert!(std::mem::size_of::<EeCombatDamageRowIn>() == 284);
const _: () = assert!(std::mem::size_of::<EeCombatDamageRowOut>() == 816);

/// `UMCeil`: round to nearest, then up one when that went down (the comparison in `f32`).
#[inline]
pub fn ceil(value: f32) -> i32 {
    let result = lrintf(value);
    if (result as f32) < value {
        result.wrapping_add(1)
    } else {
        result
    }
}

/// A C++ `(long)` cast of a float on x86-64: truncation, and the conversion's "indefinite" value for NaN or a value
/// out of range (where `as i32` would saturate).
#[inline]
pub fn truncate(value: f32) -> i32 {
    let truncated = value.trunc();
    if (-2_147_483_648.0..2_147_483_648.0).contains(&truncated) {
        truncated as i32
    } else {
        i32::MIN
    }
}

/// `EEToHitTable::GetAttackFactor`: the damage table's factor, scaled by the difficulty's rule when exactly one side
/// is human; 0 for a target that cannot be attacked or a missing player.
pub fn attack_factor(input: &EeCombatAttackFactorIn) -> f32 {
    if input.attackable == 0 || input.players_present == 0 {
        return 0.0;
    }
    let human = input.attacker_human != 0;
    let target_human = input.target_human != 0;
    if human == target_human || input.has_rule == 0 {
        return input.factor;
    }
    (if target_human { input.scale_human_target } else { input.scale_human_attacker }) * input.factor
}

/// The to-hit factor of a hit: the attack factor x the weapon factor, 0 for a direct hit without attack.
pub fn hit_factor(input: &EeCombatDamageIn) -> f32 {
    if input.form == DAMAGE_DIRECT && !(input.attack > 0.0) {
        return 0.0;
    }
    input.attack_factor * input.weapon_factor
}

/// `EEUnitClass::ComputeDamage`. Direct and splash: to-hit x attack (x the fanatic factor for a fanatic direct hit),
/// less the armour, moved by the elevation factor at least `elevation_step` either way, x the friendly factor for
/// splash on the own player, rounded, at least the minimum; a direct hit on an own or allied unit: the ceiling of
/// that x the ally (or herd ally) factor. Trample: `trample_damage` when to-hit x (attack less the rounded armour) is
/// above the threshold.
pub fn damage(input: &EeCombatDamageIn) -> EeCombatDamageOut {
    let none = EeCombatDamageOut { hit: 0, amount: 0 };
    let hit = hit_factor(input);
    if input.form == DAMAGE_TRAMPLE {
        let mut amount = input.attack;
        if hit > 0.0 {
            amount -= lrintf(input.armor) as f32;
        }
        if !(hit * amount > input.trample_threshold) {
            return none;
        }
        return EeCombatDamageOut { hit: 1, amount: input.trample_damage };
    }
    if hit <= 0.0 {
        return none;
    }
    let mut amount = hit * input.attack;
    if input.form == DAMAGE_DIRECT && input.fanatic != 0 {
        amount *= input.fanatic_factor;
    }
    amount -= input.armor;
    let modifier = input.elevation;
    if modifier != 1.0 {
        let step = input.elevation_step;
        if modifier > 1.0 {
            let scaled = modifier * amount;
            amount = if scaled >= amount + step { scaled } else { amount + step };
        } else if modifier < 1.0 {
            let scaled = modifier * amount;
            amount = if scaled <= amount - step { scaled } else { amount - step };
        }
    }
    if input.form == DAMAGE_SPLASH && input.same_player != 0 {
        amount *= input.friendly_factor;
    }
    let mut rounded = lrintf(amount);
    if rounded < input.minimum_damage {
        rounded = input.minimum_damage;
    }
    if input.form == DAMAGE_DIRECT && (input.same_player != 0 || input.allied != 0) {
        let factor = if input.herd != 0 { input.herd_ally_factor } else { input.ally_factor };
        rounded = ceil(rounded as f32 * factor);
    }
    EeCombatDamageOut { hit: 1, amount: rounded }
}

/// `EEUnitClass::GetElevationFactor`: the high ground factor (plus the player's bonus) at least `high_height` x
/// 1/sqrt(6) above the target, the low ground factor as far below, else 1.
pub fn elevation(input: &EeCombatElevationIn) -> f32 {
    if input.applies == 0 {
        return 1.0;
    }
    let difference = input.own_z - input.target_z;
    if difference >= input.high_height * ELEVATION_SCALE {
        input.high_bonus + input.high_factor
    } else if difference <= -(input.low_height * ELEVATION_SCALE) {
        input.low_factor
    } else {
        1.0
    }
}

/// `EEComplexUnit::ApplyMorale`: positive morale takes its share off, negative morale adds its share.
pub fn morale(input: &EeCombatMoraleIn) -> i32 {
    let mut value = input.value;
    if input.positive > 0 {
        value = value.wrapping_sub(lrintf(input.value as f32 * input.positive_share));
    }
    if input.negative > 0 {
        value = value.wrapping_add(lrintf(input.value as f32 * input.negative_share));
    }
    value
}

/// `EEUnit::TakeHit`: the hit points after the damage (at least 0), and whether the hit took the unit across a
/// tenth of its maximum (`BadlyHurt`).
pub fn hit_points(input: &EeCombatHitPointsIn) -> EeCombatHitPointsOut {
    let threshold = input.max_hit_points as f32 * 0.1;
    let after = input.hit_points.wrapping_sub(input.damage);
    let badly_hurt = input.hit_points as f32 > threshold && after as f32 <= threshold;
    EeCombatHitPointsOut { hit_points: after.max(0), badly_hurt: badly_hurt as i32 }
}

/// `EEUnit::TakeHit`'s damage effect: the share of the maximum was at least the threshold before the hit and is
/// below it after.
pub fn damage_effect(input: &EeCombatDamageEffectIn) -> i32 {
    let inverse = 1.0 / input.max_hit_points as f32;
    (input.old_hit_points as f32 * inverse >= input.threshold && input.threshold > input.hit_points as f32 * inverse)
        as i32
}

/// `EEUnitClass::Regenerate`: once per interval a living unit gains its regeneration, up to its maximum.
pub fn regenerate(input: &EeCombatRegenerateIn) -> EeCombatRegenerateOut {
    let mut out =
        EeCombatRegenerateOut { timer: input.timer.wrapping_sub(input.tick_ms), hit_points: input.hit_points, set: 0 };
    if out.timer <= 0 {
        let healed = input.regeneration.wrapping_add(input.hit_points).min(input.max_hit_points);
        if input.hit_points > 0 && healed > 0 {
            out.hit_points = healed;
            out.set = 1;
        }
        out.timer = input.interval;
    }
    out
}

/// `EEUnitClass::HealGarrison`'s timer: due when it runs out, then restarted at the interval.
pub fn timer(input: &EeCombatTimerIn) -> EeCombatTimerOut {
    let left = input.timer.wrapping_sub(input.tick_ms);
    if left <= 0 {
        EeCombatTimerOut { timer: input.interval, due: 1 }
    } else {
        EeCombatTimerOut { timer: left, due: 0 }
    }
}

/// A heal (`HealGarrison`, `EEUGHealUnit::Work`): the healing added, up to the maximum; `set` for a living unit.
pub fn heal(input: &EeCombatHealIn) -> EeCombatHealOut {
    EeCombatHealOut {
        hit_points: input.healing.wrapping_add(input.hit_points).min(input.max_hit_points),
        set: (input.hit_points > 0) as i32,
    }
}

/// `EEUGHealUnit::Work`'s clock: the tick (at least 1 ms) added; due past a full second, which it then takes off.
pub fn heal_step(input: &EeCombatHealStepIn) -> EeCombatHealStepOut {
    let tick = if input.tick_ms == 0 { 1 } else { input.tick_ms as u32 };
    let elapsed = input.elapsed.wrapping_add(tick);
    if elapsed > 1000 {
        EeCombatHealStepOut { elapsed: elapsed - 1000, due: 1 }
    } else {
        EeCombatHealStepOut { elapsed, due: 0 }
    }
}

/// `EEGetConversionCost`: the world's faith cost, at most the unit's power, 0 if that is not positive.
pub fn conversion_cost(input: &EeCombatConversionCostIn) -> i32 {
    input.cost.min(input.power).max(0)
}

/// `UpdateConversion`'s clock and chance: the work timer runs on by the tick (`(long)(tick x -1000)`, truncated);
/// at a second or more (as unsigned) an attempt is due, which converts when the draw is at most
/// (1 - resistance) x the target player's factor x the rate (resistance 1 counts as 0.99 with adaptation).
pub fn conversion(input: &EeCombatConversionIn) -> EeCombatConversionOut {
    let time = input.work_timer.wrapping_sub(truncate(input.tick_seconds * -1000.0));
    let mut out = EeCombatConversionOut { timer: time, ..Default::default() };
    if time as u32 >= 1000 {
        let resistance = if input.resistance == 1.0 && input.adaptation != 0 { 0.99 } else { input.resistance };
        out.due = 1;
        out.threshold = (1.0 - resistance) * input.factor * input.rate;
        out.miss_timer = time.wrapping_sub(1000);
    }
    out
}

/// `EEGetBallisticPitch`: the launch angle for a shot at `speed` under `acceleration`, the low arc or the high one;
/// not reached (and pi/4) when the discriminant is negative.
pub fn ballistic(input: &EeCombatBallisticIn) -> EeCombatBallisticOut {
    let dx = input.from_x - input.to_x;
    let dy = input.from_y - input.to_y;
    let distance = sqrt(dx * dx + dy * dy);
    let speed = input.speed;
    let a = -(input.acceleration * (distance * distance)) / (speed * speed * 2.0);
    let discriminant = distance * distance - (a - (input.to_z - input.from_z)) * a * 4.0;
    if discriminant >= 0.0 {
        let root = sqrt(discriminant);
        let mut low = um_arctan((root - distance) / (a + a));
        let mut high = um_arctan((-distance - root) / (a + a));
        if low > high {
            std::mem::swap(&mut low, &mut high);
        }
        EeCombatBallisticOut { reached: 1, angle: if input.high != 0 { high } else { low } }
    } else {
        EeCombatBallisticOut { reached: 0, angle: std::f32::consts::FRAC_PI_4 }
    }
}

/// `EEPMProjectile::Step0`'s arc: the move from the launch point after the flight time (the launcher's launch speed,
/// else the projectile's own; a meteor at its top speed without gravity) and the pitch of the arc's tangent there.
pub fn arc(input: &EeCombatArcIn) -> EeCombatArcOut {
    let time = input.flight_time_ms as f32 * 0.001;
    let cos = um_cos(input.pitch);
    let sin = um_cos(HALF_PI - input.pitch);
    let (speed, gravity) = if input.meteor != 0 {
        (input.max_velocity, 0.0)
    } else if input.launcher_speed > 0.001 {
        (input.launcher_speed, input.gravity)
    } else {
        (input.own_speed, input.gravity)
    };
    let horizontal = speed * cos;
    let vertical = speed * sin;
    let move_y = horizontal * time;
    let move_z = vertical * time - (time * time) * gravity * 0.5;
    // GEVector::Normalize of (0, horizontal, the vertical speed now): only its z is read
    let mut z = vertical - time * gravity;
    let magnitude = sqrt(0.0 * 0.0 + horizontal * horizontal + z * z);
    if (-0.0001..=0.0001).contains(&magnitude) {
        z = 0.0;
    } else {
        z *= 1.0 / magnitude;
    }
    EeCombatArcOut { move_y, move_z, pitch: um_arccos(-z) - HALF_PI }
}

/// `EEIsInRangeToHit`'s distance test: within the range (a crusader's heal range), a land building's half inner
/// sphere taken off.
pub fn range(input: &EeCombatRangeIn) -> i32 {
    let reach = if input.crusader != 0 { input.heal_range * input.heal_range } else { input.range * input.range };
    if input.building == 0 {
        return (input.distance_squared <= reach) as i32;
    }
    let half = input.inner_sphere * 0.5;
    (-(half * half) + input.distance_squared <= reach) as i32
}

#[inline]
fn reaches(distance_squared: f32, first: f32, second: f32, scale: f32) -> i32 {
    let reach = (CONTACT_MARGIN * 0.05 + first + second) * scale;
    (distance_squared * 4.0 < reach * reach) as i32
}

/// `EECanReach`: two radii plus a margin, scaled, reach across the distance.
pub fn can_reach(input: &EeCombatReachIn) -> i32 {
    reaches(input.distance_squared, input.radius, input.other_radius, input.scale)
}

/// `EEIsInWorkReach`: a wall counts as radius 1, a robotic farm 2, any other target at least 1.
pub fn work_reach(input: &EeCombatReachIn) -> i32 {
    let target = if input.flag_a != 0 {
        1.0
    } else if input.flag_b != 0 {
        2.0
    } else if input.other_radius < 1.0 {
        1.0
    } else {
        input.other_radius
    };
    reaches(input.distance_squared, input.radius, target, WORK_REACH)
}

/// `EEIsInGatherReach`: the target at least the unit's radius, a tree 1, a naval citizen's own radius 1.
pub fn gather_reach(input: &EeCombatReachIn) -> i32 {
    let mut target = input.other_radius;
    if target < input.radius {
        target = input.radius;
    }
    if input.flag_a != 0 {
        target = 1.0;
    }
    let radius = if input.flag_b != 0 { 1.0 } else { input.radius };
    reaches(input.distance_squared, target, radius, GATHER_REACH)
}

/// `EEUGHealUnit::IsInRange`: the squared distance at most the range's square.
pub fn within_range(distance_squared: f32, range: f32) -> i32 {
    (distance_squared <= range * range) as i32
}

/// One damage table row: the percentages as fractions, and the two difficulty twins (scale x value).
pub fn damage_row(input: &EeCombatDamageRowIn) -> EeCombatDamageRowOut {
    let mut out = EeCombatDamageRowOut {
        base: [0.0; DAMAGE_COLUMNS],
        scaled1: [0.0; DAMAGE_COLUMNS],
        scaled2: [0.0; DAMAGE_COLUMNS],
    };
    for j in 0..DAMAGE_COLUMNS {
        out.base[j] = input.percent[j] as f32 * input.scale;
        out.scaled1[j] = input.twin1 * out.base[j];
        out.scaled2[j] = input.twin2 * out.base[j];
    }
    out
}
