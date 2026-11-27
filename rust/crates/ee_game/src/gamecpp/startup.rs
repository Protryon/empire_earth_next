//! The game's start-up and shutdown wiring (was Empire Earth.cpp): the engine's initialisations in order (the display,
//! the graphics and their fonts, the sound, the network and its checksum, the database tables, the game objects, the
//! threads), the screen changes (the video mode, the menus' screen, a new screen size), the game's end and the
//! shutdown routines. Each C++ call or member is a case of Empire Earth.cpp's `EEStartupOp` ([`super::EeGameCpp`]);
//! the C++ functions forward here.
//!
//! Only the game's start (the tour, gates.sh) runs most of this; EE-SIM runs the same start-up without a window.

#![allow(non_snake_case)]
// the C++'s own casts and tests, kept as written
#![allow(clippy::unnecessary_cast, clippy::bool_comparison, clippy::nonminimal_bool)]

use std::ffi::{c_char, c_long, c_ulong, c_void, CStr};
use std::sync::atomic::{AtomicI64, AtomicUsize, Ordering};

use ee_base::math::types::GeRectangle;
use ee_base::util::fp;

use super::{cptr, h, hd, hf, hp, pa, ptr, Owned, O, P};

/// `keseOK`.
const KESE_OK: i64 = 0;
/// float.h `_PC_53`: the FPU's 53-bit precision.
const PRECISION_53: c_ulong = ee_base::lle_enums::UMFloatingPointPrecision::kFPPrecision53 as c_ulong;
/// `kPixelFormatZPrefer16` (Empire Earth.cpp static_asserts it).
const PIXEL_FORMAT_Z_PREFER_16: i64 = 12;
/// The start-up's game view (in 1600 x 1200 layout units; .rdata 0x442090..0x4420a8) and the water ripple.
const START_VIEW: GeRectangle = GeRectangle { bottom: 1085.0, left: 0.0, right: 1600.0, top: 35.0 };
const START_WATER_RIPPLE: [f32; 3] = [3.1415927, 0.025, 0.05];
/// The game view's layout units -> the window (0x44217c, 0x4413bc).
const GAME_VIEW_SCALE_X: f32 = 0.000625;
const GAME_VIEW_SCALE_Y: f32 = 0.00083333335;
/// The font families' resource names (`IDS_ARIAL`, `IDS_TIMES_NEW_ROMAN`, `IDS_COURIER_NEW`, `IDS_TAHOMA`): the
/// index is the family in [`FONTS`]. Empire Earth.cpp static_asserts them.
const FONT_FAMILY_NAMES: [i64; 4] = [IDS_ARIAL, IDS_TIMES_NEW_ROMAN, IDS_COURIER_NEW, IDS_TAHOMA];
const IDS_ARIAL: i64 = 1;
const IDS_TIMES_NEW_ROMAN: i64 = 2;
const IDS_COURIER_NEW: i64 = 3;
const IDS_TAHOMA: i64 = 4;
/// `IDS_EMPIRE_EARTH_ART_OF_CONQUEST_REQUIRES_SOUND`, `IDS_ERROR_CANT_RUN_16EA`.
const IDS_REQUIRES_SOUND: i64 = 5865;
const IDS_CANT_RUN: i64 = 5866;

/// The game's fonts in creation order (Empire Earth.cpp's `kFonts`, index for index): the family, bold, the size
/// and the original's assert line.
const FONTS: [(u8, bool, i64, i64); 38] = [
    (0, false, 8, 1399),
    (0, false, 9, 1402),
    (0, false, 10, 1405),
    (0, false, 12, 1408),
    (0, false, 14, 1411),
    (0, false, 16, 1414),
    (0, false, 18, 1417),
    (0, false, 20, 1420),
    (0, false, 24, 1423),
    (0, false, 36, 1426),
    (0, false, 48, 1429),
    (0, false, 72, 1432),
    (0, true, 8, 1435),
    (0, true, 9, 1438),
    (0, true, 10, 1441),
    (0, true, 12, 1444),
    (0, true, 14, 1447),
    (0, true, 16, 1450),
    (1, false, 9, 1457),
    (1, false, 10, 1460),
    (1, false, 12, 1463),
    (1, false, 14, 1466),
    (2, false, 8, 1473),
    (2, false, 10, 1476),
    (2, false, 12, 1479),
    (2, false, 14, 1482),
    (3, false, 8, 1489),
    (3, false, 9, 1492),
    (3, false, 10, 1495),
    (3, false, 12, 1498),
    (3, false, 14, 1501),
    (3, false, 16, 1504),
    (3, false, 18, 1507),
    (3, false, 20, 1510),
    (3, false, 24, 1513),
    (3, false, 36, 1516),
    (3, false, 48, 1519),
    (3, false, 72, 1522),
];

/// The font globals in [`FONTS`] order (were Empire Earth.cpp's `kFonts`).
fn font_slot(k: usize) -> *mut *mut c_void {
    use super::globals as g;
    use core::ptr::addr_of_mut;
    // (the addresses of the globals' storage: nothing is read or written here)
    [
        addr_of_mut!(g::gFontArial8),
        addr_of_mut!(g::gFontArial9),
        addr_of_mut!(g::gU531930),
        addr_of_mut!(g::gU531934),
        addr_of_mut!(g::gU531938),
        addr_of_mut!(g::gFontArial16),
        addr_of_mut!(g::gFontArial18),
        addr_of_mut!(g::gFontArial20),
        addr_of_mut!(g::gFontArial24),
        addr_of_mut!(g::gFontArial36),
        addr_of_mut!(g::gFontArial48),
        addr_of_mut!(g::gFontArial72),
        addr_of_mut!(g::gFontArialBold8),
        addr_of_mut!(g::gFontArialBold9),
        addr_of_mut!(g::gFontArialBold10),
        addr_of_mut!(g::gFontArialBold12),
        addr_of_mut!(g::gFontArialBold14),
        addr_of_mut!(g::gFontArialBold16),
        addr_of_mut!(g::gFontTimesNewRoman9),
        addr_of_mut!(g::gFontTimesNewRoman10),
        addr_of_mut!(g::gFontTimesNewRoman12),
        addr_of_mut!(g::gFontTimesNewRoman14),
        addr_of_mut!(g::gFontCourier8),
        addr_of_mut!(g::gFontCourier10),
        addr_of_mut!(g::gFontCourier12),
        addr_of_mut!(g::gFontCourier14),
        addr_of_mut!(g::gFontTahoma8),
        addr_of_mut!(g::gU531994),
        addr_of_mut!(g::gFontTahoma10),
        addr_of_mut!(g::gFontTahoma12),
        addr_of_mut!(g::gFontTahoma14),
        addr_of_mut!(g::gFontTahoma16),
        addr_of_mut!(g::gFontTahoma18),
        addr_of_mut!(g::gFontTahoma20),
        addr_of_mut!(g::gFontTahoma24),
        addr_of_mut!(g::gFontTahoma36),
        addr_of_mut!(g::gFontTahoma48),
        addr_of_mut!(g::gFontTahoma72),
    ][k]
}

/// The game's timer (was Empire Earth.cpp's `sTimer`, 0x5319ec): a `TMTimer*`, made with the game objects and
/// handed to the physics.
static GAME_TIMER: AtomicUsize = AtomicUsize::new(0);
/// The video mode index found at start-up, restored at shutdown (was Empire Earth.cpp's `sOriginalVideoMode`,
/// 0x52a1a8).
static ORIGINAL_VIDEO_MODE: AtomicI64 = AtomicI64::new(0);
/// The start-up's user folders under the start-up directory (were Empire Earth.cpp's `kStartupFolders`): data, its
/// campaigns, saved games and scenarios, Users and its default.
const STARTUP_FOLDERS: [&CStr; 6] =
    [c"data\\", c"data\\Campaigns\\", c"data\\Saved Games\\", c"data\\Scenarios\\", c"Users\\", c"Users\\default\\"];

/// `spec->SetDirectory(gStartupDirectory + STARTUP_FOLDERS[k])`.
fn set_startup_folder(spec: P, k: usize) {
    h(O::UsgSetDirectory, spec, &[pa(STARTUP_FOLDERS[k].as_ptr() as P)]);
}

/// `strtoul(text, NULL, 10)` (the C library's: white space, a sign, the digits; ULONG_MAX past its range).
pub fn startup_strtoul(text: &[u8]) -> c_ulong {
    let mut i = 0;
    while i < text.len() && (text[i] == b' ' || (b'\t'..=b'\r').contains(&text[i])) {
        i += 1;
    }
    let negative = i < text.len() && text[i] == b'-';
    if i < text.len() && (text[i] == b'-' || text[i] == b'+') {
        i += 1;
    }
    let mut value: c_ulong = 0;
    let mut overflow = false;
    while i < text.len() && text[i].is_ascii_digit() {
        match value.checked_mul(10).and_then(|v| v.checked_add((text[i] - b'0') as c_ulong)) {
            Some(v) => value = v,
            None => overflow = true,
        }
        i += 1;
    }
    if overflow {
        c_ulong::MAX
    } else if negative {
        value.wrapping_neg()
    } else {
        value
    }
}

/// The fonts' release order (EEGraphicsShutdown): the Times New Roman fonts first, then the others in creation
/// order.
fn font_release_order() -> impl Iterator<Item = usize> {
    (18..22).chain(0..18).chain(22..38)
}

/// `GERectangle(origin, end)`: the top left and the bottom right corners.
fn window_rect(origin: &[f32; 2], end: &[f32; 2]) -> GeRectangle {
    GeRectangle { bottom: end[1], left: origin[0], right: end[0], top: origin[1] }
}

/// `URegisterShutdown`: a routine run at the game's shutdown, last registered first.
fn register_shutdown(routine: unsafe extern "C" fn(bool)) -> i64 {
    i64::from(ee_base::util::system::ee_util_register_shutdown(routine))
}

// the shutdown routines whose argument is not read (EEGraphicsShutdown, EEShutdownThreads, EEDeleteSoundManager,
// EEDeleteGameObjects)
extern "C" fn graphics_shutdown(_shutdown: bool) {
    ee_gamecpp_startup_graphics_shutdown();
}
extern "C" fn shutdown_threads(_shutdown: bool) {
    ee_gamecpp_startup_shutdown_threads();
}
extern "C" fn delete_sound_manager(_shutdown: bool) {
    ee_gamecpp_startup_delete_sound_manager();
}
extern "C" fn delete_game_objects(_shutdown: bool) {
    ee_gamecpp_startup_delete_game_objects();
}

fn verify(ok: bool, line: i64) {
    h(O::Verify, 0, &[ok as i64, line]);
}

fn screen() -> i64 {
    h(O::PrefScreenID, 0, &[])
}

/// The rasterizer's render states (EEInitializeGraphics, EEChangeGameScreen): fog off and black, white ambient
/// light at 0, colour keying, clockwise culling, lighting, (the start-up only: the projection mode), shading, no
/// specular, no z-buffer, less-equal z, dithering, the anti-aliasing and the vsync of the preferences, alpha testing
/// above 0, the water ripple.
fn render_states(screen: i64, projection: bool) {
    h(O::RastSetFog, 0, &[0, screen]);
    hf(O::RastSetFogColor, 0, &[screen], &[0.0, 0.0, 0.0]);
    h(O::RastSetFogType, 0, &[0, screen]);
    hf(O::RastSetAmbientColor, 0, &[screen], &[1.0, 1.0, 1.0]);
    hf(O::RastSetAmbientIntensity, 0, &[screen], &[0.0]);
    h(O::RastSetColorKeying, 0, &[1, screen]);
    h(O::RastSetCullingCW, 0, &[]);
    h(O::RastSetLighting, 0, &[1, screen]);
    if projection {
        h(O::RastSetProjectionMode, 0, &[1, screen]);
    }
    h(O::RastSetShading, 0, &[1, screen]);
    h(O::RastSetSpecular, 0, &[0, screen]);
    h(O::RastSetZBufferOff, 0, &[screen]);
    h(O::RastSetZCompareLessEqual, 0, &[screen]);
    h(O::RastSetDithering, 0, &[1, screen]);
    h(O::RastSetAntiAliasingPref, 0, &[screen]);
    h(O::RastSetAlphaTesting, 0, &[1, screen]);
    h(O::RastSetAlphaCompareGreater, 0, &[screen]);
    hf(O::RastSetAlphaRef, 0, &[screen], &[0.0]);
    h(O::RastSetVSyncPref, 0, &[screen]);
    hf(O::RastSetWaterRipple, 0, &[], &START_WATER_RIPPLE);
}

/// Waits for the graphics thread to pause after asking it to.
fn pause_graphics() {
    super::globals::graphics_post_pause();
    let mut started = h(O::GfxIsPaused, 0, &[]) != 0;
    while !started {
        ee_base::plat_core::time::PlatSleep(1);
        started = h(O::GfxIsPaused, 0, &[]) != 0;
    }
}

/// Stops the physics' timer (message 0x402) and waits for it.
fn stop_physics_timer() {
    h(O::PhysPost, 0, &[0x402]);
    while h(O::PhysTimerRunning, 0, &[]) != 0 {
        ee_base::plat_core::time::PlatSleep(1);
    }
}

/// EESetThreadFloatingPoint: the calling thread's FPU at 53-bit precision and round to nearest.
#[no_mangle]
pub extern "C" fn ee_gamecpp_startup_thread_fp() {
    if fp::ee_util_fp_precision() != PRECISION_53 {
        fp::ee_util_fp_set_precision(PRECISION_53);
    }
    if fp::ee_util_fp_rounding() != 0 {
        fp::ee_util_fp_set_rounding(0);
    }
}

/// EEAttackPlayers: every computer player at war with the player attacks it; with the flag, the others (its allies
/// and its own computer player) defend it.
#[no_mangle]
pub extern "C" fn ee_gamecpp_startup_attack_players(player: c_long, flag: u8) {
    let pl = hp(O::WorldPlayer, 0, &[player as i64]);
    if pl == 0 {
        return;
    }
    let mut allies = Vec::new();
    let mut enemies = Vec::new();
    // SAFETY: the world's member
    let players = unsafe { *super::rec::<c_ulong>(O::WNumPlayers, hp(O::WorldAddr, 0, &[])) };
    let pid = |p: P| -> c_ulong {
        // SAFETY: a live player's record
        unsafe { (*crate::gamecpp::records::pl_rts(p)).mPlayerID }
    };
    for i in 1..players {
        let other = hp(O::WorldPlayer, 0, &[i as i64]);
        if other == 0 {
            continue;
        }
        let computer = hp(O::EgComputer, other, &[]);
        // SAFETY: as pid
        let observer = unsafe { (*crate::gamecpp::records::pl_rts(other)).mObserverMode };
        if computer != 0 && !observer {
            // SAFETY: the player's saved numbers
            let enemy = unsafe { (*crate::gamecpp::records::pl_saved(pl)).mEnemies[pid(computer) as usize] };
            let is_enemy = computer != pl && enemy != 0;
            if !is_enemy {
                allies.push(computer);
            } else {
                enemies.push(computer);
            }
        }
    }
    // every computer player's Lua AI gets the order as an event (the original's combat managers took it)
    for &cp in enemies.iter().rev() {
        h(O::CpOnAttackPlayer, cp, &[pid(pl) as i64, 0]);
    }
    if flag != 0 {
        for &cp in allies.iter().rev() {
            h(O::CpOnAttackPlayer, cp, &[pid(pl) as i64, 1]);
        }
    }
}

/// EELoadNetworkSettings: the five campaign checksums from string resources 0x32..0x34, 0x37, 0x36 (a missing
/// string keeps the value).
#[no_mangle]
pub extern "C" fn ee_gamecpp_startup_network_settings() {
    let s = Owned::new(O::StrNew, O::StrFree);
    for (k, id) in [0x32i64, 0x33, 0x34, 0x37, 0x36].into_iter().enumerate() {
        if h(O::ResString, s.0, &[id]) == KESE_OK {
            // SAFETY: the string's text, read before the next C++ call
            let text = unsafe { CStr::from_ptr(hp(O::EntryStrData, s.0, &[]) as *const c_char) };
            let value = startup_strtoul(text.to_bytes()) as c_long;
            use super::globals as g;
            match k {
                0 => g::set!(gAOCRomanChecksum, value),
                1 => g::set!(gAOCPacificChecksum, value),
                2 => g::set!(gAOCAsianChecksum, value),
                3 => g::set!(gEEDemoChecksum, value),
                _ => g::set!(gEELearningChecksum, value),
            }
        }
    }
}

/// EEInitializePart1: the engine (input, threads, timers) started, the display opened and the graphics, sound,
/// network, database, game objects and user interface initialised; false (after a message box) when the sound fails.
#[no_mangle]
pub extern "C" fn ee_gamecpp_startup_part1() -> bool {
    let r = h(O::RegisterShutdownMutex, 0, &[]);
    verify(r == KESE_OK, 796);
    let r = i64::from(ee_base::ts::ee_ts_system_initialize());
    verify(r == KESE_OK, 805);
    ee_gamecpp_startup_thread_fp();
    let r = i64::from(ee_base::time::ee_time_system_initialize());
    verify(r == KESE_OK, 810);
    ee_gamecpp_startup_thread_fp();
    let initialized = ee_gamecpp_startup_display();
    if initialized {
        ee_gamecpp_startup_graphics();
        ee_gamecpp_startup_thread_fp();
        let r = h(O::IsInitialize, 0, &[]);
        verify(r == KESE_OK, 823);
        ee_gamecpp_startup_thread_fp();
        if !ee_gamecpp_startup_sounds() {
            h(O::WindowHide, 0, &[]);
            let message = Owned::new(O::StrNew, O::StrFree);
            let title = Owned::new(O::StrNew, O::StrFree);
            h(O::ResString, message.0, &[IDS_REQUIRES_SOUND]);
            h(O::ResString, title.0, &[IDS_CANT_RUN]);
            h(O::MessageShowError, title.0, &[pa(message.0)]);
            return false;
        }
        ee_gamecpp_startup_thread_fp();
        super::entry::ee_gamecpp_entry_initialize_network();
        ee_gamecpp_startup_thread_fp();
        ee_gamecpp_startup_database();
        ee_gamecpp_startup_thread_fp();
        ee_gamecpp_startup_game_objects();
        ee_gamecpp_startup_thread_fp();
        h(O::NewUserInterface, 0, &[]);
        verify(h(O::UserInterfacePresent, 0, &[]) != 0, 853);
    }
    initialized
}

/// EEInitializeGraphics: the texture and model managers, the screen at 640x480, the render states, the render
/// objects and every UI font; the graphics shutdown registered.
#[no_mangle]
pub extern "C" fn ee_gamecpp_startup_graphics() {
    let resource_name = Owned::new(O::StrNew, O::StrFree);
    let font_name = Owned::new(O::UStrNew, O::UStrFree);
    h(O::NewTextureManager, 0, &[]);
    verify(crate::gamecpp::globals::have!(gTextureManager), 1332);
    h(O::NewModelManager, 0, &[]);
    verify(crate::gamecpp::globals::have!(gModelManager), 1336);
    ee_gamecpp_startup_video_mode(640, 480, h(O::PrefGameBitDepth, 0, &[]) as c_ulong, true);
    h(O::WindowResize, 0, &[640, 480]);
    // kPixelFormatZPrefer16
    let r = h(O::RastActivateScreen, 0, &[0, PIXEL_FORMAT_Z_PREFER_16]);
    verify(r == KESE_OK, 1346);
    // SAFETY: a constant rectangle
    unsafe { ee_gamecpp_startup_game_view(&START_VIEW) };
    render_states(screen(), true);
    for op in [O::NewRoTerrain, O::NewRoGroundShadows, O::NewRoUnits, O::NewRoWater, O::NewRoEffects] {
        h(op, 0, &[]);
    }
    let mut family = usize::MAX;
    for (k, &(f, bold, size, line)) in FONTS.iter().enumerate() {
        if f as usize != family {
            family = f as usize;
            h(O::ResString, resource_name.0, &[FONT_FAMILY_NAMES[family]]);
            h(O::UStrFromW, font_name.0, &[pa(resource_name.0)]);
        }
        let font = hp(O::FontCreate, font_name.0, &[bold as i64, size]) as *mut c_void;
        // SAFETY: the font global's storage, written on the start-up thread as the C++ did
        unsafe { *font_slot(k) = font };
        verify(!font.is_null(), line);
    }
    let r = register_shutdown(graphics_shutdown);
    verify(r == KESE_OK, 1526);
}

/// EERegisterPersistents: the saved game's persistent sections (checksum, map, scenario, players, world, effects,
/// ...) in their order, and the list's shutdown.
#[no_mangle]
pub extern "C" fn ee_gamecpp_startup_persistents() {
    for op in [
        O::PersistChecksum,
        O::PersistGeneric2,
        O::PersistMap,
        O::PersistPoint3,
        O::PersistPlayers,
        O::PersistScenario,
        O::PersistWorld,
        O::PersistMultiplayer,
        O::PersistEffects,
        O::PersistCalamity,
        O::PersistEnvironment,
    ] {
        h(op, 0, &[]);
    }
    let r = h(O::RegisterShutdownPersistents, 0, &[]);
    verify(r == KESE_OK, 1863);
}

/// CreatePlaneModels: the effects' plane models, made with the render objects' rasterizer.
pub(super) fn create_plane_models() {
    // SAFETY: the render objects' rasterizer
    unsafe { ee_render::gfx::ffi::ee_gfx_effect_planes_create(super::ro_raster::ee_gamecpp_ro_raster()) };
}

/// EELoadGameMedia: the status box's icons, the mini map's, the effects', the units', the sky's and the ground
/// shadows' media.
#[no_mangle]
pub extern "C" fn ee_gamecpp_startup_load_media() {
    h(O::UiLoadIcons, 0, &[]);
    h(O::UiMiniMapLoadMedia, 0, &[]);
    create_plane_models();
    h(O::RoUnitsLoadMedia, 0, &[]);
    super::ro_views::ee_gamecpp_te_create_sky_model();
    // SAFETY: the render objects' rasterizer
    unsafe { ee_render::gfx::shadows::ee_gfx_ground_shadows_create(super::ro_raster::ee_gamecpp_ro_raster()) };
}

/// EEUnloadGameMedia: the same released.
#[no_mangle]
pub extern "C" fn ee_gamecpp_startup_unload_media() {
    h(O::UiUnloadIcons, 0, &[]);
    h(O::UiMiniMapUnloadMedia, 0, &[]);
    // SAFETY: the render objects' rasterizer
    unsafe { ee_render::gfx::ffi::ee_gfx_effect_planes_release(super::ro_raster::ee_gamecpp_ro_raster()) };
    super::ro_units::ee_gamecpp_ru_release_media();
    super::ro_views::ee_gamecpp_te_release_sky_models();
    // SAFETY: the same
    unsafe { ee_render::gfx::shadows::ee_gfx_ground_shadows_release(super::ro_raster::ee_gamecpp_ro_raster()) };
}

/// EESetDataPaths: the texture, model and sound managers pointed at the data search path.
#[no_mangle]
pub extern "C" fn ee_gamecpp_startup_set_data_paths() {
    h(O::TexturesSetPath, 0, &[]);
    h(O::ModelsSetPath, 0, &[]);
    h(O::SoundSetPath, 0, &[]);
}

/// EEShowSinglePlayerScreen: the data search path (the game's Data folder, behind a Data\data.ssa when one is
/// present) and the civilization path made, replacing the previous ones.
#[no_mangle]
pub extern "C" fn ee_gamecpp_startup_data_paths() {
    let archive = Owned::new(O::ArchiveNew, O::ArchiveDelete);
    let archive_file = Owned::new(O::FileSpecNew, O::FxFileSpecDelete);
    let directory = Owned::new(O::FileSpecNew, O::FxFileSpecDelete);
    let data = hp(O::NewFsPath, 0, &[]);
    let civilizations = hp(O::NewFsPath, 0, &[]);
    h(O::FileSpecSetVolumeStartup, archive_file.0, &[]);
    set_startup_folder(archive_file.0, 0);
    h(O::FileSpecSetArchiveName, archive_file.0, &[]);
    if h(O::DataArchivePresent, 0, &[]) != 0 {
        h(O::ArchiveSet, archive.0, &[pa(archive_file.0)]);
        h(O::FsPathAddArchive, data, &[pa(archive.0)]);
    }
    h(O::FileSpecSetVolumeStartup, directory.0, &[]);
    set_startup_folder(directory.0, 0);
    h(O::FsPathAddDirectory, data, &[pa(directory.0)]);
    h(O::FileSpecSetVolumeStartup, directory.0, &[]);
    h(O::FileSpecSetCivilizationDir, directory.0, &[]);
    h(O::FsPathAddDirectory, civilizations, &[pa(directory.0)]);
    if crate::gamecpp::globals::have!(gDataPath) {
        h(O::DataPathDereference, 0, &[]);
    }
    if crate::gamecpp::globals::have!(gCivilizationDataPath) {
        h(O::CivPathDereference, 0, &[]);
    }
    h(O::SetCivPath, civilizations, &[]);
    h(O::SetDataPath, data, &[]);
}

/// EESetGameViewRectangle: the viewport set to the rectangle (in 1600x1200 layout units, scaled to the window and
/// rounded unless the camera is free); the camera stops following.
///
/// # Safety
/// `rectangle` is the caller's rectangle.
#[no_mangle]
pub unsafe extern "C" fn ee_gamecpp_startup_game_view(rectangle: *const GeRectangle) {
    // SAFETY: the caller's rectangle
    let input = unsafe { *rectangle };
    let mut r = GeRectangle::default();
    if !crate::gamecpp::globals::get!(gFreeCamera) {
        let width = h(O::GameWindowWidth, 0, &[]) as c_ulong as f32;
        r.left = input.left * GAME_VIEW_SCALE_X * width;
        r.right = input.right * GAME_VIEW_SCALE_X * width;
        let height = h(O::GameWindowHeight, 0, &[]) as c_ulong as f32;
        r.top = input.top * GAME_VIEW_SCALE_Y * height;
        r.bottom = input.bottom * GAME_VIEW_SCALE_Y * height;
        h(O::RectRoundToNearest, 0, &[ptr(&mut r)]);
    } else {
        r = input;
    }
    h(O::ViewportSetViewRectangle, 0, &[cptr(&r)]);
    h(O::SceneFlagsClear, 0, &[]);
}

/// EEInitializeThreads: the music manager, the mouse pointer, graphics, data preloading and physics threads; their
/// shutdown registered.
#[no_mangle]
pub extern "C" fn ee_gamecpp_startup_threads() {
    h(O::NewMusicManager, 0, &[]);
    verify(crate::gamecpp::globals::have!(gMusicManager), 2006);
    h(O::NewMousePointer, 0, &[]);
    verify(crate::gamecpp::globals::have!(gMousePointer), 2010);
    h(O::NewGraphics, 0, &[]);
    verify(crate::gamecpp::globals::have!(gGraphics), 2018);
    h(O::NewDataPreLoading, 0, &[]);
    verify(crate::gamecpp::globals::have!(gDataPreLoading), 2022);
    h(O::NewPhysics, GAME_TIMER.load(Ordering::Relaxed) as P, &[]);
    verify(crate::gamecpp::globals::have!(gPhysics), 2026);
    let r = register_shutdown(shutdown_threads);
    verify(r == KESE_OK, 2030);
}

/// EESetVideoMode: the screen switched to the mode of that size and depth (optionally the lowest refresh above
/// 60 Hz), the window resized and both surfaces cleared; nothing when the mode is already active.
#[no_mangle]
pub extern "C" fn ee_gamecpp_startup_video_mode(width: c_ulong, height: c_ulong, bits: c_ulong, prefer_refresh: bool) {
    let screen = screen();
    let count = h(O::ScreenModeCount, 0, &[screen]) as c_ulong;
    let mut refresh: c_ulong = 0;
    let mut mode = [0 as c_ulong; 5];
    h(O::ScreenCurrentMode, 0, &[screen, ptr(&mut mode)]);
    let changed = mode[0] != width || mode[1] != height || mode[3] != bits;
    if !changed {
        h(O::DebugVideoModeActive, 0, &[]);
        return;
    }
    let mut index: c_ulong;
    if prefer_refresh {
        index = count;
        for i in 0..count {
            h(O::ScreenMode, 0, &[screen, i as i64, ptr(&mut mode)]);
            if mode[0] == width
                && mode[1] == height
                && mode[3] == bits
                && (index == count || (refresh == 60 && mode[2] > 60) || (mode[2] > 60 && mode[2] < refresh))
            {
                refresh = mode[2];
                index = i;
            }
        }
    } else {
        index = 0;
        while index < count {
            h(O::ScreenMode, 0, &[screen, index as i64, ptr(&mut mode)]);
            if mode[0] == width && mode[1] == height && mode[3] == bits {
                break;
            }
            index += 1;
        }
    }
    verify(index < count, 2477);
    h(O::ScreenSetMode, 0, &[screen, index as i64]);
    h(O::WindowResize, 0, &[width as i64, height as i64]);
    let mut origin = [0f32; 2];
    h(O::WindowClientOrigin, 0, &[ptr(&mut origin)]);
    let end = [
        origin[0] + h(O::RuWindowWidth, 0, &[]) as c_ulong as f32,
        origin[1] + h(O::WindowHeight, 0, &[]) as c_ulong as f32,
    ];
    if h(O::RastPrimaryPresent, 0, &[screen]) != 0 {
        h(O::RastPrimaryClear, 0, &[screen, cptr(&window_rect(&origin, &end))]);
    }
    if h(O::RastSecondaryPresent, 0, &[screen]) != 0 {
        h(O::RastSecondaryClear, 0, &[screen, cptr(&window_rect(&origin, &end))]);
    }
    h(O::ScreenMode, 0, &[screen, index as i64, ptr(&mut mode)]);
    h(O::DebugVideoModeChanged, 0, &[cptr(&mode)]);
}

/// EEStopAllSounds: the music, the ambient sounds (disabled) and every one-shot sound stopped.
#[no_mangle]
pub extern "C" fn ee_gamecpp_startup_stop_sounds() {
    if crate::gamecpp::globals::have!(gMusicManager) {
        h(O::MusicStop, 0, &[]);
    }
    h(O::AmbientStop, 0, &[]);
    h(O::AmbientSetEnabled, 0, &[0]);
    h(O::SoundStopOneShots, 0, &[]);
}

/// EEClearGame: graphics and physics paused, the scene released, the communications deleted, the world and the
/// game state cleared under the world lock.
#[no_mangle]
pub extern "C" fn ee_gamecpp_startup_clear_game() {
    pause_graphics();
    stop_physics_timer();
    h(O::GraphicsReleaseScene, 0, &[]);
    h(O::GfxPostMessage50AAndWait, 0, &[]);
    h(O::PreloadWaitForQueue, 0, &[]);
    if super::net::present() {
        h(O::CommDeleteDetached, 0, &[]);
    }
    let w = hp(O::WorldAddr, 0, &[]);
    h(O::WWriteLock, w, &[]);
    h(O::WorldClear, 0, &[]);
    h(O::GameStateClear, 0, &[]);
    h(O::PhysPostInit, 0, &[]);
    h(O::SetGameScreenActive, 0, &[0]);
    h(O::WWriteUnlock, w, &[]);
}

/// EEInitializePart2: the game threads and the saved game's persistent sections.
#[no_mangle]
pub extern "C" fn ee_gamecpp_startup_part2() {
    ee_gamecpp_startup_threads();
    ee_gamecpp_startup_thread_fp();
    ee_gamecpp_startup_persistents();
    ee_gamecpp_startup_thread_fp();
}

/// EEStartMenuScreen: the menus' screen after start-up (the port plays no intro movies).
#[no_mangle]
pub extern "C" fn ee_gamecpp_startup_menu_screen() {
    let screen = screen();
    h(O::WindowRaise, 0, &[]);
    h(O::IsMouseResume, 0, &[]);
    h(O::IsKeyboardResume, 0, &[]);
    h(O::RastClearStart, 0, &[screen]);
    h(O::RastShowScene, 0, &[screen]);
    // the menus run at EEMenuWidth x EEMenuHeight (the original's 1024x768)
    let (w, hgt) = (h(O::MenuWidth, 0, &[]), h(O::MenuHeight, 0, &[]));
    ee_gamecpp_startup_change_screen(w as c_ulong, hgt as c_ulong, false);
    h(O::IsMouseSuspend, 0, &[]);
    h(O::IsKeyboardSuspend, 0, &[]);
    h(O::RastSetVSyncPref, 0, &[screen]);
}

/// EEGraphicsShutdown: the models, textures and fonts released, the start-up video mode restored, the game window
/// destroyed, the render objects and the texture and model managers deleted.
#[no_mangle]
pub extern "C" fn ee_gamecpp_startup_graphics_shutdown() {
    h(O::ModelsRemoveAll, 0, &[]);
    for i in 0..h(O::RoCliffTexturesCount, 0, &[]) {
        h(O::EffectTextureDereference, 0, &[i]);
    }
    h(O::TexturesRemoveAll, 0, &[]);
    h(O::ScreenSetMode, 0, &[screen(), ORIGINAL_VIDEO_MODE.load(Ordering::Relaxed)]);
    h(O::ScreenReleaseWindow, 0, &[]);
    for k in font_release_order() {
        // SAFETY: the font global's storage
        h(O::FontDereference, unsafe { *font_slot(k) } as P, &[]);
    }
    h(O::DuDevicesClear, 0, &[]);
    h(O::DeleteGameWindow, 0, &[]);
    super::effect_host::ee_gamecpp_fx_batch_delete();
    h(O::RastDeactivate, 0, &[]);
    for op in [
        O::DeleteRoWater,
        O::DeleteRoGroundShadows,
        O::DeleteRo4C,
        O::DeleteRoTerrain,
        O::DeleteRoUnits,
        O::DeleteRoEffects,
    ] {
        h(op, 0, &[]);
    }
    h(O::DeleteTextureManager, 0, &[]);
    h(O::DeleteModelManager, 0, &[]);
}

/// `kThreadMessageQuit`: every thread's message loop stops on it.
const THREAD_MESSAGE_QUIT: c_ulong = 0x401;

/// A thread's Shutdown (were RTSMousePointer's, RTSPhysics' and EEDataPreLoading's inlines): the quit message posted
/// and queued, then the thread's done event awaited.
///
/// # Safety
/// `thread` is the object's live TSThread, `done` its TSEvent (the Thread System's records).
#[no_mangle]
pub unsafe extern "C" fn ee_gamecpp_startup_quit_thread(thread: *mut c_void, done: *mut c_void) {
    // SAFETY: the caller's live thread and event
    unsafe {
        ee_base::ts::ee_ts_thread_post(
            thread.cast(),
            THREAD_MESSAGE_QUIT,
            core::ptr::null_mut(),
            core::ptr::null_mut(),
            true,
        );
        ee_base::ts::ee_ts_event_wait(done.cast(), ee_base::plat_sys::constants::PLAT_WAIT_INFINITE);
    }
}

/// EEShutdownThreads: the physics stopped, the communications and the WON connection deleted, then the physics,
/// data preloading, graphics and mouse pointer threads shut down and deleted, the music manager and the user
/// interface deleted.
#[no_mangle]
pub extern "C" fn ee_gamecpp_startup_shutdown_threads() {
    stop_physics_timer();
    h(O::CommDelete, 0, &[]);
    h(O::WonNetDeleteDetached, 0, &[]);
    h(O::MousePointerPost, 0, &[0x402]);
    while h(O::MousePointerShown, 0, &[]) != 0 {
        ee_base::plat_core::time::PlatSleep(1);
    }
    pause_graphics();
    for (shutdown, delete) in [
        (O::PhysShutdown, O::PhysDelete),
        (O::PreloadShutdown, O::PreloadDelete),
        (O::GfxShutdown, O::GfxDelete),
        (O::MousePointerShutdown, O::MousePointerDelete),
    ] {
        h(shutdown, 0, &[]);
        h(delete, 0, &[]);
    }
    h(O::MusicManagerDelete, 0, &[]);
    h(O::UserInterfaceDelete, 0, &[]);
    h(O::PickerReset, 0, &[]);
}

/// EEChangeGameScreen: the display made again at the new size (full screen or not): the rasterizer reactivated,
/// the render states and the camera reset, the mouse, textures, buttons and icons reloaded and the mouse pointer
/// thread restarted.
#[no_mangle]
pub extern "C" fn ee_gamecpp_startup_change_screen(width: c_ulong, height: c_ulong, full_screen: bool) {
    let mut origin = [0f32; 2];
    h(O::WindowClientOrigin, 0, &[ptr(&mut origin)]);
    let screen = screen();
    let window_end = |o: &[f32; 2]| {
        [o[0] + h(O::RuWindowWidth, 0, &[]) as c_ulong as f32, o[1] + h(O::WindowHeight, 0, &[]) as c_ulong as f32]
    };
    let old_end = window_end(&origin);
    let precision = fp::ee_util_fp_precision();
    let mut had_pointer = false;
    if crate::gamecpp::globals::have!(gMousePointer) {
        had_pointer = true;
        h(O::MousePointerShutdown, 0, &[]);
        h(O::MousePointerDelete, 0, &[]);
    }
    h(O::MouseRelease, 0, &[]);
    if h(O::RastIsActivated, 0, &[]) != 0 {
        h(O::RastPrimaryClear, 0, &[screen, cptr(&window_rect(&origin, &old_end))]);
        h(O::RastSecondaryClear, 0, &[screen, cptr(&window_rect(&origin, &old_end))]);
        h(O::ScreenReleaseWindow, 0, &[]);
        super::effect_host::ee_gamecpp_fx_batch_delete();
        h(O::RastDeactivate, 0, &[]);
    }
    let r = h(O::RastActivate, 0, &[]);
    verify(r == KESE_OK, 2978);
    super::effect_host::ee_gamecpp_fx_batch_create();
    let r = h(O::ScreenCapture, 0, &[]);
    verify(r == KESE_OK, 2987);
    ee_gamecpp_startup_video_mode(width, height, h(O::PrefGameBitDepth, 0, &[]) as c_ulong, true);
    h(O::WindowResize, 0, &[width as i64, height as i64]);
    let format = if h(O::PrefGameBitDepth, 0, &[]) == 16 { 12 } else { 13 };
    let r = h(O::RastActivateScreen, 0, &[full_screen as i64, format]);
    verify(r == KESE_OK, 3004);
    if crate::gamecpp::globals::have!(gGraphics) {
        h(O::GfxSetSecondarySurface, 0, &[]);
    }
    super::camera_io::set_zoom(1.0);
    super::camera_io::set_changed();
    // SAFETY: a constant rectangle
    unsafe { ee_gamecpp_startup_game_view(&START_VIEW) };
    render_states(screen, false);
    let end = window_end(&origin);
    h(O::RastPrimaryClear, 0, &[screen, cptr(&window_rect(&origin, &end))]);
    let end = window_end(&origin);
    h(O::RastSecondaryClear, 0, &[screen, cptr(&window_rect(&origin, &end))]);
    h(O::MouseSetWindowBounds, 0, &[]);
    h(O::MouseLoad, 0, &[]);
    let r = h(O::TexturesRefreshAll, 0, &[]);
    verify(r == KESE_OK, 3070);
    super::db_media::ee_gamecpp_db_button_reload();
    if crate::gamecpp::globals::get!(sIconsLoaded) {
        h(O::UiReloadIcons, 0, &[]);
        h(O::UiUnloadIcons, 0, &[]);
        h(O::UiLoadIcons, 0, &[]);
    }
    if had_pointer {
        h(O::NewMousePointer, 0, &[]);
        verify(crate::gamecpp::globals::have!(gMousePointer), 3094);
    }
    if crate::gamecpp::globals::have!(gMousePointer) {
        h(O::MousePointerPostSurface, 0, &[]);
    }
    fp::ee_util_fp_set_precision(precision);
    if fp::ee_util_fp_rounding() != 0 {
        fp::ee_util_fp_set_rounding(0);
    }
}

/// EEInitializeDisplay: the graphics engine started, the preferred rasterizer picked, the screen, depths and
/// resolution validated (800x600x16 as the fallback), the game window made and captured.
#[no_mangle]
pub extern "C" fn ee_gamecpp_startup_display() -> bool {
    let r = h(O::GeInitialize, 0, &[]);
    verify(r == KESE_OK, 3369);
    super::utilities::ee_gamecpp_enumerate_display_devices();
    let count = h(O::DuRasterizerCount, 0, &[]) as c_ulong;
    let mut k: c_ulong = 0;
    while k < count {
        h(O::RasterizerSelect, 0, &[k as i64]);
        if h(O::RasterizerIsPreferred, 0, &[]) != 0 {
            break;
        }
        k += 1;
    }
    if k < h(O::DuRasterizerCount, 0, &[]) as c_ulong {
        h(O::RasterizerSelect, 0, &[k as i64]);
    } else {
        h(O::RasterizerSelect, 0, &[0]);
        h(O::PrefSetRasterizerName, 0, &[]);
    }
    let r = h(O::RastActivate, 0, &[]);
    verify(r == KESE_OK, 3402);
    if h(O::RastScreenCount, 0, &[]) <= h(O::PrefScreenIDRaw, 0, &[]) {
        h(O::SetScreenIndexZero, 0, &[]);
    }
    h(O::RastSetScreenID, 0, &[]);
    // (a depth's validity is a C++ case: the test profile's overlay accepts and stores 32 where 16 is, build.py)
    for (depth, valid, support, set16) in [
        (O::PrefGameBitDepth, O::PrefGameBitDepthValid, O::Screen32BitDevice, O::PrefSetGameBitDepth16),
        (O::PrefTextureBitDepth, O::PrefTextureBitDepthValid, O::Screen32BitTexture, O::PrefSetTextureBitDepth16),
    ] {
        let bits = h(depth, 0, &[]);
        if h(valid, 0, &[]) == 0 || (bits == 32 && h(support, 0, &[]) == 0) {
            h(set16, 0, &[]);
        }
    }
    // the game's resolution follows the UI scale (the display's logical size divided by it; windowed: of a window of
    // 80% of the display): the render scale fills the display's pixels
    h(O::GameSizeFromPreferences, 0, &[]);
    let modes = h(O::ScreenModeCount, 0, &[screen()]) as c_ulong;
    let mut mode = [0 as c_ulong; 5];
    let (w, hgt, bits) = (
        h(O::GameWindowWidth, 0, &[]) as c_ulong,
        h(O::GameWindowHeight, 0, &[]) as c_ulong,
        h(O::PrefGameBitDepth, 0, &[]) as c_ulong,
    );
    let mut index: c_ulong = 0;
    while index < modes {
        h(O::ScreenMode, 0, &[screen(), index as i64, ptr(&mut mode)]);
        if mode[0] == w && mode[1] == hgt && mode[3] == bits {
            break;
        }
        index += 1;
    }
    if modes <= index {
        h(O::SetGameWindowFallback, 0, &[]);
    }
    h(O::PrefSave, 0, &[]);
    super::effect_host::ee_gamecpp_fx_batch_create();
    let mut desktop = GeRectangle::default();
    h(O::ScreenDesktopRect, 0, &[ptr(&mut desktop)]);
    let origin = [desktop.left, desktop.top];
    let r = if h(O::PrefFullScreen, 0, &[]) == 0 {
        // windowed, a window of 80% of the display that keeps its size (GXGameSize; the frames scale to it)
        let (mut ww, mut wh) = (0i32, 0i32);
        if h(O::DisplayLogicalSize, 0, &[ptr(&mut ww), ptr(&mut wh)]) == 0 || ww <= 0 {
            ww = 800;
            wh = 600;
        } else {
            ww = ww * 4 / 5;
            wh = wh * 4 / 5;
        }
        let r = hf(O::CreateGameWindow, 0, &[ww as u32 as i64, wh as u32 as i64, 1], &origin);
        if r == KESE_OK {
            h(O::WindowSetFixedSize, 0, &[]);
        }
        r
    } else {
        let (dw, dh) = ((desktop.right - desktop.left) as c_ulong, (desktop.bottom - desktop.top) as c_ulong);
        hf(O::CreateGameWindow, 0, &[dw as i64, dh as i64, 0], &origin)
    };
    verify(r == KESE_OK, 3523);
    // typed text always arrives (WM_CHAR did): the window's text input is on from the start
    h(O::WindowTextInputStart, 0, &[]);
    let r = h(O::ScreenCapture, 0, &[]);
    verify(r == KESE_OK, 3527);
    ORIGINAL_VIDEO_MODE.store(h(O::ScreenCurrentModeIndex, 0, &[]), Ordering::Relaxed);
    true
}

/// EEInitializeSounds: the sound engine and the music conductor from the preferences and the sound manager; false
/// if the sound engine fails.
#[no_mangle]
pub extern "C" fn ee_gamecpp_startup_sounds() -> bool {
    if h(O::SeInitialize, 0, &[]) != KESE_OK {
        return false;
    }
    let r = h(O::MixerGet, 0, &[]);
    verify(r == KESE_OK, 0x770);
    let r = h(O::MixerUse, 0, &[]);
    verify(r == KESE_OK, 0x772);
    let r = h(O::ConductorSetChannelGroups, 0, &[8]);
    verify(r == KESE_OK, 0x775);
    if h(O::PrefMusicEnabled, 0, &[]) != 0 {
        let r = h(O::ConductorSetState, 0, &[1]);
        verify(r == KESE_OK || r == 2, 0x77a);
    } else {
        let r = h(O::ConductorSetState, 0, &[0]);
        verify(r == KESE_OK, 0x77f);
    }
    h(O::ConductorSetReverb, 0, &[(h(O::PrefReverbEnabled, 0, &[]) != 0) as i64]);
    h(O::ConductorSetVolumePref, 0, &[]);
    h(O::ConductorSetIntensity0, 0, &[]);
    // the music is the game's own Data\Music (the original read the original Empire Earth's)
    let folder = Owned::new(O::StrNew, O::StrFree);
    h(O::StrFormatMusicFolder, folder.0, &[]);
    let r = h(O::ConductorSetMusicFolder, folder.0, &[]);
    verify(r == KESE_OK, 0x7a4);
    h(O::NewSoundManager, 0, &[]);
    verify(h(O::SoundManagerPresent, 0, &[]) != 0, 0x7a8);
    let channels = h(O::SoundMaxChannels, 0, &[]) as c_ulong as f32 * hd(O::PrefMaxSounds, 0, &[], &[]);
    let maximum = ee_base::math::um::lrintf(channels) as c_ulong;
    h(O::SoundSetMaxChannels, 0, &[if maximum < 8 { 8 } else { maximum as i64 }]);
    h(O::SoundSetMasterVolumePref, 0, &[]);
    h(O::SoundSetCategoryPriority, 0, &[0x31, 0]);
    hf(O::SoundSetCategoryPercentage, 0, &[0x31], &[1.0]);
    h(O::SoundSetLatency, 0, &[200]);
    let r = register_shutdown(delete_sound_manager);
    verify(r == KESE_OK, 0x7c4);
    true
}

/// EEDeleteSoundManager: the sound manager destroyed, its memory pool logged and freed with its locks.
#[no_mangle]
pub extern "C" fn ee_gamecpp_startup_delete_sound_manager() {
    h(O::SoundManagerDestroy, 0, &[]);
    h(O::SoundPoolDump, 0, &[]);
    ee_audio::sound::objects::ee_sound_pool_cleanup();
}

/// A db table found (its text form: EEDbFindFile, the name `k` of db_text.rs's `DB_TABLE_FILES`) and loaded
/// by its case; a failure stops the game (ESVerify at the original's lines: `line` the find, + 1 the file, + 3 the
/// load).
fn load_table(k: usize, load: O, line: i64) {
    let mut file: P = 0;
    let r = super::db_text::find_in_data_path(super::db_text::DB_TABLE_FILES[k], &mut file);
    verify(r == KESE_OK, line);
    verify(file != 0, line + 1);
    let r = h(load, file, &[]);
    verify(r == KESE_OK, line + 3);
    h(O::FxFileSpecDelete, file, &[]);
}

/// The db tables in start-up order with the original's lines: the first group, then (after the events) the second,
/// (after the terrain colours) the third.
const TABLES_1: [(O, i64); 10] = [
    (O::DbLoadStartingResources, 909),
    (O::DbLoadUnitBehavior, 917),
    (O::DbLoadSounds, 925),
    (O::DbLoadGraphics, 933),
    (O::DbLoadGfxEffects, 941),
    (O::DbLoadMusic, 949),
    (O::DbLoadWorld, 962),
    (O::DbLoadFamily, 970),
    (O::DbLoadButtons, 978),
    (O::DbLoadTechTree, 986),
];
const TABLES_2: [(O, i64); 4] =
    [(O::DbLoadAnimals, 1006), (O::DbLoadColorTable, 1014), (O::DbLoadAIUnitTargeting, 1022), (O::DbLoadTerrain, 1030)];
const TABLES_3: [(O, i64); 12] = [
    (O::DbLoadTerrainType, 1042),
    (O::DbLoadCliffTerrain, 1050),
    (O::DbLoadCivPowers, 1058),
    (O::DbLoadCivilization, 1066),
    (O::DbLoadWeaponToHit, 1082),
    (O::DbLoadCalamity, 1090),
    (O::DbLoadUnitSet, 1098),
    (O::DbLoadAreaEffect, 1106),
    (O::DbLoadUpgrade, 1114),
    (O::DbLoadCPBehavior, 1122),
    (O::DbLoadGameVariant, 1130),
    (O::DbLoadPremadeCivs, 1138),
];

/// EEInitializeDatabase: the rules and every db table in start-up order, the user folders (campaigns, saved games,
/// scenarios, users) made, the ambient sounds loaded, the game pools sized and the database shutdown registered.
#[no_mangle]
pub extern "C" fn ee_gamecpp_startup_database() {
    h(O::LoadRules, 0, &[]);
    let mut k = 0usize;
    for (load, line) in TABLES_1 {
        load_table(k, load, line);
        k += 1;
    }
    let (mut events, mut effects): (P, P) = (0, 0);
    let r = super::db_text::find_in_data_path(super::db_text::DB_EVENTS_FILE, &mut events);
    verify(r == KESE_OK, 0x3e2);
    verify(events != 0, 0x3e3);
    let r = super::db_text::find_in_data_path(super::db_text::DB_EFFECTS_FILE, &mut effects);
    verify(r == KESE_OK, 0x3e5);
    verify(effects != 0, 0x3e6);
    let r = i64::from(ee_sim::scenario::event::ee_scenario_ev_load(events as *mut c_void, effects as *mut c_void));
    verify(r == KESE_OK, 0x3e8);
    h(O::FxFileSpecDelete, events, &[]);
    h(O::FxFileSpecDelete, effects, &[]);
    for (load, line) in TABLES_2 {
        load_table(k, load, line);
        k += 1;
    }
    let r = h(O::TerrainColorsLoad, 0, &[]);
    verify(r == KESE_OK, 0x40e);
    for (load, line) in TABLES_3 {
        load_table(k, load, line);
        k += 1;
    }
    let r = h(O::UiInitialize, 0, &[]);
    verify(r == KESE_OK, 0x47a);
    let directory = Owned::new(O::FileSpecNew, O::FxFileSpecDelete);
    // the user folders: data\Campaigns, data\Saved Games, data\Scenarios, Users, Users\default, its civilizations
    for (k, volume) in [(1usize, true), (2, true), (3, false), (4, false), (5, false)] {
        if volume {
            h(O::FileSpecSetVolumeStartup, directory.0, &[]);
        }
        set_startup_folder(directory.0, k);
        if h(O::FileSpecIsDirectory, directory.0, &[]) == 0 {
            h(O::FileSpecCreateDirectory, directory.0, &[]);
        }
    }
    let user = Owned::new(O::StrNew, O::StrFree);
    let r = h(O::ResString, user.0, &[IDS_CIVILIZATIONS]);
    verify(r == KESE_OK, 0x4a8);
    h(O::FileSpecSetUserCivilizationsDir, directory.0, &[pa(user.0)]);
    if h(O::FileSpecIsDirectory, directory.0, &[]) == 0 {
        h(O::FileSpecCreateDirectory, directory.0, &[]);
    }
    let mut file: P = 0;
    let r = super::db_text::find_in_data_path(super::db_text::DB_AMBIENT_SOUNDS_FILE, &mut file);
    verify(r == KESE_OK, 0x4b3);
    verify(file != 0, 0x4b4);
    h(O::AmbientLoad, file, &[]);
    h(O::FxFileSpecDelete, file, &[]);
    let alarm = hp(O::SoundObjectGet, 0, &[SOUND_HOT_KEY_ALARM]);
    if alarm != 0 {
        h(O::FxSoundObjectLoad, hp(O::SoundObjectGet, 0, &[SOUND_HOT_KEY_ALARM]), &[]);
    }
    h(O::GfxEffectsSortMedia, 0, &[]);
    // (EEFn0FCA98, which returns 0, is checked here: 0x4c1)
    create_plane_models();
    ee_gamecpp_startup_game_pools();
    super::db_text::ee_gamecpp_db_dump_rules();
    let r = register_shutdown(ee_gamecpp_startup_delete_database);
    verify(r == KESE_OK, 0x4cb);
}

/// `IDS_CIVILIZATIONS`, `kSoundHotKeyAlarm` (Empire Earth.cpp static_asserts them).
const IDS_CIVILIZATIONS: i64 = 29783;
const SOUND_HOT_KEY_ALARM: i64 = 0x3c2;

/// EEInitGamePools: the memory pools of the tile unit lists, sounds, doppelgangers and particles.
#[no_mangle]
pub extern "C" fn ee_gamecpp_startup_game_pools() {
    h(O::TileSetUnitListPool, 0, &[500, 100]);
    h(O::SoundsPoolInit, 0, &[500, 100, 10]);
    h(O::DoppelgangerSetPool, 0, &[500, 100]);
    h(O::DoppelgangerSetComplexPool, 0, &[100, 100]);
}

/// EEInitializeGameObjects: the random maps, the game options, the physics models and sight rings, the mouse, the
/// unit classes, the UI manager, the AI, the cheat codes and the game timer; their shutdown registered.
#[no_mangle]
pub extern "C" fn ee_gamecpp_startup_game_objects() {
    ee_sim::rmap::object::ee_rmap_initialize_generators();
    h(O::LobbyOptionsSetDefaults, 0, &[]);
    h(O::WorldOptionsSetDefaults, 0, &[]);
    h(O::InitPhysicsModels, 0, &[]);
    h(O::NewMouse, 0, &[]);
    verify(crate::gamecpp::globals::have!(gMouse), 0x4ee);
    let r = h(O::MouseLoad, 0, &[]);
    verify(r == KESE_OK, 0x4f2);
    h(O::MouseSetWindowBounds, 0, &[]);
    hf(O::MouseSetPositionXY, 0, &[], &[512.0, 384.0]);
    h(O::MouseSetSensitivityPref, 0, &[]);
    h(O::MouseSetPointer, 0, &[0]);
    h(O::CreateUnitClasses, 0, &[]);
    crate::tech_tree::objects::tree_walk(
        hp(O::TbGlobalTree, 0, &[]),
        ee_sim::tech::nodes::EeTechTreeOp::BuildUnitClassInfo,
        0,
        0,
    );
    h(O::NewUIManager, 0, &[]);
    verify(h(O::UiManagerPresent, 0, &[]) != 0, 0x50a);
    h(O::UiSetLeftHandedPref, 0, &[]);
    hf(O::UiSetBorderColors, 0, &[], &[0.34765625, 0.39453125, 0.390625, 0.69140625, 0.76171875, 0.7578125]);
    h(O::NewArtificialIntelligence, 0, &[]);
    verify(crate::gamecpp::globals::have!(gArtificialIntelligence), 0x514);
    h(O::NewCheatCodes, 0, &[]);
    verify(crate::gamecpp::globals::have!(gCheatCodes), 0x518);
    let timer = h(O::NewGameTimer, 0, &[]);
    GAME_TIMER.store(timer as usize, Ordering::Relaxed);
    verify(timer != 0, 1876);
    let r = register_shutdown(delete_game_objects);
    verify(r == KESE_OK, 0x51f);
}

/// EEDeleteGameObjects: the game timer, the AI, the cheat codes, the UI manager, the mouse and the mini map deleted
/// and the world and the game state cleared.
#[no_mangle]
pub extern "C" fn ee_gamecpp_startup_delete_game_objects() {
    h(O::DeleteGameTimer, GAME_TIMER.swap(0, Ordering::Relaxed) as P, &[]);
    h(O::ContinentsShutdown, 0, &[]);
    h(O::DeleteCheatCodes, 0, &[]);
    h(O::DeleteArtificialIntelligence, 0, &[]);
    h(O::DeleteUIManager, 0, &[]);
    h(O::MouseRelease, 0, &[]);
    h(O::DeleteMouse, 0, &[]);
    h(O::WorldClear, 0, &[]);
    if crate::gamecpp::globals::have!(gMiniMap) {
        h(O::MiniMapDelete, 0, &[]);
        super::globals::set!(gMiniMap, core::ptr::null_mut());
    }
    h(O::AeManagerClear, 0, &[]);
    h(O::GameStateClear, 0, &[]);
    h(O::FreePhysicsModels, 0, &[]);
    ee_sim::rmap::object::ee_rmap_generators_clear();
}

/// EEDeleteDatabase: every db table and its media unloaded, the calamity manager and the language resources
/// deleted, the pools freed.
#[no_mangle]
pub extern "C" fn ee_gamecpp_startup_delete_database(shutdown: bool) {
    h(O::PathManagerClear, 0, &[]);
    h(O::TileReleaseUnitListPool, 0, &[]);
    h(O::DoppelgangerRelease, 0, &[]);
    h(O::ReportLeakingUnits, 0, &[]);
    ee_sim::units::script::ee_units_script_delete_all();
    h(O::ReleaseUnitClasses, 0, &[]);
    let alarm = hp(O::SoundObjectGet, 0, &[962]);
    if alarm != 0 {
        h(O::FxSoundObjectUnload, hp(O::SoundObjectGet, 0, &[962]), &[]);
    }
    h(O::AnimalsUnload, 0, &[]);
    h(O::AmbientClearSounds, 0, &[]);
    ee_sim::combat::damage_table::ee_combat_damage_table_clear();
    h(O::CalamityClassesDestroy, 0, &[shutdown as i64]);
    h(O::DatabaseObjectsClear, 0, &[]);
    h(O::ButtonsUnload, 0, &[]);
    crate::preload_media::ee_preload_unload_effects();
    crate::preload_media::ee_preload_delete_media_table();
    h(O::MusicObjectsClear, 0, &[]);
    crate::preload_media::ee_preload_unload_sounds();
    for op in [
        O::ColorTableUnload,
        O::TargetingTableUnload,
        O::TerrainTableUnload,
        O::TerrainColorsUnload,
        O::TerrainTypesUnload,
        O::CliffTerrainUnload,
        O::UpgradeTableUnload,
        O::UnitBehaviorUnload,
        O::StartingResourcesUnload,
        O::GameVariantUnload,
        O::WorldTableClear,
    ] {
        h(op, 0, &[]);
    }
    crate::tech_tree::host::ee_gamecpp_tech_unload_all(hp(O::TbGlobalTree, 0, &[]) as *mut std::ffi::c_void);
    for op in [O::CalamityManagerDestroy, O::DeleteResource, O::ComplexUnitReleasePool, O::NodePoolDumpStats] {
        h(op, 0, &[]);
    }
}

/// EELoadSound: a .wav or .mp3 sound loaded and started (a bare name tries .wav, then .mp3), registered as a
/// one-shot when asked; the sample and the instance, -1 on failure.
///
/// # Safety
/// `name` is a UString, `sample` and `instance` the caller's.
#[no_mangle]
pub unsafe extern "C" fn ee_gamecpp_startup_load_sound(
    name: *const c_void,
    kind: c_ulong,
    sample: *mut c_ulong,
    instance: *mut c_ulong,
    flag: c_ulong,
) {
    let name = name as P;
    let mut s: c_ulong = c_ulong::MAX;
    let mut i: c_ulong = c_ulong::MAX;
    let none = |v: c_ulong| v as u32 == u32::MAX;
    if h(O::UStrHasSoundExtension, name, &[]) != 0 {
        s = h(O::SoundLoad, name, &[kind as i64]) as c_ulong;
        if !none(s) {
            i = h(O::SoundPlaySample, 0, &[s as i64]) as c_ulong;
            if none(i) {
                h(O::SoundUnloadSample, 0, &[s as i64]);
                s = c_ulong::MAX;
            } else if flag as u8 == 1 {
                h(O::SoundRegisterOneShot, 0, &[s as i64, i as i64]);
            }
        }
    } else {
        let other = Owned::new(O::UStrNew, O::UStrFree);
        for ext in [0i64, 1] {
            if ext == 1 && !none(s) {
                break;
            }
            h(O::UStrWithExtension, other.0, &[pa(name), ext]);
            // SAFETY: our own outputs
            unsafe { ee_gamecpp_startup_load_sound(other.0 as *const c_void, kind, &mut s, &mut i, flag) };
        }
    }
    // SAFETY: the caller's outputs
    unsafe {
        *sample = s;
        *instance = i;
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn strtoul_is_the_c_librarys() {
        assert_eq!(startup_strtoul(b"12345"), 12345);
        assert_eq!(startup_strtoul(b"  	+77xyz"), 77);
        assert_eq!(startup_strtoul(b"-1"), c_ulong::MAX);
        assert_eq!(startup_strtoul(b"abc"), 0);
        assert_eq!(startup_strtoul(b""), 0);
        assert_eq!(startup_strtoul(b"99999999999999999999999"), c_ulong::MAX);
    }

    #[test]
    fn font_slots_and_folders() {
        let mut slots: Vec<usize> = (0..FONTS.len()).map(|k| font_slot(k) as usize).collect();
        slots.sort_unstable();
        slots.dedup();
        assert_eq!(slots.len(), 38);
        assert_eq!(font_slot(0), core::ptr::addr_of_mut!(super::super::globals::gFontArial8));
        assert_eq!(font_slot(37), core::ptr::addr_of_mut!(super::super::globals::gFontTahoma72));
        assert_eq!(STARTUP_FOLDERS[0].to_bytes(), b"data\\");
        assert_eq!(STARTUP_FOLDERS[5].to_bytes(), b"Users\\default\\");
    }

    #[test]
    fn the_fonts_are_the_globals_order_and_lines() {
        assert_eq!(FONTS.len(), 38);
        for w in FONTS.windows(2) {
            assert!(w[1].3 > w[0].3);
        }
        let order: Vec<usize> = font_release_order().collect();
        assert_eq!(order.len(), 38);
        assert_eq!(&order[..5], &[18, 19, 20, 21, 0]);
        let mut sorted = order.clone();
        sorted.sort();
        assert_eq!(sorted, (0..38).collect::<Vec<_>>());
    }

    #[test]
    fn the_tables_are_twenty_six() {
        assert_eq!(TABLES_1.len() + TABLES_2.len() + TABLES_3.len(), 26);
    }
}
