use super::*;
use core::cell::RefCell;

/// A host recording its ops: the player 0x50, the selected group's first unit 0x60, its saved group's 0x61 (only
/// when `saved`), the index 0xd6 (group 2), the unit's portrait texture 0x70 at the cell (`cell`), Shift `shift`.
struct Host {
    ops: RefCell<Vec<(u32, i64, i64)>>,
    saved: bool,
    shift: bool,
    cell: Cell<(f32, f32)>,
}

impl ShellHost for Host {
    fn call(&self, _ctl: Cpp, op: u32, a: i64, b: i64, f: &mut [f32; 4]) -> i64 {
        self.ops.borrow_mut().push((op, a, b));
        match op {
            EE_SCREENS_SH_SG_NEW_PORTRAIT => 0x40,
            EE_SCREENS_SH_SG_SOUND => 0x30 + a,
            EE_SCREENS_SH_SG_PLAYER => 0x50,
            EE_SCREENS_SH_SG_FIRST_UNIT => match (b, self.saved) {
                (0, _) => 0x60,
                (_, true) => 0x61,
                _ => 0,
            },
            EE_SCREENS_SH_SG_INDEX => 0xd6,
            EE_SCREENS_SH_SG_SHIFT => i64::from(self.shift),
            EE_SCREENS_SH_SG_FACE => {
                f[0] = self.cell.get().0;
                f[1] = self.cell.get().1;
                0x70
            }
            _ => 0,
        }
    }
}

fn host(saved: bool, shift: bool) -> Host {
    Host { ops: RefCell::new(Vec::new()), saved, shift, cell: Cell::new((0.25, 0.5)) }
}

fn sent(h: &Host) -> Vec<(i64, i64)> {
    h.ops.borrow().iter().filter(|o| o.0 == EE_SCREENS_SH_SG_SEND).map(|o| (o.1, o.2)).collect()
}

#[test]
fn a_click_saves_the_selection_then_selects_the_group() {
    let h = host(false, false);
    let c = Ctx { ctl: core::ptr::null_mut(), host: &h };
    let k = make(EE_SCREENS_SK_SAVED_GROUP, &[]).unwrap();
    assert_eq!(k.name(), c"EEUIBSavedGroup");
    k.init(&c);
    let mut f = [0.0; 4];
    k.virt(&c, EE_SCREENS_SV_MOUSE_L_UP, 0, 0, &mut f);
    // empty: the selection saved as group 2, its first unit's portrait taken (dirty, referenced)
    assert_eq!(sent(&h), vec![(EE_SCREENS_SG_MSG_CREATE, 2)]);
    assert!(h.ops.borrow().contains(&(EE_SCREENS_SH_SG_FACE, 0x60, 0x40)));
    assert!(h.ops.borrow().contains(&(EE_SCREENS_SH_SG_TEXTURE_REF, 0x70, 1)));
    assert!(h.ops.borrow().contains(&(EE_SCREENS_SH_SG_PLAY, 0x30, 0)));
    assert_eq!(h.ops.borrow().last().copied(), Some((EE_SCREENS_SH_BASE + EE_SCREENS_SV_MOUSE_L_UP, 0, 0)));
    // now held: a click selects, a double click selects with the flag
    h.ops.borrow_mut().clear();
    k.virt(&c, EE_SCREENS_SV_MOUSE_L_UP, 0, 0, &mut f);
    k.virt(&c, EE_SCREENS_SV_MOUSE_L_DOUBLE, 0, 0, &mut f);
    assert_eq!(sent(&h), vec![(EE_SCREENS_SG_MSG_SELECT, 2), (EE_SCREENS_SG_MSG_SELECT, 2 | 1 << 8)]);
    // the copy draws the portrait with its texture
    h.ops.borrow_mut().clear();
    k.virt(&c, EE_SCREENS_SV_COPY_DATA, 0, 0, &mut f);
    k.virt(&c, EE_SCREENS_SV_DRAW_POLYGONS, 0, 0, &mut f);
    assert_eq!(h.ops.borrow()[0], (EE_SCREENS_SH_SG_SET_TEXTURE, 0x40, 0x70));
    assert!(h.ops.borrow().contains(&(EE_SCREENS_SH_SG_DRAW, 0x40, 0)));
    // a right click clears it once, then plays the up sound
    h.ops.borrow_mut().clear();
    k.virt(&c, EE_SCREENS_SV_MOUSE_R_UP, 0, 0, &mut f);
    k.virt(&c, EE_SCREENS_SV_MOUSE_R_UP, 0, 0, &mut f);
    assert_eq!(sent(&h), vec![(EE_SCREENS_SG_MSG_CLEAR, 2)]);
    assert!(h.ops.borrow().contains(&(EE_SCREENS_SH_SG_PLAY, 0x31, 0)));
}

#[test]
fn shift_adds_and_refresh_follows_the_players_group() {
    let h = host(true, true);
    let c = Ctx { ctl: core::ptr::null_mut(), host: &h };
    let k = make(EE_SCREENS_SK_SAVED_GROUP, &[]).unwrap();
    k.init(&c);
    let mut f = [0.0; 4];
    k.virt(&c, EE_SCREENS_SV_MOUSE_L_UP, 0, 0, &mut f);
    assert_eq!(sent(&h), vec![(EE_SCREENS_SG_MSG_ADD, 2)]);
    // a refresh takes the saved group's first unit (dirty again: the held texture compared after its release)
    h.ops.borrow_mut().clear();
    k.op(&c, EE_SCREENS_SKOP_SG_REFRESH, 0, 0, &mut f);
    assert!(h.ops.borrow().contains(&(EE_SCREENS_SH_SG_FACE, 0x61, 0x40)));
    assert!(h.ops.borrow().contains(&(EE_SCREENS_SH_BASE + EE_SCREENS_SV_SET_DIRTY, 1, 0)));
    // an empty group: no portrait, dirty once
    let h2 = host(false, false);
    let c2 = Ctx { ctl: core::ptr::null_mut(), host: &h2 };
    k.op(&c2, EE_SCREENS_SKOP_SG_REFRESH, 0, 0, &mut f);
    assert_eq!(h2.ops.borrow().iter().filter(|o| o.0 == EE_SCREENS_SH_BASE + EE_SCREENS_SV_SET_DIRTY).count(), 1);
    h2.ops.borrow_mut().clear();
    k.virt(&c2, EE_SCREENS_SV_COPY_DATA, 0, 0, &mut f);
    assert!(!h2.ops.borrow().iter().any(|o| o.0 == EE_SCREENS_SH_SG_SET_TEXTURE));
    // the free lets the portrait and the sounds go
    h2.ops.borrow_mut().clear();
    k.free(&c2);
    assert_eq!(h2.ops.borrow()[0], (EE_SCREENS_SH_BITMAP_RELEASE, 0x40, 0));
}
