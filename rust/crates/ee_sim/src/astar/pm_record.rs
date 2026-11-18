//! The path manager's request record (`EEPathManager`'s `mHistory`, `mRegions` and their two prune timers), whole:
//! `AddHistory`, the history walk of `SetGraphRevs`, `PruneHistory`, `PruneRegions`' choice, `FreeRegions`, the prune
//! schedule of `UpdatePaths`, and the record's part of `SaveRunningState` / `LoadRunningState` (save 0.256's layout,
//! unchanged). It is the Rust path manager's (pm.rs), which also cancels the stranded units' paths and tells their
//! players.
//!
//! The orders are the C++ containers': the history is a deque, newest first; the region maps are
//! `std::map<long, std::map<U2DPoint<long>, unsigned long>*>`, by unit id and then by `U2DPoint<long>::operator<`
//! (lower y first, then the larger x). `long` is the Windows build's `i32`; the game time is `unsigned long` (`u32`),
//! and the timers' `long <= unsigned long` compares as unsigned.

use std::cmp::Reverse;
use std::collections::{BTreeMap, VecDeque};

/// `EEPathHistoryEntry`: a unit's request for a path to a tile at a game time.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct HistoryEntry {
    pub x: i32,
    pub y: i32,
    pub unit: i32,
    pub time: u32,
}

/// A region map's key in `U2DPoint<long>::operator<`'s order.
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord)]
pub struct TileKey(i32, Reverse<i32>);

impl TileKey {
    pub fn new(x: i32, y: i32) -> TileKey {
        TileKey(y, Reverse(x))
    }
    pub fn x(&self) -> i32 {
        (self.1).0
    }
    pub fn y(&self) -> i32 {
        self.0
    }
}

/// `PruneHistory`'s window (5.5 s) and the prune periods of `UpdatePaths`.
const HISTORY_WINDOW: u32 = 0x157c;
const HISTORY_PERIOD: u32 = 10000;
const REGION_PERIOD: u32 = 30000;
/// A repeated goal tile's count from which `PruneRegions` calls its unit stranded.
const STRANDED_COUNT: u32 = 8;

/// The record.
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct PathRecord {
    pub history: VecDeque<HistoryEntry>,
    pub regions: BTreeMap<i32, BTreeMap<TileKey, u32>>,
    /// `mNextHistoryPruneTime`, `mNextRegionPruneTime` (`long`)
    pub next_history_prune: i32,
    pub next_region_prune: i32,
}

impl PathRecord {
    /// `AddHistory`: newest first.
    pub fn add_history(&mut self, unit: i32, x: i32, y: i32, time: u32) {
        self.history.push_front(HistoryEntry { x, y, unit, time });
    }

    /// `SetGraphRevs`' walk: whether the unit's newest request at or after `since` was for this tile.
    pub fn repeated(&self, unit: i32, x: i32, y: i32, since: u32) -> bool {
        for e in &self.history {
            if e.time < since {
                break;
            }
            if e.unit != unit {
                continue;
            }
            return e.x == x && e.y == y;
        }
        false
    }

    /// `PruneHistory`: the requests of the last 5.5 s counted per unit and tile; a tile asked for more than once goes
    /// into its unit's region map (with its count when new, one more when there); then the history is empty. While
    /// a saved game loads it only empties the history.
    pub fn prune_history(&mut self, now: u32, loading: bool) {
        let mut time = now;
        if time > HISTORY_WINDOW {
            time -= HISTORY_WINDOW;
        }
        if !loading {
            let mut counts: Vec<(HistoryEntry, u32)> = Vec::new();
            for i in 0..self.history.len() {
                let e = self.history[i];
                if e.time <= time {
                    self.history.truncate(i);
                    break;
                }
                match counts.iter().rposition(|(c, _)| c.unit == e.unit && c.x == e.x && c.y == e.y) {
                    Some(k) => counts[k].1 = counts[k].1.wrapping_add(1),
                    None => counts.push((e, 1)),
                }
            }
            for &(e, n) in counts.iter().rev() {
                if n <= 1 {
                    continue;
                }
                let key = TileKey::new(e.x, e.y);
                match self.regions.get_mut(&e.unit) {
                    Some(map) => match map.get_mut(&key) {
                        Some(v) => *v = v.wrapping_add(1),
                        None => {
                            map.insert(key, n);
                        }
                    },
                    None => {
                        self.regions.insert(e.unit, BTreeMap::from([(key, n)]));
                    }
                }
            }
        }
        self.history.clear();
    }

    /// `PruneRegions`' choice: the units with a tile counted 8 or more, in the order the C++ handles them (the last
    /// found first); every region map is freed. While a saved game loads: nothing, the maps stay.
    pub fn prune_regions(&mut self, loading: bool) -> Vec<i32> {
        if loading {
            return Vec::new();
        }
        let mut ids: Vec<i32> =
            self.regions.iter().filter(|(_, m)| m.values().any(|&v| v >= STRANDED_COUNT)).map(|(&id, _)| id).collect();
        self.regions.clear();
        ids.reverse();
        ids
    }

    /// `UpdatePaths`' schedule after the searches: whether to prune the history and the regions now (each only when
    /// it holds something), their next times set when due.
    pub fn due(&mut self, now: u32) -> (bool, bool) {
        let mut history = false;
        let mut regions = false;
        if self.next_history_prune as u32 <= now {
            history = !self.history.is_empty();
            self.next_history_prune = now.wrapping_add(HISTORY_PERIOD) as i32;
        }
        if self.next_region_prune as u32 <= now {
            regions = !self.regions.is_empty();
            self.next_region_prune = now.wrapping_add(REGION_PERIOD) as i32;
        }
        (history, regions)
    }

    /// `SaveRunningState`'s first part as the 4-byte words the C++ writes: the history's count and entries (x, y,
    /// unit, time), the region maps' count, each unit's id, tile count and tiles (x, y, count), the two timers.
    pub fn save_words(&self) -> Vec<u32> {
        let mut w = vec![self.history.len() as u32];
        for e in &self.history {
            w.extend([e.x as u32, e.y as u32, e.unit as u32, e.time]);
        }
        w.push(self.regions.len() as u32);
        for (&id, map) in &self.regions {
            w.extend([id as u32, map.len() as u32]);
            for (k, &v) in map {
                w.extend([k.x() as u32, k.y() as u32, v]);
            }
        }
        w.extend([self.next_history_prune as u32, self.next_region_prune as u32]);
        w
    }

    /// `LoadRunningState`'s first part from the words `read` gives (`None`: a failed read).
    pub fn load_words(mut read: impl FnMut() -> Option<u32>) -> Option<PathRecord> {
        let mut r = PathRecord::default();
        let count = read()?;
        for _ in 0..count {
            let (x, y, unit, time) = (read()? as i32, read()? as i32, read()? as i32, read()?);
            r.history.push_back(HistoryEntry { x, y, unit, time });
        }
        let regions = read()?;
        for _ in 0..regions {
            let id = read()? as i32;
            let tiles = read()?;
            let mut map = BTreeMap::new();
            for _ in 0..tiles {
                let (x, y, v) = (read()? as i32, read()? as i32, read()?);
                map.entry(TileKey::new(x, y)).or_insert(v);
            }
            r.regions.entry(id).or_insert(map);
        }
        r.next_history_prune = read()? as i32;
        r.next_region_prune = read()? as i32;
        Some(r)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn e(unit: i32, x: i32, y: i32, time: u32) -> HistoryEntry {
        HistoryEntry { x, y, unit, time }
    }

    #[test]
    fn the_tile_order_is_u2dpoints() {
        // lower y first, then the larger x
        let mut keys = [TileKey::new(1, 5), TileKey::new(3, 5), TileKey::new(9, 2), TileKey::new(0, 7)];
        keys.sort();
        let got: Vec<(i32, i32)> = keys.iter().map(|k| (k.x(), k.y())).collect();
        assert_eq!(got, vec![(9, 2), (3, 5), (1, 5), (0, 7)]);
    }

    #[test]
    fn the_history_is_newest_first_and_the_walk_stops_at_the_units_newest() {
        let mut r = PathRecord::default();
        r.add_history(7, 1, 1, 100);
        r.add_history(8, 2, 2, 200);
        r.add_history(7, 3, 3, 300);
        assert_eq!(r.history[0], e(7, 3, 3, 300));
        assert!(r.repeated(7, 3, 3, 0));
        assert!(!r.repeated(7, 1, 1, 0)); // the unit's newest was another tile
        assert!(r.repeated(8, 2, 2, 150));
        assert!(!r.repeated(8, 2, 2, 250)); // older than the window
        assert!(!r.repeated(9, 2, 2, 0));
    }

    #[test]
    fn prune_history_counts_repeats_into_the_region_maps() {
        let mut r = PathRecord::default();
        // oldest first: one stale request, then three for (4, 4) and one for (5, 5) by unit 1, two by unit 2
        r.add_history(1, 4, 4, 1000);
        for t in [7000, 7100, 7200] {
            r.add_history(1, 4, 4, t);
        }
        r.add_history(1, 5, 5, 7300);
        r.add_history(2, 6, 6, 7400);
        r.add_history(2, 6, 6, 7500);
        r.prune_history(10000, false); // the window starts after 4500
        assert!(r.history.is_empty());
        assert_eq!(r.regions[&1][&TileKey::new(4, 4)], 3);
        assert!(!r.regions[&1].contains_key(&TileKey::new(5, 5)));
        assert_eq!(r.regions[&2][&TileKey::new(6, 6)], 2);
        // an existing entry counts up by one, whatever the new count
        for t in [17000, 17100, 17200] {
            r.add_history(1, 4, 4, t);
        }
        r.prune_history(18000, false);
        assert_eq!(r.regions[&1][&TileKey::new(4, 4)], 4);
        // while loading: the history goes, nothing is counted
        r.add_history(2, 6, 6, 19000);
        r.add_history(2, 6, 6, 19001);
        r.prune_history(19500, true);
        assert!(r.history.is_empty());
        assert_eq!(r.regions[&2][&TileKey::new(6, 6)], 2);
    }

    #[test]
    fn prune_history_with_a_small_time_has_no_window() {
        let mut r = PathRecord::default();
        r.add_history(1, 4, 4, 10);
        r.add_history(1, 4, 4, 20);
        r.prune_history(5000, false); // under 5.5 s: entries at or before 5000 are stale
        assert!(r.regions.is_empty());
    }

    #[test]
    fn prune_regions_names_the_stranded_units_last_found_first_and_frees_the_maps() {
        let mut r = PathRecord::default();
        r.regions.insert(3, BTreeMap::from([(TileKey::new(1, 1), 8)]));
        r.regions.insert(-2, BTreeMap::from([(TileKey::new(1, 1), 9), (TileKey::new(2, 1), 1)]));
        r.regions.insert(5, BTreeMap::from([(TileKey::new(1, 1), 7)]));
        r.regions.insert(9, BTreeMap::from([(TileKey::new(1, 1), 1), (TileKey::new(0, 3), 12)]));
        assert!(r.prune_regions(true).is_empty());
        assert_eq!(r.regions.len(), 4);
        assert_eq!(r.prune_regions(false), vec![9, 3, -2]);
        assert!(r.regions.is_empty());
    }

    #[test]
    fn the_schedule_prunes_only_what_holds_something() {
        let mut r = PathRecord::default();
        assert_eq!(r.due(500), (false, false));
        assert_eq!((r.next_history_prune, r.next_region_prune), (10500, 30500));
        r.add_history(1, 1, 1, 600);
        assert_eq!(r.due(10499), (false, false));
        assert_eq!(r.due(10500), (true, false));
        r.history.clear(); // (PruneHistory's work)
        r.regions.insert(1, BTreeMap::new());
        assert_eq!(r.due(30500), (false, true));
        assert_eq!((r.next_history_prune, r.next_region_prune), (40500, 60500));
        // a negative timer compares as unsigned: never due
        r.next_history_prune = -1;
        assert!(!r.due(70000).0);
    }

    #[test]
    fn the_save_words_round_trip_in_the_cpp_layout() {
        let mut r = PathRecord::default();
        r.add_history(7, 1, 2, 300);
        r.add_history(8, -1, 4, 400);
        r.regions.insert(7, BTreeMap::from([(TileKey::new(1, 2), 3), (TileKey::new(5, 2), 2)]));
        r.next_history_prune = 10400;
        r.next_region_prune = 30400;
        let w = r.save_words();
        assert_eq!(w, vec![2, 0xffff_ffff, 4, 8, 400, 1, 2, 7, 300, 1, 7, 2, 5, 2, 2, 1, 2, 3, 10400, 30400]);
        let mut it = w.iter().copied();
        assert_eq!(PathRecord::load_words(|| it.next()), Some(r.clone()));
        // a short file reads nothing
        let mut short = w[..w.len() - 1].iter().copied();
        assert_eq!(PathRecord::load_words(|| short.next()), None);
    }
}
