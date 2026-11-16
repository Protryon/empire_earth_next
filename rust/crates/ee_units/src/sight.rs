//! `EEComplexUnit::SightOn` and `SightOff` (R5b.4, the thirty-fifth round): the unit's line of sight added to or
//! removed from a tile for its player, its sight sharers (a land building's) and, for a unit of a kind-4 tech node,
//! every player; the mini map's cells and the terrain mesh's tiles counted for the viewing player. The two C++ bodies
//! were one statement list with Add and Remove swapped, except that SightOff does not test the tiles it removes from
//! for NULL (kept: the host's remove takes the tile as it is). Every world call is one host call ([`SightHost`];
//! `ee_units_sight_host`, EEComplexUnit.cpp).

/// A unit as the body reads it.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeUnitsSightUnit {
    /// `GetLineOfSight()`
    pub line_of_sight: f32,
    /// `mPlayer`
    pub player: u64,
    /// the player's `mSaveVisMap`
    pub save_vis_map: u8,
    pub hit_points: i32,
    pub is_built: u8,
    /// the class's `IsLandBuilding()`; the unit's `IsFoundation()` (asked only of a land building)
    pub land_building: u8,
    pub tile_size: i32,
    /// the class's `mWonderBonus`
    pub wonder_bonus: i32,
    /// `GetTechTreeNode()->mData->mKind` (-1: no node)
    pub node_kind: i32,
}

/// The world the body acts on (players and tiles are their addresses).
pub trait SightHost {
    fn unit(&mut self, u: u64) -> EeUnitsSightUnit;
    /// `gSightUpdatesEnabled`
    fn updates_enabled(&mut self) -> bool;
    /// `IsFoundation()`
    fn is_foundation(&mut self, u: u64) -> bool;
    /// `AddCenterOffset(x, y)`
    fn center_offset(&mut self, u: u64, x: f32, y: f32) -> (f32, f32);
    /// `gWorld.GetCurrentPlayer()`
    fn current_player(&mut self) -> u64;
    /// the player's `mSharedLOS`
    fn shared_los(&mut self, player: u64) -> bool;
    /// `a->IsAlliedWith(b)`
    fn allied(&mut self, a: u64, b: u64) -> bool;
    /// the player's `mSightSharers`
    fn sharers(&mut self, player: u64) -> Vec<u64>;
    /// `EEAddSightCircle` / `EERemoveSightCircle(x, y, radius, player)`
    fn circle(&mut self, on: bool, x: f32, y: f32, radius: f32, player: u64);
    /// `gWorld.mMap->GetTile(x, y)`
    fn tile(&mut self, x: i32, y: i32) -> u64;
    /// the tile's `AddSight(player)` / `RemoveSight(player)`
    fn tile_sight(&mut self, on: bool, tile: u64, player: u64);
    /// the player's `AddSharedSight(tile, x, y)` / `RemoveSharedSight(tile)`
    fn shared_sight(&mut self, on: bool, player: u64, tile: u64, x: i32, y: i32);
    /// `gMiniMap->CountCellOn/Off(x, y)`, then the terrain mesh's `ReferenceTile/DereferenceTile(x, y)`
    fn show(&mut self, on: bool, x: i32, y: i32);
    /// `gWorld.mMap->AddSight/RemoveSight(player, x, y, the wonder sight radius, false)`
    fn wonder_sight(&mut self, on: bool, player: u64, x: f32, y: f32);
    /// `gWorld.mNumPlayers`; `gWorld.mPlayers[i]`
    fn num_players(&mut self) -> u32;
    fn player_at(&mut self, i: u32) -> u64;
}

/// The tiles of a unit `size` tiles wide from (x, y): `for (long x = inX; x < (long)(inX + size); x++)`, y inside.
fn footprint(x: u32, y: u32, size: i32) -> impl Iterator<Item = (i32, i32)> {
    let end_x = x.wrapping_add(size as u32) as i32;
    let end_y = y.wrapping_add(size as u32) as i32;
    (x as i32..end_x).flat_map(move |tx| (y as i32..end_y).map(move |ty| (tx, ty)))
}

/// One tile's sight for `player` by `apply` (the tile sight or the shared sight); SightOn skips a missing tile and
/// shows only a found one, SightOff neither tests nor skips.
fn at<H: SightHost + ?Sized>(
    h: &mut H,
    on: bool,
    x: i32,
    y: i32,
    visible: bool,
    apply: &mut impl FnMut(&mut H, u64, i32, i32),
) {
    let tile = h.tile(x, y);
    if on && tile == 0 {
        return;
    }
    apply(h, tile, x, y);
    if visible {
        h.show(on, x, y);
    }
}

/// `SightOn(x, y)` (`on`) and `SightOff(x, y)`.
#[allow(clippy::neg_cmp_op_on_partial_ord)] // the C++ test (NaN: no sight)
pub fn sight<H: SightHost + ?Sized>(h: &mut H, u: u64, x: u32, y: u32, on: bool) {
    let s = h.unit(u);
    let player = s.player;
    if !(s.line_of_sight > 0.0) || player == 0 || s.save_vis_map == 0 || x == -10000i32 as u32 || !h.updates_enabled() {
        return;
    }
    let (fx, fy) = h.center_offset(u, x as i32 as f32, y as i32 as f32);
    if s.land_building != 0 && h.is_foundation(u) {
        return;
    }
    let size = s.tile_size;
    if s.hit_points > 0 && s.is_built != 0 {
        h.circle(on, fx, fy, s.line_of_sight, player);
        if s.land_building != 0 {
            for sharer in h.sharers(player) {
                let current = h.current_player();
                let visible = current == sharer && h.shared_los(current) && h.shared_los(sharer);
                let mut apply = |h: &mut H, tile: u64, _: i32, _: i32| h.tile_sight(on, tile, sharer);
                if size <= 1 {
                    at(h, on, x as i32, y as i32, visible, &mut apply);
                } else {
                    for (tx, ty) in footprint(x, y, size) {
                        at(h, on, tx, ty, visible, &mut apply);
                    }
                }
            }
            if s.wonder_bonus == 3 {
                h.wonder_sight(on, player, fx, fy);
            }
        }
    } else {
        let current = h.current_player();
        let visible = current == player || (h.allied(current, player) && h.shared_los(current) && h.shared_los(player));
        let mut apply = |h: &mut H, tile: u64, tx: i32, ty: i32| h.shared_sight(on, player, tile, tx, ty);
        if size <= 1 {
            at(h, on, x as i32, y as i32, visible, &mut apply);
        } else {
            for (tx, ty) in footprint(x, y, size) {
                at(h, on, tx, ty, visible, &mut apply);
            }
        }
    }
    if s.node_kind != 4 {
        return;
    }
    for i in 1..h.num_players() {
        let other = h.player_at(i);
        if other == 0 {
            continue;
        }
        let visible = other == h.current_player();
        let mut apply = |h: &mut H, tile: u64, tx: i32, ty: i32| h.shared_sight(on, other, tile, tx, ty);
        for (tx, ty) in footprint(x, y, size) {
            at(h, on, tx, ty, visible, &mut apply);
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[derive(Default)]
    struct Mock {
        unit: EeUnitsSightUnit,
        foundation: bool,
        current: u64,
        shared: bool,
        allied: bool,
        sharers: Vec<u64>,
        players: Vec<u64>,
        missing: Option<(i32, i32)>,
        log: Vec<String>,
    }

    impl SightHost for Mock {
        fn unit(&mut self, _u: u64) -> EeUnitsSightUnit {
            self.unit
        }
        fn updates_enabled(&mut self) -> bool {
            true
        }
        fn is_foundation(&mut self, _u: u64) -> bool {
            self.foundation
        }
        fn center_offset(&mut self, _u: u64, x: f32, y: f32) -> (f32, f32) {
            (x + 0.5, y + 0.5)
        }
        fn current_player(&mut self) -> u64 {
            self.current
        }
        fn shared_los(&mut self, _p: u64) -> bool {
            self.shared
        }
        fn allied(&mut self, _a: u64, _b: u64) -> bool {
            self.allied
        }
        fn sharers(&mut self, _p: u64) -> Vec<u64> {
            self.sharers.clone()
        }
        fn circle(&mut self, on: bool, x: f32, y: f32, radius: f32, player: u64) {
            self.log.push(format!("circle {on} {x} {y} {radius} {player}"));
        }
        fn tile(&mut self, x: i32, y: i32) -> u64 {
            if self.missing == Some((x, y)) {
                0
            } else {
                (x * 100 + y) as u64 + 1
            }
        }
        fn tile_sight(&mut self, on: bool, tile: u64, player: u64) {
            self.log.push(format!("tile {on} {tile} {player}"));
        }
        fn shared_sight(&mut self, on: bool, player: u64, tile: u64, x: i32, y: i32) {
            self.log.push(format!("shared {on} {player} {tile} {x} {y}"));
        }
        fn show(&mut self, on: bool, x: i32, y: i32) {
            self.log.push(format!("show {on} {x} {y}"));
        }
        fn wonder_sight(&mut self, on: bool, player: u64, x: f32, y: f32) {
            self.log.push(format!("wonder {on} {player} {x} {y}"));
        }
        fn num_players(&mut self) -> u32 {
            self.players.len() as u32
        }
        fn player_at(&mut self, i: u32) -> u64 {
            self.players[i as usize]
        }
    }

    fn mobile() -> Mock {
        Mock {
            unit: EeUnitsSightUnit {
                line_of_sight: 6.0,
                player: 7,
                save_vis_map: 1,
                hit_points: 5,
                is_built: 1,
                tile_size: 1,
                node_kind: -1,
                ..Default::default()
            },
            current: 7,
            ..Default::default()
        }
    }

    #[test]
    fn a_built_unit_adds_its_circle_only() {
        let mut m = mobile();
        sight(&mut m, 1, 3, 4, true);
        assert_eq!(m.log, vec!["circle true 3.5 4.5 6 7"]);
        m.log.clear();
        m.unit.line_of_sight = 0.0;
        sight(&mut m, 1, 3, 4, true);
        assert!(m.log.is_empty());
    }

    #[test]
    fn an_unbuilt_unit_shares_its_tiles_and_shows_them_to_its_player() {
        let mut m = mobile();
        m.unit.is_built = 0;
        m.unit.tile_size = 2;
        m.missing = Some((3, 5));
        sight(&mut m, 1, 3, 4, true);
        assert_eq!(
            m.log,
            vec![
                "shared true 7 305 3 4",
                "show true 3 4",
                "shared true 7 405 4 4",
                "show true 4 4",
                "shared true 7 406 4 5",
                "show true 4 5"
            ]
        );
        // off: the missing tile is not skipped
        m.log.clear();
        m.current = 9;
        sight(&mut m, 1, 3, 4, false);
        assert_eq!(
            m.log,
            vec!["shared false 7 305 3 4", "shared false 7 0 3 5", "shared false 7 405 4 4", "shared false 7 406 4 5"]
        );
    }

    #[test]
    fn a_land_building_lends_its_sharers_and_a_wonder_sees_far() {
        let mut m = mobile();
        m.unit.land_building = 1;
        m.unit.wonder_bonus = 3;
        m.sharers = vec![7, 8];
        m.shared = true;
        sight(&mut m, 1, 2, 2, false);
        assert_eq!(
            m.log,
            vec![
                "circle false 2.5 2.5 6 7",
                "tile false 203 7",
                "show false 2 2",
                "tile false 203 8",
                "wonder false 7 2.5 2.5"
            ]
        );
        m.foundation = true;
        m.log.clear();
        sight(&mut m, 1, 2, 2, true);
        assert!(m.log.is_empty());
    }

    #[test]
    fn a_kind_4_unit_shares_with_every_player() {
        let mut m = mobile();
        m.unit.node_kind = 4;
        m.players = vec![0, 7, 0, 9];
        sight(&mut m, 1, 1, 1, true);
        assert_eq!(
            m.log,
            vec!["circle true 1.5 1.5 6 7", "shared true 7 102 1 1", "show true 1 1", "shared true 9 102 1 1"]
        );
    }
}
