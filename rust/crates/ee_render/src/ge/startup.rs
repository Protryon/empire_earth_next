//! The Graphics Engine's start-up (Graphics Engine.cpp's `GEInitialize` and `GEShutdown`, folded into
//! GERasterizer.cpp): the shutdown hook, the built-in fonts, the instance, and the T&L rasterizer added when it
//! activates and finds a screen (the original's `PIMIsValid`: activate, count the screens, deactivate). The calls on
//! the C++ objects are one-statement shims (GERasterizer.cpp, `ee_gecpp_*`).

use std::ffi::{c_long, c_ulong, c_void};

use crate::ge::rastobj::ee_ge_rast_list;

ee_base::cpp_shims! {
    trait GeStartCpp;
    fn fonts_register_builtin() -> bool = ee_gecpp_fonts_register_builtin;
    fn fonts_missing() = ee_gecpp_fonts_missing;
    fn set_instance(instance: *mut c_void) = ee_gecpp_set_instance;
    fn tnl_rasterizer() -> *mut c_void = ee_gecpp_tnl_rasterizer;
    fn activate(rasterizer: *mut c_void) -> c_long = ee_gecpp_rast_activate;
    fn screen_count(rasterizer: *mut c_void) -> c_ulong = ee_gecpp_rast_screen_count;
    fn deactivate(rasterizer: *mut c_void) = ee_gecpp_rast_deactivate;
}

/// The engine's own lists the start-up changes (Rust's: the shutdown list, the rasterizers).
pub trait GeStartLists {
    fn register_shutdown(&mut self, function: unsafe extern "C" fn(bool));
    /// `ee_ge_rast_list`: 0 clears, 1 adds.
    fn rasterizers(&mut self, op: u32, rasterizer: *mut c_void);
}

#[cfg(not(test))]
struct Lists;

#[cfg(not(test))]
impl GeStartLists for Lists {
    fn register_shutdown(&mut self, function: unsafe extern "C" fn(bool)) {
        ee_base::util::system::ee_util_register_shutdown(function);
    }
    fn rasterizers(&mut self, op: u32, rasterizer: *mut c_void) {
        ee_ge_rast_list(op, rasterizer, 0);
    }
}

/// `GEShutdown`: forgets the rasterizers.
unsafe extern "C" fn shutdown(_forced: bool) {
    ee_ge_rast_list(0, std::ptr::null_mut(), 0);
}

/// `GEInitialize`: always 0 (`kErrorOK`). A built-in font that does not register stops the game (ESVerify): its
/// fonts would fall back to another face, or with none have no platform font and a height of 0.
pub fn initialize<C: GeStartCpp, L: GeStartLists>(c: &mut C, lists: &mut L, instance: *mut c_void) -> c_long {
    lists.register_shutdown(shutdown);
    if !c.fonts_register_builtin() {
        c.fonts_missing();
    }
    c.set_instance(instance);
    lists.rasterizers(0, std::ptr::null_mut());
    let rasterizer = c.tnl_rasterizer();
    let valid = c.activate(rasterizer) == 0 && c.screen_count(rasterizer) > 0;
    c.deactivate(rasterizer);
    if valid {
        lists.rasterizers(1, rasterizer);
    }
    0
}

/// `GEInitialize(directory, instance)` (the directory is not read).
#[cfg(not(test))]
#[no_mangle]
pub extern "C" fn ee_ge_initialize(instance: *mut c_void) -> c_long {
    initialize(&mut ee_base::shim::ShimFfi, &mut Lists, instance)
}

#[cfg(test)]
mod tests {
    use super::*;
    use ee_base::shim::ShimFake;

    #[derive(Default)]
    struct FakeLists(Vec<String>);

    impl GeStartLists for FakeLists {
        fn register_shutdown(&mut self, _function: unsafe extern "C" fn(bool)) {
            self.0.push("shutdown".into());
        }
        fn rasterizers(&mut self, op: u32, rasterizer: *mut c_void) {
            self.0.push(format!("list({op}, {rasterizer:?})"));
        }
    }

    #[test]
    fn a_rasterizer_with_a_screen_is_added() {
        let mut c = ShimFake::default();
        c.answer("fonts_register_builtin", &[1]).answer("tnl_rasterizer", &[0x40]);
        c.answer("activate", &[0]).answer("screen_count", &[1]);
        let mut l = FakeLists::default();
        assert_eq!(initialize(&mut c, &mut l, 0x7 as *mut c_void), 0);
        assert_eq!(
            c.calls(),
            "fonts_register_builtin()\nset_instance(0x7)\ntnl_rasterizer()\nactivate(0x40)\nscreen_count(0x40)\n\
             deactivate(0x40)"
        );
        assert_eq!(l.0, ["shutdown", "list(0, 0x0)", "list(1, 0x40)"]);
    }

    #[test]
    fn a_failed_activation_counts_no_screens_and_adds_nothing() {
        let mut c = ShimFake::default();
        c.answer("fonts_register_builtin", &[0]).answer("tnl_rasterizer", &[0x40]).answer("activate", &[9]);
        let mut l = FakeLists::default();
        initialize(&mut c, &mut l, std::ptr::null_mut());
        assert_eq!(
            c.calls(),
            "fonts_register_builtin()\nfonts_missing()\nset_instance(0x0)\ntnl_rasterizer()\nactivate(0x40)\n\
             deactivate(0x40)"
        );
        assert_eq!(l.0, ["shutdown", "list(0, 0x0)"]);
        let mut c = ShimFake::default();
        c.answer("fonts_register_builtin", &[1]).answer("tnl_rasterizer", &[0x40]);
        c.answer("activate", &[0]).answer("screen_count", &[0]);
        let mut l = FakeLists::default();
        initialize(&mut c, &mut l, std::ptr::null_mut());
        assert_eq!(l.0, ["shutdown", "list(0, 0x0)"]);
    }
}
