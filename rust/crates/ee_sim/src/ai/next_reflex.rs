//! The computer player's reflexes (EECPReflexes.cpp before; lane A, round 3; docs/AI_LUA_VERBS.md §5): the citizen
//! militia, the danger zones and the gatherers' flight from them, the naval flight. They act on the damage events
//! (EEComputerPlayer::DamageTaken); ai.reflex turns them off and on for a seat. A child module of `next_game` (its
//! host and its reads). The danger zones stay the computer player's member (`mDangerZones`), read and written
//! through the host; the orders are the C++'s goal calls (`EE_AI_NEXT_HOST_ORDER`).

use super::*;

/// `kFleeRange`, `kDangerRange`, `kDangerMs`, `kHomeRange`, `kMaxRaiders`, `kDefendRange`, `kDefenders`
const FLEE_RANGE: c_long = 8;
const DANGER_RANGE: c_long = 10;
const DANGER_MS: c_ulong = 30000;
const HOME_RANGE: c_long = 12;
const MAX_RAIDERS: c_long = 2;
const DEFEND_RANGE: c_long = 30;
const DEFENDERS: usize = 4;
/// `kMilitiaRange`, `kMilitiaTowerRange`, `kMilitiaOdds`, `kMilitiaMs`
const MILITIA_RANGE: c_long = 10;
const MILITIA_TOWER_RANGE: c_long = 12;
const MILITIA_ODDS: f32 = 1.5;
const MILITIA_MS: c_ulong = 4000;
/// `kNavalFleeRange`, `kNavalFleeMs`
const NAVAL_FLEE_RANGE: c_long = 12;
const NAVAL_FLEE_MS: c_ulong = 4000;

const CAT_AMBIENT: u64 = 0;
const CAT_RESOURCE: u64 = 1;
const CAT_NAVAL: u64 = 4;
const CAT_PROJECTILE: u64 = 9;

/// The danger zones (`mDangerZones`: tile, game ms)
fn zones(me: *mut c_void) -> Vec<(c_long, c_long, c_ulong)> {
    let n = host(EE_AI_NEXT_HOST_DANGER_COUNT, me, 0, 0, 0).max(0);
    (0..n)
        .map(|z| {
            let mut v = [0i64; 3];
            host_out(EE_AI_NEXT_HOST_DANGER, me, z, 0, 0, &mut v);
            (v[0] as c_long, v[1] as c_long, v[2] as c_ulong)
        })
        .collect()
}

fn zone_add(me: *mut c_void, x: c_long, y: c_long) {
    host(EE_AI_NEXT_HOST_DANGER_ADD, me, x as i64, y as i64, now() as i64);
}

/// `EECPRMInDanger`
fn in_danger_range(me: *mut c_void, x: c_long, y: c_long, range: c_long) -> bool {
    zones(me).iter().any(|&(zx, zy, _)| {
        let (dx, dy) = (zx.wrapping_sub(x), zy.wrapping_sub(y));
        dx.wrapping_mul(dx).wrapping_add(dy.wrapping_mul(dy)) <= range * range
    })
}

/// `EECPRMResourceInDanger`: whether a tile lies in one of the player's danger zones
pub(super) fn in_danger(me: *mut c_void, x: c_long, y: c_long) -> bool {
    in_danger_range(me, x, y, DANGER_RANGE)
}

/// `EECPRMNear` (in `long`)
fn rm_near(u: &UnitView<'_>, x: c_long, y: c_long, range: c_long) -> bool {
    let (ux, uy) = xy(u);
    let (dx, dy) = (ux.wrapping_sub(x), uy.wrapping_sub(y));
    dx.wrapping_mul(dx).wrapping_add(dy.wrapping_mul(dy)) <= range * range
}

fn sq(ax: c_long, ay: c_long, bx: c_long, by: c_long) -> c_long {
    let (dx, dy) = (ax.wrapping_sub(bx), ay.wrapping_sub(by));
    dx.wrapping_mul(dx).wrapping_add(dy.wrapping_mul(dy))
}

/// `EECPRMIsSoldier`: a living land soldier that is not a land building
fn rm_soldier(u: &UnitView<'_>) -> bool {
    let Some(k) = u.class() else { return false };
    if u.complex_unit_handle() == 0 || hp(u) <= 0 || !armed(u) || is_land_building(&k) {
        return false;
    }
    matches!(k.category(), CAT_LAND | CAT_HERO)
}

fn attack_f(u: &UnitView<'_>) -> f32 {
    attack(u).unwrap_or(0.0)
}

fn disabled(c: &UnitView<'_>) -> bool {
    crate::world_field!(c.complex(), mIsDisabled)
}

fn detached(c: &UnitView<'_>) -> bool {
    crate::world_field!(c.complex(), mIsDetachedFromAI)
}

/// The player's per-unit state (`EECPUnitsReflexOn`, `EECPUnitsHolds`)
fn reflex_on(me: *mut c_void, name: &CStr) -> bool {
    let u = host(EE_AI_NEXT_HOST_UNITS_STATE, me, 0, 0, 0) as usize as *mut c_void;
    if u.is_null() {
        return name != c"held";
    }
    // SAFETY: the player's live per-unit state
    unsafe { crate::ai::units_bind::ee_ai_units_reflex_on(u, name.as_ptr()) }
}

fn holds(me: *mut c_void, id: c_long) -> bool {
    let u = host(EE_AI_NEXT_HOST_UNITS_STATE, me, 0, 0, 0) as usize as *mut c_void;
    // SAFETY: as above
    !u.is_null() && unsafe { crate::ai::units_bind::ee_ai_units_holds(u, id) }
}

/// The orders (`ClearGoalQueue(0)`, `SetGoal(...)`, `StartAction(kActionPrepareToMove)`, `ActionChanged`; with
/// `reference`, inside `Reference`/`Dereference`): attack `target`, or go to the tile (x, y)
fn order_attack(c: &UnitView<'_>, target: u64, reference: bool) {
    let mut v = [0i64, target as i64, 0, 0, reference as i64];
    host_out(EE_AI_NEXT_HOST_ORDER, std::ptr::null_mut(), c.complex_unit_handle() as i64, 0, 0, &mut v);
}

fn order_move(c: &UnitView<'_>, x: c_long, y: c_long) {
    let mut v = [1i64, 0, x as i64, y as i64, 1];
    host_out(EE_AI_NEXT_HOST_ORDER, std::ptr::null_mut(), c.complex_unit_handle() as i64, 0, 0, &mut v);
}

/// The per-player timers of the militia and the naval flight (`sLast[64]`)
static MILITIA_LAST: Mutex<[c_ulong; 64]> = Mutex::new([0; 64]);
static NAVAL_LAST: Mutex<[c_ulong; 64]> = Mutex::new([0; 64]);

/// `sLast[id] != 0 && now >= sLast[id] && now - sLast[id] < ms`: too soon; else the time is taken
fn too_soon(last: &Mutex<[c_ulong; 64]>, id: c_ulong, ms: c_ulong) -> bool {
    let k = if id < 64 { id as usize } else { 63 };
    let now = now();
    let mut l = last.lock().unwrap_or_else(|e| e.into_inner());
    if l[k] != 0 && now >= l[k] && now - l[k] < ms {
        return true;
    }
    l[k] = now;
    false
}

fn format_f2(v: f32) -> String {
    let (_, b) = host_bytes(EE_AI_NEXT_HOST_FORMAT_F2, std::ptr::null_mut(), (v as f64).to_bits() as i64);
    String::from_utf8_lossy(&b).into_owned()
}

/// `EECPRMMilitia`: the town's answer to an attack group on its citizens when no army does it
fn militia(g: &G<'_>, citizen: &UnitView<'_>, attacker: &PlayerView<'_>) {
    let me = g.me_ptr;
    if too_soon(&MILITIA_LAST, g.id(), MILITIA_MS) {
        return;
    }
    let (x, y) = xy(citizen);
    let mut raiders = Vec::new();
    let (mut raider_hp, mut raider_attack) = (0.0f32, 0.0f32);
    for u in attacker.units() {
        if rm_soldier(&u) && rm_near(&u, x, y, MILITIA_RANGE) {
            raiders.push(u);
            raider_hp += hp(&u) as f32;
            raider_attack += attack_f(&u);
        }
    }
    if raiders.is_empty() {
        return;
    }
    let (mut citizens, mut soldiers) = (Vec::new(), Vec::new());
    let (mut hp_sum, mut attack_sum) = (0.0f32, 0.0f32);
    for u in g.me.units() {
        let Some(c) = u.complex_unit() else { continue };
        let category = u.class().map_or(u64::MAX, |k| k.category());
        let held_citizen = category == CAT_LAND_CITIZEN && reflex_on(me, c"held") && holds(me, c.id() as c_long);
        if hp(&u) <= 0 || disabled(&c) || (detached(&c) && !held_citizen) || c.container_handle() != 0 {
            continue;
        }
        if category == CAT_LAND_CITIZEN
            && rm_near(&u, x, y, MILITIA_RANGE)
            && host(EE_AI_NEXT_HOST_CITIZEN_QUEUE, me, c.handle() as i64, 0, 0) == 0
        {
            citizens.push(c);
            hp_sum += hp(&u) as f32;
            attack_sum += attack_f(&u);
        } else if rm_soldier(&u)
            && rm_near(&u, x, y, DEFEND_RANGE)
            && c.active_goal().is_none_or(|(kind, _)| kind != GOAL_ATTACK_UNIT)
        {
            soldiers.push(c);
            hp_sum += hp(&u) as f32;
            attack_sum += attack_f(&u);
        }
    }
    // military.militia_towers: the own built towers within kMilitiaTowerRange count in the defence, and a raid the
    // town cannot fight sends the citizens to the nearest tower within kDefendRange instead of the far drop site
    let mut tower: Option<UnitView<'_>> = None;
    if host(EE_AI_NEXT_HOST_SWITCH, me, 1, 0, 0) != 0 {
        let mut best: c_long = -1;
        for u in g.me.units() {
            let Some(c) = u.complex_unit() else { continue };
            if !u.class().is_some_and(|k| is_tower(&k)) || hp(&u) <= 0 || !c.is_built() || !armed(&u) {
                continue;
            }
            if rm_near(&u, x, y, MILITIA_TOWER_RANGE) {
                hp_sum += hp(&u) as f32;
                attack_sum += attack_f(&u);
            }
            let (ux, uy) = xy(&u);
            let distance = sq(ux, uy, x, y);
            if distance <= DEFEND_RANGE * DEFEND_RANGE && (best < 0 || distance < best) {
                best = distance;
                tower = Some(u);
            }
        }
    }
    let fight = hp_sum * attack_sum >= MILITIA_ODDS * raider_hp * raider_attack;
    if fight {
        // the soldiers first, then only as many of the nearest citizens as the odds need
        let mut all = soldiers.clone();
        let (mut need_hp, mut need_attack) = (0.0f32, 0.0f32);
        for s in &soldiers {
            need_hp += hp(s) as f32;
            need_attack += attack_f(s);
        }
        let mut by_distance: Vec<(c_long, UnitView<'_>)> = citizens
            .iter()
            .map(|c| {
                let (cx, cy) = xy(c);
                (sq(cx, cy, x, y), *c)
            })
            .collect();
        by_distance.sort_by_key(|e| e.0); // stable
        citizens.clear();
        for &(_, c) in &by_distance {
            if need_hp * need_attack >= MILITIA_ODDS * raider_hp * raider_attack {
                break;
            }
            citizens.push(c);
            all.push(c);
            need_hp += hp(&c) as f32;
            need_attack += attack_f(&c);
        }
        for u in &all {
            let (ux, uy) = xy(u);
            let mut target: Option<&UnitView<'_>> = None;
            let mut best: c_long = 0;
            for r in &raiders {
                let (rx, ry) = xy(r);
                let distance = sq(rx, ry, ux, uy);
                if target.is_none() || distance < best {
                    target = Some(r);
                    best = distance;
                }
            }
            order_attack(u, target.map_or(0, |t| t.complex_unit_handle()), true);
        }
    } else {
        let town = town_tile(me);
        let mut home: Option<(c_long, c_long)> = None;
        if let Some(t) = tower.as_ref().filter(|t| !rm_near(t, x, y, 3)) {
            home = Some(xy(t));
        } else if let Some((tx, ty)) = town.filter(|&(tx, ty)| !rm_near(citizen, tx, ty, HOME_RANGE)) {
            home = Some((tx, ty));
        } else {
            // the attack is at the town centre: to the drop site farthest from it
            let mut refuge: Option<UnitView<'_>> = None;
            let mut farthest: c_long = -1;
            for k in 0..rc_count(me) {
                let (unit, _, _) = rc(me, k);
                let Some(u) = g.unit(unit) else { continue };
                if !u.class().is_some_and(|c| is_land_building(&c)) || hp(&u) <= 0 {
                    continue;
                }
                let (ux, uy) = xy(&u);
                let distance = sq(ux, uy, x, y);
                if distance > farthest {
                    farthest = distance;
                    refuge = Some(u);
                }
            }
            if let Some(r) = refuge.filter(|_| farthest > MILITIA_RANGE * MILITIA_RANGE) {
                home = Some(xy(&r));
            }
        }
        match home {
            None => citizens.clear(), // nowhere to run: they keep working
            Some((hx, hy)) => {
                for c in &citizens {
                    order_move(c, hx, hy);
                }
            }
        }
        if !citizens.is_empty() {
            zone_add(me, x, y);
        }
    }
    if trace_on() {
        let odds =
            if raider_hp * raider_attack > 0.0 { hp_sum * attack_sum / (raider_hp * raider_attack) } else { 0.0 };
        trace_line(
            me,
            None,
            "militia",
            format!(
                "{} citizens={} soldiers={} raiders={} odds={} tower={}",
                if fight { "fight" } else { "run" },
                citizens.len(),
                soldiers.len(),
                raiders.len(),
                format_f2(odds),
                tower.is_some() as i32
            )
            .as_bytes(),
        );
    }
}

/// `EECPRMNoteDanger`: a citizen hit by a player's unit: the militia or a danger zone and its defenders.
///
/// # Safety
/// `me` is a live computer player, `citizen` a live unit.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_next_note_danger(me: *mut c_void, citizen: u64, attacker: c_ulong) {
    let t = WorldRead::begin();
    let Some(g) = G::new(&t, me) else { return };
    let Some(citizen) = g.unit(citizen) else { return };
    if attacker == 0 || attacker as u64 >= g.players() {
        return;
    }
    let Some(foe) = g.world.player(attacker as usize) else { return };
    if host(EE_AI_NEXT_HOST_SWITCH, me, 0, 0, 0) != 0 && reflex_on(me, c"militia") {
        militia(&g, &citizen, &foe); // (ai.reflex{militia = false}: a script's own militia instead)
        return;
    }
    if host(EE_AI_NEXT_HOST_IS_HUMAN, std::ptr::null_mut(), foe.handle() as i64, 0, 0) == 0 && !experiment(me, c"flee")
    {
        return;
    }
    let (cx, cy) = xy(&citizen);
    let mut raiders: c_long = 0;
    for u in foe.units() {
        let Some(k) = u.class() else { continue };
        if u.complex_unit_handle() == 0 || hp(&u) <= 0 || !armed(&u) || is_land_building(&k) {
            continue;
        }
        if matches!(k.category(), CAT_LAND_CITIZEN | CAT_NAVAL_CITIZEN | CAT_AMBIENT | CAT_RESOURCE | CAT_PROJECTILE) {
            continue;
        }
        let (ux, uy) = xy(&u);
        if sq(ux, uy, cx, cy) <= DANGER_RANGE * DANGER_RANGE {
            raiders += 1;
            if raiders > MAX_RAIDERS {
                return;
            }
        }
    }
    if let Some((tx, ty)) = town_tile(me) {
        if sq(cx, cy, tx, ty) < HOME_RANGE * HOME_RANGE {
            return; // at home: the town's defence answers, the gatherers keep working
        }
    }
    let new = !in_danger_range(me, cx, cy, DANGER_RANGE);
    zone_add(me, cx, cy);
    // a new zone: up to kDefenders of the nearest soldiers within kDefendRange tiles that are not attacking already
    // go for the raider
    let raider = citizen
        .complex_unit()
        .and_then(|c| g.unit(host(EE_AI_NEXT_HOST_LAST_ATTACKER, me, c.handle() as i64, 0, 0) as u64));
    let Some(raider) = raider.filter(|r| new && hp(r) > 0) else { return };
    let (rx, ry) = xy(&raider);
    let mut near: Vec<(c_long, UnitView<'_>)> = Vec::new();
    for u in g.me.units() {
        let Some(c) = u.complex_unit() else { continue };
        let Some(k) = u.class() else { continue };
        if hp(&u) <= 0 || !armed(&u) || is_land_building(&k) || disabled(&c) || detached(&c) {
            continue;
        }
        if !matches!(k.category(), CAT_LAND | CAT_HERO) {
            continue;
        }
        if c.active_goal().is_some_and(|(kind, _)| kind == GOAL_ATTACK_UNIT) {
            continue;
        }
        let (ux, uy) = xy(&u);
        let distance = sq(ux, uy, rx, ry);
        if distance <= DEFEND_RANGE * DEFEND_RANGE {
            near.push((distance, c));
        }
    }
    near.sort_by_key(|e| e.0); // stable
    let sent = near.len().min(DEFENDERS);
    for (_, c) in &near[..sent] {
        order_attack(c, raider.complex_unit_handle(), false);
    }
    trace(me, None, "raid.defend", &format!("sent={sent} near={} raider={}", near.len(), raider.id()));
}

/// `EECPRMNavalFlee`: a boat or land unit hit by a warship: the units round it leave (to the farthest dock, or the
/// town centre).
///
/// # Safety
/// `me` is a live computer player, `victim` a live unit, `attacker` 0 or a live complex unit.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_next_naval_flee(me: *mut c_void, victim: u64, attacker: u64) {
    let t = WorldRead::begin();
    let Some(g) = G::new(&t, me) else { return };
    let (Some(victim), Some(attacker)) = (g.unit(victim), g.unit(attacker)) else { return };
    if hp(&attacker) <= 0 || attacker.class().map(|k| k.category()) != Some(CAT_NAVAL) {
        return; // warships only (not boats, not land units on the shore)
    }
    if too_soon(&NAVAL_LAST, g.id(), NAVAL_FLEE_MS) {
        return;
    }
    let (x, y) = xy(&victim);
    let boat = victim.class().map(|k| k.category()) == Some(CAT_NAVAL_CITIZEN);
    let refuge = if boat {
        // the own built dock farthest from the attacker (the nearest one is often where it shoots)
        let mut docks: Vec<u64> = Vec::new();
        host_out(EE_AI_NEXT_HOST_UNITS_OF_TYPE, std::ptr::null_mut(), TYPE_DOCK as i64, g.id() as i64, 0, &mut docks);
        let (ax, ay) = xy(&attacker);
        let mut best: c_long = -1;
        let mut at = (0, 0);
        for d in docks.iter().filter_map(|&h| g.unit(h)) {
            if !d.complex_unit().is_some_and(|c| c.is_built()) || hp(&d) <= 0 {
                continue;
            }
            let (dx, dy) = xy(&d);
            let distance = sq(dx, dy, ax, ay);
            if distance > best {
                best = distance;
                at = (dx, dy);
            }
        }
        if best < 0 {
            return;
        }
        at
    } else {
        let Some(t) = town_tile(me) else { return };
        t
    };
    let mut count = 0usize;
    for u in g.me.units() {
        let Some(c) = u.complex_unit() else { continue };
        if hp(&u) <= 0
            || disabled(&c)
            || detached(&c)
            || c.container_handle() != 0
            || !rm_near(&u, x, y, NAVAL_FLEE_RANGE)
        {
            continue;
        }
        let category = u.class().map_or(u64::MAX, |k| k.category());
        let leaves = if boat {
            category == CAT_NAVAL_CITIZEN
        } else {
            // the land units that cannot answer a ship: citizens and melee soldiers
            category == CAT_LAND_CITIZEN
                || (matches!(category, CAT_LAND | CAT_HERO) && u.stats().is_some_and(|s| s.range() < 3.0))
        };
        if !leaves {
            continue;
        }
        order_move(&c, refuge.0, refuge.1);
        count += 1;
    }
    if count > 0 {
        zone_add(me, x, y);
    }
    trace(me, None, "navalflee", &format!("{} units={count}", if boat { "boats" } else { "land" }));
}

/// `EECPRMFleeRaids`: every tick: the zones expire; the gatherers in one go home.
///
/// # Safety
/// `me` is a live computer player.
pub(super) unsafe fn flee_raids(me: *mut c_void) {
    let t = WorldRead::begin();
    let Some(g) = G::new(&t, me) else { return };
    let now = now();
    let mut z = 0;
    let mut n = host(EE_AI_NEXT_HOST_DANGER_COUNT, me, 0, 0, 0).max(0);
    while z < n {
        let mut v = [0i64; 3];
        host_out(EE_AI_NEXT_HOST_DANGER, me, z, 0, 0, &mut v);
        if now.wrapping_sub(v[2] as c_ulong) > DANGER_MS {
            host(EE_AI_NEXT_HOST_DANGER_ERASE, me, z, 0, 0);
            n -= 1;
        } else {
            z += 1;
        }
    }
    let Some((hx, hy)) = town_tile(me) else { return };
    if n == 0 {
        return;
    }
    let held = reflex_on(me, c"held");
    let mut run = Vec::new();
    for u in g.me.units() {
        let Some(c) = u.complex_unit() else { continue };
        if c.class().map(|k| k.category()) != Some(CAT_LAND_CITIZEN) || disabled(&c) || hp(&u) <= 0 {
            continue;
        }
        if c.active_goal().is_none_or(|(kind, _)| kind != GOAL_GATHER) {
            continue;
        }
        if detached(&c) && !(held && holds(me, c.id() as c_long)) {
            continue;
        }
        let (ux, uy) = xy(&c);
        if in_danger_range(me, ux, uy, FLEE_RANGE) {
            run.push(c);
        }
    }
    for c in &run {
        order_move(c, hx, hy);
    }
    if !run.is_empty() {
        trace(me, None, "flee", &format!("n={} zones={n}", run.len()));
    }
}

/// `EECPRMFleeRaids` for the C++.
///
/// # Safety
/// `me` is a live computer player.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_next_flee_raids(me: *mut c_void) {
    flee_raids(me);
}
