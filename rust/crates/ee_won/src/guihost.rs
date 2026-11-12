//! WONGUI's component objects' one outward call table (R6 WON round 21): every C++ call a Rust component object
//! makes (its base class's virtuals and fields, the component's services, the fonts and segments it holds, the
//! Graphics) is one op code of a single host function, `ee_won_guicpp_call` in `WONGUI/GuiHost.cpp`. As the lobby
//! logic's table (`lobbyhost`), an op is a run of C++ statements with no decision between them, so the host stays a
//! switch of short cases and the Rust keeps every branch and the call order.
//!
//! The pointers the ops take and return (segments, the Graphics, a draw operation) are opaque here: Rust only
//! passes them back. A segment Rust holds carries a reference (`SegHold`), given back with `SegRelease`.
//!
//! The base class's fields are read and written through `Get` / `Set` (`EeWonGuiField`): the base class's own code
//! (ScrollArea's clamp, the scroller) reads and changes them between the Rust object's statements, so the Rust
//! keeps no copy.

use std::ffi::c_void;

/// The host: `(component, op, a, b, c, d) -> result`. `component` is the C++ object the Rust one belongs to.
pub type EeWonGuiHostFn = Option<
    unsafe extern "C" fn(component: *mut c_void, op: EeWonGuiOp, a: isize, b: isize, c: isize, d: isize) -> isize,
>;

/// A base class field the Rust object reads or writes (`Get` / `Set`).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord)]
pub enum EeWonGuiField {
    /// `mScrollArea.width`.
    ScrollWidth = 0,
    /// `mScrollArea.height`.
    ScrollHeight,
    /// `mVertOffset`.
    VertOffset,
    /// `mHorzOffset`.
    HorzOffset,
    /// `mTotalWidth`.
    TotalWidth,
    /// `mTotalHeight`.
    TotalHeight,
    /// `mExtraHeight`.
    ExtraHeight,
    /// `mUserHasScrolled` (0 or 1).
    UserHasScrolled,
    /// `mUpScrollAmount`.
    UpScrollAmount,
    /// `mDownScrollAmount`.
    DownScrollAmount,
    /// `Width()` (read only).
    Width,
    /// `Height()` (read only).
    Height,
}

/// The cursor `SetParentCursor` sets on the parent.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonGuiCursor {
    /// NULL (the parent's own).
    None = 0,
    /// `Cursor::GetStandardCursor(StandardCursor_Hand)`.
    Hand = 1,
    /// `Cursor::GetStandardCursor(StandardCursor_IBeam)`.
    IBeam = 2,
}

/// The op codes. `a`..`d` are integers or pointers as each says.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonGuiOp {
    // ---- the base class (a ScrollArea for the text area)
    /// the field `a` (`EeWonGuiField`).
    Get = 1,
    /// the field `a` = `b`.
    Set,
    /// `ScrollArea::Virtual08()` (it calls the virtual AdjustPosition back).
    BaseSizeChanged,
    /// `ScrollArea::AdjustPosition(a != 0)`.
    BaseAdjustPosition,
    /// `ScrollArea::NotifyRootVisibility(a != 0)`.
    BaseNotifyRootVisibility,
    /// `ScrollArea::MouseDown(a, b, c)`.
    BaseMouseDown,
    /// `ScrollArea::MouseUp(a, b, c)`.
    BaseMouseUp,
    /// `ScrollArea::MouseExit()`.
    BaseMouseExit,
    /// `ScrollArea::MouseMove(a, b)`.
    BaseMouseMove,
    /// `ScrollArea::MouseEnter(a, b)`.
    BaseMouseEnter,
    /// `ScrollArea::MouseDrag(a, b)`.
    BaseMouseDrag,
    /// `ScrollArea::KeyDown(a)` (0 or 1).
    BaseKeyDown,
    /// `ScrollArea::KeyChar(a)` (0 or 1).
    BaseKeyChar,
    /// `ScrollTranslate(*a, *b)` (`a`, `b` point at i32s).
    ScrollTranslate,
    /// `Component::GotFocus()` (the input box's base).
    BaseGotFocus = 20,
    /// `Component::LostFocus()`.
    BaseLostFocus,
    /// `Component::Paint(*a)` (`a` the Graphics).
    BasePaint,
    // ---- the component
    /// `Invalidate()`.
    Invalidate = 30,
    /// `RequestTimer(a != 0)`.
    RequestTimer,
    /// `RequestFocus()`.
    RequestFocus,
    /// `GetMousePos(*a, *b)` (`a`, `b` point at i32s).
    GetMousePos,
    /// `GetKeyMod()`.
    KeyMod,
    /// `mParent->SetCursor(a)` (`EeWonGuiCursor`).
    SetParentCursor,
    /// the selection colours into `*a` (an `EeWonSelColor`).
    SelColor,
    /// `mLinkParam = a->GetLinkParam(); DeliverEvent(new TextLinkActivatedEvent(this, a), false)`.
    LinkActivated,
    /// the copied text (a GUIString the C++ object keeps) `.append(a->mText, b, c)`.
    SelTextAppend,
    /// the copied text `.append("\r\n")`.
    SelTextBreak,
    /// `Clipboard::Set(copied text)`, then the copied text emptied.
    SelTextCopy,
    /// `SetSize(a, b)`.
    SetSize = 45,
    /// `SetDefaultCursor(a)` (`EeWonGuiCursor`).
    SetDefaultCursor,
    /// `FireEvent(a, false)` (`EeWonGuiEvent`).
    FireEvent,
    /// `Clipboard::Set(text)`, the text `b` units (u32) at `a`.
    ClipboardSet,
    /// `Clipboard::Get()`'s units appended to the Rust buffer `a` with `ee_won_gui_sink_units`.
    ClipboardTake,
    // ---- a segment (`a` = its C++ object)
    /// `a->AddRef()`.
    SegHold = 60,
    /// `a->Release()`.
    SegRelease,
    /// `a->mLineBreak` (0 or 1).
    SegLineBreak,
    /// `a->GetLength()`.
    SegLength,
    /// `a->CalcDimensions()`'s measures into `*b` (three i32s: the text's width, the font's height and ascent).
    SegMeasure,
    /// `a->mFont->GetStringWidth(a->mText, b, c)`.
    SegStringWidth,
    /// `a->mFont->GetCharacterWidth(a->mText[b])`.
    SegCharWidth,
    /// `a`'s text units appended to the Rust buffer `b` with `ee_won_gui_sink_units`.
    SegTextTake,
    /// `a->IsLink()` (0 or 1).
    SegIsLink,
    /// `a->Pressed()`.
    SegPressed,
    /// `a->Released()` (0 or 1).
    SegReleased,
    /// `a->mColor`.
    SegColor,
    // ---- the Graphics
    /// executes the draw operation `*b` (an `EeWonDrawOp`) on the Graphics `a`, its font and string slots the
    /// segment `c`'s font and text (none when `c` is 0).
    Draw = 90,
    // ---- the input box (its font, icon, filter and the texts the C++ keeps for its readers)
    /// `mFont->GetHeight()`.
    FontHeight = 100,
    /// `mFont->GetCharacterWidth(a)`.
    FontCharWidth,
    /// `mFont->GetStringWidth(mText, a, b)` (both cast to unsigned short).
    TextWidth,
    /// `mFont->GetStringWidth(mText)`.
    TextWidthAll,
    /// `mFont->GetCharacterWidth(mText[a])`.
    TextCharWidth,
    /// `mFont->GetStringWidth(GUIString("M"))`.
    FontStringM,
    /// the C++ copy of a text (`a`: 0 the shown text `mText`, 1 the real one `GetText()`) = the `c` units (u32)
    /// at `b`.
    TextStore,
    /// `mInputFilter(this, s)` over the string `s` of the `b` units (u32) at `a`: 2 without a filter, else the
    /// filter's answer (0 or 1) with `s` after it appended to the Rust buffer `c`.
    Filter,
    /// `mIcon->mWidth`, or -1 without an icon.
    IconWidth,
    /// the C++ members of the paint into `*a` (an `EeWonInputPaint`: the disabled look, the icon and its height,
    /// the focus, the text colours, the font's height, the selection colours).
    PaintInputs,
    // ---- the label (its wrapped text and background)
    /// `mWrappedText->SetMaxWidth(a)`.
    WrapSetMaxWidth = 130,
    /// `mWrappedText->SetCentered(a != 0)`.
    WrapSetCentered,
    /// `mWrappedText->SetWrap(a != 0)`.
    WrapSetWrap,
    /// `mWrappedText->GetWidth()`.
    WrapWidth,
    /// `mWrappedText->GetHeight()`.
    WrapHeight,
    /// `mBackground.Paint(*a, 0, 0, b, c)`.
    PaintBackground,
}

/// The events a component object fires (`FireEvent`).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonGuiEvent {
    /// `ComponentEvent_InputReturn`.
    InputReturn = 1,
    /// `ComponentEvent_InputTextChanged`.
    InputTextChanged = 2,
}

/// Appends `count` text units of `unit_size` bytes (2 or 4: a `wchar_t`) to the Rust buffer of a take op.
///
/// # Safety
/// `sink` is the `b` of a take op; `units` holds `count` units of `unit_size` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_sink_units(
    sink: *mut c_void,
    units: *const c_void,
    count: usize,
    unit_size: usize,
) {
    if sink.is_null() || units.is_null() || count == 0 {
        return;
    }
    // SAFETY: the contract
    let out = unsafe { &mut *(sink as *mut Vec<u32>) };
    for i in 0..count {
        // SAFETY: the contract (unaligned reads: the units are a wchar_t array)
        let u = unsafe {
            match unit_size {
                2 => u32::from((units as *const u16).add(i).read_unaligned()),
                4 => (units as *const u32).add(i).read_unaligned(),
                _ => u32::from((units as *const u8).add(i).read()),
            }
        };
        out.push(u);
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn sink_reads_both_unit_sizes() {
        let mut v: Vec<u32> = Vec::new();
        let a: [u16; 3] = [0x41, 0x20, 0x263a];
        let b: [u32; 2] = [0x1f600, 0x42];
        // SAFETY: the buffers hold the counts given
        unsafe {
            ee_won_gui_sink_units(&mut v as *mut Vec<u32> as *mut c_void, a.as_ptr() as *const c_void, 3, 2);
            ee_won_gui_sink_units(&mut v as *mut Vec<u32> as *mut c_void, b.as_ptr() as *const c_void, 2, 4);
            ee_won_gui_sink_units(std::ptr::null_mut(), b.as_ptr() as *const c_void, 2, 4);
        }
        assert_eq!(v, vec![0x41, 0x20, 0x263a, 0x1f600, 0x42]);
    }
}
