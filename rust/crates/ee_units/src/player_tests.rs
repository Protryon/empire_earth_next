use super::*;

type NodeRow = (c_long, bool, i64, c_long, c_long, c_long, bool, bool, Vec<i64>, [c_long; 7]);
type ClassRow = (i64, c_long, i64, c_long, bool, bool, bool);
type UpgradeRow = (i64, [c_long; 13], [c_long; 13], c_long, bool);

/// A player's world: tech nodes by id (their node data and costs by resource), unit classes, upgrades (level, cost
/// per improvement), the resources the takes and adds change, and the calls made.
#[derive(Default)]
struct World {
    /// id -> (makes unit, unit class, cost resource, required statistic, required value, available, researched,
    /// unit classes, costs [7])
    nodes: Vec<NodeRow>,
    /// type -> (tech, weight, population, teleporter, hero, improvements)
    classes: Vec<ClassRow>,
    /// type -> (level per improvement, cost per improvement, total cost, can upgrade)
    upgrades: Vec<UpgradeRow>,
    scale: c_long,
    metallurgy: bool,
    extra: c_long,
    limit: c_long,
    heroes: c_long,
    scenario: bool,
    local: bool,
    points_max: c_long,
    max_level: c_long,
    step: f32,
    factor: f32,
    saved: Box<EeUnitsPlayerSaved>,
    log: Vec<String>,
}

impl World {
    fn node_at(&self, ptr: i64) -> &NodeRow {
        &self.nodes[ptr as usize - 1]
    }
    fn upgrade(&self, t: i64) -> Option<&UpgradeRow> {
        self.upgrades.iter().find(|u| u.0 == t)
    }
}

impl PlayerHost for World {
    fn call(&mut self, op: i32, io: &mut EeUnitsPlayerIo) -> i64 {
        let a = io.i;
        match op {
            EE_UNITS_PLAYER_NODE => self.nodes.iter().position(|n| n.0 as i64 == a[0]).map_or(0, |p| p as i64 + 1),
            EE_UNITS_PLAYER_NODE_INFO => {
                let n = self.node_at(a[0]).clone();
                io.i = [i64::from(n.1), n.2, n.3 as i64, n.4 as i64, n.5 as i64, 0, n.8.len() as i64, i64::from(n.7)];
                0
            }
            EE_UNITS_PLAYER_NODE_AVAILABLE => i64::from(self.node_at(a[0]).6),
            EE_UNITS_PLAYER_NODE_UNIT_CLASS => self.node_at(a[0]).8[a[1] as usize],
            EE_UNITS_PLAYER_NODE_COST => {
                self.log.push(format!("cost {} {}", self.node_at(a[0]).0, a[1]));
                self.node_at(a[0]).9[a[1] as usize] as i64
            }
            EE_UNITS_PLAYER_SCALE_COST => a[0] * self.scale as i64,
            EE_UNITS_PLAYER_METALLURGY => i64::from(self.metallurgy),
            EE_UNITS_PLAYER_TAKE | EE_UNITS_PLAYER_ADD => {
                let sign = if op == EE_UNITS_PLAYER_TAKE { -1 } else { 1 };
                self.saved.mResourceCaps[a[0] as usize] += sign * a[1] as c_long;
                self.log.push(format!("{} {} {}", if sign < 0 { "take" } else { "add" }, a[0], a[1]));
                0
            }
            EE_UNITS_PLAYER_CLASS => match self.classes.iter().find(|c| c.0 == a[0]) {
                Some(c) => {
                    io.i = [c.1 as i64, c.2, c.3 as i64, i64::from(c.4), i64::from(c.5), i64::from(c.6), 0, 0];
                    1
                }
                None => 0,
            },
            EE_UNITS_PLAYER_EXTRA_POPULATION => self.extra as i64,
            EE_UNITS_PLAYER_POPULATION_LIMIT => self.limit as i64,
            EE_UNITS_PLAYER_HERO_COUNT => self.heroes as i64,
            EE_UNITS_PLAYER_SCENARIO => i64::from(self.scenario),
            EE_UNITS_PLAYER_CAN_AFFORD => {
                self.log.push("can afford".into());
                1
            }
            EE_UNITS_PLAYER_REASON => {
                self.log.push(format!("reason {}", a[1]));
                0
            }
            EE_UNITS_PLAYER_STATS_LOWER => {
                self.log.push(format!("lower {} {}", a[0], a[1]));
                0
            }
            EE_UNITS_PLAYER_IMPROVEMENT => {
                let (step, factor, points, max) = (self.step, self.factor, self.points_max, self.max_level);
                io.f = [if a[1] == 12 { 0.0 } else { step }, factor];
                match self.upgrade(a[0]) {
                    Some(u) => {
                        io.i = [
                            i64::from(u.4),
                            u.2[a[1] as usize] as i64,
                            u.3 as i64,
                            u.1[a[1] as usize] as i64,
                            max as i64,
                            points as i64,
                            0,
                            0,
                        ];
                        1
                    }
                    None => 0,
                }
            }
            EE_UNITS_PLAYER_IMPROVEMENT_COST => self.upgrade(a[0]).map_or(0, |u| u.2[a[1] as usize] as i64),
            EE_UNITS_PLAYER_LEVEL => match self.upgrade(a[0]) {
                Some(u) => {
                    io.i[0] = u.1[a[1] as usize] as i64;
                    1
                }
                None => 0,
            },
            EE_UNITS_PLAYER_IMPROVEMENT_TIME => {
                if a[1] != 0 {
                    2000
                } else {
                    1000
                }
            }
            EE_UNITS_PLAYER_IS_LOCAL => i64::from(self.local),
            EE_UNITS_PLAYER_UPDATE_JIT => {
                self.log.push("jit".into());
                0
            }
            EE_UNITS_PLAYER_PAY_IMPROVEMENT => {
                self.log.push(format!("pay improvement {} {}", a[0], a[1]));
                0
            }
            _ => panic!("op {op}"),
        }
    }
}

struct Run {
    w: World,
    fields: Box<EeUnitsPlayerFields>,
    queue: Box<EeUnitsQueued>,
    stats: Box<EeUnitsPlayerStats>,
}

impl Run {
    fn new(w: World) -> Self {
        let mut fields = Box::new(EeUnitsPlayerFields::default());
        reset_fields(&mut fields, 3, 5);
        Run { w, fields, queue: Box::default(), stats: Box::default() }
    }
    fn caps(&mut self, caps: [c_long; 7]) -> &mut Self {
        self.w.saved.mResourceCaps = caps;
        self
    }
    fn p<R>(&mut self, body: impl FnOnce(&mut Player<World>) -> R) -> R {
        self.fields.mQueued = &mut *self.queue;
        let saved: *mut EeUnitsPlayerSaved = &mut *self.w.saved;
        let fields: *mut EeUnitsPlayerFields = &mut *self.fields;
        let stats: *const EeUnitsPlayerStats = &*self.stats;
        body(&mut Player { h: &mut self.w, saved, fields, stats })
    }
}

fn node(id: c_long, costs: [c_long; 7]) -> NodeRow {
    (id, false, 0, 0, 0, 0, true, false, vec![], costs)
}

#[test]
fn reset_sets_the_members_and_leaves_the_queue() {
    let mut f = EeUnitsPlayerFields { mNumHeroes: 9, mDamageFrom: [4; 17], mInstantBuild: true, ..Default::default() };
    reset_fields(&mut f, 7, 8);
    assert_eq!((f.mConversionFactor, f.mTributeFee, f.mHeroLimit, f.mNumHeroes), (1.0, 0.4, 1, 0));
    assert_eq!((f.mMaxStartingCitizens, f.mMiningLimit, f.mHackUnitClass), (7, 8, 0xffff_ffff));
    assert_eq!(f.mDamageFrom, [0; 17]);
    assert!(!f.mInstantBuild && f.mQueued.is_null());
}

#[test]
fn the_scaled_cost_adds_its_epsilon_and_truncates_and_metallurgy_pools_iron_and_gold() {
    let mut r = Run::new(World { nodes: vec![node(1000, [0, 0, 10, 0, 0, 50, 50])], scale: 1, ..Default::default() });
    r.caps([0, 0, 15, 0, 0, 30, 80]);
    // food 10 * 1.5 = 15 (+ 0.0001: 15); gold 75 > 30
    assert!(!r.p(|p| p.can_afford_scaled(1, 0, 1.5, 1)));
    // the costs asked in order, stopping at the gold
    assert_eq!(r.w.log, ["cost 1000 2", "cost 1000 3", "cost 1000 4", "cost 1000 5"]);
    r.w.metallurgy = true;
    r.w.log.clear();
    // gold short: iron 75 and gold 75 against 30 + 80 = 110 < 150, still short
    assert!(!r.p(|p| p.can_afford_scaled(1, 0, 1.5, 1)));
    assert_eq!(r.w.log[4..], ["cost 1000 6", "cost 1000 5"]);
    r.caps([0, 0, 15, 0, 0, 30, 130]);
    assert!(r.p(|p| p.can_afford_scaled(1, 0, 1.5, 1)));
    // a count multiplies each scaled cost
    assert!(!r.p(|p| p.can_afford_scaled(1, 0, 1.0, 2)));
    assert_eq!(trunc_long(f32::NAN), c_long::MIN);
    assert_eq!(trunc_long(-2.9), -2);
}

#[test]
fn a_single_resource_needs_the_required_statistic() {
    let mut n = node(1001, [0, 0, 40, 40, 0, 0, 0]);
    n.3 = 2;
    n.4 = 0x18;
    n.5 = 3;
    let mut r = Run::new(World { nodes: vec![n], scale: 1, ..Default::default() });
    r.caps([0, 0, 50, 0, 0, 0, 0]);
    // food covers its 40, but the statistic is 0 < 3
    assert!(!r.p(|p| p.can_afford_single(1, 0, 1)));
    r.stats.set(0x18, 3, 1, 0);
    assert!(r.p(|p| p.can_afford_single(1, 0, 1)));
    // CanAfford: all resources (wood 40 > 0) fail, the single one holds
    assert!(r.p(|p| p.can_afford(1, 0, 1)));
    // no required value: never
    r.w.nodes[0].5 = 0;
    assert!(!r.p(|p| p.can_afford_single(1, 0, 1)));
}

#[test]
fn pay_takes_the_single_resource_and_lowers_the_statistic() {
    let mut n = node(1001, [0, 0, 40, 40, 0, 0, 0]);
    n.3 = 2;
    n.4 = 0x18;
    n.5 = 3;
    let mut r = Run::new(World { nodes: vec![n], scale: 1, ..Default::default() });
    r.caps([0, 0, 50, 0, 0, 0, 0]);
    r.stats.set(0x18, 3, 1, 0);
    r.p(|p| p.pay(1, 0, 0.5));
    assert_eq!(r.w.saved.mResourceCaps[2], 30);
    assert!(r.w.log.contains(&"lower 24 3".to_string()));
}

#[test]
fn pay_takes_every_resource_or_nothing_past_a_short_one_and_metallurgy_covers_gold_with_iron() {
    let mut r = Run::new(World { nodes: vec![node(1002, [0, 0, 10, 20, 0, 30, 0])], scale: 1, ..Default::default() });
    r.caps([0, 0, 100, 100, 0, 100, 100]);
    r.p(|p| p.pay(1, 0, 1.0));
    assert_eq!(r.w.saved.mResourceCaps, [0, 0, 90, 80, 0, 70, 100]);
    // gold short (30 > 20): with metallurgy, gold 20 and iron 10
    r.caps([0, 0, 100, 100, 0, 20, 100]);
    r.w.metallurgy = true;
    r.p(|p| p.pay(1, 0, 1.0));
    assert_eq!(r.w.saved.mResourceCaps, [0, 0, 90, 80, 0, 0, 90]);
    // wood short: nothing taken
    r.caps([0, 0, 100, 10, 0, 100, 100]);
    r.p(|p| p.pay(1, 0, 1.0));
    assert_eq!(r.w.saved.mResourceCaps, [0, 0, 100, 10, 0, 100, 100]);
}

#[test]
fn can_produce_checks_population_teleporter_heroes_cost_and_a_producer() {
    let mut n = node(1003, [0; 7]);
    n.1 = true;
    n.2 = 40;
    n.8 = vec![1];
    let mut r = Run::new(World {
        nodes: vec![n],
        classes: vec![(40, 1003, 1, 2, false, true, false)],
        scale: 1,
        limit: 10,
        heroes: 1,
        ..Default::default()
    });
    r.w.saved.mPopulationCount = 9;
    // 2 + 9 > 0 + 10, and 9 == 10 - 1 but 11 > 10: over the limit
    assert!(!r.p(|p| p.can_produce(1003, 0, 1, false, 3, 7)));
    assert_eq!(r.w.log, ["reason 3"]);
    r.w.extra = 1;
    // the hero limit (1 hero of 1)
    r.w.log.clear();
    assert!(!r.p(|p| p.can_produce(1003, 0, 1, false, 3, 7)));
    assert_eq!(r.w.log, ["reason 2"]);
    // no wonder mode: no hero test; the cost asked through the virtual CanAfford
    r.w.log.clear();
    assert!(r.p(|p| p.can_produce(1003, 0, 1, true, 0, 7)));
    assert_eq!(r.w.log, ["can afford"]);
    // a scenario game ignores the hero limit
    r.w.scenario = true;
    assert!(r.p(|p| p.can_produce(1003, 0, 1, false, 3, 0)));
    // the teleporter: one
    r.w.classes[0].4 = true;
    r.fields.mNumTeleporters = 1;
    assert!(!r.p(|p| p.can_produce(1003, 0, 1, false, 3, 0)));
    // an unavailable node or one without a unit class: no
    r.w.nodes[0].6 = false;
    assert!(!r.p(|p| p.can_produce(1003, 0, 1, false, 3, 0)));
    r.w.nodes[0].6 = true;
    r.w.nodes[0].1 = false;
    r.w.nodes[0].8.clear();
    assert!(!r.p(|p| p.can_produce(1003, 0, 1, false, 3, 0)));
}

fn improving() -> Run {
    let mut levels = [0; 13];
    levels[3] = 1;
    let mut costs = [0; 13];
    costs[3] = 2;
    costs[4] = 3;
    Run::new(World {
        nodes: vec![node(1004, [0, 0, 100, 0, 0, 0, 0])],
        classes: vec![(40, 1004, 1, 1, false, false, true)],
        upgrades: vec![(40, levels, costs, 1, true)],
        scale: 1,
        points_max: 10,
        max_level: 3,
        step: 0.5,
        factor: 0.5,
        ..Default::default()
    })
}

#[test]
fn the_level_counts_the_queue_and_can_buy_counts_its_points_and_cost() {
    let mut r = improving();
    r.queue.map.insert(40, (0.0, vec![3, 4, 3]));
    assert_eq!(r.p(|p| p.level(40, 3)), 3);
    assert_eq!(r.p(|p| p.count_queued(40, 3)), 1);
    assert_eq!(r.p(|p| p.level(41, 3)), -1);
    // points: 2 (its own) + 1 + 2 + 3 + 2 = 10 > 10 is false: 10 <= 10; the level 3 <= 3; the cost 100 * 3 * 0.5
    r.caps([0, 0, 149, 0, 0, 0, 0]);
    assert!(!r.p(|p| p.can_buy(40, 3, 7)));
    assert_eq!(*r.w.log.last().unwrap(), "reason 4");
    r.caps([0, 0, 150, 0, 0, 0, 0]);
    assert!(r.p(|p| p.can_buy(40, 3, 7)));
    // one more queued: 12 points, the maximum
    r.queue.map.get_mut(&40).unwrap().1.push(3);
    assert!(!r.p(|p| p.can_buy(40, 3, 7)));
    assert_eq!(*r.w.log.last().unwrap(), "reason 6");
    assert!(r.p(|p| p.is_improvement_maxed(40, 3)));
    // CanImprove ignores the queue; improvement 12 has no step
    assert!(r.p(|p| p.can_improve(40, 3)));
    assert!(!r.p(|p| p.can_improve(40, 12)));
}

#[test]
fn queue_pays_unless_paid_and_remove_refunds_and_stops_the_running_one() {
    let mut r = improving();
    r.caps([0, 0, 1000, 0, 0, 0, 0]);
    r.p(|p| p.queue_improvement(40, 3, false, false));
    r.p(|p| p.queue_improvement(40, 4, true, true));
    assert_eq!(r.queue.map[&40].1, vec![4, 3]);
    assert_eq!(r.w.log.iter().filter(|l| l.starts_with("pay improvement")).count(), 1);
    r.queue.map.get_mut(&40).unwrap().0 = 5.0;
    // not the running one: 4 is running, so 4 is not found past it
    r.p(|p| p.remove_queued(40, 4, true));
    assert_eq!(r.queue.map[&40].1, vec![4, 3]);
    r.w.log.clear();
    r.p(|p| p.remove_queued(40, 4, false));
    assert_eq!(r.queue.map[&40], (0.0, vec![3]));
    // the refund at the level after the removal: improvement 4's is 0 (none queued now), so nothing
    assert_eq!(r.w.log.iter().filter(|l| l.starts_with("add 2")).collect::<Vec<_>>(), ["add 2 0"]);
    r.p(|p| p.remove_queued(40, 3, false));
    assert!(r.queue.map.is_empty());
}

#[test]
fn advance_starts_the_first_improvement_for_its_level_and_reports_an_empty_queue() {
    let mut r = improving();
    r.w.local = true;
    r.queue.map.insert(40, (0.0, vec![3]));
    assert!(!r.p(|p| p.advance(40)));
    // its level is 1: the first time
    assert_eq!(r.queue.map[&40].0, 1000.0);
    assert_eq!(r.w.log, ["jit"]);
    r.fields.mInstantBuild = true;
    r.p(|p| p.advance(40));
    assert_eq!(r.queue.map[&40].0, 1.0);
    r.queue.map.get_mut(&40).unwrap().1.clear();
    assert!(r.p(|p| p.advance(40)));
    assert!(!r.p(|p| p.advance(99)));
}

#[test]
fn the_requeue_keeps_what_can_be_bought() {
    let mut r = improving();
    r.caps([0, 0, 1000, 0, 0, 0, 0]);
    r.queue.map.insert(40, (7.0, vec![3, 3, 3]));
    r.queue.map.insert(41, (0.0, vec![3]));
    r.w.max_level = 2;
    r.p(|p| p.requeue());
    // 40: the level may reach 2 (1 + 1 queued, then 2 <= 2), a third asks at 3 > 2; 41 has no class: erased
    assert_eq!(r.queue.map.len(), 1);
    assert_eq!(r.queue.map[&40], (7.0, vec![3, 3]));
    assert!(!r.w.log.iter().any(|l| l.starts_with("pay improvement")));
}

#[test]
fn pay_and_refund_improvement_at_the_queued_level() {
    let mut r = improving();
    r.caps([0, 0, 30, 0, 0, 0, 0]);
    // level 1 * 0.5: food 50; short: no cover outside gold and iron, so nothing taken for food
    r.p(|p| p.pay_improvement(40, 3));
    assert_eq!(r.w.saved.mResourceCaps[2], 30);
    r.caps([0, 0, 60, 0, 0, 0, 0]);
    r.p(|p| p.pay_improvement(40, 3));
    assert_eq!(r.w.saved.mResourceCaps[2], 10);
    r.p(|p| p.refund_improvement(40, 3, 0.5));
    assert_eq!(r.w.saved.mResourceCaps[2], 35);
}

#[test]
fn the_list_io_reads_into_the_last_value_and_writes_until_a_failure() {
    let words = std::cell::RefCell::new(vec![2i64, 7, 9]);
    let mut list = vec![1];
    let r = load_list(&mut list, |_, io| {
        let mut w = words.borrow_mut();
        if w.is_empty() {
            return 5;
        }
        io.i = w.remove(0);
        0
    });
    assert_eq!((r, list), (0, vec![1, 7, 9]));
    let mut out = Vec::new();
    assert_eq!(
        save_list(&[4, 5], |_, io| {
            out.push(io.i);
            0
        }),
        0
    );
    assert_eq!(out, [2, 4, 5]);
}

#[test]
fn the_queue_ffi_erases_moves_and_reads() {
    let q = ee_units_queued_new();
    // SAFETY: a live queue from ee_units_queued_new, freed at the end
    unsafe {
        (*q).map.insert(3, (2.5, vec![9, 1]));
        assert_eq!(ee_units_queued_timer(q, 3), 2.5);
        assert_eq!(ee_units_queued_timer(q, 4), 0.0);
        assert_eq!(ee_units_queued_op(q, 2, 3, 0), 9);
        assert_eq!(ee_units_queued_op(q, 2, 4, 0), 0x10);
        ee_units_queued_op(q, 1, 3, 8);
        assert_eq!((*q).map.keys().copied().collect::<Vec<_>>(), [8]);
        let d = ee_units_queued_digest(q);
        ee_units_queued_op(q, 0, 8, 0);
        assert!((*q).map.is_empty());
        assert_ne!(d, ee_units_queued_digest(q));
        ee_units_queued_free(q);
    }
}
