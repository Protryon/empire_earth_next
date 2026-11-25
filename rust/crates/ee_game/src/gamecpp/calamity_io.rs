//! The calamities' C++ glue in Rust (were EECalamity.cpp's `EECalamityIoCall` and the Load/Save frames,
//! EECalamityUpdate.cpp's `EECalamityRun`, EEContagionManager.cpp's `ee_contagion_world`): the saved game's I/O
//! switch of a calamity's record, a class's run over its host (the class, the calamity, the two sounds, the searches'
//! vectors: a C++ `EECalamityUpdateHost` made and deleted around the run, the calamity's members in and out), and
//! the world under the Rust contagion manager. Each C++ call or member is a one-statement case of
//! EEContagionManager.cpp's `EECore11OpC5` ([`super::EeGameCpp`]'s `C11Cal` / `C11Cg` ops).

use std::ffi::c_void;
use std::os::raw::{c_long, c_ulong};

use ee_sim::tech::calamity_record::*;
use ee_sim::tech::calamity_update::EeCalamityParams;
use ee_sim::tech::contagion_save::*;
use ee_sim::tech::contagion_store::*;
use ee_sim::tech::ffi::{ee_calamity_start, ee_calamity_stop, ee_calamity_update, EeCalamityHostFfi};

use super::calamity::{ee_gamecpp_calamity_call, ee_gamecpp_calamity_set_state, ee_gamecpp_calamity_state};
use super::calamity_hosts::*;
use super::persistent::Pf;
use super::{h, hd, hf, hp, rec, Owned, O, P};

/// `kErrorOK`, `kErrorFailed` (lle_enums.h).
const ERROR_OK: c_long = 0;
const ERROR_FAILED: c_long = 9;

/// The I/O context of one calamity's Load or Save.
struct CalamityIo {
    calamity: P,
    file: Pf,
    loading: bool,
}

/// The loaded or the saved form of a member's Load / Save.
fn either(loading: bool, load: O, save: O) -> O {
    if loading {
        load
    } else {
        save
    }
}

/// The calamity record's I/O (ee_sim calamity_record.rs `EE_CALAMITY_IO_*`): the numbers through the FSFile overload
/// the original called, the members by part.
unsafe extern "C" fn calamity_io_call(ctx: *mut c_void, op: u32, value: *mut c_void) -> c_long {
    let io = &*(ctx as *const CalamityIo);
    let (c, f, l) = (io.calamity, io.file, io.loading);
    let file = f.0 as i64;
    match op {
        EE_CALAMITY_IO_READ_ULONG => f.read_ulong(value as *mut c_ulong) as c_long,
        EE_CALAMITY_IO_READ_LONG => f.read_long(value as *mut c_long) as c_long,
        EE_CALAMITY_IO_READ_FLOAT => f.read_f32(value as *mut f32) as c_long,
        EE_CALAMITY_IO_READ_BOOL => f.read_bool(value as *mut bool) as c_long,
        EE_CALAMITY_IO_WRITE_ULONG => f.write_ulong(*(value as *const c_ulong)) as c_long,
        EE_CALAMITY_IO_WRITE_LONG => f.write_long(*(value as *const c_long)) as c_long,
        EE_CALAMITY_IO_WRITE_FLOAT => f.write_f32(*(value as *const f32)) as c_long,
        EE_CALAMITY_IO_WRITE_BOOL => f.write_bool(*(value as *const bool)) as c_long,
        EE_CALAMITY_IO_RECTANGLE => h(either(l, O::C11CalRectLoad, O::C11CalRectSave), c, &[file]) as c_long,
        EE_CALAMITY_IO_TARGET_LOCATION => h(either(l, O::C11CalTargetLoad, O::C11CalTargetSave), c, &[file]) as c_long,
        EE_CALAMITY_IO_SHAKE_LOCATION => h(either(l, O::C11CalShakeLoad, O::C11CalShakeSave), c, &[file]) as c_long,
        EE_CALAMITY_IO_EFFECT_GENERATION => {
            h(either(l, O::C11CalGenerationLoad, O::C11CalGenerationSave), c, &[file]) as c_long
        }
        EE_CALAMITY_IO_CLASS => ee_gamecpp_calamity_io_class(c as *mut c_void, f.0 as *mut c_void, l),
        EE_CALAMITY_IO_TARGET_ID => {
            *(value as *mut c_ulong) = h(O::C11CalTargetId, c, &[]) as c_ulong;
            ERROR_OK
        }
        EE_CALAMITY_IO_CASTER_ID => {
            *(value as *mut c_ulong) = h(O::C11CalCasterId, c, &[]) as c_ulong;
            ERROR_OK
        }
        EE_CALAMITY_IO_TILE_X => ee_gamecpp_calamity_io_tile(c as *mut c_void, f.0 as *mut c_void, l, false),
        EE_CALAMITY_IO_TILE_Y => ee_gamecpp_calamity_io_tile(c as *mut c_void, f.0 as *mut c_void, l, true),
        EE_CALAMITY_IO_EFFECT_HANDLE => h(either(l, O::C11CalHandleRead, O::C11CalHandleWrite), c, &[file]) as c_long,
        _ => ERROR_FAILED,
    }
}

/// EECalamity::Load (`loading`) or Save: the calamity's record read from or written to the saved game (the class by
/// id; target and caster as ids, -1 if none); the first error.
///
/// # Safety
/// `calamity` and `file` are live.
#[no_mangle]
pub unsafe extern "C" fn ee_gamecpp_calamity_io(calamity: *mut c_void, file: *mut c_void, loading: bool) -> c_long {
    let c = calamity as P;
    let mut io = CalamityIo { calamity: c, file: Pf(file as P), loading };
    let ffi = EeCalamityIoFfi { ctx: &mut io as *mut CalamityIo as *mut c_void, call: Some(calamity_io_call) };
    let fields = rec::<EeCalamityFields>(O::CalFields, c);
    if loading {
        ee_calamity_record_load(fields, ffi)
    } else {
        ee_calamity_record_save(fields, ffi)
    }
}

/// A class's run (`which`: ee_sim calamity_class.rs `EE_CALAMITY_RUN_*`) over its host: the class, the calamity and
/// the two sounds in a new `EECalamityUpdateHost`, the calamity's members read before and written after.
pub fn class_run(which: u32, class: P, calamity: P, params: &EeCalamityParams, sounds: [P; 2], elapsed: f32) -> c_long {
    let host =
        Owned(hp(O::C11CalHostNew, class, &[calamity as i64, sounds[0] as i64, sounds[1] as i64]), O::C11CalHostDelete);
    // SAFETY: the live calamity; the host lives through the run
    unsafe {
        let mut state = ee_gamecpp_calamity_state(calamity as *mut c_void);
        let ffi = EeCalamityHostFfi { ctx: host.0 as *mut c_void, call: ee_gamecpp_calamity_call };
        let r = match which {
            ee_sim::tech::calamity_class::EE_CALAMITY_RUN_START => {
                ee_calamity_start(params, &mut state, &ffi) as c_long
            }
            ee_sim::tech::calamity_class::EE_CALAMITY_RUN_STOP => {
                ee_calamity_stop(params, &mut state, &ffi);
                0
            }
            _ => ee_calamity_update(params, &mut state, elapsed, &ffi) as c_long,
        };
        ee_gamecpp_calamity_set_state(&state, calamity as *mut c_void);
        r
    }
}

/// A float's bits as the world answers it.
fn float_bits(v: f32) -> i64 {
    v.to_bits() as i64
}

/// The contagion manager's file I/O (`EE_CONTAGION_IO_*` of the numbers).
///
/// # Safety
/// `value` points at the number of the op's type.
unsafe fn contagion_file(f: Pf, op: u64, value: *mut c_void) -> i64 {
    match op as u32 {
        EE_CONTAGION_IO_READ_ULONG => f.read_ulong(value as *mut c_ulong),
        EE_CONTAGION_IO_READ_LONG => f.read_long(value as *mut c_long),
        EE_CONTAGION_IO_READ_FLOAT => f.read_f32(value as *mut f32),
        EE_CONTAGION_IO_WRITE_ULONG => f.write_ulong(*(value as *const c_ulong)),
        EE_CONTAGION_IO_WRITE_LONG => f.write_long(*(value as *const c_long)),
        EE_CONTAGION_IO_WRITE_FLOAT => f.write_f32(*(value as *const f32)),
        _ => ERROR_FAILED as i64,
    }
}

/// The contagion world's export, out of the generated headers (ee_sim's tech.h declares it: the Rust store calls it).
/// cbindgen:ignore
mod world {
    use super::*;

    /// The world under the Rust contagion manager and helpers (ee_sim contagion_store.rs `EE_CONTAGION_WORLD_*`): each
    /// op the original helper's or manager's statement on the units, functors and calamities by address.
    ///
    /// # Safety
    /// The objects named are live; `io` is what the op says.
    #[no_mangle]
    pub unsafe extern "C" fn ee_contagion_world(op: u32, a: u64, b: u64, c: u64, x: f32, io: *mut c_void) -> i64 {
        let (pa, ia, ib, ic) = (a as P, a as i64, b as i64, c as i64);
        match op {
            EE_CONTAGION_WORLD_REFERENCE => ee_gamecpp_contagion_reference(a, b),
            EE_CONTAGION_WORLD_MARK => ee_gamecpp_contagion_mark(a, b, c),
            EE_CONTAGION_WORLD_INFECT => h(O::C11CgInfect, pa, &[ib, ic]),
            EE_CONTAGION_WORLD_CURE => h(O::C11CgCure, pa, &[ib, ic]),
            EE_CONTAGION_WORLD_APPLY => hf(O::C11CgApply, pa, &[ib, ic], &[x]),
            EE_CONTAGION_WORLD_FUNCTOR_UPDATE => hf(O::C11CgFunctorUpdate, pa, &[ic], &[x]),
            EE_CONTAGION_WORLD_NEAR => ee_gamecpp_contagion_near(a, x, io),
            EE_CONTAGION_WORLD_CAN_INFECT => h(O::C11CgCanInfect, pa, &[ib]),
            EE_CONTAGION_WORLD_IMMUNE => ee_gamecpp_contagion_immune(a),
            EE_CONTAGION_WORLD_RANDOM => float_bits(hd(O::C11CgRandom, 0, &[], &[])),
            EE_CONTAGION_WORLD_DISTANCE => float_bits(hd(O::C11CgDistance, pa, &[ib], &[])),
            EE_CONTAGION_WORLD_UNIT => h(O::C11CgUnit, 0, &[ia]),
            EE_CONTAGION_WORLD_CALAMITY_CHECKSUM => h(O::C11CgChecksum, pa, &[]),
            EE_CONTAGION_WORLD_CALAMITY_INDEX => ee_gamecpp_contagion_calamity_index(a),
            EE_CONTAGION_WORLD_CALAMITY_FUNCTOR => h(O::C11CgFunctor, pa, &[]),
            EE_CONTAGION_WORLD_CALAMITY_DUMP => h(O::C11CgDump, pa, &[ib]),
            EE_CONTAGION_WORLD_TEXT => h(O::C11CgText, b as P, &[io as i64]),
            EE_CONTAGION_WORLD_FILE => contagion_file(Pf(b as P), a, io),
            EE_CONTAGION_WORLD_CAPTURE => {
                // CONT (TEST ONLY)
                crate::gamecpp::calamity_capture::ee_gamecpp_capture_contagion_event(
                    a as i32 as c_long,
                    b as i32 as c_long,
                    c as i32 as c_long,
                    x,
                    *(io as *const f32),
                );
                0
            }
            _ => h(O::C11CgUnknown, 0, &[]),
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_float_bits_are_the_unsigned_word() {
        assert_eq!(float_bits(1.0), 0x3f80_0000);
        assert_eq!(float_bits(-0.0), 0x8000_0000);
        assert!(float_bits(-1.0) > 0);
    }

    #[test]
    fn the_member_io_picks_load_or_save() {
        assert_eq!(either(true, O::C11CalRectLoad, O::C11CalRectSave), O::C11CalRectLoad);
        assert_eq!(either(false, O::C11CalRectLoad, O::C11CalRectSave), O::C11CalRectSave);
    }

    #[test]
    fn an_unknown_calamity_io_op_fails_without_a_host_call() {
        let mut io = CalamityIo { calamity: 0, file: Pf(0), loading: true };
        let r = unsafe { calamity_io_call(&mut io as *mut CalamityIo as *mut c_void, 0xffff, std::ptr::null_mut()) };
        assert_eq!(r, ERROR_FAILED);
        assert_eq!(unsafe { contagion_file(Pf(0), 0xffff, std::ptr::null_mut()) }, 9);
    }
}
