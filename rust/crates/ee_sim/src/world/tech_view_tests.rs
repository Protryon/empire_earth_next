//! The tech tree's views over fake objects laid out like the C++ classes, and a fake tree host.

use std::mem::offset_of;
use std::os::raw::{c_long, c_ulong, c_void};

use super::*;
use crate::tech::nodes::{EeTechNodeDataFields, EeTechNodeFields, EeTechTreeFields};
use crate::world::layout::{with_test_layout, EeWorldLayout};

#[repr(C)]
struct FakeData {
    map: [usize; 3],
    fields: EeTechNodeDataFields,
    classes: [usize; 3],
}

#[repr(C)]
struct FakeNode {
    vptr: usize,
    fields: EeTechNodeFields,
    data: usize,
}

#[repr(C)]
struct FakeTree {
    vptr: usize,
    fields: EeTechTreeFields,
    epochs: [usize; 2],
    nodes: [usize; 3],
}

#[repr(C)]
struct FakePlayer {
    rts: [u8; 16],
    tree: usize,
    cp: EeCpFields,
}

fn layout_for(host: usize) -> EeWorldLayout {
    EeWorldLayout {
        size: std::mem::size_of::<EeWorldLayout>() as u64,
        player_technologies: offset_of!(FakePlayer, tree) as u64,
        cp_fields: offset_of!(FakePlayer, cp) as u64,
        tech_tree_fields: offset_of!(FakeTree, fields) as u64,
        node_fields: offset_of!(FakeNode, fields) as u64,
        node_data: offset_of!(FakeNode, data) as u64,
        node_data_fields: offset_of!(FakeData, fields) as u64,
        node_data_unit_classes: offset_of!(FakeData, classes) as u64,
        tech_host: host as u64,
        ..Default::default()
    }
}

/// The fake tree's host: two epochs (the fake tree's `epochs`), epoch 1 holding the three `nodes`; `Get` finds a
/// node by its data's id
unsafe extern "C" fn fake_host(tree: *mut c_void, op: i32, io: *mut EeTechHostIo) -> i32 {
    let t = &*(tree as *const FakeTree);
    let io = &mut *io;
    let r = |at: usize| {
        let n = at as *mut FakeNode;
        EeTechNodeRef { obj: n.cast(), node: std::ptr::addr_of_mut!((*n).fields), data: (*n).data as *mut _ }
    };
    let id = |at: usize| (*((*(at as *const FakeNode)).data as *const FakeData)).fields.mID as i32;
    match op {
        EE_TECH_HOST_EPOCHS => io.v[0] = 2,
        EE_TECH_HOST_EPOCH => io.node = r(t.epochs[io.v[0] as usize]),
        EE_TECH_HOST_EPOCH_NODES => {
            if io.node.obj as usize != t.epochs[1] {
                return 0;
            }
            for k in 0..(io.count as usize).min(3) {
                *io.nodes.add(k) = r(t.nodes[k]);
            }
            return 3;
        }
        EE_TECH_HOST_NODE => {
            for &n in t.epochs.iter().chain(t.nodes.iter()) {
                if id(n) == io.v[0] {
                    io.node = r(n);
                    return 1;
                }
            }
            return 0;
        }
        _ => {}
    }
    0
}

fn node(id: c_long, epoch: c_long, classes: &[c_ulong]) -> (Box<FakeNode>, Box<FakeData>, Vec<c_ulong>) {
    let mut v = classes.to_vec();
    let begin = v.as_mut_ptr() as usize;
    let end = begin + v.len() * std::mem::size_of::<c_ulong>();
    let mut d = Box::new(FakeData {
        map: [0; 3],
        fields: EeTechNodeDataFields { mID: id, mDefaultUnitClass: 0x20, mKind: 4, ..Default::default() },
        classes: [begin, end, end],
    });
    d.fields.mCosts[2] = 150;
    let mut f = EeTechNodeFields { mEpoch: epoch, mObsoleteEpoch: 14, mIsResearched: id == 1001, ..Default::default() };
    f.mRequiredSOL = 7;
    f.mCurrentSOL = 3;
    let n = Box::new(FakeNode { vptr: 0, fields: f, data: &mut *d as *mut FakeData as usize });
    (n, d, v)
}

#[test]
fn tree_and_nodes() {
    let e0 = node(0, 0, &[]);
    let e1 = node(1, 1, &[]);
    let a = node(1000, 1, &[0x20, 0x41]);
    let b = node(1001, 1, &[]);
    let c = node(1002, 1, &[0x99]);
    let at = |n: &(Box<FakeNode>, Box<FakeData>, Vec<c_ulong>)| &*n.0 as *const FakeNode as usize;
    let tree = Box::new(FakeTree {
        vptr: 0,
        fields: EeTechTreeFields { mCurrentEpoch: 1, ..Default::default() },
        epochs: [at(&e0), at(&e1)],
        nodes: [at(&a), at(&b), at(&c)],
    });
    let cp = EeCpFields { mDifficulty: 2, ..Default::default() };
    let player = Box::new(FakePlayer { rts: [0; 16], tree: &*tree as *const FakeTree as usize, cp });
    let _l = with_test_layout(layout_for(fake_host as *const () as usize));
    let t = unsafe { WorldRead::begin() };
    let p = unsafe { PlayerView::of(&t, &*player as *const FakePlayer as u64) }.unwrap();
    let cpf = unsafe { p.computer_fields() };
    assert_eq!(crate::world_field!(cpf, mDifficulty), 2);
    let tv = p.tech_tree().unwrap();
    assert_eq!(tv.current_epoch(), 1);
    assert_eq!(tv.epochs(), 2);
    assert!(tv.epoch(2).is_none());
    assert_eq!(tv.epoch(1).unwrap().id(), 1);
    let mut ids = Vec::new();
    tv.epoch_node_ids(0, &mut ids);
    assert!(ids.is_empty());
    tv.epoch_node_ids(1, &mut ids);
    assert_eq!(ids, vec![1000, 1001, 1002]);
    let n = tv.node(1000).unwrap();
    assert_eq!((n.id(), n.epoch(), n.obsolete_epoch(), n.researched()), (1000, 1, 14, false));
    assert_eq!((n.unit_classes_len(), n.unit_class(0), n.unit_class(1), n.unit_class(2)), (2, 0x20, 0x41, 0x20));
    assert_eq!(crate::world_field!(n.data().unwrap(), mCosts)[2], 150);
    assert!(tv.node(1001).unwrap().researched());
    assert!(tv.node(5000).is_none());
    let e = tv.epoch(1).unwrap();
    assert_eq!((e.required_sol(), e.current_sol(), e.unit_classes_len()), (7, 3, 0));
}

#[test]
fn no_tree_no_data() {
    let player = Box::new(FakePlayer { rts: [0; 16], tree: 0, cp: EeCpFields::default() });
    let _l = with_test_layout(layout_for(0));
    let t = unsafe { WorldRead::begin() };
    let p = unsafe { PlayerView::of(&t, &*player as *const FakePlayer as u64) }.unwrap();
    assert!(p.tech_tree().is_none());
    let bare = Box::new(FakeNode { vptr: 0, fields: EeTechNodeFields::default(), data: 0 });
    let n = unsafe { NodeView::of(&t, &*bare as *const FakeNode as u64) }.unwrap();
    assert!(n.data().is_none());
    assert_eq!((n.id(), n.unit_classes_len(), n.unit_class(0)), (-1, 0, 0));
    assert!(unsafe { NodeView::of(&t, 0) }.is_none());
}
