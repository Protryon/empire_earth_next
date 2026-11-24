//! The POSIX entry point's own work (was Empire Earth.cpp's `main`, EEDataPresent, EEReportMissingData and
//! EESmokeTest): EE-SIM's argument test, the game's command line from the arguments, the data root's check and
//! entry, and `--smoke-test` (CI: the window system, a window, the renderer's wgpu device and 30 presented frames
//! read back, then whether the data root holds the game's data). The C++ `main` keeps the calls of the C++ entry
//! points (EESimMain, EERun, EESimGameEnded). Built on every platform; only the POSIX `main` calls it (Windows'
//! entry is WinMain).

use std::ffi::{c_char, c_int, CStr, CString};
use std::sync::Mutex;

/// The data root's text (`PlatPathGetDataRoot`) and whether it holds the game's data (a `Data\Db` folder).
fn data_present() -> (bool, String) {
    let mut root = [0u8; 260];
    // SAFETY: a PLAT_MAX_PATH buffer
    if unsafe { sys::PlatPathGetDataRoot(root.as_mut_ptr().cast(), root.len() as u32) } == 0 {
        return (false, String::new());
    }
    let root = CStr::from_bytes_until_nul(&root).map(|c| c.to_string_lossy().into_owned()).unwrap_or_default();
    let db = CString::new(format!("{root}Data\\Db")).unwrap_or_default();
    // SAFETY: a NUL-ended path
    let attributes = unsafe { sys::PlatPathGetAttributes(db.as_ptr()) };
    (attributes != 0xFFFF_FFFF && attributes & 0x10 != 0, root)
}

/// cbindgen:ignore
#[cfg(not(test))]
mod sys {
    use std::ffi::{c_char, c_int};
    use std::ffi::{c_ulong, c_void};

    // the platform's (ee_base) and the renderer's (ee_render gx ffi.rs) C entry points
    extern "C" {
        pub fn PlatPathGetAttributes(path: *const c_char) -> u32;
        pub fn PlatPathGetDataRoot(out: *mut c_char, size: u32) -> u32;
        pub fn PlatPathEnterDataRoot() -> c_int;
        pub fn PlatMessageShow(title: *const c_char, text: *const c_char, flags: u32);
    }
    extern "C" {
        pub fn PlatSleep(milliseconds: u32);
        pub fn PlatWindowSetAppName(name: *const c_char, icon_resource: u32);
        pub fn PlatWindowSystemStart() -> c_int;
        pub fn PlatWindowSystemStop();
        pub fn PlatWindowLastError() -> *const c_char;
        pub fn PlatWindowCreate(title: *const c_char, width: i32, height: i32, flags: u32) -> *mut c_void;
        pub fn PlatWindowDestroy(window: *mut c_void);
        pub fn PlatWindowPollEvent(out: *mut ee_base::plat_ui::event::PlatInputEvent) -> c_int;
        pub fn gx_direct_draw_create(out: *mut *mut c_void) -> GxRes;
        pub fn gx_direct_draw_release(dd: *mut c_void) -> c_ulong;
        pub fn gx_direct_draw_set_cooperative_level(dd: *mut c_void, window: *mut c_void, flags: c_ulong) -> GxRes;
        pub fn gx_direct_draw_create_surface(
            dd: *mut c_void,
            desc: *mut ee_render::gx::abi::GxSurfaceDesc,
            out: *mut *mut c_void,
        ) -> GxRes;
        pub fn gx_surface_release(surface: *mut c_void) -> c_ulong;
        pub fn gx_surface_blt(
            surface: *mut c_void,
            destination: *const ee_render::gx::abi::Rect,
            source: *mut c_void,
            source_rect: *const ee_render::gx::abi::Rect,
            flags: c_ulong,
            fx: *const ee_render::gx::abi::GxBltFx,
        ) -> GxRes;
        pub fn gx_surface_lock(
            surface: *mut c_void,
            rect: *const ee_render::gx::abi::Rect,
            out: *mut ee_render::gx::abi::GxSurfaceDesc,
            flags: c_ulong,
        ) -> GxRes;
        pub fn gx_surface_unlock(surface: *mut c_void) -> GxRes;
    }
    type GxRes = ee_render::gx::abi::GxResult;
}

/// The tests' platform: no data root, no messages.
#[cfg(test)]
#[allow(non_snake_case)]
mod sys {
    use std::ffi::{c_char, c_int};

    pub unsafe fn PlatPathGetAttributes(_: *const c_char) -> u32 {
        0xFFFF_FFFF
    }
    pub unsafe fn PlatPathGetDataRoot(_: *mut c_char, _: u32) -> u32 {
        0
    }
    pub unsafe fn PlatPathEnterDataRoot() -> c_int {
        0
    }
    pub unsafe fn PlatMessageShow(_: *const c_char, _: *const c_char, _: u32) {}
}

/// The message box's error text when the data root has no game data.
pub fn missing_data_text(root: &str) -> String {
    format!(
        "Empire Earth: The Art of Conquest: game data not found in {} (no Data\\Db folder there).\nCopy the installed \
         game (its Data and Users folders) there, or point EE_DATA_ROOT at a copy of it: see README.md, 'Linux and \
         macOS'.",
        if root.is_empty() { "the data root" } else { root }
    )
}

fn message(text: &str) {
    let title = c"Empire Earth";
    let text = CString::new(text.replace('\0', "")).unwrap_or_default();
    // SAFETY: two NUL-ended texts; PLAT_MESSAGE_ERROR
    unsafe { sys::PlatMessageShow(title.as_ptr(), text.as_ptr(), 0x10) };
}

/// EE-SIM's arguments: the count to skip before EESimMain's (1 for `--sim`, 0 for an executable named `ee-sim*`),
/// or -1 for the game.
pub fn sim_skip(args: &[&[u8]]) -> c_int {
    let name = args.first().copied().unwrap_or(b"");
    let name = name.rsplit(|b| *b == b'/').next().unwrap_or(name);
    let sim_flag = args.len() > 1 && args[1] == b"--sim";
    if sim_flag {
        1
    } else if name.starts_with(b"ee-sim") {
        0
    } else {
        -1
    }
}

/// What the arguments ask: the smoke test, or the game with the command line WinMain would get (the arguments after
/// the program's name, joined by spaces).
#[derive(Debug, PartialEq, Eq)]
pub enum Start {
    SmokeTest,
    Game(Vec<u8>),
}

pub fn start_of(args: &[&[u8]]) -> Start {
    let mut line = Vec::new();
    for (i, a) in args.iter().enumerate().skip(1) {
        if *a == b"--smoke-test" {
            return Start::SmokeTest;
        }
        if i > 1 {
            line.push(b' ');
        }
        line.extend_from_slice(a);
    }
    Start::Game(line)
}

/// The command line the game runs with (kept for the process's life: EERun's argument).
static LINE: Mutex<Vec<u8>> = Mutex::new(Vec::new());

/// The POSIX `main`'s start after EE-SIM's turn: -1 and the command line in `out_line` to run the game (EERun);
/// else the exit code (the smoke test's, 3 for no game data, 1 when the data root cannot be entered).
///
/// # Safety
/// `argv` holds `argc` NUL-ended arguments; `out_line` is writable.
#[no_mangle]
pub unsafe extern "C" fn ee_gamecpp_posix_start(
    argc: c_int,
    argv: *const *const c_char,
    out_line: *mut *mut c_char,
) -> c_int {
    // SAFETY: the caller's arguments
    let args: Vec<&[u8]> = (0..argc.max(0) as usize)
        .map(|i| unsafe { *argv.add(i) })
        .map(|a| if a.is_null() { &b""[..] } else { unsafe { CStr::from_ptr(a) }.to_bytes() })
        .collect();
    let line = match start_of(&args) {
        Start::SmokeTest => return smoke_test(),
        Start::Game(line) => line,
    };
    let (present, root) = data_present();
    if !present {
        message(&missing_data_text(&root));
        return 3;
    }
    // SAFETY: the platform's data root
    if unsafe { sys::PlatPathEnterDataRoot() } == 0 {
        message("The game's data directory could not be entered (EE_DATA_ROOT).");
        return 1;
    }
    let mut kept = LINE.lock().unwrap_or_else(|e| e.into_inner());
    *kept = line;
    kept.push(0);
    // SAFETY: the caller's out pointer; the buffer lives in a static for the process's life
    unsafe { *out_line = kept.as_mut_ptr().cast() };
    -1
}

/// EE-SIM's argument test for the POSIX `main` (see [`sim_skip`]).
///
/// # Safety
/// `argv` holds `argc` NUL-ended arguments.
#[no_mangle]
pub unsafe extern "C" fn ee_gamecpp_posix_sim_skip(argc: c_int, argv: *const *const c_char) -> c_int {
    // SAFETY: the caller's arguments
    let args: Vec<&[u8]> = (0..argc.max(0) as usize)
        .map(|i| unsafe { *argv.add(i) })
        .map(|a| if a.is_null() { &b""[..] } else { unsafe { CStr::from_ptr(a) }.to_bytes() })
        .collect();
    sim_skip(&args)
}

#[cfg(not(test))]
fn smoke_test() -> c_int {
    use ee_render::gx::abi::*;
    use std::ffi::c_void;
    use sys as gx;
    use sys as plat;

    const GXSCAPS_OFFSCREENPLAIN: u32 = 0x40;
    const GXSCL_NORMAL: u32 = 0x08;
    let last_error = || {
        // SAFETY: the window system's last error text
        unsafe { CStr::from_ptr(plat::PlatWindowLastError()) }.to_string_lossy().into_owned()
    };
    // SAFETY: the platform's window and renderer C entry points, in the order the C++ called them
    unsafe {
        let (present, root) = data_present();
        plat::PlatWindowSetAppName(c"Empire Earth Expansion".as_ptr(), 0);
        if plat::PlatWindowSystemStart() == 0 {
            eprintln!("smoke test: no window system: {}", last_error());
            return 2;
        }
        let window = plat::PlatWindowCreate(c"Empire Earth: The Art of Conquest (smoke test)".as_ptr(), 640, 480, 0);
        if window.is_null() {
            eprintln!("smoke test: no window: {}", last_error());
            return 2;
        }
        let mut dd: *mut c_void = std::ptr::null_mut();
        if gx::gx_direct_draw_create(&mut dd) != GX_OK {
            eprintln!("smoke test: no wgpu device (no GPU adapter)");
            return 2;
        }
        gx::gx_direct_draw_set_cooperative_level(dd, window.cast(), GXSCL_NORMAL as _);
        let surface = |caps: u32, width: u32, height: u32| {
            let mut desc = GxSurfaceDesc {
                dwSize: std::mem::size_of::<GxSurfaceDesc>() as _,
                dwFlags: (GXSD_CAPS | GXSD_WIDTH | GXSD_HEIGHT | GXSD_PIXELFORMAT) as _,
                dwWidth: width as _,
                dwHeight: height as _,
                ddpfPixelFormat: GxPixelFormat::rgb(32, 0xff_0000, 0xff00, 0xff),
                ..GxSurfaceDesc::default()
            };
            desc.ddsCaps.dwCaps = caps as _;
            let mut s: *mut c_void = std::ptr::null_mut();
            if gx::gx_direct_draw_create_surface(dd, &mut desc, &mut s) != GX_OK {
                std::ptr::null_mut()
            } else {
                s
            }
        };
        let primary = surface(GXSCAPS_PRIMARYSURFACE, 0, 0);
        let back = surface(GXSCAPS_OFFSCREENPLAIN | GXSCAPS_3DDEVICE, 640, 480);
        if primary.is_null() || back.is_null() {
            eprintln!("smoke test: FAIL surfaces");
            return 1;
        }
        let mut failed = false;
        for frame in 0..30u32 {
            let fill = GxBltFx {
                dwSize: std::mem::size_of::<GxBltFx>() as _,
                dwFillColor: (0x0010_2030u32.wrapping_mul(frame % 8)) as _,
            };
            gx::gx_surface_blt(
                back,
                std::ptr::null(),
                std::ptr::null_mut(),
                std::ptr::null(),
                GXBLT_COLORFILL as _,
                &fill,
            );
            let mut locked =
                GxSurfaceDesc { dwSize: std::mem::size_of::<GxSurfaceDesc>() as _, ..GxSurfaceDesc::default() };
            if gx::gx_surface_lock(back, std::ptr::null(), &mut locked, 0) == GX_OK {
                let pixel = *((locked.lpSurface as *const u8).offset(10 * locked.lPitch as isize + 40) as *const u32);
                gx::gx_surface_unlock(back);
                if pixel & 0xff_ffff != fill.dwFillColor as u32 & 0xff_ffff {
                    failed = true;
                }
            } else {
                failed = true;
            }
            let all = Rect::whole(640, 480);
            gx::gx_surface_blt(primary, &all, back, std::ptr::null(), 0, std::ptr::null()); // presents into the window
            let mut event = std::mem::zeroed();
            while plat::PlatWindowPollEvent(&mut event) != 0 {}
            plat::PlatSleep(16);
        }
        gx::gx_surface_release(back);
        gx::gx_surface_release(primary);
        gx::gx_direct_draw_release(dd);
        plat::PlatWindowDestroy(window);
        println!(
            "smoke test: window, wgpu device, 30 frames presented and read back{}",
            if failed { ": FAIL" } else { "" }
        );
        if present {
            println!("smoke test: the game's data is in {root}");
        } else {
            println!(
                "smoke test: game data not found in {} (EE_DATA_ROOT; README.md 'Linux and macOS')",
                if root.is_empty() { "the data root" } else { &root }
            );
        }
        plat::PlatWindowSystemStop();
        c_int::from(failed)
    }
}

/// EEMenuWidth: the menus' width at the window's size: the window's when it is at most 768 high, else the width of the
/// window's shape at 768 high (rounded, even), at least the original's 1024 (a 5:4 screen: 1024x768).
#[no_mangle]
pub extern "C" fn ee_gamecpp_menu_width(width: std::ffi::c_ulong, height: std::ffi::c_ulong) -> std::ffi::c_ulong {
    if height <= 768 || height == 0 {
        return width;
    }
    let w = (width.wrapping_mul(768).wrapping_add(height / 2) / height) & !1;
    w.max(1024)
}

#[cfg(test)]
fn smoke_test() -> c_int {
    9
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_menu_width() {
        assert_eq!(ee_gamecpp_menu_width(1024, 768), 1024);
        assert_eq!(ee_gamecpp_menu_width(1920, 1080), 1364); // 1365.8 truncated, even
        assert_eq!(ee_gamecpp_menu_width(1280, 1024), 1024); // 960 -> at least 1024
        assert_eq!(ee_gamecpp_menu_width(800, 0), 800);
    }

    #[test]
    fn sim_skip_is_the_flag_or_the_name() {
        assert_eq!(sim_skip(&[b"./ee-aoc", b"--sim", b"--seed"]), 1);
        assert_eq!(sim_skip(&[b"/opt/x/ee-sim", b"--seed"]), 0);
        assert_eq!(sim_skip(&[b"ee-sim-asan"]), 0);
        assert_eq!(sim_skip(&[b"/bin/ee-aoc", b"-x"]), -1);
        assert_eq!(sim_skip(&[]), -1);
    }

    #[test]
    fn the_command_line_joins_the_arguments() {
        assert_eq!(start_of(&[b"ee-aoc"]), Start::Game(Vec::new()));
        assert_eq!(start_of(&[b"ee-aoc", b"/a", b"b c"]), Start::Game(b"/a b c".to_vec()));
        assert_eq!(start_of(&[b"ee-aoc", b"/a", b"--smoke-test"]), Start::SmokeTest);
    }

    #[test]
    fn the_missing_data_text_names_the_root() {
        assert!(
            missing_data_text("/x/").starts_with("Empire Earth: The Art of Conquest: game data not found in /x/ (no")
        );
        assert!(
            missing_data_text("").contains("found in the data root (no Data\\Db folder there).\nCopy the installed")
        );
    }

    #[test]
    fn the_smoke_test_and_no_data() {
        let args = [c"ee-aoc".as_ptr(), c"--smoke-test".as_ptr()];
        let mut line = std::ptr::null_mut();
        assert_eq!(unsafe { ee_gamecpp_posix_start(2, args.as_ptr(), &mut line) }, 9);
        assert_eq!(unsafe { ee_gamecpp_posix_sim_skip(2, args.as_ptr()) }, -1);
    }
}
