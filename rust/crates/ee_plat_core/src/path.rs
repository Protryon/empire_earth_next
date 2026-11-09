//! `platform/PlatPath.h`: the game's Windows-form paths on the native file system, and the data root. Windows: the
//! paths are native already, the conversions copy. POSIX: `resolve` and `game_form` below, on the same logic as the
//! C++ they replace (volumes, case-insensitive lookup, dots and trailing blanks trimmed as Win32 does).

use std::ffi::{c_char, c_int};

// ---- POSIX: the game's Windows paths on the native file system. Pure over `NativeFs`, so it is tested anywhere.

/// Win32 error codes this module's POSIX side records (the values `PlatGetLastError` reports on every system).
#[cfg(any(unix, test))]
mod errors {
    pub(super) const PATH_NOT_FOUND: u32 = 3;
    pub(super) const INVALID_PARAMETER: u32 = 87;
    pub(super) const FILENAME_TOO_LONG: u32 = 206;
}

/// How long a game path may be (`PLAT_MAX_PATH`, platform/PlatFile.h).
#[cfg(any(unix, test))]
const PLAT_MAX_PATH: usize = 260;

/// What `resolve` needs from the file system, so that its logic can be tested alone.
#[cfg(any(unix, test))]
pub(crate) trait NativeFs {
    /// The path exists (`lstat`).
    fn exists(&self, path: &[u8]) -> bool;
    /// The entry of `directory` whose name equals `name` ignoring ASCII case, the exact name when several match.
    fn find_no_case(&self, directory: &[u8], name: &[u8]) -> Option<Vec<u8>>;
    /// The current directory, or errno.
    fn current_directory(&self) -> Result<Vec<u8>, c_int>;
}

/// The volume roots: for each letter a native directory, empty for "/" (`PlatPathSetVolumeRoot`).
#[cfg(any(unix, test))]
pub(crate) struct Volumes([Vec<u8>; 26]);

#[cfg(any(unix, test))]
impl Volumes {
    pub(crate) const fn new() -> Self {
        Self([const { Vec::new() }; 26])
    }

    fn index(volume: u8) -> usize {
        (volume.to_ascii_uppercase() - b'A') as usize
    }

    fn root(&self, volume: u8) -> Vec<u8> {
        let root = &self.0[Self::index(volume)];
        if root.is_empty() {
            b"/".to_vec()
        } else {
            root.clone()
        }
    }

    pub(crate) fn set(&mut self, volume: u8, root: &[u8]) {
        self.0[Self::index(volume)] = root.to_vec();
    }
}

#[cfg(any(unix, test))]
#[derive(Debug, PartialEq, Eq)]
pub(crate) enum Failure {
    /// A Win32 error code.
    Code(u32),
    Errno(c_int),
}

#[cfg(any(unix, test))]
fn is_separator(c: u8) -> bool {
    c == b'\\' || c == b'/'
}

#[cfg(any(unix, test))]
fn join(directory: &[u8], name: &[u8]) -> Vec<u8> {
    let mut out = directory.to_vec();
    if out.last() != Some(&b'/') {
        out.push(b'/');
    }
    out.extend_from_slice(name);
    out
}

/// Splits `text` into components: "." goes, ".." removes the previous component (never the base), and trailing dots
/// and spaces are trimmed from a name, as Win32 trims them ("file." is "file").
#[cfg(any(unix, test))]
fn split_into(text: &[u8], components: &mut Vec<Vec<u8>>) {
    for raw in text.split(|&c| is_separator(c)) {
        if raw.is_empty() || raw == b"." {
            continue;
        }
        if raw == b".." {
            components.pop();
            continue;
        }
        let end = raw.iter().rposition(|&c| c != b'.' && c != b' ').map_or(0, |last| last + 1);
        if end > 0 {
            components.push(raw[..end].to_vec());
        }
    }
}

/// The native path of a game path, the missing tail kept as written (`PlatPathToNative`).
#[cfg(any(unix, test))]
pub(crate) fn resolve(input: &[u8], volumes: &Volumes, fs: &impl NativeFs) -> Result<Vec<u8>, Failure> {
    if input.is_empty() {
        return Err(Failure::Code(errors::PATH_NOT_FOUND));
    }
    if input.len() >= PLAT_MAX_PATH {
        return Err(Failure::Code(errors::FILENAME_TOO_LONG));
    }
    let mut components = Vec::new();
    let mut rest = input;
    let base;
    if input.len() >= 2 && input[0].is_ascii_alphabetic() && input[1] == b':' {
        base = volumes.root(input[0]);
        rest = &input[2..];
    } else if input.len() >= 2 && is_separator(input[0]) && is_separator(input[1]) {
        return Err(Failure::Code(errors::PATH_NOT_FOUND)); // UNC paths have no native meaning here
    } else if input[0] == b'/' {
        base = b"/".to_vec();
    } else if input[0] == b'\\' {
        base = volumes.root(b'C');
    } else {
        let current = fs.current_directory().map_err(Failure::Errno)?;
        base = b"/".to_vec();
        split_into(&current, &mut components);
    }
    split_into(rest, &mut components);
    let mut current = base;
    let mut missing = false;
    for component in &components {
        let candidate = join(&current, component);
        if missing || fs.exists(&candidate) {
            current = candidate;
        } else if let Some(name) = fs.find_no_case(&current, component) {
            current = join(&current, &name);
        } else {
            missing = true;
            current = candidate;
        }
    }
    Ok(current)
}

/// The game form of a native absolute path: the volume whose root is the longest prefix, C: first among equals
/// (`PlatPathFromNative`).
#[cfg(any(unix, test))]
pub(crate) fn game_form(native: &[u8], volumes: &Volumes) -> Result<Vec<u8>, Failure> {
    if native.first() != Some(&b'/') {
        return Err(Failure::Code(errors::INVALID_PARAMETER));
    }
    let mut best: Option<(u8, usize)> = None;
    for i in 0..26u8 {
        let volume = (i + 2) % 26; // C, D, ..., Z, A, B
        let mut root = volumes.root(b'A' + volume);
        if root.len() > 1 && root.last() == Some(&b'/') {
            root.pop();
        }
        let is_root = root == b"/";
        let matches = is_root
            || native == root.as_slice()
            || (native.starts_with(&root) && native.get(root.len()) == Some(&b'/'));
        let length = if is_root { 0 } else { root.len() };
        if matches && best.is_none_or(|(_, longest)| length > longest) {
            best = Some((volume, length));
        }
    }
    let Some((volume, length)) = best else {
        return Err(Failure::Code(errors::PATH_NOT_FOUND));
    };
    let mut out = vec![b'A' + volume, b':'];
    let rest = &native[length..];
    if rest.first() != Some(&b'/') {
        out.push(b'\\');
    }
    out.extend(rest.iter().map(|&c| if c == b'/' { b'\\' } else { c }));
    Ok(out)
}

/// The volume roots a game path resolves against (`PlatPathSetVolumeRoot`).
#[cfg(unix)]
static VOLUMES: std::sync::Mutex<Volumes> = std::sync::Mutex::new(Volumes::new());

/// The real file system, for `resolve`.
#[cfg(unix)]
struct RealFs;

#[cfg(unix)]
impl NativeFs for RealFs {
    fn exists(&self, path: &[u8]) -> bool {
        std::fs::symlink_metadata(imp::os_path(path)).is_ok()
    }

    fn find_no_case(&self, directory: &[u8], name: &[u8]) -> Option<Vec<u8>> {
        use std::os::unix::ffi::OsStrExt;
        let entries = std::fs::read_dir(imp::os_path(directory)).ok()?;
        let mut found = None;
        for entry in entries.flatten() {
            let entry_name = entry.file_name();
            let bytes = entry_name.as_bytes();
            if bytes.eq_ignore_ascii_case(name) {
                found = Some(bytes.to_vec());
                if bytes == name {
                    break;
                }
            }
        }
        found
    }

    fn current_directory(&self) -> Result<Vec<u8>, c_int> {
        use std::os::unix::ffi::OsStringExt;
        std::env::current_dir().map(|p| p.into_os_string().into_vec()).map_err(|e| e.raw_os_error().unwrap_or(0))
    }
}

/// `PlatPathToNative` for Rust callers (PlatFile, `tools`): the native path of a game path; the failure is recorded
/// as the last error.
#[cfg(unix)]
pub fn resolve_path(input: &[u8]) -> Option<Vec<u8>> {
    let volumes = VOLUMES.lock().unwrap();
    match resolve(input, &volumes, &RealFs) {
        Ok(native) => Some(native),
        Err(failure) => {
            imp::record(&failure);
            None
        }
    }
}

// ---- the C interface

#[cfg(windows)]
mod imp {
    use super::*;
    use windows_sys::Win32::Foundation::{
        SetLastError, ERROR_FILENAME_EXCED_RANGE, ERROR_INVALID_PARAMETER, ERROR_NOT_SUPPORTED, ERROR_PATH_NOT_FOUND,
        MAX_PATH,
    };
    use windows_sys::Win32::System::Environment::{GetEnvironmentVariableA, SetCurrentDirectoryA};
    use windows_sys::Win32::System::LibraryLoader::GetModuleFileNameA;

    const MAX: usize = MAX_PATH as usize;

    /// C++'s CopyPath: a whole path into `out`, or 0 with the last error set.
    unsafe fn copy_path(text: &[u8], out: *mut c_char, size: u32) -> u32 {
        if text.is_empty() || text.len() + 1 > size as usize {
            SetLastError(if text.is_empty() { ERROR_PATH_NOT_FOUND } else { ERROR_FILENAME_EXCED_RANGE });
            return 0;
        }
        std::ptr::copy_nonoverlapping(text.as_ptr(), out.cast::<u8>(), text.len());
        *out.add(text.len()) = 0;
        text.len() as u32
    }

    unsafe fn copy_c_path(text: *const c_char, out: *mut c_char, size: u32) -> u32 {
        if text.is_null() || out.is_null() {
            SetLastError(ERROR_INVALID_PARAMETER);
            return 0;
        }
        copy_path(crate::c_text::bytes(text), out, size)
    }

    /// The executable's full path, or None (GetModuleFileNameA failed, or it needs more than MAX_PATH).
    unsafe fn module_file_name() -> Option<Vec<u8>> {
        let mut buffer = [0u8; MAX];
        let length = GetModuleFileNameA(std::ptr::null_mut(), buffer.as_mut_ptr(), MAX_PATH) as usize;
        (length != 0 && length < MAX).then(|| buffer[..length].to_vec())
    }

    pub unsafe fn to_native(input: *const c_char, out: *mut c_char, size: u32) -> u32 {
        copy_c_path(input, out, size)
    }

    pub unsafe fn from_native(input: *const c_char, out: *mut c_char, size: u32) -> u32 {
        copy_c_path(input, out, size)
    }

    pub unsafe fn set_volume_root(_volume: c_char, _root: *const c_char) -> c_int {
        SetLastError(ERROR_NOT_SUPPORTED);
        0
    }

    pub unsafe fn get_data_root(out: *mut c_char, size: u32) -> u32 {
        let mut env = [0u8; MAX];
        let length = GetEnvironmentVariableA(c"EE_DATA_ROOT".as_ptr().cast(), env.as_mut_ptr(), MAX_PATH) as usize;
        if length > 0 && length < MAX - 1 {
            if env[length - 1] != b'\\' && env[length - 1] != b'/' {
                env[length] = b'\\';
                return copy_path(&env[..=length], out, size);
            }
            return copy_path(&env[..length], out, size);
        }
        let Some(module) = module_file_name() else {
            return 0;
        };
        let Some(slash) = module.iter().rposition(|&c| c == b'\\') else {
            return 0;
        };
        copy_path(&module[..=slash], out, size)
    }

    pub unsafe fn enter_data_root() -> c_int {
        let mut root = [0u8; MAX];
        if get_data_root(root.as_mut_ptr().cast(), MAX_PATH) == 0 {
            return 0;
        }
        (SetCurrentDirectoryA(root.as_ptr()) != 0) as c_int
    }

    pub unsafe fn get_executable_name(out: *mut c_char, size: u32) -> u32 {
        let Some(module) = module_file_name() else {
            return 0;
        };
        let start = module.iter().rposition(|&c| c == b'\\').map_or(0, |slash| slash + 1);
        let name = &module[start..];
        if name.len() + 1 > size as usize {
            return 0;
        }
        std::ptr::copy_nonoverlapping(name.as_ptr(), out.cast::<u8>(), name.len());
        *out.add(name.len()) = 0;
        name.len() as u32
    }
}

#[cfg(unix)]
mod imp {
    use super::*;
    use crate::seam;
    use std::ffi::OsStr;
    use std::os::unix::ffi::{OsStrExt, OsStringExt};
    use std::path::Path;

    pub(super) fn os_path(path: &[u8]) -> &Path {
        Path::new(OsStr::from_bytes(path))
    }

    pub(super) fn record(failure: &Failure) {
        match *failure {
            // SAFETY: plain C functions, callable from any thread.
            Failure::Code(code) => unsafe { seam::ee_plat_posix_set_last_error(code) },
            Failure::Errno(errno) => unsafe { seam::ee_plat_posix_set_last_error_errno(errno) },
        }
    }

    fn fail(error: u32) -> c_int {
        record(&Failure::Code(error));
        0
    }

    unsafe fn copy_out(text: &[u8], out: *mut c_char, size: u32) -> u32 {
        if text.len() + 1 > size as usize || out.is_null() {
            fail(errors::FILENAME_TOO_LONG);
            return 0;
        }
        std::ptr::copy_nonoverlapping(text.as_ptr(), out.cast::<u8>(), text.len());
        *out.add(text.len()) = 0;
        text.len() as u32
    }

    pub unsafe fn to_native(input: *const c_char, out: *mut c_char, size: u32) -> u32 {
        match super::resolve_path(crate::c_text::bytes(input)) {
            Some(native) => copy_out(&native, out, size),
            None => 0,
        }
    }

    pub unsafe fn from_native(input: *const c_char, out: *mut c_char, size: u32) -> u32 {
        if input.is_null() {
            return 0;
        }
        let result = game_form(crate::c_text::bytes(input), &VOLUMES.lock().unwrap());
        match result {
            Ok(path) => copy_out(&path, out, size),
            Err(failure) => {
                record(&failure);
                0
            }
        }
    }

    pub unsafe fn set_volume_root(volume: c_char, root: *const c_char) -> c_int {
        let volume = volume as u8;
        let root = crate::c_text::bytes(root);
        if !volume.is_ascii_alphabetic() || (!root.is_empty() && root[0] != b'/') {
            return fail(errors::INVALID_PARAMETER);
        }
        let mut end = root.len();
        while end > 1 && root[end - 1] == b'/' {
            end -= 1;
        }
        VOLUMES.lock().unwrap().set(volume, &root[..end]);
        1
    }

    fn executable_directory() -> Option<Vec<u8>> {
        let exe = std::env::current_exe().ok()?;
        #[cfg(target_os = "macos")]
        let exe = std::fs::canonicalize(exe).ok()?;
        let mut directory = exe.into_os_string().into_vec();
        let slash = directory.iter().rposition(|&c| c == b'/')?;
        directory.truncate(if slash == 0 { 1 } else { slash });
        #[cfg(target_os = "macos")]
        {
            // an app bundle keeps its data in Contents/Resources
            const MACOS: &[u8] = b"/Contents/MacOS";
            if directory.ends_with(MACOS) && directory.len() > MACOS.len() {
                let mut resources = directory[..directory.len() - MACOS.len()].to_vec();
                resources.extend_from_slice(b"/Contents/Resources");
                if os_path(&resources).is_dir() {
                    directory = resources;
                }
            }
        }
        Some(directory)
    }

    fn native_data_root() -> Option<Vec<u8>> {
        // SAFETY: getenv's result is read before anything in this function can change the environment.
        let env = unsafe { crate::c_text::bytes(libc::getenv(c"EE_DATA_ROOT".as_ptr())) };
        if !env.is_empty() {
            return Some(match std::fs::canonicalize(os_path(env)) {
                Ok(real) => real.into_os_string().into_vec(),
                Err(_) => env.to_vec(),
            });
        }
        executable_directory()
    }

    pub unsafe fn get_data_root(out: *mut c_char, size: u32) -> u32 {
        let Some(native) = native_data_root() else {
            return 0;
        };
        let result = game_form(&native, &VOLUMES.lock().unwrap());
        let mut path = match result {
            Ok(path) => path,
            Err(failure) => {
                record(&failure);
                return 0;
            }
        };
        if path.last() != Some(&b'\\') {
            path.push(b'\\');
        }
        copy_out(&path, out, size)
    }

    pub unsafe fn enter_data_root() -> c_int {
        let Some(root) = native_data_root() else {
            return fail(errors::PATH_NOT_FOUND);
        };
        let terminated = crate::c_text::terminated(&root);
        if libc::chdir(terminated.as_ptr().cast()) != 0 {
            let errno = std::io::Error::last_os_error().raw_os_error().unwrap_or(0);
            record(&Failure::Errno(errno));
            return 0;
        }
        1
    }

    pub unsafe fn get_executable_name(out: *mut c_char, size: u32) -> u32 {
        let Ok(exe) = std::env::current_exe() else {
            return 0;
        };
        let bytes = exe.into_os_string().into_vec();
        let start = bytes.iter().rposition(|&c| c == b'/').map_or(0, |slash| slash + 1);
        let name = &bytes[start..];
        if name.len() + 1 > size as usize {
            return 0;
        }
        std::ptr::copy_nonoverlapping(name.as_ptr(), out.cast::<u8>(), name.len());
        *out.add(name.len()) = 0;
        name.len() as u32
    }
}

#[no_mangle]
pub unsafe extern "C" fn PlatPathToNative(input: *const c_char, out: *mut c_char, size: u32) -> u32 {
    imp::to_native(input, out, size)
}

#[no_mangle]
pub unsafe extern "C" fn PlatPathFromNative(input: *const c_char, out: *mut c_char, size: u32) -> u32 {
    imp::from_native(input, out, size)
}

#[no_mangle]
pub unsafe extern "C" fn PlatPathSetVolumeRoot(volume: c_char, root: *const c_char) -> c_int {
    imp::set_volume_root(volume, root)
}

#[no_mangle]
pub unsafe extern "C" fn PlatPathGetDataRoot(out: *mut c_char, size: u32) -> u32 {
    imp::get_data_root(out, size)
}

#[no_mangle]
pub unsafe extern "C" fn PlatPathEnterDataRoot() -> c_int {
    imp::enter_data_root()
}

#[no_mangle]
pub unsafe extern "C" fn PlatPathGetExecutableName(out: *mut c_char, size: u32) -> u32 {
    imp::get_executable_name(out, size)
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::collections::BTreeSet;

    /// A file system of whole paths, with a current directory.
    struct FakeFs {
        entries: BTreeSet<Vec<u8>>,
        current: Result<Vec<u8>, c_int>,
    }

    fn fake(paths: &[&[u8]]) -> FakeFs {
        FakeFs { entries: paths.iter().map(|p| p.to_vec()).collect(), current: Ok(b"/work/dir".to_vec()) }
    }

    impl NativeFs for FakeFs {
        fn exists(&self, path: &[u8]) -> bool {
            self.entries.contains(path) || path == b"/"
        }

        fn find_no_case(&self, directory: &[u8], name: &[u8]) -> Option<Vec<u8>> {
            let mut found = None;
            for entry in &self.entries {
                let Some(slash) = entry.iter().rposition(|&c| c == b'/') else { continue };
                let parent: &[u8] = if slash == 0 { b"/" } else { &entry[..slash] };
                let leaf = &entry[slash + 1..];
                if parent == directory && leaf.eq_ignore_ascii_case(name) {
                    found = Some(leaf.to_vec());
                    if leaf == name {
                        break;
                    }
                }
            }
            found
        }

        fn current_directory(&self) -> Result<Vec<u8>, c_int> {
            self.current.clone()
        }
    }

    fn resolved(input: &[u8], fs: &FakeFs) -> Vec<u8> {
        resolve(input, &Volumes::new(), fs).unwrap()
    }

    #[test]
    fn volume_paths_become_native_and_keep_the_missing_tail() {
        let fs = fake(&[b"/tmp", b"/tmp/Data"]);
        assert_eq!(resolved(b"C:\\tmp\\Data\\new.txt", &fs), b"/tmp/Data/new.txt");
        assert_eq!(resolved(b"c:/tmp/Missing/Sub/x", &fs), b"/tmp/Missing/Sub/x");
        // the last component is matched in its directory only
        assert_eq!(resolved(b"C:\\TMP\\DATA", &fs), b"/tmp/Data");
    }

    #[test]
    fn an_exact_match_beats_other_cases() {
        let fs = fake(&[b"/d", b"/d/FILE", b"/d/file"]);
        assert_eq!(resolved(b"C:\\d\\file", &fs), b"/d/file");
        // no exact match: the last of the case-insensitive matches found
        let fs = fake(&[b"/d", b"/d/FILE"]);
        assert_eq!(resolved(b"C:\\d\\FiLe", &fs), b"/d/FILE");
    }

    #[test]
    fn dots_and_trailing_blanks_are_trimmed_as_win32_does() {
        // golden: tests/platform/plat_test.cpp "volumes" and PORT.md's path rules
        let fs = fake(&[b"/Windows", b"/tmp"]);
        assert_eq!(resolved(b"C:\\Windows\\..\\tmp\\.\\x.", &fs), b"/tmp/x");
        assert_eq!(resolved(b"C:\\tmp\\name.. ", &fs), b"/tmp/name");
        assert_eq!(resolved(b"C:\\..\\..\\tmp", &fs), b"/tmp"); // ".." never leaves the base
        assert_eq!(resolved(b"/tmp//a\\\\b", &fs), b"/tmp/a/b"); // both separators, doubled
    }

    #[test]
    fn relative_paths_start_at_the_current_directory() {
        let fs = fake(&[b"/work", b"/work/dir", b"/work/dir/Data"]);
        assert_eq!(resolved(b"data\\x.dat", &fs), b"/work/dir/Data/x.dat");
    }

    #[test]
    fn leading_separator_is_the_c_volume() {
        let mut volumes = Volumes::new();
        volumes.set(b'C', b"/games");
        let fs = fake(&[b"/games", b"/games/ee"]);
        assert_eq!(resolve(b"\\EE\\f", &volumes, &fs).unwrap(), b"/games/ee/f");
        assert_eq!(resolve(b"Z:\\x", &volumes, &fs).unwrap(), b"/x"); // other letters: "/" by default
    }

    #[test]
    fn resolve_errors() {
        let fs = fake(&[]);
        assert_eq!(resolve(b"", &Volumes::new(), &fs), Err(Failure::Code(errors::PATH_NOT_FOUND)));
        assert_eq!(resolve(b"\\\\server\\share", &Volumes::new(), &fs), Err(Failure::Code(errors::PATH_NOT_FOUND)));
        assert_eq!(resolve(b"//x", &Volumes::new(), &fs), Err(Failure::Code(errors::PATH_NOT_FOUND)));
        let long = vec![b'a'; 260];
        assert_eq!(resolve(&long, &Volumes::new(), &fs), Err(Failure::Code(errors::FILENAME_TOO_LONG)));
        let failing = FakeFs { entries: BTreeSet::new(), current: Err(13) };
        assert_eq!(resolve(b"rel", &Volumes::new(), &failing), Err(Failure::Errno(13)));
    }

    #[test]
    fn game_form_picks_the_longest_volume_root() {
        let mut volumes = Volumes::new();
        assert_eq!(game_form(b"/home/u/ee", &volumes).unwrap(), b"C:\\home\\u\\ee");
        volumes.set(b'Q', b"/tmp/q");
        assert_eq!(game_form(b"/tmp/q/f.txt", &volumes).unwrap(), b"Q:\\f.txt");
        assert_eq!(game_form(b"/tmp/q", &volumes).unwrap(), b"Q:\\");
        // a directory that only starts with the root's name is not inside it
        assert_eq!(game_form(b"/tmp/qx/f", &volumes).unwrap(), b"C:\\tmp\\qx\\f");
        // equal roots: C: first
        volumes.set(b'D', b"/tmp/q");
        assert_eq!(game_form(b"/tmp/q/g", &volumes).unwrap(), b"D:\\g");
    }

    #[test]
    fn game_form_rejects_relative_paths() {
        assert_eq!(game_form(b"", &Volumes::new()), Err(Failure::Code(errors::INVALID_PARAMETER)));
        assert_eq!(game_form(b"rel/x", &Volumes::new()), Err(Failure::Code(errors::INVALID_PARAMETER)));
    }

    #[cfg(windows)]
    mod ffi {
        use super::*;
        use crate::{test_globals, test_set_env};
        use windows_sys::Win32::Foundation::GetLastError;

        fn to_vec(buffer: &[u8], length: u32) -> Vec<u8> {
            buffer[..length as usize].to_vec()
        }

        #[test]
        fn native_conversions_copy() {
            let mut out = [0u8; 64];
            let n = unsafe { PlatPathToNative(c"C:\\Games".as_ptr(), out.as_mut_ptr().cast(), 64) };
            assert_eq!(to_vec(&out, n), b"C:\\Games");
            assert_eq!(out[8], 0);
            let n = unsafe { PlatPathFromNative(c"D:\\x".as_ptr(), out.as_mut_ptr().cast(), 64) };
            assert_eq!(to_vec(&out, n), b"D:\\x");
        }

        #[test]
        fn native_conversion_errors_set_the_win32_last_error() {
            let mut out = [0u8; 8];
            assert_eq!(unsafe { PlatPathToNative(c"".as_ptr(), out.as_mut_ptr().cast(), 8) }, 0);
            assert_eq!(unsafe { GetLastError() }, 3); // ERROR_PATH_NOT_FOUND
            assert_eq!(unsafe { PlatPathToNative(c"longer than 8".as_ptr(), out.as_mut_ptr().cast(), 8) }, 0);
            assert_eq!(unsafe { GetLastError() }, 206); // ERROR_FILENAME_EXCED_RANGE
            assert_eq!(unsafe { PlatPathToNative(std::ptr::null(), out.as_mut_ptr().cast(), 8) }, 0);
            assert_eq!(unsafe { GetLastError() }, 87); // ERROR_INVALID_PARAMETER
            assert_eq!(unsafe { PlatPathSetVolumeRoot(b'Q' as c_char, c"/x".as_ptr()) }, 0);
            assert_eq!(unsafe { GetLastError() }, 50); // ERROR_NOT_SUPPORTED
        }

        #[test]
        fn data_root_is_the_executable_directory_or_the_environment() {
            let _guard = test_globals();
            test_set_env("EE_DATA_ROOT", "");
            let mut out = [0u8; 260];
            let n = unsafe { PlatPathGetDataRoot(out.as_mut_ptr().cast(), 260) };
            assert!(n > 2);
            assert_eq!(out[n as usize - 1], b'\\');
            let exe = std::env::current_exe().unwrap();
            assert_eq!(&out[..n as usize - 1], exe.parent().unwrap().to_str().unwrap().as_bytes());

            test_set_env("EE_DATA_ROOT", "C:\\Some\\Root");
            let n = unsafe { PlatPathGetDataRoot(out.as_mut_ptr().cast(), 260) };
            assert_eq!(to_vec(&out, n), b"C:\\Some\\Root\\");
            test_set_env("EE_DATA_ROOT", "C:/Other/");
            let n = unsafe { PlatPathGetDataRoot(out.as_mut_ptr().cast(), 260) };
            assert_eq!(to_vec(&out, n), b"C:/Other/");
            test_set_env("EE_DATA_ROOT", "");
        }

        #[test]
        fn enter_data_root_changes_the_directory() {
            let _guard = test_globals();
            let before = std::env::current_dir().unwrap();
            let dir = crate::test_dir("enter_root");
            test_set_env("EE_DATA_ROOT", dir.to_str().unwrap());
            assert_eq!(unsafe { PlatPathEnterDataRoot() }, 1);
            assert_eq!(
                std::fs::canonicalize(std::env::current_dir().unwrap()).unwrap(),
                std::fs::canonicalize(&dir).unwrap()
            );
            test_set_env("EE_DATA_ROOT", "C:\\no such directory at all");
            assert_eq!(unsafe { PlatPathEnterDataRoot() }, 0);
            test_set_env("EE_DATA_ROOT", "");
            std::env::set_current_dir(before).unwrap();
            std::fs::remove_dir_all(dir).unwrap();
        }

        #[test]
        fn executable_name_has_no_directory() {
            let mut out = [0u8; 260];
            let n = unsafe { PlatPathGetExecutableName(out.as_mut_ptr().cast(), 260) };
            assert!(n > 0);
            let exe = std::env::current_exe().unwrap();
            assert_eq!(to_vec(&out, n), exe.file_name().unwrap().to_str().unwrap().as_bytes());
            assert_eq!(unsafe { PlatPathGetExecutableName(out.as_mut_ptr().cast(), 1) }, 0);
        }
    }

    #[cfg(unix)]
    mod ffi {
        use super::*;
        use crate::test_seam::LAST_ERROR;
        use std::sync::atomic::Ordering;

        #[test]
        fn c_api_on_the_real_file_system() {
            let _guard = crate::test_globals();
            let dir = crate::test_dir("volumes");
            let root = std::ffi::CString::new(dir.to_str().unwrap()).unwrap();
            assert_eq!(unsafe { PlatPathSetVolumeRoot(b'q' as c_char, root.as_ptr()) }, 1);
            std::fs::create_dir(dir.join("Data")).unwrap();
            let mut out = [0u8; 512];
            let n = unsafe { PlatPathToNative(c"Q:\\DATA\\new".as_ptr(), out.as_mut_ptr().cast(), 512) };
            assert!(n > 0);
            let expected = dir.join("Data").join("new");
            assert_eq!(&out[..n as usize], expected.to_str().unwrap().as_bytes());
            let native = std::ffi::CString::new(expected.to_str().unwrap()).unwrap();
            let m = unsafe { PlatPathFromNative(native.as_ptr(), out.as_mut_ptr().cast(), 512) };
            assert_eq!(&out[..m as usize], b"Q:\\Data\\new");
            assert_eq!(unsafe { PlatPathSetVolumeRoot(b'q' as c_char, std::ptr::null()) }, 1);
            assert_eq!(unsafe { PlatPathSetVolumeRoot(b'1' as c_char, root.as_ptr()) }, 0);
            assert_eq!(LAST_ERROR.load(Ordering::SeqCst), 87);
            assert_eq!(unsafe { PlatPathToNative(c"".as_ptr(), out.as_mut_ptr().cast(), 512) }, 0);
            assert_eq!(LAST_ERROR.load(Ordering::SeqCst), 3);
            std::fs::remove_dir_all(dir).unwrap();
        }
    }
}
