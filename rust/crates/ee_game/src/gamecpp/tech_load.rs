//! The tech tree's loads and copies in Rust (game core lane, round 10; was EETechTree.cpp's constructor, SumCosts'
//! TSUM capture and operator=, EETechTreeEpoch.cpp's Load, GetBuildTime and operator=, EETechTreeNode.cpp's
//! destructor, ClearClassCosts, Unload, Load, NeedsUnitMedia, SetButton and operator=): the tech tree file's epoch
//! blocks and node records into the C++ nodes (the record's fill and the class cost tables are ee_sim tech's), the
//! trees' deep copies (the node data stays shared), the nodes' release. Each C++ call or member is a case of
//! EETechTreeEpoch.cpp's `EECore10OpB7` or of round 9's tech host (`Tk` ops); the methods forward here.
//!
//! Simulation code (RUST_PLAN 2.1): the loads and copies are the C++'s, call for call; TSUM and TNMT are written
//! where the C++ wrote them.

#![allow(clippy::not_unsafe_ptr_arg_deref, clippy::missing_safety_doc)]

use std::cell::Cell;
use std::ffi::{c_long, c_ulong, c_void};

use ee_sim::tech::capture_rec::ee_tech_capture_walk_begin;
use ee_sim::tech::ffi::{ee_tech_class_costs, ee_tech_epoch_build_time, ee_tech_needs_unit_media};
use ee_sim::tech::nodes::{
    ee_tech_fill_from_record, ee_tech_node_assign, ee_tech_walk_tree, EeTechHostIo, EeTechNodeDataFields,
    EeTechNodeFields, EeTechTreeFields, EeTechTreeOp, EeTechTreeRecord, EE_TECH_HOST_READ_BOOL, EE_TECH_HOST_READ_LONG,
    EE_TECH_HOST_WRITE_BOOL, EE_TECH_HOST_WRITE_LONG,
};
use ee_sim::tech::nodes_capture::ee_tech_capture_node_walk;
use ee_sim::tech::tech_ids::{ee_tech_epoch_count, ee_tech_epoch_of, ee_tech_register_node, ee_tech_set_epoch_title};

use super::persistent::Pf;
use super::tech::{ee_gamecpp_tech_epoch_delete_nodes, ee_gamecpp_tech_host, ee_gamecpp_tech_sum_costs};
use super::{h, hp, pa, rec, O, P};

/// `kErrorOK`.
const TB_OK: i64 = 0;
/// `kTechFirstNodeID` (static_assert'd in EETechTreeEpoch.cpp): the first id of a node (an epoch's ids are below).
const TB_FIRST_NODE_ID: c_long = 1000;
/// `IDS_PREHISTORIC_AGE`: a new tree's epoch title.
const TB_PREHISTORIC_AGE: c_long = 30145;

pub(super) fn tree_fields(tree: P) -> *mut EeTechTreeFields {
    // SAFETY: the tree's record (EETechTree's second base; NULL for a NULL tree)
    unsafe { rec::<EeTechTreeFields>(O::TkTreeFields, tree) }
}

pub(super) fn node_fields(node: P) -> *mut EeTechNodeFields {
    // SAFETY: the node's record (RTSTechTreeNode's base)
    unsafe { rec::<EeTechNodeFields>(O::TkNodeFields, node) }
}

pub(super) fn node_data(node: P) -> *mut EeTechNodeDataFields {
    // SAFETY: the node's data record (RTSTechTreeNodeData's base; NULL without data)
    unsafe { rec::<EeTechNodeDataFields>(O::TkNodeData, node) }
}

pub(super) fn data_of(node: P) -> EeTechNodeDataFields {
    // SAFETY: a node with data: its record, copied out
    unsafe { *node_data(node) }
}

pub(super) fn fields_of(node: P) -> EeTechNodeFields {
    // SAFETY: the node's record, copied out
    unsafe { *node_fields(node) }
}

/// The node's unit classes (its data's `mUnitClasses`).
fn unit_classes(node: P) -> Vec<c_ulong> {
    let count = h(O::TbClassCount, node, &[]);
    (0..count).map(|i| h(O::TbClassAt, node, &[i]) as c_ulong).collect()
}

/// The record and the unit classes of a node or an epoch, after `mData` is made: the record read, the unit class
/// count (its status the result) and each class read, the record's fields filled in.
fn read_record(node: P, file: P, epoch: bool) -> (EeTechTreeRecord, i64) {
    // SAFETY: a plain record of integers, floats and bytes, all zero before the read
    let mut record: EeTechTreeRecord = unsafe { std::mem::zeroed() };
    let f = Pf(file);
    f.read_chars(&mut record as *mut EeTechTreeRecord as *mut u8, std::mem::size_of::<EeTechTreeRecord>() as c_ulong);
    let mut count: c_ulong = 0;
    let result = f.read_ulong(&mut count);
    h(O::TbClassesResize, node, &[count as i64]);
    for j in 0..count {
        h(O::TbClassRead, node, &[pa(file), j as i64]);
    }
    // SAFETY: the node's records and the read record are live
    unsafe { ee_tech_fill_from_record(node_fields(node), node_data(node), &record, epoch) };
    (record, result)
}

/// EETechTreeNode::Load: the node's record from the tech tree file, a cost table per unit class scaled by its
/// factor, its epoch and its button.
#[no_mangle]
pub extern "C" fn ee_gamecpp_tech_node_load(node: *mut c_void, file: *mut c_void, tree: *mut c_void) -> c_long {
    let node = node as P;
    let file = file as P;
    hp(O::TbNewData, node, &[]);
    h(O::TbSetTree, node, &[tree as i64]);
    let (record, result) = read_record(node, file, false);
    let data = data_of(node);
    let classes = h(O::TbClassCount, node, &[]);
    for k in 0..classes {
        // the k-th class's table: every cost times the record's factor of that class (ee_tech class_costs)
        let base: [i32; 7] = std::array::from_fn(|r| data.mCosts[r] as i32);
        let factors = [record.mFactor1, record.mFactor2, record.mFactor3, record.mFactor4];
        let mut table = [0i32; 7];
        // SAFETY: seven costs, four factors, seven out
        unsafe { ee_tech_class_costs(base.as_ptr(), k as u32, factors.as_ptr(), table.as_mut_ptr()) };
        let mut a = [k; 8];
        for r in 0..7 {
            a[r + 1] = table[r] as i64;
        }
        h(O::TbCostsInsert, node, &a);
    }
    // a node's epoch: set by its epoch's Load (the block holding it)
    let epoch = ee_tech_epoch_of(data.mID as i32) as c_long;
    // SAFETY: the node's record
    unsafe { (*node_fields(node)).mEpoch = epoch };
    h(O::TbSetButton, node, &[record.mButton as i64]);
    result as c_long
}

/// EETechTreeEpoch::Load: the epoch's record and its tech nodes from the tech tree file (nodes with epoch ids are
/// dropped).
#[no_mangle]
pub extern "C" fn ee_gamecpp_tech_epoch_load(epoch: *mut c_void, file: *mut c_void, tree: *mut c_void) -> c_long {
    let epoch = epoch as P;
    let file = file as P;
    let tree = tree as P;
    h(O::TbSetTree, epoch, &[tree as i64]);
    hp(O::TbNewData, epoch, &[]);
    let mut node_count: c_ulong = 0;
    Pf(file).read_ulong(&mut node_count);
    let (record, mut result) = read_record(epoch, file, true);
    let data = data_of(epoch);
    if tree == hp(O::TbGlobalTree, 0, &[]) {
        // the epochs' titles (EEEpochTitleID: ee_sim tech tech_ids.rs)
        ee_tech_set_epoch_title(data.mID as i32, fields_of(epoch).mTitleID as i32);
    }
    h(O::TbSetButton, epoch, &[record.mButton as i64]);
    if result == TB_OK {
        for _ in 0..node_count {
            if result != TB_OK {
                break;
            }
            let node = hp(O::TbNewNode, 0, &[]);
            result = ee_gamecpp_tech_node_load(
                node as *mut c_void,
                file as *mut c_void,
                hp(O::TkNodeTree, epoch, &[]) as *mut c_void,
            ) as i64;
            let id = data_of(node).mID;
            if result == TB_OK && id >= TB_FIRST_NODE_ID {
                // the node belongs to the epoch whose block holds it (the original: id / 1000 - 1)
                // SAFETY: the node's record
                unsafe { (*node_fields(node)).mEpoch = data.mID };
                ee_tech_register_node(id as i32, data.mID as i32);
                h(O::TbEpochInsert, epoch, &[node as i64]);
            } else {
                h(O::TkNodeUnload, node, &[]);
                h(O::TkNodeDelete, node, &[]);
            }
        }
    }
    result as c_long
}

/// EETechTreeNode::operator=: another node's state, its data (with its class cost tables) shared.
#[no_mangle]
pub extern "C" fn ee_gamecpp_tech_node_assign(node: *mut c_void, other: *const c_void) -> *mut c_void {
    let (n, o) = (node as P, other as P);
    if n != o {
        // the record's fields: ee_tech nodes.rs assign (the original's copy of mData->mMakesUnit onto the same
        // shared data was a self-assignment, dropped); the shared block already holds the class cost tables
        h(O::TbShareData, n, &[o as i64]);
        // SAFETY: two distinct live nodes' records
        unsafe { ee_tech_node_assign(node_fields(n), node_fields(o), false) };
        h(O::TbCopyButton, n, &[o as i64]);
    }
    node
}

/// EETechTreeEpoch::operator=: another epoch's state, its nodes deep-copied for this epoch's tree (the node data
/// stays shared).
#[no_mangle]
pub extern "C" fn ee_gamecpp_tech_epoch_assign(epoch: *mut c_void, other: *const c_void) -> *mut c_void {
    let (e, o) = (epoch as P, other as P);
    h(O::TbShareData, e, &[o as i64]);
    h(O::TbCopyButton, e, &[o as i64]);
    // SAFETY: the two epochs' records (the record's fields, counting off: ee_tech nodes.rs assign)
    unsafe { ee_tech_node_assign(node_fields(e), node_fields(o), true) };
    ee_gamecpp_tech_epoch_delete_nodes(epoch, false);
    let count = h(O::TkNodeCount, o, &[]);
    for i in 0..count {
        let copy = hp(O::TbNewNode, 0, &[]);
        ee_gamecpp_tech_node_assign(copy as *mut c_void, hp(O::TkNodeAt, o, &[i]) as *const c_void);
        h(O::TbSetTree, copy, &[h(O::TkNodeTree, e, &[])]);
        h(O::TbEpochInsert, e, &[copy as i64]);
    }
    epoch
}

/// The EETechTree constructor's body: every epoch, the prehistoric one current, the end the last.
#[no_mangle]
pub extern "C" fn ee_gamecpp_tech_tree_init(tree: *mut c_void) {
    let tree = tree as P;
    let t = tree_fields(tree);
    // SAFETY: the new tree's record
    unsafe {
        (*t).mCurrentEpoch = 0;
        (*t).mStartEpoch = 0;
        (*t).mEndEpoch = ee_tech_epoch_count() as c_long - 1;
        (*t).mMediaID = 0;
        (*t).mEpochNameID = TB_PREHISTORIC_AGE;
        (*t).mIsLoaded = false;
        (*t).mSaved = std::ptr::null_mut();
        (*t).mEpochsBeforeWonders = 1;
    }
    h(O::TkEpochsResize, tree, &[ee_tech_epoch_count() as i64]);
}

/// EETechTree::operator=: another tree's epochs (deep copies), epoch range and player; the copy is not loaded.
#[no_mangle]
pub extern "C" fn ee_gamecpp_tech_tree_assign(tree: *mut c_void, other: *const c_void) -> *mut c_void {
    let (t, o) = (tree as P, other as P);
    let (tf, of) = (tree_fields(t), tree_fields(o));
    // SAFETY: the two trees' records
    unsafe {
        (*tf).mCurrentEpoch = (*of).mCurrentEpoch;
        (*tf).mStartEpoch = (*of).mStartEpoch;
        (*tf).mEndEpoch = (*of).mEndEpoch;
    }
    h(O::TbSetPlayerIndex, t, &[h(O::TkPlayerIndex, o, &[])]);
    h(O::TbCopyPlayer, t, &[o as i64]);
    let count = h(O::TkEpochCount, o, &[]);
    h(O::TkEpochsResize, t, &[count]);
    for i in 0..count {
        let epoch = hp(O::TkEpochAt, t, &[i]);
        h(O::TbSetTree, epoch, &[t as i64]);
        ee_gamecpp_tech_epoch_assign(epoch as *mut c_void, hp(O::TkEpochAt, o, &[i]) as *const c_void);
    }
    // SAFETY: the tree's record
    unsafe { (*tf).mIsLoaded = false };
    tree
}

/// The node's destructor (`destroy`: its unit media unloaded when loaded and its unit classes given their default
/// buttons back) and ClearClassCosts (the class cost tables freed).
#[no_mangle]
pub extern "C" fn ee_gamecpp_tech_node_free(node: *mut c_void, destroy: bool) {
    let n = node as P;
    if destroy {
        let tree = hp(O::TkNodeTree, n, &[]);
        if fields_of(n).mIsUnitMediaLoaded {
            let player = hp(O::TkGetPlayer, tree, &[]);
            if player != 0 {
                h(O::TkUnloadClassMedia, player, &[data_of(n).mUnitClass as i64]);
            }
        }
        if !node_data(n).is_null() {
            for class in unit_classes(n) {
                let player = hp(O::TkGetPlayer, tree, &[]);
                if player != 0 {
                    h(O::TbSwapButton, player, &[class as i64, h(O::TbButton, n, &[]), 0]);
                }
            }
        }
    }
    if node_data(n).is_null() {
        return;
    }
    let count = h(O::TbCostsCount, n, &[]);
    for i in 0..count {
        h(O::TbCostsDelete, n, &[i]);
    }
    h(O::TbCostsClear, n, &[]);
}

/// EETechTreeNode::Unload: the node's data deleted; 0.
#[no_mangle]
pub extern "C" fn ee_gamecpp_tech_node_unload(node: *mut c_void) -> c_long {
    if !node_data(node as P).is_null() {
        h(O::TbDeleteData, node as P, &[]);
    }
    0
}

/// EETechTreeNode::SetButton: a new button, swapped into the node's unit classes.
#[no_mangle]
pub extern "C" fn ee_gamecpp_tech_node_set_button(node: *mut c_void, button: c_ulong) {
    let n = node as P;
    for class in unit_classes(n) {
        let player = hp(O::TkGetPlayer, hp(O::TkNodeTree, n, &[]), &[]);
        let new = hp(O::TbButtonGet, 0, &[button as i64]);
        h(O::TbSwapButton, player, &[class as i64, h(O::TbButton, n, &[]), new as i64]);
    }
    h(O::TbSetButton, n, &[button as i64]);
}

/// EETechTreeNode::NeedsUnitMedia (`captured`: TNMT records the answer) and NeedsUnitMediaUncaptured: whether the
/// node makes a building or mine whose media this game needs (ee_tech tree.rs needs_unit_media).
#[no_mangle]
pub extern "C" fn ee_gamecpp_tech_needs_unit_media(node: *mut c_void, captured: bool) -> bool {
    let n = node as P;
    // the node's class (none for an id 0 or less or an empty table entry) and its family
    let class_family = || {
        let unit_class = data_of(n).mUnitClass;
        let class = if unit_class > 0 { hp(O::TkClassAt, 0, &[unit_class as i64]) } else { 0 };
        (unit_class, if class != 0 { Some(h(O::TbClassFamily, class, &[]) as i32) } else { None })
    };
    let (_, family) = class_family();
    let f = fields_of(n);
    let tree = hp(O::TkNodeTree, n, &[]);
    // SAFETY: the node's tree's record
    let start = unsafe { (*tree_fields(tree)).mStartEpoch };
    let result = ee_tech_needs_unit_media(
        family.is_some(),
        family.unwrap_or(-1),
        f.mScenEditorObsoleted,
        f.mObsoleteEpoch as i32,
        start as i32,
    );
    if captured {
        ee_tech_capture_walk_begin(); // TNMT (TEST ONLY: ee_sim tech nodes_capture.rs)
        let (unit_class, family) = class_family();
        // SAFETY: the node's, its data's and its tree's records
        unsafe {
            ee_tech_capture_node_walk(
                10,
                std::ptr::null(),
                -1,
                node_fields(n),
                node_data(n),
                tree_fields(tree),
                unit_class as i32,
                family.unwrap_or(-1),
                result as i32,
            )
        };
    }
    result
}

/// EETechTreeEpoch::GetBuildTime: the epoch's research time (computer players need half of it); TNMT records it.
#[no_mangle]
pub extern "C" fn ee_gamecpp_tech_epoch_build_time(epoch: *mut c_void) -> c_long {
    let e = epoch as P;
    // (TNWK, TEST ONLY: asked once, as the test did)
    let human = h(O::TbIsHuman, hp(O::TkGetPlayer, hp(O::TkNodeTree, e, &[]), &[]), &[]) as i32;
    let build_time = data_of(e).mBuildTime as i32;
    let result = ee_tech_epoch_build_time(human, build_time);
    ee_tech_capture_walk_begin(); // TNMT (TEST ONLY)
                                  // SAFETY: no records (the epoch's kind 6 carries its values only)
    unsafe {
        ee_tech_capture_node_walk(
            6,
            std::ptr::null(),
            -1,
            std::ptr::null(),
            std::ptr::null(),
            std::ptr::null(),
            human,
            build_time,
            result,
        )
    };
    result as c_long
}

/// The saved game's walks' context: the tree and the file.
struct TbSave {
    tree: P,
    file: P,
}

/// The saved game's walks' host: the file's long and bool (as EESavePrimitive reads and writes them), the rest the
/// tech host's.
unsafe extern "C" fn tb_save_host(context: *mut c_void, op: i32, io: *mut EeTechHostIo) -> i32 {
    // SAFETY: the context ee_gamecpp_tech_tree_save passed, alive for the walk
    let c = unsafe { &*(context as *const TbSave) };
    if !(EE_TECH_HOST_READ_LONG..=EE_TECH_HOST_WRITE_BOOL).contains(&op) {
        // SAFETY: the walk's io
        return unsafe { ee_gamecpp_tech_host(c.tree as *mut c_void, op, io as *mut c_void) };
    }
    // SAFETY: the walk's io
    let io = unsafe { &mut *io };
    let f = Pf(c.file);
    let result = match op {
        EE_TECH_HOST_READ_LONG => {
            let mut v = io.v[0] as c_long;
            let r = f.read_long(&mut v);
            io.v[0] = v as i32;
            r
        }
        EE_TECH_HOST_READ_BOOL => {
            let mut v = io.v[0] != 0;
            let r = f.read_bool(&mut v);
            io.v[0] = v as i32;
            r
        }
        EE_TECH_HOST_WRITE_LONG => f.write_long(io.v[0] as c_long),
        _ => f.write_bool(io.v[0] != 0),
    };
    result as i32
}

/// EETechTree::Load (`load`: the saved research, obsolete and editor-obsoleted ids, the epochs and each epoch's
/// state) and Save (the same written; the last epoch's write result) of a saved game (ee_tech tree_load, tree_save).
#[no_mangle]
pub extern "C" fn ee_gamecpp_tech_tree_save(
    tree: *mut c_void,
    file: *mut c_void,
    load: bool,
    major: c_ulong,
    minor: c_ulong,
) -> c_long {
    let mut context = TbSave { tree: tree as P, file: file as P };
    let op = if load { EeTechTreeOp::Load } else { EeTechTreeOp::Save };
    // SAFETY: the context outlives the walk; the tree's record is live
    unsafe {
        ee_tech_walk_tree(
            &mut context as *mut TbSave as *mut c_void,
            tb_save_host,
            op,
            tree_fields(tree as P),
            major as i32,
            minor as i32,
            std::ptr::null_mut(),
        ) as c_long
    }
}

/// A capture record (EESimCaptureRecord): the payload's bytes, committed whole.
#[derive(Default)]
pub(super) struct TbRecord(pub(super) Vec<u8>);

impl TbRecord {
    pub(super) fn i32(&mut self, v: i32) {
        self.0.extend_from_slice(&v.to_le_bytes());
    }
    pub(super) fn u32(&mut self, v: u32) {
        self.0.extend_from_slice(&v.to_le_bytes());
    }
    pub(super) fn bytes(&mut self, v: &[u8]) {
        self.0.extend_from_slice(v);
    }
    pub(super) fn commit(self, tag: &[u8; 4]) {
        // SAFETY: our payload
        unsafe { ee_sim::app::capture::ee_app_capture_commit(u32::from_le_bytes(*tag), self.0.as_ptr(), self.0.len()) };
    }
}

thread_local! {
    /// SumCosts' depth: only the outermost call is recorded (the walk's own recursion is not a call of the game's).
    static TB_SUM_DEPTH: Cell<i32> = const { Cell::new(0) };
}

/// EETechTree::SumCosts: the resource costs of a tech and every unresearched prerequisite and upgrade path to it;
/// TSUM (TEST ONLY) records the outermost call: player, id, unit class, the seven costs before and after.
#[no_mangle]
pub unsafe extern "C" fn ee_gamecpp_tech_sum_costs_captured(
    tree: *mut c_void,
    id: c_long,
    unit_class: c_long,
    costs: *mut c_long,
) {
    if ee_sim::app::capture::ee_app_capture_active() && TB_SUM_DEPTH.get() == 0 {
        TB_SUM_DEPTH.set(TB_SUM_DEPTH.get() + 1);
        let mut r = TbRecord::default();
        r.i32(h(O::TkPlayerIndex, tree as P, &[]) as i32);
        r.i32(id as i32);
        r.i32(unit_class as i32);
        for i in 0..7 {
            // SAFETY: the caller's seven costs
            r.i32(unsafe { *costs.add(i) } as i32);
        }
        // SAFETY: the caller's contract
        unsafe { ee_gamecpp_tech_sum_costs(tree, id, unit_class, costs) };
        for i in 0..7 {
            // SAFETY: as above
            r.i32(unsafe { *costs.add(i) } as i32);
        }
        r.commit(b"TSUM");
        TB_SUM_DEPTH.set(TB_SUM_DEPTH.get() - 1);
        return;
    }
    // SAFETY: the caller's contract
    unsafe { ee_gamecpp_tech_sum_costs(tree, id, unit_class, costs) };
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_record_appends_little_endian() {
        let mut r = TbRecord::default();
        r.i32(-1);
        r.u32(2);
        r.bytes(&[7]);
        assert_eq!(r.0, vec![0xff, 0xff, 0xff, 0xff, 2, 0, 0, 0, 7]);
    }

    #[test]
    fn the_tech_record_is_the_files() {
        assert_eq!(std::mem::size_of::<EeTechTreeRecord>(), 0x10c);
        assert_eq!((TB_FIRST_NODE_ID, TB_PREHISTORIC_AGE), (1000, 30145));
    }

    #[test]
    #[should_panic(expected = "no C++")]
    fn a_node_load_is_host_calls() {
        read_record(1, 2, false);
    }
}
