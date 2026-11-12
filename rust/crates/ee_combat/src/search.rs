//! The target searches' scoring (R5b.2, third round): `EESearchTarget` (the target search `EEUUTargetSearch` over
//! the unit regions, `EEUUSearchTiles` in its two forms), `EESearchTargetAlongList` (along the x-sorted unit list) and
//! `EEGetPursuerOf`.
//!
//! The walks' order, the scores, the ties and the done tests are here; the world stays in C++ behind `SearchHost`
//! (the FFI table `EeCombatSearchHost`): units are opaque handles (`EeCombatHandle`, the C++ pointers), every test
//! of a candidate is asked when the original asked it (several erase expired ignored targets or write capture
//! records). The region order's permutation (`PermuteValues`) takes the physics' tick value, not a draw: these
//! searches draw no random number themselves.

use crate::walks::EeCombatHandle;
use ee_ge::types::Ge2DPoint;
use ee_ge::um::lrintf;

/// The tests a search asks of a candidate (`SearchHost::test`). The C++ host answers each as its search's original
/// expression did (`EESearchTarget`'s candidate is a complex unit, `EESearchTargetAlongList`'s a list unit whose
/// complex unit may be missing).
/// `IsCandidate`: an enemy's live unit (not disabled unless protected).
pub const EE_COMBAT_SEARCH_TEST_CANDIDATE: u32 = 1;
/// The candidate's owner is player 0 (nature).
pub const EE_COMBAT_SEARCH_TEST_NATURE: u32 = 2;
/// `EEIsAttackedBy(unit, candidate)`.
pub const EE_COMBAT_SEARCH_TEST_ATTACKED_BY: u32 = 3;
/// The candidate's class is a wall and not a tower.
pub const EE_COMBAT_SEARCH_TEST_WALL: u32 = 4;
/// `EECanTargetClass(unit, candidate's class)`.
pub const EE_COMBAT_SEARCH_TEST_CAN_TARGET_CLASS: u32 = 5;
/// The candidate (its complex unit) `IsFoundation`.
pub const EE_COMBAT_SEARCH_TEST_FOUNDATION: u32 = 6;
/// `EEIsSeenBy(candidate, unit's player, true)`.
pub const EE_COMBAT_SEARCH_TEST_SEEN: u32 = 7;
/// `unit->IsOnOtherContinentThan(candidate)`.
pub const EE_COMBAT_SEARCH_TEST_OTHER_CONTINENT: u32 = 8;
/// The candidate's class `IsPartLeftOut(candidate)`.
pub const EE_COMBAT_SEARCH_TEST_PART_LEFT_OUT: u32 = 9;
/// The unit's (`EESearchTargetAlongList`) or the candidate's (`EESearchTarget`) class `IsPriest`.
pub const EE_COMBAT_SEARCH_TEST_PRIEST: u32 = 10;
/// `EECanPriestConvert(unit, candidate)` (false for a list unit without a complex unit).
pub const EE_COMBAT_SEARCH_TEST_CAN_PRIEST_CONVERT: u32 = 11;
/// `unit->CanAttackClass(candidate's class)`.
pub const EE_COMBAT_SEARCH_TEST_CAN_ATTACK_CLASS: u32 = 12;
/// The unit has ignored targets and `IsTargetIgnored(candidate, true)` (which may drop expired ones).
pub const EE_COMBAT_SEARCH_TEST_IGNORED: u32 = 13;
/// The candidate's goal attacks the unit.
pub const EE_COMBAT_SEARCH_TEST_ATTACKS_UNIT: u32 = 14;
/// The candidate's owner is the unit's player or not its enemy.
pub const EE_COMBAT_SEARCH_TEST_NOT_ENEMY: u32 = 15;
/// The candidate class's technology in its owner's tree is of kind 6.
pub const EE_COMBAT_SEARCH_TEST_TECH_KIND_6: u32 = 16;
/// The candidate's hit points are positive.
pub const EE_COMBAT_SEARCH_TEST_ALIVE: u32 = 17;
/// The candidate pursues the unit (`EEGetPursuerOf`): a complex unit, no projectile, whose goal attacks the unit or
/// gathers from it.
pub const EE_COMBAT_SEARCH_TEST_PURSUES: u32 = 18;

/// The outcome of one candidate in the capture's trace (`SearchHost::trace`).
/// Rejected by a test.
pub const EE_COMBAT_SEARCH_TRACE_REJECTED: i32 = 0;
/// A large unit scored already (`EESearchTarget`'s scratch list).
pub const EE_COMBAT_SEARCH_TRACE_SCORED: i32 = 1;
/// Scored, not better than the best.
pub const EE_COMBAT_SEARCH_TRACE_NOT_BETTER: i32 = 2;
/// Reachable: the best so far.
pub const EE_COMBAT_SEARCH_TRACE_BEST: i32 = 3;
/// Not reachable: ignored by the unit from now on.
pub const EE_COMBAT_SEARCH_TRACE_IGNORED: i32 = 4;
/// Of the targeting table's preferred unit type (`EESearchTargetAlongList`: the search ends on it).
pub const EE_COMBAT_SEARCH_TRACE_PREFERRED: i32 = 5;

/// `kUnitType14AntiMissileBattery`: always scored as a still target.
pub const EE_COMBAT_ANTI_MISSILE_BATTERY: i32 = 0x2e5;

/// A unit region's coordinates.
type Region = (i32, i32);

/// A search's inputs: the searching unit's stats and tile, the search's tile, area and scale, and the targeting
/// table (`EEAIUnitTargetingTable::sTable`, three rows of ten).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeCombatSearchIn {
    /// its stats' `GetAttack`, `GetMaxVelocity`, `GetRange`; its `GetLineOfSight`
    pub attack: f32,
    pub max_velocity: f32,
    pub range: f32,
    pub reach: f32,
    pub scale: f32,
    /// its class's `mMinRange`
    pub min_range: f32,
    /// its class's `mIsMelee`, `mShootsHigh`; its stats' `mCanFireOverWalls`
    pub melee: i32,
    pub shoots_high: i32,
    pub over_walls: i32,
    /// its tile
    pub unit_x: i32,
    pub unit_y: i32,
    /// the search's tile
    pub x: i32,
    pub y: i32,
    /// `EESearchTarget`: the area (inclusive), the unit regions' size, the physics' tick value, the unit's location
    pub left: i32,
    pub top: i32,
    pub right: i32,
    pub bottom: i32,
    pub region_size: i32,
    pub tick_random: i32,
    pub position_x: f32,
    pub position_y: f32,
    /// the targeting table, row by row
    pub table: [i32; 30],
}

/// A candidate's own values as a search reads them (`SearchHost::target`; pure reads).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeCombatSearchTarget {
    /// its class's `mTileSize > 1`
    pub multiple: i32,
    /// its class's `mUnitType`
    pub unit_type: i32,
    /// its stats' attack and maximum velocity
    pub attack: f32,
    pub max_velocity: f32,
}

/// The world a search walks. `EESearchTarget` asks the regions, `EESearchTargetAlongList` and `EEGetPursuerOf` the
/// unit list.
pub trait SearchHost {
    /// `mUnitRegions->GetRegions(area)`: the regions' rectangle (left, top, right, bottom).
    fn regions(&mut self, left: i32, top: i32, right: i32, bottom: i32) -> (i32, i32, i32, i32);
    /// The size of region (x, y), -1 for none.
    fn region_len(&mut self, x: i32, y: i32) -> i32;
    /// The `index`th unit of region (x, y).
    fn region_unit(&mut self, x: i32, y: i32, index: i32) -> EeCombatHandle;
    /// The unit's tile.
    fn unit_tile(&mut self, unit: EeCombatHandle) -> (i32, i32);
    /// The unit's location's x and y.
    fn unit_position(&mut self, unit: EeCombatHandle) -> (f32, f32);
    /// The unit before (`prev`) or after it in the x-sorted unit list (its tile node's `mPrev` / `mNext`), 0 for none.
    fn list_next(&mut self, unit: EeCombatHandle, prev: bool) -> EeCombatHandle;
    /// `RTSDummyHead(unit) != NULL`: the list's head, not a unit.
    fn dummy_head(&mut self, unit: EeCombatHandle) -> bool;
    /// One of the `SEARCH_TEST_*` tests.
    fn test(&mut self, test: u32, unit: EeCombatHandle) -> bool;
    /// The candidate's own values.
    fn target(&mut self, unit: EeCombatHandle) -> EeCombatSearchTarget;
    /// `EEGetHitDistanceSquared(unit, candidate, x, y)`.
    fn hit_distance_squared(&mut self, unit: EeCombatHandle, x: f32, y: f32) -> f32;
    /// `gToHitTable.GetAttackFactor(the unit's ammo, the candidate's class, the players)`.
    fn attack_factor(&mut self, unit: EeCombatHandle) -> f32;
    /// `EEIsLineToUnitBlocked(unit, candidate, ...)`.
    fn line_blocked(&mut self, unit: EeCombatHandle) -> bool;
    /// `EEIsAttackReachable(unit, candidate, &tile)`.
    fn reachable(&mut self, unit: EeCombatHandle) -> bool;
    /// `unit->IgnoreTarget(candidate)`.
    fn ignore(&mut self, unit: EeCombatHandle);
    /// `inUnit->GetNearestFootprintTile(unit, &x, &y)` (`EEGetPursuerOf`).
    fn footprint_tile(&mut self, unit: EeCombatHandle) -> (i32, i32);
    /// The capture's trace of one candidate (`SEARCH_TRACE_*`, the score); nothing else.
    fn trace(&mut self, unit: EeCombatHandle, code: i32, score: i32);
}

/// A search's result: the best target (0 for none), its score, and whether the search ended early.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeCombatSearchOut {
    pub best: u64,
    pub score: i32,
    pub done: i32,
}

/// `UMSquare`.
#[inline]
fn square(value: f32) -> f32 {
    value * value
}

/// `PermuteValues(values, start)`: the game's fixed shuffle from a start index (the signed modulo of the size).
pub fn permute<T>(values: &mut [T], start: i32) {
    if values.len() <= 1 {
        return;
    }
    let count = values.len() as i32;
    // the C++'s `long % long`: a negative start (the tick value never is) would index before the array
    let mut index = start.wrapping_rem(count);
    let at = |n: i32| n as usize;
    let mut i = count - 1;
    while i > 0 {
        if i != index {
            values.swap(at(i), at(index));
        }
        i -= 1;
        let opposite = count - index - 1;
        if i != opposite {
            values.swap(at(i), at(opposite));
        }
        index = (opposite * 2 + 1) % count;
        i -= 1;
    }
    if i == 0 && index < count && index != 0 {
        values.swap(0, at(index));
    }
}

/// The search's derived values (the `EEUUTargetSearch` constructor's, and `EESearchTargetAlongList`'s locals).
struct Search {
    attack: i32,
    max_velocity: f32,
    scaled_range_squared: f32,
    reach_squared: f32,
    outer_squared: f32,
    min_range_squared: f32,
    mode: usize,
    immobile: bool,
    center_x: f32,
    center_y: f32,
    best: EeCombatHandle,
    best_score: i32,
    done: bool,
    scratch: Vec<EeCombatHandle>,
}

impl Search {
    fn new(i: &EeCombatSearchIn) -> Search {
        let scaled_range = i.range * i.scale;
        let scaled_range_squared = square(scaled_range);
        let (reach_squared, outer_squared) = if scaled_range > i.reach {
            (scaled_range_squared, square(scaled_range + 4.0))
        } else {
            (square(i.reach), square(i.reach + 4.0))
        };
        let mode = if i.melee != 0 {
            0
        } else if i.range > i.reach {
            2
        } else {
            1
        };
        Search {
            attack: lrintf(i.attack),
            max_velocity: i.max_velocity,
            scaled_range_squared,
            reach_squared,
            outer_squared,
            min_range_squared: square(i.min_range),
            mode,
            immobile: i.max_velocity == 0.0,
            center_x: i.x as f32 + 0.5,
            center_y: i.y as f32 + 0.5,
            best: 0,
            best_score: -100000,
            done: false,
            scratch: Vec::new(),
        }
    }

    fn row<'a>(&self, i: &'a EeCombatSearchIn) -> &'a [i32] {
        &i.table[self.mode * 10..self.mode * 10 + 10]
    }

    /// The hit distance from the unit, or from the search tile's centre when the unit is elsewhere.
    fn distance(&self, i: &EeCombatSearchIn, host: &mut dyn SearchHost, unit: EeCombatHandle) -> f32 {
        if i.unit_x == i.x && i.unit_y == i.y {
            host.hit_distance_squared(unit, -10000.0, -10000.0)
        } else {
            host.hit_distance_squared(unit, self.center_x, self.center_y)
        }
    }

    /// The score's sum past the preferred type: attacked by it, in range, weaker, slower, still, near, easy to hit.
    fn sum(
        &self,
        i: &EeCombatSearchIn,
        t: &EeCombatSearchTarget,
        attacks_unit: bool,
        distance: f32,
        to_hit: f32,
        still: bool,
    ) -> i32 {
        let row = self.row(i);
        let mut score = 0i32;
        if attacks_unit {
            score = row[0];
        }
        if distance <= self.scaled_range_squared {
            score = score.wrapping_add(row[2]);
        }
        if t.attack < self.attack as f32 {
            score = score.wrapping_add(row[4]);
        }
        if t.max_velocity < self.max_velocity {
            score = score.wrapping_add(row[3]);
        }
        if still {
            score = score.wrapping_add(row[8]);
        }
        let near = lrintf(row[7] as f32 / distance);
        let hit = lrintf(row[6] as f32 * to_hit);
        score.wrapping_add(hit.wrapping_add(near))
    }

    /// `EEUUTargetSearch::Score`: one candidate against the targeting table row of the search's mode; the best so
    /// far (reachable) becomes `best`, a score at the row's threshold (or the preferred type) ends the search.
    fn score(&mut self, i: &EeCombatSearchIn, host: &mut dyn SearchHost, unit: EeCombatHandle) {
        let t = host.target(unit);
        if t.multiple != 0 {
            if self.scratch.contains(&unit) {
                return host.trace(unit, EE_COMBAT_SEARCH_TRACE_SCORED, 0);
            }
            self.scratch.push(unit);
        }
        let distance = self.distance(i, host, unit);
        let rejected = distance > self.reach_squared
            || (host.test(EE_COMBAT_SEARCH_TEST_NATURE, unit) && !host.test(EE_COMBAT_SEARCH_TEST_ATTACKED_BY, unit))
            || host.test(EE_COMBAT_SEARCH_TEST_WALL, unit)
            || !host.test(EE_COMBAT_SEARCH_TEST_CAN_TARGET_CLASS, unit)
            || host.test(EE_COMBAT_SEARCH_TEST_FOUNDATION, unit)
            || !host.test(EE_COMBAT_SEARCH_TEST_SEEN, unit)
            || host.test(EE_COMBAT_SEARCH_TEST_OTHER_CONTINENT, unit)
            || host.test(EE_COMBAT_SEARCH_TEST_PART_LEFT_OUT, unit)
            || (host.test(EE_COMBAT_SEARCH_TEST_PRIEST, unit)
                && !host.test(EE_COMBAT_SEARCH_TEST_CAN_PRIEST_CONVERT, unit)
                && !host.test(EE_COMBAT_SEARCH_TEST_CAN_ATTACK_CLASS, unit))
            || host.test(EE_COMBAT_SEARCH_TEST_IGNORED, unit);
        if rejected {
            return host.trace(unit, EE_COMBAT_SEARCH_TRACE_REJECTED, 0);
        }
        let to_hit = host.attack_factor(unit);
        if (self.immobile && self.mode == 1 && distance >= self.scaled_range_squared)
            || distance < self.min_range_squared
            || to_hit <= 0.0
            || (i.shoots_high == 0 && i.over_walls == 0 && host.line_blocked(unit))
        {
            return host.trace(unit, EE_COMBAT_SEARCH_TRACE_REJECTED, 0);
        }
        let row = self.row(i);
        let preferred = t.unit_type == row[5];
        let (threshold, mut score) = (row[9], 0);
        if !preferred {
            let attacks_unit = host.test(EE_COMBAT_SEARCH_TEST_ATTACKS_UNIT, unit);
            let still = t.max_velocity == 0.0 || t.unit_type == EE_COMBAT_ANTI_MISSILE_BATTERY;
            score = self.sum(i, &t, attacks_unit, distance, to_hit, still);
            if score <= self.best_score {
                return host.trace(unit, EE_COMBAT_SEARCH_TRACE_NOT_BETTER, score);
            }
        }
        if host.reachable(unit) {
            self.best = unit;
            self.best_score = score;
            if preferred || score >= threshold {
                self.done = true;
            }
            host.trace(unit, EE_COMBAT_SEARCH_TRACE_BEST, score);
        } else {
            host.ignore(unit);
            host.trace(unit, EE_COMBAT_SEARCH_TRACE_IGNORED, score);
        }
    }

    fn inside(i: &EeCombatSearchIn, (x, y): (i32, i32)) -> bool {
        x >= i.left && x <= i.right && y >= i.top && y <= i.bottom
    }

    /// The regions of the area: the nine around the unit's region (`inner`) and the others, column by column.
    fn regions(i: &EeCombatSearchIn, host: &mut dyn SearchHost) -> (Vec<Region>, Vec<Region>) {
        let (left, top, right, bottom) = host.regions(i.left, i.top, i.right, i.bottom);
        let center = (i.unit_x.wrapping_div(i.region_size), i.unit_y.wrapping_div(i.region_size));
        let (mut inner, mut outer) = (Vec::new(), Vec::new());
        let mut x = left;
        while x <= right {
            let mut y = top;
            while y <= bottom {
                let near = center.0.wrapping_sub(x).wrapping_abs() <= 1 && center.1.wrapping_sub(y).wrapping_abs() <= 1;
                if host.region_len(x, y) >= 0 {
                    if near { &mut inner } else { &mut outer }.push((x, y));
                }
                y = y.wrapping_add(1);
            }
            x = x.wrapping_add(1);
        }
        (inner, outer)
    }

    /// The units of `list`'s regions from the back, each region from its back, scored while the search goes on;
    /// false when it ended.
    fn walk(&mut self, i: &EeCombatSearchIn, host: &mut dyn SearchHost, list: &[(i32, i32)]) -> bool {
        for &(x, y) in list.iter().rev() {
            let mut j = host.region_len(x, y).wrapping_sub(1);
            while j >= 0 {
                let unit = host.region_unit(x, y, j);
                if Self::inside(i, host.unit_tile(unit)) && host.test(EE_COMBAT_SEARCH_TEST_CANDIDATE, unit) {
                    self.score(i, host, unit);
                }
                if self.done {
                    return false;
                }
                j -= 1;
            }
        }
        true
    }

    fn out(&self) -> EeCombatSearchOut {
        EeCombatSearchOut { best: self.best, score: self.best_score, done: self.done as i32 }
    }
}

/// `EESearchTarget`: the best target around the search tile within the area, by `EEUUTargetSearch`'s scores. A
/// ranged unit walks the nine regions around its own first, then the others, each list in `PermuteValues` order; a
/// melee unit scores the near regions' candidates by their distance from its location (ties in walk order), then the
/// other regions permuted.
pub fn search_target(i: &EeCombatSearchIn, host: &mut dyn SearchHost) -> EeCombatSearchOut {
    let mut s = Search::new(i);
    if i.melee == 0 {
        let (mut inner, mut outer) = Search::regions(i, host);
        permute(&mut inner, i.tick_random);
        if s.walk(i, host, &inner) {
            permute(&mut outer, i.tick_random);
            s.walk(i, host, &outer);
        }
        return s.out();
    }
    // the near regions' candidates into a multimap by distance (equal keys keep their insertion order)
    let (left, top, right, bottom) = host.regions(i.left, i.top, i.right, i.bottom);
    let center = (i.unit_x.wrapping_div(i.region_size), i.unit_y.wrapping_div(i.region_size));
    let position = Ge2DPoint { x: i.position_x, y: i.position_y };
    let mut near: Vec<(f32, EeCombatHandle)> = Vec::new();
    let mut outer = Vec::new();
    let mut x = left;
    while x <= right {
        let mut y = top;
        while y <= bottom {
            let len = host.region_len(x, y);
            if center.0.wrapping_sub(x).wrapping_abs() <= 1 && center.1.wrapping_sub(y).wrapping_abs() <= 1 {
                let mut j = len.wrapping_sub(1);
                while len >= 0 && j >= 0 {
                    let unit = host.region_unit(x, y, j);
                    j -= 1;
                    if !Search::inside(i, host.unit_tile(unit)) || !host.test(EE_COMBAT_SEARCH_TEST_CANDIDATE, unit) {
                        continue;
                    }
                    let (px, py) = host.unit_position(unit);
                    let key = position.distance_to_squared(Ge2DPoint { x: px, y: py });
                    // `multimap::insert`: after every element whose key is not greater
                    let at = near.partition_point(|&(k, _)| !(key < k));
                    near.insert(at, (key, unit));
                }
            } else if len >= 0 {
                outer.push((x, y));
            }
            y = y.wrapping_add(1);
        }
        x = x.wrapping_add(1);
    }
    for &(_, unit) in &near {
        if host.test(EE_COMBAT_SEARCH_TEST_CANDIDATE, unit) {
            s.score(i, host, unit);
        }
        if s.done {
            return s.out();
        }
    }
    permute(&mut outer, i.tick_random);
    s.walk(i, host, &outer);
    s.out()
}

/// `EESearchTargetAlongList`: the best target around the search tile along the x-sorted unit list, both ways from
/// the unit (one step each way a turn) while the list's units stay within the outer reach in x. The list ends when
/// the backward neighbour is missing only for that side; a missing forward neighbour ends both.
pub fn search_along_list(i: &EeCombatSearchIn, unit: EeCombatHandle, host: &mut dyn SearchHost) -> EeCombatSearchOut {
    let mut s = Search::new(i);
    let mut forward = host.list_next(unit, true);
    let mut backward = host.list_next(unit, false);
    let (mut forward_done, mut backward_done) = (false, false);
    if i.attack == 0.0 {
        forward_done = true;
        backward_done = true;
    }
    let row = s.row(i).to_vec();
    while (!forward_done || !backward_done) && forward != 0 {
        if !forward_done {
            if host.dummy_head(forward) || {
                let dx = i.x.wrapping_sub(host.unit_tile(forward).0);
                dx.wrapping_mul(dx) as f32 > s.outer_squared
            } {
                forward_done = true;
            } else {
                let preferred = s.along(i, host, forward, true, &row);
                if s.done {
                    forward_done = true;
                    backward_done = true;
                }
                if preferred {
                    // `continue`: no step forward
                    continue;
                }
                forward = host.list_next(forward, true);
            }
        }
        if backward_done {
            continue;
        }
        if backward == 0 || {
            let dx = host.unit_tile(backward).0.wrapping_sub(i.x);
            dx.wrapping_mul(dx) as f32 > s.outer_squared
        } {
            backward_done = true;
        } else {
            let preferred = s.along(i, host, backward, false, &row);
            if s.done {
                forward_done = true;
                backward_done = true;
            }
            if preferred {
                continue;
            }
            backward = host.list_next(backward, false);
        }
    }
    s.out()
}

impl Search {
    /// One unit of the list search; true for the preferred type (the search ends there, before the next step).
    fn along(
        &mut self,
        i: &EeCombatSearchIn,
        host: &mut dyn SearchHost,
        unit: EeCombatHandle,
        forward: bool,
        row: &[i32],
    ) -> bool {
        // the technology's kind is read before the tests, as the original did
        let kind_6 = host.test(EE_COMBAT_SEARCH_TEST_TECH_KIND_6, unit);
        let passes = !host.test(EE_COMBAT_SEARCH_TEST_NOT_ENEMY, unit)
            && (!host.test(EE_COMBAT_SEARCH_TEST_NATURE, unit) || host.test(EE_COMBAT_SEARCH_TEST_ATTACKED_BY, unit))
            && !kind_6
            && host.test(EE_COMBAT_SEARCH_TEST_CAN_TARGET_CLASS, unit)
            && !host.test(EE_COMBAT_SEARCH_TEST_FOUNDATION, unit)
            && host.test(EE_COMBAT_SEARCH_TEST_ALIVE, unit)
            && host.test(EE_COMBAT_SEARCH_TEST_SEEN, unit)
            && !host.test(EE_COMBAT_SEARCH_TEST_PART_LEFT_OUT, unit)
            && (!host.test(EE_COMBAT_SEARCH_TEST_PRIEST, unit)
                || host.test(EE_COMBAT_SEARCH_TEST_CAN_PRIEST_CONVERT, unit)
                || host.test(EE_COMBAT_SEARCH_TEST_CAN_ATTACK_CLASS, unit))
            && !host.test(EE_COMBAT_SEARCH_TEST_IGNORED, unit);
        if !passes {
            host.trace(unit, EE_COMBAT_SEARCH_TRACE_REJECTED, 0);
            return false;
        }
        let distance = self.distance(i, host, unit);
        let to_hit = host.attack_factor(unit);
        // (the backward side has no `mShootsHigh` test, as in the original)
        if !(distance <= self.reach_squared
            && (!self.immobile || self.mode != 1 || distance < self.scaled_range_squared)
            && distance >= self.min_range_squared
            && to_hit > 0.0
            && ((forward && i.shoots_high != 0) || i.over_walls != 0 || !host.line_blocked(unit)))
        {
            host.trace(unit, EE_COMBAT_SEARCH_TRACE_REJECTED, 0);
            return false;
        }
        let t = host.target(unit);
        if t.unit_type == row[5] {
            host.trace(unit, EE_COMBAT_SEARCH_TRACE_PREFERRED, 0);
            self.best = unit;
            self.done = true;
            return true;
        }
        let attacks_unit = host.test(EE_COMBAT_SEARCH_TEST_ATTACKS_UNIT, unit);
        let score = self.sum(i, &t, attacks_unit, distance, to_hit, t.max_velocity == 0.0);
        if score > self.best_score {
            if host.reachable(unit) {
                host.trace(unit, EE_COMBAT_SEARCH_TRACE_BEST, score);
                self.best = unit;
                self.best_score = score;
                if score >= row[9] {
                    // both sides end (the loop's next test)
                    self.done = true;
                }
            } else {
                host.ignore(unit);
                host.trace(unit, EE_COMBAT_SEARCH_TRACE_IGNORED, score);
            }
        } else {
            host.trace(unit, EE_COMBAT_SEARCH_TRACE_NOT_BETTER, score);
        }
        false
    }
}
