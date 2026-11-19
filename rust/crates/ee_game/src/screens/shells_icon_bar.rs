//! The main game form's selected-units icon bar as a kind over the control shell (UI cluster round 4; once
//! `EEUICUnitIconBar`): its state and logic are `icon_bar.rs`' (over `EEUIRustControl`'s host, whose class ops the
//! shell answers for this kind, `EEUIShell.cpp`); the icon bitmap, the frame bitmap and the two sounds are the
//! shell's slots, made by the control factory (the controls table's bitmap) and released by the host.

use core::cell::Cell;
use core::ffi::{c_void, CStr};

use crate::screens::shells::*;

/// `EEUICUnitIconBar` (a control; no args).
pub const EE_SCREENS_SK_ICON_BAR: u32 = 0x81;

/// `Update()` (the refill from the selection).
pub const EE_SCREENS_SKOP_IB_UPDATE: u32 = 0x88;
/// `f[0..4]`: one icon's rectangle (left, top, right, bottom; the frame bitmap's).
pub const EE_SCREENS_SKOP_IB_ICON_RECT: u32 = 0x89;

/// The bar's slots let go (the sounds unloaded, the icon and frame bitmaps released).
pub const EE_SCREENS_SH_IB_FREE: u32 = 0xbc;
/// `f[0..2]`: the control's width and height (`GetRectangle()`'s).
pub const EE_SCREENS_SH_SIZE: u32 = 0xbd;

/// The bar's calls into `icon_bar_ffi.rs` (its state and the control).
pub(crate) trait IconBarFfi {
    fn new_state(&self, w: f32, h: f32, icon: &mut [f32; 4]) -> *mut c_void;
    fn call(&self, which: u32, state: *mut c_void, ctl: Cpp) -> i64;
}

/// The members `call` reaches: the destructor's release, CopyData, DrawPolygons, the releases, the move, Update.
pub(crate) const IB_FREE: u32 = 0;
pub(crate) const IB_COPY: u32 = 1;
pub(crate) const IB_DRAW: u32 = 2;
pub(crate) const IB_L_UP: u32 = 3;
pub(crate) const IB_R_UP: u32 = 4;
pub(crate) const IB_MOVE: u32 = 5;
pub(crate) const IB_UPDATE: u32 = 6;

#[cfg(not(test))]
pub(crate) struct Ffi;

#[cfg(not(test))]
impl IconBarFfi for Ffi {
    fn new_state(&self, w: f32, h: f32, icon: &mut [f32; 4]) -> *mut c_void {
        // SAFETY: four floats
        unsafe { crate::screens::icon_bar_ffi::ee_screens_icon_bar_new(w, h, icon.as_mut_ptr()) }
    }
    fn call(&self, which: u32, state: *mut c_void, ctl: Cpp) -> i64 {
        use crate::screens::icon_bar_ffi::*;
        // SAFETY: the bar's live state and control
        unsafe {
            match which {
                IB_FREE => {
                    ee_screens_icon_bar_free(state, ctl);
                    0
                }
                IB_COPY => ee_screens_icon_bar_copy_data(state, ctl),
                IB_DRAW => ee_screens_icon_bar_draw(state, ctl),
                IB_L_UP => ee_screens_icon_bar_mouse_l_up(state, ctl),
                IB_R_UP => ee_screens_icon_bar_mouse_r_up(state, ctl),
                IB_MOVE => ee_screens_icon_bar_mouse_move(state, ctl),
                _ => {
                    ee_screens_icon_bar_update(state, ctl);
                    0
                }
            }
        }
    }
}

pub(crate) struct IconBarKind<F: IconBarFfi> {
    state: Cell<*mut c_void>,
    icon: Cell<[f32; 4]>,
    ffi: F,
}

impl<F: IconBarFfi> IconBarKind<F> {
    pub(crate) fn new(ffi: F) -> Self {
        IconBarKind { state: Cell::new(core::ptr::null_mut()), icon: Cell::new([0.0; 4]), ffi }
    }
}

impl<F: IconBarFfi> Kind for IconBarKind<F> {
    fn name(&self) -> &'static CStr {
        c"EEUICUnitIconBar"
    }
    fn overrides(&self) -> u64 {
        (1 << EE_SCREENS_SV_COPY_DATA)
            | (1 << EE_SCREENS_SV_DRAW_POLYGONS)
            | (1 << EE_SCREENS_SV_MOUSE_L_DOWN)
            | (1 << EE_SCREENS_SV_MOUSE_L_UP)
            | (1 << EE_SCREENS_SV_MOUSE_MOVE)
            | (1 << EE_SCREENS_SV_MOUSE_R_DOWN)
            | (1 << EE_SCREENS_SV_MOUSE_R_UP)
    }
    fn init(&self, c: &Ctx) {
        let mut f = [0.0f32; 4];
        c.host.call(c.ctl, EE_SCREENS_SH_SIZE, 0, 0, &mut f);
        let mut icon = [0.0f32; 4];
        self.state.set(self.ffi.new_state(f[0], f[1], &mut icon));
        self.icon.set(icon);
    }
    fn virt(&self, c: &Ctx, v: u32, a: i64, b: i64, f: &mut [f32; 4]) -> i64 {
        let s = self.state.get();
        match v {
            EE_SCREENS_SV_COPY_DATA => self.ffi.call(IB_COPY, s, c.ctl),
            EE_SCREENS_SV_DRAW_POLYGONS => self.ffi.call(IB_DRAW, s, c.ctl),
            EE_SCREENS_SV_MOUSE_L_UP => self.ffi.call(IB_L_UP, s, c.ctl),
            EE_SCREENS_SV_MOUSE_R_UP => self.ffi.call(IB_R_UP, s, c.ctl),
            EE_SCREENS_SV_MOUSE_MOVE => self.ffi.call(IB_MOVE, s, c.ctl),
            // a press (left or right: ICF-folded) captures the mouse
            EE_SCREENS_SV_MOUSE_L_DOWN | EE_SCREENS_SV_MOUSE_R_DOWN => {
                c.call(crate::screens::shells_paint::EE_SCREENS_SH_CAPTURE_MOUSE, 0, 0);
                0
            }
            _ => c.base(v, a, b, f),
        }
    }
    fn op(&self, c: &Ctx, op: u32, _a: i64, _b: i64, f: &mut [f32; 4]) -> i64 {
        match op {
            EE_SCREENS_SKOP_IB_UPDATE => {
                self.ffi.call(IB_UPDATE, self.state.get(), c.ctl);
            }
            EE_SCREENS_SKOP_IB_ICON_RECT => *f = self.icon.get(),
            _ => {}
        }
        0
    }
    fn free(&self, c: &Ctx) {
        c.call(EE_SCREENS_SH_IB_FREE, 0, 0);
        let s = self.state.replace(core::ptr::null_mut());
        if !s.is_null() {
            self.ffi.call(IB_FREE, s, c.ctl);
        }
    }
}

pub(crate) fn make(kind: u32, _args: &[i64]) -> Option<Box<dyn Kind>> {
    match kind {
        #[cfg(not(test))]
        EE_SCREENS_SK_ICON_BAR => Some(Box::new(IconBarKind::new(Ffi))),
        _ => None,
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use core::cell::RefCell;

    #[derive(Default)]
    struct Rec {
        calls: RefCell<Vec<(u32, usize)>>,
        ops: RefCell<Vec<u32>>,
    }

    impl IconBarFfi for &Rec {
        fn new_state(&self, w: f32, h: f32, icon: &mut [f32; 4]) -> *mut c_void {
            *icon = [1.0, 2.0, w, h];
            0x77 as *mut c_void
        }
        fn call(&self, which: u32, state: *mut c_void, _ctl: Cpp) -> i64 {
            self.calls.borrow_mut().push((which, state as usize));
            i64::from(which)
        }
    }

    impl ShellHost for Rec {
        fn call(&self, _ctl: Cpp, op: u32, _a: i64, _b: i64, f: &mut [f32; 4]) -> i64 {
            self.ops.borrow_mut().push(op);
            if op == EE_SCREENS_SH_SIZE {
                f[0] = 300.0;
                f[1] = 40.0;
            }
            0
        }
    }

    #[test]
    fn the_bar_forwards_to_its_state_and_keeps_its_icon() {
        let r = Rec::default();
        let c = Ctx { ctl: core::ptr::null_mut(), host: &r };
        let k = IconBarKind::new(&r);
        assert_eq!(k.name(), c"EEUICUnitIconBar");
        k.init(&c);
        let mut f = [0.0; 4];
        k.op(&c, EE_SCREENS_SKOP_IB_ICON_RECT, 0, 0, &mut f);
        assert_eq!(f, [1.0, 2.0, 300.0, 40.0]);
        assert_eq!(k.virt(&c, EE_SCREENS_SV_DRAW_POLYGONS, 0, 0, &mut f), i64::from(IB_DRAW));
        assert_eq!(k.virt(&c, EE_SCREENS_SV_MOUSE_R_DOWN, 0, 0, &mut f), 0);
        k.op(&c, EE_SCREENS_SKOP_IB_UPDATE, 0, 0, &mut f);
        k.free(&c);
        k.free(&c);
        assert_eq!(*r.calls.borrow(), vec![(IB_DRAW, 0x77), (IB_UPDATE, 0x77), (IB_FREE, 0x77)]);
        assert!(r.ops.borrow().contains(&crate::screens::shells_paint::EE_SCREENS_SH_CAPTURE_MOUSE));
        assert_eq!(r.ops.borrow().iter().filter(|&&o| o == EE_SCREENS_SH_IB_FREE).count(), 2);
    }
}
