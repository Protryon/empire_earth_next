use super::*;

/// A recording statement host: every statement's op, pointers and first numbers; made objects answer 0x1000 + the
/// statement's ordinal, the reads `answer(op)`.
/// One recorded statement: op, pointers, numbers, floats.
type Entry = (u32, [usize; 3], [i32; 4], [f32; 4]);

struct Rec {
    log: Vec<Entry>,
    answer: fn(u32, &[i32; 4]) -> i32,
}

impl Rec {
    fn new(answer: fn(u32, &[i32; 4]) -> i32) -> Rec {
        Rec { log: Vec::new(), answer }
    }
    fn run<R>(&mut self, f: impl FnOnce(&mut Sefmx) -> R) -> R {
        let answer = self.answer;
        let log = &mut self.log;
        let mut x = |op: u32, p: [usize; 3], i: &mut [i32; 4], fl: &mut [f32; 4]| {
            log.push((op, p, *i, *fl));
            let n = 0x1000 + log.len();
            let r = answer(op, i);
            if r == MAKE {
                i[0] = n as i32;
                i[1] = 0;
                return 0;
            }
            if op == EE_SCREENS_SEFMX_VIEW_RIGHT {
                fl[0] = 1023.0;
            }
            r
        };
        f(&mut x)
    }
    fn ops(&self) -> Vec<u32> {
        self.log.iter().map(|e| e.0).collect()
    }
}

/// `answer`'s mark for a making statement.
const MAKE: i32 = -77;

fn makes(op: u32, _: &[i32; 4]) -> i32 {
    match op {
        EE_SCREENS_SEFMX_BUTTON_TEXTURE
        | EE_SCREENS_SEFMX_NEW_SCROLL
        | EE_SCREENS_SEFMX_NEW_POLL
        | EE_SCREENS_SEFMX_SCREEN
        | EE_SCREENS_SEFMX_PART_TEXTURE
        | EE_SCREENS_SEFMX_NEW_BITMAP
        | EE_SCREENS_SEFMX_NEW_FULL_BACKGROUND
        | EE_SCREENS_SEFMX_NEW_MAIN_RADIO
        | EE_SCREENS_SEFMX_NEW_BUTTON
        | EE_SCREENS_SEFMX_FORM_BUTTON_TEXTURE
        | EE_SCREENS_SEFMX_SCREEN_EVENT
        | EE_SCREENS_SEFMX_SEE_EVENT
        | EE_SCREENS_SEFMX_FILE_NEW
        | EE_SCREENS_SEFMX_TEXT_OPEN
        | EE_SCREENS_SEFMX_NEW_HOT_KEY
        | EE_SCREENS_SEFMX_NEW_EXIT_ACTION => MAKE,
        EE_SCREENS_SEFMX_SCREEN_COUNT => 3,
        EE_SCREENS_SEFMX_SCREEN_ACTIVATED => 1,
        _ => 0,
    }
}

#[test]
fn basics_makes_the_five_objects_in_order() {
    let mut r =
        Rec::new(|op, i| if op == EE_SCREENS_SEFMX_SCREEN_ACTIVATED { (i[0] == 1) as i32 } else { makes(op, i) });
    let mut out = [0usize; 5];
    let p = [out.as_mut_ptr() as usize, 0x50, 0];
    let (mut i, mut f) = ([0; 4], [0.0; 4]);
    r.run(|x| unsafe { compose(x, EE_SCREENS_SEFM_OP_BASICS, p, &mut i, &mut f) });
    let ops = r.ops();
    let mut want = vec![
        EE_SCREENS_SEFMX_BUTTON_TEXTURE,
        EE_SCREENS_SEFMX_TEXTURE_REFERENCE,
        EE_SCREENS_SEFMX_NEW_SCROLL,
        EE_SCREENS_SEFMX_NEW_POLL,
        EE_SCREENS_SEFMX_POLL_START,
        EE_SCREENS_SEFMX_SCREEN_COUNT,
        EE_SCREENS_SEFMX_SCREEN_ACTIVATED,
        EE_SCREENS_SEFMX_SCREEN_ACTIVATED,
        EE_SCREENS_SEFMX_SCREEN,
        EE_SCREENS_SEFMX_PART_TEXTURE,
    ];
    want.extend((0..8).map(|k| EE_SCREENS_SEFMX_TEXTURE_SETUP + k));
    want.extend([
        EE_SCREENS_SEFMX_NEW_BITMAP,
        EE_SCREENS_SEFMX_SCREEN_INDEX,
        EE_SCREENS_SEFMX_BITMAP_TEXTURE,
        EE_SCREENS_SEFMX_NEW_FULL_BACKGROUND,
    ]);
    assert_eq!(ops, want);
    // the second screen (the first activated) is the part texture's
    assert_eq!(r.log[8].2[0], 1);
    assert_eq!(r.log[9].1[0], 0x1000 + 9);
    assert_eq!(out, [0x1001, 0x1003, 0x1004, 0x1000 + 19, 0x1000 + 22]);
}

#[test]
fn a_bitmap_button_and_a_main_button() {
    let mut b = EeScreensSefmButton { kind: 1, id: 7, index: 9, help: 33, event: 5, ..Default::default() };
    b.normal = [1.0, 2.0];
    b.pressed = [3.0, 4.0];
    b.rect = [10.0, 11.0, 12.0, 13.0];
    let mut out = 0usize;
    let mut r = Rec::new(makes);
    r.run(|x| button(x, &mut out as *mut usize as usize, 0x50, &b));
    assert_eq!(
        r.ops(),
        vec![
            EE_SCREENS_SEFMX_FORM_SCREEN,
            EE_SCREENS_SEFMX_NEW_BITMAP,
            EE_SCREENS_SEFMX_NEW_BITMAP,
            EE_SCREENS_SEFMX_FORM_BUTTON_TEXTURE,
            EE_SCREENS_SEFMX_BITMAP_TEXTURE,
            EE_SCREENS_SEFMX_FORM_BUTTON_TEXTURE,
            EE_SCREENS_SEFMX_BITMAP_TEXTURE,
            EE_SCREENS_SEFMX_BITMAP_STRETCH,
            EE_SCREENS_SEFMX_BITMAP_STRETCH,
            EE_SCREENS_SEFMX_NEW_BUTTON,
            EE_SCREENS_SEFMX_STRING_ID,
            EE_SCREENS_SEFMX_ADD_CONTROL,
            EE_SCREENS_SEFMX_SCREEN_EVENT,
            EE_SCREENS_SEFMX_SET_TRIGGER,
            EE_SCREENS_SEFMX_EVENT_RELEASE,
            EE_SCREENS_SEFMX_BITMAP_RELEASE,
            EE_SCREENS_SEFMX_BITMAP_RELEASE,
        ]
    );
    assert_eq!(r.log[9].2[..2], [7, 9]);
    assert_eq!(r.log[9].3, [10.0, 11.0, 12.0, 13.0]);
    assert_eq!(r.log[13].2[0], SEFMX_TRIGGER_BUTTON_UP);
    assert_eq!(out, 0x1000 + 10);
    // the main button: a radio, its help, added
    b.kind = 0;
    let mut r = Rec::new(makes);
    r.run(|x| button(x, &mut out as *mut usize as usize, 0x50, &b));
    assert_eq!(
        r.ops(),
        vec![EE_SCREENS_SEFMX_NEW_MAIN_RADIO, EE_SCREENS_SEFMX_STRING_ID, EE_SCREENS_SEFMX_ADD_CONTROL]
    );
    assert_eq!(r.log[1].2[0], 33);
}

#[test]
fn copy_world_and_cargo() {
    let mut r = Rec::new(makes);
    let (mut i, mut f) = ([2, 0, 0, 0], [0.0; 4]);
    r.run(|x| unsafe { compose(x, EE_SCREENS_SEFM_OP_COPY_WORLD, [0x60, 0x50, 0], &mut i, &mut f) });
    assert_eq!(r.ops(), vec![EE_SCREENS_SEFMX_PREVIEW_PLACEMENT]);
    let mut r = Rec::new(makes);
    i[0] = 1;
    r.run(|x| unsafe { compose(x, EE_SCREENS_SEFM_OP_COPY_WORLD, [0x60, 0x50, 0], &mut i, &mut f) });
    let ops = r.ops();
    assert_eq!(ops[4..], [EE_SCREENS_SEFMX_ADD_DIRTY, EE_SCREENS_SEFMX_SET_DIRTY]);
    assert_eq!(r.log[4].3[2], 1022.0);
    // placed, no room: the cargo count asked
    let mut r =
        Rec::new(|op, _| (op == EE_SCREENS_SEFMX_UNIT_PLACED || op == EE_SCREENS_SEFMX_UNIT_CARGO_COUNT) as i32);
    let got = r.run(|x| unsafe { compose(x, EE_SCREENS_SEFM_OP_UNIT_CARGO, [0x60, 0x50, 0], &mut i, &mut f) });
    assert_eq!(got, 1);
    assert_eq!(r.ops().len(), 3);
    // not placed: nothing more
    let mut r = Rec::new(|_, _| 0);
    let got = r.run(|x| unsafe { compose(x, EE_SCREENS_SEFM_OP_UNIT_CARGO, [0x60, 0x50, 0], &mut i, &mut f) });
    assert_eq!((got, r.ops().len()), (0, 1));
}

#[test]
fn the_records_text_and_report() {
    // trigger 3 of 2: none
    let mut r = Rec::new(|op, _| if op == EE_SCREENS_SEFMX_SM_COUNT { 2 } else { 0 });
    let (mut i, mut f) = ([EE_SCREENS_SEFM_LIST_TRIGGERS, 3, 0, 8], [0.0; 4]);
    let got = r.run(|x| unsafe { compose(x, EE_SCREENS_SEFM_OP_SM_TEXT, [0, 0, 0], &mut i, &mut f) });
    assert_eq!(got, -1);
    // the report's save: made, named, created, opened, saved (OK), closed, freed
    let mut r = Rec::new(makes);
    let got = r.run(|x| unsafe { compose(x, EE_SCREENS_SEFM_OP_REPORT_SAVE, [0, 0, 0], &mut i, &mut f) });
    assert_eq!(got, 0);
    assert_eq!(
        r.ops(),
        vec![
            EE_SCREENS_SEFMX_FILE_NEW,
            EE_SCREENS_SEFMX_FILE_VOLUME,
            EE_SCREENS_SEFMX_FILE_DIRECTORY,
            EE_SCREENS_SEFMX_FILE_TRIGGERS,
            EE_SCREENS_SEFMX_FILE_CREATE,
            EE_SCREENS_SEFMX_FILE_OPEN_WRITE,
            EE_SCREENS_SEFMX_SM_SAVE,
            EE_SCREENS_SEFMX_FILE_CLOSE,
            EE_SCREENS_SEFMX_FILE_DELETE,
        ]
    );
    // a failed open: no save
    let mut r = Rec::new(|op, i| if op == EE_SCREENS_SEFMX_FILE_OPEN_READ { 5 } else { makes(op, i) });
    r.run(load_triggers);
    assert!(!r.ops().contains(&EE_SCREENS_SEFMX_SM_LOAD));
    assert_eq!(r.ops().len(), 7);
}

#[test]
fn text_copies_at_most_the_capacity() {
    let mut w: crate::screens::hud_direct::CppWide = unsafe { core::mem::zeroed() };
    let bytes = b"Trigger 1\0";
    w.s.string = bytes.as_ptr() as *mut _;
    w.s.byte_length = 9;
    let mut out = [0u8; 4];
    let n = unsafe { text(&w as *const _ as usize, out.as_mut_ptr() as usize, 4) };
    assert_eq!((n, &out), (9, b"Trig"));
}

#[test]
fn close_restores_the_game_in_order() {
    let mut r = Rec::new(makes);
    r.run(|x| close(x, 0x50));
    assert_eq!(
        r.ops(),
        vec![
            EE_SCREENS_SEFMX_FREE_CAMERA,
            EE_SCREENS_SEFMX_CAMERA_MODE_PREFERRED,
            EE_SCREENS_SEFMX_CAMERA_CHANGED,
            EE_SCREENS_SEFMX_UNLOAD_BACKGROUND,
            EE_SCREENS_SEFMX_CLEAR_RENDER,
            EE_SCREENS_SEFMX_PUSH_BASE_RENDER,
            EE_SCREENS_SEFMX_MOUSE_BOUNDS_WINDOW,
            EE_SCREENS_SEFMX_B529065,
            EE_SCREENS_SEFMX_FORM_CLOSE,
        ]
    );
    assert_eq!(r.log[0].2[0], 0);
}

#[test]
fn hot_keys() {
    let events = [0x71usize, 0x72, 0x73];
    let mut r = Rec::new(makes);
    let (mut i, mut f) = ([4, 0, 0, 0], [0.0; 4]);
    r.run(|x| unsafe { compose(x, EE_SCREENS_SEFM_OP_HOT_KEY, [events.as_ptr() as usize, 0x50, 0], &mut i, &mut f) });
    assert_eq!(r.log[2].1[1], 0x71);
    assert_eq!(r.log[3].1[2], 0x72);
    assert_eq!(r.log[4].1[2], 0x73);
    assert_eq!(*r.ops().last().unwrap(), EE_SCREENS_SEFMX_HOT_KEY_DELETE);
    let mut r = Rec::new(makes);
    let mut i = [1, 2, 3, (5 << 4) | 2];
    r.run(|x| unsafe { compose(x, EE_SCREENS_SEFM_OP_ADD_HOT_KEY, [0x99, 0x50, 0], &mut i, &mut f) });
    assert_eq!(r.log[0].2, [1, 2, 3, 5]);
}
