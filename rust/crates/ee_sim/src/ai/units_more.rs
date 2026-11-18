//! The rest of the per-unit Lua interface's bindings (lane A, round 2; EECPNextUnits.cpp before): the class and
//! tech tree queries (ai.class, ai.can_hurt, ai.wonders, ai.techs, ai.epoch, ai.improve_cost), the seat's tuning,
//! the commands over the player's own state (ai.improve, ai.gate, ai.tribute, ai.trade, ai.diplomacy,
//! ai.garrison_fill, ai.test_spawn), the view's safety tests and searches (ai.can_place, ai.enemy_near,
//! ai.site_blocked, ai.build_unsafe, ai.place), and the continent manager's and the path manager's (ai.area,
//! ai.area_point, ai.forest_point, ai.hill_point, ai.choke_point, ai.water_sites, ai.unexplored, ai.wall_line,
//! ai.wall_stretches, ai.wall_path, ai.wall_route, ai.wall_route_result).
//!
//! A child of `units_bind` (its helpers, its host, its rules: the same Lua calls in the same order, no destructor
//! across a Lua call, scratch in the state). The tech tree is read through `ee_sim::world`'s tech views; the node
//! methods with bodies (`GetIsAvailable`, `GetIsObsolete`, `GetCost`: the walks and the capture), the classic's
//! data (the personality, the cheats, the behaviour table) and the continent manager's objects are host ops.
#![allow(clippy::missing_safety_doc)]

use std::ffi::{c_char, c_int, c_long, c_void, CStr};

use super::*;
use crate::world::{NodeView, TechTreeView};

// ---------------------------------------------------------------------------------------------------------------
// The host's further operations (EECPNextUnits.cpp ee_ai_units_host)

/// The class `a`'s `FliesOver()`.
pub const EE_AI_UNITS_HOST_FLIES_OVER: i32 = 50;
/// The class `a`'s `GetPopulationCost()`.
pub const EE_AI_UNITS_HOST_POP_COST: i32 = 51;
/// `gDamageTable[a][b] != 0` (0 without a table).
pub const EE_AI_UNITS_HOST_DAMAGES: i32 = 52;
/// The player `a`'s `GetTeamIndex()`.
pub const EE_AI_UNITS_HOST_TEAM_INDEX: i32 = 53;
/// The player `a`'s `GetNumWonderUnits()`.
pub const EE_AI_UNITS_HOST_WONDER_COUNT: i32 = 54;
/// `player->ScaleCost(node a->GetCost(b, c, 1.0f))` (the node's virtual: the capture's record).
pub const EE_AI_UNITS_HOST_NODE_COST: i32 = 55;
/// `player->ScaleCost(a)`.
pub const EE_AI_UNITS_HOST_SCALE_COST: i32 = 56;
/// The node `a`'s `GetIsAvailable()` (virtual: the walk).
pub const EE_AI_UNITS_HOST_NODE_AVAILABLE: i32 = 57;
/// The node `a`'s `GetIsObsolete()` (virtual: the walk).
pub const EE_AI_UNITS_HOST_NODE_OBSOLETE: i32 = 58;
/// `EECPNextTechName(a)` into `out` (1024 bytes, NUL-terminated) -> its length.
pub const EE_AI_UNITS_HOST_TECH_NAME: i32 = 59;
/// `player->CanProduce(a, b, 1, c != 0, 3, NULL)` (the computer player's).
pub const EE_AI_UNITS_HOST_CAN_PRODUCE_AS: i32 = 60;
/// The node `a`'s researched event improves a citizen's gathering (the classic research manager's test).
pub const EE_AI_UNITS_HOST_CITIZEN_UPGRADE: i32 = 61;
/// `EEIsEpochID(a)`.
pub const EE_AI_UNITS_HOST_IS_EPOCH_ID: i32 = 62;
/// `EEEpochCount()`.
pub const EE_AI_UNITS_HOST_EPOCH_COUNT: i32 = 63;
/// The next level's cost of the class `a`'s improvement `b` (`EeAiUnitsImproveCost` into `out`) -> 1, or 0: no
/// node now or no such improvement.
pub const EE_AI_UNITS_HOST_IMPROVE_COST: i32 = 64;
/// `player->CanBuy(a, b, NULL)`.
pub const EE_AI_UNITS_HOST_CAN_BUY: i32 = 65;
/// `EECPGateClass(player)`.
pub const EE_AI_UNITS_HOST_GATE_CLASS: i32 = 66;
/// The class `a` is a gate class (`EEUnitClassOfKind<EE_CLASS_KIND_GATE>`).
pub const EE_AI_UNITS_HOST_IS_GATE_CLASS: i32 = 67;
/// The side (1..4) a gate fits on over the player's finished wall at the tile (a, b); 0: none.
pub const EE_AI_UNITS_HOST_GATE_SIDE: i32 = 68;
/// The tribute of `c` of the resource `b` to the player `a` (the moves and both ledgers).
pub const EE_AI_UNITS_HOST_TRIBUTE: i32 = 69;
/// `gWorld.SetDiplomaticStance(player, a, b)`.
pub const EE_AI_UNITS_HOST_STANCE: i32 = 70;
/// The own finished building `a`'s garrison raised by `b` (`AddGarrison`) -> whether done.
pub const EE_AI_UNITS_HOST_GARRISON_FILL: i32 = 71;
/// `EECPNextSiteBlocked(player, unit a, NULL)`.
pub const EE_AI_UNITS_HOST_SITE_BLOCKED: i32 = 72;
/// `EECPNextBuildUnsafe(player, a, b)`.
pub const EE_AI_UNITS_HOST_BUILD_UNSAFE: i32 = 73;
/// `EECPNextSiteThreat(player, a, b, false, NULL)`.
pub const EE_AI_UNITS_HOST_SITE_THREAT: i32 = 74;
/// The classic's enemy test through the fog (the influence regions round the tile (a, b)) -> the count.
pub const EE_AI_UNITS_HOST_ENEMY_NEAR_ALL: i32 = 75;
/// `EECPNextPlaceSearch` with `out` an `EeAiUnitsPlace` -> 1 found.
pub const EE_AI_UNITS_HOST_PLACE: i32 = 76;
/// `EECPEngineFillsGarrison(player)`.
pub const EE_AI_UNITS_HOST_FILLS_GARRISON: i32 = 77;
/// The seat's tuning beyond its record (`EeAiUnitsTuning` into `out`).
pub const EE_AI_UNITS_HOST_TUNING: i32 = 78;

/// `EE_AI_UNITS_HOST_IMPROVE_COST`'s answer.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct EeAiUnitsImproveCost {
    pub cost: [c_long; 5],
    pub level: c_long,
    pub max: c_long,
}

/// `EE_AI_UNITS_HOST_PLACE`'s arguments and answer.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeAiUnitsPlace {
    pub unit_type: c_long,
    pub mode: c_long,
    pub x: c_long,
    pub y: c_long,
    pub first: c_long,
    pub last: c_long,
    /// the site's index (the script's, less one)
    pub site: c_long,
    pub units: *const c_long,
    pub units_len: usize,
    pub out_x: c_long,
    pub out_y: c_long,
    pub out_gap: c_long,
}

/// What `ai.tuning` reads beyond the computer player's record (`EE_AI_UNITS_HOST_TUNING`): the game's options,
/// the map, the continents, the manifest's switches, the cheats, the personality and the experiments, the
/// behaviour table's values.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeAiUnitsTuning {
    pub deathmatch: bool,
    pub all_managers: bool,
    pub epoch_start: c_long,
    pub epoch_end: c_long,
    pub space: bool,
    pub water_tiles: c_long,
    pub land_tiles: c_long,
    pub space_tiles: c_long,
    pub largest_sea: c_long,
    pub fills_garrison: bool,
    pub sees_all: bool,
    pub population_limit: c_long,
    pub cheats: [bool; 7],
    /// the personality's id (NUL-terminated)
    pub id: [c_char; 128],
    pub epoch_pressure_start: f32,
    pub epoch_pressure_full: f32,
    pub stone_span: f32,
    pub safe_bonus: f32,
    pub surplus: c_long,
    pub surplus_cut: f32,
    pub low_stock: c_long,
    pub floor: [f32; 5],
    pub max_citizen_share: f32,
    pub wood_slots: c_long,
    pub moves_per_cycle: c_long,
    pub demand_split: bool,
    pub coast_fishing: bool,
    pub forage_patches: bool,
    pub claim_range: bool,
    pub claim_needed: bool,
    pub early_town_centers: c_long,
    pub early_trainers: c_long,
    pub patch_expand: bool,
    pub standing_army: f32,
    pub army_floor: f32,
    pub militia: bool,
    pub militia_towers: bool,
    pub prehistoric_army: c_long,
    pub prehistoric_peace: bool,
    pub stone_peace: bool,
    pub naval_flee: bool,
    pub epoch_advance_time_scale: f32,
    /// the carrier fighters to keep: easy, medium, hard; then in a deathmatch
    pub carrier_fighters: [c_long; 6],
    pub population_cap_own_weight: f32,
    pub population_cap_other_weight: f32,
}

/// The host records' sizes summed (the C++ checks its copy of their layout against it once; it also puts the
/// records in the header).
///
/// # Safety
/// None: the pointers are not read.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_units_records_size(
    a: *const EeAiUnitsImproveCost,
    b: *const EeAiUnitsPlace,
    c: *const EeAiUnitsTuning,
    d: *const EeAiUnitsArea,
    e: *const EeAiUnitsAreaPick,
    f: *const EeAiUnitsTiles,
) -> usize {
    let _ = (a, b, c, d, e, f);
    std::mem::size_of::<EeAiUnitsImproveCost>()
        + std::mem::size_of::<EeAiUnitsPlace>()
        + std::mem::size_of::<EeAiUnitsTuning>()
        + std::mem::size_of::<EeAiUnitsArea>()
        + std::mem::size_of::<EeAiUnitsAreaPick>()
        + std::mem::size_of::<EeAiUnitsTiles>()
}

pub const EE_AI_UNITS_MSG_IMPROVE: i32 = 27;
pub const EE_AI_UNITS_MSG_HACK_NEW_UNIT: i32 = 28;
pub const EE_AI_UNITS_MSG_BUILD_GATE: i32 = 29;
pub const EE_AI_UNITS_MSG_EXCHANGE: i32 = 30;

/// `EE_AI_UNITS_HOST_EXEC`'s modes beyond 0 (the selection) and 1 (as it is): 2 under the borrowed slot; 3 under
/// it when it names the player
const MODE_BORROWED: i32 = 2;
const MODE_BORROWED_CHECKED: i32 = 3;

const RESOURCE_NAMES: [&CStr; 5] = [c"food", c"wood", c"stone", c"gold", c"iron"];
const RESOURCE_GOLD: c_long = 5;
const TYPE_TOWN_CENTER_L: c_long = 0x23;
const TYPE_CITIZEN: c_long = 0x20;
const TYPE_CITIZEN_2: c_long = 0x1e0;
const CAT_PLANET_SPACESHIP: u64 = 0x22;
const FAMILY_MINES: u64 = 0x16;
const MOVE_AIR: u8 = 1;
const MOVE_SEA: u8 = 4;
const MOVE_SPACE: u8 = 8;
const BLOCKED: [&CStr; 6] = [c"", c"unexplored", c"enemy_near", c"danger", c"area", c"tower"];

fn exec(g: &Game<'_>, mode: i32, id: i32, values: &[V]) {
    let mut m = message(id, mode, values);
    host_out(EE_AI_UNITS_HOST_EXEC, g.me_ptr, 0, (&mut m as *mut EeAiUnitsMessage).cast());
}

fn class_ok<'t>(g: &Game<'t>, t: c_long) -> Option<ClassView<'t>> {
    class_at(g, t)
}

fn tree<'t>(g: &Game<'t>) -> Option<TechTreeView<'t>> {
    g.me.tech_tree()
}

fn node_cost(g: &Game<'_>, n: &NodeView<'_>, resource: c_long, unit_type: c_long) -> c_long {
    host(EE_AI_UNITS_HOST_NODE_COST, g.me_ptr, n.handle() as i64, resource as i64, unit_type as i64) as c_long
}

fn scale_cost(g: &Game<'_>, v: c_long) -> c_long {
    host(EE_AI_UNITS_HOST_SCALE_COST, g.me_ptr, v as i64, 0, 0) as c_long
}

fn node_flag(g: &Game<'_>, op: i32, n: &NodeView<'_>) -> bool {
    host(op, g.me_ptr, n.handle() as i64, 0, 0) != 0
}

fn data_costs(n: &NodeView<'_>) -> [c_long; 7] {
    n.data().map_or([0; 7], |d| crate::world_field!(d, mCosts))
}

/// The name of a tech into a buffer (the binding pushes it before the buffer goes)
fn tech_name(id: c_long, buf: &mut [u8; 1024]) -> *const c_char {
    let n = host_out(EE_AI_UNITS_HOST_TECH_NAME, std::ptr::null_mut(), id as i64, buf.as_mut_ptr().cast());
    buf[n.clamp(0, 1023) as usize] = 0;
    buf.as_ptr().cast()
}

/// `EECPUnitsMilli`: a float as thousandths
fn milli(v: f32) -> c_long {
    um::lrintf(v * 1000.0) as c_long
}

unsafe fn push_ints(l: *mut lua_State, values: &[c_long]) {
    for (k, v) in values.iter().enumerate() {
        lua_pushinteger(l, *v as lua_Integer);
        lua_rawseti(l, -2, k as lua_Integer + 1);
    }
}

/// `EECPUnitsPushCost`: the node's cost as the player pays it (for the class), then its listed cost scaled
unsafe fn push_cost(l: *mut lua_State, g: &Game<'_>, n: &NodeView<'_>, unit_type: c_long) {
    lua_createtable(l, 0, 5);
    for (i, name) in RESOURCE_NAMES.iter().enumerate() {
        lua_pushinteger(l, node_cost(g, n, RESOURCE_FOOD as c_long + i as c_long, unit_type) as lua_Integer);
        lua_setfield(l, -2, name.as_ptr());
    }
    lua_setfield(l, -2, c"cost".as_ptr());
    let costs = data_costs(n);
    lua_createtable(l, 0, 5);
    for (i, name) in RESOURCE_NAMES.iter().enumerate() {
        lua_pushinteger(l, scale_cost(g, costs[RESOURCE_FOOD as usize + i]) as lua_Integer);
        lua_setfield(l, -2, name.as_ptr());
    }
    lua_setfield(l, -2, c"data_cost".as_ptr());
}

// ---------------------------------------------------------------------------------------------------------------
// The class and tech tree queries

/// `ai.can_hurt(attacker_type, target_type)` -> whether the attacker's weapon hurts the target's family (the
/// damage table)
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_lua_can_hurt(l: *mut c_void) -> c_int {
    let l = l as *mut lua_State;
    let me_ptr = me_of(l);
    let s = state_of(me_ptr);
    charge(l, s, COST_CALL);
    let attacker = class_arg(l, 1);
    let target = class_arg(l, 2);
    let t = WorldRead::begin();
    let mut hurts = false;
    if let Some(g) = Game::new(&t, me_ptr) {
        if let (Some(a), Some(b)) = (class_ok(&g, attacker), class_ok(&g, target)) {
            let ammo: c_long = crate::world_field!(a.fields(), mAmmoType);
            let family = b.family() as c_long;
            let flare: bool = crate::world_field!(b.fields(), mIsFlare);
            hurts = ammo >= 0
                && family >= 0
                && !flare
                && host(EE_AI_UNITS_HOST_DAMAGES, me_ptr, family as i64, ammo as i64, 0) != 0;
        }
    }
    lua_pushboolean(l, hurts as c_int);
    1
}

/// `ai.wonders()` -> {victory, to_win, players = {{player, team, count}}, list = {{id, player, team, type, built,
/// x, y}}}: the wonders; a wonder's tile only where the player has explored it (or "sees_all")
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_lua_wonders(l: *mut c_void) -> c_int {
    let l = l as *mut lua_State;
    let me_ptr = me_of(l);
    let s = state_of(me_ptr);
    charge(l, s, COST_CALL);
    let t = WorldRead::begin();
    let Some(g) = Game::new(&t, me_ptr) else { return 0 };
    let all = host(EE_AI_UNITS_HOST_SEES_ALL, me_ptr, 0, 0, 0) != 0;
    lua_createtable(l, 0, 4);
    set_bool(l, c"victory", g.world.victory_enabled());
    set_int(l, c"to_win", g.world.wonders_to_win() as c_long);
    lua_createtable(l, 0, 0);
    lua_createtable(l, 0, 0);
    let mut count: lua_Integer = 0;
    let mut listed: lua_Integer = 0;
    for i in 1..g.world.num_players() as usize {
        let Some(p) = g.world.player(i) else { continue };
        let Some(tree) = p.tech_tree().filter(|_| !p.observer()) else { continue };
        let team = host(EE_AI_UNITS_HOST_TEAM_INDEX, me_ptr, p.handle() as i64, 0, 0) as c_long;
        lua_createtable(l, 0, 3);
        set_int(l, c"player", p.id() as c_long);
        set_int(l, c"team", team);
        set_int(l, c"count", host(EE_AI_UNITS_HOST_WONDER_COUNT, me_ptr, p.handle() as i64, 0, 0) as c_long);
        count += 1;
        lua_rawseti(l, -3, count);
        for u in p.units() {
            let Some(c) = u.complex_unit() else { continue };
            let Some(class) = c.class() else { continue };
            if c.hit_points() <= 0 || host(EE_AI_UNITS_HOST_LAND_BUILDING, me_ptr, class.handle() as i64, 0, 0) == 0 {
                continue;
            }
            let Some(node) = tree.node(class.technology()) else { continue };
            let kind: c_long = node.data().map_or(0, |d| crate::world_field!(d, mKind));
            if kind != 4 {
                continue;
            }
            charge(l, s, COST_ID);
            lua_createtable(l, 0, 7);
            set_int(l, c"id", c.id() as c_long);
            set_int(l, c"player", p.id() as c_long);
            set_int(l, c"team", team);
            set_str(l, c"type", name_of(&mut *s, class.unit_type() as c_long));
            set_bool(l, c"built", c.is_built());
            if p.handle() == g.me.handle() || all || g.explored(&c) {
                let (x, y) = c.tile_xy();
                set_int(l, c"x", x as c_long);
                set_int(l, c"y", y as c_long);
            }
            listed += 1;
            lua_rawseti(l, -2, listed);
        }
    }
    lua_setfield(l, -3, c"list".as_ptr());
    lua_setfield(l, -2, c"players".as_ptr());
    1
}

/// `ai.class(type)` -> static data from the class and the player's improvements (what the unit's panel shows)
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_lua_class(l: *mut c_void) -> c_int {
    let l = l as *mut lua_State;
    let me_ptr = me_of(l);
    let s = state_of(me_ptr);
    charge(l, s, COST_RECORD);
    let unit_type = class_arg(l, 1);
    let t = WorldRead::begin();
    let Some(g) = Game::new(&t, me_ptr) else {
        lua_pushnil(l);
        return 1;
    };
    let Some(c) = class_ok(&g, unit_type) else {
        lua_pushnil(l);
        return 1;
    };
    let f = c.fields();
    lua_createtable(l, 0, 16);
    set_str(l, c"type", name_of(&mut *s, unit_type));
    set_str(l, c"category", category_class(&c, false).as_ptr());
    set_int(l, c"family", c.family() as c_long);
    set_int(l, c"tech", c.technology() as c_long);
    {
        // the cost as the player pays it (0 for a class the player's tree has no node for)
        let node = tree(&g).and_then(|tr| tr.node(c.technology()));
        lua_createtable(l, 0, 0);
        for (i, name) in RESOURCE_NAMES.iter().enumerate() {
            let v = node
                .as_ref()
                .map_or(0, |n| node_cost(&g, n, RESOURCE_FOOD as c_long + i as c_long, TYPE_TOWN_CENTER_L));
            lua_pushinteger(l, v as lua_Integer);
            lua_setfield(l, -2, name.as_ptr());
        }
        lua_setfield(l, -2, c"cost".as_ptr());
        lua_createtable(l, 0, 0);
        for (i, name) in RESOURCE_NAMES.iter().enumerate() {
            let v = node.as_ref().map_or(0, |n| scale_cost(&g, data_costs(n)[RESOURCE_FOOD as usize + i]));
            lua_pushinteger(l, v as lua_Integer);
            lua_setfield(l, -2, name.as_ptr());
        }
        lua_setfield(l, -2, c"data_cost".as_ptr());
    }
    set_bool(l, c"building", is_building(&c));
    set_int(l, c"size", crate::world_field!(f, mTileSize));
    set_int(l, c"garrison", crate::world_field!(f, mGarrisonCapacity));
    set_int(l, c"transport", crate::world_field!(f, mTransportCapacity));
    set_bool(l, c"can_garrison", crate::world_field!(f, mCanGarrison));
    set_int(l, c"ai_kind", c.ai_kind() as c_long);
    lua_createtable(l, 3, 0);
    let counters: [c_long; 3] = [
        crate::world_field!(f, mCounterKind1) as c_long,
        crate::world_field!(f, mCounterKind2) as c_long,
        crate::world_field!(f, mCounterKind3) as c_long,
    ];
    push_ints(l, &counters);
    lua_setfield(l, -2, c"counters".as_ptr());
    set_bool(l, c"attack_unit", crate::world_field!(f, mIsAttackUnit));
    set_bool(l, c"defense_unit", crate::world_field!(f, mIsDefenseUnit));
    set_bool(l, c"medic", crate::world_field!(f, mIsMedic));
    let rate: f32 = crate::world_field!(f, mConversionRate);
    set_bool(l, c"converts", crate::world_field!(f, mHasCalamityPowers) || rate != 0.0);
    set_bool(l, c"is_transport", host(EE_AI_UNITS_HOST_IS_TRANSPORT, me_ptr, c.handle() as i64, 0, 0) != 0);
    set_bool(l, c"planet_spaceship", c.category() == CAT_PLANET_SPACESHIP);
    set_bool(l, c"mine", c.family() == FAMILY_MINES);
    set_int(l, c"influence", crate::world_field!(f, mInfluence) as c_long);
    set_int(l, c"epoch", crate::world_field!(f, mEpoch) as c_long);
    set_int(l, c"pop", host(EE_AI_UNITS_HOST_POP_COST, me_ptr, c.handle() as i64, 0, 0) as c_long);
    {
        let d: u8 = crate::world_field!(f, mDomains);
        let domain = if (d & MOVE_AIR) != 0 && host(EE_AI_UNITS_HOST_FLIES_OVER, me_ptr, c.handle() as i64, 0, 0) == 0 {
            c"air"
        } else if (d & MOVE_SEA) != 0 {
            c"sea"
        } else if (d & MOVE_SPACE) != 0 {
            c"space"
        } else {
            c"land"
        };
        set_str(l, c"domain", domain.as_ptr());
    }
    let upgrade = host(EE_AI_UNITS_HOST_UPGRADE, me_ptr, g.me.handle() as i64, unit_type as i64, 0) as u64;
    if let Some(u) = StatsView::of(&t, upgrade) {
        let uf = u.fields();
        set_int(l, c"hp", crate::world_field!(uf, mMaxHitPoints));
        set_int(l, c"attack", um::lrintf(crate::world_field!(uf, mAttack)) as c_long);
        set_int(l, c"range", centi(crate::world_field!(uf, mRange)));
        set_int(l, c"sight", centi(crate::world_field!(uf, mLineOfSight)));
        set_int(l, c"speed", centi(crate::world_field!(uf, mMaxVelocity)));
        set_int(l, c"reload", crate::world_field!(uf, mReloadTime));
        // an aircraft's fuel (ms in the air; 0: no limit)
        set_int(l, c"flight_time", crate::world_field!(uf, mFlightTime));
    }
    1
}

/// `ai.techs{epoch = n, id = tech, name = data id}` -> the player's tech tree nodes of epochs n..0 (default: the
/// current epoch) in the tree's order, or the one node
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_lua_techs(l: *mut c_void) -> c_int {
    let l = l as *mut lua_State;
    let me_ptr = me_of(l);
    let s = state_of(me_ptr);
    let t = WorldRead::begin();
    let g = Game::new(&t, me_ptr);
    let Some((g, tr)) = g.and_then(|g| tree(&g).map(|tr| (g, tr))) else {
        lua_createtable(l, 0, 0);
        return 1;
    };
    let mut epoch = tr.current_epoch() as c_long;
    let mut only: c_long = -1;
    opt_int(l, 1, c"epoch", &mut epoch);
    opt_int(l, 1, c"id", &mut only);
    let name = crate::ai::units_lua::ee_ai_units_opt_string(l.cast(), 1, c"name".as_ptr());
    if !name.is_null() {
        only = host(EE_AI_UNITS_HOST_TECH_BY_NAME, me_ptr, name as i64, 0, 0) as c_long;
    }
    if !name.is_null() && only < 0 {
        lua_createtable(l, 0, 0);
        return 1;
    }
    // the ids in the state's scratch (a raising charge must not drop a Rust vector)
    let ids = &mut (*s).ids;
    ids.clear();
    if only >= 0 {
        ids.push(only);
    } else {
        let last = tr.epochs() - 1;
        let mut i = (epoch as i64).min(last);
        while i >= 0 {
            tr.epoch_node_ids(i, ids);
            i -= 1;
        }
    }
    let n_ids = (*s).ids.len();
    charge(l, s, COST_CALL + n_ids as i64 * COST_RECORD);
    lua_createtable(l, n_ids as c_int, 0);
    let mut n: lua_Integer = 0;
    let mut buf = [0u8; 1024];
    for k in 0..n_ids {
        let id = (&(*s).ids)[k];
        let Some(node) = tr.node(id as i64) else { continue };
        let Some(d) = node.data() else { continue };
        lua_createtable(l, 0, 18);
        let did: c_long = crate::world_field!(d, mID);
        set_int(l, c"id", did);
        set_str(l, c"name", tech_name(did, &mut buf));
        set_int(l, c"epoch", node.epoch() as c_long);
        set_int(l, c"kind", crate::world_field!(d, mKind));
        let makes: bool = crate::world_field!(d, mMakesUnit);
        let mut class: c_long = if makes { crate::world_field!(d, mUnitClass) as c_long } else { -1 };
        if class >= 0 && class_ok(&g, class).is_some() {
            set_str(l, c"makes", name_of(&mut *s, class));
        } else {
            class = -1;
        }
        set_bool(l, c"counts_toward_epoch", crate::world_field!(d, mCountsTowardEpoch));
        set_bool(l, c"researched", node.researched());
        set_bool(l, c"available", node_flag(&g, EE_AI_UNITS_HOST_NODE_AVAILABLE, &node));
        set_bool(l, c"obsolete", node_flag(&g, EE_AI_UNITS_HOST_NODE_OBSOLETE, &node));
        set_int(l, c"obsolete_epoch", node.obsolete_epoch() as c_long);
        let producers = node.unit_classes_len();
        lua_createtable(l, producers as c_int, 0);
        let mut plain = false;
        let mut np: lua_Integer = 0;
        for i in 0..producers {
            let producer = node.unit_class(i) as c_long;
            let Some(pc) = class_ok(&g, producer) else { continue };
            lua_pushstring(l, name_of(&mut *s, producer));
            np += 1;
            lua_rawseti(l, -2, np);
            if let Some(other) = tr.node(pc.technology()) {
                if other.data().is_some()
                    && host(EE_AI_UNITS_HOST_IS_EPOCH_ID, me_ptr, other.id(), 0, 0) == 0
                    && producer != TYPE_CITIZEN
                    && producer != TYPE_CITIZEN_2
                {
                    plain = true;
                }
            }
        }
        lua_setfield(l, -2, c"producers".as_ptr());
        set_bool(l, c"plain_producer", plain);
        let first = if producers > 0 { node.unit_class(0) as c_long } else { TYPE_CITIZEN };
        push_cost(l, &g, &node, if class >= 0 { class } else { TYPE_CITIZEN });
        let produce =
            |afford: i64| host(EE_AI_UNITS_HOST_CAN_PRODUCE_AS, me_ptr, did as i64, first as i64, afford) != 0;
        set_bool(l, c"producible", produce(0));
        set_bool(l, c"affordable", produce(1));
        set_bool(l, c"citizen_upgrade", node_flag(&g, EE_AI_UNITS_HOST_CITIZEN_UPGRADE, &node));
        n += 1;
        lua_rawseti(l, -2, n);
    }
    1
}

/// `ai.epoch(n)` -> epoch n's node: id, name, required_sol, current_sol, cost, available, researched; nil off the
/// tree
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_lua_epoch(l: *mut c_void) -> c_int {
    let l = l as *mut lua_State;
    let me_ptr = me_of(l);
    let s = state_of(me_ptr);
    charge(l, s, COST_RECORD);
    let t = WorldRead::begin();
    let index: i64 = if is_int(l, 1) { lua_tointeger(l, 1) as c_long as i64 } else { -1 };
    let g = Game::new(&t, me_ptr);
    let found = g.as_ref().and_then(|g| {
        let tr = tree(g)?;
        if index < 0 || index >= tr.epochs() || index >= host(EE_AI_UNITS_HOST_EPOCH_COUNT, me_ptr, 0, 0, 0) {
            return None;
        }
        let e = tr.epoch(index)?;
        e.data()?;
        let node = tr.node(e.id())?;
        Some((e, node))
    });
    let (Some(g), Some((e, node))) = (g.as_ref(), found) else {
        lua_pushnil(l);
        return 1;
    };
    let mut buf = [0u8; 1024];
    lua_createtable(l, 0, 8);
    set_int(l, c"id", node.id() as c_long);
    set_str(l, c"name", tech_name(node.id() as c_long, &mut buf));
    set_int(l, c"required_sol", e.required_sol() as c_long);
    set_int(l, c"current_sol", e.current_sol() as c_long);
    push_cost(l, g, &node, TYPE_TOWN_CENTER_L);
    set_bool(l, c"available", node_flag(g, EE_AI_UNITS_HOST_NODE_AVAILABLE, &node));
    set_bool(l, c"researched", node.researched());
    1
}

/// `ai.improve_cost(class, what)` -> {food=, wood=, stone=, gold=, iron=, level=, max=}: the next level's cost of a
/// class's improvement (what: "attack", "hit_points", "range", "speed" or its number), or nil
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_lua_improve_cost(l: *mut c_void) -> c_int {
    let l = l as *mut lua_State;
    let me_ptr = me_of(l);
    let s = state_of(me_ptr);
    charge(l, s, COST_CALL);
    let unit_type = class_arg(l, 1);
    let mut what: c_long = -1;
    if is_int(l, 2) {
        what = int_at(l, 2);
    } else if lua_type(l, 2) == LUA_TSTRING {
        what = match CStr::from_ptr(lua_tostring(l, 2)).to_bytes() {
            b"range" => 11,
            b"attack" => 1,
            b"hit_points" => 9,
            b"speed" => 14,
            _ => -1,
        };
    }
    let mut c = EeAiUnitsImproveCost::default();
    if unit_type < 0
        || what < 0
        || ee_ai_units_host(
            EE_AI_UNITS_HOST_IMPROVE_COST,
            me_ptr,
            unit_type as i64,
            what as i64,
            0,
            (&mut c as *mut EeAiUnitsImproveCost).cast(),
        ) == 0
    {
        return 0;
    }
    lua_createtable(l, 0, 7);
    for (i, name) in RESOURCE_NAMES.iter().enumerate() {
        set_int(l, name, c.cost[i]);
    }
    set_int(l, c"level", c.level);
    set_int(l, c"max", c.max);
    1
}

/// `ai.tuning()` -> what the classic computer player reads from data for this seat (floats as thousandths, keys
/// ending in _k)
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_lua_tuning(l: *mut c_void) -> c_int {
    let l = l as *mut lua_State;
    let me_ptr = me_of(l);
    let s = state_of(me_ptr);
    charge(l, s, COST_CALL + 40 * COST_RECORD / 10);
    let t = WorldRead::begin();
    let Some(g) = Game::new(&t, me_ptr) else { return 0 };
    // SAFETY: the binding's upvalue is the computer player
    let cp = g.me.computer_fields();
    let mut tu: EeAiUnitsTuning = std::mem::zeroed();
    host_out(EE_AI_UNITS_HOST_TUNING, me_ptr, 0, (&mut tu as *mut EeAiUnitsTuning).cast());
    lua_createtable(l, 0, 16);
    set_int(l, c"difficulty", crate::world_field!(cp, mDifficulty) as c_long);
    set_bool(l, c"deathmatch", tu.deathmatch);
    set_bool(l, c"all_managers", tu.all_managers);
    set_int(l, c"epoch_start", tu.epoch_start);
    set_int(l, c"epoch_end", tu.epoch_end);
    let kind: std::ffi::c_ulong = g.world.map().map_or(0, |m| crate::world_field!(m.fields(), mMapCharacteristics));
    set_bool(l, c"island", (kind & 0x1) != 0);
    set_bool(l, c"planets", (kind & 0x4) != 0);
    set_bool(l, c"waterless", (kind & 0x2) != 0);
    set_bool(l, c"space", tu.space);
    set_int(l, c"water_tiles", tu.water_tiles);
    set_int(l, c"land_tiles", tu.land_tiles);
    set_int(l, c"space_tiles", tu.space_tiles);
    set_int(l, c"largest_sea", tu.largest_sea);
    set_bool(l, c"fills_garrison", tu.fills_garrison);
    set_bool(l, c"sees_all", tu.sees_all);
    set_int(l, c"population_limit", tu.population_limit);
    lua_createtable(l, 0, 7);
    const CHEATS: [&CStr; 7] = [c"cost", c"reveal", c"epochs", c"extra", c"popband", c"catchup", c"retire"];
    for (k, name) in CHEATS.iter().enumerate() {
        set_bool(l, name, tu.cheats[k]);
    }
    lua_setfield(l, -2, c"cheats".as_ptr());
    lua_createtable(l, 0, 32);
    set_str(l, c"id", tu.id.as_ptr());
    set_int(l, c"epoch_pressure_start_k", milli(tu.epoch_pressure_start));
    set_int(l, c"epoch_pressure_full_k", milli(tu.epoch_pressure_full));
    set_int(l, c"stone_span_k", milli(tu.stone_span));
    set_int(l, c"safe_bonus_k", milli(tu.safe_bonus));
    set_int(l, c"surplus", tu.surplus);
    set_int(l, c"surplus_cut_k", milli(tu.surplus_cut));
    set_int(l, c"low_stock", tu.low_stock);
    lua_createtable(l, 0, 5);
    for (k, name) in RESOURCE_NAMES.iter().enumerate() {
        set_int(l, name, milli(tu.floor[k]));
    }
    lua_setfield(l, -2, c"floor_k".as_ptr());
    set_int(l, c"max_citizen_share_k", milli(tu.max_citizen_share));
    set_int(l, c"wood_slots", tu.wood_slots);
    set_int(l, c"moves_per_cycle", tu.moves_per_cycle);
    set_bool(l, c"demand_split", tu.demand_split);
    set_bool(l, c"coast_fishing", tu.coast_fishing);
    set_bool(l, c"forage_patches", tu.forage_patches);
    set_bool(l, c"claim_range", tu.claim_range);
    set_bool(l, c"claim_needed", tu.claim_needed);
    set_int(l, c"early_town_centers", tu.early_town_centers);
    set_int(l, c"early_trainers", tu.early_trainers);
    set_bool(l, c"patch_expand", tu.patch_expand);
    set_int(l, c"standing_army_k", milli(tu.standing_army));
    set_int(l, c"army_floor_k", milli(tu.army_floor));
    set_bool(l, c"militia", tu.militia);
    set_bool(l, c"militia_towers", tu.militia_towers);
    set_int(l, c"prehistoric_army", tu.prehistoric_army);
    set_bool(l, c"prehistoric_attacks", !tu.prehistoric_peace);
    set_bool(l, c"stone_attacks", !tu.stone_peace);
    set_bool(l, c"naval_flee", tu.naval_flee);
    lua_setfield(l, -2, c"personality".as_ptr());
    // the behaviour record values (EEComputerPlayer::Init; a scenario's SetAIVariable may have changed them)
    lua_createtable(l, 0, 180);
    tuning_behavior(l, &g, cp);
    set_int(l, c"epoch_advance_time_scale_k", milli(tu.epoch_advance_time_scale));
    const FIGHTERS: [&CStr; 6] = [
        c"carrier_fighters_easy",
        c"carrier_fighters_medium",
        c"carrier_fighters_hard",
        c"carrier_fighters_dm_easy",
        c"carrier_fighters_dm_medium",
        c"carrier_fighters_dm_hard",
    ];
    for (k, name) in FIGHTERS.iter().enumerate() {
        set_int(l, name, tu.carrier_fighters[k]);
    }
    set_int(l, c"population_cap_own_weight_k", milli(tu.population_cap_own_weight));
    set_int(l, c"population_cap_other_weight_k", milli(tu.population_cap_other_weight));
    lua_setfield(l, -2, c"behavior".as_ptr());
    1
}

/// The behaviour record's values (EEComputerPlayer::Init's copies, `EeCpFields`), in the C++'s order
unsafe fn tuning_behavior(
    l: *mut lua_State,
    g: &Game<'_>,
    cp: crate::world::Record<'_, crate::cp::fields::EeCpFields>,
) {
    set_int(l, c"assist_allies_delay_easy", crate::world_field!(cp, mAssistAlliesDelayEasy) as c_long);
    set_int(l, c"assist_allies_delay_medium", crate::world_field!(cp, mAssistAlliesDelayMedium) as c_long);
    set_int(l, c"assist_allies_delay_hard", crate::world_field!(cp, mAssistAlliesDelayHard) as c_long);
    set_int(l, c"naval_unit_weight_k", milli(crate::world_field!(cp, mNavalUnitWeight)));
    set_int(l, c"air_unit_weight_k", milli(crate::world_field!(cp, mAirUnitWeight)));
    set_int(l, c"max_flame_throwers", crate::world_field!(cp, mMaxFlameThrowers) as c_long);
    set_int(l, c"unit_cost_medium_k", milli(crate::world_field!(cp, mUnitCostMedium)));
    set_int(l, c"carrier_max_per_group", crate::world_field!(cp, mCarrierMaxPerGroup) as c_long);
    set_int(l, c"carrier_pop_percentage_k", milli(crate::world_field!(cp, mCarrierPopPercentage)));
    set_int(l, c"carrier_chance_k", milli(crate::world_field!(cp, mCarrierChance)));
    set_int(l, c"num_air_explorers", crate::world_field!(cp, mNumAirExplorers) as c_long);
    set_int(l, c"computer_aggression_k", milli(crate::world_field!(cp, mComputerAggression)));
    set_int(l, c"attack_enemy_chance", crate::world_field!(cp, mAttackEnemyChance) as c_long);
    set_int(l, c"attack_to_claim_resource_chance", crate::world_field!(cp, mAttackToClaimResourceChance) as c_long);
    set_int(l, c"attack_for_revenge_chance", crate::world_field!(cp, mAttackForRevengeChance) as c_long);
    set_int(l, c"attack_group_size_min", crate::world_field!(cp, mAttackGroupSizeMin) as c_long);
    set_int(l, c"attack_group_size_max", crate::world_field!(cp, mAttackGroupSizeMax) as c_long);
    set_int(l, c"attack_frequency_per_epoch_k", milli(crate::world_field!(cp, mAttackFrequencyPerEpoch)));
    set_int(l, c"attack_frequency_modifier_easy", crate::world_field!(cp, mAttackFrequencyModifierEasy) as c_long);
    set_int(l, c"attack_frequency_modifier_hard", crate::world_field!(cp, mAttackFrequencyModifierHard) as c_long);
    set_int(l, c"attack_frequency_floor", crate::world_field!(cp, mAttackFrequencyFloor) as c_long);
    set_int(l, c"attack_frequency_ceiling", crate::world_field!(cp, mAttackFrequencyCeiling) as c_long);
    set_int(l, c"attack_frequency_min", crate::world_field!(cp, mAttackFrequencyMin) as c_long);
    set_int(l, c"attack_frequency_max", crate::world_field!(cp, mAttackFrequencyMax) as c_long);
    set_int(l, c"initial_attack_delay_per_epoch", crate::world_field!(cp, mInitialAttackDelayPerEpoch) as c_long);
    set_int(l, c"attack_delay_modifier_easy", crate::world_field!(cp, mAttackDelayModifierEasy) as c_long);
    set_int(l, c"attack_delay_modifier_hard", crate::world_field!(cp, mAttackDelayModifierHard) as c_long);
    set_int(l, c"attack_delay_floor", crate::world_field!(cp, mAttackDelayFloor) as c_long);
    set_int(l, c"attack_delay_ceiling", crate::world_field!(cp, mAttackDelayCeiling) as c_long);
    set_int(l, c"initial_attack_delay_min", crate::world_field!(cp, mInitialAttackDelayMin) as c_long);
    set_int(l, c"initial_attack_delay_max", crate::world_field!(cp, mInitialAttackDelayMax) as c_long);
    set_int(l, c"specialized_unit_percentage_k", milli(crate::world_field!(cp, mSpecializedUnitPercentage)));
    set_int(l, c"attack_group_modifier_easy", crate::world_field!(cp, mAttackGroupModifierEasy) as c_long);
    set_int(l, c"attack_group_modifier_medium", crate::world_field!(cp, mAttackGroupModifierMedium) as c_long);
    set_int(l, c"attack_group_modifier_hard", crate::world_field!(cp, mAttackGroupModifierHard) as c_long);
    set_int(l, c"rps_selection_non_islands_k", milli(crate::world_field!(cp, mRPSSelectionNonIslands)));
    set_int(l, c"rps_selection_islands_k", milli(crate::world_field!(cp, mRPSSelectionIslands)));
    set_int(l, c"specialized_unit_chance_k", milli(crate::world_field!(cp, mSpecializedUnitChance)));
    set_int(l, c"bonzai_attack_level", crate::world_field!(cp, mBonzaiAttackLevel) as c_long);
    set_int(l, c"dm_early_attack_time", crate::world_field!(cp, mDMEarlyAttackTime) as c_long);
    set_int(l, c"dm_early_attack_scale_k", milli(crate::world_field!(cp, mDMEarlyAttackScale)));
    set_int(l, c"town_guard_percentage_k", milli(crate::world_field!(cp, mTownGuardPercentage)));
    set_int(l, c"citizen_percentage_k", milli(crate::world_field!(cp, mCitizenPercentage)));
    set_int(l, c"unit_cost_reduction_easy", crate::world_field!(cp, mUnitCostReductionEasy) as c_long);
    set_int(l, c"unit_cost_reduction_hard", crate::world_field!(cp, mUnitCostReductionHard) as c_long);
    set_int(l, c"defense_group_size_min", crate::world_field!(cp, mDefenseGroupSizeMin) as c_long);
    set_int(l, c"defense_group_size_max", crate::world_field!(cp, mDefenseGroupSizeMax) as c_long);
    set_int(l, c"epoch_frequency_min", crate::world_field!(cp, mEpochFrequencyMin) as c_long);
    set_int(l, c"epoch_frequency_max", crate::world_field!(cp, mEpochFrequencyMax) as c_long);
    set_int(l, c"epoch_catch_up_min", crate::world_field!(cp, mEpochCatchUpMin) as c_long);
    set_int(l, c"epoch_catch_up_max", crate::world_field!(cp, mEpochCatchUpMax) as c_long);
    set_int(l, c"epoch_catch_up_easy_k", milli(crate::world_field!(cp, mEpochCatchUpEasy)));
    set_int(l, c"epoch_catch_up_hard_k", milli(crate::world_field!(cp, mEpochCatchUpHard)));
    set_int(l, c"epoch_catch_up_deathmatch_k", milli(crate::world_field!(cp, mEpochCatchUpDeathmatch)));
    set_int(l, c"epoch_frequency_modifier_easy", crate::world_field!(cp, mEpochFrequencyModifierEasy) as c_long);
    set_int(l, c"epoch_frequency_modifier_hard", crate::world_field!(cp, mEpochFrequencyModifierHard) as c_long);
    set_int(l, c"expansion_timer_easy", crate::world_field!(cp, mExpansionTimerEasy) as c_long);
    set_int(l, c"expansion_timer_medium", crate::world_field!(cp, mExpansionTimerMedium) as c_long);
    set_int(l, c"expansion_timer_hard", crate::world_field!(cp, mExpansionTimerHard) as c_long);
    set_int(l, c"guard_continent_percentage_k", milli(crate::world_field!(cp, mGuardContinentPercentage)));
    set_int(l, c"guard_chokepoint_air_percentage_k", milli(crate::world_field!(cp, mGuardChokepointAirPercentage)));
    set_int(l, c"guard_chokepoint_percentage_k", milli(crate::world_field!(cp, mGuardChokepointPercentage)));
    set_int(l, c"guard_high_ground_percentage_k", milli(crate::world_field!(cp, mGuardHighGroundPercentage)));
    set_int(l, c"guard_town_percentage_k", milli(crate::world_field!(cp, mGuardTownPercentage)));
    set_int(l, c"guard_town_land_percentage_k", milli(crate::world_field!(cp, mGuardTownLandPercentage)));
    set_int(l, c"guard_waterway_percentage_k", milli(crate::world_field!(cp, mGuardWaterwayPercentage)));
    set_int(l, c"hero_max_per_group", crate::world_field!(cp, mHeroMaxPerGroup) as c_long);
    set_int(l, c"hero_pop_percentage_k", milli(crate::world_field!(cp, mHeroPopPercentage)));
    set_int(l, c"hero_chance_k", milli(crate::world_field!(cp, mHeroChance)));
    set_int(l, c"num_improved_unit_types", crate::world_field!(cp, mNumImprovedUnitTypes) as c_long);
    set_int(l, c"num_improved_attributes", crate::world_field!(cp, mNumImprovedAttributes) as c_long);
    set_int(l, c"replenish_delay_min", crate::world_field!(cp, mReplenishDelayMin) as c_long);
    set_int(l, c"replenish_delay_max", crate::world_field!(cp, mReplenishDelayMax) as c_long);
    set_int(l, c"num_land_explorers", crate::world_field!(cp, mNumLandExplorers) as c_long);
    set_int(l, c"num_sea_explorers", crate::world_field!(cp, mNumSeaExplorers) as c_long);
    set_int(l, c"num_choke_points_to_guard", crate::world_field!(cp, mNumChokePointsToGuard) as c_long);
    set_int(l, c"anti_air_max", crate::world_field!(cp, mAntiAirMax) as c_long);
    set_int(l, c"airport_max", crate::world_field!(cp, mAirportMax) as c_long);
    set_int(l, c"archery_range_max", crate::world_field!(cp, mArcheryRangeMax) as c_long);
    set_int(l, c"barracks_max", crate::world_field!(cp, mBarracksMax) as c_long);
    set_int(l, c"dock_max", crate::world_field!(cp, mDockMax) as c_long);
    set_int(l, c"granary_max", crate::world_field!(cp, mGranaryMax) as c_long);
    set_int(l, c"hospital_max", crate::world_field!(cp, mHospitalMax) as c_long);
    set_int(l, c"cyber_factory_max", crate::world_field!(cp, mCyberFactoryMax) as c_long);
    set_int(l, c"cyber_lab_max", crate::world_field!(cp, mCyberLabMax) as c_long);
    set_int(l, c"naval_yard_max", crate::world_field!(cp, mNavalYardMax) as c_long);
    set_int(l, c"university_max", crate::world_field!(cp, mUniversityMax) as c_long);
    set_int(l, c"siege_factory_max", crate::world_field!(cp, mSiegeFactoryMax) as c_long);
    set_int(l, c"space_dock_max", crate::world_field!(cp, mSpaceDockMax) as c_long);
    set_int(l, c"stable_max", crate::world_field!(cp, mStableMax) as c_long);
    set_int(l, c"tank_factory_max", crate::world_field!(cp, mTankFactoryMax) as c_long);
    set_int(l, c"temple_max", crate::world_field!(cp, mTempleMax) as c_long);
    set_int(l, c"tower_max", crate::world_field!(cp, mTowerMax) as c_long);
    set_int(l, c"defensive_unit_cap_k", milli(crate::world_field!(cp, mDefensiveUnitCap)));
    set_int(l, c"siege_weapon_percentage_k", milli(crate::world_field!(cp, mSiegeWeaponPercentage)));
    set_int(l, c"max_attack_response_range_k", milli(crate::world_field!(cp, mMaxAttackResponseRange)));
    set_int(l, c"min_military_buildings_easy", crate::world_field!(cp, mMinMilitaryBuildingsEasy) as c_long);
    set_int(l, c"min_military_buildings_easy_dm", crate::world_field!(cp, mMinMilitaryBuildingsEasyDM) as c_long);
    set_int(l, c"min_military_buildings_medium", crate::world_field!(cp, mMinMilitaryBuildingsMedium) as c_long);
    set_int(l, c"min_military_buildings_medium_dm", crate::world_field!(cp, mMinMilitaryBuildingsMediumDM) as c_long);
    set_int(l, c"min_military_buildings_hard", crate::world_field!(cp, mMinMilitaryBuildingsHard) as c_long);
    set_int(l, c"min_military_buildings_hard_dm", crate::world_field!(cp, mMinMilitaryBuildingsHardDM) as c_long);
    set_int(l, c"military_pop_percentage_k", milli(crate::world_field!(cp, mMilitaryPopPercentage)));
    set_int(l, c"aid_threat_threshold_k", milli(crate::world_field!(cp, mAidThreatThreshold)));
    set_int(l, c"pop_limit_adjustment_easy", crate::world_field!(cp, mPopLimitAdjustmentEasy) as c_long);
    set_int(l, c"pop_limit_adjustment_medium", crate::world_field!(cp, mPopLimitAdjustmentMedium) as c_long);
    set_int(l, c"pop_limit_adjustment_hard", crate::world_field!(cp, mPopLimitAdjustmentHard) as c_long);
    set_int(l, c"priest_max_per_group", crate::world_field!(cp, mPriestMaxPerGroup) as c_long);
    set_int(l, c"priest_pop_percentage_k", milli(crate::world_field!(cp, mPriestPopPercentage)));
    set_int(l, c"priest_chance_k", milli(crate::world_field!(cp, mPriestChance)));
    set_int(l, c"prophet_max_per_group", crate::world_field!(cp, mProphetMaxPerGroup) as c_long);
    set_int(l, c"prophet_pop_percentage_k", milli(crate::world_field!(cp, mProphetPopPercentage)));
    set_int(l, c"prophet_chance_k", milli(crate::world_field!(cp, mProphetChance)));
    set_int(l, c"research_frequency", crate::world_field!(cp, mResearchFrequency) as c_long);
    set_int(l, c"research_frequency_modifier_easy", crate::world_field!(cp, mResearchFrequencyModifierEasy) as c_long);
    set_int(l, c"research_frequency_modifier_hard", crate::world_field!(cp, mResearchFrequencyModifierHard) as c_long);
    set_int(l, c"drop_off_distance", crate::world_field!(cp, mDropOffDistance) as c_long);
    set_int(l, c"extra_resources", crate::world_field!(cp, mExtraResources) as c_long);
    set_int(l, c"town_center_upgrade_timer", crate::world_field!(cp, mTownCenterUpgradeTimer) as c_long);
    set_int(l, c"capitol_upgrade_timer", crate::world_field!(cp, mCapitolUpgradeTimer) as c_long);
    set_int(l, c"max_enabled_managers", crate::world_field!(cp, mMaxEnabledManagers) as c_long);
    set_int(l, c"manager_attack_wait", crate::world_field!(cp, mManagerAttackWait) as c_long);
    set_int(l, c"manager_epoch_advance_wait", crate::world_field!(cp, mManagerEpochAdvanceWait) as c_long);
    set_int(l, c"enable_stock_production", crate::world_field!(cp, mEnableStockProduction) as c_long);
    set_int(l, c"enable_stock_resource", crate::world_field!(cp, mEnableStockResource) as c_long);
    set_int(l, c"enable_stock_civilization", crate::world_field!(cp, mEnableStockCivilization) as c_long);
    set_int(l, c"enable_stock_exploration", crate::world_field!(cp, mEnableStockExploration) as c_long);
    set_int(l, c"enable_stock_defense", crate::world_field!(cp, mEnableStockDefense) as c_long);
    set_int(l, c"enable_stock_build", crate::world_field!(cp, mEnableStockBuild) as c_long);
    set_int(l, c"enable_stock_5", crate::world_field!(cp, mLE80) as c_long);
    set_int(l, c"enable_stock_research", crate::world_field!(cp, mEnableStockResearch) as c_long);
    set_int(l, c"enable_stock_combat", crate::world_field!(cp, mEnableStockCombat) as c_long);
    set_int(l, c"enable_interval_k", milli(crate::world_field!(cp, mEnableInterval)));
    set_int(l, c"military_count_weight_k", milli(crate::world_field!(cp, mMilitaryCountWeight)));
    set_int(l, c"worker_count_weight_k", milli(crate::world_field!(cp, mWorkerCountWeight)));
    set_int(l, c"units_per_anti_air", crate::world_field!(cp, mUnitsPerAntiAirBuilding) as c_long);
    set_int(l, c"units_per_airport", crate::world_field!(cp, mUnitsPerAirport) as c_long);
    set_int(l, c"units_per_archery_range", crate::world_field!(cp, mUnitsPerArcheryRange) as c_long);
    set_int(l, c"units_per_barracks", crate::world_field!(cp, mUnitsPerBarracks) as c_long);
    set_int(l, c"units_per_dock", crate::world_field!(cp, mUnitsPerDock) as c_long);
    set_int(l, c"units_per_granary", crate::world_field!(cp, mUnitsPerGranary) as c_long);
    set_int(l, c"units_per_hospital", crate::world_field!(cp, mUnitsPerHospital) as c_long);
    set_int(l, c"units_per_cyber_factory", crate::world_field!(cp, mUnitsPerCyberFactory) as c_long);
    set_int(l, c"units_per_cyber_lab", crate::world_field!(cp, mUnitsPerCyberLab) as c_long);
    set_int(l, c"units_per_naval_yard", crate::world_field!(cp, mUnitsPerNavalYard) as c_long);
    set_int(l, c"units_per_university", crate::world_field!(cp, mUnitsPerUniversity) as c_long);
    set_int(l, c"units_per_siege_factory", crate::world_field!(cp, mUnitsPerSiegeFactory) as c_long);
    set_int(l, c"units_per_space_dock", crate::world_field!(cp, mUnitsPerSpaceDock) as c_long);
    set_int(l, c"units_per_stable", crate::world_field!(cp, mUnitsPerStable) as c_long);
    set_int(l, c"units_per_tank_factory", crate::world_field!(cp, mUnitsPerTankFactory) as c_long);
    set_int(l, c"units_per_temple", crate::world_field!(cp, mUnitsPerTemple) as c_long);
    set_int(l, c"units_per_tower", crate::world_field!(cp, mUnitsPerTower) as c_long);
    set_int(l, c"tower_spacing_k", milli(crate::world_field!(cp, mTowerSpacing)));
    set_int(l, c"ships_to_maintain_dm_easy", crate::world_field!(cp, mShipsToMaintainDMEasy) as c_long);
    set_int(l, c"ships_to_maintain_dm_medium", crate::world_field!(cp, mShipsToMaintainDMMedium) as c_long);
    set_int(l, c"ships_to_maintain_dm_hard", crate::world_field!(cp, mShipsToMaintainDMHard) as c_long);
    set_int(l, c"ships_to_maintain_easy", crate::world_field!(cp, mShipsToMaintainEasy) as c_long);
    set_int(l, c"ships_to_maintain_medium", crate::world_field!(cp, mShipsToMaintainMedium) as c_long);
    set_int(l, c"ships_to_maintain_hard", crate::world_field!(cp, mShipsToMaintainHard) as c_long);
    set_int(l, c"ship_maintenance_timer", crate::world_field!(cp, mShipMaintenanceTimer) as c_long);
    set_int(l, c"defensive_unit_update_easy", crate::world_field!(cp, mDefensiveUnitUpdateEasy) as c_long);
    set_int(l, c"defensive_unit_update_medium", crate::world_field!(cp, mDefensiveUnitUpdateMedium) as c_long);
    set_int(l, c"defensive_unit_update_hard", crate::world_field!(cp, mDefensiveUnitUpdateHard) as c_long);
    set_int(l, c"dm_defensive_unit_update_easy", crate::world_field!(cp, mDMDefensiveUnitUpdateEasy) as c_long);
    set_int(l, c"dm_defensive_unit_update_medium", crate::world_field!(cp, mDMDefensiveUnitUpdateMedium) as c_long);
    set_int(l, c"dm_defensive_unit_update_hard", crate::world_field!(cp, mDMDefensiveUnitUpdateHard) as c_long);
    set_int(l, c"town_guard_update_easy", crate::world_field!(cp, mTownGuardUpdateEasy) as c_long);
    set_int(l, c"town_guard_update_medium", crate::world_field!(cp, mTownGuardUpdateMedium) as c_long);
    set_int(l, c"town_guard_update_hard", crate::world_field!(cp, mTownGuardUpdateHard) as c_long);
    set_int(l, c"dm_town_guard_update_easy", crate::world_field!(cp, mDMTownGuardUpdateEasy) as c_long);
    set_int(l, c"dm_town_guard_update_medium", crate::world_field!(cp, mDMTownGuardUpdateMedium) as c_long);
    set_int(l, c"dm_town_guard_update_hard", crate::world_field!(cp, mDMTownGuardUpdateHard) as c_long);
    set_int(l, c"island_expansion_timer_easy", crate::world_field!(cp, mIslandExpansionTimerEasy) as c_long);
    set_int(l, c"island_expansion_timer_medium", crate::world_field!(cp, mIslandExpansionTimerMedium) as c_long);
    set_int(l, c"island_expansion_timer_hard", crate::world_field!(cp, mIslandExpansionTimerHard) as c_long);
    set_int(l, c"dm_attack_frequency_modifier_easy_k", milli(crate::world_field!(cp, mDMAttackFrequencyModifierEasy)));
    set_int(
        l,
        c"dm_attack_frequency_modifier_medium_k",
        milli(crate::world_field!(cp, mDMAttackFrequencyModifierMedium)),
    );
    set_int(l, c"dm_attack_frequency_modifier_hard_k", milli(crate::world_field!(cp, mDMAttackFrequencyModifierHard)));
    set_int(l, c"dm_initial_attack_modifier_easy_k", milli(crate::world_field!(cp, mDMInitialAttackModifierEasy)));
    set_int(l, c"dm_initial_attack_modifier_medium_k", milli(crate::world_field!(cp, mDMInitialAttackModifierMedium)));
    set_int(l, c"dm_initial_attack_modifier_hard_k", milli(crate::world_field!(cp, mDMInitialAttackModifierHard)));
    set_int(l, c"mining_limit", crate::world_field!(g.me.fields(), mMiningLimit) as c_long);
}

// ---------------------------------------------------------------------------------------------------------------
// The commands over the player's own state

/// `ai.improve(unit, n)`: one level of improvement n for the unit's class
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_lua_improve(l: *mut c_void) -> c_int {
    let l = l as *mut lua_State;
    let me_ptr = me_of(l);
    let s = state_of(me_ptr);
    charge(l, s, COST_COMMAND);
    let t = WorldRead::begin();
    let Some(g) = Game::new(&t, me_ptr) else { return result(l, BAD_ARGUMENT, 0, 0) };
    let id = if is_int(l, 1) { int_at(l, 1) } else { 0 };
    let unit = g.own(&*s, id);
    let Some(unit) = unit.filter(|_| is_int(l, 2) && (0..=15).contains(&lua_tointeger(l, 2))) else {
        return result(l, BAD_ARGUMENT, 0, 0);
    };
    let improvement = int_at(l, 2);
    let unit_type = unit.class().map_or(0, |c| c.unit_type()) as i64;
    if host(EE_AI_UNITS_HOST_CAN_BUY, me_ptr, unit_type, improvement as i64, 0) == 0 {
        return result(l, CANNOT_AFFORD, 0, 0);
    }
    exec(&g, MODE_BORROWED, EE_AI_UNITS_MSG_IMPROVE, &[V::I(unit.id() as i64), V::I(improvement as i64)]);
    result(l, OK, 1, 0)
}

/// `ai.gate(type, x, y)`: a gate of the class over the finished wall pieces at the tile, at once (EEBuildGate);
/// cannot_place where no wall of five finished pieces lies
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_lua_gate(l: *mut c_void) -> c_int {
    let l = l as *mut lua_State;
    let me_ptr = me_of(l);
    let s = state_of(me_ptr);
    charge(l, s, COST_COMMAND);
    let t = WorldRead::begin();
    let Some(g) = Game::new(&t, me_ptr) else { return result(l, BAD_ARGUMENT, 0, 0) };
    let named =
        lua_type(l, 1) == LUA_TNIL || (lua_type(l, 1) == LUA_TSTRING && CStr::from_ptr(lua_tostring(l, 1)) == c"gate");
    let unit_type = if named && tree(&g).is_some() {
        host(EE_AI_UNITS_HOST_GATE_CLASS, me_ptr, 0, 0, 0) as c_long
    } else {
        class_arg(l, 1)
    };
    let class =
        class_ok(&g, unit_type).filter(|c| host(EE_AI_UNITS_HOST_IS_GATE_CLASS, me_ptr, c.handle() as i64, 0, 0) != 0);
    let Some(class) = class.filter(|_| is_int(l, 2) && is_int(l, 3)) else { return result(l, BAD_ARGUMENT, 0, 0) };
    let (x, y) = (int_at(l, 2), int_at(l, 3));
    if !g.world.map().is_some_and(|m| m.tile(x as i64, y as i64).is_some()) {
        return result(l, BAD_ARGUMENT, 0, 0);
    }
    let tech = class.technology();
    (*s).ids2.clear();
    if tree(&g).and_then(|tr| tr.node(tech)).is_none() {
        trace_cmd(&g, &*s, "gate", NOT_IN_EPOCH, 0, unit_type, 0);
        return result(l, NOT_IN_EPOCH, 0, 0);
    }
    let side = host(EE_AI_UNITS_HOST_GATE_SIDE, me_ptr, x as i64, y as i64, 0);
    if side == 0 {
        trace_cmd(&g, &*s, "gate", CANNOT_PLACE, 0, unit_type, x * 10000 + y);
        return result(l, CANNOT_PLACE, 0, 0);
    }
    let me_id = g.me.id() as i64;
    exec(&g, 1, EE_AI_UNITS_MSG_BUILD_GATE, &[V::P(x as f32, y as f32), V::I(tech), V::I(side), V::I(me_id)]);
    trace_cmd(&g, &*s, "gate", OK, 1, unit_type, x * 10000 + y);
    result(l, OK, 1, 0)
}

fn resource_of(g: &Game<'_>, k: c_long) -> c_long {
    let caps: [c_long; 7] = crate::world_field!(g.me.saved(), mResourceCaps);
    caps[k as usize]
}

/// The other player argument: in the game, not the player itself
unsafe fn other_player(l: *mut lua_State, g: &Game<'_>) -> c_long {
    let other = if is_int(l, 1) { int_at(l, 1) } else { -1 };
    if other <= 0 || other as u64 >= g.world.num_players() {
        return -1;
    }
    match g.world.player(other as usize) {
        Some(p) if p.handle() != g.me.handle() => other,
        _ => -1,
    }
}

/// `ai.tribute(player, {food=, wood=, stone=, gold=, iron=})`: the resources given as a human's tribute
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_lua_tribute(l: *mut c_void) -> c_int {
    let l = l as *mut lua_State;
    let me_ptr = me_of(l);
    let s = state_of(me_ptr);
    charge(l, s, COST_COMMAND);
    (*s).ids2.clear();
    let t = WorldRead::begin();
    let Some(g) = Game::new(&t, me_ptr) else { return result(l, BAD_ARGUMENT, 0, 0) };
    let other = other_player(l, &g);
    if other < 0 || lua_type(l, 2) != LUA_TTABLE {
        return result(l, BAD_ARGUMENT, 0, 0);
    }
    let mut amount = [0 as c_long; 5];
    let mut total: c_long = 0;
    for k in 0..5 {
        amount[k] = crate::ai::units_lua::ee_ai_units_opt_amount(l.cast(), 2, RESOURCE_NAMES[k].as_ptr());
        if amount[k] > resource_of(&g, RESOURCE_FOOD as c_long + k as c_long) {
            trace_cmd(&g, &*s, "tribute", CANNOT_AFFORD, 0, other, amount[k]);
            return result(l, CANNOT_AFFORD, 0, 0);
        }
        total = total.wrapping_add(amount[k]);
    }
    if total <= 0 {
        return result(l, BAD_ARGUMENT, 0, 0);
    }
    for (k, &a) in amount.iter().enumerate() {
        if a > 0 {
            host(EE_AI_UNITS_HOST_TRIBUTE, me_ptr, other as i64, RESOURCE_FOOD as i64 + k as i64, a as i64);
        }
    }
    trace_cmd(&g, &*s, "tribute", OK, 1, other, total);
    result(l, OK, 1, 0)
}

/// `ai.trade(resource, "buy" or "sell", n)`: n lots of 100 bought with gold or sold for it (1 or 5 a call)
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_lua_trade(l: *mut c_void) -> c_int {
    let l = l as *mut lua_State;
    let me_ptr = me_of(l);
    let s = state_of(me_ptr);
    charge(l, s, COST_COMMAND);
    (*s).ids2.clear();
    let t = WorldRead::begin();
    let Some(g) = Game::new(&t, me_ptr) else { return result(l, BAD_ARGUMENT, 0, 0) };
    let mut resource: c_long = -1;
    if lua_type(l, 1) == LUA_TSTRING {
        resource = match CStr::from_ptr(lua_tostring(l, 1)).to_bytes() {
            b"food" => 2,
            b"wood" => 3,
            b"stone" => 4,
            b"iron" => 6,
            _ => -1,
        };
    }
    let way: &[u8] = if lua_type(l, 2) == LUA_TSTRING { CStr::from_ptr(lua_tostring(l, 2)).to_bytes() } else { b"" };
    let sell = way == b"sell";
    let n = if is_int(l, 3) { int_at(l, 3) } else { 1 };
    if resource < 0 || (!sell && way != b"buy") || (n != 1 && n != 5) {
        return result(l, BAD_ARGUMENT, 0, 0);
    }
    let stock = if sell { resource_of(&g, resource) } else { resource_of(&g, RESOURCE_GOLD) };
    if stock <= 0 {
        trace_cmd(&g, &*s, "trade", CANNOT_AFFORD, 0, resource, n);
        return result(l, CANNOT_AFFORD, 0, 0);
    }
    exec(
        &g,
        MODE_BORROWED_CHECKED,
        EE_AI_UNITS_MSG_EXCHANGE,
        &[V::I(sell as i64), V::I(resource as i64), V::I((n == 5) as i64)],
    );
    trace_cmd(&g, &*s, "trade", OK, n, resource, sell as c_long);
    result(l, OK, n, 0)
}

/// `ai.diplomacy(player, "ally" or "enemy")`: the stance towards another player
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_lua_diplomacy(l: *mut c_void) -> c_int {
    let l = l as *mut lua_State;
    let me_ptr = me_of(l);
    let s = state_of(me_ptr);
    charge(l, s, COST_COMMAND);
    (*s).ids2.clear();
    let t = WorldRead::begin();
    let Some(g) = Game::new(&t, me_ptr) else { return result(l, BAD_ARGUMENT, 0, 0) };
    let other = other_player(l, &g);
    let stance: &[u8] = if lua_type(l, 2) == LUA_TSTRING { CStr::from_ptr(lua_tostring(l, 2)).to_bytes() } else { b"" };
    let ally = stance == b"ally";
    if other < 0 || (!ally && stance != b"enemy") {
        return result(l, BAD_ARGUMENT, 0, 0);
    }
    let v = if ally { 0 } else { 1 };
    host(EE_AI_UNITS_HOST_STANCE, me_ptr, other as i64, v, 0);
    trace_cmd(&g, &*s, "diplomacy", OK, 1, other, v as c_long);
    result(l, OK, 1, 0)
}

/// `ai.garrison_fill(id, n)` -> ok: an own finished building's garrison raised by n with no citizen going in (only
/// for an AI whose manifest entry declares "fills_garrison")
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_lua_garrison_fill(l: *mut c_void) -> c_int {
    let l = l as *mut lua_State;
    let me_ptr = me_of(l);
    let s = state_of(me_ptr);
    charge(l, s, COST_COMMAND);
    if host(EE_AI_UNITS_HOST_FILLS_GARRISON, me_ptr, 0, 0, 0) == 0 || !is_int(l, 1) || !is_int(l, 2) {
        lua_pushboolean(l, 0);
        return 1;
    }
    let t = WorldRead::begin();
    let Some(g) = Game::new(&t, me_ptr) else {
        lua_pushboolean(l, 0);
        return 1;
    };
    let c = g.own(&*s, int_at(l, 1));
    let n = int_at(l, 2);
    let mut done = false;
    if let Some(c) = c {
        let capacity: c_long = c.class().map_or(0, |k| crate::world_field!(k.fields(), mGarrisonCapacity));
        if n > 0 && c.is_built() && c.hit_points() > 0 && capacity > 0 {
            done = host(EE_AI_UNITS_HOST_GARRISON_FILL, me_ptr, c.handle() as i64, n as i64, 0) != 0;
        }
    }
    trace(me_ptr, c"next.garrison_fill", &format!("id={} n={} done={}", int_at(l, 1), n, done as i32));
    lua_pushboolean(l, done as c_int);
    1
}

/// TEST ONLY (EE_AI_TEST_SPAWN=1): `ai.test_spawn(type, x, y, player)` -> true: the cheat's unit (EEMHackANewUnit)
/// is made. Off: false
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_lua_test_spawn(l: *mut c_void) -> c_int {
    let l = l as *mut lua_State;
    let on = test_spawn_on();
    let me_ptr = me_of(l);
    let s = state_of(me_ptr);
    charge(l, s, COST_COMMAND);
    let unit_type = class_arg(l, 1);
    let t = WorldRead::begin();
    let Some(g) = Game::new(&t, me_ptr) else {
        lua_pushboolean(l, 0);
        return 1;
    };
    if !on
        || unit_type < 0
        || unit_type as usize >= ClassView::count(&t)
        || !is_int(l, 2)
        || !is_int(l, 3)
        || !tile_ok(&g, int_at(l, 2), int_at(l, 3))
    {
        lua_pushboolean(l, 0);
        return 1;
    }
    let player = if is_int(l, 4) { int_at(l, 4) } else { g.me.id() as c_long };
    if player < 0 || player as u64 >= g.world.num_players() {
        lua_pushboolean(l, 0);
        return 1;
    }
    let (x, y) = (int_at(l, 2), int_at(l, 3));
    exec(
        &g,
        MODE_BORROWED,
        EE_AI_UNITS_MSG_HACK_NEW_UNIT,
        &[V::I(unit_type as i64), V::P(x as f32, y as f32), V::I(player as u8 as i64)],
    );
    lua_pushboolean(l, 1);
    1
}

// ---------------------------------------------------------------------------------------------------------------
// The view's safety tests and the searches

/// `ai.can_place(type, x, y)` -> whether the class can be placed there now
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_lua_can_place(l: *mut c_void) -> c_int {
    let l = l as *mut lua_State;
    let me_ptr = me_of(l);
    let s = state_of(me_ptr);
    charge(l, s, COST_COMMAND);
    let unit_type = class_arg(l, 1);
    if unit_type < 0 || !is_int(l, 2) || !is_int(l, 3) {
        lua_pushboolean(l, 0);
        return 1;
    }
    let ok = host(EE_AI_UNITS_HOST_CAN_PLACE, me_ptr, unit_type as i64, int_at(l, 2) as i64, int_at(l, 3) as i64);
    lua_pushboolean(l, (ok != 0) as c_int);
    1
}

/// `ai.enemy_near(x, y)` -> whether armed enemy units stand near the tile: the seat's threat memory, or with
/// "sees_all" the classic's influence regions through the fog
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_lua_enemy_near(l: *mut c_void) -> c_int {
    let l = l as *mut lua_State;
    let me_ptr = me_of(l);
    let s = state_of(me_ptr);
    charge(l, s, COST_CALL + 20 * COST_SCAN);
    let has_map = {
        let t = WorldRead::begin();
        WorldView::global(&t).is_some_and(|w| w.map().is_some())
    };
    if !is_int(l, 1) || !is_int(l, 2) || !has_map {
        lua_pushboolean(l, 0);
        return 1;
    }
    let (x, y) = (int_at(l, 1) as i64, int_at(l, 2) as i64);
    let near = if host(EE_AI_UNITS_HOST_SEES_ALL, me_ptr, 0, 0, 0) == 0 {
        host(EE_AI_UNITS_HOST_SITE_THREAT, me_ptr, x, y, 0) != 0
    } else {
        host(EE_AI_UNITS_HOST_ENEMY_NEAR_ALL, me_ptr, x, y, 0) > 0
    };
    lua_pushboolean(l, near as c_int);
    1
}

/// `ai.site_blocked(resource)` -> nil when gatherers may go to the resource, else why not
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_lua_site_blocked(l: *mut c_void) -> c_int {
    let l = l as *mut lua_State;
    let me_ptr = me_of(l);
    let s = state_of(me_ptr);
    charge(l, s, COST_CALL + 400);
    let handle = if is_int(l, 1) { unit_handle(lua_tointeger(l, 1) as c_long) } else { 0 };
    let usable = {
        let t = WorldRead::begin();
        UnitView::of(&t, handle).is_some_and(|u| u.valid() && u.tile().is_some())
    };
    if !usable {
        lua_pushstring(l, c"unexplored".as_ptr());
        return 1;
    }
    let kind = host(EE_AI_UNITS_HOST_SITE_BLOCKED, me_ptr, handle as i64, 0, 0);
    if kind <= 0 || kind >= 6 {
        return 0;
    }
    lua_pushstring(l, BLOCKED[kind as usize].as_ptr());
    1
}

/// `ai.build_unsafe(x, y)` -> nil when builders may go to the tile, else why not
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_lua_build_unsafe(l: *mut c_void) -> c_int {
    let l = l as *mut lua_State;
    let me_ptr = me_of(l);
    let s = state_of(me_ptr);
    charge(l, s, COST_CALL + 400);
    if !is_int(l, 1) || !is_int(l, 2) {
        return 0;
    }
    let kind = host(EE_AI_UNITS_HOST_BUILD_UNSAFE, me_ptr, int_at(l, 1) as i64, int_at(l, 2) as i64, 0);
    if kind <= 0 || kind >= 6 {
        return 0;
    }
    lua_pushstring(l, BLOCKED[kind as usize].as_ptr());
    1
}

/// `ai.place(type, mode, {x =, y =, first =, last =, units = {ids}, site = index})` -> x, y, gap: a building site
/// by one of build()'s rules; nil: none
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_lua_place(l: *mut c_void) -> c_int {
    let l = l as *mut lua_State;
    let me_ptr = me_of(l);
    let s = state_of(me_ptr);
    let unit_type = class_arg(l, 1);
    let mode: &[u8] = if lua_type(l, 2) == LUA_TSTRING { CStr::from_ptr(lua_tostring(l, 2)).to_bytes() } else { b"" };
    const MODES: [&[u8]; 6] = [b"ring", b"tower", b"adjacent", b"centre", b"classic", b"dock"];
    let kind = MODES.iter().position(|m| *m == mode).map_or(-1, |k| k as c_long);
    let (mut x, mut y, mut first, mut last, mut site): (c_long, c_long, c_long, c_long, c_long) = (-1, -1, 0, 6, 0);
    opt_int(l, 3, c"x", &mut x);
    opt_int(l, 3, c"y", &mut y);
    opt_int(l, 3, c"first", &mut first);
    opt_int(l, 3, c"last", &mut last);
    opt_int(l, 3, c"site", &mut site);
    (*s).ids.clear();
    if lua_type(l, 3) == LUA_TTABLE {
        lua_getfield(l, 3, c"units".as_ptr());
        if lua_type(l, -1) == LUA_TTABLE {
            read_ids(l, lua_gettop(l), &mut (*s).ids);
        }
        lua_pop(l, 1);
    }
    let first = first.max(0);
    let last = last.min(30);
    if kind < 0 || unit_type < 0 {
        charge(l, s, COST_CALL);
        return 0;
    }
    // classic, centre, dock: 30 rings
    let rings: i64 = if kind == 4 || kind == 3 || kind == 5 { 30 } else { last as i64 };
    charge(l, s, COST_CALL + (2 * rings + 1) * (2 * rings + 1) * 6 + (*s).ids.len() as i64 * 50);
    let mut p = EeAiUnitsPlace {
        unit_type,
        mode: kind,
        x,
        y,
        first,
        last,
        site: site - 1,
        units: (*s).ids.as_ptr(),
        units_len: (*s).ids.len(),
        out_x: 0,
        out_y: 0,
        out_gap: 0,
    };
    if host_out(EE_AI_UNITS_HOST_PLACE, me_ptr, 0, (&mut p as *mut EeAiUnitsPlace).cast()) == 0 {
        return 0;
    }
    lua_pushinteger(l, p.out_x as lua_Integer);
    lua_pushinteger(l, p.out_y as lua_Integer);
    lua_pushinteger(l, p.out_gap as lua_Integer);
    3
}

/// `ai.used()` -> the instructions and charges this call has used so far (ee_ai lua_host.rs)
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_lua_used(l: *mut c_void) -> c_int {
    crate::ai::lua_ffi::ee_ai_lua_used_binding(l)
}

// ---------------------------------------------------------------------------------------------------------------
// The continent manager's areas and the walls (the classic's continents, forests, hills, choke points; the wall
// planner's utilities and the path manager's requests). The continent manager's objects are reached only through
// these host ops: lane C re-points them when the continents move.

/// `gContinentManager.GetContinentAt(a, b)` (an `EEAIContinent*`; 0: none).
pub const EE_AI_UNITS_HOST_AREA_AT: i32 = 80;
/// `GetContinentByID(a)` for `0 < a < mContinents.size()` (0: none).
pub const EE_AI_UNITS_HOST_AREA_BY_ID: i32 = 81;
/// The area `a`'s id, category and tile count (`EeAiUnitsArea` into `out`).
pub const EE_AI_UNITS_HOST_AREA_INFO: i32 = 82;
/// A random tile of the area `a` from a stream seeded `(unsigned long)b` (`EeAiUnitsArea`'s x, y) -> 1, or 0 for an
/// empty area.
pub const EE_AI_UNITS_HOST_AREA_POINT: i32 = 83;
/// A forest's, a hill's or a choke point's random point (`EeAiUnitsAreaPick` in and out).
pub const EE_AI_UNITS_HOST_AREA_PICK: i32 = 84;
/// The shore tiles of the largest water body next to the land mass at (a, b) into `out` (a `EeAiUnitsTiles`) ->
/// their count, -1 when the water body does not touch it.
pub const EE_AI_UNITS_HOST_WATER_SHORE: i32 = 85;
/// `EEGetWallLine(from, to)` into `out` (a `EeAiUnitsTiles`) -> the count.
pub const EE_AI_UNITS_HOST_WALL_LINE: i32 = 86;
/// The wall line `out` (a `EeAiUnitsTiles`) is blocked by a land building (IsPathBlocked).
pub const EE_AI_UNITS_HOST_WALL_BLOCKED: i32 = 87;
/// `gContinentManager.GetForestAt((a, b)) != NULL`.
pub const EE_AI_UNITS_HOST_FOREST_AT: i32 = 88;
/// `EEAreTilesLevel` of the five tiles `out` (a `EeAiUnitsTiles`).
pub const EE_AI_UNITS_HOST_TILES_LEVEL: i32 = 89;
/// The own unit `a`'s wall path from `from` to `to` (`EeAiUnitsTiles` in `out`) -> the count.
pub const EE_AI_UNITS_HOST_WALL_PATH: i32 = 90;
/// A wall route's request for the enemy `a` from `from` to `to` (`EeAiUnitsTiles` in `out`; the old request
/// dropped) -> its handle (0: none).
pub const EE_AI_UNITS_HOST_ROUTE_REQUEST: i32 = 91;
/// The route `a`: -2 not held, -1 running, else its path's length.
pub const EE_AI_UNITS_HOST_ROUTE_STATE: i32 = 92;
/// The route `a`'s path into `out` (a `EeAiUnitsTiles`) -> its length; then the route is dropped.
pub const EE_AI_UNITS_HOST_ROUTE_TAKE: i32 = 93;

/// The list the last op handed out (it did not fit) again into `out` (a `EeAiUnitsTiles`) -> its length: no list is
/// computed twice (the shore's GetTilesNextTo and FindPath are captured).
pub const EE_AI_UNITS_HOST_TILES_AGAIN: i32 = 94;

/// An area's facts (`EE_AI_UNITS_HOST_AREA_INFO`, `AREA_POINT`).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct EeAiUnitsArea {
    pub id: c_long,
    pub category: c_long,
    pub tiles: c_long,
    pub x: c_long,
    pub y: c_long,
}

/// `EE_AI_UNITS_HOST_AREA_PICK`: `kind` 0 the land's forests, 1 its hills (those whose hull holds (x, y) or comes
/// within r tiles), 2 its choke points (the `r` nearest when r is above 0); the (pick mod count)-th one's random
/// point
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct EeAiUnitsAreaPick {
    pub kind: c_long,
    pub land: u64,
    pub x: c_long,
    pub y: c_long,
    pub r: c_long,
    pub pick: i64,
    pub seed: i64,
    /// out: the land's ids looked at, the ones picked from, whether the picked one has tiles, its point
    pub total: c_long,
    pub found: c_long,
    pub ok: bool,
    pub px: c_long,
    pub py: c_long,
}

/// A tile list the host fills or reads: `len` (x, y) pairs at `xy` (room for `cap`); `from`, `to` the ends
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeAiUnitsTiles {
    pub xy: *mut c_long,
    pub len: usize,
    pub cap: usize,
    pub from: [c_long; 2],
    pub to: [c_long; 2],
}

fn area_at(x: c_long, y: c_long) -> u64 {
    host(EE_AI_UNITS_HOST_AREA_AT, std::ptr::null_mut(), x as i64, y as i64, 0) as u64
}

fn area_info(a: u64) -> EeAiUnitsArea {
    let mut r = EeAiUnitsArea::default();
    host_out(EE_AI_UNITS_HOST_AREA_INFO, std::ptr::null_mut(), a as i64, (&mut r as *mut EeAiUnitsArea).cast());
    r
}

/// The host fills `s.tiles` (the op with `a` and the list's ends) -> the count it has (-1: none). A list longer
/// than the room is read again from the host's copy (`EE_AI_UNITS_HOST_TILES_AGAIN`), never computed twice.
unsafe fn fill_tiles(op: i32, me: *mut c_void, s: *mut UnitsState, a: i64, ends: [[c_long; 2]; 2]) -> i64 {
    let v = &mut (*s).tiles;
    let cap = v.capacity().max(256);
    v.resize(cap, (0, 0));
    let mut t = EeAiUnitsTiles { xy: v.as_mut_ptr().cast(), len: 0, cap, from: ends[0], to: ends[1] };
    let n = ee_ai_units_host(op, me, a, 0, 0, (&mut t as *mut EeAiUnitsTiles).cast());
    if n < 0 {
        v.clear();
        return n;
    }
    if n as usize > cap {
        let cap = n as usize;
        v.resize(cap, (0, 0));
        let mut t = EeAiUnitsTiles { xy: v.as_mut_ptr().cast(), len: 0, cap, from: ends[0], to: ends[1] };
        ee_ai_units_host(EE_AI_UNITS_HOST_TILES_AGAIN, me, 0, 0, 0, (&mut t as *mut EeAiUnitsTiles).cast());
    }
    v.truncate(n as usize);
    n
}

/// The host reads `tiles` (the op with `a`) -> its answer
unsafe fn read_tiles(op: i32, me: *mut c_void, tiles: &mut [(c_long, c_long)], a: i64) -> i64 {
    let mut t =
        EeAiUnitsTiles { xy: tiles.as_mut_ptr().cast(), len: tiles.len(), cap: tiles.len(), from: [0; 2], to: [0; 2] };
    ee_ai_units_host(op, me, a, 0, 0, (&mut t as *mut EeAiUnitsTiles).cast())
}

/// `{{x, y}, ...}` of `s.tiles` into the table on top
unsafe fn push_tiles(l: *mut lua_State, s: *mut UnitsState) {
    let n = (*s).tiles.len();
    for k in 0..n {
        let (x, y) = (&(*s).tiles)[k];
        lua_createtable(l, 2, 0);
        lua_pushinteger(l, x as lua_Integer);
        lua_rawseti(l, -2, 1);
        lua_pushinteger(l, y as lua_Integer);
        lua_rawseti(l, -2, 2);
        lua_rawseti(l, -2, k as lua_Integer + 1);
    }
}

fn in_map(x: c_long, y: c_long) -> bool {
    let (w, h) = map_size();
    x >= 0 && y >= 0 && x < w && y < h
}

fn has_map() -> bool {
    // SAFETY: inside a binding (the simulation thread)
    let t = unsafe { WorldRead::begin() };
    WorldView::global(&t).is_some_and(|w| w.map().is_some())
}

/// `ai.water_sites(x, y, {max = 64})` -> {{x, y}, ...}: the water tiles by the shore for docks, nearest first: the
/// largest water body's shore next to the land mass at (x, y) and the tiles round it, explored ones only
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_lua_water_sites(l: *mut c_void) -> c_int {
    let l = l as *mut lua_State;
    let me_ptr = me_of(l);
    let s = state_of(me_ptr);
    lua_createtable(l, 0, 0);
    if !is_int(l, 1) || !is_int(l, 2) || !has_map() {
        return 1;
    }
    let (x, y) = (int_at(l, 1), int_at(l, 2));
    let mut max: c_long = 64;
    opt_int(l, 3, c"max", &mut max);
    let max = max.clamp(1, 512);
    charge(l, s, COST_CALL);
    let ends = [[x, y], [0, 0]];
    let shore = fill_tiles(EE_AI_UNITS_HOST_WATER_SHORE, me_ptr, s, 0, ends);
    if shore < 0 {
        return 1;
    }
    charge(l, s, shore * 9 * COST_SCAN);
    // the shore tiles and the tiles round them, once each (their order goes: the sites are sorted below)
    let n = (*s).tiles.len();
    for k in 0..n {
        let (sx, sy) = (&(*s).tiles)[k];
        for dy in -1..=1 {
            for dx in -1..=1 {
                (*s).tiles.push((sx + dx, sy + dy));
            }
        }
    }
    (*s).tiles.drain(..n);
    (*s).tiles.sort_unstable();
    (*s).tiles.dedup();
    let (w, h) = map_size();
    (*s).sites.clear();
    {
        let t = WorldRead::begin();
        let Some(g) = Game::new(&t, me_ptr) else { return 1 };
        let Some(map) = g.world.map() else { return 1 };
        for &(tx, ty) in &(*s).tiles {
            if tx < 0 || ty < 0 || tx >= w || ty >= h {
                continue;
            }
            let Some(tile) = map.tile(tx as i64, ty as i64) else { continue };
            // explored, deep or shallow water
            if !g.tile_explored(&tile) || !matches!(tile.terrain_type(), 1 | 10) {
                continue;
            }
            (*s).sites.push((d2(tx, ty, x, y), ty, tx));
        }
    }
    (*s).sites.sort_unstable(); // the distance, then the tile: the same order on every machine
    let mut pushed: c_long = 0;
    let count = (*s).sites.len();
    for k in 0..count {
        if pushed >= max {
            break;
        }
        let (_, ty, tx) = (&(*s).sites)[k];
        lua_createtable(l, 2, 0);
        lua_pushinteger(l, tx as lua_Integer);
        lua_rawseti(l, -2, 1);
        lua_pushinteger(l, ty as lua_Integer);
        lua_rawseti(l, -2, 2);
        pushed += 1;
        lua_rawseti(l, -2, pushed as lua_Integer);
    }
    charge(l, s, pushed as i64 * COST_FIELD * 3);
    1
}

/// `ai.area(id)` or `ai.area(x, y)` -> {id, category = 1 land / 2 water / 3 cliff / 4 space / 5 map edge, tiles}; nil
/// for none
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_lua_area(l: *mut c_void) -> c_int {
    let l = l as *mut lua_State;
    let me_ptr = me_of(l);
    let s = state_of(me_ptr);
    charge(l, s, COST_CALL);
    let mut area = 0u64;
    if is_int(l, 1) && is_int(l, 2) {
        let (x, y) = (int_at(l, 1), int_at(l, 2));
        if in_map(x, y) {
            area = area_at(x, y);
        }
    } else if is_int(l, 1) {
        let id = lua_tointeger(l, 1);
        if id > 0 {
            area = host(EE_AI_UNITS_HOST_AREA_BY_ID, std::ptr::null_mut(), id, 0, 0) as u64;
        }
    }
    if area == 0 {
        return 0;
    }
    let a = area_info(area);
    lua_createtable(l, 0, 3);
    set_int(l, c"id", a.id);
    set_int(l, c"category", a.category);
    set_int(l, c"tiles", a.tiles);
    1
}

/// The forest, hill and choke point bindings: (x, y, r or nearest, pick, seed) -> x, y, n
unsafe fn area_pick(l: *mut c_void, kind: c_long) -> c_int {
    let l = l as *mut lua_State;
    let me_ptr = me_of(l);
    let s = state_of(me_ptr);
    charge(l, s, COST_CALL);
    if !(1..=5).all(|i| is_int(l, i)) {
        return 0;
    }
    let (x, y, r) = (int_at(l, 1), int_at(l, 2), int_at(l, 3));
    if !in_map(x, y) {
        return 0;
    }
    let land = area_at(x, y);
    if land == 0 {
        return 0;
    }
    let mut p = EeAiUnitsAreaPick {
        kind,
        land,
        x,
        y,
        r,
        pick: lua_tointeger(l, 4),
        seed: lua_tointeger(l, 5),
        ..Default::default()
    };
    host_out(EE_AI_UNITS_HOST_AREA_PICK, me_ptr, 0, (&mut p as *mut EeAiUnitsAreaPick).cast());
    charge(l, s, p.total as i64 * COST_SCAN * 4);
    if p.found == 0 || !p.ok {
        return 0;
    }
    lua_pushinteger(l, p.px as lua_Integer);
    lua_pushinteger(l, p.py as lua_Integer);
    lua_pushinteger(l, if kind == 2 { p.total } else { p.found } as lua_Integer);
    3
}

/// `ai.forest_point(x, y, r, pick, seed)` -> x, y, n: a random point of one of the land's forests near the tile
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_lua_forest_point(l: *mut c_void) -> c_int {
    area_pick(l, 0)
}

/// `ai.hill_point(x, y, r, pick, seed)` -> x, y, n: as ai.forest_point over the land's hills
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_lua_hill_point(l: *mut c_void) -> c_int {
    area_pick(l, 1)
}

/// `ai.choke_point(x, y, nearest, pick, seed)` -> x, y, n: a random point of one of the land's choke points (of the
/// `nearest` nearest when above 0); n all of them
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_lua_choke_point(l: *mut c_void) -> c_int {
    area_pick(l, 2)
}

/// `ai.area_point(id, seed)` -> x, y: a random tile of the area; nil for none or an empty one
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_lua_area_point(l: *mut c_void) -> c_int {
    let l = l as *mut lua_State;
    let me_ptr = me_of(l);
    let s = state_of(me_ptr);
    charge(l, s, COST_CALL);
    if !is_int(l, 1) || !is_int(l, 2) {
        return 0;
    }
    let id = lua_tointeger(l, 1);
    if id <= 0 {
        return 0;
    }
    let area = host(EE_AI_UNITS_HOST_AREA_BY_ID, std::ptr::null_mut(), id, 0, 0);
    if area == 0 {
        return 0;
    }
    let mut a = EeAiUnitsArea::default();
    let seed = lua_tointeger(l, 2);
    if ee_ai_units_host(EE_AI_UNITS_HOST_AREA_POINT, me_ptr, area, seed, 0, (&mut a as *mut EeAiUnitsArea).cast()) == 0
    {
        return 0;
    }
    lua_pushinteger(l, a.x as lua_Integer);
    lua_pushinteger(l, a.y as lua_Integer);
    2
}

/// `ai.unexplored(x, y, {land = id, sea = false, grid = 8, away = {ax, ay}, skip = {{x, y}, ...}})` -> x, y: the
/// unexplored grid point on the land mass nearest (x, y), scored by its distance less half its distance from away;
/// points within grid of a skip entry left out
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_lua_unexplored(l: *mut c_void) -> c_int {
    let l = l as *mut lua_State;
    let me_ptr = me_of(l);
    let s = state_of(me_ptr);
    if !is_int(l, 1) || !is_int(l, 2) || !has_map() {
        return 0;
    }
    let (x, y) = (int_at(l, 1), int_at(l, 2));
    let (mut land, mut grid, mut ax, mut ay): (c_long, c_long, c_long, c_long) = (0, 8, x, y);
    opt_int(l, 3, c"land", &mut land);
    opt_int(l, 3, c"grid", &mut grid);
    let sea = opt_bool(l, 3, c"sea", false); // over a water body
    let grid = grid.clamp(4, 32);
    (*s).ids2.clear();
    if lua_type(l, 3) == LUA_TTABLE {
        lua_getfield(l, 3, c"away".as_ptr());
        if lua_type(l, -1) == LUA_TTABLE {
            lua_rawgeti(l, -1, 1);
            lua_rawgeti(l, -2, 2);
            if is_int(l, -2) && is_int(l, -1) {
                ax = int_at(l, -2);
                ay = int_at(l, -1);
            }
            lua_pop(l, 2);
        }
        lua_pop(l, 1);
        lua_getfield(l, 3, c"skip".as_ptr());
        if lua_type(l, -1) == LUA_TTABLE {
            let n = lua_rawlen(l, -1) as lua_Integer;
            let mut i: lua_Integer = 1;
            while i <= n && i <= 128 {
                lua_rawgeti(l, -1, i);
                if lua_type(l, -1) == LUA_TTABLE {
                    lua_rawgeti(l, -1, 1);
                    lua_rawgeti(l, -2, 2);
                    if is_int(l, -2) && is_int(l, -1) {
                        (*s).ids2.push(int_at(l, -2));
                        (*s).ids2.push(int_at(l, -1));
                    }
                    lua_pop(l, 2);
                }
                lua_pop(l, 1);
                i += 1;
            }
        }
        lua_pop(l, 1);
    }
    if land == 0 {
        land = area_near(x, y, sea) as c_long;
    }
    let (w, h) = map_size();
    charge(l, s, COST_CALL + ((w / grid + 1) * (h / grid + 1) * 3) as i64);
    let t = WorldRead::begin();
    let Some(g) = Game::new(&t, me_ptr) else { return 0 };
    let Some(map) = g.world.map() else { return 0 };
    let me_id = g.me.id() as usize;
    let mut best: Option<(c_long, c_long, c_long)> = None;
    let mut gy = grid / 2;
    while gy < h {
        let mut gx = grid / 2;
        while gx < w {
            let fresh = map.tile(gx as i64, gy as i64).is_some_and(|tile| tile.visibility(me_id) == 0);
            let skip = !fresh
                || continent(gx, gy, sea) as c_long != land
                || (*s).ids2.as_chunks::<2>().0.iter().any(|p| d2(gx, gy, p[0], p[1]) <= grid * grid);
            if !skip {
                let score = isqrt_up(d2(gx, gy, x, y)) - isqrt_up(d2(gx, gy, ax, ay)) / 2;
                if best.is_none_or(|b| score < b.0) {
                    best = Some((score, gx, gy));
                }
            }
            gx += grid;
        }
        gy += grid;
    }
    let Some((_, bx, by)) = best else { return 0 };
    lua_pushinteger(l, bx as lua_Integer);
    lua_pushinteger(l, by as lua_Integer);
    2
}

/// `ai.wall_line(x1, y1, x2, y2)` -> {{x, y}, ...}: the tiles a wall between the two lays (EEGetWallLine), in order
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_lua_wall_line(l: *mut c_void) -> c_int {
    let l = l as *mut lua_State;
    let me_ptr = me_of(l);
    let s = state_of(me_ptr);
    lua_createtable(l, 0, 0);
    if !(1..=4).all(|i| is_int(l, i)) {
        return 1;
    }
    let ends = [[int_at(l, 1), int_at(l, 2)], [int_at(l, 3), int_at(l, 4)]];
    let n = fill_tiles(EE_AI_UNITS_HOST_WALL_LINE, me_ptr, s, 0, ends);
    charge(l, s, COST_CALL + n * COST_FIELD * 2);
    push_tiles(l, s);
    1
}

/// `ai.wall_stretches(x1, y1, x2, y2)` -> blocked, {{ax, ay, bx, by, len, level}, ...}: the classic wall planner's
/// look at one wall line (a land building on it or within 2 tiles; the open runs that follow a blocked tile, and
/// whether five level tiles in a row lie on each)
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_lua_wall_stretches(l: *mut c_void) -> c_int {
    let l = l as *mut lua_State;
    let me_ptr = me_of(l);
    let s = state_of(me_ptr);
    if !(1..=4).all(|i| is_int(l, i)) || !has_map() {
        lua_pushboolean(l, 1);
        lua_createtable(l, 0, 0);
        return 2;
    }
    let ends = [[int_at(l, 1), int_at(l, 2)], [int_at(l, 3), int_at(l, 4)]];
    let n = fill_tiles(EE_AI_UNITS_HOST_WALL_LINE, me_ptr, s, 0, ends);
    charge(l, s, COST_CALL + n * 25 * COST_SCAN);
    let blocked = read_tiles(EE_AI_UNITS_HOST_WALL_BLOCKED, me_ptr, &mut (*s).tiles, 0) != 0;
    lua_pushboolean(l, blocked as c_int);
    lua_createtable(l, 0, 0);
    if blocked {
        return 2;
    }
    // the stretches between the blocks: (first, last) indexes into the line, in `sites`
    (*s).sites.clear();
    let (mut in_run, mut seen_block, mut run_start) = (false, false, 0usize);
    {
        let t = WorldRead::begin();
        let map = WorldView::global(&t).and_then(|w| w.map());
        for j in 0..(*s).tiles.len() {
            let (tx, ty) = (&(*s).tiles)[j];
            // off the map, a forest, or not on land (water, a cliff, space)
            let block = if !map.as_ref().is_some_and(|m| m.tile(tx as i64, ty as i64).is_some())
                || host(EE_AI_UNITS_HOST_FOREST_AT, me_ptr, tx as i64, ty as i64, 0) != 0
            {
                true
            } else {
                let a = area_at(tx, ty);
                let kind = if a != 0 { area_info(a).category } else { 2 };
                matches!(kind, 2..=4) // water, cliff, space
            };
            if block {
                if in_run {
                    in_run = false;
                    (*s).sites.push((run_start as c_long, j as c_long - 1, 0));
                }
                seen_block = true;
            } else if seen_block && !in_run {
                in_run = true;
                run_start = j;
            }
        }
    }
    for k in 0..(*s).sites.len() {
        let (a, b, _) = (&(*s).sites)[k];
        let (a, b) = (a as usize, b as usize);
        let mut level = false;
        let mut m = 0usize;
        while a + m + 4 <= b && !level {
            let mut five: [(c_long, c_long); 5] = [(0, 0); 5];
            five.copy_from_slice(&(&(*s).tiles)[a + m..a + m + 5]);
            level = read_tiles(EE_AI_UNITS_HOST_TILES_LEVEL, me_ptr, &mut five, 0) != 0;
            m += 1;
        }
        let (pa, pb) = ((&(*s).tiles)[a], (&(*s).tiles)[b]);
        lua_createtable(l, 0, 6);
        set_int(l, c"ax", pa.0);
        set_int(l, c"ay", pa.1);
        set_int(l, c"bx", pb.0);
        set_int(l, c"by", pb.1);
        set_int(l, c"len", (b - a + 1) as c_long);
        set_bool(l, c"level", level);
        lua_rawseti(l, -2, k as lua_Integer + 1);
    }
    2
}

/// `ai.wall_path(unit, x1, y1, x2, y2)` -> {{x, y}, ...}: the tile path an own unit would take inside the box of
/// the two tiles (the classic wall planner's FindPath with the wall pather)
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_lua_wall_path(l: *mut c_void) -> c_int {
    let l = l as *mut lua_State;
    let me_ptr = me_of(l);
    let s = state_of(me_ptr);
    lua_createtable(l, 0, 0);
    if !(2..=5).all(|i| is_int(l, i)) || !has_map() {
        return 1;
    }
    let unit = {
        let t = WorldRead::begin();
        match Game::new(&t, me_ptr) {
            Some(g) if is_int(l, 1) => g.own(&*s, int_at(l, 1)).map_or(0, |u| u.handle()),
            _ => 0,
        }
    };
    if unit == 0 {
        return 1;
    }
    let ends = [[int_at(l, 2), int_at(l, 3)], [int_at(l, 4), int_at(l, 5)]];
    let n = fill_tiles(EE_AI_UNITS_HOST_WALL_PATH, me_ptr, s, unit as i64, ends);
    let area = ((ends[1][0] - ends[0][0]).abs() + 1) * ((ends[1][1] - ends[0][1]).abs() + 1);
    charge(l, s, COST_CALL + area as i64 * COST_SCAN + n * COST_FIELD * 2);
    push_tiles(l, s);
    1
}

/// `ai.wall_route(enemy, x1, y1, x2, y2)` -> handle: the classic wall planner's way from the enemy's centre to the
/// own town (the path manager's request on its coarse graph); one request a player at a time
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_lua_wall_route(l: *mut c_void) -> c_int {
    let l = l as *mut lua_State;
    let me_ptr = me_of(l);
    let s = state_of(me_ptr);
    charge(l, s, COST_COMMAND);
    if !(1..=5).all(|i| is_int(l, i)) || !has_map() {
        lua_pushnil(l);
        return 1;
    }
    let enemy = int_at(l, 1);
    let ok = {
        let t = WorldRead::begin();
        Game::new(&t, me_ptr).is_some_and(|g| {
            enemy > 0
                && (enemy as u64) < g.world.num_players()
                && g.world.player(enemy as usize).is_some()
                && class_at(&g, TYPE_CITIZEN).is_some()
        })
    };
    if !ok {
        lua_pushnil(l);
        return 1;
    }
    let mut t = EeAiUnitsTiles {
        xy: std::ptr::null_mut(),
        len: 0,
        cap: 0,
        from: [int_at(l, 2), int_at(l, 3)],
        to: [int_at(l, 4), int_at(l, 5)],
    };
    let route = EE_AI_UNITS_HOST_ROUTE_REQUEST;
    let handle = ee_ai_units_host(route, me_ptr, enemy as i64, 0, 0, (&mut t as *mut EeAiUnitsTiles).cast());
    if handle == 0 {
        lua_pushnil(l);
        return 1;
    }
    lua_pushinteger(l, handle as lua_Integer);
    1
}

/// `ai.wall_route_result(handle)` -> nil while it runs, the tiles {{x, y}, ...} when done (empty: no way), false for
/// a handle it does not hold (read already, or a loaded game)
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_lua_wall_route_result(l: *mut c_void) -> c_int {
    let l = l as *mut lua_State;
    let me_ptr = me_of(l);
    let s = state_of(me_ptr);
    charge(l, s, COST_CALL);
    let state = if is_int(l, 1) { host(EE_AI_UNITS_HOST_ROUTE_STATE, me_ptr, int_at(l, 1) as i64, 0, 0) } else { -2 };
    if state == -2 {
        lua_pushboolean(l, 0);
        return 1;
    }
    if state == -1 {
        lua_pushnil(l);
        return 1;
    }
    charge(l, s, state * COST_FIELD * 2);
    lua_createtable(l, state as c_int, 0);
    (*s).tiles.reserve(state as usize);
    fill_tiles(EE_AI_UNITS_HOST_ROUTE_TAKE, me_ptr, s, int_at(l, 1) as i64, [[0; 2]; 2]);
    push_tiles(l, s);
    1
}
