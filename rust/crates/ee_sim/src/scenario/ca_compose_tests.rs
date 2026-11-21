use super::*;

/// A fake world: player handle 10, upgrade 20, class 30, units 100.. (unit 101 of another class, 102 not complex),
/// herds 200..; records the statements in order.
struct World {
    ops: Vec<(u32, Vec<i64>)>,
    upgrade: EeUnitsUnitClassUpgradeFields,
    class: EeUnitsUnitClassFields,
    effect: EeScenarioEffectFields,
    added: Option<EeScenarioEffectFields>,
    has_upgrade: bool,
    herds: Vec<i64>,
    active: Vec<(i64, i64)>,
}

impl World {
    fn new() -> World {
        let mut w = World {
            ops: Vec::new(),
            upgrade: EeUnitsUnitClassUpgradeFields::default(),
            class: EeUnitsUnitClassFields::default(),
            effect: EeScenarioEffectFields::default(),
            added: None,
            has_upgrade: true,
            herds: Vec::new(),
            active: Vec::new(),
        };
        w.class.mUnitType = 7;
        w.class.mReloadTime = 3;
        w.class.mRange = 4.5;
        w.class.mArmor = [1, 2, 3, 4, 5, 6];
        w.upgrade.mPower = -9;
        w.upgrade.mAttack = 2.25;
        w.upgrade.mArmor[2] = 8.5;
        w
    }

    fn run(&mut self, op: u32, a: [i64; 6]) -> i64 {
        let me = self as *mut World;
        let mut cpp = |op: u32, v: [i64; 6]| -> i64 {
            // SAFETY: the test's own world, alive through the call
            let w = unsafe { &mut *me };
            w.ops.push((op, v.to_vec()));
            match op {
                EE_SCENARIO_WK_PLAYER => 10,
                EE_SCENARIO_WK_UPGRADE => {
                    if w.has_upgrade {
                        20
                    } else {
                        0
                    }
                }
                EE_SCENARIO_WK_UPGRADE_FIELDS => &mut w.upgrade as *mut _ as usize as i64,
                EE_SCENARIO_WK_CLASS => 30,
                EE_SCENARIO_WK_CLASS_FIELDS => &mut w.class as *mut _ as usize as i64,
                EE_SCENARIO_WK_PLAYER_ID => 3,
                EE_SCENARIO_WK_EFFECT_NEW => 40,
                EE_SCENARIO_WK_EFFECT_FIELDS => &mut w.effect as *mut _ as usize as i64,
                EE_SCENARIO_WK_EFFECT_ADD => {
                    w.added = Some(w.effect);
                    0
                }
                EE_SCENARIO_WK_FIRST_UNIT => 100,
                EE_SCENARIO_WK_NEXT_UNIT => {
                    if v[0] < 102 {
                        v[0] + 1
                    } else {
                        0
                    }
                }
                EE_SCENARIO_WK_UNIT_COMPLEX => (v[0] != 102) as i64,
                EE_SCENARIO_WK_UNIT_CLASS => {
                    if v[0] == 101 {
                        31
                    } else {
                        30
                    }
                }
                EE_SCENARIO_WK_HERDS_CLEAR => {
                    w.herds.clear();
                    0
                }
                EE_SCENARIO_WK_HERDS => {
                    w.herds.extend([200, 201]);
                    0
                }
                EE_SCENARIO_WK_HERD_COUNT => w.herds.len() as i64,
                EE_SCENARIO_WK_HERD_AT => w.herds[v[0] as usize],
                EE_SCENARIO_WK_HERD_ACTIVE => {
                    w.active.push((v[0], v[1]));
                    0
                }
                _ => 0,
            }
        };
        compose(&mut cpp, op, a).expect("a class attribute op")
    }

    fn ops(&self) -> Vec<u32> {
        self.ops.iter().map(|(op, _)| *op).collect()
    }
}

#[test]
fn other_ops_are_the_cpps() {
    let mut cpp = |_: u32, _: [i64; 6]| -> i64 { panic!("no statement") };
    assert_eq!(compose(&mut cpp, EE_SCENARIO_WORLD_CA_UPGRADE - 1, [0; 6]), None);
    assert_eq!(compose(&mut cpp, EE_SCENARIO_WORLD_CA_CAPTURE + 1, [0; 6]), None);
}

#[test]
fn upgrade_test_looks_up_player_then_upgrade() {
    let mut w = World::new();
    assert_eq!(w.run(EE_SCENARIO_WORLD_CA_UPGRADE, [2, 7, 0, 0, 0, 0]), 1);
    assert_eq!(w.ops(), vec![EE_SCENARIO_WK_PLAYER, EE_SCENARIO_WK_UPGRADE]);
    assert_eq!(w.ops[1].1[..2], [10, 7]);
    let mut w = World::new();
    w.has_upgrade = false;
    assert_eq!(w.run(EE_SCENARIO_WORLD_CA_UPGRADE, [2, 7, 0, 0, 0, 0]), 0);
    // no player: no lookups at all
    let mut w = World::new();
    assert_eq!(w.run(EE_SCENARIO_WORLD_CA_UPGRADE, [-1, 7, 0, 0, 0, 0]), 0);
    assert!(w.ops.is_empty());
    let mut w = World::new();
    assert_eq!(w.run(EE_SCENARIO_WORLD_CA_UPGRADE, [2, -1, 0, 0, 0, 0]), 0);
    assert_eq!(w.ops(), vec![EE_SCENARIO_WK_PLAYER]);
}

#[test]
fn values_by_kind() {
    let mut w = World::new();
    assert_eq!(w.run(EE_SCENARIO_WORLD_CA_VALUE, [2, 7, POWER_PLUS, 0, 0, 0]), -9);
    assert_eq!(w.run(EE_SCENARIO_WORLD_CA_VALUE, [2, 7, ATTACK_PLUS, 0, 0, 0]), bits(2.25));
    assert_eq!(w.run(EE_SCENARIO_WORLD_CA_VALUE, [2, 7, 22, 0, 0, 0]), bits(8.5));
    w.upgrade.mIsInitialized = true;
    assert_eq!(w.run(EE_SCENARIO_WORLD_CA_VALUE, [2, 7, ANIMAL_AI_ACTIVE_PLUS, 0, 0, 0]), 1);
    assert_eq!(w.run(EE_SCENARIO_WORLD_CA_BASE, [2, 7, RELOAD_TIME_PLUS, 0, 0, 0]), bits(3.0));
    assert_eq!(w.run(EE_SCENARIO_WORLD_CA_BASE, [2, 7, RANGE_PLUS, 0, 0, 0]), bits(4.5));
    assert_eq!(w.run(EE_SCENARIO_WORLD_CA_BASE, [2, 7, 25, 0, 0, 0]), bits(6.0));
}

#[test]
fn plain_sets_write_the_record() {
    let mut w = World::new();
    w.run(EE_SCENARIO_WORLD_CA_SET, [2, 7, HIT_POINTS_PLUS, 450, 0, 0]);
    assert_eq!(w.upgrade.mMaxHitPoints, 450);
    w.run(EE_SCENARIO_WORLD_CA_SET, [2, 7, CAMERA_FOCUS_PLUS, bits(1.5), 0, 0]);
    assert_eq!(w.upgrade.mCameraFocus, 1.5);
    w.run(EE_SCENARIO_WORLD_CA_SET, [2, 7, SHOCK_ARMOR_PLUS, bits(0.25), 0, 0]);
    assert_eq!(w.upgrade.mArmor[0], 0.25);
    let mut w = World::new();
    w.run(EE_SCENARIO_WORLD_CA_SET, [2, 7, RANGE_PLUS, bits(9.0), 0, 0]);
    assert_eq!(
        w.ops(),
        vec![EE_SCENARIO_WK_PLAYER, EE_SCENARIO_WK_UPGRADE, EE_SCENARIO_WK_CLASS, EE_SCENARIO_WK_SET_RANGE]
    );
    assert_eq!(w.ops[3].1[..2], [20, bits(9.0)]);
    assert_eq!(w.upgrade.mRange, 0.0, "the setter's, not the record's");
}

#[test]
fn line_of_sight_goes_off_then_on() {
    let mut w = World::new();
    w.run(EE_SCENARIO_WORLD_CA_SET, [2, 7, LINE_OF_SIGHT_PLUS, bits(12.0), 0, 0]);
    let sight: Vec<(u32, i64)> = w
        .ops
        .iter()
        .filter(|(op, _)| *op == EE_SCENARIO_WK_SIGHT_OFF || *op == EE_SCENARIO_WK_SIGHT_ON)
        .map(|(op, v)| (*op, v[0]))
        .collect();
    // unit 100 only: 101 is another class, 102 has no complex unit
    assert_eq!(sight, vec![(EE_SCENARIO_WK_SIGHT_OFF, 100), (EE_SCENARIO_WK_SIGHT_ON, 100)]);
    assert_eq!(w.upgrade.mLineOfSight, 12.0);
    // the class test runs only for complex units
    let classes = w.ops.iter().filter(|(op, _)| *op == EE_SCENARIO_WK_UNIT_CLASS).count();
    assert_eq!(classes, 2);
}

#[test]
fn animal_flag_sets_the_herds_last_first() {
    let mut w = World::new();
    w.run(EE_SCENARIO_WORLD_CA_SET, [2, 7, ANIMAL_AI_ACTIVE_PLUS, 1, 0, 0]);
    assert!(w.upgrade.mIsInitialized);
    assert_eq!(w.active, vec![(201, 1), (200, 1)]);
    let herds = w.ops.iter().find(|(op, _)| *op == EE_SCENARIO_WK_HERDS).unwrap();
    assert_eq!(herds.1[..2], [10, 7]);
}

#[test]
fn pending_effect_and_capture() {
    let mut w = World::new();
    w.run(EE_SCENARIO_WORLD_CA_PENDING, [2, 7, ATTACK_PLUS, 1, 15, 2]);
    let e = w.added.expect("added");
    assert_eq!(e.mAttributeUnit, c_ulong::MAX);
    assert_eq!((e.mType, e.mObjectAction), (EFFECT_OBJECT, OBJECT_ACTION_UNIT_ATTRIBUTE));
    assert_eq!((e.mAttributeUnitType, e.mAttributePlayer, e.mObject), (7, 3, 0));
    assert_eq!((e.mAttribute, e.mUsesPercent, e.mAttributeAmount, e.mAttributeMode), (27, true, 15, 2));
    assert_eq!(*w.ops().last().unwrap(), EE_SCENARIO_WK_EFFECT_FREE);
    let mut w = World::new();
    w.has_upgrade = false;
    w.run(EE_SCENARIO_WORLD_CA_CAPTURE, [2, -1, ATTACK_PLUS | 1 << 32, 15, 2, 99]);
    assert_eq!(*w.ops().last().unwrap(), EE_SCENARIO_WK_CAPTURE);
    assert_eq!(w.ops.last().unwrap().1[1..4], [10, 0, 99]);
}
