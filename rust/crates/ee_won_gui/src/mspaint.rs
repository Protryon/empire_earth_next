//! The Windows skin's drawing in Rust (R6 WON round 36): MSGraphicsUtil.cpp's helpers (the 3D, flat and
//! three-line frames, the arrows, the check box, the radio circle, the tab and the close X) and every MS control's
//! paint over them (MSButton, MSTabButton, MSLabel, MSHeaderButton's sort arrow, MSCloseButton, MSScrollThumb,
//! MSArrowButton, MS3DFrame, MSPopup, MSDlgBack, MSEtchedFrame, MSInputBox's frame, MSScrollbar's track,
//! MSComboBox's frame, MSScrollContainer's corner and frame, MSEtchedFrameGroup's frame, and WONMSSkin's MSImage).
//!
//! Each is a plan in guidraw's manner: the original's statements with every `Graphics` call an operation appended
//! in the original's order, over the numbers the C++ read before the call (the component's size and state; nothing
//! the plan reads changes while its list runs). The C++ makes the base class's paint itself (it is the first
//! statement, or MSScrollbar's last) and executes the list (`WONGUIDraw.h`); an `EE_WON_DRAW_CALL` with `a` =
//! [`EE_WON_MSPAINT_CALL_TEXT`] is the button's `DrawButtonText(g, b, c)`.
//!
//! Ints wrap (the game builds with -fwrapv).

use crate::guidraw::{
    EeWonDrawOp, EE_WON_DRAW_CALL, EE_WON_DRAW_FILL_RECT, EE_WON_DRAW_LINE, EE_WON_DRAW_RECT,
    EE_WON_DRAW_SET_SYS_COLOR, EE_WON_DRAW_TRANSLATE,
};

/// `Graphics::SetPixel(a, b, g.GetStandardColor(c))`: a pixel in a standard colour.
pub const EE_WON_DRAW_SYS_PIXEL: i32 = 22;
/// `Graphics::FillEllipse(a, b, c, d)`.
pub const EE_WON_DRAW_FILL_ELLIPSE: i32 = 23;
/// `Graphics::DrawArc(a, b, c, d, e, f)` (the start and arc angles in degrees).
pub const EE_WON_DRAW_ARC: i32 = 24;
/// The `EE_WON_DRAW_CALL` of a button's paint: `DrawButtonText(g, b, c)`.
pub const EE_WON_MSPAINT_CALL_TEXT: i32 = 1;

/// `StandardColor_*` (WONLobbyGUI.h's enum; the C++ host asserts the ones used here).
const SYS_3D_DARK_SHADOW: i32 = 0;
const SYS_3D_FACE: i32 = 1;
const SYS_3D_HILIGHT: i32 = 2;
const SYS_3D_SHADOW: i32 = 3;
const SYS_SCROLLBAR: i32 = 4;
const SYS_BUTTON_TEXT: i32 = 5;
const SYS_GRAY_TEXT: i32 = 6;
const SYS_MENU_BACK: i32 = 11;
const SYS_BACK: i32 = 13;
const SYS_TEXT: i32 = 14;

/// The state bits of an [`EeWonMsPaint`].
pub const EE_WON_MSPAINT_PRESSED: u32 = 1;
pub const EE_WON_MSPAINT_DISABLED: u32 = 2;
pub const EE_WON_MSPAINT_TRANSPARENT: u32 = 4;
pub const EE_WON_MSPAINT_DEFAULT: u32 = 8;
pub const EE_WON_MSPAINT_FRAME2: u32 = 16;
pub const EE_WON_MSPAINT_CHECKED: u32 = 32;
pub const EE_WON_MSPAINT_DRAW_FRAME: u32 = 64;
pub const EE_WON_MSPAINT_DRAW_3D: u32 = 128;
pub const EE_WON_MSPAINT_VERT: u32 = 256;
pub const EE_WON_MSPAINT_HORZ: u32 = 512;
pub const EE_WON_MSPAINT_INVALID: u32 = 1024;

/// What is drawn; the arguments per kind are [`EeWonMsPaint`]'s.
#[repr(i32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonMsPaintKind {
    /// `LobbyDraw3DFrame(g, a, b, width, height, PRESSED)`.
    Frame3D = 1,
    /// `LobbyDrawFrame(g, a, b, width, height, PRESSED)`.
    Frame,
    /// `MSGraphicsUtil::DrawFrame3(g, a, b, width, height, _)` (the down flag is not read).
    Frame3,
    /// `MSGraphicsUtil::DrawArrow(g, a, _, _, width, height, DISABLED)` (the offsets are not read).
    Arrow,
    /// `MSGraphicsUtil::DrawCheck(g, a, b, width, height, PRESSED, CHECKED)`.
    Check,
    /// `MSGraphicsUtil::DrawRadio(g, a, b, width, height, PRESSED, CHECKED)`.
    Radio,
    /// `MSGraphicsUtil::DrawTab(g, a, b, width, height)`.
    Tab,
    /// `MSGraphicsUtil::DrawCloseX(g, a, b, width, height)`.
    CloseX,
    /// `MSButton::Paint` after `Component::Paint` (PRESSED, TRANSPARENT, DEFAULT: `IsDefaultButton`, FRAME2).
    Button,
    /// `MSTabButton::Paint` after `Component::Paint`.
    TabButton,
    /// `MSLabel::Paint` after `MSButtonBase::Paint` (TRANSPARENT).
    Label,
    /// `MSHeaderButton::Paint` after `MSButton::Paint`: the sort arrow (`a` the sort type, `b` the text's width,
    /// `c` the left pad, PRESSED).
    HeaderSort,
    /// `MSCloseButton::Paint` after `Button::Paint` (PRESSED).
    CloseButton,
    /// `MSScrollThumb::Paint` after `Component::Paint`.
    ScrollThumb,
    /// `MSArrowButton::Paint` after `Component::Paint` (`a` the direction, PRESSED, DISABLED).
    ArrowButton,
    /// `MS3DFrame::Paint` after `Component::Paint` (`a` the type, PRESSED: not `mIsUp`).
    Frame3DComponent,
    /// `MSPopup::Paint` after `Component::Paint`.
    Popup,
    /// `MSDlgBack::Paint` after `Component::Paint`.
    DlgBack,
    /// `MSEtchedFrame::Paint` after `Component::Paint`.
    EtchedFrame,
    /// `MSInputBox::Paint` after `InputBox::Paint`.
    InputBox,
    /// `MSScrollbar::Paint` before `Container::Paint` (INVALID: the scroll rectangle's invalid state, TRANSPARENT,
    /// `a`..`d` the rectangle's left, top, width and height).
    Scrollbar,
    /// `MSComboBox::Paint` after `ComboBox::Paint` (DRAW_FRAME).
    ComboBox,
    /// `MSScrollContainer::Paint` after `Container::Paint` (VERT / HORZ: the scrollbars visible, TRANSPARENT,
    /// DRAW_FRAME, DRAW_3D; `a`, `b`, `c`, `d` the vertical bar's left, top, width and height, `e` the horizontal
    /// bar's height).
    ScrollContainer,
    /// `MSEtchedFrameGroup::Paint` after `Container::Paint` (`a` the label's height, `b` its left, `c` its right).
    EtchedGroup,
    /// `WONMSSkin::MSImage::Paint` (`a` the image type, `b` the image flags; the size is the image's).
    SkinImage,
}

/// One paint's inputs: the kind, the state bits (`EE_WON_MSPAINT_*`), the size and the kind's arguments.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct EeWonMsPaint {
    pub kind: EeWonMsPaintKind,
    pub flags: u32,
    pub width: i32,
    pub height: i32,
    pub a: i32,
    pub b: i32,
    pub c: i32,
    pub d: i32,
    pub e: i32,
}

/// `Direction_Left`, `_Right`, `_Up`, `_Down`.
const DIR_LEFT: i32 = 0;
const DIR_RIGHT: i32 = 1;
const DIR_UP: i32 = 2;
const DIR_DOWN: i32 = 3;

/// `ListSortType_Ascending`, `_Descending`.
const SORT_ASCENDING: i32 = 0;
const SORT_DESCENDING: i32 = 1;
const SORT_NONE: i32 = 2;

/// WONMSSkin's `MSImageFlag_*`.
const IMG_DOWN: i32 = 1;
const IMG_DEFAULT: i32 = 2;
const IMG_DISABLED: i32 = 4;
const IMG_CHECKED: i32 = 8;

/// A plan being written: the original's Graphics calls.
#[derive(Default)]
pub struct Plan {
    pub ops: Vec<EeWonDrawOp>,
}

impl Plan {
    fn push(&mut self, kind: i32, args: &[i32]) {
        let mut o = EeWonDrawOp { kind, ..Default::default() };
        let slots = [&mut o.a, &mut o.b, &mut o.c, &mut o.d, &mut o.e, &mut o.f];
        for (s, v) in slots.into_iter().zip(args) {
            *s = *v;
        }
        self.ops.push(o);
    }
    fn sys(&mut self, c: i32) {
        self.push(EE_WON_DRAW_SET_SYS_COLOR, &[c]);
    }
    fn line(&mut self, x1: i32, y1: i32, x2: i32, y2: i32) {
        self.push(EE_WON_DRAW_LINE, &[x1, y1, x2, y2]);
    }
    fn rect(&mut self, x: i32, y: i32, w: i32, h: i32) {
        self.push(EE_WON_DRAW_RECT, &[x, y, w, h]);
    }
    fn fill(&mut self, x: i32, y: i32, w: i32, h: i32) {
        self.push(EE_WON_DRAW_FILL_RECT, &[x, y, w, h]);
    }
    fn translate(&mut self, x: i32, y: i32) {
        self.push(EE_WON_DRAW_TRANSLATE, &[x, y]);
    }
    fn sys_pixel(&mut self, x: i32, y: i32, c: i32) {
        self.push(EE_WON_DRAW_SYS_PIXEL, &[x, y, c]);
    }
    fn text(&mut self, x: i32, y: i32) {
        self.push(EE_WON_DRAW_CALL, &[EE_WON_MSPAINT_CALL_TEXT, x, y]);
    }

    /// `LobbyDraw3DFrame`: a two-pixel 3D frame, raised or sunken.
    pub fn frame_3d(&mut self, x: i32, y: i32, w: i32, h: i32, down: bool) {
        self.translate(x, y);
        self.sys(SYS_3D_HILIGHT);
        self.rect(0, 0, w, h);
        self.sys(SYS_3D_DARK_SHADOW);
        if down {
            self.line(0, 0, w.wrapping_sub(1), 0);
            self.line(0, 0, 0, h.wrapping_sub(1));
            self.sys(SYS_3D_FACE);
            self.line(w.wrapping_sub(2), 1, w.wrapping_sub(2), h.wrapping_sub(2));
            self.line(w.wrapping_sub(2), h.wrapping_sub(2), 2, h.wrapping_sub(2));
            self.sys(SYS_3D_SHADOW);
            self.line(1, 1, w.wrapping_sub(2), 1);
            self.line(1, 1, 1, h.wrapping_sub(2));
        } else {
            self.line(w.wrapping_sub(1), 0, w.wrapping_sub(1), h.wrapping_sub(1));
            self.line(0, h.wrapping_sub(1), w.wrapping_sub(1), h.wrapping_sub(1));
            self.sys(SYS_3D_SHADOW);
            self.line(1, h.wrapping_sub(2), w.wrapping_sub(2), h.wrapping_sub(2));
            self.line(w.wrapping_sub(2), h.wrapping_sub(2), w.wrapping_sub(2), 1);
        }
        self.translate(x.wrapping_neg(), y.wrapping_neg());
    }

    /// `LobbyDrawFrame`: a raised button frame, or a flat shadow rectangle when down.
    pub fn frame(&mut self, x: i32, y: i32, w: i32, h: i32, down: bool) {
        self.translate(x, y);
        if !down {
            self.sys(SYS_3D_DARK_SHADOW);
            self.line(w.wrapping_sub(1), 0, w.wrapping_sub(1), h.wrapping_sub(1));
            self.line(0, h.wrapping_sub(1), w.wrapping_sub(1), h.wrapping_sub(1));
            self.sys(SYS_3D_HILIGHT);
            self.line(1, 1, 1, h.wrapping_sub(2));
            self.line(1, 1, w.wrapping_sub(2), 1);
            self.sys(SYS_3D_SHADOW);
            self.line(1, h.wrapping_sub(2), w.wrapping_sub(2), h.wrapping_sub(2));
            self.line(w.wrapping_sub(2), h.wrapping_sub(2), w.wrapping_sub(2), 1);
        } else {
            self.sys(SYS_3D_SHADOW);
            self.rect(0, 0, w, h);
        }
        self.translate(x.wrapping_neg(), y.wrapping_neg());
    }

    /// `MSGraphicsUtil::DrawFrame3`: a sunken frame inside a dark outline.
    pub fn frame3(&mut self, x: i32, y: i32, w: i32, h: i32) {
        self.translate(x, y);
        self.sys(SYS_3D_DARK_SHADOW);
        self.rect(0, 0, w, h);
        self.sys(SYS_3D_HILIGHT);
        self.line(w.wrapping_sub(1), 0, w.wrapping_sub(1), h.wrapping_sub(1));
        self.line(0, h.wrapping_sub(1), w.wrapping_sub(1), h.wrapping_sub(1));
        self.sys(SYS_3D_SHADOW);
        self.line(1, 1, w.wrapping_sub(2), 1);
        self.line(1, 1, 1, h.wrapping_sub(2));
        self.sys(SYS_3D_FACE);
        self.line(w.wrapping_sub(2), 1, w.wrapping_sub(2), h.wrapping_sub(2));
        self.line(1, h.wrapping_sub(2), w.wrapping_sub(2), h.wrapping_sub(2));
        self.translate(x.wrapping_neg(), y.wrapping_neg());
    }

    /// `MSGraphicsUtil::DrawArrow`: four strokes centred in the size, grey with a highlight when disabled.
    pub fn arrow(&mut self, dir: i32, w: i32, h: i32, disabled: bool) {
        self.sys(if disabled { SYS_GRAY_TEXT } else { SYS_BUTTON_TEXT });
        const SIZE: i32 = 4;
        const HALF: i32 = SIZE / 2;
        match dir {
            DIR_LEFT => {
                let (x, y) = ((w / 2).wrapping_sub(HALF), h / 2);
                for i in 0..SIZE {
                    self.line(x + i, y - i, x + i, y + i);
                }
                if disabled {
                    let i = SIZE;
                    self.sys(SYS_3D_HILIGHT);
                    self.line(
                        x.wrapping_add(i),
                        y.wrapping_sub(i).wrapping_add(HALF),
                        x.wrapping_add(i),
                        y.wrapping_add(i),
                    );
                }
            }
            DIR_RIGHT => {
                let (x, y) = ((w / 2).wrapping_sub(HALF), h / 2);
                for i in 0..SIZE {
                    self.line(x.wrapping_add(3 - i), y.wrapping_sub(i), x.wrapping_add(3 - i), y.wrapping_add(i));
                    if !disabled || i >= 3 {
                        continue;
                    }
                    self.sys(SYS_3D_HILIGHT);
                    let xx = x.wrapping_add(SIZE - i - 1);
                    self.line(xx, y.wrapping_add(i + 1), xx, y.wrapping_add(i + 2));
                    self.sys(SYS_GRAY_TEXT);
                }
                if disabled {
                    self.sys_pixel(x.wrapping_add(4), y.wrapping_add(1), SYS_3D_HILIGHT);
                }
            }
            DIR_UP => {
                let (x, y) = (w / 2, (h / 2).wrapping_sub(HALF));
                for i in 0..SIZE {
                    self.line(x.wrapping_sub(i), y.wrapping_add(i), x.wrapping_add(i), y.wrapping_add(i));
                }
                if disabled {
                    let i = SIZE;
                    self.sys(SYS_3D_HILIGHT);
                    self.line(
                        x.wrapping_sub(i).wrapping_add(2),
                        y.wrapping_add(i),
                        x.wrapping_add(i),
                        y.wrapping_add(i),
                    );
                }
            }
            DIR_DOWN => {
                let (x, y) = (w / 2, (h / 2).wrapping_sub(HALF));
                for i in 0..SIZE {
                    let r = SIZE - i - 1;
                    self.line(x.wrapping_sub(r), y.wrapping_add(i), x.wrapping_add(r), y.wrapping_add(i));
                    if !disabled || i <= 0 {
                        continue;
                    }
                    self.sys(SYS_3D_HILIGHT);
                    self.line(x.wrapping_add(r + 1), y.wrapping_add(i), x.wrapping_add(r + 2), y.wrapping_add(i));
                    self.sys(SYS_GRAY_TEXT);
                }
                if disabled {
                    self.sys_pixel(x.wrapping_add(1), y.wrapping_add(SIZE), SYS_3D_HILIGHT);
                }
            }
            _ => {}
        }
    }

    /// `MSGraphicsUtil::DrawCheck`: a sunken box (face colour while pressed) with a tick when checked.
    pub fn check(&mut self, x: i32, y: i32, w: i32, h: i32, down: bool, checked: bool) {
        self.translate(x, y);
        self.sys(if down { SYS_3D_FACE } else { SYS_BACK });
        self.fill(0, 0, w, h);
        self.sys(SYS_3D_SHADOW);
        self.line(0, 0, w.wrapping_sub(1), 0);
        self.line(0, 0, 0, h.wrapping_sub(1));
        self.sys(SYS_3D_HILIGHT);
        self.line(w.wrapping_sub(1), 0, w.wrapping_sub(1), h.wrapping_sub(1));
        self.line(0, h.wrapping_sub(1), w.wrapping_sub(1), h.wrapping_sub(1));
        self.sys(SYS_3D_DARK_SHADOW);
        self.line(1, 1, w.wrapping_sub(2), 1);
        self.line(1, 1, 1, h.wrapping_sub(2));
        self.sys(SYS_3D_FACE);
        self.line(w.wrapping_sub(2), 1, w.wrapping_sub(2), h.wrapping_sub(2));
        self.line(1, h.wrapping_sub(2), w.wrapping_sub(2), h.wrapping_sub(2));
        if checked {
            self.sys(SYS_TEXT);
            let mut tx = 3i32;
            let mut ty = (h / 2).wrapping_sub(1);
            for _ in 0..3 {
                self.line(tx, ty, tx, ty.wrapping_add(2));
                tx = tx.wrapping_add(1);
                ty = ty.wrapping_add(1);
            }
            ty = ty.wrapping_sub(2);
            for _ in 0..4 {
                self.line(tx, ty, tx, ty.wrapping_add(2));
                tx = tx.wrapping_add(1);
                ty = ty.wrapping_sub(1);
            }
        }
        self.translate(x.wrapping_neg(), y.wrapping_neg());
    }

    /// `MSGraphicsUtil::DrawRadio`: a sunken circle (face colour while pressed) with a dot when checked.
    pub fn radio(&mut self, x: i32, y: i32, w: i32, h: i32, down: bool, checked: bool) {
        self.translate(x, y);
        self.sys(if down { SYS_3D_FACE } else { SYS_BACK });
        self.push(EE_WON_DRAW_FILL_ELLIPSE, &[0, 0, w, h]);
        let (w2, h2) = (w.wrapping_sub(2), h.wrapping_sub(2));
        self.sys(SYS_3D_SHADOW);
        self.push(EE_WON_DRAW_ARC, &[0, 0, w, h, 45, 180]);
        self.sys(SYS_3D_DARK_SHADOW);
        self.push(EE_WON_DRAW_ARC, &[1, 1, w2, h2, 45, 180]);
        self.sys(SYS_3D_HILIGHT);
        self.push(EE_WON_DRAW_ARC, &[0, 0, w, h, 235, 180]);
        self.sys(SYS_3D_FACE);
        self.push(EE_WON_DRAW_ARC, &[1, 1, w2, h2, 235, 180]);
        if checked {
            self.sys(SYS_TEXT);
            self.push(EE_WON_DRAW_FILL_ELLIPSE, &[(w / 2).wrapping_sub(2), (h / 2).wrapping_sub(2), 4, 4]);
        }
        self.translate(x.wrapping_neg(), y.wrapping_neg());
    }

    /// `MSGraphicsUtil::DrawTab`: a tab with rounded top corners.
    pub fn tab(&mut self, x: i32, y: i32, w: i32, h: i32) {
        let (w1, w2, h1) = (w.wrapping_sub(1), w.wrapping_sub(2), h.wrapping_sub(1));
        self.sys(SYS_3D_FACE);
        self.fill(x, y.wrapping_add(2), w.wrapping_sub(x), h.wrapping_sub(y).wrapping_sub(2));
        self.line(x.wrapping_add(2), y.wrapping_add(1), w1, y.wrapping_add(1));
        self.line(x.wrapping_add(1), y.wrapping_add(2), w1, y.wrapping_add(2));
        self.sys(SYS_3D_HILIGHT);
        self.line(x, y.wrapping_add(2), x, h1);
        self.sys_pixel(x.wrapping_add(1), y.wrapping_add(1), SYS_3D_HILIGHT);
        self.line(x.wrapping_add(2), y, w2, y);
        self.sys(SYS_3D_SHADOW);
        self.line(w2, y.wrapping_add(2), w2, h1);
        self.sys(SYS_3D_DARK_SHADOW);
        self.sys_pixel(w2, y.wrapping_add(1), SYS_3D_DARK_SHADOW);
        self.line(w1, y.wrapping_add(2), w1, h1);
    }

    /// `MSGraphicsUtil::DrawCloseX`: the 8 x 8 X centred in the size, offset by x and y.
    pub fn close_x(&mut self, x: i32, y: i32, w: i32, h: i32) {
        self.translate(x, y);
        self.sys(SYS_BUTTON_TEXT);
        let (hmid, vmid) = (w / 2, h / 2);
        let (left, right) = (hmid.wrapping_sub(4), hmid.wrapping_add(3));
        let (top, bottom) = (vmid.wrapping_sub(4), vmid.wrapping_add(3));
        for i in 0..4 {
            self.line(left.wrapping_add(i), top.wrapping_add(i), left.wrapping_add(i + 1), top.wrapping_add(i));
            self.line(right.wrapping_sub(i), top.wrapping_add(i), right.wrapping_sub(1 + i), top.wrapping_add(i));
            self.line(left.wrapping_add(i), bottom.wrapping_sub(i), left.wrapping_add(i + 1), bottom.wrapping_sub(i));
            self.line(right.wrapping_sub(i), bottom.wrapping_sub(i), right.wrapping_sub(i + 1), bottom.wrapping_sub(i));
        }
        self.translate(x.wrapping_neg(), y.wrapping_neg());
    }

    /// `WONMSSkin::MSImage::DrawPushButton`: the face and frame; a default or pressed image (when asked) gets a
    /// dark outline first, and pressed then uses the flat frame.
    fn push_button(&mut self, w: i32, h: i32, flags: i32, frame2: bool, check_default: bool) {
        let down = flags & IMG_DOWN != 0;
        let (mut x, mut y, mut width, mut height) = (0, 0, w, h);
        let mut frame2 = frame2;
        self.sys(SYS_3D_FACE);
        self.fill(0, 0, width, height);
        if check_default && (flags & IMG_DEFAULT != 0 || down) {
            self.sys(SYS_3D_DARK_SHADOW);
            self.rect(0, 0, width, height);
            x += 1;
            y += 1;
            width = width.wrapping_sub(2);
            height = height.wrapping_sub(2);
            if down {
                frame2 = true;
            }
        }
        if frame2 {
            self.frame(x, y, width, height, down);
        } else {
            self.frame_3d(x, y, width, height, down);
        }
    }

    /// `WONMSSkin::MSImage::Paint`: by the image's type; an unknown type draws nothing.
    fn skin_image(&mut self, ty: i32, flags: i32, w: i32, h: i32) {
        let down = flags & IMG_DOWN != 0;
        let arrow = |p: &mut Plan, dir: i32| {
            p.push_button(w, h, flags, true, false);
            p.arrow(dir, w, h, flags & IMG_DISABLED != 0);
        };
        match ty {
            0 => self.frame_3d(0, 0, w, h, down),
            1 => self.frame(0, 0, w, h, down),
            2 => self.frame3(0, 0, w, h),
            3 => arrow(self, DIR_LEFT),
            4 => arrow(self, DIR_RIGHT),
            5 => arrow(self, DIR_UP),
            6 => arrow(self, DIR_DOWN),
            7 => self.push_button(w, h, flags, false, true),
            8 => self.check(0, 0, w, h, down, flags & IMG_CHECKED != 0),
            9 => self.radio(0, 0, w, h, down, flags & IMG_CHECKED != 0),
            10 | 13 => self.push_button(w, h, flags, true, false),
            11 => {
                self.sys(SYS_SCROLLBAR);
                self.fill(0, 0, w, h);
            }
            12 => {
                self.sys(SYS_3D_FACE);
                self.fill(0, 0, w, h);
            }
            14 => self.tab(0, 0, w, h),
            15 => self.push_button(w, h, flags, false, false),
            16 => {
                self.sys(SYS_3D_DARK_SHADOW);
                self.rect(0, 0, w, h);
            }
            17 => {
                self.push_button(w, h, flags, false, false);
                let o = down as i32;
                self.close_x(o, o, w, h);
            }
            _ => {}
        }
    }

    /// The plan of one paint.
    pub fn paint(&mut self, p: &EeWonMsPaint) {
        type K = EeWonMsPaintKind;
        let on = |bit: u32| p.flags & bit != 0;
        let (w, h) = (p.width, p.height);
        let down = on(EE_WON_MSPAINT_PRESSED);
        match p.kind {
            K::Frame3D => self.frame_3d(p.a, p.b, w, h, down),
            K::Frame => self.frame(p.a, p.b, w, h, down),
            K::Frame3 => self.frame3(p.a, p.b, w, h),
            K::Arrow => self.arrow(p.a, w, h, on(EE_WON_MSPAINT_DISABLED)),
            K::Check => self.check(p.a, p.b, w, h, down, on(EE_WON_MSPAINT_CHECKED)),
            K::Radio => self.radio(p.a, p.b, w, h, down, on(EE_WON_MSPAINT_CHECKED)),
            K::Tab => self.tab(p.a, p.b, w, h),
            K::CloseX => self.close_x(p.a, p.b, w, h),
            K::Button => {
                let offset = down as i32;
                self.sys(SYS_3D_FACE);
                if !on(EE_WON_MSPAINT_TRANSPARENT) {
                    self.fill(0, 0, w, h);
                }
                let (mut x, mut y, mut width, mut height) = (0, 0, w, h);
                let mut frame2 = on(EE_WON_MSPAINT_FRAME2);
                if on(EE_WON_MSPAINT_DEFAULT) {
                    self.sys(SYS_3D_DARK_SHADOW);
                    self.rect(0, 0, w, h);
                    x += 1;
                    y += 1;
                    width = width.wrapping_sub(2);
                    height = height.wrapping_sub(2);
                    if down {
                        frame2 = true;
                    }
                }
                if frame2 {
                    self.frame(x, y, width, height, down);
                } else {
                    self.frame_3d(x, y, width, height, down);
                }
                self.text(offset, offset);
            }
            K::TabButton => {
                self.tab(0, 0, w, h);
                self.text(0, 0);
            }
            K::Label => {
                self.sys(SYS_3D_FACE);
                if !on(EE_WON_MSPAINT_TRANSPARENT) {
                    self.fill(0, 0, w, h);
                }
                self.text(0, 0);
            }
            K::HeaderSort => {
                if p.a == SORT_NONE {
                    return;
                }
                self.sys(SYS_BUTTON_TEXT);
                let mut x = if p.b == 0 { w.wrapping_sub(4) / 2 } else { p.b.wrapping_add(10).wrapping_add(p.c) };
                let mut y = (h / 2).wrapping_sub(2);
                let offset = down as i32;
                x = x.wrapping_add(offset);
                y = y.wrapping_add(offset);
                if p.a != SORT_ASCENDING && p.a != SORT_DESCENDING {
                    return;
                }
                for i in 0..4 {
                    let r = if p.a == SORT_ASCENDING { i } else { 3 - i };
                    self.line(x.wrapping_sub(r), y.wrapping_add(i), x.wrapping_add(r), y.wrapping_add(i));
                }
            }
            K::CloseButton => {
                let offset = down as i32;
                self.sys(SYS_3D_FACE);
                self.fill(0, 0, w, h);
                self.frame_3d(0, 0, w, h, down);
                self.close_x(offset, offset, w, h);
            }
            K::ScrollThumb => {
                self.sys(SYS_3D_FACE);
                self.fill(0, 0, w, h);
                self.frame(0, 0, w, h, false);
            }
            K::ArrowButton => {
                self.sys(SYS_3D_FACE);
                self.fill(0, 0, w, h);
                self.frame(0, 0, w, h, down);
                self.arrow(p.a, w, h, on(EE_WON_MSPAINT_DISABLED));
            }
            K::Frame3DComponent => match p.a {
                1 => self.frame_3d(0, 0, w, h, down),
                2 => self.frame(0, 0, w, h, down),
                3 => self.frame3(0, 0, w, h),
                _ => {}
            },
            K::Popup => {
                let (w1, w2, h1, h2) = (w.wrapping_sub(1), w.wrapping_sub(2), h.wrapping_sub(1), h.wrapping_sub(2));
                self.sys(SYS_MENU_BACK);
                self.fill(0, 0, w, h);
                self.sys(SYS_3D_HILIGHT);
                self.line(1, 1, w2, 1);
                self.line(1, 1, 1, h2);
                self.sys(SYS_3D_DARK_SHADOW);
                self.line(w1, 0, w1, h1);
                self.line(1, h1, w1, h1);
                self.sys(SYS_3D_SHADOW);
                self.line(w2, 0, w2, h2);
                self.line(2, h2, w2, h2);
            }
            K::DlgBack => {
                self.sys(SYS_3D_FACE);
                self.fill(0, 0, w, h);
            }
            K::EtchedFrame => {
                self.sys(SYS_3D_HILIGHT);
                self.rect(0, 0, w, h);
                self.rect(1, 1, w.wrapping_sub(1), h.wrapping_sub(1));
                self.sys(SYS_3D_SHADOW);
                self.rect(0, 0, w.wrapping_sub(1), h.wrapping_sub(1));
            }
            K::InputBox => self.frame3(0, 0, w, h),
            K::Scrollbar => {
                if on(EE_WON_MSPAINT_INVALID) && !on(EE_WON_MSPAINT_TRANSPARENT) {
                    self.sys(SYS_SCROLLBAR);
                    self.fill(p.a, p.b, p.c, p.d);
                }
            }
            K::ComboBox => {
                if on(EE_WON_MSPAINT_DRAW_FRAME) {
                    self.frame3(0, 0, w, h);
                }
            }
            K::ScrollContainer => {
                if on(EE_WON_MSPAINT_VERT) && on(EE_WON_MSPAINT_HORZ) && !on(EE_WON_MSPAINT_TRANSPARENT) {
                    self.sys(SYS_SCROLLBAR);
                    self.fill(p.a, p.b.wrapping_add(p.d), p.c, p.e);
                }
                if !on(EE_WON_MSPAINT_DRAW_FRAME) {
                    return;
                }
                if on(EE_WON_MSPAINT_DRAW_3D) {
                    self.frame_3d(0, 0, w, h, true);
                } else {
                    self.sys(SYS_3D_DARK_SHADOW);
                    self.rect(0, 0, w, h);
                }
            }
            K::EtchedGroup => {
                let dy = p.a / 2;
                self.translate(0, dy);
                let right = w.wrapping_sub(1);
                let bottom = h.wrapping_sub(1).wrapping_sub(dy);
                let (ll, lr) = (p.b, p.c);
                self.sys(SYS_3D_HILIGHT);
                self.line(0, 1, ll, 1);
                self.line(lr, 1, right, 1);
                self.line(right, 0, right, bottom);
                self.line(0, bottom, right, bottom);
                self.line(1, 1, 1, bottom);
                self.sys(SYS_3D_SHADOW);
                let (r1, b1) = (right.wrapping_sub(1), bottom.wrapping_sub(1));
                self.line(0, 0, ll, 0);
                self.line(lr, 0, r1, 0);
                self.line(r1, 0, r1, b1);
                self.line(0, b1, r1, b1);
                self.line(0, 0, 0, b1);
                self.translate(0, dy.wrapping_neg());
            }
            K::SkinImage => self.skin_image(p.a, p.b, w, h),
        }
    }
}

/// The plan of `p` (an [`EeWonMsPaintKind`] paint) into `out` (`cap` entries); answers the plan's length, which
/// may be more than `cap` (the C++ asks again with room).
///
/// # Safety
/// `p` is live; `out` has room for `cap` operations (or is NULL with `cap` 0).
#[no_mangle]
pub unsafe extern "C" fn ee_won_mspaint_plan(p: *const EeWonMsPaint, out: *mut EeWonDrawOp, cap: usize) -> usize {
    let mut plan = Plan::default();
    // SAFETY: the contract
    plan.paint(unsafe { &*p });
    let n = plan.ops.len();
    if !out.is_null() {
        for (i, o) in plan.ops.iter().take(cap).enumerate() {
            // SAFETY: i < cap
            unsafe { *out.add(i) = *o };
        }
    }
    n
}

#[cfg(test)]
#[path = "mspaint_tests.rs"]
mod tests;
