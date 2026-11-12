//! A scenario area's queries (`EESMArea::ContainsTile`, `ContainsUnit`, `GetCenterTile`): a rectangle, the anchor's
//! continent or the towns whose hull holds the anchor, over the host's reads of the continents, towns and the unit.

use crate::ftol;
use crate::host::*;

/// The anchor's "no area" coordinate.
const EE_SCENARIO_K_NO_AREA: i32 = -10000;

/// An area's settings and its cached centre (`EESMArea`'s plain members).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeScenarioAreaRecord {
    pub select_mode: u32,
    pub anchor_x: i32,
    pub anchor_y: i32,
    pub left: f32,
    pub top: f32,
    pub right: f32,
    pub bottom: f32,
    /// `mCenterTile` (in and out: the cache, -10000 while unset)
    pub center_x: i32,
    pub center_y: i32,
}

/// Two `i32`s of a packed read (x in the low half).
pub fn unpack(value: i64) -> (i32, i32) {
    (value as i32, (value >> 32) as i32)
}

impl EeScenarioAreaRecord {
    /// `IsUnset`: an anchor coordinate is the no-area value.
    pub fn is_unset(&self) -> bool {
        self.anchor_x == EE_SCENARIO_K_NO_AREA || self.anchor_y == EE_SCENARIO_K_NO_AREA
    }

    /// `IsOffMap`: a negative anchor coordinate (compared as floats, as the original).
    pub fn is_off_map(&self) -> bool {
        (self.anchor_x as f32) < 0.0 || (self.anchor_y as f32) < 0.0
    }
}

/// The towns whose hull holds the anchor, from the last player down to player 1 and each player's last town down.
fn anchor_towns<H: Host>(host: &mut H, a: &EeScenarioAreaRecord) -> Vec<(i32, i32)> {
    let mut towns = Vec::new();
    let players = host.read(EE_SCENARIO_READ_NUM_PLAYERS, 0, 0) as i32;
    let mut i = players - 1;
    while i > 0 {
        let mut j = host.read(EE_SCENARIO_READ_TOWN_COUNT, i, 0) as i32 - 1;
        while j >= 0 {
            if host.read4(EE_SCENARIO_READ_TOWN_CONTAINS, i, j, a.anchor_x, a.anchor_y) != 0 {
                towns.push((i, j));
            }
            j -= 1;
        }
        i -= 1;
    }
    towns
}

/// `EESMArea::ContainsTile`.
pub fn contains_tile<H: Host>(host: &mut H, a: &EeScenarioAreaRecord, x: i32, y: i32) -> bool {
    match a.select_mode {
        0 => {
            let (px, py) = (x as f32, y as f32);
            px >= a.left && px <= a.right && py >= a.top && py <= a.bottom
        }
        1 => {
            if x >= 0 && y >= 0 {
                let anchor = host.read(EE_SCENARIO_READ_CONTINENT, a.anchor_x, a.anchor_y);
                host.read(EE_SCENARIO_READ_CONTINENT, x, y) == anchor
            } else {
                false
            }
        }
        2 => {
            let towns = anchor_towns(host, a);
            let mut result = false;
            for &(p, t) in towns.iter().rev() {
                if result {
                    break;
                }
                result = host.read4(EE_SCENARIO_READ_TOWN_CONTAINS, p, t, x, y) != 0;
            }
            result
        }
        _ => false,
    }
}

/// `EESMArea::ContainsUnit` of the host's unit (the C++ answers false for no unit itself).
pub fn contains_unit<H: Host>(host: &mut H, a: &EeScenarioAreaRecord) -> bool {
    let (x, y) = unpack(host.read(EE_SCENARIO_READ_UNIT_TILE, 0, 0));
    if a.is_unset() {
        x == EE_SCENARIO_K_NO_AREA
    } else if a.is_off_map() {
        host.read(EE_SCENARIO_READ_UNIT_HAS_COMPLEX, 0, 0) == 0
            || unpack(host.read(EE_SCENARIO_READ_UNIT_PREVIOUS_TILE, 0, 0)) == (x, y)
    } else {
        contains_tile(host, a, x, y)
    }
}

/// `EESMArea::GetCenterTile`: the cached centre, else the rectangle's centre, the continent's or the hull centre of
/// the first town holding the anchor (else the anchor), cached into `a`.
pub fn center_tile<H: Host>(host: &mut H, a: &mut EeScenarioAreaRecord) -> (i32, i32) {
    if a.center_x != EE_SCENARIO_K_NO_AREA {
        return (a.center_x, a.center_y);
    }
    if a.select_mode == 0 {
        let cx = (a.right - a.left) * 0.5 + a.left;
        let cy = (a.bottom - a.top) * 0.5 + a.top;
        a.center_x = ftol(cx);
        a.center_y = ftol(cy);
    } else if a.select_mode == 1 {
        let continent = host.read(EE_SCENARIO_READ_CONTINENT, a.anchor_x, a.anchor_y) as i32;
        (a.center_x, a.center_y) = unpack(host.read(EE_SCENARIO_READ_CONTINENT_CENTER, continent, 0));
    } else {
        let mut found = false;
        let players = host.read(EE_SCENARIO_READ_NUM_PLAYERS, 0, 0) as i32;
        let mut i = players - 1;
        while i > 0 && !found {
            let mut j = host.read(EE_SCENARIO_READ_TOWN_COUNT, i, 0) as i32 - 1;
            while j >= 0 && !found {
                if host.read4(EE_SCENARIO_READ_TOWN_CONTAINS, i, j, a.anchor_x, a.anchor_y) != 0 {
                    (a.center_x, a.center_y) = unpack(host.read(EE_SCENARIO_READ_TOWN_CENTER, i, j));
                    found = true;
                }
                j -= 1;
            }
            i -= 1;
        }
        if !found {
            (a.center_x, a.center_y) = (a.anchor_x, a.anchor_y);
        }
    }
    (a.center_x, a.center_y)
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::collections::BTreeMap;

    /// Towns as hulls of tile rectangles per player, continents by x / 10, one unit.
    #[derive(Default)]
    struct World {
        players: i32,
        towns: BTreeMap<i32, Vec<(i32, i32, i32, i32)>>,
        unit: Option<(i32, i32)>,
        previous: Option<(i32, i32)>,
        log: Vec<(u32, i32, i32, i32, i32)>,
    }

    fn pack(x: i32, y: i32) -> i64 {
        (x as u32 as i64) | ((y as i64) << 32)
    }

    impl Host for World {
        fn read4(&mut self, op: u32, a: i32, b: i32, c: i32, d: i32) -> i64 {
            self.log.push((op, a, b, c, d));
            match op {
                EE_SCENARIO_READ_NUM_PLAYERS => self.players as i64,
                EE_SCENARIO_READ_CONTINENT => (a / 10) as i64,
                EE_SCENARIO_READ_CONTINENT_CENTER => pack(a * 10 + 5, 7),
                EE_SCENARIO_READ_TOWN_COUNT => self.towns.get(&a).map_or(0, |t| t.len()) as i64,
                EE_SCENARIO_READ_TOWN_CONTAINS => {
                    let (l, t, r, bo) = self.towns[&a][b as usize];
                    (c >= l && c <= r && d >= t && d <= bo) as i64
                }
                EE_SCENARIO_READ_TOWN_CENTER => {
                    let (l, t, r, bo) = self.towns[&a][b as usize];
                    pack((l + r) / 2, (t + bo) / 2)
                }
                EE_SCENARIO_READ_UNIT_TILE => self.unit.map(|(x, y)| pack(x, y)).unwrap(),
                EE_SCENARIO_READ_UNIT_HAS_COMPLEX => self.previous.is_some() as i64,
                EE_SCENARIO_READ_UNIT_PREVIOUS_TILE => self.previous.map(|(x, y)| pack(x, y)).unwrap(),
                _ => panic!("unexpected read {op}"),
            }
        }
    }

    fn rect(l: f32, t: f32, r: f32, b: f32) -> EeScenarioAreaRecord {
        EeScenarioAreaRecord {
            left: l,
            top: t,
            right: r,
            bottom: b,
            center_x: -10000,
            center_y: -10000,
            ..Default::default()
        }
    }

    #[test]
    fn rectangles_hold_their_edges_and_centre_on_the_cast() {
        let mut w = World::default();
        let mut a = rect(2.0, 3.0, 7.0, 8.5);
        assert!(contains_tile(&mut w, &a, 2, 3) && contains_tile(&mut w, &a, 7, 8));
        assert!(!contains_tile(&mut w, &a, 7, 9) && !contains_tile(&mut w, &a, 1, 5));
        assert_eq!(center_tile(&mut w, &mut a), (4, 5));
        // cached: a moved rectangle keeps the centre until the cache is cleared
        a.left = 100.0;
        assert_eq!(center_tile(&mut w, &mut a), (4, 5));
        let mut n = rect(-7.0, -7.0, -2.0, -2.0);
        assert_eq!(center_tile(&mut w, &mut n), (-4, -4));
        assert!(w.log.is_empty());
    }

    #[test]
    fn continents_compare_the_anchor_first() {
        let mut w = World::default();
        let mut a =
            EeScenarioAreaRecord { select_mode: 1, anchor_x: 23, anchor_y: 4, center_x: -10000, ..Default::default() };
        assert!(contains_tile(&mut w, &a, 29, 0));
        assert!(!contains_tile(&mut w, &a, 30, 0));
        assert!(!contains_tile(&mut w, &a, -1, 0));
        assert_eq!(w.log[0], (EE_SCENARIO_READ_CONTINENT, 23, 4, 0, 0));
        assert_eq!(center_tile(&mut w, &mut a), (25, 7));
    }

    #[test]
    fn towns_holding_the_anchor_take_the_tile_last_first() {
        let mut w = World { players: 4, ..World::default() };
        w.towns.insert(1, vec![(0, 0, 10, 10), (50, 50, 60, 60)]);
        w.towns.insert(3, vec![(5, 5, 20, 20)]);
        let mut a =
            EeScenarioAreaRecord { select_mode: 2, anchor_x: 6, anchor_y: 6, center_x: -10000, ..Default::default() };
        assert!(contains_tile(&mut w, &a, 15, 15));
        assert!(contains_tile(&mut w, &a, 1, 1));
        assert!(!contains_tile(&mut w, &a, 55, 55));
        // the first holder is player 3's town
        assert_eq!(center_tile(&mut w, &mut a), (12, 12));
        let mut b =
            EeScenarioAreaRecord { select_mode: 2, anchor_x: 99, anchor_y: 99, center_x: -10000, ..Default::default() };
        assert_eq!(center_tile(&mut w, &mut b), (99, 99));
        assert!(!contains_tile(&mut w, &b, 99, 99));
        // another mode holds nothing
        let c = EeScenarioAreaRecord { select_mode: 3, ..Default::default() };
        assert!(!contains_tile(&mut w, &c, 0, 0));
    }

    #[test]
    fn units_unset_off_map_or_by_tile() {
        let mut w = World::default();
        let mut a = rect(0.0, 0.0, 5.0, 5.0);
        w.unit = Some((3, 3));
        assert!(contains_unit(&mut w, &a));
        a.anchor_x = -10000;
        assert!(!contains_unit(&mut w, &a));
        w.unit = Some((-10000, 4));
        assert!(contains_unit(&mut w, &a));
        a.anchor_x = -1;
        a.anchor_y = -1;
        // off the map: no complex unit, or the unit is where it was (also at the packing's -1, -1)
        assert!(contains_unit(&mut w, &a));
        w.unit = Some((-1, -1));
        w.previous = Some((-1, -1));
        assert!(contains_unit(&mut w, &a));
        w.unit = Some((-10000, 4));
        w.previous = Some((-10000, 4));
        assert!(contains_unit(&mut w, &a));
        w.previous = Some((1, 4));
        assert!(!contains_unit(&mut w, &a));
    }
}
