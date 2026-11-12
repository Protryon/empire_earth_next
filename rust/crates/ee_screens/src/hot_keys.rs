//! The main game form's hot key events whole: the original `EEUIEHotKeyEvents.cpp` (the control groups' select, add
//! and create, the player event, the unit selection hot keys and the closest aircraft) over a host ([`HotKeyHost`])
//! whose calls are each one game call (`UI Events/EEScreens.cpp`). The current player's units come as a snapshot of
//! records ([`EeScreensHotKeyUnit`], one per unit of its list, in order); the decisions (which units a hot key takes,
//! which one is next, which aircraft is closest) are here. The events have no state: the original's objects held
//! only their constructor's arguments, which are the event's code here.

use core::ffi::c_long;

// ---- the events (EEScreenEvent::mEvent)

/// `EEUIEHKSelectGroup(group, false)` / `(group, true)`: the number keys (with Alt: the true one).
pub const EE_SCREENS_HK_SELECT_GROUP: u32 = 0x0e00;
pub const EE_SCREENS_HK_SELECT_GROUP_ADD: u32 = 0x0e10;
/// `EEUIEHKAddSelectionToGroup(group)` (Shift) and `EEUIEHKCreateGroup(group)` (Ctrl).
pub const EE_SCREENS_HK_ADD_TO_GROUP: u32 = 0x0e20;
pub const EE_SCREENS_HK_CREATE_GROUP: u32 = 0x0e30;
/// Space: the player event action.
pub const EE_SCREENS_HK_PLAYER_EVENT: u32 = 0x0e40;
/// `EEUIESelectClosestAircraft(kind)` (kinds 0..5).
pub const EE_SCREENS_HK_AIRCRAFT: u32 = 0x0e50;
/// `EEUIEHKUnitSelection(type, idle workers, idle military, select all)`: this base, the flags in bits 16..18, the
/// unit type in the low 16 bits ([`ee_screens_hot_key_unit_selection`]).
pub const EE_SCREENS_HK_UNIT_SELECTION: u32 = 0x0f00_0000;

const GROUPS: u32 = 10;
const AIRCRAFT_KINDS: u32 = 6;

pub fn is_hot_key_event(event: u32) -> bool {
    let in_range = |base: u32, n: u32| (base..base + n).contains(&event);
    in_range(EE_SCREENS_HK_SELECT_GROUP, GROUPS)
        || in_range(EE_SCREENS_HK_SELECT_GROUP_ADD, GROUPS)
        || in_range(EE_SCREENS_HK_ADD_TO_GROUP, GROUPS)
        || in_range(EE_SCREENS_HK_CREATE_GROUP, GROUPS)
        || event == EE_SCREENS_HK_PLAYER_EVENT
        || in_range(EE_SCREENS_HK_AIRCRAFT, AIRCRAFT_KINDS)
        || event & 0xff00_0000 == EE_SCREENS_HK_UNIT_SELECTION
}

/// The unit selection hot key's code for its constructor's arguments (the unit type a 16-bit type id).
#[no_mangle]
pub extern "C" fn ee_screens_hot_key_unit_selection(
    unit_type: i32,
    idle_workers: bool,
    idle_military: bool,
    select_all: bool,
) -> u32 {
    EE_SCREENS_HK_UNIT_SELECTION
        | (idle_workers as u32) << 16
        | (idle_military as u32) << 17
        | (select_all as u32) << 18
        | (unit_type as u32 & 0xffff)
}

// ---- the host's ops (ee_screenscpp_call; the units: ee_screenscpp_hot_key_units)

pub const EE_SCREENS_GAME_HK_UPDATE_SELECTION: u32 = 0x260; // the current player's UpdateSelection(NULL, true)
pub const EE_SCREENS_GAME_HK_RESET_SAVED: u32 = 0x261; // its mSavedGroups.Reset(0x14, true)
pub const EE_SCREENS_GAME_HK_GROUP: u32 = 0x262; // (0 select / 1 add / 2 create, group, flag) the saved group message
pub const EE_SCREENS_OP_HK_GROUP_BUTTON: u32 = 0x263; // (group) the group's button shows the selection's first unit
pub const EE_SCREENS_GAME_HK_PLAYER_EVENT: u32 = 0x264; // PostAction(new EEAPlayerEvent)
pub const EE_SCREENS_GAME_HK_SHIFT: u32 = 0x265; // -> the keyboard's Shift entity's value
pub const EE_SCREENS_GAME_HK_SELECT_UNIT: u32 = 0x266; // (id) EEMSelectUnit
pub const EE_SCREENS_GAME_HK_ADD_UNIT: u32 = 0x267; // (id) EEMAlwaysAddUnitToSelectedGroup
pub const EE_SCREENS_GAME_HK_CENTER: u32 = 0x268; // (record) the camera on that record's complex unit, no following
pub const EE_SCREENS_GAME_HK_ALARM: u32 = 0x269; // the hot key alarm sound, if loaded

/// A unit of the current player's list as the hot keys see it. The unit's own fields (`unit_type`, `category`,
/// `command`, `max_velocity`, `complex_id`, [`EE_SCREENS_HK_F_SELECTED`]) are filled for a complex, living unit on a tile; the
/// complex unit's (`c_*`, `distance`) for any unit with a complex unit, and `distance` only for the aircraft's
/// snapshot (the squared distance from the point under the view's centre, z 0; a carried unit's its container's).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeScreensHotKeyUnit {
    pub unit_id: u32,
    pub complex_id: u32,
    pub flags: u32,
    pub unit_type: i32,
    pub category: u32,
    pub command: u32,
    pub max_velocity: u32,
    pub c_category: u32,
    pub c_hit_points: i32,
    pub c_goal_kind: u32,
    pub c_family: u32,
    pub c_ammo: u32,
    pub c_type: i32,
    pub c_max_hit_points: i32,
    pub distance: u32,
}

/// [`EeScreensHotKeyUnit::flags`]
pub const EE_SCREENS_HK_F_COMPLEX: u32 = 1; // the unit has a complex unit
pub const EE_SCREENS_HK_F_ALIVE: u32 = 2; // the unit's hit points > 0
pub const EE_SCREENS_HK_F_ON_TILE: u32 = 4; // the unit is on a tile
pub const EE_SCREENS_HK_F_SELECTED: u32 = 8; // the selected group contains its complex unit
pub const EE_SCREENS_HK_F_C_COMMANDABLE: u32 = 0x10;
pub const EE_SCREENS_HK_F_C_TILE: u32 = 0x20; // the complex unit's tile x is not -10000
pub const EE_SCREENS_HK_F_C_CONTAINER: u32 = 0x40;
pub const EE_SCREENS_HK_F_C_DISABLED: u32 = 0x80;
pub const EE_SCREENS_HK_F_C_PREVIOUS_PART: u32 = 0x100;
pub const EE_SCREENS_HK_F_C_GOAL: u32 = 0x200;

// the unit categories, types, families, actions and goals (ee/EEUnitClassIDs.h, ee/EEUnitGoal.h)
const CATEGORY_LAND_CITIZEN: u32 = 6;
const CATEGORY_NAVAL_CITIZEN: u32 = 0x16;
const CATEGORY_AIRPLANE: u32 = 3;
const CATEGORY_PLANET_SPACESHIP: u32 = 0x22;
const TYPE_TOWN_CENTER: i32 = 0x23;
const TYPE_CAPITOL: i32 = 0x29;
const TYPE_GRANARY: i32 = 0x34;
const TYPE_GRANARY_IMPROVED: i32 = 0x2ba;
const TYPE_SPACE_FIGHTER: i32 = 0x2f6;
const TYPE_PLANETARY_FIGHTER: i32 = 0x330;
const FAMILY_AIRCRAFT: u32 = 5;
const FAMILY_BOMBER: u32 = 0x14;
const FAMILY_ATOMIC_BOMBER: u32 = 0x2a;
const ACTION_IDLE: u32 = 5000;
const ACTION_WAIT_FOR_OPEN_POSITION: u32 = 4026;
const GOAL_LOCATION: u32 = 1;
const GROUP_BUTTONS: u32 = 0xd4;

/// The game's calls of the hot keys.
pub trait HotKeyHost {
    /// `EE_SCREENS_GAME_*` / `EE_SCREENS_OP_*` with up to three arguments and an answer.
    fn call(&mut self, op: u32, a: u32, b: u32, c: u32) -> u32;
    /// The current player's units (the aircraft's snapshot with the distances).
    fn units(&mut self, aircraft: bool) -> Vec<EeScreensHotKeyUnit>;
}

/// The unit selection's arguments of a code.
fn unit_selection(event: u32) -> (i32, bool, bool, bool) {
    (event as u16 as i32, event & 1 << 16 != 0, event & 1 << 17 != 0, event & 1 << 18 != 0)
}

/// `EEUIEHKUnitSelection`: the hot key's units (idle citizens, idle military, town centres and capitols, or a unit
/// type; granaries take improved ones too); the next one by id after the selection's highest (or the lowest
/// unselected, or the lowest) with the camera on it, or all of them; Shift adds to the selection.
fn select_units<H: HotKeyHost + ?Sized>(h: &mut H, event: u32) {
    let (unit_type, idle_workers, idle_military, select_all) = unit_selection(event);
    h.call(EE_SCREENS_GAME_HK_UPDATE_SELECTION, 0, 0, 0);
    let special = unit_type == TYPE_TOWN_CENTER;
    // the candidates: (record index, complex unit's id, selected), once per matching rule as the original
    let mut units: Vec<(usize, u32, bool)> = Vec::new();
    let all = h.units(false);
    let want = EE_SCREENS_HK_F_COMPLEX | EE_SCREENS_HK_F_ALIVE | EE_SCREENS_HK_F_ON_TILE;
    for (i, u) in all.iter().enumerate().filter(|(_, u)| u.flags & want == want) {
        let entry = (i, u.complex_id, u.flags & EE_SCREENS_HK_F_SELECTED != 0);
        let citizen = u.category == CATEGORY_LAND_CITIZEN || u.category == CATEGORY_NAVAL_CITIZEN;
        if idle_workers && citizen && (u.command == ACTION_IDLE || u.command == ACTION_WAIT_FOR_OPEN_POSITION) {
            units.push(entry);
        }
        if idle_military && !citizen && u.command == ACTION_IDLE && f32::from_bits(u.max_velocity) > 0.0 {
            units.push(entry);
        }
        if special && (u.unit_type == TYPE_CAPITOL || u.unit_type == TYPE_TOWN_CENTER) {
            units.push(entry);
        }
        if unit_type == TYPE_GRANARY && u.unit_type == TYPE_GRANARY_IMPROVED {
            units.push(entry);
        }
        if !idle_workers && !idle_military && !special && unit_type == u.unit_type {
            units.push(entry);
        }
    }
    if units.is_empty() {
        return;
    }
    h.call(EE_SCREENS_GAME_HK_RESET_SAVED, 0, 0, 0);
    let selection: Vec<(usize, u32)> = if !select_all {
        let (mut lowest_selected, mut highest_selected, mut lowest_unselected) = (None, None, None);
        let id = |u: Option<(usize, u32)>| u.map(|(_, id)| id);
        for &(i, uid, selected) in &units {
            if selected {
                if id(lowest_selected).is_none_or(|l| uid < l) {
                    lowest_selected = Some((i, uid));
                }
                if id(highest_selected).is_none_or(|l| uid > l) {
                    highest_selected = Some((i, uid));
                }
            } else if id(lowest_unselected).is_none_or(|l| uid < l) {
                lowest_unselected = Some((i, uid));
            }
        }
        let next = if let Some((_, highest)) = highest_selected {
            let mut next: Option<(usize, u32)> = None;
            for &(i, uid, selected) in &units {
                if uid > highest && id(next).is_none_or(|n| uid < n) && !selected {
                    next = Some((i, uid));
                }
            }
            next.or(lowest_unselected).or(lowest_selected)
        } else {
            lowest_unselected
        };
        vec![next.expect("a candidate")]
    } else {
        units.iter().rev().map(|&(i, uid, _)| (i, uid)).collect()
    };
    let add = h.call(EE_SCREENS_GAME_HK_SHIFT, 0, 0, 0) as i32 > 0;
    // the original popped its selection from the back
    for (count, &(i, uid)) in selection.iter().rev().enumerate() {
        if !add && count == 0 {
            h.call(EE_SCREENS_GAME_HK_SELECT_UNIT, uid, 0, 0);
            if !select_all {
                h.call(EE_SCREENS_GAME_HK_CENTER, i as u32, 0, 0);
            }
        } else {
            h.call(EE_SCREENS_GAME_HK_ADD_UNIT, uid, 0, 0);
        }
    }
}

/// `IsSelectableAircraft`: a living, commandable, free aircraft of the kind (bombers, atomic bombers, two fighter
/// groups, space or planetary fighters); one inside a container only at full hit points.
fn selectable_aircraft(u: &EeScreensHotKeyUnit, kind: u32) -> bool {
    if u.flags & EE_SCREENS_HK_F_COMPLEX == 0
        || (u.c_category != CATEGORY_AIRPLANE && u.c_category != CATEGORY_PLANET_SPACESHIP)
    {
        return false;
    }
    let free = u.c_hit_points > 0
        && u.flags & EE_SCREENS_HK_F_C_COMMANDABLE != 0
        && u.flags & (EE_SCREENS_HK_F_C_TILE | EE_SCREENS_HK_F_C_CONTAINER) != 0
        && u.flags & EE_SCREENS_HK_F_C_DISABLED == 0
        && u.flags & EE_SCREENS_HK_F_C_PREVIOUS_PART == 0
        && (u.flags & EE_SCREENS_HK_F_C_GOAL == 0 || u.c_goal_kind == GOAL_LOCATION);
    if !free {
        return false;
    }
    let (family, ammo) = (u.c_family, u.c_ammo);
    let kind_matches = match kind {
        0 => family == FAMILY_BOMBER && ammo == 0x1f,
        1 => family == FAMILY_ATOMIC_BOMBER && ammo == 0x1f,
        2 => family == FAMILY_AIRCRAFT && ammo == 0xc,
        3 => family == FAMILY_AIRCRAFT && (ammo == 0x28 || ammo == 0x1e || ammo == 0x29),
        4 => u.c_type == TYPE_SPACE_FIGHTER,
        5 => u.c_type == TYPE_PLANETARY_FIGHTER,
        _ => false,
    };
    kind_matches && (u.flags & EE_SCREENS_HK_F_C_CONTAINER == 0 || u.c_hit_points == u.c_max_hit_points)
}

/// `EEUIESelectClosestAircraft`: the matching aircraft closest to the point under the view's centre (Shift: one not
/// yet selected, added); the hot key alarm when there is none.
fn select_aircraft<H: HotKeyHost + ?Sized>(h: &mut H, kind: u32) {
    let replace = h.call(EE_SCREENS_GAME_HK_SHIFT, 0, 0, 0) == 0;
    let units = h.units(true);
    let mut closest: Option<(u32, f32)> = None;
    for u in &units {
        if !selectable_aircraft(u, kind) || (!replace && u.flags & EE_SCREENS_HK_F_SELECTED != 0) {
            continue;
        }
        let distance = f32::from_bits(u.distance);
        if closest.is_none_or(|(_, best)| distance < best) {
            closest = Some((u.unit_id, distance));
        }
    }
    match closest {
        Some((id, _)) if replace => h.call(EE_SCREENS_GAME_HK_SELECT_UNIT, id, 0, 0),
        Some((id, _)) => h.call(EE_SCREENS_GAME_HK_ADD_UNIT, id, 0, 0),
        None => h.call(EE_SCREENS_GAME_HK_ALARM, 0, 0, 0),
    };
}

/// One of the hot key events; `None` for another form's, else its `ProcessEvent` result (always `kErrorOK`).
pub fn dispatch<H: HotKeyHost + ?Sized>(h: &mut H, event: u32) -> Option<c_long> {
    if !is_hot_key_event(event) {
        return None;
    }
    let group = |base: u32| (base..base + GROUPS).contains(&event).then(|| event - base);
    let saved = |h: &mut H, kind: u32, g: u32, flag: u32| {
        h.call(EE_SCREENS_GAME_HK_UPDATE_SELECTION, 0, 0, 0);
        h.call(EE_SCREENS_GAME_HK_RESET_SAVED, 0, 0, 0);
        h.call(EE_SCREENS_GAME_HK_GROUP, kind, g, flag);
    };
    if let Some(g) = group(EE_SCREENS_HK_SELECT_GROUP) {
        saved(h, 0, g, 0);
    } else if let Some(g) = group(EE_SCREENS_HK_SELECT_GROUP_ADD) {
        saved(h, 0, g, 1);
    } else if let Some(g) = group(EE_SCREENS_HK_ADD_TO_GROUP) {
        saved(h, 1, g, 0);
    } else if let Some(g) = group(EE_SCREENS_HK_CREATE_GROUP) {
        saved(h, 2, g, 0);
        h.call(EE_SCREENS_OP_HK_GROUP_BUTTON, GROUP_BUTTONS + g, 0, 0);
    } else if event == EE_SCREENS_HK_PLAYER_EVENT {
        h.call(EE_SCREENS_GAME_HK_PLAYER_EVENT, 0, 0, 0);
    } else if (EE_SCREENS_HK_AIRCRAFT..EE_SCREENS_HK_AIRCRAFT + AIRCRAFT_KINDS).contains(&event) {
        select_aircraft(h, event - EE_SCREENS_HK_AIRCRAFT);
    } else {
        select_units(h, event);
    }
    Some(0)
}

#[cfg(test)]
mod golden;

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn codes() {
        let code = ee_screens_hot_key_unit_selection(0x20, true, false, true);
        assert_eq!(code, 0x0f05_0020);
        assert_eq!(unit_selection(code), (0x20, true, false, true));
        assert!(is_hot_key_event(code) && is_hot_key_event(EE_SCREENS_HK_AIRCRAFT + 5));
        assert!(!is_hot_key_event(EE_SCREENS_HK_AIRCRAFT + 6) && !is_hot_key_event(0x0e0a));
    }
}
