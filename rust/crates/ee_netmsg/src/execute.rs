//! The game messages' `Execute` decisions (RUST_PLAN R5b.4 leaf 4): the tests and the arithmetic of
//! `EEMessage.cpp`'s bodies over plain values. The C++ keeps the objects, the locks and the world calls; it reads the
//! values, asks here, and applies the answer (ee/EEMessageExecute.h).
//!
//! - the selection id check (`EEMessage::ExecuteSelectionMessage`, `ExecuteMessage`) and the selection toggles'
//!   passes.

/// `EEMGameRestarting`: always runs.
pub const TYPE_GAME_RESTARTING: u8 = 0x91;
/// `EEMMigrationSlotInfo`: runs on a client whatever its selection id.
pub const TYPE_MIGRATION_SLOT_INFO: u8 = 0x7c;

/// `ExecuteSelectionMessage`: a client marks the world as executing the local selection while the local sender's
/// selection message runs (`gWorld.mExecutingLocalSelection`); the server does not.
pub fn selection_marks_local(is_server: bool, is_local: bool) -> bool {
    !is_server && is_local
}

/// `ExecuteSelectionMessage`, after `Execute`: the local sender's selected group copies its active group when the
/// message's selection id is the communications object's current one.
pub fn selection_copies(is_local: bool, message_id: u8, current_id: u8) -> bool {
    is_local && message_id == current_id
}

/// `ExecuteMessage`: a command runs when its selection id is the sender's last one; Game Restarting always, and on a
/// client Migration Slot Info too. Otherwise the sender's active group is reset to its selection.
pub fn command_runs(is_server: bool, message_type: u8, message_id: u8, last_id: u8) -> bool {
    if message_id == last_id || message_type == TYPE_GAME_RESTARTING {
        return true;
    }
    !is_server && message_type == TYPE_MIGRATION_SLOT_INFO
}

/// The selection toggles (`EEMAddUnitToSelectedGroup`, `EEMAddAndRemoveUnitsFromSelectedGroup`) run twice while the
/// communications object replays a selection.
pub fn selection_passes(executing_selection: bool) -> u32 {
    if executing_selection {
        2
    } else {
        1
    }
}

pub mod ffi {
    //! The C ABI (ee/EEMessageExecute.h's declarations; `ee_rust.h`).
    use super::*;

    #[no_mangle]
    pub extern "C" fn ee_netmsg_selection_marks_local(is_server: u8, is_local: u8) -> u8 {
        selection_marks_local(is_server != 0, is_local != 0) as u8
    }

    #[no_mangle]
    pub extern "C" fn ee_netmsg_selection_copies(is_local: u8, message_id: u8, current_id: u8) -> u8 {
        selection_copies(is_local != 0, message_id, current_id) as u8
    }

    #[no_mangle]
    pub extern "C" fn ee_netmsg_command_runs(is_server: u8, message_type: u8, message_id: u8, last_id: u8) -> u8 {
        command_runs(is_server != 0, message_type, message_id, last_id) as u8
    }

    #[no_mangle]
    pub extern "C" fn ee_netmsg_selection_passes(executing_selection: u8) -> u32 {
        selection_passes(executing_selection != 0)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_selection_check() {
        assert!(selection_marks_local(false, true));
        assert!(!selection_marks_local(true, true));
        assert!(!selection_marks_local(false, false));
        assert!(selection_copies(true, 7, 7));
        assert!(!selection_copies(true, 7, 8));
        assert!(!selection_copies(false, 7, 7));
        for server in [false, true] {
            for t in 0..=255u8 {
                assert!(command_runs(server, t, 3, 3));
                let late = command_runs(server, t, 3, 4);
                let want = t == 0x91 || (!server && t == 0x7c);
                assert_eq!(late, want, "server {server} type {t:#x}");
            }
        }
        assert_eq!(selection_passes(false), 1);
        assert_eq!(selection_passes(true), 2);
    }
}
