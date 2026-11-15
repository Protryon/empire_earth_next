//! `UMath.cpp`'s floating-point control in Rust (LLE lane round 7): `UMGetFloatingPointPrecision`,
//! `UMSetFloatingPointPrecision`, `UMGetFloatingPointRoundingMode`, `UMSetFloatingPointRoundingMode`.
//!
//! The engine stores and compares float.h's `_RC_*` rounding values (0 nearest, 0x100 down, 0x200 up, 0x300
//! chop). Windows sets them with the C runtime's `_controlfp(mode, _MCW_RC)` as the original; elsewhere they map to
//! `<fenv.h>`'s modes (`fegetround`/`fesetround`, whose values depend on the architecture). Only the x87 has a
//! precision control: SSE and NEON compute doubles at 53 bits, the precision the game asks for, so the precision
//! reads as `_PC_53` and setting it does nothing (the CRT treats `_MCW_PC` on x64 as an invalid parameter).

use std::ffi::c_ulong;

/// float.h `_PC_53` (`kFPPrecision53`).
const PC_53: c_ulong = 0x10000;
/// float.h `_MCW_RC`: the rounding-control bits.
const MCW_RC: u32 = 0x300;

#[cfg(windows)]
mod imp {
    use super::MCW_RC;
    extern "C" {
        fn _controlfp(new: u32, mask: u32) -> u32;
    }
    pub fn rounding() -> u32 {
        // SAFETY: a mask of 0 only reads the control word
        unsafe { _controlfp(0, 0) & MCW_RC }
    }
    pub fn set_rounding(mode: u32) {
        // SAFETY: the rounding bits of the calling thread's control word
        unsafe { _controlfp(mode, MCW_RC) };
    }
}

#[cfg(not(windows))]
mod imp {
    use std::ffi::c_int;
    extern "C" {
        fn fegetround() -> c_int;
        fn fesetround(mode: c_int) -> c_int;
    }
    /// `<fenv.h>`'s FE_TONEAREST, FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO, in `_RC_*` order.
    #[cfg(target_arch = "x86_64")]
    const MODES: [c_int; 4] = [0, 0x400, 0x800, 0xc00];
    #[cfg(target_arch = "aarch64")]
    const MODES: [c_int; 4] = [0, 0x80_0000, 0x40_0000, 0xc0_0000];
    pub fn rounding() -> u32 {
        // SAFETY: reads the calling thread's mode
        let mode = unsafe { fegetround() };
        MODES.iter().position(|&m| m == mode).map_or(0, |i| (i as u32) << 8)
    }
    pub fn set_rounding(mode: u32) {
        // SAFETY: one of the four modes
        unsafe { fesetround(MODES[((mode & super::MCW_RC) >> 8) as usize]) };
    }
}

/// `UMGetFloatingPointPrecision`: `_PC_53`.
#[no_mangle]
pub extern "C" fn ee_util_fp_precision() -> c_ulong {
    PC_53
}

/// `UMSetFloatingPointPrecision`: nothing to set without an x87.
#[no_mangle]
pub extern "C" fn ee_util_fp_set_precision(_precision: c_ulong) {}

/// `UMGetFloatingPointRoundingMode`: the `_RC_*` value.
#[no_mangle]
pub extern "C" fn ee_util_fp_rounding() -> c_ulong {
    imp::rounding() as c_ulong
}

/// `UMSetFloatingPointRoundingMode`.
#[no_mangle]
pub extern "C" fn ee_util_fp_set_rounding(mode: c_ulong) {
    imp::set_rounding(mode as u32);
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn rounding_modes_round_trip() {
        assert_eq!(ee_util_fp_precision(), 0x10000);
        assert_eq!(ee_util_fp_rounding(), 0);
        for mode in [0x300, 0x100, 0x200, 0] {
            ee_util_fp_set_rounding(mode);
            assert_eq!(ee_util_fp_rounding(), mode);
        }
        ee_util_fp_set_rounding(0x100);
        let down = std::hint::black_box(1.0f32) / std::hint::black_box(3.0f32);
        ee_util_fp_set_rounding(0x200);
        let up = std::hint::black_box(1.0f32) / std::hint::black_box(3.0f32);
        ee_util_fp_set_rounding(0);
        assert!(down < up, "the mode reaches the arithmetic");
    }
}
