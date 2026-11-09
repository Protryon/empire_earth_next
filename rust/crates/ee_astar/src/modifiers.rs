//! The path goals' modifiers' step costs and tile tests (R5b.1 step 4; `EEAIAStarBoundedPathGoalModifiers.cpp`,
//! `EEEMStorm.cpp`), line by line. The C++ modifier classes stay (the goals hold them, call their virtuals and
//! `dynamic_cast` them); each slot hands its state to one function here and asks the world through a quiet view of
//! the world map (`ee/EEAIAStarView.h`: the modifier's answer is the fact the capture notes, not its reads).
//!
//! Integer arithmetic only, `long` as `i32`, wrapping where the C++ wraps. The storm's tests draw from the physics
//! random stream through the view (`random_at_most`), in the C++'s order and number.

// the C++'s shape (its nested tests and late results) is kept line by line
#![allow(clippy::collapsible_if, clippy::collapsible_match, clippy::needless_late_init)]

use crate::goals::{View, TERRAIN_CLIFF, TERRAIN_DEEP_WATER, TERRAIN_SPACE};
use crate::storage::AStarRect;
use std::ffi::c_void;

/// `EEMap.h`'s shallow water terrain.
pub(crate) const TERRAIN_SHALLOW_WATER: i32 = 10;
/// `kTechEpochAtomicWW1`: from this epoch a walker keeps two tiles from blockers.
pub(crate) const EPOCH_ATOMIC_WW1: i32 = 9;

/// A unit as the view names it (`EEComplexUnit*`, opaque here).
pub type Unit = *mut c_void;

/// `EEAIAStarView::ArePassableExcept`'s skip (`kSkipNone`, `kSkipWalls`, `kSkipTrees`).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Skip {
    None = 0,
    Walls = 1,
    Trees = 2,
}

/// The view queries the modifiers make (`ee/EEAIAStarView.h`).
pub trait ModView: View {
    fn ai_value(&mut self, x: i32, y: i32) -> i32;
    fn tile_flag(&mut self, x: i32, y: i32) -> i32;
    fn can_move_on(&mut self, walker: Unit, terrain: i32) -> bool;
    fn are_passable(&mut self, walker: Unit, x: i32, y: i32, skip: Skip) -> bool;
    fn formation_rank(&mut self, walker: Unit) -> i32;
    /// `HasStepBlocker`: a stopped collidable unit on the tile that blocks the walker's step (`sea`: at sea)
    fn step_blocker(&mut self, walker: Unit, sea: bool, x: i32, y: i32) -> bool;
    /// `GetUnitRegionState`: -1 no region, 0 empty, 1 with units
    fn unit_region_state(&mut self, x: i32, y: i32) -> i32;
    fn epoch(&mut self, walker: Unit) -> i32;
    fn is_alone(&mut self, walker: Unit) -> bool;
    fn water_state(&mut self, walker: Unit) -> i32;
    fn is_sea_going(&mut self, walker: Unit) -> bool;
    fn has_hill(&mut self, x: i32, y: i32) -> bool;
    /// one draw of the physics stream: `Generate() <= chance`
    fn random_at_most(&mut self, chance: f32) -> bool;
}

/// 0x248e06 (`EEGetTileInDirection`): the tile `distance` steps from (x, y) in `direction` (0..7; others stay).
pub fn tile_in_direction(x: i32, y: i32, direction: i32, distance: i32) -> (i32, i32) {
    match direction {
        0 => (x.wrapping_sub(distance), y.wrapping_add(distance)),
        1 => (x.wrapping_sub(distance), y),
        2 => (x.wrapping_sub(distance), y.wrapping_sub(distance)),
        3 => (x, y.wrapping_sub(distance)),
        4 => (x, y.wrapping_add(distance)),
        5 => (x.wrapping_add(distance), y.wrapping_add(distance)),
        6 => (x.wrapping_add(distance), y),
        7 => (x.wrapping_add(distance), y.wrapping_sub(distance)),
        _ => (x, y),
    }
}

/// `U2DRectangle<long>::Contains`: edges included.
pub(crate) fn contains(r: &AStarRect, x: i32, y: i32) -> bool {
    x >= r.left && x <= r.right && y >= r.top && y <= r.bottom
}

/// The goal's state the step cost reads (`EEAIAStarPathfindingGoalBase`).
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeAStarStepGoal {
    /// `mUnit`
    pub walker: Unit,
    /// `mAvoidUnits`
    pub avoid_units: bool,
    /// `mIsBounded`
    pub is_bounded: bool,
    /// `mGoalBounds`
    pub goal_bounds: AStarRect,
}

/// 0x40f53b (`EEAIPathGoalModifierBase::GetStepCost`): a sea (`sea`) or space walker's step cost: 10/14, x5 onto a
/// blocker, +6 for a turn, x2 per blocked tile within 1 (2 from epoch 9 when unbounded; x3 at far corners) when
/// avoiding units, plus the slope or x2 in a narrow passage for a ranked walker.
#[allow(clippy::too_many_arguments)]
pub fn step_cost<V: ModView + ?Sized>(
    view: &mut V,
    goal: &EeAStarStepGoal,
    sea: bool,
    from_x: i32,
    from_y: i32,
    x: i32,
    y: i32,
    parent_x: i32,
    parent_y: i32,
) -> i32 {
    let walker = goal.walker;
    let mut cost: i32 = if from_x == x || from_y == y { 10 } else { 14 };
    let slope = view.ai_value(x, y);
    let mut blocked = false;
    let range = view.formation_rank(walker);
    let mut margin: i32 = 1;

    if view.step_blocker(walker, sea, x, y) {
        cost = cost.wrapping_mul(5);
    }
    if x.wrapping_sub(from_x) != from_x.wrapping_sub(parent_x)
        || y.wrapping_sub(from_y) != from_y.wrapping_sub(parent_y)
    {
        cost = cost.wrapping_add(6);
    }

    if goal.avoid_units {
        if view.epoch(walker) >= EPOCH_ATOMIC_WW1 && !goal.is_bounded {
            margin = 2;
        }
        let empty = view.unit_region_state(x, y) == 0
            && view.unit_region_state(x.wrapping_sub(margin), y.wrapping_sub(margin)) == 0
            && view.unit_region_state(x.wrapping_add(margin), y.wrapping_add(margin)) == 0
            && view.unit_region_state(x.wrapping_add(margin), y.wrapping_sub(margin)) == 0
            && view.unit_region_state(x.wrapping_sub(margin), y.wrapping_add(margin)) == 0;
        if !empty {
            let mut i = x.wrapping_sub(margin);
            while i <= x.wrapping_add(margin) {
                let mut j = y.wrapping_sub(margin);
                while j <= y.wrapping_add(margin) {
                    if !(i == from_x && j == from_y) && view.has_tile(i, j) && view.step_blocker(walker, sea, i, j) {
                        if !goal.is_bounded {
                            if x.wrapping_sub(i).wrapping_abs() > 1 && y.wrapping_sub(j).wrapping_abs() > 1 {
                                cost = cost.wrapping_mul(3);
                            } else {
                                cost = cost.wrapping_mul(2);
                            }
                        }
                    }
                    j = j.wrapping_add(1);
                }
                i = i.wrapping_add(1);
            }
        }
    }

    if range != 0 && !goal.is_bounded {
        if slope != 0 {
            blocked = true;
            cost = cost.wrapping_add(slope.wrapping_abs());
        }
        const DIRECTIONS: [i32; 4] = [4, 3, 1, 6];
        let mut k = 0;
        while k < 4 && !blocked {
            let (tx, ty) = tile_in_direction(x, y, DIRECTIONS[k], range);
            k += 1;
            if !view.has_tile(tx, ty) {
                continue;
            }
            let open = view.ai_value(tx, ty) == 0 && {
                let t = view.terrain(tx, ty);
                if sea {
                    t == TERRAIN_DEEP_WATER || t == TERRAIN_SHALLOW_WATER
                } else {
                    t == TERRAIN_SPACE
                }
            };
            blocked = !open;
        }
        if blocked {
            cost = cost.wrapping_mul(2);
        }
    } else if slope != 0 && view.is_alone(walker) && !contains(&goal.goal_bounds, x, y) {
        cost = cost.wrapping_add(slope.wrapping_abs());
    }
    cost
}

/// The water state test: a tile flagged by the continent manager is taken in state 2 only when the flag is not
/// negative, never in state 3.
fn water_state_allows<V: ModView + ?Sized>(view: &mut V, walker: Unit, x: i32, y: i32) -> bool {
    let flag = view.tile_flag(x, y);
    if flag != 0 {
        match view.water_state(walker) {
            2 => return flag >= 0,
            3 => return false,
            _ => {}
        }
    }
    true
}

/// 0x40f9bb (`EEAIPathGoalModifierBase::IsPassable`): not a map edge, terrain the walker can move on, every unit
/// passable, and its water state agreeing with the tile flag.
pub fn is_passable<V: ModView + ?Sized>(view: &mut V, walker: Unit, x: i32, y: i32) -> bool {
    if view.is_edge(x, y) {
        return false;
    }
    let terrain = view.terrain(x, y);
    if !view.can_move_on(walker, terrain) {
        return false;
    }
    if !view.are_passable(walker, x, y, Skip::None) {
        return false;
    }
    water_state_allows(view, walker, x, y)
}

/// 0x40fce5 (`EEAIBPGModifierEnemyIsAttackableByUnit::CanEnter`): `is_passable` with the water state tested only for
/// a sea-going walker.
pub fn attacker_can_enter<V: ModView + ?Sized>(view: &mut V, walker: Unit, x: i32, y: i32) -> bool {
    if view.is_edge(x, y) {
        return false;
    }
    let terrain = view.terrain(x, y);
    if !view.can_move_on(walker, terrain) {
        return false;
    }
    if !view.are_passable(walker, x, y, Skip::None) {
        return false;
    }
    if view.is_sea_going(walker) {
        return water_state_allows(view, walker, x, y);
    }
    true
}

/// 0x40fdc3 (`EEAIBPGModifierWallsDoNotBlock::CanEnter`): not a map edge, terrain the walker can move on, every unit
/// but walls passable.
pub fn walls_can_enter<V: ModView + ?Sized>(view: &mut V, walker: Unit, x: i32, y: i32) -> bool {
    if view.is_edge(x, y) {
        return false;
    }
    let terrain = view.terrain(x, y);
    if !view.can_move_on(walker, terrain) {
        return false;
    }
    view.are_passable(walker, x, y, Skip::Walls)
}

/// 0x41083c (`EEAIGPGModifierComputerPlayerWallPather::CanEnter`): not a map edge, shallow water or terrain the
/// walker can move on, every unit but trees passable.
pub fn wall_pather_can_enter<V: ModView + ?Sized>(view: &mut V, walker: Unit, x: i32, y: i32) -> bool {
    if view.is_edge(x, y) {
        return false;
    }
    let terrain = view.terrain(x, y);
    if terrain != TERRAIN_SHALLOW_WATER && !view.can_move_on(walker, terrain) {
        return false;
    }
    view.are_passable(walker, x, y, Skip::Trees)
}

/// 0x40fdc3's and 0x410814's cost (`EEAIBPGModifierWallsDoNotBlock`, `EEAIGPGModifierComputerPlayerWallPather`):
/// 10 straight, 14 diagonal.
pub fn straight_or_diagonal(from_x: i32, from_y: i32, x: i32, y: i32) -> i32 {
    if from_x == x || from_y == y {
        10
    } else {
        14
    }
}

/// `EEAIBPGModifierStormShaper`'s state.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeAStarStorm {
    /// `mInner`, `mOuter`
    pub inner: AStarRect,
    pub outer: AStarRect,
    /// `mDistanceSquared`
    pub distance_squared: i32,
    /// `mSnow`, `mRain`
    pub snow: bool,
    pub rain: bool,
    /// the bounded goal's `mStartTile`
    pub start_x: i32,
    pub start_y: i32,
}

/// 0x52e3ee (`EEAIBPGModifierStormShaper::GetCost`): 40; snow: 1 on cliffs and hills, 5 off deep water; rain: 1 on
/// water.
pub fn storm_cost<V: ModView + ?Sized>(view: &mut V, storm: &EeAStarStorm, x: i32, y: i32) -> i32 {
    let mut cost = 40;
    if view.has_tile(x, y) {
        let terrain = view.terrain(x, y);
        if storm.snow {
            if terrain == TERRAIN_CLIFF || view.has_hill(x, y) {
                cost = 1;
            } else if terrain != TERRAIN_DEEP_WATER {
                cost = 5;
            }
        } else if storm.rain && (terrain == TERRAIN_DEEP_WATER || terrain == TERRAIN_SHALLOW_WATER) {
            cost = 1;
        }
    }
    cost
}

/// The shaper's acceptance chances (`EEEMStorm.cpp`'s .rdata).
pub(crate) const STORM_FAR_CHANCE: f32 = 0.2325;
pub(crate) const STORM_NEAR_CHANCE: f32 = 0.685;
pub(crate) const STORM_OUTER_CHANCE: f32 = 0.83;

/// 0x52e48b (`EEAIBPGModifierStormShaper::CanEnter`): always inside the inner box, else one draw against a chance
/// that falls with the distance from the start.
pub fn storm_can_enter<V: ModView + ?Sized>(view: &mut V, storm: &EeAStarStorm, x: i32, y: i32) -> bool {
    if !view.has_tile(x, y) {
        return false;
    }
    if contains(&storm.inner, x, y) {
        return true;
    }
    if contains(&storm.outer, x, y) {
        return view.random_at_most(STORM_OUTER_CHANCE);
    }
    let dx = x.wrapping_sub(storm.start_x);
    let dy = y.wrapping_sub(storm.start_y);
    if dx.wrapping_mul(dx).wrapping_add(dy.wrapping_mul(dy)) <= storm.distance_squared {
        return view.random_at_most(STORM_NEAR_CHANCE);
    }
    view.random_at_most(STORM_FAR_CHANCE)
}
