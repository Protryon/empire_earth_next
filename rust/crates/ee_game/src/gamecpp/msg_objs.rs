//! The game messages' factory and the box selection's record (were EEMessage.cpp's `EEMessage::BuildEEMessage`,
//! `EENewSelectUnits`, `EESelectUnitsIDs`): the received buffer's record by its type byte, the local player's picked
//! units compressed into a new selection record (ee_sim netmsg units.rs), a selection's ids decoded, and the farm
//! sorts' orders (EETileDistanceLess's and EEFarmDistanceGreater's over complex units). Each C++ call
//! or member is a one-statement case of EEMessage.cpp's `EECore11OpC4` ([`super::EeGameCpp`]'s `C11Msg` ops).

use std::ffi::{c_long, c_void};

use ee_sim::netmsg::ffi::{ee_netmsg_class_of_type, ee_netmsg_compress_units, ee_netmsg_decompress_units};
use ee_sim::netmsg::records::ee_netmsg_record_class;

use super::{cptr, h, hp, pa, Owned, O, P};

/// The capacity of a selection's decoded ids (the C++'s `int32_t theIDs[0x100]`).
const SELECT_IDS: usize = 0x100;

/// Whether the factory builds a record for a type: a record type of a game class (not EEMEndCinematic's 0x9d).
pub fn builds_record(record_class: u32, message_class: u32) -> bool {
    record_class != u32::MAX && message_class != u32::MAX
}

/// The ids of a selection's compact buffer (at most 256).
///
/// # Safety
/// `data` holds `size` bytes (NULL when 0).
unsafe fn select_ids(data: *const u8, size: usize) -> Vec<i32> {
    let mut buffer = [0u8; 0x100];
    if size != 0 {
        std::ptr::copy_nonoverlapping(data, buffer.as_mut_ptr(), size.min(0x100));
    }
    let mut ids = [0i32; SELECT_IDS];
    let n = ee_netmsg_decompress_units(buffer.as_ptr(), size as u8, ids.as_mut_ptr(), SELECT_IDS);
    ids[..n.min(SELECT_IDS)].to_vec()
}

/// EEMessage::BuildEEMessage: the game message object of a received buffer from its type byte (a record; NULL and
/// an assert for an unknown type), then the capture's NMIM record (TEST ONLY).
///
/// # Safety
/// `memory` is a received message buffer (its type at byte 1).
#[no_mangle]
pub unsafe extern "C" fn ee_gamecpp_msg_build(memory: *mut c_void) -> *mut c_void {
    let m = memory as P;
    let kind = *(memory as *const u8).add(1);
    h(O::C11MsgBuildVerify, m, &[]);
    let message = if builds_record(ee_netmsg_record_class(kind), ee_netmsg_class_of_type(kind)) {
        hp(O::C11MsgNewRecord, m, &[])
    } else {
        h(O::C11MsgBuildUndefined, m, &[]);
        0
    };
    // TEST ONLY: NMIM
    super::msg_capture::ee_gamecpp_msgcap_import(message as *mut c_void, memory as *const u8);
    message as *mut c_void
}

/// EENewSelectUnits: the local player's units of a type in the rectangle picked and stored compressed (the count,
/// the blob), the rectangle and the type beside (members 3 and 4, off the wire); the UI fixture's hook sees the ids.
///
/// # Safety
/// `rectangle` is a live GERectangle.
#[no_mangle]
pub unsafe extern "C" fn ee_gamecpp_msg_select_units(
    rectangle: *const c_void,
    unit_type: c_long,
    kind: u8,
) -> *mut c_void {
    let picked = Owned::new(O::C11UlongsNew, O::C11UlongsDelete);
    h(O::C11PickUnits, picked.0, &[pa(rectangle as P), unit_type as i64]);
    // only the low 32 (24) bits count
    let ids: Vec<u32> =
        (0..h(O::C11UlongsCount, picked.0, &[])).map(|i| h(O::C11UlongsAt, picked.0, &[i]) as u32).collect();
    let owner = super::message::prts(hp(O::WorldCurrentPlayer, 0, &[])).mPlayerID as u32;
    let mut buffer = [0u8; 0x100];
    let size = ee_netmsg_compress_units(ids.as_ptr(), ids.len(), owner, buffer.as_mut_ptr());
    // the list's size, 0, 0, the unit type (the C++'s `(unsigned char)`, `(long)`)
    let values =
        [size as u8 as i64, 0, 0, unit_type as std::ffi::c_long as i64].map(super::mrecord::EeGamecppMrValue::integer);
    let message = super::mrecord::new_record(kind as u8, &values);
    h(O::C11MsgListAssign, message, &[cptr(&buffer), size as i64]);
    h(O::C11MsgSetRectangle, message, &[pa(rectangle as P)]);
    if crate::fixture::boxsel::LASSO_ON.load(std::sync::atomic::Ordering::SeqCst) {
        // TEST ONLY: the UI fixture's box selection (the message's list, as EESelectUnitsIDs decodes it)
        let ids: Vec<i64> = select_ids(buffer.as_ptr(), size as usize).into_iter().map(i64::from).collect();
        crate::fixture::boxsel::lasso(&ids);
    }
    message as *mut c_void
}

/// EESelectUnitsIDs: a box selection's compact buffer decoded into unit ids, replacing the list (a C++
/// `std::vector<long>`: each the 32-bit value, sign-extended on LP64).
///
/// # Safety
/// `message` is a live record, `out` a live `std::vector<long>`.
#[no_mangle]
pub unsafe extern "C" fn ee_gamecpp_msg_select_ids(message: *const c_void, out: *mut c_void) {
    let m = message as P;
    let data = hp(O::C11MsgListData, m, &[]) as *const u8;
    let size = h(O::C11MsgListSize, m, &[]) as u8 as usize;
    let ids = select_ids(data, size);
    h(O::C11LongsAssign, out as P, &[pa(ids.as_ptr() as P), ids.len() as i64]);
}

/// A farm's sort key: built, paid for, its tile (EEMessage.cpp's `C11FarmKey` fills it in one statement).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeGamecppFarmKey {
    pub built: bool,
    pub paid: bool,
    pub tile_x: c_long,
    pub tile_y: c_long,
}

/// EETileDistanceLess::DistanceSquared: the tile's squared distance from the center (the C++'s `long`s).
pub fn distance_squared(cx: c_long, cy: c_long, k: &EeGamecppFarmKey) -> c_long {
    let dy = cy.wrapping_sub(k.tile_y);
    let dx = cx.wrapping_sub(k.tile_x);
    dx.wrapping_mul(dx).wrapping_add(dy.wrapping_mul(dy))
}

/// The farms' order: built before paid-for before the rest, the nearer first in a tier (EEMBuildUnit's sort,
/// EETileDistanceLess); `greater` the smart farmers' (EEFarmDistanceGreater): unpaid before paid-for before built, the
/// farther first in a tier.
pub fn farm_order(cx: c_long, cy: c_long, a: &EeGamecppFarmKey, b: &EeGamecppFarmKey, greater: bool) -> bool {
    let (da, db) = (distance_squared(cx, cy, a), distance_squared(cx, cy, b));
    let near = if greater { da > db } else { da < db };
    if a.built {
        return b.built && near || !greater && !b.built;
    }
    if b.built {
        return greater;
    }
    if a.paid {
        return b.paid && near || !greater && !b.paid;
    }
    if b.paid {
        return greater;
    }
    near
}

/// The farm sorts' comparison of two complex units (the comparators' bodies).
///
/// # Safety
/// `first` and `second` are live complex units.
#[no_mangle]
pub unsafe extern "C" fn ee_gamecpp_farm_order(
    cx: c_long,
    cy: c_long,
    first: *mut c_void,
    second: *mut c_void,
    greater: bool,
) -> bool {
    let (a, b) = (farm_key(first as P), farm_key(second as P));
    ee_gamecpp_farm_key_order(cx, cy, &a, &b, greater)
}

/// A farm's key: its built and paid-for flags and its tile (its records' members).
fn farm_key(farm: P) -> EeGamecppFarmKey {
    let (cx, rts) = (super::message::cf(farm), super::message::rts(farm));
    EeGamecppFarmKey { built: cx.mIsBuilt, paid: cx.mIsPaidFor, tile_x: rts.mTileX, tile_y: rts.mTileY }
}

/// The farm order of two keys (exported: the header declares the key).
#[no_mangle]
pub extern "C" fn ee_gamecpp_farm_key_order(
    cx: c_long,
    cy: c_long,
    a: &EeGamecppFarmKey,
    b: &EeGamecppFarmKey,
    greater: bool,
) -> bool {
    farm_order(cx, cy, a, b, greater)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn only_a_game_record_type_is_built() {
        assert!(builds_record(1, 2));
        assert!(!builds_record(u32::MAX, 2));
        assert!(!builds_record(1, u32::MAX));
    }

    #[test]
    fn the_ids_round_trip_through_the_compact_buffer() {
        let ids = [0x0102_0304u32 & 0xff_ffff, 0x0102_0305 & 0xff_ffff, 0x0102_0400 & 0xff_ffff];
        let mut buffer = [0u8; 0x100];
        let size = unsafe { ee_netmsg_compress_units(ids.as_ptr(), ids.len(), 2, buffer.as_mut_ptr()) };
        let back = unsafe { select_ids(buffer.as_ptr(), size as usize) };
        assert_eq!(back.len(), 3);
        assert!(back.iter().zip(&ids).all(|(&b, &i)| b as u32 & 0xff_ffff == i));
        assert!(unsafe { select_ids(std::ptr::null(), 0) }.is_empty());
    }

    fn key(built: bool, paid: bool, x: c_long, y: c_long) -> EeGamecppFarmKey {
        EeGamecppFarmKey { built, paid, tile_x: x, tile_y: y }
    }

    /// The C++ comparators as they were written (EETileDistanceLess's, EEFarmDistanceGreater's).
    fn less_cpp(a: &EeGamecppFarmKey, b: &EeGamecppFarmKey) -> bool {
        let d = |k| distance_squared(0, 0, k);
        if a.built {
            if !b.built {
                return true;
            }
            return d(a) < d(b);
        }
        if b.built {
            return false;
        }
        if a.paid {
            if !b.paid {
                return true;
            }
            return d(a) < d(b);
        }
        if b.paid {
            return false;
        }
        d(a) < d(b)
    }
    fn greater_cpp(a: &EeGamecppFarmKey, b: &EeGamecppFarmKey) -> bool {
        let d = |k| distance_squared(0, 0, k);
        if a.built {
            if b.built {
                d(a) > d(b)
            } else {
                false
            }
        } else if b.built {
            true
        } else if a.paid {
            if b.paid {
                d(a) > d(b)
            } else {
                false
            }
        } else if b.paid {
            true
        } else {
            d(a) > d(b)
        }
    }

    #[test]
    fn the_farm_orders_are_the_comparators() {
        let mut keys = Vec::new();
        for built in [false, true] {
            for paid in [false, true] {
                for (x, y) in [(0, 0), (1, 2), (3, 0), (2, 1)] {
                    keys.push(key(built, paid, x, y));
                }
            }
        }
        for a in &keys {
            for b in &keys {
                assert_eq!(farm_order(0, 0, a, b, false), less_cpp(a, b), "{a:?} {b:?}");
                assert_eq!(farm_order(0, 0, a, b, true), greater_cpp(a, b), "{a:?} {b:?}");
            }
        }
        assert_eq!(distance_squared(5, 7, &key(false, false, 2, 3)), 25);
    }
}
