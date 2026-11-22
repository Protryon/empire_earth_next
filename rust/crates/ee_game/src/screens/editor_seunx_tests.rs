use super::*;

/// A recorded statement: the op, the two operands, the integers and floats it got.
type Line = (u32, usize, usize, [i32; 4], [f32; 4]);

/// The fake C++: records every statement, makes handles 0x100, 0x101, ... into out slots, answers `answers[op]`.
struct Fake {
    lines: Vec<Line>,
    next: usize,
    answers: std::collections::BTreeMap<u32, i32>,
    tile: (c_long, c_long),
    found_direction: Option<c_long>,
}

/// The ops whose p0 is an out slot.
const MAKERS: &[u32] = &[
    EE_SCREENS_SEUNX_NEW_BITMAP,
    EE_SCREENS_SEUNX_BUTTON_TEXTURE,
    EE_SCREENS_SEUNX_TYPE_RADIO,
    EE_SCREENS_SEUNX_REMOVE_CARGO_BUTTON,
    EE_SCREENS_SEUNX_ROTATION_BUTTON,
    EE_SCREENS_SEUNX_ACTION_CLICK,
    EE_SCREENS_SEUNX_ACTION_LASSO,
    EE_SCREENS_SEUNX_ACTION_PLACE_UNIT,
    EE_SCREENS_SEUNX_ACTION_PLACE_GATE,
    EE_SCREENS_SEUNX_ACTION_PLACE_WALL,
    EE_SCREENS_SEUNX_ACTION_MOVE_UNIT,
    EE_SCREENS_SEUNX_SELECT_UNITS,
    EE_SCREENS_SEUNX_COMMAND_UNITS,
    EE_SCREENS_SEUNX_LOAD_SCENUNIT,
    EE_SCREENS_SEUNX_UPGRADE_PANEL,
    EE_SCREENS_SEUNX_SEW_ACTION,
    EE_SCREENS_SEUNX_PLAYER,
    EE_SCREENS_SEUNX_TREE_NODE,
    EE_SCREENS_SEUNX_CLASS,
    EE_SCREENS_SEUNX_SELECTED_COMPLEX,
    EE_SCREENS_SEUNX_PICK_UNIT,
    EE_SCREENS_SEUNX_GET_UNIT,
    EE_SCREENS_SEUNX_KEY_DELETE,
    EE_SCREENS_SEUNX_KEY_SHIFT,
    EE_SCREENS_SEUNX_DELETE_ACTION,
];

impl Fake {
    fn new() -> Self {
        Fake { lines: Vec::new(), next: 0x100, answers: Default::default(), tile: (0, 0), found_direction: None }
    }

    fn op(&mut self, op: u32, p: [usize; 3], i: &mut [i32; 4], f: &mut [f32; 4]) -> i32 {
        self.lines.push((op, p[1], p[2], *i, *f));
        match op {
            EE_SCREENS_SEUNX_MOUSE_TILE => {
                // SAFETY: the composed op's out pointers (a usize and two c_longs)
                unsafe {
                    *(p[1] as *mut c_long) = self.tile.0;
                    *(p[2] as *mut c_long) = self.tile.1;
                }
                return 0;
            }
            EE_SCREENS_SEUNX_PREVIEW_GATE => {
                // SAFETY: p2 the composed op's direction; the preview turns it once more
                unsafe { *(p[2] as *mut c_long) += 1 };
                return 0;
            }
            EE_SCREENS_SEUNX_FIND_GATE_WALL => {
                if let Some(d) = self.found_direction {
                    // SAFETY: p2 the composed op's direction
                    unsafe { *(p[2] as *mut c_long) = d };
                    return 1;
                }
                return 0;
            }
            EE_SCREENS_SEUNX_WALL_START => {
                f[0] = 3.0;
                f[1] = 4.0;
            }
            EE_SCREENS_SEUNX_WALL_END => {
                f[0] = 7.0;
                f[1] = 8.0;
            }
            EE_SCREENS_SEUNX_UPGRADE_PANEL => {
                // SAFETY: p2 the composed op's index
                unsafe {
                    assert_eq!(*(p[2] as *const c_ulong), 0x41a);
                    *(p[2] as *mut c_ulong).add(1) += 3;
                }
            }
            _ => {}
        }
        if MAKERS.contains(&op) {
            let h = self.answers.get(&(op | 0x8000_0000)).map_or(self.next, |v| *v as usize);
            self.next += 1;
            // SAFETY: p0 the composed op's out slot
            unsafe { *(p[0] as *mut usize) = h };
            return 0;
        }
        *self.answers.get(&op).unwrap_or(&0)
    }

    fn ops(&self) -> Vec<u32> {
        self.lines.iter().map(|l| l.0).collect()
    }
}

fn run(fake: &mut Fake, op: u32, p: [usize; 3], i: &mut [i32; 4], f: &mut [f32; 4]) -> (Option<i32>, Vec<SewCall>) {
    let mut calls = Vec::new();
    let r = {
        let mut x = |op: u32, p: [usize; 3], i: &mut [i32; 4], f: &mut [f32; 4]| fake.op(op, p, i, f);
        let mut sew = |c: SewCall| {
            calls.push(c);
            7
        };
        // SAFETY: the out slots are the test's
        unsafe { compose(&mut x, &mut sew, op, p, i, f) }
    };
    (r, calls)
}

#[test]
fn type_button_cuts_both_bitmaps_and_makes_the_radio() {
    let mut fake = Fake::new();
    fake.answers.insert(EE_SCREENS_SEUNX_TEXTURE_WIDTH, 256);
    let mut out = [0usize; 3];
    let mut i = [9, 2, 0x5000, 140];
    let mut f = [10.0, 20.0, 45.0, 20.0];
    let (r, _) =
        run(&mut fake, EE_SCREENS_SEUN_OP_TYPE_BUTTON, [out.as_mut_ptr() as usize, 0xf0, 0xe0], &mut i, &mut f);
    assert_eq!(r, Some(0));
    // texture 0x100, normal 0x101, pressed 0x102, the radio 0x103
    assert_eq!(out, [0x103, 0x101, 0x102]);
    let ops = fake.ops();
    assert_eq!(
        ops,
        vec![
            EE_SCREENS_SEUNX_BUTTON_TEXTURE,
            EE_SCREENS_SEUNX_TEXTURE_WIDTH,
            EE_SCREENS_SEUNX_NEW_BITMAP,
            EE_SCREENS_SEUNX_NEW_BITMAP,
            EE_SCREENS_SEUNX_BITMAP_TEXTURE_FORM,
            EE_SCREENS_SEUNX_BITMAP_TEXTURE_FORM,
            EE_SCREENS_SEUNX_BITMAP_COORDS,
            EE_SCREENS_SEUNX_BITMAP_COORDS,
            EE_SCREENS_SEUNX_BITMAP_SCALE,
            EE_SCREENS_SEUNX_BITMAP_SCALE,
            EE_SCREENS_SEUNX_TYPE_RADIO,
            EE_SCREENS_SEUNX_RADIO_UP_CHECKED,
            EE_SCREENS_SEUNX_RADIO_DOWN_CHECKED,
            EE_SCREENS_SEUNX_RADIO_DOWN_UNCHECKED,
            EE_SCREENS_SEUNX_RADIO_UP_UNCHECKED,
            EE_SCREENS_SEUNX_RADIO_NO_FOCUS,
            EE_SCREENS_SEUNX_RADIO_DRAW_OFFSET,
            EE_SCREENS_SEUNX_RADIO_TRIGGER,
            EE_SCREENS_SEUNX_RADIO_STRING_ID,
            EE_SCREENS_SEUNX_ADD_RADIO,
        ]
    );
    let l = &fake.lines;
    // the corners of the normal cell (10, 20) and the pressed cell (45, 20) over the 256 texture
    assert_eq!(l[6].1, 0x101);
    assert_eq!(l[6].4, [10.0 / 256.0, 20.0 / 256.0, 45.0 / 256.0, 45.0 / 256.0]);
    assert_eq!(l[7].4, [45.0 / 256.0, 20.0 / 256.0, 80.0 / 256.0, 45.0 / 256.0]);
    assert_eq!(l[8].4, [35.0 / 256.0, 25.0 / 256.0, 0.0, 0.0]);
    assert_eq!(l[10].3, [9, 2, 0, 140]);
    // the up unchecked bitmap is the normal one, the others the pressed one; the event; the string; layer 6
    assert_eq!((l[11].2, l[14].2), (0x102, 0x101));
    assert_eq!(l[17].2, 0xe0);
    assert_eq!(l[18].3[0], 0x5000);
    assert_eq!((l[19].1, l[19].2, l[19].3[0]), (0xf0, 0x103, 6));
}

#[test]
fn place_wall_and_gate_call_the_rust_action() {
    let mut fake = Fake::new();
    fake.answers.insert(EE_SCREENS_SEUNX_WALL_TECH, 77);
    let (_, calls) = run(&mut fake, EE_SCREENS_SEUN_OP_PLACE_WALL, [0x10, 0x20, 0], &mut [2, 0, 0, 0], &mut [0.0; 4]);
    assert_eq!(calls, vec![SewCall::Wall(0x100, 3.0, 4.0, 7.0, 8.0, 2, 77)]);
    assert_eq!(fake.lines[2].1, 0x20);
    let mut fake = Fake::new();
    fake.answers.insert(EE_SCREENS_SEUNX_GATE_TILE_X, 5);
    fake.answers.insert(EE_SCREENS_SEUNX_GATE_TILE_Y, 6);
    fake.answers.insert(EE_SCREENS_SEUNX_GATE_TECH, 8);
    fake.answers.insert(EE_SCREENS_SEUNX_GATE_DIRECTION, 3);
    let (_, calls) = run(&mut fake, EE_SCREENS_SEUN_OP_PLACE_GATE, [0x10, 0x20, 0], &mut [1, 0, 0, 0], &mut [0.0; 4]);
    assert_eq!(calls, vec![SewCall::Gate(0x100, 5, 6, 1, 8, 3)]);
    let mut fake = Fake::new();
    let (r, calls) = run(&mut fake, EE_SCREENS_SEUN_OP_MOVING, [0x30, 0, 0], &mut [0; 4], &mut [0.0; 4]);
    assert_eq!((r, calls), (Some(1), vec![SewCall::Moving(0x100)]));
    assert_eq!(fake.lines[0].1, 0x30);
}

#[test]
fn wall_preview_walks_from_the_start_to_the_mouse() {
    // state 1: the tiles from the start (3, 4) to the mouse's tile centre (6.5, 4.5)
    let mut fake = Fake::new();
    fake.tile = (6, 4);
    fake.answers.insert(EE_SCREENS_SEUNX_WALL_STATE, 1);
    run(&mut fake, EE_SCREENS_SEUN_OP_PREVIEW, [0x10, 0, 0], &mut [1, 0, 0, 0], &mut [0.0; 4]);
    let pushes: Vec<[i32; 2]> =
        fake.lines.iter().filter(|l| l.0 == EE_SCREENS_SEUNX_TILES_PUSH).map(|l| [l.3[0], l.3[1]]).collect();
    let mut want = Vec::new();
    Ge2DPoint { x: 3.0, y: 4.0 }
        .interpolate_to(Ge2DPoint { x: 6.5, y: 4.5 }, 1.0, |q| want.push([q.x as i32, q.y as i32]));
    assert_eq!(pushes, want);
    assert_eq!(*pushes.last().unwrap(), [6, 4]);
    let tail: Vec<u32> = fake.ops().into_iter().rev().take(3).collect();
    assert_eq!(tail, vec![EE_SCREENS_SEUNX_LOCK_LEAVE, EE_SCREENS_SEUNX_PREVIEW_WALL, EE_SCREENS_SEUNX_LOCK_ENTER]);
    // state 0: the first tile alone
    let mut fake = Fake::new();
    fake.tile = (2, 9);
    run(&mut fake, EE_SCREENS_SEUN_OP_PREVIEW, [0x10, 0, 0], &mut [1, 0, 0, 0], &mut [0.0; 4]);
    let pushes: Vec<[i32; 2]> =
        fake.lines.iter().filter(|l| l.0 == EE_SCREENS_SEUNX_TILES_PUSH).map(|l| [l.3[0], l.3[1]]).collect();
    assert_eq!(pushes, vec![[2, 9]]);
}

#[test]
fn gate_preview_keeps_the_found_direction() {
    let mut fake = Fake::new();
    fake.tile = (4, 5);
    fake.answers.insert(EE_SCREENS_SEUNX_GATE_DIRECTION, 2);
    fake.found_direction = Some(1);
    run(&mut fake, EE_SCREENS_SEUN_OP_PREVIEW, [0x10, 0, 0], &mut [2, 0, 0, 0], &mut [0.0; 4]);
    let ops = fake.ops();
    assert_eq!(
        ops,
        vec![
            EE_SCREENS_SEUNX_MOUSE_TILE,
            EE_SCREENS_SEUNX_GATE_DIRECTION,
            EE_SCREENS_SEUNX_FIND_GATE_WALL,
            EE_SCREENS_SEUNX_CREATE_GATE,
            EE_SCREENS_SEUNX_PREVIEW_GATE,
            EE_SCREENS_SEUNX_SET_DIRECTION,
        ]
    );
    assert_eq!(fake.lines[2].3[..2], [4, 5]);
    // found 1, turned to 2 by the preview: the gate keeps 2
    assert_eq!(fake.lines[5].3[0], 2);
    let mut fake = Fake::new();
    run(&mut fake, EE_SCREENS_SEUN_OP_PREVIEW, [0x10, 0, 0], &mut [2, 0, 0, 0], &mut [0.0; 4]);
    assert_eq!(
        fake.ops()[3..],
        [EE_SCREENS_SEUNX_LOCK_ENTER, EE_SCREENS_SEUNX_CLEAR_GHOSTS, EE_SCREENS_SEUNX_LOCK_LEAVE]
    );
}

#[test]
fn place_info_reads_the_pick_in_order() {
    let mut fake = Fake::new();
    fake.answers.insert(EE_SCREENS_SEUNX_TYPE_AT, 12);
    fake.answers.insert(EE_SCREENS_SEUNX_CLASS_EXISTS, 1);
    fake.answers.insert(EE_SCREENS_SEUNX_CLASS_TECH, 300);
    fake.answers.insert(EE_SCREENS_SEUNX_PLAYER_ID, 0);
    fake.answers.insert(EE_SCREENS_SEUNX_GAIA_PLACEABLE, 1);
    fake.answers.insert(EE_SCREENS_SEUNX_NODE_KIND, 6);
    let mut i = [0, 3, 0, 0];
    let (r, _) = run(&mut fake, EE_SCREENS_SEUN_OP_PLACE_INFO, [0x40, 0, 0], &mut i, &mut [0.0; 4]);
    assert_eq!(r, Some(0));
    // gaia and gaia placeable: placeable; a wall node
    assert_eq!((i[1], i[2]), (1, EE_SCREENS_SEUN_KIND_WALL));
    assert_eq!(
        fake.ops(),
        vec![
            EE_SCREENS_SEUNX_PLAYER,
            EE_SCREENS_SEUNX_TYPE_AT,
            EE_SCREENS_SEUNX_CLASS_EXISTS,
            EE_SCREENS_SEUNX_CLASS_TECH,
            EE_SCREENS_SEUNX_TREE_NODE,
            EE_SCREENS_SEUNX_CLASS,
            EE_SCREENS_SEUNX_PLAYER_ID,
            EE_SCREENS_SEUNX_GAIA_PLACEABLE,
            EE_SCREENS_SEUNX_NODE_KIND,
        ]
    );
    assert_eq!((fake.lines[1].1, fake.lines[1].3[0]), (0x40, 3));
    assert_eq!(fake.lines[4].3[0], 300);
    // a player that may not place the class, a gate; no class technology: the node of kTechNone
    let mut fake = Fake::new();
    fake.answers.insert(EE_SCREENS_SEUNX_PLAYER_ID, 2);
    fake.answers.insert(EE_SCREENS_SEUNX_CLASS_IS_GATE, 1);
    let mut i = [2, 0, 0, 0];
    let (r, _) = run(&mut fake, EE_SCREENS_SEUN_OP_PLACE_INFO, [0x40, 0, 0], &mut i, &mut [0.0; 4]);
    assert_eq!((r, i[1], i[2]), (Some(2), 0, EE_SCREENS_SEUN_KIND_GATE));
    assert_eq!(fake.lines[3].3[0], TECH_NONE);
    assert!(fake.ops().contains(&EE_SCREENS_SEUNX_PLAYER_PLACEABLE));
    assert!(!fake.ops().contains(&EE_SCREENS_SEUNX_GAIA_PLACEABLE));
}

#[test]
fn start_build_sets_the_gate_from_the_node() {
    let mut fake = Fake::new();
    fake.answers.insert(EE_SCREENS_SEUNX_NODE_ID, 55);
    fake.answers.insert(EE_SCREENS_SEUNX_NODE_UNIT_CLASS, 66);
    fake.answers.insert(EE_SCREENS_SEUNX_PLAYER_ID, 3);
    run(&mut fake, EE_SCREENS_SEUN_OP_START_BUILD, [0x10, 0x40, 0], &mut [3, 1, 1, 0], &mut [0.0; 4]);
    let tail: Vec<(u32, i32)> = fake.lines[5..].iter().map(|l| (l.0, l.3[0])).collect();
    assert_eq!(
        tail,
        vec![
            (EE_SCREENS_SEUNX_NODE_ID, 0),
            (EE_SCREENS_SEUNX_GATE_SET_TECH, 55),
            (EE_SCREENS_SEUNX_NODE_UNIT_CLASS, 0),
            (EE_SCREENS_SEUNX_GATE_SET_WALL_TYPE, 66),
            (EE_SCREENS_SEUNX_GATE_SET_CLASS, 0),
            (EE_SCREENS_SEUNX_PLAYER_ID, 0),
            (EE_SCREENS_SEUNX_GATE_INITIALIZE, 3),
        ]
    );
    // the class (made fifth, 0x104... after player 0x100, node 0x101, class 0x102) on the gate
    assert_eq!(fake.lines[9].2, 0x102);
}

#[test]
fn pick_keys_mouse_and_the_rest() {
    let mut fake = Fake::new();
    let (r, _) = run(&mut fake, EE_SCREENS_SEUN_OP_PICK, [0, 0, 0], &mut [0; 4], &mut [1.0, 2.0, 0.0, 0.0]);
    assert_eq!(r, Some(0));
    assert_eq!(
        fake.ops(),
        vec![
            EE_SCREENS_SEUNX_PICK_UNIT,
            EE_SCREENS_SEUNX_UNIT_ID,
            EE_SCREENS_SEUNX_UNIT_IS_GATE,
            EE_SCREENS_SEUNX_UNIT_DEREFERENCE
        ]
    );
    let mut fake = Fake::new();
    fake.answers.insert(EE_SCREENS_SEUNX_PICK_UNIT | 0x8000_0000, 0);
    let (r, _) = run(&mut fake, EE_SCREENS_SEUN_OP_PICK, [0, 0, 0], &mut [0; 4], &mut [0.0; 4]);
    assert_eq!((r, fake.lines.len()), (Some(-1), 1));
    let mut fake = Fake::new();
    fake.answers.insert(EE_SCREENS_SEUNX_KEY_PRESSED, 1);
    fake.answers.insert(EE_SCREENS_SEUNX_KEY_DOWN, 1);
    assert_eq!(run(&mut fake, EE_SCREENS_SEUN_OP_KEYS, [0; 3], &mut [0; 4], &mut [0.0; 4]).0, Some(3));
    let mut fake = Fake::new();
    fake.answers.insert(EE_SCREENS_SEUNX_L_PRESSED, 1);
    fake.answers.insert(EE_SCREENS_SEUNX_R_RELEASED, 1);
    assert_eq!(
        run(&mut fake, EE_SCREENS_SEUN_OP_MOUSE, [0; 3], &mut [0; 4], &mut [0.0; 4]).0,
        Some(EE_SCREENS_SEUN_MOUSE_L_PRESSED | EE_SCREENS_SEUN_MOUSE_R_RELEASED)
    );
    let mut fake = Fake::new();
    let mut i = [4, 0, 0, 0];
    let mut out = 0usize;
    run(&mut fake, EE_SCREENS_SEUN_OP_UPGRADE_PANEL, [&mut out as *mut usize as usize, 0x50, 0], &mut i, &mut [0.0; 4]);
    assert_eq!((i[0], out), (7, 0x100));
    let mut fake = Fake::new();
    fake.answers.insert(EE_SCREENS_SEUNX_ROTATION_ANGLE, 1.5f32.to_bits() as i32);
    let mut f = [0.0; 4];
    run(&mut fake, EE_SCREENS_SEUN_OP_ANGLE, [0x60, 0, 0], &mut [0; 4], &mut f);
    assert_eq!(f[0], 1.5);
    let mut fake = Fake::new();
    run(&mut fake, EE_SCREENS_SEUN_OP_GAME_VIEW, [0; 3], &mut [1, 0, 0, 0], &mut [0.0; 4]);
    assert_eq!(fake.lines[0].4, [225.0, 0.0, 1024.0, 606.0]);
    assert!(!fake.ops().contains(&EE_SCREENS_SEUNX_MINIMAP_REFRESH));
    // the rotation button: its string, added on layer 6
    let mut fake = Fake::new();
    let mut out = 0usize;
    run(
        &mut fake,
        EE_SCREENS_SEUN_OP_ROTATION,
        [&mut out as *mut usize as usize, 0x70, 0],
        &mut [71, 0, 0, 0],
        &mut [0.0; 4],
    );
    assert_eq!(
        fake.ops(),
        vec![EE_SCREENS_SEUNX_ROTATION_BUTTON, EE_SCREENS_SEUNX_ROTATION_STRING, EE_SCREENS_SEUNX_ADD_BUTTON]
    );
    assert_eq!((fake.lines[2].1, fake.lines[2].2, fake.lines[2].3[0], out), (0x70, 0x100, 6, 0x100));
    let mut fake = Fake::new();
    assert_eq!(run(&mut fake, EE_SCREENS_SEUN_OP_SORT_EVENT, [0; 3], &mut [0; 4], &mut [0.0; 4]).0, None);
}
