//! The WON skin's button, WONButton, as a whole Rust object (R6 WON round 35): the skinned lobby's push buttons,
//! check buttons and list headers. The original's WONButton.cpp and WONButtonSetup.cpp with their state: the ten
//! button states (a wrapped text, an image and a colour per state, checked or not), the wrapped text per font, the
//! text and focus pads, the push offsets, the default text colour, the flags and the alignments.
//!
//! The C++ `WONButton` stays the type the lobby holds (a `Button` in the tree, its vtable); its virtuals and methods
//! are one-line forwarders to `ee_won_wonbtn_call`, and every call out is an op of the component op table
//! (`guihost`). The wrapped texts, images and fonts are C++ objects: the Rust holds their references as the
//! original's `SmartPtr`s did (`WbText` / `WbImage` / `WbFont` hold and release ops); the font map's keys are raw,
//! as the original's were. The paint is ee_won's existing plans (`guidraw::button`, `button_text_paint`) through
//! the `Draw` op, its image and text slots the state's.

use std::cell::RefCell;
use std::collections::BTreeMap;
use std::ffi::c_void;

use crate::guidraw::{self, EeWonButtonPaint, EeWonDrawOp};
use crate::guifocus;
use crate::guihost::{
    EeWonGuiField as F, EeWonGuiHostFn, EeWonGuiOp as Op, EeWonSkinBtn as Sb, EeWonWbFont as Wf, EeWonWbImage as Wi,
    EeWonWbText as Wt,
};

/// `HorzAlign_Center` / `VertAlign_Center`.
const ALIGN_CENTER: u8 = 1;
/// `ComponentFlag_GrabBG`, `ComponentFlag_Translucent`.
const FLAG_GRAB_BG: isize = 0x100;
const FLAG_TRANSLUCENT: isize = 0x200;

/// A field of the button (`Get` / `Set` calls).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonWbField {
    /// `mAdjustHeightToFitText` (the flag alone).
    AdjustHeight = 1,
    LeftTextPad,
    TopTextPad,
    RightTextPad,
    BottomTextPad,
    LeftFocusPad,
    TopFocusPad,
    RightFocusPad,
    BottomFocusPad,
    PushDX,
    PushDY,
    DefaultTextColor,
    HasCheck,
    ScaleImage,
    /// `mWrapText` (the flag alone; `SetWrapText` also sets the texts').
    WrapText,
    DrawGrayText,
    /// `mIsTransparent` (the flag alone; `SetTransparent` also grabs the background).
    Transparent,
    /// `mHAlign` (the field alone; `SetHorzAlign` also centres the texts).
    HAlign,
    VAlign,
    FocusStyle,
}

/// The inward calls (the C++ shell's virtuals and methods).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonWbCall {
    /// `SetTextHook(*(const GUIString*)a)`.
    SetTextHook = 1,
    /// `Virtual08` (SizeChanged).
    SizeChanged,
    /// `AdjustHeightToFitText()`.
    AdjustHeightToFitText,
    /// `SetAdjustHeightToFitText(a != 0)`.
    SetAdjustHeightToFitText,
    /// `SetWrapText(a != 0)`.
    SetWrapText,
    /// `SetTransparent(a != 0)`.
    SetTransparent,
    /// `SetButtonState(a, (Image*)b, c, (Font*)d, checked)` with the checked flag as `e`: see
    /// `ee_won_wonbtn_set_state`.
    SetButtonState,
    /// `GetTextSize(*a, *b)` (`a`, `b` point at i32s).
    GetTextSize,
    /// `GetDesiredSize(*a, *b)`.
    DesiredSize,
    /// `SetHorzAlign(a)`.
    SetHorzAlign,
    /// `SetTextColor(a)`.
    SetTextColor,
    /// `SetFont((Font*)a)`.
    SetFont,
    /// `CopyAttributes(from)`, `a` the source's Rust button (`ee_won_wonbtn_of`) and `b` its C++ `WONButton*`.
    CopyAttributes,
    /// `DrawButtonText(*a, (WrappedText*)b, c)`.
    DrawButtonText,
    /// `GetButtonState`: the colour into `*(int*)b`, the text into `*(WrappedText**)c` and the image into
    /// `*(Image**)d` (each skipped when NULL); `a` the Graphics (unused, as in the original).
    GetButtonState,
    /// `Paint(*a)`.
    Paint,
    /// `GetText()`: the first font's text's `GUIString*`, 0 when there is no font.
    GetText,
    /// the field `a` (`EeWonWbField`).
    Get,
    /// the field `a` = `b`.
    Set,
}

#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
struct Cell {
    text: isize,
    image: isize,
    color: i32,
}

#[derive(Debug, Clone, PartialEq, Eq)]
struct State {
    cells: [[Cell; 2]; 5],
    /// the font map: `Font*` -> its wrapped text (held; 0 for an entry `operator[]` made empty)
    fonts: BTreeMap<isize, isize>,
    adjust_height: bool,
    pads: [i32; 4],
    focus_pads: [i32; 4],
    push_dx: i32,
    push_dy: i32,
    default_text_color: i32,
    has_check: bool,
    scale_image: bool,
    wrap_text: bool,
    draw_gray_text: bool,
    transparent: bool,
    h_align: u8,
    v_align: u8,
    focus_style: u8,
}

/// The Rust WONButton.
pub struct EeWonWonButton {
    comp: *mut c_void,
    host: EeWonGuiHostFn,
    st: RefCell<State>,
}

impl EeWonWonButton {
    /// WONButtonSetup.cpp's state: 8/4 text pads, 4/3/4/4 focus pads, a 1-pixel push, centred, scaled images,
    /// grey disabled text, the focus rectangle on, the default text colour 0x80000005 (the C++ constructor sets
    /// the button flag 1 and WantFocus).
    pub fn new(comp: *mut c_void, host: EeWonGuiHostFn) -> Self {
        let cell = Cell { text: 0, image: 0, color: -1 };
        EeWonWonButton {
            comp,
            host,
            st: RefCell::new(State {
                cells: [[cell; 2]; 5],
                fonts: BTreeMap::new(),
                adjust_height: false,
                pads: [8, 4, 8, 4],
                focus_pads: [4, 3, 4, 4],
                push_dx: 1,
                push_dy: 1,
                default_text_color: 0x80000005_u32 as i32,
                has_check: false,
                scale_image: true,
                wrap_text: false,
                draw_gray_text: true,
                transparent: false,
                h_align: ALIGN_CENTER,
                v_align: ALIGN_CENTER,
                focus_style: 1,
            }),
        }
    }

    fn op(&self, op: Op, a: isize, b: isize, c: isize, d: isize) -> isize {
        match self.host {
            // SAFETY: the host was given with this object's component; no borrow of the state is live here
            Some(h) => unsafe { h(self.comp, op, a, b, c, d) },
            None => 0,
        }
    }
    fn sb(&self, which: Sb, b: isize, c: isize) -> isize {
        self.op(Op::SkinBtn, which as isize, b, c, 0)
    }
    fn text(&self, which: Wt, text: isize, c: isize) -> isize {
        self.op(Op::WbText, which as isize, text, c, 0)
    }
    fn image(&self, which: Wi, image: isize) -> isize {
        self.op(Op::WbImage, which as isize, image, 0, 0)
    }
    fn width(&self) -> i32 {
        self.op(Op::Get, F::Width as isize, 0, 0, 0) as i32
    }
    fn height(&self) -> i32 {
        self.op(Op::Get, F::Height as isize, 0, 0, 0) as i32
    }
    /// the map's texts in its order (a copy: the calls out may not change the map, but no borrow is held)
    fn texts(&self) -> Vec<isize> {
        self.st.borrow().fonts.values().copied().collect()
    }
    /// a `SmartPtr<WrappedText>` assignment: the new one held, then the old one given back
    fn assign_text(&self, slot: &mut isize, new: isize) {
        if new != 0 {
            self.text(Wt::Hold, new, 0);
        }
        let old = std::mem::replace(slot, new);
        if old != 0 {
            self.text(Wt::Release, old, 0);
        }
    }
    fn assign_image(&self, slot: &mut isize, new: isize) {
        if new != 0 {
            self.image(Wi::Hold, new);
        }
        let old = std::mem::replace(slot, new);
        if old != 0 {
            self.image(Wi::Release, old);
        }
    }
    /// a cell's text set (the state not borrowed across the calls out)
    fn set_cell_text(&self, i: usize, j: usize, new: isize) {
        let mut slot = self.st.borrow().cells[i][j].text;
        self.assign_text(&mut slot, new);
        self.st.borrow_mut().cells[i][j].text = slot;
    }
    fn set_cell_image(&self, i: usize, j: usize, new: isize) {
        let mut slot = self.st.borrow().cells[i][j].image;
        self.assign_image(&mut slot, new);
        self.st.borrow_mut().cells[i][j].image = slot;
    }

    /// `SetTextHook`: the text set in every font's wrapped text, then the height refitted.
    pub fn set_text_hook(&self, text: isize) {
        for t in self.texts() {
            self.text(Wt::SetText, t, text);
        }
        self.adjust_height_to_fit_text();
    }

    /// `Virtual08`: the base's; when wrapping, every text wrapped to the width inside the pads and the height
    /// refitted.
    pub fn size_changed(&self) {
        self.op(Op::BaseSizeChanged, 0, 0, 0, 0);
        if !self.st.borrow().wrap_text {
            return;
        }
        for t in self.texts() {
            let p = self.st.borrow().pads;
            let w = self.width().wrapping_sub(p[0]).wrapping_sub(p[2]);
            self.text(Wt::SetMaxWidth, t, w as isize);
        }
        self.adjust_height_to_fit_text();
    }

    /// `AdjustHeightToFitText`: the height set to the texts' largest plus the vertical pads, when fitting is on.
    /// (`GetTextSize` is virtual; no subclass overrides it.)
    pub fn adjust_height_to_fit_text(&self) {
        if !self.st.borrow().adjust_height {
            return;
        }
        let (_, height) = self.text_size();
        let p = self.st.borrow().pads;
        if self.height().wrapping_sub(p[1]).wrapping_sub(p[3]) != height {
            let w = self.width();
            let p = self.st.borrow().pads;
            self.op(Op::SetSize, w as isize, height.wrapping_add(p[1]).wrapping_add(p[3]) as isize, 0, 0);
        }
    }

    pub fn set_adjust_height_to_fit_text(&self, adjust: bool) {
        if adjust == self.st.borrow().adjust_height {
            return;
        }
        self.st.borrow_mut().adjust_height = adjust;
        self.adjust_height_to_fit_text();
    }

    /// `SetWrapText`: every font's text wrapped or not, when the flag changes.
    pub fn set_wrap_text(&self, wrap: bool) {
        if self.st.borrow().wrap_text == wrap {
            return;
        }
        self.st.borrow_mut().wrap_text = wrap;
        for t in self.texts() {
            self.text(Wt::SetWrap, t, wrap as isize);
        }
    }

    /// `SetTransparent`: the flag; a transparent button grabs the background.
    pub fn set_transparent(&self, transparent: bool) {
        self.st.borrow_mut().transparent = transparent;
        if transparent {
            self.sb(Sb::SetGrabBG, 0, 0);
        }
    }

    /// `SetButtonState`: one state's image, colour and font's text (the default font for the first state given
    /// none); one wrapped text per font.
    pub fn set_button_state(&self, state: i32, image: isize, color: i32, font: isize, checked: bool) {
        let ci = checked as usize;
        // the original indexed mStates without a check: a state outside 0..4 wrote past the array
        let si = state.clamp(0, 4) as usize;
        if ci != 0 {
            self.st.borrow_mut().has_check = true;
        }
        let mut font = font;
        let mut local_font = 0isize; // the SmartPtr<Font> aFont
        if self.st.borrow().fonts.is_empty() && font == 0 {
            let f = self.op(Op::WbFont, Wf::Default as isize, 0, 0, 0);
            if f != 0 {
                self.op(Op::WbFont, Wf::Hold as isize, f, 0, 0);
            }
            local_font = f;
            font = f;
        }
        if font != 0 {
            let entry = *self.st.borrow_mut().fonts.entry(font).or_insert(0);
            let entry = if entry == 0 {
                // SmartPtr<WrappedText> aText = new WrappedText (answered with that one reference)
                let t = self.text(Wt::New, 0, 0);
                let centered = self.st.borrow().h_align == ALIGN_CENTER;
                self.text(Wt::SetCentered, t, centered as isize);
                self.text(Wt::SetWrap, t, 0);
                self.text(Wt::SetFont, t, font);
                // anItr->second = aText (the entry was empty), then the local given back
                self.text(Wt::Hold, t, 0);
                self.st.borrow_mut().fonts.insert(font, t);
                self.text(Wt::Release, t, 0);
                t
            } else {
                entry
            };
            self.set_cell_text(si, ci, entry);
        }
        self.set_cell_image(si, ci, image);
        self.st.borrow_mut().cells[si][ci].color = color;
        if local_font != 0 {
            self.op(Op::WbFont, Wf::Release as isize, local_font, 0, 0);
        }
    }

    /// `GetTextSize`: the largest width and height of the texts.
    pub fn text_size(&self) -> (i32, i32) {
        let (mut width, mut height) = (0i32, 0i32);
        for t in self.texts() {
            if self.text(Wt::Width, t, 0) as i32 > width {
                width = self.text(Wt::Width, t, 0) as i32;
            }
            if self.text(Wt::Height, t, 0) as i32 > height {
                height = self.text(Wt::Height, t, 0) as i32;
            }
        }
        (width, height)
    }

    /// `GetDesiredSize`: the text size grown to the largest state image unless images are scaled, plus the pads.
    pub fn desired_size(&self) -> (i32, i32) {
        let (mut width, mut height) = self.text_size();
        for i in 0..5 {
            for j in 0..2 {
                let (img, scale) = {
                    let s = self.st.borrow();
                    (s.cells[i][j].image, s.scale_image)
                };
                if img == 0 {
                    continue;
                }
                if !scale && self.image(Wi::Width, img) as i32 > width {
                    width = self.image(Wi::Width, img) as i32;
                }
                if !scale && self.image(Wi::Height, img) as i32 > height {
                    height = self.image(Wi::Height, img) as i32;
                }
            }
        }
        let p = self.st.borrow().pads;
        (width.wrapping_add(p[0]).wrapping_add(p[2]), height.wrapping_add(p[1]).wrapping_add(p[3]))
    }

    /// `SetHorzAlign`: only to or from centred (left <-> right is ignored); every text centred or not.
    pub fn set_horz_align(&self, align: i32) {
        let cur = self.st.borrow().h_align as i32;
        if align == cur {
            return;
        }
        if cur != ALIGN_CENTER as i32 && align != ALIGN_CENTER as i32 {
            return;
        }
        self.st.borrow_mut().h_align = align as u8;
        for t in self.texts() {
            let centered = self.st.borrow().h_align == ALIGN_CENTER;
            self.text(Wt::SetCentered, t, centered as isize);
        }
    }

    /// `SetTextColor`: every state's colour; with scaled images only the normal and default states'.
    pub fn set_text_color(&self, color: i32) {
        let mut s = self.st.borrow_mut();
        if !s.scale_image {
            for row in s.cells.iter_mut() {
                for c in row.iter_mut() {
                    c.color = color;
                }
            }
        } else {
            s.cells[0][0].color = color;
            s.cells[0][1].color = color;
            s.cells[4][0].color = color;
            s.cells[4][1].color = color;
        }
    }

    /// `SetFont`: the map emptied, the normal state's text set to the font and made every state's.
    pub fn set_font(&self, font: isize) {
        let old: Vec<isize> = std::mem::take(&mut self.st.borrow_mut().fonts).into_values().collect();
        for t in old {
            if t != 0 {
                self.text(Wt::Release, t, 0);
            }
        }
        // SmartPtr<WrappedText> aText = mStates[0][0].mText.get() (the original read through it unchecked)
        let a = self.st.borrow().cells[0][0].text;
        if a != 0 {
            self.text(Wt::Hold, a, 0);
        }
        self.text(Wt::SetFont, a, font);
        // mFontTextMap[theFont] = aText
        if a != 0 {
            self.text(Wt::Hold, a, 0);
        }
        self.st.borrow_mut().fonts.insert(font, a);
        for i in 0..5 {
            for j in 0..2 {
                self.set_cell_text(i, j, a);
            }
        }
        if a != 0 {
            self.text(Wt::Release, a, 0);
        }
    }

    /// `CopyAttributes`: the base's, then the source's pads, alignments, flags and colours, a fresh wrapped text
    /// per source font, and the source's states (images duplicated when scaled).
    pub fn copy_attributes(&self, from: &EeWonWonButton, from_comp: isize) {
        self.op(Op::WbBaseCopy, from_comp, 0, 0, 0);
        let f = from.st.borrow().clone();
        {
            let mut s = self.st.borrow_mut();
            s.pads = f.pads;
            s.focus_pads = f.focus_pads;
            s.push_dx = f.push_dx;
            s.push_dy = f.push_dy;
            s.has_check = f.has_check;
            s.scale_image = f.scale_image;
            s.h_align = f.h_align;
            s.v_align = f.v_align;
            s.draw_gray_text = f.draw_gray_text;
            s.transparent = f.transparent;
            s.focus_style = f.focus_style;
            s.default_text_color = f.default_text_color;
        }
        if self.sb(Sb::FlagSet, from_comp, FLAG_GRAB_BG) != 0 {
            self.sb(Sb::SetFlag, FLAG_GRAB_BG, 0);
        }
        if self.sb(Sb::FlagSet, from_comp, FLAG_TRANSLUCENT) != 0 {
            self.sb(Sb::SetFlag, FLAG_TRANSLUCENT, 0);
        }
        // mFontTextMap = from->mFontTextMap: the source's texts held, this one's given back
        let copy: BTreeMap<isize, isize> = from.st.borrow().fonts.clone();
        for t in copy.values() {
            if *t != 0 {
                self.text(Wt::Hold, *t, 0);
            }
        }
        let old: Vec<isize> = std::mem::replace(&mut self.st.borrow_mut().fonts, copy).into_values().collect();
        for t in old {
            if t != 0 {
                self.text(Wt::Release, t, 0);
            }
        }
        let keys: Vec<isize> = self.st.borrow().fonts.keys().copied().collect();
        for k in keys {
            // anItr->second = new WrappedText
            let t = self.text(Wt::New, 0, 0);
            let mut slot = self.st.borrow().fonts.get(&k).copied().unwrap_or(0);
            self.assign_text(&mut slot, t);
            self.st.borrow_mut().fonts.insert(k, slot);
            self.text(Wt::Release, t, 0); // the New op's reference: the map's is the one kept
            let (centered, wrap) = {
                let s = self.st.borrow();
                (s.h_align == ALIGN_CENTER, s.wrap_text)
            };
            self.text(Wt::SetCentered, t, centered as isize);
            self.text(Wt::SetWrap, t, wrap as isize);
            self.text(Wt::SetFont, t, k);
        }
        for i in 0..5 {
            for j in 0..2 {
                let src = from.st.borrow().cells[i][j];
                self.st.borrow_mut().cells[i][j].color = src.color;
                if src.image != 0 {
                    if self.st.borrow().scale_image {
                        // dst.mImage = src.mImage->Duplicate(): answered with the temporary's reference
                        let d = self.image(Wi::Duplicate, src.image);
                        self.set_cell_image(i, j, d);
                        if d != 0 {
                            self.image(Wi::Release, d);
                        }
                    } else {
                        self.set_cell_image(i, j, src.image);
                    }
                }
                if src.text != 0 {
                    let font = self.text(Wt::Font, src.text, 0);
                    let t = *self.st.borrow_mut().fonts.entry(font).or_insert(0);
                    self.set_cell_text(i, j, t);
                }
            }
        }
    }

    /// `GetButtonState`: the current state's colour, text and image, falling back through the normal states and
    /// to the default text colour.
    pub fn button_state(&self) -> (i32, isize, isize) {
        let disabled = self.sb(Sb::IsDisabled, 0, 0) != 0;
        let flags = self.sb(Sb::ButtonFlags, 0, 0) as u32;
        let is_default = self.sb(Sb::IsDefaultButton, 0, 0) != 0;
        let has_check = self.st.borrow().has_check;
        let (state, checked) = guifocus::button_state(disabled, flags, is_default, has_check);
        let s = self.st.borrow();
        let (mut image, mut color, mut text) = (0isize, -1i32, 0isize);
        for (si, ci) in guifocus::button_state_cells(state, checked) {
            let c = s.cells[si][ci];
            if image == 0 {
                image = c.image;
            }
            if color == -1 {
                color = c.color;
            }
            if text == 0 {
                text = c.text;
            }
        }
        if color == -1 {
            color = s.default_text_color;
        }
        (color, text, image)
    }

    /// The paint's inputs (WONButton.cpp's `WONButtonPaintOf`), read in its order.
    fn paint_of(&self, image: isize, text: isize, color: i32) -> EeWonButtonPaint {
        let s = self.st.borrow().clone();
        let mut p = EeWonButtonPaint {
            width: self.width(),
            height: self.height(),
            transparent: s.transparent,
            scale_image: s.scale_image,
            has_image: image != 0,
            has_text: text != 0,
            ..Default::default()
        };
        p.pressed = self.sb(Sb::IsPressed, 0, 0) != 0;
        p.disabled = self.sb(Sb::IsDisabled, 0, 0) != 0;
        p.draw_gray_text = s.draw_gray_text;
        p.focus_shown = s.focus_style != 0
            && self.sb(Sb::HasFocus, 0, 0) != 0
            && self.sb(Sb::UsedDialogKeys, self.sb(Sb::Parent, 0, 0), 0) != 0;
        p.h_align = s.h_align as i32;
        p.v_align = s.v_align as i32;
        if image != 0 {
            p.image_width = self.image(Wi::Width, image) as i32;
            p.image_height = self.image(Wi::Height, image) as i32;
        }
        if text != 0 {
            p.text_width = self.text(Wt::Width, text, 0) as i32;
            p.text_height = self.text(Wt::Height, text, 0) as i32;
        }
        p.color = color;
        [p.left_text_pad, p.top_text_pad, p.right_text_pad, p.bottom_text_pad] = s.pads;
        [p.left_focus_pad, p.top_focus_pad, p.right_focus_pad, p.bottom_focus_pad] = s.focus_pads;
        p.push_dx = s.push_dx;
        p.push_dy = s.push_dy;
        p
    }

    fn run(&self, g: isize, ops: &[EeWonDrawOp], image: isize, text: isize) {
        for o in ops {
            self.op(Op::Draw, g, o as *const EeWonDrawOp as isize, image, text);
        }
    }

    /// `DrawButtonText`: nothing without a text; else the text placed by the alignment and push, embossed grey
    /// when disabled, with the focus rectangle.
    pub fn draw_button_text(&self, g: isize, text: isize, color: i32) {
        if text == 0 {
            return;
        }
        let p = self.paint_of(0, text, color);
        self.run(g, &guidraw::button_text_paint(&p), 0, text);
    }

    /// `Paint`: the base's, then the face, the state's image and its text.
    pub fn paint(&self, g: isize) {
        self.op(Op::BasePaint, g, 0, 0, 0);
        let (color, text, image) = self.button_state();
        let p = self.paint_of(image, text, color);
        self.run(g, &guidraw::button(&p), image, text);
    }

    fn get(&self, f: EeWonWbField) -> i32 {
        let s = self.st.borrow();
        type B = EeWonWbField;
        match f {
            B::AdjustHeight => s.adjust_height as i32,
            B::LeftTextPad => s.pads[0],
            B::TopTextPad => s.pads[1],
            B::RightTextPad => s.pads[2],
            B::BottomTextPad => s.pads[3],
            B::LeftFocusPad => s.focus_pads[0],
            B::TopFocusPad => s.focus_pads[1],
            B::RightFocusPad => s.focus_pads[2],
            B::BottomFocusPad => s.focus_pads[3],
            B::PushDX => s.push_dx,
            B::PushDY => s.push_dy,
            B::DefaultTextColor => s.default_text_color,
            B::HasCheck => s.has_check as i32,
            B::ScaleImage => s.scale_image as i32,
            B::WrapText => s.wrap_text as i32,
            B::DrawGrayText => s.draw_gray_text as i32,
            B::Transparent => s.transparent as i32,
            B::HAlign => s.h_align as i32,
            B::VAlign => s.v_align as i32,
            B::FocusStyle => s.focus_style as i32,
        }
    }

    fn set(&self, f: EeWonWbField, v: i32) {
        let mut s = self.st.borrow_mut();
        type B = EeWonWbField;
        match f {
            B::AdjustHeight => s.adjust_height = v != 0,
            B::LeftTextPad => s.pads[0] = v,
            B::TopTextPad => s.pads[1] = v,
            B::RightTextPad => s.pads[2] = v,
            B::BottomTextPad => s.pads[3] = v,
            B::LeftFocusPad => s.focus_pads[0] = v,
            B::TopFocusPad => s.focus_pads[1] = v,
            B::RightFocusPad => s.focus_pads[2] = v,
            B::BottomFocusPad => s.focus_pads[3] = v,
            B::PushDX => s.push_dx = v,
            B::PushDY => s.push_dy = v,
            B::DefaultTextColor => s.default_text_color = v,
            B::HasCheck => s.has_check = v != 0,
            B::ScaleImage => s.scale_image = v != 0,
            B::WrapText => s.wrap_text = v != 0,
            B::DrawGrayText => s.draw_gray_text = v != 0,
            B::Transparent => s.transparent = v != 0,
            B::HAlign => s.h_align = v as u8,
            B::VAlign => s.v_align = v as u8,
            B::FocusStyle => s.focus_style = v as u8,
        }
    }

    /// One inward call (`SetButtonState` and `CopyAttributes` have their own entry points).
    pub fn call(&self, call: EeWonWbCall, a: isize, b: isize, c: isize, d: isize) -> isize {
        type C = EeWonWbCall;
        let ai = a as i32;
        match call {
            C::SetTextHook => self.set_text_hook(a),
            C::SizeChanged => self.size_changed(),
            C::AdjustHeightToFitText => self.adjust_height_to_fit_text(),
            C::SetAdjustHeightToFitText => self.set_adjust_height_to_fit_text(a != 0),
            C::SetWrapText => self.set_wrap_text(a != 0),
            C::SetTransparent => self.set_transparent(a != 0),
            C::GetTextSize | C::DesiredSize => {
                let (w, h) = if call == C::GetTextSize { self.text_size() } else { self.desired_size() };
                // SAFETY: the C++ passes two live ints
                unsafe {
                    *(a as *mut i32) = w;
                    *(b as *mut i32) = h;
                }
            }
            C::SetHorzAlign => self.set_horz_align(ai),
            C::SetTextColor => self.set_text_color(ai),
            C::SetFont => self.set_font(a),
            C::DrawButtonText => self.draw_button_text(a, b, c as i32),
            C::GetButtonState => {
                let (color, text, image) = self.button_state();
                // SAFETY: each out-pointer is NULL or live (the C++ forwarder's)
                unsafe {
                    if b != 0 {
                        *(b as *mut i32) = color;
                    }
                    if c != 0 {
                        *(c as *mut isize) = text;
                    }
                    if d != 0 {
                        *(d as *mut isize) = image;
                    }
                }
            }
            C::Paint => self.paint(a),
            C::GetText => {
                let first = self.st.borrow().fonts.values().next().copied();
                return match first {
                    Some(t) => self.text(Wt::Text, t, 0),
                    None => 0,
                };
            }
            C::Get => return wb_field_of(ai).map(|f| self.get(f) as isize).unwrap_or(0),
            C::Set => {
                if let Some(f) = wb_field_of(ai) {
                    self.set(f, b as i32);
                }
            }
            C::SetButtonState | C::CopyAttributes => {}
        }
        0
    }
}

impl Drop for EeWonWonButton {
    /// The members given back as the original's destructor gave them: the font map, then the states from the last
    /// (each its image, then its text).
    fn drop(&mut self) {
        let fonts: Vec<isize> = std::mem::take(&mut self.st.borrow_mut().fonts).into_values().collect();
        for t in fonts {
            if t != 0 {
                self.text(Wt::Release, t, 0);
            }
        }
        for i in (0..5).rev() {
            for j in (0..2).rev() {
                let c = self.st.borrow().cells[i][j];
                if c.image != 0 {
                    self.image(Wi::Release, c.image);
                }
                if c.text != 0 {
                    self.text(Wt::Release, c.text, 0);
                }
            }
        }
    }
}

fn wb_field_of(v: i32) -> Option<EeWonWbField> {
    type B = EeWonWbField;
    const ALL: [EeWonWbField; 20] = [
        B::AdjustHeight,
        B::LeftTextPad,
        B::TopTextPad,
        B::RightTextPad,
        B::BottomTextPad,
        B::LeftFocusPad,
        B::TopFocusPad,
        B::RightFocusPad,
        B::BottomFocusPad,
        B::PushDX,
        B::PushDY,
        B::DefaultTextColor,
        B::HasCheck,
        B::ScaleImage,
        B::WrapText,
        B::DrawGrayText,
        B::Transparent,
        B::HAlign,
        B::VAlign,
        B::FocusStyle,
    ];
    ALL.iter().copied().find(|f| *f as i32 == v)
}

// ---- the C ABI

/// A new Rust WONButton for the C++ `component`, its calls out through `host`.
#[no_mangle]
pub extern "C" fn ee_won_wonbtn_new(component: *mut c_void, host: EeWonGuiHostFn) -> *mut EeWonWonButton {
    Box::into_raw(Box::new(EeWonWonButton::new(component, host)))
}

/// # Safety
/// `btn` came from `ee_won_wonbtn_new` (or is NULL) and is not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_won_wonbtn_free(btn: *mut EeWonWonButton) {
    if !btn.is_null() {
        // SAFETY: the contract
        drop(unsafe { Box::from_raw(btn) });
    }
}

/// One of WONButton's methods (`EeWonWbCall`).
///
/// # Safety
/// `btn` is live; the call's arguments as documented.
#[no_mangle]
pub unsafe extern "C" fn ee_won_wonbtn_call(
    btn: *const EeWonWonButton,
    call: EeWonWbCall,
    a: isize,
    b: isize,
    c: isize,
    d: isize,
) -> isize {
    // SAFETY: live (the contract); a shared reference: the calls out re-enter through this function
    unsafe { (*btn).call(call, a, b, c, d) }
}

/// `SetButtonState(state, image, color, font, checked)`.
///
/// # Safety
/// `btn` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_wonbtn_set_state(
    btn: *const EeWonWonButton,
    state: i32,
    image: *mut c_void,
    color: i32,
    font: *mut c_void,
    checked: bool,
) {
    // SAFETY: live (the contract)
    unsafe { (*btn).set_button_state(state, image as isize, color, font as isize, checked) }
}

/// `CopyAttributes(from)`: `from` the source's Rust button, `from_comp` its C++ `WONButton*`.
///
/// # Safety
/// Both buttons are live and distinct (`Duplicate` copies onto a new button).
#[no_mangle]
pub unsafe extern "C" fn ee_won_wonbtn_copy(
    btn: *const EeWonWonButton,
    from: *const EeWonWonButton,
    from_comp: *mut c_void,
) {
    // SAFETY: live (the contract)
    unsafe { (*btn).copy_attributes(&*from, from_comp as isize) }
}

#[cfg(test)]
#[path = "wonbtnobj_tests.rs"]
mod tests;
