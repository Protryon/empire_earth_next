//! The search machine of the tile searches (`EEAIAStarMachine<S, G, M>`'s `Start`, `Expand`, `Restart`,
//! `TestTile`, `NextNode` and `AddNode`, `ee/EEAIAStarMachineSteps.h`), line by line. The goal stays the caller's:
//! every question the C++ machine asked its goal `G` is a `Host` call, in the same order and number, so a goal's
//! side effects (flood counts, bounds, collected tiles) happen exactly as before.
//!
//! The machine's own members (`mTile`, `mRevs`, `mNumRuns`, `mSteps`, `mCheckCorners`, `mFound`) stay the C++
//! machine's: a goal writes `mCheckCorners` from inside `AcceptTile`, the path manager reads `mFound` and `mSteps`.
//! The machine reads and writes them through `Host` every time, never caching one across a call into the goal.

use crate::storage::{Storage, NIL, TILE_ACCEPTED, TILE_REJECTED};

/// The goal and the machine's members, as the C++ machine sees them.
pub trait Host {
    /// `G::AcceptTile(x, y, flag)`; `storage` is the machine's own (some goals read it, none writes it)
    fn accept_tile(&mut self, x: i32, y: i32, flag: bool) -> bool;
    /// `G::GetCost(from, to)`; `parent` is the current node's parent tile, which the path goals hand their modifier
    fn get_cost(&mut self, from_x: i32, from_y: i32, x: i32, y: i32, parent: Option<(i32, i32)>) -> i32;
    /// `G::Estimate(from, to)`
    fn estimate(&mut self, from_x: i32, from_y: i32, to_x: i32, to_y: i32) -> i32;
    /// `G::IsGoalNode(node)` (`None`: the C++ NULL)
    fn is_goal_node(&mut self, node: Option<(i32, i32)>) -> bool;
    /// `G::SearchDone(machine)`, given the current node and whether an open node is left (what the path goals read)
    fn search_done(&mut self, current: Option<(i32, i32)>, has_open: bool);
    /// `G::mSearchFailed`
    fn search_failed(&mut self) -> bool;
    /// `G::mAcceptNearest`
    fn accept_nearest(&mut self) -> bool;
    /// `G::IsStepLimit`, `G::IsRevLimit`, `G::IsCheaper`
    fn is_step_limit(&mut self, steps: i32) -> bool;
    fn is_rev_limit(&mut self, revs: i32) -> bool;
    fn is_cheaper(&mut self, cost: u32, old: u32) -> bool;
    /// `G::mStartTile`, `G::mGoalTile`
    fn start_tile(&mut self) -> (i32, i32);
    fn goal_tile(&mut self) -> (i32, i32);
    /// The machine's members.
    fn state(&mut self) -> MachineState;
    fn set_state(&mut self, state: &MachineState);
}

/// `EEAIAStarMachineBase`'s search members and `EEAIAStarMachine`'s map size.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct MachineState {
    pub tile_x: i32,
    pub tile_y: i32,
    pub revs: i32,
    pub num_runs: i32,
    pub steps: i32,
    pub check_corners: bool,
    pub found: bool,
    pub map_width: i32,
    pub map_height: i32,
}

fn with<H: Host + ?Sized>(host: &mut H, f: impl FnOnce(&mut MachineState)) {
    let mut s = host.state();
    f(&mut s);
    host.set_state(&s);
}

/// 0x1d63b `TestTile`: the goal's verdict, cached in the tile flags; on the grid the bounds grow
pub fn test_tile<H: Host + ?Sized>(st: &mut Storage, host: &mut H, x: u32, y: u32) -> bool {
    let state = host.state();
    let mut result = false;
    if x < state.map_width as u32 && y < state.map_height as u32 {
        let flags = st.tile_flags(x as i32, y as i32);
        if flags & TILE_ACCEPTED != 0 || flags & TILE_REJECTED != 0 {
            result = flags & TILE_ACCEPTED != 0;
        } else {
            result = host.accept_tile(x as i32, y as i32, false);
            // the C++ holds the flag's address across the call: the grid is not reallocated by a goal
            let i = (st.width.wrapping_mul(x as i32).wrapping_add(y as i32)) as usize;
            let f = &mut st.flags.as_mut().expect("the tile flags")[i];
            *f |= if result { TILE_ACCEPTED } else { TILE_REJECTED };
        }
        st.extend_bounds(x as i32, y as i32);
    }
    result
}

/// 0x1d184 `Start`: the start node, the goal tile as the machine's tile
pub fn start<H: Host + ?Sized>(st: &mut Storage, host: &mut H) -> bool {
    let (sx, sy) = host.start_tile();
    let result = test_tile(st, host, sx as u32, sy as u32);
    let goal = host.goal_tile();
    with(host, |s| {
        s.tile_x = goal.0;
        s.tile_y = goal.1;
    });
    let (sx, sy) = host.start_tile();
    let node = st.new_node(sx, sy);
    st.current = node;
    let state = host.state();
    let estimate = host.estimate(sx, sy, state.tile_x, state.tile_y);
    let n = &mut st.nodes[node as usize];
    n.path_cost = 0;
    n.estimate = estimate;
    n.cost = (n.estimate.wrapping_add(n.path_cost)) as u32;
    st.add_open(node);
    result
}

/// 0x1d1fb `Expand`
pub fn expand<H: Host + ?Sized>(st: &mut Storage, host: &mut H) {
    with(host, |s| {
        s.steps = 0;
        s.num_runs = s.num_runs.wrapping_add(1);
        s.revs = s.revs.wrapping_add(1);
    });
    loop {
        st.current = st.remove_best_open();
        let cur = st.current;
        let node = if cur == NIL { None } else { Some((st.node(cur).x, st.node(cur).y)) };
        if host.is_goal_node(node) {
            with(host, |s| s.found = false);
            break;
        }
        let c = *st.node(cur);
        let (x, y) = (c.x, c.y);
        let (left, bottom, right, top) = (x.wrapping_sub(1), y.wrapping_sub(1), x.wrapping_add(1), y.wrapping_add(1));
        let (px, py) = if c.parent != NIL { (st.node(c.parent).x, st.node(c.parent).y) } else { (0, 0) };
        let below = test_tile(st, host, x as u32, bottom as u32);
        let left_ok = test_tile(st, host, left as u32, y as u32);
        let right_ok = test_tile(st, host, right as u32, y as u32);
        let above = test_tile(st, host, x as u32, top as u32);
        if below && (px != x || py != bottom) {
            add_node(st, host, cur, x, bottom);
        }
        if left_ok && (px != left || py != y) {
            add_node(st, host, cur, left, y);
        }
        if right_ok && (px != right || py != y) {
            add_node(st, host, cur, right, y);
        }
        if above && (px != x || py != top) {
            add_node(st, host, cur, x, top);
        }
        if host.state().check_corners {
            if (left_ok || below) && (px != left || py != bottom) && test_tile(st, host, left as u32, bottom as u32) {
                add_node(st, host, cur, left, bottom);
            }
            if (below || right_ok) && (px != right || py != bottom) && test_tile(st, host, right as u32, bottom as u32)
            {
                add_node(st, host, cur, right, bottom);
            }
            if (right_ok || above) && (px != right || py != top) && test_tile(st, host, right as u32, top as u32) {
                add_node(st, host, cur, right, top);
            }
            if (above || left_ok) && (px != left || py != top) && test_tile(st, host, left as u32, top as u32) {
                add_node(st, host, cur, left, top);
            }
        } else {
            if test_tile(st, host, left as u32, bottom as u32) && (px != left || py != bottom) {
                add_node(st, host, cur, left, bottom);
            }
            if test_tile(st, host, right as u32, bottom as u32) && (px != right || py != bottom) {
                add_node(st, host, cur, right, bottom);
            }
            if test_tile(st, host, right as u32, top as u32) && (px != right || py != top) {
                add_node(st, host, cur, right, top);
            }
            if test_tile(st, host, left as u32, top as u32) && (px != left || py != top) {
                add_node(st, host, cur, left, top);
            }
        }
        st.add_closed(cur);
        let steps = host.state().steps;
        if host.is_step_limit(steps) {
            st.current = NIL;
            with(host, |s| s.found = true);
            break;
        }
        let revs = host.state().revs;
        if host.is_rev_limit(revs) {
            st.current = NIL;
            with(host, |s| s.found = false);
            break;
        }
        with(host, |s| {
            s.steps = s.steps.wrapping_add(1);
            s.revs = s.revs.wrapping_add(1);
        });
    }
    if !host.state().found {
        let cur = st.current;
        let current = if cur == NIL { None } else { Some((st.node(cur).x, st.node(cur).y)) };
        let has_open = st.has_open();
        host.search_done(current, has_open);
    }
    if host.search_failed() {
        if st.current != NIL {
            let c = st.current;
            st.add_closed(c);
            st.current = NIL;
        }
        restart(st, host);
    } else if !host.state().found && st.current != NIL {
        st.path = st.current;
    } else {
        st.current = NIL;
    }
}

/// 0x1d527 `Restart`: the nearest remaining node the goal accepts, else a new start node
pub fn restart<H: Host + ?Sized>(st: &mut Storage, host: &mut H) -> bool {
    let at_goal = host.accept_nearest();
    let mut done = at_goal;
    st.path = NIL;
    if at_goal {
        let node = next_node(st, host);
        if node != NIL {
            st.path = node;
            st.current = node;
            let (x, y) = (st.node(node).x, st.node(node).y);
            with(host, |s| {
                s.tile_x = x;
                s.tile_y = y;
            });
        } else {
            new_start_node(st, host);
        }
        with(host, |s| s.found = false);
        done = true;
    }
    while !done {
        let node = next_node(st, host);
        if node != NIL {
            let (x, y) = (st.node(node).x, st.node(node).y);
            done = host.accept_tile(x, y, true);
            if done {
                st.path = node;
                st.current = node;
                with(host, |s| {
                    s.tile_x = x;
                    s.tile_y = y;
                    s.found = false;
                });
            } else {
                st.release_node(node);
            }
        } else {
            new_start_node(st, host);
            with(host, |s| s.found = false);
            done = true;
        }
    }
    done
}

/// Restart's fallback: a node at the start tile with cost 1, the path.
fn new_start_node<H: Host + ?Sized>(st: &mut Storage, host: &mut H) {
    let (sx, sy) = host.start_tile();
    let node = st.new_node(sx, sy);
    st.current = node;
    let n = &mut st.nodes[node as usize];
    n.path_cost = 0;
    n.estimate = 1;
    n.cost = 1;
    st.path = node;
}

/// 0x1d6b1 `NextNode`: the nearer of the nearest open and closed nodes to the machine's tile (the open one on a tie)
pub fn next_node<H: Host + ?Sized>(st: &mut Storage, host: &mut H) -> i32 {
    let s = host.state();
    let mut open = st.remove_nearest_open(s.tile_x, s.tile_y);
    let closed = st.remove_nearest_closed(s.tile_x, s.tile_y);
    if open != NIL {
        if closed != NIL {
            if st.node(open).cost <= st.node(closed).cost {
                st.add_closed(closed);
            } else {
                st.add_open(open);
                open = closed;
            }
        }
    } else {
        open = closed;
    }
    open
}

/// 0x28fc9 `AddNode`: a new open node for the tile unless it is known at no more cost
pub fn add_node<H: Host + ?Sized>(st: &mut Storage, host: &mut H, parent: i32, x: i32, y: i32) {
    let p = *st.node(parent);
    // GetCost reads the current node's parent (the path goals' modifiers)
    let cur = st.current;
    let cur_parent = if cur != NIL && st.node(cur).parent != NIL {
        let pp = st.node(st.node(cur).parent);
        Some((pp.x, pp.y))
    } else {
        None
    };
    let cost = (host.get_cost(p.x, p.y, x, y, cur_parent) as u32).wrapping_add(p.path_cost as u32);
    let open = st.find_open(x, y);
    let closed = st.find_closed(x, y);
    if open != NIL && !host.is_cheaper(cost, st.node(open).path_cost as u32) {
        return;
    }
    if closed != NIL {
        if !host.is_cheaper(cost, st.node(closed).path_cost as u32) {
            return;
        }
        st.remove_closed(closed);
        st.release_node(closed);
    }
    if open == NIL {
        let node = st.new_node(x, y);
        st.nodes[node as usize].parent = parent;
        st.nodes[node as usize].path_cost = cost as i32;
        let s = host.state();
        let estimate = host.estimate(x, y, s.tile_x, s.tile_y);
        let n = &mut st.nodes[node as usize];
        n.estimate = estimate;
        n.cost = (n.estimate as u32).wrapping_add(cost);
        st.add_open(node);
    } else {
        let n = &mut st.nodes[open as usize];
        n.path_cost = cost as i32;
        n.parent = parent;
        let total = (n.estimate as u32).wrapping_add(cost);
        if n.cost != total {
            n.cost = total;
            st.update_open(open);
        }
    }
}
