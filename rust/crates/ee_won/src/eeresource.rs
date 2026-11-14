//! The EE game lobby's resource strings (EELobbyResource.cpp, R6 WONLobby round 5): every game lobby string global,
//! the section and key of the lobby's resource files it is read from, in the order the original read them
//! (`EELobbyResource_Load`, which looked a section up where it changed) and emptied them (`EELobbyResource_Destroy`,
//! the same order). The 73 globals and their index table (`EELobbyResource_GetString`) stay C++: the lobby's code
//! uses them by name; the C++ runs the loads over these rows, the global of row `i` its table's entry `i`.
//! `golden_eeresource.txt` holds the original's statements, read from the C++ before the port.

use std::ffi::{c_char, CStr};

/// One row: the section, the key, the global's name.
#[derive(Clone, Copy, Debug)]
pub struct EeRow {
    pub section: &'static CStr,
    pub key: &'static CStr,
    pub name: &'static CStr,
}

const fn r(section: &'static CStr, key: &'static CStr, name: &'static CStr) -> EeRow {
    EeRow { section, key, name }
}

/// The rows, in the original's order (its globals' definition order, its table's and its loads').
pub static EE_RESOURCES: &[EeRow] = &[
    r(c"GameOptionsCtrl", c"Epoch1", c"gEELobbyGameOptionsCtrl_Epoch1"),
    r(c"GameOptionsCtrl", c"Epoch2", c"gEELobbyGameOptionsCtrl_Epoch2"),
    r(c"GameOptionsCtrl", c"Epoch3", c"gEELobbyGameOptionsCtrl_Epoch3"),
    r(c"GameOptionsCtrl", c"Epoch4", c"gEELobbyGameOptionsCtrl_Epoch4"),
    r(c"GameOptionsCtrl", c"Epoch5", c"gEELobbyGameOptionsCtrl_Epoch5"),
    r(c"GameOptionsCtrl", c"Epoch6", c"gEELobbyGameOptionsCtrl_Epoch6"),
    r(c"GameOptionsCtrl", c"Epoch7", c"gEELobbyGameOptionsCtrl_Epoch7"),
    r(c"GameOptionsCtrl", c"Epoch8", c"gEELobbyGameOptionsCtrl_Epoch8"),
    r(c"GameOptionsCtrl", c"Epoch9", c"gEELobbyGameOptionsCtrl_Epoch9"),
    r(c"GameOptionsCtrl", c"Epoch10", c"gEELobbyGameOptionsCtrl_Epoch10"),
    r(c"GameOptionsCtrl", c"Epoch11", c"gEELobbyGameOptionsCtrl_Epoch11"),
    r(c"GameOptionsCtrl", c"Epoch12", c"gEELobbyGameOptionsCtrl_Epoch12"),
    r(c"GameOptionsCtrl", c"Epoch13", c"gEELobbyGameOptionsCtrl_Epoch13"),
    r(c"GameOptionsCtrl", c"Epoch14", c"gEELobbyGameOptionsCtrl_Epoch14"),
    r(c"GameOptionsCtrl", c"Epoch15", c"gEELobbyGameOptionsCtrl_Epoch15"),
    r(c"GameOptionsCtrl", c"Epoch16", c"gEELobbyGameOptionsCtrl_Epoch16"),
    r(c"GameOptionsCtrl", c"GameType1", c"gEELobbyGameOptionsCtrl_GameType1"),
    r(c"GameOptionsCtrl", c"GameType2", c"gEELobbyGameOptionsCtrl_GameType2"),
    r(c"GameOptionsCtrl", c"GameType3", c"gEELobbyGameOptionsCtrl_GameType3"),
    r(c"GameOptionsCtrl", c"MapSize1", c"gEELobbyGameOptionsCtrl_MapSize1"),
    r(c"GameOptionsCtrl", c"MapSize2", c"gEELobbyGameOptionsCtrl_MapSize2"),
    r(c"GameOptionsCtrl", c"MapSize3", c"gEELobbyGameOptionsCtrl_MapSize3"),
    r(c"GameOptionsCtrl", c"MapSize4", c"gEELobbyGameOptionsCtrl_MapSize4"),
    r(c"GameOptionsCtrl", c"MapSize5", c"gEELobbyGameOptionsCtrl_MapSize5"),
    r(c"GameOptionsCtrl", c"MapSize6", c"gEELobbyGameOptionsCtrl_MapSize6"),
    r(c"GameOptionsCtrl", c"Resources1", c"gEELobbyGameOptionsCtrl_Resources1"),
    r(c"GameOptionsCtrl", c"Resources2", c"gEELobbyGameOptionsCtrl_Resources2"),
    r(c"GameOptionsCtrl", c"Resources3", c"gEELobbyGameOptionsCtrl_Resources3"),
    r(c"GameOptionsCtrl", c"Resources4", c"gEELobbyGameOptionsCtrl_Resources4"),
    r(c"GameOptionsCtrl", c"Resources5", c"gEELobbyGameOptionsCtrl_Resources5"),
    r(c"GameOptionsCtrl", c"GameVariant1", c"gEELobbyGameOptionsCtrl_GameVariant1"),
    r(c"GameOptionsCtrl", c"GameVariant2", c"gEELobbyGameOptionsCtrl_GameVariant2"),
    r(c"GameOptionsCtrl", c"Difficulty1", c"gEELobbyGameOptionsCtrl_Difficulty1"),
    r(c"GameOptionsCtrl", c"Difficulty2", c"gEELobbyGameOptionsCtrl_Difficulty2"),
    r(c"GameOptionsCtrl", c"Difficulty3", c"gEELobbyGameOptionsCtrl_Difficulty3"),
    r(c"GameOptionsCtrl", c"GameSpeed1", c"gEELobbyGameOptionsCtrl_GameSpeed1"),
    r(c"GameOptionsCtrl", c"GameSpeed2", c"gEELobbyGameOptionsCtrl_GameSpeed2"),
    r(c"GameOptionsCtrl", c"GameSpeed3", c"gEELobbyGameOptionsCtrl_GameSpeed3"),
    r(c"GameOptionsCtrl", c"GameSpeed4", c"gEELobbyGameOptionsCtrl_GameSpeed4"),
    r(c"GameOptionsCtrl", c"OptionOn", c"gEELobbyGameOptionsCtrl_OptionOn"),
    r(c"GameOptionsCtrl", c"OptionOff", c"gEELobbyGameOptionsCtrl_OptionOff"),
    r(c"GameOptionsCtrl", c"WondersOff", c"gEELobbyGameOptionsCtrl_WondersOff"),
    r(c"GameOptionsCtrl", c"GameType", c"gEELobbyGameOptionsCtrl_GameType"),
    r(c"GameOptionsCtrl", c"MapSize", c"gEELobbyGameOptionsCtrl_MapSize"),
    r(c"GameOptionsCtrl", c"MapType", c"gEELobbyGameOptionsCtrl_MapType"),
    r(c"GameOptionsCtrl", c"Resources", c"gEELobbyGameOptionsCtrl_Resources"),
    r(c"GameOptionsCtrl", c"StartingEpoch", c"gEELobbyGameOptionsCtrl_StartingEpoch"),
    r(c"GameOptionsCtrl", c"EndingEpoch", c"gEELobbyGameOptionsCtrl_EndingEpoch"),
    r(c"GameOptionsCtrl", c"GameUnitLimit", c"gEELobbyGameOptionsCtrl_GameUnitLimit"),
    r(c"GameOptionsCtrl", c"GameVariant", c"gEELobbyGameOptionsCtrl_GameVariant"),
    r(c"GameOptionsCtrl", c"DifficultyLevel", c"gEELobbyGameOptionsCtrl_DifficultyLevel"),
    r(c"GameOptionsCtrl", c"GameSpeed", c"gEELobbyGameOptionsCtrl_GameSpeed"),
    r(c"GameOptionsCtrl", c"RevealMap", c"gEELobbyGameOptionsCtrl_RevealMap"),
    r(c"GameOptionsCtrl", c"UseCustomCivs", c"gEELobbyGameOptionsCtrl_UseCustomCivs"),
    r(c"GameOptionsCtrl", c"LockTeams", c"gEELobbyGameOptionsCtrl_LockTeams"),
    r(c"GameOptionsCtrl", c"LockSpeed", c"gEELobbyGameOptionsCtrl_LockSpeed"),
    r(c"GameOptionsCtrl", c"VictoryAllowed", c"gEELobbyGameOptionsCtrl_VictoryAllowed"),
    r(c"GameOptionsCtrl", c"CampaignInformation", c"gEELobbyGameOptionsCtrl_CampaignInformation"),
    r(c"GameOptionsCtrl", c"NumberOfWonders", c"gEELobbyGameOptionsCtrl_NumberOfWonders"),
    r(c"PlayerOptionsCtrl", c"TransferPercent", c"gEELobbyPlayerOptionsCtrl_TransferPercent"),
    r(c"EELobbyConnectDialog", c"LeaveGame", c"gEELobbyConnectDialog_LeaveGame"),
    r(c"EELobbyConnectDialog", c"AbortLaunch", c"gEELobbyConnectDialog_AbortLaunch"),
    r(c"EELobbyFileDialog", c"DeleteTitle", c"gEELobbyFileDialog_DeleteTitle"),
    r(c"EELobbyFileDialog", c"DeleteDetails", c"gEELobbyFileDialog_DeleteDetails"),
    r(c"EELobbyGameStatus", c"NotInSavedGame", c"gEELobbyGameStatus_NotInSavedGame"),
    r(c"EELobbyGameStatus", c"InvalidVersion", c"gEELobbyGameStatus_InvalidVersion"),
    r(c"EELobbyGameStatus", c"CorruptBinaries", c"gEELobbyGameStatus_CorruptBinaries"),
    r(c"EELobbyInterface", c"LoadGameError", c"gEELobbyInterface_LoadGameError"),
    r(c"EELobbyInterface", c"ErrorReadingFile", c"gEELobbyInterface_ErrorReadingFile"),
    r(c"EELobbyInterface", c"PlayerNotInGame", c"gEELobbyInterface_PlayerNotInGame"),
    r(c"EELobbyInterface", c"Players", c"gEELobbyInterface_Players"),
    r(c"EELobbyGame", c"StartGameError", c"gEELobbyGame_StartGameError"),
    r(c"EELobbyGame", c"EveryoneOnSameTeam", c"gEELobbyGame_EveryoneOnSameTeam"),
];

/// The statements the table stands for, as the golden writes them: a section lookup where the section changes, a
/// read per row, then an empty string per row in the same order.
pub fn statements() -> Vec<String> {
    let mut out = Vec::new();
    let mut section: Option<&CStr> = None;
    for row in EE_RESOURCES {
        if section != Some(row.section) {
            out.push(format!("config {}", row.section.to_str().unwrap_or("")));
            section = Some(row.section);
        }
        out.push(format!("get {} -> {}", row.key.to_str().unwrap_or(""), row.name.to_str().unwrap_or("")));
    }
    for row in EE_RESOURCES {
        out.push(format!("erase {}", row.name.to_str().unwrap_or("")));
    }
    out
}

// ---- the C ABI

/// A row for the C++: NUL-terminated texts; `new_section` when the section differs from the previous row's.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeWonEeResource {
    pub section: *const c_char,
    pub key: *const c_char,
    pub name: *const c_char,
    pub new_section: bool,
}

/// The number of game lobby strings.
#[no_mangle]
pub extern "C" fn ee_won_eeresource_count() -> usize {
    EE_RESOURCES.len()
}

/// Row `index` (below `ee_won_eeresource_count`) into `out`; false past the end.
///
/// # Safety
/// `out` is writable.
#[no_mangle]
pub unsafe extern "C" fn ee_won_eeresource_row(index: usize, out: *mut EeWonEeResource) -> bool {
    let Some(row) = EE_RESOURCES.get(index) else {
        return false;
    };
    let new_section = index == 0 || EE_RESOURCES[index - 1].section != row.section;
    // SAFETY: writable (the contract); the texts are 'static
    unsafe {
        *out = EeWonEeResource {
            section: row.section.as_ptr(),
            key: row.key.as_ptr(),
            name: row.name.as_ptr(),
            new_section,
        }
    };
    true
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_table_is_the_originals_statements() {
        let golden: Vec<&str> = include_str!("golden_eeresource.txt").lines().collect();
        let got = statements();
        assert_eq!(got.len(), golden.len());
        for (k, (g, w)) in got.iter().zip(&golden).enumerate() {
            assert_eq!(g, w, "statement {k}");
        }
    }

    #[test]
    fn rows() {
        assert_eq!(ee_won_eeresource_count(), 73);
        let names: std::collections::BTreeSet<&CStr> = EE_RESOURCES.iter().map(|r| r.name).collect();
        assert_eq!(names.len(), EE_RESOURCES.len(), "each global once");
        // every global is its key's, after its section's prefix
        for row in EE_RESOURCES {
            let name = row.name.to_str().unwrap();
            assert!(name.starts_with("gEELobby") && name.ends_with(&format!("_{}", row.key.to_str().unwrap())));
        }
        let mut out = EeWonEeResource {
            section: std::ptr::null(),
            key: std::ptr::null(),
            name: std::ptr::null(),
            new_section: false,
        };
        // SAFETY: a writable row
        assert!(unsafe { ee_won_eeresource_row(0, &mut out) });
        assert!(out.new_section);
        // SAFETY: the row's static text
        assert_eq!(unsafe { CStr::from_ptr(out.name) }, c"gEELobbyGameOptionsCtrl_Epoch1");
        // SAFETY: a writable row
        assert!(unsafe { ee_won_eeresource_row(1, &mut out) });
        assert!(!out.new_section);
        // SAFETY: a writable row
        assert!(!unsafe { ee_won_eeresource_row(73, &mut out) });
    }
}
