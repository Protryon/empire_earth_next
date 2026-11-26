//! The measures and paints of three WONGUI leaf classes in Rust (R6 WON round 34): ImageButton (its desired size,
//! the largest of its five state images; its paint, the state's image or the normal one, stretched to the button
//! when it stretches), TextListItem (CalcDimensions: the list's font and text colour taken when unset, the size from
//! the text and the image; Paint: guidraw.rs's text row over the item's and the list's members) and MenuItem
//! (CalcDimensions: the text item's plus room for the icon and the submenu arrow; Paint: guidraw.rs's menu item);
//! ComponentListItem (its size, viewport and place: its component's); and Background::Paint (guidraw.rs's background over the background's members, its component's call made here).
//!
//! The C++ classes keep their members (the images, the text, the font, the colour, the sizes); the methods are one
//! call here each, and each host op (`EeWonLeafOp`; WONGUI/ImageButton.cpp, TextListItem.cpp, MenuBehavior.cpp) is
//! one of the original's statements or reads. A paint's Graphics calls are guidraw.rs operations the host runs one
//! at a time with the item's font, text and image as the slots.

use std::ffi::c_void;

use crate::guidraw::{
    background, menu_item, text_item_size, text_row, EeWonBackground, EeWonDrawOp, EeWonMenuItemPaint, EeWonSelColor,
    EeWonTextRowPaint, EE_WON_DRAW_CALL,
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
    /// `ComponentListItem::CalcDimensions(list)`.
    ComponentItemMeasure,
    /// `ComponentListItem::SetViewport(list, (Container*)b)`.
    ComponentItemViewport,
    /// `ComponentListItem::SetPos(a, b)`.
    ComponentItemSetPos,
    /// `ComponentListItem::Paint(*(Graphics*)a, (ListArea*)b)`.
    ComponentItemPaint,
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
    /// `t->mImage.get() != NULL` (0 or 1; `t` a Background).
    BgHasImage,
    /// `t->mStretchImage` (0 or 1).
    BgStretch,
    /// `t->mUseOffsets` (0 or 1).
    BgUseOffsets,
    /// `t->mComponent.get() != NULL` (0 or 1).
    BgHasComponent,
    /// `t->mImage->GetWidth()`.
    BgImageWidth,
    /// `t->mImage->GetHeight()`.
    BgImageHeight,
    /// `t->mColor`.
    BgColor,
    /// `t->mBorderColor`.
    BgBorderColor,
    /// one operation (the `EeWonDrawOp` at `b`) on `g` with the background's image as slot 0.
    BgDraw,
    /// `t->mComponent->SetSize(a, b)`.
    BgCompSetSize,
    /// `t->mComponent->Invalidate()`.
    BgCompInvalidate,
    /// `t->mComponent->Paint(g)`.
    BgCompPaint,
    /// `t->mComponent.get() != NULL` (0 or 1; `t` a ComponentListItem, whose `ItemWidthAt` and `ItemHeightAt` the
    /// host answers too).
    CompItemHasComponent,
    /// `t->mComponent->Width()`.
    CompItemWidth,
    /// `t->mComponent->Height()`.
    CompItemHeight,
    /// `t->mComponent->mParent = (Container*)a`.
    CompItemSetParent,
    /// `t->mComponent->AddedToParent()`.
    CompItemAdded,
    /// `t->ListItem::SetPos(a, b)`.
    CompItemBaseSetPos,
    /// `t->mComponent->SetPos(a, b)`.
    CompItemSetPos,
    /// `t->mIsSizable` (0 or 1).
    CompItemSizable,
    /// `((ListArea*)a)->GetPaintWidth()`.
    CompItemListPaintWidth,
    /// `t->mComponent->SetSize(a, b)`.
    CompItemSetSize,
    /// `t->mComponent->MarkInvalid()`.
    CompItemMarkInvalid,
    /// `t->mComponent->Paint(*(Graphics*)a)`.
    CompItemPaint,
}

/// The host: `me` the C++ object, `op`, two arguments.
/// The offset of ListItem's members ([`EeWonListItemRec`]) in a C++ `ListItem` (after RefCount's head).
pub const EE_WON_LIST_ITEM_AT: usize = 16;

/// ListItem's members, in their C++ order (`mSelected` .. `mHeight`), read and written in place in the game (R6 WON
/// GUI round 38; TextListItem.cpp asserts the layout). A driver's world answers the ops instead.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeWonListItemRec {
    /// `mSelected` (a `bool`).
    pub selected: u8,
    /// `mSelectable` (a `bool`).
    pub selectable: u8,
    /// `mTop`.
    pub top: i32,
    /// `mWidth`.
    pub width: i32,
    /// `mHeight`.
    pub height: i32,
}
const _: () = assert!(
    std::mem::offset_of!(EeWonListItemRec, top) == 4
        && std::mem::offset_of!(EeWonListItemRec, width) == 8
        && std::mem::offset_of!(EeWonListItemRec, height) == 12
);

/// The members of the game's list item `item` (a `ListItem*`).
pub fn item_rec(item: isize) -> *mut EeWonListItemRec {
    (item as usize).wrapping_add(EE_WON_LIST_ITEM_AT) as *mut EeWonListItemRec
}

/// The offset of `ImageButton::mImages` (five `SmartPtr<Image>`, each the image's pointer, then `mStretch`) in the
/// C++ object (Button.cpp registers it; 0 in a driver's world).
static IMGBTN_AT: std::sync::atomic::AtomicUsize = std::sync::atomic::AtomicUsize::new(0);

/// `offsetof(ImageButton, mImages)` (Button.cpp at static initialisation; R6 WON GUI round 39).
#[no_mangle]
pub extern "C" fn ee_won_leaf_imgbtn_register(at: usize) {
    IMGBTN_AT.store(at, std::sync::atomic::Ordering::Release);
}

/// Where the game's text list items, menu items, component list items, backgrounds and lists keep the members the
/// measures and paints read (offsets in a `TextListItem`, a `MenuItem`, a `ComponentListItem`, a `Background`, a
/// `ListArea` and a `Font`; Button.cpp registers them; R6 WON GUI round 40).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct EeWonLeafAt {
    /// `TextListItem::mText` (a GUIString: its text's pointer)
    pub text: usize,
    /// `TextListItem::mColor` (a C `long`)
    pub color: usize,
    /// `TextListItem::mFont`, `mImage` (SmartPtrs: one pointer each)
    pub font: usize,
    pub image: usize,
    /// `MenuItem::mSubMenu` (a SmartPtr), `mDisabled` (a bool)
    pub sub_menu: usize,
    pub disabled: usize,
    /// `ListArea::mFont`, `mTextColor` (an int), `mSelColor` (an `EeWonSelColor`), `mRust`, `ScrollArea::mPane`
    pub list_font: usize,
    pub list_text_color: usize,
    pub list_sel: usize,
    pub list_rust: usize,
    pub list_pane: usize,
    /// `Font::mRust`
    pub font_rust: usize,
    /// `ComponentListItem::mComponent` (a SmartPtr), `mIsSizable` (a bool)
    pub comp_item_component: usize,
    pub comp_item_sizable: usize,
    /// `Background::mImage`, `mComponent` (SmartPtrs), `mColor`, `mBorderColor` (ints), `mUseOffsets`,
    /// `mStretchImage` (bools)
    pub bg_image: usize,
    pub bg_component: usize,
    pub bg_color: usize,
    pub bg_border_color: usize,
    pub bg_use_offsets: usize,
    pub bg_stretch: usize,
}
static LEAF_AT: std::sync::Mutex<Option<EeWonLeafAt>> = std::sync::Mutex::new(None);

/// Registers the text items' member offsets (Button.cpp at static initialisation).
///
/// # Safety
/// `at` points at the offsets.
#[no_mangle]
pub unsafe extern "C" fn ee_won_leaf_members_register(at: *const EeWonLeafAt) {
    if let Ok(mut s) = LEAF_AT.lock() {
        // SAFETY: the contract
        *s = Some(unsafe { *at });
    }
}

/// A word at `p + off` (a pointer member or a SmartPtr).
///
/// # Safety
/// `p + off` holds a pointer-sized member of a live object.
unsafe fn word(p: isize, off: usize) -> isize {
    // SAFETY: the contract
    unsafe { *((p as usize + off) as *const isize) }
}

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
        if let Some(r) = self.game_op(o) {
            return r;
        }
        if let Some(r) = self.image_button_op(o, a, b) {
            return r;
        }
        if let Some(r) = self.text_item_op(o, a, b) {
            return r;
        }
        if let Some(r) = self.member_op(o, a, b) {
            return r;
        }
        match self.host {
            // SAFETY: the host was given with this object
            Some(h) => unsafe { h(self.me, o, a, b) },
            None => 0,
        }
    }
    /// The game's answers that are no C++ statement (R6 WON GUI round 38): an image button's state (its component's
    /// members, its Rust button) and a list item's members in place. None in a driver's world.
    fn game_op(&self, o: EeWonLeafOp) -> Option<isize> {
        use crate::compobj::EE_WON_COMP_FLAG_DISABLED;
        use EeWonLeafOp::*;
        if !matches!(
            o,
            BtnDisabled
                | BtnChecked
                | BtnPressed
                | BtnMouseOver
                | BtnWidth
                | BtnHeight
                | ItemWidthAt
                | ItemHeightAt
                | ItemSelected
        ) || !crate::compobj::is_game()
        {
            return None;
        }
        let me = self.me as isize;
        let item = item_rec(me);
        // SAFETY: the game's live button (the Btn ops' `t`, its Rust button registered by btnobj.rs)
        let btn =
            || unsafe { crate::registry::get::<crate::btnobj::EeWonButton>(me, crate::registry::EeWonGuiKind::Button) };
        let flags = || btn().map_or(0, |b| b.field(crate::btnobj::EeWonBtnField::Flags as i32) as u32);
        Some(match o {
            BtnDisabled => isize::from(crate::compmem::bits(me) & EE_WON_COMP_FLAG_DISABLED != 0),
            BtnChecked => isize::from(flags() & 0x04 != 0),
            BtnPressed => isize::from(flags() & 0x20 != 0),
            BtnMouseOver => isize::from(btn().is_some_and(|b| b.is_mouse_over())),
            BtnWidth => crate::compmem::bounds(me).width as isize,
            BtnHeight => crate::compmem::bounds(me).height as isize,
            // SAFETY: the game's live list item (the Item ops' `t`)
            ItemWidthAt => (unsafe { std::ptr::addr_of_mut!((*item).width) }) as isize,
            // SAFETY: as above
            ItemHeightAt => (unsafe { std::ptr::addr_of_mut!((*item).height) }) as isize,
            // SAFETY: as above
            _ => isize::from(unsafe { (*item).selected } != 0),
        })
    }
    /// A text list item's and a menu item's reads and the list's (`a` the `ListArea*`) in the game (R6 WON GUI round
    /// 40): the members in place at their registered offsets, the font's measures its platform font's, the list's
    /// flags and paint width its Rust list's, the view width its pane's. None in a driver's world.
    fn text_item_op(&self, o: EeWonLeafOp, a: isize, b: isize) -> Option<isize> {
        use EeWonLeafOp::*;
        if !matches!(
            o,
            ItemHasFont
                | ItemTakeListFont
                | ItemColor
                | ItemTakeListColor
                | ItemHasImage
                | ItemImageWidth
                | ItemImageHeight
                | ItemTextWidth
                | ItemFontHeight
                | ItemListFlag
                | ItemListSel
                | ItemListPaintWidth
                | MenuDisabled
                | MenuHasSubMenu
                | MenuListSel
                | MenuListViewWidth
        ) || !crate::compobj::is_game()
        {
            return None;
        }
        let at = (*LEAF_AT.lock().ok()?)?;
        let me = self.me as isize;
        let m = |off: usize| (me as usize + off) as *mut u8;
        // SAFETY (the reads below): the game's live item `me` and list `a` (the ops' contracts), each member at its
        // registered offset; the item's font and image are live (its SmartPtrs hold them)
        let font = || unsafe { word(word(me, at.font), at.font_rust) } as *const crate::platfont::EeWonPlatFont;
        // SAFETY: as above
        let image = || unsafe { crate::imgobj::image_of(word(me, at.image)) };
        // SAFETY: as above
        let list = |c: crate::listobj::EeWonListCall| unsafe {
            crate::listobj::ee_won_list_call(word(a, at.list_rust) as *const crate::listobj::EeWonList, c, 0, 0, 0)
        };
        let sel = || {
            // SAFETY: as above; `b` the record the op fills
            unsafe {
                *(b as *mut crate::guidraw::EeWonSelColor) =
                    *((a as usize + at.list_sel) as *const crate::guidraw::EeWonSelColor)
            };
            0
        };
        Some(match o {
            // SAFETY: as above
            ItemHasFont => isize::from(unsafe { word(me, at.font) } != 0),
            ItemTakeListFont => {
                // SAFETY: as above
                let f = unsafe { word(a, at.list_font) };
                if f != 0 {
                    crate::gamewin::rf(f, true)?;
                }
                // SAFETY: the item's font slot
                let old = unsafe { std::mem::replace(&mut *(m(at.font) as *mut isize), f) };
                if old != 0 {
                    crate::gamewin::rf(old, false)?;
                }
                0
            }
            // SAFETY: as above
            ItemColor => (unsafe { *(m(at.color) as *const std::ffi::c_long) }) as isize,
            ItemTakeListColor => {
                // SAFETY: as above (`GetTextColor()` answers `mTextColor`)
                unsafe {
                    *(m(at.color) as *mut std::ffi::c_long) =
                        *((a as usize + at.list_text_color) as *const i32) as std::ffi::c_long
                };
                0
            }
            // SAFETY: as above
            ItemHasImage => isize::from(unsafe { word(me, at.image) } != 0),
            ItemImageWidth => image().map_or(0, |i| i.width() as u16 as isize),
            ItemImageHeight => image().map_or(0, |i| i.height() as u16 as isize),
            ItemTextWidth => {
                // SAFETY: as above: the item's text (a guistrobj.rs block) and font
                unsafe {
                    let t = word(me, at.text) as *const u8;
                    let buf =
                        &*(t.add(crate::guistrobj::EE_WON_GSTR_DATA_AT) as *const crate::guistrobj::EeWonGuiStrBuf);
                    if buf.length == 0 {
                        0
                    } else {
                        crate::platfont::ee_won_plat_font_width(font(), buf.text, buf.length) as isize
                    }
                }
            }
            // SAFETY: as above
            ItemFontHeight => (unsafe { crate::platfont::ee_won_plat_font_metric(font(), 0) }) as isize,
            ItemListFlag => isize::from(list(crate::listobj::EeWonListCall::ListFlags) & b != 0),
            ItemListPaintWidth => list(crate::listobj::EeWonListCall::PaintWidth) as i32 as isize,
            ItemListSel | MenuListSel => sel(),
            // SAFETY: as above
            MenuDisabled => isize::from(unsafe { *m(at.disabled) } != 0),
            // SAFETY: as above
            MenuHasSubMenu => isize::from(unsafe { word(me, at.sub_menu) } != 0),
            _ => {
                let f = crate::guihost::EeWonGuiField::ScrollWidth as isize;
                // SAFETY: as above: the list's Rust pane
                let w = unsafe {
                    crate::paneobj::ee_won_pane_call(
                        word(a, at.list_pane) as *const crate::paneobj::EeWonScrollPane,
                        crate::paneobj::EeWonPaneCall::GetField,
                        f,
                        0,
                        0,
                    )
                };
                w as i32 as isize
            }
        })
    }
    /// A component list item's and a background's members and their component's non-virtual calls in the game (R6
    /// WON GUI round 40): in place at their registered offsets, the component's through Component's own calls. None
    /// in a driver's world.
    fn member_op(&self, o: EeWonLeafOp, a: isize, b: isize) -> Option<isize> {
        use EeWonLeafOp::*;
        if !matches!(
            o,
            CompItemHasComponent
                | CompItemWidth
                | CompItemHeight
                | CompItemSetParent
                | CompItemBaseSetPos
                | CompItemSetPos
                | CompItemSizable
                | CompItemListPaintWidth
                | CompItemSetSize
                | CompItemMarkInvalid
                | BgHasImage
                | BgStretch
                | BgUseOffsets
                | BgHasComponent
                | BgImageWidth
                | BgImageHeight
                | BgColor
                | BgBorderColor
                | BgCompSetSize
                | BgCompInvalidate
        ) || !crate::compobj::is_game()
        {
            return None;
        }
        let at = (*LEAF_AT.lock().ok()?)?;
        let me = self.me as isize;
        // SAFETY (the reads below): the game's live item or background `me` (the ops' contracts), each member at its
        // registered offset; the component and image its SmartPtrs hold are live
        let flag = |off: usize| isize::from(unsafe { *((me as usize + off) as *const u8) } != 0);
        // SAFETY: as above
        let int = |off: usize| (unsafe { *((me as usize + off) as *const i32) }) as isize;
        // SAFETY: as above
        let ptr = |off: usize| unsafe { word(me, off) };
        // SAFETY: as above
        let image = || unsafe { crate::imgobj::image_of(ptr(at.bg_image)) };
        let comp = crate::compobj::comp();
        // Component's own SetPos and SetSize, as the C++ calls them (no class overrides them)
        // SAFETY: the game's live component; the call re-enters through Component's entry
        let kc = |c: isize, k: crate::compobj::EeWonCompCall, x: isize, y: isize| unsafe {
            crate::compobj::ee_won_comp_call(
                c as *mut c_void,
                crate::compobj::host(),
                k,
                x as i32 as isize,
                y as i32 as isize,
                0,
            )
        };
        let item_comp = || ptr(at.comp_item_component);
        Some(match o {
            CompItemHasComponent => isize::from(item_comp() != 0),
            CompItemWidth => crate::compmem::bounds(item_comp()).width as isize,
            CompItemHeight => crate::compmem::bounds(item_comp()).height as isize,
            CompItemSetParent => {
                crate::compmem::set_parent(item_comp(), a);
                0
            }
            CompItemBaseSetPos => {
                // SAFETY: the item's own members (ListItem::SetPos sets the top alone)
                unsafe { (*item_rec(me)).top = b as i32 };
                0
            }
            CompItemSetPos => {
                kc(item_comp(), crate::compobj::EeWonCompCall::SetPos, a, b);
                0
            }
            CompItemSizable => flag(at.comp_item_sizable),
            CompItemListPaintWidth => {
                // SAFETY: the game's live list `a` and its Rust list at the registered offset
                let w = unsafe {
                    crate::listobj::ee_won_list_call(
                        word(a, at.list_rust) as *const crate::listobj::EeWonList,
                        crate::listobj::EeWonListCall::PaintWidth,
                        0,
                        0,
                        0,
                    )
                };
                w as i32 as isize
            }
            CompItemSetSize => {
                kc(item_comp(), crate::compobj::EeWonCompCall::SetSize, a, b);
                0
            }
            CompItemMarkInvalid => {
                comp.v_mark_invalid(item_comp());
                0
            }
            BgHasImage => isize::from(ptr(at.bg_image) != 0),
            BgStretch => flag(at.bg_stretch),
            BgUseOffsets => flag(at.bg_use_offsets),
            BgHasComponent => isize::from(ptr(at.bg_component) != 0),
            BgImageWidth => image().map_or(0, |i| i.width() as u16 as isize),
            BgImageHeight => image().map_or(0, |i| i.height() as u16 as isize),
            BgColor => int(at.bg_color),
            BgBorderColor => int(at.bg_border_color),
            BgCompSetSize => {
                kc(ptr(at.bg_component), crate::compobj::EeWonCompCall::SetSize, a, b);
                0
            }
            _ => {
                comp.v_invalidate(ptr(at.bg_component));
                0
            }
        })
    }
    /// An image button's images (its `mImages`, read in place: R6 WON GUI round 39): whether it has the image `a`, its
    /// width and height, the stretch flag, the image sized to the button, the image `b` drawn on the Graphics `a` at
    /// the origin. None in a driver's world.
    fn image_button_op(&self, o: EeWonLeafOp, a: isize, b: isize) -> Option<isize> {
        use EeWonLeafOp::*;
        let at = IMGBTN_AT.load(std::sync::atomic::Ordering::Acquire);
        if at == 0
            || !matches!(o, BtnHasImage | BtnImageWidth | BtnImageHeight | BtnStretch | BtnImageFit | BtnDrawImage)
            || !crate::compobj::is_game()
        {
            return None;
        }
        let me = self.me as usize;
        // SAFETY: the game's live image button (the Btn ops' `t`): five image pointers at its registered offset, the
        // stretch flag after them
        let image = |i: isize| unsafe { *((me + at) as *const isize).add(i as usize) };
        // SAFETY: the button's live image (its SmartPtr holds it)
        let img = |i: isize| unsafe { crate::imgobj::image_of(image(i)) };
        Some(match o {
            BtnHasImage => isize::from(image(a) != 0),
            BtnImageWidth => img(a).map_or(0, |i| i.width() as u16 as isize),
            BtnImageHeight => img(a).map_or(0, |i| i.height() as u16 as isize),
            // SAFETY: as above
            BtnStretch => isize::from(unsafe { *((me + at + 5 * std::mem::size_of::<usize>()) as *const u8) } != 0),
            BtnImageFit => {
                let r = crate::compmem::bounds(me as isize);
                if let Some(i) = img(a) {
                    // SAFETY: a live image
                    unsafe {
                        crate::imgobj::ee_won_image_call(
                            i,
                            crate::imgobj::EeWonImageCall::SetSize,
                            r.width as isize,
                            r.height as isize,
                            0,
                            0,
                        )
                    };
                }
                0
            }
            _ => {
                let g = a as *mut crate::gfxrun::EeWonGraphics;
                // SAFETY: the game's live Graphics and the button's image
                unsafe {
                    crate::gfxrun::ee_won_graphics_call(
                        g,
                        crate::gfxobj::EeWonGfxCall::DrawImage,
                        0,
                        0,
                        0,
                        0,
                        0,
                        0,
                        0,
                        image(b) as *const c_void,
                    )
                };
                0
            }
        })
    }
    #[cfg(test)]
    pub(crate) fn test_op(&self, o: EeWonLeafOp) -> isize {
        self.op(o, 0, 0)
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

    /// `Background::Paint(g, x, y, width, height, x_offset, y_offset)`: the stretched image alone, or the fill, the
    /// border, the tiled image and the component (sized to the area, invalidated and painted).
    pub fn background_paint(&self, g: isize, area: [i32; 6]) {
        use EeWonLeafOp::*;
        let has_image = self.yes(BgHasImage, 0);
        let b = EeWonBackground {
            has_image,
            stretch_image: self.yes(BgStretch, 0),
            use_offsets: self.yes(BgUseOffsets, 0),
            has_component: self.yes(BgHasComponent, 0),
            image_width: if has_image { self.get(BgImageWidth, 0) } else { 0 },
            image_height: if has_image { self.get(BgImageHeight, 0) } else { 0 },
            color: self.get(BgColor, 0),
            border_color: self.get(BgBorderColor, 0),
            x: area[0],
            y: area[1],
            width: area[2],
            height: area[3],
            x_offset: area[4],
            y_offset: area[5],
        };
        for o in background(&b) {
            if o.kind == EE_WON_DRAW_CALL {
                self.op(BgCompSetSize, o.b as isize, o.c as isize);
                self.op(BgCompInvalidate, 0, 0);
                self.op(BgCompPaint, g, 0);
            } else {
                self.op(BgDraw, g, &o as *const EeWonDrawOp as isize);
            }
        }
    }

    /// `ComponentListItem::CalcDimensions`: the item's size its component's, when it has one.
    pub fn component_item_measure(&self) {
        use EeWonLeafOp::*;
        if self.yes(CompItemHasComponent, 0) {
            self.set_size(self.get(CompItemWidth, 0), self.get(CompItemHeight, 0));
        }
    }

    /// `ComponentListItem::SetViewport`: the component parented to the list's viewport, told when it has one.
    pub fn component_item_viewport(&self, viewport: isize) {
        self.op(EeWonLeafOp::CompItemSetParent, viewport, 0);
        if viewport != 0 {
            self.op(EeWonLeafOp::CompItemAdded, 0, 0);
        }
    }

    /// `ComponentListItem::SetPos`: the item and its component moved.
    pub fn component_item_set_pos(&self, x: isize, y: isize) {
        self.op(EeWonLeafOp::CompItemBaseSetPos, x, y);
        self.op(EeWonLeafOp::CompItemSetPos, x, y);
    }

    /// `ComponentListItem::Paint`: the item's component drawn, first stretched to the list's paint width when the item
    /// is sizable; nothing without a component.
    pub fn component_item_paint(&self, g: isize, list: isize) {
        use EeWonLeafOp::*;
        if !self.yes(CompItemHasComponent, 0) {
            return;
        }
        if self.yes(CompItemSizable, 0) {
            let width = self.get(CompItemListPaintWidth, list);
            self.op(CompItemSetSize, width as isize, self.get(CompItemHeight, 0) as isize);
        }
        self.op(CompItemMarkInvalid, 0, 0);
        self.op(CompItemPaint, g, 0);
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
            EeWonLeafCall::ComponentItemMeasure => self.component_item_measure(),
            EeWonLeafCall::ComponentItemViewport => self.component_item_viewport(b),
            EeWonLeafCall::ComponentItemSetPos => self.component_item_set_pos(a, b),
            EeWonLeafCall::ComponentItemPaint => self.component_item_paint(a, b),
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

/// `Background::Paint(*(Graphics*)g, x, y, width, height, x_offset, y_offset)` on the C++ background `me` through its
/// host.
///
/// # Safety
/// `host` answers `me`'s ops; `g` is the Graphics the host's draw ops take.
#[no_mangle]
pub unsafe extern "C" fn ee_won_leaf_background(
    me: *mut c_void,
    host: EeWonLeafHostFn,
    g: isize,
    x: i32,
    y: i32,
    width: i32,
    height: i32,
    x_offset: i32,
    y_offset: i32,
) {
    EeWonLeaf::new(me, host).background_paint(g, [x, y, width, height, x_offset, y_offset])
}

#[cfg(test)]
#[path = "leafpaint_tests.rs"]
mod tests;
