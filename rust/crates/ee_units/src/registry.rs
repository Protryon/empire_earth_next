//! A unit id -> unit object table that any thread may read while another registers or unregisters.
//!
//! A unit id is the player id in the top byte and a 24-bit per-player counter below (`RTSPlayer::AddUnit`); ids are
//! never reused within a game. The table is a three-level radix tree over the id's bits (8 / 12 / 12): a root of
//! 256 branch pointers, a branch of 4096 leaf pointers, a leaf of 4096 unit pointers. Every pointer in it is
//! atomic; branches and leaves are allocated on first use and never freed while the registry lives, so a reader
//! holds no lock and follows at most three loads. Register is a store, unregister a compare-exchange (the entry is
//! cleared only if it still names that object: a second object with the same id replaces the first, and the first
//! one's unregister then leaves the entry alone).
//!
//! What a reader sees while a writer runs: the pointer or null, never a torn value or a freed table. Whether the
//! unit behind the pointer is still alive is the caller's business, as it was with the raw pointer the handle
//! replaces (a holder that referenced the unit still does).

use std::ptr;
use std::sync::atomic::{AtomicPtr, Ordering};

const TOP_BITS: u32 = 8;
const MID_BITS: u32 = 12;
const LOW_BITS: u32 = 12;
const TOP_COUNT: usize = 1 << TOP_BITS;
const MID_COUNT: usize = 1 << MID_BITS;
const LOW_COUNT: usize = 1 << LOW_BITS;

struct Leaf<T> {
    slots: [AtomicPtr<T>; LOW_COUNT],
}

struct Branch<T> {
    leaves: [AtomicPtr<Leaf<T>>; MID_COUNT],
}

/// The table. `T` is the unit type the pointers name; the registry never reads through them.
pub struct Registry<T> {
    roots: [AtomicPtr<Branch<T>>; TOP_COUNT],
}

impl<T> Leaf<T> {
    fn boxed() -> Box<Self> {
        Box::new(Leaf { slots: [const { AtomicPtr::new(ptr::null_mut()) }; LOW_COUNT] })
    }
}

impl<T> Branch<T> {
    fn boxed() -> Box<Self> {
        Box::new(Branch { leaves: [const { AtomicPtr::new(ptr::null_mut()) }; MID_COUNT] })
    }
}

/// The three indices of an id.
fn split(id: u32) -> (usize, usize, usize) {
    (
        (id >> (MID_BITS + LOW_BITS)) as usize,
        ((id >> LOW_BITS) & (MID_COUNT as u32 - 1)) as usize,
        (id & (LOW_COUNT as u32 - 1)) as usize,
    )
}

/// The node behind `slot`, allocated by `make` when there is none yet. Two threads allocating at once both try
/// to install theirs; the loser frees its own and uses the winner's.
fn get_or_insert<N>(slot: &AtomicPtr<N>, make: impl FnOnce() -> Box<N>) -> &N {
    let existing = slot.load(Ordering::Acquire);
    if !existing.is_null() {
        // SAFETY: a node installed in a slot lives as long as the registry (see Drop)
        return unsafe { &*existing };
    }
    let fresh = Box::into_raw(make());
    match slot.compare_exchange(ptr::null_mut(), fresh, Ordering::AcqRel, Ordering::Acquire) {
        // SAFETY: installed, so it lives as long as the registry
        Ok(_) => unsafe { &*fresh },
        Err(winner) => {
            // SAFETY: `fresh` came from Box::into_raw above and was never shared
            drop(unsafe { Box::from_raw(fresh) });
            // SAFETY: as above
            unsafe { &*winner }
        }
    }
}

impl<T> Registry<T> {
    /// An empty registry (no allocation: a `static` can hold one).
    pub const fn new() -> Self {
        Registry { roots: [const { AtomicPtr::new(ptr::null_mut()) }; TOP_COUNT] }
    }

    /// The leaf that holds `id`'s slot, if any level on the way has been allocated.
    fn leaf(&self, top: usize, mid: usize) -> Option<&Leaf<T>> {
        let branch = self.roots[top].load(Ordering::Acquire);
        if branch.is_null() {
            return None;
        }
        // SAFETY: an installed node lives as long as the registry
        let leaf = unsafe { (*branch).leaves[mid].load(Ordering::Acquire) };
        if leaf.is_null() {
            return None;
        }
        // SAFETY: as above
        Some(unsafe { &*leaf })
    }

    /// The object registered under `id`, or null.
    pub fn resolve(&self, id: u32) -> *mut T {
        let (top, mid, low) = split(id);
        match self.leaf(top, mid) {
            Some(leaf) => leaf.slots[low].load(Ordering::Acquire),
            None => ptr::null_mut(),
        }
    }

    /// Makes `id` resolve to `unit` (replacing whatever it resolved to).
    pub fn register(&self, id: u32, unit: *mut T) {
        let (top, mid, low) = split(id);
        let branch = get_or_insert(&self.roots[top], Branch::boxed);
        let leaf = get_or_insert(&branch.leaves[mid], Leaf::boxed);
        leaf.slots[low].store(unit, Ordering::Release);
    }

    /// Clears `id`'s entry if it still names `unit`; returns whether it did.
    pub fn unregister(&self, id: u32, unit: *mut T) -> bool {
        let (top, mid, low) = split(id);
        match self.leaf(top, mid) {
            Some(leaf) => {
                leaf.slots[low].compare_exchange(unit, ptr::null_mut(), Ordering::AcqRel, Ordering::Relaxed).is_ok()
            }
            None => false,
        }
    }

    /// How many branches and leaves are allocated (the table's memory: 32 KiB each on a 64-bit build).
    pub fn allocated_nodes(&self) -> (usize, usize) {
        let mut branches = 0;
        let mut leaves = 0;
        for root in &self.roots {
            let branch = root.load(Ordering::Acquire);
            if branch.is_null() {
                continue;
            }
            branches += 1;
            // SAFETY: an installed node lives as long as the registry
            let branch = unsafe { &*branch };
            leaves += branch.leaves.iter().filter(|leaf| !leaf.load(Ordering::Acquire).is_null()).count();
        }
        (branches, leaves)
    }
}

impl<T> Default for Registry<T> {
    fn default() -> Self {
        Self::new()
    }
}

impl<T> Drop for Registry<T> {
    /// Frees the tree. A `static` registry is never dropped; a registry a test owns is dropped when no other thread
    /// reads it any more (`&self` methods borrow it).
    fn drop(&mut self) {
        for root in &self.roots {
            let branch = root.swap(ptr::null_mut(), Ordering::AcqRel);
            if branch.is_null() {
                continue;
            }
            // SAFETY: installed by get_or_insert from Box::into_raw, unlinked just now, and `&mut self` means no
            // reader can reach it
            let branch = unsafe { Box::from_raw(branch) };
            for leaf in &branch.leaves {
                let leaf = leaf.swap(ptr::null_mut(), Ordering::AcqRel);
                if !leaf.is_null() {
                    // SAFETY: as above
                    drop(unsafe { Box::from_raw(leaf) });
                }
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::sync::atomic::AtomicBool;
    use std::sync::Arc;
    use std::thread;

    /// A pointer that stands for the unit of `id` (never dereferenced): distinct per id, never null.
    fn unit(id: u32) -> *mut u8 {
        (id as usize + 1).wrapping_mul(16) as *mut u8
    }

    #[test]
    fn empty_registry_resolves_nothing() {
        let registry: Registry<u8> = Registry::new();
        for id in [0, 1, 0x0100_0000, 0x0100_0001, 0x00ff_ffff, 0xffff_ffff] {
            assert!(registry.resolve(id).is_null());
            assert!(!registry.unregister(id, unit(id)));
        }
        assert_eq!(registry.allocated_nodes(), (0, 0));
    }

    #[test]
    fn register_resolve_unregister() {
        let registry: Registry<u8> = Registry::new();
        registry.register(0x0100_0007, unit(7));
        assert_eq!(registry.resolve(0x0100_0007), unit(7));
        assert!(registry.resolve(0x0100_0006).is_null());
        assert!(registry.resolve(0x0200_0007).is_null());
        assert!(registry.unregister(0x0100_0007, unit(7)));
        assert!(registry.resolve(0x0100_0007).is_null());
        assert!(!registry.unregister(0x0100_0007, unit(7)));
    }

    #[test]
    fn unregister_clears_only_the_same_object() {
        let registry: Registry<u8> = Registry::new();
        registry.register(5, unit(5));
        registry.register(5, unit(6)); // a second object with the same id replaces the first
        assert_eq!(registry.resolve(5), unit(6));
        assert!(!registry.unregister(5, unit(5))); // the first one's FreeNode leaves the entry alone
        assert_eq!(registry.resolve(5), unit(6));
        assert!(registry.unregister(5, unit(6)));
        assert!(registry.resolve(5).is_null());
    }

    #[test]
    fn every_bit_of_the_id_counts() {
        let registry: Registry<u8> = Registry::new();
        let ids: Vec<u32> = (0..32).map(|bit| 1u32 << bit).chain([0, 0xffff_ffff, 0x0100_1001]).collect();
        for &id in &ids {
            registry.register(id, unit(id));
        }
        for &id in &ids {
            assert_eq!(registry.resolve(id), unit(id), "id {id:#x}");
        }
        // bits 0..11 share one leaf, 12..23 one branch; the others each need a branch and a leaf
        assert_eq!(registry.allocated_nodes(), (8 + 1 + 1, 8 + 12 + 1 + 1 + 1));
        for &id in &ids {
            assert!(registry.unregister(id, unit(id)));
        }
        for &id in &ids {
            assert!(registry.resolve(id).is_null());
        }
    }

    #[test]
    fn a_players_units_share_pages() {
        let registry: Registry<u8> = Registry::new();
        for counter in 0..10_000u32 {
            registry.register(0x0300_0000 | counter, unit(counter));
        }
        assert_eq!(registry.allocated_nodes(), (1, 3)); // 10,000 ids fill two and a half leaves
        for counter in 0..10_000u32 {
            assert_eq!(registry.resolve(0x0300_0000 | counter), unit(counter));
        }
        assert!(registry.resolve(0x0300_0000 | 10_000).is_null());
        assert!(registry.resolve(0x0400_0000).is_null());
    }

    #[test]
    fn a_dropped_registry_frees_its_pages() {
        let registry: Registry<u8> = Registry::new();
        for top in 0..TOP_COUNT as u32 {
            registry.register(top << 24, unit(top));
        }
        assert_eq!(registry.allocated_nodes(), (256, 256));
        drop(registry); // no leak check here: the test runs under the address sanitiser in CI when it does
    }

    /// Writers register and unregister their own ids while readers resolve them: a reader sees the id's object or
    /// null, never another id's object. The ids share leaves (same top byte and page) so the writers race on one
    /// node's allocation too.
    #[test]
    fn readers_see_the_object_or_null_while_writers_run() {
        let registry: Arc<Registry<u8>> = Arc::new(Registry::new());
        let stop = Arc::new(AtomicBool::new(false));
        let writers: Vec<_> = (0..4u32)
            .map(|w| {
                let registry = Arc::clone(&registry);
                thread::spawn(move || {
                    for round in 0..2_000u32 {
                        for k in 0..64u32 {
                            let id = 0x0500_0000 | (w * 64 + k) | ((round % 3) << 12);
                            registry.register(id, unit(id));
                        }
                        for k in 0..64u32 {
                            let id = 0x0500_0000 | (w * 64 + k) | ((round % 3) << 12);
                            assert_eq!(registry.resolve(id), unit(id));
                            assert!(registry.unregister(id, unit(id)));
                        }
                    }
                })
            })
            .collect();
        let readers: Vec<_> = (0..3)
            .map(|_| {
                let registry = Arc::clone(&registry);
                let stop = Arc::clone(&stop);
                thread::spawn(move || {
                    let mut seen = 0u64;
                    while !stop.load(Ordering::Relaxed) {
                        for id in 0x0500_0000..0x0500_0100u32 {
                            for page in 0..3u32 {
                                let id = id | (page << 12);
                                let p = registry.resolve(id);
                                assert!(p.is_null() || p == unit(id), "id {id:#x} resolved to another object");
                                seen += !p.is_null() as u64;
                            }
                        }
                    }
                    seen
                })
            })
            .collect();
        for w in writers {
            w.join().unwrap();
        }
        stop.store(true, Ordering::Relaxed);
        for r in readers {
            r.join().unwrap();
        }
        for id in 0x0500_0000..0x0500_0100u32 {
            assert!(registry.resolve(id).is_null());
        }
    }

    /// The case the lock is for: the last Dereference (unregister) on one thread while another resolves the same
    /// id. The resolve gives the object or null, and the unregister succeeds exactly once.
    #[test]
    fn unregister_on_one_thread_resolve_on_another() {
        let registry: Arc<Registry<u8>> = Arc::new(Registry::new());
        for round in 0..200u32 {
            let id = 0x0600_0000 | round;
            registry.register(id, unit(id));
            let reader = {
                let registry = Arc::clone(&registry);
                thread::spawn(move || {
                    let mut last_seen_object = false;
                    for _ in 0..1_000 {
                        let p = registry.resolve(id);
                        assert!(p.is_null() || p == unit(id));
                        if p.is_null() {
                            // once gone, it stays gone
                            last_seen_object = false;
                        } else {
                            assert!(!last_seen_object || p == unit(id));
                            last_seen_object = true;
                        }
                    }
                })
            };
            let unregistered = registry.unregister(id, unit(id));
            reader.join().unwrap();
            assert!(unregistered);
            assert!(!registry.unregister(id, unit(id)));
        }
    }
}
