//! `EESMEffect::SetPlayerAttribute` as a plan: the Rust decides each write from the host's reads (the player's
//! resources, share bonus, AI difficulty, scenario variables, tributes, mining limit) and the C++ makes it
//! (`EE_SCENARIO_WRITE_*`, one statement of the original each). The any and all resources cases go back through
//! the C++ entry for each resource, as the original recursed through the member (so each call keeps its own capture
//! record).

#![allow(clippy::unnecessary_cast)]

use std::os::raw::{c_long, c_ulong};

use crate::scenario::host::{
    Host, EE_SCENARIO_READ_COMPUTER_DIFFICULTY, EE_SCENARIO_READ_MINING_LIMIT, EE_SCENARIO_READ_PLAYER_EXISTS,
    EE_SCENARIO_READ_PLAYER_ID, EE_SCENARIO_READ_RANDOM, EE_SCENARIO_READ_RESOURCE, EE_SCENARIO_READ_SHARE_BONUS,
    EE_SCENARIO_READ_TRIBUTE, EE_SCENARIO_READ_VARIABLE, EE_SCENARIO_READ_WORLD_POPULATION_LIMIT,
};

/// `SetPlayerAttribute(player, b, c, d)` again, through the C++ entry (the any/all resources' recursion).
pub const EE_SCENARIO_WRITE_SET_ATTRIBUTE: u32 = 0;
/// `mResourceCaps[b] = c`.
pub const EE_SCENARIO_WRITE_RESOURCE: u32 = 1;
/// `mShareBonus = b`.
pub const EE_SCENARIO_WRITE_SHARE_BONUS: u32 = 2;
/// A copy of the effect with `mPlayer = b`, `mPlayerAttributeMode = 0`, `mPlayerAttributeAmount = c` handed to
/// `AddV134Effect` (the population cap a saved game replays).
pub const EE_SCENARIO_WRITE_PENDING_POPULATION: u32 = 3;
/// `mComputerPlayer->mDifficulty = b`.
pub const EE_SCENARIO_WRITE_DIFFICULTY: u32 = 4;
/// `mScenarioVariables[b] = c`.
pub const EE_SCENARIO_WRITE_VARIABLE: u32 = 5;
/// `mTributes[b] = c`.
pub const EE_SCENARIO_WRITE_TRIBUTE: u32 = 6;
/// `AddTribute(b, c, d)`.
pub const EE_SCENARIO_WRITE_ADD_TRIBUTE: u32 = 7;
/// `mSavedGroups.Reset(b, c)` (the statistics).
pub const EE_SCENARIO_WRITE_STATISTIC_ADD: u32 = 8;
/// `mSavedGroups.Set(b, c)`.
pub const EE_SCENARIO_WRITE_STATISTIC_SET: u32 = 9;
/// `mMiningLimit = b`.
pub const EE_SCENARIO_WRITE_MINING_LIMIT: u32 = 10;

/// The value modes (`kSMValueMode*`).
const MODE_EXACTLY: c_long = 0;
const MODE_DECREASE: c_long = 2;

/// The tribute attributes (21..30): the `mTributes` index, the direction (0 sent, 1 received), the resource.
fn tribute(attribute: c_ulong) -> (i64, i64, i64) {
    let k = (attribute - 21) as i64;
    let direction = k % 2;
    // food, wood, stone, gold, iron
    let resource = 2 + k / 2;
    (direction * 5 + resource - 2, direction, resource)
}

/// `EESMEffect::SetPlayerAttribute(player, attribute, mode, amount)`.
pub fn set_player_attribute<H: Host>(h: &mut H, player: c_long, attribute: c_ulong, mode: c_long, amount: c_long) {
    let mut a = amount;
    if mode == MODE_DECREASE {
        a = a.wrapping_neg();
    }
    let p = player as i32;
    if h.read(EE_SCENARIO_READ_PLAYER_EXISTS, p, 0) == 0 {
        return;
    }
    let exact = mode == MODE_EXACTLY;
    // a field set to the amount, or raised by it (wrapping as the C++ `long`)
    let level = |old: c_long| if exact { a } else { old.wrapping_add(a) };
    match attribute {
        0 => {
            let resource = h.read(EE_SCENARIO_READ_RANDOM, 2, 6);
            h.write(EE_SCENARIO_WRITE_SET_ATTRIBUTE, player, resource, mode as i64, amount as i64);
        }
        1 => {
            for resource in 2..7 {
                h.write(EE_SCENARIO_WRITE_SET_ATTRIBUTE, player, resource, mode as i64, amount as i64);
            }
        }
        2..=6 => {
            // stone, gold, wood, food, iron (kResource*)
            let index = [4, 5, 3, 2, 6][(attribute - 2) as usize];
            let old = h.read(EE_SCENARIO_READ_RESOURCE, p, index) as c_long;
            h.write(EE_SCENARIO_WRITE_RESOURCE, player, index as i64, level(old) as i64, 0);
        }
        7 | 9 => {}
        8 => {
            let world = h.read(EE_SCENARIO_READ_WORLD_POPULATION_LIMIT, 0, 0) as c_long;
            let limit = if exact {
                a.wrapping_sub(world)
            } else {
                (h.read(EE_SCENARIO_READ_SHARE_BONUS, p, 0) as c_long).wrapping_add(a)
            };
            h.write(EE_SCENARIO_WRITE_SHARE_BONUS, player, limit as i64, 0, 0);
            let id = h.read(EE_SCENARIO_READ_PLAYER_ID, p, 0) as c_ulong;
            let world = h.read(EE_SCENARIO_READ_WORLD_POPULATION_LIMIT, 0, 0) as c_long;
            h.write(
                EE_SCENARIO_WRITE_PENDING_POPULATION,
                player,
                id.wrapping_add(6) as i64,
                world.wrapping_add(limit) as i64,
                0,
            );
        }
        10 => {
            let difficulty = h.read(EE_SCENARIO_READ_COMPUTER_DIFFICULTY, p, 0);
            if difficulty >= 0 {
                let mut v = if exact { a } else { a.wrapping_add(difficulty as c_ulong as c_long) };
                if v >= 3 {
                    v = 2;
                }
                if !exact && v < 0 {
                    v = 0;
                }
                h.write(EE_SCENARIO_WRITE_DIFFICULTY, player, v as i64, 0, 0);
            }
        }
        11..=20 => {
            let index = (attribute - 11) as i32;
            let old = h.read(EE_SCENARIO_READ_VARIABLE, p, index) as c_ulong;
            let new = if exact { a as c_ulong } else { old.wrapping_add(a as c_ulong) };
            h.write(EE_SCENARIO_WRITE_VARIABLE, player, index as i64, new as i64, 0);
        }
        21..=30 => {
            let (index, direction, resource) = tribute(attribute);
            if exact {
                let old = h.read(EE_SCENARIO_READ_TRIBUTE, p, index as i32) as c_ulong as c_long;
                let series = if direction == 0 { 0x11 } else { 0x12 };
                h.write(EE_SCENARIO_WRITE_STATISTIC_ADD, player, series, a.wrapping_sub(old) as i64, 0);
                h.write(EE_SCENARIO_WRITE_TRIBUTE, player, index, a as c_ulong as i64, 0);
            } else {
                h.write(EE_SCENARIO_WRITE_ADD_TRIBUTE, player, direction, resource, a as i64);
            }
        }
        _ => {
            if attribute.wrapping_sub(30) < 31 {
                let series = attribute.wrapping_sub(30) as c_long as i64;
                let op = if exact { EE_SCENARIO_WRITE_STATISTIC_SET } else { EE_SCENARIO_WRITE_STATISTIC_ADD };
                h.write(op, player, series, a as i64, 0);
            } else if attribute == 61 {
                let old = h.read(EE_SCENARIO_READ_MINING_LIMIT, p, 0) as c_long;
                h.write(EE_SCENARIO_WRITE_MINING_LIMIT, player, level(old) as i64, 0, 0);
            }
        }
    }
}

#[cfg(test)]
#[allow(clippy::field_reassign_with_default)]
mod tests {
    use super::*;
    use crate::scenario::host::Host;
    use std::fmt::Write as _;

    /// The driver's stand-in world (golden_player_attribute.txt, gen_pa_driver.py): player slot 3 (none, a human,
    /// a computer at difficulty 1), the world's population limit 200, a random stream `a + seq % (b - a + 1)`.
    struct Mock {
        exists: bool,
        computer: Option<c_ulong>,
        resources: [c_long; 7],
        share: c_long,
        variables: [c_ulong; 10],
        tributes: [c_ulong; 10],
        mining: c_long,
        seq: u32,
        log: String,
    }

    impl Mock {
        fn new(kind: u32) -> Self {
            Mock {
                exists: kind > 0,
                computer: (kind == 2).then_some(1),
                resources: std::array::from_fn(|i| 1000 + 10 * i as c_long),
                share: 5,
                variables: std::array::from_fn(|i| 100 + i as c_ulong),
                tributes: std::array::from_fn(|i| 200 + i as c_ulong),
                mining: 7,
                seq: 0,
                log: String::new(),
            }
        }
        /// The driver's dump (32-bit `%ld` / `%lu`).
        fn dump(&mut self) {
            self.log.push_str(" |");
            for r in self.resources {
                write!(self.log, " {}", r as i32).unwrap();
            }
            write!(self.log, " sb={}", self.share as i32).unwrap();
            if let Some(d) = self.computer {
                write!(self.log, " d={}", d as u32).unwrap();
            }
            self.log.push_str(" v=");
            for v in self.variables {
                write!(self.log, "{},", v as u32).unwrap();
            }
            self.log.push_str(" t=");
            for t in self.tributes {
                write!(self.log, "{},", t as u32).unwrap();
            }
            write!(self.log, " m={}", self.mining as i32).unwrap();
        }
    }

    impl Host for Mock {
        fn read4(&mut self, op: u32, a: i32, b: i32, _c: i32, _d: i32) -> i64 {
            match op {
                EE_SCENARIO_READ_PLAYER_EXISTS => (self.exists && a == 3) as i64,
                EE_SCENARIO_READ_RANDOM => {
                    let v = a as i64 + (self.seq % (b - a + 1) as u32) as i64;
                    self.seq += 1;
                    write!(self.log, " Random({a},{b})={v}").unwrap();
                    v
                }
                EE_SCENARIO_READ_RESOURCE => self.resources[b as usize] as i64,
                EE_SCENARIO_READ_COMPUTER_DIFFICULTY => self.computer.map_or(-1, |d| d as i64),
                EE_SCENARIO_READ_VARIABLE => self.variables[b as usize] as i64,
                EE_SCENARIO_READ_TRIBUTE => self.tributes[b as usize] as i64,
                EE_SCENARIO_READ_MINING_LIMIT => self.mining as i64,
                EE_SCENARIO_READ_SHARE_BONUS => self.share as i64,
                EE_SCENARIO_READ_WORLD_POPULATION_LIMIT => 200,
                EE_SCENARIO_READ_PLAYER_ID => 3,
                _ => panic!("read {op}"),
            }
        }
        fn write(&mut self, op: u32, player: c_long, b: i64, c: i64, d: i64) {
            self.write_op(op, player, b, c, d);
        }
    }

    impl Mock {
        fn write_op(&mut self, op: u32, player: c_long, b: i64, c: i64, d: i64) {
            match op {
                EE_SCENARIO_WRITE_SET_ATTRIBUTE => {
                    write!(self.log, " R({},{},{},{})", player as i32, b as u32, c as i32, d as i32).unwrap();
                    set_player_attribute(self, player, b as c_ulong, c as c_long, d as c_long);
                }
                EE_SCENARIO_WRITE_RESOURCE => self.resources[b as usize] = c as c_long,
                EE_SCENARIO_WRITE_SHARE_BONUS => self.share = b as c_long,
                EE_SCENARIO_WRITE_PENDING_POPULATION => {
                    write!(self.log, " Pending({},0,{})", b as u32, c as u32).unwrap()
                }
                EE_SCENARIO_WRITE_DIFFICULTY => self.computer = Some(b as c_long as c_ulong),
                EE_SCENARIO_WRITE_VARIABLE => self.variables[b as usize] = c as c_ulong,
                EE_SCENARIO_WRITE_TRIBUTE => self.tributes[b as usize] = c as c_ulong,
                EE_SCENARIO_WRITE_ADD_TRIBUTE => {
                    let i = (b * 5 + c - 2) as usize;
                    self.tributes[i] = self.tributes[i].wrapping_add(d as c_long as c_ulong);
                    write!(self.log, " AddTribute({},{},{})", b as i32, c as i32, d as i32).unwrap();
                }
                EE_SCENARIO_WRITE_STATISTIC_ADD => write!(self.log, " StatAdd({},{})", b as i32, c as i32).unwrap(),
                EE_SCENARIO_WRITE_STATISTIC_SET => write!(self.log, " StatSet({},{})", b as i32, c as i32).unwrap(),
                EE_SCENARIO_WRITE_MINING_LIMIT => self.mining = b as c_long,
                _ => panic!("write {op}"),
            }
        }
    }

    /// Every line of the original's driver (3 player kinds x 73 attributes x 4 modes x 9 amounts) again: the same
    /// calls in order and the same numbers after. Where `long` is 64 bits the amounts at the 32-bit limits wrap
    /// differently (as the C++ there): those lines are compared on Windows only.
    #[test]
    fn golden_matches_the_original_body() {
        let golden = include_str!("golden_player_attribute.txt");
        let mut n = 0;
        for line in golden.lines() {
            let (head, want) = line.split_once(':').unwrap();
            let v: Vec<i64> = head.split(' ').map(|x| x.parse().unwrap()).collect();
            let (kind, attribute, mode, amount) = (v[0] as u32, v[1] as c_ulong, v[2] as c_long, v[3]);
            if cfg!(not(windows)) && (amount == 2_147_483_647 || amount == -2_147_483_648) {
                continue;
            }
            let mut m = Mock::new(kind);
            set_player_attribute(&mut m, 3, attribute, mode, amount as c_long);
            m.dump();
            assert_eq!(m.log, want, "{head}");
            n += 1;
        }
        assert!(n >= 6000, "{n}");
    }
}
