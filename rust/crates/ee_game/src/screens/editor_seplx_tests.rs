use super::*;
use std::collections::HashMap;

/// A recording host: the ops with p0 and i, the texts passed in, the answers by op (texts out by op).
#[derive(Default)]
struct Rec {
    ops: Vec<(u32, usize, [i32; 4], [f32; 4])>,
    answers: HashMap<u32, Vec<i32>>,
    texts_out: HashMap<(u32, i32), Vec<u8>>,
    texts: Vec<(u32, Vec<u8>)>,
}

impl Rec {
    fn answer(&mut self, op: u32, values: &[i32]) {
        self.answers.insert(op, values.iter().rev().copied().collect());
    }
    fn run<R>(&mut self, f: impl FnOnce(&mut Sepx) -> R) -> R {
        let mut x = |op: u32, p: [usize; 3], i: &mut [i32; 4], g: &mut [f32; 4]| -> i32 {
            self.ops.push((op, p[0], *i, *g));
            if i[3] > 0 {
                let t = self.texts_out.get(&(op, i[0])).cloned().unwrap_or_default();
                let n = t.len().min(i[3] as usize);
                // SAFETY: p1 holds i3 bytes
                unsafe { core::ptr::copy_nonoverlapping(t.as_ptr(), p[1] as *mut u8, n) };
                return t.len() as i32;
            }
            if i[2] > 0 || p[1] != 0 {
                // SAFETY: p1 holds i2 bytes
                let t = unsafe { core::slice::from_raw_parts(p[1] as *const u8, i[2].max(0) as usize) }.to_vec();
                self.texts.push((op, t));
            }
            match self.answers.get_mut(&op) {
                Some(v) if v.len() > 1 => v.pop().unwrap(),
                Some(v) => v.first().copied().unwrap_or(0),
                None => 0,
            }
        };
        f(&mut x)
    }
    fn names(&self) -> Vec<u32> {
        self.ops.iter().map(|o| o.0).collect()
    }
}

#[test]
fn civ_fill_lists_none_the_built_ins_and_the_files() {
    let mut r = Rec::default();
    r.answer(EE_SCREENS_SEPX_CIV_COUNT, &[2]);
    r.answer(EE_SCREENS_SEPX_FILES_COUNT, &[1]);
    r.texts_out.insert((EE_SCREENS_SEPX_FILE_STEM, 0), b"Mine".to_vec());
    r.run(|x| civ_fill(x, 9));
    let appends: Vec<_> = r.texts.iter().filter(|t| t.0 == EE_SCREENS_SEPX_R_COMBO_APPEND).collect();
    assert_eq!(appends.len(), 4);
    assert_eq!(appends[0].1, resource_string(EE_SCREENS_SEPX_IDS_NONE).unwrap());
    assert_eq!(appends[3].1, b"Mine");
    let n = r.names();
    assert_eq!(n[0], EE_SCREENS_SEPX_R_COMBO_CLEAR);
    assert_eq!(*n.last().unwrap(), EE_SCREENS_SEPX_R_COMBO_SELECT);
    assert!(n.contains(&EE_SCREENS_SEPX_FILES_CLEAR) && n.contains(&EE_SCREENS_SEPX_FILES_LIST));
}

#[test]
fn civ_select_finds_the_built_in_or_the_file() {
    // no file: none
    let mut r = Rec::default();
    r.run(|x| civ_select(x, 9, 1));
    assert_eq!(r.names(), [EE_SCREENS_SEPX_PLAYER_CIV, EE_SCREENS_SEPX_R_COMBO_SELECT]);
    assert_eq!(r.ops[1].2[0], 0);
    // a built-in: its line (index + 1)
    let mut r = Rec::default();
    r.answer(EE_SCREENS_SEPX_CIV_COUNT, &[3]);
    r.texts_out.insert((EE_SCREENS_SEPX_PLAYER_CIV, 1), b"b.civ".to_vec());
    r.texts_out.insert((EE_SCREENS_SEPX_CIV_FILE, 0), b"a.civ".to_vec());
    r.texts_out.insert((EE_SCREENS_SEPX_CIV_FILE, 1), b"b.civ".to_vec());
    r.run(|x| civ_select(x, 9, 1));
    let last = r.ops.last().unwrap();
    assert_eq!((last.0, last.2[0]), (EE_SCREENS_SEPX_R_COMBO_SELECT, 2));
    // a custom file: the matching line from the built-ins' count on
    let mut r = Rec::default();
    r.answer(EE_SCREENS_SEPX_CIV_COUNT, &[1]);
    r.answer(EE_SCREENS_SEPX_R_COMBO_LINES, &[3]);
    r.texts_out.insert((EE_SCREENS_SEPX_PLAYER_CIV, 1), b"Mine".to_vec());
    r.texts_out.insert((EE_SCREENS_SEPX_R_COMBO_LINE, 2), b"Mine".to_vec());
    r.run(|x| civ_select(x, 9, 1));
    let lines: Vec<_> = r.ops.iter().filter(|o| o.0 == EE_SCREENS_SEPX_R_COMBO_LINE).map(|o| o.2[0]).collect();
    assert_eq!(lines, [1, 2]);
    assert_eq!(r.ops.last().unwrap().2[0], 2);
    // unknown: none, titled "unknown"
    let mut r = Rec::default();
    r.texts_out.insert((EE_SCREENS_SEPX_PLAYER_CIV, 1), b"Gone".to_vec());
    r.run(|x| civ_select(x, 9, 1));
    assert_eq!(
        r.texts.last().unwrap(),
        &(
            EE_SCREENS_SEPX_R_COMBO_SET_LABEL,
            resource_string(EE_SCREENS_SEPX_IDS_UNKNOWN_CIVILIZATION).unwrap().to_vec()
        )
    );
}

#[test]
fn civ_apply_sets_clears_or_keeps() {
    // a built-in other than the player's: set and applied
    let mut r = Rec::default();
    r.answer(EE_SCREENS_SEPX_R_COMBO_SELECTION, &[1]);
    r.answer(EE_SCREENS_SEPX_CIV_COUNT, &[2]);
    r.texts_out.insert((EE_SCREENS_SEPX_CIV_FILE, 0), b"a.civ".to_vec());
    assert!(r.run(|x| civ_apply(x, 9, 2)));
    assert_eq!(r.texts.last().unwrap(), &(EE_SCREENS_SEPX_PLAYER_SET_CIV, b"a.civ".to_vec()));
    assert_eq!(*r.names().last().unwrap(), EE_SCREENS_SEPX_PLAYER_APPLY_CIV);
    // the same: unchanged
    let mut r = Rec::default();
    r.answer(EE_SCREENS_SEPX_R_COMBO_SELECTION, &[1]);
    r.answer(EE_SCREENS_SEPX_CIV_COUNT, &[2]);
    r.texts_out.insert((EE_SCREENS_SEPX_CIV_FILE, 0), b"a.civ".to_vec());
    r.texts_out.insert((EE_SCREENS_SEPX_PLAYER_CIV, 2), b"a.civ".to_vec());
    assert!(!r.run(|x| civ_apply(x, 9, 2)));
    // none picked over a file: cleared
    let mut r = Rec::default();
    r.texts_out.insert((EE_SCREENS_SEPX_PLAYER_CIV, 2), b"a.civ".to_vec());
    r.texts_out.insert((EE_SCREENS_SEPX_R_COMBO_LABEL, 0), b"None".to_vec());
    assert!(r.run(|x| civ_apply(x, 9, 2)));
    let n = r.names();
    assert_eq!(
        n[n.len() - 3..],
        [EE_SCREENS_SEPX_PLAYER_CIV_CLEAR, EE_SCREENS_SEPX_PLAYER_CIV_POINTS, EE_SCREENS_SEPX_PLAYER_SET_CIV]
    );
    // none picked while the label reads "unknown": kept
    let mut r = Rec::default();
    r.texts_out.insert((EE_SCREENS_SEPX_PLAYER_CIV, 2), b"a.civ".to_vec());
    r.texts_out.insert(
        (EE_SCREENS_SEPX_R_COMBO_LABEL, 0),
        resource_string(EE_SCREENS_SEPX_IDS_UNKNOWN_CIVILIZATION).unwrap().to_vec(),
    );
    assert!(!r.run(|x| civ_apply(x, 9, 2)));
}

#[test]
fn fill_epochs_replaces_the_lines() {
    let mut r = Rec::default();
    r.run(|x| fill_epochs(x, 9, 0));
    let n = r.names();
    assert_eq!(n[0], EE_SCREENS_SEPX_LINES_CLEAR);
    assert_eq!(n[n.len() - 2..], [EE_SCREENS_SEPX_R_COMBO_CLEAR, EE_SCREENS_SEPX_COMBO_APPEND_LINES]);
    let pushes = n.iter().filter(|&&o| o == EE_SCREENS_SEPX_LINES_PUSH).count();
    assert_eq!(pushes as i32, ee_sim::tech::tech_ids::ee_tech_epoch_count().max(0));
}

#[test]
fn the_diplomacy_bitmaps() {
    let mut r = Rec::default();
    r.answer(EE_SCREENS_SEPX_DIPLO_WIDTH, &[256]);
    r.run(|x| diplomacy_load(x, 5));
    assert_eq!(r.ops.len(), 1 + 2 * 3 * 4);
    // the enemies' pressed cell: slot 2, state 1, from 69/256 to 92/256
    let coords: Vec<_> = r.ops.iter().filter(|o| o.0 == EE_SCREENS_SEPX_DIPLO_COORDS).collect();
    assert_eq!((coords[4].2[0], coords[4].2[1]), (2, 1));
    assert_eq!(coords[4].3, [69.0 / 256.0, 92.0 / 256.0, 230.0 / 256.0, 253.0 / 256.0]);
    let mut r = Rec::default();
    r.run(diplomacy_release);
    let slots: Vec<_> = r.ops.iter().map(|o| (o.2[0], o.2[1])).collect();
    assert_eq!(slots, [(0, 0), (2, 0), (1, 0), (0, 1), (2, 1), (1, 1)]);
    let mut r = Rec::default();
    r.run(|x| diplomacy_show(x, 7, 1, true));
    assert_eq!(
        r.names(),
        [
            EE_SCREENS_SEPX_DIPLO_PRESSED,
            EE_SCREENS_SEPX_DIPLO_FOCUSED,
            EE_SCREENS_SEPX_DIPLO_NORMAL,
            EE_SCREENS_SEPX_R_HELP_ID,
            EE_SCREENS_SEPX_DIPLO_UPDATE_HELP
        ]
    );
    assert_eq!(r.ops[3].2[0], EE_SCREENS_SEPX_IDS_PLAYERS_CURRENTLY_ENEMIES);
    let mut r = Rec::default();
    r.run(|x| diplomacy_show(x, 7, 2, false));
    assert_eq!(r.names().len(), 3);
}

fn ctl(text: u32, string: u32, number: i32, extra1: i32) -> EeScreensSeplCtl {
    let mut c = EeScreensSeplCtl::new(0, 0, 0.0, 0.0, 0.0, 0.0);
    c.text = text;
    c.string = string;
    c.number = number;
    c.extra[1] = extra1;
    c
}

#[test]
fn prestep_sets_the_text_and_the_lines() {
    use crate::screens::editor_sepl::*;
    assert!(needs_prestep(EE_SCREENS_SEPX_MAKE_LABEL) && !needs_prestep(EE_SCREENS_SEPX_MAKE_SLIDER));
    // a label's text: "%s %ld" of the string and the number
    let mut r = Rec::default();
    let c = ctl(EE_SCREENS_SEPL_TEXT_STRING_NUMBER, EE_SCREENS_SEPX_IDS_NONE as u32, 7, 0);
    r.run(|x| prestep(x, EE_SCREENS_SEPX_MAKE_LABEL, &c));
    let none = resource_string(EE_SCREENS_SEPX_IDS_NONE).unwrap();
    assert_eq!(r.texts, [(EE_SCREENS_SEPX_SET_TEXT, [none, b" 7"].concat())]);
    // the max player combo: the counts 2..16
    let mut r = Rec::default();
    let c = ctl(EE_SCREENS_SEPL_TEXT_NUMBER, 0, 0, 0);
    r.run(|x| prestep(x, EE_SCREENS_SEPX_MAKE_MAX_PLAYER, &c));
    assert_eq!(r.names()[0], EE_SCREENS_SEPX_LINES_CLEAR);
    assert_eq!(r.texts.len(), 15);
    assert_eq!((r.texts[0].1.as_slice(), r.texts[14].1.as_slice()), (&b"2"[..], &b"16"[..]));
    // the families and the shown sets
    let mut r = Rec::default();
    r.answer(EE_SCREENS_SEPX_SETS_COUNT, &[2]);
    r.answer(EE_SCREENS_SEPX_SET_SHOWN, &[0, 1]);
    r.texts_out.insert((EE_SCREENS_SEPX_SET_NAME, 1), b"Ships".to_vec());
    let c = ctl(0, 0, 0, crate::screens::editor_object::EE_SCREENS_SEPL_COMBO_LINES_FAMILIES);
    r.run(|x| prestep(x, EE_SCREENS_SEPX_MAKE_SHELL_COMBO, &c));
    let families = crate::screens::editor_object::EE_SCREENS_SEOB_FAMILIES as usize;
    assert_eq!(r.texts.len(), families + 1);
    let set = resource_string(EE_SCREENS_SEPX_IDS_SET).unwrap();
    assert_eq!(
        r.texts[families].1,
        crate::screens::editor_selt::c_format(set, &[crate::screens::editor_selt::EeScreensSeltArg::Text(b"Ships")])
    );
    // the pending lines taken
    let mut r = Rec::default();
    let c = ctl(0, 0, 0, crate::screens::editor_map::EE_SCREENS_SEPL_COMBO_LINES_PENDING);
    r.run(|x| prestep(x, EE_SCREENS_SEPX_MAKE_PLAIN_COMBO, &c));
    assert_eq!(
        r.names(),
        [EE_SCREENS_SEPX_LINES_CLEAR, EE_SCREENS_SEPX_LINES_TAKE_PENDING, EE_SCREENS_SEPX_PENDING_CLEAR]
    );
    // the record's strings
    let mut r = Rec::default();
    let c =
        ctl(0, EE_SCREENS_SEPX_IDS_NONE as u32, 3, crate::screens::editor_object::EE_SCREENS_SEPL_COMBO_LINES_STRINGS);
    r.run(|x| prestep(x, EE_SCREENS_SEPX_MAKE_TRIGGER_COMBO, &c));
    assert_eq!(r.texts.len(), 3);
}

#[test]
fn the_brush_group_is_built_in_the_screen_font() {
    let mut g = EeScreensSeBrushGroup {
        bottom: 0.0,
        left: 0.0,
        right: 0.0,
        top: 0.0,
        buttons: [0; 5],
        title: 0,
        brush_size: 9,
        highlight_shown: true,
        centered: true,
    };
    let mut r = Rec::default();
    r.answer(EE_SCREENS_SEPX_BRUSH_SIZE, &[2]);
    let mut index: c_ulong = 5;
    r.run(|x| {
        x(EE_SCREENS_SEPX_SCREEN_FONT, [0; 3], &mut [0; 4], &mut [0.0; 4]);
        brush_group_new(
            x,
            &mut g,
            77,
            |next| {
                *next += 6;
                [10, 11, 12, 13, 14, 15]
            },
            &mut index,
        )
    });
    assert_eq!(index, 11);
    assert_eq!(
        (g.title, g.buttons, g.brush_size, g.highlight_shown, g.centered),
        (10, [11, 12, 13, 14, 15], 2, false, false)
    );
    let n: Vec<_> = r.ops.iter().map(|o| (o.0, o.1)).collect();
    assert_eq!(
        n[1..],
        [
            (EE_SCREENS_SEPX_SCREEN_FONT, 0),
            (EE_SCREENS_SEPX_SET_SCREEN_FONT, 77),
            (EE_SCREENS_SEPX_SET_SCREEN_FONT, 0),
            (EE_SCREENS_SEPX_BRUSH_SIZE, 11)
        ]
    );
    brush_group_highlight(&mut g, 3.0, 4.0, true);
    let r = crate::screens::editor_brush::highlight(3.0, 4.0, 2, true);
    assert_eq!([g.left, g.top, g.right, g.bottom], r);
    assert!(g.centered);
}

#[test]
fn the_brush_group_deletes_its_buttons_then_its_title() {
    let g = EeScreensSeBrushGroup {
        bottom: 0.0,
        left: 0.0,
        right: 0.0,
        top: 0.0,
        buttons: [11, 12, 13, 14, 15],
        title: 10,
        brush_size: 0,
        highlight_shown: false,
        centered: false,
    };
    let mut deleted = Vec::new();
    brush_group_free(
        &mut |op: u32, p: [usize; 3], _: &mut [i32; 4], _: &mut [f32; 4]| {
            assert_eq!(op, EE_SCREENS_SEPL_OP_DELETE);
            deleted.push(p[0]);
            0
        },
        &g,
    );
    assert_eq!(deleted, [11, 12, 13, 14, 15, 10]);
}
