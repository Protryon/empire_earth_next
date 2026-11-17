//! TEST ONLY: the SGVW trace (object storage plan, lane S): every value the screens and the scenario editor read
//! from the game's objects, with its arguments, in the order they are read. It is the gate for moving the screens'
//! game reads (the read view's C++ fills, `game_view.rs`; the world reads of `EEScreens.cpp`; the editor's unit and
//! player tab hosts, `EESEUnit.cpp` and `EESERBUnitTypes.cpp`) onto the world's typed views: the trace is recorded
//! where the Rust asks, before the answer is made, so the same lines must come out whichever side answers.
//!
//! On when `EE_SGVW` names a file (it is replaced). One line per read:
//!
//! - `V <op> <a> <b> -> <answer>`: a read view op (`game_view::op`);
//! - `T <which> <arg> -> "<text>"`: a read view text (`game_view::text`; bytes outside printable ASCII as `\xNN`);
//! - `C <op> <a> <b> <c> <d> -> <answer>`: a world read of the screens' C++ host ([`HOST_WORLD_OPS`]);
//! - `HK <aircraft> <records>`, `DP <index> <record>`, `PH <record>`, `PR <index> <record>`: the hot keys' unit
//!   snapshot, the diplomacy form's player, the post-game summary's history and record (their record fills);
//! - `E <op> <i> <f> -> <i> <f> <answer>`: an editor tab host op among [`EDITOR_WORLD_OPS`] (`ee_screenscpp_sepl`,
//!   `ee_screenscpp_seun`): its integers and floats before and after, and its answer.
//!
//! Each line ends `|L<r><w> t<n>`: the world lock's read and write holds by the reading thread when the read began
//! (`ee_base::ts::held`), and the thread's number (1 for the first thread that read, and so on). A line equal to the
//! one before it is not repeated: `R <n>` counts its repeats, written before the next different line.

use std::fmt::Write as _;

use crate::screens::debug_sound::EE_SCREENS_GAME_DSU_SOUND;
use crate::screens::editor_players::*;
use crate::screens::editor_unit::*;
use crate::screens::main_game::{EE_SCREENS_GAME_MG_DIALOG_SHOWN, EE_SCREENS_GAME_MG_SELECTED_COMMAND};

/// The screens' host ops that read the world (`ee_screenscpp_call`'s cases the views replace).
#[cfg_attr(test, allow(dead_code))]
pub(crate) const HOST_WORLD_OPS: [u32; 3] =
    [EE_SCREENS_GAME_MG_SELECTED_COMMAND, EE_SCREENS_GAME_MG_DIALOG_SHOWN, EE_SCREENS_GAME_DSU_SOUND];

/// The editor tab hosts' ops that read (or read and write) the world, its players, units, classes and tech trees.
pub(crate) const EDITOR_WORLD_OPS: [u32; 35] = [
    EE_SCREENS_SEPL_OP_UNIT_LIMIT_RULES,
    EE_SCREENS_SEPL_OP_NUM_PLAYERS,
    EE_SCREENS_SEPL_OP_PLAYER_EXISTS,
    EE_SCREENS_SEPL_OP_MAX_PLAYERS,
    EE_SCREENS_SEPL_OP_IS_COMPUTER,
    EE_SCREENS_SEPL_OP_TECH_TYPES,
    EE_SCREENS_SEPL_OP_FILL_TECHS,
    EE_SCREENS_SEPL_OP_PLAYER_GET,
    EE_SCREENS_SEPL_OP_PLAYER_SET,
    EE_SCREENS_SEPL_OP_CIV_SELECT,
    EE_SCREENS_SEPL_OP_CIV_APPLY,
    EE_SCREENS_SEPL_OP_NAME_TO_EDIT,
    EE_SCREENS_SEPL_OP_NAME_FROM_EDIT,
    EE_SCREENS_SEPL_OP_NAME_TO_LABEL,
    EE_SCREENS_SEPL_OP_EPOCH_RANGE,
    EE_SCREENS_SEPL_OP_APPLY_COLOR,
    EE_SCREENS_SEPL_OP_ENEMY,
    EE_SCREENS_SEPL_OP_SET_STANCE,
    EE_SCREENS_SEPL_OP_WORLD_GET,
    EE_SCREENS_SEPL_OP_WORLD_SET,
    EE_SCREENS_SEPL_OP_LOAD_THREAD,
    EE_SCREENS_SEUN_OP_GATE_HIGHLIGHTS,
    EE_SCREENS_SEUN_OP_SHOW_UNIT,
    EE_SCREENS_SEUN_OP_PLACE_INFO,
    EE_SCREENS_SEUN_OP_START_BUILD,
    EE_SCREENS_SEUN_OP_POST_GHOST,
    EE_SCREENS_SEUN_OP_SELECTED_COUNT,
    EE_SCREENS_SEUN_OP_CARGO_COUNT,
    EE_SCREENS_SEUN_OP_CARGO_ADD,
    EE_SCREENS_SEUN_OP_LIST_ADD,
    EE_SCREENS_SEUN_OP_CLASS_COUNT,
    EE_SCREENS_SEUN_OP_CLASS,
    EE_SCREENS_SEUN_OP_PICK,
    EE_SCREENS_SEUN_OP_HOLD_GATE,
    EE_SCREENS_SEUN_OP_DELETE_SELECTION,
];

/// A text as the trace writes it: printable ASCII as is (but `\` and `"`), the rest as `\xNN`.
pub(crate) fn quote(text: &[u8]) -> String {
    let mut s = String::with_capacity(text.len() + 2);
    s.push('"');
    for &c in text {
        if (0x20..0x7f).contains(&c) && c != b'\\' && c != b'"' {
            s.push(c as char);
        } else {
            let _ = write!(s, "\\x{c:02x}");
        }
    }
    s.push('"');
    s
}

/// The lines so far and the repeat count of the last one.
#[derive(Default)]
pub(crate) struct Lines {
    last: String,
    repeats: u32,
}

impl Lines {
    /// What to write for `line`: nothing for a repeat of the last line, else the last line's repeats (`R <n>`)
    /// and `line`.
    pub(crate) fn push(&mut self, line: &str) -> String {
        if line == self.last {
            self.repeats += 1;
            return String::new();
        }
        let mut out = String::new();
        if self.repeats > 0 {
            let _ = writeln!(out, "R {}", self.repeats);
        }
        out.push_str(line);
        out.push('\n');
        self.last = line.to_owned();
        self.repeats = 0;
        out
    }
}

/// A read's line: its body and where it began (the world lock's holds, the thread).
pub(crate) fn line(body: &str, held: (i32, i32), thread: u32) -> String {
    format!("{body} |L{}{} t{thread}", held.0, held.1)
}

/// An editor op's line (`E`): the op, its integers and floats before and after, its answer.
pub(crate) fn editor_line(
    op: u32,
    before: (&[i32; 4], &[f32; 4]),
    after: (&[i32; 4], &[f32; 4]),
    answer: i32,
) -> String {
    let bits = |f: &[f32; 4]| f.map(f32::to_bits);
    format!("E {op} {:?} {:x?} -> {:?} {:x?} {answer}", before.0, bits(before.1), after.0, bits(after.1))
}

#[cfg(not(test))]
pub(crate) use game::*;

/// The trace in the game: the file, the world lock, the thread numbers.
#[cfg(not(test))]
mod game {
    use std::io::Write as _;
    use std::sync::atomic::{AtomicU32, Ordering::Relaxed};
    use std::sync::{Mutex, OnceLock};

    use super::Lines;

    /// cbindgen:ignore
    mod sys {
        extern "C" {
            /// `gWorld.mLock` (TEST ONLY, `UI Events/EEScreens.cpp`).
            pub fn ee_screenscpp_sgvw_world_lock() -> *const core::ffi::c_void;
        }
    }

    struct Sink {
        file: std::fs::File,
        lines: Lines,
    }

    static SINK: OnceLock<Option<Mutex<Sink>>> = OnceLock::new();
    static THREADS: AtomicU32 = AtomicU32::new(0);

    thread_local! {
        static THREAD: u32 = THREADS.fetch_add(1, Relaxed) + 1;
    }

    fn sink() -> Option<&'static Mutex<Sink>> {
        SINK.get_or_init(|| {
            let path = std::env::var_os("EE_SGVW")?;
            let file = std::fs::File::create(path).ok()?;
            Some(Mutex::new(Sink { file, lines: Lines::default() }))
        })
        .as_ref()
    }

    /// Where a read began, when the trace is on: the world lock's holds by this thread and its number.
    #[derive(Clone, Copy)]
    pub(crate) struct Begun((i32, i32), u32);

    /// A read begins: `None` when the trace is off.
    pub(crate) fn begin() -> Option<Begun> {
        sink()?;
        // SAFETY: a TEST ONLY accessor of a global's member
        let lock = unsafe { sys::ee_screenscpp_sgvw_world_lock() };
        let held = if lock.is_null() { (0, 0) } else { ee_base::ts::held::held(lock as usize) };
        Some(Begun(held, THREAD.with(|t| *t)))
    }

    /// The read begun at `b` with its line's body.
    pub(crate) fn record(b: Begun, body: &str) {
        let Some(sink) = sink() else { return };
        let line = super::line(body, b.0, b.1);
        let mut s = sink.lock().unwrap_or_else(|e| e.into_inner());
        let out = s.lines.push(&line);
        if !out.is_empty() {
            let _ = s.file.write_all(out.as_bytes());
        }
    }

    /// An editor tab host op through `call`, traced when it is one of the world ops.
    pub(crate) fn editor(
        op: u32,
        i: &mut [i32; 4],
        f: &mut [f32; 4],
        call: impl FnOnce(&mut [i32; 4], &mut [f32; 4]) -> i32,
    ) -> i32 {
        let begun = if super::EDITOR_WORLD_OPS.contains(&op) { begin() } else { None };
        let before = (*i, *f);
        let r = call(i, f);
        if let Some(b) = begun {
            record(b, &super::editor_line(op, (&before.0, &before.1), (i, f), r));
        }
        r
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn quotes_bytes() {
        assert_eq!(quote(b"Ab 1"), "\"Ab 1\"");
        assert_eq!(quote(b"a\\b\"c"), "\"a\\x5cb\\x22c\"");
        assert_eq!(quote("é\n".as_bytes()), "\"\\xc3\\xa9\\x0a\"");
        assert_eq!(quote(b""), "\"\"");
    }

    #[test]
    fn repeats_are_counted() {
        let mut l = Lines::default();
        assert_eq!(l.push("a"), "a\n");
        assert_eq!(l.push("a"), "");
        assert_eq!(l.push("a"), "");
        assert_eq!(l.push("b"), "R 2\nb\n");
        assert_eq!(l.push("a"), "a\n");
        assert_eq!(l.push("b"), "b\n");
    }

    #[test]
    fn formats_lines() {
        assert_eq!(line("V 1 2 3 -> 4", (1, 0), 2), "V 1 2 3 -> 4 |L10 t2");
        assert_eq!(
            editor_line(39, (&[1, 2, 0, 0], &[0.0, 1.0, 0.0, 0.0]), (&[1, 2, 7, 0], &[0.5, 1.0, 0.0, 0.0]), 7),
            "E 39 [1, 2, 0, 0] [0, 3f800000, 0, 0] -> [1, 2, 7, 0] [3f000000, 3f800000, 0, 0] 7"
        );
    }

    #[test]
    fn op_lists_have_no_duplicates() {
        let mut e = EDITOR_WORLD_OPS.to_vec();
        e.sort_unstable();
        e.dedup();
        assert_eq!(e.len(), EDITOR_WORLD_OPS.len());
        // the player tab's (below 300) and the unit tab's (300 and up) never share a number
        assert!(EDITOR_WORLD_OPS.iter().all(|&op| !(100..300).contains(&op)));
    }
}
