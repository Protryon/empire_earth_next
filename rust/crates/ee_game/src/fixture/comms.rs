//! TEST ONLY: the legacy debugger harness's communications mirror (was EETestFixture.cpp's `gEETestComms` and
//! `EETestCommsRefresh`). tools/harness/game_state_probe.py reads the numbers from the mirror by its map name: their
//! state is Rust's (ee_net comms.rs), at no offset of the C++ object. The presentation bridge
//! (tools/harness/presentation_bridge.c) refreshes it before each present through [`EETestCommsRefresh`].
#![allow(non_snake_case, non_upper_case_globals)]

/// The mirror (the probe's layout).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EETestComms {
    pub mLastTickReceived: i32,
    pub mCTsProcessed: u32,
    pub mPaused: u8,
    pub mMultiplayer: u8,
    /// 0: no communications object
    pub mValid: u8,
}

/// The mirror the probe reads.
#[no_mangle]
pub static mut gEETestComms: EETestComms =
    EETestComms { mLastTickReceived: 0, mCTsProcessed: 0, mPaused: 0, mMultiplayer: 0, mValid: 0 };

/// The communications' numbers into the mirror: `None` (no communications object) leaves only `mValid` 0.
pub fn fill(m: &mut EETestComms, fields: Option<[u32; 4]>) {
    match fields {
        None => m.mValid = 0,
        Some([last_tick, cts, paused, multiplayer]) => {
            m.mLastTickReceived = last_tick as i32;
            m.mCTsProcessed = cts;
            m.mPaused = u8::from(paused != 0);
            m.mMultiplayer = u8::from(multiplayer != 0);
            m.mValid = 1;
        }
    }
}

/// The render thread calls it, so it reads `gCommunications` only under the UI copy lock, taken without waiting (as
/// the bridge's ui_test takes it: a UI event can hold the lock while it waits for the render thread): the UI thread's
/// actions and events replace the communications object under that lock (EEAJoinOrHostAGame's delete and new when a
/// game is set up), and a read between the delete and the new (the legacy tour's crash here) used a freed object.
/// When the lock is busy the mirror keeps its last values until the next present.
#[no_mangle]
pub extern "C" fn EETestCommsRefresh() {
    #[cfg(not(test))]
    {
        use crate::gamecpp::globals::get;
        use ee_base::ts::{ee_ts_section_leave, ee_ts_section_try_enter, EeTsSection};
        use ee_net::net::ffi_comms::{ee_net_comms_get, EeNetComms, EeNetCommsField as F};
        let manager = get!(gUIManager);
        if manager.is_null() {
            return;
        }
        let lock = ee_ui::direct::manager_call(manager.cast(), ee_ui::managerobj::EeUiManagerCall::UiLock, 0, 0, 0)
            as *const EeTsSection;
        // SAFETY: the UI manager's live section
        if lock.is_null() || !unsafe { ee_ts_section_try_enter(lock) } {
            return;
        }
        let communications = get!(gCommunications) as *const u8;
        let fields = (!communications.is_null()).then(|| {
            // SAFETY: NESynchronousCommunications's first member after the vtable pointer, mComms (the Rust fields),
            // read under the lock that guards the object's replacement
            let comms = unsafe { *(communications.add(size_of::<usize>()) as *const *const EeNetComms) };
            // SAFETY: as above
            let get = |f| unsafe { ee_net_comms_get(comms, f) };
            [get(F::LastTickReceived), get(F::CtsProcessed), get(F::IsGamePaused), get(F::IsMultiplayerSession)]
        });
        // SAFETY: the mirror, written by the render thread only
        fill(unsafe { &mut *core::ptr::addr_of_mut!(gEETestComms) }, fields);
        // SAFETY: the section entered above
        unsafe { ee_ts_section_leave(lock) };
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_mirror_takes_the_fields_or_only_drops_its_valid_flag() {
        let mut m = EETestComms::default();
        fill(&mut m, Some([0xffff_fffe, 7, 1, 0]));
        assert_eq!(m, EETestComms { mLastTickReceived: -2, mCTsProcessed: 7, mPaused: 1, mMultiplayer: 0, mValid: 1 });
        fill(&mut m, None);
        assert_eq!((m.mValid, m.mCTsProcessed), (0, 7));
    }
}
