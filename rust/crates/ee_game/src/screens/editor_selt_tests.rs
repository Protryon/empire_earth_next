use super::*;
use std::collections::HashMap;

/// A recording host: every statement's op and integers in order, the answers by op (0 otherwise), the texts posted.
#[derive(Default)]
struct Rec {
    ops: Vec<(u32, [usize; 2], [i64; 2])>,
    answers: HashMap<u32, Vec<i64>>,
    texts: Vec<(u32, Vec<u8>)>,
    out_text: Vec<u8>,
}

impl Rec {
    fn answer(&mut self, op: u32, values: &[i64]) {
        self.answers.insert(op, values.iter().rev().copied().collect());
    }
    fn run(&mut self, f: impl FnOnce(&mut Seltx)) {
        let mut x = |op: u32, p: [usize; 3], a: [i64; 4]| -> i64 {
            self.ops.push((op, [p[0], p[1]], [a[0], a[1]]));
            if a[2] > 0 && p[2] != 0 {
                // SAFETY: the statement's text (p2, a2 bytes)
                let t = unsafe { core::slice::from_raw_parts(p[2] as *const u8, a[2] as usize) }.to_vec();
                self.texts.push((op, t));
            }
            if a[3] > 0 && p[2] != 0 && !self.out_text.is_empty() {
                let n = self.out_text.len().min(a[3] as usize);
                // SAFETY: p2 holds a3 bytes
                unsafe { core::ptr::copy_nonoverlapping(self.out_text.as_ptr(), p[2] as *mut u8, n) };
                return self.out_text.len() as i64;
            }
            match self.answers.get_mut(&op) {
                Some(v) if v.len() > 1 => v.pop().unwrap(),
                Some(v) => v.first().copied().unwrap_or(0),
                None => 0,
            }
        };
        f(&mut x);
    }
    fn names(&self) -> Vec<u32> {
        self.ops.iter().map(|o| o.0).collect()
    }
}

#[test]
fn c_format_is_sprintf_s() {
    use EeScreensSeltArg::*;
    assert_eq!(
        c_format(b"Loading (%s), for Player (%d)", &[Text(b"Archer"), Number(3)]),
        b"Loading (Archer), for Player (3)"
    );
    assert_eq!(c_format(b"%ld%%-%u %x", &[Number(-2), Number(-1)]), b"-2%-4294967295 %x");
    assert_eq!(c_format(b"no args %s", &[]), b"no args ");
    assert_eq!(c_format(b"tail %", &[]), b"tail %");
}

#[test]
fn header_version_reads_the_braces() {
    assert_eq!(header_version(b"{Version_1}\n"), Some(1));
    assert_eq!(header_version(b"junk {Version_12}x"), Some(12));
    assert_eq!(header_version(b"{Version_-1}"), Some(u32::MAX));
    assert_eq!(header_version(b"{Version_1"), None);
    assert_eq!(header_version(b"Version_1}"), None);
}

#[test]
fn construct_and_destroy_run_the_ctor_s_statements() {
    let mut r = Rec::default();
    r.run(|x| construct(x, 7, 9, 2));
    assert_eq!(
        r.names(),
        [
            EE_SCREENS_SELTX_SET_DONE,
            EE_SCREENS_SELTX_SET_EDITOR,
            EE_SCREENS_SELTX_CREATE_EVENT_CREATED,
            EE_SCREENS_SELTX_CREATE_EVENT_DONE,
            EE_SCREENS_SELTX_CREATE_EVENT_QUEUE,
            EE_SCREENS_SELTX_CREATE_THREAD,
            EE_SCREENS_SELTX_SET_PRIORITY,
            EE_SCREENS_SELTX_WAIT_CREATED,
            EE_SCREENS_SELTX_SET_NAME,
            EE_SCREENS_SELTX_NEW_LOAD_TEXT
        ]
    );
    assert_eq!(r.ops[1].1, [7, 9]);
    assert_eq!(r.ops[6].2[0], 2);
    let mut r = Rec::default();
    r.run(|x| destroy(x, 7));
    assert_eq!(r.names()[0], EE_SCREENS_SELTX_LOAD_TEXT_RELEASE);
    assert_eq!(r.names().len(), 5);
}

#[test]
fn main_loops_until_done_and_deletes() {
    let mut r = Rec::default();
    r.answer(EE_SCREENS_SELTX_IS_DONE, &[0, 0, 1]);
    r.answer(EE_SCREENS_SELTX_RECEIVE, &[1, 1]);
    r.answer(EE_SCREENS_SELTX_R_FP_IS53, &[0, 1]);
    r.answer(EE_SCREENS_SELTX_R_FP_ROUNDING, &[0, 2]);
    r.run(|x| {
        main(x, 5);
    });
    use EE_SCREENS_SELTX_R_FP_SET53 as SET53;
    assert_eq!(
        r.names(),
        [
            EE_SCREENS_SELTX_SIGNAL_CREATED,
            EE_SCREENS_SELTX_PRIORITY_ABOVE_NORMAL,
            SET53,
            EE_SCREENS_SELTX_IS_DONE,
            EE_SCREENS_SELTX_RECEIVE,
            EE_SCREENS_SELTX_R_FP_IS53,
            SET53,
            EE_SCREENS_SELTX_R_FP_ROUNDING,
            EE_SCREENS_SELTX_R_SLEEP,
            EE_SCREENS_SELTX_IS_DONE,
            EE_SCREENS_SELTX_RECEIVE,
            EE_SCREENS_SELTX_R_FP_IS53,
            EE_SCREENS_SELTX_R_FP_ROUNDING,
            EE_SCREENS_SELTX_R_FP_ROUND0,
            EE_SCREENS_SELTX_R_SLEEP,
            EE_SCREENS_SELTX_IS_DONE,
            EE_SCREENS_SELTX_DELETE
        ]
    );
}

#[test]
fn the_messages_dispatch() {
    let mut r = Rec::default();
    r.run(|x| {
        handle_message(x, 1, 0x401, 0, 0);
        handle_message(x, 1, 0x40b, 44, 0);
        handle_message(x, 1, 0x40c, 45, 0);
        handle_message(x, 1, 0x40a, 0, 0);
        handle_message(x, 1, 0x404, 0, 0);
        handle_message(x, 1, 0x405, 0, 0);
        handle_message(x, 1, 0x999, 0, 0);
    });
    assert_eq!(
        r.names(),
        [
            EE_SCREENS_SELTX_LEAVE_EDITOR,
            EE_SCREENS_SELTX_SET_DONE,
            EE_SCREENS_SELTX_POST_EVENT,
            EE_SCREENS_SELTX_DEREFERENCE,
            EE_SCREENS_SELTX_SHOW_LOADED,
            EE_SCREENS_SELTX_DEREFERENCE,
            EE_SCREENS_SELTX_QUEUE_DONE,
            EE_SCREENS_SELTX_START_TEST,
            EE_SCREENS_SELTX_SET_DONE,
            EE_SCREENS_SELTX_START_TEST2,
            EE_SCREENS_SELTX_SET_DONE
        ]
    );
    assert_eq!(r.ops[1].2[0], 1);
    assert_eq!(r.ops[3].1[1], 44);
}

#[test]
fn handle_406_posts_the_epochs() {
    let mut r = Rec::default();
    r.answer(EE_SCREENS_SELTX_PLAYER, &[77]);
    r.run(|x| handle_message(x, 1, 0x406, 2, 2));
    assert_eq!(
        r.names(),
        [
            EE_SCREENS_SELTX_READ_LOCK,
            EE_SCREENS_SELTX_PLAYER,
            EE_SCREENS_SELTX_READ_UNLOCK,
            EE_SCREENS_SELTX_POST_TEXT,
            EE_SCREENS_SELTX_LOAD_ALL_MEDIA,
            EE_SCREENS_SELTX_POST_TEXT,
            EE_SCREENS_SELTX_TECH_LOAD_MEDIA,
            EE_SCREENS_SELTX_POST_TEXT,
            EE_SCREENS_SELTX_TECH_LOAD_MEDIA,
            EE_SCREENS_SELTX_POST_TEXT
        ]
    );
    assert_eq!(r.ops[1].2[0], 2);
    assert_eq!(r.ops[6].1[1], 77);
    assert_eq!((r.ops[6].2[0], r.ops[8].2[0]), (0, 1));
    // the format "Loading %s Epoch , for Player (%d)" with the epochs' titles, then the empty text (not counted)
    let format = resource_string(EE_SCREENS_SELTX_IDS_LOADING_EPOCH_FOR_PLAYER).unwrap();
    assert!(format.windows(2).any(|w| w == b"%s"));
    assert_eq!(r.texts.len(), 3);
    assert!(r.texts.iter().all(|t| t.0 == EE_SCREENS_SELTX_POST_TEXT && t.1.ends_with(b"(2)")));
}

#[test]
fn handle_408_loads_and_unloads_the_terrain_s_classes() {
    let mut r = Rec::default();
    r.answer(EE_SCREENS_SELTX_PLAYER, &[10]);
    r.answer(EE_SCREENS_SELTX_TERRAIN, &[20]);
    r.answer(EE_SCREENS_SELTX_MAP_EXISTS, &[1]);
    r.answer(EE_SCREENS_SELTX_TERRAIN_CLASSES, &[1]);
    r.answer(EE_SCREENS_SELTX_TERRAIN_CLASS, &[5]);
    r.answer(EE_SCREENS_SELTX_CLASS, &[30]);
    r.run(|x| handle_message(x, 1, 0x408, 3, 0));
    let n = r.names();
    assert_eq!(
        n[..4],
        [
            EE_SCREENS_SELTX_PLAYER,
            EE_SCREENS_SELTX_TERRAIN,
            EE_SCREENS_SELTX_MAP_EXISTS,
            EE_SCREENS_SELTX_TERRAIN_NAME_ID
        ]
    );
    assert!(n.contains(&EE_SCREENS_SELTX_TERRAIN_LOAD));
    assert!(n.contains(&EE_SCREENS_SELTX_SET_CLASS_MEDIA) && n.contains(&EE_SCREENS_SELTX_LOAD_CLASS_BUTTONS));
    assert_eq!(*n.last().unwrap(), EE_SCREENS_SELTX_POST_TEXT);
    let mut r2 = Rec::default();
    r2.answer(EE_SCREENS_SELTX_TERRAIN, &[20]);
    r2.answer(EE_SCREENS_SELTX_MAP_EXISTS, &[1]);
    r2.answer(EE_SCREENS_SELTX_TERRAIN_CLASSES, &[1]);
    r2.answer(EE_SCREENS_SELTX_CLASS, &[0]);
    r2.run(|x| handle_message(x, 1, 0x409, 3, 0));
    let n = r2.names();
    assert!(n.contains(&EE_SCREENS_SELTX_TERRAIN_UNLOAD) && !n.contains(&EE_SCREENS_SELTX_UNLOAD_CLASS_MEDIA));
    assert!(!n.contains(&EE_SCREENS_SELTX_POST_TEXT));
    // no terrain: nothing past the reads
    let mut r3 = Rec::default();
    r3.run(|x| handle_message(x, 1, 0x408, 3, 0));
    assert_eq!(r3.names(), [EE_SCREENS_SELTX_PLAYER, EE_SCREENS_SELTX_TERRAIN]);
}

#[test]
fn handle_402_makes_the_ghost() {
    let mut r = Rec::default();
    r.answer(EE_SCREENS_SELTX_CLASS, &[30]);
    r.answer(EE_SCREENS_SELTX_PLAYER, &[10]);
    r.answer(EE_SCREENS_SELTX_CLASS_TYPE, &[12]);
    r.run(|x| handle_message(x, 1, 0x402, 4, 1));
    let n = r.names();
    let ghost = n.iter().position(|&o| o == EE_SCREENS_SELTX_GHOST).unwrap();
    assert_eq!(r.ops[ghost].2, [12, 1]);
    assert_eq!(r.ops[ghost].1, [1, 10]);
    assert_eq!(n[ghost + 1], EE_SCREENS_SELTX_UNLOAD_CLASS_MEDIA);
    // a missing class: nothing after the unlock
    let mut r2 = Rec::default();
    r2.run(|x| handle_message(x, 1, 0x402, 4, 1));
    assert_eq!(*r2.names().last().unwrap(), EE_SCREENS_SELTX_READ_UNLOCK);
}

#[test]
fn handle_403_and_407() {
    let mut r = Rec::default();
    r.run(|x| {
        handle_message(x, 1, 0x403, 2, 1);
        handle_message(x, 1, 0x407, 2, 3);
    });
    let n = r.names();
    assert_eq!(
        n[..8],
        [
            EE_SCREENS_SELTX_WRITE_LOCK,
            EE_SCREENS_SELTX_PLAYER,
            EE_SCREENS_SELTX_RELOAD_TECH,
            EE_SCREENS_SELTX_FLUSH_PLACEMENTS,
            EE_SCREENS_SELTX_ADD_PENDING,
            EE_SCREENS_SELTX_REMOVE_KILLED,
            EE_SCREENS_SELTX_RELEASE_PENDING,
            EE_SCREENS_SELTX_WRITE_UNLOCK
        ]
    );
    assert_eq!(r.ops[2].2[0], 1);
    assert_eq!(n.iter().filter(|&&o| o == EE_SCREENS_SELTX_TECH_UNLOAD_MEDIA).count(), 3);
}

#[test]
fn export_reports_success_or_error() {
    let mut r = Rec::default();
    r.answer(EE_SCREENS_SELTX_EXPORT_FILE, &[50]);
    r.answer(EE_SCREENS_SELTX_WRITE_HEADER, &[14]);
    r.answer(EE_SCREENS_SELTX_NAME_CHANGED, &[1]);
    r.run(|x| export(x, 3));
    assert_eq!(
        r.names(),
        [
            EE_SCREENS_SELTX_EXPORT_FILE,
            EE_SCREENS_SELTX_FOPEN_W,
            EE_SCREENS_SELTX_REFRESH_GROUP,
            EE_SCREENS_SELTX_WRITE_HEADER,
            EE_SCREENS_SELTX_EXPORT_TEXT,
            EE_SCREENS_SELTX_NAME_CHANGED,
            EE_SCREENS_SELTX_SET_EXPORT_NAME,
            EE_SCREENS_SELTX_MESSAGE,
            EE_SCREENS_SELTX_CLOSE
        ]
    );
    let ok = resource_string(EE_SCREENS_SELTX_IDS_FILE_EXPORTED_SUCCESSFULLY).unwrap();
    let changed = resource_string(EE_SCREENS_SELTX_IDS_EXPORTED_FILE_NAME_HAS_CHANGED).unwrap();
    assert_eq!(r.texts[0].1, [ok, b"\n", changed].concat());
    // a failed open: the file error, no close
    let mut r = Rec::default();
    r.answer(EE_SCREENS_SELTX_FOPEN_W, &[3]);
    r.run(|x| export(x, 3));
    assert_eq!(r.names(), [EE_SCREENS_SELTX_EXPORT_FILE, EE_SCREENS_SELTX_FOPEN_W, EE_SCREENS_SELTX_MESSAGE]);
    assert_eq!(r.texts[0].1, resource_string(EE_SCREENS_SELTX_IDS_FILE_ERROR).unwrap());
}

#[test]
fn the_events_build_the_spec() {
    // no name: the message, the spec freed
    let mut r = Rec::default();
    r.answer(EE_SCREENS_SELTX_SPEC_NEW, &[60]);
    r.run(|x| assert_eq!(export_event(x, 9), 0));
    assert_eq!(
        r.names(),
        [
            EE_SCREENS_SELTX_SPEC_NEW,
            EE_SCREENS_SELTX_R_EDIT_TEXT,
            EE_SCREENS_SELTX_FORM_MESSAGE,
            EE_SCREENS_SELTX_SPEC_DELETE
        ]
    );
    // a new file: posted at once
    let mut r = Rec { out_text: b"camp".to_vec(), ..Rec::default() };
    r.answer(EE_SCREENS_SELTX_SPEC_NEW, &[60]);
    r.answer(EE_SCREENS_SELTX_FOPEN_R, &[5]);
    r.answer(EE_SCREENS_SELTX_SPEC_COPY, &[61]);
    r.run(|x| assert_eq!(export_event(x, 9), 5));
    assert_eq!(
        r.names(),
        [
            EE_SCREENS_SELTX_SPEC_NEW,
            EE_SCREENS_SELTX_R_EDIT_TEXT,
            EE_SCREENS_SELTX_SPEC_VOLUME,
            EE_SCREENS_SELTX_SPEC_DIRECTORY,
            EE_SCREENS_SELTX_SPEC_FILE,
            EE_SCREENS_SELTX_FOPEN_R,
            EE_SCREENS_SELTX_SPEC_COPY,
            EE_SCREENS_SELTX_POST_EXPORT,
            EE_SCREENS_SELTX_SPEC_DELETE,
            EE_SCREENS_SELTX_SPEC_DELETE
        ]
    );
    assert_eq!(r.texts[0].1, b"camp.lcl");
    assert_eq!((r.ops[7].1[1], r.ops[8].1[1], r.ops[9].1[1]), (61, 61, 60));
    // the import of an existing file: the question carrying the info, then the close
    let mut r = Rec { out_text: b"camp".to_vec(), ..Rec::default() };
    r.answer(EE_SCREENS_SELTX_SPEC_NEW, &[60]);
    r.answer(EE_SCREENS_SELTX_INFO_NEW, &[62]);
    r.run(|x| assert_eq!(import_event(x, 9), 0));
    let n = r.names();
    let q = n.iter().position(|&o| o == EE_SCREENS_SELTX_FORM_QUESTION).unwrap();
    assert_eq!(n[q - 1], EE_SCREENS_SELTX_INFO_NEW);
    assert_eq!(r.ops[q - 1].2[0], 1);
    assert_eq!((r.ops[q].1, r.ops[q].2[0]), ([9, 62], 1));
    assert_eq!(n[q + 1..], [EE_SCREENS_SELTX_CLOSE, EE_SCREENS_SELTX_SPEC_DELETE]);
}

#[test]
fn import_checks_the_version() {
    // a good header, a validation run with errors: the question with the error list
    let mut r = Rec { out_text: b"{Version_1}\n".to_vec(), ..Rec::default() };
    r.answer(EE_SCREENS_SELTX_IMPORT_FILE, &[50]);
    r.answer(EE_SCREENS_SELTX_FGETS, &[1]);
    r.answer(EE_SCREENS_SELTX_TEXT_VERSION, &[1]);
    r.answer(EE_SCREENS_SELTX_IMPORT_VALIDATE, &[1]);
    r.answer(EE_SCREENS_SELTX_IMPORT_TEXT, &[1]);
    r.answer(EE_SCREENS_SELTX_ERROR_COUNT, &[1]);
    r.run(|x| import(x, 3));
    let n = r.names();
    assert_eq!(
        n[..4],
        [EE_SCREENS_SELTX_IMPORT_FILE, EE_SCREENS_SELTX_FOPEN_R, EE_SCREENS_SELTX_FEOF, EE_SCREENS_SELTX_FGETS]
    );
    let q = n.iter().position(|&o| o == EE_SCREENS_SELTX_QUESTION).unwrap();
    assert_eq!(n[q - 1], EE_SCREENS_SELTX_INFO_NEW);
    assert_eq!(n[q + 1..], [EE_SCREENS_SELTX_RESUME_GROUP, EE_SCREENS_SELTX_CLOSE]);
    let head = resource_string(EE_SCREENS_SELTX_IDS_FOLLOWING_ERRORS_OCCURRED_WHILE_IMPORTING).unwrap();
    assert_eq!(r.texts[0].1, [head, b"\n\n{Version_1}\n\n\n"].concat());
    // a newer version: the invalid version message after the close
    let mut r = Rec { out_text: b"{Version_2}".to_vec(), ..Rec::default() };
    r.answer(EE_SCREENS_SELTX_FGETS, &[1]);
    r.answer(EE_SCREENS_SELTX_TEXT_VERSION, &[1]);
    r.run(|x| import(x, 3));
    let n = r.names();
    assert_eq!(n[n.len() - 2..], [EE_SCREENS_SELTX_CLOSE, EE_SCREENS_SELTX_MESSAGE]);
    assert_eq!(r.texts[0].1, resource_string(EE_SCREENS_SELTX_IDS_LCL_FILE_HAS_INVALID_VERSION).unwrap());
    // a success
    let mut r = Rec { out_text: b"{Version_1}".to_vec(), ..Rec::default() };
    r.answer(EE_SCREENS_SELTX_FGETS, &[1]);
    r.answer(EE_SCREENS_SELTX_TEXT_VERSION, &[1]);
    r.run(|x| import(x, 3));
    assert_eq!(r.texts[0].1, resource_string(EE_SCREENS_SELTX_IDS_FILE_IMPORTED_SUCCESSFULLY).unwrap());
}

#[test]
fn the_answers_post_and_free() {
    let mut r = Rec::default();
    r.run(|x| {
        export_answer(x, 4);
        import_answer(x, 5);
    });
    assert_eq!(
        r.names(),
        [
            EE_SCREENS_SELTX_POST_EXPORT,
            EE_SCREENS_SELTX_SPEC_DELETE,
            EE_SCREENS_SELTX_POST_IMPORT,
            EE_SCREENS_SELTX_INFO_DELETE
        ]
    );
}
