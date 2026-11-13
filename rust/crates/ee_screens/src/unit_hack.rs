//! The main game form's unit hack whole in Rust (the original `EEUIFMGUnitHack`, `EEUIFMainGame.cpp`
//! 0x2628e5..0x262e73; no RTTI, not polymorphic): the alphabetical list of the player-placeable unit classes, the
//! object combo box it fills, the reload of a class's data and the developer placement of a unit on a Shift or Ctrl
//! click. The C++ class is a shim holding this record; the world, the controls, the keyboard and the messages are
//! host ops ([`UnitHackHost`]).
//!
//! The placement needs a mode above 0, and nothing sets the mode (the constructor's 0): the original's placement
//! returns after its tile and key reads, kept as it was.

/// `FillComboBox`.
pub const EE_SCREENS_UH_FILL_COMBO_BOX: u32 = 1;
/// `LoadUnitData`.
pub const EE_SCREENS_UH_LOAD_UNIT_DATA: u32 = 2;
/// `PlaceUnit(x, y)`.
pub const EE_SCREENS_UH_PLACE_UNIT: u32 = 3;
/// `Update(x, y)`.
pub const EE_SCREENS_UH_UPDATE: u32 = 4;

/// The game's unit class count (`EEUnitClassCount()`).
pub const EE_SCREENS_UH_OP_CLASS_COUNT: u32 = 1;
/// Class a: 0 no class, else 1 | enabled << 1 | placeable << 2.
pub const EE_SCREENS_UH_OP_CLASS: u32 = 2;
/// Class a's category.
pub const EE_SCREENS_UH_OP_CLASS_CATEGORY: u32 = 3;
/// Class a's unit type.
pub const EE_SCREENS_UH_OP_CLASS_TYPE: u32 = 4;
/// Class a's name: its byte length, the bytes written into the buffer when it has room.
pub const EE_SCREENS_UH_OP_CLASS_NAME: u32 = 5;
/// The object combo box: `DeleteAllEntries`.
pub const EE_SCREENS_UH_OP_COMBO_CLEAR: u32 = 6;
/// The object combo box: `AppendLine` of the buffer's bytes.
pub const EE_SCREENS_UH_OP_COMBO_APPEND: u32 = 7;
/// The object combo box: `SetCurrentSelection(0)`.
pub const EE_SCREENS_UH_OP_COMBO_SELECT_FIRST: u32 = 8;
/// The object combo box's `GetCurrentSelection` (the selection; none is all ones).
pub const EE_SCREENS_UH_OP_COMBO_SELECTION: u32 = 9;
/// A list's `GetListBox()->GetCurrentSelection(f)`: a 1 the object list, 2 the player list. Returns the call's error
/// code; the selection goes to `out`.
pub const EE_SCREENS_UH_OP_LIST_SELECTION: u32 = 10;
/// Whether the map has the tile at (`(long)f[0]`, `(long)f[1]`).
pub const EE_SCREENS_UH_OP_TILE: u32 = 11;
/// The Shift (a 0) or Ctrl (a 1) key's value.
pub const EE_SCREENS_UH_OP_KEY: u32 = 12;
/// Whether player slot a is empty or the local player.
pub const EE_SCREENS_UH_OP_PLAYER_SKIPPED: u32 = 13;
/// The local player's id.
pub const EE_SCREENS_UH_OP_LOCAL_PLAYER: u32 = 14;
/// `EEMHackANewUnit(a, the tile f, (unsigned char)b)` sent as local input.
pub const EE_SCREENS_UH_OP_SEND_PLACE: u32 = 15;
/// `EEMLoadUnitData(a, true)` sent as local input.
pub const EE_SCREENS_UH_OP_SEND_LOAD: u32 = 16;
/// Whether the view rectangle contains the point f.
pub const EE_SCREENS_UH_OP_IN_VIEW: u32 = 17;
/// The left mouse button: a 0 `JustPressed`, 1 `JustUnpressed`.
pub const EE_SCREENS_UH_OP_LEFT_BUTTON: u32 = 18;
/// `EEPointUnitOrTile(f, ..., true)`: whether a point was found; its x and y into f.
pub const EE_SCREENS_UH_OP_POINT: u32 = 19;

const UH_CATEGORY_PROJECTILE: i32 = 9;
const UH_CATEGORY_BOMB: i32 = 0xb;
const UH_CATEGORY_MISSILE: i32 = 0xc;
const UH_CATEGORY_TORPEDO: i32 = 0x14;
const UH_TYPE_PATH_POINT: i32 = 0xdf;
const UH_TYPE_WAYPOINT: i32 = 0x104;
const UH_TYPE_FOUNDATION_2X2: i32 = 0x132;
const UH_TYPE_FOUNDATION_3X3: i32 = 0x133;
const UH_TYPE_FOUNDATION_1X1: i32 = 0x136;
const UH_TYPE_FOUNDATION_6X6: i32 = 0x168;
const UH_ERROR_OK: i32 = 0;
const UH_ERROR_D: i32 = 0xd;

pub trait UnitHackHost {
    /// The op with its arguments, the point `f` (in and out), the selection `out` and the text buffer.
    fn op(&mut self, op: u32, a: i32, b: i32, f: &mut [f32; 2], out: &mut u32, text: &mut [u8]) -> i32;
}

/// The hack's record (`mNames`, `mClassIDs`, `mMode`, `mL28`).
#[derive(Clone, Debug, Default, PartialEq)]
pub struct UnitHack {
    pub names: Vec<Vec<u8>>,
    pub class_ids: Vec<i32>,
    pub mode: u32,
    pub l28: i32,
}

fn int(h: &mut (impl UnitHackHost + ?Sized), op: u32, a: i32) -> i32 {
    h.op(op, a, 0, &mut [0.0; 2], &mut 0, &mut [])
}

impl UnitHack {
    /// 0x2628e5: the player-placeable unit classes, enabled, not projectiles, bombs, missiles or torpedoes, not
    /// waypoints, path points or foundations, by name in byte order (a name equal to an earlier one goes after it).
    pub fn new(h: &mut (impl UnitHackHost + ?Sized)) -> Self {
        let mut hack = UnitHack { l28: -1, ..Self::default() };
        let count = int(h, EE_SCREENS_UH_OP_CLASS_COUNT, 0);
        for i in 0..count {
            let class = int(h, EE_SCREENS_UH_OP_CLASS, i);
            if class & 1 == 0 || class & 2 == 0 {
                continue;
            }
            let category = int(h, EE_SCREENS_UH_OP_CLASS_CATEGORY, i);
            if matches!(category, UH_CATEGORY_PROJECTILE | UH_CATEGORY_BOMB | UH_CATEGORY_MISSILE | UH_CATEGORY_TORPEDO)
            {
                continue;
            }
            if class & 4 == 0 {
                continue;
            }
            let unit_type = int(h, EE_SCREENS_UH_OP_CLASS_TYPE, i);
            if matches!(
                unit_type,
                UH_TYPE_WAYPOINT
                    | UH_TYPE_PATH_POINT
                    | UH_TYPE_FOUNDATION_1X1
                    | UH_TYPE_FOUNDATION_2X2
                    | UH_TYPE_FOUNDATION_3X3
                    | UH_TYPE_FOUNDATION_6X6
            ) {
                continue;
            }
            let name = class_name(h, i);
            let j = hack.names.iter().position(|n| name < *n).unwrap_or(hack.names.len());
            hack.names.insert(j, name);
            hack.class_ids.insert(j, i);
        }
        hack
    }

    /// 0x262c6f: the object combo box refilled with the names, the first selected.
    pub fn fill_combo_box(&self, h: &mut (impl UnitHackHost + ?Sized)) {
        int(h, EE_SCREENS_UH_OP_COMBO_CLEAR, 0);
        for name in &self.names {
            let mut text = name.clone();
            h.op(EE_SCREENS_UH_OP_COMBO_APPEND, 0, 0, &mut [0.0; 2], &mut 0, &mut text);
        }
        int(h, EE_SCREENS_UH_OP_COMBO_SELECT_FIRST, 0);
    }

    /// 0x262cdc: a reload of the data of the class selected in the object combo box.
    pub fn load_unit_data(&self, h: &mut (impl UnitHackHost + ?Sized)) {
        let selection = int(h, EE_SCREENS_UH_OP_COMBO_SELECTION, 0) as u32;
        if selection != u32::MAX {
            int(h, EE_SCREENS_UH_OP_SEND_LOAD, self.class_ids[selection as usize]);
        }
    }

    /// 0x262e29: the selection of the object list (mode 1) or the player list (mode 2), else 0.
    pub fn selection(&self, h: &mut (impl UnitHackHost + ?Sized)) -> u32 {
        let mut selection = 0;
        match self.mode {
            1 => {
                h.op(EE_SCREENS_UH_OP_LIST_SELECTION, 1, 0, &mut [0.0; 2], &mut selection, &mut []);
            }
            2 => {
                h.op(EE_SCREENS_UH_OP_LIST_SELECTION, 2, 0, &mut [0.0; 2], &mut selection, &mut []);
            }
            _ => {}
        }
        selection
    }

    /// 0x262aa5: in mode 1 with Shift or Ctrl held, a unit of the listed class placed at the tile: for the player
    /// list's player (counted past the empty slots and the local player) with Ctrl alone, else for the local player.
    pub fn place_unit(&self, h: &mut (impl UnitHackHost + ?Sized), tile: [f32; 2]) {
        if h.op(EE_SCREENS_UH_OP_TILE, 0, 0, &mut tile.clone(), &mut 0, &mut []) == 0 {
            return;
        }
        let shift = int(h, EE_SCREENS_UH_OP_KEY, 0);
        let control = int(h, EE_SCREENS_UH_OP_KEY, 1);
        if self.mode == 0 || shift == 0 && control == 0 {
            return;
        }
        if self.mode != 1 {
            return;
        }
        let unit_type = self.class_ids[self.selection(h) as usize];
        let mut player = 0u32;
        let listed = shift == 0 && {
            let first = h.op(EE_SCREENS_UH_OP_LIST_SELECTION, 2, 0, &mut [0.0; 2], &mut player, &mut []);
            first == UH_ERROR_OK
                || h.op(EE_SCREENS_UH_OP_LIST_SELECTION, 2, 0, &mut [0.0; 2], &mut player, &mut []) == UH_ERROR_D
        };
        let player = if listed {
            let mut index = 0i32;
            let mut left = player.wrapping_add(1);
            loop {
                index += 1;
                while int(h, EE_SCREENS_UH_OP_PLAYER_SKIPPED, index) != 0 {
                    index += 1;
                }
                left = left.wrapping_sub(1);
                if left == 0 {
                    break;
                }
            }
            index
        } else {
            int(h, EE_SCREENS_UH_OP_LOCAL_PLAYER, 0)
        };
        h.op(EE_SCREENS_UH_OP_SEND_PLACE, unit_type, player, &mut tile.clone(), &mut 0, &mut []);
    }

    /// 0x262d66: the left button over the game view: a press finds the point (and does nothing more: the
    /// original's empty 0x157269), a release places a unit at the found tile.
    pub fn update(&self, h: &mut (impl UnitHackHost + ?Sized), location: [f32; 2]) {
        let inside = h.op(EE_SCREENS_UH_OP_IN_VIEW, 0, 0, &mut location.clone(), &mut 0, &mut []) != 0;
        if int(h, EE_SCREENS_UH_OP_LEFT_BUTTON, 0) != 0 {
            if inside {
                let mut point = location;
                h.op(EE_SCREENS_UH_OP_POINT, 0, 0, &mut point, &mut 0, &mut []);
            }
        } else if int(h, EE_SCREENS_UH_OP_LEFT_BUTTON, 1) != 0 && inside {
            let mut point = location;
            if h.op(EE_SCREENS_UH_OP_POINT, 0, 0, &mut point, &mut 0, &mut []) != 0 {
                self.place_unit(h, point);
            }
        }
    }
}

fn class_name(h: &mut (impl UnitHackHost + ?Sized), class: i32) -> Vec<u8> {
    let mut text = vec![0u8; 64];
    loop {
        let n = h.op(EE_SCREENS_UH_OP_CLASS_NAME, class, 0, &mut [0.0; 2], &mut 0, &mut text) as usize;
        if n <= text.len() {
            text.truncate(n);
            return text;
        }
        text = vec![0u8; n];
    }
}

#[cfg(test)]
mod tests;
