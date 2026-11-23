use super::*;
use std::cell::RefCell;

/// One recorded statement: which host (`'x'` the form's, `'y'` the file's), its op, pointers, numbers and floats.
type Entry = (char, u32, [usize; 3], [i32; 4], [f32; 4]);

/// Runs `f` over two recording statement hosts sharing one log; made objects answer 0x1000 + their place in the
/// log, the reads `answer(host, op, i)` (`MAKE` for a making statement).
fn run<R>(
    answer: impl Fn(char, u32, &[i32; 4]) -> i32,
    f: impl FnOnce(&mut Sefmx, &mut Sefmx) -> R,
) -> (R, Vec<Entry>) {
    let log = RefCell::new(Vec::<Entry>::new());
    let step = |h: char, op: u32, p: [usize; 3], i: &mut [i32; 4], fl: &mut [f32; 4]| {
        log.borrow_mut().push((h, op, p, *i, *fl));
        let n = 0x1000 + log.borrow().len();
        let r = answer(h, op, i);
        if r == MAKE {
            i[0] = n as i32;
            i[1] = 0;
            return 0;
        }
        r
    };
    let mut x = |op: u32, p: [usize; 3], i: &mut [i32; 4], fl: &mut [f32; 4]| step('x', op, p, i, fl);
    let mut y = |op: u32, p: [usize; 3], i: &mut [i32; 4], fl: &mut [f32; 4]| step('y', op, p, i, fl);
    let r = f(&mut x, &mut y);
    (r, log.into_inner())
}

const MAKE: i32 = -77;

fn ops(log: &[Entry]) -> Vec<(char, u32)> {
    log.iter().map(|e| (e.0, e.1)).collect()
}

#[test]
fn the_map_tabs_terrain_and_lines() {
    use crate::screens::editor_map::*;
    // a table outside the editor: no more reads
    let ((r, i), log) = run(
        |_, op, _| (op == EE_SCREENS_SEMPX_TERRAIN_EXISTS) as i32,
        |x, y| {
            let mut i = [4, 0, 0, 0];
            (unsafe { semp(x, y, EE_SCREENS_SEMP_OP_TERRAIN, [0; 3], &mut i, &[0.0; 4]) }, i)
        },
    );
    assert_eq!((r, i[1], log.len()), (Some(0), 0, 2));
    let ((r, i), _) = run(
        |_, op, _| match op {
            EE_SCREENS_SEMPX_TERRAIN_ID => 11,
            EE_SCREENS_SEMPX_TERRAIN_NAME => 22,
            _ => 1,
        },
        |x, y| {
            let mut i = [4, 0, 0, 0];
            (unsafe { semp(x, y, EE_SCREENS_SEMP_OP_TERRAIN, [0; 3], &mut i, &[0.0; 4]) }, i)
        },
    );
    assert_eq!((r, i[1], i[2]), (Some(1), 11, 22));
    // three map sizes: each name read then pushed; the count asked again each time
    let (_, log) = run(
        |_, op, _| if op == EE_SCREENS_SEMPX_MAP_SIZES { 3 } else { 0 },
        |x, y| unsafe { semp(x, y, EE_SCREENS_SEMP_OP_LINES_MAP_SIZES, [0; 3], &mut [0; 4], &[0.0; 4]) },
    );
    assert_eq!(log.len(), 4 + 3 * 2);
    assert_eq!((log[1].1, log[1].3[0]), (EE_SCREENS_SEMPX_MAP_SIZE_NAME, 0));
    assert_eq!(log[4].3[0], 1);
}

#[test]
fn the_map_seed_and_radio() {
    use crate::screens::editor_map::*;
    let (r, log) = run(
        |_, op, _| match op {
            EE_SCREENS_SEMPX_RANDOM_NEW => MAKE,
            EE_SCREENS_SEMPX_RANDOM_DRAW => 4242,
            EE_SCREENS_SEMPX_SEED => 4242,
            _ => 0,
        },
        |x, y| unsafe { semp(x, y, EE_SCREENS_SEMP_OP_SEED_DRAW, [0; 3], &mut [0; 4], &[0.0; 4]) },
    );
    assert_eq!(r, Some(4242));
    assert_eq!(
        ops(&log),
        vec![
            ('y', EE_SCREENS_SEMPX_RANDOM_NEW),
            ('y', EE_SCREENS_SEMPX_RANDOM_SEED),
            ('y', EE_SCREENS_SEMPX_RANDOM_DRAW),
            ('y', EE_SCREENS_SEMPX_SEED_SET),
            ('y', EE_SCREENS_SEMPX_SEED),
            ('y', EE_SCREENS_SEMPX_RANDOM_DELETE),
        ]
    );
    assert_eq!(log[3].3[0], 4242);
    let mut out = 0usize;
    let (_, log) = run(
        |_, op, _| match op {
            EE_SCREENS_SEMPX_NEW_RADIO | EE_SCREENS_SEMPX_RADIO_LABEL | EE_SCREENS_SEFMX_LABEL_GETEXT => MAKE,
            _ => 0,
        },
        |x, y| unsafe {
            let p = [&mut out as *mut usize as usize, 0x50, 0];
            semp(x, y, EE_SCREENS_SEMP_OP_MAKE_RADIO, p, &mut [7, 99, 3, 1], &[1.0, 2.0, 30.0, 0.0])
        },
    );
    // the string first; the label's text transparent before its colour (`SempColor`)
    assert_eq!(log[0].1, EE_SCREENS_SEFMX_STRING);
    assert_eq!(log[0].3[0], 99);
    assert_eq!(log[5].1, EE_SCREENS_SEFMX_TEXT_TRANSPARENT);
    assert_eq!(log[6].1, EE_SCREENS_SEFMX_TEXT_FG);
    assert_eq!(log[7].3[0], 1);
    assert_eq!(out, 0x1002);
}

#[test]
fn the_elevation_slider_and_highlight() {
    use crate::screens::editor_elevation::*;
    let mut out = 0usize;
    let (r, log) = run(
        |_, op, _| match op {
            EE_SCREENS_SEELX_NEW_ELEVATION_SLIDER => MAKE,
            EE_SCREENS_SEELX_SLIDER_MAX => 0x40,
            EE_SCREENS_SEELX_SLIDER_VALUE => 0x28,
            _ => 0,
        },
        |x, y| unsafe {
            let p = [&mut out as *mut usize as usize, 0x50, 0];
            seel(x, y, EE_SCREENS_SEEL_OP_ELEVATION_SLIDER, p, &[5, 0, 0, 0], &mut [1.0, 2.0, 100.0, 0.0])
        },
    );
    assert_eq!(r, 0x40 - 0x28 - 0x18);
    assert_eq!(log[3].3[0], 0x40 - 0x18);
    assert_eq!(out, 0x1001);
    // outside the view: hidden only
    let (_, log) = run(
        |_, _, _| 0,
        |x, y| unsafe { seel(x, y, EE_SCREENS_SEEL_OP_HIGHLIGHT, [0x50, 0, 0], &[0; 4], &mut [3.0, 4.0, 0.0, 0.0]) },
    );
    assert_eq!(ops(&log), vec![('y', EE_SCREENS_SEELX_VIEW_HOLDS), ('y', EE_SCREENS_SEELX_HIGHLIGHT_SHOWN)]);
    // the tile's corner, as floats
    let (_, log) = run(
        |_, op, _| match op {
            EE_SCREENS_SEELX_VIEW_HOLDS => 1,
            EE_SCREENS_SEELX_TILE_X => 12,
            EE_SCREENS_SEELX_TILE_Y => -3,
            _ => 0,
        },
        |x, y| unsafe { seel(x, y, EE_SCREENS_SEEL_OP_HIGHLIGHT, [0x50, 0, 0], &[0; 4], &mut [3.0, 4.0, 0.0, 0.0]) },
    );
    let last = log.last().unwrap();
    assert_eq!((last.1, last.4[0], last.4[1], last.3[0]), (EE_SCREENS_SEELX_SET_HIGHLIGHT, 12.0, -3.0, 0));
}

#[test]
fn the_upgrade_panel_counts_its_controls() {
    let (mut id, mut index): (c_ulong, c_ulong) = (100, 10);
    let mut panel = SeupPanel::default();
    let (_, log) = run(
        |_, op, _| match op {
            EE_SCREENS_SEUPX_PORTRAIT_RIGHT => 0,
            EE_SCREENS_SEFMX_LABEL_GETEXT | EE_SCREENS_SEUPX_NEW_LAYOUT | EE_SCREENS_SEUPX_NEW_BOX => MAKE,
            o if o == EE_SCREENS_SEUPX_NEW_PORTRAIT
                || o == EE_SCREENS_SEUPX_NEW_NAME_LABEL
                || o == EE_SCREENS_SEUPX_NEW_BAR
                || o == EE_SCREENS_SEUPX_NEW_ATTRIBUTE
                || o == EE_SCREENS_SEUPX_NEW_ROW_LABEL
                || o == EE_SCREENS_SEUPX_NEW_ARMOR_LABEL =>
            {
                MAKE
            }
            _ => 0,
        },
        |x, y| unsafe { upgrade_panel(x, y, &mut panel, &mut id, &mut index, 0x50) },
    );
    // the portrait, the name, two bars, nine attribute rows and their labels, the armor title
    assert_eq!((id, index), (123, 33));
    assert_eq!(log.iter().filter(|e| e.1 == EE_SCREENS_SEFMX_HIDE && e.0 == 'x').count(), 23);
    // the box first; the portrait at (6, 619) .. (36, 649) with the first id, wired into the box
    assert_eq!((log[0].1, panel.status_box), (EE_SCREENS_SEUPX_NEW_BOX, 0x1001));
    let portrait = log.iter().find(|e| e.1 == EE_SCREENS_SEUPX_NEW_PORTRAIT && e.0 == 'y').unwrap();
    assert_eq!((portrait.3[0], portrait.3[1], portrait.4), (100, 10, [6.0, 619.0, 36.0, 649.0]));
    let wired = log.iter().find(|e| e.1 == EE_SCREENS_SEUPX_BOX_PORTRAIT).unwrap();
    assert_eq!(wired.2, [panel.status_box, panel.portrait, 0]);
    // the sixth attribute row: the second column's first line, 48 + 19 wide; its row wires the box, icon and label
    let rows: Vec<_> = log.iter().filter(|e| e.1 == EE_SCREENS_SEUPX_NEW_ATTRIBUTE && e.0 == 'y').collect();
    assert_eq!(rows[5].4, [156.0, 654.0, 223.0, 671.0]);
    assert_eq!(rows[5].3[2], 5);
    let boxed: Vec<_> = log.iter().filter(|e| e.1 == EE_SCREENS_SEUPX_BOX_ROW).collect();
    assert_eq!((boxed[5].2, boxed[5].3[2]), ([panel.status_box, panel.attributes[5], panel.labels[5]], 5));
    // every control kept, distinct
    let mut all = vec![panel.portrait, panel.name, panel.left_bar, panel.middle_bar, panel.armor];
    all.extend(panel.attributes);
    all.extend(panel.labels);
    assert!(all.iter().all(|&h| h > 0x1000));
    all.sort_unstable();
    all.dedup();
    assert_eq!(all.len(), 23);
    // the layout freed last
    assert_eq!(log.last().unwrap().1, EE_SCREENS_SEUPX_LAYOUT_DELETE);
    let (_, log) = run(|_, _, _| 0, |_, y| upgrade_panel_free(y, &panel));
    assert_eq!(log.len(), 6 + 18);
    assert_eq!(
        log[..6].iter().map(|e| (e.1, e.2[0])).collect::<Vec<_>>(),
        [
            (EE_SCREENS_SEUPX_DELETE_BOX, panel.status_box),
            (EE_SCREENS_SEUPX_DELETE_PORTRAIT, panel.portrait),
            (EE_SCREENS_SEUPX_DELETE_LABEL, panel.armor),
            (EE_SCREENS_SEUPX_DELETE_BAR, panel.left_bar),
            (EE_SCREENS_SEUPX_DELETE_BAR, panel.middle_bar),
            (EE_SCREENS_SEUPX_DELETE_LABEL, panel.name),
        ]
    );
    assert_eq!((log[22].1, log[22].2[0]), (EE_SCREENS_SEUPX_DELETE_ATTRIBUTE, panel.attributes[8]));
    let (_, log) = run(|_, _, _| 0, |_, y| upgrade_panel_show(y, &panel, 0x77));
    assert_eq!((log[0].1, log[0].2), (EE_SCREENS_SEUPX_FILL, [panel.status_box, 0x77, 0]));
}

#[test]
fn the_editor_events() {
    use crate::screens::scenario_editor::*;
    let (r, log) = run(
        |_, op, _| (op == EE_SCREENS_SEX_DEBUG_IDS) as i32,
        |x, y| unsafe { se(x, y, 0x50, 0, EE_SCREENS_SE_OP_TOGGLE_DEBUG_IDS, 0, 0) },
    );
    assert_eq!(r, Some(0));
    assert_eq!(log[1].3[0], 0);
    // the test action posted and released
    let (_, log) = run(
        |_, op, _| if op == EE_SCREENS_SEX_NEW_TEST_ACTION { MAKE } else { 0 },
        |x, y| unsafe { se(x, y, 0x50, 0, EE_SCREENS_SE_OP_POST_ACTION, EE_SCREENS_SE_ACTION_TEST, 0) },
    );
    assert_eq!(
        ops(&log),
        vec![
            ('y', EE_SCREENS_SEX_NEW_TEST_ACTION),
            ('x', EE_SCREENS_SEFMX_POST_ACTION),
            ('x', EE_SCREENS_SEFMX_DEREF_ACTION)
        ]
    );
    assert_eq!(log[1].2[0], 0x1001);
    // no area screen: no centre
    let (r, _) = run(|_, _, _| 0, |x, y| unsafe { se(x, y, 0x50, 0, EE_SCREENS_SE_OP_AREA_CENTER, 0, 0) });
    assert_eq!(r, Some(0));
    // the home rotation: a fresh turn as the view, then the editor's turn as the offset
    let (_, log) = run(|_, _, _| 0, |x, y| unsafe { se(x, y, 0x50, 0, EE_SCREENS_SE_OP_HOME_ROTATION, 0, 0) });
    assert_eq!(
        ops(&log),
        vec![
            ('x', EE_SCREENS_SEFMX_TURN_RESET),
            ('x', EE_SCREENS_SEFMX_TURN_VIEW),
            ('x', EE_SCREENS_SEFMX_TURN_YPR),
            ('x', EE_SCREENS_SEFMX_CAMERA_OFFSET_TURN)
        ]
    );
    // an op left to the C++
    let (r, _) = run(|_, _, _| 0, |x, y| unsafe { se(x, y, 0x50, 0, EE_SCREENS_SE_OP_OPEN_CIV_BUILDER, 0, 0) });
    assert_eq!(r, None);
}

#[test]
fn the_form_events() {
    use crate::screens::editor_events::*;
    let (_, log) = run(
        |_, op, _| if op == EE_SCREENS_SEEX_EPOCH_COUNT { 5 } else { 0 },
        |x, y| unsafe { see(x, y, EE_SCREENS_SEE_OP_FILL_EPOCHS, [0x60, 0, 0], &[2, 0, 0, 0], &[0.0; 4]) },
    );
    let titles: Vec<i32> = log.iter().filter(|e| e.1 == EE_SCREENS_SEEX_EPOCH_TITLE).map(|e| e.3[0]).collect();
    assert_eq!(titles, vec![2, 3, 4]);
    assert_eq!(log.last().unwrap().1, EE_SCREENS_SEEX_COMBO_APPEND);
    let (_, log) = run(
        |_, _, _| 0,
        |x, y| unsafe { see(x, y, EE_SCREENS_SEE_OP_SLIDER_TEXT, [0x60, 0x61, 0], &[0; 4], &[2.5, 0.0, 0.0, 0.0]) },
    );
    assert_eq!(ops(&log), vec![('y', EE_SCREENS_SEEX_PRINT), ('y', EE_SCREENS_SEEX_LABEL_TEXT)]);
    assert_eq!((log[0].2[1], log[0].4[0], log[1].2[0]), (0x61, 2.5, 0x60));
    let (r, _) =
        run(|_, _, _| 0, |x, y| unsafe { see(x, y, EE_SCREENS_SEE_OP_MAX_PLAYERS, [0; 3], &[0; 4], &[0.0; 4]) });
    assert_eq!(r, None);
}
