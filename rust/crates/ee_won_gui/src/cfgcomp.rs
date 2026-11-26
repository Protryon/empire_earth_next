//! The table reader's compound setters and reads composed in Rust (R6 WON round 34).
//!
//! A setter or read of the table reader (cfgtab.rs) that was more than one C++ statement in the host
//! (include/ee/wonapi/WONCfgTable.inl's `CfgTabSet` and `CfgTabRead`: a condition, a local, a loop over the layout
//! state, the stretch and filter configs' children with their names) is composed here over the host's one-statement
//! ops (`CfgTabOp`). The host's case calls [`ee_won_cfg_comp`] with the composition's number; an op whose statement
//! can throw runs under the host's guard, which keeps the exception in the frame, and the composition stops (the
//! host's locals it made released as the original's unwinding released them) and answers false; the host rethrows.
//! A failure the original threw itself (`Type not found: `, `Invalid LayoutChain`, `Expecting ScrollArea`) is the
//! frame's `Fail` op (the original's exception, kept) and false.
//!
//! `ComponentConfig::Virtual24`'s walk of the kind's steps is [`ee_won_cfg_v24`] over a host of three ops.

use std::ffi::c_void;

use crate::skinobj::EeWonSkinSlot as Sl;

use crate::cfgkind::{v24_steps, EeWonCfgKind};
use crate::guiconf::*;

/// A composition (one compound case of the host).
#[repr(i32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonCfgComp {
    /// WONSimpleButtonConfig's TYPE: the skin's type copied, else "Type not found: " and the name.
    SimpleType = 0,
    /// WONButtonConfig's TYPE, the same over the WON button.
    ButtonType,
    /// WONButtonConfig's HORZTEXTPAD: the left pad and the right one (the button's own when only one is given).
    HorzTextPad,
    /// WONScrollbarConfig's SCROLLRECT: a new button as the scroll rectangle when the flag is set.
    ScrollRect,
    /// WONScrollContainerConfig's FRAME flag: no frame when it is clear.
    ScrollContNoFrame,
    /// ComponentConfig's LEFT: the position's x, the top kept.
    Left,
    /// ComponentConfig's TOP: the position's y, the left kept.
    Top,
    /// ComponentConfig's WIDTH: the width, the height kept.
    Width,
    /// ComponentConfig's HEIGHT: the height, the width kept.
    Height,
    /// ComponentConfig's FOCUS: the component asks for the focus when the flag is set.
    Focus,
    /// WONSkinConfig's RADIOBUTTON: the skin's radio button, its radio flag set.
    SkinRadio,
    /// WONSkinConfig's TABCTRL: the skin's tab control, its skin made.
    SkinTabCtrl,
    /// FontConfig's style flag: set or cleared in the descriptor.
    FontStyle,
    /// ContainerConfig's NEWLAYOUTPOINT: a layout point with a config of its own under the name.
    NewLayoutPoint,
    /// ContainerConfig's ENABLELAYOUT: the layout id (the last int read) enabled or disabled.
    EnableLayout,
    /// The LAYOUT family: the previous rule's instruction and offsets pushed.
    LayoutPrev,
    /// The LAYOUT family: the previous rule's reference pushed ("Invalid LayoutChain" without one).
    LayoutChain,
    /// The LAYOUT family: the four offsets read kept.
    LayoutOffsets,
    /// The LAYOUT family: the rule added (the frame's last six ints).
    LayoutRule,
    /// StretchImageConfig's EXPORT by number: the image and config names, the child.
    StretchExport,
    /// StretchImageConfig's MULTIHORZCROP column: the cut and its child.
    StretchCut,
    /// StretchImageConfig's named child (the frame's child and raw image).
    StretchNamed,
    /// StretchImageConfig's child made (the frame's child).
    StretchNewChild,
    /// ImageFilterConfig's MULTIHORZCROP column: the cut and its child.
    FilterCut,
    /// ImageFilterConfig's named child of the raw image.
    FilterNamed,
    /// The read of a scroll area ("Expecting ScrollArea" for another component).
    ReadScrollArea,
    /// The read of WONFrameValueConfig's frame: a stretch image alone, or an image and one border or four.
    ReadFrame,
    /// ComponentConfig's SIZEATLEAST: the set-size flags from the size read, then the size.
    SizeAtLeast,
    /// ComponentConfig's STARTPARSETIMER: the line count reset, the timer started.
    ParseTimerStart,
    /// ComponentConfig's STOPPARSETIMER: the timer stopped, the lines parsed kept.
    ParseTimerStop,
    /// WONSkinConfig's CURSOR: a standard cursor made of the image (the cursor held while it is set).
    SkinCursor,
    /// ContainerConfig's SPACEEVENLYH: a horizontal space-evenly layout of the last component; the ints used.
    SpaceEvenlyH,
    /// ContainerConfig's SPACEEVENLYV, vertical.
    SpaceEvenlyV,
    /// ContainerConfig's CLEARRIGHT layout; the ints used.
    ClearRight,
    /// ContainerConfig's SIZEATLEAST layout; the ints used.
    SizeAtLeastLayout,
    /// ContainerConfig's NOOVERLAP layout; the components used.
    NoOverlap,
}

/// The host's one-statement ops over its frame (`CfgTabFrame`: the ints `i`, the strings `s`, the raw strings
/// `rs`, the components `c`, the frame's config and its kind's typed component).
#[repr(i32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonCfgTabOp {
    /// `i[x]`.
    Int = 0,
    /// The ints' count.
    IntCount,
    /// `i[--ni]`.
    PopInt,
    /// `i[ni++] = x`.
    PushInt,
    /// `c[nc++] = x`.
    PushComp,
    /// The `Fail` callback: the original's ConfigObjectException of the `y` bytes at `x`, then string `z` (none if
    /// < 0), kept.
    Fail,
    /// `ee_won_skin_copy_type` of the simple button type `s[0]` onto the component (guarded).
    SkinCopySimple,
    /// The same of the WON button type.
    SkinCopyButton,
    /// The WON button's `GetRightTextPad()`.
    RightTextPad,
    /// The WON button's `SetHorzTextPad(x, y)`.
    SetHorzTextPad,
    /// The scrollbar's `SetScrollRect(new Button)`.
    NewScrollRect,
    /// The scroll container's frame set to none.
    ClearScrollFrame,
    /// The component's `Left()`.
    CompLeft,
    /// The component's `Top()`.
    CompTop,
    /// The component's `Width()`.
    CompWidth,
    /// The component's `Height()`.
    CompHeight,
    /// The component's `SetPos(x, y)`.
    CompSetPos,
    /// The component's `SetSize(x, y)`.
    CompSetSize,
    /// The component's `RequestFocus()`.
    RequestFocus,
    /// `SafeConfigGetComponent<WONButton*>(c[0], "WONButton")` (guarded).
    SafeWonButton,
    /// `WONSkinSet((EeWonSkinSlot)x, y, (Component*)z)`: the skin's slot `x` (index `y`) = component `z`.
    SkinSet,
    /// `ee_won_skin_set_type(WONSkinGlobal(), x != 0, &s[0], (Component*)y)`: the button type `s[0]` (simple: `x`).
    SkinSetType,
    /// The WON button `x`'s `SetButtonFlags(8, true)`.
    RadioFlag,
    /// `SafeConfigGetSkinPart(c[0], (const char*)x)` (guarded; `x` a NUL-terminated name).
    SafePart,
    /// `WONShell::Op(x, SetSkin, 0)`.
    ShellSetSkin,
    /// The font config's descriptor style.
    FontStyleOf,
    /// The font config's descriptor style = `x`.
    SetFontStyle,
    /// A new `SmartPtr<ComponentConfig>` holding a new ComponentConfig.
    PointConfigNew,
    /// A new `SmartPtr<LayoutPoint>` holding a new LayoutPoint.
    PointNew,
    /// Config holder `x`'s `SetComponent` of point holder `y`'s point.
    PointBind,
    /// Config holder `x`'s parent = the frame's config.
    PointParent,
    /// `AddChildToConfigMap(s[--ns], config holder x's config, true)` of the frame's config.
    PointAddConfig,
    /// The container's `AddLayoutPoint` of point holder `x`'s point.
    PointAdd,
    /// Point holder `x` deleted.
    PointFree,
    /// Config holder `x` deleted.
    PointConfigFree,
    /// The container's `EnableLayoutInstructions(x, y != 0)`.
    EnableLayout,
    /// The container config's `mLayoutInstruction`.
    LayoutInstruction,
    /// The container config's `mLayoutParams[x]`.
    LayoutParam,
    /// The container config's `mLayoutRef`.
    LayoutRef,
    /// The container config's `mLayoutParams[x] = y`.
    SetLayoutParam,
    /// The container's `Fn2F6140(c[0], v[0], c[1], v[1], v[2], v[3], v[4], mCurLayoutId)` of the five ints at `x`.
    AddLayoutRule,
    /// The container config's `mLayoutRef = c[x]`.
    SetLayoutRef,
    /// The container config's `mLayoutInstruction = x`.
    SetLayoutInstruction,
    /// The raw string `rs[x]` appended to the `Vec<u8>` at `y` (`ee_won_cfg_comp_append`).
    RawStr,
    /// The string `s[x]` narrowed, appended to the `Vec<u8>` at `y`.
    StrNarrow,
    /// The ints' count = `x`.
    SetIntCount,
    /// A new `SmartPtr<RawImage>` of `GetRawImageFromStr(the x bytes, y long)` (guarded).
    RawFromStr,
    /// A new `SmartPtr<RawImage>` of `ImageCrop(the frame's raw image, x, 0, y, its height)`.
    CropRaw,
    /// A new `SmartPtr<RawImage>` of `ImageCrop(the filter config's raw image, x, 0, y, its height)`.
    CropFilter,
    /// The `SmartPtr<RawImage>` holder `x`'s image.
    HeldRaw,
    /// Raw image holder `x` deleted.
    RawFree,
    /// A new StretchImageConfig (no reference).
    StretchNew,
    /// Stretch config `x`'s stretch image `CopyAttributes` of the frame's config's.
    StretchCopy,
    /// A new `SmartPtr<StretchImageConfig>` holding stretch config `x`.
    StretchHold,
    /// Stretch holder `x` deleted.
    StretchFree,
    /// Stretch config `x`'s stretch image `SetImage(y)`.
    StretchSetImage,
    /// The frame config's parent's `AddChildToConfigMap(name the y bytes at x, stretch config z)`.
    StretchToParent,
    /// The same under the name `s[0]`.
    StretchToParentS0,
    /// The frame's child (`child.get()`).
    FrameChild,
    /// The frame's raw image (`raw.get()`).
    FrameRaw,
    /// The frame's child = stretch config `x`.
    SetFrameChild,
    /// A new `SmartPtr<ImageFilterConfig>` holding a new ImageFilterConfig.
    FilterNew,
    /// Filter holder `x`'s `SetRawImage(y)`.
    FilterSetRaw,
    /// The frame config's parent's `AddChildToConfigMap(name the y bytes at x, filter holder z's config)`.
    FilterToParent,
    /// The same under the name `s[0]`.
    FilterToParentS0,
    /// Filter holder `x` deleted.
    FilterFree,
    /// The config's `ReadComponent()` (guarded).
    ReadComp,
    /// Whether component `x` is a ScrollArea.
    IsScrollArea,
    /// `c[nc++] = dynamic_cast<ScrollArea*>(x)`.
    PushScrollArea,
    /// A new `SmartPtr<Image>` of the config's `ReadImage(false)` (guarded).
    ReadImageHold,
    /// Whether image holder `x`'s image is a StretchImage.
    IsStretch,
    /// The frame's frame = holder `x`'s stretch image.
    FrameOfStretch,
    /// The config's `EnsureComma()` (guarded).
    Comma,
    /// The config's `ReadInt()` (guarded).
    ReadInt,
    /// The config's `EndOfString()`.
    EndOfString,
    /// The frame's frame = `WONFrame(holder x's image, y)`.
    FrameOne,
    /// The frame's frame = `WONFrame(holder x's image, the four ints at y)`.
    FrameFour,
    /// Image holder `x` deleted.
    ImageFree,
    /// The config's `mHasSetWidth = x != 0`.
    SetHasWidth,
    /// The config's `mHasSetHeight = x != 0`.
    SetHasHeight,
    /// The component's `SetSizeAtLeast(x, y)`.
    CompSizeAtLeast,
    /// `ConfigParser::ResetLineCount()`.
    ResetLineCount,
    /// `PerfTimer::GlobalStart(NULL)`.
    TimerStart,
    /// `PerfTimer::GlobalStop(NULL)`.
    TimerStop,
    /// `gLinesParsed = ConfigParser::GetLineCount()`.
    KeepLinesParsed,
    /// A new `SmartPtr<Cursor>` of a new ComponentCursor of a new ImageComponent of the frame's image.
    CursorNew,
    /// `Cursor::SetStandardCursor(i[0], cursor holder x's cursor)`.
    SetStdCursor,
    /// Cursor holder `x` deleted.
    CursorFree,
    /// The frame's layout = a new SpaceEvenlyLayout(c[--nc], i[0], i[1], x != 0).
    LaySpaceEvenly,
    /// The frame's layout = a new ClearRightLayout(c[--nc], i[0]).
    LayClearRight,
    /// The frame's layout = a new SizeAtLeastLayout(c[--nc], i[0], i[1]).
    LaySizeAtLeast,
    /// The frame's layout = a new NoOverlapLayout(c[0], i[2], c[1], i[0], i[1]).
    LayNoOverlap,
    /// The components' count = `x`.
    SetCompCount,
}

/// The host's op function: `ok` cleared by an op that threw (its exception kept in the frame).
pub type EeWonCfgTabOpFn = Option<
    unsafe extern "C" fn(frame: *mut c_void, op: EeWonCfgTabOp, x: isize, y: isize, z: isize, ok: *mut bool) -> isize,
>;

/// An op threw (or the composition failed with the frame's `Fail`): the frame holds the exception.
struct Thrown;
type R<T> = Result<T, Thrown>;

struct Ops {
    f: unsafe extern "C" fn(*mut c_void, EeWonCfgTabOp, isize, isize, isize, *mut bool) -> isize,
    frame: *mut c_void,
}

impl Ops {
    fn op3(&self, op: EeWonCfgTabOp, x: isize, y: isize, z: isize) -> R<isize> {
        let mut ok = true;
        // SAFETY: the host's contract (the frame is its own)
        let r = unsafe { (self.f)(self.frame, op, x, y, z, &mut ok) };
        if ok {
            Ok(r)
        } else {
            Err(Thrown)
        }
    }
    fn op(&self, op: EeWonCfgTabOp, x: isize, y: isize) -> R<isize> {
        self.op3(op, x, y, 0)
    }
    /// An op that cannot throw.
    fn call(&self, op: EeWonCfgTabOp, x: isize, y: isize) -> isize {
        self.op(op, x, y).unwrap_or(0)
    }
    fn int(&self, slot: isize) -> i32 {
        self.call(EeWonCfgTabOp::Int, slot, 0) as i32
    }
    fn count(&self) -> isize {
        self.call(EeWonCfgTabOp::IntCount, 0, 0)
    }
    fn fail(&self, msg: &[u8], slot: isize) -> R<()> {
        self.op3(EeWonCfgTabOp::Fail, msg.as_ptr() as isize, msg.len() as isize, slot)?;
        Err(Thrown)
    }
    fn bytes(&self, op: EeWonCfgTabOp, slot: isize) -> Vec<u8> {
        let mut v: Vec<u8> = Vec::new();
        self.call(op, slot, &mut v as *mut Vec<u8> as isize);
        v
    }
    /// `CfgStretchChild`: a new stretch config with the frame config's stretch attributes.
    fn stretch_child(&self) -> isize {
        let child = self.call(EeWonCfgTabOp::StretchNew, 0, 0);
        self.call(EeWonCfgTabOp::StretchCopy, child, 0);
        child
    }
    /// `CfgStretchAdd`: the child (held while it is set) given the image, then put under the name in the parent
    /// (`name` None: the frame's `s[0]`).
    fn stretch_add(&self, name: Option<&[u8]>, child: isize, image: isize) -> R<()> {
        use EeWonCfgTabOp as O;
        let keep = self.call(O::StretchHold, child, 0);
        self.call(O::StretchSetImage, child, image);
        let r = match name {
            Some(n) => self.op3(O::StretchToParent, n.as_ptr() as isize, n.len() as isize, child),
            None => self.op3(O::StretchToParentS0, 0, 0, child),
        };
        self.call(O::StretchFree, keep, 0);
        r.map(|_| ())
    }
    /// `CfgFilterChild`: a new filter config of the image, put under the name in the parent (`name` None: `s[0]`).
    fn filter_child(&self, name: Option<&[u8]>, image: isize) -> R<()> {
        use EeWonCfgTabOp as O;
        let child = self.call(O::FilterNew, 0, 0);
        self.call(O::FilterSetRaw, child, image);
        let r = match name {
            Some(n) => self.op3(O::FilterToParent, n.as_ptr() as isize, n.len() as isize, child),
            None => self.op3(O::FilterToParentS0, 0, 0, child),
        };
        self.call(O::FilterFree, child, 0);
        r.map(|_| ())
    }
}

/// `sprintf("%s%d", text, n)`: the text to its first NUL, then the number.
fn numbered(text: &[u8], n: i32) -> Vec<u8> {
    let end = text.iter().position(|&b| b == 0).unwrap_or(text.len());
    let mut v = text[..end].to_vec();
    v.extend_from_slice(n.to_string().as_bytes());
    v
}

fn run(o: &Ops, which: EeWonCfgComp) -> R<()> {
    use EeWonCfgComp as W;
    use EeWonCfgTabOp as O;
    match which {
        W::SimpleType | W::ButtonType => {
            let op = if which == W::SimpleType { O::SkinCopySimple } else { O::SkinCopyButton };
            if o.op(op, 0, 0)? == 0 {
                o.fail(b"Type not found: ", 0)?;
            }
        }
        W::HorzTextPad => {
            let right = if o.count() > 1 { o.int(1) as isize } else { o.call(O::RightTextPad, 0, 0) };
            o.call(O::SetHorzTextPad, o.int(0) as isize, right);
        }
        W::ScrollRect => {
            if o.int(0) != 0 {
                o.call(O::NewScrollRect, 0, 0);
            }
        }
        W::ScrollContNoFrame => {
            if o.int(0) == 0 {
                o.call(O::ClearScrollFrame, 0, 0);
            }
        }
        W::Left => {
            let top = o.call(O::CompTop, 0, 0);
            o.call(O::CompSetPos, o.int(0) as isize, top);
        }
        W::Top => {
            let left = o.call(O::CompLeft, 0, 0);
            o.call(O::CompSetPos, left, o.int(0) as isize);
        }
        W::Width => {
            let height = o.call(O::CompHeight, 0, 0);
            o.call(O::CompSetSize, o.int(0) as isize, height);
        }
        W::Height => {
            let width = o.call(O::CompWidth, 0, 0);
            o.call(O::CompSetSize, width, o.int(0) as isize);
        }
        W::Focus => {
            if o.int(0) != 0 {
                o.call(O::RequestFocus, 0, 0);
            }
        }
        W::SkinRadio => {
            let radio = o.op(O::SafeWonButton, 0, 0)?;
            o.op3(O::SkinSet, Sl::RadioButton as isize, 0, radio)?;
            o.call(O::RadioFlag, radio, 0);
        }
        W::SkinTabCtrl => {
            let tab = o.op(O::SafePart, c"WONTabCtrl".as_ptr() as isize, 0)?;
            o.op3(O::SkinSet, Sl::TabCtrl as isize, 0, tab)?;
            o.call(O::ShellSetSkin, tab, 0);
        }
        W::FontStyle => {
            let style = o.call(O::FontStyleOf, 0, 0) as i32;
            let flag = o.int(1);
            let v = if o.int(0) != 0 { style | flag } else { style & !flag };
            o.call(O::SetFontStyle, v as isize, 0);
        }
        W::NewLayoutPoint => {
            // the original's locals: the config, then the point (released in reverse)
            let config = o.call(O::PointConfigNew, 0, 0);
            let point = o.call(O::PointNew, 0, 0);
            let r = (|| -> R<()> {
                o.op(O::PointBind, config, point)?;
                o.call(O::PointParent, config, 0);
                o.op(O::PointAddConfig, config, 0)?;
                o.op(O::PointAdd, point, 0)?;
                Ok(())
            })();
            o.call(O::PointFree, point, 0);
            o.call(O::PointConfigFree, config, 0);
            r?;
        }
        W::EnableLayout => {
            let id = o.call(O::PopInt, 0, 0);
            o.call(O::EnableLayout, id, (o.int(0) != 0) as isize);
        }
        W::LayoutPrev => {
            o.call(O::PushInt, o.call(O::LayoutInstruction, 0, 0), 0);
            for j in 0..4 {
                o.call(O::PushInt, o.call(O::LayoutParam, j, 0), 0);
            }
        }
        W::LayoutChain => {
            let r = o.call(O::LayoutRef, 0, 0);
            if r == 0 {
                o.fail(b"Invalid LayoutChain", -1)?;
            }
            o.call(O::PushComp, r, 0);
        }
        W::LayoutOffsets => {
            let n = o.count();
            for j in 0..4 {
                o.call(O::SetLayoutParam, j, o.int(n - 4 + j) as isize);
            }
        }
        W::LayoutRule => {
            let n = o.count();
            let v: [i32; 6] = std::array::from_fn(|k| o.int(n - 6 + k as isize));
            for j in 0..4 {
                o.call(O::SetLayoutParam, j as isize, v[1 + j] as isize);
            }
            o.op(O::AddLayoutRule, v.as_ptr() as isize, 0)?;
            o.call(O::SetLayoutRef, v[5] as isize, 0);
            o.call(O::SetLayoutInstruction, v[0] as isize, 0);
        }
        W::StretchExport => {
            let n = o.count();
            let image = numbered(&o.bytes(O::RawStr, 0), o.int(n - 2));
            let config = numbered(&o.bytes(O::RawStr, 1), o.int(n - 1));
            o.call(O::SetIntCount, n - 2, 0);
            let raw = o.op(O::RawFromStr, image.as_ptr() as isize, image.len() as isize)?;
            let r = o.stretch_add(Some(&config), o.stretch_child(), o.call(O::HeldRaw, raw, 0));
            o.call(O::RawFree, raw, 0);
            r?;
        }
        W::StretchCut => {
            let n = o.call(O::PopInt, 0, 0) as i32;
            let config = numbered(&o.bytes(O::RawStr, 0), n.wrapping_add(1));
            let w = o.int(1);
            let cut = o.call(O::CropRaw, n.wrapping_mul(w) as isize, w as isize);
            let r = o.stretch_add(Some(&config), o.stretch_child(), o.call(O::HeldRaw, cut, 0));
            o.call(O::RawFree, cut, 0);
            r?;
        }
        W::StretchNamed => {
            let child = o.call(O::FrameChild, 0, 0);
            o.stretch_add(None, child, o.call(O::FrameRaw, 0, 0))?;
        }
        W::StretchNewChild => {
            o.call(O::SetFrameChild, o.stretch_child(), 0);
        }
        W::FilterCut => {
            let n = o.call(O::PopInt, 0, 0) as i32;
            let w = o.int(1);
            let image = o.call(O::CropFilter, n.wrapping_mul(w) as isize, w as isize);
            let name = numbered(&o.bytes(O::StrNarrow, 0), n.wrapping_add(1));
            let r = o.filter_child(Some(&name), o.call(O::HeldRaw, image, 0));
            o.call(O::RawFree, image, 0);
            r?;
        }
        W::FilterNamed => o.filter_child(None, o.call(O::FrameRaw, 0, 0))?,
        W::ReadScrollArea => {
            let c = o.op(O::ReadComp, 0, 0)?;
            if o.call(O::IsScrollArea, c, 0) == 0 {
                o.fail(b"Expecting ScrollArea", -1)?;
            }
            o.call(O::PushScrollArea, c, 0);
        }
        W::SizeAtLeast => {
            let (w, h) = (o.int(0), o.int(1));
            o.call(O::SetHasWidth, (w > 0) as isize, 0);
            o.call(O::SetHasHeight, (h > 0) as isize, 0);
            o.call(O::CompSizeAtLeast, w as isize, h as isize);
        }
        W::ParseTimerStart => {
            o.call(O::ResetLineCount, 0, 0);
            o.call(O::TimerStart, 0, 0);
        }
        W::ParseTimerStop => {
            o.call(O::TimerStop, 0, 0);
            o.call(O::KeepLinesParsed, 0, 0);
        }
        W::SkinCursor => {
            let cursor = o.call(O::CursorNew, 0, 0);
            o.call(O::SetStdCursor, cursor, 0);
            o.call(O::CursorFree, cursor, 0);
        }
        W::SpaceEvenlyH | W::SpaceEvenlyV | W::ClearRight | W::SizeAtLeastLayout => {
            match which {
                W::SpaceEvenlyH => o.call(O::LaySpaceEvenly, 1, 0),
                W::SpaceEvenlyV => o.call(O::LaySpaceEvenly, 0, 0),
                W::ClearRight => o.call(O::LayClearRight, 0, 0),
                _ => o.call(O::LaySizeAtLeast, 0, 0),
            };
            // the ints the layout took are used up
            o.call(O::SetIntCount, 0, 0);
        }
        W::NoOverlap => {
            o.call(O::LayNoOverlap, 0, 0);
            o.call(O::SetCompCount, 0, 0);
        }
        W::ReadFrame => {
            let image = o.op(O::ReadImageHold, 0, 0)?;
            let r = read_frame(o, image);
            o.call(O::ImageFree, image, 0);
            r?;
        }
    }
    Ok(())
}

/// WONFrameValueConfig's frame value over the image read (`image` its holder).
fn read_frame(o: &Ops, image: isize) -> R<()> {
    use EeWonCfgTabOp as O;
    if o.call(O::IsStretch, image, 0) != 0 {
        o.call(O::FrameOfStretch, image, 0);
        return Ok(());
    }
    o.op(O::Comma, 0, 0)?;
    let left = o.op(O::ReadInt, 0, 0)?;
    if o.call(O::EndOfString, 0, 0) != 0 {
        o.call(O::FrameOne, image, left);
        return Ok(());
    }
    o.op(O::Comma, 0, 0)?;
    let mut v = [left as i32, 0, 0, 0];
    for (k, slot) in v.iter_mut().enumerate().skip(1) {
        *slot = o.op(O::ReadInt, 0, 0)? as i32;
        if k < 3 {
            o.op(O::Comma, 0, 0)?;
        }
    }
    o.call(O::FrameFour, image, v.as_ptr() as isize);
    Ok(())
}

/// The composition `which` over the host's frame: true when it ran, false when an op threw or it failed (the
/// frame holds the exception; the host rethrows it).
///
/// # Safety
/// `ops` and `frame` are the C++ host's (`CfgTabOp` and its `CfgTabFrame`).
#[no_mangle]
pub unsafe extern "C" fn ee_won_cfg_comp(ops: EeWonCfgTabOpFn, frame: *mut c_void, which: EeWonCfgComp) -> bool {
    let Some(f) = ops else { return false };
    run(&Ops { f, frame }, which).is_ok()
}

/// The composition a table setter is (`table << 8 | n`), None for another setter (R6 WON lobby and network round 39:
/// WONCfgTable.inl's CfgTabSet cases that only ran one).
fn of_setter(setter: i32) -> Option<EeWonCfgComp> {
    use crate::cfgtab::*;
    use EeWonCfgComp as W;
    Some(match (setter >> 8, setter & 0xff) {
        (EE_WON_CFG_T_SIMPLE, 1) => W::SimpleType,
        (EE_WON_CFG_T_WONBUTTON, 1) => W::ButtonType,
        (EE_WON_CFG_T_WONBUTTON, 12) => W::HorzTextPad,
        (EE_WON_CFG_T_WSCROLLBAR, 0) => W::ScrollRect,
        (EE_WON_CFG_T_WSCROLLCONT, 0) => W::ScrollContNoFrame,
        (EE_WON_CFG_T_COMPONENT, 3) => W::Left,
        (EE_WON_CFG_T_COMPONENT, 4) => W::Top,
        (EE_WON_CFG_T_COMPONENT, 5) => W::Width,
        (EE_WON_CFG_T_COMPONENT, 6) => W::Height,
        (EE_WON_CFG_T_COMPONENT, 10) => W::SizeAtLeast,
        (EE_WON_CFG_T_COMPONENT, 17) => W::Focus,
        (EE_WON_CFG_T_COMPONENT, 22) => W::ParseTimerStart,
        (EE_WON_CFG_T_COMPONENT, 23) => W::ParseTimerStop,
        (EE_WON_CFG_T_SKIN, 5) => W::SkinRadio,
        (EE_WON_CFG_T_SKIN, 16) => W::SkinTabCtrl,
        (EE_WON_CFG_T_SKIN, 21) => W::SkinCursor,
        (EE_WON_CFG_T_FONT, 2) => W::FontStyle,
        (EE_WON_CFG_T_CONTAINER, 2) => W::NewLayoutPoint,
        (EE_WON_CFG_T_CONTAINER, 5) => W::EnableLayout,
        (EE_WON_CFG_T_CONTAINER, 15) => W::SpaceEvenlyH,
        (EE_WON_CFG_T_CONTAINER, 16) => W::SpaceEvenlyV,
        (EE_WON_CFG_T_CONTAINER, 17) => W::ClearRight,
        (EE_WON_CFG_T_CONTAINER, 19) => W::SizeAtLeastLayout,
        (EE_WON_CFG_T_CONTAINER, 25) => W::LayoutPrev,
        (EE_WON_CFG_T_CONTAINER, 26) => W::LayoutChain,
        (EE_WON_CFG_T_CONTAINER, 27) => W::LayoutOffsets,
        (EE_WON_CFG_T_CONTAINER, 28) => W::LayoutRule,
        (EE_WON_CFG_T_CONTAINER, 29) => W::NoOverlap,
        (EE_WON_CFG_T_STRETCHIMAGE, 4) => W::StretchExport,
        (EE_WON_CFG_T_STRETCHIMAGE, 6) => W::StretchCut,
        (EE_WON_CFG_T_STRETCHIMAGE, 7) => W::StretchNamed,
        (EE_WON_CFG_T_STRETCHIMAGE, 8) => W::StretchNewChild,
        (EE_WON_CFG_T_IMAGEFILTER, 4) => W::FilterCut,
        (EE_WON_CFG_T_IMAGEFILTER, 5) => W::FilterNamed,
        _ => return None,
    })
}

/// A skin slot's index as the setter's expression had it: 0, `i[0]`, `i[0]?1:0`.
#[derive(Clone, Copy)]
enum Idx {
    Zero,
    Int,
    Bool,
}

/// The setters made of the frame's ops (R6 WON lobby and network round 39): WONSkinConfig's slots and button types
/// (`WONSkinSet(slot, index, SafeConfigGetSkinPart(c[0], name))`, a WON button's `SafeConfigGetComponent<WONButton*>`),
/// ComponentConfig's POS, SIZE and the set-size flags.
#[derive(Clone, Copy)]
enum Small {
    /// The slot, its index, whether the part is a WON button, the part's NUL-terminated name.
    SkinPart(Sl, Idx, bool, &'static std::ffi::CStr),
    /// The button type `s[0]`: simple (a skin part) or a WON button.
    SkinType(bool),
    Pos,
    Size,
    /// mHasSetWidth, mHasSetHeight set.
    HasSet(bool, bool),
}

fn small_of(setter: i32) -> Option<Small> {
    use crate::cfgtab::*;
    use Small::*;
    Some(match (setter >> 8, setter & 0xff) {
        (EE_WON_CFG_T_SKIN, 3) => SkinPart(Sl::PushButton, Idx::Zero, true, c"WONButton"),
        (EE_WON_CFG_T_SKIN, 4) => SkinPart(Sl::Checkbox, Idx::Zero, true, c"WONButton"),
        (EE_WON_CFG_T_SKIN, 6) => SkinPart(Sl::HeaderButton, Idx::Zero, false, c"WONHeaderButton"),
        (EE_WON_CFG_T_SKIN, 7) => SkinPart(Sl::TabButton, Idx::Zero, true, c"WONButton"),
        (EE_WON_CFG_T_SKIN, 8) => SkinPart(Sl::Arrow0, Idx::Int, false, c"WONSimpleButton"),
        (EE_WON_CFG_T_SKIN, 9) => SkinPart(Sl::ComboButton, Idx::Zero, false, c"WONSimpleButton"),
        (EE_WON_CFG_T_SKIN, 10) => SkinPart(Sl::CloseButton, Idx::Zero, false, c"WONSimpleButton"),
        (EE_WON_CFG_T_SKIN, 11) => SkinPart(Sl::ScrollGutter0, Idx::Bool, false, c"WONSimpleButton"),
        (EE_WON_CFG_T_SKIN, 12) => SkinPart(Sl::ScrollThumb0, Idx::Bool, false, c"WONSimpleButton"),
        (EE_WON_CFG_T_SKIN, 13) => SkinPart(Sl::Scrollbar0, Idx::Bool, false, c"WONScrollbar"),
        (EE_WON_CFG_T_SKIN, 14) => SkinPart(Sl::ScrollContainer, Idx::Zero, false, c"WONScrollContainer"),
        (EE_WON_CFG_T_SKIN, 15) => SkinPart(Sl::ListCtrl, Idx::Zero, false, c"WONListCtrl"),
        (EE_WON_CFG_T_SKIN, 17) => SkinPart(Sl::ComboBox, Idx::Zero, false, c"WONComboBox"),
        (EE_WON_CFG_T_SKIN, 18) => SkinPart(Sl::PopupBack, Idx::Zero, false, c"WONPopupBack"),
        (EE_WON_CFG_T_SKIN, 19) => SkinPart(Sl::DialogBack, Idx::Zero, false, c"WONDialogBack"),
        (EE_WON_CFG_T_SKIN, 1) => SkinType(false),
        (EE_WON_CFG_T_SKIN, 2) => SkinType(true),
        (EE_WON_CFG_T_COMPONENT, 7) => Pos,
        (EE_WON_CFG_T_COMPONENT, 8) => Size,
        (EE_WON_CFG_T_COMPONENT, 30) => HasSet(true, false),
        (EE_WON_CFG_T_COMPONENT, 31) => HasSet(false, true),
        (EE_WON_CFG_T_COMPONENT, 32) => HasSet(true, true),
        _ => return None,
    })
}

fn run_small(o: &Ops, which: Small) -> R<()> {
    use EeWonCfgTabOp as O;
    let part = |button: bool, name: &'static std::ffi::CStr| {
        if button {
            o.op(O::SafeWonButton, 0, 0)
        } else {
            o.op(O::SafePart, name.as_ptr() as isize, 0)
        }
    };
    match which {
        Small::SkinPart(slot, idx, button, name) => {
            let i = match idx {
                Idx::Zero => 0,
                Idx::Int => o.int(0) as isize,
                Idx::Bool => isize::from(o.int(0) != 0),
            };
            let c = part(button, name)?;
            o.op3(O::SkinSet, slot as isize, i, c)?;
        }
        Small::SkinType(simple) => {
            let c = part(!simple, c"WONSimpleButton")?;
            o.op(O::SkinSetType, isize::from(simple), c)?;
        }
        Small::Pos => {
            o.call(O::CompSetPos, o.int(0) as isize, o.int(1) as isize);
        }
        Small::Size => {
            o.call(O::CompSetSize, o.int(0) as isize, o.int(1) as isize);
        }
        Small::HasSet(w, h) => {
            if w {
                o.call(O::SetHasWidth, 1, 0);
            }
            if h {
                o.call(O::SetHasHeight, 1, 0);
            }
        }
    }
    Ok(())
}

/// The table setter `setter` when it is a composition: 1 when it ran, 0 when an op threw (the host keeps the
/// exception), -1 for another setter (the host's own case).
///
/// # Safety
/// As `ee_won_cfg_comp`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cfg_comp_setter(ops: EeWonCfgTabOpFn, frame: *mut c_void, setter: i32) -> i32 {
    if let Some(w) = of_setter(setter) {
        // SAFETY: the contract
        return i32::from(unsafe { ee_won_cfg_comp(ops, frame, w) });
    }
    let (Some(f), Some(small)) = (ops, small_of(setter)) else { return -1 };
    i32::from(run_small(&Ops { f, frame }, small).is_ok())
}

/// Appends bytes to the `Vec<u8>` at `v` (the host's way to hand back a string).
///
/// # Safety
/// `v` is a `Vec<u8>` handed out by this module; `bytes` holds `length` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cfg_comp_append(v: *mut c_void, bytes: *const u8, length: usize) {
    if v.is_null() || bytes.is_null() || length == 0 {
        return;
    }
    // SAFETY: the contract
    unsafe { (*(v as *mut Vec<u8>)).extend_from_slice(std::slice::from_raw_parts(bytes, length)) };
}

/// The ops `ComponentConfig::Virtual24` makes on its config.
#[repr(i32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonCfgKeyOp {
    /// `RunTable(t, key)` (guarded): true when the table holds the key.
    RunTable = 0,
    /// `HandleModify()` (guarded).
    HandleModify,
    /// `HandleMapComponent()` (guarded).
    HandleMapComponent,
}

/// The op function of `Virtual24`'s config: `ok` cleared by an op that threw (the host keeps the exception).
pub type EeWonCfgKeyHost = Option<
    unsafe extern "C" fn(
        config: *mut c_void,
        op: EeWonCfgKeyOp,
        table: i32,
        key: *const u8,
        len: usize,
        ok: *mut bool,
    ) -> bool,
>;

/// `ComponentConfig::Virtual24(key)` of a `kind` config: its kind's tables and its bases' in the original's order
/// (cfgkind.rs); ComponentConfig's own step answers MODIFY and MAPCOMPONENT through their handlers, the rest is its
/// table; the first step that holds the key ends the walk. 1 held, 0 not, 2 an op threw (the host rethrows).
///
/// # Safety
/// `host` and `config` are the C++ end's; `key` holds `len` bytes (or is NULL with 0).
#[no_mangle]
pub unsafe extern "C" fn ee_won_cfg_v24(
    host: EeWonCfgKeyHost,
    config: *mut c_void,
    kind: EeWonCfgKind,
    key: *const u8,
    len: usize,
) -> i32 {
    let Some(f) = host else { return 2 };
    let k: &[u8] = if key.is_null() || len == 0 {
        &[]
    } else {
        // SAFETY: the contract
        unsafe { std::slice::from_raw_parts(key, len) }
    };
    let op = |op: EeWonCfgKeyOp, table: i32| -> Option<bool> {
        let mut ok = true;
        // SAFETY: the host's contract
        let r = unsafe { f(config, op, table, key, len, &mut ok) };
        ok.then_some(r)
    };
    for step in v24_steps(kind) {
        let held = if step >= 0 {
            op(EeWonCfgKeyOp::RunTable, step)
        } else {
            match key_index(EE_WON_CFG_KEYS_COMPONENTCONFIG_VIRTUAL24_KEY, k) {
                0 => op(EeWonCfgKeyOp::HandleModify, 0),
                1 => op(EeWonCfgKeyOp::HandleMapComponent, 0),
                _ => op(EeWonCfgKeyOp::RunTable, EE_WON_CFG_KEYS_COMPONENTCONFIG_VIRTUAL24_KEY),
            }
        };
        match held {
            None => return 2,
            Some(true) => return 1,
            Some(false) => {}
        }
    }
    0
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn numbered_names_stop_at_a_nul() {
        assert_eq!(numbered(b"img", 3), b"img3");
        assert_eq!(numbered(b"a\0b", -1), b"a-1");
    }
}
