//! PlatFile (`include/platform/PlatFile.h`): files, directories, directory search. Paths are byte strings in the
//! game's form: Win32 hands them to the ANSI file functions unchanged, POSIX resolves them first (`native_path`).

#[cfg(unix)]
#[path = "file_posix.rs"]
pub mod imp;
#[cfg(windows)]
#[path = "file_win32.rs"]
pub mod imp;

#[cfg(test)]
mod tests {
    use super::imp::*;
    use crate::constants::*;
    use crate::cstr::cstr_bytes;
    use crate::test_util::{c, c_text, scratch};
    use crate::thread::imp::PlatGetLastError;
    use std::ffi::{c_char, c_void};
    #[allow(unused_imports)]
    use std::ptr::{null, null_mut};

    extern "C" {
        fn fread(buffer: *mut c_void, size: usize, count: usize, stream: *mut c_void) -> usize;
        fn fclose(stream: *mut c_void) -> i32;
        fn fseek(stream: *mut c_void, offset: std::ffi::c_long, origin: i32) -> i32;
    }

    fn cp(text: &str) -> *const c_char {
        // leaked: lives for the test process
        c(text).into_raw()
    }

    unsafe fn write_file(path: &str, content: &[u8]) {
        let created = PlatFileCreateNew(cp(path));
        assert!(created == PlatFileResult::Ok || created == PlatFileResult::Exists, "{created:?}");
        let f = PlatFileOpen(cp(path), PLAT_FILE_WRITE);
        assert!(!f.is_null());
        let mut written = 0u32;
        assert_eq!(PlatFileWrite(f, content.as_ptr().cast(), content.len() as u32, &mut written), 1);
        assert_eq!(written as usize, content.len());
        assert_eq!(PlatFileTruncate(f), 1);
        assert_eq!(PlatFileClose(f), 1);
    }

    unsafe fn read_all(f: *mut PlatFile) -> Vec<u8> {
        let mut out = Vec::new();
        let mut buffer = [0u8; 64];
        loop {
            let mut n = 0u32;
            assert_eq!(PlatFileRead(f, buffer.as_mut_ptr().cast(), 64, &mut n), 1);
            if n == 0 {
                return out;
            }
            out.extend_from_slice(&buffer[..n as usize]);
        }
    }

    unsafe fn find_names(pattern: &str) -> Vec<String> {
        let mut entry: PlatFindData = std::mem::zeroed();
        let find = PlatFindFirst(cp(pattern), &mut entry);
        let mut names = Vec::new();
        if find.is_null() {
            return names;
        }
        loop {
            names.push(c_text(&entry.mName));
            if PlatFindNext(find, &mut entry) != PlatFileResult::Ok {
                break;
            }
        }
        PlatFindClose(find);
        names.retain(|n| n != "." && n != "..");
        names
    }

    #[test]
    fn create_directory_results() {
        unsafe {
            let dir = scratch("dirs");
            assert_eq!(PlatCreateDirectory(cp(&dir)), PlatFileResult::Exists);
            assert_eq!(PlatGetLastError(), 183); // ERROR_ALREADY_EXISTS
            assert_eq!(PlatCreateDirectory(cp(&format!("{dir}Sub"))), PlatFileResult::Ok);
            assert_eq!(PlatCreateDirectory(cp(&format!("{dir}No\\Sub"))), PlatFileResult::NativeError);
            assert_eq!(PlatGetLastError(), 3); // ERROR_PATH_NOT_FOUND
            let attributes = PlatPathGetAttributes(cp(&format!("{dir}Sub")));
            assert_ne!(attributes, PLAT_ATTR_INVALID);
            assert_ne!(attributes & PLAT_ATTR_DIRECTORY, 0);
            assert_eq!(PlatRemoveDirectory(cp(&format!("{dir}Sub"))), 1);
            assert_eq!(PlatRemoveDirectory(cp(&dir)), 1);
        }
    }

    #[test]
    fn missing_paths_report_their_codes() {
        unsafe {
            let dir = scratch("missing");
            assert_eq!(PlatPathGetAttributes(cp(&format!("{dir}nothing.txt"))), PLAT_ATTR_INVALID);
            assert_eq!(PlatGetLastError(), 2); // ERROR_FILE_NOT_FOUND
            assert_eq!(PlatPathGetAttributes(cp(&format!("{dir}no\\nothing.txt"))), PLAT_ATTR_INVALID);
            assert_eq!(PlatGetLastError(), 3); // ERROR_PATH_NOT_FOUND
            assert_eq!(PlatFileCreateNew(cp(&format!("{dir}no\\file.txt"))), PlatFileResult::BadPath);
            assert!(PlatFileOpen(cp(&format!("{dir}absent.txt")), PLAT_FILE_READ).is_null());
            assert_eq!(PlatGetLastError(), 2);
            assert!(PlatFileOpen(cp(&format!("{dir}no\\absent.txt")), PLAT_FILE_READ).is_null());
            assert_eq!(PlatGetLastError(), 3);
            assert_eq!(PlatRemoveDirectory(cp(&dir)), 1);
        }
    }

    #[test]
    fn create_new_fails_when_the_file_exists() {
        unsafe {
            let dir = scratch("createnew");
            let file = format!("{dir}file.txt");
            assert_eq!(PlatFileCreateNew(cp(&file)), PlatFileResult::Ok);
            assert_eq!(PlatFileCreateNew(cp(&file)), PlatFileResult::Exists);
            assert_eq!(PlatDeleteFile(cp(&file)), 1);
            assert_eq!(PlatDeleteFile(cp(&file)), 0);
            assert_eq!(PlatGetLastError(), 2);
            assert_eq!(PlatRemoveDirectory(cp(&dir)), 1);
        }
    }

    #[test]
    fn a_directory_is_not_opened_as_a_file() {
        unsafe {
            let dir = scratch("opendir");
            assert!(PlatFileOpen(cp(dir.trim_end_matches('\\')), PLAT_FILE_READ).is_null());
            assert_eq!(PlatGetLastError(), 5); // ERROR_ACCESS_DENIED
            assert_eq!(PlatRemoveDirectory(cp(&dir)), 1);
        }
    }

    #[test]
    fn read_write_seek_truncate() {
        unsafe {
            let dir = scratch("rw");
            let file = format!("{dir}data.bin");
            write_file(&file, b"Hello, Empire Earth");
            let f = PlatFileOpen(cp(&file), PLAT_FILE_READ | PLAT_FILE_WRITE);
            assert!(!f.is_null());
            assert_eq!(PlatFileGetSize(f), 19);
            let mut buffer = [0u8; 64];
            let mut n = 0u32;
            assert_eq!(PlatFileRead(f, buffer.as_mut_ptr().cast(), 5, &mut n), 1);
            assert_eq!(&buffer[..n as usize], b"Hello");
            assert_eq!(PlatFileSeek(f, 7, PLAT_SEEK_BEGIN), 7);
            assert_eq!(PlatFileRead(f, buffer.as_mut_ptr().cast(), 64, &mut n), 1);
            assert_eq!(&buffer[..n as usize], b"Empire Earth");
            assert_eq!(PlatFileRead(f, buffer.as_mut_ptr().cast(), 64, &mut n), 1);
            assert_eq!(n, 0); // the end of the file
            assert_eq!(PlatFileSeek(f, -5, PLAT_SEEK_END), 14);
            assert_eq!(PlatFileSeek(f, 2, PLAT_SEEK_CURRENT), 16);
            assert_eq!(PlatFileSeek(f, -100, PLAT_SEEK_CURRENT), 0xFFFF_FFFF);
            assert_eq!(PlatGetLastError(), 131); // ERROR_NEGATIVE_SEEK
            assert_eq!(PlatFileSeek(f, 5, PLAT_SEEK_BEGIN), 5);
            assert_eq!(PlatFileTruncate(f), 1);
            assert_eq!(PlatFileGetSize(f), 5);
            let mut time = 0u64;
            assert_eq!(PlatFileGetWriteTime(f, &mut time), 1);
            assert!(time > 116_444_736_000_000_000); // FILETIME after 1970
            assert_eq!(PlatFileClose(f), 1);
            assert_eq!(PlatDeleteFile(cp(&file)), 1);
            assert_eq!(PlatRemoveDirectory(cp(&dir)), 1);
        }
    }

    #[test]
    fn other_openers_may_read_while_a_file_is_open_for_reading() {
        unsafe {
            let dir = scratch("share");
            let file = format!("{dir}shared.txt");
            write_file(&file, b"abc");
            let reader = PlatFileOpen(cp(&file), PLAT_FILE_READ);
            assert!(!reader.is_null());
            let second = PlatFileOpen(cp(&file), PLAT_FILE_READ);
            assert!(!second.is_null(), "FILE_SHARE_READ");
            assert_eq!(read_all(second), b"abc");
            PlatFileClose(second);
            PlatFileClose(reader);
            PlatDeleteFile(cp(&file));
            PlatRemoveDirectory(cp(&dir));
        }
    }

    #[cfg(windows)]
    #[test]
    fn a_write_open_excludes_other_writers() {
        // FILE_SHARE_READ only: a second opener for writing is refused (POSIX has no such lock)
        unsafe {
            let dir = scratch("sharew");
            let file = format!("{dir}shared.txt");
            write_file(&file, b"abc");
            let writer = PlatFileOpen(cp(&file), PLAT_FILE_WRITE);
            assert!(!writer.is_null());
            assert!(PlatFileOpen(cp(&file), PLAT_FILE_WRITE).is_null());
            assert_eq!(PlatGetLastError(), 32); // ERROR_SHARING_VIOLATION
            PlatFileClose(writer);
            PlatDeleteFile(cp(&file));
            PlatRemoveDirectory(cp(&dir));
        }
    }

    #[test]
    fn map_view_shows_the_file() {
        unsafe {
            let dir = scratch("map");
            let file = format!("{dir}mapped.txt");
            write_file(&file, b"Hello");
            let f = PlatFileOpen(cp(&file), PLAT_FILE_READ);
            let mut view: *const c_void = null();
            let mapping = PlatFileMap(f, &mut view);
            assert!(!mapping.is_null());
            assert!(!view.is_null());
            assert_eq!(std::slice::from_raw_parts(view.cast::<u8>(), 5), b"Hello");
            PlatFileUnmap(mapping, view);
            PlatFileClose(f);
            PlatDeleteFile(cp(&file));
            PlatRemoveDirectory(cp(&dir));
        }
    }

    #[test]
    fn map_of_an_empty_file_fails() {
        unsafe {
            let dir = scratch("mapempty");
            let file = format!("{dir}empty.txt");
            assert_eq!(PlatFileCreateNew(cp(&file)), PlatFileResult::Ok);
            let f = PlatFileOpen(cp(&file), PLAT_FILE_READ);
            let mut view: *const c_void = null();
            assert!(PlatFileMap(f, &mut view).is_null());
            assert_eq!(PlatGetLastError(), 1006); // ERROR_FILE_INVALID
            PlatFileClose(f);
            PlatDeleteFile(cp(&file));
            PlatRemoveDirectory(cp(&dir));
        }
    }

    #[test]
    fn copy_file_and_read_only_rules() {
        unsafe {
            let dir = scratch("copy");
            let source = format!("{dir}a.txt");
            let copy = format!("{dir}b.txt");
            write_file(&source, b"content");
            assert_eq!(PlatCopyFile(cp(&source), cp(&copy), 1), 1);
            assert_eq!(PlatCopyFile(cp(&source), cp(&copy), 1), 0);
            assert_eq!(PlatGetLastError(), 80); // ERROR_FILE_EXISTS
            assert_eq!(PlatCopyFile(cp(&source), cp(&copy), 0), 1);
            let f = PlatFileOpen(cp(&copy), PLAT_FILE_READ);
            assert_eq!(read_all(f), b"content");
            PlatFileClose(f);
            assert_eq!(PlatPathSetAttributes(cp(&copy), PLAT_ATTR_READONLY), 1);
            assert_ne!(PlatPathGetAttributes(cp(&copy)) & PLAT_ATTR_READONLY, 0);
            assert_eq!(PlatDeleteFile(cp(&copy)), 0); // read-only
            assert_eq!(PlatGetLastError(), 5);
            assert_eq!(PlatPathSetAttributes(cp(&copy), PLAT_ATTR_NORMAL), 1);
            assert_eq!(PlatPathGetAttributes(cp(&copy)) & PLAT_ATTR_READONLY, 0);
            assert_eq!(PlatCopyFile(cp(&format!("{dir}none.txt")), cp(&copy), 0), 0);
            assert_eq!(PlatGetLastError(), 2);
            PlatDeleteFile(cp(&source));
            PlatDeleteFile(cp(&copy));
            assert_eq!(PlatRemoveDirectory(cp(&dir)), 1);
        }
    }

    #[test]
    fn directory_search() {
        unsafe {
            let dir = scratch("find");
            for name in ["b.dat", "A.dat", "copy.txt"] {
                assert_eq!(PlatFileCreateNew(cp(&format!("{dir}{name}"))), PlatFileResult::Ok);
            }
            assert_eq!(PlatCreateDirectory(cp(&format!("{dir}Sub"))), PlatFileResult::Ok);
            // NTFS's order: case-insensitive
            assert_eq!(find_names(&format!("{dir}*.*")), ["A.dat", "b.dat", "copy.txt", "Sub"]);
            assert_eq!(find_names(&format!("{dir}*.dat")), ["A.dat", "b.dat"]);
            // a plain name finds the entry itself
            let mut entry: PlatFindData = std::mem::zeroed();
            let find = PlatFindFirst(cp(&format!("{dir}Sub")), &mut entry);
            assert!(!find.is_null());
            assert_eq!(c_text(&entry.mName), "Sub");
            assert_ne!(entry.mAttributes & PLAT_ATTR_DIRECTORY, 0);
            PlatFindClose(find);
            assert!(PlatFindFirst(cp(&format!("{dir}*.none")), &mut entry).is_null());
            for name in ["b.dat", "A.dat", "copy.txt"] {
                assert_eq!(PlatDeleteFile(cp(&format!("{dir}{name}"))), 1);
            }
            assert_eq!(PlatRemoveDirectory(cp(&format!("{dir}Sub"))), 1);
            assert_eq!(PlatRemoveDirectory(cp(&dir)), 1);
        }
    }

    #[test]
    fn find_next_reports_no_more_and_close_keeps_the_error() {
        unsafe {
            let dir = scratch("findend");
            assert_eq!(PlatFileCreateNew(cp(&format!("{dir}only.txt"))), PlatFileResult::Ok);
            let mut entry: PlatFindData = std::mem::zeroed();
            let find = PlatFindFirst(cp(&format!("{dir}only.txt")), &mut entry);
            assert!(!find.is_null());
            let result = PlatFindNext(find, &mut entry);
            assert_eq!(result, PlatFileResult::NoMore);
            assert_eq!(PlatGetLastError(), 18); // ERROR_NO_MORE_FILES
            PlatFindClose(find);
            assert_eq!(PlatGetLastError(), 18, "the failed PlatFindNext's error survives the close");
            PlatDeleteFile(cp(&format!("{dir}only.txt")));
            PlatRemoveDirectory(cp(&dir));
        }
    }

    #[test]
    fn directory_not_empty() {
        unsafe {
            let dir = scratch("notempty");
            assert_eq!(PlatFileCreateNew(cp(&format!("{dir}x"))), PlatFileResult::Ok);
            assert_eq!(PlatRemoveDirectory(cp(&dir)), 0);
            assert_eq!(PlatGetLastError(), 145); // ERROR_DIR_NOT_EMPTY
            PlatDeleteFile(cp(&format!("{dir}x")));
            assert_eq!(PlatRemoveDirectory(cp(&dir)), 1);
        }
    }

    #[test]
    fn current_and_temp_directories_are_in_game_form() {
        unsafe {
            let mut path = [0u8; PLAT_MAX_PATH];
            let n = PlatGetTempDirectory(path.as_mut_ptr().cast(), PLAT_MAX_PATH as u32) as usize;
            assert!(n > 3);
            assert_eq!(path[1], b':');
            assert_eq!(path[n - 1], b'\\');
            let n = PlatGetCurrentDirectory(path.as_mut_ptr().cast(), PLAT_MAX_PATH as u32);
            assert!(n > 2);
            assert_eq!(path[1], b':');
            // too small: the size it needs
            let mut small = [0u8; 2];
            let needed = PlatGetCurrentDirectory(small.as_mut_ptr().cast(), 2);
            assert_eq!(needed, n + 1);
        }
    }

    #[test]
    fn temp_file_has_the_prefix_and_exists() {
        unsafe {
            let dir = scratch("tempfile");
            let mut path = [0u8; PLAT_MAX_PATH];
            assert_eq!(PlatCreateTempFile(cp(&dir), cp("eeTest"), path.as_mut_ptr().cast()), 1);
            let name = c_text(&path);
            assert!(name.contains("eeT"), "{name}");
            assert!(name.ends_with(".TMP") || name.ends_with(".tmp"), "{name}");
            let attributes = PlatPathGetAttributes(path.as_ptr().cast());
            assert_ne!(attributes, PLAT_ATTR_INVALID);
            assert_eq!(PlatDeleteFile(path.as_ptr().cast()), 1);
            assert_eq!(PlatRemoveDirectory(cp(&dir)), 1);
        }
    }

    #[test]
    fn fopen_text_mode_reads_crlf_as_lf() {
        // The contract on every system: Win32 through the CRT, POSIX through the filter.
        unsafe {
            let dir = scratch("fopen");
            let file = format!("{dir}text.txt");
            write_file(&file, b"one\r\ntwo\r\n");
            let stream: *mut c_void = PlatFOpen(cp(&file), cp("r")).cast();
            assert!(!stream.is_null());
            let mut buffer = [0u8; 32];
            let n = fread(buffer.as_mut_ptr().cast(), 1, 32, stream);
            assert_eq!(&buffer[..n], b"one\ntwo\n");
            assert_eq!(fseek(stream, 0, 0), 0); // rewind
            let n = fread(buffer.as_mut_ptr().cast(), 1, 4, stream);
            assert_eq!(&buffer[..n], b"one\n");
            assert_eq!(fclose(stream), 0);

            let stream: *mut c_void = PlatFOpen(cp(&file), cp("rb")).cast();
            let n = fread(buffer.as_mut_ptr().cast(), 1, 32, stream);
            assert_eq!(&buffer[..n], b"one\r\ntwo\r\n");
            fclose(stream);
            PlatDeleteFile(cp(&file));
            PlatRemoveDirectory(cp(&dir));
        }
    }

    #[test]
    fn fopen_of_a_missing_file_fails() {
        unsafe {
            let dir = scratch("fopen2");
            assert!(PlatFOpen(cp(&format!("{dir}absent")), cp("r")).is_null());
            PlatRemoveDirectory(cp(&dir));
        }
    }

    #[cfg(unix)]
    #[test]
    fn hidden_attribute_for_dot_files() {
        unsafe {
            let dir = scratch("hidden");
            let file = format!("{dir}.dotfile");
            assert_eq!(PlatFileCreateNew(cp(&file)), PlatFileResult::Ok);
            assert_ne!(PlatPathGetAttributes(cp(&file)) & PLAT_ATTR_HIDDEN, 0);
            PlatDeleteFile(cp(&file));
            PlatRemoveDirectory(cp(&dir));
        }
    }

    #[cfg(unix)]
    #[test]
    fn file_times_are_filetime_units() {
        assert_eq!(super::imp::file_time(0, 0), 116_444_736_000_000_000);
        assert_eq!(super::imp::file_time(1, 500), 116_444_736_010_000_005);
    }

    #[cfg(unix)]
    #[test]
    fn null_handles_are_invalid() {
        unsafe {
            assert_eq!(PlatFileClose(null_mut()), 0);
            assert_eq!(PlatGetLastError(), 6);
            let mut entry: PlatFindData = std::mem::zeroed();
            assert_eq!(PlatFindNext(null_mut(), &mut entry), PlatFileResult::NativeError);
            assert_eq!(PlatGetLastError(), 6);
            assert!(PlatFindFirst(cp(""), &mut entry).is_null());
            assert_eq!(PlatGetLastError(), 2);
            PlatFindClose(null_mut());
        }
    }

    #[test]
    fn strncpy_shape_of_find_names() {
        // sanity of the helper used for entry names
        let text = c("A.dat");
        assert_eq!(unsafe { cstr_bytes(text.as_ptr()) }, b"A.dat");
        let _ = null::<c_void>();
    }
}
