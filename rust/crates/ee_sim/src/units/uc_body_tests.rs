use super::*;
use std::collections::BTreeMap;

/// A recording host: each statement asked (op, a, b, c), its answer from `answers` keyed by (op, a) (0 else).
#[derive(Default)]
struct Mock {
    log: Vec<(i32, u64, i64, i64)>,
    answers: BTreeMap<(i32, u64), i64>,
    sequences: BTreeMap<(i32, u64), Vec<i64>>,
    points: Vec<[f32; 4]>,
}

impl Mock {
    fn answer(mut self, op: i32, a: u64, value: i64) -> Self {
        self.answers.insert((op, a), value);
        self
    }
    fn sequence(mut self, op: i32, a: u64, values: &[i64]) -> Self {
        self.sequences.insert((op, a), values.to_vec());
        self
    }
    fn ops(&self) -> Vec<i32> {
        self.log.iter().map(|e| e.0).collect()
    }
    fn count(&self, op: i32) -> usize {
        self.log.iter().filter(|e| e.0 == op).count()
    }
}

impl UcHost for Mock {
    #[allow(clippy::too_many_arguments)]
    fn uc(&mut self, op: i32, class: u64, unit: u64, a: u64, b: i64, c: i64, f: &mut [f32; 4]) -> i64 {
        assert_eq!((class, unit), (100, 1));
        self.log.push((op, a, b, c));
        match op {
            EE_UNITS_UC_LOCATION => *f = [1.0, 2.0, -1.0, 0.0],
            EE_UNITS_UC_SOUND_PARAMETERS => f[0] = 0.5,
            EE_UNITS_UC_SET_SOUND_PARAMETERS | EE_UNITS_UC_NEW_ATTACK_GROUND | EE_UNITS_UC_PLACED_EFFECT => {
                self.points.push(*f)
            }
            _ => {}
        }
        if let Some(values) = self.sequences.get_mut(&(op, a)) {
            if !values.is_empty() {
                return values.remove(0);
            }
        }
        *self.answers.get(&(op, a)).unwrap_or(&0)
    }
}

#[test]
fn heal_needs_a_goal_in_range() {
    let mut h = Mock::default();
    heal(&mut h, 100, 1);
    assert_eq!(h.ops(), [EE_UNITS_UC_HEAL_GOAL, EE_UNITS_UC_DROP_PATH]);
    let mut h = Mock::default().answer(EE_UNITS_UC_HEAL_GOAL, 0, 7);
    heal(&mut h, 100, 1);
    assert_eq!(h.ops(), [EE_UNITS_UC_HEAL_GOAL, EE_UNITS_UC_DROP_PATH, EE_UNITS_UC_HEAL_IN_RANGE]);
}

#[test]
fn heal_draws_its_effect_by_kind_and_keeps_the_handle() {
    let base = || {
        Mock::default()
            .answer(EE_UNITS_UC_HEAL_GOAL, 0, 7)
            .answer(EE_UNITS_UC_HEAL_IN_RANGE, 7, 1)
            .answer(EE_UNITS_UC_HEAL_EFFECT, 0, 30)
            .answer(EE_UNITS_UC_HANDLE, 0, 55)
    };
    let mut h = base().answer(EE_UNITS_UC_EFFECT_KIND, 30, 0x21);
    heal(&mut h, 100, 1);
    assert!(h.log.contains(&(EE_UNITS_UC_HEAL_BETWEEN, 30, 7, 0)));
    assert!(h.log.contains(&(EE_UNITS_UC_SET_HEAL_HANDLE, 0, 55, 0)));
    assert!(h.log.contains(&(EE_UNITS_UC_SET_ANIMATION, 0, 3, 0)));
    let mut h = base().answer(EE_UNITS_UC_EFFECT_KIND, 30, 0x2c).answer(EE_UNITS_UC_HEAL_HANDLE, 0, 55);
    heal(&mut h, 100, 1);
    assert_eq!(h.count(EE_UNITS_UC_HEAL_ON_TARGET), 1);
    assert_eq!(h.count(EE_UNITS_UC_SET_HEAL_HANDLE), 0, "the same handle is not stored again");
    // a failed creation keeps the old handle; an unlisted kind creates nothing
    let mut h = base().answer(EE_UNITS_UC_EFFECT_KIND, 30, 0x14).answer(EE_UNITS_UC_HEAL_BETWEEN, 30, 1);
    heal(&mut h, 100, 1);
    assert_eq!(h.ops().last(), Some(&EE_UNITS_UC_HEAL_BETWEEN));
    let mut h = base().answer(EE_UNITS_UC_EFFECT_KIND, 30, 0x11);
    heal(&mut h, 100, 1);
    assert_eq!(h.count(EE_UNITS_UC_HEAL_BETWEEN) + h.count(EE_UNITS_UC_HEAL_ON_TARGET), 0);
}

#[test]
fn building_production_counts_down_and_produces() {
    let mut h = Mock::default()
        .answer(EE_UNITS_UC_PRODUCTION_TIME, 0, 30)
        .answer(EE_UNITS_UC_HIT_POINTS, 0, 10)
        .answer(EE_UNITS_UC_TICK_MS, 0, 50)
        .answer(EE_UNITS_UC_RESEARCH_MAKES_UNIT, 0, 1)
        .answer(EE_UNITS_UC_RESEARCH_CLASS, 0, 40);
    update_building(&mut h, 100, 1);
    assert!(h.log.contains(&(EE_UNITS_UC_PRODUCE_UNIT, 40, 0, 0)));
    assert!(h.log.contains(&(EE_UNITS_UC_SET_PRODUCTION_TIME, 0, 5000, 0)), "a failed production waits 5 s");
    // the garrison's healing (uc_anim.rs) reads the healing points first: none here
    assert_eq!(h.ops().last(), Some(&crate::units::uc_anim::EE_UNITS_UC_HEALING_POINTS));
    // a technology is researched at once; a disabled living building does nothing
    let mut h = Mock::default()
        .answer(EE_UNITS_UC_PRODUCTION_TIME, 0, 30)
        .answer(EE_UNITS_UC_HIT_POINTS, 0, 10)
        .answer(EE_UNITS_UC_INSTANT_BUILD, 0, 1);
    update_building(&mut h, 100, 1);
    assert_eq!(h.count(EE_UNITS_UC_RESEARCH), 1);
    assert!(h.log.contains(&(EE_UNITS_UC_SET_PRODUCTION_TIME, 0, 0, 0)));
    let mut h = Mock::default().answer(EE_UNITS_UC_DISABLED, 0, 1).answer(EE_UNITS_UC_HIT_POINTS, 0, 10);
    update_building(&mut h, 100, 1);
    assert_eq!(h.ops(), [EE_UNITS_UC_DISABLED, EE_UNITS_UC_HIT_POINTS]);
}

#[test]
fn a_rotting_building_rots_its_parts() {
    let mut h = Mock::default()
        .answer(EE_UNITS_UC_COMMAND_KIND, 0, 6002)
        .answer(EE_UNITS_UC_UPDATE_ROTTING, 0, 900)
        .answer(EE_UNITS_UC_NEXT_PART, 0, 2)
        .answer(EE_UNITS_UC_NEXT_PART, 2, 3);
    update_building(&mut h, 100, 1);
    let rotted: Vec<(u64, i64)> = h.log.iter().filter(|e| e.0 == EE_UNITS_UC_SET_ROTTING).map(|e| (e.1, e.2)).collect();
    assert_eq!(rotted, [(2, 900), (3, 900)]);
}

#[test]
fn an_animal_fires_on_its_attack_frames() {
    let mut h = Mock::default()
        .answer(EE_UNITS_UC_FRAME, 0, 12)
        .answer(EE_UNITS_UC_COMMAND_TYPE, 0, 2021)
        .answer(EE_UNITS_UC_HAS_PHYSICS_MODEL, 0, 1)
        .answer(EE_UNITS_UC_TILE_X, 0, -10000);
    update_animal(&mut h, 100, 1);
    // the animation (uc_anim.rs), then the attack frame's test, in place
    assert_eq!(h.count(crate::units::uc_anim::EE_UNITS_UC_ATTACK_FRAME_SET), 1);
    assert!(
        h.log.iter().position(|e| e.0 == crate::units::uc_anim::EE_UNITS_UC_PAUSED)
            < h.log.iter().position(|e| e.0 == crate::units::uc_anim::EE_UNITS_UC_ATTACK_FRAME_SET)
    );
    assert_eq!(h.count(EE_UNITS_UC_PHYSICS_STEP), 0, "off the map");
    assert_eq!(h.ops().last(), Some(&EE_UNITS_UC_UPDATE_DONE));
}

#[test]
fn the_sound_is_refreshed_on_its_ticks() {
    let base = || {
        Mock::default()
            .answer(EE_UNITS_UC_HAS_SOUND, 0, 1)
            .answer(EE_UNITS_UC_TICKS_DONE, 0, 21)
            .answer(EE_UNITS_UC_UNIT_ID, 0, 42)
            .answer(EE_UNITS_UC_SOUND_SAMPLE, 0, 9)
            .answer(EE_UNITS_UC_MODEL_SOUND2, 0, 80)
            .answer(EE_UNITS_UC_SAMPLE_ID, 80, 9)
    };
    let mut h = base();
    refresh_sound(&mut h, 100, 1);
    assert_eq!(h.count(EE_UNITS_UC_IN_VIEW), 0, "42 and 21 agree mod 21");
    assert!(h.log.contains(&(EE_UNITS_UC_SOUND_PARAMETERS, 80, 0, 0)));
    assert_eq!(h.points, [[0.5, 0.0, 0.0, 0.0]]);
    let mut h = base().answer(EE_UNITS_UC_TICKS_DONE, 0, 22);
    refresh_sound(&mut h, 100, 1);
    assert_eq!(h.count(EE_UNITS_UC_SOUND_SAMPLE), 0, "not this unit's tick");
}

#[test]
fn the_death_unit_is_launched_from_the_ground() {
    let mut h = Mock::default()
        .answer(EE_UNITS_UC_DEATH_SPAWN_CLASS, 0, 40)
        .answer(EE_UNITS_UC_UNIT_TILE_X, 1, 5)
        .answer(EE_UNITS_UC_UNIT_TILE_Y, 1, 6)
        .answer(EE_UNITS_UC_CREATE_UNIT, 40, 9)
        .answer(EE_UNITS_UC_NEW_ATTACK_GROUND, 0, 77)
        .answer(EE_UNITS_UC_MUZZLE_FLASH, 9, 3);
    create_death_unit(&mut h, 100, 1);
    assert!(h.log.contains(&(EE_UNITS_UC_CREATE_UNIT, 40, 5, 6)));
    assert!(h.log.contains(&(EE_UNITS_UC_SET_GOAL, 9, 77, 0)));
    assert!(h.log.contains(&(EE_UNITS_UC_START_ACTION, 9, 10003, 0)));
    assert_eq!(h.points[0], [1.0, 2.0, 0.3, 0.0], "a point under the ground is raised");
    assert_eq!(h.points.len(), 2);
    let _ = h.sequence(EE_UNITS_UC_HAS_SOUND, 0, &[]);
}
