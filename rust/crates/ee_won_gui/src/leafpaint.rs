//! The measures and paints of three WONGUI leaf classes in Rust (R6 WON round 34): ImageButton (its desired size,
//! the largest of its five state images; its paint, the state's image or the normal one, stretched to the button
//! when it stretches), TextListItem (CalcDimensions: the list's font and text colour taken when unset, the size from
//! the text and the image; Paint: guidraw.rs's text row over the item's and the list's members) and MenuItem
//! (CalcDimensions: the text item's plus room for the icon and the submenu arrow; Paint: guidraw.rs's menu item).
//!
//! The C++ classes keep their members (the images, the text, the font, the colour, the sizes); the methods are one
//! call here each, and each host op (`EeWonLeafOp`; WONGUI/ImageButton.cpp, TextListItem.cpp, MenuBehavior.cpp) is
//! one of the original's statements or reads. A paint's Graphics calls are guidraw.rs operations the host runs one
//! at a time with the item's font, text and image as the slots.

use std::ffi::c_void;

use crate::guidraw::{
    menu_item, text_item_size, text_row, EeWonDrawOp, EeWonMenuItemPaint, EeWonSelColor, EeWonTextRowPaint,
};
use crate::guiwin::menu_item_size;

/// `ImageButtonType`: the five state images.
pub const EE_WON_IMGBTN_NORMAL: isize = 0;
pub const EE_WON_IMGBTN_OVER: isize = 1;
pub const EE_WON_IMGBTN_DOWN: isize = 2;
pub const EE_WON_IMGBTN_DISABLED: isize = 3;
pub const EE_WON_IMGBTN_CHECKED: isize = 4;

/// The inward calls.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonLeafCall {
    /// `ImageButton::GetDesiredSize(*(int*)a, *(int*)b)`.
    ImageButtonSize = 1,
    /// `ImageButton::Paint(*(Graphics*)a)`.
    ImageButtonPaint,
    /// `TextListItem::CalcDimensions((ListArea*)a)`.
    TextItemMeasure,
    /// `TextListItem::Paint(*(Graphics*)a, (ListArea*)b)`.
    TextItemPaint,
    /// `MenuItem::CalcDimensions((ListArea*)a)`.
    MenuItemMeasure,
    /// `MenuItem::Paint(*(Graphics*)a, (ListArea*)b)`.
    MenuItemPaint,
}

/// The host's calls (`t` the C++ object: an ImageButton for the `Btn` ops, a TextListItem for the `Item` ops, a
/// MenuItem for the `Menu` ops; `g` the Graphics at `a`, `l` the ListArea at `a` where they take one).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonLeafOp {
    /// `t->mImages[a].get() != NULL` (0 or 1).
    BtnHasImage = 1,
    /// `t->mImages[a]->GetWidth()`.
    BtnImageWidth,
    /// `t->mImages[a]->GetHeight()`.
    BtnImageHeight,
    /// `t->Component::Paint(g)`.
    BtnBasePaint,
    /// `t->IsDisabled()` (0 or 1).
    BtnDisabled,
    /// `t->IsChecked()` (0 or 1).
    BtnChecked,
    /// `t->IsPressed()` (0 or 1).
    BtnPressed,
    /// `t->IsMouseOver()` (0 or 1).
    BtnMouseOver,
    /// `t->mStretch` (0 or 1).
    BtnStretch,
    /// `t->Width()`.
    BtnWidth,
    /// `t->Height()`.
    BtnHeight,
    /// `t->mImages[a]->SetSize(t->Width(), t->Height())`.
    BtnImageFit,
    /// `g.DrawImage(t->mImages[b].get(), 0, 0)`.
    BtnDrawImage,
    /// `t->mFont.get() != NULL` (0 or 1).
    ItemHasFont,
    /// `t->mFont = l->mFont`.
    ItemTakeListFont,
    /// `t->mColor`.
    ItemColor,
    /// `t->mColor = l->GetTextColor()`.
    ItemTakeListColor,
    /// `t->mImage.get() != NULL` (0 or 1).
    ItemHasImage,
    /// `t->mImage->GetWidth()`.
    ItemImageWidth,
    /// `t->mImage->GetHeight()`.
    ItemImageHeight,
    /// `t->mFont->GetStringWidth(t->mText)`.
    ItemTextWidth,
    /// `t->mFont->GetHeight()`.
    ItemFontHeight,
    /// `&t->mWidth` (the item's size the measure writes).
    ItemWidthAt,
    /// `&t->mHeight`.
    ItemHeightAt,
    /// `t->mSelected` (0 or 1).
    ItemSelected,
    /// `l->ListFlagSet(b)` (0 or 1).
    ItemListFlag,
    /// `WONDrawSelOf(l->GetSelColor())` into the `EeWonSelColor` at `b`.
    ItemListSel,
    /// `l->GetPaintWidth()`.
    ItemListPaintWidth,
    /// one operation (the `EeWonDrawOp` at `b`) on `g` with the item's font, text and image as the slots.
    ItemDraw,
    /// `t->mDisabled` (0 or 1).
    MenuDisabled,
    /// `t->mSubMenu.get() != NULL` (0 or 1).
    MenuHasSubMenu,
    /// `WONDrawSelOf(l->mSelColor)` into the `EeWonSelColor` at `b`.
    MenuListSel,
    /// `l->GetViewWidth()`.
    MenuListViewWidth,
}

/// The host: `me` the C++ object, `op`, two arguments.
pub type EeWonLeafHostFn = Option<unsafe extern "C" fn(me: *mut c_void, op: EeWonLeafOp, a: isize, b: isize) -> isize>;

/// One C++ object and its host.
pub struct EeWonLeaf {
    me: *mut c_void,
    host: EeWonLeafHostFn,
}

impl EeWonLeaf {
    pub fn new(me: *mut c_void, host: EeWonLeafHostFn) -> Self {
        EeWonLeaf { me, host }
    }
    fn op(&self, o: EeWonLeafOp, a: isize, b: isize) -> isize {
        match self.host {
            // SAFETY: the host was given with this object
            Some(h) => unsafe { h(self.me, o, a, b) },
            None => 0,
        }
    }
    fn get(&self, o: EeWonLeafOp, a: isize) -> i32 {
        self.op(o, a, 0) as i32
    }
    fn yes(&self, o: EeWonLeafOp, a: isize) -> bool {
        self.op(o, a, 0) != 0
    }

    /// `ImageButton::GetDesiredSize`: the largest width and height among the five state images.
    pub fn image_button_size(&self) -> (i32, i32) {
        use EeWonLeafOp::*;
        let (mut w, mut h) = (0, 0);
        for i in 0..5 {
            if self.yes(BtnHasImage, i) {
                w = w.max(self.get(BtnImageWidth, i));
                h = h.max(self.get(BtnImageHeight, i));
            }
        }
        (w, h)
    }

    /// `ImageButton::Paint`: the image of the state (disabled, checked if it has one, down, over), else the normal
    /// one, sized to the button first when it stretches.
    pub fn image_button_paint(&self, g: isize) {
        use EeWonLeafOp::*;
        self.op(BtnBasePaint, g, 0);
        let pick = if self.yes(BtnDisabled, 0) {
            Some(EE_WON_IMGBTN_DISABLED)
        } else if self.yes(BtnChecked, 0) && self.yes(BtnHasImage, EE_WON_IMGBTN_CHECKED) {
            Some(EE_WON_IMGBTN_CHECKED)
        } else if self.yes(BtnPressed, 0) {
            Some(EE_WON_IMGBTN_DOWN)
        } else if self.yes(BtnMouseOver, 0) {
            Some(EE_WON_IMGBTN_OVER)
        } else {
            None
        };
        let mut image = pick.filter(|&i| self.yes(BtnHasImage, i));
        if image.is_none() && self.yes(BtnHasImage, EE_WON_IMGBTN_NORMAL) {
            image = Some(EE_WON_IMGBTN_NORMAL);
        }
        let Some(i) = image else { return };
        if self.yes(BtnStretch, 0)
            && (self.get(BtnImageWidth, i) != self.get(BtnWidth, 0)
                || self.get(BtnImageHeight, i) != self.get(BtnHeight, 0))
        {
            self.op(BtnImageFit, i, 0);
        }
        self.op(BtnDrawImage, g, i);
    }

    /// `TextListItem::CalcDimensions`: the list's font and text colour when the item has none, then the size.
    pub fn text_item_measure(&self, list: isize) {
        use EeWonLeafOp::*;
        if !self.yes(ItemHasFont, 0) {
            self.op(ItemTakeListFont, list, 0);
        }
        if self.op(ItemColor, 0, 0) == -1 {
            self.op(ItemTakeListColor, list, 0);
        }
        let image = self.yes(ItemHasImage, 0);
        let (iw, ih) = if image { (self.get(ItemImageWidth, 0), self.get(ItemImageHeight, 0)) } else { (0, 0) };
        let (w, h) = text_item_size(self.get(ItemTextWidth, 0), self.get(ItemFontHeight, 0), image, iw, ih, false);
        self.set_size(w, h);
    }
    fn set_size(&self, w: i32, h: i32) {
        // SAFETY: the item's own members, live while its method runs
        unsafe {
            *(self.op(EeWonLeafOp::ItemWidthAt, 0, 0) as *mut i32) = w;
            *(self.op(EeWonLeafOp::ItemHeightAt, 0, 0) as *mut i32) = h;
        }
    }
    fn size(&self) -> (i32, i32) {
        // SAFETY: as set_size
        unsafe {
            (
                *(self.op(EeWonLeafOp::ItemWidthAt, 0, 0) as *const i32),
                *(self.op(EeWonLeafOp::ItemHeightAt, 0, 0) as *const i32),
            )
        }
    }
    fn sel(&self, o: EeWonLeafOp, list: isize) -> EeWonSelColor {
        let mut s = EeWonSelColor { background: 0, foreground: 0, mode: 0 };
        self.op(o, list, &mut s as *mut EeWonSelColor as isize);
        s
    }
    fn run(&self, g: isize, ops: &[EeWonDrawOp]) {
        for o in ops {
            self.op(EeWonLeafOp::ItemDraw, g, o as *const EeWonDrawOp as isize);
        }
    }

    /// `TextListItem::Paint`: the selection behind a selected item, the image, the text centred on the row.
    pub fn text_item_paint(&self, g: isize, list: isize) {
        use EeWonLeafOp::*;
        let has_image = self.yes(ItemHasImage, 0);
        let (width, height) = self.size();
        let r = EeWonTextRowPaint {
            selected: self.yes(ItemSelected, 0),
            full_row_select: self.op(ItemListFlag, list, 0x10) != 0,
            no_image_select: self.op(ItemListFlag, list, 0x20) != 0,
            has_image,
            sel: self.sel(ItemListSel, list),
            color: self.op(ItemColor, 0, 0) as u32,
            paint_width: self.get(ItemListPaintWidth, list),
            width,
            height,
            image_width: if has_image { self.get(ItemImageWidth, 0) } else { 0 },
            image_height: if has_image { self.get(ItemImageHeight, 0) } else { 0 },
            font_height: self.get(ItemFontHeight, 0),
        };
        self.run(g, &text_row(&r));
    }

    /// `MenuItem::CalcDimensions`: the text item's size, 32 wider and 4 taller.
    pub fn menu_item_measure(&self, list: isize) {
        self.text_item_measure(list);
        let (w, h) = self.size();
        let (w, h) = menu_item_size(w, h);
        self.set_size(w, h);
    }

    /// `MenuItem::Paint`: the icon, the selection with an icon box, the text (embossed grey when disabled), the
    /// submenu arrow.
    pub fn menu_item_paint(&self, g: isize, list: isize) {
        use EeWonLeafOp::*;
        let has_image = self.yes(ItemHasImage, 0);
        let (width, height) = self.size();
        let r = EeWonMenuItemPaint {
            selected: self.yes(ItemSelected, 0),
            full_row_select: self.op(ItemListFlag, list, 0x10) != 0,
            has_image,
            disabled: self.yes(MenuDisabled, 0),
            has_submenu: self.yes(MenuHasSubMenu, 0),
            sel: self.sel(MenuListSel, list),
            color: self.op(ItemColor, 0, 0) as u32,
            paint_width: self.get(ItemListPaintWidth, list),
            width,
            height,
            image_height: if has_image { self.get(ItemImageHeight, 0) } else { 0 },
            font_height: self.get(ItemFontHeight, 0),
            view_width: self.get(MenuListViewWidth, list),
        };
        self.run(g, &menu_item(&r));
    }

    fn call(&self, c: EeWonLeafCall, a: isize, b: isize) {
        match c {
            EeWonLeafCall::ImageButtonSize => {
                let (w, h) = self.image_button_size();
                // SAFETY: the width at `a`, the height at `b` (the call's contract)
                unsafe {
                    *(a as *mut i32) = w;
                    *(b as *mut i32) = h;
                }
            }
            EeWonLeafCall::ImageButtonPaint => self.image_button_paint(a),
            EeWonLeafCall::TextItemMeasure => self.text_item_measure(a),
            EeWonLeafCall::TextItemPaint => self.text_item_paint(a, b),
            EeWonLeafCall::MenuItemMeasure => self.menu_item_measure(a),
            EeWonLeafCall::MenuItemPaint => self.menu_item_paint(a, b),
        }
    }
}

/// One of the classes' methods (`EeWonLeafCall`) on the C++ object `me` through its host.
///
/// # Safety
/// `host` answers `me`'s ops; the arguments as the call says.
#[no_mangle]
pub unsafe extern "C" fn ee_won_leaf_call(
    me: *mut c_void,
    host: EeWonLeafHostFn,
    c: EeWonLeafCall,
    a: isize,
    b: isize,
) {
    EeWonLeaf::new(me, host).call(c, a, b)
}

#[cfg(test)]
#[path = "leafpaint_tests.rs"]
mod tests;
