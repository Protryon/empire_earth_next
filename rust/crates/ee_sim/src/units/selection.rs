//! A player's selection (the groups lane, round 1): `EEPlayer`'s selection members (EEPlayer.cpp: UpdateSelection,
//! TrySelectUnit, SelectUnit, SelectUnits, AddUnitToSelectedGroup, AddUnitsToSelectedGroup,
//! AddAndRemoveUnitsFromSelectedGroup, RemoveUnitFromSelectedGroup, DeselectUnit, PlayAcknowledgement) over the
//! group objects (group_obj.rs) and [`SelectionHost`]: the player's units by id, the class's selectable test, the
//! doppelgangers, the selection's sounds, the viewed unit and the acknowledgement's timing. The player's "active"
//! selection (`mActiveGroup`, an `EESelectedGroup`) is the one these members change; `mSelectedGroup` is the network
//! selection.

use crate::units::group_obj::{self as go, GroupHost, G};
use std::os::raw::{c_long, c_ulong};

/// The C++ the selection's bodies call beyond the groups'.
pub trait SelectionHost: GroupHost {
    /// `RTSPlayer::GetUnit(id)` (the player's own unit of an id)
    fn player_unit(&mut self, player: u64, id: i64) -> u64;
    /// the unit's class's `IsSelectable(unit)`
    fn selectable(&mut self, unit: u64) -> bool;
    /// `gDoppelgangerManager.HasDoppelganger(unit's tile, unit, player id)`
    fn has_doppelganger(&mut self, player: u64, unit: u64) -> bool;
    /// `gWorld.mExecutingLocalSelection`
    fn executing_local_selection(&mut self) -> bool;
    /// `EEPlayForLocalPlayer(player, unit->mStats->GetSelectionSound(-1))` (0) or `GetAcknowledgeSound(-1)` (1)
    fn play_sound(&mut self, player: u64, unit: u64, which: u32);
    /// the world table's acknowledgement delay
    fn acknowledgement_delay(&mut self) -> c_ulong;
    /// the player's `mViewedUnit` and its store (no reference counted)
    fn viewed_unit(&mut self, player: u64) -> u64;
    fn set_viewed_unit(&mut self, player: u64, unit: u64);
    /// the player's `mSeesAll`
    fn sees_all(&mut self, player: u64) -> bool;
    /// the player's `mLastAcknowledgedGroup`, `mLastAcknowledgementTime` and their store
    fn acknowledged(&mut self, player: u64) -> (c_long, c_long);
    fn set_acknowledged(&mut self, player: u64, group: c_long, time: c_long);
    /// `EESelectPick(player, ids)` and its USPK record (TEST ONLY)
    fn select_pick(&mut self, player: u64, ids: &[u32]) -> Vec<u32>;
}

/// `kUnitTypePathPoint`, `kUnitTypeVolcano`, `kUnitTypeFarm`
const TYPE_PATH_POINT: i64 = 0xdf;
const TYPE_VOLCANO: i64 = 0x16e;
const TYPE_FARM: i64 = 0x2a;
const SELECTION_SOUND: u32 = 0;
const ACKNOWLEDGE_SOUND: u32 = 1;

fn active<H: SelectionHost>(h: &mut H, player: u64) -> G {
    h.player_selection(player, 0) as G
}

/// `UpdateSelection(unit, flag)`: a unit with the flag empties the selection first; the unit becomes the viewed one
/// (the reference moves).
pub fn update_selection<H: SelectionHost>(h: &mut H, player: u64, unit: u64, flag: bool) {
    if unit != 0 && flag {
        let g = active(h, player);
        // SAFETY: the player's live selection
        unsafe { go::clear(h, g) };
    }
    let old = h.viewed_unit(player);
    if old != 0 {
        h.dereference_unit(old);
    }
    h.set_viewed_unit(player, unit);
    if unit != 0 {
        h.reference_unit(unit);
    }
}

/// `TrySelectUnit(unit, flag)`: an own unit on the map joins the selection; a foreign unit is only viewed (refused
/// while own units are selected, with the flag).
pub fn try_select_unit<H: SelectionHost>(h: &mut H, player: u64, unit: u64, flag: bool) -> bool {
    let u = h.unit(unit);
    let mut result = true;
    // SAFETY (below): the player's live selection
    if flag {
        if u.player == player {
            if u.has_tile_node {
                update_selection(h, player, 0, true);
                let g = active(h, player);
                if !unsafe { go::contains(g, u.complex) } {
                    unsafe { go::add(h, g, u.complex) };
                }
            }
        } else if unsafe { go::first_unit(active(h, player)) } != 0 {
            result = false;
        } else {
            update_selection(h, player, unit, true);
        }
    } else if u.has_tile_node {
        update_selection(h, player, 0, true);
        let g = active(h, player);
        if !unsafe { go::contains(g, u.complex) } {
            unsafe { go::add(h, g, u.complex) };
        }
    } else if unsafe { go::len(active(h, player)) } == 0 {
        update_selection(h, player, unit, true);
    }
    result
}

/// `SelectUnit(id, flag, new_group)`: a selectable unit selected (a path point always; a living unit the player may
/// see, playing its sound for the local player's own; else a visible resource other than a farm); never a volcano.
pub fn select_unit<H: SelectionHost>(h: &mut H, player: u64, id: i64, flag: bool, new_group: bool) {
    let unit = h.world_unit(id);
    update_selection(h, player, 0, true);
    if new_group {
        let g = active(h, player);
        // SAFETY: the player's live selection
        unsafe { go::clear(h, g) };
    }
    if unit == 0 || !h.selectable(unit) {
        return;
    }
    let u = h.unit(unit);
    if u.unit_type == TYPE_PATH_POINT {
        try_select_unit(h, player, unit, flag);
    } else if u.unit_type != TYPE_VOLCANO {
        if u.hit_points > 0
            && (!flag || h.sees_all(player) || u.player == player || !h.has_doppelganger(player, unit))
            && try_select_unit(h, player, unit, flag)
        {
            let owner = u.player;
            if owner == player || h.player_id(owner) == 0 {
                if h.executing_local_selection() && owner == player {
                    h.play_sound(player, unit, SELECTION_SOUND);
                }
                return;
            }
        }
        if u.resource_amount > 0 && u.unit_type != TYPE_FARM && !h.has_doppelganger(player, unit) {
            try_select_unit(h, player, unit, flag);
        }
    }
}

/// `AddUnitToSelectedGroup(id, flag)`: selected, a new selection when adding (flag) a unit of another player than
/// the player's or the selection's.
pub fn add_unit_to_selected<H: SelectionHost>(h: &mut H, player: u64, id: i64, flag: bool) {
    let unit = h.world_unit(id);
    if unit == 0 {
        return;
    }
    let owner = h.unit(unit).player;
    let owner_id = h.player_id(owner);
    let new_group = flag && {
        // SAFETY: the player's live selection
        let first = unsafe { go::first_unit(active(h, player)) };
        owner_id != h.player_id(player) || first == 0 || {
            let first_owner = h.unit(first).player;
            h.player_id(first_owner) != owner_id
        }
    };
    select_unit(h, player, id, flag, new_group);
}

/// `AddUnitsToSelectedGroup(ids)` (`toggle` false) and `AddAndRemoveUnitsFromSelectedGroup(ids)` (true): the
/// player's built units not selected yet (not path points) added, the others (when toggling) removed; the last added
/// plays its sound during a local selection.
pub fn add_units<H: SelectionHost>(h: &mut H, player: u64, ids: &[c_long], toggle: bool) {
    let mut last = 0;
    let g = active(h, player);
    update_selection(h, player, 0, true);
    for &id in ids {
        let unit = h.player_unit(player, id as i64);
        if unit == 0 {
            continue;
        }
        let complex = h.unit(unit).complex;
        let c = h.unit(complex);
        // SAFETY: the player's live selection
        unsafe {
            if !go::contains(g, complex) && c.built && c.unit_type != TYPE_PATH_POINT {
                go::add(h, g, complex);
                last = complex;
            } else if toggle {
                go::remove_unit(h, g, complex);
            }
        }
    }
    if last != 0 && h.executing_local_selection() {
        h.play_sound(player, last, SELECTION_SOUND);
    }
}

/// `SelectUnits(ids)`: the selection emptied, then the pick's units (`EESelectPick`) added; the last plays its sound
/// during a local selection.
pub fn select_units<H: SelectionHost>(h: &mut H, player: u64, ids: &[c_long]) {
    let mut last = 0;
    let g = active(h, player);
    update_selection(h, player, 0, true);
    // SAFETY: the player's live selection
    unsafe { go::clear(h, g) };
    let ids32: Vec<u32> = ids.iter().map(|&i| i as u32).collect();
    for id in h.select_pick(player, &ids32) {
        let unit = h.player_unit(player, id as i64);
        let complex = h.unit(unit).complex;
        // SAFETY: as above
        unsafe { go::add(h, g, complex) };
        last = complex;
    }
    if last != 0 && h.executing_local_selection() {
        h.play_sound(player, last, SELECTION_SOUND);
    }
}

/// `DeselectUnit(unit)`: out of both selections, or (the viewed unit, or one with no complex unit) no longer viewed.
pub fn deselect_unit<H: SelectionHost>(h: &mut H, player: u64, unit: u64) {
    let viewed = h.viewed_unit(player);
    let complex = h.unit(unit).complex;
    if unit != viewed && complex != 0 {
        let (a, s) = (h.player_selection(player, 0), h.player_selection(player, 1));
        // SAFETY: the player's live selections
        unsafe {
            go::remove_unit(h, a as G, complex);
            go::remove_unit(h, s as G, complex);
        }
    } else if viewed != 0 {
        h.dereference_unit(viewed);
        h.set_viewed_unit(player, 0);
    }
}

/// `RemoveUnitFromSelectedGroup(id)`.
pub fn remove_unit_from_selected<H: SelectionHost>(h: &mut H, player: u64, id: i64) {
    let unit = h.world_unit(id);
    if unit != 0 {
        deselect_unit(h, player, unit);
    }
}

/// `PlayAcknowledgement(unit)`: the unit's acknowledgement, at most once per delay for the same selection.
pub fn play_acknowledgement<H: SelectionHost>(h: &mut H, player: u64, unit: u64) {
    let (last_group, last_time) = h.acknowledged(player);
    // SAFETY: the player's live selection
    let change = unsafe { (*active(h, player)).mChangeTime };
    let mut group = last_group;
    if last_group == change {
        let delay = h.acknowledgement_delay();
        if (h.time() as c_ulong).wrapping_sub(last_time as c_ulong) <= delay {
            return;
        }
    } else {
        group = change;
    }
    let now = h.time();
    h.set_acknowledged(player, group, now);
    h.play_sound(player, unit, ACKNOWLEDGE_SOUND);
}

/// The player calls of `RefreshUpgrades` beyond the selection's.
pub trait UpgradesHost: SelectionHost {
    /// `EEUnitClassCount()` and the player's `GetUpgrade(i)` (0 for none)
    fn class_count(&mut self) -> u64;
    fn upgrade(&mut self, player: u64, class: u64) -> u64;
    /// the upgrade's `mFlightTime`, `mMaxHitPoints`, `mAreaEffectTemplates[0]` and its class's unit type
    fn upgrade_values(&mut self, upgrade: u64) -> (c_long, c_long, c_long, i64);
    /// the upgrade's `Init()`
    fn init_upgrade(&mut self, upgrade: u64);
    /// a complex unit's `mFlightTime`
    fn flight_time(&mut self, complex: u64) -> c_long;
    /// the unit's `SetHitPoints(v)`, the complex unit's `SetFlightTime(v)` and `MakeAreaEffects()`
    fn set_hit_points(&mut self, unit: u64, value: c_long);
    fn set_flight_time(&mut self, complex: u64, value: c_long);
    fn make_area_effects(&mut self, complex: u64);
    /// `mCivilization.AddUpgrades(mCivilization.GetPurchasedUpgrades(), GetPlayerID(), false)`
    fn add_civilization_upgrades(&mut self, player: u64);
}

/// A value scaled from `old` to `new` as the original did: `(float)value / old * new`, rounded up (`UMCeil`).
fn rescale(value: c_long, old: c_long, new: c_long) -> c_long {
    let scaled = (value as f32 / old as f32) * new as f32;
    crate::units::load::um_ceil(scaled) as f32 as c_long
}

/// `EEPlayer::RefreshUpgrades`: each class's upgrade made again from its class; where its hit points, flight time or
/// first area effect changed, the player's units of the class take them (hit points and flight time scaled, at least
/// one), then the civilization's purchased upgrades applied again.
pub fn refresh_upgrades<H: UpgradesHost>(h: &mut H, player: u64) {
    for class in 0..h.class_count() {
        let upgrade = h.upgrade(player, class);
        if upgrade == 0 {
            continue;
        }
        let (old_flight, old_hit_points, old_kind, _) = h.upgrade_values(upgrade);
        h.init_upgrade(upgrade);
        let (flight, hit_points, kind, unit_type) = h.upgrade_values(upgrade);
        if old_hit_points == hit_points && old_flight == flight && old_kind == kind {
            continue;
        }
        for unit in h.player_units(player) {
            let u = h.unit(unit);
            if u.unit_type != unit_type {
                continue;
            }
            let complex = u.complex;
            let built = complex != 0 && h.unit(complex).built;
            if old_hit_points != hit_points && (complex == 0 || built) {
                let v = rescale(u.hit_points as c_long, old_hit_points, hit_points);
                if v > 0 {
                    h.set_hit_points(unit, v);
                }
            }
            if complex == 0 {
                continue;
            }
            if old_flight != flight {
                let v = rescale(h.flight_time(complex), old_flight, flight);
                if v > 0 {
                    h.set_flight_time(complex, v);
                }
            }
            if old_kind != kind {
                h.make_area_effects(complex);
            }
        }
    }
    h.add_civilization_upgrades(player);
}
