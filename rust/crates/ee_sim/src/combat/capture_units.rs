//! TEST ONLY: the combat records of a unit's state (the units' lane, round 5; were EECombatGoals.cpp's
//! `EECombatCaptureShot`, `EECombatCaptureXAction`, `EECombatCaptureHostRead`, `EECombatCaptureXRead`): each call's
//! words, read through the world views, then folded (capture_fold.rs: CBSO, CBXA, CBQR, CBXR). The C++ callers
//! call these entry points through ee/EECombatGoals.h's inline members of the same names.

use crate::combat::goal_dispatch::{GOAL_ATTACK_GROUND, GOAL_ATTACK_UNIT};

/// What a shot's and an action's record read of a unit (`EEComplexUnit`'s members).
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct UnitFacts {
    pub id: u32,
    pub command: u32,
    pub animation: u32,
    pub physics_action: u32,
    pub hit_points: u32,
    /// the goal's kind (None: no goal) and, for an attack goal, its target's id (0 none); for an attack-ground goal,
    /// its point's bits
    pub goal: Option<u8>,
    pub attack_target: u32,
    pub ground: [u32; 3],
    pub location: [f32; 3],
    pub rotting: u32,
    pub flight_time: u32,
    pub cloaked: u32,
    pub opacity: f32,
    pub velocity: f32,
    pub population: u32,
    pub tile_x: u32,
    pub reload_timer: u32,
    pub weapon_tag_point: u32,
    pub ammunition: u32,
    pub special_attack: bool,
    pub firing_high: bool,
    pub experience: u32,
    pub resource_amount: u32,
    pub last_attacker: u32,
    pub target_search_timer: u32,
}

/// CBSO's 22 words (`EECombatCaptureShot(func, unit, before)`).
pub fn shot_words(func: u32, u: &UnitFacts, before: u32) -> [u32; 22] {
    let mut r = [0u32; 22];
    r[0] = func;
    r[1] = u.id;
    r[2] = before;
    r[3] = u.command;
    r[4] = u.animation;
    r[5] = u.physics_action;
    r[6] = u.hit_points;
    r[7] = u.goal.map_or(0xffff_ffff, u32::from);
    match u.goal {
        Some(GOAL_ATTACK_UNIT) => r[8] = u.attack_target,
        Some(GOAL_ATTACK_GROUND) => r[9..12].copy_from_slice(&u.ground),
        _ => {}
    }
    r[12] = u.location[0].to_bits();
    r[13] = u.location[1].to_bits();
    r[14] = u.location[2].to_bits();
    r[15] = u.rotting;
    r[16] = u.flight_time;
    r[17] = u.cloaked;
    r[18] = u.opacity.to_bits();
    r[19] = u.velocity.to_bits();
    r[20] = u.population;
    r[21] = u.tile_x;
    r
}

/// CBXA's 20 words (`EECombatCaptureXAction(func, unit, target, before)`; `target` the target's facts, if any).
pub fn action_words(func: u32, u: &UnitFacts, target: Option<&UnitFacts>, before: u32) -> [u32; 20] {
    [
        func,
        u.id,
        target.map_or(0, |t| t.id),
        before,
        u.command,
        u.animation,
        u.hit_points,
        u.reload_timer,
        u.weapon_tag_point,
        u.ammunition,
        u.flight_time,
        u32::from(u.special_attack),
        u32::from(u.firing_high),
        u.experience,
        u.goal.map_or(0xffff_ffff, u32::from),
        // EECombatShotTarget: the attack goal's target
        if u.goal == Some(GOAL_ATTACK_UNIT) { u.attack_target } else { 0 },
        target.map_or(0, |t| t.hit_points),
        target.map_or(0, |t| t.resource_amount),
        u.last_attacker,
        u.target_search_timer,
    ]
}

fn bytes(words: &[u32]) -> Vec<u8> {
    words.iter().flat_map(|w| w.to_le_bytes()).collect()
}

#[cfg(not(test))]
pub use game::ee_combat_capture_read;
#[cfg(not(test))]
pub use game::ee_combat_capture_x_action;

#[cfg(not(test))]
#[allow(clippy::unnecessary_cast)] // c_ulong and c_long are 64 bits off Windows
mod game {
    use super::*;
    use crate::combat::capture_fold::{
        fold, EE_COMBAT_FOLD_HOST_READS, EE_COMBAT_FOLD_SHOTS, EE_COMBAT_FOLD_X_ACTIONS, EE_COMBAT_FOLD_X_READS,
    };
    use crate::world::{UnitView, WorldRead};

    fn active() -> bool {
        crate::app::capture_record::active()
    }

    /// A unit's id through the views (0 for no unit).
    fn id_of(t: &WorldRead, unit: u64) -> u32 {
        // SAFETY: the C++ caller's live unit (or 0)
        unsafe { UnitView::of(t, unit) }.map_or(0, |u| u.id())
    }

    fn facts(t: &WorldRead, u: &UnitView<'_>) -> UnitFacts {
        let mut f = UnitFacts {
            id: u.id(),
            hit_points: u.hit_points() as u32,
            resource_amount: u.resource_amount() as u32,
            tile_x: u.tile_xy().0 as u32,
            ..Default::default()
        };
        if !u.is_complex() {
            return f;
        }
        let c = u.complex();
        f.command = u.command_type();
        f.animation = crate::world_field!(c, mAnimation) as u32;
        f.physics_action = crate::world_field!(c, mCurrentPhysicsAction) as u32;
        f.goal = u.goal().map(|(kind, _)| kind);
        let goal = u.goal_handle();
        match f.goal {
            Some(GOAL_ATTACK_UNIT) => f.attack_target = id_of(t, crate::combat::goal_attack::attack_of(goal).target),
            Some(GOAL_ATTACK_GROUND) => f.ground = crate::combat::goal_objects::ground_target_bits(goal),
            _ => {}
        }
        f.location = u.location();
        f.rotting = crate::world_field!(c, mRottingAwayTime) as u32;
        f.flight_time = crate::world_field!(c, mFlightTime) as u32;
        f.cloaked = crate::world_field!(c, mCloaked) as u32;
        f.opacity = crate::world_field!(c, mOpacity);
        f.velocity = crate::world_field!(c, mVelocity);
        f.population = u.player().map_or(0, |p| crate::world_field!(p.saved(), mPopulationCount) as u32);
        f.reload_timer = crate::world_field!(c, mReloadTimer) as u32;
        f.weapon_tag_point = crate::world_field!(c, mWeaponTagPoint) as u32;
        f.ammunition = crate::world_field!(c, mAmmunition) as u32;
        f.special_attack = crate::world_field!(c, mIsSpecialAttack);
        f.firing_high = crate::world_field!(c, mIsFiringHigh);
        f.experience = u32::from(crate::world_field!(c, mExperience));
        f.last_attacker = id_of(t, u.last_attacker_handle());
        f.target_search_timer = crate::world_field!(c, mTargetSearchTimer) as u32;
        f
    }

    fn with_facts(unit: u64, f: impl FnOnce(&WorldRead, UnitFacts)) {
        // SAFETY: the C++ caller holds the world; its unit is live
        let t = unsafe { WorldRead::begin() };
        // SAFETY: as above
        if let Some(u) = unsafe { UnitView::of(&t, unit) } {
            let facts = facts(&t, &u);
            f(&t, facts);
        }
    }

    /// `EECombatCaptureShot(func, unit, before)` (CBSO).
    #[no_mangle]
    pub extern "C" fn ee_combat_capture_shot(func: u32, unit: u64, before: u32) {
        if active() {
            with_facts(unit, |_, u| fold(EE_COMBAT_FOLD_SHOTS, &bytes(&shot_words(func, &u, before))));
        }
    }

    /// `EECombatCaptureXAction(func, unit, target, before)` (CBXA; `target` 0: none).
    #[no_mangle]
    pub extern "C" fn ee_combat_capture_x_action(func: u32, unit: u64, target: u64, before: u32) {
        if active() {
            with_facts(unit, |t, u| {
                // SAFETY: the caller's live target (or 0)
                let target = unsafe { UnitView::of(t, target) }.map(|v| facts(t, &v));
                fold(EE_COMBAT_FOLD_X_ACTIONS, &bytes(&action_words(func, &u, target.as_ref(), before)));
            });
        }
    }

    /// `EECombatCaptureHostRead(func, unit, target, result, extras)` (CBQR; `which` 0) and `EECombatCaptureXRead`
    /// (CBXR; 1, the first two extras).
    #[no_mangle]
    pub extern "C" fn ee_combat_capture_read(
        which: u32,
        func: u32,
        unit: u64,
        target: u64,
        result: i32,
        extra: *const [u32; 3],
    ) {
        if !active() {
            return;
        }
        // SAFETY: the caller holds the world; three extras
        let (t, e) = unsafe { (WorldRead::begin(), *extra) };
        let head = [func, id_of(&t, unit), id_of(&t, target), result as u32];
        if which == 0 {
            fold(EE_COMBAT_FOLD_HOST_READS, &bytes(&[head[0], head[1], head[2], head[3], e[0], e[1], e[2]]));
        } else {
            fold(EE_COMBAT_FOLD_X_READS, &bytes(&[head[0], head[1], head[2], head[3], e[0], e[1]]));
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_shot_record_puts_the_goal_target_where_its_kind_says() {
        let mut u = UnitFacts {
            id: 7,
            goal: Some(GOAL_ATTACK_UNIT),
            attack_target: 9,
            ground: [1, 2, 3],
            ..Default::default()
        };
        u.location = [1.0, 2.0, 3.0];
        let r = shot_words(5, &u, 4);
        assert_eq!(&r[..3], &[5, 7, 4]);
        assert_eq!(r[7], 2);
        assert_eq!(&r[8..12], &[9, 0, 0, 0]);
        assert_eq!(r[12], 1.0f32.to_bits());
        u.goal = Some(GOAL_ATTACK_GROUND);
        assert_eq!(&shot_words(5, &u, 4)[8..12], &[0, 1, 2, 3]);
        u.goal = None;
        let r = shot_words(5, &u, 4);
        assert_eq!(r[7], 0xffff_ffff);
        assert_eq!(&r[8..12], &[0, 0, 0, 0]);
    }

    #[test]
    fn an_action_record_reads_its_target() {
        let u = UnitFacts {
            id: 1,
            goal: Some(GOAL_ATTACK_UNIT),
            attack_target: 3,
            special_attack: true,
            ..Default::default()
        };
        let t = UnitFacts { id: 3, hit_points: 40, resource_amount: 2, ..Default::default() };
        let r = action_words(2, &u, Some(&t), 8);
        assert_eq!(&r[..4], &[2, 1, 3, 8]);
        assert_eq!(r[11], 1);
        assert_eq!(&r[14..18], &[2, 3, 40, 2]);
        let r = action_words(2, &UnitFacts::default(), None, 0);
        assert_eq!(r[2], 0);
        assert_eq!(r[14], 0xffff_ffff);
        assert_eq!(r[15], 0);
    }

    #[test]
    fn words_are_little_endian_bytes() {
        assert_eq!(bytes(&[1, 0x0102_0304]), vec![1, 0, 0, 0, 4, 3, 2, 1]);
    }
}
