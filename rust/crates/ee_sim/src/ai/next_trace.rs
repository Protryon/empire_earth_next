//! The computer players' trace lines beyond the decisions (EECPTrace.cpp before; lane A, round 3; docs/AI_RESEARCH.md
//! §5): the stats lines every 30 game s, the losses' counters and the unit.death line, the income and the TEST ONLY
//! spending, the real-time profile, the sim hash line every 10 game s, the computer players' share of the
//! out-of-synch checksum and its TEST ONLY dump. A child module of `next_game` (its host and its reads); the
//! continent manager's areas are read through one host op (`EE_AI_NEXT_HOST_AREAS`) Lane C can re-point.

use super::*;

const MAX_TRACED: usize = 64;
const RESOURCE_ANY: c_long = 1;
const RESOURCE_IRON: usize = 6;
const GOAL_ATTACK_GROUND: u8 = 3;
const GOAL_PATROL: u8 = 9;
const GOAL_EXPLORE: u8 = 10;
const GOAL_LOAD_GARRISON: u8 = 12;
const ACTION_BUILD_BUILDING: u32 = 4000;
const ACTION_REPAIR_BUILDING: u32 = 4002;
const ACTION_GATHER_RESOURCE: u32 = 4004;
const ACTION_DROPOFF_RESOURCE: u32 = 4011;
const ACTION_MOVE_TO_WAITING_AREA: u32 = 4023;
const ACTION_IDLE: u32 = 5000;
const ACTION_MOVE_TO_RESOURCE: u32 = 8008;
const ACTION_MOVE_TO_STORAGE: u32 = 8009;
const ACTION_MOVE_TO_REPAIR_SITE: u32 = 8017;
const CAT_PROJECTILE: u64 = 9;

/// The per-player counters (the C++'s statics)
#[derive(Clone, Copy)]
struct Counters {
    next_stats: c_ulong,
    next_profile: c_ulong,
    profile_sum: i64,
    profile_max: i64,
    profile_count: c_ulong,
    /// since the last stats line: citizens, buildings, other units
    lost: [c_long; 3],
    /// citizens lost by distance from the town centre: <15, 15-30, more tiles
    lost_near: [c_long; 3],
    /// citizens lost by their last attacker: a player's unit, a wild one (player 0), none
    lost_by: [c_long; 3],
    income: [c_long; RESOURCE_IRON + 1],
    spend: [[c_long; 5]; 8],
}

const ZERO: Counters = Counters {
    next_stats: 0,
    next_profile: 0,
    profile_sum: 0,
    profile_max: 0,
    profile_count: 0,
    lost: [0; 3],
    lost_near: [0; 3],
    lost_by: [0; 3],
    income: [0; RESOURCE_IRON + 1],
    spend: [[0; 5]; 8],
};

static COUNTERS: Mutex<[Counters; MAX_TRACED]> = Mutex::new([ZERO; MAX_TRACED]);
static NEXT_HASH: Mutex<c_ulong> = Mutex::new(0);

/// `EECPNewGame` (EEWorld::StartGame, before its players are made): the computer players' per-game statics cleared as
/// at the process's start, so a game plays and traces the same after another game in the process (EE-SIM --games) as
/// alone: the trace's counters and next sim hash time, the loss incidents (`EECPNextArmyNewGame`) and the reflexes'
/// cool-downs (`EECPReflexesNewGame`)
#[no_mangle]
pub extern "C" fn ee_ai_new_game() {
    *COUNTERS.lock().unwrap_or_else(|e| e.into_inner()) = [ZERO; MAX_TRACED];
    *NEXT_HASH.lock().unwrap_or_else(|e| e.into_inner()) = 0;
    new_game_losses();
    super::reflex::new_game();
}

fn counters<R>(id: c_ulong, f: impl FnOnce(&mut Counters) -> R) -> R {
    let mut all = COUNTERS.lock().unwrap_or_else(|e| e.into_inner());
    f(&mut all[id as usize])
}

/// `EECPIsBuildingCategory`
fn building_category(c: u64) -> bool {
    matches!(c, 0x02 | 0x15 | 0x18 | 0x19 | 0x1d | 0x1e | 0x20)
}

/// `EECPTraceIncome`: a resource amount the player got (EEComputerPlayer::AddResource), for the next income line.
#[no_mangle]
pub extern "C" fn ee_ai_trace_income(player: c_ulong, resource: c_long, amount: c_long) {
    if (player as usize) < MAX_TRACED && (0..=RESOURCE_IRON as c_long).contains(&resource) && amount > 0 {
        counters(player, |c| c.income[resource as usize] = c.income[resource as usize].wrapping_add(amount));
    }
}

/// `EECPTraceSpend`'s count (TEST ONLY, EE_AI_EXP spending; the caller tests the trace and the experiment).
///
/// # Safety
/// `cost` holds five values.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_trace_spend(player: c_ulong, kind: c_long, cost: *const c_long) {
    if (player as usize) < MAX_TRACED && (0..8).contains(&kind) {
        let cost = std::slice::from_raw_parts(cost, 5);
        counters(player, |c| {
            for (s, &v) in c.spend[kind as usize].iter_mut().zip(cost) {
                *s = s.wrapping_add(v);
            }
        });
    }
}

fn unit_goal_name(c: &UnitView<'_>) -> &'static str {
    match c.active_goal() {
        None => "none",
        Some((k, _)) => match k {
            GOAL_LOCATION => "move",
            GOAL_ATTACK_UNIT => "attack",
            GOAL_ATTACK_GROUND => "attack_ground",
            GOAL_PATROL => "patrol",
            GOAL_EXPLORE => "explore",
            GOAL_GATHER => "gather",
            GOAL_LOAD_GARRISON => "garrison",
            _ => "other",
        },
    }
}

/// `(long)sqrt((double)(dx * dx + dy * dy))` (the squares in `long`)
fn tiles(ax: c_long, ay: c_long, bx: c_long, by: c_long) -> c_long {
    let (dx, dy) = (ax.wrapping_sub(bx), ay.wrapping_sub(by));
    (dx.wrapping_mul(dx).wrapping_add(dy.wrapping_mul(dy)) as f64).sqrt() as c_long
}

fn rts_xy(u: &UnitView<'_>) -> (c_long, c_long) {
    xy(u)
}

/// `EECPTraceSoldierLost`: one "unit.death" line (under "cp")
fn soldier_lost(g: &G<'_>, u: &UnitView<'_>, c: &UnitView<'_>) {
    if !trace_on() {
        return;
    }
    let me = g.me_ptr;
    let (x, y) = rts_xy(u);
    let mut from_town = -1;
    if let Some((tx, ty)) = town(me).filter(|t| t.0 != NO_TILE) {
        if x != NO_TILE {
            from_town = tiles(x, y, tx, ty);
        }
    }
    let goal = unit_goal_name(c);
    let killer = g.unit(host(EE_AI_NEXT_HOST_LAST_ATTACKER, me, c.handle() as i64, 0, 0) as u64);
    let (mut kp, mut kc, mut kd, mut kind) = (-1 as c_long, -1 as c_long, -1 as c_long, '-');
    let (mut kgoal, mut ktarget) = ("-", '-');
    if let Some((k, kclass)) = killer.as_ref().and_then(|k| Some((k, k.class()?))) {
        kp = k.player().map_or(-1, |p| p.id() as c_long);
        kc = kclass.unit_type() as c_long;
        let (kx, ky) = xy(k);
        if kx != NO_TILE && x != NO_TILE {
            kd = tiles(kx, ky, x, y);
        }
        kind = if kp == 0 {
            'a'
        } else if is_building(&kclass) {
            if is_tower(&kclass) {
                't'
            } else {
                'b'
            }
        } else if kclass.category() == CAT_LAND_CITIZEN {
            'c'
        } else if domains(&kclass) & MOVEMENT_SEA != 0 {
            'n'
        } else {
            's'
        };
    }
    if let Some((k, (gk, _))) = killer.as_ref().and_then(|k| Some((k, k.active_goal()?))) {
        kgoal = match gk {
            GOAL_ATTACK_UNIT => "attack",
            GOAL_LOCATION => "move",
            _ => "other",
        };
        if gk == GOAL_ATTACK_UNIT {
            if let Some(tc) = g.goal_target(k, GOAL_ATTACK_UNIT, 1).and_then(|t| t.class()) {
                ktarget = if is_building(&tc) {
                    'b'
                } else if tc.category() == CAT_LAND_CITIZEN {
                    'c'
                } else if domains(&tc) & MOVEMENT_SEA != 0 {
                    'n'
                } else {
                    's'
                };
            }
        }
    }
    let name = u.class().map_or(Vec::new(), |k| class_name(k.unit_type() as c_long));
    let kname = if kc >= 0 { String::from_utf8_lossy(&class_name(kc)).into_owned() } else { "-".to_string() };
    trace(
        me,
        Some(c"cp"),
        "unit.death",
        &format!(
            "unit={} x={x} y={y} d={from_town} goal={goal} killer={kp}:{kname}:{kind} kd={kd} kgoal={kgoal}:{ktarget} \
             sub=-1:-1:-1:-1:-1 grp=-1:-1",
            String::from_utf8_lossy(&name)
        ),
    );
}

/// `EECPTraceUnitLost`: a unit leaving the player (not swapped), by kind, for the next stats line.
///
/// # Safety
/// `me` is a live computer player, `unit` a live unit (or 0).
#[no_mangle]
pub unsafe extern "C" fn ee_ai_trace_unit_lost(me: *mut c_void, unit: u64) {
    let t = WorldRead::begin();
    let Some(g) = G::new(&t, me) else { return };
    let id = g.id();
    let Some(u) = g.unit(unit) else { return };
    let (Some(k), Some(c)) = (u.class(), u.complex_unit()) else { return };
    if id as usize >= MAX_TRACED {
        return;
    }
    let category = k.category();
    if category == CAT_LAND_CITIZEN || category == CAT_NAVAL_CITIZEN {
        let killer = g.unit(host(EE_AI_NEXT_HOST_LAST_ATTACKER, me, c.handle() as i64, 0, 0) as u64);
        let by = match killer {
            None => 2,
            Some(k) if k.player().is_some_and(|p| p.id() != 0) => 0,
            Some(_) => 1,
        };
        let near = town(me).filter(|t| t.0 != NO_TILE).map(|(tx, ty)| {
            let (x, y) = xy(&u);
            let (dx, dy) = (x.wrapping_sub(tx), y.wrapping_sub(ty));
            let sq = dx.wrapping_mul(dx).wrapping_add(dy.wrapping_mul(dy));
            if sq < 225 {
                0
            } else if sq < 900 {
                1
            } else {
                2
            }
        });
        counters(id, |n| {
            n.lost[0] += 1;
            n.lost_by[by] += 1;
            if let Some(k) = near {
                n.lost_near[k] += 1;
            }
        });
        if !has_state(me) {
            soldier_lost(&g, &u, &c); // (a script's seat has its next.death line)
        }
    } else if building_category(category) {
        counters(id, |n| n.lost[1] += 1);
    } else if category != CAT_PROJECTILE {
        counters(id, |n| n.lost[2] += 1);
        soldier_lost(&g, &u, &c);
    }
}

fn act_slot(act: u32) -> usize {
    match act {
        ACTION_GATHER_RESOURCE => 0,
        ACTION_DROPOFF_RESOURCE => 1,
        ACTION_MOVE_TO_RESOURCE => 2,
        ACTION_MOVE_TO_STORAGE => 3,
        ACTION_MOVE_TO_WAITING_AREA => 4,
        ACTION_IDLE | 0 => 5,
        _ => 6,
    }
}

fn food_slot(k: &ClassView<'_>) -> usize {
    let t = k.unit_type();
    let f = k.family();
    if t == TYPE_FORAGE_BUSH {
        0
    } else if f == FAMILY_ANIMAL {
        1
    } else if f == FAMILY_FISH || t == TYPE_FISH {
        2
    } else if t == TYPE_FARM || t == TYPE_ROBOTIC_FARM {
        3
    } else {
        4
    }
}

fn five(v: &[c_long]) -> String {
    format!("{}/{}/{}/{}/{}", v[0], v[1], v[2], v[3], v[4])
}

/// `EECPTraceStats`: the stats lines per player every 30 game s (trace on).
///
/// # Safety
/// `me` is a live computer player.
pub(super) unsafe fn stats(me: *mut c_void, time: c_ulong) {
    if !trace_on() {
        return;
    }
    let t = WorldRead::begin();
    let Some(g) = G::new(&t, me) else { return };
    let id = g.id();
    if id as usize >= MAX_TRACED {
        return;
    }
    let due = counters(id, |c| {
        if c.next_stats > time.wrapping_add(30000) {
            c.next_stats = 0; // a new game in the same process
        }
        if time < c.next_stats {
            return false;
        }
        c.next_stats = time.wrapping_add(30000);
        true
    });
    if !due {
        return;
    }
    let (mut citizens, mut buildings, mut other, mut unfinished, mut idle) = (0 as c_long, 0, 0, 0, 0);
    let mut gatherers = [0 as c_long; RESOURCE_IRON + 1];
    let mut food = [0 as c_long; 5];
    let mut goals = [0 as c_long; 16];
    let mut builders = [0 as c_long; 6];
    let (mut build_class, mut other_action, mut other_health) = (-1 as c_long, -1 as c_long, -1 as c_long);
    let mut gathered: Vec<UnitView<'_>> = Vec::new();
    let mut other_acts: BTreeMap<c_long, c_long> = BTreeMap::new();
    let mut acts = [0 as c_long; 7];
    let mut stores: Vec<UnitView<'_>> = Vec::new();
    let mut drop_sites = [0 as c_long; 3];
    for u in g.me.units() {
        let Some(c) = u.complex_unit() else { continue };
        if hp(&u) <= 0 {
            continue;
        }
        let Some(k) = u.class() else { continue };
        let category = k.category();
        if category == CAT_LAND_CITIZEN || category == CAT_NAVAL_CITIZEN {
            citizens += 1;
            let goal = c.active_goal();
            goals[match goal {
                None => 0,
                Some((gk, _)) if (gk as usize) < 16 => gk as usize,
                Some(_) => 15,
            }] += 1;
            if goal.is_some_and(|(gk, _)| gk == GOAL_BUILD_REPAIR) {
                let target = g.goal_target(&c, GOAL_BUILD_REPAIR, 2);
                let built = target.as_ref().and_then(|t| t.complex_unit()).is_some_and(|tc| tc.is_built());
                builders[built as usize] += 1;
                if !built && build_class < 0 {
                    if let Some(tk) = target.as_ref().and_then(|t| t.class()) {
                        build_class = tk.unit_type() as c_long;
                    }
                }
                let action = c.command_type();
                builders[match action {
                    ACTION_BUILD_BUILDING => 2,
                    ACTION_REPAIR_BUILDING => 3,
                    ACTION_MOVE_TO_REPAIR_SITE => 4,
                    _ => 5,
                }] += 1;
                if !matches!(action, ACTION_BUILD_BUILDING | ACTION_REPAIR_BUILDING | ACTION_MOVE_TO_REPAIR_SITE)
                    && other_action < 0
                {
                    if let Some(t) = &target {
                        other_action = action as c_long;
                        other_health = um::lrintf(hp(t) as f32) as c_long;
                    }
                }
            }
            match goal {
                None => idle += 1,
                Some((GOAL_GATHER, _)) => {
                    let resource = g.goal_target(&c, GOAL_GATHER, 0);
                    let rtype = resource.as_ref().map_or(0, |r| r.resource_type() as usize);
                    let in_range = (RESOURCE_FOOD as usize..=RESOURCE_IRON).contains(&rtype);
                    gatherers[if in_range { rtype } else { 0 }] += 1;
                    if let Some(r) = resource.filter(|_| in_range) {
                        gathered.push(r);
                    }
                    let act = c.command_type();
                    acts[act_slot(act)] += 1;
                    if act_slot(act) == 6 {
                        *other_acts.entry(act as c_long).or_default() += 1;
                    }
                    if rtype == RESOURCE_FOOD as usize {
                        if let Some(rk) = resource.and_then(|r| r.class()) {
                            food[food_slot(&rk)] += 1;
                        }
                    }
                }
                Some((GOAL_ATTACK_UNIT, _)) => food[1] += 1, // a citizen attacking: hunting
                Some(_) => {}
            }
        } else if building_category(category) {
            buildings += 1;
            let ut = k.unit_type();
            if c.is_built() && is_drop_site(&k) {
                stores.push(c);
            }
            if c.is_built() {
                if ut == TYPE_SETTLEMENT {
                    drop_sites[0] += 1;
                } else if ut == TYPE_TOWN_CENTER || ut == TYPE_CAPITOL {
                    drop_sites[1] += 1;
                } else if ut == TYPE_GRANARY {
                    drop_sites[2] += 1;
                }
            }
            if !c.is_built() {
                unfinished += 1;
            }
        } else {
            other += 1;
        }
    }
    let mut free = 0usize; // the citizens idle or gathering
    for u in g.me.units() {
        let Some(c) = u.complex_unit() else { continue };
        if u.class().map(|k| k.category()) == Some(CAT_LAND_CITIZEN)
            && hp(&u) > 0
            && xy(&c).0 != NO_TILE
            && c.active_goal().is_none_or(|(gk, _)| gk == GOAL_GATHER)
        {
            free += 1;
        }
    }
    let epoch = g.tree().map_or(-1, |t| t.current_epoch() as c_long);
    let saved = scope(c"cp");
    let (lost, lost_near, lost_by) = counters(id, |n| {
        let v = (n.lost, n.lost_near, n.lost_by);
        n.lost = [0; 3];
        n.lost_near = [0; 3];
        n.lost_by = [0; 3];
        v
    });
    trace_line(
        me,
        None,
        "losses",
        format!(
            "citizens={} buildings={} other={} citizens_by_distance={}/{}/{} citizens_by_killer={}/{}/{}",
            lost[0], lost[1], lost[2], lost_near[0], lost_near[1], lost_near[2], lost_by[0], lost_by[1], lost_by[2]
        )
        .as_bytes(),
    );
    trace_line(
        me,
        None,
        "stats",
        format!(
            "epoch={epoch} pop={} cap={} limit={} citizens={citizens} idle={idle} free={free} gather={}/{} \
             buildings={buildings} unfinished={unfinished} other={other}",
            host(EE_AI_NEXT_HOST_POPULATION, me, 2, 0, 0),
            host(EE_AI_NEXT_HOST_POPULATION, me, 4, 0, 0),
            host(EE_AI_NEXT_HOST_POPULATION, me, 3, 0, 0),
            five(&gatherers[2..7]),
            gatherers[0]
        )
        .as_bytes(),
    );
    // the resource units the player's resource centres hold, by resource
    let mut sites = [0 as c_ulong; RESOURCE_IRON + 1];
    let centres = rc_count(me);
    for k in 0..centres {
        for u in rc(me, k).2.iter().filter_map(|&h| g.unit(h)) {
            let r = u.resource_type() as usize;
            if (RESOURCE_FOOD as usize..=RESOURCE_IRON).contains(&r) && hp(&u) > 0 {
                sites[r] += 1;
            }
        }
    }
    let mining: c_long = crate::world_field!(g.me.fields(), mMiningLimit);
    trace_line(
        me,
        None,
        "sites",
        format!(
            "units={}/{}/{}/{}/{} limit={mining} centers={centres}",
            sites[2], sites[3], sites[4], sites[5], sites[6]
        )
        .as_bytes(),
    );
    // the mean distance from each gatherer's resource to the nearest built drop site that takes it, by resource
    let mut trip = [0.0f64; RESOURCE_IRON + 1];
    let mut trips = [0 as c_long; RESOURCE_IRON + 1];
    let mut far = [0 as c_long; RESOURCE_IRON + 1];
    let mut no_drop = [0 as c_long; RESOURCE_IRON + 1];
    let dropoff = host(EE_AI_NEXT_HOST_POPULATION, me, 5, 0, 0) as c_long;
    for r in &gathered {
        let rtype = r.resource_type() as usize;
        let mut best = -1.0f32;
        for s in &stores {
            let takes = s.class().map_or(0, |k| class_resource(&k));
            if takes != RESOURCE_ANY && takes != rtype as c_long {
                continue;
            }
            let d = f32::from_bits(host(
                EE_AI_NEXT_HOST_DISTANCE2,
                std::ptr::null_mut(),
                s.handle() as i64,
                r.handle() as i64,
                0,
            ) as u32);
            if best < 0.0 || d < best {
                best = d;
            }
        }
        if best < 0.0 {
            continue;
        }
        let k = r.class();
        let t = k.as_ref().map_or(u64::MAX, |k| k.unit_type());
        let f = k.as_ref().map_or(u64::MAX, |k| k.family());
        if f != FAMILY_ANIMAL && f != FAMILY_FISH && t != TYPE_FISH && t != TYPE_FARM && t != TYPE_ROBOTIC_FARM {
            let ready = if rtype == RESOURCE_WOOD as usize {
                let (x, y) = xy(r);
                drop_in(&ready_drops(&g, false), x, y, WOOD_SLACK)
            } else {
                unit_drop_ready(&g, r)
            };
            if !ready {
                no_drop[rtype] += 1;
            }
        }
        let length = (best as f64).sqrt();
        trip[rtype] += length;
        trips[rtype] += 1;
        if length > dropoff as f64 {
            far[rtype] += 1;
        }
    }
    let mut mean = [0 as c_long; RESOURCE_IRON + 1];
    for r in 0..=RESOURCE_IRON {
        mean[r] = if trips[r] > 0 { (trip[r] / trips[r] as f64 + 0.5) as c_long } else { -1 };
    }
    trace_line(
        me,
        None,
        "trips",
        format!(
            "mean={} n={} far={} dropoff={dropoff} settlements={} towncenters={} granaries={} nodrop={}",
            five(&mean[2..7]),
            five(&trips[2..7]),
            five(&far[2..7]),
            drop_sites[0],
            drop_sites[1],
            drop_sites[2],
            five(&no_drop[2..7])
        )
        .as_bytes(),
    );
    let (mut top_other, mut top_count) = (-1 as c_long, 0 as c_long);
    for (&a, &n) in &other_acts {
        if n > top_count {
            top_count = n;
            top_other = a;
        }
    }
    trace_line(
        me,
        None,
        "gatheract",
        format!(
            "gather={} dropoff={} to_resource={} to_storage={} waiting={} idle={} other={} top_other={top_other}",
            acts[0], acts[1], acts[2], acts[3], acts[4], acts[5], acts[6]
        )
        .as_bytes(),
    );
    let income = counters(id, |n| n.income);
    trace_line(me, None, "income", format!("got={} gatherers={}", five(&income[2..7]), five(&trips[2..7])).as_bytes());
    if experiment(me, c"spending") {
        // TEST ONLY: what was bought since the last line
        const KINDS: [&str; 8] = ["citizens", "soldiers", "buildings", "techs", "epochs", "boats", "improve", "other"];
        let spend = counters(id, |n| std::mem::take(&mut n.spend));
        let mut line = String::new();
        for k in 0..8 {
            if line.len() >= 900 {
                break;
            }
            line.push_str(&format!("{}{}={}", if k > 0 { " " } else { "" }, KINDS[k], five(&spend[k])));
        }
        truncate(&mut line, 1023);
        trace_line(me, None, "spend", line.as_bytes());
    }
    counters(id, |n| n.income = [0; RESOURCE_IRON + 1]);
    trace_line(
        me,
        None,
        "food",
        format!(
            "bush={} hunt={} fish={} farm={} other={} goals gather={} build={} move={} attack={} garrison={} none={}",
            food[0], food[1], food[2], food[3], food[4], goals[6], goals[4], goals[1], goals[2], goals[12], goals[0]
        )
        .as_bytes(),
    );
    trace_line(
        me,
        None,
        "builders",
        format!(
            "unbuilt={} built={} act build={} repair={} move={} other={} cls={build_class} \
             other_action={other_action} other_hp={other_health}",
            builders[0], builders[1], builders[2], builders[3], builders[4], builders[5]
        )
        .as_bytes(),
    );
    scope_end(saved);
}

/// `EECPTraceStats` for the C++.
///
/// # Safety
/// `me` is a live computer player.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_trace_stats(me: *mut c_void, time: c_ulong) {
    stats(me, time);
}

/// `EECPTraceProfile`: the AI's real time per player: every 60 game s the sum and the largest single update.
#[no_mangle]
pub extern "C" fn ee_ai_trace_profile(player: c_ulong, time: c_ulong, microseconds: i64) {
    if !trace_on() || player as usize >= MAX_TRACED {
        return;
    }
    let line = counters(player, |c| {
        c.profile_sum = c.profile_sum.wrapping_add(microseconds);
        c.profile_count = c.profile_count.wrapping_add(1);
        if microseconds > c.profile_max {
            c.profile_max = microseconds;
        }
        if c.next_profile > time.wrapping_add(60000) {
            c.next_profile = 0; // a new game in the same process
        }
        if time < c.next_profile {
            return None;
        }
        c.next_profile = time.wrapping_add(60000);
        let line = format!(
            "EE-AIPROF {time} p{player} updates={} sum_us={} max_us={}\n",
            c.profile_count, c.profile_sum, c.profile_max
        );
        c.profile_sum = 0;
        c.profile_max = 0;
        c.profile_count = 0;
        Some(line)
    });
    if let Some(line) = line {
        let c = cstr(line.as_bytes());
        host(EE_AI_NEXT_HOST_DEBUG_OUT, std::ptr::null_mut(), c.as_ptr() as i64, 0, 0);
    }
}

/// The computer player of a player (`mComputerPlayer`; 0: none)
fn computer_of(p: &PlayerView<'_>) -> *mut c_void {
    host(EE_AI_NEXT_HOST_COMPUTER, std::ptr::null_mut(), p.handle() as i64, 0, 0) as usize as *mut c_void
}

/// `EECPOutOfSynchChecksum`: the computer players' share of the out-of-synch checksum (every player's resources;
/// per computer player its pop cap, its town tile and its danger zones), in `unsigned long`.
#[no_mangle]
pub extern "C" fn ee_ai_out_of_synch_checksum() -> c_ulong {
    // SAFETY: the simulation thread (the physics' checksum, the sim's lines)
    let t = unsafe { WorldRead::begin() };
    let Some(world) = WorldView::global(&t) else { return 0 };
    let mut checksum: c_ulong = 0;
    for i in 0..world.num_players() {
        let Some(p) = world.player(i as usize) else { continue };
        let caps: [c_long; 7] = crate::world_field!(p.saved(), mResourceCaps);
        let mut sum: c_ulong = 0;
        for &v in &caps[2..7] {
            sum = sum.wrapping_mul(31).wrapping_add(v as c_ulong);
        }
        let cp = computer_of(&p);
        if !cp.is_null() {
            sum = sum.wrapping_mul(31).wrapping_add(host(EE_AI_NEXT_HOST_POPULATION, cp, 4, 0, 0) as c_long as c_ulong);
            if let Some((x, y)) = town(cp) {
                sum = sum.wrapping_mul(31).wrapping_add(x.wrapping_mul(1024).wrapping_add(y) as c_ulong);
            }
            sum = sum.wrapping_mul(31).wrapping_add(host(EE_AI_NEXT_HOST_DANGER_COUNT, cp, 0, 0, 0) as c_ulong);
        }
        checksum = checksum.wrapping_add(sum.wrapping_mul((p.id() as c_ulong).wrapping_add(1)));
    }
    checksum
}

/// `EECPOutOfSynchDump` (TEST ONLY, EE-SIM's state dumps): the parts of the checksum per player and the computer
/// players' own state, one line each, appended to `out` (a C++ `std::string`).
///
/// # Safety
/// `out` is a live `std::string`.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_out_of_synch_dump(out: *mut c_void) {
    let mut text = Vec::new();
    // the continent manager's areas (serial/kind), its serial counter and its lists' sizes
    host_out(EE_AI_NEXT_HOST_AREAS, std::ptr::null_mut(), 0, 0, 0, &mut text);
    let t = WorldRead::begin();
    if let Some(world) = WorldView::global(&t) {
        for i in 0..world.num_players() {
            let Some(p) = world.player(i as usize) else { continue };
            let caps: [c_long; 7] = crate::world_field!(p.saved(), mResourceCaps);
            let mut line = format!("AI {} res={},{},{},{},{}", p.id(), caps[2], caps[3], caps[4], caps[5], caps[6]);
            let cp = computer_of(&p);
            if !cp.is_null() {
                line.push_str(&format!(" popcap={}", host(EE_AI_NEXT_HOST_POPULATION, cp, 4, 0, 0)));
                if let Some((x, y)) = town(cp) {
                    line.push_str(&format!(" town={x},{y}"));
                }
                line.push_str(&format!(" danger={} rc=", host(EE_AI_NEXT_HOST_DANGER_COUNT, cp, 0, 0, 0)));
                for k in 0..rc_count(cp) {
                    let (unit, active, units) = rc(cp, k);
                    // SAFETY: a centre's unit (0: none)
                    let id = UnitView::of(&t, unit).map_or(0, |u| u.id());
                    line.push_str(&format!("{}{id}:{}:{}", if k > 0 { "," } else { "" }, active as i32, units.len()));
                }
                line.push_str(&format!(" started={}", host(EE_AI_NEXT_HOST_STARTED, cp, 0, 0, 0)));
            }
            line.push('\n');
            text.extend_from_slice(line.as_bytes());
        }
    }
    ee_ai_next_string_append(out, text.as_ptr(), text.len());
}

/// `EECPTraceHash` (with the trace): every 10 game s, the parts of the out-of-synch checksum, as a compared trace
/// line ("EE-AI <ms> p0 sim hash ...").
#[no_mangle]
pub extern "C" fn ee_ai_trace_hash(time: c_ulong) {
    if !trace_on() {
        return;
    }
    {
        let mut next = NEXT_HASH.lock().unwrap_or_else(|e| e.into_inner());
        if *next > time.wrapping_add(10000) {
            *next = 0; // a new game in the same process
        }
        if time < *next {
            return;
        }
        *next = time.wrapping_add(10000);
    }
    // the units' checksum (GetChecksum(3): hit points, tiles, positions, commands), the tick, the shared seed, the
    // inputs
    let mut v = [0i64; 5];
    host_out(EE_AI_NEXT_HOST_SIM_HASH, std::ptr::null_mut(), 0, 0, 0, &mut v);
    let line = format!(
        "EE-AI {time} p0 sim hash pt={} units={}/{:08x} rng={:08x} inputs={} ai={:08x} | f=0 w=0 s=0 g=0 i=0\n",
        v[0] as c_ulong,
        v[1] as c_ulong,
        v[2] as c_ulong,
        v[3] as c_ulong,
        v[4] as c_long,
        ee_ai_out_of_synch_checksum()
    );
    let c = cstr(line.as_bytes());
    host(EE_AI_NEXT_HOST_DEBUG_OUT, std::ptr::null_mut(), c.as_ptr() as i64, 0, 0);
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn new_game_clears_the_per_game_statics() {
        counters(5, |c| {
            c.lost[0] = 3;
            c.income[2] = 7;
            c.next_stats = 30000;
            c.profile_sum = 99;
        });
        *NEXT_HASH.lock().unwrap() = 20000;
        with_losses(5, 1000, |l| {
            l.next_id = 4;
            l.incidents.push(Incident::default());
            l.hits.insert(1, LossHit::default());
        });
        ee_ai_new_game();
        counters(5, |c| {
            assert_eq!((c.lost, c.income[2], c.next_stats, c.profile_sum), ([0; 3], 0, 0, 0));
        });
        assert_eq!(*NEXT_HASH.lock().unwrap(), 0);
        // a later time than before: the "new game" heuristic does not fire, the reset alone cleared them
        with_losses(5, 2000, |l| {
            assert_eq!((l.next_id, l.incidents.len(), l.hits.len()), (0, 0, 0));
        });
    }
}
