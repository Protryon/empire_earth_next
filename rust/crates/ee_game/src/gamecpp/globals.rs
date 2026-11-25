//! The game's plain global pointers and scalars, stored by Rust (were EEGlobals.cpp's definitions; the C++ headers
//! keep declaring them `extern`, so every C++ user reads the same symbol as before): the subsystem, manager, font
//! and render object pointers, the TEST ONLY hooks, the campaigns' network checksums, the install checksum, the
//! space units' heights, the collision period and its unit (were EEPhysicsModel.cpp's), gFreeCamera, sIconsLoaded, gEmptyString, gLocalPlayerOut (was EEMessage.cpp's), the default player colours (EEServer.cpp's) and the render flags, the grid phase and the letterbox zoom (were EERenderObject.cpp's),
//! and the game's UI table (gEEGameUI: ee/EEGameUI.h; its members are function
//! pointers, so the storage is the C++ struct's on every platform; the UI's units fill it at static initialization). Each is exported under the C++ symbol: MSVC's
//! mangled name on Windows (from the link map), the plain name elsewhere (Itanium does not mangle a global's name).
//! Rust reads and writes them through `get!`, `have!` and `set!` where a host case only did that, and passes a
//! few pointers on to a Rust export (gDataPath, gGraphics, gPhysics, gDataPreLoading: reads on the thread that
//! owns the object). Types: a pointer is `*mut c_void` (the C++'s typed
//! pointer has the same size and alignment), `long` is `c_long` (32 bits on Windows, 64 on LP64), `bool` is `bool`.
#![allow(non_upper_case_globals)]

use core::ffi::{c_char, c_long, c_ulong, c_void};

/// A pointer global's storage: `*mut c_void` is not Sync, so the statics are `static mut` (C++ writes them).
macro_rules! ee_globals {
    ($(ptr $name:ident = $msvc:literal;)* $(val $vname:ident: $ty:ty = $init:expr => $vmsvc:literal;)*) => {
        $(
            #[cfg_attr(windows, export_name = $msvc)]
            #[cfg_attr(not(windows), no_mangle)]
            pub static mut $name: *mut c_void = core::ptr::null_mut();
        )*
        $(
            #[cfg_attr(windows, export_name = $vmsvc)]
            #[cfg_attr(not(windows), no_mangle)]
            pub static mut $vname: $ty = $init;
        )*
        /// The globals' names and their Windows symbols, for the tests.
        #[cfg(test)]
        pub(crate) const NAMES: &[(&str, &str)] = &[$((stringify!($name), $msvc),)* $((stringify!($vname), $vmsvc),)*];
    };
}

ee_globals! {
    ptr gTextureManager = "?gTextureManager@@3PEAVGETextureManager@@EA";
    ptr gModelManager = "?gModelManager@@3PEAVGEModelManager@@EA";
    ptr gU56535C = "?gU56535C@@3PEAVRTSRenderObject@@EA";
    ptr gU565358 = "?gU565358@@3PEAVRTSRenderObject@@EA";
    ptr gU565354 = "?gU565354@@3PEAVRTSRenderObject@@EA";
    ptr gU565350 = "?gU565350@@3PEAVRTSRenderObject@@EA";
    ptr gU565360 = "?gU565360@@3PEAVRTSRenderObject@@EA";
    ptr gFontArial8 = "?gFontArial8@@3PEAVGEFont@@EA";
    ptr gFontArial9 = "?gFontArial9@@3PEAVGEFont@@EA";
    ptr gU531930 = "?gU531930@@3PEAVGEFont@@EA";
    ptr gU531934 = "?gU531934@@3PEAVGEFont@@EA";
    ptr gU531938 = "?gU531938@@3PEAVGEFont@@EA";
    ptr gFontArial16 = "?gFontArial16@@3PEAVGEFont@@EA";
    ptr gFontArial18 = "?gFontArial18@@3PEAVGEFont@@EA";
    ptr gFontArial20 = "?gFontArial20@@3PEAVGEFont@@EA";
    ptr gFontArial24 = "?gFontArial24@@3PEAVGEFont@@EA";
    ptr gFontArial36 = "?gFontArial36@@3PEAVGEFont@@EA";
    ptr gFontArial48 = "?gFontArial48@@3PEAVGEFont@@EA";
    ptr gFontArial72 = "?gFontArial72@@3PEAVGEFont@@EA";
    ptr gFontArialBold8 = "?gFontArialBold8@@3PEAVGEFont@@EA";
    ptr gFontArialBold9 = "?gFontArialBold9@@3PEAVGEFont@@EA";
    ptr gFontArialBold10 = "?gFontArialBold10@@3PEAVGEFont@@EA";
    ptr gFontArialBold12 = "?gFontArialBold12@@3PEAVGEFont@@EA";
    ptr gFontArialBold14 = "?gFontArialBold14@@3PEAVGEFont@@EA";
    ptr gFontArialBold16 = "?gFontArialBold16@@3PEAVGEFont@@EA";
    ptr gFontTimesNewRoman9 = "?gFontTimesNewRoman9@@3PEAVGEFont@@EA";
    ptr gFontTimesNewRoman10 = "?gFontTimesNewRoman10@@3PEAVGEFont@@EA";
    ptr gFontTimesNewRoman12 = "?gFontTimesNewRoman12@@3PEAVGEFont@@EA";
    ptr gFontTimesNewRoman14 = "?gFontTimesNewRoman14@@3PEAVGEFont@@EA";
    ptr gFontCourier8 = "?gFontCourier8@@3PEAVGEFont@@EA";
    ptr gFontCourier10 = "?gFontCourier10@@3PEAVGEFont@@EA";
    ptr gFontCourier12 = "?gFontCourier12@@3PEAVGEFont@@EA";
    ptr gFontCourier14 = "?gFontCourier14@@3PEAVGEFont@@EA";
    ptr gFontTahoma8 = "?gFontTahoma8@@3PEAVGEFont@@EA";
    ptr gU531994 = "?gU531994@@3PEAVGEFont@@EA";
    ptr gFontTahoma10 = "?gFontTahoma10@@3PEAVGEFont@@EA";
    ptr gFontTahoma12 = "?gFontTahoma12@@3PEAVGEFont@@EA";
    ptr gFontTahoma14 = "?gFontTahoma14@@3PEAVGEFont@@EA";
    ptr gFontTahoma16 = "?gFontTahoma16@@3PEAVGEFont@@EA";
    ptr gFontTahoma18 = "?gFontTahoma18@@3PEAVGEFont@@EA";
    ptr gFontTahoma20 = "?gFontTahoma20@@3PEAVGEFont@@EA";
    ptr gFontTahoma24 = "?gFontTahoma24@@3PEAVGEFont@@EA";
    ptr gFontTahoma36 = "?gFontTahoma36@@3PEAVGEFont@@EA";
    ptr gFontTahoma48 = "?gFontTahoma48@@3PEAVGEFont@@EA";
    ptr gFontTahoma72 = "?gFontTahoma72@@3PEAVGEFont@@EA";
    ptr gSeplScreenFont = "?gSeplScreenFont@@3PEAVGEFont@@EA";
    ptr gConductor = "?gConductor@@3PEAVSEMixer@@EA";
    ptr gSoundManager = "?gSoundManager@@3PEAVSESoundManager@@EA";
    ptr gCommunications = "?gCommunications@@3PEAVNESynchronousCommunications@@EA";
    ptr gWonNet = "?gWonNet@@3PEAVEEWonNet@@EA";
    ptr gArtificialIntelligence = "?gArtificialIntelligence@@3PEAVEEArtificialIntelligence@@EA";
    ptr gCheatCodes = "?gCheatCodes@@3PEAVEECheatCodes@@EA";
    ptr gUIManager = "?gUIManager@@3PEAVEEUIManager@@EA";
    ptr gMouse = "?gMouse@@3PEAVEEMouse@@EA";
    ptr gMiniMap = "?gMiniMap@@3PEAVEEMiniMap@@EA";
    ptr gU56534C = "?gU56534C@@3PEAVRTSRenderObject@@EA";
    ptr gUserInterface = "?gUserInterface@@3PEAVEEUserInterface@@EA";
    ptr gMusicManager = "?gMusicManager@@3PEAVEEMusicManager@@EA";
    ptr gMousePointer = "?gMousePointer@@3PEAVRTSMousePointer@@EA";
    ptr gGraphics = "?gGraphics@@3PEAVEEGraphics@@EA";
    ptr gDataPreLoading = "?gDataPreLoading@@3PEAVEEDataPreLoading@@EA";
    ptr gPhysics = "?gPhysics@@3PEAVEEPhysics@@EA";
    ptr gDataPath = "?gDataPath@@3PEAVFSPath@@EA";
    ptr gCivilizationDataPath = "?gCivilizationDataPath@@3PEAVFSPath@@EA";
    ptr gRasterizer = "?gRasterizer@@3PEAVGERasterizer@@EA";
    ptr gGameWindow = "?gGameWindow@@3PEAVGEWindow@@EA";
    ptr gResource = "?gResource@@3PEAVUResourceFile@@EA";
    ptr gPhysicsSteppingUnit = "?gPhysicsSteppingUnit@@3PEAVEEComplexUnit@@EA";
    ptr gEETestFixtureConfigure = "?gEETestFixtureConfigure@@3P6AXXZEA";
    ptr gPhysicsAfterTickHook = "?gPhysicsAfterTickHook@@3P6AXXZEA";
    ptr gEEMessageExecutedTestHook = "?gEEMessageExecutedTestHook@@3P6AXPEAVNEMessage@@H@ZEA";
    ptr gEERenderCaptureFill = "?gEERenderCaptureFill@@3P6AXAEBUEERenderFrameInfo@@IM@ZEA";
    val gAOCRomanChecksum: c_long = 1 => "?gAOCRomanChecksum@@3JA";
    val gAOCPacificChecksum: c_long = 1 => "?gAOCPacificChecksum@@3JA";
    val gAOCAsianChecksum: c_long = 1 => "?gAOCAsianChecksum@@3JA";
    val gEEDemoChecksum: c_long = 1 => "?gEEDemoChecksum@@3JA";
    val gEELearningChecksum: c_long = 1 => "?gEELearningChecksum@@3JA";
    val gBinaryChecksum: u32 = 0 => "?gBinaryChecksum@@3IA";
    val gSpaceTurretHeight: f32 = 0.7 => "?gSpaceTurretHeight@@3MA";
    val gSpaceStationHeight: f32 = 1.5 => "?gSpaceStationHeight@@3MA";
    val gCollisionDetectionUnit: c_ulong = 2000 => "?gCollisionDetectionUnit@@3KA";
    val gCollisionPeriod: c_ulong = 1 => "?gCollisionPeriod@@3KA";
    val gFreeCamera: bool = false => "?gFreeCamera@@3_NA";
    val sIconsLoaded: bool = false => "?sIconsLoaded@@3_NA";
    val gEmptyString: [c_char; 4] = [0; 4] => "?gEmptyString@@3PADA";
    // port: the farthest zoom (1.0 in the original; the camera's distance grows linearly past it, its pitch stays at
    // the farthest), from config.yaml's camera: maximum_zoom_out (EEPreferences; was EECameraControl.cpp's)
    val gCameraMaxZoom: f32 = 2.0 => "?gCameraMaxZoom@@3MA";
    val gEEGameUI: [*mut c_void; 9] = [core::ptr::null_mut(); 9] => "?gEEGameUI@@3UEEGameUI@@A";
    val gLocalPlayerOut: bool = false => "?gLocalPlayerOut@@3_NA";
    val gB529065: bool = false => "?gB529065@@3_NA";
    val gB529066: bool = false => "?gB529066@@3_NA";
    val gB529068: bool = false => "?gB529068@@3_NA";
    val gCameraFollowing: bool = false => "?gCameraFollowing@@3_NA";
    val gU4E9330: f32 = 0.0 => "?gU4E9330@@3MA";
    val gF4BE374: f32 = 1.0 => "?gF4BE374@@3MA";
    val gDefaultPlayerColors: [c_long; 16] = [0, 1, 4, 9, 2, 15, 5, 6, 8, 9, 10, 11, 12, 13, 14, 15]
        => "?gDefaultPlayerColors@@3PAJA";
}

/// `gGraphics->PostResume()` (rts graphics.rs; its FFI is absent from a test binary).
pub(crate) fn graphics_post_resume() {
    // SAFETY: the game's graphics thread object
    #[cfg(not(test))]
    unsafe {
        crate::rts::graphics::ee_rts_graphics_post_resume(*core::ptr::addr_of!(gGraphics))
    };
}

/// `gGraphics->PostPause()`.
pub(crate) fn graphics_post_pause() {
    // SAFETY: the game's graphics thread object
    #[cfg(not(test))]
    unsafe {
        crate::rts::graphics::ee_rts_graphics_post_pause(*core::ptr::addr_of!(gGraphics))
    };
}

/// A global's value, read at once (as the C++ statement `gX` read it: on the thread that owns the subsystem).
macro_rules! get {
    ($g:ident) => {
        // SAFETY: a plain load of the global's storage; C++ writes it on the same threads as before
        unsafe { *core::ptr::addr_of!($crate::gamecpp::globals::$g) }
    };
}
pub(crate) use get;

/// `gX != NULL`: whether a pointer global is set.
macro_rules! have {
    ($g:ident) => {
        !$crate::gamecpp::globals::get!($g).is_null()
    };
}
pub(crate) use have;

/// `gX = value`.
macro_rules! set {
    ($g:ident, $value:expr) => {
        // SAFETY: a plain store to the global's storage, on the thread the C++ statement ran on
        unsafe { *core::ptr::addr_of_mut!($crate::gamecpp::globals::$g) = $value }
    };
}
pub(crate) use set;

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_macros_read_and_write_the_storage() {
        assert!(!have!(gFontTahoma72));
        assert!(!get!(gFreeCamera));
        set!(gU531930, 8 as *mut c_void);
        assert!(have!(gU531930));
        set!(gU531930, core::ptr::null_mut());
        assert!(!have!(gU531930));
    }

    #[test]
    fn initial_values() {
        // the original's .data values (EELoadNetworkSettings' defaults, the space heights); the rest zero
        // SAFETY: reads of the statics (no writer in the tests)
        unsafe {
            use core::ptr::addr_of;
            assert_eq!(*addr_of!(gAOCRomanChecksum), 1);
            assert_eq!(*addr_of!(gEELearningChecksum), 1);
            assert_eq!(*addr_of!(gBinaryChecksum), 0);
            assert_eq!(*addr_of!(gSpaceTurretHeight), 0.7f32);
            assert_eq!(*addr_of!(gSpaceStationHeight), 1.5f32);
            assert!(!*addr_of!(gFreeCamera));
            assert_eq!(*addr_of!(gEmptyString), [0; 4]);
            assert_eq!(*addr_of!(gF4BE374), 1.0f32);
            let colors = *addr_of!(gDefaultPlayerColors);
            assert_eq!(colors[..6], [0, 1, 4, 9, 2, 15]);
            assert!(!*addr_of!(gCameraFollowing) && !*addr_of!(gLocalPlayerOut));
            assert!((*addr_of!(gPhysics)).is_null());
            assert!((*addr_of!(gFontTahoma72)).is_null());
        }
    }

    #[test]
    fn symbols_are_msvc_names() {
        assert_eq!(NAMES.len(), 96);
        for (name, msvc) in NAMES {
            assert!(msvc.starts_with(&format!("?{name}@@3")), "{name}: {msvc}");
        }
        let mut names: Vec<_> = NAMES.iter().map(|n| n.0).collect();
        names.sort_unstable();
        names.dedup();
        assert_eq!(names.len(), NAMES.len());
    }
}
