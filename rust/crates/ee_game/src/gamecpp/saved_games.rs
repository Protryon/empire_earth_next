//! "update_saved_games" (EECommandLine.cpp's EEUpdateSavedGames and EEConvertGameFile, game core round 10): every
//! saved game and scenario loaded and saved again as "UPDATED <name>", a log written beside the exe, each original
//! deleted once its conversion succeeded. The C++ calls are EECommandLine.cpp's EECommandLineOp cases.

use std::ffi::{c_char, CStr, CString};

use ee_base::time::{ee_time_get, ee_time_of_day, EeTimeOfDay};

use super::{h, hp, pa, Owned, O, P};

/// The log's first line (the C++'s `%02ld/%02ld/%02ld, %02ld:%02ld.%02ld`, the month counted from 1).
pub fn saved_games_log_start(t: &EeTimeOfDay) -> String {
    format!(
        "Saved game conversion started on {:02}/{:02}/{:02}, {:02}:{:02}.{:02}\n",
        t.mMonth + 1,
        t.mDay,
        t.mYear,
        t.mHour,
        t.mMinute,
        t.mSecond
    )
}

/// A file the pass converts: not one it made (`Find("UPDATED ", 0) != 0`).
pub fn saved_games_wanted(name: &[u8]) -> bool {
    !name.starts_with(b"UPDATED ")
}

/// The one-line verdict after "Beginning conversion of file ...".
pub fn saved_games_verdict(status: i64) -> &'static str {
    if status == 0 {
        "success!\n"
    } else {
        "failed!\n"
    }
}

fn puts(log: P, text: &[u8]) {
    let text = CString::new(text.iter().copied().filter(|&b| b != 0).collect::<Vec<u8>>()).unwrap_or_default();
    h(O::UsgPuts, log, &[pa(text.as_ptr() as P)]);
    h(O::UsgFlush, log, &[]);
}

/// A spec at the start-up volume and `directory` under the start-up directory.
fn at(spec: P, directory: &CStr) {
    h(O::FileSpecSetVolumeStartup, spec, &[]);
    h(O::UsgSetDirectory, spec, &[pa(directory.as_ptr() as P)]);
}

/// EEConvertGameFile: loads one saved game or scenario from a directory and saves it again under a new name; the
/// first error.
fn convert(directory: &CStr, file: &CStr, new_file: &CStr) -> i64 {
    let result = h(O::UsgLoad, 0, &[pa(file.as_ptr() as P), pa(directory.as_ptr() as P)]);
    if result != 0 {
        return result;
    }
    super::camera_io::set_changed();
    super::camera_io::update(1.0);
    h(O::UsgSave, 0, &[pa(new_file.as_ptr() as P), ee_time_get() as i64, pa(directory.as_ptr() as P)])
}

/// One folder's pass: the files matching `pattern` not made by an earlier pass, each converted and its original
/// deleted when that succeeded.
fn folder(log: P, search: P, file: P, directory: &CStr, pattern: &CStr) {
    let mut names: Vec<CString> = Vec::new();
    at(search, directory);
    if h(O::UsgFirstFile, search, &[pa(pattern.as_ptr() as P), pa(file)]) == 0 {
        loop {
            // SAFETY: the spec's own file name, read before its next C++ call
            let name = unsafe { CStr::from_ptr(hp(O::UsgFileName, file, &[]) as *const c_char) };
            if saved_games_wanted(name.to_bytes()) {
                names.push(name.to_owned());
            }
            if h(O::EntrySpecNextFile, search, &[pa(file)]) != 0 {
                break;
            }
        }
    }
    for name in &names {
        let mut line = b"Beginning conversion of file ".to_vec();
        line.extend_from_slice(name.to_bytes());
        line.extend_from_slice(b"... ");
        puts(log, &line);
        let mut updated = b"UPDATED ".to_vec();
        updated.extend_from_slice(name.to_bytes());
        let updated = CString::new(updated).unwrap_or_default();
        let status = convert(directory, name, &updated);
        puts(log, saved_games_verdict(status).as_bytes());
        if status != 0 {
            continue; // keep the original when its conversion failed
        }
        at(file, directory);
        h(O::FileSpecSetFileC, file, &[pa(name.as_ptr() as P)]);
        h(O::UsgDelete, file, &[]);
    }
}

/// EEUpdateSavedGames: under the world's write lock, with the plane models made and a local server started.
#[no_mangle]
pub extern "C" fn ee_gamecpp_update_saved_games() {
    h(O::WorldWriteLock, 0, &[]);
    super::startup::create_plane_models();
    h(O::EntryDeleteCommunicationsKeep, 0, &[]);
    h(O::EntryNewServer, 0, &[]);
    h(O::EntryServerStartThreads, 0, &[]);
    let mut time = EeTimeOfDay { mMonth: 0, mDay: 0, mYear: 0, mHour: 0, mMinute: 0, mSecond: 0 };
    // SAFETY: a local the call fills
    unsafe { ee_time_of_day(&mut time) };
    let log = Owned::new(O::FileSpecNew, O::FxFileSpecDelete);
    let search = Owned::new(O::FileSpecNew, O::FxFileSpecDelete);
    let file = Owned::new(O::FileSpecNew, O::FxFileSpecDelete);
    at(log.0, c"");
    h(O::FileSpecSetFileC, log.0, &[pa(c"saved game conversion.log".as_ptr() as P)]);
    h(O::UsgDelete, log.0, &[]);
    h(O::UsgCreate, log.0, &[]);
    if h(O::UsgFOpenWrite, log.0, &[]) == 0 {
        puts(log.0, saved_games_log_start(&time).as_bytes());
        folder(log.0, search.0, file.0, c"Data\\Saved Games\\", c"*.ees");
        folder(log.0, search.0, file.0, c"Data\\Scenarios\\", c"*.scn");
        puts(log.0, b"Conversion completed!\n");
        h(O::TkFileClose, log.0, &[]);
    }
    h(O::WorldWriteUnlock, 0, &[]);
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_log_line_counts_months_from_one_and_pads_to_two() {
        let t = EeTimeOfDay { mMonth: 0, mDay: 5, mYear: 2026, mHour: 9, mMinute: 3, mSecond: 7 };
        assert_eq!(saved_games_log_start(&t), "Saved game conversion started on 01/05/2026, 09:03.07\n");
    }

    #[test]
    fn only_files_not_yet_updated_are_converted() {
        assert!(saved_games_wanted(b"my game.ees"));
        assert!(saved_games_wanted(b"x UPDATED y.ees"));
        assert!(!saved_games_wanted(b"UPDATED my game.ees"));
        assert_eq!(saved_games_verdict(0), "success!\n");
        assert_eq!(saved_games_verdict(-3), "failed!\n");
    }
}
