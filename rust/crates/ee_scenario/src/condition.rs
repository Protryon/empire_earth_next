//! A trigger condition's test (`EESMCondition::Evaluate`, `Compare`, `GetPlayerValue`) and the scenario's player
//! choice (`EEScenarioManager::ChoosePlayer`) over the host's reads, in the original's order of reads and draws.

use crate::ftol;
use crate::host::*;

/// `kSMCondition*` (ee/EEScenarioIDs.h).
pub const EE_SCENARIO_CONDITION_ALWAYS_TRUE: u32 = 0;
pub const EE_SCENARIO_CONDITION_OBJECT: u32 = 1;
pub const EE_SCENARIO_CONDITION_PLAYER_ATTRIBUTE: u32 = 2;
pub const EE_SCENARIO_CONDITION_PLAYER_TECHNOLOGY: u32 = 3;
pub const EE_SCENARIO_CONDITION_TRIGGER_ATTRIBUTE: u32 = 4;
pub const EE_SCENARIO_CONDITION_PLAYER_DIPLOMACY: u32 = 5;
pub const EE_SCENARIO_CONDITION_GAME_ATTRIBUTE: u32 = 6;
pub const EE_SCENARIO_CONDITION_CHAT_CONTAINS: u32 = 7;

/// `kSMCompare*`: <, <=, ==, >=, >.
const EE_SCENARIO_K_LESS: u32 = 0;
const EE_SCENARIO_K_LESS_OR_EQUAL: u32 = 1;
const EE_SCENARIO_K_EQUAL: u32 = 2;
const EE_SCENARIO_K_GREATER_OR_EQUAL: u32 = 3;
const EE_SCENARIO_K_GREATER: u32 = 4;

/// `kResource*`: the player's resource indices.
const EE_SCENARIO_K_FOOD: i32 = 2;
const EE_SCENARIO_K_WOOD: i32 = 3;
const EE_SCENARIO_K_STONE: i32 = 4;
const EE_SCENARIO_K_GOLD: i32 = 5;
const EE_SCENARIO_K_IRON: i32 = 6;

/// `kSMConditionGameAttribute`'s time of day factor (0x44605c).
const EE_SCENARIO_K_HOURS_PER_DAY: f32 = -24.0;
/// `kSMPlayerValueMiningLimit`.
const EE_SCENARIO_K_VALUE_MINING_LIMIT: u32 = 61;

/// A condition's settings (`EESMCondition`'s plain members).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeScenarioCondition {
    pub kind: u32,
    pub op: u32,
    pub difficulty: u32,
    pub diplomacy: u32,
    pub attribute: u32,
    pub other_attribute: u32,
    pub technology: i32,
    pub amount: u32,
    pub object_index: u32,
    pub player: u32,
    pub other_player: u32,
    pub min_time: u32,
    pub max_time: u32,
    pub hour: u32,
    pub trigger: u32,
    pub is_trigger_on: bool,
    pub has_trigger_fired: bool,
    pub uses_difficulty: bool,
    pub uses_elapsed_time: bool,
    pub uses_hour: bool,
    pub uses_player_attribute: bool,
}

/// `EESMCondition::Compare`: the value against the target under the operator; false for an unknown operator.
pub fn compare(value: u32, op: u32, target: u32) -> bool {
    match op {
        EE_SCENARIO_K_LESS => value < target,
        EE_SCENARIO_K_LESS_OR_EQUAL => value <= target,
        EE_SCENARIO_K_EQUAL => value == target,
        EE_SCENARIO_K_GREATER_OR_EQUAL => value >= target,
        EE_SCENARIO_K_GREATER => value > target,
        _ => false,
    }
}

/// `EEAIRandomShuffle` (Dinkum's `random_shuffle` with the AI's generator): element k swaps with a draw of
/// `GetRandom(0, k)` for k from 1, the first element left alone.
pub fn shuffle<H: Host>(host: &mut H, values: &mut [i32]) {
    for at in 1..values.len() {
        // the generator's `max(n - 1, 0)` of n = at + 1
        let pick = host.read(EE_SCENARIO_READ_RANDOM, 0, at as i32) as usize;
        values.swap(at, pick);
    }
}

/// `EEScenarioManager::ChoosePlayer` with its capture record (`listed`: whether the caller takes the players).
pub fn choose_player_recorded<H: Host>(host: &mut H, kind: i32, single: bool, listed: bool) -> (i32, Vec<i32>) {
    let seed = host.read(EE_SCENARIO_READ_CAPTURE_SEED, 0, 0);
    let (result, players) = choose_player(host, kind, single);
    if seed >= 0 {
        host.players(kind, single, result, if listed { Some(&players) } else { None }, seed as u32);
    }
    (result, players)
}

/// `EEScenarioManager::ChoosePlayer`: a scenario player choice (0 nature, 1/3 a random/every human, 2/4 a
/// random/every computer, 5/6 a random player with/without nature, 7+ player n-6) as the players and the chosen one
/// (-1 for none or a group).
pub fn choose_player<H: Host>(host: &mut H, kind: i32, single: bool) -> (i32, Vec<i32>) {
    let count = host.read(EE_SCENARIO_READ_NUM_PLAYERS, 0, 0) as u32;
    let mut result = -1i32;
    let mut players = Vec::new();
    match kind {
        0 => {
            result = 0;
            players.push(0);
        }
        1..=4 => {
            let op =
                if kind == 1 || kind == 3 { EE_SCENARIO_READ_PLAYER_HUMAN } else { EE_SCENARIO_READ_PLAYER_COMPUTER };
            for i in 1..count {
                if host.read(op, i as i32, 0) != 0 {
                    players.push(i as i32);
                }
            }
            if !players.is_empty() && (kind == 1 || kind == 2) {
                shuffle(host, &mut players);
                result = players[0];
                if single {
                    players = vec![result];
                }
            }
        }
        5 | 6 => {
            let first = if kind == 5 { 0 } else { 1 };
            for i in first..count {
                players.push(i as i32);
            }
            result = host.read(EE_SCENARIO_READ_RANDOM, first as i32, count.wrapping_sub(1) as i32) as i32;
            if single {
                players = vec![result];
            }
        }
        _ => {
            result = kind.wrapping_sub(6);
            if result < count as i32 {
                players.push(result);
            } else {
                result = -1;
            }
        }
    }
    (result, players)
}

/// `EESMCondition::GetPlayerValue`: a player's value for a condition (a resource, a random one, population,
/// units, difficulty, a scenario variable, tribute, a statistic, the mining limit); 0 without the player.
pub fn player_value<H: Host>(host: &mut H, player: i32, kind: u32) -> u32 {
    if host.read(EE_SCENARIO_READ_PLAYER_EXISTS, player, 0) == 0 {
        return 0;
    }
    let resource = |host: &mut H, r: i32| host.read(EE_SCENARIO_READ_RESOURCE, player, r) as i32;
    let tribute = |host: &mut H, t: i32| host.read(EE_SCENARIO_READ_TRIBUTE, player, t) as u32;
    match kind {
        0 => {
            let pick = host.read(EE_SCENARIO_READ_RANDOM, 2, 6) as u32;
            player_value(host, player, pick)
        }
        1 => {
            let sum = resource(host, EE_SCENARIO_K_IRON)
                .wrapping_add(resource(host, EE_SCENARIO_K_GOLD))
                .wrapping_add(resource(host, EE_SCENARIO_K_STONE))
                .wrapping_add(resource(host, EE_SCENARIO_K_WOOD))
                .wrapping_add(resource(host, EE_SCENARIO_K_FOOD));
            sum as u32
        }
        2 => resource(host, EE_SCENARIO_K_STONE) as u32,
        3 => resource(host, EE_SCENARIO_K_GOLD) as u32,
        4 => resource(host, EE_SCENARIO_K_WOOD) as u32,
        5 => resource(host, EE_SCENARIO_K_FOOD) as u32,
        6 => resource(host, EE_SCENARIO_K_IRON) as u32,
        7 => host.read(EE_SCENARIO_READ_POPULATION, player, 0) as u32,
        8 => host.read(EE_SCENARIO_READ_POPULATION_LIMIT, player, 0) as u32,
        9 => host.read(EE_SCENARIO_READ_UNIT_COUNT, player, 0) as u32,
        10 => {
            let d = host.read(EE_SCENARIO_READ_COMPUTER_DIFFICULTY, player, 0);
            if d < 0 {
                0
            } else {
                d as u32
            }
        }
        11..=20 => host.read(EE_SCENARIO_READ_VARIABLE, player, (kind - 11) as i32) as u32,
        // the tributes: food, wood, stone, gold, iron sent [0..4], received [5..9]
        21 => tribute(host, 0),
        22 => tribute(host, 5),
        23 => tribute(host, 1),
        24 => tribute(host, 6),
        25 => tribute(host, 2),
        26 => tribute(host, 7),
        27 => tribute(host, 3),
        28 => tribute(host, 8),
        29 => tribute(host, 4),
        30 => tribute(host, 9),
        _ => {
            if kind.wrapping_sub(30) < 31 {
                host.read(EE_SCENARIO_READ_STATISTIC, player, (kind - 30) as i32) as u32
            } else if kind == EE_SCENARIO_K_VALUE_MINING_LIMIT {
                host.read(EE_SCENARIO_READ_MINING_LIMIT, player, 0) as u32
            } else {
                0
            }
        }
    }
}

/// `EESMCondition::Evaluate`: whether the condition holds now.
pub fn evaluate<H: Host>(host: &mut H, c: &EeScenarioCondition) -> bool {
    let mut result = false;
    match c.kind {
        EE_SCENARIO_CONDITION_ALWAYS_TRUE => result = true,
        EE_SCENARIO_CONDITION_OBJECT => {
            result = host.read(EE_SCENARIO_READ_OBJECT, c.object_index as i32, 0) == 1;
        }
        EE_SCENARIO_CONDITION_PLAYER_ATTRIBUTE
        | EE_SCENARIO_CONDITION_PLAYER_TECHNOLOGY
        | EE_SCENARIO_CONDITION_PLAYER_DIPLOMACY => result = evaluate_players(host, c),
        EE_SCENARIO_CONDITION_TRIGGER_ATTRIBUTE => {
            result = true;
            // a missing trigger has neither fired nor is on
            if c.has_trigger_fired {
                let t = host.read(EE_SCENARIO_READ_TRIGGER, c.trigger as i32, 0);
                result = t >= 0 && t & 1 != 0;
            }
            if c.is_trigger_on {
                let t = host.read(EE_SCENARIO_READ_TRIGGER, c.trigger as i32, 0);
                result = t >= 0 && t & 2 != 0;
            }
        }
        EE_SCENARIO_CONDITION_GAME_ATTRIBUTE => {
            let mut time_result = true;
            if c.uses_elapsed_time {
                let time = f32::from_bits(host.read(EE_SCENARIO_READ_TIME, 0, 0) as u32) * 0.001;
                result = c.min_time as f32 <= time && c.max_time as f32 >= time;
                time_result = result;
            }
            if c.uses_hour && time_result {
                let day = f32::from_bits(host.read(EE_SCENARIO_READ_TIME_OF_DAY, 0, 0) as u32);
                // `unsigned long theHour = (long)(...)`: the cast's 32-bit result, then unsigned arithmetic
                let hour = ftol(day * EE_SCENARIO_K_HOURS_PER_DAY) as u32;
                result = (30u32.wrapping_sub(hour) % 24) >= c.hour;
                time_result = result;
            }
            if c.uses_difficulty && time_result {
                result = host.read(EE_SCENARIO_READ_DIFFICULTY, 0, 0) as i32 == c.difficulty as i32;
            }
        }
        EE_SCENARIO_CONDITION_CHAT_CONTAINS => {
            result = host.read(EE_SCENARIO_READ_CHAT_CONTAINS, 0, 0) != 0;
        }
        _ => {}
    }
    result
}

/// The player conditions (attribute, technology, diplomacy) for any or all of the chosen players.
fn evaluate_players<H: Host>(host: &mut H, c: &EeScenarioCondition) -> bool {
    let (chosen, players) = choose_player_recorded(host, c.player as i32, false, true);
    let any = chosen == -1;
    let mut result = false;
    for (i, &p) in players.iter().enumerate() {
        let mut player_result = false;
        if any && result {
            break;
        }
        if c.kind == EE_SCENARIO_CONDITION_PLAYER_DIPLOMACY {
            if host.read(EE_SCENARIO_READ_PLAYER_EXISTS, p, 0) != 0 {
                let (other_chosen, others) = choose_player_recorded(host, c.other_player as i32, false, true);
                let any_other = other_chosen == -1;
                for (j, &q) in others.iter().enumerate() {
                    if any_other {
                        player_result = host.read(EE_SCENARIO_READ_ENEMY, p, q) as u32 == c.diplomacy;
                        if player_result {
                            break;
                        }
                    } else {
                        if (player_result || j == 0) && host.read(EE_SCENARIO_READ_ENEMY, p, q) as u32 == c.diplomacy {
                            player_result = true;
                            continue;
                        }
                        player_result = false;
                    }
                }
            }
        } else if c.kind == EE_SCENARIO_CONDITION_PLAYER_ATTRIBUTE {
            let value = player_value(host, p, c.attribute);
            if c.uses_player_attribute {
                let (other_chosen, others) = choose_player_recorded(host, c.other_player as i32, false, true);
                let any_other = other_chosen == -1;
                player_result = false;
                for (j, &q) in others.iter().enumerate() {
                    let other_value = player_value(host, q, c.other_attribute);
                    if any_other {
                        player_result = compare(value, c.op, other_value);
                        if player_result {
                            break;
                        }
                    } else {
                        player_result = (player_result || j == 0) && compare(value, c.op, other_value);
                    }
                }
            } else {
                player_result = compare(value, c.op, c.amount);
            }
        } else if c.kind == EE_SCENARIO_CONDITION_PLAYER_TECHNOLOGY
            && host.read(EE_SCENARIO_READ_PLAYER_EXISTS, p, 0) != 0
        {
            let tech = c.technology;
            if (tech as i64) < host.read(EE_SCENARIO_READ_EPOCH_COUNT, 0, 0) {
                player_result = host.read(EE_SCENARIO_READ_EPOCH, p, 0) >= tech as i64;
            } else {
                let node = host.read(EE_SCENARIO_READ_TECH_NODE, p, tech);
                if node >= 0 && node & 3 != 0 {
                    player_result = host.read(EE_SCENARIO_READ_TECH_AVAILABLE, p, tech) != 0;
                }
            }
        }
        if any {
            result = player_result;
            if player_result {
                break;
            }
        } else {
            result = (result || i == 0) && player_result;
        }
    }
    result
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::collections::BTreeMap;

    /// A scripted world: players with flags and values, a random stream, a log of every read.
    #[derive(Default)]
    struct World {
        players: u32,
        exists: Vec<bool>,
        human: Vec<bool>,
        computer: Vec<bool>,
        values: BTreeMap<(u32, i32, i32), i64>,
        draws: Vec<i64>,
        log: Vec<(u32, i32, i32)>,
    }

    impl Host for World {
        fn read4(&mut self, op: u32, a: i32, b: i32, _c: i32, _d: i32) -> i64 {
            self.log.push((op, a, b));
            match op {
                EE_SCENARIO_READ_NUM_PLAYERS => self.players as i64,
                EE_SCENARIO_READ_PLAYER_EXISTS => *self.exists.get(a as usize).unwrap_or(&false) as i64,
                EE_SCENARIO_READ_PLAYER_HUMAN => self.human[a as usize] as i64,
                EE_SCENARIO_READ_PLAYER_COMPUTER => self.computer[a as usize] as i64,
                EE_SCENARIO_READ_CAPTURE_SEED => -1,
                EE_SCENARIO_READ_RANDOM => {
                    let d = self.draws.remove(0);
                    assert!(d >= a as i64 && d <= b as i64, "draw {d} outside {a}..={b}");
                    d
                }
                _ => *self.values.get(&(op, a, b)).unwrap_or(&0),
            }
        }
    }

    fn world() -> World {
        World {
            players: 4,
            exists: vec![true, true, true, false],
            human: vec![false, true, false, false],
            computer: vec![false, false, true, true],
            ..World::default()
        }
    }

    #[test]
    fn compare_takes_the_five_operators_and_nothing_else() {
        assert!(compare(1, 0, 2) && !compare(2, 0, 2));
        assert!(compare(2, 1, 2) && !compare(3, 1, 2));
        assert!(compare(2, 2, 2) && !compare(1, 2, 2));
        assert!(compare(2, 3, 2) && !compare(1, 3, 2));
        assert!(compare(3, 4, 2) && !compare(2, 4, 2));
        assert!(!compare(2, 5, 2) && !compare(0, u32::MAX, 0));
    }

    #[test]
    fn the_shuffle_draws_from_one_up_as_random_shuffle() {
        let mut w = World { draws: vec![0, 2, 1], ..World::default() };
        let mut v = [10, 20, 30, 40];
        shuffle(&mut w, &mut v);
        // at 1: swap with 0; at 2: with 2; at 3: with 1
        assert_eq!(v, [20, 40, 30, 10]);
        assert_eq!(
            w.log,
            vec![(EE_SCENARIO_READ_RANDOM, 0, 1), (EE_SCENARIO_READ_RANDOM, 0, 2), (EE_SCENARIO_READ_RANDOM, 0, 3)]
        );
        let mut one = [7];
        shuffle(&mut w, &mut one);
        assert_eq!(w.log.len(), 3);
    }

    #[test]
    fn choose_player_covers_every_kind() {
        let mut w = world();
        assert_eq!(choose_player(&mut w, 0, false), (0, vec![0]));
        assert_eq!(choose_player(&mut w, 3, false), (-1, vec![1]));
        assert_eq!(choose_player(&mut w, 4, true), (-1, vec![2, 3]));
        w.draws = vec![0];
        assert_eq!(choose_player(&mut w, 2, false), (3, vec![3, 2]));
        w.draws = vec![1];
        assert_eq!(choose_player(&mut w, 2, true), (2, vec![2]));
        // one human: no draw
        let before = w.log.len();
        assert_eq!(choose_player(&mut w, 1, true), (1, vec![1]));
        assert!(!w.log[before..].iter().any(|r| r.0 == EE_SCENARIO_READ_RANDOM));
        w.draws = vec![2];
        assert_eq!(choose_player(&mut w, 5, false), (2, vec![0, 1, 2, 3]));
        w.draws = vec![3];
        assert_eq!(choose_player(&mut w, 6, true), (3, vec![3]));
        assert_eq!(choose_player(&mut w, 8, false), (2, vec![2]));
        assert_eq!(choose_player(&mut w, 10, false), (-1, vec![]));
        // a negative choice is "player n - 6" as the original computes it
        assert_eq!(choose_player(&mut w, -1, false), (-7, vec![-7]));
        assert!(w.draws.is_empty());
    }

    #[test]
    fn player_values_read_the_original_fields() {
        let mut w = world();
        for (r, v) in [(2, 10), (3, 20), (4, 30), (5, 40), (6, 50)] {
            w.values.insert((EE_SCENARIO_READ_RESOURCE, 1, r), v);
        }
        assert_eq!(player_value(&mut w, 1, 1), 150);
        assert_eq!(player_value(&mut w, 1, 2), 30);
        assert_eq!(player_value(&mut w, 1, 3), 40);
        assert_eq!(player_value(&mut w, 1, 4), 20);
        assert_eq!(player_value(&mut w, 1, 5), 10);
        assert_eq!(player_value(&mut w, 1, 6), 50);
        w.draws = vec![5];
        assert_eq!(player_value(&mut w, 1, 0), 10);
        // no player: no draw
        assert_eq!(player_value(&mut w, 3, 0), 0);
        w.values.insert((EE_SCENARIO_READ_COMPUTER_DIFFICULTY, 2, 0), 2);
        w.values.insert((EE_SCENARIO_READ_COMPUTER_DIFFICULTY, 1, 0), -1);
        assert_eq!(player_value(&mut w, 2, 10), 2);
        assert_eq!(player_value(&mut w, 1, 10), 0);
        w.values.insert((EE_SCENARIO_READ_TRIBUTE, 1, 7), 77);
        assert_eq!(player_value(&mut w, 1, 26), 77);
        w.values.insert((EE_SCENARIO_READ_VARIABLE, 1, 9), 99);
        assert_eq!(player_value(&mut w, 1, 20), 99);
        w.values.insert((EE_SCENARIO_READ_STATISTIC, 1, 30), 5);
        assert_eq!(player_value(&mut w, 1, 60), 5);
        w.values.insert((EE_SCENARIO_READ_MINING_LIMIT, 1, 0), 12);
        assert_eq!(player_value(&mut w, 1, 61), 12);
        assert_eq!(player_value(&mut w, 1, 62), 0);
        // the sum wraps as the C++ longs do
        w.values.insert((EE_SCENARIO_READ_RESOURCE, 2, 6), -1);
        assert_eq!(player_value(&mut w, 2, 1), u32::MAX);
    }

    fn condition(kind: u32) -> EeScenarioCondition {
        EeScenarioCondition { kind, player: 9, other_player: 8, op: 2, ..EeScenarioCondition::default() }
    }

    #[test]
    fn attribute_conditions_compare_against_the_amount_or_other_players() {
        let mut w = world();
        w.values.insert((EE_SCENARIO_READ_RESOURCE, 3, 4), 30);
        w.exists[3] = true;
        let mut c = condition(EE_SCENARIO_CONDITION_PLAYER_ATTRIBUTE);
        c.player = 9; // player 3
        c.attribute = 2;
        c.amount = 30;
        assert!(evaluate(&mut w, &c));
        c.amount = 31;
        assert!(!evaluate(&mut w, &c));
        // against player 2's stone, every other of a group (all computers: 2 and 3)
        c.uses_player_attribute = true;
        c.other_player = 4;
        c.other_attribute = 2;
        c.op = 4; // >
        w.values.insert((EE_SCENARIO_READ_RESOURCE, 2, 4), 20);
        assert!(evaluate(&mut w, &c));
        w.values.insert((EE_SCENARIO_READ_RESOURCE, 2, 4), 40);
        assert!(!evaluate(&mut w, &c));
    }

    #[test]
    fn group_conditions_need_any_player_and_a_random_one_every_player() {
        let mut w = world();
        w.exists[3] = true;
        let mut c = condition(EE_SCENARIO_CONDITION_PLAYER_DIPLOMACY);
        c.player = 4; // every computer: a group (the choice -1), so any of them
        c.other_player = 7; // player 1
        c.diplomacy = 1;
        assert!(!evaluate(&mut w, &c));
        w.values.insert((EE_SCENARIO_READ_ENEMY, 3, 1), 1);
        assert!(evaluate(&mut w, &c));
        // a random player with nature, not single: one chosen, so every listed player must hold
        c.player = 5;
        w.draws = vec![3];
        assert!(!evaluate(&mut w, &c));
        for p in 0..3 {
            w.values.insert((EE_SCENARIO_READ_ENEMY, p, 1), 1);
        }
        w.draws = vec![1];
        assert!(evaluate(&mut w, &c));
        assert!(w.draws.is_empty());
    }

    #[test]
    fn technology_conditions_take_epochs_then_nodes() {
        let mut w = world();
        let mut c = condition(EE_SCENARIO_CONDITION_PLAYER_TECHNOLOGY);
        c.player = 7;
        w.values.insert((EE_SCENARIO_READ_EPOCH_COUNT, 0, 0), 15);
        w.values.insert((EE_SCENARIO_READ_EPOCH, 1, 0), 4);
        c.technology = 4;
        assert!(evaluate(&mut w, &c));
        c.technology = 5;
        assert!(!evaluate(&mut w, &c));
        c.technology = 100;
        w.values.insert((EE_SCENARIO_READ_TECH_NODE, 1, 100), 0);
        w.values.insert((EE_SCENARIO_READ_TECH_AVAILABLE, 1, 100), 1);
        assert!(!evaluate(&mut w, &c));
        w.values.insert((EE_SCENARIO_READ_TECH_NODE, 1, 100), 2);
        assert!(evaluate(&mut w, &c));
        w.values.insert((EE_SCENARIO_READ_TECH_NODE, 1, 100), -1);
        assert!(!evaluate(&mut w, &c));
    }

    #[test]
    fn game_attributes_chain_time_hour_and_difficulty() {
        let mut w = world();
        let mut c = condition(EE_SCENARIO_CONDITION_GAME_ATTRIBUTE);
        assert!(!evaluate(&mut w, &c));
        c.uses_elapsed_time = true;
        c.min_time = 10;
        c.max_time = 20;
        w.values.insert((EE_SCENARIO_READ_TIME, 0, 0), 15000.0f32.to_bits() as i64);
        assert!(evaluate(&mut w, &c));
        w.values.insert((EE_SCENARIO_READ_TIME, 0, 0), 25000.0f32.to_bits() as i64);
        assert!(!evaluate(&mut w, &c));
        c.uses_elapsed_time = false;
        c.uses_hour = true;
        // time of day 0.25: (long)(-6.0) = -6; (30 + 6) % 24 = 12
        w.values.insert((EE_SCENARIO_READ_TIME_OF_DAY, 0, 0), 0.25f32.to_bits() as i64);
        c.hour = 12;
        assert!(evaluate(&mut w, &c));
        c.hour = 13;
        assert!(!evaluate(&mut w, &c));
        c.hour = 0;
        c.uses_difficulty = true;
        c.difficulty = 2;
        w.values.insert((EE_SCENARIO_READ_DIFFICULTY, 0, 0), 2);
        assert!(evaluate(&mut w, &c));
        w.values.insert((EE_SCENARIO_READ_DIFFICULTY, 0, 0), 1);
        assert!(!evaluate(&mut w, &c));
    }

    #[test]
    fn trigger_attributes_test_fired_then_on() {
        let mut w = world();
        let mut c = condition(EE_SCENARIO_CONDITION_TRIGGER_ATTRIBUTE);
        c.trigger = 3;
        assert!(evaluate(&mut w, &c));
        c.has_trigger_fired = true;
        w.values.insert((EE_SCENARIO_READ_TRIGGER, 3, 0), 1);
        assert!(evaluate(&mut w, &c));
        c.is_trigger_on = true;
        assert!(!evaluate(&mut w, &c));
        w.values.insert((EE_SCENARIO_READ_TRIGGER, 3, 0), -1);
        c.is_trigger_on = false;
        assert!(!evaluate(&mut w, &c));
    }
}
