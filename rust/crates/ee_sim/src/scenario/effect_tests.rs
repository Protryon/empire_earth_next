//! effect.rs, effect_ai.rs and task.rs over a world that logs its calls.

use std::collections::BTreeMap;

use super::*;
use crate::scenario::fields::{effect_defaults, EeScenarioObjectFields};
use crate::scenario::placement::EE_SCENARIO_READ_OBJECT_UNIT_COUNT;
use crate::scenario::task::*;

#[derive(Clone, Default)]
struct Unit {
    player: i64,
    hp: i64,
    x: i64,
    y: i64,
    ty: i64,
    category: i64,
    complex: bool,
    command: i64,
    commandable: bool,
    container: i64,
    flags: BTreeMap<i64, i64>,
}

fn living(player: i64, x: i64, y: i64) -> Unit {
    Unit {
        player,
        hp: 10,
        x,
        y,
        complex: true,
        commandable: true,
        container: -1,
        command: EE_SCENARIO_K_ACTION_IDLE,
        ..Default::default()
    }
}

#[derive(Default)]
struct World {
    log: Vec<String>,
    units: BTreeMap<i64, Unit>,
    objects: BTreeMap<i32, (EeScenarioObjectFields, Vec<i64>)>,
    players: i32,
    current: i64,
    draws: Vec<i64>,
    answers: BTreeMap<u32, i64>,
    tiles: Vec<(i32, i32)>,
    group: Vec<i64>,
}

impl World {
    fn new(players: i32) -> World {
        World { players, current: 1, ..Default::default() }
    }
    fn object(mut self, index: i32, units: &[i64]) -> World {
        self.objects.insert(index, (EeScenarioObjectFields::default(), units.to_vec()));
        self
    }
    fn unit(mut self, id: i64, u: Unit) -> World {
        self.units.insert(id, u);
        self
    }
    fn calls(&self) -> Vec<&str> {
        self.log.iter().map(|s| s.as_str()).collect()
    }
}

impl Host for World {
    fn read4(&mut self, op: u32, a: i32, b: i32, _c: i32, _d: i32) -> i64 {
        match op {
            EE_SCENARIO_READ_NUM_PLAYERS => self.players as i64,
            EE_SCENARIO_READ_PLAYER_EXISTS => (0..self.players).contains(&a) as i64,
            EE_SCENARIO_READ_PLAYER_HUMAN => (a == 1) as i64,
            EE_SCENARIO_READ_PLAYER_COMPUTER => (a >= 2) as i64,
            EE_SCENARIO_READ_PLAYER_ID => a as i64,
            EE_SCENARIO_READ_ENEMY => (a != b) as i64,
            EE_SCENARIO_READ_CAPTURE_SEED => -1,
            EE_SCENARIO_READ_CLASS_SELECTABLE => (a != 99) as i64,
            EE_SCENARIO_READ_OBJECT_EXISTS => self.objects.contains_key(&a) as i64,
            EE_SCENARIO_READ_MAP_TILE_EXISTS => (a >= 0) as i64,
            EE_SCENARIO_READ_RANDOM => {
                self.log.push(format!("random {a} {b}"));
                if self.draws.is_empty() {
                    a as i64
                } else {
                    self.draws.remove(0)
                }
            }
            EE_SCENARIO_READ_OBJECT_UNIT_COUNT => self.objects[&a].1.len() as i64,
            crate::scenario::placement::EE_SCENARIO_READ_OBJECT_UNIT_AT => self.objects[&a].1[b as usize],
            _ => panic!("read {op}"),
        }
    }
    fn tiles(&mut self, _op: u32, a: i32, _b: i32, _c: i32) -> Vec<(i32, i32)> {
        self.log.push(format!("tiles {a}"));
        self.tiles.clone()
    }
    fn write(&mut self, op: u32, player: std::os::raw::c_long, b: i64, c: i64, d: i64) {
        self.log.push(format!("write {op} {player} {b} {c} {d}"));
    }
    fn object(&mut self, index: i32) -> Option<EeScenarioObjectFields> {
        self.objects.get(&index).map(|o| o.0)
    }
    fn world(&mut self, op: u32, a: [i64; 6]) -> i64 {
        let u = self.units.get(&a[0]).cloned();
        match op {
            EE_SCENARIO_WORLD_UNIT => {
                let Some(u) = u else {
                    assert_eq!(a[1], EE_SCENARIO_UV_EXISTS, "a missing unit's value {}", a[1]);
                    return 0;
                };
                return match a[1] {
                    EE_SCENARIO_UV_EXISTS => 1,
                    EE_SCENARIO_UV_COMPLEX => u.complex as i64,
                    EE_SCENARIO_UV_HIT_POINTS => u.hp,
                    EE_SCENARIO_UV_TILE_X => u.x,
                    EE_SCENARIO_UV_TILE_Y => u.y,
                    EE_SCENARIO_UV_PLAYER | EE_SCENARIO_UV_PLAYER_INDEX => u.player,
                    EE_SCENARIO_UV_TYPE => u.ty,
                    EE_SCENARIO_UV_CATEGORY => u.category,
                    EE_SCENARIO_UV_COMMAND => u.command,
                    EE_SCENARIO_UV_COMMANDABLE => u.commandable as i64,
                    EE_SCENARIO_UV_CONTAINER => u.container,
                    EE_SCENARIO_UV_ID => a[0],
                    EE_SCENARIO_UV_ON_TILE => (u.x != -10000) as i64,
                    f => u.flags.get(&f).copied().unwrap_or(0),
                };
            }
            EE_SCENARIO_WORLD_CURRENT_PLAYER => return self.current,
            EE_SCENARIO_WORLD_OBJECT_EVALUATE => {
                self.log.push(format!("evaluate {}", a[0]));
                return self.objects.contains_key(&(a[0] as i32)) as i64;
            }
            EE_SCENARIO_WORLD_GROUP_NEW => self.group.clear(),
            EE_SCENARIO_WORLD_GROUP_ADD | EE_SCENARIO_WORLD_GROUP_ADD_TASKED => self.group.push(a[0]),
            EE_SCENARIO_WORLD_GROUP_COUNT => return self.group.len() as i64,
            EE_SCENARIO_WORLD_GROUP_UNIT => return self.group[a[0] as usize],
            EE_SCENARIO_WORLD_GROUP_FIRST => return self.group.first().copied().unwrap_or(-1),
            _ => {}
        }
        let args: Vec<String> = a.iter().rev().skip_while(|&&v| v == 0).map(|v| v.to_string()).collect();
        let args: Vec<String> = args.into_iter().rev().collect();
        self.log.push(format!("{op} {}", args.join(" ")).trim_end().to_string());
        self.answers.get(&op).copied().unwrap_or(0)
    }
}

fn effect(ty: c_ulong) -> EeScenarioEffectFields {
    let mut e = effect_defaults();
    e.mType = ty;
    e
}

/// The log's world calls of `op` (as "op args").
fn of(w: &World, op: u32) -> Vec<String> {
    let prefix = format!("{op}");
    w.log.iter().filter(|l| l.split(' ').next() == Some(&prefix)).cloned().collect()
}

#[test]
fn victory_ends_the_chosen_players_game_and_stops_their_ai() {
    let mut w = World::new(4);
    w.answers.insert(EE_SCENARIO_WORLD_PLAYER_OBSERVER, 0);
    let mut e = effect(2);
    e.mPlayerAction = 0;
    e.mPlayer = 4; // every computer player
    execute(&mut w, &e, false);
    let r = EE_SCENARIO_WORLD_PLAYER_RESULT;
    let a = EE_SCENARIO_WORLD_COMPUTER_SET_ACTIVE;
    assert_eq!(of(&w, r), vec![format!("{r} 2 1"), format!("{r} 3 1")]);
    assert_eq!(of(&w, a), vec![format!("{a} 2"), format!("{a} 3")]);
}

#[test]
fn diplomacy_skips_the_player_itself_and_notices_only_enemies() {
    let mut w = World::new(4);
    let mut e = effect(2);
    e.mPlayerAction = 3;
    e.mPlayer = 8; // player 2
    e.mDiplomacyPlayer = 4; // every computer player: 2 and 3
    e.mDiplomacy = 0;
    execute(&mut w, &e, false);
    let s = EE_SCENARIO_WORLD_SET_STANCE;
    let n = EE_SCENARIO_WORLD_NOTICE_TARGET;
    assert_eq!(of(&w, s), vec![format!("{s} 2 3")]);
    assert_eq!(of(&w, n), vec![format!("{n} 2 3")]);
}

#[test]
fn trigger_states_write_the_trigger() {
    for (state, want) in
        [(0, vec!["54 9", "54 9 1 1"]), (1, vec!["54 9 1"]), (2, vec!["54 9 2 1"]), (3, vec!["54 9 2"])]
    {
        let mut w = World::new(2);
        let mut e = effect(3);
        e.mTrigger = 9;
        e.mTriggerState = state;
        execute(&mut w, &e, false);
        assert_eq!(w.calls(), want);
    }
}

#[test]
fn daylight_hour_and_cinematic_speed_as_the_original_rounds_them() {
    let mut w = World::new(2);
    let mut e = effect(5);
    e.mGameAction = 0;
    e.mDaylightHour = 9;
    execute(&mut w, &e, false);
    let want = (3.0f32 * (1.0f32 / 24.0)).to_bits();
    assert_eq!(w.calls(), vec![format!("62 {want}")]);
    let mut w = World::new(2);
    e.mGameAction = 4;
    e.mDaylightHour = 7;
    execute(&mut w, &e, false);
    let speed = 7.0f32 * 0.1f32 + 0.1f32;
    assert_eq!(w.calls(), vec!["77".to_string(), format!("78 {}", speed.to_bits())]);
}

#[test]
fn end_cinematic_waits_for_the_dialogue() {
    let mut w = World::new(2);
    let mut e = effect(5);
    e.mGameAction = 5;
    execute(&mut w, &e, false);
    assert_eq!(w.calls(), vec!["9", "80"]);
    let mut w = World::new(2);
    w.answers.insert(EE_SCENARIO_WORLD_DIALOG_IDLE, 1);
    execute(&mut w, &e, false);
    assert_eq!(w.calls(), vec!["9", "79"]);
}

#[test]
fn kill_takes_only_living_units_and_evaluates_a_dynamic_object() {
    let mut w =
        World::new(2).object(3, &[10, 11, 12]).unit(10, living(1, 0, 0)).unit(11, Unit { hp: 0, ..living(1, 0, 0) });
    w.objects.get_mut(&3).unwrap().0.mIsDynamic = true;
    let mut e = effect(1);
    e.mObjectAction = 3;
    e.mObject = 3;
    execute(&mut w, &e, false);
    assert_eq!(w.calls(), vec!["evaluate 3", "21 10 1"]);
}

#[test]
fn freeze_circles_aircraft_closes_gates_and_leaves_walls() {
    let mut wall = living(1, 0, 0);
    wall.flags.insert(EE_SCENARIO_UV_WALL, 1);
    let mut w = World::new(2)
        .object(0, &[1, 2, 3])
        .unit(1, Unit { category: EE_SCENARIO_K_CATEGORY_AIRPLANE, command: 1000, ..living(1, 0, 0) })
        .unit(2, Unit { category: EE_SCENARIO_K_CATEGORY_GATE, ..living(1, 0, 0) })
        .unit(3, wall);
    let mut e = effect(1);
    e.mObjectAction = 1;
    execute(&mut w, &e, false);
    let actions = of(&w, EE_SCENARIO_WORLD_ACTION);
    assert_eq!(
        actions,
        vec![format!("24 1 {EE_SCENARIO_K_ACTION_CIRCLE}"), format!("24 2 {EE_SCENARIO_K_ACTION_CLOSING}")]
    );
    assert_eq!(of(&w, EE_SCENARIO_WORLD_SET_DISABLED), vec!["26 1 1", "26 2 1", "26 3 1"]);
    assert_eq!(of(&w, EE_SCENARIO_WORLD_SET_COMMANDABLE), vec!["27 1", "27 2", "27 3"]);
}

#[test]
fn owner_converts_farms_last_and_the_newest_first() {
    let farm = |id| Unit { ty: EE_SCENARIO_K_TYPE_FARM, ..living(1, id, 0) };
    let mut w = World::new(4)
        .object(0, &[5, 9, 7, 8])
        .unit(5, farm(5))
        .unit(9, farm(9))
        .unit(7, living(1, 0, 0))
        .unit(8, living(1, 0, 0));
    let mut e = effect(1);
    e.mObjectAction = 9;
    e.mNewOwner = 8; // player 2
    execute(&mut w, &e, false);
    let c = EE_SCENARIO_WORLD_CONVERT;
    assert_eq!(of(&w, c), vec![format!("{c} 8 2"), format!("{c} 7 2"), format!("{c} 9 2"), format!("{c} 5 2")]);
}

#[test]
fn class_attributes_go_once_to_each_type_and_player() {
    let mut w = World::new(4)
        .object(0, &[1, 2, 3])
        .unit(1, Unit { ty: 40, ..living(2, 0, 0) })
        .unit(2, Unit { ty: 40, ..living(2, 0, 0) })
        .unit(3, Unit { ty: 41, ..living(3, 0, 0) });
    w.objects.get_mut(&0).unwrap().0.mNearCheck = 1;
    let mut e = effect(1);
    e.mObjectAction = 12;
    e.mAttribute = 25;
    e.mAttributeAmount = 5;
    w.answers.insert(crate::scenario::class_attribute::EE_SCENARIO_WORLD_CA_UPGRADE, 1);
    execute(&mut w, &e, false);
    let m = crate::scenario::class_attribute::EE_SCENARIO_WORLD_CA_PENDING;
    assert_eq!(of(&w, m), vec![format!("{m} 2 40 25 0 5"), format!("{m} 3 41 25 0 5")]);
}

#[test]
fn a_location_calamity_strikes_the_first_shuffled_tile() {
    let mut w = World::new(2);
    w.tiles = vec![(1, 1), (2, 2), (3, 3)];
    w.draws = vec![0, 2]; // the shuffle: (2,2) to the front, then (3,3) stays
    w.answers.insert(EE_SCENARIO_WORLD_CAL_CLASS, 1);
    w.answers.insert(EE_SCENARIO_WORLD_CAL_TYPE, 2);
    w.answers.insert(EE_SCENARIO_WORLD_CAL_CAN_CAST, 1);
    w.answers.insert(EE_SCENARIO_WORLD_AREA_STATE, 0);
    let mut e = effect(5);
    e.mGameAction = 2;
    e.mPlacedEffect = 4;
    e.mUsesPlaceArea = true;
    e.mPlaceArea = 6;
    execute(&mut w, &e, false);
    assert_eq!(of(&w, EE_SCENARIO_WORLD_CAL_LOCATION), vec!["68 4 2 2"]);
}

#[test]
fn task_object_deals_units_round_robin_and_moves_each_player_group() {
    // three sources of players 2, 2, 3 and two targets of players 2 and 3 (ENEMY: any other player)
    let mut w = World::new(4)
        .object(0, &[1, 2, 3])
        .object(1, &[20, 21])
        .unit(1, living(2, 0, 0))
        .unit(2, living(2, 0, 0))
        .unit(3, living(3, 0, 0))
        .unit(20, living(2, 5, 6))
        .unit(21, living(3, 7, 8));
    let mut e = effect(1);
    e.mObjectAction = 5;
    e.mTaskObject = 1;
    e.mTaskAction = 0;
    execute(&mut w, &e, false);
    // target 20 takes sources 1 and 3 (players 2 and 3), target 21 takes source 2 (player 2)
    assert_eq!(of(&w, EE_SCENARIO_WORLD_GROUP_NEW), vec!["141 2", "141 3", "141 2"]);
    // another player's target is attacked, an own unit moved to
    assert_eq!(of(&w, EE_SCENARIO_WORLD_GROUP_ATTACK), vec!["150 20", "150 21"]);
    let (x5, y6) = (5.0f32.to_bits(), 6.0f32.to_bits());
    assert_eq!(of(&w, EE_SCENARIO_WORLD_GROUP_MOVE), vec![format!("174 {x5} {y6} 1")]);
}

#[test]
fn task_area_places_off_map_units_and_groups_the_rest() {
    let mut w = World::new(4).object(0, &[1, 2]).unit(1, living(2, 4, 4)).unit(2, living(2, -10000, -10000));
    w.tiles = vec![(9, 9), (10, 11)];
    w.draws = vec![1];
    w.answers.insert(EE_SCENARIO_WORLD_AREA_STATE, 0);
    let mut e = effect(1);
    e.mObjectAction = 6;
    e.mTaskArea = 2;
    e.mTaskAction = 1;
    execute(&mut w, &e, false);
    let (x, y) = (10.0f32.to_bits(), 11.0f32.to_bits());
    assert_eq!(of(&w, EE_SCENARIO_WORLD_AREA_LOCATION), vec![format!("184 {x} {y}")]);
    assert_eq!(of(&w, EE_SCENARIO_WORLD_AREA_PLACE), vec!["181 2"]);
    assert_eq!(of(&w, EE_SCENARIO_WORLD_GROUP_PATROL_TO), vec![format!("171 2 {x} {y} 1")]);
    assert_eq!(w.log.last().unwrap(), "183");
}

#[test]
fn task_area_with_no_tiles_does_nothing_more() {
    let mut w = World::new(4).object(0, &[1]).unit(1, living(2, 4, 4));
    w.answers.insert(EE_SCENARIO_WORLD_AREA_STATE, 0);
    let mut e = effect(1);
    e.mObjectAction = 6;
    execute(&mut w, &e, false);
    assert_eq!(w.calls(), vec!["6", "tiles 0"]);
}

#[test]
fn the_ai_military_effects_do_nothing() {
    // main's stage 5 deleted the C++ attack groups these effects raised and aimed
    for action in 0..3 {
        let mut w = World::new(4).object(5, &[30]).unit(30, living(1, 3, 3));
        w.answers.insert(EE_SCENARIO_WORLD_PLAYER_IS_COMPUTER, 1);
        let mut e = effect(6);
        e.mAIAction = action;
        e.mPlayer = 4;
        e.mTargetObject = 5;
        execute(&mut w, &e, false);
        assert!(w.calls().is_empty(), "action {action}: {:?}", w.calls());
    }
}

#[test]
fn script_camera_follows_camera_objects() {
    let cam = |x| Unit { ty: EE_SCENARIO_K_TYPE_CAMERA_OBJECT, ..living(0, x, 0) };
    let mut w = World::new(2).object(4, &[1, 2]).unit(1, cam(1)).unit(2, cam(2));
    w.current = 1;
    let mut e = effect(2);
    e.mPlayerAction = 1;
    e.mPlayer = 7; // player 1
    e.mTargetObject = 4;
    e.mLocationObject = 4;
    e.mIntValue1 = 500;
    e.mIntValue2 = 3;
    e.mUsesTargetArea = false;
    e.mUsesLocationArea = false;
    execute(&mut w, &e, false);
    // (zoom 3: level 2 * 0.02 - 1 is below 0, so the zoom's argument is 0)
    let keys = of(&w, EE_SCENARIO_WORLD_CAMERA_KEY);
    assert_eq!(keys, vec!["89 1".to_string(), "89 2 500".to_string()]);
    assert_eq!(of(&w, EE_SCENARIO_WORLD_CAMERA_ZOOM), vec!["88"]);
    assert_eq!(w.log.last().unwrap(), "90");
}

#[test]
fn script_camera_moves_between_units_without_camera_objects() {
    let mut w = World::new(2).object(4, &[1]).object(5, &[2]).unit(1, living(1, 0, 0)).unit(2, living(1, 0, 0));
    let mut e = effect(2);
    e.mPlayerAction = 1;
    e.mPlayer = 7;
    e.mTargetObject = 4;
    e.mLocationObject = 5;
    e.mIntValue1 = 100;
    e.mUsesTargetArea = false;
    e.mUsesLocationArea = false;
    execute(&mut w, &e, false);
    let level = (-1.1f32).to_bits();
    assert_eq!(of(&w, EE_SCENARIO_WORLD_CAMERA_MOVE), vec![format!("93 2 1 0 100 {level}")]);
}

#[test]
fn set_ai_variable_stores_on_computers_and_defers_per_player() {
    let mut w = World::new(4);
    let mut e = effect(6);
    e.mAIAction = 5;
    e.mPlayer = 4;
    e.mAIAttribute = 0;
    execute(&mut w, &e, false);
    let stores = of(&w, EE_SCENARIO_WORLD_AI_STORE);
    let mut writes = Vec::new();
    let deferred = crate::scenario::aivar::ai_variable(&e, &mut writes);
    assert_eq!(stores.len(), writes.len() * 2);
    assert_eq!(of(&w, EE_SCENARIO_WORLD_ADD_PENDING_PLAYER).len(), if deferred { 2 } else { 0 });
}

#[test]
fn dialogue_goes_to_the_current_player_and_an_empty_box_to_the_others() {
    let mut w = World::new(4);
    w.current = 2;
    let mut e = effect(4);
    e.mMediaAction = 2;
    e.mMessagePlayer = 4;
    execute(&mut w, &e, true);
    assert_eq!(w.calls(), vec!["57", "58 1", "59"]);
}
