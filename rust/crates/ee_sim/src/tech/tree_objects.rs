//! The tech tree's objects as Rust's (player/tech lane, round 3): the nodes and epochs ([`EeTechNodeFields`], which
//! `EETechTreeNode` and `EETechTreeEpoch` derive from without members of their own), their data
//! ([`EeTechNodeDataFields`], `RTSTechTreeNodeData`), a tree's epoch list (`mEpochs`, was a C++ vector of owners) and
//! an epoch's nodes by id and loaded unit classes (`mLists`, were a `std::map` and a `std::vector`). `Get` is here: a
//! node's epoch from the id tables (tech_ids.rs), then the epoch itself or one of its nodes.
//!
//! The objects' destructors' world work (media, buttons) is ee_game tech_tree's; this module only makes, finds and
//! frees memory.
#![allow(non_snake_case)]
#![allow(clippy::missing_safety_doc)]

use std::collections::BTreeMap;
use std::os::raw::c_long;

use crate::tech::nodes::{node_fields_new, EeTechNodeDataFields, EeTechNodeFields, EeTechNodeRef, EeTechTreeFields};
use crate::tech::tech_ids;

/// An epoch's containers: its nodes by id (in the map's order) and the unit classes whose media LoadMedia loaded.
#[derive(Default)]
pub struct EeTechEpochLists {
    pub nodes: BTreeMap<c_long, usize>,
    pub loaded: Vec<c_long>,
}

/// A new node (`epoch`: an epoch, with its lists): the constructors' values, no data, tree or button.
pub fn node_new(epoch: bool) -> *mut EeTechNodeFields {
    let mut n = node_fields_new();
    if epoch {
        n.mLists = Box::into_raw(Box::<EeTechEpochLists>::default()) as usize;
    }
    Box::into_raw(Box::new(n))
}

/// Frees a node's memory (its lists for an epoch; its data stays: `Unload` frees it).
///
/// # Safety
/// `n` is a node [`node_new`] made, unused after.
pub unsafe fn node_drop(n: *mut EeTechNodeFields) {
    // SAFETY: the contract
    let n = unsafe { Box::from_raw(n) };
    if n.mLists != 0 {
        // SAFETY: the lists node_new made
        drop(unsafe { Box::from_raw(n.mLists as *mut EeTechEpochLists) });
    }
}

/// The node an `EETechTreeNode*` is (its record is the object).
pub fn node(n: usize) -> *mut EeTechNodeFields {
    n as *mut EeTechNodeFields
}

/// Whether the object is an epoch (`EETechTreeEpoch`).
///
/// # Safety
/// `n` is a live node.
pub unsafe fn is_epoch(n: usize) -> bool {
    // SAFETY: the contract
    unsafe { (*node(n)).mLists != 0 }
}

/// The node as the walks take it.
///
/// # Safety
/// `n` is a live node.
pub unsafe fn node_ref(n: usize) -> EeTechNodeRef {
    // SAFETY: the contract
    EeTechNodeRef { obj: n as *mut _, node: node(n), data: unsafe { (*node(n)).mData } }
}

/// An epoch's lists (no other borrow of them lives while this one is used).
///
/// # Safety
/// `e` is a live epoch.
pub unsafe fn lists<'a>(e: usize) -> &'a mut EeTechEpochLists {
    // SAFETY: the contract: an epoch's lists node_new made
    unsafe { &mut *((*node(e)).mLists as *mut EeTechEpochLists) }
}

/// A new node data record (`new RTSTechTreeNodeData()`: zeroed, no class table).
pub fn data_new() -> *mut EeTechNodeDataFields {
    Box::into_raw(Box::default())
}

/// `delete mData`: its class table, then the record.
///
/// # Safety
/// `d` is a record [`data_new`] made, unused after.
pub unsafe fn data_drop(d: *mut EeTechNodeDataFields) {
    // SAFETY: the contract
    unsafe {
        crate::tech::nodes::ee_tech_node_classes_free(d);
        drop(Box::from_raw(d));
    }
}

/// The tree's epoch list (made on first use).
///
/// # Safety
/// `t` is a live tree record; no other borrow of its list lives.
pub unsafe fn epochs<'a>(t: *mut EeTechTreeFields) -> &'a mut Vec<usize> {
    // SAFETY: the contract
    let t = unsafe { &mut *t };
    if t.mEpochs == 0 {
        t.mEpochs = Box::into_raw(Box::<Vec<usize>>::default()) as usize;
    }
    // SAFETY: the list this module made
    unsafe { &mut *(t.mEpochs as *mut Vec<usize>) }
}

/// `mEpochs.size()`.
///
/// # Safety
/// `t` is a live tree record.
pub unsafe fn epoch_count(t: *const EeTechTreeFields) -> usize {
    // SAFETY: the contract
    let at = unsafe { (*t).mEpochs };
    if at == 0 {
        0
    } else {
        // SAFETY: the list this module made
        unsafe { (*(at as *const Vec<usize>)).len() }
    }
}

/// `&mEpochs[i]` (0 past the list).
///
/// # Safety
/// `t` is a live tree record.
pub unsafe fn epoch_at(t: *const EeTechTreeFields, i: i64) -> usize {
    // SAFETY: the contract
    if i < 0 || i as usize >= unsafe { epoch_count(t) } {
        return 0;
    }
    // SAFETY: the list this module made, `i` in it
    unsafe { (&*((*t).mEpochs as *const Vec<usize>))[i as usize] }
}

/// The epoch list taken out of the tree (its destructor frees the epochs, in order).
///
/// # Safety
/// `t` is a live tree record.
pub unsafe fn epochs_take(t: *mut EeTechTreeFields) -> Vec<usize> {
    // SAFETY: the contract
    let at = std::mem::replace(unsafe { &mut (*t).mEpochs }, 0);
    if at == 0 {
        Vec::new()
    } else {
        // SAFETY: the list this module made
        *unsafe { Box::from_raw(at as *mut Vec<usize>) }
    }
}

/// `EETechTreeEpoch::Get`: the epoch for its own id, else one of its nodes, else 0.
///
/// # Safety
/// `e` is a live epoch with its data.
pub unsafe fn epoch_get(e: usize, id: c_long) -> usize {
    // SAFETY: the contract
    if unsafe { (*(*node(e)).mData).mID } == id {
        return e;
    }
    // SAFETY: as above
    unsafe { lists(e) }.nodes.get(&id).copied().unwrap_or(0)
}

/// `EETechTree::Get`: the node (or epoch) of an id in its epoch (`EETechTreeEpochOf`), or NULL.
///
/// # Safety
/// `t` is a live tree record (an `EETechTree*`).
pub unsafe fn tree_get(t: *mut EeTechTreeFields, id: c_long) -> *mut EeTechNodeFields {
    let epoch = tech_ids::epoch_of(id as i32) as i64;
    // SAFETY: the contract
    let e = unsafe { epoch_at(t, epoch) };
    if e == 0 {
        return std::ptr::null_mut();
    }
    // SAFETY: a live epoch of the tree
    node(unsafe { epoch_get(e, id) })
}

/// The player's tree (`mTechnologies`; 0 for no player or no tree).
///
/// # Safety
/// `player` is 0 or a live `EEPlayer`.
pub unsafe fn player_tree(player: usize) -> usize {
    let Some(l) = crate::world::layout::get() else { return 0 };
    if player == 0 {
        return 0;
    }
    // SAFETY: the contract: a live player's pointer member
    unsafe { *((player + l.player_technologies as usize) as *const usize) }
}

/// `gWorld.mPlayers[i]` (0 off the slots or for an empty one).
pub fn world_player(i: i64) -> usize {
    // SAFETY: a read of the world's player slots
    let t = unsafe { crate::world::WorldRead::begin() };
    let w = crate::world::WorldView::global(&t);
    usize::try_from(i).ok().and_then(|i| w?.player(i)).map_or(0, |p| p.handle() as usize)
}

/// The tree's walks' host (`&EETechHost`, the layout's).
fn host() -> Option<crate::tech::nodes::EeTechHostFn> {
    let at = crate::world::layout::get()?.tech_host as usize;
    // SAFETY: the registered address of `EETechHost` (EEWorldLayout.cpp), a function of this signature
    (at != 0).then(|| unsafe { std::mem::transmute::<usize, crate::tech::nodes::EeTechHostFn>(at) })
}

/// `node->Walk(op, a, b, unit)` over its tree (0 without the layout).
///
/// # Safety
/// `n` is a live node of a live tree.
pub unsafe fn walk_node(n: usize, op: crate::tech::nodes::EeTechNodeOp, a: i32, b: bool, unit: usize) -> i32 {
    let Some(f) = host() else { return 0 };
    // SAFETY: the contract
    unsafe {
        let t = (*node(n)).mTree as *mut EeTechTreeFields;
        crate::tech::nodes::ee_tech_walk_node(t as *mut _, f, op, t, node_ref(n), a, b, unit as *mut _)
    }
}

/// `tree->Walk(op, a, b, unit)`.
///
/// # Safety
/// `t` is a live tree.
pub unsafe fn walk_tree(t: usize, op: crate::tech::nodes::EeTechTreeOp, a: i32, b: i32, unit: usize) -> i32 {
    let Some(f) = host() else { return 0 };
    // SAFETY: the contract
    unsafe { crate::tech::nodes::ee_tech_walk_tree(t as *mut _, f, op, t as *mut EeTechTreeFields, a, b, unit as *mut _) }
}

/// The virtual `GetIsObsolete`: an epoch's flag, a node's walk.
///
/// # Safety
/// As [`walk_node`].
pub unsafe fn is_obsolete(n: usize) -> bool {
    // SAFETY: the contract
    unsafe {
        if is_epoch(n) {
            return (*node(n)).mIsObsolete;
        }
        walk_node(n, crate::tech::nodes::EeTechNodeOp::IsObsolete, 0, false, 0) != 0
    }
}

/// `GetUnitClass(i)`: the data's unit class `i`, its default class past the end (0 without data).
///
/// # Safety
/// `n` is a live node.
pub unsafe fn unit_class(n: usize, i: usize) -> c_long {
    // SAFETY: the contract
    let d = unsafe { (*node(n)).mData };
    if d.is_null() {
        return 0;
    }
    // SAFETY: the node's data
    let d = unsafe { &*d };
    d.unit_classes().get(i).map_or(d.mDefaultUnitClass, |&c| c as c_long)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_node_and_an_epoch_are_made_and_freed() {
        let n = node_new(false);
        let e = node_new(true);
        // SAFETY: the two new nodes
        unsafe {
            assert!(!is_epoch(n as usize) && is_epoch(e as usize));
            assert!((*n).mIsAllowed && (*n).mObsoleteEpoch == 14 && (*n).mData.is_null());
            let d = data_new();
            (*d).mID = 3;
            (*e).mData = d;
            lists(e as usize).nodes.insert(3001, n as usize);
            assert_eq!(epoch_get(e as usize, 3), e as usize);
            assert_eq!(epoch_get(e as usize, 3001), n as usize);
            assert_eq!(epoch_get(e as usize, 3002), 0);
            data_drop(d);
            node_drop(n);
            node_drop(e);
        }
    }

    #[test]
    fn a_tree_s_epochs() {
        let mut t = EeTechTreeFields::default();
        // SAFETY: the local record
        unsafe {
            assert_eq!(epoch_count(&t), 0);
            assert_eq!(epoch_at(&t, 0), 0);
            epochs(&mut t).push(7);
            assert_eq!((epoch_count(&t), epoch_at(&t, 0), epoch_at(&t, 1)), (1, 7, 0));
            assert_eq!(epochs_take(&mut t), vec![7]);
            assert_eq!(t.mEpochs, 0);
        }
    }
}
