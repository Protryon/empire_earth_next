use super::*;
use std::collections::{BTreeMap, VecDeque};

/// A node's two records, at a stable address.
#[derive(Default)]
struct Rec {
    f: EeTechNodeFields,
    d: EeTechNodeDataFields,
}

fn make(id: i32, prerequisites: [i32; 4]) -> *mut Rec {
    let mut r = Rec { f: node_fields_new(), ..Default::default() };
    r.d.mID = id as c_long;
    r.d.mPrerequisite1 = prerequisites[0] as c_long;
    r.d.mPrerequisite2 = prerequisites[1] as c_long;
    r.d.mPrerequisite3 = prerequisites[2] as c_long;
    r.d.mPrerequisite4 = prerequisites[3] as c_long;
    r.f.mPrerequisiteFlags = tree::initial_prerequisite_flags(&prerequisites);
    Box::into_raw(Box::new(r))
}

fn reference(p: *mut Rec) -> EeTechNodeRef {
    // SAFETY: the world's boxes live until it drops
    unsafe { EeTechNodeRef { obj: p.cast(), node: &mut (*p).f, data: &mut (*p).d } }
}

/// The C++ side: the tree's containers as boxes, the captured entry points back into the bodies, a log of the rest.
struct World {
    tree: *mut EeTechTreeFields,
    epochs: Vec<*mut Rec>,
    nodes: Vec<BTreeMap<i32, *mut Rec>>,
    log: Vec<String>,
    file: VecDeque<i32>,
    fail_read: Option<usize>,
    reads: usize,
    effects: BTreeMap<i32, Vec<[i32; 3]>>,
    classes: BTreeMap<i32, i32>,
    local: bool,
}

impl Drop for World {
    fn drop(&mut self) {
        // SAFETY: the boxes this world made
        unsafe {
            ee_tech_tree_fields_free(self.tree);
            drop(Box::from_raw(self.tree));
            for p in self.epochs.drain(..).chain(self.nodes.drain(..).flat_map(|m| m.into_values())) {
                drop(Box::from_raw(p));
            }
        }
    }
}

impl World {
    /// Epochs 0..3 with required counts; nodes by epoch: (id, prerequisites).
    fn new(nodes: &[(i32, i32, [i32; 4])]) -> World {
        let tree =
            Box::into_raw(Box::new(EeTechTreeFields { mEndEpoch: 2, mEpochsBeforeWonders: 1, ..Default::default() }));
        let mut w = World {
            tree,
            epochs: Vec::new(),
            nodes: vec![BTreeMap::new(); 3],
            log: Vec::new(),
            file: VecDeque::new(),
            fail_read: None,
            reads: 0,
            effects: BTreeMap::new(),
            classes: BTreeMap::new(),
            local: false,
        };
        for e in 0..3 {
            let p = make(e, [0; 4]);
            // SAFETY: just made
            unsafe {
                (*p).f.mEpoch = e as c_long;
                (*p).f.mRequiredSOL = 1;
                (*p).d.mResearchedEvent = 100 + e as c_long;
            }
            w.epochs.push(p);
        }
        for &(epoch, id, prerequisites) in nodes {
            let p = make(id, prerequisites);
            // SAFETY: just made
            unsafe {
                (*p).f.mEpoch = epoch as c_long;
                (*p).d.mResearchedEvent = id as c_long + 5;
                (*p).d.mObsoleteEvent = id as c_long + 6;
            }
            w.nodes[epoch as usize].insert(id, p);
        }
        w
    }
    fn t(&self) -> Tree {
        Tree(self.tree)
    }
    fn rec(&self, id: i32) -> &mut Rec {
        let p = if id < 3 { self.epochs[id as usize] } else { *self.nodes.iter().find_map(|m| m.get(&id)).unwrap() };
        // SAFETY: a test's short access, no host call meanwhile
        unsafe { &mut *p }
    }
    fn tree(&self) -> &mut EeTechTreeFields {
        // SAFETY: as rec
        unsafe { &mut *self.tree }
    }
    fn epoch_index(&self, obj: *mut c_void) -> Option<usize> {
        self.epochs.iter().position(|p| p.cast::<c_void>() == obj)
    }
    fn logged(&self, prefix: &str) -> Vec<String> {
        self.log.iter().filter(|l| l.starts_with(prefix)).cloned().collect()
    }
}

impl TechHost for World {
    fn call(&mut self, op: i32, io: &mut EeTechHostIo) -> i32 {
        let v = io.v;
        let t = self.t();
        match op {
            EE_TECH_HOST_EPOCHS => io.v[0] = self.epochs.len() as i32,
            EE_TECH_HOST_EPOCH_COUNT => io.v[0] = 3,
            EE_TECH_HOST_EPOCH => io.node = reference(self.epochs[v[0] as usize]),
            EE_TECH_HOST_EPOCH_NODES => {
                let e = self.epoch_index(io.node.obj).unwrap();
                let refs: Vec<_> = self.nodes[e].values().map(|p| reference(*p)).collect();
                for (k, r) in refs.iter().enumerate().take(io.count as usize) {
                    // SAFETY: the caller's buffer of count refs
                    unsafe { *io.nodes.add(k) = *r };
                }
                return refs.len() as i32;
            }
            EE_TECH_HOST_NODE => {
                let id = v[0];
                let found = if (0..3).contains(&id) {
                    Some(self.epochs[id as usize])
                } else {
                    self.nodes.iter().find_map(|m| m.get(&id).copied())
                };
                return match found {
                    Some(p) => {
                        io.node = reference(p);
                        1
                    }
                    None => 0,
                };
            }
            EE_TECH_HOST_EPOCH_OF => {
                let id = v[0];
                io.v[0] = if (0..3).contains(&id) {
                    id
                } else {
                    self.nodes.iter().position(|m| m.contains_key(&id)).map_or(-1, |e| e as i32)
                };
            }
            EE_TECH_HOST_RESEARCH_NODE => {
                self.log.push(format!("research {}", id_of(io.node)));
                return if self.epoch_index(io.node.obj).is_some() {
                    let id = id_of(io.node);
                    epoch_research(self, t, io.node, id, io.unit)
                } else {
                    node_research(self, io.node, io.unit)
                };
            }
            EE_TECH_HOST_SET_PREREQUISITE => node_set_prerequisite(self, io.node, v[0]),
            EE_TECH_HOST_EPOCH_RESEARCH => return epoch_research(self, t, io.node, v[0], io.unit),
            EE_TECH_HOST_ADD_SOL => epoch_add_sol(self, t, io.node),
            EE_TECH_HOST_UPDATE_AVAILABILITY => epoch_update_availability(self, t, io.node),
            EE_TECH_HOST_NEEDS_UNIT_MEDIA => return i32::from(data(io.node).mUnitClass == 77),
            EE_TECH_HOST_IS_LOCAL => return i32::from(self.local),
            EE_TECH_HOST_READ_LONG | EE_TECH_HOST_READ_BOOL => {
                self.reads += 1;
                if self.fail_read == Some(self.reads) {
                    return 1;
                }
                match self.file.pop_front() {
                    Some(x) => io.v[0] = x,
                    None => return 1,
                }
            }
            EE_TECH_HOST_WRITE_LONG | EE_TECH_HOST_WRITE_BOOL => self.file.push_back(v[0]),
            EE_TECH_HOST_EFFECT_COUNT => {
                return self.effects.get(&id_of(io.node)).map_or(0, |e| e.len() as i32);
            }
            EE_TECH_HOST_EFFECT => io.v[..3].copy_from_slice(&self.effects[&id_of(io.node)][v[0] as usize]),
            EE_TECH_HOST_CLASS_TYPE => {
                return match self.classes.get(&v[0]) {
                    Some(t) => {
                        io.v[0] = *t;
                        1
                    }
                    None => 0,
                };
            }
            EE_TECH_HOST_TRIGGER => self.log.push(format!("trigger {} {}", v[0], v[1])),
            EE_TECH_HOST_UPDATE_INTERFACE => self.log.push(format!("ui {}", v[0])),
            EE_TECH_HOST_ON_RESEARCHED => self.log.push(format!("researched {}", v[0])),
            EE_TECH_HOST_STATISTIC => self.log.push(format!("stat {} {} {}", v[0], v[1], v[2])),
            EE_TECH_HOST_UNLOAD_UNIT_MEDIA => self.log.push(format!("unload {}", v[0])),
            EE_TECH_HOST_WALK => {
                let node = if io.node.obj.is_null() { -1 } else { id_of(io.node) };
                self.log.push(format!("walk {} {} {} {} {} {}", v[0], v[1], v[2], v[3], v[4], node));
            }
            EE_TECH_HOST_WALK_STEP => self.log.push(format!("step {} {}", v[0], v[1])),
            EE_TECH_HOST_WALK_BEGIN => self.log.push("begin".into()),
            _ => self.log.push(format!("op {op} {v:?}")),
        }
        0
    }
}

/// The game's epoch 1 tree: 1000 (comes with epoch 0), 1001 (needs 1000; counts toward the next epoch), 2000 in
/// epoch 1.
fn game() -> World {
    let w = World::new(&[(0, 1000, [0; 4]), (0, 1001, [1000, 0, 0, 0]), (1, 2000, [0; 4])]);
    w.rec(1000).d.mAvailableWithEpoch = true;
    w.rec(1001).d.mCountsTowardEpoch = true;
    w
}

#[test]
fn the_start_unlocks_and_researches_the_first_epoch() {
    let mut w = game();
    let t = w.t();
    tree_set_epoch_range(&mut w, t, 0, 2);
    // the captures and the steps in the original's order
    assert_eq!(w.log[0], "op 33 [2, 0, 2, 0, 0]");
    assert_eq!(w.log[1], "op 33 [1, 0, 0, 0, 0]");
    assert_eq!(w.log[2], "op 25 [0, 0, 0, 0, 0]");
    assert!(w.log.last().unwrap() == "op 25 [1, 0, 0, 0, 0]");
    let e0 = w.rec(0);
    assert!(e0.f.mIsAvailable && e0.f.mIsResearched && e0.f.mCurrentSOL == 1);
    assert!(w.rec(1000).f.mIsAvailable);
    assert_eq!(w.rec(1000).f.mPrerequisiteFlags, tree::PREREQUISITE_ALL);
    // 1001 has the epoch, waits for 1000
    assert!(!w.rec(1001).f.mIsAvailable);
    assert_eq!(w.rec(1001).f.mPrerequisiteFlags, 0x1d);
    assert_eq!(w.tree().mCurrentEpoch, 0);
    assert!(w.log.contains(&"trigger 100 0".to_string()));
    assert!(w.log.contains(&"researched 0".to_string()));
    // the unlock's walk: its steps (1000 made available, 1001 given the epoch), then the record
    let unlock = w.log.iter().position(|l| l.starts_with("walk 0 ")).unwrap();
    assert_eq!(w.log[unlock], "walk 0 0 0 0 1 0");
    assert_eq!(&w.logged("step 100")[..2], &["step 1000 1", "step 1001 2"]);
}

#[test]
fn research_counts_toward_the_next_epoch() {
    let mut w = game();
    let t = w.t();
    tree_set_epoch_range(&mut w, t, 0, 2);
    w.rec(1).f.mIsCountingSOL = true;
    w.log.clear();
    tree_research(&mut w, t, 1000, ptr::null_mut());
    assert!(w.rec(1000).f.mIsResearched);
    assert_eq!(w.log, vec!["research 1000", "trigger 1005 1000", "ui 0", "researched 1000"]);
    // not available yet: refused, nothing counted
    w.log.clear();
    tree_research(&mut w, t, 1001, ptr::null_mut());
    assert_eq!(w.log, vec!["research 1001"]);
    tree_set_prerequisite(&mut w, 1001, 1000);
    assert!(w.rec(1001).f.mIsAvailable);
    assert!(w.log.contains(&format!("trigger {} 1001", EVENT_HOOK_TECH_RESEARCHED)));
    assert!(w.log.contains(&"stat 1 22 0".to_string()));
    w.log.clear();
    tree_research(&mut w, t, 1001, ptr::null_mut());
    assert_eq!(w.rec(1).f.mCurrentSOL, 1);
    assert!(w.rec(1).f.mIsAvailable);
    assert!(w.log.contains(&format!("trigger {} 1", EVENT_HOOK_EPOCH_AVAILABLE)));
    // the epoch's research: the statistics series, the tree's epoch
    w.log.clear();
    tree_research(&mut w, t, 1, ptr::null_mut());
    assert_eq!(w.tree().mCurrentEpoch, 1);
    assert!(w.log.contains(&"stat 0 12 0".to_string()));
    assert_eq!(w.rec(2000).f.mPrerequisiteFlags, 0x1f);
    // a refused id outside every epoch: nothing at all
    w.log.clear();
    tree_research(&mut w, t, 9999, ptr::null_mut());
    assert!(w.log.is_empty());
}

#[test]
fn availability_and_obsolescence_by_kind() {
    let w = game();
    let mut w = w;
    let t = w.t();
    let n = reference(w.epochs[0]);
    w.rec(0).f.mIsAvailable = true;
    assert!(is_available(&mut w, t, n));
    w.rec(0).f.mObsoleteEpoch = -5;
    // an epoch's obsolescence is its flag alone
    assert!(!is_obsolete(&mut w, t, n));
    let n = reference(*w.nodes[0].get(&1000).unwrap());
    w.rec(1000).f.mObsoleteEpoch = -1;
    assert!(is_obsolete(&mut w, t, n));
    w.rec(1000).f.mIsAvailable = true;
    assert!(!is_available(&mut w, t, n));
}

#[test]
fn obsolete_nodes_unload_their_media_unless_kept() {
    let mut w = game();
    let t = w.t();
    w.rec(1001).d.mUnitClass = 77;
    w.rec(1001).f.mIsUnitMediaLoaded = true;
    tree_make_obsolete(&mut w, t, 1001);
    assert!(w.rec(1001).f.mIsObsolete && !w.rec(1001).f.mIsUnitMediaLoaded);
    assert!(w.log.contains(&"unload 77".to_string()));
    assert!(w.log.contains(&"walk 5 1 0 3 1 1001".to_string()));
    assert_eq!(w.log.last().unwrap(), "walk 4 1001 0 1 1 0");
    // again: already obsolete, nothing fires
    w.log.clear();
    let n = reference(*w.nodes[0].get(&1001).unwrap());
    node_set_obsolete(&mut w, n, true, false);
    assert_eq!(w.log, vec!["begin", "walk 5 1 0 0 1 1001"]);
    // kept media
    w.rec(1000).d.mUnitClass = 77;
    w.rec(1000).f.mIsUnitMediaLoaded = true;
    let n = reference(*w.nodes[0].get(&1000).unwrap());
    node_set_obsolete(&mut w, n, true, true);
    assert!(w.rec(1000).f.mIsUnitMediaLoaded);
    // the epoch itself while ahead: its obsolete event; once reached: nothing
    w.log.clear();
    tree_make_obsolete(&mut w, t, 2);
    assert!(w.rec(2).f.mIsObsolete);
    assert_eq!(w.log[1], "trigger 0 2");
    w.tree().mCurrentEpoch = 1;
    w.log.clear();
    tree_make_obsolete(&mut w, t, 1);
    assert_eq!(w.log, vec!["begin", "walk 4 1 0 3 1 1"]);
    // outdated nodes of the epochs up to the current one
    w.rec(2000).f.mObsoleteEpoch = 0;
    tree_obsolete_outdated(&mut w, t);
    assert!(w.rec(2000).f.mIsObsolete);
}

#[test]
fn the_editor_flag_recurses_both_ways() {
    let mut w =
        World::new(&[(0, 1000, [0; 4]), (0, 1001, [1000, 0, 0, 0]), (0, 1002, [0, 0, 0, 1001]), (1, 2000, [0; 4])]);
    let a = reference(*w.nodes[0].get(&1000).unwrap());
    node_set_editor_obsoleted(&mut w, a, true, true);
    assert!(
        w.rec(1000).f.mScenEditorObsoleted && w.rec(1001).f.mScenEditorObsoleted && w.rec(1002).f.mScenEditorObsoleted
    );
    assert!(!w.rec(2000).f.mScenEditorObsoleted);
    let c = reference(*w.nodes[0].get(&1002).unwrap());
    node_set_editor_obsoleted(&mut w, c, false, true);
    assert!(!w.rec(1000).f.mScenEditorObsoleted && !w.rec(1001).f.mScenEditorObsoleted);
    // without recursion only the node
    node_set_editor_obsoleted(&mut w, a, true, false);
    assert!(w.rec(1000).f.mScenEditorObsoleted && !w.rec(1001).f.mScenEditorObsoleted);
    assert!(node_depends_on(&mut w, c, 1000));
    assert!(!node_depends_on(&mut w, c, 2000));
}

#[test]
fn a_saved_tree_loads_and_applies_back() {
    let mut w = game();
    let t = w.t();
    tree_set_epoch_range(&mut w, t, 0, 2);
    tree_research(&mut w, t, 1000, ptr::null_mut());
    w.rec(2000).f.mScenEditorObsoleted = true;
    w.rec(1001).f.mIsObsolete = true;
    w.rec(1).f.mCurrentSOL = 1;
    assert_eq!(tree_save(&mut w, t), 0);
    // epochs, wonders, 4 ids (1000 researched, 1001 obsolete, 2000 editor, epoch 0 researched)
    let words: Vec<i32> = w.file.iter().copied().collect();
    assert_eq!(&words[..3], &[3, 1, 4]);
    assert_eq!(&words[3..19], &[1000, 1, 0, 0, 1001, 0, 1, 0, 2000, 0, 0, 1, 0, 1, 0, 0]);
    assert_eq!(&words[19..], &[0, 0, 2, 1, 1, 0]);
    let file = w.file.clone();

    let mut b = game();
    b.file = file;
    let bt = b.t();
    assert_eq!(tree_load(&mut b, bt, 0, 0x118), 0);
    assert!(b.tree().mIsLoaded);
    let saved = bt.saved_copy();
    assert_eq!(saved.researched, vec![0, 1000, 0]);
    assert_eq!(saved.obsolete, vec![1001]);
    assert_eq!(saved.editor_obsoleted, vec![2000]);
    assert_eq!(saved.sols, vec![(0, 1), (1, 1), (2, 0)]);
    assert!(!b.rec(1).f.mIsCountingSOL);
    tree_apply_saved_state(&mut b, bt, true, true);
    for id in [0, 1000, 1001, 2000] {
        assert_eq!(b.rec(id).f.mIsResearched, w.rec(id).f.mIsResearched, "{id}");
        assert_eq!(b.rec(id).f.mIsObsolete, w.rec(id).f.mIsObsolete, "{id}");
        assert_eq!(b.rec(id).f.mScenEditorObsoleted, w.rec(id).f.mScenEditorObsoleted, "{id}");
    }
    assert!(b.rec(1).f.mIsCountingSOL && b.rec(1).f.mIsAvailable);
    assert_eq!(b.log.last().unwrap(), "ui 1");
    let checksum = tree_checksum(&mut b, bt);
    assert_eq!(checksum, tree_checksum(&mut w, t));
}

#[test]
fn old_saves_read_fourteen_epochs_and_a_failed_read_stops() {
    let mut w = game();
    let t = w.t();
    w.file = VecDeque::from(vec![3, 1, 0, 2, 0, 2, 7, 8, 9]);
    // 0.0xe9: 14 epochs asked, the tree's three read
    assert_eq!(tree_load(&mut w, t, 0, 0xe9), 0);
    assert_eq!(w.tree().mCurrentEpoch, 2);
    assert_eq!(w.rec(2).f.mCurrentSOL, 9);
    assert_eq!(w.t().saved_copy().researched, vec![0, 1, 2]);
    // the wonders' read fails: nothing more
    let mut b = game();
    b.file = VecDeque::from(vec![3, 1, 0]);
    b.fail_read = Some(2);
    let bt = b.t();
    assert_eq!(tree_load(&mut b, bt, 0, 0x118), 1);
    assert!(!b.tree().mIsLoaded);
}

#[test]
fn checksum_matches_the_weighted_sum() {
    let mut w = game();
    let t = w.t();
    w.rec(1000).f.mIsResearched = true;
    w.rec(2000).f.mIsObsolete = true;
    let ids = [1000, 1001, 2000, 0, 1, 2];
    let entries: Vec<_> = ids
        .iter()
        .map(|&id| tree::EeTechChecksumEntry { id, researched: id == 1000, obsolete: id == 2000, pad: [0; 2] })
        .collect();
    assert_eq!(tree_checksum(&mut w, t), tree::checksum(&entries));
}

#[test]
fn epochs_set_advance_and_nodes_wait_for_their_delay() {
    let mut w = game();
    let t = w.t();
    w.local = true;
    tree_set_epoch(&mut w, t, 1);
    assert_eq!(w.tree().mCurrentEpoch, 1);
    assert!(w.log.contains(&"stat 2 12 1".to_string()));
    assert!(w.log.contains(&"op 27 [1, 0, 0, 0, 0]".to_string()));
    assert_eq!(w.log.last().unwrap(), "walk 7 1 0 1 1 -1");
    // never back
    w.log.clear();
    tree_set_epoch(&mut w, t, 0);
    assert_eq!(w.log.last().unwrap(), "walk 7 0 0 0 1 -1");
    tree_advance_epoch(&mut w, t);
    assert_eq!(w.tree().mCurrentEpoch, 2);
    w.log.clear();
    tree_advance_epoch(&mut w, t);
    assert_eq!(w.log, vec!["begin", "walk 8 3 2 0 1 -1"]);
    tree_reset_epoch_range(&mut w, t, 1, 2);
    assert_eq!(w.tree().mCurrentEpoch, 1);
    w.rec(2000).f.mResearchDelay = 2;
    tree_set_node_available(&mut w, 2000, false);
    assert!(w.rec(2000).f.mIsAvailable);
    w.rec(2000).f.mResearchDelay = 0;
    tree_set_node_available(&mut w, 2000, false);
    assert!(!w.rec(2000).f.mIsAvailable);
    assert_eq!(w.log.last().unwrap(), "walk 9 2000 0 1 1 2000");
}

#[test]
fn a_research_delay_counts_down_and_kinds_research_with_their_epoch() {
    let mut w = game();
    let n = reference(*w.nodes[0].get(&1000).unwrap());
    w.rec(1000).f.mResearchDelay = 2;
    w.rec(1000).f.mIsAvailable = true;
    assert_eq!(node_research(&mut w, n, ptr::null_mut()), tree::RESULT_REFUSED);
    assert_eq!(w.rec(1000).f.mResearchDelay, 1);
    w.rec(1001).d.mKind = 7;
    w.rec(1001).d.mResearchedWithEpoch = true;
    let e = reference(w.epochs[0]);
    w.log.clear();
    epoch_research_kind(&mut w, e, 7, true);
    assert_eq!(w.log, vec!["begin", "step 1000 0", "step 1001 1", "research 1001", "walk 2 7 1 0 1 0"]);
}

#[test]
fn the_record_fills_the_node_and_the_epoch() {
    // SAFETY: an all-zero record is a valid value
    let mut r: EeTechTreeRecord = unsafe { std::mem::zeroed() };
    r.mID = 1203;
    r.mEpoch = 4;
    r.mObsoleteEpoch = 9;
    r.mCost2 = 50;
    r.mCost3 = 30;
    r.mCost4 = 40;
    r.mCost5 = 60;
    r.mCost6 = 70;
    r.mPrerequisite2 = 1100;
    r.mMakesUnit = 1;
    r.mObsoletedByDefault = 1;
    r.mRequiredSOL = 6;
    r.mCalamity = -1;
    let mut n = node_fields_new();
    let mut d = EeTechNodeDataFields { mCosts: [9; 7], ..Default::default() };
    // SAFETY: live locals
    unsafe { ee_tech_fill_from_record(&mut n, &mut d, &r, false) };
    assert_eq!(d.mCosts, [0, 0, 50, 30, 40, 60, 70]);
    assert!(d.mMakesUnit && d.mObsoletedByDefault && !d.mCountsTowardEpoch);
    assert_eq!(d.mCalamity, 0xffff_ffff);
    assert_eq!(n.mPrerequisiteFlags, 2 | 8 | 0x10);
    assert!(n.mScenEditorObsoleted && n.mIsAllowed);
    assert_eq!((n.mEpoch, n.mObsoleteEpoch, n.mRequiredSOL), (0, 9, 0));
    let mut e = node_fields_new();
    // SAFETY: as above
    unsafe { ee_tech_fill_from_record(&mut e, &mut d, &r, true) };
    assert_eq!((e.mEpoch, e.mObsoleteEpoch, e.mRequiredSOL, e.mPrerequisiteFlags), (4, 9, 6, 0));
    assert!(!e.mScenEditorObsoleted);
}

#[test]
fn the_media_table_files_effects_by_unit_type() {
    let mut w = game();
    w.classes.insert(5, 50);
    w.classes.insert(6, 60);
    w.rec(2000).d.mUnitClass = 6;
    // 1000's event: a model for class 5, a sound for class 5, an upgrade to what 2000 builds; 1001's: a model
    w.effects.insert(1000, vec![[6, 5, 0], [0x11, 5, 0], [9, 0, 2000], [6, 9, 0]]);
    w.effects.insert(1001, vec![[6, 5, 0]]);
    let mut table = UnitClassTable::new();
    build_unit_class_info(&mut w, &mut table);
    assert_eq!(table[&50].techs[0], vec![1000, 1001]);
    assert_eq!(table[&50].techs[3], vec![1000]);
    assert_eq!(table[&60].technology, 1000);
    assert_eq!(table[&50].technology, tree::TECH_NONE);
    let t = w.t();
    w.tree().mStartEpoch = 1;
    // 1001 comes later and is used (its epoch 0 before the start): 1000's model is not the latest
    assert!(!is_latest_effect(&mut w, t, &table, 1000, 50, 6));
    assert!(is_latest_effect(&mut w, t, &table, 1001, 50, 6));
    w.rec(1001).f.mScenEditorObsoleted = true;
    assert!(is_latest_effect(&mut w, t, &table, 1000, 50, 6));
    assert!(is_latest_effect(&mut w, t, &table, 1000, 51, 6));
}
