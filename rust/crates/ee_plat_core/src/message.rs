//! `platform/PlatMessage.h`: a modal message to the user, a line for a developer, and the system facts the engine
//! asks about. Windows calls the Win32 functions the C++ called (`MessageBoxA`, `GetVersion`, `CreateMutexA`...);
//! POSIX has the installed handler, else stderr, and the locale table below.

use std::ffi::{c_char, c_int};

use crate::sim::PlatDebugGetHandler;

/// `PlatMessageHandler`: NULL is the default.
pub type PlatMessageHandler = Option<unsafe extern "C" fn(*const c_char, *const c_char, u32)>;

const PLAT_MESSAGE_ERROR: u32 = 0x10;

/// One line of a message: "error: title: text\n" (an error flag, a missing title or text is just dropped).
fn message_line(title: &[u8], text: &[u8], flags: u32) -> Vec<u8> {
    let mut line = Vec::with_capacity(title.len() + text.len() + 10);
    if flags & PLAT_MESSAGE_ERROR != 0 {
        line.extend_from_slice(b"error: ");
    }
    line.extend_from_slice(title);
    line.extend_from_slice(b": ");
    line.extend_from_slice(text);
    line.push(b'\n');
    line
}

/// The language prefixes of `PlatSystemLanguageId` on POSIX, in order (the first that the locale starts with).
#[cfg(any(unix, test))]
const LANGUAGES: [(&[u8], u16); 19] = [
    (b"zh_TW", 0x404),
    (b"zh_HK", 0xc04),
    (b"zh", 0x804),
    (b"de", 0x407),
    (b"en_GB", 0x809),
    (b"en", 0x409),
    (b"es", 0xc0a),
    (b"fr", 0x40c),
    (b"it", 0x410),
    (b"ja", 0x411),
    (b"ko", 0x412),
    (b"nl", 0x413),
    (b"pl", 0x415),
    (b"pt_BR", 0x416),
    (b"pt", 0x816),
    (b"ru", 0x419),
    (b"sv", 0x41d),
    (b"cs", 0x405),
    (b"hu", 0x40e),
];

/// A locale name's Windows LANGID (0x409 when unset, too short or unknown).
#[cfg(any(unix, test))]
fn language_id(locale: Option<&[u8]>) -> u16 {
    let Some(locale) = locale.filter(|l| l.len() >= 2) else {
        return 0x409;
    };
    LANGUAGES.iter().find(|(prefix, _)| locale.starts_with(prefix)).map_or(0x409, |&(_, id)| id)
}

/// FNV-1a, 32 bits (the machine id on POSIX).
#[cfg(any(unix, test))]
fn fnv1a(bytes: &[u8]) -> u32 {
    bytes.iter().fold(2_166_136_261, |hash, &b| (hash ^ b as u32).wrapping_mul(16_777_619))
}

/// The machine id of a machine-id file's first bytes: blanks at the end trimmed; None for an empty file.
#[cfg(any(all(unix, not(target_os = "macos")), test))]
fn machine_id_of(content: &[u8]) -> Option<u32> {
    let mut length = content.len().min(128);
    while length > 0 && matches!(content[length - 1], b'\n' | b'\r' | b' ') {
        length -= 1;
    }
    (length > 0).then(|| fnv1a(&content[..length]))
}

/// The lock file of `PlatSystemSingleInstance` on POSIX: "<dir>/<name>-<uid>.lock" (`dir`: TMPDIR or /tmp), and C's
/// 1023-byte cap of the C++ snprintf.
#[cfg(any(unix, test))]
fn single_instance_path(directory: Option<&[u8]>, name: &[u8], uid: u32) -> Vec<u8> {
    let directory = directory.filter(|d| !d.is_empty()).unwrap_or(b"/tmp");
    let mut path = directory.to_vec();
    path.push(b'/');
    path.extend_from_slice(name);
    path.extend_from_slice(format!("-{uid}.lock").as_bytes());
    path.truncate(1023);
    path
}

#[cfg(windows)]
mod imp {
    use super::*;
    use crate::sim::gPlatSimMode;
    use windows_sys::Win32::Foundation::GetLastError;
    use windows_sys::Win32::Globalization::GetUserDefaultLangID;
    use windows_sys::Win32::Storage::FileSystem::GetVolumeInformationA;
    use windows_sys::Win32::System::Com::CoUninitialize;
    use windows_sys::Win32::System::Diagnostics::Debug::OutputDebugStringA;
    use windows_sys::Win32::System::SystemInformation::{GetVersion, GetWindowsDirectoryA};
    use windows_sys::Win32::System::Threading::CreateMutexA;
    use windows_sys::Win32::UI::WindowsAndMessaging::MessageBoxA;

    extern "C" {
        fn __acrt_iob_func(index: u32) -> *mut libc::FILE;
        fn _splitpath(
            path: *const c_char,
            drive: *mut c_char,
            directory: *mut c_char,
            name: *mut c_char,
            extension: *mut c_char,
        );
    }

    pub fn language() -> u16 {
        // SAFETY: no arguments.
        unsafe { GetUserDefaultLangID() }
    }

    pub fn version() -> u32 {
        // SAFETY: no arguments.
        unsafe { GetVersion() }
    }

    pub fn shutdown() {
        // SAFETY: no arguments.
        unsafe { CoUninitialize() }
    }

    /// CDKey::GetMachineId: the volume of the Windows directory, its serial (0 when GetVolumeInformation fails).
    pub unsafe fn machine_id(ok: *mut c_int) -> u32 {
        const MAX_PATH: usize = 260;
        const MAX_DRIVE: usize = 3;
        const MAX_COMPONENT: usize = 256;
        let mut path = [0u8; MAX_PATH];
        let mut drive = [0u8; MAX_DRIVE];
        let mut directory = [0u8; MAX_COMPONENT];
        let mut name = [0u8; MAX_COMPONENT];
        let mut extension = [0u8; MAX_COMPONENT];
        if !ok.is_null() {
            *ok = 1;
        }
        GetWindowsDirectoryA(path.as_mut_ptr(), MAX_PATH as u32);
        _splitpath(
            path.as_ptr().cast(),
            drive.as_mut_ptr().cast(),
            directory.as_mut_ptr().cast(),
            name.as_mut_ptr().cast(),
            extension.as_mut_ptr().cast(),
        );
        let length = drive.iter().position(|&c| c == 0).unwrap_or(MAX_DRIVE);
        if length != 0 {
            let mut root = [0u8; MAX_DRIVE + 1];
            root[..length].copy_from_slice(&drive[..length]);
            if root[length - 1] != b'\\' {
                root[length] = b'\\';
            }
            let mut serial = 0u32;
            GetVolumeInformationA(
                root.as_ptr(),
                std::ptr::null_mut(),
                0,
                &mut serial,
                std::ptr::null_mut(),
                std::ptr::null_mut(),
                std::ptr::null_mut(),
                0,
            );
            return serial;
        }
        if !ok.is_null() {
            *ok = 0;
        }
        0
    }

    pub unsafe fn show(title: *const c_char, text: *const c_char, flags: u32) {
        if gPlatSimMode.get() != 0 {
            // the headless simulation: no modal box (it would wait for ever); C stderr, the game's bytes unconverted
            let line = message_line(crate::c_text::bytes(title), crate::c_text::bytes(text), flags);
            let stderr = __acrt_iob_func(2);
            libc::fwrite(line.as_ptr().cast(), 1, line.len(), stderr);
            libc::fflush(stderr);
            return;
        }
        MessageBoxA(std::ptr::null_mut(), text.cast(), title.cast(), flags);
    }

    pub fn set_handler(_handler: PlatMessageHandler) {}

    pub unsafe fn debug_output(text: *const c_char) {
        OutputDebugStringA(text.cast());
    }

    pub unsafe fn single_instance(name: *const c_char) -> c_int {
        let mutex = CreateMutexA(std::ptr::null(), 0, name.cast()); // held for the process's lifetime
        (!mutex.is_null() && GetLastError() == 0) as c_int
    }
}

#[cfg(unix)]
mod imp {
    use super::*;
    use std::sync::atomic::{AtomicUsize, Ordering};

    static HANDLER: AtomicUsize = AtomicUsize::new(0);

    fn handler() -> PlatMessageHandler {
        match HANDLER.load(Ordering::SeqCst) {
            0 => None,
            // SAFETY: only `set_handler` stores here, and only such a function pointer.
            f => Some(unsafe {
                std::mem::transmute::<usize, unsafe extern "C" fn(*const c_char, *const c_char, u32)>(f)
            }),
        }
    }

    fn write_stderr(bytes: &[u8]) {
        // SAFETY: `bytes` is readable for its length; the fd is stderr. C's stderr is unbuffered, so the two writers
        // of the process (this and the C++'s stdio) keep their order.
        unsafe { libc::write(2, bytes.as_ptr().cast(), bytes.len()) };
    }

    fn getenv(name: &std::ffi::CStr) -> Option<Vec<u8>> {
        // SAFETY: NUL-terminated name; the value is copied at once.
        let value = unsafe { crate::c_text::bytes(libc::getenv(name.as_ptr())) };
        (!value.is_empty()).then(|| value.to_vec())
    }

    pub fn language() -> u16 {
        let locale = [c"LC_ALL", c"LC_MESSAGES", c"LANG"].iter().find_map(|name| getenv(name));
        language_id(locale.as_deref())
    }

    pub fn version() -> u32 {
        0x0a00_000a
    }

    pub fn shutdown() {}

    pub unsafe fn machine_id(ok: *mut c_int) -> u32 {
        if !ok.is_null() {
            *ok = 0;
        }
        #[cfg(target_os = "macos")]
        {
            let mut uuid = [0u8; 16];
            let wait = libc::timespec { tv_sec: 1, tv_nsec: 0 };
            if libc::gethostuuid(uuid.as_mut_ptr(), &wait) != 0 {
                return 0;
            }
            if !ok.is_null() {
                *ok = 1;
            }
            fnv1a(&uuid)
        }
        #[cfg(not(target_os = "macos"))]
        {
            // systemd's id, else D-Bus's older copy of it
            for path in ["/etc/machine-id", "/var/lib/dbus/machine-id"] {
                let Ok(content) = std::fs::read(path) else {
                    continue;
                };
                let Some(id) = machine_id_of(&content) else {
                    continue;
                };
                if !ok.is_null() {
                    *ok = 1;
                }
                return id;
            }
            0
        }
    }

    pub unsafe fn show(title: *const c_char, text: *const c_char, flags: u32) {
        if let Some(handler) = handler() {
            handler(
                if title.is_null() { c"".as_ptr() } else { title },
                if text.is_null() { c"".as_ptr() } else { text },
                flags,
            );
            return;
        }
        write_stderr(&message_line(crate::c_text::bytes(title), crate::c_text::bytes(text), flags));
    }

    pub fn set_handler(handler: PlatMessageHandler) {
        HANDLER.store(handler.map_or(0, |f| f as usize), Ordering::SeqCst);
    }

    pub unsafe fn debug_output(text: *const c_char) {
        write_stderr(crate::c_text::bytes(text));
    }

    pub unsafe fn single_instance(name: *const c_char) -> c_int {
        let path = single_instance_path(getenv(c"TMPDIR").as_deref(), crate::c_text::bytes(name), libc::getuid());
        let path = crate::c_text::terminated(&path);
        let file = libc::open(path.as_ptr().cast(), libc::O_RDWR | libc::O_CREAT | libc::O_CLOEXEC, 0o600);
        if file < 0 {
            return 1; // no lock possible: let the game run
        }
        if libc::flock(file, libc::LOCK_EX | libc::LOCK_NB) != 0 {
            libc::close(file);
            return 0;
        }
        1 // kept open for the process's lifetime
    }
}

#[no_mangle]
pub extern "C" fn PlatSystemLanguageId() -> u16 {
    imp::language()
}

#[no_mangle]
pub extern "C" fn PlatSystemVersion() -> u32 {
    imp::version()
}

#[no_mangle]
pub extern "C" fn PlatSystemShutdown() {
    imp::shutdown()
}

#[no_mangle]
pub unsafe extern "C" fn PlatSystemMachineId(ok: *mut c_int) -> u32 {
    imp::machine_id(ok)
}

#[no_mangle]
pub unsafe extern "C" fn PlatSystemSingleInstance(name: *const c_char) -> c_int {
    imp::single_instance(name)
}

#[no_mangle]
pub unsafe extern "C" fn PlatMessageShow(title: *const c_char, text: *const c_char, flags: u32) {
    imp::show(title, text, flags)
}

#[no_mangle]
pub extern "C" fn PlatMessageSetHandler(handler: PlatMessageHandler) {
    imp::set_handler(handler)
}

#[no_mangle]
pub unsafe extern "C" fn PlatDebugOutput(text: *const c_char) {
    if let Some(handler) = PlatDebugGetHandler() {
        handler(text);
        return;
    }
    imp::debug_output(text)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::sim::PlatDebugSetHandler;
    use std::sync::Mutex;

    #[test]
    fn message_lines() {
        assert_eq!(message_line(b"T", b"body", 0), b"T: body\n");
        assert_eq!(message_line(b"T", b"body", 0x10), b"error: T: body\n");
        assert_eq!(message_line(b"", b"", 0x1000), b": \n");
    }

    #[test]
    fn language_ids_from_locale_names() {
        // golden from the C++ table
        assert_eq!(language_id(None), 0x409);
        assert_eq!(language_id(Some(b"x")), 0x409);
        assert_eq!(language_id(Some(b"xx_YY")), 0x409);
        assert_eq!(language_id(Some(b"ko_KR.UTF-8")), 0x412);
        assert_eq!(language_id(Some(b"en_GB")), 0x809);
        assert_eq!(language_id(Some(b"en_US.UTF-8")), 0x409);
        assert_eq!(language_id(Some(b"zh_TW")), 0x404);
        assert_eq!(language_id(Some(b"zh_HK")), 0xc04);
        assert_eq!(language_id(Some(b"zh_CN")), 0x804);
        assert_eq!(language_id(Some(b"pt_BR")), 0x416);
        assert_eq!(language_id(Some(b"pt_PT")), 0x816);
        for (prefix, id) in LANGUAGES {
            let mut name = prefix.to_vec();
            name.extend_from_slice(b"_x");
            assert_eq!(language_id(Some(&name)), id);
        }
    }

    #[test]
    fn fnv1a_golden() {
        assert_eq!(fnv1a(b""), 2_166_136_261);
        assert_eq!(fnv1a(b"a"), 0xe40c292c);
        assert_eq!(fnv1a(b"foobar"), 0xbf9cf968);
    }

    #[test]
    fn machine_id_content() {
        assert_eq!(machine_id_of(b"abc\n \r"), Some(fnv1a(b"abc")));
        assert_eq!(machine_id_of(b"\n"), None);
        assert_eq!(machine_id_of(b""), None);
        let long = vec![b'x'; 300];
        assert_eq!(machine_id_of(&long), Some(fnv1a(&long[..128])));
    }

    #[test]
    fn single_instance_paths() {
        assert_eq!(single_instance_path(None, b"game", 7), b"/tmp/game-7.lock");
        assert_eq!(single_instance_path(Some(b""), b"game", 7), b"/tmp/game-7.lock");
        assert_eq!(single_instance_path(Some(b"/run/u"), b"n", 0), b"/run/u/n-0.lock");
        assert_eq!(single_instance_path(None, &[b'a'; 2000], 1).len(), 1023);
    }

    static SEEN: Mutex<Vec<Vec<u8>>> = Mutex::new(Vec::new());

    unsafe extern "C" fn record_debug(text: *const c_char) {
        SEEN.lock().unwrap().push(crate::c_text::bytes(text).to_vec());
    }

    #[test]
    fn debug_output_goes_to_the_handler() {
        let _guard = crate::test_globals();
        SEEN.lock().unwrap().clear();
        PlatDebugSetHandler(Some(record_debug));
        unsafe {
            PlatDebugOutput(c"one".as_ptr());
            PlatDebugOutput(c"".as_ptr());
        }
        PlatDebugSetHandler(None);
        assert_eq!(*SEEN.lock().unwrap(), vec![b"one".to_vec(), b"".to_vec()]);
        // no handler: the default path must not crash
        unsafe { PlatDebugOutput(c"to the debugger or stderr\n".as_ptr()) };
    }

    #[test]
    fn system_facts() {
        let _ = PlatSystemLanguageId();
        assert_ne!(PlatSystemVersion(), 0);
        let mut ok = -1;
        let id = unsafe { PlatSystemMachineId(&mut ok) };
        assert_eq!(ok, 1, "machine id found");
        let _ = id;
    }

    #[cfg(windows)]
    mod windows {
        use super::*;
        use crate::sim::gPlatSimMode;
        use windows_sys::Win32::Foundation::GetLastError;

        #[test]
        fn simulation_mode_writes_no_message_box() {
            let _guard = crate::test_globals();
            gPlatSimMode.set(1);
            // a real MessageBoxA would block the test run for ever; the simulation branch writes one line to stderr
            unsafe { PlatMessageShow(c"title".as_ptr(), c"text".as_ptr(), 0x10) };
            gPlatSimMode.set(0);
        }

        #[test]
        fn single_instance_reports_a_second_copy() {
            let name = std::ffi::CString::new(format!("ee_plat_core_test_{}", std::process::id())).unwrap();
            assert_eq!(unsafe { PlatSystemSingleInstance(name.as_ptr()) }, 1);
            assert_eq!(unsafe { PlatSystemSingleInstance(name.as_ptr()) }, 0);
            assert_eq!(unsafe { GetLastError() }, 183); // ERROR_ALREADY_EXISTS
        }
    }

    #[cfg(unix)]
    mod unix {
        use super::*;
        use std::sync::atomic::{AtomicU32, Ordering};

        #[test]
        fn messages_go_to_the_handler() {
            static CALLS: AtomicU32 = AtomicU32::new(0);
            unsafe extern "C" fn handler(_title: *const c_char, _text: *const c_char, flags: u32) {
                assert_eq!(flags, 7);
                CALLS.fetch_add(1, Ordering::SeqCst);
            }
            let _guard = crate::test_globals();
            PlatMessageSetHandler(Some(handler));
            unsafe { PlatMessageShow(std::ptr::null(), c"x".as_ptr(), 7) };
            PlatMessageSetHandler(None);
            assert_eq!(CALLS.load(Ordering::SeqCst), 1);
        }

        #[test]
        fn single_instance_locks_a_file() {
            let _guard = crate::test_globals();
            let name = std::ffi::CString::new(format!("ee_plat_core_test_{}", std::process::id())).unwrap();
            assert_eq!(unsafe { PlatSystemSingleInstance(name.as_ptr()) }, 1);
        }
    }
}
