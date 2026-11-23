//! The game's plain global pointers and scalars, stored by Rust (were EEGlobals.cpp's definitions; the C++ headers
//! keep declaring them `extern`, so every C++ user reads the same symbol as before): the subsystem, manager, font
//! and render object pointers, the TEST ONLY hooks, the campaigns' network checksums, the install checksum, the
//! space units' heights, gFreeCamera, sIconsLoaded and gEmptyString. Each is exported under the C++ symbol: MSVC's
//! mangled name on Windows (from the link map), the plain name elsewhere (Itanium does not mangle a global's name).
//! Rust never reads or writes them; they are only storage. Types: a pointer is `*mut c_void` (the C++'s typed
//! pointer has the same size and alignment), `long` is `c_long` (32 bits on Windows, 64 on LP64), `bool` is `bool`.
#![allow(non_upper_case_globals)]

use core::ffi::{c_char, c_long, c_void};

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
    val gFreeCamera: bool = false => "?gFreeCamera@@3_NA";
    val sIconsLoaded: bool = false => "?sIconsLoaded@@3_NA";
    val gEmptyString: [c_char; 4] = [0; 4] => "?gEmptyString@@3PADA";
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn initial_values() {
        // the original's .data values (EELoadNetworkSettings' defaults, the space heights); the rest zero
        unsafe {
            assert_eq!(gAOCRomanChecksum, 1);
            assert_eq!(gEELearningChecksum, 1);
            assert_eq!(gBinaryChecksum, 0);
            assert_eq!(gSpaceTurretHeight, 0.7f32);
            assert_eq!(gSpaceStationHeight, 1.5f32);
            assert!(!gFreeCamera);
            assert_eq!(gEmptyString, [0; 4]);
            assert!((*core::ptr::addr_of!(gPhysics)).is_null());
            assert!((*core::ptr::addr_of!(gFontTahoma72)).is_null());
        }
    }

    #[test]
    fn symbols_are_msvc_names() {
        assert_eq!(NAMES.len(), 83);
        for (name, msvc) in NAMES {
            assert!(msvc.starts_with(&format!("?{name}@@3")), "{name}: {msvc}");
        }
        let mut names: Vec<_> = NAMES.iter().map(|n| n.0).collect();
        names.sort_unstable();
        names.dedup();
        assert_eq!(names.len(), NAMES.len());
    }
}
