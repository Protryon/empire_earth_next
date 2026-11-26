//! gamewin.rs's game answers over stand-in objects laid out as the game's (R6 WON GUI round 38): a list item's
//! members in place, a skinned button's and an image button's state.

use super::*;
use crate::btnobj::{ee_won_btn_free, ee_won_btn_new, EeWonBtnCall as B};
use crate::compobj::FORCE_GAME;
use crate::guihost::EeWonSkinBtn as Sb;
use crate::leafpaint::{EeWonLeaf, EeWonLeafOp as Lo};

#[test]
fn list_items_are_read_and_set_in_place() {
    FORCE_GAME.with(|f| f.set(true));
    // a ListItem: RefCount's head, then mSelected, mSelectable, mTop, mWidth, mHeight
    let mut item = [0u64; 4];
    let a = item.as_mut_ptr() as isize;
    let set = |op: Op, v: isize| assert_eq!(common_op(0, op, a, v, 0), Some(0));
    set(Op::ItemSetTop, 7);
    set(Op::ItemSetWidth, -3);
    set(Op::ItemSetHeight, 12);
    set(Op::ItemSetSelected, 5);
    set(Op::ItemSetSelectable, 0);
    assert_eq!(item[2], (7u32 as u64) << 32 | 1);
    assert_eq!(item[3], (12u64 << 32) | (-3i32 as u32 as u64));
    assert_eq!(common_op(0, Op::ItemTop, a, 0, 0), Some(7));
    assert_eq!(common_op(0, Op::ItemWidth, a, 0, 0), Some(-3));
    assert_eq!(common_op(0, Op::ItemHeight, a, 0, 0), Some(12));
    assert_eq!(common_op(0, Op::ItemSelected, a, 0, 0), Some(1));
    assert_eq!(common_op(0, Op::ItemSelectable, a, 0, 0), Some(0));
    // the leaf paints' item reads
    let leaf = EeWonLeaf::new(a as *mut c_void, None);
    assert_eq!(leaf.test_op(Lo::ItemSelected), 1);
    assert_eq!(leaf.test_op(Lo::ItemWidthAt), a + 24);
    assert_eq!(leaf.test_op(Lo::ItemHeightAt), a + 28);
    FORCE_GAME.with(|f| f.set(false));
}

#[test]
fn a_buttons_state_is_its_members_and_its_rust_button() {
    FORCE_GAME.with(|f| f.set(true));
    // a Component: RefCount's head, then compmem's block (the parent, the bounds, the bits)
    let mut comp = [0u64; 16];
    let c = comp.as_mut_ptr() as isize;
    let btn = ee_won_btn_new(c as *mut c_void, None);
    assert_eq!(common_op(c, Op::BtnOf, c, 0, 0), Some(btn as isize));
    assert_eq!(common_op(c, Op::BtnOf, c + 8, 0, 0), Some(0));
    crate::compmem::edit(c, |m| {
        m.parent = 0x1234;
        m.bounds = crate::guilayout::EeWonRect { x: 1, y: 2, width: 30, height: 40 };
        m.bits = 0x80 | 0x10;
    });
    // SAFETY: the live button
    unsafe { (*btn).call(B::SetButtonFlags, 0x24, 1, 0) };
    let sb = |w: Sb, b: isize, x: isize| common_op(c, Op::SkinBtn, w as isize, b, x).unwrap();
    assert_eq!(sb(Sb::IsDisabled, 0, 0), 1);
    assert_eq!(sb(Sb::HasFocus, 0, 0), 1);
    assert_eq!(sb(Sb::Parent, 0, 0), 0x1234);
    assert_eq!(sb(Sb::IsPressed, 0, 0), 1);
    assert_eq!(sb(Sb::IsChecked, 0, 0), 1);
    assert_eq!(sb(Sb::ButtonFlags, 0, 0), 0x24);
    assert_eq!(sb(Sb::IsDefaultButton, 0, 0), 0);
    assert_eq!(sb(Sb::FlagSet, c, 0x10), 1);
    assert_eq!(sb(Sb::FlagSet, c, 0x20), 0);
    assert_eq!(sb(Sb::SetButtonFlags, 0x20, 0), 0);
    assert_eq!(sb(Sb::ButtonFlags, 0, 0), 0x04);
    // the image button's paint reads
    let leaf = EeWonLeaf::new(c as *mut c_void, None);
    assert_eq!(leaf.test_op(Lo::BtnDisabled), 1);
    assert_eq!(leaf.test_op(Lo::BtnChecked), 1);
    assert_eq!(leaf.test_op(Lo::BtnPressed), 0);
    assert_eq!(leaf.test_op(Lo::BtnMouseOver), 0);
    assert_eq!(leaf.test_op(Lo::BtnWidth), 30);
    assert_eq!(leaf.test_op(Lo::BtnHeight), 40);
    // SAFETY: made above, not used again
    unsafe { ee_won_btn_free(btn) };
    FORCE_GAME.with(|f| f.set(false));
}
