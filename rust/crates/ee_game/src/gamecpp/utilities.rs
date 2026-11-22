//! The game's helpers (game core lane, round 10, lane D; was EEGameUtilities.cpp's bodies): the local player's
//! sound, the load or sync failure's report (with the game state's checksum), the display devices' list, a graphic
//! object's frame sound, the camera-space point, the click confirmation effect, the ground or tile under a screen
//! point, the first selected unit and leaving the game. Each C++ call or member is a one-statement case of
//! EEGameUtilities.cpp's `EECore10OpD` ([`super::EeGameCpp`]'s `Du` ops).
//!
//! None of it is simulation code: the sounds, the report, the devices, the camera point and the effects run on the
//! UI and graphics threads (only a live game reaches them: the tour).
#![allow(clippy::not_unsafe_ptr_arg_deref, clippy::missing_safety_doc)]

use std::ffi::{c_char, c_long, c_ulong, c_void, CStr, CString};

use ee_base::math::types::Ge3DPoint;

use super::{cptr, h, hd, hf, hp, pa, ptr, Owned, O, P};

/// `kGraphicEffectCommandConfirmationMouseClickNoUnit`, `...Unit`.
const EFFECT_CLICK_NO_UNIT: i64 = 0xd1;
const EFFECT_CLICK_UNIT: i64 = 0xd3;
/// `kServerStateOpen`, `kServerStateRunning`, `kServerStatePaused`.
const SERVER_OPEN: i64 = 0;
const SERVER_RUNNING: i64 = 3;
const SERVER_PAUSED: i64 = 4;
/// The video memory assumed at least (three buffers of 640 x 480 x 16 bits, about).
const VIDEO_MEMORY_MINIMUM: c_ulong = 0x3e8000;

/// `GEVideoMode`.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeGamecppVideoMode {
    pub width: c_ulong,
    pub height: c_ulong,
    pub refresh_rate: c_ulong,
    pub bit_count: c_ulong,
    pub pixel_format: c_ulong,
}

fn c_text(t: P) -> Vec<u8> {
    if t == 0 {
        return Vec::new();
    }
    // SAFETY: a C string the C++ answered, read before its next call
    unsafe { CStr::from_ptr(t as *const c_char) }.to_bytes().to_vec()
}

fn c_string(bytes: &[u8]) -> CString {
    let end = bytes.iter().position(|&b| b == 0).unwrap_or(bytes.len());
    CString::new(&bytes[..end]).unwrap_or_default()
}

/// `EEPlayForLocalPlayer`: the sound played when the player is the local one; the instance, or -1.
#[no_mangle]
pub extern "C" fn ee_gamecpp_play_for_local(player: *mut c_void, sound: *mut c_void) -> c_ulong {
    if player as P == hp(O::DuLocalPlayer, 0, &[]) && !sound.is_null() {
        return h(O::DuPlaySound, sound as P, &[]) as c_ulong;
    }
    c_ulong::MAX
}

/// The text a report appends after the checksum: `ok`, else whether the errors are listed (verbose) or not.
pub fn checksum_verdict(ok: bool, verbose: bool) -> &'static [u8] {
    if ok {
        b" (checksum OK)"
    } else if verbose {
        b" (wrong checksum, see error log)"
    } else {
        b" (wrong checksum, game saved in release so extended info not available)"
    }
}

/// `EEReportLoadFailure`: the message in the main game form; with a checksum, the game state's checksum (under the
/// world's read lock when asked) and whether it matches, its differences logged in verbose mode.
#[no_mangle]
pub extern "C" fn ee_gamecpp_report_load_failure(message: *const c_char, lock: bool, checksum: *mut c_void) {
    let mut text = c_text(message as P);
    let expected = checksum as P;
    if expected != 0 {
        if lock {
            h(O::DuWorldReadLock, 0, &[]);
        }
        let state = hp(O::DuNewChecksum, expected, &[]);
        h(O::DuWorldChecksum, state, &[]);
        if lock {
            h(O::DuWorldReadUnlock, 0, &[]);
        }
        let total = h(O::DuChecksumTotal, state, &[]) as c_ulong;
        text.extend_from_slice(format!(" Game state checksum: {total} ").as_bytes());
        let errors = Owned::new(O::DuErrorsNew, O::DuErrorsDelete);
        let ok = h(O::DuChecksumCompare, expected, &[pa(state), pa(errors.0)]) != 0;
        let verbose = h(O::DuChecksumVerbose, state, &[]) != 0;
        text.extend_from_slice(checksum_verdict(ok, verbose));
        if !ok {
            let mut debug = text.clone();
            if verbose {
                debug.extend_from_slice(b" (wrong checksum, see following errors)\r\n");
            }
            debug_out(&debug);
            if verbose {
                for i in 0..h(O::DuErrorCount, errors.0, &[]) {
                    debug_out(&c_text(hp(O::DuErrorAt, errors.0, &[i])));
                }
                debug_out(b"End checksum errors");
            }
        }
        h(O::DuChecksumDelete, state, &[]);
    }
    if h(O::DuHaveUi, 0, &[]) != 0 {
        let text = c_string(&text);
        h(O::DuSetErrorString, 0, &[text.as_ptr() as usize as i64]);
    }
}

fn debug_out(text: &[u8]) {
    let text = c_string(text);
    h(O::DuDebugOut, 0, &[text.as_ptr() as usize as i64]);
}

/// Whether a video mode is listed: 16 bits (32 when the screen supports it), at least 640 x 480, not the last
/// mode's size and depth again.
pub fn mode_listed(mode: &EeGamecppVideoMode, last: &EeGamecppVideoMode, supports_32: bool) -> bool {
    (mode.bit_count == 16 || (mode.bit_count == 32 && supports_32))
        && mode.width >= 640
        && mode.height >= 480
        && (mode.bit_count != last.bit_count || mode.height != last.height || mode.width != last.width)
}

/// Whether three buffers of the mode fit the video memory (when it is known).
pub fn mode_fits(mode: &EeGamecppVideoMode, memory: Option<c_ulong>) -> bool {
    match memory {
        Some(m) => m >= (mode.bit_count / 8).wrapping_mul(mode.height).wrapping_mul(mode.width).wrapping_mul(3),
        None => true,
    }
}

/// A mode's name ("640 x 480 x 16 Bit").
pub fn mode_name(mode: &EeGamecppVideoMode) -> String {
    format!("{} x {} x {} Bit", mode.width, mode.height, mode.bit_count)
}

/// `EEEnumerateDisplayDevices`: every rasterizer that activates, its screens, and their 16-bit (and supported
/// 32-bit) modes of at least 640 x 480 whose three buffers fit the video memory; nothing while one is active.
#[no_mangle]
pub extern "C" fn ee_gamecpp_enumerate_display_devices() {
    let mut last = EeGamecppVideoMode {
        pixel_format: h(O::DuPixelFormatA8R8G8B8, 0, &[]) as c_ulong,
        ..EeGamecppVideoMode::default()
    };
    if hp(O::DuActiveRasterizer, 0, &[]) != 0 {
        return;
    }
    h(O::DuDevicesClear, 0, &[]);
    for i in 0..h(O::DuRasterizerCount, 0, &[]) {
        let device = hp(O::DuRasterizerAt, 0, &[i]);
        if h(O::DuActivate, device, &[]) != 0 {
            continue;
        }
        h(O::DuPushRasterizer, device, &[i]);
        for j in 0..h(O::DuScreenCount, device, &[]) {
            let screen = hp(O::DuScreenAt, device, &[j]);
            h(O::DuPushScreen, screen, &[j]);
            let mut memory: c_ulong = 0;
            let known = h(O::DuVideoMemory, device, &[j, ptr(&mut memory)]) == 0;
            if memory < VIDEO_MEMORY_MINIMUM {
                memory = VIDEO_MEMORY_MINIMUM;
            }
            for k in 0..h(O::DuModeCount, screen, &[]) {
                let mut mode = EeGamecppVideoMode::default();
                h(O::DuModeAt, screen, &[k, ptr(&mut mode)]);
                if mode_listed(&mode, &last, mode.bit_count == 32 && h(O::Du32Bit, screen, &[]) != 0)
                    && mode_fits(&mode, known.then_some(memory))
                {
                    let name = c_string(mode_name(&mode).as_bytes());
                    h(O::DuPushMode, 0, &[name.as_ptr() as usize as i64, cptr(&mode), k]);
                }
                last = mode;
            }
        }
        h(O::DuDeactivate, device, &[]);
    }
}

/// The volume, pan and pitch of a sound at a location (falloff 4).
fn parameters(sound: P, location: P) -> [f32; 3] {
    let mut out = [0f32; 3];
    let volume = hd(O::SoVolume, sound, &[], &[]);
    hf(O::DuSoundParameters, 0, &[pa(location), ptr(&mut out[0]), ptr(&mut out[1]), ptr(&mut out[2])], &[volume]);
    out
}

/// `PlayAnimationSound`: an animation frame's sound at a location started, restarted or updated; the referenced
/// instance now playing (the given one when nothing changes).
#[no_mangle]
pub extern "C" fn ee_gamecpp_play_animation_sound(
    location: *const c_void,
    graphic: *mut c_void,
    animation: c_long,
    node: *mut c_void,
    frame: c_ulong,
    start: bool,
) -> *mut c_void {
    let (loc, go, node) = (location as P, graphic as P, node as P);
    let mut result = node;
    if node != 0 {
        h(O::DuNodeReference, node, &[]);
    }
    let anim = animation as i64;
    if start {
        let sound = if frame as i64 == h(O::DuSound1Frame, go, &[anim]) {
            hp(O::DuSound1, go, &[anim])
        } else if frame as i64 != h(O::DuSound2Frame, go, &[anim]) {
            return result as *mut c_void;
        } else {
            hp(O::DuSound2, go, &[anim])
        };
        if sound == 0 {
            return result as *mut c_void;
        }
        if h(O::DuSoundSample, sound, &[]) == h(O::DuNodeSample, node, &[]) {
            let [v, pan, pitch] = parameters(sound, loc);
            if h(O::DuNodeLooping, node, &[]) != 0 {
                hf(O::DuNodeSetParameters, node, &[], &[v, pan, pitch]);
                return result as *mut c_void;
            }
            if node != 0 {
                h(O::NodeDereference, node, &[]);
            }
            result = hf(O::SoPlayNode, sound, &[], &[v, pan, pitch]) as P;
        } else {
            h(O::DuNodeStop, node, &[(h(O::DuNodeLooping, node, &[]) == 0) as i64]);
            // the reference taken above dropped, as the same-sample branch does
            if node != 0 {
                h(O::NodeDereference, node, &[]);
            }
            let [v, pan, pitch] = parameters(sound, loc);
            result = hf(O::SoPlayNode, sound, &[], &[v, pan, pitch]) as P;
        }
    } else if node != 0 {
        let sample = h(O::DuNodeSample, node, &[]);
        let mut sound = hp(O::DuSound1, go, &[anim]);
        if sound == 0 || sample != h(O::DuSoundSample, sound, &[]) {
            sound = hp(O::DuSound2, go, &[anim]);
            if sound == 0 || sample != h(O::DuSoundSample, sound, &[]) {
                sound = 0;
            }
        }
        if sound != 0 {
            let [v, pan, pitch] = parameters(sound, loc);
            hf(O::DuNodeSetParameters, node, &[], &[v, pan, pitch]);
        }
    }
    result as *mut c_void
}

/// The camera-space point divided by its depth (1 at depth 0).
pub fn by_depth(p: Ge3DPoint, depth: f32) -> Ge3DPoint {
    let scale = if depth != 0.0 { 1.0 / depth } else { 1.0 };
    Ge3DPoint::new(p.x * scale, p.y * scale, p.z * scale)
}

/// `EEGetCameraPoint`: a world point in camera space, divided by its depth, and the depth.
#[no_mangle]
pub extern "C" fn ee_gamecpp_camera_point(point: *const c_void, out: *mut c_void, depth: *mut f32) {
    let mut camera = Ge3DPoint::default();
    let mut normalized = Ge3DPoint::default();
    let mut d = 0f32;
    h(O::DuCameraInverseApply, point as P, &[ptr(&mut camera)]);
    h(O::DuCameraNormalize, 0, &[cptr(&camera), ptr(&mut normalized), ptr(&mut d)]);
    // SAFETY: the caller's GE3DPoint and float
    unsafe {
        *(out as *mut Ge3DPoint) = by_depth(normalized, d);
        *depth = d;
    }
}

/// `EEOrderAtCursor`: the click confirmation at the ground point under the cursor.
#[no_mangle]
pub extern "C" fn ee_gamecpp_order_at_cursor(screen: *const c_void, unit: *mut c_void) {
    let mut point = Ge3DPoint::new(0.0, 0.0, 0.0);
    let mut tile: P = 0;
    ee_gamecpp_point_unit_or_tile(
        screen,
        &mut tile as *mut P as *mut *mut c_void,
        &mut point as *mut _ as *mut c_void,
        true,
    );
    ee_gamecpp_make_click_effect(&point as *const _ as *const c_void, unit);
}

/// `EEMakeCalamityEffect`: the local-only click confirmation on a unit or on the ground.
#[no_mangle]
pub extern "C" fn ee_gamecpp_make_click_effect(location: *const c_void, unit: *mut c_void) {
    // SAFETY: the caller's GE3DPoint
    let mut at = unsafe { *(location as *const Ge3DPoint) };
    let u = unit as P;
    let graphic = if u == 0 {
        let graphic = hp(O::GfxEffectGet, 0, &[EFFECT_CLICK_NO_UNIT]);
        at.z = hd(O::MeshZ, 0, &[], &[at.x, at.y]);
        graphic
    } else {
        let graphic = hp(O::GfxEffectGet, 0, &[EFFECT_CLICK_UNIT]);
        if hp(O::UnTileNode, u, &[]) != 0 {
            h(O::DuCxPosition, hp(O::UnComplex, u, &[]), &[ptr(&mut at)]);
        } else {
            h(O::UnPosition, u, &[ptr(&mut at)]);
        }
        graphic
    };
    let mut handle: u32 = 0;
    h(O::EffAt, u, &[ptr(&mut at), pa(graphic), ptr(&mut handle), 0]);
    let flare = hp(O::EffGet, 0, &[handle as i64]);
    h(O::DuSetLocal, flare, &[]);
}

/// `EEGetFirstSelectedUnit`: the local player's first selected unit; false when nothing is selected.
#[no_mangle]
pub extern "C" fn ee_gamecpp_first_selected_unit(out: *mut *mut c_void) -> bool {
    let group = hp(O::DuSelectedGroup, 0, &[]);
    if h(O::DuGroupCount, group, &[]) == 0 {
        return false;
    }
    // SAFETY: the caller's out pointer
    unsafe { *out = hp(O::DuGroupUnit, group, &[0]) as *mut c_void };
    true
}

/// `EEPointUnitOrTileXY`: the tile under a screen point (the visible tile picked first); false when only the plain
/// map lookup hit.
#[no_mangle]
pub extern "C" fn ee_gamecpp_point_tile_xy(
    screen: *const c_void,
    tile: *mut *mut c_void,
    x: *mut c_long,
    y: *mut c_long,
    flag: bool,
) -> bool {
    let s = screen as usize as i64;
    let picked = hp(O::DuPickScreenTile, 0, &[s, pa(x as P), pa(y as P), flag as i64]);
    // SAFETY: the caller's out pointer
    unsafe { *tile = picked as *mut c_void };
    if picked != 0 {
        return true;
    }
    // SAFETY: as above
    unsafe { *tile = hp(O::DuTileAt, 0, &[s, pa(x as P), pa(y as P)]) as *mut c_void };
    false
}

/// `EEPointUnitOrTile`: the ground point under a screen point; without a pick the tile centre at terrain height,
/// and false.
#[no_mangle]
pub extern "C" fn ee_gamecpp_point_unit_or_tile(
    screen: *const c_void,
    tile: *mut *mut c_void,
    out: *mut c_void,
    flag: bool,
) -> bool {
    let s = screen as usize as i64;
    let at = hp(O::DuLocationAt, 0, &[s, out as usize as i64, flag as i64]);
    // SAFETY: the caller's out pointers
    unsafe { *tile = at as *mut c_void };
    if at != 0 {
        return true;
    }
    let (mut x, mut y): (c_long, c_long) = (0, 0);
    let t = hp(O::DuTileAt, 0, &[s, ptr(&mut x), ptr(&mut y)]);
    // SAFETY: as above
    unsafe {
        *tile = t as *mut c_void;
        let p = &mut *(out as *mut Ge3DPoint);
        p.x = x as f32 + 0.5;
        p.y = y as f32 + 0.5;
        p.z = hd(O::DuMapHeight, 0, &[out as usize as i64], &[]);
    }
    false
}

/// `EEExitGame`: a client removes itself (the server lost) or tells the server; the server posts the removal of
/// every player when the game is open, running or paused.
#[no_mangle]
pub extern "C" fn ee_gamecpp_exit_game() {
    if h(O::DuIsClient, 0, &[]) != 0 {
        if h(O::DuClientConnected, 0, &[]) != 0 {
            if h(O::DuServerLost, 0, &[]) != 0 {
                h(O::DuRemoveSelf, 0, &[]);
            } else {
                h(O::DuNotifyDisconnect, 0, &[]);
            }
        }
    } else {
        let state = h(O::DuServerState, 0, &[]);
        if state == SERVER_OPEN || state == SERVER_RUNNING || state == SERVER_PAUSED {
            let action = hp(O::DuNewRemoveAll, 0, &[]);
            h(O::DuPostAction, action, &[]);
            h(O::DuActionDereference, action, &[]);
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn mode(width: c_ulong, height: c_ulong, bit_count: c_ulong) -> EeGamecppVideoMode {
        EeGamecppVideoMode { width, height, bit_count, ..EeGamecppVideoMode::default() }
    }

    #[test]
    fn the_listed_modes_are_the_cpps() {
        let none = EeGamecppVideoMode::default();
        assert!(mode_listed(&mode(640, 480, 16), &none, false));
        assert!(!mode_listed(&mode(640, 480, 32), &none, false));
        assert!(mode_listed(&mode(640, 480, 32), &none, true));
        assert!(!mode_listed(&mode(320, 240, 16), &none, false));
        assert!(!mode_listed(&mode(800, 600, 16), &mode(800, 600, 16), false));
        assert!(mode_fits(&mode(800, 600, 16), Some(VIDEO_MEMORY_MINIMUM)));
        assert!(!mode_fits(&mode(1600, 1200, 32), Some(VIDEO_MEMORY_MINIMUM)));
        assert!(mode_fits(&mode(1600, 1200, 32), None));
        assert_eq!(mode_name(&mode(1024, 768, 32)), "1024 x 768 x 32 Bit");
        assert_eq!(std::mem::size_of::<EeGamecppVideoMode>(), 5 * std::mem::size_of::<c_ulong>());
    }

    #[test]
    fn the_report_and_the_depth_scale() {
        assert_eq!(checksum_verdict(true, true), b" (checksum OK)");
        assert_eq!(checksum_verdict(false, true), b" (wrong checksum, see error log)");
        assert_eq!(by_depth(Ge3DPoint::new(2.0, 4.0, 8.0), 2.0), Ge3DPoint::new(1.0, 2.0, 4.0));
        assert_eq!(by_depth(Ge3DPoint::new(2.0, 4.0, 8.0), 0.0), Ge3DPoint::new(2.0, 4.0, 8.0));
        assert_eq!(c_string(b"a\0b").as_bytes(), b"a");
    }
}
