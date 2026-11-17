//! The scenario editor's instructions tab whole in Rust (`EESEInstructions`, once its members in
//! `Scenario Editor/EESEInstructions.cpp` 0x5707ff..0x57202b): the constructor (the map, movie and sound file boxes
//! with their directories made when missing, the instructions, aftermath, hints and history labels and boxes, the
//! text import and export buttons, the localization file name), the destructor, `Load`, `Enter`, `Store`.
//!
//! The controls are the shared maker's records; the file boxes (`EEUIFileComboBox` over a directory's files), the
//! import and export buttons' events (`EESEIEImport`, `EESEIEExport`) and the scenario's media strings (private to
//! `EESMMediaStrings`, a friend of the class) are the tab's host, `ee_screenscpp_sein` (ops from 700,
//! `Scenario Editor/EESEInstructions.cpp`). The texts move between the controls and the scenario inside the host.

use crate::screens::editor_object::{screen_event, EE_SCREENS_SEPL_ON_ENTER};
use crate::screens::editor_players::{
    display, EeScreensSeplCtl, Maker, SeplHost, EE_SCREENS_SEPL_ALIGN_CENTER, EE_SCREENS_SEPL_BUTTON,
    EE_SCREENS_SEPL_EDIT, EE_SCREENS_SEPL_FG, EE_SCREENS_SEPL_FONT_538, EE_SCREENS_SEPL_FONT_HEIGHT,
    EE_SCREENS_SEPL_LABEL, EE_SCREENS_SEPL_OP_DELETE,
};
use crate::screens::scenario_editor::EE_SCREENS_SE_FIND_TEXT;

// ---- the tab's host (`ee_screenscpp_sein`)

/// The scenarios', movies' and sounds' directories made when missing -> their paths (`FSPath`) into p0's three
/// words / the three paths p0 released.
pub const EE_SCREENS_SEIN_OP_PATHS: u32 = 700;
pub const EE_SCREENS_SEIN_OP_PATHS_RELEASE: u32 = 701;
/// A file box (`EEUIFileComboBox`) of the kind i0 (`EE_SCREENS_SEIN_FILES_*`: its path from p1's words, its
/// pattern, its id, whether it lists none) made on the form p2 at the index i1, at (f0, f1), 150 wide; its text
/// yellow, its list's line blue, its text box at 0.4; group 7 -> into p0.
pub const EE_SCREENS_SEIN_OP_MAKE_FILES: u32 = 702;
/// The button p0's event: the import's (i0 = 0, `EESEIEImport`) or the export's (`EESEIEExport`).
pub const EE_SCREENS_SEIN_OP_BUTTON_EVENT: u32 = 703;
/// The file box p0's entries made again (`CreateEntries`).
pub const EE_SCREENS_SEIN_OP_FILE_ENTRIES: u32 = 704;
/// `Load`'s and `Store`'s transfers over the controls p0's words (`EE_SCREENS_SEIN_C_*`): the scenario's media
/// strings into the controls, the localization file's name into its box / the controls into the media strings.
pub const EE_SCREENS_SEIN_OP_LOAD: u32 = 705;
pub const EE_SCREENS_SEIN_OP_STORE: u32 = 706;

/// The file boxes' kinds: the scenario's map picture (`*.jpg`), the movie (`*.mpg`), the sound (`*.mp3`).
pub const EE_SCREENS_SEIN_FILES_MAP: i32 = 0;
pub const EE_SCREENS_SEIN_FILES_MOVIE: i32 = 1;
pub const EE_SCREENS_SEIN_FILES_SOUND: i32 = 2;

/// The controls `Load` and `Store` read, in the words of their array: the three file boxes, the four text boxes
/// (instructions, aftermath, hints, history), the localization box.
pub const EE_SCREENS_SEIN_C_MAP: usize = 0;
pub const EE_SCREENS_SEIN_C_MOVIE: usize = 1;
pub const EE_SCREENS_SEIN_C_SOUND: usize = 2;
pub const EE_SCREENS_SEIN_C_INSTRUCTIONS: usize = 3;
pub const EE_SCREENS_SEIN_C_AFTERMATH: usize = 4;
pub const EE_SCREENS_SEIN_C_HINTS: usize = 5;
pub const EE_SCREENS_SEIN_C_HISTORY: usize = 6;
pub const EE_SCREENS_SEIN_C_LOCALIZATION: usize = 7;
pub const EE_SCREENS_SEIN_C_COUNT: usize = 8;

/// The tab's members (`ee_screens_sein_call`): `Load`, `Enter`, `Store` (and `Leave`, which stores).
pub const EE_SCREENS_SEIN_LOAD: u32 = 1;
pub const EE_SCREENS_SEIN_ENTER: u32 = 2;
pub const EE_SCREENS_SEIN_STORE: u32 = 3;

const IDS_SCEN_INST: u32 = 20543;
const IDS_MOVIE: u32 = 20544;
const IDS_INST_SOUND_OVER: u32 = 20545;
const IDS_SCENARIO_INSTRUCTIONS: u32 = 20549;
const IDS_AFTERMATH: u32 = 20550;
const IDS_SE_HINTS: u32 = 20551;
const IDS_SE_HISTORY: u32 = 20555;
const IDS_IMPORT: u32 = 30471;
const IDS_EXPORT: u32 = 30472;
const IDS_IMPORT_LOCALIZED_TEXT_FILE_INTO_SCENARIO: u32 = 11001;
const IDS_EXPORT_SCENARIO_TO_TEXT_FILE: u32 = 11002;
const IDS_LOCALIZATION_TEXT_FILE_NAME: u32 = 30473;

/// The tab's control group.
const GROUP: u32 = 7;

fn call<H: SeplHost + ?Sized>(h: &mut H, op: u32, p: [usize; 3], i: [i32; 4], f: [f32; 4]) {
    let (mut i, mut f) = (i, f);
    h.op(op, p, &mut i, &mut f);
}

/// A yellow centred label at (`x`, `y`), `w` wide, the font's height high (`h` 0) or `h`.
fn label(id: u32, string: u32, x: f32, y: f32, w: f32, h: f32) -> EeScreensSeplCtl {
    let c = EeScreensSeplCtl::new(EE_SCREENS_SEPL_LABEL, id, x, y, w, h)
        .string(string)
        .align(EE_SCREENS_SEPL_ALIGN_CENTER)
        .bg(0.4)
        .flags(EE_SCREENS_SEPL_FG)
        .group(GROUP);
    if h == 0.0 {
        c.flags(EE_SCREENS_SEPL_FONT_HEIGHT)
    } else {
        c
    }
}

/// A multi-line yellow text box 984 x 120 at `y` (its Enter finds the text).
fn text_box(id: u32, y: f32) -> EeScreensSeplCtl {
    EeScreensSeplCtl::new(EE_SCREENS_SEPL_EDIT, id, 20.0, y, 984.0, 120.0)
        .bg(0.4)
        .flags(EE_SCREENS_SEPL_FG)
        .extra([0, 0, 1, 0])
        .group(GROUP)
}

/// A text button 150 x 20 at `y` (in `gU531938`), its help string `help`.
fn button(id: u32, string: u32, y: f32) -> EeScreensSeplCtl {
    EeScreensSeplCtl::new(EE_SCREENS_SEPL_BUTTON, id, 20.0, y, 150.0, 20.0)
        .string(string)
        .font(EE_SCREENS_SEPL_FONT_538)
        .bg(0.4)
        .flags(EE_SCREENS_SEPL_FG)
        .extra([2, 0, 0, 0])
        .group(GROUP)
}

/// The instructions tab (`EESEInstructions`): its controls, in the class's member order.
#[derive(Debug, Default, Clone, PartialEq)]
pub struct InstructionsTab {
    pub map_label: usize,
    pub movie_label: usize,
    pub sound_label: usize,
    pub map_files: usize,
    pub movie_files: usize,
    pub sound_files: usize,
    pub instructions_label: usize,
    pub instructions: usize,
    pub aftermath_label: usize,
    pub aftermath: usize,
    pub hints_label: usize,
    pub hints: usize,
    pub history_label: usize,
    pub history: usize,
    pub import: usize,
    pub export: usize,
    pub localization_label: usize,
    pub localization: usize,
}

impl InstructionsTab {
    /// The constructor (0x5707ff) on `form` from the control index `*index` (advanced). The history label and
    /// box reuse the hints' ids (as the original).
    pub fn new<H: SeplHost + ?Sized>(h: &mut H, form: usize, index: &mut u32) -> InstructionsTab {
        let mut paths = [0usize; 3];
        call(h, EE_SCREENS_SEIN_OP_PATHS, [paths.as_mut_ptr() as usize, 0, 0], [0; 4], [0.0; 4]);
        let mut t = InstructionsTab::default();
        let mut m = Maker { h, form, index: *index };
        t.map_label = m.make(label(0x44c, IDS_SCEN_INST, 20.0, 2.0, 150.0, 0.0));
        t.movie_label = m.make(label(0x44e, IDS_MOVIE, 437.0, 2.0, 150.0, 0.0));
        t.sound_label = m.make(label(0x450, IDS_INST_SOUND_OVER, 854.0, 2.0, 150.0, 0.0));
        t.instructions_label = m.make(label(0x452, IDS_SCENARIO_INSTRUCTIONS, 20.0, 62.0, 984.0, 0.0));
        t.aftermath_label = m.make(label(0x454, IDS_AFTERMATH, 20.0, 386.0, 984.0, 0.0));
        display(m.h, t.aftermath_label, false);
        t.hints_label = m.make(label(0x456, IDS_SE_HINTS, 20.0, 202.0, 984.0, 0.0));
        t.history_label = m.make(label(0x456, IDS_SE_HISTORY, 20.0, 342.0, 984.0, 0.0));
        let boxes = [(0x453, 80.0), (0x455, 404.0), (0x457, 220.0), (0x457, 360.0)];
        let mut made = [0usize; 4];
        for (k, (id, y)) in boxes.into_iter().enumerate() {
            made[k] = m.make(text_box(id, y));
            screen_event(m.h, made[k], EE_SCREENS_SE_FIND_TEXT, EE_SCREENS_SEPL_ON_ENTER);
            if k == 1 {
                display(m.h, made[k], false);
            }
        }
        [t.instructions, t.aftermath, t.hints, t.history] = made;
        let files = [
            (EE_SCREENS_SEIN_FILES_MAP, 20.0),
            (EE_SCREENS_SEIN_FILES_MOVIE, 437.0),
            (EE_SCREENS_SEIN_FILES_SOUND, 854.0),
        ];
        let mut made = [0usize; 3];
        for (k, (kind, x)) in files.into_iter().enumerate() {
            let mut out = 0usize;
            call(
                m.h,
                EE_SCREENS_SEIN_OP_MAKE_FILES,
                [&mut out as *mut usize as usize, paths.as_ptr() as usize, form],
                [kind, m.index as i32, 0, 0],
                [x, 22.0, 0.0, 0.0],
            );
            m.index += 1;
            made[k] = out;
        }
        [t.map_files, t.movie_files, t.sound_files] = made;
        let helps = [IDS_IMPORT_LOCALIZED_TEXT_FILE_INTO_SCENARIO, IDS_EXPORT_SCENARIO_TO_TEXT_FILE];
        for (k, (id, string, y)) in [(0x458, IDS_IMPORT, 494.0), (0x459, IDS_EXPORT, 516.0)].into_iter().enumerate() {
            let b = m.make(button(id, string, y));
            call(
                m.h,
                crate::screens::editor_map::EE_SCREENS_SEPL_OP_HELP,
                [b, 0, 0],
                [helps[k] as i32, 0, 0, 0],
                [0.0; 4],
            );
            call(m.h, EE_SCREENS_SEIN_OP_BUTTON_EVENT, [b, 0, 0], [k as i32, 0, 0, 0], [0.0; 4]);
            if k == 0 {
                t.import = b;
            } else {
                t.export = b;
            }
        }
        t.localization_label = m.make(label(0x45a, IDS_LOCALIZATION_TEXT_FILE_NAME, 200.0, 494.0, 400.0, 20.0));
        t.localization = m.make(
            EeScreensSeplCtl::new(EE_SCREENS_SEPL_EDIT, 0x45b, 200.0, 516.0, 400.0, 20.0)
                .bg(0.4)
                .flags(EE_SCREENS_SEPL_FG)
                .extra([0x100, 0, 0, 0])
                .group(GROUP),
        );
        *index = m.index;
        call(h, EE_SCREENS_SEIN_OP_PATHS_RELEASE, [paths.as_ptr() as usize, 0, 0], [0; 4], [0.0; 4]);
        t
    }

    /// The destructor (0x571bf5): every control deleted, in its order.
    pub fn destroy<H: SeplHost + ?Sized>(&self, h: &mut H) {
        let order = [
            self.map_label,
            self.movie_label,
            self.sound_label,
            self.map_files,
            self.movie_files,
            self.sound_files,
            self.instructions_label,
            self.instructions,
            self.aftermath_label,
            self.aftermath,
            self.hints_label,
            self.history_label,
            self.hints,
            self.history,
            self.import,
            self.export,
            self.localization_label,
            self.localization,
        ];
        for c in order {
            call(h, EE_SCREENS_SEPL_OP_DELETE, [c, 0, 0], [0; 4], [0.0; 4]);
        }
    }

    /// The controls `Load` and `Store` read (`EE_SCREENS_SEIN_C_*`).
    pub fn texts(&self) -> [usize; EE_SCREENS_SEIN_C_COUNT] {
        [
            self.map_files,
            self.movie_files,
            self.sound_files,
            self.instructions,
            self.aftermath,
            self.hints,
            self.history,
            self.localization,
        ]
    }

    /// `Load` (0x571ce3), `Enter` (0x571f62: the movie, sound and map files listed again), `Store` (0x571f8b).
    pub fn call<H: SeplHost + ?Sized>(&self, h: &mut H, what: u32) {
        let texts = self.texts();
        match what {
            EE_SCREENS_SEIN_LOAD => call(h, EE_SCREENS_SEIN_OP_LOAD, [texts.as_ptr() as usize, 0, 0], [0; 4], [0.0; 4]),
            EE_SCREENS_SEIN_STORE => {
                call(h, EE_SCREENS_SEIN_OP_STORE, [texts.as_ptr() as usize, 0, 0], [0; 4], [0.0; 4])
            }
            EE_SCREENS_SEIN_ENTER => {
                for c in [self.movie_files, self.sound_files, self.map_files] {
                    if c != 0 {
                        call(h, EE_SCREENS_SEIN_OP_FILE_ENTRIES, [c, 0, 0], [0; 4], [0.0; 4]);
                    }
                }
            }
            _ => {}
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    /// A host recording the ops: handles 1, 2, ... for the controls.
    #[derive(Default)]
    struct Rec {
        ops: Vec<(u32, [i32; 4])>,
        next: usize,
        ids: Vec<u32>,
    }

    impl SeplHost for Rec {
        fn op(&mut self, op: u32, p: [usize; 3], i: &mut [i32; 4], _f: &mut [f32; 4]) -> i32 {
            self.ops.push((op, *i));
            if op == crate::screens::editor_players::EE_SCREENS_SEPL_OP_MAKE || op == EE_SCREENS_SEIN_OP_MAKE_FILES {
                self.next += 1;
                // SAFETY: the makers pass their out handle in p0
                unsafe { *(p[0] as *mut usize) = self.next };
                if op == crate::screens::editor_players::EE_SCREENS_SEPL_OP_MAKE {
                    // SAFETY: the maker passes its record in p1
                    self.ids.push(unsafe { &*(p[1] as *const EeScreensSeplCtl) }.id);
                }
            }
            0
        }
    }

    #[test]
    fn new_makes_eighteen_controls_in_the_original_order() {
        let mut h = Rec::default();
        let mut index = 500;
        let t = InstructionsTab::new(&mut h, 9, &mut index);
        assert_eq!(index, 518);
        assert_eq!(
            h.ids,
            vec![
                0x44c, 0x44e, 0x450, 0x452, 0x454, 0x456, 0x456, 0x453, 0x455, 0x457, 0x457, 0x458, 0x459, 0x45a, 0x45b
            ]
        );
        assert_eq!((t.map_files, t.movie_files, t.sound_files), (12, 13, 14));
        // the paths made first and released last
        assert_eq!(h.ops.first().unwrap().0, EE_SCREENS_SEIN_OP_PATHS);
        assert_eq!(h.ops.last().unwrap().0, EE_SCREENS_SEIN_OP_PATHS_RELEASE);
        let mut all = t.texts().to_vec();
        all.extend([t.map_label, t.movie_label, t.sound_label, t.instructions_label, t.aftermath_label]);
        all.extend([t.hints_label, t.history_label, t.import, t.export, t.localization_label]);
        all.sort();
        assert_eq!(all, (1..=18).collect::<Vec<_>>());
    }

    #[test]
    fn enter_lists_the_movie_sound_and_map_files() {
        let mut h = Rec::default();
        let mut index = 0;
        let t = InstructionsTab::new(&mut h, 9, &mut index);
        h.ops.clear();
        t.call(&mut h, EE_SCREENS_SEIN_ENTER);
        assert_eq!(h.ops.len(), 3);
        assert!(h.ops.iter().all(|o| o.0 == EE_SCREENS_SEIN_OP_FILE_ENTRIES));
    }
}
