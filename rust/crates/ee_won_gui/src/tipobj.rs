//! WONGUI's ListAreaToolTip as a whole Rust object (R6 WON round 34): the tooltip a list box opens over an item
//! whose text it cuts off, the item drawn in full on a tooltip background. The original's ListAreaToolTip.cpp: the
//! paint (the list's colour scheme unless the tip has its own, the background, the item through the list's
//! ScrollPaint clipped and moved under the tip, the border), DoTip (the old tip closed unless the same item is
//! shown, the tip sized at least as wide as the list and opened over the item) and EndTip.
//!
//! The C++ `ListAreaToolTip` keeps the list (borrowed) and the item (a `SmartPtr`) the host reads and sets; its
//! methods are one-line forwarders and each of its host's ops (`EeWonTipOp`, WONGUI/ListAreaToolTip.cpp) one of the
//! original's statements. The paint's Graphics calls are guidraw.rs operations the host runs one at a time.

use std::ffi::c_void;

use crate::guidraw::{
    EeWonDrawOp, EE_WON_DRAW_FILL_RECT, EE_WON_DRAW_POP_CLIP, EE_WON_DRAW_PUSH_CLIP, EE_WON_DRAW_RECT,
    EE_WON_DRAW_SET_SYS_COLOR, EE_WON_DRAW_TRANSLATE,
};

/// `StandardColor_ToolTipBack` and `StandardColor_ToolTipText` (the C++ asserts them).
pub const EE_WON_TIP_COLOR_BACK: i32 = 9;
pub const EE_WON_TIP_COLOR_TEXT: i32 = 10;
/// The popup's flags (`PopupParams(this, 0x6247, list, x, y)`).
pub const EE_WON_TIP_POPUP_FLAGS: i32 = 0x6247;

/// The inward calls (the C++ class's methods).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonTipCall {
    /// `Paint(*(Graphics*)a)`.
    Paint = 1,
    /// `DoTip((ListItem*)a)`.
    DoTip,
    /// `EndTip()`.
    EndTip,
}

/// The host's calls (`t` the C++ `ListAreaToolTip*`, `g` the Graphics at `a` where they take one; one statement
/// each).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonTipOp {
    /// `t->Component::Paint(g)`.
    BasePaint = 1,
    /// `t->mItem.get() == (ListItem*)a` (0 or 1).
    ItemIs,
    /// `t->mItem.get() != NULL` (0 or 1).
    HasItem,
    /// `t->mParent != NULL` (0 or 1).
    HasParent,
    /// `t->mItem = (ListItem*)a`.
    SetItem,
    /// `t->mItem->mTop`.
    ItemTop,
    /// `t->mItem->mWidth`.
    ItemWidth,
    /// `t->mItem->mHeight`.
    ItemHeight,
    /// `g.mColorScheme` (a pointer).
    Scheme,
    /// `g.mColorScheme = (ColorScheme*)b`.
    SetScheme,
    /// `t->GetOwnColorScheme() != NULL` (0 or 1).
    HasOwnScheme,
    /// `t->mListArea->GetColorScheme()` (a pointer).
    ListScheme,
    /// `t->mListArea->GetLeftPad()`.
    ListLeftPad,
    /// `t->mListArea->GetRightPad()`.
    ListRightPad,
    /// `t->mListArea->GetTopPad()`.
    ListTopPad,
    /// `t->mListArea->GetVertOffset()`.
    ListVertOffset,
    /// `t->mListArea->Width()`.
    ListWidth,
    /// `t->Width()`.
    Width,
    /// `t->Height()`.
    Height,
    /// one draw operation (the `EeWonDrawOp` at `b`) on `g`.
    Draw,
    /// `t->mListArea->ScrollPaint(g)`.
    ScrollPaint,
    /// `t->mListArea->LocalToRoot(p[0], p[1])` with `p` the two ints at `a`.
    ListLocalToRoot,
    /// `t->SetSize(a, b)`.
    SetSize,
    /// `t->mListArea->GetRoot()->OpenPopup(PopupParams(t, 0x6247, t->mListArea, a, b))`.
    OpenPopup,
    /// `t->mListArea->GetRoot()->ClosePopup(t)`.
    ClosePopup,
}

/// The host: `me` the C++ object, `op`, two arguments.
pub type EeWonTipHostFn = Option<unsafe extern "C" fn(me: *mut c_void, op: EeWonTipOp, a: isize, b: isize) -> isize>;

/// The Rust ListAreaToolTip over its C++ object (no state of its own: the list and the item are the C++ members).
pub struct EeWonTip {
    me: *mut c_void,
    host: EeWonTipHostFn,
}

fn draw(kind: i32, a: i32, b: i32, c: i32, d: i32) -> EeWonDrawOp {
    EeWonDrawOp { kind, a, b, c, d, e: 0, f: 0, g: 0, h: 0 }
}

impl EeWonTip {
    pub fn new(me: *mut c_void, host: EeWonTipHostFn) -> Self {
        EeWonTip { me, host }
    }

    fn op(&self, o: EeWonTipOp, a: isize, b: isize) -> isize {
        // the Graphics' own in the game (gfxrun.rs: TextListItem.cpp's cases went, R6 WON GUI round 38)
        use crate::guihost::EeWonGuiOp as G;
        let g = match o {
            EeWonTipOp::Draw => Some(G::Draw),
            EeWonTipOp::Scheme => Some(G::CtScheme),
            EeWonTipOp::SetScheme => Some(G::CtSetScheme),
            _ => None,
        };
        if let Some(r) = g.filter(|_| crate::compobj::is_game()).and_then(|g| crate::gfxrun::game_op(g, a, b)) {
            return r;
        }
        match self.host {
            // SAFETY: the host was given with this object
            Some(h) => unsafe { h(self.me, o, a, b) },
            None => 0,
        }
    }
    fn get(&self, o: EeWonTipOp) -> i32 {
        self.op(o, 0, 0) as i32
    }
    fn draw(&self, g: isize, o: EeWonDrawOp) {
        self.op(EeWonTipOp::Draw, g, &o as *const EeWonDrawOp as isize);
    }

    /// `Paint`: the tipped item in full on a tooltip background with a tooltip-coloured border, in the list box's
    /// colour scheme unless the tip has its own.
    pub fn paint(&self, g: isize) {
        use EeWonTipOp::*;
        self.op(BasePaint, g, 0);
        if self.get(HasItem) == 0 {
            return;
        }
        let scheme = self.op(Scheme, g, 0);
        if self.get(HasOwnScheme) == 0 {
            let list = self.get_ptr(ListScheme);
            self.op(SetScheme, g, list);
        }
        let x = self.get(ListLeftPad);
        let y = 2i32.wrapping_sub(self.get(ItemTop));
        let (w, h) = (self.get(Width), self.get(Height));
        self.draw(g, draw(EE_WON_DRAW_SET_SYS_COLOR, EE_WON_TIP_COLOR_BACK, 0, 0, 0));
        self.draw(g, draw(EE_WON_DRAW_FILL_RECT, 0, 0, w, h));
        let (w, h) = (self.get(Width), self.get(Height));
        self.draw(g, draw(EE_WON_DRAW_PUSH_CLIP, 0, 2, w, h.wrapping_sub(4)));
        self.draw(g, draw(EE_WON_DRAW_TRANSLATE, x, y, 0, 0));
        self.op(ScrollPaint, g, 0);
        self.draw(g, draw(EE_WON_DRAW_TRANSLATE, x.wrapping_neg(), y.wrapping_neg(), 0, 0));
        self.draw(g, draw(EE_WON_DRAW_POP_CLIP, 0, 0, 0, 0));
        self.draw(g, draw(EE_WON_DRAW_SET_SYS_COLOR, EE_WON_TIP_COLOR_TEXT, 0, 0, 0));
        let (w, h) = (self.get(Width), self.get(Height));
        self.draw(g, draw(EE_WON_DRAW_RECT, 0, 0, w, h));
        self.op(SetScheme, g, scheme);
    }
    fn get_ptr(&self, o: EeWonTipOp) -> isize {
        self.op(o, 0, 0)
    }

    /// `DoTip`: the tip opened over a list item, at least as wide as the list box; the old tip closed first
    /// unless the same item is shown, and nothing more for no item.
    pub fn do_tip(&self, item: isize) {
        use EeWonTipOp::*;
        if self.op(ItemIs, item, 0) != 0 && self.get(HasItem) != 0 && self.get(HasParent) != 0 {
            return;
        }
        self.end_tip();
        self.op(SetItem, item, 0);
        if self.get(HasItem) == 0 {
            return;
        }
        let mut p = [0i32, self.get(ListTopPad).wrapping_sub(self.get(ListVertOffset))];
        self.op(ListLocalToRoot, p.as_mut_ptr() as isize, 0);
        p[1] = p[1].wrapping_add(self.get(ItemTop).wrapping_sub(2));
        let mut width = self.get(ItemWidth).wrapping_add(self.get(ListLeftPad)).wrapping_add(self.get(ListRightPad));
        let list_width = self.get(ListWidth);
        if width < list_width {
            width = list_width;
        }
        self.op(SetSize, width as isize, self.get(ItemHeight).wrapping_add(4) as isize);
        self.op(OpenPopup, p[0] as isize, p[1] as isize);
    }

    /// `EndTip`: the tip's popup closed if it is open.
    pub fn end_tip(&self) {
        if self.get(EeWonTipOp::HasParent) != 0 {
            self.op(EeWonTipOp::ClosePopup, 0, 0);
        }
    }

    fn call(&self, c: EeWonTipCall, a: isize) {
        match c {
            EeWonTipCall::Paint => self.paint(a),
            EeWonTipCall::DoTip => self.do_tip(a),
            EeWonTipCall::EndTip => self.end_tip(),
        }
    }
}

/// One of the class's methods (`EeWonTipCall`) on the C++ tip `me` through its host.
///
/// # Safety
/// `host` answers `me`'s ops; the argument as the call says.
#[no_mangle]
pub unsafe extern "C" fn ee_won_tip_call(me: *mut c_void, host: EeWonTipHostFn, c: EeWonTipCall, a: isize) {
    EeWonTip::new(me, host).call(c, a)
}

#[cfg(test)]
#[path = "tipobj_tests.rs"]
mod tests;
