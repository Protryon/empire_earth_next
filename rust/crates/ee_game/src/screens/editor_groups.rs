//! The scenario editor's seven tab groups as Rust objects (the scenario editor lane, round 1): once the C++ base
//! `EEUIFSEMainGroup` (vtable 0x442e14: eleven slots, all empty but `MouseMove`'s pure one) and its subclasses
//! `EEUIFSEMap`, `EEUIFSEElevation`, `EEUIFSETerrain`, `EESEUnit`, `EESETriggerScreen`, `EESEPlayerScreen` and
//! `EESEInstructions`, each a holder of its Rust tab (`editor_map.rs` .. `editor_instructions.rs`) whose slots
//! forwarded to it. The editor form (`editor_form.rs`) makes them by kind and calls their slots through its host
//! (`EE_SCREENS_SEFM_OP_TAB_GROUP`, `_GROUP_CALL`); the group (`editor_groups_ffi.rs`) holds the tab and answers.
//!
//! [`overrides`] is the classes' vtables: which slot each tab overrode (the others were the base's empty one).

use crate::screens::editor_form::*;

/// The tabs (`kEditorTab*`, ee/EEScenarioEditorIDs.h: `CreateTabGroup`'s cases).
pub const TAB_MAP: u32 = 0;
pub const TAB_ELEVATION: u32 = 1;
pub const TAB_TERRAIN: u32 = 2;
pub const TAB_UNITS: u32 = 3;
pub const TAB_TRIGGERS: u32 = 4;
pub const TAB_PLAYERS: u32 = 5;
pub const TAB_INSTRUCTIONS: u32 = 6;

/// Whether the tab `tab`'s class overrode the base slot `slot` (`EE_SCREENS_SEFM_GROUP_*`).
pub fn overrides(tab: u32, slot: i32) -> bool {
    const UPDATE: u16 = 1 << EE_SCREENS_SEFM_GROUP_UPDATE;
    const LEAVE: u16 = 1 << EE_SCREENS_SEFM_GROUP_LEAVE;
    const DRAW: u16 = 1 << EE_SCREENS_SEFM_GROUP_DRAW;
    const DISABLE: u16 = 1 << EE_SCREENS_SEFM_GROUP_DISABLE;
    const LOAD: u16 = 1 << EE_SCREENS_SEFM_GROUP_LOAD;
    const ENTER: u16 = 1 << EE_SCREENS_SEFM_GROUP_ENTER;
    const STORE: u16 = 1 << EE_SCREENS_SEFM_GROUP_STORE;
    const ENABLE: u16 = 1 << EE_SCREENS_SEFM_GROUP_ENABLE;
    const MOUSE_MOVE: u16 = 1 << EE_SCREENS_SEFM_GROUP_MOUSE_MOVE;
    const UPDATE_KEYS: u16 = 1 << EE_SCREENS_SEFM_GROUP_UPDATE_KEYS;
    let mask = match tab {
        // (the map's and the player tab's MouseMove were empty overrides of the pure slot)
        TAB_MAP => ENTER,
        TAB_ELEVATION => UPDATE | MOUSE_MOVE,
        TAB_TERRAIN => UPDATE | LEAVE | ENTER | MOUSE_MOVE,
        TAB_UNITS => UPDATE | LEAVE | DRAW | ENTER | MOUSE_MOVE | UPDATE_KEYS,
        TAB_TRIGGERS => UPDATE | LEAVE | DRAW | DISABLE | LOAD | ENTER | STORE | ENABLE | MOUSE_MOVE | UPDATE_KEYS,
        TAB_PLAYERS => LEAVE | LOAD | ENTER | STORE,
        // (Leave was `this->Store()`)
        TAB_INSTRUCTIONS => LEAVE | LOAD | ENTER | STORE,
        _ => 0,
    };
    (0..16).contains(&slot) && mask & (1 << slot) != 0
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_vtables() {
        assert!(overrides(TAB_MAP, EE_SCREENS_SEFM_GROUP_ENTER));
        assert!(!overrides(TAB_MAP, EE_SCREENS_SEFM_GROUP_MOUSE_MOVE));
        assert!(overrides(TAB_ELEVATION, EE_SCREENS_SEFM_GROUP_MOUSE_MOVE));
        assert!(!overrides(TAB_ELEVATION, EE_SCREENS_SEFM_GROUP_ENTER));
        assert!(overrides(TAB_TERRAIN, EE_SCREENS_SEFM_GROUP_LEAVE));
        assert!(!overrides(TAB_TERRAIN, EE_SCREENS_SEFM_GROUP_DRAW));
        assert!(overrides(TAB_UNITS, EE_SCREENS_SEFM_GROUP_DRAW));
        assert!(!overrides(TAB_UNITS, EE_SCREENS_SEFM_GROUP_STORE));
        for slot in 0..10 {
            assert!(overrides(TAB_TRIGGERS, slot), "the trigger tab overrides every slot ({slot})");
        }
        assert!(overrides(TAB_PLAYERS, EE_SCREENS_SEFM_GROUP_STORE));
        assert!(!overrides(TAB_PLAYERS, EE_SCREENS_SEFM_GROUP_UPDATE));
        assert!(overrides(TAB_INSTRUCTIONS, EE_SCREENS_SEFM_GROUP_LEAVE));
        assert!(!overrides(TAB_INSTRUCTIONS, EE_SCREENS_SEFM_GROUP_MOUSE_MOVE));
        assert!(!overrides(7, EE_SCREENS_SEFM_GROUP_ENTER));
        assert!(!overrides(TAB_TRIGGERS, -1));
        assert!(!overrides(TAB_TRIGGERS, 40));
    }
}
