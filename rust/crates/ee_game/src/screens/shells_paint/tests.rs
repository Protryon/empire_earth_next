use super::*;
use core::cell::RefCell;

/// A host answering through a closure (op, a, b, floats) and recording the ops, the words and the areas painted.
struct Host<F: Fn(u32, i64, i64, &mut [f32; 4]) -> i64> {
    answer: F,
    ops: RefCell<Vec<(u32, i64, i64)>>,
    areas: RefCell<Vec<[i32; 4]>>,
}

impl<F: Fn(u32, i64, i64, &mut [f32; 4]) -> i64> ShellHost for Host<F> {
    fn call(&self, _ctl: Cpp, op: u32, a: i64, b: i64, f: &mut [f32; 4]) -> i64 {
        self.ops.borrow_mut().push((op, a, b));
        if op == EE_SCREENS_SH_PAINT_AREA {
            // SAFETY: the kind's four longs
            self.areas.borrow_mut().push(unsafe { *(b as *const [i32; 4]) });
        }
        (self.answer)(op, a, b, f)
    }
}

fn host<F: Fn(u32, i64, i64, &mut [f32; 4]) -> i64>(answer: F) -> Host<F> {
    Host { answer, ops: RefCell::new(Vec::new()), areas: RefCell::new(Vec::new()) }
}

fn ctx<F: Fn(u32, i64, i64, &mut [f32; 4]) -> i64>(h: &Host<F>) -> Ctx<'_> {
    Ctx { ctl: core::ptr::null_mut(), host: h }
}

const ACTION: i64 = 0x5000;

#[test]
fn the_kinds_keep_their_class_names() {
    assert_eq!(make(EE_SCREENS_SK_PAINT_ELEVATION, &[1, 2]).unwrap().name(), c"EEUIFSEPaintElevation");
    assert_eq!(make(EE_SCREENS_SK_TERRAIN_PAINT, &[1, 2, 3]).unwrap().name(), c"EEUIFSETerrainPaint");
    assert!(make(0x7f, &[]).is_none());
    let mut f = [0.0; 4];
    let h = host(|_, _, _, _| 0);
    // the terrain area starts centred, the elevation area not
    assert_eq!(
        make(EE_SCREENS_SK_TERRAIN_PAINT, &[0, 0, 9]).unwrap().op(
            &ctx(&h),
            EE_SCREENS_SKOP_PAINT_CENTERED,
            0,
            0,
            &mut f
        ),
        1
    );
    assert_eq!(
        make(EE_SCREENS_SK_PAINT_ELEVATION, &[]).unwrap().op(&ctx(&h), EE_SCREENS_SKOP_PAINT_CENTERED, 0, 0, &mut f),
        0
    );
}

/// A world: the view (0, 0, 1024, 606), the window 1024x768, every tile present, the brush size 2, the mouse at
/// (300, 200), the picked point `pick`, the areas accepted.
fn world(pick: [f32; 2]) -> impl Fn(u32, i64, i64, &mut [f32; 4]) -> i64 {
    move |op, _a, _b, f| match op {
        EE_SCREENS_SH_VIEW_RECT => {
            *f = [0.0, 0.0, 1024.0, 606.0];
            0
        }
        EE_SCREENS_SH_WINDOW_SIZE => 1024 | 768 << 32,
        EE_SCREENS_SH_MOUSE_AT => {
            *f = [300.0, 200.0, 0.0, 0.0];
            0
        }
        EE_SCREENS_SH_PICK => {
            *f = [pick[0], pick[1], 1.5, 0.0];
            1
        }
        EE_SCREENS_SH_MAP_TILE => 1,
        EE_SCREENS_SH_BRUSH_GROUP_SIZE => 2,
        EE_SCREENS_SH_PAINT_AREA => 1,
        _ => 0,
    }
}

#[test]
fn a_centred_press_paints_the_brush_square_and_a_drag_paints_the_path() {
    let h = host(world([10.7, 20.2]));
    let k = make(EE_SCREENS_SK_TERRAIN_PAINT, &[0x100, 0x200, ACTION]).unwrap();
    let mut f = [0.0; 4];
    k.virt(&ctx(&h), EE_SCREENS_SV_MOUSE_L_DOWN, 0, 0, &mut f);
    // centred: the tile half a tile down-left (10, 19), the radius the brush size
    assert_eq!(*h.areas.borrow(), vec![[8, 21, 12, 17]]);
    let ops = h.ops.borrow().iter().map(|o| o.0).collect::<Vec<_>>();
    assert_eq!(
        ops,
        vec![
            EE_SCREENS_SH_VIEW_RECT,
            EE_SCREENS_SH_CAPTURE_MOUSE,
            EE_SCREENS_SH_MOUSE_AT,
            EE_SCREENS_SH_PICK,
            EE_SCREENS_SH_MOUSE_BOUNDS,
            EE_SCREENS_SH_SCROLL_EDGES,
            EE_SCREENS_SH_MAP_TILE,
            EE_SCREENS_SH_BRUSH_GROUP_SIZE,
            EE_SCREENS_SH_PAINT_AREA,
            EE_SCREENS_SH_ACTION_POST,
        ]
    );
    assert!(h.ops.borrow().contains(&(EE_SCREENS_SH_SCROLL_EDGES, 0x100, 0)));
    // a move three tiles right: each tile after the last one painted, one post, the highlight at the point
    let h2 = host(world([13.7, 20.2]));
    k.virt(&ctx(&h2), EE_SCREENS_SV_MOUSE_MOVE, 0, 0, &mut [301.0, 200.0, 0.0, 0.0]);
    assert_eq!(*h2.areas.borrow(), vec![[9, 21, 13, 17], [10, 21, 14, 17], [11, 21, 15, 17]]);
    assert_eq!(h2.ops.borrow().iter().filter(|o| o.0 == EE_SCREENS_SH_ACTION_POST).count(), 1);
    assert_eq!(h2.ops.borrow().last().copied(), Some((EE_SCREENS_SH_BRUSH_HIGHLIGHT, 1, 0)));
    // the release lets the action go and bounds the mouse to the window
    let h3 = host(world([0.0, 0.0]));
    k.virt(&ctx(&h3), EE_SCREENS_SV_MOUSE_L_UP, 0, 0, &mut f);
    assert!(h3.ops.borrow().contains(&(EE_SCREENS_SH_ACTION_RELEASE, ACTION, 0)));
    // no action: a move does nothing
    let h4 = host(world([20.0, 20.0]));
    k.virt(&ctx(&h4), EE_SCREENS_SV_MOUSE_MOVE, 0, 0, &mut f);
    assert!(h4.ops.borrow().is_empty());
}

#[test]
fn the_elevation_level_comes_from_the_sliders() {
    // raise checked: max - value - 0x18; else the strength slider's (max - value) - 6
    for (raise, slider, want) in [(true, (64i64, 30i64), 10i64), (false, (12, 3), 3)] {
        let h = host(move |op, a, _b, _f| match op {
            EE_SCREENS_SH_FORM_CHECKED => i64::from(raise && a == RAISE_BUTTON),
            EE_SCREENS_SH_SLIDER_OF => slider.0 << 32 | slider.1,
            EE_SCREENS_SH_PAINT_NEW => ACTION,
            _ => 0,
        });
        let k = make(EE_SCREENS_SK_PAINT_ELEVATION, &[0, 0]).unwrap();
        let mut f = [0.0; 4];
        assert_eq!(k.op(&ctx(&h), EE_SCREENS_SKOP_PAINT_CREATE, 0, 0, &mut f), ACTION);
        assert!(h.ops.borrow().contains(&(EE_SCREENS_SH_PAINT_NEW, 1, want)));
    }
}

#[test]
fn a_right_drag_raises_the_grabbed_square_to_the_height_under_the_mouse() {
    // the grabbed tile (5, 6) at height 0 (level 0); the screen line straight down from (5, 6, 1): parallel to the
    // vertical drag plane, so t is 0 and the point the line's start, 1 above the plane's point
    let h = host(|op, a, _b, f| match op {
        EE_SCREENS_SH_VIEW_RECT => {
            *f = [0.0, 0.0, 1024.0, 606.0];
            0
        }
        EE_SCREENS_SH_PICK => {
            *f = [5.5, 6.25, 0.0, 0.0];
            1
        }
        EE_SCREENS_SH_BRUSH_GROUP_SIZE => 1,
        EE_SCREENS_SH_MOUSE_BUTTONS => RIGHT_DOWN | HAS_CAPTURE,
        EE_SCREENS_SH_FORM_CHECKED => i64::from(a == RAISE_BUTTON),
        EE_SCREENS_SH_SCREEN_LINE => {
            // SAFETY: the kind's six floats
            unsafe { *(a as *mut [f32; 6]) = [5.0, 6.0, 1.0, 0.0, 0.0, -1.0] };
            0
        }
        EE_SCREENS_SH_MAP_TILE => {
            f[0] = 0.0;
            1
        }
        EE_SCREENS_SH_PAINT_NEW => ACTION,
        EE_SCREENS_SH_PAINT_AREA => 1,
        _ => 0,
    });
    let k = make(EE_SCREENS_SK_PAINT_ELEVATION, &[0, 0]).unwrap();
    let mut f = [0.0; 4];
    k.virt(&ctx(&h), EE_SCREENS_SV_MOUSE_R_DOWN, 0, 0, &mut f);
    k.virt(&ctx(&h), EE_SCREENS_SV_MOUSE_MOVE, 0, 0, &mut [5.0, 6.0, 0.0, 0.0]);
    // the new level floor(1 * 4.899 + 0.00001) = 4, the old 0: the square around (5, 6) painted at 4
    assert!(h.ops.borrow().contains(&(EE_SCREENS_SH_PAINT_NEW, 1, 4)));
    assert_eq!(*h.areas.borrow(), vec![[4, 7, 6, 5]]);
    assert!(h.ops.borrow().contains(&(EE_SCREENS_SH_ACTION_RELEASE, ACTION, 0)));
}

#[test]
fn cvtt_is_the_x64_conversion() {
    assert_eq!(cvtt(-0.5), 0);
    assert_eq!(cvtt(2.99), 2);
    assert_eq!(cvtt(f32::NAN), i32::MIN);
    assert_eq!(cvtt(3e9), i32::MIN);
}
