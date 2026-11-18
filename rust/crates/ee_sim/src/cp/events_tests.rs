//! The computer player's own bodies over the placement tests' fake world.

use super::events::*;
use super::fields::EeCpFields;
use super::place_host::{PlaceHost, Tile};
use super::placement_tests::Fake;

impl PlayerHost for Fake {
    fn fields(&mut self) -> *mut EeCpFields {
        &mut *self.rec
    }
    fn behavior_float(&mut self, _i: i32) -> f32 {
        1.0
    }
    fn base_population_limit(&mut self) -> i32 {
        100
    }
    fn is_deathmatch(&mut self) -> bool {
        false
    }
    fn cheat_on(&mut self, cheat: i32) -> bool {
        self.cheats & cheat != 0
    }
    fn num_teams(&mut self) -> u32 {
        self.wonder_teams.len() as u32
    }
    fn team_has_wonders(&mut self, team: u32) -> bool {
        self.wonder_teams[team as usize]
    }
    fn team_contains(&mut self, _team: u32, _player: u32) -> bool {
        false
    }
    fn num_players(&mut self) -> u32 {
        self.players.len() as u32
    }
    fn num_world_players(&mut self) -> u32 {
        self.players.len() as u32
    }
    fn player_human(&mut self, i: u32) -> Option<bool> {
        self.players.get(i as usize).map(|p| p.0)
    }
    fn player_population(&mut self, i: u32) -> i32 {
        self.players[i as usize].1
    }
    fn player_handle_at(&mut self, i: u32) -> u64 {
        0x500 + i as u64
    }
    fn path_area_id(&mut self, t: Tile) -> u16 {
        if t[0] < 10 {
            1
        } else {
            2
        }
    }
    fn unit_continent_at(&mut self, unit: u64) -> u64 {
        self.unit_continent(unit) as u64
    }
    fn center_create(&mut self, _unit: u64) -> u64 {
        0
    }
    fn center_list(&mut self, _center: u64, _front: bool) {}
    fn center_is_within(&mut self, _center: u64, _unit: u64) -> bool {
        false
    }
    fn center_add(&mut self, _center: u64, _unit: u64) {}
    fn center_contains(&mut self, _center: u64, _unit: u64) -> bool {
        false
    }
    fn center_adopt(&mut self, _center: u64, _unit: u64) {}
    fn center_set_main(&mut self, _center: u64, _unit: u64) {}
    fn center_get_center(&mut self, _center: u64, _tile: &mut Tile) {}
    fn center_set_active(&mut self, _center: u64, _active: bool) {}
    fn centers_sort(&mut self, _from: Tile) {}
    fn favorite_candidate(&mut self, class: u32, _kind: i32, _epoch: i32) -> Option<(i32, i32)> {
        self.candidates.get(class as usize).copied().flatten()
    }
    fn history_push(&mut self, unit_type: u32, epoch: i32) {
        self.history.push((unit_type, epoch));
    }
    fn is_epoch(&mut self, id: i32) -> bool {
        (0..15).contains(&id)
    }
    fn unit_is_building(&mut self, unit: u64) -> bool {
        self.unit(unit).land_building != 0
    }
    fn unit_research(&mut self, unit: u64) -> Vec<i32> {
        vec![if self.unit(unit).technology == 0 { CP_TECH_NONE } else { self.unit(unit).technology }, 2000, 7]
    }
    fn unit_obsolete(&mut self, _unit: u64) -> bool {
        true
    }
    fn seen_by(&mut self, _unit: u64, _player: u64) -> bool {
        false
    }
    fn kill(&mut self, unit: u64) {
        self.units[unit as usize - 1].hit_points = 0;
    }
    fn is_transport(&mut self, unit: u64) -> bool {
        self.unit(unit).unit_type == 0x99
    }
    fn cargo(&mut self, unit: u64) -> Vec<u64> {
        (1..=self.units.len() as u64).filter(|&u| u != unit && self.units[u as usize - 1].wall != 0).collect()
    }
}

#[test]
fn population_cap() {
    let mut h = Fake::default();
    h.rec.mPopLimitAdjustmentEasy = 100;
    h.players = vec![(false, 0), (true, 60), (false, 500)];
    // without the band cheat no pull toward the human population
    work_out_population(&mut h);
    assert_eq!(h.rec.mPopulationCap, 100);
    h.cheats = CP_CHEAT_POP_BAND;
    work_out_population(&mut h);
    assert_eq!(h.rec.mPopulationCap, 80);
    // the difficulty's scale, capped by the own limit
    h.rec.mPopLimitAdjustmentEasy = 50;
    work_out_population(&mut h);
    assert_eq!(h.rec.mPopulationCap, 40);
    h.rec.mPopLimitAdjustmentEasy = 200;
    work_out_population(&mut h);
    assert_eq!(h.rec.mPopulationCap, 100);
    // another team winning by wonders: the own limit
    h.rec.mPopLimitAdjustmentEasy = 50;
    h.wonder_teams = vec![false, true];
    work_out_population(&mut h);
    assert_eq!(h.rec.mPopulationCap, 100);
    // ignoring the others: unchanged
    h.rec.mIgnoreOthers = true;
    h.rec.mPopulationCap = 7;
    work_out_population(&mut h);
    assert_eq!(h.rec.mPopulationCap, 7);
}

#[test]
fn object_region_counts_the_ring() {
    let mut h = Fake::default();
    // the ring round (9, 5): x 8, 9 in area 1 (6 tiles), x 10 in area 2 (3 tiles)
    let u = h.add(0x61, [9, 5], false);
    assert_eq!(object_region(&mut h, u), 1);
    let u = h.add(0x61, [10, 5], false);
    assert_eq!(object_region(&mut h, u), 2);
}

#[test]
fn civilizations_by_epoch() {
    assert_eq!(civilizations(0, false, false), vec![0, 1, 2, 3, 4, 5]);
    assert_eq!(civilizations(0, true, false), vec![0, 3, 4, 5]);
    assert_eq!(civilizations(8, false, false), vec![6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17]);
    assert_eq!(civilizations(13, false, true), vec![12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22]);
    assert!(civilizations(15, false, false).is_empty());
    let mut h = Fake { random_value: 1, ..Default::default() };
    assert_eq!(choose_civilization(&mut h, 0, false, false), Some(1));
    assert_eq!(h.draws, vec![5]);
    assert_eq!(choose_civilization(&mut h, CP_TECH_NONE, false, false), None);
    assert_eq!(CP_CIVILIZATIONS[21], "Japan\0");
}

#[test]
fn favorite_by_line() {
    let mut h = Fake { epoch: 3, ..Default::default() };
    h.candidates = vec![None, Some((5, 40)), Some((2, 41)), Some((5, 42)), Some((0, 43))];
    h.rec.mFavoriteLandType = 9;
    // the lines by AI kind: {2: [41], 5: [40, 42]}; kind 0 never makes a line
    h.random_value = 1;
    choose_favorite(&mut h, EE_CP_FAVORITE_LAND);
    assert_eq!(h.rec.mFavoriteLandType, 42);
    assert_eq!(h.draws, vec![1, 1]);
    assert_eq!(h.history, vec![(42, 3)]);
    // none passes: the type as it was, still a history entry
    h.candidates.clear();
    choose_favorite(&mut h, EE_CP_FAVORITE_SEA);
    assert_eq!(h.rec.mFavoriteSeaType, 0);
    assert_eq!(h.history, vec![(42, 3), (0, 3)]);
}

#[test]
fn avoids_enemies_and_pending_epoch() {
    let mut h = Fake::default();
    let citizen = h.add(0x20, [1, 1], true);
    h.units[citizen as usize - 1].category = CP_CATEGORY_LAND_CITIZEN;
    h.units[citizen as usize - 1].wall = 1;
    let boat = h.add(0x99, [2, 2], true);
    assert!(avoids_enemies(&mut h, citizen));
    assert!(avoids_enemies(&mut h, boat));
    let soldier = h.add(0x30, [3, 3], true);
    h.units[soldier as usize - 1].wall = 1;
    assert!(!avoids_enemies(&mut h, boat) && !avoids_enemies(&mut h, soldier));
    // the queue's epoch at a living building
    assert_eq!(pending_epoch(&mut h), CP_TECH_NONE);
    let barracks = h.add(0x22, [5, 5], true);
    h.units[barracks as usize - 1].land_building = 1;
    assert_eq!(pending_epoch(&mut h), 7);
    h.units[barracks as usize - 1].technology = 4;
    assert_eq!(pending_epoch(&mut h), 4);
}

#[test]
fn retire_kills_the_unseen_obsolete() {
    let mut h = Fake { epoch: CP_EPOCH_WW1, ..Default::default() };
    let knight = h.add(0x8b, [1, 1], true);
    let other = h.add(0x30, [2, 2], true);
    h.players = vec![(false, 0), (true, 10)];
    retire_obsolete_units(&mut h);
    assert_eq!(h.units[knight as usize - 1].hit_points, 10);
    h.cheats = CP_CHEAT_RETIRE;
    retire_obsolete_units(&mut h);
    assert_eq!(h.units[knight as usize - 1].hit_points, 0);
    assert_eq!(h.units[other as usize - 1].hit_points, 10);
}
