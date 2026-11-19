use super::*;
use core::cell::RefCell;

/// A box host recording its ops and the slots' address; DLG_NEW fills slot `a` with `0x100 + a`.
#[derive(Default)]
struct Box_ {
    ops: RefCell<Vec<(u32, u32, u32)>>,
}

impl DialogCpp for &Box_ {
    fn dialog(&self, _form: Cpp, slots: *mut usize, op: u32, a: u32, b: u32) -> u32 {
        self.ops.borrow_mut().push((op, a, b));
        if op == crate::screens::dialog::EE_SCREENS_DLG_NEW {
            // SAFETY: the kind's six slots
            unsafe { *slots.add(a as usize) = 0x100 + a as usize };
        }
        0
    }
}

/// A shell host recording its calls.
#[derive(Default)]
struct Shell {
    ops: RefCell<Vec<(u32, i64, i64)>>,
}

impl ShellHost for Shell {
    fn call(&self, _ctl: Cpp, op: u32, a: i64, b: i64, _f: &mut [f32; 4]) -> i64 {
        self.ops.borrow_mut().push((op, a, b));
        0
    }
}

#[test]
fn the_box_makes_its_controls_into_its_slots_and_keeps_its_mode() {
    let cpp = Box_::default();
    let shell = Shell::default();
    let c = Ctx { ctl: core::ptr::null_mut(), host: &shell };
    let d = Dialog::new(&cpp);
    assert_eq!(d.name(), c"EEUIFError");
    d.init(&c);
    let s = d.slots.get();
    assert_eq!(&s[..5], &[0x100, 0x101, 0x102, 0x103, 0x104]);
    // the mode is the kind's: Display sets it, Open lays out for it after the base's Open and the background's scale
    d.op(
        &c,
        EE_SCREENS_SKOP_DIALOG_SHOW,
        i64::from(crate::screens::dialog::EE_SCREENS_DIALOG_QUESTION),
        0,
        &mut [0.0; 4],
    );
    assert_eq!(d.mode.get(), crate::screens::dialog::EE_SCREENS_DIALOG_QUESTION);
    assert!(!cpp.ops.borrow().iter().any(|o| o.0 == EE_SCREENS_DLG_MODE));
    cpp.ops.borrow_mut().clear();
    d.virt(&c, EE_SCREENS_SV_OPEN, 0, 0, &mut [0.0; 4]);
    assert_eq!(shell.ops.borrow()[0], (EE_SCREENS_SH_FORM_BASE + EE_SCREENS_SV_OPEN, 0, 0));
    assert_eq!(cpp.ops.borrow()[0].0, EE_SCREENS_DLG_SCALE_BACKGROUND);
    // Close posts the pressed button's callback, then closes; the free deletes the controls
    cpp.ops.borrow_mut().clear();
    shell.ops.borrow_mut().clear();
    d.virt(&c, EE_SCREENS_SV_CLOSE, 0, 0, &mut [0.0; 4]);
    assert_eq!(cpp.ops.borrow()[0].0, EE_SCREENS_DLG_POST_CLOSED);
    assert_eq!(shell.ops.borrow()[0], (EE_SCREENS_SH_FORM_BASE + EE_SCREENS_SV_CLOSE, 0, 0));
    d.free(&c);
    assert_eq!(cpp.ops.borrow().last().unwrap().0, EE_SCREENS_DLG_DELETE);
}

#[test]
fn a_button_press_marks_it_and_closes_its_form() {
    let shell = Shell::default();
    let c = Ctx { ctl: core::ptr::null_mut(), host: &shell };
    let b = DialogButton::new(true);
    assert_eq!(b.name(), c"EEUIFEBCancel");
    assert_eq!(DialogButton::new(false).name(), c"EEUIFEBOK");
    let mut f = [0.0; 4];
    b.op(&c, EE_SCREENS_SKOP_DIALOG_SET_CALLBACK, 0x11, 0x22, &mut f);
    b.op(&c, EE_SCREENS_SKOP_DIALOG_SET_CLOSE, 0x33, 0, &mut f);
    assert_eq!(b.op(&c, EE_SCREENS_SKOP_DIALOG_PRESSED, 0, 0, &mut f), 0);
    b.virt(&c, EE_SCREENS_SV_BUTTON_UP, 0, 0, &mut f);
    assert_eq!(
        *shell.ops.borrow(),
        vec![(EE_SCREENS_SH_BASE + EE_SCREENS_SV_BUTTON_UP, 0, 0), (EE_SCREENS_SH_CLOSE_FORM, 0x33, 0)]
    );
    assert_eq!(b.op(&c, EE_SCREENS_SKOP_DIALOG_PRESSED, 0, 0, &mut f), 1);
    assert_eq!(b.op(&c, EE_SCREENS_SKOP_DIALOG_CALLBACK, 0, 0, &mut f), 0x11);
    assert_eq!(b.op(&c, EE_SCREENS_SKOP_DIALOG_DATA, 0, 0, &mut f), 0x22);
    // the draw tints the copied rectangle before the base's draw
    shell.ops.borrow_mut().clear();
    b.virt(&c, EE_SCREENS_SV_DRAW_POLYGONS, 0, 0, &mut f);
    assert_eq!(shell.ops.borrow()[0], (EE_SCREENS_SH_FILL, 0, 0));
}
