//! The unit path goals' machine hooks (R5b.1 step 4): `EEAIAStarPathfindingGoal` and `EEAIAStarBoundedPathGoal`'s
//! `AcceptTile`, `GetCost` (with `GetDefaultCost`), `IsGoalNode` and `SearchDone`, line by line (the two goals are
//! twins; the bounded one also keeps to its search window). They ask the world only through `PathView` (in C,
//! `ffi::EeAStarViewCallbacks` over the goal's `EEAIAStarView`), in the C++'s order; a goal's modifier stays a C++
//! object asked through the view (`can_enter`, `modifier_cost`), whose slots run `modifiers`.
//!
//! The goal's state comes in as `EeAStarPathGoal` (the C++ copies it in for every hook and the flags back out);
//! nothing is cached across hooks, since the bounded goal's modifier may move the goal tile between them.

use crate::modifiers::{contains, tile_in_direction, ModView, Skip, Unit};
use crate::storage::AStarRect;
use std::ffi::c_void;

/// The path goals' further view queries (`ee/EEAIAStarView.h`).
pub trait PathView: ModView {
    /// `GetCrowdCost`: +50 per idle or attacking collidable unit (+150 for a big one when the walker is small)
    fn crowd_cost(&mut self, walker: Unit, x: i32, y: i32) -> i32;
    fn is_wide(&mut self, walker: Unit) -> bool;
    fn ignores_formation(&mut self, walker: Unit) -> bool;
    fn rank_probe_blocked(&mut self, walker: Unit, continent_id: u16, x: i32, y: i32) -> bool;
    /// the modifier's `CanEnter` (one noted query)
    fn can_enter(&mut self, modifier: *mut c_void, x: i32, y: i32, at_goal: bool) -> bool;
    /// the modifier's `GetCost` (one noted query; the parent -10000, -10000 when there is none)
    #[allow(clippy::too_many_arguments)]
    fn modifier_cost(
        &mut self,
        modifier: *mut c_void,
        from_x: i32,
        from_y: i32,
        x: i32,
        y: i32,
        px: i32,
        py: i32,
    ) -> i32;
}

/// A path goal's state (`EEAIAStarGoalBase` and `EEAIAStarPathfindingGoalBase` members, the bounded goal's
/// window, the machine's members its hooks read).
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeAStarPathGoal {
    /// `EEAIAStarBoundedPathGoal`: tiles outside `bounds` are refused
    pub bounded: bool,
    /// `mBounds` (the bounded goal's)
    pub bounds: AStarRect,
    /// `mGoalTile`
    pub goal_x: i32,
    pub goal_y: i32,
    /// `mUnit`
    pub walker: Unit,
    /// `mModifier` (null: the default tests and costs)
    pub modifier: *mut c_void,
    /// `mContinentID`
    pub continent_id: u16,
    /// the machine's `mCheckCorners`
    pub check_corners: bool,
    /// `mAcceptNearest`, `mSearchFailed`, `mTookNearest`
    pub accept_nearest: bool,
    pub search_failed: bool,
    pub took_nearest: bool,
    /// the machine's `mRevs` and `GetMaxRevs()` (the goal's `mMaxRevs`)
    pub revs: i32,
    pub max_revs: i32,
}

/// 0x19a3f / 0x1990c: the modifier decides, else not a map edge, terrain the walker can move on, every unit passable.
fn accept_on_map<V: PathView + ?Sized>(goal: &EeAStarPathGoal, view: &mut V, x: i32, y: i32, at_goal: bool) -> bool {
    if !goal.modifier.is_null() {
        return view.can_enter(goal.modifier, x, y, at_goal);
    }
    if view.is_edge(x, y) {
        return false;
    }
    let terrain = view.terrain(x, y);
    if !view.can_move_on(goal.walker, terrain) {
        return false;
    }
    view.are_passable(goal.walker, x, y, Skip::None)
}

/// 0x199fa / 0xf4bc (`AcceptTile`): on the map (and, bounded, inside the window), then `accept_on_map`.
pub fn accept_tile<V: PathView + ?Sized>(goal: &EeAStarPathGoal, view: &mut V, x: i32, y: i32, at_goal: bool) -> bool {
    if view.has_tile(x, y) && (!goal.bounded || contains(&goal.bounds, x, y)) {
        return accept_on_map(goal, view, x, y, at_goal);
    }
    false
}

/// 0x19595 / 0xee51 (`GetDefaultCost`): 10 straight or 14 diagonal, plus the crowd off the goal tile; doubled for a
/// wide ranked walker when a probe a rank away is blocked (the probe's continent test uses the walker's continent).
pub fn default_cost<V: PathView + ?Sized>(
    goal: &EeAStarPathGoal,
    view: &mut V,
    from_x: i32,
    from_y: i32,
    x: i32,
    y: i32,
) -> i32 {
    const DIRECTIONS: [i32; 4] = [4, 3, 1, 6];
    let mut cost: i32 = if from_x == x || from_y == y { 10 } else { 14 };
    if x != goal.goal_x || y != goal.goal_y {
        cost = cost.wrapping_add(view.crowd_cost(goal.walker, x, y));
    }
    if !view.is_wide(goal.walker) {
        return cost;
    }
    let range = view.formation_rank(goal.walker);
    if range == 0 || view.ignores_formation(goal.walker) {
        return cost;
    }
    for direction in DIRECTIONS {
        let (tx, ty) = tile_in_direction(x, y, direction, range);
        if view.rank_probe_blocked(goal.walker, goal.continent_id, tx, ty) {
            return cost.wrapping_mul(2);
        }
    }
    cost
}

/// 0x19528 / 0xede4 (`GetCost`): the modifier's cost (given the current node's parent, or -10000s), else the default.
#[allow(clippy::too_many_arguments)]
pub fn get_cost<V: PathView + ?Sized>(
    goal: &EeAStarPathGoal,
    view: &mut V,
    from_x: i32,
    from_y: i32,
    x: i32,
    y: i32,
    parent: Option<(i32, i32)>,
) -> i32 {
    if goal.modifier.is_null() {
        return default_cost(goal, view, from_x, from_y, x, y);
    }
    let (px, py) = parent.unwrap_or((-10000, -10000));
    view.modifier_cost(goal.modifier, from_x, from_y, x, y, px, py)
}

/// 0x191d1 / 0xeb14 (`IsGoalNode`, without the bounded goal's `CheckNode`, which the C++ calls after): no node, the
/// goal tile, or a neighbour of an unenterable goal (a diagonal one also needs both corner tiles enterable while
/// corner checks are on).
pub fn is_goal_node<V: PathView + ?Sized>(goal: &EeAStarPathGoal, view: &mut V, node: Option<(i32, i32)>) -> bool {
    let Some((x, y)) = node else {
        return true;
    };
    if x.wrapping_sub(goal.goal_x).wrapping_abs() > 1 || y.wrapping_sub(goal.goal_y).wrapping_abs() > 1 {
        return false;
    }
    if x == goal.goal_x && y == goal.goal_y {
        return true;
    }
    let mut result = !accept_tile(goal, view, goal.goal_x, goal.goal_y, true);
    if result {
        let dx = x.wrapping_sub(goal.goal_x);
        let dy = y.wrapping_sub(goal.goal_y);
        if goal.check_corners && dx.wrapping_abs() == 1 && dy.wrapping_abs() == 1 {
            result = accept_tile(goal, view, x.wrapping_add(dx), y.wrapping_add(dy), false);
            if result {
                result = accept_tile(goal, view, x.wrapping_sub(dx), y.wrapping_sub(dy), false);
            }
        }
    }
    result
}

/// 0x19138 / 0xea7b (`SearchDone`): failed when the open lists ran dry, when the current node next to the goal is not
/// enterable, or when the revs reached `GetMaxRevs()` (also when it is -1); a failure takes the nearest tile if
/// allowed. Updates `search_failed` and `took_nearest`.
pub fn search_done<V: PathView + ?Sized>(
    goal: &mut EeAStarPathGoal,
    view: &mut V,
    current: Option<(i32, i32)>,
    has_open: bool,
) {
    match current {
        None => {
            if !has_open {
                goal.search_failed = true;
                goal.took_nearest = goal.accept_nearest;
            }
        }
        Some((x, y)) => {
            if x.wrapping_sub(goal.goal_x).wrapping_abs() <= 1 && y.wrapping_sub(goal.goal_y).wrapping_abs() <= 1 {
                goal.search_failed = !accept_tile(goal, view, x, y, true);
                goal.took_nearest = goal.accept_nearest;
            }
        }
    }
    if !goal.search_failed {
        goal.search_failed = goal.revs >= goal.max_revs;
    }
}
