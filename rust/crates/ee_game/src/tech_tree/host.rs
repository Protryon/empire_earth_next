//! The tech tree's C++ bodies (was EETechTree.cpp's EETechHost and EETechMedia, the tree's file load, its
//! interface refresh and the cost sum's callbacks; EETechTreeEpoch.cpp's node list walks and media; EETechTreeNode.cpp's
//! cost): the walks themselves are ee_sim tech's (nodes.rs); this is the host they call, now Rust over one-statement
//! cases of EETechTree.cpp's `EECore9OpD6` and EETechTreeNode.cpp's `EECore9OpD7` ([`crate::gamecpp::EeGameCpp`]'s `Tk` ops).
//! The trees', epochs', nodes' and node data's plain members are read in place through their Rust records.
//!
//! Simulation code (RUST_PLAN 2.1): the cost's inputs are the C++'s values; the formula is ee_sim's `ee_tech_node_cost`.

use std::ffi::{c_long, c_void};

use ee_sim::tech::ffi::{ee_tech_node_cost, ee_tech_sum_costs, EeTechSumCostsCallbacks, EeTechSumNode};
use ee_sim::tech::node_media::{EE_TECH_HOST_MEDIA, EE_TECH_HOST_MEDIA_EFFECT};
use ee_sim::tech::nodes::{
    EeTechHostIo, EeTechNodeDataFields, EeTechNodeFields, EeTechTreeFields, EE_TECH_HOST_ASSERT,
    EE_TECH_HOST_CLASS_TYPE, EE_TECH_HOST_EFFECT, EE_TECH_HOST_EFFECT_COUNT, EE_TECH_HOST_EPOCH, EE_TECH_HOST_EPOCHS,
    EE_TECH_HOST_EPOCH_NODES, EE_TECH_HOST_EVENTS_ENABLED, EE_TECH_HOST_IS_LOCAL, EE_TECH_HOST_NEEDS_UNIT_MEDIA,
    EE_TECH_HOST_NODE, EE_TECH_HOST_ON_RESEARCHED, EE_TECH_HOST_SHOW_EPOCH_NAME, EE_TECH_HOST_STATISTIC,
    EE_TECH_HOST_TRIGGER, EE_TECH_HOST_UNLOAD_UNIT_MEDIA, EE_TECH_HOST_UPDATE_INTERFACE,
};
use ee_sim::tech::nodes_capture::{EE_TECH_HOST_PLAYER_INDEX, EE_TECH_HOST_UNIT_ID};
use ee_sim::tech::tech_ids::{ee_tech_clear_node_epochs, ee_tech_set_epoch_count};
use ee_sim::tech::tree::EeTechCostInput;

use super::load;
use super::objects as ob;
use crate::gamecpp::{h, hd, hp, ptr, O, P};
use ee_data::tables::EeDataGameVariantFields;
use ee_sim::tech::capture_rec::ee_tech_capture_cost;
use ee_sim::tech::ffi::ee_tech_epoch_cost_scale;
use ee_sim::tech::node_media::cost_string;
use ee_sim::tech::nodes::EeTechNodeOp;
use ee_sim::tech::tree_objects::EeTechEpochLists;

/// `kErrorOK`.
const TK_OK: i64 = 0;
/// The statistics series OnResearched resets (EEPlayer::mSavedGroups' 0x13).
const TK_STAT_EPOCHS: i64 = 0x13;

fn tree_fields(tree: P) -> *mut EeTechTreeFields {
    ob::tree_fields(tree)
}

fn node_fields(node: P) -> EeTechNodeFields {
    // SAFETY: the node's record, copied out
    unsafe { *ob::node_fields(node) }
}

fn node_data(node: P) -> EeTechNodeDataFields {
    // SAFETY: the node's data record, copied out
    unsafe { *ob::node_data(node) }
}

/// The media mask's media kinds (ee_sim tech node_media.rs EE_TECH_MEDIA_*): one media call on the player's behalf.
fn media(player: P, what: i32, id: i32, load: bool) {
    use ee_sim::tech::node_media::{
        EE_TECH_MEDIA_AREA_EFFECT, EE_TECH_MEDIA_BUTTON, EE_TECH_MEDIA_CLASS, EE_TECH_MEDIA_CLASS_BUTTONS,
        EE_TECH_MEDIA_EPOCH, EE_TECH_MEDIA_ITEM, EE_TECH_MEDIA_SLEEP, EE_TECH_MEDIA_SOUND,
    };
    let id = id as i64;
    let op = match what {
        EE_TECH_MEDIA_BUTTON => {
            if load {
                O::TkButtonAcquire
            } else {
                O::TkButtonRelease
            }
        }
        EE_TECH_MEDIA_AREA_EFFECT => {
            // (the original's unload called LoadTemplateMedia again)
            if load {
                O::TkAeTemplateLoad
            } else {
                O::TkAeTemplateUnload
            }
        }
        EE_TECH_MEDIA_ITEM => {
            if load {
                O::TkItemLoad
            } else {
                O::TkItemUnload
            }
        }
        EE_TECH_MEDIA_SOUND => {
            if load {
                O::TkSoundLoad
            } else {
                O::TkSoundUnload
            }
        }
        EE_TECH_MEDIA_CLASS => {
            if load {
                O::TkClassMedia
            } else {
                O::TkUnloadClassMedia
            }
        }
        EE_TECH_MEDIA_CLASS_BUTTONS => {
            if load {
                O::TkClassButtonsLoad
            } else {
                O::TkClassButtonsUnload
            }
        }
        EE_TECH_MEDIA_EPOCH => {
            if load {
                O::TkEpochMediaLoaded
            } else {
                O::TkEpochMediaUnloaded
            }
        }
        EE_TECH_MEDIA_SLEEP => {
            ee_base::plat_core::time::PlatSleep(1);
            return;
        }
        _ => return,
    };
    h(op, player, &[id]);
}

/// EETechTree::UpdateInterface: for the local player's tree, the new epoch's name shown and the main game form's
/// group panel refreshed.
#[no_mangle]
pub extern "C" fn ee_gamecpp_tech_update_interface(tree: *mut c_void, epoch_changed: bool) {
    let tree = tree as P;
    let current = ob::current_player();
    if current == 0 || load::player_index(tree) as i64 != ob::player_id(current) {
        return;
    }
    if epoch_changed {
        show_epoch_name(tree, None);
    } else {
        refresh_group();
    }
}

/// The epoch's name (the current epoch's, or `epoch`'s) posted, then the group panel refreshed.
fn show_epoch_name(tree: P, epoch: Option<i64>) {
    let t = tree_fields(tree);
    // SAFETY: the tree's record, no C++ call while read
    let epoch = epoch.unwrap_or_else(|| unsafe { (*t).mCurrentEpoch } as i64);
    let title = ee_sim::tech::tech_ids::ee_tech_epoch_title_id(epoch as c_long) as c_long;
    // SAFETY: as above
    unsafe { (*t).mEpochNameID = title };
    h(O::TkPostShowEpochName, 0, &[]);
    refresh_group();
}

fn refresh_group() {
    if hp(O::TkMainGameForm, 0, &[]) != 0 {
        h(O::TkRefreshGroup, 0, &[]);
    }
}

/// The walks' host (ee_sim tech nodes.rs EE_TECH_HOST_*; was EETechTree.cpp's EETechHost): the tree's containers, the
/// entry points the simulation capture wraps, the world, the events, the interface and the media.
///
/// # Safety
/// `io` is the walk's live record; `tree` an EETechTree.
#[no_mangle]
pub unsafe extern "C" fn ee_gamecpp_tech_host(tree: *mut c_void, op: i32, io: *mut c_void) -> i32 {
    let tree = tree as P;
    // SAFETY: the caller's record
    let io = unsafe { &mut *(io as *mut EeTechHostIo) };
    let node = io.node.obj as P;
    let unit = io.unit as P;
    let (nodes, count) = (io.nodes, io.count);
    let v = &mut io.v;
    match op {
        EE_TECH_HOST_EPOCHS => v[0] = ob::epoch_count(tree) as i32,
        EE_TECH_HOST_EPOCH => io.node = ob::node_ref(ob::epoch_at(tree, v[0] as i64)),
        EE_TECH_HOST_EPOCH_NODES => {
            // the epoch's nodes in its map's order into the walk's buffer (at most its count); their count
            let all = epoch_nodes(node);
            for (k, &at) in all.iter().take(count as usize).enumerate() {
                // SAFETY: the walk's buffer of `count` refs
                unsafe { *nodes.add(k) = ob::node_ref(at) };
            }
            return all.len() as i32;
        }
        EE_TECH_HOST_PLAYER_INDEX => v[0] = load::player_index(tree) as i64 as i32,
        EE_TECH_HOST_UNIT_ID => v[0] = if unit != 0 { h(O::TkUnitId, unit, &[]) as i32 } else { -1 },
        EE_TECH_HOST_NODE => {
            let found = ob::get(tree, v[0] as c_long);
            if found == 0 {
                return 0;
            }
            io.node = ob::node_ref(found);
            return 1;
        }
        EE_TECH_HOST_NEEDS_UNIT_MEDIA => {
            return load::ee_gamecpp_tech_needs_unit_media(node as *mut c_void, true) as i32
        }
        EE_TECH_HOST_TRIGGER => {
            let (index, player) = (load::player_index(tree), load::player_pointer(tree));
            ee_sim::scenario::event::ee_scenario_ev_trigger(
                v[0] as c_long,
                index,
                v[1] as c_long,
                player as *mut c_void,
                unit as *mut c_void,
            );
        }
        EE_TECH_HOST_UPDATE_INTERFACE => ee_gamecpp_tech_update_interface(tree as *mut c_void, v[0] != 0),
        EE_TECH_HOST_ON_RESEARCHED => {
            // OnResearched 0x1ccdac: the player's epoch hook and statistics series 0x13 (its unit: always NULL)
            let player = tree_player(tree);
            if player != 0 {
                h(O::TkEpochReached, player, &[v[0] as i64]);
                h(O::TkStatReset, player, &[TK_STAT_EPOCHS]);
            }
        }
        EE_TECH_HOST_STATISTIC => {
            if v[0] == 2 {
                let player = load::player_pointer(tree);
                h(O::TkStatSet, player, &[v[1] as i64, v[2] as i64]);
            } else {
                let player = if v[0] == 0 { load::player_pointer(tree) } else { tree_player(tree) };
                h(O::TkStatReset, player, &[v[1] as i64]);
            }
        }
        EE_TECH_HOST_UNLOAD_UNIT_MEDIA => {
            let player = tree_player(tree);
            h(O::TkUnloadClassMedia, player, &[v[0] as i64]);
        }
        EE_TECH_HOST_EVENTS_ENABLED => {
            h(O::TkEventsEnabled, 0, &[(v[0] != 0) as i64]);
        }
        EE_TECH_HOST_IS_LOCAL => {
            return (ob::world_player(load::player_index(tree) as i64) == ob::current_player()) as i32
        }
        EE_TECH_HOST_SHOW_EPOCH_NAME => show_epoch_name(tree, Some(v[0] as i64)),
        EE_TECH_HOST_ASSERT => {
            h(O::TkAssert, 0, &[v[0] as i64]);
        }
        EE_TECH_HOST_EFFECT_COUNT => {
            let event = hp(O::TkEvent, 0, &[node_data(node).mResearchedEvent as i64]);
            return if event != 0 { h(O::TkEventEffects, event, &[]) as i32 } else { 0 };
        }
        EE_TECH_HOST_EFFECT => {
            let e = effect(node, v[0]);
            (v[0], v[1], v[2]) = (e.mKind, e.mUnitClass, e.mTechnology);
        }
        EE_TECH_HOST_MEDIA_EFFECT => {
            let e = effect(node, v[0]);
            v[0] = e.mKind;
            v[1] = match e.mKind {
                1 => e.mButton,
                2 => e.mAreaEffect,
                6 => e.mMediaIndex,
                0x13 => e.mNewUnitClass,
                _ => e.mSoundID,
            };
            v[2] = (e.mAttribute == 0x28) as i32;
        }
        EE_TECH_HOST_MEDIA => media(unit, v[0], v[1], v[2] != 0),
        EE_TECH_HOST_CLASS_TYPE => {
            let class = if v[0] >= 0 && (v[0] as i64) < ob::class_count() { ob::class_at(v[0] as i64) } else { 0 };
            if class == 0 {
                return 0;
            }
            v[0] = ob::class_type(class) as i32;
            return 1;
        }
        _ => {}
    }
    0
}

/// Effect `i` of the node's researched event (a record of gEventEffects, copied out).
fn effect(node: P, i: i32) -> ee_sim::scenario::event::EeScenarioEvEffect {
    let event = hp(O::TkEvent, 0, &[node_data(node).mResearchedEvent as i64]);
    let at = hp(O::TkEventEffect, event, &[i as i64]);
    // SAFETY: the event's effect record (gEventEffects' element)
    unsafe { *(at as *const ee_sim::scenario::event::EeScenarioEvEffect) }
}

/// EETechTree::Load (the tech tree file): every epoch block (the original: exactly 15), the epoch count, and the
/// epoch range reset to all epochs.
#[no_mangle]
pub extern "C" fn ee_gamecpp_tech_load_file(tree: *mut c_void, file: *mut c_void) -> c_long {
    let (tree, file) = (tree as P, file as P);
    let mut result = TK_OK;
    h(O::TkFileOpenRead, file, &[]);
    let mut length: std::ffi::c_ulong = 0;
    h(O::TkFileLength, file, &[ptr(&mut length)]);
    let mut position: c_long = 0;
    let mut count: i64 = 0;
    ee_tech_clear_node_epochs();
    while result == TK_OK
        && h(O::TkFilePosition, file, &[ptr(&mut position)]) == TK_OK
        && (position as std::ffi::c_ulong) < length
    {
        ob::epochs_resize(tree, count + 1);
        result = load::ee_gamecpp_tech_epoch_load(
            ob::epoch_at(tree, count) as *mut c_void,
            file as *mut c_void,
            tree as *mut c_void,
        ) as i64;
        count += 1;
    }
    ee_tech_set_epoch_count(count as i32);
    let t = tree_fields(tree);
    // SAFETY: the tree's record
    unsafe {
        (*t).mStartEpoch = 0;
        (*t).mIsLoaded = false;
        (*t).mEndEpoch = (count - 1) as c_long;
    }
    h(O::TkFileClose, file, &[]);
    result as c_long
}

/// EETechTree::UnloadAll: every epoch unloaded.
#[no_mangle]
pub extern "C" fn ee_gamecpp_tech_unload_all(tree: *mut c_void) {
    let tree = tree as P;
    for i in 0..ob::epoch_count(tree) {
        ee_gamecpp_tech_epoch_delete_nodes(ob::epoch_at(tree, i) as *mut c_void, true);
    }
}

/// EETechTree::GetNodeIDs (`all`) and GetEpochNodeIDs: the node ids of every epoch, or of one (out of range: none).
#[no_mangle]
pub extern "C" fn ee_gamecpp_tech_node_ids(tree: *mut c_void, all: bool, epoch: c_long, out: *mut c_void) {
    let tree = tree as P;
    let count = ob::epoch_count(tree);
    for i in 0..count {
        if all || i == epoch as i64 {
            ee_gamecpp_tech_epoch_node_ids(ob::epoch_at(tree, i) as *mut c_void, out);
        }
    }
}

unsafe extern "C" fn sum_node(tree: *mut c_void, id: i32, out: *mut EeTechSumNode) -> bool {
    let node = ob::get(tree as P, id as c_long);
    if node == 0 {
        return false;
    }
    let d = node_data(node);
    let n = node_fields(node);
    let unit_class = ob::unit_class(node, 0) as i32;
    let available = ob::is_available(node);
    // SAFETY: the caller's record
    unsafe {
        *out = EeTechSumNode {
            id: d.mID as i32,
            unit_class,
            prerequisites: [
                d.mPrerequisite1 as i32,
                d.mPrerequisite2 as i32,
                d.mPrerequisite3 as i32,
                d.mPrerequisite4 as i32,
            ],
            available,
            researched: n.mIsResearched,
            pad: [0, 0],
        };
    }
    true
}

unsafe extern "C" fn sum_cost(tree: *mut c_void, id: i32, resource: i32, unit_class: i32) -> i32 {
    let node = ob::get(tree as P, id as c_long);
    ob::ee_gamecpp_tech_obj_cost(node as *mut c_void, resource as c_long, unit_class as c_long, 1.0) as i32
}

unsafe extern "C" fn sum_class_technology(_tree: *mut c_void, unit_class: i32) -> i32 {
    ob::class_technology(unit_class as i64) as i32
}

/// EETechTree::SumCostsUncaptured: the resource costs of a tech and every unresearched prerequisite and upgrade path
/// to it (ee_sim's sum over the tree's nodes).
///
/// # Safety
/// `costs` holds 7 longs.
#[no_mangle]
pub unsafe extern "C" fn ee_gamecpp_tech_sum_costs(
    tree: *mut c_void,
    id: c_long,
    unit_class: c_long,
    costs: *mut c_long,
) {
    let callbacks =
        EeTechSumCostsCallbacks { ctx: tree, node: sum_node, cost: sum_cost, class_technology: sum_class_technology };
    // SAFETY: the caller's array
    let costs = unsafe { std::slice::from_raw_parts_mut(costs, 7) };
    let mut sums = [0i32; 7];
    for (s, c) in sums.iter_mut().zip(costs.iter()) {
        *s = *c as i32;
    }
    // SAFETY: the callbacks and the array
    unsafe { ee_tech_sum_costs(&callbacks, id as i32, unit_class as i32, sums.as_mut_ptr()) };
    for (c, s) in costs.iter_mut().zip(sums.iter()) {
        *c = *s as c_long;
    }
}

/// EETechTreeNode::GetCostUncaptured: the node's cost in a resource for a unit class (the class's own costs when it
/// has some), scaled; a unit's by the wonder's epochs, the variant's cost set, the player's upgrade and the
/// just-in-time manufacturing (ee_sim's `ee_tech_node_cost`).
#[no_mangle]
pub extern "C" fn ee_gamecpp_tech_node_cost(
    node: *mut c_void,
    resource: c_long,
    unit_class: c_long,
    scale: f32,
) -> c_long {
    let node = node as P;
    let tree = ob::tree_of(node);
    let player = tree_player(tree);
    let d = node_data(node);
    let mut input = EeTechCostInput {
        // the class's own costs (Load made them) when it has some
        cost: d.class_costs(unit_class as c_long).map_or(d.mCosts[resource as usize], |c| c[resource as usize]) as i32,
        scale,
        makes_unit: d.mMakesUnit,
        ..EeTechCostInput::default()
    };
    if d.mMakesUnit {
        let stats = hp(O::TkUpgrade, player, &[d.mUnitClass as i64]);
        let epoch = node_fields(node).mEpoch;
        input.wonder_epoch_scaled = d.mKind == 4 && epoch != 0;
        if input.wonder_epoch_scaled {
            // SAFETY: the tree's record
            input.epochs_since = (unsafe { (*tree_fields(tree)).mCurrentEpoch } - epoch) as i32;
            input.epoch_cost_factor = hd(O::TkEpochCostFactor, 0, &[], &[]);
        }
        input.in_cost_set = h(O::TkInCostSet, 0, &[d.mUnitClass as i64]) != 0;
        if input.in_cost_set {
            input.cost_set_factor = hd(O::TkCostSetFactor, 0, &[], &[]);
        }
        input.has_upgrade = stats != 0;
        if stats != 0 {
            input.cost_scale = hd(O::TkStatsCostScale, stats, &[], &[]);
        }
        input.jit = player_jit(player) && h(O::TkClassHasRallyPoint, 0, &[unit_class as i64]) != 0;
        if input.jit {
            input.jit_factor = hd(O::TkJitFactor, 0, &[], &[]);
        }
    }
    // SAFETY: the record
    unsafe { ee_tech_node_cost(&input) as c_long }
}

/// The player's `mJITManufacturing`.
fn player_jit(player: P) -> bool {
    let at = player + ee_sim::world::layout::layout().player_fields as P;
    // SAFETY: a live player's record
    unsafe { (*(at as *const ee_sim::world::player_fields::EeUnitsPlayerFields)).mJITManufacturing }
}

/// TCST (TEST ONLY: ee_sim tech capture_rec.rs): a node's (`kind` 0) or an epoch's (1) GetCost, recorded while the
/// capture is active (was EETechCapture.cpp's EETechCaptureCost hook).
fn capture_cost(node: P, kind: c_long, resource: c_long, unit_class: c_long, scale: f32, cost: c_long) {
    if !ee_sim::app::capture::ee_app_capture_active() {
        return;
    }
    let tree = ob::tree_of(node);
    let (player, epoch) = if tree != 0 {
        // SAFETY: the tree's record
        (load::player_index(tree) as i64 as c_long, unsafe { (*tree_fields(tree)).mCurrentEpoch })
    } else {
        (-1, -1)
    };
    ee_tech_capture_cost(kind, player, node_data(node).mID, epoch, resource, unit_class, scale, cost);
}

/// EETechTreeNode::GetCost: [`ee_gamecpp_tech_node_cost`], recorded (TCST kind 0).
#[no_mangle]
pub extern "C" fn ee_gamecpp_tech_node_get_cost(
    node: *mut c_void,
    resource: c_long,
    unit_class: c_long,
    scale: f32,
) -> c_long {
    let cost = ee_gamecpp_tech_node_cost(node, resource, unit_class, scale);
    capture_cost(node as P, 0, resource, unit_class, scale, cost);
    cost
}

/// EETechTreeEpoch::GetCost: the node's cost (recorded) at the scale of the epochs advanced in this game (the game
/// variant's first, second or later epoch cost factor: ee_sim's `ee_tech_epoch_cost_scale`), recorded (TCST kind 1).
#[no_mangle]
pub extern "C" fn ee_gamecpp_tech_epoch_get_cost(
    epoch: *mut c_void,
    resource: c_long,
    unit_class: c_long,
    scale: f32,
) -> c_long {
    // SAFETY: the game variant's record (EEGameVariant's base), copied out
    let v = unsafe { *crate::gamecpp::rec::<EeDataGameVariantFields>(O::TkVariant, 0) };
    let t = tree_fields(ob::tree_of(epoch as P));
    // SAFETY: the epoch's tree's record
    let epochs = unsafe { (*t).mCurrentEpoch.wrapping_sub((*t).mStartEpoch) } as u32;
    let s = ee_tech_epoch_cost_scale(
        epochs,
        v.mEpochCostFactor,
        v.mEpochCostFactor,
        v.mSecondEpochCostFactor,
        v.mFirstEpochCostFactor,
        scale,
    );
    let cost = ee_gamecpp_tech_node_get_cost(epoch, resource, unit_class, s);
    capture_cost(epoch as P, 1, resource, unit_class, scale, cost);
    cost
}

/// EETechTreeNode::GetCostString: the "Food: n Gold: n ..." text of the node's (virtual) GetCost for a unit class
/// into `out` (a UWideString; wood shown as carbon on the carbon maps; ee_sim tech node_media.rs; 511 bytes at most).
#[no_mangle]
pub extern "C" fn ee_gamecpp_tech_cost_string(node: *mut c_void, unit_class: c_long, scale: f32, out: *mut c_void) {
    let node = node as P;
    // SAFETY: the world's bool member
    let carbon = unsafe { *(hp(O::WUsingCarbon, hp(O::WorldAddr, 0, &[]), &[]) as *const bool) };
    let mut cost =
        |r: i32, s: f32| ob::ee_gamecpp_tech_obj_cost(node as *mut c_void, r as c_long, unit_class, s) as i32;
    let mut text = cost_string(&mut cost, scale, carbon).into_bytes();
    text.truncate(511);
    text.push(0);
    h(O::EpWideSet, out as P, &[text.as_ptr() as usize as i64]);
}

/// EETechTree::GetPlayer: the tree's player, by its index when no player pointer is set.
#[no_mangle]
pub extern "C" fn ee_gamecpp_tech_tree_player(tree: *mut c_void) -> *mut c_void {
    tree_player(tree as P) as *mut c_void
}

/// [`ee_gamecpp_tech_tree_player`].
pub(crate) fn tree_player(tree: P) -> P {
    let player = load::player_pointer(tree);
    if player != 0 {
        player
    } else {
        ob::world_player(load::player_index(tree) as i64)
    }
}

/// EETechTreeNode::GetHotKey: the node's hot key (its data's mHotKey), or NULL.
#[no_mangle]
pub extern "C" fn ee_gamecpp_tech_node_hot_key(node: *mut c_void) -> *mut c_void {
    crate::gamecpp::db_media::ee_gamecpp_db_hot_key(node_data(node as P).mHotKey)
}

/// The epoch's lists (no C++ call is made while a borrow of them lives).
fn lists<'a>(epoch: P) -> &'a mut EeTechEpochLists {
    // SAFETY: a live epoch's lists (ee_sim tech tree_objects.rs made them)
    unsafe { ee_sim::tech::tree_objects::lists(epoch) }
}

/// A node into its epoch by its id (a node already there under the id is kept: std::map::insert).
pub(crate) fn epoch_insert(epoch: P, node: P) {
    let id = node_data(node).mID;
    lists(epoch).nodes.entry(id).or_insert(node);
}

/// The epoch's nodes, in their map's order.
pub(crate) fn epoch_nodes(epoch: P) -> Vec<P> {
    lists(epoch).nodes.values().copied().collect()
}

/// EETechTreeEpoch's destructor's body: its nodes deleted (its lists go with the epoch: objects.rs node_delete).
pub(crate) fn ee_gamecpp_tech_epoch_free(epoch: *mut c_void) {
    ee_gamecpp_tech_epoch_delete_nodes(epoch, false);
}

/// EETechTreeEpoch::DeleteNodes (`unload`: Unload, each node unloaded first, then the epoch's data deleted; 0).
#[no_mangle]
pub extern "C" fn ee_gamecpp_tech_epoch_delete_nodes(epoch: *mut c_void, unload: bool) -> c_long {
    let epoch = epoch as P;
    for node in epoch_nodes(epoch) {
        if unload {
            load::ee_gamecpp_tech_node_unload(node as *mut c_void);
        }
        ob::node_delete(node);
    }
    lists(epoch).nodes.clear();
    if unload {
        ob::data_delete(epoch);
    }
    0
}

/// EETechTreeEpoch::GetNodeIDs: the epoch's node ids appended.
#[no_mangle]
pub extern "C" fn ee_gamecpp_tech_epoch_node_ids(epoch: *mut c_void, out: *mut c_void) {
    let ids: Vec<c_long> = lists(epoch as P).nodes.keys().copied().collect();
    for id in ids {
        h(O::TkPushId, 0, &[out as i64, id as i64]);
    }
}

/// EETechTreeEpoch::LoadMedia: the media of every unit class the epoch's buildings make (kept in its loaded list),
/// then the epoch's own.
#[no_mangle]
pub extern "C" fn ee_gamecpp_tech_epoch_load_media(epoch: *mut c_void, player: *mut c_void) {
    let (epoch, player) = (epoch as P, player as P);
    for node in epoch_nodes(epoch) {
        if load::ee_gamecpp_tech_needs_unit_media(node as *mut c_void, true) {
            let unit_class = node_data(node).mUnitClass;
            h(O::TkClassMedia, player, &[unit_class as i64]);
            lists(epoch).loaded.push(unit_class);
        }
    }
    ob::walk(epoch, EeTechNodeOp::Media, 1, false, player);
}

/// EETechTreeEpoch::UnloadMedia: the unit class media it loaded, then its own.
#[no_mangle]
pub extern "C" fn ee_gamecpp_tech_epoch_unload_media(epoch: *mut c_void, player: *mut c_void) {
    let (epoch, player) = (epoch as P, player as P);
    let loaded = lists(epoch).loaded.clone();
    for unit_class in loaded {
        h(O::TkUnloadClassMedia, player, &[unit_class as i64]);
    }
    lists(epoch).loaded.clear();
    ob::walk(epoch, EeTechNodeOp::Media, 0, false, player);
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_cost_input_defaults_to_no_unit_factors() {
        let input = EeTechCostInput { cost: 50, scale: 1.0, ..EeTechCostInput::default() };
        assert!(!input.makes_unit && !input.jit && !input.in_cost_set);
        assert_eq!(input.epochs_since, 0);
    }

    #[test]
    fn the_statistic_series_is_the_epochs() {
        assert_eq!(TK_STAT_EPOCHS, 0x13);
    }
}
