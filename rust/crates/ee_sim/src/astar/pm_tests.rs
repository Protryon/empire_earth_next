//! The path manager over a fake host: the queues, the pools, the budgets, the cancels, the saved game's layouts.

use super::*;
use std::collections::HashMap;

/// A machine of the fake host: it runs `runs` more times before it is done, `steps` per run.
#[derive(Clone, Debug, Default)]
struct FakeMachine {
    #[allow(dead_code)]
    graph: bool,
    runs: i64,
    steps: i64,
    rev_limit: i64,
    max_revs: i64,
    kind: i64,
    found: bool,
    deleted: bool,
}

/// The fake world: units by id (pointer = id + 0x1000), machines by handle (>= 0x100), the calls in order.
#[derive(Default)]
struct Fake {
    machines: HashMap<u64, FakeMachine>,
    next: u64,
    log: Vec<(i32, u64, i64)>,
    refs: HashMap<u64, i64>,
    runs_per_search: i64,
    steps: i64,
    time: i64,
    waypoints: i64,
    short_done: bool,
}

const UNIT: u64 = 0x1000;

impl Fake {
    fn new(runs: i64, steps: i64) -> Fake {
        Fake { next: 0x100, runs_per_search: runs, steps, ..Fake::default() }
    }
    fn machine(&mut self, graph: bool) -> u64 {
        self.next += 1;
        let m =
            FakeMachine { graph, runs: self.runs_per_search, steps: self.steps, rev_limit: -1, ..Default::default() };
        self.machines.insert(self.next, m);
        self.next
    }
    fn count(&self, op: i32) -> usize {
        self.log.iter().filter(|e| e.0 == op).count()
    }
}

impl PathHost for Fake {
    fn call(&mut self, op: i32, io: &mut EePathIo) -> i64 {
        self.log.push((op, io.h[0], io.i[0]));
        let h = io.h[0];
        match op {
            EE_PATH_UNIT_ID => (h - UNIT) as i64,
            EE_PATH_UNIT_RESOLVE | EE_PATH_WORLD_UNIT => io.i[0] + UNIT as i64,
            EE_PATH_UNIT_REFERENCE | EE_PATH_REQUEST_REFERENCE => {
                *self.refs.entry(h).or_default() += 1;
                0
            }
            EE_PATH_UNIT_DEREFERENCE | EE_PATH_REQUEST_DEREFERENCE => {
                *self.refs.entry(h).or_default() -= 1;
                0
            }
            EE_PATH_GAME_TIME => self.time,
            EE_PATH_SUB_SECTORS => 9,
            EE_PATH_MAP => 0x77,
            EE_PATH_CLAMP => 0,
            EE_PATH_PM_NEW => self.machine(false) as i64,
            EE_PATH_GM_NEW => self.machine(true) as i64,
            EE_PATH_PM_DELETE | EE_PATH_GM_DELETE => {
                self.machines.get_mut(&h).unwrap().deleted = true;
                0
            }
            EE_PATH_PM_MAP => 0x77,
            EE_PATH_GM_COUNT => 10,
            EE_PATH_PM_TYPE | EE_PATH_GM_TYPE => {
                self.machines.get_mut(&h).unwrap().kind = io.i[0];
                0
            }
            EE_PATH_TM_RUN | EE_PATH_GM_RUN => {
                let m = self.machines.get_mut(&h).unwrap();
                m.runs -= 1;
                m.found = m.runs > 0;
                m.found as i64
            }
            EE_PATH_TM_SET_REV_LIMIT | EE_PATH_GM_SET_REV_LIMIT => {
                self.machines.get_mut(&h).unwrap().rev_limit = io.i[0];
                0
            }
            EE_PATH_TM_REV_LIMIT | EE_PATH_GM_REV_LIMIT => self.machines[&h].rev_limit,
            EE_PATH_TM_STEPS | EE_PATH_GM_STEPS => self.machines[&h].steps,
            EE_PATH_TM_FOUND | EE_PATH_GM_FOUND => self.machines[&h].found as i64,
            EE_PATH_PM_RELEASE | EE_PATH_GM_RELEASE | EE_PATH_PM_RESET | EE_PATH_GM_RESET => {
                let runs = self.runs_per_search;
                let m = self.machines.get_mut(&h).unwrap();
                m.found = false;
                m.runs = runs;
                0
            }
            EE_PATH_PM_START => {
                self.machines.get_mut(&h).unwrap().rev_limit = io.i[4];
                self.machines.get_mut(&h).unwrap().max_revs = io.i[5];
                0
            }
            EE_PATH_GM_START => {
                if io.i[5] == 0 {
                    self.machines.get_mut(&h).unwrap().rev_limit = 0xf;
                }
                0
            }
            EE_PATH_SET_MAX_REVS => {
                self.machines.get_mut(&h).unwrap().max_revs = io.i[1];
                0
            }
            EE_PATH_PATH_REVS | EE_PATH_GRAPH_REVS => {
                io.i[0] = 10;
                io.i[1] = 1;
                io.i[2] = EE_PATH_NO_UNIT as i64;
                0
            }
            EE_PATH_UNIT_PLAYER => 1,
            EE_PATH_UNIT_WAYPOINTS => self.waypoints,
            EE_PATH_UNIT_PATH_TYPE => 1,
            EE_PATH_UNIT_COUNT_REQUEST => 1,
            // the short search's goal took the nearest tile and accepted it (pm_bounded.rs try_short_path)
            crate::astar::pm_bounded::EE_PATH_TM_FLAGS => {
                if self.short_done {
                    3
                } else {
                    0
                }
            }
            EE_PATH_REQUEST_NEW => 0x5000 + io.i[0],
            EE_PATH_HAS_MAP | EE_PATH_PM_LOAD => 1,
            _ => 0,
        }
    }
}

fn unit(id: u64) -> u64 {
    UNIT + id
}

/// A unit's tile search: queued while it runs, the step budget and the growing step limit, finished and its machine
/// kept as a spare, the unit's reference given back.
#[test]
fn tile_search_runs_until_done() {
    // 32 steps a run: one run per update (a lone search under the budget runs again in the same update)
    let mut host = Fake::new(3, 32);
    let mut m = PathManager::default();
    m.request_unit_path(&mut host, unit(7), (1, 2), (5, 6), false, -1, false, 0, false);
    assert_eq!(m.paths.len(), 1);
    let n = m.paths[0];
    assert_eq!(m.node(n).tile, (5, 6));
    assert_eq!(m.node(n).passes, 1);
    assert_eq!(m.node(n).unit, 7);
    assert_eq!(host.refs[&unit(7)], 1);
    // the history counts the request at its unclamped goal; the short search ran first
    assert_eq!(m.record.history.len(), 1);
    assert_eq!(host.count(crate::astar::pm_bounded::EE_PATH_BM_RUN), 1);
    assert!(!m.is_path_done(&mut host, unit(7)));
    assert_eq!(m.path_goal(&mut host, unit(7)), (false, (5, 6)));
    m.update(&mut host);
    // the second run is still running: the step limit grew from 0xf to 0x10, back in the queue
    let mach = m.node(m.paths[0]).path_machine;
    assert_eq!(host.machines[&mach].rev_limit, 0x10);
    m.update(&mut host);
    assert!(m.paths.is_empty());
    // the short search's result and the tile search's: each snaps the unit's last waypoint
    assert_eq!(host.count(crate::astar::pm_bounded::EE_PATH_UNIT_SNAP), 2);
    assert_eq!(host.refs[&unit(7)], 0);
    assert_eq!(m.spare_paths.len(), 1);
    assert!(m.is_path_done(&mut host, unit(7)));
    // the spare machine is the next search's
    m.request_unit_path(&mut host, unit(8), (1, 2), (3, 3), false, -1, false, 0, false);
    assert_eq!(m.node(m.paths[0]).path_machine, mach);
}

/// The budget: searches run in turn until 32 steps are spent.
#[test]
fn update_spends_32_steps() {
    let mut host = Fake::new(100, 10);
    let mut m = PathManager::default();
    for id in 1..=5 {
        m.request_unit_path(&mut host, unit(id), (0, 0), (id as i32, 0), false, -1, false, 0, false);
    }
    let first: Vec<u32> = m.paths.iter().copied().collect();
    m.update(&mut host);
    // four runs of 10 steps: the first four moved to the back in order
    let after: Vec<u32> = m.paths.iter().copied().collect();
    assert_eq!(after, [first[4], first[0], first[1], first[2], first[3]]);
    for (k, &n) in first.iter().enumerate() {
        assert_eq!(m.node(n).passes, if k < 4 { 2 } else { 1 });
    }
}

/// The short search settles a request: no tile search queued.
#[test]
fn short_path_settles() {
    let mut host = Fake::new(3, 1);
    host.short_done = true;
    let mut m = PathManager::default();
    m.request_unit_path(&mut host, unit(1), (0, 0), (2, 2), false, -1, false, 0, false);
    assert!(m.paths.is_empty());
    // an unlimited request skips it
    m.request_unit_path(&mut host, unit(1), (0, 0), (2, 2), true, -1, false, 0, false);
    assert_eq!(m.paths.len(), 1);
    let mach = m.node(m.paths[0]).path_machine;
    assert_eq!(host.machines[&mach].rev_limit, -1);
    // a start of -10000 asks nothing
    let calls = host.log.len();
    m.request_unit_path(&mut host, unit(1), (-10000, 0), (2, 2), true, -1, false, 0, false);
    assert_eq!(host.log.len(), calls);
}

/// A request's graph search: queued with its request referenced, cancelled by the request, completed at once.
#[test]
fn graph_requests() {
    let mut host = Fake::new(3, 1);
    let mut m = PathManager::default();
    let r = m.request_path(&mut host, 2, (0, 0), (9, 9), 0xf1a6, 1, -1, 0, false);
    assert_eq!(r, 0x5000);
    assert_eq!(m.next_request_id, 1);
    assert_eq!(m.auto_paths.len(), 1);
    assert_eq!(host.refs[&r], 1);
    // a node without a unit: `HasAutoPath(NULL)` sees it, as the C++'s handle compare did
    assert!(m.has_auto_path(&mut host, 0));
    assert!(!m.has_auto_path(&mut host, unit(3)));
    let r2 = m.request_path(&mut host, 2, (0, 0), (8, 8), 0xf1a6, 1, -1, 0, false);
    m.complete_auto_path(&mut host, r2);
    assert_eq!(m.auto_paths.len(), 1);
    assert_eq!(host.count(EE_PATH_REQUEST_UNIT_PATHS), 1);
    m.cancel_auto_path_request(&mut host, r);
    assert!(m.auto_paths.is_empty());
    assert_eq!(m.spare_graphs.len(), 2);
    // the released nodes keep their requests until reused (the pool's)
    assert_eq!(host.refs[&r], 1);
    m.reset(&mut host);
    assert_eq!(host.refs[&r], 0);
    assert_eq!(host.refs[&r2], 0);
    assert_eq!(m.next_request_id, 0);
}

/// The spare pools keep 35 path machines and 25 graph machines.
#[test]
fn spare_pool_caps() {
    let mut host = Fake::new(100, 1);
    let mut m = PathManager::default();
    for id in 0..40 {
        m.request_unit_path(&mut host, unit(id), (0, 0), (1, 1), false, -1, false, 0, false);
    }
    m.free_paths(&mut host);
    assert_eq!(m.spare_paths.len(), SPARE_PATH_MACHINES);
    assert_eq!(host.machines.values().filter(|x| x.deleted).count(), 40 - SPARE_PATH_MACHINES);
    m.free_machines(&mut host);
    assert!(m.spare_paths.is_empty());
    assert!(host.machines.values().all(|x| x.deleted));
}

/// The pending units ask again last first, then none is pending.
#[test]
fn pending_last_first() {
    let mut host = Fake::new(3, 1);
    let mut m = PathManager::default();
    m.pending.push(Pending { unit: 4, x: 1, y: 1 });
    m.pending.push(Pending { unit: 5, x: 2, y: 2 });
    m.request_pending_paths(&mut host);
    assert!(m.pending.is_empty());
    let units: Vec<u64> = host.log.iter().filter(|e| e.0 == EE_PATH_PENDING_UNIT).map(|e| e.1).collect();
    assert_eq!(units, [unit(5), unit(4)]);
}

/// The region updates: scheduled now or after the delay, rebuilt when due.
#[test]
fn region_updates() {
    let mut host = Fake::new(3, 1);
    host.time = 500;
    let mut m = PathManager::default();
    m.set_region_updates(&mut host, true, true);
    assert!(m.updating_regions);
    assert_eq!(m.next_region_time, 501);
    host.time = 501;
    m.update(&mut host);
    // the fake's rebuild answers 0: not done, again in 0.1 s
    assert!(m.updating_regions);
    assert_eq!(m.next_region_time, 601);
}

/// The dump's lines.
#[test]
fn dump_lines() {
    let mut host = Fake::new(3, 2);
    let mut m = PathManager::default();
    m.request_unit_path(&mut host, unit(7), (1, 2), (5, 6), false, -1, false, 0, false);
    m.pending.push(Pending { unit: 9, x: 3, y: 4 });
    let s = m.dump(&mut host);
    assert_eq!(
        s,
        "PATHS 1: u7@5,6/p1/s2/f1\nAUTOPATHS 0:\nPENDING 1: u9@3,4\nPATHMANAGER history=1 regions=0 prunehistory=0 \
         pruneregions=0 nextrequest=0 nextregion=0 updating=0\n"
    );
}

/// The saved layouts: the typed values in the C++ writers' order, read back.
#[test]
fn saved_layouts() {
    let mut s = SavedSearch { unit: 12, tile: [3, 4], node: [1, 0, 0, 0, 0, 0], ..SavedSearch::default() };
    s.m.fields = [1, 2, 3, 4, 5, 6, 7, 8];
    s.m.offset = [0.5, 0.25];
    s.m.continent = 3;
    s.m.flags[2] = true;
    s.lists.blocked = vec![[1, 1], [2, 2]];
    s.lists.storage = vec![9, 8, 7];
    let mut w: Vec<Item> = Vec::new();
    s.write(&mut w);
    // unit, tile 2, node 6, machine tile 2, fields 8, 2 flags, goal 6, 6 flags, blocked 1 + 4, unit tiles 1,
    // bounds 12, destination 2, l64, offsets 2, continent, 2 flags, length, bytes
    assert_eq!(w.len(), 1 + 2 + 6 + 2 + 8 + 2 + 6 + 6 + 5 + 1 + 12 + 2 + 1 + 2 + 1 + 2 + 1 + 1);
    assert_eq!(w[0], Item::U32(12));
    assert_eq!(w[w.len() - 6], Item::F32(0.25));
    assert_eq!(w[w.len() - 5], Item::U16(3));
    assert_eq!(w[w.len() - 1], Item::Bytes(vec![9, 8, 7]));
    let mut r: VecDeque<Item> = w.into_iter().collect();
    assert_eq!(SavedSearch::read(&mut r).unwrap(), s);
    assert!(r.is_empty());

    let mut g = SavedGraph { unit: -1, node: [0, 5, 6, 0, 0, 0, 0, 2, 0], ..SavedGraph::default() };
    g.request = Some(RequestRecord {
        r: EePathRequestRecord { id: 4, done: true, flags: [true; 17], ..Default::default() },
        path: vec![[1, 2]],
    });
    g.g.modifier = 1;
    g.g.weights[3] = 2.5;
    let mut w: Vec<Item> = Vec::new();
    g.write(&mut w);
    let mut r: VecDeque<Item> = w.clone().into_iter().collect();
    assert_eq!(SavedGraph::read(&mut r).unwrap(), g);
    // a storm modifier carries no influence values
    g.g.modifier = EE_PATH_MODIFIER_STORM_RAIN;
    let mut w2: Vec<Item> = Vec::new();
    g.write(&mut w2);
    assert_eq!(w.len() - w2.len(), 15 + 2 + 2 + 3);
    // empty storage: only its length
    assert_eq!(w2.last(), Some(&Item::U32(0)));

    let mut rs =
        RunningState { lcc: 3, next_request_id: 7, next_region_time: 9, updating_regions: true, ..Default::default() };
    rs.record.add_history(1, 2, 3, 4);
    let mut w: Vec<Item> = Vec::new();
    rs.write(&mut w);
    let mut r: VecDeque<Item> = w.into_iter().collect();
    assert_eq!(RunningState::read(&mut r).unwrap(), rs);
    // a short record is the file's end
    let mut r: VecDeque<Item> = vec![Item::U32(1), Item::U32(2)].into_iter().collect();
    assert_eq!(RunningState::read(&mut r), Err(EE_PATH_ERROR_BEYOND_END));
}

/// A loaded game: the searches' units ask no path again, the searches go back at the queues' front.
#[test]
fn loaded_searches_go_first() {
    let mut host = Fake::new(5, 1);
    let mut m = PathManager::default();
    m.request_unit_path(&mut host, unit(1), (0, 0), (1, 1), false, -1, false, 0, false);
    let s = SavedSearch { unit: 2, tile: [7, 7], ..SavedSearch::default() };
    let mut w: Vec<Item> = vec![Item::U32(1)];
    s.write(&mut w);
    m.pending.push(Pending { unit: 2, x: 7, y: 7 });
    m.pending.push(Pending { unit: 3, x: 1, y: 1 });
    let mut r: VecDeque<Item> = w.into_iter().collect();
    assert_eq!(m.load_searches(&mut r), 0);
    assert_eq!(m.pending, [Pending { unit: 3, x: 1, y: 1 }]);
    // the fake's map is there and its machines load: the loaded search goes first
    m.restore_searches(&mut host);
    assert_eq!(m.paths.len(), 2);
    assert_eq!(m.node(m.paths[0]).unit, 2);
    assert_eq!(m.node(m.paths[0]).tile, (7, 7));
}
