//! The scenario editor form's own events with state, whole in Rust behind one C++ event (`EESEERust`,
//! `ee/EESEARust.h`): the hide-UI check box (`EESEEHideUI`: the framed or the full game view posted as a viewport
//! change), the viewport change (`EESEEChangeViewport`: the editor's game view set, its UI shown or hidden, all
//! redrawn), the toolbar's quick load and quick save (`EESEEQuickLoad`, `EESEEQuickSave`: the editor's shared quick
//! save/load action, referenced while the event lives, shown loaded or saved through a quick save action).

// ---- the kinds (`EESEERust`'s constructor)

/// `EESEEHideUI`.
pub const EE_SCREENS_SEE_HIDE_UI: u32 = 1;
/// `EESEEChangeViewport` (its rectangle and flag: `ee_screens_see_viewport`).
pub const EE_SCREENS_SEE_CHANGE_VIEWPORT: u32 = 2;
/// `EESEEQuickLoad` (object: the quick save/load action).
pub const EE_SCREENS_SEE_QUICK_LOAD: u32 = 3;
/// `EESEEQuickSave` (object: the quick save/load action).
pub const EE_SCREENS_SEE_QUICK_SAVE: u32 = 4;

// ---- the host's ops (`ee_screenscpp_see`: `p` a pointer, `i` up to 2 integers, `f` up to 4 floats)

/// The object p referenced / released.
pub const EE_SCREENS_SEE_OP_REFERENCE: u32 = 1;
pub const EE_SCREENS_SEE_OP_DEREFERENCE: u32 = 2;
/// -> whether the check box p is checked.
pub const EE_SCREENS_SEE_OP_CHECKED: u32 = 3;
/// A viewport change to the rectangle f0..f3 (left, top, right, bottom), the UI hidden when i0, posted -> the
/// post's result.
pub const EE_SCREENS_SEE_OP_POST_VIEWPORT: u32 = 4;
/// -> whether the scenario editor's form exists.
pub const EE_SCREENS_SEE_OP_HAS_EDITOR: u32 = 5;
/// The editor's game view set to the rectangle f0..f3, its UI shown when i0 (`SetGameView`).
pub const EE_SCREENS_SEE_OP_SET_GAME_VIEW: u32 = 6;
/// Everything redrawn (`gUIManager->RedrawAll`).
pub const EE_SCREENS_SEE_OP_REDRAW_ALL: u32 = 7;
/// The scenario editor shows the loaded object p (`PostShowLoadedObject`; the event's form is the editor's).
pub const EE_SCREENS_SEE_OP_SHOW_LOADED: u32 = 8;
/// A quick save action of the quick save/load action p posted.
pub const EE_SCREENS_SEE_OP_QUICK_SAVE: u32 = 9;

/// The form, the controls and the actions.
pub trait SeeHost {
    fn op(&mut self, op: u32, p: usize, i: &mut [i32; 2], f: &mut [f32; 4]) -> i32;
}

fn call<H: SeeHost + ?Sized>(h: &mut H, op: u32, p: usize, ints: [i32; 2], floats: [f32; 4]) -> i32 {
    let (mut i, mut f) = (ints, floats);
    h.op(op, p, &mut i, &mut f)
}

/// What an event does.
#[derive(Clone, Debug, PartialEq)]
pub enum SeeKind {
    /// the framed view (unchecked) and the full view (checked): the original's two default rectangles, all zero
    HideUi {
        framed: [f32; 4],
        full: [f32; 4],
    },
    ChangeViewport {
        rectangle: [f32; 4],
        hidden: bool,
    },
    QuickLoad {
        owner: usize,
    },
    QuickSave {
        owner: usize,
    },
}

impl SeeKind {
    /// The event of `kind` (`object`: the quick save/load action, referenced); None for an unknown kind.
    pub fn new<H: SeeHost + ?Sized>(h: &mut H, kind: u32, object: usize) -> Option<SeeKind> {
        Some(match kind {
            EE_SCREENS_SEE_HIDE_UI => SeeKind::HideUi { framed: [0.0; 4], full: [0.0; 4] },
            EE_SCREENS_SEE_CHANGE_VIEWPORT => SeeKind::ChangeViewport { rectangle: [0.0; 4], hidden: false },
            EE_SCREENS_SEE_QUICK_LOAD | EE_SCREENS_SEE_QUICK_SAVE => {
                call(h, EE_SCREENS_SEE_OP_REFERENCE, object, [0; 2], [0.0; 4]);
                if kind == EE_SCREENS_SEE_QUICK_LOAD {
                    SeeKind::QuickLoad { owner: object }
                } else {
                    SeeKind::QuickSave { owner: object }
                }
            }
            _ => return None,
        })
    }

    /// The viewport change's rectangle and flag.
    pub fn set_viewport(&mut self, to: [f32; 4], to_hidden: bool) {
        if let SeeKind::ChangeViewport { rectangle, hidden } = self {
            *rectangle = to;
            *hidden = to_hidden;
        }
    }

    /// `ProcessEvent(form, entity)` (the form is the editor's: unused).
    pub fn process<H: SeeHost + ?Sized>(&self, h: &mut H, entity: usize) -> i32 {
        match self {
            SeeKind::HideUi { framed, full } => {
                let checked = call(h, EE_SCREENS_SEE_OP_CHECKED, entity, [0; 2], [0.0; 4]) != 0;
                let rectangle = if checked { *full } else { *framed };
                // the check box read again for the flag, as the original's second GetIsChecked
                let hidden = call(h, EE_SCREENS_SEE_OP_CHECKED, entity, [0; 2], [0.0; 4]);
                call(h, EE_SCREENS_SEE_OP_POST_VIEWPORT, 0, [hidden, 0], rectangle)
            }
            SeeKind::ChangeViewport { rectangle, hidden } => {
                if call(h, EE_SCREENS_SEE_OP_HAS_EDITOR, 0, [0; 2], [0.0; 4]) != 0 {
                    call(h, EE_SCREENS_SEE_OP_SET_GAME_VIEW, 0, [i32::from(!*hidden), 0], *rectangle);
                    call(h, EE_SCREENS_SEE_OP_REDRAW_ALL, 0, [0; 2], [0.0; 4]);
                }
                0
            }
            SeeKind::QuickLoad { owner } => {
                call(h, EE_SCREENS_SEE_OP_SHOW_LOADED, *owner, [0; 2], [0.0; 4]);
                0
            }
            SeeKind::QuickSave { owner } => {
                call(h, EE_SCREENS_SEE_OP_QUICK_SAVE, *owner, [0; 2], [0.0; 4]);
                0
            }
        }
    }

    /// The event freed: the quick save/load action released.
    pub fn free<H: SeeHost + ?Sized>(&self, h: &mut H) {
        if let SeeKind::QuickLoad { owner } | SeeKind::QuickSave { owner } = self {
            call(h, EE_SCREENS_SEE_OP_DEREFERENCE, *owner, [0; 2], [0.0; 4]);
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    struct Rec {
        log: Vec<String>,
        checked: bool,
        editor: bool,
    }

    impl SeeHost for Rec {
        fn op(&mut self, op: u32, p: usize, i: &mut [i32; 2], f: &mut [f32; 4]) -> i32 {
            match op {
                EE_SCREENS_SEE_OP_CHECKED => return i32::from(self.checked),
                EE_SCREENS_SEE_OP_HAS_EDITOR => return i32::from(self.editor),
                EE_SCREENS_SEE_OP_POST_VIEWPORT => {
                    self.log.push(format!("post {f:?} {}", i[0]));
                    return 5;
                }
                _ => {}
            }
            self.log.push(format!("{op} p{p} {i:?} {f:?}"));
            0
        }
    }

    fn rec() -> Rec {
        Rec { log: Vec::new(), checked: false, editor: true }
    }

    #[test]
    fn hide_ui_posts_the_view_of_the_check_state() {
        let mut h = rec();
        let e = SeeKind::new(&mut h, EE_SCREENS_SEE_HIDE_UI, 0).unwrap();
        assert_eq!(e.process(&mut h, 2), 5);
        h.checked = true;
        e.process(&mut h, 2);
        assert_eq!(h.log, ["post [0.0, 0.0, 0.0, 0.0] 0", "post [0.0, 0.0, 0.0, 0.0] 1"]);
    }

    #[test]
    fn viewport_change_sets_the_view_when_the_editor_is_open() {
        let mut h = rec();
        let mut e = SeeKind::new(&mut h, EE_SCREENS_SEE_CHANGE_VIEWPORT, 0).unwrap();
        e.set_viewport([1.0, 2.0, 3.0, 4.0], true);
        e.process(&mut h, 0);
        assert_eq!(h.log, ["6 p0 [0, 0] [1.0, 2.0, 3.0, 4.0]", "7 p0 [0, 0] [0.0, 0.0, 0.0, 0.0]"]);
        h.log.clear();
        h.editor = false;
        e.process(&mut h, 0);
        assert!(h.log.is_empty());
        assert_eq!(SeeKind::new(&mut h, 9, 0), None);
    }

    #[test]
    fn quick_load_and_save_hold_their_action() {
        let mut h = rec();
        let l = SeeKind::new(&mut h, EE_SCREENS_SEE_QUICK_LOAD, 0x40).unwrap();
        let s = SeeKind::new(&mut h, EE_SCREENS_SEE_QUICK_SAVE, 0x40).unwrap();
        l.process(&mut h, 0);
        s.process(&mut h, 0);
        l.free(&mut h);
        s.free(&mut h);
        let ops: Vec<&str> = h.log.iter().map(|x| x.split(' ').next().unwrap()).collect();
        assert_eq!(ops, ["1", "1", "8", "9", "2", "2"]);
        assert!(h.log.iter().all(|x| x.contains("p64")));
    }
}
