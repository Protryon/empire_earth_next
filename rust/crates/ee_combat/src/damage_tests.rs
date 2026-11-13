use super::*;
use crate::reads::{QueryHost, EE_COMBAT_H_OWNER};
use crate::world::EeCombatWorldArgs;
use ee_ge::types::{Ge3DLine, GeTransformation};
use std::collections::BTreeMap;

const UNIT: u64 = 0x100;
const TARGET: u64 = 0x200;
const OTHER: u64 = 0x300;
const PLAYER: u64 = 0x9000;
const ENEMY: u64 = 0x9100;

/// A world of plain values: reads by (code, object), handles by (code, object), the C++'s damage part (to-hit 1,
/// the weapon factor 1, armour `armor`, minimum 1), each tile's unit list, every operation logged; a hit takes its
/// amount off the reads' hit points.
struct World {
    reads: BTreeMap<(i32, u64), u32>,
    handles: BTreeMap<(i32, u64), u64>,
    class: EeCombatWorldArgs,
    armor: f32,
    elevation: f32,
    tiles: BTreeMap<(i32, i32), Vec<u64>>,
    effect: bool,
    log: Vec<(i32, u64, u64, i64)>,
    records: Vec<EeCombatDamageIn>,
    victims: Vec<u64>,
}

impl World {
    fn new() -> World {
        let mut w = World {
            reads: BTreeMap::new(),
            handles: BTreeMap::new(),
            class: [0; 12],
            armor: 2.0,
            elevation: 1.5,
            tiles: BTreeMap::new(),
            effect: false,
            log: Vec::new(),
            records: Vec::new(),
            victims: Vec::new(),
        };
        for (unit, player, id) in [(UNIT, PLAYER, 1), (TARGET, ENEMY, 2), (OTHER, ENEMY, 2)] {
            w.handles.insert((EE_COMBAT_H_PLAYER, unit), player);
            w.handles.insert((EE_COMBAT_H_COMPLEX, unit), unit);
            w.reads.insert((EE_COMBAT_Q_PLAYER_ID, player), id);
            w.reads.insert((EE_COMBAT_Q_HIT_POINTS, unit), 100);
        }
        // the attacker's player is at war with player 2
        w.reads.insert((EE_COMBAT_Q_ENEMY, PLAYER), 1);
        w.set_class(10.0, 0.0);
        w
    }
    fn set_class(&mut self, attack: f32, radius: f32) {
        self.class[0] = i64::from(attack.to_bits());
        self.class[1] = i64::from(radius.to_bits());
        self.class[7] = i64::from(0.25f32.to_bits());
    }
    fn set(&mut self, code: i32, object: u64, value: u32) {
        self.reads.insert((code, object), value);
    }
    fn ops(&self) -> Vec<i32> {
        self.log.iter().map(|l| l.0).collect()
    }
}

impl QueryHost for World {
    fn read(&mut self, code: i32, object: EeCombatHandle, arg: i32) -> u32 {
        if code == EE_COMBAT_Q_ENEMY {
            // only the attacker's player's table: at war with every other id but its own
            return u32::from(object == PLAYER && arg != 1 && self.reads.get(&(code, object)) == Some(&1));
        }
        if code == EE_COMBAT_Q_TARGETABLE {
            return 1;
        }
        if code == EE_COMBAT_Q_ARMOR {
            return self.armor.to_bits();
        }
        self.reads.get(&(code, object)).copied().unwrap_or(0)
    }
    fn handle(&mut self, code: i32, object: EeCombatHandle, _arg: i32) -> EeCombatHandle {
        self.handles.get(&(code, object)).copied().unwrap_or(0)
    }
    fn act(&mut self, _code: i32, _object: EeCombatHandle, _arg: i32) {}
    fn location(&mut self, _object: EeCombatHandle) -> GeTransformation {
        GeTransformation::IDENTITY
    }
    fn intersects(&mut self, _: EeCombatHandle, _: &Ge3DLine, _: bool, _: u32, _: f32) -> bool {
        false
    }
    fn footprint_tile(&mut self, _object: EeCombatHandle) -> (i32, i32) {
        (0, 0)
    }
}

impl WorldHost for World {
    fn op(&mut self, code: i32, object: EeCombatHandle, other: EeCombatHandle, args: &mut EeCombatWorldArgs) -> u64 {
        self.log.push((code, object, other, args[0]));
        match code {
            EE_COMBAT_W_DAMAGE_IN => {
                // SAFETY: the operation's contract: the caller's record
                let i = unsafe { &mut *(args[0] as *mut EeCombatDamageIn) };
                if i.form != DAMAGE_DIRECT || i.attack > 0.0 {
                    i.attack_factor = 1.0;
                    i.weapon_factor = 1.0;
                }
                i.minimum_damage = 1;
                i.ally_factor = 0.5;
                i.herd_ally_factor = 0.1;
                i.fanatic_factor = 2.0;
                i.elevation_step = 1.0;
                i.trample_damage = 5;
                i.trample_threshold = 1.0;
            }
            EE_COMBAT_W_ELEVATION => return u64::from(self.elevation.to_bits()),
            EE_COMBAT_W_DAMAGE_CAPTURE => {
                // SAFETY: as above
                self.records.push(unsafe { *(args[0] as *const EeCombatDamageIn) });
            }
            EE_COMBAT_W_HIT_CLASS => *args = self.class,
            EE_COMBAT_W_UNIT_EFFECT => return u64::from(self.effect),
            EE_COMBAT_W_TAKE_HIT => {
                let hp = self.reads.entry((EE_COMBAT_Q_HIT_POINTS, object)).or_default();
                *hp = (*hp as i32 - args[0] as i32).max(0) as u32;
            }
            EE_COMBAT_W_TILE_UNITS => {
                // a node: the tile's index + 1 in the high half, the unit's position in the low
                let key = (args[0] as i32, args[1] as i32);
                return self.tiles.get(&key).map_or(0, |_| node(key, 0));
            }
            EE_COMBAT_W_NODE_NEXT => {
                let (key, index) = unnode(object);
                return if index + 1 < self.tiles[&key].len() { node(key, index + 1) } else { 0 };
            }
            EE_COMBAT_W_NODE_UNIT => {
                let (key, index) = unnode(object);
                return self.tiles[&key][index];
            }
            EE_COMBAT_W_AREA_CAPTURE => {
                // SAFETY: the operation's contract: `args[1]` handles at `args[0]`
                self.victims = unsafe { std::slice::from_raw_parts(args[0] as *const u64, args[1] as usize) }.to_vec();
            }
            _ => {}
        }
        0
    }
}

fn node(key: (i32, i32), index: usize) -> u64 {
    ((key.0 as u64 & 0xff) << 40) | ((key.1 as u64 & 0xff) << 32) | (index as u64 + 1)
}

fn unnode(node: u64) -> ((i32, i32), usize) {
    ((((node >> 40) & 0xff) as i8 as i32, ((node >> 32) & 0xff) as i8 as i32), (node & 0xffff_ffff) as usize - 1)
}

fn direct(attack: f32) -> EeCombatDamageCall {
    EeCombatDamageCall { form: DAMAGE_DIRECT, attack, ammo: 3, weapon: 1, friendly_factor: 0.0, amount: 0 }
}

// ---- ComputeDamage

#[test]
fn the_damage_record_takes_the_cpps_part_then_the_players_and_the_elevation() {
    let mut w = World::new();
    // (10 - 2) x 1.5 = 12
    assert_eq!(compute_damage(&mut w, UNIT, TARGET, &direct(10.0)), Some(12));
    let i = w.records[0];
    assert_eq!((i.form, i.attack, i.attack_factor, i.armor, i.elevation), (DAMAGE_DIRECT, 10.0, 1.0, 2.0, 1.5));
    assert_eq!((i.same_player, i.allied, i.herd, i.fanatic), (0, 0, 0, 0));
    assert_eq!(w.ops(), vec![EE_COMBAT_W_DAMAGE_IN, EE_COMBAT_W_ELEVATION, EE_COMBAT_W_DAMAGE_CAPTURE]);
    // the ammunition and the weapon to the C++, the elevation of the target's complex unit
    assert_eq!(w.log[0].2, TARGET);
    assert_eq!(w.log[1].2, TARGET);
}

#[test]
fn a_direct_hit_without_attack_reads_no_elevation_and_does_nothing() {
    let mut w = World::new();
    assert_eq!(compute_damage(&mut w, UNIT, TARGET, &direct(0.0)), None);
    assert_eq!((w.records[0].attack_factor, w.records[0].elevation), (0.0, 1.0));
    assert_eq!(w.ops(), vec![EE_COMBAT_W_DAMAGE_IN, EE_COMBAT_W_DAMAGE_CAPTURE]);
}

#[test]
fn a_cloaked_owner_makes_a_fanatic_hit_and_an_ally_takes_a_share() {
    let mut w = World::new();
    w.handles.insert((EE_COMBAT_H_OWNER, UNIT), OTHER);
    w.set(EE_COMBAT_Q_CLOAK_TIMER, OTHER, 500);
    compute_damage(&mut w, UNIT, TARGET, &direct(10.0));
    assert_eq!(w.records[0].fanatic, 1);
    // splash is never fanatic
    let splash = EeCombatDamageCall { form: DAMAGE_SPLASH, ..direct(10.0) };
    compute_damage(&mut w, UNIT, TARGET, &splash);
    assert_eq!(w.records[1].fanatic, 0);
    // the own player's unit
    w.handles.insert((EE_COMBAT_H_PLAYER, TARGET), PLAYER);
    w.set(EE_COMBAT_Q_HERD, UNIT, 1);
    compute_damage(&mut w, UNIT, TARGET, &direct(10.0));
    assert_eq!((w.records[2].same_player, w.records[2].allied, w.records[2].herd), (1, 1, 1));
}

#[test]
fn the_trample_reads_no_elevation() {
    let mut w = World::new();
    let trample = EeCombatDamageCall { form: DAMAGE_TRAMPLE, ..direct(10.0) };
    assert_eq!(compute_damage(&mut w, UNIT, TARGET, &trample), Some(5));
    assert_eq!(w.records[0].elevation, 1.0);
    assert!(!w.ops().contains(&EE_COMBAT_W_ELEVATION));
}

// ---- ApplyHit

#[test]
fn a_hit_damages_the_target_and_its_class_reacts() {
    let mut w = World::new();
    apply_hit(&mut w, UNIT, TARGET);
    let ops = w.ops();
    assert_eq!(
        ops,
        vec![
            EE_COMBAT_W_HIT_CLASS,
            EE_COMBAT_W_DAMAGE_IN,
            EE_COMBAT_W_ELEVATION,
            EE_COMBAT_W_DAMAGE_CAPTURE,
            EE_COMBAT_W_TAKE_HIT,
            EE_COMBAT_W_UNIT_DAMAGED,
            EE_COMBAT_W_HIT_CAPTURE,
        ]
    );
    assert_eq!(w.log[4], (EE_COMBAT_W_TAKE_HIT, TARGET, UNIT, 12));
    assert_eq!(w.log[5], (EE_COMBAT_W_UNIT_DAMAGED, TARGET, UNIT, 12));
    // a class with a shot effect: no reaction
    let mut w = World::new();
    w.class[8] = 1;
    apply_hit(&mut w, UNIT, TARGET);
    assert!(!w.ops().contains(&EE_COMBAT_W_UNIT_DAMAGED));
    // no target: only the capture
    let mut w = World::new();
    apply_hit(&mut w, UNIT, 0);
    assert_eq!(w.ops(), vec![EE_COMBAT_W_HIT_CLASS, EE_COMBAT_W_HIT_CAPTURE]);
}

#[test]
fn the_volcano_projectile_only_plays_the_targets_effect() {
    let mut w = World::new();
    w.set(EE_COMBAT_Q_UNIT_TYPE, UNIT, TYPE_VOLCANO_PROJECTILE);
    w.effect = true;
    apply_hit(&mut w, UNIT, TARGET);
    assert_eq!(w.ops(), vec![EE_COMBAT_W_UNIT_EFFECT, EE_COMBAT_W_HIT_CAPTURE]);
    // without the effect it hits as any shot
    w.effect = false;
    w.log.clear();
    apply_hit(&mut w, UNIT, TARGET);
    assert!(w.ops().contains(&EE_COMBAT_W_TAKE_HIT));
}

#[test]
fn a_kill_counts_and_a_hunted_animal_keeps_a_share_of_its_food() {
    let mut w = World::new();
    w.set(EE_COMBAT_Q_HIT_POINTS, TARGET, 5);
    w.set(EE_COMBAT_Q_CATEGORY, TARGET, CATEGORY_LAND);
    w.set(EE_COMBAT_Q_RESOURCE_TYPE, TARGET, RESOURCE_FOOD);
    w.set(EE_COMBAT_Q_RESOURCE_AMOUNT, TARGET, 150);
    apply_hit(&mut w, UNIT, TARGET);
    let ops = w.ops();
    assert_eq!(&ops[ops.len() - 3..], &[EE_COMBAT_W_EXPERIENCE, EE_COMBAT_W_SET_RESOURCE, EE_COMBAT_W_HIT_CAPTURE]);
    // lrintf(150 x 0.25 = 37.5) = 38 (to even)
    assert_eq!(w.log[w.log.len() - 2], (EE_COMBAT_W_SET_RESOURCE, TARGET, 0, 38));
    // a citizen's kill leaves the food whole; a unit already dead before the hit too
    for (category, before) in [(CATEGORY_LAND_CITIZEN, 5), (0, 0)] {
        let mut w = World::new();
        w.class[4] = i64::from(category);
        w.set(EE_COMBAT_Q_HIT_POINTS, TARGET, before);
        w.set(EE_COMBAT_Q_CATEGORY, TARGET, CATEGORY_LAND);
        w.set(EE_COMBAT_Q_RESOURCE_TYPE, TARGET, RESOURCE_FOOD);
        w.set(EE_COMBAT_Q_RESOURCE_AMOUNT, TARGET, 150);
        apply_hit(&mut w, UNIT, TARGET);
        assert!(!w.ops().contains(&EE_COMBAT_W_SET_RESOURCE));
    }
}

// ---- ApplyAreaDamage

#[test]
fn the_splash_hits_each_unit_around_once_and_rounds_the_food_up() {
    let mut w = World::new();
    w.set_class(10.0, 1.0);
    w.class[6] = 3;
    w.set(EE_COMBAT_Q_TILE_X, UNIT, 4);
    w.set(EE_COMBAT_Q_TILE_Y, UNIT, 4);
    // the target (counted as hit), another unit twice (two tiles), the attacker's own unit (no friendly factor)
    const OWN: u64 = 0x400;
    w.handles.insert((EE_COMBAT_H_PLAYER, OWN), PLAYER);
    w.tiles.insert((4, 4), vec![TARGET, OTHER, OWN]);
    w.tiles.insert((5, 4), vec![OTHER]);
    w.set(EE_COMBAT_Q_HIT_POINTS, OTHER, 5);
    w.set(EE_COMBAT_Q_CATEGORY, OTHER, CATEGORY_LAND);
    w.set(EE_COMBAT_Q_RESOURCE_TYPE, OTHER, RESOURCE_FOOD);
    w.set(EE_COMBAT_Q_RESOURCE_AMOUNT, OTHER, 150);
    apply_hit(&mut w, UNIT, TARGET);
    assert_eq!(w.victims, vec![OTHER]);
    let takes: Vec<_> = w.log.iter().filter(|l| l.0 == EE_COMBAT_W_TAKE_HIT).map(|l| (l.1, l.3)).collect();
    // splash: (10 - 2) x 1.5 = 12 on the other unit, then the direct hit on the target
    assert_eq!(takes, vec![(OTHER, 12), (TARGET, 12)]);
    // UMCeil(37.5) = 38
    assert!(w.log.contains(&(EE_COMBAT_W_SET_RESOURCE, OTHER, 0, 38)));
    let capture = w.log.iter().find(|l| l.0 == EE_COMBAT_W_AREA_CAPTURE).unwrap();
    assert_eq!((capture.1, capture.2), (UNIT, TARGET));
    // the splash's damage records come first, the target's last
    assert_eq!(w.records.iter().map(|r| r.form).collect::<Vec<_>>(), vec![DAMAGE_SPLASH, DAMAGE_DIRECT]);
}

#[test]
fn a_splash_with_a_friendly_factor_hits_the_own_units() {
    let mut w = World::new();
    w.set_class(10.0, 1.0);
    w.class[5] = i64::from(0.5f32.to_bits());
    const OWN: u64 = 0x400;
    w.handles.insert((EE_COMBAT_H_PLAYER, OWN), PLAYER);
    w.tiles.insert((0, 0), vec![OWN]);
    apply_hit(&mut w, UNIT, 0);
    assert_eq!(w.victims, vec![OWN]);
    assert_eq!(w.records[0].friendly_factor, 0.5);
    assert_eq!(w.records[0].same_player, 1);
}
