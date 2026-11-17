//! The tech tree's state as Rust records and its walks over them (RUST_PLAN R5b.2/R5b.4, the twenty-ninth round of
//! the world lane).
//!
//! The C++ classes derive from the records, so the members keep their names, C++ types and order:
//! - [`EeTechNodeFields`]: `RTSTechTreeNode`'s, `EETechTreeNode`'s and `EETechTreeEpoch`'s plain members (an epoch is
//!   a node; a node's epoch counters stay 0);
//! - [`EeTechNodeDataFields`]: `RTSTechTreeNodeData`'s plain members (the class cost tables and the unit class list
//!   stay C++ containers);
//! - [`EeTechTreeFields`]: `EETechTree`'s plain members and the saved game's research lists (Rust-owned, behind
//!   `mSaved`).
//!
//! The walks are the C++ bodies in their order: a node's prerequisite, research, availability, obsolescence and
//! editor flag; an epoch's research counter, availability, unlock, research of its nodes, obsolescence; the tree's
//! research, epochs, saved game (`Load`/`Save` and the applied state) and checksum. The world stays C++ behind one
//! host callback ([`EeTechHostFn`]): the epochs' and nodes' containers (`std::map`, read as [`EeTechNodeRef`]s), the
//! entry points the simulation capture wraps (a node's `ResearchNode` and `SetPrerequisite`, an epoch's `Research`,
//! `AddSOL` and `UpdateAvailability`: called through the host so the records keep their order), the events, the
//! interface, the player's statistics, the media, the file and the capture's walk records. No Rust reference to a
//! record lives across a host call: each access copies through the raw pointer.

#![allow(non_snake_case)]
// `c_long` is 32 bits on Windows and 64 elsewhere: the casts are needed there
#![allow(clippy::unnecessary_cast)]

use std::ffi::c_void;
use std::os::raw::{c_long, c_ulong};
use std::ptr;

use crate::tech::tree::{
    self, EeTechEpochCounter, EeTechMakeObsolete, EeTechNode, EeTechTreeState, EeTechUnlockAction,
};

/// `RTSTechTreeNodeData`'s plain members (ee/EEPlayer.h), copied from the tech tree file's record.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeTechNodeDataFields {
    /// the costs by resource (`Load` zeroes all seven, then copies five)
    pub mCosts: [c_long; 7],
    pub mCostResource: c_long,
    pub mRequiredStatistic: c_long,
    /// past the end of the unit classes (`GetUnitClass`)
    pub mDefaultUnitClass: c_long,
    /// a calamity's `EECalamityEnum` (the cast-calamity goal)
    pub mCalamity: c_ulong,
    /// 3 repeatable, 4 a wonder, 5 never researched
    pub mKind: c_long,
    /// an epoch's: its position; a node's: from 1000
    pub mID: c_long,
    pub mResearchedEvent: c_long,
    pub mObsoleteEvent: c_long,
    pub mBuildTime: c_long,
    pub mPrerequisite1: c_long,
    pub mPrerequisite2: c_long,
    pub mPrerequisite3: c_long,
    pub mPrerequisite4: c_long,
    /// the `gUnitClassTable` index of what it builds
    pub mUnitClass: c_long,
    pub mRequiredStatisticValue: c_long,
    pub mNameID: c_long,
    pub mHotKey: c_long,
    pub mMakesUnit: bool,
    pub mCountsTowardEpoch: bool,
    pub mShowInEditor: bool,
    pub mObsoletedByDefault: bool,
    pub mAvailableWithEpoch: bool,
    pub mResearchedWithEpoch: bool,
}

/// A node's and an epoch's plain members (ee/EEPlayer.h: `RTSTechTreeNode` derives from it).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeTechNodeFields {
    pub mIsResearched: bool,
    /// the flag (`GetIsObsolete` adds the obsolete epoch for a node)
    pub mIsObsolete: bool,
    pub mIsAvailable: bool,
    /// 1 the epoch, 2/4/8/0x10 the four prerequisites (never set for an epoch)
    pub mPrerequisiteFlags: u8,
    pub mIsAllowed: bool,
    pub mScenEditorObsoleted: bool,
    pub mIsUnitMediaLoaded: bool,
    /// an epoch's: off between a saved game's load and its applied state
    pub mIsCountingSOL: bool,
    /// the epoch it belongs to
    pub mEpoch: c_long,
    pub mObsoleteEpoch: c_long,
    /// `ResearchNode` counts it down
    pub mResearchDelay: c_long,
    // an epoch's
    pub mRequiredSOL: c_long,
    pub mFirstNodeID: c_long,
    pub mCurrentSOL: c_long,
    pub mNextEpoch: c_long,
    /// the title string (epochs.yaml; 0: 30145 + the epoch)
    pub mTitleID: c_long,
}

/// The saved game's lists of a tree (`EETechTree`'s `mResearchedIDs`, `mObsoleteIDs`, `mScenEditorObsoletedIDs`,
/// `mSavedSOLs`), applied by `ApplySavedState`.
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct EeTechTreeSaved {
    pub researched: Vec<i32>,
    pub obsolete: Vec<i32>,
    pub editor_obsoleted: Vec<i32>,
    /// each epoch's `{id, mCurrentSOL}` as read
    pub sols: Vec<(i32, i32)>,
}

/// `EETechTree`'s plain members (ee/EEPlayer.h: `EETechTree` derives from it).
#[repr(C)]
#[derive(Debug)]
pub struct EeTechTreeFields {
    pub mCurrentEpoch: c_long,
    pub mStartEpoch: c_long,
    pub mEndEpoch: c_long,
    pub mEpochsBeforeWonders: c_long,
    pub mMediaID: c_long,
    /// the epoch name the interface shows (a string id)
    pub mEpochNameID: c_long,
    pub mIsLoaded: bool,
    /// the saved game's lists (null until a load; freed by [`ee_tech_tree_fields_free`])
    pub mSaved: *mut EeTechTreeSaved,
}

impl Default for EeTechTreeFields {
    fn default() -> Self {
        EeTechTreeFields {
            mCurrentEpoch: 0,
            mStartEpoch: 0,
            mEndEpoch: 0,
            mEpochsBeforeWonders: 0,
            mMediaID: 0,
            mEpochNameID: 0,
            mIsLoaded: false,
            mSaved: ptr::null_mut(),
        }
    }
}

/// A node as the host hands it: the C++ object (for the host's calls) and its two records.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct EeTechNodeRef {
    pub obj: *mut c_void,
    pub node: *mut EeTechNodeFields,
    pub data: *mut EeTechNodeDataFields,
}

impl Default for EeTechNodeRef {
    fn default() -> Self {
        EeTechNodeRef { obj: ptr::null_mut(), node: ptr::null_mut(), data: ptr::null_mut() }
    }
}

/// One host call's values.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeTechHostIo {
    pub node: EeTechNodeRef,
    /// the researching unit (`EEComplexUnit*`, may be null)
    pub unit: *mut c_void,
    pub v: [i32; 5],
    /// a buffer of `count` refs (`EPOCH_NODES`)
    pub nodes: *mut EeTechNodeRef,
    pub count: u32,
}

impl Default for EeTechHostIo {
    fn default() -> Self {
        EeTechHostIo {
            node: EeTechNodeRef::default(),
            unit: ptr::null_mut(),
            v: [0; 5],
            nodes: ptr::null_mut(),
            count: 0,
        }
    }
}

/// The host: `(ctx, op, io) -> the operation's result`; `ctx` is the tree (`EETechTree*`).
pub type EeTechHostFn = unsafe extern "C" fn(*mut c_void, i32, *mut EeTechHostIo) -> i32;

// ---- the host's operations
/// `mEpochs.size()` -> `v[0]`
pub const EE_TECH_HOST_EPOCHS: i32 = 1;
/// `mEpochs[v[0]]` -> `node`
pub const EE_TECH_HOST_EPOCH: i32 = 2;
/// the nodes of the epoch `node` in id order into `nodes` (at most `count`); returns how many it has
pub const EE_TECH_HOST_EPOCH_NODES: i32 = 3;
/// `Get(v[0])` -> `node`; returns 1 when there is one
pub const EE_TECH_HOST_NODE: i32 = 4;
/// `EETechTreeEpochOf(v[0])` -> `v[0]`
pub const EE_TECH_HOST_EPOCH_OF: i32 = 5;
/// `gEpochCount` -> `v[0]`
pub const EE_TECH_HOST_EPOCH_COUNT: i32 = 6;
/// the virtual `node->ResearchNode(unit)` (a node's or an epoch's, through the capture's wrapper); its result
pub const EE_TECH_HOST_RESEARCH_NODE: i32 = 10;
/// `node->SetPrerequisite(v[0])` (through the capture's wrapper)
pub const EE_TECH_HOST_SET_PREREQUISITE: i32 = 11;
/// the epoch `node`'s `Research(v[0], unit)` (through the capture's wrapper); its result
pub const EE_TECH_HOST_EPOCH_RESEARCH: i32 = 12;
/// the epoch `node`'s `AddSOL()` (through the capture's wrapper)
pub const EE_TECH_HOST_ADD_SOL: i32 = 13;
/// the epoch `node`'s `UpdateAvailability()` (through the capture's wrapper)
pub const EE_TECH_HOST_UPDATE_AVAILABILITY: i32 = 14;
/// `node->NeedsUnitMedia()`; returns 1 for true
pub const EE_TECH_HOST_NEEDS_UNIT_MEDIA: i32 = 16;
/// `EEEvent::Trigger(v[0], mPlayerIndex, v[1], mPlayer, unit)`
pub const EE_TECH_HOST_TRIGGER: i32 = 20;
/// `UpdateInterface(v[0] != 0)`
pub const EE_TECH_HOST_UPDATE_INTERFACE: i32 = 21;
/// `OnResearched(v[0], NULL)`
pub const EE_TECH_HOST_ON_RESEARCHED: i32 = 22;
/// the player's statistics: `v[0]` 0 `mPlayer->mSavedGroups.Reset(v[1], 1)`, 1 `GetPlayer()->...Reset(v[1], 1)`,
/// 2 `mPlayer->mSavedGroups.Set(v[1], v[2])`
pub const EE_TECH_HOST_STATISTIC: i32 = 23;
/// `GetPlayer()->UnloadUnitClassMedia(v[0])`
pub const EE_TECH_HOST_UNLOAD_UNIT_MEDIA: i32 = 24;
/// `gEventsEnabled = v[0] != 0`
pub const EE_TECH_HOST_EVENTS_ENABLED: i32 = 25;
/// whether the tree is the local player's (`gWorld.mPlayers[mPlayerIndex] == gWorld.GetCurrentPlayer()`)
pub const EE_TECH_HOST_IS_LOCAL: i32 = 26;
/// the epoch name of the epoch id `v[0]` for the local player: `mEpochNameID`, `PostShowEpochName`, the refresh
pub const EE_TECH_HOST_SHOW_EPOCH_NAME: i32 = 27;
/// `ESVERIFY_AT(false, v[0])`
pub const EE_TECH_HOST_ASSERT: i32 = 28;
/// TNWK (TEST ONLY): `EETechCaptureWalkBegin()`
pub const EE_TECH_HOST_WALK_BEGIN: i32 = 30;
/// TNWK: `EETechCaptureWalkStep(v[0], v[1])`
pub const EE_TECH_HOST_WALK_STEP: i32 = 31;
/// TNWK: `EETechCaptureWalk(v[0], v[4] ? tree : NULL, node.obj, v[1], v[2], v[3])`
pub const EE_TECH_HOST_WALK: i32 = 32;
/// TEPO (TEST ONLY): the record of `SetEpoch` (`v[0]` 0), `ResearchToEpoch` (1), `SetEpochRange` (2): `v[1]`, `v[2]`
pub const EE_TECH_HOST_CAPTURE_EPOCHS: i32 = 33;
/// TRTE (TEST ONLY): `EETechCaptureEpochStep(v[0], v[1])`
pub const EE_TECH_HOST_CAPTURE_EPOCH_STEP: i32 = 34;
/// TRTE: `EETechCaptureResearchToEpoch(tree, v[0], v[1] != 0)`
pub const EE_TECH_HOST_CAPTURE_RESEARCH_TO_EPOCH: i32 = 35;
/// the file (the saved game's `long` and `bool`): `v[0]` the old value in, the read value out; the result
pub const EE_TECH_HOST_READ_LONG: i32 = 50;
pub const EE_TECH_HOST_READ_BOOL: i32 = 51;
pub const EE_TECH_HOST_WRITE_LONG: i32 = 52;
pub const EE_TECH_HOST_WRITE_BOOL: i32 = 53;
/// the effects of the researched event of `node`: their count (0 without an event)
pub const EE_TECH_HOST_EFFECT_COUNT: i32 = 40;
/// effect `v[0]` of the researched event of `node` -> `v[0]` its kind, `v[1]` its unit class, `v[2]` its technology
pub const EE_TECH_HOST_EFFECT: i32 = 41;
/// unit class `v[0]`'s unit type -> `v[0]`; returns 1 when the class exists
pub const EE_TECH_HOST_CLASS_TYPE: i32 = 42;

/// The event hooks the walks fire (`kEventHookTechResearched`, `kEventHookEpochAvailable`, ee/EEEvent.h).
pub const EVENT_HOOK_TECH_RESEARCHED: i32 = -0x7d1;
pub const EVENT_HOOK_EPOCH_AVAILABLE: i32 = -0x7d2;

const OK: i32 = 0;

/// The tree's C++ side: one call per operation.
pub trait TechHost {
    fn call(&mut self, op: i32, io: &mut EeTechHostIo) -> i32;

    fn op(&mut self, op: i32, v: &[i32]) -> i32 {
        let mut io = EeTechHostIo::default();
        io.v[..v.len()].copy_from_slice(v);
        self.call(op, &mut io)
    }
    fn op_node(&mut self, op: i32, n: EeTechNodeRef, v: &[i32]) -> i32 {
        let mut io = EeTechHostIo { node: n, ..Default::default() };
        io.v[..v.len()].copy_from_slice(v);
        self.call(op, &mut io)
    }
    fn value(&mut self, op: i32, v0: i32) -> i32 {
        let mut io = EeTechHostIo::default();
        io.v[0] = v0;
        self.call(op, &mut io);
        io.v[0]
    }
    fn epochs(&mut self) -> i32 {
        self.value(EE_TECH_HOST_EPOCHS, 0)
    }
    fn epoch_count(&mut self) -> i32 {
        self.value(EE_TECH_HOST_EPOCH_COUNT, 0)
    }
    fn epoch(&mut self, index: i32) -> EeTechNodeRef {
        let mut io = EeTechHostIo::default();
        io.v[0] = index;
        self.call(EE_TECH_HOST_EPOCH, &mut io);
        io.node
    }
    /// The epoch's nodes in id order (the `std::map`'s).
    fn epoch_nodes(&mut self, epoch: EeTechNodeRef) -> Vec<EeTechNodeRef> {
        let mut io = EeTechHostIo { node: epoch, ..Default::default() };
        let n = self.call(EE_TECH_HOST_EPOCH_NODES, &mut io).max(0) as usize;
        let mut nodes = vec![EeTechNodeRef::default(); n];
        let mut io = EeTechHostIo { node: epoch, nodes: nodes.as_mut_ptr(), count: n as u32, ..Default::default() };
        let filled = self.call(EE_TECH_HOST_EPOCH_NODES, &mut io).max(0) as usize;
        nodes.truncate(filled.min(n));
        nodes
    }
    fn node(&mut self, id: i32) -> Option<EeTechNodeRef> {
        let mut io = EeTechHostIo::default();
        io.v[0] = id;
        (self.call(EE_TECH_HOST_NODE, &mut io) != 0).then_some(io.node)
    }
    fn epoch_of(&mut self, id: i32) -> i32 {
        self.value(EE_TECH_HOST_EPOCH_OF, id)
    }
    fn trigger(&mut self, event: i32, id: i32, unit: *mut c_void) {
        let mut io = EeTechHostIo { unit, ..Default::default() };
        io.v[0] = event;
        io.v[1] = id;
        self.call(EE_TECH_HOST_TRIGGER, &mut io);
    }
    fn update_interface(&mut self, epoch_changed: bool) {
        self.op(EE_TECH_HOST_UPDATE_INTERFACE, &[i32::from(epoch_changed)]);
    }
    fn research_node(&mut self, n: EeTechNodeRef, unit: *mut c_void) -> i32 {
        let mut io = EeTechHostIo { node: n, unit, ..Default::default() };
        self.call(EE_TECH_HOST_RESEARCH_NODE, &mut io)
    }
    fn walk_begin(&mut self) {
        self.op(EE_TECH_HOST_WALK_BEGIN, &[]);
    }
    fn walk_step(&mut self, id: i32, action: i32) {
        self.op(EE_TECH_HOST_WALK_STEP, &[id, action]);
    }
    /// `EETechCaptureWalk(kind, with_tree ? tree : NULL, node, a, b, result)`
    fn walk(&mut self, kind: i32, with_tree: bool, node: Option<EeTechNodeRef>, a: i32, b: i32, result: i32) {
        self.op_node(EE_TECH_HOST_WALK, node.unwrap_or_default(), &[kind, a, b, result, i32::from(with_tree)]);
    }
    fn read_long(&mut self, v: &mut i32) -> i32 {
        let mut io = EeTechHostIo::default();
        io.v[0] = *v;
        let r = self.call(EE_TECH_HOST_READ_LONG, &mut io);
        *v = io.v[0];
        r
    }
    fn read_bool(&mut self, v: &mut bool) -> i32 {
        let mut io = EeTechHostIo::default();
        io.v[0] = i32::from(*v);
        let r = self.call(EE_TECH_HOST_READ_BOOL, &mut io);
        *v = io.v[0] != 0;
        r
    }
    fn write_long(&mut self, v: i32) -> i32 {
        self.op(EE_TECH_HOST_WRITE_LONG, &[v])
    }
    fn write_bool(&mut self, v: bool) -> i32 {
        self.op(EE_TECH_HOST_WRITE_BOOL, &[i32::from(v)])
    }
}

// ---- the records through their raw pointers

fn rd(n: EeTechNodeRef) -> EeTechNodeFields {
    // SAFETY: the host hands live records; copied out, no reference kept
    unsafe { *n.node }
}

fn wr(n: EeTechNodeRef, f: impl FnOnce(&mut EeTechNodeFields)) {
    // SAFETY: as rd; the reference ends with the closure, before any host call
    unsafe { f(&mut *n.node) }
}

fn data(n: EeTechNodeRef) -> EeTechNodeDataFields {
    // SAFETY: as rd (a node of a loaded tree has its data)
    unsafe { *n.data }
}

fn id_of(n: EeTechNodeRef) -> i32 {
    data(n).mID as i32
}

/// The tree's record behind its raw pointer.
#[derive(Clone, Copy)]
pub struct Tree(pub *mut EeTechTreeFields);

impl Tree {
    fn get(self) -> TreeView {
        // SAFETY: the caller's contract (a live record); copied out field by field
        unsafe {
            let t = &*self.0;
            TreeView {
                current: t.mCurrentEpoch as i32,
                start: t.mStartEpoch as i32,
                end: t.mEndEpoch as i32,
                before_wonders: t.mEpochsBeforeWonders as i32,
            }
        }
    }
    fn set(self, f: impl FnOnce(&mut EeTechTreeFields)) {
        // SAFETY: as get; the reference ends with the closure
        unsafe { f(&mut *self.0) }
    }
    /// The saved lists, made on first use.
    fn saved(self, f: impl FnOnce(&mut EeTechTreeSaved)) {
        self.set(|t| {
            if t.mSaved.is_null() {
                t.mSaved = Box::into_raw(Box::default());
            }
            // SAFETY: a box this module made
            f(unsafe { &mut *t.mSaved })
        })
    }
    fn saved_copy(self) -> EeTechTreeSaved {
        // SAFETY: as get
        unsafe {
            let p = (*self.0).mSaved;
            if p.is_null() {
                EeTechTreeSaved::default()
            } else {
                (*p).clone()
            }
        }
    }
}

#[derive(Clone, Copy)]
struct TreeView {
    current: i32,
    start: i32,
    end: i32,
    before_wonders: i32,
}

fn state(t: Tree, epoch_count: i32) -> EeTechTreeState {
    let v = t.get();
    EeTechTreeState {
        current_epoch: v.current,
        start_epoch: v.start,
        end_epoch: v.end,
        epochs_before_wonders: v.before_wonders,
        epoch_count,
    }
}

/// The decision view of tree.rs from the records.
fn view(n: EeTechNodeRef) -> EeTechNode {
    let f = rd(n);
    let d = data(n);
    EeTechNode {
        id: d.mID as i32,
        kind: d.mKind as i32,
        epoch: f.mEpoch as i32,
        obsolete_epoch: f.mObsoleteEpoch as i32,
        research_delay: f.mResearchDelay as i32,
        prerequisites: [
            d.mPrerequisite1 as i32,
            d.mPrerequisite2 as i32,
            d.mPrerequisite3 as i32,
            d.mPrerequisite4 as i32,
        ],
        prerequisite_flags: f.mPrerequisiteFlags,
        researched: f.mIsResearched,
        obsolete: f.mIsObsolete,
        available: f.mIsAvailable,
        editor_obsoleted: f.mScenEditorObsoleted,
        makes_unit: d.mMakesUnit,
        counts_toward_epoch: d.mCountsTowardEpoch,
        pad: 0,
    }
}

// ---- a node

/// The virtual `GetIsAvailable`: an epoch's (available, not obsolete by flag, not researched) or a node's.
pub fn is_available<H: TechHost + ?Sized>(h: &mut H, t: Tree, n: EeTechNodeRef) -> bool {
    let count = h.epoch_count();
    if tree::is_epoch_id(id_of(n), count) {
        tree::epoch_is_available(&view(n))
    } else {
        tree::node_is_available(&view(n), &state(t, count))
    }
}

/// The virtual `GetIsObsolete`: an epoch's flag, a node's flag or its passed obsolete epoch.
pub fn is_obsolete<H: TechHost + ?Sized>(h: &mut H, t: Tree, n: EeTechNodeRef) -> bool {
    let count = h.epoch_count();
    if tree::is_epoch_id(id_of(n), count) {
        rd(n).mIsObsolete
    } else {
        tree::node_is_obsolete(&view(n), &state(t, count))
    }
}

/// `EETechTreeNode::SetPrerequisite`'s body.
pub fn node_set_prerequisite<H: TechHost + ?Sized>(h: &mut H, n: EeTechNodeRef, id: i32) {
    let count = h.epoch_count();
    let outcome = tree::set_prerequisite(&view(n), id, count);
    wr(n, |f| f.mPrerequisiteFlags = outcome.flags);
    if !outcome.becomes_available {
        return;
    }
    wr(n, |f| f.mIsAvailable = true);
    h.trigger(EVENT_HOOK_TECH_RESEARCHED, id_of(n), ptr::null_mut());
    h.op(EE_TECH_HOST_STATISTIC, &[1, 0x16]);
    h.update_interface(false);
}

/// `EETechTreeNode::MakeAvailable`: every prerequisite met.
pub fn node_make_available<H: TechHost + ?Sized>(h: &mut H, n: EeTechNodeRef) {
    wr(n, |f| {
        f.mIsObsolete = false;
        f.mPrerequisiteFlags = tree::PREREQUISITE_ALL;
        f.mIsAvailable = true;
    });
    h.update_interface(false);
}

/// `EETechTreeNode::ResearchNode`'s body: 0 researched (its event, the interface, the player), 2 already, 9 refused;
/// the research delay counts down as the event left it.
pub fn node_research<H: TechHost + ?Sized>(h: &mut H, n: EeTechNodeRef, unit: *mut c_void) -> i32 {
    let decision = tree::research_node(&view(n));
    if decision.research {
        wr(n, |f| f.mIsResearched = true);
        let d = data(n);
        h.trigger(d.mResearchedEvent as i32, d.mID as i32, unit);
        h.update_interface(false);
        h.op(EE_TECH_HOST_ON_RESEARCHED, &[d.mID as i32]);
    }
    let delay = tree::research_delay_after(rd(n).mResearchDelay as i32);
    wr(n, |f| f.mResearchDelay = delay as c_long);
    decision.result
}

/// Every node id of the tree, epoch by epoch (`GetNodeIDs`).
pub fn node_ids<H: TechHost + ?Sized>(h: &mut H) -> Vec<i32> {
    let mut ids = Vec::new();
    for e in 0..h.epochs() {
        let epoch = h.epoch(e);
        ids.extend(h.epoch_nodes(epoch).into_iter().map(id_of));
    }
    ids
}

fn prerequisites(d: &EeTechNodeDataFields) -> [i32; 4] {
    [d.mPrerequisite1 as i32, d.mPrerequisite2 as i32, d.mPrerequisite3 as i32, d.mPrerequisite4 as i32]
}

/// `EETechTreeNode::SetScenEditorObsoleted`: obsoleting recurses into the nodes that list this one as a
/// prerequisite; clearing recurses into this one's prerequisites.
pub fn node_set_editor_obsoleted<H: TechHost + ?Sized>(h: &mut H, n: EeTechNodeRef, value: bool, recurse: bool) {
    if value == rd(n).mScenEditorObsoleted {
        return;
    }
    let own = data(n);
    if recurse && own.mID != 0 {
        let ids = node_ids(h);
        for id in ids {
            let Some(other) = h.node(id) else { continue };
            let theirs = data(other);
            let related = if value {
                theirs.mID != own.mID && prerequisites(&theirs).contains(&(own.mID as i32))
            } else {
                theirs.mID != own.mID && prerequisites(&own).contains(&(theirs.mID as i32))
            };
            if related {
                node_set_editor_obsoleted(h, other, value, true);
            }
        }
    }
    wr(n, |f| f.mScenEditorObsoleted = value);
}

/// `EETechTreeNode::SetObsolete`: obsoleting fires the obsolete event, refreshes the interface and (unless the media
/// are kept) unloads the unit media it loaded; clearing only clears the flag.
pub fn node_set_obsolete<H: TechHost + ?Sized>(h: &mut H, n: EeTechNodeRef, value: bool, keep_media: bool) {
    h.walk_begin();
    let mut action = if value { 0 } else { 4 };
    let decision = tree::set_obsolete(value, keep_media, rd(n).mIsObsolete);
    wr(n, |f| f.mIsObsolete = decision.obsolete);
    if decision.fires {
        action |= 1;
        let d = data(n);
        h.trigger(d.mObsoleteEvent as i32, d.mID as i32, ptr::null_mut());
        h.update_interface(false);
        if decision.check_media && h.op_node(EE_TECH_HOST_NEEDS_UNIT_MEDIA, n, &[]) != 0 && rd(n).mIsUnitMediaLoaded {
            action |= 2;
            h.op(EE_TECH_HOST_UNLOAD_UNIT_MEDIA, &[data(n).mUnitClass as i32]);
            wr(n, |f| f.mIsUnitMediaLoaded = false);
        }
    }
    h.walk(5, true, Some(n), i32::from(value), i32::from(keep_media), action);
}

struct HostPrerequisites<'a, H: TechHost + ?Sized>(std::cell::RefCell<&'a mut H>);

impl<H: TechHost + ?Sized> tree::PrerequisiteView for HostPrerequisites<'_, H> {
    fn prerequisites(&self, id: i32) -> Option<[i32; 4]> {
        let n = self.0.borrow_mut().node(id)?;
        Some(prerequisites(&data(n)))
    }
}

/// `EETechTreeNode::DependsOn` over the tree's nodes.
pub fn node_depends_on<H: TechHost + ?Sized>(h: &mut H, n: EeTechNodeRef, id: i32) -> bool {
    let d = data(n);
    let view = HostPrerequisites(std::cell::RefCell::new(h));
    tree::depends_on(&view, d.mID as i32, &prerequisites(&d), id)
}

// ---- an epoch

fn counter(e: EeTechNodeRef) -> EeTechEpochCounter {
    let f = rd(e);
    EeTechEpochCounter {
        required: f.mRequiredSOL as i32,
        current: f.mCurrentSOL as i32,
        counting: f.mIsCountingSOL,
        available: f.mIsAvailable,
        pad: [0; 2],
    }
}

fn epoch_becomes_available<H: TechHost + ?Sized>(h: &mut H, e: EeTechNodeRef) {
    wr(e, |f| f.mIsAvailable = true);
    h.trigger(EVENT_HOOK_EPOCH_AVAILABLE, id_of(e), ptr::null_mut());
    h.update_interface(false);
}

/// `EETechTreeEpoch::AddSOL`'s body: one more tech toward the epoch; enough make it available.
pub fn epoch_add_sol<H: TechHost + ?Sized>(h: &mut H, t: Tree, e: EeTechNodeRef) {
    let count = h.epoch_count();
    let update = tree::add_sol(id_of(e), &counter(e), &state(t, count));
    wr(e, |f| f.mCurrentSOL = update.current as c_long);
    if update.becomes_available {
        epoch_becomes_available(h, e);
    }
}

/// `EETechTreeEpoch::UpdateAvailability`'s body: with the count reached the epoch becomes available (if it was not)
/// and its nodes get the epoch prerequisite.
pub fn epoch_update_availability<H: TechHost + ?Sized>(h: &mut H, t: Tree, e: EeTechNodeRef) {
    let count = h.epoch_count();
    let update = tree::update_availability(id_of(e), &counter(e), &state(t, count));
    if !update.considered {
        return;
    }
    if update.becomes_available {
        epoch_becomes_available(h, e);
    }
    let id = id_of(e);
    for n in h.epoch_nodes(e) {
        h.op_node(EE_TECH_HOST_SET_PREREQUISITE, n, &[id]);
    }
}

fn find<H: TechHost + ?Sized>(h: &mut H, e: EeTechNodeRef, id: i32) -> Option<EeTechNodeRef> {
    h.epoch_nodes(e).into_iter().find(|n| id_of(*n) == id)
}

/// `EETechTreeEpoch::MakeObsolete`: a node of the epoch, or the epoch itself while it is still ahead.
pub fn epoch_make_obsolete<H: TechHost + ?Sized>(h: &mut H, t: Tree, e: EeTechNodeRef, id: i32) {
    h.walk_begin();
    let mut action = 0;
    match tree::epoch_make_obsolete(id_of(e), id, rd(e).mIsObsolete, t.get().current) {
        EeTechMakeObsolete::Node => {
            if let Some(n) = find(h, e, id) {
                action = 1;
                node_set_obsolete(h, n, true, false);
            }
        }
        EeTechMakeObsolete::Epoch => {
            action = 2;
            wr(e, |f| f.mIsObsolete = true);
            let d = data(e);
            h.trigger(d.mObsoleteEvent as i32, d.mID as i32, ptr::null_mut());
            h.update_interface(false);
        }
        EeTechMakeObsolete::Nothing => action = 3,
    }
    h.walk(4, true, Some(e), id, 0, action);
}

/// `EETechTreeEpoch::ObsoleteOutdatedNodes`: the nodes past their obsolete epoch become obsolete.
pub fn epoch_obsolete_outdated<H: TechHost + ?Sized>(h: &mut H, t: Tree, e: EeTechNodeRef) {
    let epoch = t.get().current;
    h.walk_begin();
    for n in h.epoch_nodes(e) {
        let f = rd(n);
        let outdated = tree::node_is_outdated(f.mIsObsolete, f.mObsoleteEpoch as i32, epoch);
        h.walk_step(id_of(n), i32::from(outdated));
        if outdated {
            node_set_obsolete(h, n, true, false);
        }
    }
    h.walk(3, true, Some(e), epoch, 0, 0);
}

/// `EETechTreeEpoch::Unlock`: available with its count complete; its nodes available or given the epoch.
pub fn epoch_unlock<H: TechHost + ?Sized>(h: &mut H, t: Tree, e: EeTechNodeRef) {
    h.walk_begin();
    wr(e, |f| {
        f.mIsObsolete = false;
        f.mCurrentSOL = f.mRequiredSOL;
        f.mIsAvailable = true;
    });
    h.update_interface(false);
    let id = id_of(e);
    for n in h.epoch_nodes(e) {
        let count = h.epoch_count();
        let action = tree::unlock_action(&view(n), &state(t, count), data(n).mAvailableWithEpoch);
        h.walk_step(id_of(n), action as i32);
        match action {
            EeTechUnlockAction::MakeAvailable => node_make_available(h, n),
            EeTechUnlockAction::SetPrerequisite => {
                h.op_node(EE_TECH_HOST_SET_PREREQUISITE, n, &[id]);
            }
            EeTechUnlockAction::Nothing => {}
        }
    }
    h.walk(0, true, Some(e), id, 0, 0);
}

/// `EETechTreeEpoch::ResearchSelf`: available, then researched.
pub fn epoch_research_self<H: TechHost + ?Sized>(h: &mut H, e: EeTechNodeRef) {
    if !rd(e).mIsAvailable {
        wr(e, |f| f.mIsAvailable = true);
    }
    let id = id_of(e);
    h.op_node(EE_TECH_HOST_EPOCH_RESEARCH, e, &[id]);
}

/// `EETechTreeEpoch::ResearchAll`: the epoch, then its nodes researched with it (every node when asked).
pub fn epoch_research_all<H: TechHost + ?Sized>(h: &mut H, e: EeTechNodeRef, all_nodes: bool) {
    h.walk_begin();
    epoch_research_self(h, e);
    for n in h.epoch_nodes(e) {
        let takes = tree::research_all_takes(data(n).mResearchedWithEpoch, all_nodes);
        h.walk_step(id_of(n), i32::from(takes));
        if takes {
            h.research_node(n, ptr::null_mut());
        }
    }
    h.walk(1, true, Some(e), id_of(e), i32::from(all_nodes), 0);
}

/// `EETechTreeEpoch::ResearchKind`: the epoch's nodes of one kind (only those researched with it when asked).
pub fn epoch_research_kind<H: TechHost + ?Sized>(h: &mut H, e: EeTechNodeRef, kind: i32, epoch_only: bool) {
    h.walk_begin();
    for n in h.epoch_nodes(e) {
        let d = data(n);
        let takes = tree::research_kind_takes(d.mKind as i32, kind, epoch_only, d.mResearchedWithEpoch);
        h.walk_step(d.mID as i32, i32::from(takes));
        if takes {
            h.research_node(n, ptr::null_mut());
        }
    }
    h.walk(2, true, Some(e), kind, i32::from(epoch_only), 0);
}

/// `EETechTreeEpoch::Research`'s body: a node of the epoch (9 without one) or the epoch itself (its nodes get the
/// epoch, its event, the tree's epoch, the outdated nodes obsolete, the interface, the player).
pub fn epoch_research<H: TechHost + ?Sized>(h: &mut H, t: Tree, e: EeTechNodeRef, id: i32, unit: *mut c_void) -> i32 {
    let own = id_of(e);
    if id != own {
        return match find(h, e, id) {
            Some(n) => h.research_node(n, unit),
            None => tree::RESULT_REFUSED,
        };
    }
    let count = h.epoch_count();
    let decision = tree::research_epoch(&view(e), &state(t, count));
    if !decision.research {
        return decision.result;
    }
    wr(e, |f| f.mIsResearched = true);
    for n in h.epoch_nodes(e) {
        h.op_node(EE_TECH_HOST_SET_PREREQUISITE, n, &[own]);
    }
    h.trigger(data(e).mResearchedEvent as i32, own, unit);
    t.set(|f| f.mCurrentEpoch = own as c_long);
    tree_obsolete_outdated(h, t);
    h.update_interface(true);
    h.op(EE_TECH_HOST_ON_RESEARCHED, &[own]);
    tree::RESULT_RESEARCHED
}

// ---- the tree

fn epoch_in_range<H: TechHost + ?Sized>(h: &mut H, epoch: i32) -> bool {
    epoch >= 0 && epoch < h.epochs()
}

/// `EETechTree::ObsoleteOutdatedNodes`: every epoch up to the current one.
pub fn tree_obsolete_outdated<H: TechHost + ?Sized>(h: &mut H, t: Tree) {
    let mut i = 0;
    while i <= t.get().current {
        let e = h.epoch(i);
        epoch_obsolete_outdated(h, t, e);
        i += 1;
    }
}

/// `EETechTree::MakeObsolete`: in the id's epoch.
pub fn tree_make_obsolete<H: TechHost + ?Sized>(h: &mut H, t: Tree, id: i32) {
    let epoch = h.epoch_of(id);
    if epoch_in_range(h, epoch) {
        let e = h.epoch(epoch);
        epoch_make_obsolete(h, t, e, id);
    }
}

/// `EETechTree::SetPrerequisite`: the id's node in its epoch gets the prerequisite.
pub fn tree_set_prerequisite<H: TechHost + ?Sized>(h: &mut H, id: i32, prerequisite: i32) {
    let epoch = h.epoch_of(id);
    if epoch_in_range(h, epoch) {
        let e = h.epoch(epoch);
        if let Some(n) = find(h, e, id) {
            h.op_node(EE_TECH_HOST_SET_PREREQUISITE, n, &[prerequisite]);
        }
    }
}

/// `EETechTree::Research`'s body: in the id's epoch; an epoch updates statistics series 0xc, a node counting toward
/// the next epoch adds to its count.
pub fn tree_research<H: TechHost + ?Sized>(h: &mut H, t: Tree, id: i32, unit: *mut c_void) {
    let epoch = h.epoch_of(id);
    let next = t.get().current.wrapping_add(1);
    if !epoch_in_range(h, epoch) {
        return;
    }
    let e = h.epoch(epoch);
    let mut io = EeTechHostIo { node: e, unit, ..Default::default() };
    io.v[0] = id;
    let result = h.call(EE_TECH_HOST_EPOCH_RESEARCH, &mut io);
    let count = h.epoch_count();
    let mut counts = None;
    if (result == tree::RESULT_ALREADY || result == tree::RESULT_RESEARCHED) && !tree::is_epoch_id(id, count) {
        counts = h.node(id).map(|n| data(n).mCountsTowardEpoch);
    }
    let slots = h.epochs();
    match tree::tree_research_after(id, result, count, counts, next, slots) {
        tree::EeTechTreeResearchAfter::EpochStatistics => {
            h.op(EE_TECH_HOST_STATISTIC, &[0, 0xc]);
        }
        tree::EeTechTreeResearchAfter::CountTowardNextEpoch => {
            let e = h.epoch(next);
            h.op_node(EE_TECH_HOST_ADD_SOL, e, &[]);
        }
        tree::EeTechTreeResearchAfter::Nothing => {}
    }
}

/// `EETechTree::ResearchToEpoch`: every epoch up to `epoch` unlocked and researched with events off, its kind-7
/// nodes, the epoch's name for the local player.
pub fn tree_research_to_epoch<H: TechHost + ?Sized>(h: &mut H, t: Tree, epoch: i32) {
    h.op(EE_TECH_HOST_CAPTURE_EPOCHS, &[1, epoch]);
    h.op(EE_TECH_HOST_EVENTS_ENABLED, &[0]);
    h.op(EE_TECH_HOST_CAPTURE_EPOCH_STEP, &[-1, 0]);
    let local = epoch_in_range(h, epoch) && h.op(EE_TECH_HOST_IS_LOCAL, &[]) != 0;
    let slots = h.epochs();
    for step in tree::research_to_epoch_steps(epoch, slots, local) {
        let e = h.epoch(step.epoch);
        h.op(EE_TECH_HOST_CAPTURE_EPOCH_STEP, &[step.step as i32, step.epoch]);
        match step.step {
            tree::EeTechEpochStepKind::Unlock => epoch_unlock(h, t, e),
            tree::EeTechEpochStepKind::ResearchAll => epoch_research_all(h, e, false),
            tree::EeTechEpochStepKind::ResearchSelf => epoch_research_self(h, e),
            tree::EeTechEpochStepKind::ResearchKind7 => epoch_research_kind(h, e, 7, true),
            tree::EeTechEpochStepKind::ShowName => {
                h.op(EE_TECH_HOST_SHOW_EPOCH_NAME, &[id_of(e)]);
            }
        }
    }
    h.op(EE_TECH_HOST_CAPTURE_RESEARCH_TO_EPOCH, &[epoch, i32::from(local)]);
    h.op(EE_TECH_HOST_EVENTS_ENABLED, &[1]);
}

/// `EETechTree::SetEpoch`: forward only, statistics series 0xc, research up to it.
pub fn tree_set_epoch<H: TechHost + ?Sized>(h: &mut H, t: Tree, epoch: i32) {
    h.op(EE_TECH_HOST_CAPTURE_EPOCHS, &[0, epoch]);
    h.walk_begin();
    let slots = h.epochs();
    if !tree::set_epoch_allowed(epoch, slots, t.get().current) {
        h.walk(7, true, None, epoch, 0, 0);
        return;
    }
    t.set(|f| f.mCurrentEpoch = epoch as c_long);
    if epoch != 0 {
        h.op(EE_TECH_HOST_STATISTIC, &[2, 0xc, epoch]);
    }
    tree_research_to_epoch(h, t, epoch);
    h.walk(7, true, None, epoch, 0, 1);
}

/// `EETechTree::AdvanceEpoch`: the next epoch unless past the end.
pub fn tree_advance_epoch<H: TechHost + ?Sized>(h: &mut H, t: Tree) {
    h.walk_begin();
    let v = t.get();
    let advance = tree::advance_epoch(v.current, v.end);
    if let Some(epoch) = advance {
        tree_set_epoch(h, t, epoch);
    }
    h.walk(8, true, None, v.current.wrapping_add(1), t.get().end, i32::from(advance.is_some()));
}

/// `EETechTree::SetEpochRange`: the range, research up to its start.
pub fn tree_set_epoch_range<H: TechHost + ?Sized>(h: &mut H, t: Tree, start: i32, end: i32) {
    h.op(EE_TECH_HOST_CAPTURE_EPOCHS, &[2, start, end]);
    t.set(|f| {
        f.mStartEpoch = start as c_long;
        f.mEndEpoch = end as c_long;
    });
    tree_research_to_epoch(h, t, start);
}

/// `EETechTree::ResetEpochRange`: the range and the current epoch, no research.
pub fn tree_reset_epoch_range<H: TechHost + ?Sized>(h: &mut H, t: Tree, start: i32, end: i32) {
    t.set(|f| {
        f.mStartEpoch = start as c_long;
        f.mEndEpoch = end as c_long;
        f.mCurrentEpoch = start as c_long;
    });
    h.op(EE_TECH_HOST_STATISTIC, &[2, 0xc, start]);
}

/// `EETechTree::SetNodeAvailable`: only without a research delay.
pub fn tree_set_node_available<H: TechHost + ?Sized>(h: &mut H, id: i32, value: bool) {
    let n = h.node(id);
    h.walk_begin();
    let action = match n {
        None => 0,
        Some(n) if rd(n).mResearchDelay == 0 => 1,
        Some(_) => 2,
    };
    if let Some(n) = n {
        if tree::node_availability_settable(rd(n).mResearchDelay as i32) {
            wr(n, |f| f.mIsAvailable = value);
        }
    }
    h.walk(9, true, n, id, i32::from(value), action);
}

/// `GetTechIDState` for the saves and the checksum: (researched, obsolete, editor-obsoleted).
fn id_state<H: TechHost + ?Sized>(h: &mut H, t: Tree, id: i32) -> Option<(bool, bool, bool)> {
    let n = h.node(id)?;
    let obsolete = is_obsolete(h, t, n);
    let f = rd(n);
    Some((f.mIsResearched, obsolete, f.mScenEditorObsoleted))
}

/// The nodes' ids then the epochs' (`GetNodeIDs` and the epochs' `mData->mID`).
fn all_ids<H: TechHost + ?Sized>(h: &mut H) -> Vec<i32> {
    let mut ids = node_ids(h);
    for i in 0..h.epochs() {
        let e = h.epoch(i);
        ids.push(id_of(e));
    }
    ids
}

/// `EETechTree::GetChecksum`'s value: every node's and epoch's state weighted by position (an unknown id keeps the
/// state before it, after the assertion).
pub fn tree_checksum<H: TechHost + ?Sized>(h: &mut H, t: Tree) -> u32 {
    let mut entries = Vec::new();
    let mut last = (false, false);
    for id in all_ids(h) {
        match id_state(h, t, id) {
            Some((researched, obsolete, _)) => last = (researched, obsolete),
            None => {
                h.op(EE_TECH_HOST_ASSERT, &[512]);
            }
        }
        entries.push(tree::EeTechChecksumEntry { id, researched: last.0, obsolete: last.1, pad: [0; 2] });
    }
    tree::checksum(&entries)
}

/// `EETechTree::Load` from a saved game: the research lists for `ApplySavedState`, the epochs, each epoch's counter
/// (14 epochs before 0.0xea); every epoch up to the current one researched. The read status.
pub fn tree_load<H: TechHost + ?Sized>(h: &mut H, t: Tree, major: u32, minor: u32) -> i32 {
    let mut num_epochs = 0;
    let mut num_ids = 0;
    let mut id = 0;
    let mut researched = false;
    let mut obsolete = false;
    let mut editor = false;
    t.saved(|s| *s = EeTechTreeSaved::default());
    let mut result = h.read_long(&mut num_epochs);
    if result != OK {
        return result;
    }
    let mut v = t.get().before_wonders;
    result = h.read_long(&mut v);
    t.set(|f| f.mEpochsBeforeWonders = v as c_long);
    if result != OK {
        return result;
    }
    t.set(|f| f.mIsLoaded = true);
    if num_epochs > h.epochs() {
        h.op(EE_TECH_HOST_ASSERT, &[926]);
    }
    result = h.read_long(&mut num_ids);
    if result != OK {
        return result;
    }
    for _ in 0..num_ids.max(0) {
        if h.read_long(&mut id) != OK || h.read_bool(&mut researched) != OK || h.read_bool(&mut obsolete) != OK {
            continue;
        }
        if researched {
            t.saved(|s| s.researched.push(id));
        }
        if obsolete {
            t.saved(|s| s.obsolete.push(id));
        }
        if h.read_bool(&mut editor) == OK && editor {
            t.saved(|s| s.editor_obsoleted.push(id));
        }
    }
    let mut v = t.get().current;
    result = h.read_long(&mut v);
    t.set(|f| f.mCurrentEpoch = v as c_long);
    if result == OK {
        let mut v = t.get().start;
        result = h.read_long(&mut v);
        t.set(|f| f.mStartEpoch = v as c_long);
        if result == OK {
            let mut v = t.get().end;
            result = h.read_long(&mut v);
            t.set(|f| f.mEndEpoch = v as c_long);
        }
    }
    let epochs = if major == 0 && minor < 0xea { 14 } else { num_epochs };
    let mut e = 0;
    while e < epochs && e < h.epochs() {
        let epoch = h.epoch(e);
        let mut sol = rd(epoch).mCurrentSOL as i32;
        h.read_long(&mut sol);
        wr(epoch, |f| {
            f.mCurrentSOL = sol as c_long;
            f.mIsCountingSOL = false;
        });
        let entry = (id_of(epoch), sol);
        t.saved(|s| s.sols.push(entry));
        e += 1;
    }
    let current = t.get().current;
    t.saved(|s| {
        let mut e = current;
        while e >= 0 {
            s.researched.insert(0, e);
            e -= 1;
        }
    });
    result
}

/// `EETechTree::Save`: the researched (from the start epoch), obsolete or editor-obsoleted ids with their flags,
/// the epochs, each epoch's counter; the last epoch's write result.
pub fn tree_save<H: TechHost + ?Sized>(h: &mut H, t: Tree) -> i32 {
    let slots = h.epochs();
    let mut result = h.write_long(slots);
    if result == OK {
        result = h.write_long(t.get().before_wonders);
        if result == OK {
            let start = t.get().start;
            let mut kept = Vec::new();
            for id in all_ids(h) {
                let (researched, obsolete, editor) = id_state(h, t, id).unwrap_or_else(|| {
                    h.op(EE_TECH_HOST_ASSERT, &[1319]);
                    (false, false, false)
                });
                let early = h.epoch_of(id) < start;
                if !((!researched || early) && !obsolete && !editor) {
                    kept.push(id);
                }
            }
            result = h.write_long(kept.len() as i32);
            if result == OK {
                for id in kept {
                    if result != OK {
                        break;
                    }
                    let (researched, obsolete, editor) = id_state(h, t, id).unwrap_or((false, false, false));
                    result = h.write_long(id);
                    if result != OK {
                        continue;
                    }
                    result = h.write_bool(researched);
                    if result != OK {
                        continue;
                    }
                    result = h.write_bool(obsolete);
                    if result == OK {
                        result = h.write_bool(editor);
                    }
                }
                if result == OK {
                    let v = t.get();
                    result = h.write_long(v.current);
                    if result == OK {
                        result = h.write_long(v.start);
                        if result == OK {
                            result = h.write_long(v.end);
                        }
                    }
                }
            }
        }
    }
    for i in 0..slots {
        let e = h.epoch(i);
        result = h.write_long(rd(e).mCurrentSOL as i32);
        if result != OK {
            h.op(EE_TECH_HOST_ASSERT, &[1381]);
        }
    }
    result
}

/// `EETechTree::ApplySavedResearch`: the saved obsolete ids obsoleted (media kept), the researched ones made
/// available and researched.
pub fn tree_apply_saved_research<H: TechHost + ?Sized>(h: &mut H, t: Tree) {
    let saved = t.saved_copy();
    for id in saved.obsolete {
        if let Some(n) = h.node(id) {
            node_set_obsolete(h, n, true, true);
        }
    }
    for id in saved.researched {
        if let Some(n) = h.node(id) {
            wr(n, |f| f.mIsAvailable = true);
            h.research_node(n, ptr::null_mut());
        }
    }
}

/// `EETechTree::ApplySavedScenEditorObsoleted`: the saved editor-obsoleted ids set; the nodes obsoleted by default
/// but not in the list cleared (both walks from the last).
pub fn tree_apply_saved_editor_obsoleted<H: TechHost + ?Sized>(h: &mut H, t: Tree) {
    let listed = t.saved_copy().editor_obsoleted;
    let mut cleared = Vec::new();
    for id in node_ids(h).into_iter().rev() {
        let Some(n) = h.node(id) else { continue };
        let d = data(n);
        if !d.mObsoletedByDefault {
            continue;
        }
        if !listed.contains(&(d.mID as i32)) {
            cleared.push(id);
        }
    }
    for id in listed {
        if let Some(n) = h.node(id) {
            node_set_editor_obsoleted(h, n, true, true);
        }
    }
    // the original walks its list from the last entry (the list holds them first to last in reverse id order)
    for id in cleared.into_iter().rev() {
        if let Some(n) = h.node(id) {
            node_set_editor_obsoleted(h, n, false, true);
        }
    }
}

/// `EETechTree::ApplySavedState`: the editor flags, the epoch range, the research, each epoch's saved counter, then
/// every epoch's availability with its counting on, the interface.
pub fn tree_apply_saved_state<H: TechHost + ?Sized>(h: &mut H, t: Tree, apply_obsoleted: bool, apply_research: bool) {
    if apply_obsoleted {
        tree_apply_saved_editor_obsoleted(h, t);
    }
    let v = t.get();
    tree_set_epoch_range(h, t, v.start, v.end);
    if apply_research {
        tree_apply_saved_research(h, t);
    }
    for (epoch, sol) in t.saved_copy().sols {
        if epoch_in_range(h, epoch) {
            let e = h.epoch(epoch);
            wr(e, |f| f.mCurrentSOL = sol as c_long);
        }
    }
    for i in 0..h.epochs() {
        let e = h.epoch(i);
        h.op_node(EE_TECH_HOST_UPDATE_AVAILABILITY, e, &[]);
        wr(e, |f| f.mIsCountingSOL = true);
    }
    h.update_interface(true);
}

// ---- the tech tree file's record and the nodes' fill from it

/// One 0x10c-byte record of the tech tree file (`EETechTreeEpoch::Load` 0x1ce10e and `EETechTreeNode::Load`
/// 0x1cf370 read it into a local; was ee/EETechTreeRecord.h). Fields by offset, named after the members they fill.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct EeTechTreeRecord {
    /// +0x00 (not read: the name?)
    pub mLabel: [u8; 0x68],
    pub mID: i32,
    pub mEpoch: i32,
    pub mObsoleteEpoch: i32,
    pub mResearchedEvent: i32,
    pub mObsoleteEvent: i32,
    /// +0x7c the cost of resource 3 (wood)
    pub mCost3: i32,
    /// +0x80 stone
    pub mCost4: i32,
    pub mUnused1: i32,
    /// +0x88 gold
    pub mCost5: i32,
    pub mUnused2: i32,
    /// +0x90 iron
    pub mCost6: i32,
    /// +0x94 food
    pub mCost2: i32,
    pub mBuildTime: i32,
    pub mPrerequisite1: i32,
    pub mPrerequisite2: i32,
    pub mPrerequisite3: i32,
    pub mPrerequisite4: i32,
    pub mUnitClass: i32,
    /// +0xb0 `EEButtonObject::Get` -> the node's `mButton`
    pub mButton: i32,
    pub mMakesUnit: u8,
    pub mCountsTowardEpoch: u8,
    pub mUnused3: u8,
    pub mUnused4: u8,
    pub mNameID: i32,
    pub mKind: i32,
    pub mCostResource: i32,
    pub mRequiredStatistic: i32,
    pub mRequiredStatisticValue: i32,
    pub mCalamity: i32,
    pub mHotKey: i32,
    /// +0xd4 an epoch's title string (0: 30145 + the epoch)
    pub mTitleID: i32,
    pub mUnused6: i32,
    pub mUnused7: i32,
    pub mUnused8: i32,
    /// +0xe4 the node's cost factors of its second .. fifth unit class (the others 1.0f)
    pub mFactor1: f32,
    pub mFactor2: f32,
    pub mFactor3: f32,
    pub mFactor4: f32,
    pub mShowInEditor: u8,
    pub mObsoletedByDefault: u8,
    pub mAvailableWithEpoch: u8,
    pub mResearchedWithEpoch: u8,
    pub mUnused9: i32,
    pub mNextEpoch: i32,
    pub mRequiredSOL: i32,
    pub mFirstNodeID: i32,
    pub mDefaultUnitClass: i32,
}

const _: () = assert!(std::mem::size_of::<EeTechTreeRecord>() == 268);

/// The data both loads copy (the costs zeroed, then five of them; a flag true for a byte other than 0).
pub fn fill_data(d: &mut EeTechNodeDataFields, r: &EeTechTreeRecord) {
    d.mCosts = [0; 7];
    d.mCosts[4] = r.mCost4 as c_long;
    d.mCosts[5] = r.mCost5 as c_long;
    d.mCosts[3] = r.mCost3 as c_long;
    d.mCosts[2] = r.mCost2 as c_long;
    d.mCosts[6] = r.mCost6 as c_long;
    d.mID = r.mID as c_long;
    d.mResearchedEvent = r.mResearchedEvent as c_long;
    d.mObsoleteEvent = r.mObsoleteEvent as c_long;
    d.mBuildTime = r.mBuildTime as c_long;
    d.mPrerequisite1 = r.mPrerequisite1 as c_long;
    d.mPrerequisite2 = r.mPrerequisite2 as c_long;
    d.mPrerequisite3 = r.mPrerequisite3 as c_long;
    d.mPrerequisite4 = r.mPrerequisite4 as c_long;
    d.mUnitClass = r.mUnitClass as c_long;
    d.mMakesUnit = r.mMakesUnit != 0;
    d.mDefaultUnitClass = r.mDefaultUnitClass as c_long;
    d.mCountsTowardEpoch = r.mCountsTowardEpoch != 0;
    d.mNameID = r.mNameID as c_long;
    d.mKind = r.mKind as c_long;
    d.mCostResource = r.mCostResource as c_long;
    d.mRequiredStatistic = r.mRequiredStatistic as c_long;
    d.mRequiredStatisticValue = r.mRequiredStatisticValue as c_long;
    d.mCalamity = r.mCalamity as u32 as c_ulong;
    d.mHotKey = r.mHotKey as c_long;
    d.mObsoletedByDefault = r.mObsoletedByDefault != 0;
    d.mShowInEditor = r.mShowInEditor != 0;
    d.mAvailableWithEpoch = r.mAvailableWithEpoch != 0;
    d.mResearchedWithEpoch = r.mResearchedWithEpoch != 0;
}

/// `EETechTreeNode::Load`'s own members (its epoch and button stay the C++'s): the obsolete epoch, the editor flag
/// of a node obsoleted by default, the flags of its empty prerequisites, no research delay.
pub fn fill_node(n: &mut EeTechNodeFields, r: &EeTechTreeRecord) {
    n.mObsoleteEpoch = r.mObsoleteEpoch as c_long;
    if r.mObsoletedByDefault != 0 {
        n.mScenEditorObsoleted = true;
    }
    n.mPrerequisiteFlags =
        tree::initial_prerequisite_flags(&[r.mPrerequisite1, r.mPrerequisite2, r.mPrerequisite3, r.mPrerequisite4]);
    n.mResearchDelay = 0;
}

/// `EETechTreeEpoch::Load`'s own members: its epoch, obsolete epoch, the counter's target, the first node and the
/// next epoch, its title, the counter at 0 and off.
pub fn fill_epoch(n: &mut EeTechNodeFields, r: &EeTechTreeRecord) {
    n.mEpoch = r.mEpoch as c_long;
    n.mObsoleteEpoch = r.mObsoleteEpoch as c_long;
    n.mRequiredSOL = r.mRequiredSOL as c_long;
    n.mFirstNodeID = r.mFirstNodeID as c_long;
    n.mNextEpoch = r.mNextEpoch as c_long;
    n.mTitleID = r.mTitleID as c_long;
    n.mCurrentSOL = 0;
    n.mIsCountingSOL = false;
}

/// The constructors' values (`EETechTreeNode` 0x1cedce, `EETechTreeEpoch` 0x1cdf68): allowed, obsolete after the
/// space epoch (14), the rest 0.
pub fn node_fields_new() -> EeTechNodeFields {
    EeTechNodeFields { mIsAllowed: true, mObsoleteEpoch: 14, ..Default::default() }
}

// ---- gTechTree's event effects by unit type (was sUnitClassInfo, EETechTree.cpp)

/// The effect kinds the table keeps a list for: the model (6), the sounds (0xf .. 0x15 but 0x13).
const MEDIA_KINDS: [i32; 7] = [6, 0xf, 0x10, 0x11, 0x12, 0x14, 0x15];
/// The kind whose technology upgrades a class into another.
const KIND_UPGRADE: i32 = 9;

/// One unit type's entry: per media kind the techs whose research event changes it, the kind-9 technology.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct UnitClassInfo {
    pub techs: [Vec<i32>; 7],
    pub technology: i32,
}

impl Default for UnitClassInfo {
    fn default() -> Self {
        UnitClassInfo { techs: Default::default(), technology: tree::TECH_NONE }
    }
}

type UnitClassTable = std::collections::BTreeMap<i32, UnitClassInfo>;

static UNIT_CLASS_INFO: std::sync::Mutex<UnitClassTable> = std::sync::Mutex::new(std::collections::BTreeMap::new());

fn class_type<H: TechHost + ?Sized>(h: &mut H, class: i32) -> Option<i32> {
    let mut io = EeTechHostIo::default();
    io.v[0] = class;
    (h.call(EE_TECH_HOST_CLASS_TYPE, &mut io) != 0).then_some(io.v[0])
}

/// `EETechTree::BuildUnitClassInfo` over gTechTree (the host's tree): for every epoch's nodes and the epoch, each
/// effect of its researched event filed under its unit class's type (the model and sound kinds), and a kind-9
/// effect's id under the type of the class its technology builds.
pub fn build_unit_class_info<H: TechHost + ?Sized>(h: &mut H, table: &mut UnitClassTable) {
    let count = h.epoch_count();
    for epoch in 0..count {
        let mut ids = Vec::new();
        if epoch_in_range(h, epoch) {
            let e = h.epoch(epoch);
            ids.extend(h.epoch_nodes(e).into_iter().map(id_of));
        }
        ids.push(epoch);
        for id in ids {
            let Some(n) = h.node(id) else { continue };
            if data(n).mResearchedEvent <= 0 {
                continue;
            }
            let effects = h.op_node(EE_TECH_HOST_EFFECT_COUNT, n, &[]);
            for j in 0..effects {
                let mut io = EeTechHostIo { node: n, ..Default::default() };
                io.v[0] = j;
                h.call(EE_TECH_HOST_EFFECT, &mut io);
                let [kind, unit_class, technology, ..] = io.v;
                let unit_type = if unit_class > 0 { class_type(h, unit_class) } else { None };
                if kind == KIND_UPGRADE {
                    let built = h.node(technology).map(|t| data(t).mUnitClass as i32);
                    if let Some(upgraded) = built.and_then(|c| class_type(h, c)) {
                        table.entry(upgraded).or_default().technology = id;
                    }
                } else if let (Some(t), Some(k)) = (unit_type, MEDIA_KINDS.iter().position(|m| *m == kind)) {
                    table.entry(t).or_default().techs[k].push(id);
                }
            }
        }
    }
}

/// `EETechTree::IsLatestEffect`: whether no later tech of the type's list for the kind is still used by the tree
/// (one before its start epoch and not obsoleted in the editor). True for an id or a kind the table does not list.
pub fn is_latest_effect<H: TechHost + ?Sized>(
    h: &mut H,
    t: Tree,
    table: &UnitClassTable,
    id: i32,
    unit_type: i32,
    kind: i32,
) -> bool {
    let Some(k) = MEDIA_KINDS.iter().position(|m| *m == kind) else { return true };
    let Some(info) = table.get(&unit_type) else { return true };
    let list = &info.techs[k];
    let Some(at) = list.iter().position(|x| *x == id) else { return true };
    let start = t.get().start;
    for later in &list[at + 1..] {
        if let Some(n) = h.node(*later) {
            let f = rd(n);
            if (f.mEpoch as i32) < start && !f.mScenEditorObsoleted {
                return false;
            }
        }
    }
    true
}

// ---- FFI

struct FfiHost {
    ctx: *mut c_void,
    f: EeTechHostFn,
}

impl TechHost for FfiHost {
    fn call(&mut self, op: i32, io: &mut EeTechHostIo) -> i32 {
        // SAFETY: the C++ callback with its own context; io lives for the call
        unsafe { (self.f)(self.ctx, op, io) }
    }
}

/// Frees a tree record's saved lists (the C++ destructor).
///
/// # Safety
/// `t` is a live record.
#[no_mangle]
pub unsafe extern "C" fn ee_tech_tree_fields_free(t: *mut EeTechTreeFields) {
    let p = std::mem::replace(&mut (*t).mSaved, ptr::null_mut());
    if !p.is_null() {
        drop(Box::from_raw(p));
    }
}

/// A node's and an epoch's constructor values.
///
/// # Safety
/// `n` is a writable record.
#[no_mangle]
pub unsafe extern "C" fn ee_tech_node_fields_init(n: *mut EeTechNodeFields) {
    *n = node_fields_new();
}

/// `EETechTreeNode::Load`'s and `EETechTreeEpoch::Load`'s copies from the file's record (`epoch`: the epoch's).
///
/// # Safety
/// The three pointers are live.
#[no_mangle]
pub unsafe extern "C" fn ee_tech_fill_from_record(
    n: *mut EeTechNodeFields,
    d: *mut EeTechNodeDataFields,
    r: *const EeTechTreeRecord,
    epoch: bool,
) {
    fill_data(&mut *d, &*r);
    if epoch {
        fill_epoch(&mut *n, &*r);
    } else {
        fill_node(&mut *n, &*r);
    }
}

/// The node operations of [`ee_tech_walk_node`] (`a`, `b`: the C++ arguments).
#[repr(i32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeTechNodeOp {
    /// the virtual `GetIsAvailable` (0/1)
    IsAvailable = 0,
    /// the virtual `GetIsObsolete` (0/1)
    IsObsolete = 1,
    /// `SetPrerequisite(a)`'s body
    SetPrerequisite = 2,
    MakeAvailable = 3,
    /// `ResearchNode(unit)`'s body: its result
    Research = 4,
    /// `SetScenEditorObsoleted(a, b)`
    SetEditorObsoleted = 5,
    /// `SetObsolete(a, b)`
    SetObsolete = 6,
    /// `DependsOn(a)` (0/1)
    DependsOn = 7,
}

/// A node's (or an epoch's) operation of the tree `t` (`ctx`); its answer, else 0.
///
/// # Safety
/// `f` is valid with `ctx`; `t` and `n`'s records are live.
#[no_mangle]
pub unsafe extern "C" fn ee_tech_walk_node(
    ctx: *mut c_void,
    f: EeTechHostFn,
    op: EeTechNodeOp,
    t: *mut EeTechTreeFields,
    n: EeTechNodeRef,
    a: i32,
    b: bool,
    unit: *mut c_void,
) -> i32 {
    let h = &mut FfiHost { ctx, f };
    let t = Tree(t);
    match op {
        EeTechNodeOp::IsAvailable => return i32::from(is_available(h, t, n)),
        EeTechNodeOp::IsObsolete => return i32::from(is_obsolete(h, t, n)),
        EeTechNodeOp::SetPrerequisite => node_set_prerequisite(h, n, a),
        EeTechNodeOp::MakeAvailable => node_make_available(h, n),
        EeTechNodeOp::Research => return node_research(h, n, unit),
        EeTechNodeOp::SetEditorObsoleted => node_set_editor_obsoleted(h, n, a != 0, b),
        EeTechNodeOp::SetObsolete => node_set_obsolete(h, n, a != 0, b),
        EeTechNodeOp::DependsOn => return i32::from(node_depends_on(h, n, a)),
    }
    0
}

/// The epoch operations of [`ee_tech_walk_epoch`].
#[repr(i32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeTechEpochOp {
    /// `AddSOL`'s body
    AddSol = 0,
    /// `UpdateAvailability`'s body
    UpdateAvailability = 1,
    /// `MakeObsolete(a)`
    MakeObsolete = 2,
    ObsoleteOutdated = 3,
    Unlock = 4,
    ResearchSelf = 5,
    /// `ResearchAll(b)`
    ResearchAll = 6,
    /// `ResearchKind(a, b)`
    ResearchKind = 7,
    /// `Research(a, unit)`'s body: its result
    Research = 8,
}

/// An epoch's operation; `Research`'s result, else 0.
///
/// # Safety
/// As [`ee_tech_walk_node`].
#[no_mangle]
pub unsafe extern "C" fn ee_tech_walk_epoch(
    ctx: *mut c_void,
    f: EeTechHostFn,
    op: EeTechEpochOp,
    t: *mut EeTechTreeFields,
    e: EeTechNodeRef,
    a: i32,
    b: bool,
    unit: *mut c_void,
) -> i32 {
    let h = &mut FfiHost { ctx, f };
    let t = Tree(t);
    match op {
        EeTechEpochOp::AddSol => epoch_add_sol(h, t, e),
        EeTechEpochOp::UpdateAvailability => epoch_update_availability(h, t, e),
        EeTechEpochOp::MakeObsolete => epoch_make_obsolete(h, t, e, a),
        EeTechEpochOp::ObsoleteOutdated => epoch_obsolete_outdated(h, t, e),
        EeTechEpochOp::Unlock => epoch_unlock(h, t, e),
        EeTechEpochOp::ResearchSelf => epoch_research_self(h, e),
        EeTechEpochOp::ResearchAll => epoch_research_all(h, e, b),
        EeTechEpochOp::ResearchKind => epoch_research_kind(h, e, a, b),
        EeTechEpochOp::Research => return epoch_research(h, t, e, a, unit),
    }
    0
}

/// The tree operations of [`ee_tech_walk_tree`].
#[repr(i32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeTechTreeOp {
    /// `Research(a, unit)`'s body
    Research = 0,
    /// `ResearchToEpoch(a)`
    ResearchToEpoch = 1,
    /// `SetEpoch(a)`
    SetEpoch = 2,
    AdvanceEpoch = 3,
    /// `SetEpochRange(a, b)`
    SetEpochRange = 4,
    /// `ResetEpochRange(a, b)`
    ResetEpochRange = 5,
    /// `SetNodeAvailable(a, b)`
    SetNodeAvailable = 6,
    /// `MakeObsolete(a)`
    MakeObsolete = 7,
    ObsoleteOutdated = 8,
    /// `SetPrerequisite(a, b)`
    SetPrerequisite = 9,
    /// `GetChecksum`'s value
    Checksum = 10,
    /// the saved game's `Load(a major, b minor)`: its result
    Load = 11,
    /// the saved game's `Save`: its result
    Save = 12,
    /// `ApplySavedState(a, b)`
    ApplySavedState = 13,
    /// `BuildUnitClassInfo` over this tree (gTechTree)
    BuildUnitClassInfo = 14,
}

/// A tree operation; `Checksum`'s value, `Load`'s and `Save`'s result, else 0.
///
/// # Safety
/// As [`ee_tech_walk_node`].
#[no_mangle]
pub unsafe extern "C" fn ee_tech_walk_tree(
    ctx: *mut c_void,
    f: EeTechHostFn,
    op: EeTechTreeOp,
    t: *mut EeTechTreeFields,
    a: i32,
    b: i32,
    unit: *mut c_void,
) -> i32 {
    let h = &mut FfiHost { ctx, f };
    let t = Tree(t);
    match op {
        EeTechTreeOp::Research => tree_research(h, t, a, unit),
        EeTechTreeOp::ResearchToEpoch => tree_research_to_epoch(h, t, a),
        EeTechTreeOp::SetEpoch => tree_set_epoch(h, t, a),
        EeTechTreeOp::AdvanceEpoch => tree_advance_epoch(h, t),
        EeTechTreeOp::SetEpochRange => tree_set_epoch_range(h, t, a, b),
        EeTechTreeOp::ResetEpochRange => tree_reset_epoch_range(h, t, a, b),
        EeTechTreeOp::SetNodeAvailable => tree_set_node_available(h, a, b != 0),
        EeTechTreeOp::MakeObsolete => tree_make_obsolete(h, t, a),
        EeTechTreeOp::ObsoleteOutdated => tree_obsolete_outdated(h, t),
        EeTechTreeOp::SetPrerequisite => tree_set_prerequisite(h, a, b),
        EeTechTreeOp::Checksum => return tree_checksum(h, t) as i32,
        EeTechTreeOp::Load => return tree_load(h, t, a as u32, b as u32),
        EeTechTreeOp::Save => return tree_save(h, t),
        EeTechTreeOp::ApplySavedState => tree_apply_saved_state(h, t, a != 0, b != 0),
        EeTechTreeOp::BuildUnitClassInfo => {
            let mut table = UNIT_CLASS_INFO.lock().unwrap_or_else(|e| e.into_inner());
            build_unit_class_info(h, &mut table);
        }
    }
    0
}

/// `EETechTree::IsLatestEffect` for the tree `ctx` (`t` its record).
///
/// # Safety
/// As [`ee_tech_walk_node`].
#[no_mangle]
pub unsafe extern "C" fn ee_tech_is_latest_effect(
    ctx: *mut c_void,
    f: EeTechHostFn,
    t: *mut EeTechTreeFields,
    id: i32,
    unit_type: i32,
    kind: i32,
) -> bool {
    // the table is read under its lock only: the host's calls run without it
    let list = UNIT_CLASS_INFO.lock().unwrap_or_else(|e| e.into_inner()).get(&unit_type).cloned();
    let mut table = UnitClassTable::new();
    if let Some(info) = list {
        table.insert(unit_type, info);
    }
    is_latest_effect(&mut FfiHost { ctx, f }, Tree(t), &table, id, unit_type, kind)
}

/// `EETechTreeNode::operator=` (0x5cfde2, `epoch` false) and `EETechTreeEpoch::operator=` (0x5ceb26, `epoch` true):
/// the record fields each copies (a node its flags, epochs, prerequisites, media flag, editor flag and delay; an epoch
/// its flags, epochs, counters and prerequisites, its counting off). The C++ copies the pointers (the shared data, the
/// button) and an epoch's nodes.
pub fn assign(dst: &mut EeTechNodeFields, src: &EeTechNodeFields, epoch: bool) {
    dst.mEpoch = src.mEpoch;
    dst.mObsoleteEpoch = src.mObsoleteEpoch;
    dst.mPrerequisiteFlags = src.mPrerequisiteFlags;
    dst.mIsResearched = src.mIsResearched;
    dst.mIsObsolete = src.mIsObsolete;
    dst.mIsAvailable = src.mIsAvailable;
    dst.mIsAllowed = src.mIsAllowed;
    if epoch {
        dst.mFirstNodeID = src.mFirstNodeID;
        dst.mRequiredSOL = src.mRequiredSOL;
        dst.mCurrentSOL = src.mCurrentSOL;
        dst.mNextEpoch = src.mNextEpoch;
        dst.mIsCountingSOL = false;
    } else {
        dst.mIsUnitMediaLoaded = src.mIsUnitMediaLoaded;
        dst.mScenEditorObsoleted = src.mScenEditorObsoleted;
        dst.mResearchDelay = src.mResearchDelay;
    }
}

/// [`assign`] over the records' pointers (a self-assignment does nothing).
///
/// # Safety
/// Both are live records.
#[no_mangle]
pub unsafe extern "C" fn ee_tech_node_assign(dst: *mut EeTechNodeFields, src: *const EeTechNodeFields, epoch: bool) {
    if std::ptr::eq(dst, src) {
        return;
    }
    // SAFETY: the contract; distinct records
    unsafe { assign(&mut *dst, &*src, epoch) }
}

#[cfg(test)]
#[path = "nodes_tests.rs"]
mod tests;
