//! WONFile's path logic (the lobby's file paths: its persistent data, its images, the file dialog): the separators,
//! the directory, name and extension splits, the normalisation of `.` and `..`, the absolute path (`_fullpath` as
//! the port's C++ wrote it) and the relative path. The C++ `WONFile` keeps its strings, cache flags and the platform
//! calls (attributes, size, time, the current directory); these are its algorithms, byte for byte.

use std::ffi::c_void;

use crate::ffi::{bytes, give, EeWonByteSink};
use crate::ffi_parse::CrtCtype;
use crate::parser::Ctype;

/// The Windows separator every path is turned to (`WONFile::PATH_SEP`).
pub(crate) const SEP: u8 = b'\\';

/// The original threw `std::out_of_range` here (a `substr` past the end).
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct OutOfRange;

/// `SetFilePath`: the path with every '/' turned into '\'.
pub fn given(path: &[u8]) -> Vec<u8> {
    path.iter().map(|&b| if b == b'/' { SEP } else { b }).collect()
}

/// `CalcFileDirectory`: the last separator. The directory is the part before it (left as it was when there is
/// none), the name the part after it (the whole path when there is none).
pub fn directory_split(path: &[u8]) -> Option<usize> {
    path.iter().rposition(|&b| b == SEP)
}

/// `CalcFileExtension`: the name's last dot. The extension is the part after it (left as it was when there is
/// none), the name without extension the part before it (the whole name when there is none).
pub fn extension_split(name: &[u8]) -> Option<usize> {
    name.iter().rposition(|&b| b == b'.')
}

/// `NormalizePath`: `\.` and `\..` resolved, a `..` above the start kept. `None` for an empty path (the original
/// returned false and left it). A relative path is worked on with a separator in front, which is cut off at the
/// end: when the work emptied it (the path "."), the original's `substr(1)` threw.
pub fn normalize(path: &[u8]) -> Result<Option<Vec<u8>>, OutOfRange> {
    if path.is_empty() {
        return Ok(None);
    }
    let first_is_slash = path[0] == SEP;
    let mut p = Vec::with_capacity(path.len() + 1);
    if !first_is_slash {
        p.push(SEP);
    }
    p.extend_from_slice(path);
    let mut dirs: Vec<usize> = Vec::new();
    let mut pos = 0;
    while pos < p.len() {
        if p[pos] != SEP {
            pos += 1;
            continue;
        }
        let mut replaced = false;
        if pos + 2 < p.len() && p[pos + 1] == b'.' && p[pos + 2] == b'.' {
            if pos + 3 >= p.len() || p[pos + 3] == SEP {
                replaced = true;
                // \..\ : back to the previous directory's separator
                if let Some(prev) = dirs.pop() {
                    p.drain(prev..pos + 3);
                    pos = prev;
                } else {
                    pos += 1;
                }
            }
        } else if pos + 1 < p.len() && p[pos + 1] == b'.' && (pos + 2 >= p.len() || p[pos + 2] == SEP) {
            // \.\ : dropped
            replaced = true;
            p.drain(pos..pos + 2);
        }
        if !replaced {
            dirs.push(pos);
            pos += 1;
        }
    }
    if first_is_slash {
        Ok(Some(p))
    } else if p.is_empty() {
        Err(OutOfRange)
    } else {
        Ok(Some(p[1..].to_vec()))
    }
}

/// Whether `CalcFullPath` needs the current directory: not for a path rooted on a volume ("C:\x") or a UNC path.
pub fn needs_current(path: &[u8]) -> bool {
    let has_volume = path.len() >= 2 && path[1] == b':';
    let root = if has_volume { 2 } else { 0 };
    let rooted = path.len() > root && path[root] == SEP;
    !(has_volume && rooted) && !path.starts_with(b"\\\\")
}

/// `CalcFullPath`: the path joined to `current` (the current directory, not empty; `None` when the path needs
/// none), then given and normalised as a new WONFile (an empty result when the normalisation leaves it).
pub fn absolute(path: &[u8], current: Option<&[u8]>) -> Result<Vec<u8>, OutOfRange> {
    let full = match current {
        None => path.to_vec(),
        Some(cur) => {
            let has_volume = path.len() >= 2 && path[1] == b':';
            let root = if has_volume { 2 } else { 0 };
            let rooted = path.len() > root && path[root] == SEP;
            let mut current = cur.to_vec();
            if current.last() != Some(&SEP) {
                current.push(SEP);
            }
            let volume: &[u8] = if current.len() >= 2 && current[1] == b':' { &current[..2] } else { &[] };
            if rooted {
                // "\dir": on the current volume
                [volume, path].concat()
            } else if !has_volume {
                [&current[..], path].concat()
            } else if ee_plat_core::utf8::compare_folded_bytes(volume, path, 2) == 0 {
                // "C:dir" on the current volume
                [&current[..], &path[2..]].concat()
            } else {
                // "D:dir": the root of another volume
                [&path[..2], b"\\", &path[2..]].concat()
            }
        }
    };
    Ok(normalize(&given(&full))?.unwrap_or_default())
}

/// `GetRelativePath`: `sub` after `directory` and a separator when it starts so (the bytes compared through the
/// C runtime's `toupper`), else `sub` whole.
pub fn relative(directory: &[u8], sub: &[u8], ctype: &impl Ctype) -> Vec<u8> {
    if sub.len() > directory.len() {
        let upper = |b: u8| ctype.to_upper(i32::from(b)) as u8;
        let same = directory.iter().zip(sub).take_while(|(&a, &b)| upper(a) == upper(b)).count();
        if same == directory.len() && sub[same] == SEP {
            return sub[same + 1..].to_vec();
        }
    }
    sub.to_vec()
}

// ---- the C ABI (WONFile.cpp)

/// `SetFilePath`'s separators.
///
/// # Safety
/// `path` holds `length` bytes; `sink` receives the result during the call.
#[no_mangle]
pub unsafe extern "C" fn ee_won_path_given(path: *const u8, length: usize, sink: EeWonByteSink, context: *mut c_void) {
    unsafe { give(sink, context, &given(bytes(path, length))) }
}

/// The last separator's index, or -1.
///
/// # Safety
/// `path` holds `length` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_path_directory_split(path: *const u8, length: usize) -> isize {
    directory_split(unsafe { bytes(path, length) }).map_or(-1, |i| i as isize)
}

/// The last dot's index, or -1.
///
/// # Safety
/// `name` holds `length` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_path_extension_split(name: *const u8, length: usize) -> isize {
    extension_split(unsafe { bytes(name, length) }).map_or(-1, |i| i as isize)
}

/// `NormalizePath`: 0 for an empty path (nothing given), 1 with the path given, 2 where the original threw.
///
/// # Safety
/// As `ee_won_path_given`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_path_normalize(
    path: *const u8,
    length: usize,
    sink: EeWonByteSink,
    context: *mut c_void,
) -> i32 {
    match normalize(unsafe { bytes(path, length) }) {
        Ok(None) => 0,
        Ok(Some(p)) => {
            unsafe { give(sink, context, &p) };
            1
        }
        Err(OutOfRange) => 2,
    }
}

/// Whether the absolute path needs the current directory.
///
/// # Safety
/// `path` holds `length` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_path_needs_current(path: *const u8, length: usize) -> bool {
    needs_current(unsafe { bytes(path, length) })
}

/// `CalcFullPath`'s join and normalisation; `current` NULL when none is needed. 1 with the path given, 2 where
/// the original threw.
///
/// # Safety
/// `path` holds `length` bytes, `current` (when not NULL) `current_length`; `sink` as `ee_won_path_given`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_path_absolute(
    path: *const u8,
    length: usize,
    current: *const u8,
    current_length: usize,
    sink: EeWonByteSink,
    context: *mut c_void,
) -> i32 {
    let cur = if current.is_null() { None } else { Some(unsafe { bytes(current, current_length) }) };
    match absolute(unsafe { bytes(path, length) }, cur) {
        Ok(p) => {
            unsafe { give(sink, context, &p) };
            1
        }
        Err(OutOfRange) => 2,
    }
}

/// `GetRelativePath`.
///
/// # Safety
/// `directory` and `sub` hold their lengths' bytes; `sink` as `ee_won_path_given`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_path_relative(
    directory: *const u8,
    directory_length: usize,
    sub: *const u8,
    sub_length: usize,
    sink: EeWonByteSink,
    context: *mut c_void,
) {
    let out = relative(unsafe { bytes(directory, directory_length) }, unsafe { bytes(sub, sub_length) }, &CrtCtype);
    unsafe { give(sink, context, &out) }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::parser::CLocale;

    const GOLDEN: &str = include_str!("golden_path.txt");

    // tests/won/won_path_test.cpp's tables
    const PATHS: &[&[u8]] = &[
        b"",
        b"a",
        b"a.b",
        b"dir/file.txt",
        b"C:\\dir\\file.tar.gz",
        b"\\\\server\\share\\x",
        b"..\\x",
        b"a\\..\\b",
        b"a\\.\\b",
        b"a\\b\\..\\..\\..\\c",
        b"\\a\\..",
        b".\\",
        b"a\\.",
        b"a\\..b\\c",
        b"a\\...\\c",
        b"a\\.b",
        b"dir.d\\file",
        b"trailing\\",
        b"C:",
        b"%vrel\\x",
        b"Q:rel",
        b"Q:\\abs\\..\\y",
        b"\\rooted\\p",
        b"/fw/slash/./x",
        b"x\\\\y",
        b"\\..\\..\\a",
        b"\xc3\xa9\\\xc3\xbc.txt",
        b"a.\\b",
        b".hidden",
        b"a\\b.",
        b"..",
        b".",
        b"\\",
        b"\\\\",
        b"a/../../b",
        b"%v",
        b"%v..\\..\\z",
        b"x\\..\\..",
        b"a\\b\\.\\.\\c\\..\\d",
    ];
    const RELATIVE: &[(&[u8], &[u8])] = &[
        (b"C:\\a\\b\\f.txt", b"C:\\a\\b\\c\\d"),
        (b"c:\\A\\B\\f", b"C:\\a\\b\\x"),
        (b"a\\f", b"a\\"),
        (b"a\\f", b"a"),
        (b"a\\f", b"ab\\x"),
        (b"f", b"\\x"),
        (b"f", b"x"),
        (b"\xc3\xa9\\f", b"\xc3\x89\\x"),
        (b"a\\f", b"a\\x\\y"),
        (b"a/b/f", b"a\\b\\g"),
        (b"a\\b\\f", b"a/b/g"),
        (b"", b"x"),
        (b"", b"\\"),
        (b"x\\f", b"X\\"),
    ];
    // the driver's directory, as the golden prints it: <T> on volume <V>
    const VOLUME: &[u8] = b"C:";
    const BASE: &[u8] = b"C:\\won_path_cwd";

    fn hex(s: &[u8]) -> String {
        if s.is_empty() {
            return "-".into();
        }
        s.iter()
            .map(|&c| if c > 0x20 && c < 0x7f && c != b'%' { (c as char).to_string() } else { format!("%{c:02x}") })
            .collect()
    }

    fn expand(p: &[u8]) -> Vec<u8> {
        let mut out = Vec::new();
        let mut i = 0;
        while i < p.len() {
            if p[i] == b'%' && p.get(i + 1) == Some(&b'v') {
                out.extend(VOLUME.to_ascii_lowercase());
                i += 2;
            } else {
                out.push(p[i]);
                i += 1;
            }
        }
        out
    }

    fn collapse(p: &[u8]) -> Vec<u8> {
        if let Some(rest) = p.strip_prefix(BASE) {
            [b"<T>", rest].concat()
        } else if let Some(rest) = p.strip_prefix(VOLUME) {
            [b"<V>", rest].concat()
        } else {
            p.to_vec()
        }
    }

    /// The C++ WONFile's strings and cache flags over the Rust algorithms.
    #[derive(Default)]
    struct File {
        path: Vec<u8>,
        dir: Vec<u8>,
        name: Vec<u8>,
        noext: Vec<u8>,
        ext: Vec<u8>,
        dir_valid: bool,
        name_valid: bool,
    }

    impl File {
        fn new(p: &[u8]) -> File {
            File { path: given(p), ..File::default() }
        }
        fn calc_dir(&mut self) {
            match directory_split(&self.path) {
                Some(s) => {
                    self.dir = self.path[..s].to_vec();
                    self.name = self.path[s + 1..].to_vec();
                }
                None => self.name = self.path.clone(),
            }
            self.dir_valid = true;
            self.name_valid = true;
        }
        fn dir(&mut self) -> Vec<u8> {
            if !self.dir_valid {
                self.calc_dir();
            }
            self.dir.clone()
        }
        fn name(&mut self) -> Vec<u8> {
            if !self.name_valid {
                self.dir();
            }
            self.name.clone()
        }
        fn ext(&mut self) -> Vec<u8> {
            let name = self.name();
            match extension_split(&name) {
                Some(d) => {
                    self.ext = name[d + 1..].to_vec();
                    self.noext = name[..d].to_vec();
                }
                None => self.noext = name,
            }
            self.ext.clone()
        }
        fn normalize(&mut self) -> Result<bool, OutOfRange> {
            match normalize(&self.path)? {
                None => Ok(false),
                Some(p) => {
                    self.path = p;
                    self.dir.clear();
                    self.dir_valid = false;
                    Ok(true)
                }
            }
        }
    }

    fn lines() -> Vec<String> {
        let mut out = Vec::new();
        for (i, p) in PATHS.iter().enumerate() {
            let input = expand(p);
            let case = format!("p{i:02}");
            let mut f = File::new(&input);
            let (path, dir, name, ext) = (f.path.clone(), f.dir(), f.name(), f.ext());
            out.push(format!(
                "{case} path={} dir={} name={} ext={} noext={}",
                hex(&path),
                hex(&dir),
                hex(&name),
                hex(&ext),
                hex(&f.noext)
            ));
            let mut f = File::new(&input);
            match f.normalize() {
                Ok(r) => {
                    let (path, dir, name) = (f.path.clone(), f.dir(), f.name());
                    out.push(format!(
                        "{case} norm r={} path={} dir={} name={}",
                        u8::from(r),
                        hex(&path),
                        hex(&dir),
                        hex(&name)
                    ));
                }
                Err(_) => out.push(format!("{case} norm throw")),
            }
            let mut f = File::new(&input);
            let name = f.name();
            match f.normalize() {
                Ok(_) => {
                    let (after, dir, name3) = (f.name(), f.dir(), f.name());
                    out.push(format!(
                        "{case} cached name={} after={} dir={} name={}",
                        hex(&name),
                        hex(&after),
                        hex(&dir),
                        hex(&name3)
                    ));
                }
                Err(_) => out.push(format!("{case} cached throw")),
            }
            let f = File::new(&input);
            let current = needs_current(&f.path).then(|| [BASE, b"\\a\\b"].concat());
            match absolute(&f.path, current.as_deref()) {
                Ok(full) => out.push(format!("full {case} {}", hex(&collapse(&full)))),
                Err(_) => out.push(format!("full {case} throw")),
            }
        }
        for (i, (a, b)) in RELATIVE.iter().enumerate() {
            let mut f = File::new(a);
            let sub = File::new(b);
            out.push(format!("r{i:02} rel={}", hex(&relative(&f.dir(), &sub.path, &CLocale))));
        }
        out
    }

    #[test]
    fn the_path_table_matches_the_cpp() {
        let got = lines();
        let want: Vec<&str> = GOLDEN.lines().collect();
        assert_eq!(got.len(), want.len());
        for (g, w) in got.iter().zip(&want) {
            assert_eq!(g, w);
        }
    }

    #[test]
    fn normalize_cases() {
        assert_eq!(normalize(b"a\\..\\..\\b"), Ok(Some(b"..\\b".to_vec())));
        assert_eq!(normalize(b"\\x\\.\\y"), Ok(Some(b"\\x\\y".to_vec())));
        assert_eq!(normalize(b"."), Err(OutOfRange));
        assert_eq!(normalize(b""), Ok(None));
        assert_eq!(normalize(b"a\\."), Ok(Some(b"a".to_vec())));
    }

    #[test]
    fn absolute_cases() {
        assert!(!needs_current(b"C:\\x"));
        assert!(!needs_current(b"\\\\srv\\x"));
        assert!(needs_current(b"\\x"));
        assert!(needs_current(b"C:x"));
        assert_eq!(absolute(b"x", Some(b"D:\\w")), Ok(b"D:\\w\\x".to_vec()));
        assert_eq!(absolute(b"d:x", Some(b"D:\\w\\")), Ok(b"D:\\w\\x".to_vec()));
        assert_eq!(absolute(b"E:x", Some(b"D:\\w")), Ok(b"E:\\x".to_vec()));
        assert_eq!(absolute(b"\\r", Some(b"D:\\w")), Ok(b"D:\\r".to_vec()));
        assert_eq!(absolute(b"C:\\a\\..\\b", None), Ok(b"C:\\b".to_vec()));
    }
}
