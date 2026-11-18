//! Read-only views of a player's tech tree (`EETechTree`) and its nodes (`EETechTreeNode`, an epoch's
//! `EETechTreeEpoch` too) and of a computer player's record (`EeCpFields`), through the registered layout (lane A,
//! round 2: the Lua AI's ai.techs, ai.epoch, ai.class, ai.wonders and ai.tuning).
//!
//! The plain members are the tech tree's Rust records (ee_sim::tech::nodes: `EeTechTreeFields`,
//! `EeTechNodeFields`, `EeTechNodeDataFields`), read in place. The tree's containers (the epochs' list, each epoch's
//! `std::map` of nodes) are the C++'s: a lookup goes through the tree's own host (`EETechHost`, the walks' host;
//! its address is in the layout), the same lookups the walks make. Node methods with a body of their own
//! (`GetIsAvailable`, `GetIsObsolete`, `GetCost`: the walks and the capture's records) are not here: a caller asks
//! the C++ for them.
#![allow(clippy::unnecessary_cast)]

use std::marker::PhantomData;
use std::os::raw::{c_long, c_ulong, c_void};

use crate::cp::fields::EeCpFields;
use crate::tech::nodes::{
    EeTechHostFn, EeTechHostIo, EeTechNodeDataFields, EeTechNodeFields, EeTechNodeRef, EeTechTreeFields,
    EE_TECH_HOST_EPOCH, EE_TECH_HOST_EPOCHS, EE_TECH_HOST_EPOCH_NODES, EE_TECH_HOST_NODE,
};
use crate::world::layout::layout;
use crate::world::view::{PlayerView, Record, WorldRead};

/// A player's tech tree (an `EETechTree*`).
#[derive(Clone, Copy)]
pub struct TechTreeView<'t> {
    object: usize,
    _t: PhantomData<&'t WorldRead>,
}

/// A node of a tech tree (an `EETechTreeNode*`; an epoch is one too).
#[derive(Clone, Copy)]
pub struct NodeView<'t> {
    object: usize,
    _t: PhantomData<&'t WorldRead>,
}

/// The pointer member at `object + offset` (0: none).
///
/// # Safety
/// The bytes there are a live pointer member.
unsafe fn pointer(object: usize, offset: u64) -> usize {
    // SAFETY: the caller's contract
    unsafe { std::ptr::read((object + offset as usize) as *const usize) }
}

impl<'t> PlayerView<'t> {
    /// `mTechnologies` (None: the player has no tree yet)
    pub fn tech_tree(&self) -> Option<TechTreeView<'t>> {
        // SAFETY: a live player's pointer member
        let tree = unsafe { pointer(self.handle() as usize, layout().player_technologies) };
        (tree != 0).then_some(TechTreeView { object: tree, _t: PhantomData })
    }
    /// The computer player's record (`EeCpFields`), when this player object is an `EEComputerPlayer` (the caller
    /// knows it: a computer player's own handle).
    ///
    /// # Safety
    /// The player object is an `EEComputerPlayer`.
    pub unsafe fn computer_fields(&self) -> Record<'t, EeCpFields> {
        Record::at_offset(self.handle() as usize, layout().cp_fields)
    }
}

impl<'t> TechTreeView<'t> {
    /// The view of the tree `handle` (None for 0).
    ///
    /// # Safety
    /// `handle` is 0 or a live `EETechTree` while `token` lives.
    pub unsafe fn of(token: &'t WorldRead, handle: u64) -> Option<Self> {
        let _ = token;
        (handle != 0).then_some(TechTreeView { object: handle as usize, _t: PhantomData })
    }
    pub fn handle(&self) -> u64 {
        self.object as u64
    }
    /// The tree's record (`EeTechTreeFields`)
    pub fn fields(&self) -> Record<'t, EeTechTreeFields> {
        Record::at_offset(self.object, layout().tech_tree_fields)
    }
    /// `mCurrentEpoch`
    pub fn current_epoch(&self) -> i64 {
        let v: c_long = crate::world_field!(self.fields(), mCurrentEpoch);
        v as i64
    }

    fn host(&self, op: i32, io: &mut EeTechHostIo) -> i32 {
        let at = layout().tech_host as usize;
        if at == 0 {
            return 0;
        }
        // SAFETY: the registered address of `EETechHost` (EEWorldLayout.cpp), a function of this signature
        let f: EeTechHostFn = unsafe { std::mem::transmute::<usize, EeTechHostFn>(at) };
        // SAFETY: the tree's own host over the live tree (its lookups read the containers only)
        unsafe { f(self.object as *mut c_void, op, io) }
    }

    fn node_of(r: EeTechNodeRef) -> Option<NodeView<'t>> {
        (!r.obj.is_null()).then_some(NodeView { object: r.obj as usize, _t: PhantomData })
    }

    /// `mEpochs.size()`
    pub fn epochs(&self) -> i64 {
        let mut io = EeTechHostIo::default();
        self.host(EE_TECH_HOST_EPOCHS, &mut io);
        io.v[0] as i64
    }
    /// `&mEpochs[index]` (None past the list)
    pub fn epoch(&self, index: i64) -> Option<NodeView<'t>> {
        if index < 0 || index >= self.epochs() {
            return None;
        }
        let mut io = EeTechHostIo::default();
        io.v[0] = index as i32;
        self.host(EE_TECH_HOST_EPOCH, &mut io);
        Self::node_of(io.node)
    }
    /// `Get(id)` (None: no such node)
    pub fn node(&self, id: i64) -> Option<NodeView<'t>> {
        let mut io = EeTechHostIo::default();
        io.v[0] = id as i32;
        if self.host(EE_TECH_HOST_NODE, &mut io) == 0 {
            return None;
        }
        Self::node_of(io.node)
    }
    /// `GetEpochNodeIDs(epoch, out)`: the epoch's node ids (its map's keys, in order) appended to `out`; nothing for
    /// an epoch off the list (`kTechEpochPrehistoric` is 0)
    pub fn epoch_node_ids(&self, epoch: i64, out: &mut Vec<c_long>) {
        let Some(e) = self.epoch(epoch) else { return };
        let mut io = EeTechHostIo { node: e.reference(), ..Default::default() };
        let n = self.host(EE_TECH_HOST_EPOCH_NODES, &mut io).max(0) as usize;
        let mut refs = vec![EeTechNodeRef::default(); n];
        let mut io =
            EeTechHostIo { node: e.reference(), nodes: refs.as_mut_ptr(), count: n as u32, ..Default::default() };
        let filled = (self.host(EE_TECH_HOST_EPOCH_NODES, &mut io).max(0) as usize).min(n);
        // a node's key is its data's id (EETechTreeEpoch::Load inserts it so)
        out.extend(refs[..filled].iter().filter_map(|r| Self::node_of(*r)).map(|v| v.id() as c_long));
    }
}

impl<'t> NodeView<'t> {
    /// The view of the node `handle` (an `EETechTreeNode*`, None for 0).
    ///
    /// # Safety
    /// `handle` is 0 or a live node while `token` lives.
    pub unsafe fn of(token: &'t WorldRead, handle: u64) -> Option<Self> {
        let _ = token;
        (handle != 0).then_some(NodeView { object: handle as usize, _t: PhantomData })
    }
    pub fn handle(&self) -> u64 {
        self.object as u64
    }
    /// The node as the tree's host takes it
    fn reference(&self) -> EeTechNodeRef {
        let l = layout();
        EeTechNodeRef {
            obj: self.object as *mut c_void,
            node: (self.object + l.node_fields as usize) as *mut EeTechNodeFields,
            data: self.data_address() as *mut EeTechNodeDataFields,
        }
    }
    fn data_address(&self) -> usize {
        let l = layout();
        // SAFETY: a live node's pointer member
        let data = unsafe { pointer(self.object, l.node_data) };
        if data == 0 {
            0
        } else {
            data + l.node_data_fields as usize
        }
    }
    /// The node's record (`EeTechNodeFields`)
    pub fn fields(&self) -> Record<'t, EeTechNodeFields> {
        Record::at_offset(self.object, layout().node_fields)
    }
    /// `mData`'s record (`EeTechNodeDataFields`; None while the node has no data)
    pub fn data(&self) -> Option<Record<'t, EeTechNodeDataFields>> {
        let at = self.data_address();
        (at != 0).then(|| Record::at_offset(at, 0))
    }
    /// `mData->mID` (-1 without data)
    pub fn id(&self) -> i64 {
        self.data().map_or(-1, |d| {
            let v: c_long = crate::world_field!(d, mID);
            v as i64
        })
    }
    /// `mIsResearched`
    pub fn researched(&self) -> bool {
        crate::world_field!(self.fields(), mIsResearched)
    }
    /// `mEpoch`, `mObsoleteEpoch`
    pub fn epoch(&self) -> i64 {
        let v: c_long = crate::world_field!(self.fields(), mEpoch);
        v as i64
    }
    pub fn obsolete_epoch(&self) -> i64 {
        let v: c_long = crate::world_field!(self.fields(), mObsoleteEpoch);
        v as i64
    }
    /// an epoch's `GetRequiredSOL()`, `GetCurrentSOL()` (plain members)
    pub fn required_sol(&self) -> i64 {
        let v: c_long = crate::world_field!(self.fields(), mRequiredSOL);
        v as i64
    }
    pub fn current_sol(&self) -> i64 {
        let v: c_long = crate::world_field!(self.fields(), mCurrentSOL);
        v as i64
    }
    /// `GetNumUnitClasses()`: `mData->mUnitClasses.size()` (0 without data)
    pub fn unit_classes_len(&self) -> usize {
        let (begin, end) = self.unit_classes();
        (end - begin) / std::mem::size_of::<c_ulong>()
    }
    fn unit_classes(&self) -> (usize, usize) {
        let l = layout();
        // SAFETY: a live node's pointer member
        let data = unsafe { pointer(self.object, l.node_data) };
        if data == 0 {
            return (0, 0);
        }
        let v = data + l.node_data_unit_classes as usize;
        let w = std::mem::size_of::<usize>() as u64;
        // SAFETY: the data's vector: its first two words are its begin and end (EEWorldLayout.cpp checks it)
        unsafe { (pointer(v, 0), pointer(v, w)) }
    }
    /// `GetUnitClass(i)`: `mData->mUnitClasses[i]`, `mData->mDefaultUnitClass` past the end
    pub fn unit_class(&self, i: usize) -> i64 {
        if i < self.unit_classes_len() {
            // SAFETY: an element of the live vector
            let v: c_ulong = unsafe { std::ptr::read((self.unit_classes().0 as *const c_ulong).add(i)) };
            return v as c_long as i64;
        }
        self.data().map_or(0, |d| {
            let v: c_long = crate::world_field!(d, mDefaultUnitClass);
            v as i64
        })
    }
}

#[cfg(test)]
#[path = "tech_view_tests.rs"]
mod tests;
