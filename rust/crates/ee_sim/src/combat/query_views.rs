//! The combat query host's plain reads over the world views (the units' round 1, branch rust-units1): each
//! `EE_COMBAT_Q_*` read that is one member of the object, its class, the world, the map or the physics is answered
//! here in place; the C++ host (EECombatGoals.cpp `EEQueryRead`) keeps the reads that call a method, read the
//! call's own unit or goal, or reach an object the views do not cover. `EeCombatQueryHost::read` asks here first.

// `c_ulong` and `c_long` are 32 bits on Windows and 64 elsewhere: the casts are needed there
#![allow(clippy::unnecessary_cast)]

use crate::combat::damage::EE_COMBAT_Q_RESOURCE_TYPE;
use crate::combat::leaves::{
    EE_COMBAT_Q_ACTIVE_GOAL_KIND, EE_COMBAT_Q_INNER_SPHERE, EE_COMBAT_Q_SHOOTS_HIGH, EE_COMBAT_Q_TOWER,
};
use crate::combat::reads::{
    EE_COMBAT_Q_ATTACK_GOAL, EE_COMBAT_Q_COLLISION_RADIUS, EE_COMBAT_Q_FIRING_HIGH, EE_COMBAT_Q_HEAL_RANGE,
    EE_COMBAT_Q_NONE, EE_COMBAT_Q_SPECIAL_ATTACK, EE_COMBAT_Q_WEAPON_TAG,
};
use crate::combat::world::{
    EE_COMBAT_Q_CLASS_MAX_VELOCITY, EE_COMBAT_Q_GAME_TIME, EE_COMBAT_Q_MAP_SIZE, EE_COMBAT_Q_TILE_SIZE,
};
use crate::world::view::{UnitView, WorldRead, WorldView};
use crate::world_field;

/// `kGoalAttackUnit` (`EEAttackUnitCast`: a goal of that kind)
const GOAL_ATTACK_UNIT: u8 = 2;
/// `AMMUNITION`'s code (world.rs's)
use crate::combat::world::EE_COMBAT_Q_AMMUNITION;

/// The read `code` of `object` (an `EEUnit*`; the complex members' only from a complex unit), None for one the C++
/// host answers.
pub fn query_read(t: &WorldRead, code: i32, object: u64) -> Option<u32> {
    crate::world::layout::get()?;
    let world = || WorldView::global(t);
    // the reads of the world alone
    match code {
        EE_COMBAT_Q_GAME_TIME => return Some(world()?.game_time() as u32),
        EE_COMBAT_Q_MAP_SIZE => return Some(world()?.map()?.size() as u32),
        _ => {}
    }
    // SAFETY: the host's objects are live units for the call
    let u = unsafe { UnitView::of(t, object) }?;
    if !u.is_unit() {
        return None;
    }
    let class = u.class()?;
    let cf = class.fields();
    let b = u32::from;
    if !u.is_complex() {
        // only the EEUnit and class reads
        return Some(match code {
            EE_COMBAT_Q_HEAL_RANGE => world_field!(cf, mHealRange).to_bits(),
            EE_COMBAT_Q_TILE_SIZE => class.tile_size() as u32,
            EE_COMBAT_Q_CLASS_MAX_VELOCITY => world_field!(cf, mMaxVelocity).to_bits(),
            EE_COMBAT_Q_RESOURCE_TYPE => u.resource_type() as u32,
            EE_COMBAT_Q_INNER_SPHERE => world_field!(cf, mInnerCollisionSphere).to_bits(),
            EE_COMBAT_Q_SHOOTS_HIGH => b(world_field!(cf, mShootsHigh)),
            EE_COMBAT_Q_TOWER => b(world_field!(cf, mIsTower)),
            _ => return None,
        });
    }
    let c = u.complex();
    Some(match code {
        EE_COMBAT_Q_FIRING_HIGH => b(world_field!(c, mIsFiringHigh)),
        EE_COMBAT_Q_SPECIAL_ATTACK => b(world_field!(c, mIsSpecialAttack)),
        EE_COMBAT_Q_HEAL_RANGE => world_field!(cf, mHealRange).to_bits(),
        EE_COMBAT_Q_COLLISION_RADIUS => world_field!(c, mCollisionRadius).to_bits(),
        EE_COMBAT_Q_WEAPON_TAG => world_field!(c, mWeaponTagPoint) as u32,
        EE_COMBAT_Q_ATTACK_GOAL => b(u.goal().is_some_and(|g| g.0 == GOAL_ATTACK_UNIT)),
        EE_COMBAT_Q_TILE_SIZE => class.tile_size() as u32,
        EE_COMBAT_Q_CLASS_MAX_VELOCITY => world_field!(cf, mMaxVelocity).to_bits(),
        EE_COMBAT_Q_AMMUNITION => world_field!(c, mAmmunition) as u32,
        EE_COMBAT_Q_RESOURCE_TYPE => u.resource_type() as u32,
        EE_COMBAT_Q_INNER_SPHERE => world_field!(cf, mInnerCollisionSphere).to_bits(),
        EE_COMBAT_Q_SHOOTS_HIGH => b(world_field!(cf, mShootsHigh)),
        EE_COMBAT_Q_TOWER => b(world_field!(cf, mIsTower)),
        EE_COMBAT_Q_ACTIVE_GOAL_KIND => u.active_goal().map_or(EE_COMBAT_Q_NONE, |g| u32::from(g.0)),
        _ => return None,
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn without_a_layout_every_read_is_the_hosts() {
        // SAFETY: nothing is read without a layout
        let t = unsafe { WorldRead::begin() };
        assert_eq!(query_read(&t, EE_COMBAT_Q_GAME_TIME, 0), None);
        assert_eq!(query_read(&t, EE_COMBAT_Q_FIRING_HIGH, 0x1000), None);
    }
}
