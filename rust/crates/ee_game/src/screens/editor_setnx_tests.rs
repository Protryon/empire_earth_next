use super::*;

/// A recorded statement: the op, the three handles, the integers and floats it got.
type Line = (u32, [usize; 3], [i32; 4], [f32; 4]);

/// The ops whose p0 is an out slot.
const MAKERS: &[u32] = &[
    EE_SCREENS_SETNX_TT_GET,
    EE_SCREENS_SETNX_T_AT,
    EE_SCREENS_SETNX_PAINT_RADIO,
    EE_SCREENS_SETNX_RADIO_CONTROL,
    EE_SCREENS_SETNX_TYPE_LIST_NEW,
    EE_SCREENS_SETNX_TYPE_EVENT,
    EE_SCREENS_SETNX_TYPE_CONTROL,
    EE_SCREENS_SETNX_COLOR_LIST_NEW,
    EE_SCREENS_SETNX_COLOR_EVENT,
    EE_SCREENS_SETNX_COLOR_CONTROL,
    EE_SCREENS_SETNX_PAINT_NEW,
    EE_SCREENS_SETNX_NEW_BITMAP,
    EE_SCREENS_SETNX_UIBITMAP_NEW,
    EE_SCREENS_SETNX_UIBITMAP_CONTROL,
    EE_SCREENS_SETNX_LINE_TEXTURE,
    EE_SCREENS_SETNX_CLASS_BUTTON_TEXTURE,
    EE_SCREENS_SETNX_PAINT_TERRAIN,
    EE_SCREENS_SETNX_PAINT_BULLDOZER,
    EE_SCREENS_SETNX_BRUSH_RADIO,
];

/// The fake C++: records every statement, makes handles 0x100, 0x101, ..., answers `answer(op, i)`.
struct Fake<A: FnMut(u32, &[i32; 4]) -> i32> {
    lines: Vec<Line>,
    next: usize,
    answer: A,
}

impl<A: FnMut(u32, &[i32; 4]) -> i32> Fake<A> {
    fn new(answer: A) -> Self {
        Fake { lines: Vec::new(), next: 0x100, answer }
    }

    fn run(&mut self, op: u32, p: [usize; 3], i: &mut [i32; 4], f: &mut [f32; 4]) -> Option<i32> {
        let mut x = |op: u32, p: [usize; 3], i: &mut [i32; 4], f: &mut [f32; 4]| -> i32 {
            self.lines.push((op, p, *i, *f));
            if op == EE_SCREENS_SETNX_TYPES_SELECTION || op == EE_SCREENS_SETNX_COLORS_SELECTION {
                // SAFETY: p2 the composed op's selection
                unsafe { *(p[2] as *mut c_ulong) = 4 };
            }
            if MAKERS.contains(&op) {
                // SAFETY: p0 the composed op's out slot
                unsafe { *(p[0] as *mut usize) = self.next };
                self.next += 1;
                return 0;
            }
            (self.answer)(op, i)
        };
        // SAFETY: the out slots and arrays are the test's
        unsafe { compose(&mut x, op, p, i, f) }
    }

    fn ops(&self) -> Vec<u32> {
        self.lines.iter().map(|l| l.0).collect()
    }
}

#[test]
fn type_tables_in_the_editor_fill_the_callers_array() {
    // tables 0 .. 4, 1 and 3 in the editor; room for one
    let mut fake = Fake::new(|op, i| match op {
        EE_SCREENS_SETNX_TT_COUNT => 4,
        EE_SCREENS_SETNX_TT_IN_EDITOR => i[0] % 2,
        _ => 0,
    });
    let mut out = [0usize; 2];
    let r = fake.run(EE_SCREENS_SETN_OP_TABLES, [out.as_mut_ptr() as usize, 0, 0], &mut [0, 1, 0, 0], &mut [0.0; 4]);
    assert_eq!(r, Some(2));
    assert_eq!(out, [0x100, 0]);
    let gets: Vec<i32> = fake.lines.iter().filter(|l| l.0 == EE_SCREENS_SETNX_TT_GET).map(|l| l.2[0]).collect();
    assert_eq!(gets, vec![1]);
    // counted only (no array)
    let mut fake =
        Fake::new(|op, _| (op == EE_SCREENS_SETNX_TT_COUNT) as i32 * 3 + (op == EE_SCREENS_SETNX_TT_IN_EDITOR) as i32);
    assert_eq!(fake.run(EE_SCREENS_SETN_OP_TABLES, [0; 3], &mut [0, 9, 0, 0], &mut [0.0; 4]), Some(3));
}

#[test]
fn terrains_are_collected_sorted_then_copied() {
    let mut fake = Fake::new(|op, i| match op {
        EE_SCREENS_SETNX_T_COUNT => 3,
        EE_SCREENS_SETNX_T_IN_EDITOR => (i[0] != 1) as i32,
        EE_SCREENS_SETNX_T_SIZE => 2,
        _ => 0,
    });
    let mut out = [0usize; 4];
    let r = fake.run(EE_SCREENS_SETN_OP_TABLES, [out.as_mut_ptr() as usize, 0, 0], &mut [1, 4, 0, 0], &mut [0.0; 4]);
    assert_eq!(r, Some(2));
    assert_eq!(out[..3], [0x100, 0x101, 0]);
    assert_eq!(
        fake.ops(),
        vec![
            EE_SCREENS_SETNX_T_CLEAR,
            EE_SCREENS_SETNX_T_COUNT,
            EE_SCREENS_SETNX_T_IN_EDITOR,
            EE_SCREENS_SETNX_T_PUSH,
            EE_SCREENS_SETNX_T_IN_EDITOR,
            EE_SCREENS_SETNX_T_IN_EDITOR,
            EE_SCREENS_SETNX_T_PUSH,
            EE_SCREENS_SETNX_T_SORT,
            EE_SCREENS_SETNX_T_SIZE,
            EE_SCREENS_SETNX_T_AT,
            EE_SCREENS_SETNX_T_AT,
        ]
    );
    // no array: the size, unsorted
    let mut fake = Fake::new(|op, _| (op == EE_SCREENS_SETNX_T_SIZE) as i32 * 5);
    assert_eq!(fake.run(EE_SCREENS_SETN_OP_TABLES, [0; 3], &mut [1, 4, 0, 0], &mut [0.0; 4]), Some(5));
    assert!(!fake.ops().contains(&EE_SCREENS_SETNX_T_SORT));
}

#[test]
fn a_terrains_values_and_its_preview() {
    let mut fake = Fake::new(|op, _| match op {
        EE_SCREENS_SETNX_T_ID => 7,
        EE_SCREENS_SETNX_T_COLOR_TYPE => 2,
        EE_SCREENS_SETNX_T_AMBIENTS => 1,
        EE_SCREENS_SETNX_T_CLASS_COUNT => 3,
        EE_SCREENS_SETNX_T_CLASS => 44,
        _ => 0,
    });
    let mut i = [0; 4];
    assert_eq!(fake.run(EE_SCREENS_SETN_OP_TERRAIN, [0x30, 0x31, 0], &mut i, &mut [0.0; 4]), Some(44));
    assert_eq!(i, [7, 2, 1, 3]);
    assert_eq!(fake.lines[4].1, [0, 0x30, 0x31]);
    let mut fake = Fake::new(|_, _| 0);
    assert_eq!(fake.run(EE_SCREENS_SETN_OP_TERRAIN, [0x30, 0, 0], &mut [0; 4], &mut [0.0; 4]), Some(-1));
    assert!(!fake.ops().contains(&EE_SCREENS_SETNX_T_PREVIEW));
}

#[test]
fn the_type_list_is_made_from_its_lines() {
    let terrains = [0x501usize, 0x502];
    let sets = [3i32, 5];
    let q = [0x77usize, terrains.as_ptr() as usize, sets.as_ptr() as usize];
    let mut out = 0usize;
    let mut fake = Fake::new(|_, _| 0);
    fake.run(
        EE_SCREENS_SETN_OP_TYPE_LIST,
        [&mut out as *mut usize as usize, 0x40, q.as_ptr() as usize],
        &mut [2, 11, 0, 0],
        &mut [5.0, 6.0, 120.0, 0.0],
    );
    assert_eq!(
        fake.ops(),
        vec![
            EE_SCREENS_SETNX_LINES_NEW,
            EE_SCREENS_SETNX_LINE_NAME,
            EE_SCREENS_SETNX_LINE_COLOR_SET,
            EE_SCREENS_SETNX_LINE_NAME,
            EE_SCREENS_SETNX_LINE_COLOR_SET,
            EE_SCREENS_SETNX_TYPE_LIST_NEW,
            EE_SCREENS_SETNX_TYPE_EVENT,
            EE_SCREENS_SETNX_TYPE_TRIGGER,
            EE_SCREENS_SETNX_EVENT_RELEASE,
            EE_SCREENS_SETNX_TYPE_TEXT_TRANSPARENT,
            EE_SCREENS_SETNX_TYPE_TEXT_YELLOW,
            EE_SCREENS_SETNX_TYPE_SELECTED_CLEAR,
            EE_SCREENS_SETNX_TYPE_BG,
            EE_SCREENS_SETNX_TYPE_ADD,
            EE_SCREENS_SETNX_TYPE_CONTROL,
            EE_SCREENS_SETNX_LINES_CLEAR,
        ]
    );
    let l = &fake.lines;
    assert_eq!((l[1].1[1], l[1].2[0]), (terrains.as_ptr() as usize, 0));
    assert_eq!(l[4].2[..2], [1, 5]);
    assert_eq!((l[5].1[1], l[5].2[0], l[5].3), (0x40, 11, [5.0, 6.0, 120.0, 0.0]));
    assert_eq!(l[6].1[1], 0x77);
    // the list 0x100, its event 0x101, the control 0x102
    assert_eq!((l[7].1[1], l[7].1[2]), (0x100, 0x101));
    assert_eq!((l[13].1[1], l[13].1[2]), (0x40, 0x100));
    assert_eq!(out, 0x102);
}

#[test]
fn the_colour_list_pushes_each_types_colours() {
    let tables = [0x601usize, 0x602];
    let q = [0x77usize, tables.as_ptr() as usize, 0];
    let mut out = 0usize;
    let mut fake = Fake::new(|op, i| match op {
        EE_SCREENS_SETNX_TYPE_FIRST_COLOR => [4, 9][i[0] as usize],
        EE_SCREENS_SETNX_TYPE_LAST_COLOR => [6, 9][i[0] as usize],
        _ => 0,
    });
    fake.run(
        EE_SCREENS_SETN_OP_COLOR_LIST,
        [&mut out as *mut usize as usize, 0x40, q.as_ptr() as usize],
        &mut [2, 11, 3, 0],
        &mut [0.0; 4],
    );
    let pushes: Vec<[i32; 2]> =
        fake.lines.iter().filter(|l| l.0 == EE_SCREENS_SETNX_COLORS_PUSH).map(|l| [l.2[0], l.2[1]]).collect();
    assert_eq!(pushes, vec![[0, 4], [0, 5], [0, 6], [1, 9]]);
    let select = fake.lines.iter().find(|l| l.0 == EE_SCREENS_SETNX_COLOR_SELECT).unwrap();
    assert_eq!((select.1[1], select.2[0]), (0x100, 3));
    assert_eq!(fake.ops().last(), Some(&EE_SCREENS_SETNX_COLORS_CLEAR));
    assert_eq!(out, 0x102);
}

#[test]
fn the_radios_selection_preview_and_paint_action() {
    let mut out = 0usize;
    let mut fake = Fake::new(|_, _| 0);
    fake.run(
        EE_SCREENS_SETN_OP_RADIO,
        [&mut out as *mut usize as usize, 0x40, 0],
        &mut [200, 20508, 9, 1],
        &mut [1.0, 2.0, 0.0, 0.0],
    );
    assert_eq!(
        fake.ops(),
        vec![
            EE_SCREENS_SETNX_STRING,
            EE_SCREENS_SETNX_PAINT_RADIO,
            EE_SCREENS_SETNX_LABEL_BG,
            EE_SCREENS_SETNX_LABEL_TRANSPARENT,
            EE_SCREENS_SETNX_LABEL_YELLOW,
            EE_SCREENS_SETNX_RADIO_CONTROL,
        ]
    );
    assert_eq!(fake.lines[0].2[0], 20508);
    assert_eq!(out, 0x101);
    let mut fake = Fake::new(|_, _| 0);
    fake.run(
        EE_SCREENS_SETN_OP_BRUSH,
        [&mut out as *mut usize as usize, 0x40, 0x41],
        &mut [0, 2, 20501, 9],
        &mut [0.0; 4],
    );
    assert!(!fake.ops().contains(&EE_SCREENS_SETNX_LABEL_BG));
    assert_eq!((fake.lines[0].2[0], fake.lines[1].1[2]), (20501, 0x41));
    // the selection: its value, or -1 on an error
    let mut fake = Fake::new(|_, _| 0);
    assert_eq!(fake.run(EE_SCREENS_SETN_OP_SELECTION, [0x50, 0, 0], &mut [1, 0, 0, 0], &mut [0.0; 4]), Some(4));
    assert_eq!(fake.ops(), vec![EE_SCREENS_SETNX_COLORS_SELECTION]);
    let mut fake = Fake::new(|_, _| 5);
    assert_eq!(fake.run(EE_SCREENS_SETN_OP_SELECTION, [0x50, 0, 0], &mut [0; 4], &mut [0.0; 4]), Some(-1));
    // the preview: the line's texture stretched over its whole size
    let mut fake = Fake::new(|op, _| match op {
        EE_SCREENS_SETNX_TEXTURE_WIDTH => 64,
        EE_SCREENS_SETNX_TEXTURE_HEIGHT => 32,
        _ => 0,
    });
    fake.run(EE_SCREENS_SETN_OP_SHOW_PREVIEW, [0x50, 0x51, 0], &mut [2, 0, 0, 0], &mut [0.0; 4]);
    assert_eq!(fake.lines[1].1, [0x50, 0x51, 0x100]);
    assert_eq!(fake.lines[4].3, [0.0, 0.0, 64.0, 32.0]);
    // the paint action: centred unless i3 < 0; terrain, bulldozer, none
    let mut action = 1usize;
    let mut fake = Fake::new(|_, _| 0);
    fake.run(
        EE_SCREENS_SETN_OP_PAINT_ACTION,
        [0x52, &mut action as *mut usize as usize, 0],
        &mut [0, 3, 4, 1],
        &mut [0.0; 4],
    );
    assert_eq!(fake.ops(), vec![EE_SCREENS_SETNX_PAINT_CENTERED, EE_SCREENS_SETNX_PAINT_TERRAIN]);
    assert_eq!(fake.lines[1].2[..2], [3, 4]);
    assert_eq!(action, 0x100);
    let mut fake = Fake::new(|_, _| 0);
    fake.run(
        EE_SCREENS_SETN_OP_PAINT_ACTION,
        [0x52, &mut action as *mut usize as usize, 0],
        &mut [2, 0, 0, -1],
        &mut [0.0; 4],
    );
    assert!(fake.lines.is_empty());
    assert_eq!(action, 0);
    // the class button: none without a class
    let mut fake = Fake::new(|_, _| 0);
    assert_eq!(fake.run(EE_SCREENS_SETN_OP_CLASS_BUTTON, [0; 3], &mut [5, 0, 0, 0], &mut [0.0; 4]), Some(0));
    let mut fake = Fake::new(|_, _| 0);
    assert_eq!(fake.run(EE_SCREENS_SETN_OP_FIRE, [0; 3], &mut [0; 4], &mut [0.0; 4]), None);
}

/// The list members' fake: a recorder answering `answer(op, i)`, the copy text rectangle (left 10, top 100, right
/// 109, bottom 147) and the selected line's rectangle (10, 116 .. 109, 131) written into p2.
fn list_run(which: u32, answer: impl Fn(u32, &[i32; 4]) -> i32) -> (i32, Vec<Line>) {
    let mut lines: Vec<Line> = Vec::new();
    let mut next = 0x100usize;
    let r = {
        let mut x = |op: u32, p: [usize; 3], i: &mut [i32; 4], f: &mut [f32; 4]| -> i32 {
            lines.push((op, p, *i, *f));
            match op {
                EE_SCREENS_SETNX_C_COPY_RECT | EE_SCREENS_SETNX_T_COPY_RECT | EE_SCREENS_SETNX_T_SELECTED_RECT => {
                    let r = if op == EE_SCREENS_SETNX_T_SELECTED_RECT {
                        GeRectangle { left: 10.0, top: 116.0, right: 109.0, bottom: 131.0 }
                    } else {
                        GeRectangle { left: 10.0, top: 100.0, right: 109.0, bottom: 147.0 }
                    };
                    // SAFETY: p2 the composed op's rectangle
                    unsafe { *(p[2] as *mut GeRectangle) = r };
                    0
                }
                EE_SCREENS_SETNX_RHOMBUS_NEW | EE_SCREENS_SETNX_LINE_NEW | EE_SCREENS_SETNX_T_COPY_TEXTURE => {
                    let h = if op == EE_SCREENS_SETNX_T_COPY_TEXTURE && answer(op, i) == 0 { 0 } else { next };
                    next += 1;
                    // SAFETY: p0 the composed op's out slot
                    unsafe { *(p[0] as *mut usize) = h };
                    0
                }
                _ => answer(op, i),
            }
        };
        compose_list(&mut x, which, 0x50)
    };
    (r, lines)
}

#[test]
fn colour_swatches_split_the_line_and_stop_at_the_edge() {
    // two lines of 3 and 4 colours, 16 high
    let (r, lines) = list_run(EE_SCREENS_SETLB_COLOR_DRAW, |op, i| match op {
        EE_SCREENS_SETNX_C_COPY_COUNT => 2,
        EE_SCREENS_SETNX_C_LINE_HEIGHT => 16,
        EE_SCREENS_SETNX_C_LINE_SIZE => [3, 4][i[0] as usize],
        EE_SCREENS_SETNX_C_BASE_DRAW => 9,
        _ => 0,
    });
    assert_eq!(r, 9);
    let colors: Vec<[i32; 2]> =
        lines.iter().filter(|l| l.0 == EE_SCREENS_SETNX_C_RHOMBUS_COLOR).map(|l| [l.2[0], l.2[1]]).collect();
    // width 99/3 = 33 and 99/4 = 24: every swatch starts inside the copy rectangle
    assert_eq!(colors, vec![[0, 0], [0, 1], [0, 2], [1, 0], [1, 1], [1, 2], [1, 3]]);
    assert_eq!(lines[0].0, EE_SCREENS_SETNX_RHOMBUS_NEW);
    let tail: Vec<u32> = lines.iter().rev().take(2).map(|l| l.0).collect();
    assert_eq!(tail, vec![EE_SCREENS_SETNX_RHOMBUS_DELETE, EE_SCREENS_SETNX_C_BASE_DRAW]);
    // a line past the rectangle's bottom (top 100 + 3 x 16 = 148 > 147) draws nothing
    let (_, lines) = list_run(EE_SCREENS_SETLB_COLOR_DRAW, |op, _| match op {
        EE_SCREENS_SETNX_C_COPY_COUNT => 4,
        EE_SCREENS_SETNX_C_LINE_HEIGHT => 16,
        EE_SCREENS_SETNX_C_LINE_SIZE => 1,
        _ => 0,
    });
    let drawn: Vec<i32> = lines.iter().filter(|l| l.0 == EE_SCREENS_SETNX_C_RHOMBUS_COLOR).map(|l| l.2[0]).collect();
    assert_eq!(drawn, vec![0, 1, 2]);
}

#[test]
fn type_copy_moves_the_texture_references() {
    let (r, lines) = list_run(EE_SCREENS_SETLB_TYPE_COPY, |op, i| match op {
        EE_SCREENS_SETNX_T_COPY_COUNT => 2,
        EE_SCREENS_SETNX_T_COPY_HAS_TEXTURE => (i[0] == 1) as i32,
        EE_SCREENS_SETNX_T_HAS_SELECTION => 1,
        EE_SCREENS_SETNX_T_BASE_COPY => 4,
        _ => 0,
    });
    assert_eq!(r, 4);
    let ops: Vec<u32> = lines.iter().map(|l| l.0).collect();
    assert_eq!(
        ops,
        vec![
            EE_SCREENS_SETNX_T_COPY_COUNT,
            EE_SCREENS_SETNX_T_COPY_HAS_TEXTURE,
            EE_SCREENS_SETNX_T_COPY_HAS_TEXTURE,
            EE_SCREENS_SETNX_T_COPY_RELEASE,
            EE_SCREENS_SETNX_T_COPY_CLEAR,
            EE_SCREENS_SETNX_T_BASE_COPY,
            EE_SCREENS_SETNX_T_COPY_TEXT,
            EE_SCREENS_SETNX_T_HAS_SELECTION,
            EE_SCREENS_SETNX_T_FOCUS_OFF,
            EE_SCREENS_SETNX_T_SET_FRAMED,
            EE_SCREENS_SETNX_T_FOCUS_COLOR,
            EE_SCREENS_SETNX_T_COPY_COUNT,
            EE_SCREENS_SETNX_T_COPY_HAS_TEXTURE,
            EE_SCREENS_SETNX_T_COPY_HAS_TEXTURE,
            EE_SCREENS_SETNX_T_COPY_REFERENCE,
        ]
    );
    assert_eq!((lines[9].2[0], lines[14].2[0]), (1, 1));
}

#[test]
fn type_draw_places_the_previews_and_frames_the_selection() {
    // three lines, 16 high, line 1 without a texture; framed
    let (r, lines) = list_run(EE_SCREENS_SETLB_TYPE_DRAW, |op, i| match op {
        EE_SCREENS_SETNX_T_COPY_COUNT => 3,
        EE_SCREENS_SETNX_T_COPY_TEXTURE => (i[0] != 1) as i32,
        EE_SCREENS_SETNX_T_LINE_HEIGHT => 16,
        EE_SCREENS_SETNX_TEXTURE_WIDTH => 32,
        EE_SCREENS_SETNX_TEXTURE_HEIGHT => 32,
        EE_SCREENS_SETNX_T_FRAMED => 1,
        EE_SCREENS_SETNX_T_DRAW_LINE => 6,
        EE_SCREENS_SETNX_T_DRAW_BITMAP => 5,
        _ => 0,
    });
    assert_eq!(r, 6);
    let stretches: Vec<[f32; 4]> = lines.iter().filter(|l| l.0 == EE_SCREENS_SETNX_T_STRETCH).map(|l| l.3).collect();
    assert_eq!(stretches, vec![[0.0, 0.0, 32.0, 32.0]; 2]);
    let points: Vec<(u32, [f32; 2])> = lines
        .iter()
        .filter(|l| l.0 == EE_SCREENS_SETNX_LINE_START || l.0 == EE_SCREENS_SETNX_LINE_END)
        .map(|l| (l.0, [l.3[0], l.3[1]]))
        .collect();
    assert_eq!(
        points,
        vec![
            (EE_SCREENS_SETNX_LINE_START, [10.0, 116.0]),
            (EE_SCREENS_SETNX_LINE_END, [109.0, 116.0]),
            (EE_SCREENS_SETNX_LINE_END, [10.0, 131.0]),
            (EE_SCREENS_SETNX_LINE_START, [109.0, 131.0]),
            (EE_SCREENS_SETNX_LINE_END, [109.0, 116.0]),
        ]
    );
    assert_eq!(lines.iter().filter(|l| l.0 == EE_SCREENS_SETNX_T_DRAW_LINE).count(), 4);
    assert_eq!((lines[0].0, lines[1].0), (EE_SCREENS_SETNX_LINE_NEW, EE_SCREENS_SETNX_T_BASE_DRAW));
    assert_eq!(lines.last().unwrap().0, EE_SCREENS_SETNX_LINE_DELETE);
}

#[test]
fn type_text_draws_each_name_a_line_lower() {
    let (r, lines) = list_run(EE_SCREENS_SETLB_TYPE_TEXT, |op, _| match op {
        EE_SCREENS_SETNX_T_LINE_HEIGHT => 16,
        EE_SCREENS_SETNX_T_DRAW_COUNT => 2,
        EE_SCREENS_SETNX_T_DRAW_TEXT => 3,
        _ => 0,
    });
    assert_eq!(r, 3);
    let at: Vec<[f32; 2]> =
        lines.iter().filter(|l| l.0 == EE_SCREENS_SETNX_T_TEXT_AT).map(|l| [l.3[0], l.3[1]]).collect();
    assert_eq!(at, vec![[26.0, 100.0], [26.0, 116.0]]);
    let first: Vec<u32> = lines.iter().take(3).map(|l| l.0).collect();
    assert_eq!(
        first,
        vec![EE_SCREENS_SETNX_T_LINE_HEIGHT, EE_SCREENS_SETNX_T_SCROLL_TEXT, EE_SCREENS_SETNX_T_CONTROL_TEXT]
    );
    // the colour list's copy: the base's result, then the copied focus
    let (r, lines) = list_run(EE_SCREENS_SETLB_COLOR_COPY, |op, _| (op == EE_SCREENS_SETNX_C_BASE_COPY) as i32 * 8);
    assert_eq!((r, lines.len(), lines[1].0), (8, 2, EE_SCREENS_SETNX_C_FOCUS_ON));
}

#[test]
fn set_texture_moves_the_reference() {
    let mut fake = Fake::new(|op, _| (op == EE_SCREENS_SETNX_T_DATA_HAS_TEXTURE) as i32);
    fake.run(EE_SCREENS_SETN_OP_SET_TEXTURE, [0x50, 0x60, 0], &mut [2, 0, 0, 0], &mut [0.0; 4]);
    assert_eq!(
        fake.ops(),
        vec![
            EE_SCREENS_SETNX_T_DATA_HAS_TEXTURE,
            EE_SCREENS_SETNX_T_DATA_RELEASE,
            EE_SCREENS_SETNX_T_DATA_SET,
            EE_SCREENS_SETNX_T_DATA_REFERENCE,
        ]
    );
    assert_eq!((fake.lines[2].1[2], fake.lines[2].2[0]), (0x60, 2));
    let mut fake = Fake::new(|_, _| 0);
    fake.run(EE_SCREENS_SETN_OP_SET_TEXTURE, [0x50, 0, 0], &mut [2, 0, 0, 0], &mut [0.0; 4]);
    assert_eq!(fake.ops(), vec![EE_SCREENS_SETNX_T_DATA_HAS_TEXTURE, EE_SCREENS_SETNX_T_DATA_SET]);
}
