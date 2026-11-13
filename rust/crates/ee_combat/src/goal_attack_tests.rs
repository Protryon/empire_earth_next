use super::*;
use std::collections::BTreeMap;

/// A mock world of units by handle: tile, location tile, complex unit, parts, flags, family, category, hit points,
/// id, player; the players' ids and enemies; a file; every operation logged.
#[derive(Default)]
struct World {
    units: BTreeMap<u64, Unit>,
    players: BTreeMap<u64, (i64, Vec<i64>)>,
    file: Vec<(i32, i64)>,
    read_at: usize,
    fail_read_at: Option<usize>,
    log: Vec<(i32, u64, i64)>,
    attacks: BTreeMap<u64, (i64, i64)>,
}

#[derive(Clone, Default)]
struct Unit {
    on_tile: bool,
    tile: [i64; 4],
    complex: u64,
    parts: i64,
    flags: i64,
    family: i64,
    category: i64,
    hp: i64,
    id: i64,
    attackers: i64,
    refs: i64,
    player: u64,
    ammo: i64,
    attackable: bool,
    velocity: f32,
}

impl World {
    fn unit(&mut self, h: u64) -> &mut Unit {
        self.units.entry(h).or_default()
    }
}

impl GoalHost for World {
    fn op(&mut self, code: i32, unit: u64, args: &mut EeCombatGoalArgs) -> i64 {
        self.log.push((code, unit, args[0]));
        let a0 = args[0];
        match code {
            EE_COMBAT_G_ON_TILE => i64::from(self.unit(unit).on_tile),
            EE_COMBAT_G_COMPLEX => self.unit(unit).complex as i64,
            EE_COMBAT_G_ATTACKER_FLAGS => self.unit(unit).flags,
            EE_COMBAT_G_NUM_ATTACKERS => {
                let u = self.unit(unit);
                u.attackers += a0;
                u.attackers
            }
            EE_COMBAT_G_PARTS => self.unit(unit).parts,
            EE_COMBAT_G_ATTACK_PART => 0x900,
            EE_COMBAT_G_REFERENCE => {
                self.unit(unit).refs += 1;
                0
            }
            EE_COMBAT_G_DEREFERENCE => {
                self.unit(unit).refs -= 1;
                0
            }
            EE_COMBAT_G_TILE => {
                let t = self.unit(unit).tile;
                args[..4].copy_from_slice(&t);
                0
            }
            EE_COMBAT_G_FAMILY => self.unit(unit).family,
            EE_COMBAT_G_CATEGORY => self.unit(unit).category,
            EE_COMBAT_G_HIT_POINTS => self.unit(unit).hp,
            EE_COMBAT_G_ID => self.unit(unit).id,
            EE_COMBAT_G_PLAYER_ATTACK => {
                let e = self.attacks.entry(unit).or_default();
                match a0 {
                    1 => {
                        if e.0 == 0 {
                            e.1 = 2;
                        }
                        e.0 += 1
                    }
                    -1 => {
                        e.0 -= 1;
                        if e.0 == 0 {
                            e.1 = 1;
                        }
                    }
                    _ => {
                        if e.0 == 0 && args[1] == 0 {
                            e.1 = 2;
                        }
                        e.0 += 1
                    }
                }
                0
            }
            EE_COMBAT_G_FIND_UNIT => self.units.iter().find(|(_, u)| u.id == a0).map_or(0, |(h, _)| *h as i64),
            EE_COMBAT_G_FIND_GHOST => 0,
            EE_COMBAT_G_VELOCITY => i64::from(self.unit(unit).velocity.to_bits()),
            EE_COMBAT_G_POSITION => {
                // lead: (1, 1, 1); firing: the incoming point's x and y kept, z set to 7
                if args[1] == 0 {
                    args[0] = i64::from(1.0f32.to_bits());
                    args[1] = i64::from(1.0f32.to_bits());
                    args[2] = i64::from(1.0f32.to_bits());
                } else {
                    args[0] = args[2];
                    args[1] = args[3];
                    args[2] = i64::from(7.0f32.to_bits());
                }
                0
            }
            EE_COMBAT_G_PLAYER => self.unit(unit).player as i64,
            EE_COMBAT_G_PLAYER_READ => {
                let (id, enemies) = self.players.get(&unit).cloned().unwrap_or_default();
                if a0 == 1 {
                    enemies.get(args[1] as usize).copied().unwrap_or(0)
                } else {
                    id
                }
            }
            EE_COMBAT_G_AMMO_TYPE => self.unit(unit).ammo,
            EE_COMBAT_G_CAN_BE_ATTACKED => i64::from(self.unit(unit).attackable),
            EE_COMBAT_G_WRITE_U32
            | EE_COMBAT_G_WRITE_I32
            | EE_COMBAT_G_WRITE_BOOL
            | crate::goal_objects::EE_COMBAT_G_WRITE_F32 => {
                self.file.push((code, a0));
                0
            }
            EE_COMBAT_G_READ_U32 | EE_COMBAT_G_READ_I32 | EE_COMBAT_G_READ_BOOL | EE_COMBAT_G_READ_F32 => {
                if self.fail_read_at == Some(self.read_at) {
                    return 3;
                }
                let (c, v) = self.file[self.read_at];
                assert_eq!(c + 1, code, "read as written");
                args[0] = v;
                self.read_at += 1;
                0
            }
            _ => 0,
        }
    }
}

const ATTACKER: u64 = 0x100;
const TARGET: u64 = 0x200;
const COMPLEX: u64 = 0x300;

fn world() -> World {
    let mut w = World::default();
    *w.unit(ATTACKER) = Unit { id: 11, tile: [3, 4, 3, 4], player: 0x9000, ammo: -1, ..Default::default() };
    *w.unit(TARGET) = Unit {
        on_tile: true,
        complex: COMPLEX,
        id: 22,
        hp: 50,
        family: 1,
        tile: [5, 6, 5, 6],
        player: 0x9100,
        ..Default::default()
    };
    *w.unit(COMPLEX) = Unit { id: 22, ..Default::default() };
    w.players.insert(0x9000, (1, vec![0, 0, 1]));
    w.players.insert(0x9100, (2, vec![0, 1, 0]));
    w
}

#[test]
fn create_counts_references_and_tiles_and_destroy_undoes_them() {
    let goal = 0x7001;
    let mut w = world();
    create(&mut w, goal, ATTACKER, TARGET, true);
    let a = attack_of(goal);
    assert_eq!((a.attacker, a.target, a.tile, a.convert, a.sidestep), (ATTACKER, TARGET, [5, 6], 1, 0));
    assert_eq!(a.location, [5.5f32.to_bits(), 6.5f32.to_bits(), 0]);
    assert_eq!(w.units[&TARGET].refs, 1);
    assert_eq!(w.units[&COMPLEX].attackers, 1, "an AI attacker that is no projectile");
    assert_eq!(w.attacks[&ATTACKER], (1, 2), "the first attack: combat state 2");
    destroy(&mut w, goal);
    assert_eq!(w.units[&TARGET].refs, 0);
    assert_eq!(w.units[&COMPLEX].attackers, 0);
    assert_eq!(w.attacks[&ATTACKER], (0, 1), "the last attack ended: combat state 1");
    assert_eq!(attack_of(goal), EeCombatAttackGoal::default(), "the record went");
}

#[test]
fn a_human_attacker_counts_no_attacker_and_an_animal_no_attack_and_a_carrier_gives_its_part() {
    let goal = 0x7002;
    let mut w = world();
    w.unit(ATTACKER).flags = EE_COMBAT_G_HUMAN;
    // the attack part is the target the family is read of
    w.unit(0x900).family = FAMILY_ANIMAL;
    w.unit(COMPLEX).parts = 1;
    create(&mut w, goal, ATTACKER, TARGET, false);
    assert_eq!(w.units[&COMPLEX].attackers, 0);
    assert!(!w.attacks.contains_key(&ATTACKER));
    let a = attack_of(goal);
    assert_eq!(a.target, 0x900, "the carrier's attack part");
    // the location is the original target's tile centre
    assert_eq!(a.location[0], 5.5f32.to_bits());
    with_goals(|g| g.attack.remove(&goal));
    // a priest takes the carrier itself
    let mut w = world();
    w.unit(ATTACKER).flags = EE_COMBAT_G_PRIEST;
    w.unit(COMPLEX).parts = 1;
    create(&mut w, goal, ATTACKER, TARGET, false);
    assert_eq!(attack_of(goal).target, TARGET);
    with_goals(|g| g.attack.remove(&goal));
}

#[test]
fn set_target_replace_and_gate() {
    let goal = 0x7003;
    let mut w = world();
    create(&mut w, goal, ATTACKER, TARGET, false);
    set_target(&mut w, goal, 0);
    assert_eq!(attack_of(goal).tile, [-10000, -10000]);
    assert_eq!(w.units[&TARGET].refs, 0);
    set_target(&mut w, goal, TARGET);
    replace_unit(&mut w, goal, COMPLEX, 0x400);
    assert_eq!(attack_of(goal).target, 0x400);
    assert_eq!((w.units[&TARGET].refs, w.units[&0x400].refs), (0, 1));
    replace_unit(&mut w, goal, ATTACKER, 0x500);
    assert_eq!(attack_of(goal).attacker, 0x500);
    w.unit(0x400).category = CATEGORY_GATE;
    retarget_gate_part(&mut w, goal);
    assert_eq!(attack_of(goal).target, 0x900);
    with_goals(|g| g.attack.remove(&goal));
}

#[test]
fn destination_leads_a_moving_target_and_keeps_a_partial_firing_point() {
    let goal = 0x7004;
    let mut w = world();
    create(&mut w, goal, ATTACKER, TARGET, false);
    let incoming = [2.0f32.to_bits(), 3.0f32.to_bits(), 4.0f32.to_bits()];
    let out = destination(&mut w, goal, ATTACKER, incoming);
    assert_eq!(out, [2.0f32.to_bits(), 3.0f32.to_bits(), 7.0f32.to_bits()], "firing: the caller's x, y kept");
    assert_eq!(attack_of(goal).location, out);
    w.unit(COMPLEX).velocity = 0.5;
    w.unit(TARGET).tile = [8, 9, 8, 9];
    let out = destination(&mut w, goal, ATTACKER, incoming);
    assert_eq!(out, [1.0f32.to_bits(); 3]);
    assert_eq!(attack_of(goal).tile, [8, 9], "the target's tile updated");
    assert_eq!(w.log.last().unwrap().0, EE_COMBAT_G_TELL_GROUP);
    with_goals(|g| g.attack.remove(&goal));
}

#[test]
fn suits_an_enemy_target() {
    let goal = 0x7005;
    let mut w = world();
    create(&mut w, goal, ATTACKER, TARGET, false);
    assert!(suits(&mut w, goal, ATTACKER), "no ammunition type: any enemy");
    w.unit(ATTACKER).player = 0x9100;
    assert!(!suits(&mut w, goal, ATTACKER), "the same player");
    w.unit(ATTACKER).player = 0x9200;
    w.players.insert(0x9200, (3, vec![0; 17]));
    assert!(!suits(&mut w, goal, ATTACKER), "not at war");
    w.unit(ATTACKER).player = 0x9000;
    w.unit(ATTACKER).ammo = 0;
    assert!(!suits(&mut w, goal, ATTACKER), "cannot be attacked");
    with_goals(|g| g.attack.remove(&goal));
    assert!(!suits(&mut w, goal, ATTACKER), "no target");
}

#[test]
fn save_load_round_trip_with_and_without_the_live_state() {
    let goal = 0x7006;
    let mut w = world();
    create(&mut w, goal, ATTACKER, TARGET, true);
    let mut a = attack_of(goal);
    a.sidestep = 2;
    with_goals(|g| g.attack.insert(goal, a));
    assert_eq!(save(&mut w, goal, true, false), 0);
    let codes: Vec<i32> = w.file.iter().map(|f| f.0).collect();
    assert_eq!(
        codes,
        vec![
            EE_COMBAT_G_WRITE_U32,
            EE_COMBAT_G_WRITE_U32,
            EE_COMBAT_G_WRITE_I32,
            EE_COMBAT_G_WRITE_I32,
            EE_COMBAT_G_WRITE_BOOL,
            EE_COMBAT_G_WRITE_F32,
            EE_COMBAT_G_WRITE_F32,
            EE_COMBAT_G_WRITE_F32,
            EE_COMBAT_G_WRITE_U32
        ]
    );
    assert_eq!((w.file[0].1, w.file[1].1), (22, 11));
    let loaded = 0x7007;
    create_empty(loaded);
    assert_eq!(load(&mut w, loaded, 0, 0x110, true), 0);
    let b = attack_of(loaded);
    assert_eq!((b.target, b.attacker, b.tile, b.convert, b.location, b.sidestep), (22, 11, a.tile, 1, a.location, 2));
    // RepairPointers: the ids back to units, the target referenced, the attack counted
    assert_eq!(repair(&mut w, loaded, true), 0);
    let b = attack_of(loaded);
    assert_eq!((b.target, b.attacker), (TARGET, ATTACKER));
    assert_eq!(w.units[&TARGET].refs, 2);
    // a dead target without the ghosts' format: -1, and RepairPointers clears both
    w.unit(TARGET).hp = 0;
    w.file.clear();
    assert_eq!(save(&mut w, goal, false, false), 0);
    assert_eq!(w.file.len(), 5, "no live state");
    assert_eq!(w.file[0].1, 0xffff_ffff);
    w.read_at = 0;
    let old = 0x7008;
    create_empty(old);
    // a save before 0.238 has no convert flag
    w.file.remove(4);
    assert_eq!(load(&mut w, old, 0, 0xed, false), 0);
    assert_eq!(attack_of(old).convert, 0);
    assert_eq!(repair(&mut w, old, false), 3);
    assert_eq!((attack_of(old).target, attack_of(old).attacker), (0, 0));
    // a failed read stops at its place
    w.read_at = 0;
    w.fail_read_at = Some(1);
    let failed = 0x7009;
    create_empty(failed);
    assert_eq!(load(&mut w, failed, 0, 0x110, false), 3);
    assert_eq!((attack_of(failed).target, attack_of(failed).attacker), (0xffff_ffff, 0));
    for g in [goal, loaded, old, failed] {
        with_goals(|t| t.attack.remove(&g));
    }
}

#[test]
fn the_checksum_sums_bits_ids_and_tile() {
    let goal = 0x700a;
    let mut w = world();
    create(&mut w, goal, ATTACKER, TARGET, false);
    let a = attack_of(goal);
    let want = (a.location[0] as i32)
        .wrapping_add(a.location[1] as i32)
        .wrapping_add(a.location[2] as i32)
        .wrapping_add(11)
        .wrapping_add(22)
        .wrapping_add(5 + 6);
    assert_eq!(checksum(&mut w, goal), want);
    with_goals(|g| g.attack.remove(&goal));
}
