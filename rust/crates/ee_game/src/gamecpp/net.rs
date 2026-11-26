//! The network engine's Rust objects behind the game's communications objects (`NESynchronousCommunications` and
//! its `NESynchronousServer` / `NESynchronousClient`, include/lle): those classes keep only the three handles their
//! one-line members forward to (ee_net ffi_comms.rs, ffi_server.rs, ffi_client.rs), so the game's hosts call the
//! engine here by path instead of through a C++ case per member.

use std::ffi::c_void;

use ee_net::net::ffi_client::EeNetClient;
use ee_net::net::ffi_comms::{EeNetComms, EeNetCommsField};
use ee_net::net::ffi_server::{EeNetServer, EeNetSvGet, EeNetSvSet};

/// The handles of a communications object: its fields, and its server or client (the other one null).
#[derive(Clone, Copy)]
pub struct Net {
    pub comms: *const EeNetComms,
    pub server: *const EeNetServer,
    pub client: *const EeNetClient,
}

/// The handles of a live communications object (an `NESynchronousCommunications*`). Its members after the vtable
/// pointer are `mComms`, `mServer`, `mClient` (NESynchronousCommunications.h; the subclasses add no member before
/// them: single inheritance, the base first); the fixture's EETestCommsRefresh reads `mComms` the same way.
pub fn net(communications: *const c_void) -> Net {
    let at = communications as *const *const c_void;
    // SAFETY: the caller's live communications object: three pointers after its vtable pointer
    unsafe { Net { comms: (*at.add(1)).cast(), server: (*at.add(2)).cast(), client: (*at.add(3)).cast() } }
}

/// Whether `gCommunications` is set.
pub fn present() -> bool {
    !super::globals::get!(gCommunications).is_null()
}

/// The handles of `gCommunications` (null handles when there is none).
pub fn global() -> Net {
    let c = super::globals::get!(gCommunications) as *const c_void;
    if c.is_null() {
        return Net { comms: core::ptr::null(), server: core::ptr::null(), client: core::ptr::null() };
    }
    net(c)
}

impl Net {
    /// `IsClient`.
    pub fn is_client(self) -> bool {
        !self.client.is_null()
    }
    /// `NESynchronousCommunications`'s field getters (GetPlayerNumber, GetNumHumanPlayers, ...).
    pub fn get(self, f: EeNetCommsField) -> u32 {
        // SAFETY: the live object's fields
        unsafe { ee_net::net::ffi_comms::ee_net_comms_get(self.comms, f) }
    }
    /// The field setters (SetNumHumanPlayers, SetIsMultiplayerSession, ...).
    pub fn set(self, f: EeNetCommsField, value: u32) {
        // SAFETY: as get
        unsafe { ee_net::net::ffi_comms::ee_net_comms_set(self.comms, f, value) }
    }
    /// `IncrementNumberOfCTsProcessedInPhysics`.
    pub fn increment_cts(self) {
        // SAFETY: as get
        unsafe { ee_net::net::ffi_comms::ee_net_comms_increment_cts(self.comms) }
    }
    /// `NESynchronousServer`'s field getters (GetGameState, GetCurrentPhysicsTick, ...).
    pub fn sv_get(self, f: EeNetSvGet) -> u32 {
        ee_net::net::ffi_server::ee_net_sv_get(self.server, f)
    }
    /// The server's field setters (SetGameState, StartPing, StopPing, SetCurrentPhysicsTick, ...).
    pub fn sv_set(self, f: EeNetSvSet, value: u32) {
        ee_net::net::ffi_server::ee_net_sv_set(self.server, f, value)
    }
    /// `GetCurrentCommunicationsTick`: the server's or the client's.
    pub fn current_ct(self) -> u32 {
        if self.server.is_null() {
            ee_net::net::ffi_client::ee_net_cl_current_communications_tick(self.client)
        } else {
            self.sv_get(EeNetSvGet::CurrentCommunicationsTick)
        }
    }
    /// `SetCurrentCommunicationsTick`: the server's or the client's.
    pub fn set_current_ct(self, tick: u32) {
        if self.server.is_null() {
            ee_net::net::ffi_client::ee_net_cl_set_current_communications_tick(self.client, tick)
        } else {
            self.sv_set(EeNetSvSet::CurrentCommunicationsTick, tick)
        }
    }
    /// `ProcessLocalUserInput(message, player)`: the server's or the client's (not guaranteed).
    pub fn local_input(self, message: *mut c_void, player: u8) -> i32 {
        if self.server.is_null() {
            ee_net::net::ffi_client::ee_net_cl_process_user_input(self.client, message, player, false)
        } else {
            ee_net::net::ffi_server::ee_net_sv_process_local_user_input(self.server, message, player)
        }
    }
}
