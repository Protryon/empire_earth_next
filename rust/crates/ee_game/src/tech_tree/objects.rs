//! The tech tree's objects read and changed in place (player/tech lane, round 3): a tree, a node and an epoch are
//! their Rust records (ee_sim tech tree_objects.rs makes them), so a pointer to one is its record; the virtual
//! members the original dispatched on the class (an epoch's GetIsObsolete, GetCost, GetBuildTime, LoadMedia and
//! UnloadMedia, its destructor) dispatch here on whether the object has an epoch's lists.

#![allow(clippy::not_unsafe_ptr_arg_deref, clippy::missing_safety_doc)]

use std::ffi::{c_long, c_ulong, c_void};

use ee_sim::tech::nodes::{
    ee_tech_walk_node, ee_tech_walk_tree, EeTechHostIo, EeTechNodeDataFields,
    EeTechNodeFields, EeTechNodeOp, EeTechNodeRef, EeTechTreeFields, EeTechTreeOp,
};
use ee_sim::tech::tree_objects as to;

use super::host::{ee_gamecpp_tech_epoch_free, ee_gamecpp_tech_host};
use super::load::ee_gamecpp_tech_node_free;
use crate::gamecpp::{hp, O, P};

pub(crate) fn tree_fields(tree: P) -> *mut EeTechTreeFields {
    tree as *mut EeTechTreeFields
}

pub(crate) fn node_fields(node: P) -> *mut EeTechNodeFields {
    node as *mut EeTechNodeFields
}

/// The node's data record (NULL without data, or for a NULL node).
pub(crate) fn node_data(node: P) -> *mut EeTechNodeDataFields {
    if node == 0 {
        return std::ptr::null_mut();
    }
    // SAFETY: a live node's record
    unsafe { (*node_fields(node)).mData }
}

/// The node's tree (`mTree`).
pub(crate) fn tree_of(node: P) -> P {
    // SAFETY: a live node's record
    unsafe { (*node_fields(node)).mTree as P }
}

pub(crate) fn set_tree(node: P, tree: P) {
    // SAFETY: a live node's record
    unsafe { (*node_fields(node)).mTree = tree as *mut _ };
}

/// The node's button (`mButton`, an `EEButtonObject*`).
pub(crate) fn button(node: P) -> P {
    // SAFETY: a live node's record
    unsafe { (*node_fields(node)).mButton as P }
}

/// `mButton = EEButtonObject::Get(id)`.
pub(crate) fn set_button_id(node: P, id: c_long) {
    let b = hp(O::TbButtonGet, 0, &[id as i64]);
    // SAFETY: a live node's record
    unsafe { (*node_fields(node)).mButton = b as *mut _ };
}

/// The other node's data and button shared (operator='s pointers).
pub(crate) fn share(node: P, other: P) {
    // SAFETY: two live nodes' records
    unsafe {
        (*node_fields(node)).mData = (*node_fields(other)).mData;
        (*node_fields(node)).mButton = (*node_fields(other)).mButton;
    }
}

/// `mData = new RTSTechTreeNodeData()`.
pub(crate) fn data_new(node: P) {
    // SAFETY: a live node's record
    unsafe { (*node_fields(node)).mData = to::data_new() };
}

/// `delete std::exchange(mData, nullptr)`.
pub(crate) fn data_delete(node: P) {
    // SAFETY: a live node's record; its data the load made (not shared with a live copy: Unload's contract)
    unsafe {
        let d = std::mem::replace(&mut (*node_fields(node)).mData, std::ptr::null_mut());
        if !d.is_null() {
            to::data_drop(d);
        }
    }
}

/// `new EETechTreeNode`.
pub(crate) fn node_new() -> P {
    to::node_new(false) as P
}

/// `delete node`: an epoch's nodes and lists, then the node's destructor (its unit media, its classes' buttons),
/// then its memory.
pub(crate) fn node_delete(node: P) {
    if node == 0 {
        return;
    }
    if is_epoch(node) {
        ee_gamecpp_tech_epoch_free(node as *mut c_void);
    }
    ee_gamecpp_tech_node_free(node as *mut c_void, true);
    // SAFETY: a node tree_objects made, unused after
    unsafe { to::node_drop(node_fields(node)) };
}

pub(crate) fn is_epoch(node: P) -> bool {
    // SAFETY: a live node
    unsafe { to::is_epoch(node) }
}

pub(crate) fn node_ref(node: P) -> EeTechNodeRef {
    // SAFETY: a live node
    unsafe { to::node_ref(node) }
}

/// `mEpochs.size()`.
pub(crate) fn epoch_count(tree: P) -> i64 {
    // SAFETY: a live tree's record
    unsafe { to::epoch_count(tree_fields(tree)) as i64 }
}

/// `&mEpochs[i]`.
pub(crate) fn epoch_at(tree: P, i: i64) -> P {
    // SAFETY: a live tree's record
    unsafe { to::epoch_at(tree_fields(tree), i) }
}

/// `mEpochs.Resize(n)`: new epochs until there are `n`.
pub(crate) fn epochs_resize(tree: P, n: i64) {
    // SAFETY: a live tree's record; no other borrow of its list
    let list = unsafe { to::epochs(tree_fields(tree)) };
    while (list.len() as i64) < n {
        list.push(to::node_new(true) as P);
    }
}

/// `Get(id)`.
pub(crate) fn get(tree: P, id: c_long) -> P {
    // SAFETY: a live tree's record
    unsafe { to::tree_get(tree_fields(tree), id) as P }
}

/// The player's tree (`mTechnologies`; 0 without one).
pub(crate) fn player_tree(player: P) -> P {
    let at = player + ee_sim::world::layout::layout().player_technologies as P;
    // SAFETY: a live player's pointer member
    unsafe { *(at as *const P) }
}

/// The tree walks' host (`EETechHost`).
pub(crate) unsafe extern "C" fn tech_host(tree: *mut c_void, op: i32, io: *mut EeTechHostIo) -> i32 {
    // SAFETY: the walk's live io
    unsafe { ee_gamecpp_tech_host(tree, op, io as *mut c_void) }
}

/// `EETechTreeNode::Walk`: a node's (or an epoch's) operation over its tree.
pub(crate) fn walk(node: P, op: EeTechNodeOp, a: i32, b: bool, unit: P) -> i32 {
    let tree = tree_of(node);
    // SAFETY: the node's live records and its tree's
    unsafe {
        ee_tech_walk_node(tree as *mut c_void, tech_host, op, tree_fields(tree), node_ref(node), a, b, unit as *mut c_void)
    }
}

/// `EETechTree::Walk`.
pub(crate) fn tree_walk(tree: P, op: EeTechTreeOp, a: i32, b: i32) -> i32 {
    // SAFETY: the tree's live record
    unsafe { ee_tech_walk_tree(tree as *mut c_void, tech_host, op, tree_fields(tree), a, b, std::ptr::null_mut()) }
}

/// The virtual `GetIsAvailable` (an epoch's answer is the walk's too).
pub(crate) fn is_available(node: P) -> bool {
    walk(node, EeTechNodeOp::IsAvailable, 0, false, 0) != 0
}

/// The virtual `GetIsObsolete`: an epoch's flag, a node's walk.
#[no_mangle]
pub extern "C" fn ee_gamecpp_tech_obj_obsolete(node: *mut c_void) -> bool {
    let n = node as P;
    if is_epoch(n) {
        // SAFETY: a live epoch's record
        return unsafe { (*node_fields(n)).mIsObsolete };
    }
    walk(n, EeTechNodeOp::IsObsolete, 0, false, 0) != 0
}

/// The virtual `GetCost`: an epoch's or a node's.
#[no_mangle]
pub extern "C" fn ee_gamecpp_tech_obj_cost(node: *mut c_void, resource: c_long, unit_class: c_long, scale: f32) -> c_long {
    if is_epoch(node as P) {
        super::host::ee_gamecpp_tech_epoch_get_cost(node, resource, unit_class, scale)
    } else {
        super::host::ee_gamecpp_tech_node_get_cost(node, resource, unit_class, scale)
    }
}

/// The virtual `GetBuildTime`: an epoch's (half for a computer player), a node's data's.
#[no_mangle]
pub extern "C" fn ee_gamecpp_tech_obj_build_time(node: *mut c_void) -> c_long {
    if is_epoch(node as P) {
        return super::load::ee_gamecpp_tech_epoch_build_time(node);
    }
    // SAFETY: a node with its data
    unsafe { (*node_data(node as P)).mBuildTime }
}

/// The virtual `LoadMedia` (`load`) and `UnloadMedia`: an epoch's (its buildings' unit classes, then its own), a
/// node's walk.
#[no_mangle]
pub extern "C" fn ee_gamecpp_tech_obj_media(node: *mut c_void, player: *mut c_void, load: bool) {
    let n = node as P;
    match (is_epoch(n), load) {
        (true, true) => super::host::ee_gamecpp_tech_epoch_load_media(node, player),
        (true, false) => super::host::ee_gamecpp_tech_epoch_unload_media(node, player),
        _ => {
            walk(n, EeTechNodeOp::Media, load as i32, false, player as P);
        }
    }
}

/// `GetUnitClass(i)`: the node data's unit class `i`, its default class past the end.
pub(crate) fn unit_class(node: P, i: usize) -> c_long {
    // SAFETY: a live node
    unsafe { to::unit_class(node, i) }
}

/// `GetUnitClass(i)` for the C++.
#[no_mangle]
pub extern "C" fn ee_gamecpp_tech_obj_unit_class(node: *mut c_void, i: c_ulong) -> c_long {
    unit_class(node as P, i as usize)
}

/// `EETechTree::Get` for the C++ (an `EETechTreeNode*`, or NULL).
#[no_mangle]
pub extern "C" fn ee_gamecpp_tech_obj_get(tree: *mut c_void, id: c_long) -> *mut c_void {
    get(tree as P, id) as *mut c_void
}

/// `~EETechTree`: the saved game's lists, then the epochs in order.
#[no_mangle]
pub extern "C" fn ee_gamecpp_tech_tree_free(tree: *mut c_void) {
    let t = tree_fields(tree as P);
    // SAFETY: the tree's live record, freed once
    unsafe { ee_sim::tech::nodes::ee_tech_tree_fields_free(t) };
    // SAFETY: as above
    for epoch in unsafe { to::epochs_take(t) } {
        node_delete(epoch);
    }
}

// ---- the world's reads the tree's bodies make (were EETechTree.cpp's host cases), through the world views

/// `EEUnitClassCount()`.
pub(crate) fn class_count() -> i64 {
    // SAFETY: a read of the class table
    let t = unsafe { ee_sim::world::WorldRead::begin() };
    ee_sim::world::ClassView::count(&t) as i64
}

/// `gUnitClassTable[i]` (0: none).
pub(crate) fn class_at(i: i64) -> P {
    // SAFETY: as class_count
    let t = unsafe { ee_sim::world::WorldRead::begin() };
    usize::try_from(i).ok().and_then(|i| ee_sim::world::ClassView::by_index(&t, i)).map_or(0, |c| c.handle() as P)
}

/// A class's `mUnitType`, `mFamily`.
pub(crate) fn class_type(class: P) -> i64 {
    // SAFETY: a live class
    let t = unsafe { ee_sim::world::WorldRead::begin() };
    unsafe { ee_sim::world::ClassView::of(&t, class as u64) }.map_or(0, |c| c.unit_type() as i64)
}

pub(crate) fn class_family(class: P) -> i64 {
    // SAFETY: a live class
    let t = unsafe { ee_sim::world::WorldRead::begin() };
    unsafe { ee_sim::world::ClassView::of(&t, class as u64) }.map_or(0, |c| c.family() as i64)
}

/// Class `i`'s `mTechnology` (`kTechNone` without the class).
pub(crate) fn class_technology(i: i64) -> i64 {
    // SAFETY: as class_count
    let t = unsafe { ee_sim::world::WorldRead::begin() };
    usize::try_from(i).ok().and_then(|i| ee_sim::world::ClassView::by_index(&t, i)).map_or(100000, |c| c.technology())
}

/// `gWorld.GetNumPlayers()`.
pub(crate) fn world_players() -> i64 {
    // SAFETY: a read of the world
    let t = unsafe { ee_sim::world::WorldRead::begin() };
    ee_sim::world::WorldView::global(&t).map_or(0, |w| w.num_players() as i64)
}

/// `gWorld.GetPlayer(i)` (0: none).
pub fn world_player(i: i64) -> P {
    // SAFETY: a read of the world
    let t = unsafe { ee_sim::world::WorldRead::begin() };
    let w = ee_sim::world::WorldView::global(&t);
    usize::try_from(i).ok().and_then(|i| w?.player(i)).map_or(0, |p| p.handle() as P)
}

/// `gWorld.GetCurrentPlayer()` (0: none).
pub(crate) fn current_player() -> P {
    // SAFETY: a read of the world
    let t = unsafe { ee_sim::world::WorldRead::begin() };
    ee_sim::world::WorldView::global(&t).and_then(|w| w.current_player()).map_or(0, |p| p.handle() as P)
}

/// A player's `mPlayerID`.
pub(crate) fn player_id(player: P) -> i64 {
    // SAFETY: a live player
    let t = unsafe { ee_sim::world::WorldRead::begin() };
    unsafe { ee_sim::world::PlayerView::of(&t, player as u64) }.map_or(0, |p| p.id() as i64)
}
