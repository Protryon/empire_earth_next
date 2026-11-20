//! The tech ids' two tables (were EETechTree.cpp's `gEpochCount` and `sNodeEpochs`): the epoch count, which the
//! tech tree file sets when `gTechTree` loads (15, the original's, until then), and the epoch each node id was
//! loaded in (`EETechTreeEpochOf`: an epoch's own id, a node's block; -1 for none). ee/EETechTreeTechID.h reads
//! them inline.
//!
//! Written once per database load on the loading thread; read from every thread (the physics, the AI, the
//! interface), so behind a lock.

use std::collections::BTreeMap;
use std::sync::atomic::{AtomicI32, Ordering};
use std::sync::RwLock;

/// The original's epoch count (`kTechEpochAll`).
pub const EE_TECH_DEFAULT_EPOCH_COUNT: i32 = 15;

static EPOCH_COUNT: AtomicI32 = AtomicI32::new(EE_TECH_DEFAULT_EPOCH_COUNT);
static NODE_EPOCHS: RwLock<BTreeMap<i32, i32>> = RwLock::new(BTreeMap::new());

/// `EEEpochCount()`.
pub fn epoch_count() -> i32 {
    EPOCH_COUNT.load(Ordering::Relaxed)
}

/// `EEIsEpochID`: an epoch's id is its position.
pub fn is_epoch_id(id: i32) -> bool {
    id >= 0 && id < epoch_count()
}

/// `EETechTreeEpochOf`: an epoch's own id, else the block the node was loaded in, else -1.
pub fn epoch_of(id: i32) -> i32 {
    if is_epoch_id(id) {
        return id;
    }
    let map = NODE_EPOCHS.read().unwrap_or_else(|e| e.into_inner());
    map.get(&id).copied().unwrap_or(-1)
}

/// The epoch count (`gEpochCount`).
#[no_mangle]
pub extern "C" fn ee_tech_epoch_count() -> i32 {
    epoch_count()
}

/// Sets the epoch count (the tech tree file's block count, `EETechTree::Load`).
#[no_mangle]
pub extern "C" fn ee_tech_set_epoch_count(count: i32) {
    EPOCH_COUNT.store(count, Ordering::Relaxed);
}

/// The epoch of a tech id ([`epoch_of`]).
#[no_mangle]
pub extern "C" fn ee_tech_epoch_of(id: i32) -> i32 {
    epoch_of(id)
}

/// Forgets every node's epoch (a new tech tree file).
#[no_mangle]
pub extern "C" fn ee_tech_clear_node_epochs() {
    NODE_EPOCHS.write().unwrap_or_else(|e| e.into_inner()).clear();
}

/// Records the epoch block a node was loaded in (`EERegisterTechNode`).
#[no_mangle]
pub extern "C" fn ee_tech_register_node(id: i32, epoch: i32) {
    NODE_EPOCHS.write().unwrap_or_else(|e| e.into_inner()).insert(id, epoch);
}

#[cfg(test)]
mod tests {
    use super::*;

    // one test: the tables are the process's (other tests read the default count)
    #[test]
    fn epochs_and_nodes() {
        assert_eq!(epoch_count(), EE_TECH_DEFAULT_EPOCH_COUNT);
        assert!(is_epoch_id(0) && is_epoch_id(14) && !is_epoch_id(15) && !is_epoch_id(-1));
        assert_eq!(epoch_of(3), 3);
        assert_eq!(epoch_of(1074), -1);
        ee_tech_register_node(1074, 0);
        ee_tech_register_node(5080, 4);
        assert_eq!(ee_tech_epoch_of(1074), 0);
        assert_eq!(ee_tech_epoch_of(5080), 4);
        ee_tech_set_epoch_count(16);
        assert!(is_epoch_id(15));
        ee_tech_set_epoch_count(EE_TECH_DEFAULT_EPOCH_COUNT);
        ee_tech_clear_node_epochs();
        assert_eq!(epoch_of(5080), -1);
    }
}
