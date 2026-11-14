//! The C ABI of NESynchronousCommunications' state (`comms.rs`): the C++ base class creates an `EeNetComms` in its
//! constructor, hands it to the Rust server or client its subclass makes (which keeps its own reference) and forwards
//! its accessors here.

use std::sync::Arc;

use crate::comms::Comms;

/// The base class's fields
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeNetCommsField {
    /// mLastTickReceived (a `long`: -2 before the connection, -1 "none"; its 32 bits)
    LastTickReceived = 0,
    /// mPlayerNumber
    PlayerNumber = 1,
    /// mNumberOfCTsProcessedInPhysics
    CtsProcessed = 2,
    /// mAveragePhysicsUpdateTime
    AveragePhysicsUpdateTime = 3,
    /// mAverageTimeBetweenPhysicsTicks
    AverageTimeBetweenPhysicsTicks = 4,
    /// mNumHumanPlayers (a byte)
    NumHumanPlayers = 5,
    /// mIsGamePaused
    IsGamePaused = 6,
    /// mIsMultiplayerSession
    IsMultiplayerSession = 7,
}

/// NESynchronousCommunications' state, shared by the C++ base class and the Rust server or client
pub struct EeNetComms(pub Arc<Comms>);

/// The base class's constructor: the CTs processed given, the rest as the original's.
#[no_mangle]
pub extern "C" fn ee_net_comms_new(cts_processed: u32) -> *mut EeNetComms {
    Box::into_raw(Box::new(EeNetComms(Arc::new(Comms::new(cts_processed)))))
}

/// The base class's destructor (the Rust server or client keeps the state while it lives).
///
/// # Safety
/// `c` from `ee_net_comms_new` (or NULL), used by nothing after.
#[no_mangle]
pub unsafe extern "C" fn ee_net_comms_free(c: *mut EeNetComms) {
    if !c.is_null() {
        drop(Box::from_raw(c));
    }
}

/// A field's value (the last tick received as its 32 bits, a flag 0 or 1).
///
/// # Safety
/// `c` from `ee_net_comms_new`, not freed.
#[no_mangle]
pub unsafe extern "C" fn ee_net_comms_get(c: *const EeNetComms, f: EeNetCommsField) -> u32 {
    (*c).0.get(f)
}

/// A field set.
///
/// # Safety
/// `c` from `ee_net_comms_new`, not freed.
#[no_mangle]
pub unsafe extern "C" fn ee_net_comms_set(c: *const EeNetComms, f: EeNetCommsField, value: u32) {
    (*c).0.set(f, value)
}

/// IncrementNumberOfCTsProcessedInPhysics.
///
/// # Safety
/// `c` from `ee_net_comms_new`, not freed.
#[no_mangle]
pub unsafe extern "C" fn ee_net_comms_increment_cts(c: *const EeNetComms) {
    (*c).0.increment_cts_processed()
}

/// The shared state of a C++ base class (the Rust server's or client's reference).
///
/// # Safety
/// `c` from `ee_net_comms_new`, not freed.
pub unsafe fn comms_of(c: *const EeNetComms) -> Arc<Comms> {
    (*c).0.clone()
}
